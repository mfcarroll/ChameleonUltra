#!/bin/zsh
# ⭐⭐ THE SAME BATTERY, RUN THE SAME WAY, BEFORE AND AFTER THE BENCH IS TOUCHED.
#
# C299 and C306 say the Proxmark's DOWNLINK is dead while its listening is perfect, and the
# leading account is the sandwich geometry. C305 says this bench cannot change the tag's
# protocol at all. Both are about to be tested by moving hardware — and M35 is explicit that
# against anything intermittent, A/B is not an experiment, A/B/A is. That only works if both
# halves are measured identically, which is what this script is for.
#
#   ./benchab.sh before        # with the sandwich still assembled, #2 still powered
#   ./benchab.sh nochamp2      # after powering down Chameleon #2, NOTHING ELSE MOVED
#   ./benchab.sh opened        # after taking the sandwich apart
#   ./benchab.sh restored      # after putting it back, if it goes back
#
# ⛔ RUN `nochamp2` BEFORE OPENING ANYTHING. Powering down #2 is one action and it is the whole
# C299 test; once the geometry changes that experiment is gone and cannot be recovered.
#
# Each run appends a block to bench-ab.log and prints it. Diff the blocks.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PM3=/Users/Shared/code/personal/rfid/proxmark3/pm3
PM3PORT=/dev/tty.usbmodemiceman1
CU="$HERE/../../software/script/.venv/bin/python $HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961
LOG="$HERE/bench-ab.log"
LABEL="${1:-unlabelled}"

say () { print -r -- "$@" | tee -a "$LOG"; }

say ""
say "=============================================================="
say "  BENCH A/B  —  $LABEL  —  $(date '+%Y-%m-%d %H:%M:%S')"
say "=============================================================="

say "devices enumerated: $(ls /dev/cu.usbmodem* 2>/dev/null | wc -l | tr -d ' ') of 4"
for p in /dev/cu.usbmodem*; do say "   $p"; done

# --- 1. pm3 LISTENING (known good even when the downlink is dead) -------------
say ""
say "-- pm3 lf search (listening only) --"
$PM3 -p $PM3PORT -c "lf search" 2>&1 | grep -iE "H10301|ind26|Valid HID|Chipset|No known" | sed 's/^/   /' | tee -a "$LOG"

# --- 2. pm3 DOWNLINK (C306: this is the symptom that matters) ----------------
say ""
say "-- pm3 lf t55xx detect + block reads (DOWNLINK — C306) --"
say "   ⭐ detect failing, and every block returning the SAME word, is the C306 signature."
$PM3 -p $PM3PORT -c "lf t55xx config --FSK2A --rate 50; lf t55xx detect; lf t55xx read -b 0; lf t55xx read -b 1" 2>&1 \
  | grep -iE "Could not detect|Chip type|Modulation|^\[\+\]  0[01] \|" | sed 's/^/   /' | tee -a "$LOG"

# --- 3. our reader ------------------------------------------------------------
say ""
say "-- our reader: lf hid prox read x4 --"
for i in 1 2 3 4; do
  r=$(eval $CU "\"hw connect -p $CH2\"" "\"lf hid prox read\"" 2>&1 | grep -E "FC:|CN:" | tr -d '\n ')
  say "   read $i: ${r:-NOTHING}"
done

# --- 4. our writer, data blocks (C305 says this is the ONE thing that works) ---
say ""
say "-- our writer, DATA blocks: write H10301 fc 77 cn 2468, then pm3 --"
eval $CU "\"hw connect -p $CH2\"" "\"lf hid prox write -f H10301 --fc 77 --cn 2468\"" >/dev/null 2>&1
$PM3 -p $PM3PORT -c "lf search" 2>&1 | grep -iE "H10301" | sed 's/^/   /' | tee -a "$LOG"
say "   ⇒ FC: 77  CN: 2468 means data-block writes still land."

# --- 5. our writer, BLOCK 0 (C305: fails, 0 of 9) -----------------------------
say ""
say "-- our writer, BLOCK 0: lf gproxii write, then pm3 --"
eval $CU "\"hw connect -p $CH2\"" "\"lf gproxii write --raw fac2a38c2b081af0210b12c2\"" 2>&1 | grep -iE "VERIFIED|CANNOT TELL|WRITE" | sed 's/^/   /' | tee -a "$LOG"
$PM3 -p $PM3PORT -c "lf search" 2>&1 | grep -iE "H10301|G Prox|Chipset" | sed 's/^/   /' | tee -a "$LOG"
say "   ⇒ still HID means block-0 writes STILL fail. A GProxII frame means C305 IS FIXED."

# --- 6. restore ---------------------------------------------------------------
say ""
say "-- restoring the bench credential --"
eval $CU "\"hw connect -p $CH2\"" "\"lf hid prox write -f H10301 --fc 123 --cn 4567\"" >/dev/null 2>&1
$PM3 -p $PM3PORT -c "lf search" 2>&1 | grep -iE "H10301|Valid HID" | sed 's/^/   /' | tee -a "$LOG"
say "   ⇒ must read FC: 123  CN: 4567 before you walk away."
say ""
