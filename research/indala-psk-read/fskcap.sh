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
# ⛔⛔ THE COUPLING CHECK USES EM410X, AND IT USED TO USE GALLAGHER. The control must be a pair
# that BOTH ends can do: #1 must emulate it AND #2 must read it. Gallagher satisfied the first
# (6/6, C378) and — as of C400 — NOT the second: `lf gallagher read` is 0 of 6 on a tag the
# Proxmark reads perfectly, along with Securakey, bounded to ASK RF/32 and RF/40.
# ⇒ A Gallagher control would have reported NOT COUPLED on a correctly coupled bench, blocking
# the very measurement this script exists for. EM410X is the right control precisely because it
# is the one arm proven at BOTH ends this session: #1 emulates it and #2 reads it 2 of 2, on the
# SAME GPIO/comparator path the broken ASK arms use.
# ⚠ A null here still means NOT COUPLED rather than "the emitter is broken" — that is the
# distinction a silent reader destroys, and it cost this branch a whole unit (C373/C374/C376).
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
E[em410x]="lf em 410x econfig -s $SLOT --id DEADBEEF88"
TYPE[em410x]="EM410X";        EXPECT[em410x]="64,64"
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
print -r -- "  ⛔⛔ THE ANALYSIS BELOW IS NOT VALID — tonehist.py measures noise, not tones (C401)."
print -r -- "     It prints U11's signature on an EM410X emission the receiver DECODES correctly."
print -r -- "     The CAPTURES are good and worth taking; the histograms are not evidence."
print -r -- "     ⇒ Rebuild the analysis on askdemod.py / ctest cdemod before scoring anything."
print -r -- ""
print -r -- "  U11 capture — #1 emulates, #2 listens through our own reader"
print -r -- "  ----------------------------------------------------------"

# ⛔ STEP 1, AND THE RUN STOPS HERE IF IT FAILS.
start_emu em410x
o=$(cap "hw mode -r" "lf em 410x read")
if ! print -r -- "$o" | grep -qi "EM410X"; then
  emu "hw mode -r" >/dev/null
  print -r -- "  ⛔⛔ NOT COUPLED — #2 cannot hear #1 emulating EM410X, proven at BOTH ends."
  print -r -- "     That is a BENCH state, not a firmware result: the two Chameleons are not"
  print -r -- "     facing each other yet (AUTOPILOT.md §5). Nothing below would mean anything."
  exit 1
fi
print -r -- "  ✓ coupled — #2 read #1's EM410X emulation"

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
