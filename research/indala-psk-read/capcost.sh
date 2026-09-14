#!/bin/zsh
# ⭐ WHAT DOES A SUCCESSFUL READ COST, IN CAPTURES?
#
# C335 measured that FDX-B decodes 16 of 16 from a HELD field and 5 of 16 from a cycled one,
# and `capture_begin` starts the radio afresh for every capture — so every capture the reader
# takes is a field-on transient. That left a live question nobody had asked of the OTHER
# protocols: is the per-capture field cycling actually costing yield?
#
# ⭐ The arms already report the answer. A successful read prints "N captures taken", and the
# corroboration rule needs TWO agreeing decodes — so **2 is the floor**. A protocol sitting at
# 2 is paying nothing; one sitting at 6 is throwing away two thirds of its captures.
#
#   ./capcost.sh                 # every protocol with a write arm on the sampled path
#   ./capcost.sh gallagher fdxb  # just these
#
# ⛔ Writes its own plaintext first, so the number is measured against a known credential
# rather than whatever the tag happened to hold (M33). Restores HID at the end.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961
READS="${READS:-3}"

# proto -> write args | read args
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

# ⚠ NOT `${@:-a b c}` — zsh expands that default as ONE word, so the whole list arrives as a
# single protocol name and nothing runs. Bitten by the same splitting rule before.
if (( $# )); then
  protos=("$@")
else
  protos=(indala indala224 idteck keri nexwatch gallagher securakey noralsy
          awid paradox pyramid fdxa gproxii fdxb)
fi

print -r -- ""
print -r -- "  captures per SUCCESSFUL read — floor is 2 (the corroboration rule needs two agreeing)"
print -r -- "  ------------------------------------------------------------------------------------"
for p in $protos; do
  [[ -z "${W[$p]:-}" ]] && { print -r -- "  $p: no entry"; continue; }
  "$PY" "$CU" "hw connect -p $CH2" "${W[$p]}" >/dev/null 2>&1
  # ⛔⛔ ASK TWO QUESTIONS, NOT ONE. Grepping only for the capture count conflates "the read
  # FAILED" with "this arm does not PRINT a capture count" — and NexWatch does not print one,
  # so a first version of this script reported it as 0 of 4 on a tag the Proxmark was reading
  # perfectly. Success is decided on the credential line; the cost is a separate question that
  # may have no answer.
  costs=(); ok=0; noreport=0
  for i in $(seq 1 $READS); do
    out=$("$PY" "$CU" "hw connect -p $CH2" "${R[$p]}" 2>&1)
    if print -r -- "$out" | grep -qiE "Raw|Card:|Payload|Internal ID|Country"; then
      ok=$((ok+1))
      n=$(print -r -- "$out" | grep -oE "[0-9]+ captures? taken" | grep -oE "^[0-9]+")
      if [[ -n "$n" ]]; then costs+=($n); else costs+=("?"); noreport=1; fi
    else
      costs+=("FAIL")
    fi
  done
  note=""
  (( noreport )) && note="   (? = this arm prints no capture count)"
  printf "  %-11s reads %d/%d   captures: %s%s\n" "$p" "$ok" "$READS" "${costs[*]}" "$note"
done
print -r -- ""
print -r -- "  -- restoring the bench credential --"
"$PY" "$CU" "hw connect -p $CH2" "lf hid prox write -f H10301 --fc 123 --cn 4567" 2>&1 | grep -iE "VERIFIED|CANNOT TELL|WRITE" | sed 's/^/     /'
