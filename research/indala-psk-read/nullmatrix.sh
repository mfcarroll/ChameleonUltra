#!/bin/zsh
# ⭐⭐ THE CROSS-PROTOCOL NULL, AGAINST REAL TAGS — which was never possible before.
#
# METHOD.md is explicit that a format is not done until its cross-protocol nulls pass, and
# every null this branch has run is against COMMITTED CAPTURES: host-side, one decoder fed
# another protocol's samples. That is the right test and it is not the same test. A real tag
# is actual tag behaviour — the operator's C179 distinction — and until every write arm worked
# there was no way to put fourteen different real tags in front of fourteen readers.
#
# ⇒ For each protocol: write its credential to the T5577, then ask EVERY OTHER reader. Any
#   reader but the right one returning a credential is a FALSE POSITIVE on a real tag.
#
#   ./nullmatrix.sh                    # all of them
#   ./nullmatrix.sh fdxa gproxii       # just these as the WRITTEN tag
#
# ⛔ FDX-A is the one that has never had this test at all: there was no FDX-A writer until
#   C340, so no FDX-A tag has ever been held in front of the other thirteen readers.
# ⛔ A reader that prints nothing is the PASS here. Hits are printed with what they claimed.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961

typeset -A W R
W[indala]="lf indala write -r a0000000e6bd0e92";                          R[indala]="lf indala read"
W[indala224]="lf indala write --224 -r 80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5"; R[indala224]="lf indala read --224"
W[idteck]="lf idteck write --id 4944544b55667788";                        R[idteck]="lf idteck read"
W[keri]="lf keri write --id 80003039";                                    R[keri]="lf keri read"
W[nexwatch]="lf nexwatch write --cn 87654321 -m 2 --magic quadrakey";     R[nexwatch]="lf nexwatch read"
W[gallagher]="lf gallagher write --raw 7feaa31e76d86c6d868cc249";         R[gallagher]="lf gallagher read"
W[securakey]="lf securakey write --raw 7fcb400001adea5344300000";         R[securakey]="lf securakey read"
W[noralsy]="lf noralsy write --raw bb0214ff0112402233670000";             R[noralsy]="lf noralsy read"
W[awid]="lf awid write --raw 011d81711dd1181111111111";                   R[awid]="lf awid read"
W[paradox]="lf paradox write --raw 0f55555695596a6a9999a59a";             R[paradox]="lf paradox read"
W[pyramid]="lf pyramid write --raw 00010101010101010101016eb35e5da4";     R[pyramid]="lf pyramid read"
W[fdxa]="lf fdxa write --raw 551d5699999a9aa5a5a666a9";                   R[fdxa]="lf fdxa read"
W[gproxii]="lf gproxii write --raw fac2a38c2b081af0210b12c2";             R[gproxii]="lf gproxii read"
W[fdxb]="lf fdxb write --raw 00339a080402079f8040797788040201";           R[fdxb]="lf fdxb read"

# ⛔⛔ THE READER LIST IS NOT THE TAG LIST, and conflating them left a real gap. The first
# version asked only the fourteen SAMPLED-path readers, so the GPIO/comparator readers
# (em410x, Viking, Jablotron, PAC) and HID/ioProx were never asked whether they false-positive
# on any of these tags — the untested direction, and the one where a cross-family false
# positive would actually live. Readers are now every `lf * read` the CLI has.
R[em410x]="lf em 410x read"
R[hidprox]="lf hid prox read -f H10301"
R[ioprox]="lf ioprox read"
R[pac]="lf pac read"
R[viking]="lf viking read"
R[jablotron]="lf jablotron read"
R[instafob]="lf instafob read"

tags=(indala indala224 idteck keri nexwatch gallagher securakey noralsy
      awid paradox pyramid fdxa gproxii fdxb)
readers=($tags em410x hidprox ioprox pac viking jablotron instafob)
if (( $# )); then written=("$@"); else written=($tags); fi

hit () {  # 1 if the reader returned a credential
  print -r -- "$1" | grep -qiE "Raw|Card:|Payload|Internal ID|Country|88bit" && return 0 || return 1
}

fp_total=0; tested=0
for w in $written; do
  "$PY" "$CU" "hw connect -p $CH2" "${W[$w]}" >/dev/null 2>&1
  self=""; fps=()
  for r in $readers; do
    out=$("$PY" "$CU" "hw connect -p $CH2" "${R[$r]}" 2>&1)
    if hit "$out"; then
      if [[ "$r" == "$w" ]]; then
        self="ok"
      else
        fps+=("$r"); fp_total=$((fp_total+1))
        print -r -- "      ⛔ FALSE POSITIVE: tag=$w reader=$r said: $(print -r -- "$out" | grep -iE 'Raw|Card:|Payload|Internal ID|Country' | head -1 | tr -s ' ')"
      fi
    fi
    [[ "$r" != "$w" ]] && tested=$((tested+1))
  done
  printf "  tag %-11s self-read %-5s   false positives: %s\n" "$w" "${self:-MISSED}" "${#fps[@]}"
done
print -r -- ""
printf "  ⇒ %d cross-protocol reads on real tags, %d false positives\n" "$tested" "$fp_total"
print -r -- "  -- restoring the bench credential --"
"$PY" "$CU" "hw connect -p $CH2" "lf hid prox write -f H10301 --fc 123 --cn 4567" 2>&1 | grep -iE "VERIFIED|CANNOT TELL|WRITE" | sed 's/^/     /'
