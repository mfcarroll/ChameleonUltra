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
    sh /Users/Shared/code/personal/rfid/Momentum-Firmware/T5577_block0_analysis_data/usage_check.sh 2>/dev/null \
        || echo "usage       unavailable — pace blindly, commit every 20 min"
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
