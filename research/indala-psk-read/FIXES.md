# Incidental fixes — defects found while doing something else

⭐ **Each entry is scoped as its own upstream PR.** None of these is part of the LF-protocol work
this branch exists for; they were found by tripping over them. Keeping them here means the main
work can be split out cleanly later, and means none of them gets quietly bundled into a review
that is about something else.

⛔ **Everything here is present on `main`.** Where a defect is ours, it says so.

| # | what | files | present on `main`? |
|---|---|---|---|
| F1 | T5577 writes silently password-protect the tag, with a key that differs by protocol | `t55xx.h`, `lf_reader_main.c`, `lf_t55xx_data.c`, `chameleon_cmd.py` | yes |
| F2 | Five writers can report success having written nothing | `lf_reader_main.c` | yes |
| F3 | Header declares a lock bit as a length | `t55xx.h` | yes |
| F4 | `unpack()` relabels 15 of 29 writable HID formats | `wiegand.c/.h`, `hidprox.c` | yes |
| F5 | Two 28 KB capture buffers resident at once — 22% of RAM | `lf_reader_generic.c/.h`, `lf_indala_data.c`, `app_cmd.c` | yes |
| F6 | `lf hid prox write` reported success without reading back | `chameleon_cli_unit.py` | yes — **fixed** |
| F7 | The repeat-read corroboration rule compares only the first 64 bits of any frame | `lf_indala_data.c` | ours — **fixed** |

---

## F1 — a T5577 write silently password-protects the tag ⛔ WORST OF THESE

**Symptom.** Writing any T5577 with a ChameleonUltra locks it. The Proxmark can then no longer
`detect`, `read`, `dump` or `wipe` it; every block reads back `80000000` (a start bit then
silence). The tag still emits its credential perfectly, so it looks bricked rather than locked.

**Root cause, two halves.**
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
