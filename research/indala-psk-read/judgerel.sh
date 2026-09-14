#!/bin/zsh
# ⭐⭐ HOW RELIABLE IS THE JUDGE? Every write verdict on this branch is the Proxmark's answer to
# ONE question, so whatever miss rate that reader has, the verdict inherits (M44).
#
# ⛔ This is not hypothetical. C346 measured `lf fdxb reader` missing 12% of asks on a tag it
# had just confirmed — 53 of 60 — where HID, GProxII and AWID were 60 of 60. One unretried ask
# reported a landed write as `24 of 25` and nearly earned FDX-B a phantom writer defect.
# Four judges were measured there. The other fifteen carry the rest of the write column.
#
#   ./judgerel.sh              # every protocol
#   N=30 ./judgerel.sh fdxb
#
# ⚠ The tag is written ONCE per protocol and then only ASKED, so a miss is the judge's and
# cannot be a write that never landed.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961
PM3=/Users/Shared/code/personal/rfid/proxmark3/pm3
PM3PORT=/dev/tty.usbmodemiceman1
N="${N:-15}"

typeset -A W ASK WANT
W[indala]="lf indala write -r a0000000e6bd0e92";            ASK[indala]="lf search";        WANT[indala]="a0000000e6bd0e92"
W[indala224]="lf indala write --224 -r 80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5"; ASK[indala224]="lf search"; WANT[indala224]="len 224"
W[idteck]="lf idteck write --id 4944544b55667788";          ASK[idteck]="lf search";        WANT[idteck]="Idteck"
W[keri]="lf keri write --id 80003039";                      ASK[keri]="lf search";          WANT[keri]="KERI"
W[nexwatch]="lf nexwatch write --cn 87654321 -m 2 --magic quadrakey"; ASK[nexwatch]="lf nexwatch read"; WANT[nexwatch]="87654321"
W[gallagher]="lf gallagher write --raw 7feaa31e76d86c6d868cc249"; ASK[gallagher]="lf search"; WANT[gallagher]="GALLAGHER"
W[securakey]="lf securakey write --raw 7fcb400001adea5344300000"; ASK[securakey]="lf search"; WANT[securakey]="Securakey"
W[noralsy]="lf noralsy write --raw bb0214ff0112402233670000"; ASK[noralsy]="lf search";      WANT[noralsy]="Noralsy"
W[awid]="lf awid write --raw 011d81711dd1181111111111";     ASK[awid]="lf search";          WANT[awid]="AWID"
W[paradox]="lf paradox write --raw 0f55555695596a6a9999a59a"; ASK[paradox]="lf search";      WANT[paradox]="Paradox"
W[pyramid]="lf pyramid write --raw 00010101010101010101016eb35e5da4"; ASK[pyramid]="lf search"; WANT[pyramid]="Pyramid"
W[fdxa]="lf fdxa write --raw 551d5699999a9aa5a5a666a9";     ASK[fdxa]="lf search";          WANT[fdxa]="Destron"
W[gproxii]="lf gproxii write --raw fac2a38c2b081af0210b12c2"; ASK[gproxii]="lf search";      WANT[gproxii]="G-Prox"
W[fdxb]="lf fdxb write --raw 00339a080402079f8040797788040201"; ASK[fdxb]="lf fdxb reader";  WANT[fdxb]="999-000000001337"
W[hidprox]="lf hid prox write -f H10301 --fc 123 --cn 4567"; ASK[hidprox]="lf search";       WANT[hidprox]="FC: 123"
W[em410x]="lf em 410x write --id 1234567890";               ASK[em410x]="lf search";        WANT[em410x]="1234567890"
W[ioprox]="lf ioprox write --ver 1 --fc 83 --cn 1337";      ASK[ioprox]="lf search";        WANT[ioprox]="IO Prox"
W[pac]="lf pac write --cn CARD0001";                        ASK[pac]="lf search";           WANT[pac]="CARD0001"
W[viking]="lf viking write --id 1a337f9c";                  ASK[viking]="lf search";        WANT[viking]="Viking"
W[jablotron]="lf jablotron write --id 1234567890";          ASK[jablotron]="lf search";     WANT[jablotron]="Jablotron"

allp=(indala indala224 idteck keri nexwatch gallagher securakey noralsy awid paradox
      pyramid fdxa gproxii fdxb hidprox em410x ioprox pac viking jablotron)
if (( $# )); then protos=("$@"); else protos=($allp); fi

print -r -- ""
print -r -- "  the JUDGE's own read reliability — tag written once, then only asked"
print -r -- "  --------------------------------------------------------------------"
weak=()
for p in $protos; do
  "$PY" "$CU" "hw connect -p $CH2" "${W[$p]}" >/dev/null 2>&1
  ok=0
  for i in $(seq 1 $N); do
    $PM3 -p $PM3PORT -c "${ASK[$p]}" 2>&1 | grep -qiE "${WANT[$p]}" && ok=$((ok+1))
  done
  pct=$(( 100.0 * ok / N ))
  mark=""; (( ok < N )) && { mark="   ⛔ single-ask verdicts unreliable"; weak+=("$p"); }
  printf "  %-11s %2d/%-2d  %5.1f%%%s\n" "$p" "$ok" "$N" "$pct" "$mark"
done
print -r -- ""
if (( ${#weak[@]} )); then
  print -r -- "  ⇒ judges needing a retry (M44): ${weak[*]}"
else
  print -r -- "  ⇒ every judge answered $N of $N"
fi
print -r -- "  -- restoring the bench credential --"
"$PY" "$CU" "hw connect -p $CH2" "lf hid prox write -f H10301 --fc 123 --cn 4567" 2>&1 | grep -iE "VERIFIED|CANNOT TELL|WRITE" | sed 's/^/     /'
