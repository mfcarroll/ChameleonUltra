#!/bin/zsh
# Hold the 125kHz LF field up (near-continuously) so TP7 can be probed with a scope.
#
#   ./fieldhold.sh [seconds]     default 60
#
# Works by looping a read that is EXPECTED TO FAIL. A failing LF read keeps the
# field energised for the full g_timeout_readem_ms (500ms, lf_reader_main.c:25),
# where `lf sniff` drops it after ~32ms once its 4000-sample buffer fills.
# All reads run in ONE cu.py session, so there is no reconnect gap between them.
#
# `lf hid prox read` is used because it fails on a PSK tag, which is the point:
# the field stays up while the tag sits there responding in a modulation this
# firmware cannot decode. That is exactly the condition we want on the scope.
HERE="$(cd "$(dirname "$0")" && pwd)"
CLI="$HERE/../../software/script"
SECS="${1:-60}"
N=$(( (SECS + 1) / 1 ))          # ~1 read per second incl. round-trip

cd "$CLI"
CMDS=("hw mode -r")
for i in $(seq 1 $N); do CMDS+=("lf hid prox read"); done

echo "Holding LF field for ~${SECS}s (${N} reads). Probe TP7 now."
echo "Ctrl-C to stop early."
.venv/bin/python cu.py "${CMDS[@]}" 2>&1 \
  | grep -Ev "^LF tag not found|connected:|Switch to|^$" \
  | tail -5
echo "Field released."
