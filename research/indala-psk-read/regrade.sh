#!/bin/zsh
# ⭐ U15 — RE-GRADE A WRITE ARM AGAINST AN UNLOCKED TAG.
#
# Every write arm was scored while the tag was password-locked by our own writer (C325), so
# the pass/fail numbers recorded from 2026-09-15 onward say nothing about the writer. The
# password defect is fixed (F1) and the write path is confirmed restored (C329). This script
# re-measures one arm, honestly:
#
#   write with Chameleon #2  ->  read back with the Proxmark, an INDEPENDENT tool (M33)
#
# ⛔ The Proxmark is the judge, not our own reader, and not the writer's own say-so: a T5577
# acknowledges nothing, and `write done.` meant nothing for a whole session.
#
#   ./regrade.sh gallagher 4      # four independent write+read rounds
#   ./regrade.sh restore          # put H10301 FC 123 / CN 4567 back
#
# Each round writes afresh, so a pass is four writes that each landed — not one write read
# four times.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PM3=/Users/Shared/code/personal/rfid/proxmark3/pm3
PM3PORT=/dev/tty.usbmodemiceman1
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961
LOG="$HERE/regrade.log"

CHECK="lf search"
PROTO="${1:-}"
ROUNDS="${2:-4}"

# proto -> (write args, the string the Proxmark must print, what we wrote)
case "$PROTO" in
  awid)      WCMD="lf awid write --raw 011d81711dd1181111111111"; WANT="AWID";      PLAIN="raw 011d81711dd1181111111111" ;;
  keri)      WCMD="lf keri write --id 80003039";                  WANT="Keri";      PLAIN="id 80003039" ;;
  indala)    WCMD="lf indala write -r a0000000e6bd0e92";          WANT="Indala";    PLAIN="raw a0000000e6bd0e92" ;;
  nexwatch)  WCMD="lf nexwatch write --cn 87654321 -m 2 --magic quadrakey"; WANT="87654321"; PLAIN="cn 87654321 mode 2 quadrakey"; CHECK="lf nexwatch read" ;;
  gallagher) WCMD="lf gallagher write --raw 7feaa31e76d86c6d868cc249"; WANT="Gallagher"; PLAIN="raw 7feaa31e76d86c6d868cc249" ;;
  securakey) WCMD="lf securakey write --raw 7fcb400001adea5344300000"; WANT="Securakey"; PLAIN="raw 7fcb400001adea5344300000" ;;
  noralsy)   WCMD="lf noralsy write --raw bb0214ff0112402233670000"; WANT="Noralsy"; PLAIN="raw bb0214ff0112402233670000" ;;
  gproxii)   WCMD="lf gproxii write --raw fac2a38c2b081af0210b12c2"; WANT="G-Prox"; PLAIN="raw fac2a38c2b081af0210b12c2" ;;
  restore)
      print -r -- "-- restoring H10301 FC 123 / CN 4567 --" | tee -a "$LOG"
      "$PY" "$CU" "hw connect -p $CH2" "lf hid prox write -f H10301 --fc 123 --cn 4567" 2>&1 \
        | grep -iE "VERIFIED|CANNOT TELL|WRITE" | sed 's/^/   /' | tee -a "$LOG"
      $PM3 -p $PM3PORT -c "lf search" 2>&1 | grep -iE "H10301|Valid HID" | sed 's/^/   /' | tee -a "$LOG"
      exit 0 ;;
  *)  print -r -- "usage: ./regrade.sh <awid|keri|indala|nexwatch|gallagher|securakey|noralsy|gproxii|restore> [rounds]"
      exit 2 ;;
esac

say () { print -r -- "$@" | tee -a "$LOG"; }

say ""
say "=============================================================="
say "  U15 RE-GRADE  —  $PROTO  —  $(date '+%Y-%m-%d %H:%M:%S')"
say "=============================================================="
say "wrote with : $WCMD"
say "plaintext  : $PLAIN"
say "judged by  : pm3 $CHECK, looking for '$WANT'"

hits=0
for r in $(seq 1 $ROUNDS); do
  "$PY" "$CU" "hw connect -p $CH2" "$WCMD" >/dev/null 2>&1
  out=$($PM3 -p $PM3PORT -c "$CHECK" 2>&1)
  line=$(print -r -- "$out" | grep -iE "$WANT" | head -2 | tr '\n' ' ' | tr -s ' ')
  if [[ -n "$line" ]]; then
    hits=$((hits+1))
    say "   round $r: PASS — $line"
  else
    other=$(print -r -- "$out" | grep -iE "Valid .* found|No known 125|Chipset detection" | head -2 | tr '\n' ' ' | tr -s ' ')
    say "   round $r: FAIL — pm3 saw: ${other:-nothing}"
  fi
done

say ""
say "   ⇒ $PROTO WRITE: $hits of $ROUNDS"
say ""
