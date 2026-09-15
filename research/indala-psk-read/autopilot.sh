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
    # ⛔⛔ THE GATE WAS ONLY A SECRET SCAN, AND THAT LET UNCOMPILED FIRMWARE BE COMMITTED.
    # Every tick reported "✓ gate clean" before committing and read it as *this builds*. It
    # never meant that. F13 changed `lf_tag_em.c` — a file NO host test compiles, because
    # `ctest/` builds the decoders and the emitters and not the tag-emulation driver — and the
    # change reached origin having never been through a compiler (C418).
    # ⭐ So: when the staged diff touches `firmware/`, BUILD IT. Notes-only commits are the
    # common case and stay instant, which is what keeps this from being worked around.
    # ⚠ `build.sh` needs two things this shell does not give it: the toolchain is in
    # /opt/homebrew/bin, not the /usr/bin its Makefile.posix names, and `nrfutil` is not on
    # PATH — the same absence that masqueraded as a dead device for four hours (C200).
    if git -C "$REPO" diff --cached --name-only | grep -q '^firmware/'; then
        echo "  firmware touched — compiling (notes-only commits skip this)"
        if (cd "$REPO/firmware" && PATH="/Users/Shared/code/personal/rfid/.tools/bin:$PATH" \
              GNU_INSTALL_ROOT=/opt/homebrew/bin/ GNU_VERSION=$(arm-none-eabi-gcc -dumpversion) \
              bash build.sh >/tmp/indala_fwbuild.log 2>&1); then
            echo "  ✓ firmware builds"
        elif grep -q 'ultra-dfu-app.zip' /tmp/indala_fwbuild.log 2>/dev/null || \
             [ -f "$REPO/firmware/objects/ultra-dfu-app.zip" ]; then
            # ⚠ build.sh's LAST step is `mergehex`, which is not installed here and is only
            # the SWD convenience artifact. The DFU package is already written by then, so a
            # failure after it is not a build failure — check for the artifact, not the code.
            echo "  ✓ firmware builds (packaging tail failed: mergehex absent, DFU zip written)"
        else
            echo "⛔ FIRMWARE DOES NOT BUILD — see /tmp/indala_fwbuild.log. DO NOT COMMIT."
            exit 1
        fi
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
            # ⛔⛔⛔ `pct` IS A FRACTION OF THE *EFFECTIVE WINDOW*, WHICH `autoCompactWindow`
            # SHRINKS — AND READING IT AS A FRACTION OF A FIXED 1M COST TWO TICKS (C467).
            # This block used to stop the loop at pct>=40 and say "autocompact should have taken
            # it already". Both halves were wrong once the operator set autoCompactWindow=400000:
            # the setting does NOT mean "compact at 40%", it means "the window IS 400000", and
            # context_check divides by exactly that. So 48% meant 190k used with 176k FREE, and a
            # compaction was nowhere near — while this line called it OVER THE LINE and two
            # firmware units were declined for a budget that was never tight.
            # ⇒ Compaction fires when the window is nearly full, i.e. when `free` runs down to the
            # reserved buffer — about 92% here — NOT at the setting's numeric value. The numbers
            # below are fractions of whatever `window=` actually says, so they follow the setting
            # instead of contradicting it.
            *) free=$(printf '%s' "$ctx" | sed -nE 's/.*free=([0-9]+).*/\1/p')
               win=$(printf '%s' "$ctx" | sed -nE 's/.*window=([0-9]+).*/\1/p')
               tot=$(printf '%s' "$ctx" | sed -nE 's/.*total=([0-9]+).*/\1/p')
               where="${pct}% of a ${win:-?}-token window (${tot:-?} used, ${free:-?} free)"
               if [ "$pct" -ge 85 ]; then
                   echo "⛔ context   $where — a compaction is CLOSE (it fires when free hits the"
                   echo "            reserved buffer, ~92%). Land the work NOW and start nothing that"
                   echo "            cannot be committed inside this tick."
               elif [ "$pct" -ge 70 ]; then
                   echo "⚠ context    $where — commit and push before taking anything new."
               else
                   echo "context     $where"
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
    # ⛔ `autocompact.sh` RENDERS THE SETTING AS A PERCENTAGE OF A NOTIONAL 1M WINDOW, WHICH
    # READS LIKE A TRIGGER THRESHOLD AND IS NOT ONE (C467). `autoCompactWindow: 400000` sets the
    # WINDOW SIZE; compaction fires when that window is nearly full. The context line above is
    # already a fraction of this same number, so the two must never be compared as if one were a
    # threshold for the other.
    sh "$UTIL/autocompact.sh" --project "$REPO" 2>/dev/null \
        | sed 's/^autocompact \([0-9]*\)% (\([0-9]*\) tokens)/autocompact window = \2 tokens (NOT a \1% trigger — see C467)/' \
        || echo "autocompact unavailable"
    echo "devices     $(ls /dev/cu.usbmodem* 2>/dev/null | wc -l | tr -d ' ') of 4 enumerated"
    ls /dev/cu.usbmodem* 2>/dev/null | sed 's/^/            /'
    # ⛔⛔ ASK THE HARDWARE WHAT IT IS RUNNING. Firmware was changed, committed and left
    # unflashed for a whole tick (C358): #2 sat several commits behind while the notes and the
    # gate both reported everything clean, because both check the REPOSITORY and neither asks
    # the device. A result taken against a stale build is unattributable and looks exactly like
    # a good one. ⚠ Best-effort: a missing venv, a busy port or a device in DFU must not fail
    # `status`, which callers chain with `&&`.
    # ⛔⛔ ASK BOTH DEVICES, AND NAME WHICH ONE ANSWERED. This used to ask #2 alone, and the
    # moment the research build went onto #1 the line shouted THE DEVICE IS BEHIND every tick
    # about a unit that is deliberately stock, while saying nothing about the unit actually
    # carrying the build under test. ⚠ A warning that is wrong every tick gets ignored, which is
    # how C358 happened in the first place — so a per-device answer is the point, not a tidier
    # message. ⚠ Opens each port DIRECTLY: `cu.py` without -p answers from whichever Chameleon
    # enumerates first, which is C459/C461.
    if [ -x "$REPO/software/script/.venv/bin/python" ]; then
        fwcommit=$(git -C "$REPO" log -1 --format=%h -- firmware/ 2>/dev/null)
        for dev in "#1:/dev/tty.usbmodemC3A1656543DE1" "#2:/dev/tty.usbmodemF429364E46961"; do
            name=${dev%%:*}; port=${dev#*:}
            [ -e "$port" ] || continue
            fw=$("$REPO/software/script/.venv/bin/python" - "$port" <<'PYEOF' 2>/dev/null
import sys
sys.path.insert(0, "/Users/Shared/code/personal/rfid/ChameleonUltra/software/script")
try:
    import chameleon_com, chameleon_cmd
    d = chameleon_com.ChameleonCom(); d.open(sys.argv[1])
    print(chameleon_cmd.ChameleonCMD(d).get_git_version()); d.close()
except Exception:
    pass
PYEOF
)
            built=$(printf '%s' "$fw" | sed -E 's/.*-g([0-9a-f]+).*/\1/')
            if [ -z "$fw" ]; then
                echo "firmware    $name did not answer (busy, DFU, or reader mode) — check before device work"
            elif printf '%s' "$fw" | grep -q -- "-dirty"; then
                echo "⚠ firmware  $name runs $fw — built from a DIRTY tree, so it matches no commit"
            elif git -C "$REPO" merge-base --is-ancestor "$fwcommit" "$built" 2>/dev/null; then
                echo "firmware    $name runs $fw — current (includes $fwcommit, the last firmware commit)"
            else
                echo "⛔ firmware  $name runs $fw — MISSING firmware commit $fwcommit, IT IS BEHIND (C358)"
            fi
        done
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

bench)
    # ⭐⭐ WHICH LINKS ARE ACTUALLY COUPLED — because "4 of 4 enumerated" says nothing about it.
    #
    # ⛔ This exists because §1 claimed "rig A is Flipper + Chameleon #1 ... and it WORKS" for a
    # whole session after the pads had been rearranged, and a tick then read the resulting
    # expected nulls as a four-arm regression (C402/C403, retracted by C404). Enumeration and
    # coupling are different questions and only one of them was ever asked.
    #
    # ⛔⛔ THE TOPOLOGY HAS MOVED TWICE IN ONE DAY AND A STALE MAP HERE IS WHAT PRODUCED
    # C402/C404 AND C458. The operator restored the STANDARD two-rig bench on 2026-09-15:
    #     rig A:  Flipper + Chameleon #1
    #     rig B:  Chameleon #2 – T5577 (PAC CARD0001) – Proxmark, SANDWICHED
    # Every arm below probes THAT bench. When the pads move again, this block moves with them.
    #
    # ⚠ THE SANDWICH CHANGES WHAT THE pm3 ARM CAN MEAN. The T5577 sits between #2 and the pm3,
    # so the pm3's carrier energises the TAG and it transmits PAC continuously. A capture here
    # is the TAG unless the tag is lifted out — never attribute a pm3 capture to #2 in this
    # geometry (C464 flagged exactly this superposition). ⇒ the pm3 arm probes the TAG.
    #
    # ⛔⛔⛔ AND THE PROBE MUST FIT THE INSTRUMENT. C465: the Flipper decodes NOTHING from a REAL
    # PAC tag, 3 of 3, on a tag the pm3 read byte-exact the same day — so a DECODE probe against
    # the Flipper arm would print DEAD for a perfectly coupled pad and invite exactly the false
    # bench report C458 made. The Flipper arm therefore asks for a RAW CAPTURE and scores
    # COUPLING (pair count), never a credential.
    HERE="$(cd "$(dirname "$0")" && pwd)"
    PY="$HERE/../../software/script/.venv/bin/python"
    CU="$HERE/../../software/script/cu.py"
    P1=/dev/tty.usbmodemC3A1656543DE1
    P2=/dev/tty.usbmodemF429364E46961
    PM3=/Users/Shared/code/personal/rfid/proxmark3/pm3
    cu1 () { "$PY" "$CU" "hw connect -p $P1" "$@" 2>&1; }
    cu2 () { "$PY" "$CU" "hw connect -p $P2" "$@" 2>&1; }
    echo "  bench links, standard two-rig topology: Flipper + #1, and #2 - T5577 - pm3 sandwiched"
    echo ""

    # ── arm 1: Flipper <-> Chameleon #1 ──────────────────────────────────────────────────
    # ⚠ Scored on PAIR COUNT, not on a decode, for the same reason as before: a decode probe
    # turns a coupling question into a protocol question, and C465 showed how badly that can
    # mislead. An empty capture is a 20-byte file; a coupled pad returns thousands of pairs.
    # ⚠ EM410X on purpose — #1 is on an older build (C358) and EM410X has been present and
    # working on every build this project has flashed.
    flip=DEAD; flipwhy=""
    "$PY" "$HERE/flipper.py" heap >/dev/null 2>&1 || "$PY" "$HERE/flipper.py" reboot >/dev/null 2>&1
    cu1 "hw slot type -s 8 -t EM410X" "hw slot enable -s 8 --lf" \
        "lf em 410x econfig -s 8 --id DEADBEEF88" "hw slot change -s 8" "hw mode -e" >/dev/null 2>&1
    sleep 1
    fo=$("$PY" "$HERE/flipraw.py" capture em410x --no-arm --seconds 3 2>&1)
    pairs=$(printf '%s' "$fo" | sed -n 's/.*bytes, \([0-9][0-9]*\) pulse.*/\1/p' | head -1)
    case "$fo" in *"would not load"*|*REJECTED*)
        flipwhy=" — the rfid app will not load (C377/flipraw), NOT a coupling verdict" ;; esac
    if [ -n "$pairs" ] && [ "$pairs" -gt 1000 ]; then
        flip="LIVE ($pairs pairs)"
    fi
    # ⛔ leave nothing armed.
    cu1 "hw mode -r" >/dev/null 2>&1

    # ── arm 2: pm3 <-> the real T5577 (and #2 behind it) ─────────────────────────────────
    # ⚠ The criterion is derived from pac.c's own bitstream, not from a histogram: a REPEATING
    # CARD0001 frame at 256us/bit predicts 256us ~48.5%, 512 ~29.1%, 768 ~9.7%, 1280 ~8.1% and
    # a longest run of exactly 2304us (C471). Both probes run: the raw buffer answers COUPLING
    # and `lf search` answers IDENTITY, and only the raw one returns a verdict if they disagree
    # (a decode failure is never evidence about a signal — C464, C465).
    # ⛔ Do NOT read a pass here as a statement about #2. The tag is the nearer emitter.
    pm3tag=DEAD; pm3why=""
    cap=$("$PY" "$HERE/pm3cap.py" --label "real T5577, PAC CARD0001" --samples 40000 \
          --expect 256,512,768,1280 2>&1)
    if printf '%s' "$cap" | grep -q "✓ 256"; then
        pm3tag="LIVE"
        printf '%s' "$cap" | grep -q "✓ 1280" || pm3why=" (short bins only — check the hysteresis, C471)"
    fi
    dec=no
    "$PM3" -c "lf search" 2>&1 | grep -qi "CARD0001" && dec=yes
    [ "$dec" = yes ] && pm3why="$pm3why, decoded byte-exact as CARD0001"
    [ "$dec" = no ] && [ "$pm3tag" = LIVE ] && pm3why="$pm3why, raw only — decoder silent"

    # ── arm 2b: #2 <-> the T5577, the other half of the sandwich ──────────────────────────
    # ⚠ Reading a REAL tag, so M52 does not apply — that rule is about reading an EMULATION.
    two=DEAD
    cu2 "lf pac read" 2>&1 | grep -qi "CARD0001" && two="LIVE (reads CARD0001)"

    printf "  %-34s %s\n" "Flipper <-> #1"            "$flip$flipwhy"
    printf "  %-34s %s\n" "pm3 <-> T5577 (raw buffer)" "$pm3tag$pm3why"
    printf "  %-34s %s\n" "#2 <-> T5577"              "$two"
    echo ""
    [ "$flip" = DEAD ] && echo "  ⛔ BLOCKED without the Flipper arm: re-grading GProxII and FDX-B, which"
    [ "$flip" = DEAD ] && echo "     are the two arms it still decodes byte-exact (C429/C430). ⇒ ASK FOR:"
    [ "$flip" = DEAD ] && echo "     Chameleon #1 flat on the Flipper's pad."
    [ "$pm3tag" = DEAD ] && echo "  ⛔ BLOCKED without the pm3 arm: every capture-based unit — it is the"
    [ "$pm3tag" = DEAD ] && echo "     instrument of record (C464/C466/C469/C470/C471). ⇒ ASK FOR: the T5577"
    [ "$pm3tag" = DEAD ] && echo "     flat on the Proxmark's antenna."
    [ "$two" = DEAD ] && echo "  ⚠ #2 does not read the tag — blocks the Chameleon-side read column only;"
    [ "$two" = DEAD ] && echo "     the pm3 arm above is unaffected and is the one findings rest on."
    if [ "$flip" != DEAD ] && [ "$pm3tag" = LIVE ]; then
        echo "  ✅ Both rigs live — the write column and the real-tag read column are open."
        echo "  ⚠ For an EMULATION capture the tag must come OUT of the sandwich first: in this"
        echo "     geometry the pm3 hears the TAG, not #2 (NEXT.md item 0)."
    fi
    echo "  ⚠ A DEAD arm is a PROBE RESULT, not a topology fact (C402/C404/C408/C458). With no"
    echo "     operator at the bench, report which probes were silent and stop — never report a"
    echo "     bench change, and never infer one from a silent null."
    ;;
*)
    echo "usage: autopilot.sh {beat|gate|status|bench}" >&2
    exit 2
    ;;

esac
