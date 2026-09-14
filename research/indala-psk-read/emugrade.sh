#!/bin/zsh
# ⭐⭐ THE EMULATE COLUMN, GRADED IN ONE COMMAND — ready for the next hands session.
#
# ⛔ THIS NEEDS A BENCH CHANGE AND WILL NOT WORK WITHOUT IT: Chameleon **#2** must be on the
# FLIPPER's pad, and the T5577 out. The Proxmark cannot hear our emulation at all — it is PWM
# on the coil, not load modulation — so the Flipper is the only reader for rig A. That is why
# the emulate column is the one part of the grid this run could not touch.
#
#   ./emugrade.sh                 # every arm with an econfig command
#   ./emugrade.sh indala gproxii
#
# ⭐ It encodes C328's discipline rather than just looping:
#   1. NULL FIRST — #2 in reader mode, so it is emulating nothing. A hit here is ambient and
#      every number below it would be worthless.
#   2. Each arm reads with `--mode both`. The WRONG-modulation arm is a built-in control (C83):
#      a PSK1 protocol must score on psk and 0 on ask, and vice versa. An arm that hits on
#      BOTH is not a pass — EM410X did exactly that and remains unexplained.
#   3. NULL AGAIN at the end. A/B/A, because against anything intermittent A/B is not an
#      experiment (M35).
#
# ⛔ THE econfig SIGNATURES ARE NOT THE write SIGNATURES, which cost two wrong entries here:
#    `lf indala econfig` takes `--id`, not the `-r` its writer uses, and `lf nexwatch econfig`
#    has no `--magic` at all. Both were caught by running one command rather than by reading.
#
# ⚠⚠ PLUMBING SMOKE-TESTED 2026-09-14, RESULTS NEVER TAKEN. #2 was on the Proxmark when this
#    was written, so the commands are known to parse and run but no arm has been scored with
#    it. Treat the first run as the experiment, not as a regression check.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961
SLOT="${SLOT:-8}"          # a scratch slot, so nothing anyone curated is overwritten
ATTEMPTS="${ATTEMPTS:-6}"
cu () { "$PY" "$CU" "hw connect -p $CH2" "$@" 2>&1 }
fread () { "$PY" "$HERE/flipper.py" read --mode both --attempts "$ATTEMPTS" 2>&1 }

# protocol -> econfig args | expected modulation arm
typeset -A E MOD
E[indala]="lf indala econfig -s $SLOT --id a0000000e6bd0e92";                 MOD[indala]=psk
E[idteck]="lf idteck econfig -s $SLOT --id 4944544b55667788";               MOD[idteck]=psk
E[keri]="lf keri econfig -s $SLOT --id 80003039";                           MOD[keri]=psk
E[nexwatch]="lf nexwatch econfig -s $SLOT --cn 87654321 -m 2"; MOD[nexwatch]=psk
E[gallagher]="lf gallagher econfig -s $SLOT --raw 7feaa31e76d86c6d868cc249"; MOD[gallagher]=ask
E[securakey]="lf securakey econfig -s $SLOT --raw 7fcb400001adea5344300000"; MOD[securakey]=ask
E[noralsy]="lf noralsy econfig -s $SLOT --raw bb0214ff0112402233670000";     MOD[noralsy]=ask
E[hidprox]="lf hid prox econfig -s $SLOT -f H10301 --fc 123 --cn 4567";      MOD[hidprox]=ask
E[ioprox]="lf ioprox econfig -s $SLOT --ver 1 --fc 83 --cn 1337";            MOD[ioprox]=ask
E[awid]="lf awid econfig -s $SLOT --raw 011d81711dd1181111111111";           MOD[awid]=ask
E[gproxii]="lf gproxii econfig -s $SLOT --raw fac2a38c2b081af0210b12c2";     MOD[gproxii]=ask

allp=(indala idteck keri nexwatch gallagher securakey noralsy hidprox ioprox awid gproxii)
if (( $# )); then protos=("$@"); else protos=($allp); fi

null_check () {
  cu "hw mode -r" >/dev/null
  local o=$(fread)
  if print -r -- "$o" | grep -qiE "psk [1-9]|ask [1-9]"; then
    print -r -- "  ⛔ NULL $1 FAILED — the Flipper read something with #2 emulating nothing:"
    print -r -- "$o" | sed 's/^/       /' | head -4
    return 1
  fi
  print -r -- "  ✓ null $1 clean — #2 in reader mode, Flipper reads nothing"
  return 0
}

print -r -- ""
print -r -- "  emulate arms, Flipper as reader, scratch slot $SLOT"
print -r -- "  --------------------------------------------------"
null_check "before" || { print -r -- "  ⇒ ABORTING: an ambient hit makes every arm below meaningless"; exit 1; }

for p in $protos; do
  [[ -z "${E[$p]:-}" ]] && { print -r -- "  $p: no econfig entry"; continue; }
  err=$(cu "${E[$p]}" | grep -iE "error|usage|invalid|unrecognized|Traceback" | head -1)
  if [[ -n "$err" ]]; then print -r -- "  ⛔ $p: econfig REFUSED — $err"; continue; fi
  o=$(fread)
  psk=$(print -r -- "$o" | grep -oiE "psk [0-9]+/[0-9]+" | head -1)
  ask=$(print -r -- "$o" | grep -oiE "ask [0-9]+/[0-9]+" | head -1)
  printf "  %-10s expected %-3s   %-12s %-12s\n" "$p" "${MOD[$p]}" "${psk:-psk -}" "${ask:-ask -}"
done

null_check "after"
cu "hw mode -r" >/dev/null
print -r -- ""
print -r -- "  ⚠ #2 left in READER mode. Slot $SLOT now holds the last protocol tested."
