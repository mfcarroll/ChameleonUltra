#!/bin/zsh
# ⭐⭐⭐ THE EMULATE COLUMN, RE-GRADED AGAINST THE PROXMARK (C472).
#
# ⛔⛔ WHY THIS EXISTS AND WHY `emugrade.sh` IS NOT ENOUGH. Every emulate grade this project has
# ever published was scored by the FLIPPER, and C472 showed that grade can measure the JUDGE:
# three FSK2a arms graded 0/6 by the Flipper are decoded BYTE-EXACT by the Proxmark, on two
# devices and two firmware builds. C465 had already caught the same instrument decoding nothing
# from a REAL PAC tag the pm3 read byte-exact. ⇒ every SILENT verdict in the grid is suspect
# until a second reader has been asked, and this asks it.
#
# ⛔ `emugrade.sh`'s header claims "the Proxmark cannot hear our emulation at all — it is PWM on
# the coil, not load modulation". That is FALSE and was refuted by C466 before this script
# existed: the pm3 reads our EM410X emulation byte-exact. It is the reason the emulate column
# was never pointed at the better front end.
#
#   ./pm3grade.sh                  # every arm
#   ./pm3grade.sh hidprox pac      # named arms only
#
# ⭐ BENCH: ONE Chameleon flat on the Proxmark's antenna, NOTHING between them, no tag nearby.
# ⛔ Set EMU to the device you actually put there. A wrong-device run is the C461 trap and the
# null arms below will NOT catch it — they catch a stray emitter, not a swapped one. C472 had
# to establish identity by radio (distinct EM410X ids) after a verbal mix-up.
#
# ⭐ DISCIPLINE, inherited from C328/M35 and kept:
#   1. NULL FIRST — the Chameleon in reader mode, every pm3 decoder run. A hit here is ambient
#      and everything below it is worthless.
#   2. Each arm scored BYTE-EXACT against what was armed, never "a decode happened".
#   3. NULL AGAIN at the end. A/B/A, because against anything intermittent A/B is not an
#      experiment.
# ⚠ A SILENT arm here is NOT automatically a broken emitter — it may be a pm3 decoder gap. That
# is the whole lesson of C472 with the instruments swapped. Report it as "pm3 silent", and only
# a THIRD instrument or a real tag can promote that to a statement about the emitter.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
PM3="${PM3:-/Users/Shared/code/personal/rfid/proxmark3/pm3}"
EMU="${EMU:-/dev/tty.usbmodemF429364E46961}"     # #2 by default
SLOT="${SLOT:-8}"
cu () { "$PY" "$CU" -p "$EMU" "$@" 2>&1 }

typeset -A TYPE E CMD EXPECT
#            slot type        econfig args                                                  pm3 reader            byte-exact expectation
TYPE[em410x]=EM410X;      E[em410x]="lf em 410x econfig -s $SLOT --id 2244668800";      CMD[em410x]="lf em 410x reader";   EXPECT[em410x]="2244668800"
TYPE[pac]=PAC;            E[pac]="lf pac econfig -s $SLOT --cn CARD0042";               CMD[pac]="lf pac reader";          EXPECT[pac]="CARD0042"
TYPE[viking]=Viking;      E[viking]="lf viking econfig -s $SLOT --id 1a337195";         CMD[viking]="lf viking reader";    EXPECT[viking]="1A337195"
TYPE[jablotron]=Jablotron; E[jablotron]="lf jablotron econfig -s $SLOT --id 8899aabbcc"; CMD[jablotron]="lf jablotron reader"; EXPECT[jablotron]="8899AABBCC"
TYPE[hidprox]=HIDProx;    E[hidprox]="lf hid prox econfig -s $SLOT -f H10301 --fc 123 --cn 4567"; CMD[hidprox]="lf hid reader"; EXPECT[hidprox]="FC: 123  CN: 4567"
TYPE[ioprox]=ioProx;      E[ioprox]="lf ioprox econfig -s $SLOT --ver 1 --fc 83 --cn 1337"; CMD[ioprox]="lf io reader";    EXPECT[ioprox]="XSF(01)53:01337"
TYPE[awid]=AWID;          E[awid]="lf awid econfig -s $SLOT --raw 011d81711dd1181111111111"; CMD[awid]="lf awid reader";   EXPECT[awid]="011d81711dd1181111111111"
TYPE[indala]=Indala;      E[indala]="lf indala econfig -s $SLOT --id a0000000e6bd0e92"; CMD[indala]="lf indala reader";    EXPECT[indala]="a0000000e6bd0e92"
TYPE[keri]=Keri;          E[keri]="lf keri econfig -s $SLOT --id 80003039";             CMD[keri]="lf keri reader";        EXPECT[keri]="80003039"
TYPE[nexwatch]=NexWatch;  E[nexwatch]="lf nexwatch econfig -s $SLOT --cn 87654321 -m 2"; CMD[nexwatch]="lf nexwatch reader"; EXPECT[nexwatch]="87654321"
TYPE[idteck]=IDTECK;      E[idteck]="lf idteck econfig -s $SLOT --id 4944544b55667788"; CMD[idteck]="lf idteck reader";    EXPECT[idteck]="4944544B55667788"
TYPE[gallagher]=Gallagher; E[gallagher]="lf gallagher econfig -s $SLOT --raw 7feaa31e76d86c6d868cc249"; CMD[gallagher]="lf gallagher reader"; EXPECT[gallagher]="7feaa31e76d86c6d868cc249"
TYPE[securakey]=Securakey; E[securakey]="lf securakey econfig -s $SLOT --raw 7fcb400001adea5344300000"; CMD[securakey]="lf securakey reader"; EXPECT[securakey]="7fcb400001adea5344300000"
TYPE[noralsy]=Noralsy;    E[noralsy]="lf noralsy econfig -s $SLOT --raw bb0214ff0112402233670000"; CMD[noralsy]="lf noralsy reader"; EXPECT[noralsy]="bb0214ff0112402233670000"
TYPE[gproxii]=GProxII;    E[gproxii]="lf gproxii econfig -s $SLOT --raw fac2a38c2b081af0210b12c2"; CMD[gproxii]="lf gproxii reader"; EXPECT[gproxii]="fac2a38c2b081af0210b12c2"
TYPE[fdxb]=FDXB;          E[fdxb]="lf fdxb econfig -s $SLOT --raw 00339a080402079f8040797788040201"; CMD[fdxb]="lf fdxb reader"; EXPECT[fdxb]="00339a080402079f8040797788040201"

ORDER=(em410x viking jablotron pac hidprox ioprox awid indala keri nexwatch idteck gallagher securakey noralsy gproxii fdxb)
(( $# )) && ORDER=("$@")

null_sweep () {
  local label="$1" hits=0 p out
  print -r -- "  $label — Chameleon in reader mode, every decoder asked:"
  cu "hw mode -r" >/dev/null 2>&1
  sleep 1
  for p in $ORDER; do
    out="$("$PM3" -c "${CMD[$p]}" 2>&1)"
    if print -r -- "$out" | grep -qiF -- "${EXPECT[$p]}"; then
      print -r -- "    ⛔ $p: A HIT WITH NOTHING EMULATING — ambient contamination"
      (( hits++ ))
    fi
  done
  (( hits == 0 )) && print -r -- "    ✓ clean — $#ORDER decoders, no hits"
  return $hits
}

print -r -- "pm3 re-grade of the emulate column — emulator $EMU, slot $SLOT"
print -r -- ""
null_sweep "NULL BEFORE" || { print -r -- "⛔ ABORTING: the field is not clean."; exit 1 }
print -r -- ""

typeset -a PASS SILENT REFUSED
for p in $ORDER; do
  err="$(cu "hw slot type -s $SLOT -t ${TYPE[$p]}" "hw slot enable -s $SLOT --lf" 2>&1 | grep -i "error\|invalid")"
  if [[ -n "$err" ]]; then print -r -- "  ⛔ $p: slot type REFUSED — ${err:0:70}"; REFUSED+=$p; continue; fi
  err="$(cu "${E[$p]}" 2>&1 | grep -i "error\|invalid\|unrecognized")"
  if [[ -n "$err" ]]; then print -r -- "  ⛔ $p: econfig REFUSED — ${err:0:70}"; REFUSED+=$p; continue; fi
  cu "hw slot change -s $SLOT" "hw mode -e" >/dev/null 2>&1
  sleep 1
  out="$("$PM3" -c "${CMD[$p]}" 2>&1)"
  if print -r -- "$out" | grep -qiF -- "${EXPECT[$p]}"; then
    print -r -- "  ✅ $p: BYTE-EXACT — ${EXPECT[$p]}"
    PASS+=$p
  else
    print -r -- "  ⚠  $p: pm3 SILENT (expected ${EXPECT[$p]})"
    SILENT+=$p
  fi
done

print -r -- ""
null_sweep "NULL AFTER"
print -r -- ""
print -r -- "  PASS    (${#PASS}): ${PASS}"
print -r -- "  SILENT  (${#SILENT}): ${SILENT}"
print -r -- "  REFUSED (${#REFUSED}): ${REFUSED}"
print -r -- ""
print -r -- "  ⚠ A SILENT arm is a PROBE result. It may be a pm3 decoder gap, not a broken"
print -r -- "     emitter — that is C472 with the instruments swapped. Do not promote it to a"
print -r -- "     statement about the emitter without a third reader or a real tag."
cu "hw mode -r" >/dev/null 2>&1
