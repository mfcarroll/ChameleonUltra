#!/bin/zsh
# ⭐⭐ IS EVERY FIX IN `FIXES.md` STILL FIXED?
#
# ⛔ Two audits established that the register was INCOMPLETE (C349, C350). Neither asked the
# other question: the eleven entries each say "fixed and hardware-verified", and some were
# verified months ago on firmware that has since been rewritten underneath them — F5 merged the
# capture buffers, F7 widened the corroboration comparison, the whole PSK1 path was refactored.
# A regression would silently turn an entry into a false claim, and `FIXES.md` is what a
# maintainer would be handed.
#
#   ./fixcheck.sh
#
# ⚠ Only the entries testable on rig B run here. F10 and F11 are EMULATION fixes and need the
#   Flipper as reader; they are reported as NOT CHECKED rather than quietly skipped, because a
#   register that hides its own coverage gaps is the problem this script exists for.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="$HERE/../../software/script/.venv/bin/python"
CU="$HERE/../../software/script/cu.py"
CH2=/dev/tty.usbmodemF429364E46961
PM3=/Users/Shared/code/personal/rfid/proxmark3/pm3
PM3PORT=/dev/tty.usbmodemiceman1
cu () { "$PY" "$CU" "hw connect -p $CH2" "$@" 2>&1 }
pass=0; fail=0; skip=0
ok   () { print -r -- "  ✓ $1"; pass=$((pass+1)) }
bad  () { print -r -- "  ⛔ $1"; fail=$((fail+1)) }
note () { print -r -- "  — $1"; skip=$((skip+1)) }

print -r -- ""
print -r -- "  FIXES.md regression pass"
print -r -- "  ------------------------"

# F1 — a T5577 write must NOT password-protect the tag.
cu "lf hid prox write -f H10301 --fc 123 --cn 4567" >/dev/null
d=$($PM3 -p $PM3PORT -c "lf t55xx detect; lf t55xx dump" 2>&1)
p7=$(print -r -- "$d" | grep -E "^\[\+\]  07 " | awk '{print $4}')
# ⛔ DO NOT REQUIRE BLOCK 7 == 0. Block 7 is only the PASSWORD when the password bit in block 0
# is set; with it clear the block is ordinary data, and it legitimately holds the tail of
# whatever was written last — an Indala224 frame ends `928C14E5` and lands right there. A first
# version of this check demanded zero and reported F1 as REGRESSED on a tag whose own dump says
# `Password set: No`. The bit is the fix; the block is not.
if print -r -- "$d" | grep -qi "Password set.*No"; then
  ok "F1  write leaves no password — 'Password set: No' (block 7 = ${p7:-?}, residual data, not a key)"
else
  bad "F1  REGRESSED — $(print -r -- "$d" | grep -i 'password set' | head -1 | tr -s ' ')"
fi

# F6 — `lf hid prox write` must read back rather than assert success.
o=$(cu "lf hid prox write -f H10301 --fc 123 --cn 4567")
if print -r -- "$o" | grep -qiE "VERIFIED|CANNOT TELL|WRITE (FAILED|DID NOT LAND)|WRONG DATA"; then
  ok "F6  the HID writer reports a read-back verdict — $(print -r -- "$o" | grep -oiE 'VERIFIED|CANNOT TELL|WRITE [A-Z ]+|WRONG DATA' | head -1)"
else
  bad "F6  REGRESSED — no read-back verdict printed"
fi

# F9 — `lf pac read` must work; it returned 0 of 10 before the drive sweep.
cu "lf pac write --cn CARD0001" >/dev/null
n=0; for i in 1 2 3 4; do print -r -- "$(cu 'lf pac read')" | grep -qi "CARD0001" && n=$((n+1)); done
(( n == 4 )) && ok "F9  lf pac read $n/4 (was 0 of 10 before the drive sweep)" \
              || bad "F9  lf pac read only $n/4"

# F8 — the BLE advertising guard. Re-measured at 100/100 tonight (C345); spot-check here.
cu "lf hid prox write -f H10301 --fc 123 --cn 4567" >/dev/null
n=0; for i in $(seq 1 12); do print -r -- "$(cu 'lf hid prox read -f H10301')" | grep -q "CN: 4567" && n=$((n+1)); done
(( n == 12 )) && ok "F8  HID reads $n/12 — the guard holds (C345 measured 100/100)" \
              || bad "F8  HID reads only $n/12 — the BLE guard may have regressed"

# F2/F3/F5/F7 are structural: assert the code still says what the entry claims.
S=$HERE/../../firmware/application/src
# ⛔ The SYMBOL must still exist — `write_t55xx()` sets it for opt-in passwords. What must not
# come back is its USE in a config constant, so ignore the #define and every comment.
if [[ $(grep -n "T5577_PWD" $S/rfid/nfctag/lf/protocols/t55xx.h | grep -vE ":\s*(//|\*|/\*)" | grep -vc "#define T5577_PWD") -eq 0 ]]; then
  ok "F3  T5577_PWD is defined but used by no config constant"
else
  bad "F3  REGRESSED — a config constant sets T5577_PWD again"
fi
grep -q "blk_count == 0" $S/rfid/reader/lf/lf_reader_main.c \
  && ok "F2  the blk_count guards are present" \
  || bad "F2  REGRESSED — no blk_count guard in lf_reader_main.c"
grep -q "res.frame_bits == prev_bits" $S/rfid/reader/lf/lf_indala_data.c \
  && ok "F7  corroboration compares the whole frame and the length" \
  || bad "F7  REGRESSED — the comparison is not length-aware"
# ⛔ The buffer is reached through `lf_capture_buffer()`; `m_samples` is only a #define alias
# in lf_indala_data.c. Grepping the generic file for `m_samples` finds nothing and says nothing.
if grep -q "lf_capture_buffer" $S/rfid/reader/lf/lf_reader_generic.c \
   && grep -q "define m_samples (lf_capture_buffer())" $S/rfid/reader/lf/lf_indala_data.c; then
  ok "F5  one shared capture buffer, owned by lf_reader_generic.c and aliased as m_samples"
else
  bad "F5  REGRESSED — the sampled readers no longer share lf_capture_buffer()"
fi

# ⛔ F4 IS CHECKABLE HERE — do not just assert that ctest covers it. `ambig` compiles the real
# `wiegand.c` and asserts the ambiguity counts, which is exactly what the unpack() fix changed.
if (cd "$HERE/ctest" && make ambig >/dev/null 2>&1 && ./ambig 2>&1 | grep -q "ambiguity counts unchanged"); then
  ok "F4  unpack() — ctest 'ambig' reports the ambiguity counts unchanged"
else
  bad "F4  REGRESSED — ctest 'ambig' does not report unchanged ambiguity counts"
fi
note "F10 slot-type mode cycle — EMULATION, needs the Flipper as reader (rig A)"
note "F11 emulation burst budget — EMULATION, needs a reader watching rig A"

print -r -- ""
printf "  ⇒ %d pass, %d FAIL, %d not checkable on this rig\n" "$pass" "$fail" "$skip"
print -r -- "  -- restoring the bench credential --"
cu "lf hid prox write -f H10301 --fc 123 --cn 4567" | grep -iE "VERIFIED|CANNOT TELL" | sed 's/^/     /'
