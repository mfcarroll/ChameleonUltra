# Incidental fixes — defects found while doing something else

⭐ **Each entry is scoped as its own upstream PR.** None of these is part of the LF-protocol work
this branch exists for; they were found by tripping over them. Keeping them here means the main
work can be split out cleanly later, and means none of them gets quietly bundled into a review
that is about something else.

⛔ **Everything here is present on `main`.** Where a defect is ours, it says so.

⭐⭐ **RUN `./fixcheck.sh` BEFORE TRUSTING THIS FILE.** Every entry says *fixed and hardware-verified*, and some
were verified months ago on firmware that has since been rewritten underneath them. The script re-tests each one
against the build actually flashed: **9 of 11 pass, 0 regressed** as of 2026-09-14 (C351). ⛔ F10 and F11 are
EMULATION fixes needing the Flipper as reader, and the script reports them NOT CHECKED rather than skipping them.

| # | what | files | present on `main`? |
|---|---|---|---|
| F1 | T5577 writes silently password-protect the tag, with a key that differs by protocol | `t55xx.h`, `lf_reader_main.c`, `lf_t55xx_data.c`, `chameleon_cmd.py` | yes |
| F2 | Five writers can report success having written nothing | `lf_reader_main.c` | yes |
| F3 | Header declares a lock bit as a length | `t55xx.h` | yes |
| F4 | `unpack()` relabels 15 of 29 writable HID formats | `wiegand.c/.h`, `hidprox.c` | yes |
| F5 | Two 28 KB capture buffers resident at once — 22% of RAM | `lf_reader_generic.c/.h`, `lf_indala_data.c`, `app_cmd.c` | yes |
| F6 | `lf hid prox write` reported success without reading back | `chameleon_cli_unit.py` | yes — **fixed** |
| F7 | The repeat-read corroboration rule compares only the first 64 bits of any frame | `lf_indala_data.c` | ours — **fixed** |
| F8 | A BLE advertising burst collapses the field mid-capture — 15-20% of HID/ioProx reads | `ble_main.h`, `lf_reader_data.c/.h`, `lf_hidprox_data.c`, `lf_ioprox_data.c`, `lf_pac_data.c` | yes — **fixed**, PR built |
| F9 | `lf pac read` returns nothing: the reader's own field saturates its amplifier | `lf_pac_data.c` | yes — **fixed** |
| F10 | Changing a slot's LF type silently disarms emulation until a reboot | `lf_tag_em.c/.h`, `tag_emulation.c`, `app_cmd.c` | yes — **fixed** |
| F11 | The emulation burst is a FRAME COUNT, so long-window readers fail at the boundary | `lf_tag_em.c` | yes — **fixed** |

---

## F1 — a T5577 write silently password-protects the tag ⛔ WORST OF THESE

⭐⭐ **PR 0 IS WRITTEN AND BUILD-TESTED: `pr0-f1.patch`, 3 files, +19 −12, builds on `main` (C360).**
⛔ Its implementing commit `aff5e377` does **not** apply to `main` — all four files conflict, because it was
written against a tree that already carried this branch's other work. The patch is a reconstruction, not a
cherry-pick. ⚠ `lf_t55xx_data.c` is NOT needed: `main` already writes each block twice, once authenticated and
once not, so `t55xx_write_blocks()` is this branch's refactor rather than part of the fix.

**Symptom.** Writing any T5577 with a ChameleonUltra locks it. The Proxmark can then no longer
`detect`, `read`, `dump` or `wipe` it; every block reads back `80000000` (a start bit then
silence). The tag still emits its credential perfectly, so it looks bricked rather than locked.

⛔⛔ **THE TWO HALVES ARE NOT BOTH UPSTREAM'S, and an extraction test against `main` is what
showed it (C360).** Only the first half exists on `main`; the second is this branch's own.

**Half one — UPSTREAM's, and the part the PR fixes.** All 8 `T5577_*_CONFIG` constants carry
`T5577_PWD`, so every write enables password protection whether or not anyone asked, using the
module global `new_key = 20206666`. ⛔ **`main`'s `old_keys` is `[51243648, 19920427]` and does
NOT contain `20206666`** — so a tag a Chameleon has written can be re-opened by a Chameleon (via
`try_reset_t55xx_passwd`'s new-key-to-new-key call) and by **nothing else**. The Proxmark, a
Flipper, another vendor's reader: all locked out, silently, by a write that reported success.

**Half two — OURS, and it never shipped.** The raw-frame writers `indala`, `indala224`,
`gproxii` and `awid` carried a *different* hard-coded key as a function default, so a tag locked
by one could not be rewritten by another. ⚠ **Those four writers do not exist on `main`** — they
are this branch's additions, and whoever added them copied the wrong constant. It belongs in the
branch's history, not in the upstream PR.

**Root cause, as originally written (kept because the correction is the point):**
1. All 8 `T5577_*_CONFIG` constants carried `T5577_PWD`, so every write enabled password
   protection whether or not anyone asked for one.
2. The key came from two different places: `chameleon_cmd.py`'s module globals
   (`new_key = 20206666`) for `hidprox` and `em410x`, and a function default (`51243648`) for
   `indala`, `indala224`, `gproxii` and `awid`. **Neither value was documented.** A tag locked by
   `lf hid prox write` therefore could not be opened by `lf gproxii write` — nor by anything else,
   since `20206666` was not in `old_keys`.

**Fix.** Password protection is opt-in and runtime-controlled. `T5577_PWD` removed from the
config constants; `write_t55xx()` ORs it in only when a non-zero key is supplied. `new_key`
defaults to zero. `old_keys` is now the list of keys a tag might already hold —
`[20206666, 51243648, 19920427]` — so previously-locked tags stay writable.

**Verified on hardware.** A blank tag written with the fix reads back `Password set: No`,
`Block0 00107060`, **block 7 `00000000`**, credential correct. Before the fix the same write
produced a tag no tool could open.

⚠ **Recovery for tags already locked**: `-p 20206666` if an HID/em410x write locked it,
`-p 51243648` otherwise. Two tags on this bench were recovered that way.

⭐ **PR note.** This is a behaviour change and the PR must open with it: writes stop setting a
password. Anyone relying on the old behaviour gets it back by passing a key explicitly.

## F2 — five writers can report success having written nothing

`awid`, `paradox`, `pyramid`, `gproxii` and `fdxb` — exactly the family sharing
`fsk2a_t55xx_blocks()` — lacked the `blk_count == 0` guard their eleven siblings have. A packer
returning 0 wrote nothing and still returned `STATUS_LF_TAG_OK`, because a T5577 never
acknowledges. Now they return `STATUS_PAR_ERR`. ⛔ Not the cause of anything observed here; found
by auditing all 16 writers.

## F3 — header declares a lock bit as a length

`t55xx.h` declared `t55xx_send_cmd()`'s third parameter as `data_len`; the implementation has
always treated it as `lock_bit`. A caller trusting the name and passing 32 would silently get
"no lock bit" — any value other than 0 or 1 means exactly that. Renamed to match. No behaviour
change.

## F4 — `unpack()` relabels 15 of 29 writable HID formats

`unpack()` returned the first layout of the right bit length whose unpacker did not refuse. On
real tags that relabels 15 of 29 writable formats: a Kastle tag holding fc 1 / cn 1 reports as
Check Point card 8389632. 12 of the 31 unpackers have no rejection path at all, which was
measured rather than read — they accept 100% of random frames. The table already carried
`fields.has_parity`, correct and unconsulted; the walk now prefers a format that can validate and
reports its own ambiguity. **3 files, +102 −16, builds against `main` alone for +248 bytes.**
Verified on hardware: KASTLE now reads back as itself 4 of 4.

## F5 — two 28 KB capture buffers resident at once

`m_samples` (readers) and `sniff_buf` (`lf sniff`) were both static, both 0x7000, both resident
for the life of the image — **57,344 bytes, 22% of the nRF52840's 256 KB** — for buffers that
can never be in use together, since the device dispatches one command at a time. Now one shared
buffer in `lf_reader_generic.c`, which already owns `capture_begin()`. **−28,672 bytes**, all
three callers verified on hardware. ⚠ The contract widens: a scan between two chunk fetches now
destroys the earlier capture across sniff and reader-capture, not just within each.

## F6 — `lf hid prox write` reported success without reading back ✅ FIXED

**Symptom.** It printed the arguments it was handed and `write done.` — **with no tag on the pad
at all**. A T5577 sends no acknowledgement and the firmware returns `STATUS_LF_TAG_OK`
regardless, so there was nothing behind the word "done". Its sibling writers (`gproxii`, `awid`,
`keri`, `indala`) already re-read and print a real verdict; this one was the odd one out.

⛔ **This cost real time on 2026-09-14**: a write to an empty pad was reported as a success twice
running, and believed, during the investigation that produced F1.

**Fix.** Read back after the write, and report one of five verdicts: `VERIFIED`, `CANNOT TELL`,
`WRITE FAILED`, `WRITE DID NOT LAND`, `WRONG DATA ON THE TAG`.

Two details that matter more than they look:

- ⭐ **The read-back is PINNED to the format just written.** An unpinned read would report a
  correct write as wrong for the 12 formats with no rejection path (F4) — the verification would
  then be less trustworthy than the thing it verifies.
- ⚠ **There is also a read BEFORE the write.** That is what separates "the write failed" from "no
  tag is coupled"; without it an empty pad and a failed write are indistinguishable, which is the
  entire confusion this closes.

**Verified on hardware, both directions.** Empty pad → `CANNOT TELL — nothing readable before or
after`. Tag on the pad → `VERIFIED — read back off the tag as HID H10301 26-bit`.

---

## F7 — the corroboration rule compares only the first 64 bits ✅ FIXED

**Symptom.** None visible, which is the problem. Reads were returned as corroborated that had
only ever been corroborated on their first 8 bytes.

**Root cause.** `lf_sampled_read_phases()` accepts a frame when two consecutive captures at the
same sample phase decode to the same word, and the comparison was:

```c
uint8_t prev_word[8] = { 0 };
...
if (have_prev && memcmp(prev_word, res.id, 8) == 0) {
```

A literal 8, for every protocol. That is the whole frame for Indala26, IDTECK and Keri — all
64-bit — and a minority of it for everything added since:

| frame | bytes compared | bytes NOT compared |
|---|---|---|
| NexWatch, Gallagher, Securakey, Noralsy, GProxII (96b) | 8 of 12 | 32 bits |
| FDX-B, Pyramid (128b) | 8 of 16 | 64 bits |
| Indala224 (224b) | 8 of 28 | **160 bits** |

⛔ **The returned credential is the SECOND capture's** (`winner_res = res`), so a tail that
differed between the two was accepted and shipped unseen. The rule did not do what the comment
above it says it does — *"two reads of the SAME configuration landed on the same word"*.

**Fix.** Compare the whole frame, and the length with it:

```c
uint16_t bytes = (uint16_t)((res.frame_bits + 7u) / 8u);
if (bytes == 0 || bytes > LF_DECODE_MAX_FRAME_BYTES) { bytes = LF_DECODE_MAX_FRAME_BYTES; }
if (have_prev && res.frame_bits == prev_bits && memcmp(prev_word, res.id, bytes) == 0) {
```

`prev_word` widens to `LF_DECODE_MAX_FRAME_BYTES`. Every decoder `memset`s `out->id` and fills
all `frame_bits` of it, so the widened comparison reads real decoded data rather than stale
bytes. The length check is there because a frame recovered at one length does not corroborate
one recovered at another. The clamp cannot fire today — every decoder sets `frame_bits` from
its format table — and exists because the failure it guards is silent: a zero length makes
`memcmp` of nothing succeed and the rule accept anything.

**Verified on hardware**, `v2.2.0-602-g81bdaa1`, tag written and read by Chameleon #2:

| protocol | frame | reads |
|---|---|---|
| Indala224 | 224b | **4 of 4 exact**, all 28 bytes corroborated |
| NexWatch | 96b | **4 of 4 exact** |
| GProxII, Gallagher, Securakey | 96b | **3 of 3 each** |
| Pyramid | 128b | **3 of 3** |
| **FDX-B** | 128b | ⛔ **0 of 4 — see below** |

⛔ **FDX-B's failure is NOT this fix, and my first account of it was wrong (C334).** I wrote that
the widening had exposed an unstable FDX-B tail. The A/B refutes it: rebuilt and flashed with
only the comparison reverted to `memcmp(..., 8)` and nothing else changed, **FDX-B still reads 0
of 4**. Six host-decoded captures of the same tag agree on the whole 128-bit word 5 times in 6,
and the single outlier differs in byte 3 — inside the range the old rule already compared.

⇒ **FDX-B was already broken before this change**, by something between C214 and now; the
password fix, F5's buffer merge and F6 all landed in between and none is ruled out. It is queued
as U16. This entry keeps the failing row in the table above because that is what the hardware
said on the day, not because the fix is responsible for it.

⚠ **This one is OURS, not upstream.** `lf_sampled_read_phases()` is this branch's code. It is
listed here anyway because it is not part of the LF-protocol work either — it is a defect in the
shared read engine that the protocol work happened to walk into.

---

## F8 — a BLE advertising burst collapses the field mid-capture ✅ FIXED

⭐⭐ **PR 1 IS WRITTEN AND BUILD-TESTED: `pr1-f8.patch`, 6 files, +51 −0, builds on `main` (C361).**
⛔ **The file list above was wrong in two ways and the extraction found both.** It omitted **`ble_main.h`** — `g_is_ble_connected` lives in `ble_main.c` on `main` and is not declared in the header, so the guard cannot see it
without a one-line `extern`. And it listed **`lf_reader_generic.c`, which the PR does NOT need**: the three
readers each declare their own `lf_adv_guard_t` locally, so **F8 does not depend on F5's capture-buffer merge**.
⚠ It gained `lf_pac_data.c`, which F9 also touches — the two fix-PRs overlap in one file and must be ordered.
⚠ The research switch `LF_ADV_GUARD_ENABLED` is deliberately NOT in the PR: upstream does not need a compile-time
way to turn a fix off. It exists here so the paired measurement could be re-run.

**Symptom.** `lf hid prox read` fails 15-20% of the time on a strong tag, with no amplitude
difference between the successes and the failures. ioProx shares the path and the problem.

**Root cause.** The BLE radio advertises while an LF capture is running. The burst is a **1.6 ms
hole** in the field — shorter than the capture, longer than a bit — and no amplitude median
taken around it can show it, which is why C45 chased the decoder for months and concluded "the
intermittency is the DECODER". It was not.

**Fix.** `lf_adv_suspend()` / `lf_adv_resume()` around the capture, in `lf_reader_data.c/.h` and
`lf_reader_generic.c`, called from `lf_hidprox_data.c` and `lf_ioprox_data.c`.

**Verified on hardware:** **96 of 96 with the guard, 71 of 80 without**, one tag, one session,
p = 6.4e-4 (C250). Re-measured 2026-09-14 after the capture engine was reworked: **HID 100 of
100** (C345).

⚠ **Why this is here rather than in the protocol work.** It is an upstream defect in an upstream
reader, it depends on none of the new protocols, and it is the single most user-visible bug this
branch fixed — a reader that silently fails one read in six. ⛔ It was found by a completeness
audit of the PR split (C349), not by anyone remembering it: it had no `FIXES.md` entry for the
whole session despite being fixed long before.

---

## F9 — `lf pac read` returns nothing: the reader's own field saturates its amplifier ✅ FIXED

**Symptom.** `lf pac read` returns 0 of 10 on a tag the Proxmark reads perfectly.

**Root cause.** Not the decoder. The amplifier saturates on a *well-coupled* tag, so the signal
into it is clipped and the frame is unrecoverable. The cure is counter-intuitive: drive the
field **more weakly**, which is the only way to reduce the signal without moving the tag.

**Fix.** `PAC_DRIVE_STEPS[] = { 4, 6, 2, 7 }` in `lf_pac_data.c` — sweep the drive rather than
assume the stock one works.

**Verified on hardware:** **0 of 10 → 10 of 10**. Scored as bit errors against a known 128-bit
frame rather than pass/fail: drive 2 → 6 errors, drive 4 (stock) → 6, **drive 6 → 0**, drive 7 →
1, with the rail fraction tracking it — 34.0%, 36.4%, 26.0%, 0.0% (C144).

⚠ Same reasoning as F8: an upstream defect in an upstream reader, independent of the protocol
work, and it had no entry until the PR-split audit went looking for unassigned files.

---

## F10 — changing a slot's LF type silently disarms emulation until a reboot ✅ FIXED

**Symptom.** Set a slot to one LF protocol, emulate, then change the slot's type and emulate
again: nothing transmits. A reboot fixes it. Nothing reports an error at any point.

**Root cause, three of them, found together (C129-C131).**
1. `lf_sense_disable()` nulls `m_pwm_seq` and `lf_sense_enable()` never reloads it, so a mode
   cycle throws the waveform away. The sequences are static and outlive the uninit —
   `utils/psk1.h` already documented that — so nulling them was never necessary.
2. `lf_tag_data_loadcb()` does not reinitialise the PWM base clock when the slot type changes,
   so the clock stays at whatever the previous protocol needed. 125 kHz and 1 MHz are not
   interchangeable.
3. `hw slot type` does not say the change is RAM-only until `hw slot store`, so a user who
   power-cycles loses it silently.

**Fix.** `lf_sense_disable()` keeps `m_pwm_seq`; `lf_tag_data_loadcb()` became a wrapper calling
`pwm_reinit_if_clock_changed()` after every load; `hw slot type` says what it did.

**Verified on hardware:** a full round trip with no reboot and no power cycle — Indala → EM410X
**ASK 4/4** → Indala **PSK 4/4**, where the ASK arm read **0/4** before. 19 reads across four
configurations, each protocol acting as the other's control, with the clock and sequence state
read from the device independently of the read outcome (C132).

⚠ **Why it was missed for a whole session**: C349's audit enumerates FILES and asks which PR
owns each. These three live in `lf_tag_em.c`, `tag_emulation.c` and `app_cmd.c` — all of which
ARE assigned to a PR, so the file audit passed over them without looking inside. A fix inside an
assigned file is invisible to it (C350).

---

## F11 — the emulation burst is a FRAME COUNT, so long-window readers fail at the boundary ✅ FIXED

**Symptom.** A reader taking a long capture reads our emulation only when its window happens to
fall inside one burst, and fails whenever the window crosses a boundary. The Proxmark failed
beyond ~163 ms.

**Root cause.** `lf_tag_em.c` played a fixed **frame count** — 10 frames — then paused to check
the field. A frame count is the wrong unit, because a frame is not a fixed duration: 10 frames
is 164 ms of Indala 64-bit but 328 ms of EM410x and 573 ms of Indala 224-bit, so the pause
lands in a different place for every protocol.

**Fix.** A **time budget** rather than a frame count. 500 ms, which is the value 32 Indala
frames happened to give (524 ms) and the only length with evidence behind it.

**Verified on hardware:** raising the burst moved the failure cliff with it — **197 ms ✗ → ✓ and
262 ms ✗ → ✓**, six lengths bracketed either side, with the Proxmark's demodulator as the judge
(it shares nothing with ours) and fc/2 amplitude 22.69 confirming coupling throughout (C75).

⚠ Same blind spot as F10: `lf_tag_em.c` is assigned to a PR, so the file audit never looked.
