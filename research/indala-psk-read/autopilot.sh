#!/usr/bin/env bash
# Autopilot plumbing for the unattended LF-protocol run. See AUTOPILOT.md.
#
#   ./autopilot.sh beat     touch the heartbeat (do this at every unit boundary)
#   ./autopilot.sh gate     secret-scan the STAGED diff; non-zero means DO NOT COMMIT
#   ./autopilot.sh status   heartbeat age, last commit age, usage, devices, port holders
#
# ⛔ `gate` is the one that must never be got wrong. grep SUCCEEDS when it finds a
# secret, so the abort hangs off `&&`. A `||` chain here inverts the logic and turns
# the gate into a rubber stamp — which is exactly the shape of bug that ships.
set -u
cd "$(dirname "$0")" || exit 1
BEAT=/tmp/indala_autopilot.heartbeat
REPO=$(git rev-parse --show-toplevel)

case "${1:-status}" in

beat)
    date +%s > "$BEAT"
    echo "beat $(date '+%H:%M:%S')"
    ;;

gate)
    # ⛔ PATTERNS THAT ARE NOT IN THE BRANCH TODAY AND MUST NEVER ENTER IT. Deliberately
    # NOT a list of everything sensitive-looking: the bench device serials and the
    # Flipper's port name are already published in README.md's bench table on purpose,
    # so gating on those would fire on every commit and train the next session to skip
    # the gate. A gate that cries wolf is worse than no gate.
    # ⛔⛔ THE LITERALS ARE SPLIT ON PURPOSE — DO NOT "TIDY" THEM BACK TOGETHER.
    # Written whole, this line matches ITSELF: the first run of the gate failed on its own
    # source, because a file quoting the strings it warns about is itself a match. The
    # alternative — excluding this file from the scan — would mean a secret pasted in here
    # sails through, so the exclusion would be load-bearing and silent. Splitting each
    # literal across a concatenation keeps the gate scanning EVERY staged file, this one
    # included, while the source contains no string that can match.
    PAT="sk-""ant-"
    PAT="$PAT"'|/Users/[A-Za-z.]*car'"roll"
    PAT="$PAT"'|@stand'"\.earth"
    PAT="$PAT"'|BEGIN [A-Z ]*PRIVATE KEY|AKIA[0-9A-Z]{16}'
    if git diff --cached | grep -nE "$PAT"; then
        echo "⛔ SECRET GATE: the staged diff matches the line(s) above. DO NOT COMMIT."
        echo "   Unstage the file, remove the string, and re-run. Never --no-verify past this."
        exit 1
    fi
    echo "✓ gate clean"
    ;;

status)
    now=$(date +%s)
    if [ -f "$BEAT" ]; then
        echo "heartbeat   $(( (now - $(cat "$BEAT")) / 60 )) min old"
    else
        echo "heartbeat   MISSING (counts as stale)"
    fi
    echo "last commit $(( (now - $(git -C "$REPO" log -1 --format=%ct)) / 60 )) min old   $(git -C "$REPO" log -1 --format='%h %s' | cut -c1-64)"
    echo "branch      $(git -C "$REPO" rev-parse --abbrev-ref HEAD)   $(git -C "$REPO" status --short | wc -l | tr -d ' ') dirty"
    # ⚠ BOTH LIVE IN utility-scripts/claude NOW. `usage_check.sh` moved there from the
    # Momentum tree 2026-09-14; the old path silently produced "usage unavailable" and the
    # loop paced blindly without ever saying why.
    UTIL=/Users/Shared/code/personal/utility-scripts/claude
    sh "$UTIL/usage_check.sh" 2>/dev/null \
        || echo "usage       unavailable — pace blindly, commit every 20 min"
    # ⭐ CONTEXT IS A COST, NOT JUST A CAPACITY. Every turn re-sends the whole window, so a
    # large context is paid again on every subsequent turn; compacting costs one summarisation
    # and makes every later turn cheap. ⛔ This is the opposite of what this session believed
    # until the operator corrected it (C369). The check is local, needs no credential and costs
    # no tokens, so it is safe to run every tick.
    # ⛔⛔ RUN IT FROM THE REPO ROOT, AND REFUSE A ZERO-MESSAGE ANSWER. `-c` means "the most
    # recent session in $PWD", and sessions are keyed by the directory they STARTED in — the
    # repo root, not this one. Run from here it finds nothing, silently measures a FRESH
    # session's fixed overhead and reports **4%** while the real figure is 90%. A check that
    # reports green when it should be red is worse than no check (C370).
    # ⇒ cd to $REPO, and treat `msgs=0` as "that is not our session" rather than as a number.
    #
    # ⛔⛔⛔ AND THAT GUARD WAS NOT ENOUGH — IT COST A WHOLE RUN (C406). `msgs=0` catches the check
    # measuring a FRESH session. It cannot catch the check measuring SOMEBODY ELSE'S: with
    # CLAUDE_SESSION_ID unset in an agent's shell, `-c` resolved to a real, populated session
    # (`pct=8 msgs=30886`) and every tick reported **8% while the true figure was 70%**. The ≥40%
    # stop rule — the only safety rule this loop has — was silently disabled for the entire run,
    # and only the operator running this same command from their own terminal could see it.
    #
    # ⇒ NEVER FALL BACK TO `-c`. `-c` means "the most recent session in $PWD", which is a
    # different question depending on who asks and from where. The session must be named.
    # ⭐ An agent always has its own id: it is the UUID in its scratchpad path,
    #    /private/tmp/claude-*/<repo-slug>/<THIS-UUID>/scratchpad — verified to return pct=71
    #    against an operator-observed 70%.
    # ⚠ An ABSENT number stops the loop; a WRONG one does not. So refuse, loudly, rather than guess.
    if [ -z "${CLAUDE_SESSION_ID:-}" ]; then
        echo "⛔ context   UNREADABLE — CLAUDE_SESSION_ID is unset, and this check will NOT guess (C406)."
        echo "            Export it first: it is the UUID in your scratchpad path,"
        echo "            /private/tmp/claude-*/<repo-slug>/<UUID>/scratchpad"
        echo "            Treat as OVER THE LINE until it reads a real number."
        ctx=""
    else
        ctx=$(cd "$REPO" && sh "$UTIL/context_check.sh" "$CLAUDE_SESSION_ID" 2>/dev/null | tail -1)
    fi
    case "$ctx" in
        *msgs=0*) echo "⛔ context   UNREADABLE — msgs=0 means it measured a fresh session, not ours. Do not trust it."
                  ctx="" ;;
    esac
    if [ -n "$ctx" ]; then
        pct=$(printf '%s' "$ctx" | sed -nE 's/.*pct=([0-9]+).*/\1/p')
        case "${pct:-0}" in
            ''|*[!0-9]*) echo "context     $ctx" ;;
            *) if [ "$pct" -ge 40 ]; then
                   echo "⛔ context   ${pct}% — OVER THE LINE. Unattended: autocompact should have taken it already, so if"
                   echo "            this session started before the setting was written, say so. Operator present: commit,"
                   echo "            push, and ask for a manual /compact in ONE line — you cannot trigger one (C372)."
               elif [ "$pct" -ge 30 ]; then
                   echo "⚠ context    ${pct}% — finish the current unit and leave the tree committed."
               else
                   echo "context     ${pct}% used"
               fi
               # ⭐⭐ THE STALENESS GUARD — the operator's, and it is the belt to the braces above.
               # Context CHANGES every tick: a tick appends turns, so it climbs, and a compaction
               # makes it fall sharply. What it does not do is sit on the SAME number tick after
               # tick — which is exactly what a wrong-session reading does, because that session
               # is not the one doing the work. Eight percent, unchanged, for a whole run (C406).
               # ⚠ DELIBERATELY "UNCHANGED", NOT "DID NOT INCREASE". A compact makes the number
               # DROP, and a guard demanding an increase would cry wolf on the very next tick
               # after every compaction — which is how a check gets worked around instead of
               # fixed. A drop is movement, and movement is all this asks for.
               # ⚠ Three consecutive identical readings, not two: a quiet tick can legitimately
               # round to the same percent once.
               # ⚠ TRACK THE RAW TOTAL, NOT THE ROUNDED PERCENT. A percent is ~10,000 tokens at
               # this window size, so two real ticks can legitimately round to the same number and
               # a pct-based guard would cry wolf. `total=` moves on EVERY turn, so an identical
               # total is not "quiet", it is "not this session".
               tot=$(printf '%s' "$ctx" | sed -nE 's/.*total=([0-9]+).*/\1/p')
               prev_f=/tmp/indala_autopilot.ctxtotal
               prev=$(cat "$prev_f" 2>/dev/null | head -1)
               same=$(cat "$prev_f" 2>/dev/null | tail -1)
               [ -n "$tot" ] && [ "$prev" = "$tot" ] && same=$((${same:-1} + 1)) || same=1
               printf '%s\n%s\n' "$tot" "$same" > "$prev_f"
               if [ "${same:-1}" -ge 3 ]; then
                   echo "⛔ context   total $tot UNCHANGED for $same ticks — that is not this session (C406)."
                   echo "            Context moves every tick: work adds to it, a compact drops it."
                   echo "            A number that never moves is measuring somebody else. Check CLAUDE_SESSION_ID."
               fi ;;
        esac
    fi
    # ⛔⛔ NOTHING CAN TYPE /compact FOR THIS SESSION, AND THE THRESHOLD ONLY BITES AT STARTUP.
    # Measured twice: with autoCompactWindow written and context at 209k against a 167k window, this
    # session crossed two turn boundaries without compacting, while a CLI started fresh in the same
    # directory read the new value. A settings-FILE write never reaches a running session (C372).
    # ⇒ So this line is INFORMATION, not a control. It says what a session STARTED HERE would get.
    # Unattended runs set it before launching; an operator-present run reads the context percentage
    # above and asks for a manual /compact.
    sh "$UTIL/autocompact.sh" --project "$REPO" 2>/dev/null \
        | sed 's/^autocompact /autocompact /' \
        || echo "autocompact unavailable"
    echo "devices     $(ls /dev/cu.usbmodem* 2>/dev/null | wc -l | tr -d ' ') of 4 enumerated"
    ls /dev/cu.usbmodem* 2>/dev/null | sed 's/^/            /'
    # ⛔⛔ ASK THE HARDWARE WHAT IT IS RUNNING. Firmware was changed, committed and left
    # unflashed for a whole tick (C358): #2 sat several commits behind while the notes and the
    # gate both reported everything clean, because both check the REPOSITORY and neither asks
    # the device. A result taken against a stale build is unattributable and looks exactly like
    # a good one. ⚠ Best-effort: a missing venv, a busy port or a device in DFU must not fail
    # `status`, which callers chain with `&&`.
    if [ -x "$REPO/software/script/.venv/bin/python" ] && [ -e /dev/tty.usbmodemF429364E46961 ]; then
        fw=$("$REPO/software/script/.venv/bin/python" - <<'PYEOF' 2>/dev/null
import sys
sys.path.insert(0, "/Users/Shared/code/personal/rfid/ChameleonUltra/software/script")
try:
    import chameleon_com, chameleon_cmd
    d = chameleon_com.ChameleonCom(); d.open("/dev/tty.usbmodemF429364E46961")
    print(chameleon_cmd.ChameleonCMD(d).get_git_version()); d.close()
except Exception:
    pass
PYEOF
)
        # ⛔⛔ COMPARE AGAINST THE LAST FIRMWARE COMMIT, NOT HEAD. A first version compared to
        # HEAD and went red the moment a notes-only commit landed — which is every second
        # commit here. A check that is permanently red gets worked around rather than fixed,
        # which is exactly what nearly happened to `checkdocs.sh`. What matters is whether the
        # flashed build CONTAINS the newest firmware change; notes commits after it are
        # irrelevant to the device.
        fwcommit=$(git -C "$REPO" log -1 --format=%h -- firmware/ 2>/dev/null)
        built=$(printf '%s' "$fw" | sed -E 's/.*-g([0-9a-f]+).*/\1/')
        if [ -z "$fw" ]; then
            echo "firmware    #2 did not answer (busy, DFU, or reader mode) — check before device work"
        elif printf '%s' "$fw" | grep -q -- "-dirty"; then
            echo "⛔ firmware  #2 runs $fw — built from a DIRTY tree, so it matches no commit"
        elif git -C "$REPO" merge-base --is-ancestor "$fwcommit" "$built" 2>/dev/null; then
            echo "firmware    #2 runs $fw — current (includes $fwcommit, the last firmware commit)"
        else
            echo "⛔ firmware  #2 runs $fw — MISSING firmware commit $fwcommit, THE DEVICE IS BEHIND (C358)"
        fi
    fi
    # ⚠ A held port means another session is driving that device. Skip device work.
    holders=$(lsof /dev/cu.usbmodem* 2>/dev/null | tail -n +2)
    if [ -n "$holders" ]; then
        echo "⛔ PORT HELD — another session is on the CLI:"
        echo "$holders"
    fi
    # ⚠ status must exit 0 even when every optional probe comes back empty, or a caller
    # chaining `status && work` silently stops working.
    exit 0
    ;;

*)
    echo "usage: autopilot.sh {beat|gate|status}" >&2
    exit 2
    ;;
esac
