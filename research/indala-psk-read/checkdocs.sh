#!/usr/bin/env bash
# Verify the notes have not drifted. Run before committing a notes change.
#
# Structure alone does not keep history and current knowledge apart — something has to
# fail loudly when a reference goes stale. This checks the four ways they drift:
#   1. FINDINGS cites a log entry that does not exist
#   2. NEXT or README cites a claim or rule that does not exist
#   3. a referenced file has been renamed or deleted
#   4. a commit hash in LOG is not reachable from HEAD (rebase, amend, wrong paste)
# It does NOT check that a claim is true. Only a measurement does that.
set -u
cd "$(dirname "$0")" || exit 1
fail=0
note() { echo "  ⛔ $*"; fail=1; }

ids() { grep -oE "^\| $1[0-9]+" "$2" 2>/dev/null | tr -d '| '; }
# ⚠ THE TRAILING \b IS LOAD-BEARING. Without it `C1K48S` — a real Wiegand format name, and
# now cited in NEXT.md — is read as a reference to claim C1 and reported missing. Same trap
# waits for C1K35S and anything else shaped like <letter><digits><letters> (C281).
refs() { grep -ohE "\b$1[0-9]+\b" "${@:2}" 2>/dev/null; }

echo "cross-references"
LOGIDS=$(ids L LOG.md)
for r in $(refs L FINDINGS.md NEXT.md README.md METHOD.md | sort -u); do
    grep -qx "$r" <<<"$LOGIDS" || note "$r cited but not in LOG.md"
done
# ⚠ F-NUMBERS LIVE IN `FIXES.md`, NOT `FINDINGS.md`. This read only FINDINGS.md, which was
# right until FIXES.md was created (C326) and then silently wrong: every F-citation in NEXT.md
# was reported missing. ⛔ The failure mode is the one that matters — a checker that cries wolf
# on correct notes gets worked around instead of fixed, which is what nearly happened twice.
CIDS=$(ids C FINDINGS.md; ids F FINDINGS.md; ids F FIXES.md)
# ⭐ FIXES.md is checked as a SOURCE of citations too, not just a ledger — it cites C-numbers
# and nothing was validating them.
for r in $(refs C NEXT.md README.md FIXES.md TOOLS.md | sort -u; refs F NEXT.md README.md | sort -u); do
    grep -qx "$r" <<<"$CIDS" || note "$r cited but not in FINDINGS.md or FIXES.md"
done
MIDS=$(grep -oE '^\*\*M[0-9]+' METHOD.md | tr -d '*')
for r in $(refs M NEXT.md README.md FINDINGS.md TOOLS.md | sort -u); do
    grep -qx "$r" <<<"$MIDS" || note "$r cited but not in METHOD.md"
done

echo "referenced files"
# ⚠ archive/ is deliberately excluded: those files are frozen and reference the structure
# as it was. Rewriting them to satisfy a checker would defeat the point of freezing them.
ROOT=$(git rev-parse --show-toplevel 2>/dev/null || echo ../..)
# LOG.md and METHOD.md are excluded by design: LOG names files as they were at the time
# (append-only, so a deleted file must keep its entry), and METHOD names removed files
# deliberately, as an explanation of why they were removed.
for f in $(grep -ohE '`[A-Za-z0-9_./-]+\.(md|py|sh)`' README.md FINDINGS.md NEXT.md ADVERSARIAL.md TOOLS.md |
           tr -d '`' | sort -u); do
    b=$(basename "$f")
    for cand in "$f" "$b" "$ROOT/$f" "$ROOT/software/script/$b" "$ROOT/firmware/$b"; do
        [ -e "$cand" ] && continue 2
    done
    # files that live in other repos are named, not linked; skip the ones we know
    # ⚠ `usage_check.sh` and `context_check.sh` live in utility-scripts/claude, outside this
    # repo, and are referenced by name. They are real and executable; `./autopilot.sh status`
    # runs both every tick, which is a far better liveness check than this one could be.
    case "$b" in t5577_campaign.py|usage_check.sh|context_check.sh|autocompact.sh|compact_request.sh) continue;; esac
    note "referenced but missing: $f"
done

echo "commit hashes in LOG.md"
# ⚠ only the commit column of the table — a 16-hex-digit credential like
# a0000000e6bd0e92 otherwise reads as a short hash and fails forever.
#
# ⛔ "IT RESOLVES" IS NOT THE TEST, AND THE WEAKER ONE LET A STALE HASH THROUGH. This used
# to run `git cat-file -e`, which succeeds for ANY object still in the store — including a
# commit orphaned by the amend that happened thirty seconds earlier. It passed, the notes
# pointed at a commit unreachable from any branch, and it would have kept passing until a
# gc removed the object and turned a silent rot into a sudden one.
# ⇒ The question is reachability from HEAD, not existence.
for h in $(grep -oE '^\| L[0-9]+ \|[^|]*\| `[0-9a-f]{7,40}`' LOG.md |
           grep -oE '`[0-9a-f]{7,40}`' | tr -d '`' | sort -u); do
    if ! git cat-file -e "${h}^{commit}" 2>/dev/null; then
        note "hash does not resolve: $h"
    elif ! git merge-base --is-ancestor "$h" HEAD 2>/dev/null; then
        note "hash resolves but is NOT reachable from HEAD (amended or rebased away): $h"
    fi
done

echo "duplicate sections"
# ⛔ NEXT.md reached 995 lines of which ~600 were DUPLICATED sections, and every
# cross-reference in it still validated. Repeated splice-edits matched the FIRST occurrence
# of an anchor while copies already existed, so each edit appended instead of replacing;
# §3b appeared six times. Checking references cannot see that — only counting headings can.
for f in README.md FINDINGS.md NEXT.md METHOD.md ADVERSARIAL.md; do
    [ -e "$f" ] || continue
    # ⚠ Compare WHOLE heading lines. A first-token match flags "## The ..." against any
    # other "## The ...", which is three false positives on these notes alone.
    dup=$(grep -E '^#{2,3} ' "$f" | sort | uniq -d | head -3)
    [ -n "$dup" ] && note "$f repeats a section heading: $(echo "$dup" | head -1)"
done

echo "edit-policy invariants"
# LOG.md is append-only: entries may gain a retraction pointer, never lose or reword one.
if git rev-parse HEAD >/dev/null 2>&1 && git cat-file -e HEAD:./LOG.md 2>/dev/null; then
    old=$(git show HEAD:./LOG.md | grep -cE '^\| L[0-9]+')
    new=$(grep -cE '^\| L[0-9]+' LOG.md)
    [ "$new" -ge "$old" ] || note "LOG.md lost entries ($old -> $new) — it is append-only"
fi
grep -q 'no history' FINDINGS.md || note "FINDINGS.md lost its no-history declaration"
for f in archive/*.md; do
    [ -e "$f" ] || continue
    grep -q 'FROZEN' "$f" || note "$f lost its FROZEN banner"
done

echo "stale counts"
# ⛔⛔ A FIFTH DRIFT CLASS, AND IT BIT TWICE IN ONE DAY. The four checks above ask whether a
# REFERENCE resolves. Neither of the day's two worst notes defects was a broken reference:
# §9h claimed the instrumentation gate cost "1,088 bytes of flash and 4,008 of RAM" when
# re-measurement said 1,560 and 8,016 — the RAM figure had exactly doubled (C390) — and
# FIXES.md's own header still said "9 of 11 pass ... F10 and F11 NOT CHECKED" hours after
# C393 made it 11 of 11. Both were load-bearing numbers a maintainer would be handed, and
# both were caught by someone happening to read them.
#
# ⭐ A stale count is NOT "a claim that is false", which this file correctly refuses to judge.
# It is TWO PLACES STATING DIFFERENT NUMBERS FOR THE SAME THING, which is internal consistency
# and is mechanically checkable.
#
# ⚠ DELIBERATELY NARROW. A checker that cries wolf on correct notes gets worked around instead
# of fixed — this file says so itself, twice. So only two patterns are checked, both of which
# were actually stale today, and anything ambiguous is left alone.
if [ -e FIXES.md ]; then
    entries=$(grep -cE '^\| F[0-9]+ \|' FIXES.md)
    # ⚠ A REGRESSION TARGET IS A *FIXED* ENTRY. F12 is registered and deliberately NOT FIXED
    # (C395), so it has no `fixcheck.sh` arm and must not be counted in an "N of M pass" total.
    # ⛔ This check's FIRST run flagged "11 of 11 pass" against a 12-row table — prose that was
    # correct in meaning and ambiguous on the page. Both sides were fixed: the count below
    # excludes open entries, and the sentence in FIXES.md now says which 11 it means. Loosening
    # a checker to silence a true ambiguity is how a checker stops being believed.
    fixed=$(grep -E '^\| F[0-9]+ \|' FIXES.md | grep -vc 'NOT FIXED')
    # (a) STRUCTURAL, zero false-positive risk: F-numbers must be 1..N with no gaps or repeats.
    #     Catches a row added without renumbering, or a duplicated number.
    maxf=$(grep -oE '^\| F[0-9]+ ' FIXES.md | grep -oE '[0-9]+' | sort -n | tail -1)
    uniqf=$(grep -oE '^\| F[0-9]+ ' FIXES.md | grep -oE '[0-9]+' | sort -n | uniq | wc -l | tr -d ' ')
    if [ -n "$maxf" ] && { [ "$entries" != "$maxf" ] || [ "$uniqf" != "$entries" ]; }; then
        note "FIXES.md F-numbers are not 1..N: $entries rows, highest F$maxf, $uniqf distinct"
    fi
    # (b) the exact sentence that went stale: "N of M pass".
    # ⛔⛔ CHECKING ONLY M IS NOT ENOUGH, AND THE BREAK-TEST CAUGHT ME. The first version compared
    # the DENOMINATOR against the entry count — so restoring today's actual stale sentence,
    # "9 of 11 pass", PASSED: the 11 was right and the 9 was the rot. A check written for a
    # specific failure that does not catch that failure is worse than none, because it is
    # believed. ⇒ Assert BOTH numbers.
    # ⚠ N is asserted equal to M deliberately. A genuine shortfall — a real regression, or an
    # arm that cannot run — then trips this and forces the prose to be rewritten ON PURPOSE,
    # which is the outcome wanted: `fixcheck.sh` is what REPORTS a regression, and the notes
    # claiming a stale all-clear is the thing this check exists to stop.
    while read -r pair; do
        # ⚠ A here-string always yields one line, so an EMPTY match reaches the loop as "" and
        # reported "FIXES.md says ' of  pass'" against a perfectly clean file. A checker whose
        # first output on correct notes is a false alarm is the failure mode this file warns
        # about twice — guard the empty case before comparing anything.
        [ -n "$pair" ] || continue
        n=${pair%% of *}; m=${pair##* of }
        if [ "$m" != "$fixed" ]; then
            note "FIXES.md says '$n of $m pass' but it has $fixed FIXED entries ($entries rows)"
        elif [ "$n" != "$m" ]; then
            note "FIXES.md says '$n of $m pass' — a shortfall must be stated deliberately, not left stale"
        fi
    done <<< "$(grep -oE '[0-9]+ of [0-9]+ pass' FIXES.md | sed 's/ pass//')"
    # (c) AUTOPILOT's §1 summary row states the same total.
    if [ -e AUTOPILOT.md ]; then
        while read -r n; do
            [ -n "$n" ] && [ "$n" != "$entries" ] && \
                note "AUTOPILOT.md says '$n registered' but FIXES.md has $entries entries"
        done <<< "$(grep -oE '\*\*[0-9]+ registered' AUTOPILOT.md | grep -oE '[0-9]+')"
    fi
fi

echo "tool coverage"
# ⛔⛔ A SIXTH DRIFT CLASS: A TOOL NOBODY CAN FIND GETS REBUILT. `README.md`'s Tooling table is a
# CURATED SUBSET — 13 of 43 scripts — and it silently stopped growing, so a tick that needed to
# drive a Chameleon spent four calls rediscovering `cu.py` (C409). The cost is not confusion, it
# is REWORK, and the same rediscovery had already happened for the venv python.
#
# ⭐ `TOOLS.md` is the COMPLETE inventory, and this is what keeps it complete: every executable
# in this directory must appear in it. Structural, mechanically checkable, zero judgement.
# ⚠ Only executables — a data file or a half-written draft is not a tool, and demanding a row
# for one is how a checker starts crying wolf (this file's repeated lesson).
if [ -e TOOLS.md ]; then
    missing=""
    for t in *.py *.sh; do
        [ -e "$t" ] || continue
        [ -x "$t" ] || continue
        grep -q "\`$t\`" TOOLS.md || missing="$missing $t"
    done
    [ -n "$missing" ] && note "TOOLS.md does not list:$missing (it is the complete inventory — add the row)"
else
    note "TOOLS.md is missing — it is the tool inventory checkdocs validates against"
fi

[ $fail -eq 0 ] && echo "✓ notes consistent" || echo "✗ see above"
exit $fail
