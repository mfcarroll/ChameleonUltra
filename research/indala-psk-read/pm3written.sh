#!/bin/zsh
# ⭐⭐ EVERY READ ARM AGAINST A TAG THE PROXMARK WROTE — closing the self-certification hole.
#
# ⛔ WHY. `nullmatrix.sh` and `capcost.sh` both write the tag with OUR writer. So does most of
# the write-column evidence. A reader and a writer that share a wrong convention certify each
# other perfectly — that is exactly the evidence C185 refused to accept for FDX-A, and the
# argument does not stop being true for the other twelve protocols.
#
# ⇒ The Proxmark writes the tag from ITS OWN encoder, and our reader is asked. A pass means
#   two independently written encoders agree on what the credential looks like on the wire.
#
#   ./pm3written.sh              # all of them
#   ./pm3written.sh keri gproxii
#
# ⚠ The expected string is what OUR reader prints, chosen to match the credential pm3 was
#   asked for. A mismatch is informative either way and the actual output is printed.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961
PM3=/Users/Shared/code/personal/rfid/proxmark3/pm3
PM3PORT=/dev/tty.usbmodemiceman1

typeset -A CLONE READ WANT
CLONE[indala]="lf indala clone -r a0000000e6bd0e92";        READ[indala]="lf indala read";        WANT[indala]="a0000000e6bd0e92"
CLONE[idteck]="lf idteck clone -r 4944544B55667788";        READ[idteck]="lf idteck read";        WANT[idteck]="4944544b55667788"
CLONE[keri]="lf keri clone -t i --cn 12345";                READ[keri]="lf keri read";            WANT[keri]="12345"
CLONE[nexwatch]="lf nexwatch clone -r 560000000012776A2F207B00"; READ[nexwatch]="lf nexwatch read"; WANT[nexwatch]="87654321"
CLONE[gallagher]="lf gallagher clone -r 7FEAA31E76D86C6D868CC249"; READ[gallagher]="lf gallagher read"; WANT[gallagher]="7feaa31e76d86c6d868cc249"
CLONE[securakey]="lf securakey clone -r 7FCB400001ADEA5344300000"; READ[securakey]="lf securakey read"; WANT[securakey]="7fcb400001adea5344300000"
CLONE[noralsy]="lf noralsy clone --cn 112233";              READ[noralsy]="lf noralsy read";      WANT[noralsy]="112233"
CLONE[awid]="lf awid clone --fmt 26 --fc 12 --cn 3456";     READ[awid]="lf awid read";            WANT[awid]="011d81711dd1181111111111"
CLONE[paradox]="lf paradox clone --fc 96 --cn 40426";       READ[paradox]="lf paradox read";      WANT[paradox]="0f55555695596a6a9999a59a"
CLONE[pyramid]="lf pyramid clone --fc 123 --cn 11223";      READ[pyramid]="lf pyramid read";      WANT[pyramid]="00010101010101010101016eb35e5da4"
CLONE[gproxii]="lf gproxii clone --xor 141 --fmt 26 --fc 123 --cn 1337"; READ[gproxii]="lf gproxii read"; WANT[gproxii]="fac2a38c2b081af0210b12c2"
CLONE[fdxa]="lf destron clone --uid 1A2B3C4D5E";            READ[fdxa]="lf fdxa read";            WANT[fdxa]="1aabbccd5e"
CLONE[fdxb]="lf fdxb clone -c 999 -n 1337";                 READ[fdxb]="lf fdxb read";            WANT[fdxb]="1337"

# ⛔⛔ THE SIX BELOW ARE THE ONES THAT ALREADY EXIST UPSTREAM, and leaving them out was the
# same scoping mistake `nullmatrix.sh` made: scope to the protocols this branch ADDED and the
# ones it only SHARES A CAPTURE ENGINE WITH go unchecked. If F5's buffer merge or F7's
# corroboration change regressed any of them, that is a regression in code a maintainer
# already ships — the most expensive kind to hand someone.
# ⚠ Every expected string here was OBSERVED first, not guessed. Guessed match strings have
# produced three phantom failures in this session.
CLONE[hidprox]="lf hid clone -w H10301 --fc 123 --cn 4567"; READ[hidprox]="lf hid prox read -f H10301"; WANT[hidprox]="CN: 4567"
CLONE[ioprox]="lf io clone --vn 1 --fc 83 --cn 1337";       READ[ioprox]="lf ioprox read";        WANT[ioprox]="007854E03059CDF7"
CLONE[em410x]="lf em 410x clone --id 1234567890";           READ[em410x]="lf em 410x read";       WANT[em410x]="1234567890"
CLONE[viking]="lf viking clone --cn 1A337F9C";              READ[viking]="lf viking read";        WANT[viking]="1a337f9c"
CLONE[jablotron]="lf jablotron clone --cn 1234567890";      READ[jablotron]="lf jablotron read";  WANT[jablotron]="1234567890"
CLONE[pac]="lf pac clone -r FF2049906D8541C9511C1B06C1B46551"; READ[pac]="lf pac read";           WANT[pac]="CARD0001"

all=(indala idteck keri nexwatch gallagher securakey noralsy awid paradox pyramid gproxii fdxa fdxb
     hidprox ioprox em410x viking jablotron pac)
if (( $# )); then protos=("$@"); else protos=($all); fi

READS="${READS:-4}"
pass=0; total=0
print -r -- ""
print -r -- "  our READER against a tag the PROXMARK wrote — two independent encoders"
print -r -- "  --------------------------------------------------------------------"
for p in $protos; do
  $PM3 -p $PM3PORT -c "${CLONE[$p]}" >/dev/null 2>&1
  ok=0
  for i in $(seq 1 $READS); do
    out=$("$PY" "$CU" "hw connect -p $CH2" "${READ[$p]}" 2>&1)
    print -r -- "$out" | grep -qi "${WANT[$p]}" && ok=$((ok+1))
  done
  total=$((total+READS)); pass=$((pass+ok))
  if (( ok == READS )); then
    printf "  %-11s %d/%d  ✓  matched %s\n" "$p" "$ok" "$READS" "${WANT[$p]}"
  else
    printf "  %-11s %d/%d  ⛔ expected %s, last read said: %s\n" "$p" "$ok" "$READS" "${WANT[$p]}" \
      "$(print -r -- "$out" | grep -iE 'Raw|Card|Internal|Country|not found' | head -1 | tr -s ' ')"
  fi
done
print -r -- ""
printf "  ⇒ %d of %d reads of Proxmark-written tags matched\n" "$pass" "$total"
print -r -- "  -- restoring the bench credential --"
"$PY" "$CU" "hw connect -p $CH2" "lf hid prox write -f H10301 --fc 123 --cn 4567" 2>&1 | grep -iE "VERIFIED|CANNOT TELL|WRITE" | sed 's/^/     /'
