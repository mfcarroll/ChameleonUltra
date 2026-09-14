#!/bin/zsh
# ⭐⭐⭐ U11'S MISSING MEASUREMENT, IN ONE COMMAND — for the moment the bench changes.
#
#   ./fskcap.sh                 # coupling check, then capture every arm
#   ./fskcap.sh gallagher       # one arm
#
# ⛔ NEEDS THE TWO CHAMELEONS FACING EACH OTHER. #1 emulates, #2 listens. That is the ONLY
# bench change required (AUTOPILOT.md §5); #1 stays where it is and the T5577 and Proxmark are
# not used. Until then this aborts on the coupling check rather than producing numbers.
#
# ⭐ WHY IT EXISTS. Every FSK number on this branch came through the FLIPPER's raw reader — a
# black box reporting edge timings after its own comparator and AGC. `rdrcap.py` returns OUR OWN
# undecoded SAADC samples at a rate we set, one per carrier cycle, so an RF/8 tone is 8 samples
# and RF/10 is 10 with nothing to calibrate. That separates the two live readings of C387:
#     the PWM output is clean and the antenna cannot follow the tone change
#     vs the modulation itself never changes period.
#
# ⛔ THE COUPLING CHECK USES GALLAGHER ON PURPOSE. It emulates 6/6 (C378), so a null there means
# NOT COUPLED rather than "the emitter is broken" — the distinction a silent reader destroys and
# that cost this branch a whole unit (C373/C374/C376). Never score an arm without it.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
EMU="${EMU:-/dev/tty.usbmodemC3A1656543DE1}"     # #1 — emulates
CAP="${CAP:-/dev/tty.usbmodemF429364E46961}"     # #2 — listens
SLOT="${SLOT:-8}"
SAMPLES="${SAMPLES:-14336}"
DRIVE="${DRIVE:-7}"
OUT="${OUT:-/tmp}"

emu () { "$PY" "$CU" "hw connect -p $EMU" "$@" 2>&1 }
cap () { "$PY" "$CU" "hw connect -p $CAP" "$@" 2>&1 }

typeset -A E TYPE EXPECT
E[gallagher]="lf gallagher econfig -s $SLOT --raw 7feaa31e76d86c6d868cc249"
TYPE[gallagher]="Gallagher";  EXPECT[gallagher]="32,32"
E[hidprox]="lf hid prox econfig -s $SLOT -f H10301 --fc 123 --cn 4567"
TYPE[hidprox]="HIDProx";      EXPECT[hidprox]="8,10"
E[awid]="lf awid econfig -s $SLOT --raw 011d81711dd1181111111111"
TYPE[awid]="AWID";            EXPECT[awid]="8,10"
E[ioprox]="lf ioprox econfig -s $SLOT --ver 1 --fc 83 --cn 1337"
TYPE[ioprox]="ioProx";        EXPECT[ioprox]="8,11"

start_emu () {   # $1 = protocol
  emu "hw slot type -s $SLOT -t ${TYPE[$1]}" "hw slot enable -s $SLOT --lf" \
      "${E[$1]}" "hw slot change -s $SLOT" "hw mode -e" >/dev/null
  sleep 1
}

print -r -- ""
print -r -- "  U11 capture — #1 emulates, #2 listens through our own reader"
print -r -- "  ----------------------------------------------------------"

# ⛔ STEP 1, AND THE RUN STOPS HERE IF IT FAILS.
start_emu gallagher
o=$(cap "hw mode -r" "lf gallagher read")
if ! print -r -- "$o" | grep -qi "Gallagher"; then
  emu "hw mode -r" >/dev/null
  print -r -- "  ⛔⛔ NOT COUPLED — #2 cannot hear #1 emulating Gallagher, which emulates 6/6."
  print -r -- "     That is a BENCH state, not a firmware result: the two Chameleons are not"
  print -r -- "     facing each other yet (AUTOPILOT.md §5). Nothing below would mean anything."
  exit 1
fi
print -r -- "  ✓ coupled — #2 read #1's Gallagher emulation"

protos=("${@:-hidprox awid ioprox gallagher}")
if (( $# )); then protos=("$@"); else protos=(hidprox awid ioprox gallagher); fi

for p in $protos; do
  [[ -z "${E[$p]:-}" ]] && { print -r -- "  $p: no econfig entry"; continue; }
  start_emu $p
  f="$OUT/fskcap_$p.bin"
  "$PY" "$HERE/rdrcap.py" --port "$CAP" --out "$f" --samples "$SAMPLES" --drive "$DRIVE" >/dev/null 2>&1
  print -r -- ""
  print -r -- "  --- $p (expect tones ${EXPECT[$p]} samples) ---"
  "$PY" "$HERE/tonehist.py" "$f" --expect "${EXPECT[$p]}"
done

emu "hw mode -r" >/dev/null
print -r -- ""
print -r -- "  ⚠ #1 left in READER mode. Slot $SLOT holds the last protocol captured."
