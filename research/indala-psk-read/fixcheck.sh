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
# ⭐ F10 AND F11 NOW RUN TOO (C393). They are EMULATION fixes and need the Flipper as reader,
#   which was unusable when this script was written — its `rfid` plugin would not load (C377).
#   That is fixed and automatic, so the two entries that reported NOT CHECKED are now measured.
# ⚠ They still degrade to NOT CHECKED rather than FAIL if the Flipper cannot be made ready: a
#   dead reader and a dead emulator produce identical numbers (C373/C374/C376), so a rig-A arm
#   that cannot prove its reader alive must not be scored at all — which is the same principle
#   that made this script report its gaps in the first place.
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
# ─── rig A: the two EMULATION fixes ───────────────────────────────────────────────────
#
# ⛔ THE READER MUST BE PROVEN ALIVE FIRST. `flipper.py heap` exits non-zero when the rfid
# plugin will not fit (C377), and `flipper.py reboot` clears it in ~10s. If neither works the
# two entries report NOT CHECKED, exactly as they did before — never PASS, never FAIL.
CH1=/dev/tty.usbmodemC3A1656543DE1
SLOT=8   # ⛔ THE DEVICE HAS EIGHT SLOTS, 1-8. This said 9 for one run, reasoning that
         # emugrade.sh "owns" slot 8 — which is not a real conflict, because nothing here runs
         # concurrently. `hw slot type -s 9` is REFUSED by the argument parser, so every econfig
         # was rejected, the device kept whatever was already loaded, and the checks below
         # reported "neither arm scored" and "F11 REGRESSED — the same frames-per-burst". Both
         # were my own bug. ⚠ A rig-A arm that fails should be suspected of being the HARNESS
         # before it is written down as a regression (C393).
cu1 () { "$PY" "$CU" "hw connect -p $CH1" "$@" 2>&1 }

# protocol -> slot type | econfig | the flipper.py arm that should score
emu_arm () {   # $1 type  $2 econfig  $3 mode(psk|ask) ; echoes "hits/attempts", or "-" 
  cu1 "hw slot type -s $SLOT -t $1" "hw slot enable -s $SLOT --lf" "$2" \
      "hw slot change -s $SLOT" "hw mode -e" >/dev/null
  sleep 1
  # ⚠ SINGLE-MODE PRINTS A COLON AND DOUBLE-MODE DOES NOT — `=> ASK: 3/4` against
  # `PSK 0/6 vs ASK 6/6`. Matching only the colon-less form scored every arm as a miss and
  # reported "neither arm scored", which reads as a dead rig rather than a broken grep.
  "$PY" "$HERE/flipper.py" read --mode "$3" --attempts 4 2>&1 |
    grep -oiE "(psk|ask):? +[0-9]+/[0-9]+" | tail -1 | grep -oE "[0-9]+/[0-9]+"
}
fpb () { cu1 "hw emudebug" | grep -oE "frames per burst *: *[0-9]+" | grep -oE "[0-9]+$" }

if ! "$PY" "$HERE/flipper.py" heap >/dev/null 2>&1; then
  "$PY" "$HERE/flipper.py" reboot >/dev/null 2>&1
fi
if ! "$PY" "$HERE/flipper.py" heap >/dev/null 2>&1; then
  note "F10 slot-type mode cycle — rig A reader would not start (see C377); NOT CHECKED"
  note "F11 emulation burst budget — rig A reader would not start (see C377); NOT CHECKED"
else
  # ⭐ F10: the defect was that changing a slot's LF TYPE disarmed emulation until a REBOOT.
  # So the test is two arms of DIFFERENT types back to back with no power cycle between them —
  # and Gallagher→Indala is the strongest pair available, because it also crosses the PWM base
  # clock boundary (125kHz → 1MHz), which is the other half of what the fix had to get right.
  g=$(emu_arm Gallagher "lf gallagher econfig -s $SLOT --raw 7feaa31e76d86c6d868cc249" ask)
  gf=$(fpb)
  i=$(emu_arm Indala "lf indala econfig -s $SLOT --id a0000000e6bd0e92" psk)
  if_=$(fpb)
  if [[ "${g%%/*}" -gt 0 && "${i%%/*}" -gt 0 ]]; then
    ok "F10 slot type changed Gallagher→Indala with no reboot and BOTH emulate — ask $g, psk $i"
  elif [[ "${g%%/*}" -gt 0 || "${i%%/*}" -gt 0 ]]; then
    bad "F10 REGRESSED — a type change disarmed emulation: ask ${g:--}, psk ${i:--}"
  else
    note "F10 neither arm scored — that is a silent RIG, not a verdict; NOT CHECKED"
  fi

  # ⭐ F11: the burst used to be a fixed FRAME COUNT, so a protocol whose frame is long ran a
  # short window and a long-window reader failed at the boundary. The fix derives the count from
  # the sequence's own duration — so the tell is that two protocols report DIFFERENT
  # frames-per-burst. A regression to a constant makes them EQUAL, which no read score reveals.
  if [[ -n "$gf" && -n "$if_" && "$gf" != "$if_" ]]; then
    ok "F11 frames-per-burst is derived, not constant — Gallagher $gf vs Indala $if_"
  elif [[ -n "$gf" && "$gf" == "$if_" ]]; then
    bad "F11 REGRESSED — both protocols report the same frames-per-burst ($gf); it is a constant again"
  else
    note "F11 could not read frames-per-burst from hw emudebug; NOT CHECKED"
  fi
  cu1 "hw mode -r" >/dev/null
fi

# ⛔ F12 IS AN OPEN DEFECT, NOT A FIX, so it has nothing to regress and gets no arm. It is named
# here anyway: a register that only prints the REPAIRED defects reads as a clean bill of health,
# which is the opposite of what FIXES.md is for. Remove this line when F12 is fixed and give it
# a real arm instead.
print -r -- ""
print -r -- "  ⛔ F12 FSK2a emulation emits a constant tone — OPEN, characterised only (C387)."
print -r -- "     Not a regression target. The measurement that would settle it is ./fskcap.sh,"
print -r -- "     which needs the two Chameleons facing each other (AUTOPILOT.md §5)."

print -r -- ""
printf "  ⇒ %d pass, %d FAIL, %d not checkable on this rig; 1 registered defect still OPEN (F12)\n" "$pass" "$fail" "$skip"
print -r -- "  -- restoring the bench credential --"
cu "lf hid prox write -f H10301 --fc 123 --cn 4567" | grep -iE "VERIFIED|CANNOT TELL" | sed 's/^/     /'
