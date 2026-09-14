#!/bin/zsh
# ⭐ HOW OFTEN DOES A READ THAT SHOULD SUCCEED, SUCCEED?
#
# Everything else here measures CORRECTNESS — the right credential, the right frame, no false
# positives. None of it measures RELIABILITY, because every arm is scored at n = 4 or 6 and a
# 1-in-20 failure is invisible at that n.
#
# ⛔ There is history. C45 was a 15-20% HID intermittency that took months to explain, and
# C250 found it — a BLE advertising burst collapsing the field mid-capture — measuring 96/96
# with the guard against 71/80 without. That number is from BEFORE this branch merged the two
# capture buffers (F5) and widened the corroboration comparison (F7). Nothing has re-measured
# it since, and one HID read-back returned CANNOT TELL during C342's run.
#
#   ./readsoak.sh hidprox 100
#   ./readsoak.sh indala 40 gproxii 40
#
# ⚠ The tag is written ONCE at the start of each protocol and then only read, so a failure is
# a read failure and not a write that never landed.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961

typeset -A W R WANT
W[hidprox]="lf hid prox write -f H10301 --fc 123 --cn 4567"; R[hidprox]="lf hid prox read -f H10301"; WANT[hidprox]="CN: 4567"
W[indala]="lf indala write -r a0000000e6bd0e92";             R[indala]="lf indala read";            WANT[indala]="a0000000e6bd0e92"
W[gproxii]="lf gproxii write --raw fac2a38c2b081af0210b12c2"; R[gproxii]="lf gproxii read";         WANT[gproxii]="fac2a38c2b081af0210b12c2"
W[fdxb]="lf fdxb write --raw 00339a080402079f8040797788040201"; R[fdxb]="lf fdxb read";             WANT[fdxb]="1337"
W[awid]="lf awid write --raw 011d81711dd1181111111111";      R[awid]="lf awid read";                WANT[awid]="011d81711dd1181111111111"
W[gallagher]="lf gallagher write --raw 7feaa31e76d86c6d868cc249"; R[gallagher]="lf gallagher read"; WANT[gallagher]="7feaa31e76d86c6d868cc249"

(( $# % 2 == 0 && $# > 0 )) || { print -r -- "usage: ./readsoak.sh <proto> <n> [<proto> <n> ...]"; exit 2; }

print -r -- ""
print -r -- "  read reliability — the tag is written once, then only read"
print -r -- "  ---------------------------------------------------------"
while (( $# )); do
  p=$1; n=$2; shift 2
  [[ -z "${W[$p]:-}" ]] && { print -r -- "  $p: no entry"; continue; }
  "$PY" "$CU" "hw connect -p $CH2" "${W[$p]}" >/dev/null 2>&1
  ok=0; fails=()
  for i in $(seq 1 $n); do
    out=$("$PY" "$CU" "hw connect -p $CH2" "${R[$p]}" 2>&1)
    if print -r -- "$out" | grep -qi "${WANT[$p]}"; then
      ok=$((ok+1))
    else
      fails+=("$i")
      # ⭐ Keep WHAT it said, not just that it failed — "not found" and a WRONG credential are
      # completely different defects and counting them together hides the worse one.
      print -r -- "      miss at read $i: $(print -r -- "$out" | grep -iE 'Raw|Card|CN:|not found|not decoded' | head -1 | tr -s ' ')"
    fi
  done
  pct=$(( 100.0 * ok / n ))
  printf "  %-11s %3d/%-3d  %5.1f%%   misses at: %s\n" "$p" "$ok" "$n" "$pct" "${fails[*]:-none}"
done
print -r -- ""
print -r -- "  -- restoring the bench credential --"
"$PY" "$CU" "hw connect -p $CH2" "lf hid prox write -f H10301 --fc 123 --cn 4567" 2>&1 | grep -iE "VERIFIED|CANNOT TELL|WRITE" | sed 's/^/     /'
