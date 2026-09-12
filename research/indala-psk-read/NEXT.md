# Next — ranked

**GOAL: support as many LF encodings as the Flipper Zero does, in read, write AND emulate.**

⛔ **Order of work, decided 2026-09-12:** finish Indala and its near-twin IDTECK, then fix the
readers and emulators that already exist and the bugs already found, and only then add new
protocols. Breadth on top of known-broken foundations would multiply the debt, and this
project has already spent days on measurements that turned out to describe a broken
instrument rather than the thing under test.

⛔ Method rules live in `METHOD.md`. Evidence lives in `FINDINGS.md` (what is believed now)
and `LOG.md` (what was believed when). This file is a PLAN — finished sections collapse to
one line. The bench layout, the device ports and the git conventions are in `README.md`
under **The bench** and **Working conventions**.

---

## ⚠ Needs hands — what is still queued

**Cleared 2026-09-12:** both Chameleons power-cycled (rig A emulates again, 3/3); stacking
approved for removal; the T5577 may be rewritten to whatever a test needs.

| | why a person is required |
|---|---|
| ⚠ **The bench tag is Indala `a0000000e6bd0e92` as of 2026-09-12** — it was found wearing HID Prox, not the PAC recorded here | ⭐ Left as Indala deliberately: it is now the **carrier-locked reference** C138 needs, and Chameleon #2 reads it 4/4 through the sandwich. ⛔ Its previous contents are dumped to `~/lf-t55xx-1D555955-5569A9A5-55A59569-D5B2649F-B3C6AD1F-CF649393-928C14E5-dump.json` and restore with `lf t55xx restore -f <that file>`. ⚠ §2's PAC specimen is no longer on this tag — but `pactest/` reproduces that failure on the host from a committed capture, so the physical tag is not the only specimen. ⛔ Restore `0x00081040 / 0x4944544B / 0x55667788` before relying on C90-C92's regressions again |
| ~~The bench tag is 224-bit Indala~~ ✅ done | Left that way deliberately — §1d's decode is still failing and this tag is the only specimen to diagnose against. Contents, blocks 0-7: `0x000820E0 0x80000001 0xB23523A6 0xC2E31EBA 0xBCBEE4AF 0xB3C6AD1F 0xCF649393 0x928C14E5`. ⛔ Restore `00081040 / 4944544B / 55667788` before relying on C90-C92's regressions again; the restore cycle is proven and needs no hands |
| ~~⭐ **An AIR GAP under the HID tag, to test C47**~~ ⛔ **scrapped 2026-09-12 — the user dropped the HID test** | Kept for the record: paper spacers, 1-12 mm. The T5577 wearing HID reads 12/12 flat on the pad, so there is no margin for the guard to affect (C108). ⇒ Set a gap that puts reads near 50% and the paired guard-on/guard-off test finally has somewhere to show an effect. **Protocol: one placement, then hands off** — start around 6 mm, I measure and say up or down, and once the rate is in the 20-80% band both builds are measured at that same gap without touching it |
| ~~⭐⭐ **A carrier-locked tag in front of the Proxmark — the null for C137**~~ ✅ **done, and clean** | The T5577 went back into the sandwich and the control landed: NO TONE 5 of 5 against the emulator's 131 ppm, and the full 290 ms decoded 5 of 5 against 0–197 ms (C138). ⭐ The sandwich is strictly better than the old rig B — see `README.md` |
| **A free-running source in front of a Chameleon reader** | The one case §1's status was built for. Two Chameleons must face each other and the rigs do not. The Flipper cannot stand in — it is carrier-locked and we read it 8 of 8 (C87) |
| ~~**§4 burst length**~~ ✅ done | The Proxmark had to face the emulator and faced the tag. Rig B was turned so the two face each other directly, and §4 was measured there (C135) |
| **§7 BLE transport** | The point of it is measuring with the cable out |

⚠ **A soft reboot is not a power cycle.** Rig A's emulation died during §3 work and survived a
forced sense re-enable, `hw slot store` and a **full DFU reflash**, then came back on a USB
unplug (C96). ⇒ When emulation misbehaves in a way that makes no sense, the power cycle is a
real diagnostic step, not a superstition — and it is one only a person can take.

---

## The grid — where we stand against the Flipper

⚠ Flipper column is the **local Momentum firmware** (`/Users/Shared/code/personal/rfid/Momentum-Firmware`),
not upstream — that is what this bench actually tests against, and it carries two protocols
upstream's list does not. Every one of its 26 protocols has BOTH a decoder and an encoder.
Chameleon columns come from the CLI command table; "emulate" means an `econfig` command plus a
registered `TAG_TYPE_*`.

| protocol | read | write | emulate | Momentum |
|---|---|---|---|---|
| EM410x (+16/32, Electra) | ✓ | ✓ | ✓ | ✓ |
| HID Prox (H10301, generic, ex-generic) | ⚠ **unreliable** | ✓ | ✓ | ✓ |
| ioProx (IOProxXSF) | ✓ | ✓ | ✓ | ✓ |
| PAC/Stanley | ⚠ **unreliable** | ✓ | ✓ | ✓ |
| Viking | ✓ | ✓ | ✓ | ✓ |
| Jablotron | ✓ | ✓ | ✓ | ✓ |
| **Indala 64-bit** | ✓ | ✓ | ✓ | ✓ |
| **Indala 224-bit** | ✓ | ⛔ | ⛔ | ✓ |
| **IDTECK** | ✓ | ✓ | ✓ | ✓ |
| EM4x05 | ✓ | ✗ | ✗ | — (not an lfrfid protocol) |
| AWID | ✗ | ✗ | ✗ | ✓ |
| FDX-A | ✗ | ✗ | ✗ | ✓ |
| FDX-B | ✗ | ✗ | ✗ | ✓ |
| Paradox | ✗ | ✗ | ✗ | ✓ |
| Pyramid | ✗ | ✗ | ✗ | ✓ |
| Keri | ✗ | ✗ | ✗ | ✓ |
| Gallagher | ✗ | ✗ | ✗ | ✓ |
| NexWatch | ✗ | ✗ | ✗ | ✓ |
| Securakey | ✗ | ✗ | ✗ | ✓ |
| GProxII | ✗ | ✗ | ✗ | ✓ |
| Noralsy | ✗ | ✗ | ✗ | ✓ |
| InstaFob | ✗ | ✗ | ✗ | ✓ (ASK, RF/32) |

⇒ **Twelve protocols absent, two readers unreliable. Every Indala and IDTECK READ path now works.**

⛔ **Indala is NOT finished.** Momentum implements **Indala224** as well as Indala26, and our
decoder is hard-wired to 64 bits (`INDALA_PSK_FRAME_BITS 64`). That belongs in Phase 1, and it
carries a RAM cost that collides with §8:

> A 224-bit frame at RF/32 is **7168 samples**. The two-frame rule that guarantees one whole
> frame lands inside the window (`lf_indala_psk.h`) would need **14336 samples = 28 KB** at
> 16-bit, against the 8 KB the 64-bit path uses. On a part where the reader already holds
> 48 KB, that is not a free change — and 32 KB of the current footprint is stacking that buys
> nothing at the correct placement (§8). ⇒ Decide §8 before building Indala224, not after.

⚠ The `tag_base_type.h` placeholders for **Keri** and **NexWatch** sit in the PSK block beside
Indala and IDTECK, so those two should reuse the PSK1 path nearly whole.

## The plan — three phases, in this order

**Phase 1 — finish Indala and IDTECK.** They share a physical layer, so IDTECK is nearly free
once Indala is done, and the pair is the proving ground for everything after it.
> ✅ §0 wrong-credential fix · ✅ §1 undecodable-signal status · ✅ §1c IDTECK reader · §1d Indala224 · ✅ §4 burst length · §5 carrier locking

⭐ **Do §8 first, out of phase order.** It is the only item that changes the reader's core, it
is decided (drop stacking), and §1d is impossible until it lands. Sizing the Indala224 buffer
before removing 40 KB would mean sizing it twice.

⇒ **The order now: ~~§8~~ → §1d → §2 → §3.** Everything in it runs unattended on the bench as it
stands; §5 and §7 are what remain for a person — §4 is closed (C135).

**Phase 2 — fix what already exists.** Two readers are unreliable on loud tags and there are
known bugs with reproductions attached. ⛔ Nothing new is added until these are closed:
breadth on a broken foundation multiplies the debt, and this project has already lost days to
instruments that were measuring themselves.
> §2 HID Prox + PAC readers · §3 PWM clock bug · §7 BLE test transport · §8 reader RAM

**Phase 3 — new protocols**, cheapest and most-verifiable first.
> §10 the eleven missing protocols

⭐ **What each item needs.** The bench is two fixed rigs (`README.md` → The bench) and
nothing on them moves unless a person moves it, so this is the difference between work that
can run to completion now and work that has to wait.

| item | needs |
|---|---|
| §1 status code | **nothing** — empty arm on rig A, loud-undecodable arm from the Flipper emulating IDTECK into our Indala reader. ⚠ A genuinely free-running source still needs hands (C87) |
| §1c IDTECK reader | **nothing** — the Proxmark writes IDTECK to the T5577 and Chameleon #2 reads it. Rig B is exactly this test |
| §1d Indala224 | **nothing** — §8 is settled, so the RAM is available. `lf indala clone --224` on rig B |
| §2 HID / PAC readers | **PAC: nothing** — it fails 0/10 on the T5577 right now (C109), which is the specimen to debug against. ⚠ **HID: needs a weak-coupling fob**, not a write; the T5577 wearing HID reads 12/12 either way (C108). ⛔ Restore `0x00081040 / 0x4944544B / 0x55667788` when done — C90-C92 regress against them |
| §3 PWM clock bug | **nothing** — the slot is changed over the CLI and the Flipper reads the result on rig A |
| §4 burst length | ✅ **done.** Rig B now has the Proxmark facing Chameleon #2 directly, and the sweep ran there (C135) |
| §5 carrier locking | ⛔ **a person.** A scope decision, not a task — and C134 narrowed it: the reader's carrier is not observable on this board |
| §7 BLE transport | ⚠ hands — the whole point of it is testing with the cable out |
| §8 reader RAM | ✅ **done** — stacking removed, 89.8 KB free. C97 |

⇒ The unattended path through Phase 1 and Phase 2 is **§1 → §1c → §3 → §2**. Only §7
needs hands now, and only §5 needs a decision.

⚠ **Retired section numbers.** LOG.md is append-only and cites sections that have since moved.
`§1d`, `§2b`, `§3a`, `§3b`, `§3c` were folded into the sections above during the 2026-09-12
rewrite; the pre-rewrite file is `archive/NEXT-2026-09-12-before-dedup.md`.

---

## ✅ Done — detail is in LOG.md, not here

| | outcome |
|---|---|
| Re-measure on the front (§1) | 114/160 single captures decode, 0/160 empty produce a frame. L58 |
| The straddle gate (§1c) | 110 frames on the front, **all 110 correct**. C48, L61 |
| Phase rotation (§2b) | re-derived from both placements; the "union" approach was a trap. L58 |
| Loud-signal nulls (§3) | **220 reads, 0 false positives** across HID, EM410x, IDTECK, Viking, PAC, Jablotron. L66 |
| Second Chameleon (§4) | 20/20 at the same phase. C56, L66 |
| Signal-hunting work (§5, §6) | closed as UNMOTIVATED — there was never a deficit. L69 |
| Per-lever sweep scripts (§7) | retired; the skirt does not predict decode. C38 |
| T5577 write (§8) | **9 of 9 verified writes**; the old failures were placement. C60, L68 |
| Indala emulation (§3a) | Flipper **6/6**, Proxmark to 262 ms. C81, L79 |
| Stacking and frame lock (§2) | resolved in opposite directions. C58, C59, L67 |
| Flipper read driver (§0) | `flipper.py` committed and bracketed: **PSK 4/4, ASK 0/4**. L83 |
| Undecodable-signal status (§1) | `0x43` shipped and verified on four arms, **20 reads**. C89, L85 |
| Wrong credential from IDTECK (§0) | vetoed by decoding IDTECK: **0 of 10**, was 4 of 8. C90, C91, L87 |
| IDTECK reader (§1c) | `lf idteck read`, **6/6** with two nulls. C92, L88 |
| Advertising guard shared (§2) | all four SAADC readers; effect on HID/PAC **unmeasured**. C95, L91 |
| §8 decided | drop stacking — the user's call, 2026-09-12. C94, L92 |
| Stacking removed (§8) | 48 KB → 8 KB, free RAM 49.6 → 89.8 KB, all arms pass. C97, L93 |
| Indala 224-bit reader (§1d) | **6/6** with two nulls; PSK2-only was the answer. C107, L98 |

---

## 1. ✅ Undecodable-signal status — shipped

`STATUS_LF_SIGNAL_NOT_DECODED (0x43)` returns from `lf_psk1_failure_status()` in the shared
LF path, so §1c's IDTECK reader inherits it. Verified on four arms, 20 reads (C89, L85).
`LF_TAG_LOGIN_REQUIRED (0x42)` added to the host enum in the same pass.

⚠ It fires on a marginal REAL tag as well as on a wrong-protocol one — 2 of 5 on rig B — so
the message leads with "retry or reposition" rather than with the emulator diagnosis. That is
the honest reading of what is measured: energy present, no frame.

## 1b. ✅ IDTECK null re-run and bracketed — passes

Bracketed at **22.65x the empty floor at 60–65 kHz** (the band a PSK1 subcarrier occupies,
where the original used 500–20000 Hz and measured leakage), then **20/20 not found, 0 frames**
(C55 repaired, C85, L82). The phantom-reader leg correctly reported that it contributes
nothing.

⇒ Every loud-signal null in the project now rests on a valid bracket.

## 1c. ✅ IDTECK reader — shipped

`lf idteck read`, built as the shared capture engine plus a decode function: **6 of 6** on the
bench tag, **4 of 4** not found on an empty antenna, and **5 of 5** reporting 0x43 rather than
a credential against an Indala source that `lf indala read` reads 3 of 3 in the same minute
(C92, L88). No new RAM — `lf_psk1_read()` takes the protocol as a parameter.

⭐ It delivered the control it promised: each PSK1 reader is now the other's positive control,
on the same signal at the same moment, so cross-protocol nulls no longer need an amplitude
proxy.

## 1d. ✅ Indala 224-bit — reads, 6 of 6

`lf indala read --224`. Verified on device against the tag's own memory: **6/6** correct,
empty antenna **3/3 not found**, a loud IDTECK tag **3/3 at 0x43 with zero credentials**, and
`lf idteck read` still 3/3 (C107, L98).

⭐ **The lesson worth keeping.** Three acceptance rules failed before the real obstacle was
visible: the direct view of a PSK2 tag is the *running XOR* of its data — a deterministic
transform, not noise — so it repeats at the frame period exactly as well as the data does and
is identical across captures. **No test inside one capture can separate a frame from its own
integral.** A reader has to be told the modulation; it cannot infer it. Every Indala224
specimen is PSK2, so the format decodes the differential view and only that one.

⚠ Momentum's exact two-preamble test is not available to us at ~2% bit error: it rejected the
true frame in all four captures (C106).

## 2. ⭐⭐⭐ Fix the HID Prox and PAC readers — both fail on loud tags

Three tags with byte-identical memory read **0/6, 3/6 and 7/9** on the Chameleon and 3/3 on a
Proxmark (C46). The RF path is flat while reads fail (C45). `lf pac read` scored 0/5 and 2/5
with its tag at 16x the empty floor (L66). `lf em 410x read` sits at 95%, not 100%.

✅ **Done: the advertising guard is now shared and applied to all four SAADC readers.**
`lf_adv_suspend`/`lf_adv_resume` in `lf_reader_data.c` replace `lf_reader_generic`'s private
copy; hidprox, PAC and ioProx now call them. Regression on the shared path is clean — IDTECK
6/6, Indala 0 credentials (C95, L91).

⛔ **Its effect is UNMEASURED.** The before/after read rate on HID and PAC is the whole point
and there is no HID or PAC tag on this bench. Queued at the top of this file with an offer to
reprogram the T5577 reversibly.

⚠ **C47 IS UNTESTED AND THE GUARD IS NOT THE FIX.** Measured paired on one T5577 wearing
H10301, one session, one recompile apart: **12/12 with the guard off, 12/12 with it on**
(C108). The guard cannot be shown to help a specimen that already reads perfectly, and C47
predicts an effect only on MARGINAL reads. ⇒ Testing it needs MARGIN, not a different tag. ⭐ An air
gap supplies it: paper spacers in 1 mm steps turn coupling into a dial, so the read rate can
be set near 50% where a guard that helps would show. ⛔ Not a tag to write — writing HID to the
T5577 is done and reads 12/12. ioProx never fitting the theory stands too.

⭐⭐ **PAC reproduces, and it is now narrowed to interpretation.** `lf pac read` gives **0 of
10** on a T5577 the Proxmark reads perfectly (C109). Captured at 14336 samples and demodulated
on the host, independently of the firmware:

| | |
|---|---|
| per-bit levels | sharply bimodal, clusters **2803** and **7037** |
| periodicity | **99.1% at lag 128** — the exact frame length — vs 53–65% at every other lag |
| match to the tag's blocks | ⛔ best **102/128**, on a plateau, not a peak |

⇒ A real, correctly-clocked 128-bit frame reaches the ADC, so neither coupling nor the front
end is at fault (C110). Either the on-air bits are not the raw blocks, or level-thresholding
is the wrong recovery for T5577 NRZ.

⛔⛔ **The air data is FINE and the demodulation is the bug** (C113, retracting C111). The
tag's blocks are a textbook PAC frame under `pac.c`'s own rules: first 19 bits `0x7F902` =
`PAC_PREAMBLE` exactly, twelve valid 10-bit UART frames spelling **STX '2' '0' CD4F5552**, XOR
checksum `0x72` matching the twelfth byte. A tag transmitting that is not the problem.

⚠ **How the wrong conclusion happened, because it will happen again:** the Chameleon and the
Proxmark are independent receivers, but I ran both captures through ONE level-threshold script
of mine. Their agreement measured my bug, not the air (M31).

⇒ **Next: fix the host demodulation until it recovers the frame**, then diff it against the
firmware's. That is the pattern that has solved every decode question in this project —
`mfdemod.py` against `lf_indala_psk.c`. Candidates for what is wrong, all testable on the
committed captures with no hardware:
- **Bit-boundary locking.** I averaged fixed 32-sample windows from a fixed offset; NRZ needs
  the boundaries found, and `pac.c` works on EDGE INTERVALS rather than levels for that reason.
- **A global threshold against a drifting baseline.** `pac.c` spike-clips at 3x the floor and
  recalibrates every 20480 samples — it would not do that if a single threshold worked.
- **Polarity and the dead zone.** `PAC_THRESH_FUZZ` keeps a 25-75% dead band; a hard threshold
  turns every marginal sample into a bit.

⭐ **Start with the shared capture path.** The LF readers are two families:

| family | readers | capture |
|---|---|---|
| GPIO/comparator | em410x, jablotron, viking | `register_rio_callback`, 128-entry ring, no SAADC |
| **SAADC** | **hidprox, ioprox, pac** + lf_reader_generic | own `saadc_cb`, own 6144 ring, own field start/stop |

Only `lf_reader_generic.c` suspends BLE advertising, and its comment records why: a burst
collapses the 125 kHz field for ~1.6 ms and hit **4 captures in 10**. The three that duplicate
the prologue are HID, ioProx and PAC — and HID and PAC are the two measured failing. The
SAADC reader that HAS the guard is Indala, at 60/60. `capture_begin()`/`capture_end()` already
exist and are already shared by two entry points, so moving HID onto them is small — and is
the clean test of C47.

⛔ Do not read the em410x 95% as evidence either way: it is on the GPIO path and never
touches the SAADC, which is why C47 was wrongly weakened once already.

## 3. ✅ The emulation defects — all three fixed

| | fix | verified |
|---|---|---|
| **C129** a mode cycle disarmed emulation | `lf_sense_disable()` no longer nulls `m_pwm_seq` — the sequences are static and outlive the uninit | ASK **4/4** after a mode cycle, was 0/4 |
| **C130** the PWM clock ignored type changes | `lf_tag_data_loadcb()` wraps the loader and re-inits the PWM when the required clock differs | clock follows 125kHz ↔ 1MHz; round trip reads both protocols |
| **C131** `hw slot type` silently unpersisted | the CLI now says it is RAM-only until `hw slot store` | — |

⇒ **Round trip with no reboot and no power cycle:** Indala → EM410X ASK 4/4 → Indala PSK 4/4
(C132, L113). This was the defect that most obstructed unattended work — a driver alternating
read and emulate tripped C129 every cycle.

⚠ `hw emudebug` and the counters behind it are instrumentation. ⛔ Remove before upstreaming,
and keep them until §9 is written: they are what made these three visible.

## 4–5 preamble. ⭐ Burst length and carrier lock are DIFFERENT problems

They both affect emulation and they are easy to conflate, so:

| | what is wrong | affects | fixed by |
|---|---|---|---|
| **Burst length** | the emulator transmits N frames then PAUSES to check the field. A reader whose capture spans that gap sees a discontinuity | readers that demodulate a long buffer in one go — **the Proxmark** | ⚠ a longer burst, but only up to a point: measured flat above ~500 ms (C135) |
| **Carrier lock** | the subcarrier comes from the emulator's own crystal instead of DIVIDING the reader's carrier, so it drifts | readers that assume the subcarrier sits exactly at fs/2 — **ours** | clocking the modulation from the received carrier |

They are independent, and the evidence separates them cleanly:

- **Proxmark**, burst 10 → failed beyond 163 ms; burst 32 → decodes to 262 ms. Its problem
  was boundaries, and lengthening the burst fixed it.
- **Ours**, with burst 32 and good coupling → still 0/14. The longer burst did nothing for
  us, because our problem is drift, not boundaries.
- **Flipper** → 6/6 either way. Tolerant of both.

⇒ §4 is about continuity, §5 is about frequency. Doing §4 does not help our own reader; doing
§5 does not remove burst boundaries. Neither is required for the emulation to be useful — two
independent readers already accept it.

## 4. ✅ Burst length — measured across 120×, and 500 ms stays

✅ **The unit is a time budget.** `LF_TAG_BURST_TARGET_MS (500)`, converted per protocol by
summing the sequence's own `counter_top` values — Indala 31 frames, EM410x 16, verified on
device (C133, L114). The old frame count gave Indala224 a **1.83 s** field-loss latency.

✅ **And the value is now measured** (C135, L115). Proxmark facing Chameleon #2, one frame,
one bench, 24 runs:

| burst | frames | runs | decoded |
|---|---|---|---|
| 32.8 ms | 2 | 8 | 7, every one capped at **66 ms** |
| 508 ms | 31 | 5 | 1, to 197 ms |
| 524 ms | 32 | 6 | 4, to 164–197 ms |
| 4014 ms | 245 | 5 | 4, to 131–164 ms |

⇒ **Flat above ~500 ms, falling below it.** Eight times more burst bought nothing, and at
4014 ms a 290 ms capture is essentially never boundary-crossing — yet 262 ms and 290 ms
decoded in **none** of the 5, so above ~500 ms the limit is not the boundary at all. Below it
the boundaries do cost: the 32.8 ms arm lost a factor of 2–3 **while carrying 27% more
signal**, which confounds it in the favourable direction and understates the loss.
⇒ **Keep 500 ms** — now because the curve is flat there, not because 32 frames landed near it.

⛔ **The carrier-edge replacement is dead.** L114 proposed detecting field loss by counting
carrier edges so the burst could go away entirely. It cannot be built on this board: every
path from the antenna to the MCU passes through a detector diode, and the one pin that looked
like a way in is an analog switch's select input (C134). ⚠ The notes already said so before
the proposal was made — that is M32, and it is the reason this cost a read rather than a
bench session.

**What is left, in order of value:**

| | why |
|---|---|
| ⭐ **Re-take the arms with `offsetsweep.py`** | `emutest.py` scores the longest decodable PREFIX, so one bad patch early poisons every longer prefix — a 131 ms window decodes at offset 65.5 ms in the capture whose prefix ceiling was 66 ms (C136). The SHAPE of the curve survives that; the numbers are lower bounds |
| ✅ ~~**Explain the 131–197 ms ceiling at long bursts**~~ | **done — it is clock slip, 131 ppm, one subcarrier cycle every 122 ms** (C137). Measured with `clockoffset.py`, which never looks at whether anything decoded. ⇒ It was §5's question all along, and §5 now has a number |
| **A carrier-locked positive control** | without one, that ceiling might belong to the Proxmark's demodulator at this amplitude rather than to our emulator. Needs a real tag in front of the Proxmark — ⚠ hands |
| **The knee between 33 ms and 508 ms** | ⚠ **deprioritised by C137.** With the coherent window sized at ~122 ms, any burst comfortably longer than that should behave alike, so the knee is a latency optimisation (500 ms → perhaps 250 ms) rather than a readability question. ⛔ Not worth a reflash per arm: the within-arm spread is as large as the between-arm differences. If it is taken up, make the budget settable at runtime first, the way `hw emudebug` was added for §3 |

## 5. ⚠ Carrier locking — optional, and the decision belongs to a person

⛔⛔ **THE OBVIOUS IMPLEMENTATION IS IMPOSSIBLE HERE, AND THAT CHANGES THE DECISION.** Locking
to the reader means recovering its carrier, and **this board never sees it**: every path from
the LF antenna to the MCU passes through a detector diode at the coil, so all four LF pins
carry an envelope or nothing at all (C134). The Flipper can do it because its front end hands
the carrier to a timer; ours rectifies it away in the first component. ⇒ Whatever §5 becomes,
it is not a port of the Flipper's approach — and the question a person is being asked is no
longer "should we do this" but "is there a second way to do it on hardware that discards the
clock". ⚠ Until that is answered, treat everything below as describing the *problem*, not an
available fix.

✅ **AND THE CONTROL LANDED — this is settled** (C138). A carrier-locked T5577 playing the
*same frame* in the *same field* reads NO TONE where our emulator reads 131 ppm, and the
Proxmark decodes the full 290 ms of it 5 of 5 where our emulator managed 0–197 ms — on **less**
signal. Amplitude, the demodulator and the burst boundary are all excluded. ⇒ Nothing here is
open any more except what to do about it.

⭐ **The cost of not locking is sized: a ~122 ms coherent window** (C137). Our subcarrier
runs 131 ppm off the reader's clock, so one whole subcarrier cycle of phase error accumulates
every 122 ms — which is the ceiling §4 measured from the other side. ⇒ That reframes the whole
section. This is not a defect with a fix available; it is a **hardware-imposed property** of an
emulator that cannot see the reader's clock. A reader that re-acquires inside 122 ms is
unaffected, which is why the Flipper reads our emulation 6/6 and only the Proxmark — which
demodulates one long buffer coherently — ever sees it. ⇒ The decision a person is being asked
for is now cheap: **document it and move on**, unless someone wants to chase a way to recover a
clock the front end rectifies away.

The Flipper clocks its emulation timer from the reader's own carrier (`LL_TIM_CLOCKSOURCE_EXT_MODE2`
+ `LL_TIM_ConfigETR`), so its subcarrier divides that carrier exactly as a T5577 does (C74).
Ours free-runs. That is why our reader cannot read our emulator — and why two independent
readers can (C81).

⛔ **NOT A PORT.** The STM32's TIM2 takes an external clock on ETR; the nRF52's PWM is clocked
only from PCLK16M. The output stage would have to change:

| piece | exists? |
|---|---|
| TIMER2 in `NRF_TIMER_MODE_COUNTER` | ✓ `lf_125khz_radio.c` |
| PPI into `NRF_TIMER_TASK_COUNT` | ✓ — but keyed off `PWMPERIODEND`, which only exists when WE generate the carrier |
| an event per RECEIVED carrier cycle | ⚠ needed — LPCOMP or the RIO GPIOTE edge |
| TIMER COMPARE → PPI → GPIOTE toggle on `LF_MOD` | ⚠ needed |

⭐ **It buys self-consistency, not correctness** (C82). Real tags are locked by construction,
so `lf indala read` is specialised rather than defective, and the emulation already works
against everything except us. ⇒ Worth doing only if device-to-device RF transfer matters more
than the complexity — and §1's status message is the cheap alternative.

## 6. ✅ IDTECK emulation re-tested — works, 4/4

Flipper `rfid read indala` returns **Idteck 4944544B00000000** on 4 of 4 attempts, with the
ASK-mode arm returning nothing as a control (C83, L80). ⇒ The shared PSK1 transmit path in
`utils/psk1.c` carries both protocols correctly, and C66's retraction is confirmed from the
other side — IDTECK is working, not merely unproven.

⚠ Not tested against the Proxmark, which would need the device moved off the Flipper pad. The
Flipper result plus its own ASK null is already a bracketed pass.

## 7. ⚠ Drive the Chameleon over BLE for testing

The USB cable costs **~40%** of the emulated signal (C72 retracted) and forces a physical
unplug between configuring a device and testing it — the gap that produced three empty-field
results in one session.

The firmware speaks BLE; `chameleon_com.py` is serial-only with no `bleak` anywhere, so this
is a new transport behind the existing command layer. ⚠ A live BLE connection during emulation
is itself uncharacterised, and C47 has advertising bursts collapsing the field — measure with
and without before trusting it.

## 8. ✅ Stacking removed — 48 KB → 8 KB

`.bss` down **41,176 bytes**, free RAM **49.6 → 89.8 KB**. The decoder works in place; the
two-capture agreement rule stayed and now uses consecutive captures, which are a stricter
independent pair than the two hand-separated accumulators were. Verified on four bench arms
and 480 committed captures (C97, L93).

✅ **The back-side rate is OUT OF SCOPE, decided 2026-09-12** — the reader is not trying to
support that placement, so 72% → 32% is not a cost to verify, it is a number about a use case
we do not have. No hardware arm is queued for it and none should be (C98).

⭐ The back-side *captures* remain valuable and stay committed: they are the project's only
low-SNR corpus, 26 dB down, and every gate and rule here is regression-tested against them by
`make check`. Not supporting a placement is not the same as discarding data taken there.

## 9. Upstreamable?

Read and write are solid and independently verified. Emulation works against two readers.
The pieces a PR would need: the tag-type registration (done here), emulation (done here), and
a decision on whether `lf indala read`'s specialisation is acceptable upstream — §1's status
message is what makes it honest.

⚠ `idteck.c` ships a PSK1 emulation nobody has verified end to end (§6). Worth reporting
upstream independently of anything here.

## 10. ⛔ NOT YET — the eleven missing protocols

⛔ **Phase 3. Do not start these until Phase 2 is closed.** Every one of them will need the
same capture path, the same null discipline and the same brackets, and all three are still
being repaired.

Ranked by expected cost, cheapest first:

| protocol | why it is cheap, or not |
|---|---|
| **Keri**, **NexWatch** | PSK, and `tag_base_type.h` already reserves them in the 300 block beside Indala and IDTECK. Should reuse the PSK1 path nearly whole |
| **AWID**, **Pyramid**, **Paradox** | FSK, so they reuse the HID/ioProx machinery — ⚠ which is exactly what Phase 2 is fixing |
| **Securakey**, **GProxII**, **Noralsy**, **Gallagher** | ASK/biphase variants; reuse the EM410x/Viking/Jablotron machinery |
| **FDX-A**, **FDX-B** | animal-ID, different framing and bit rates; likely the most work |

⚠ **Each one needs a real tag to verify against.** A protocol implemented against a
specification and never tested on hardware is worth less than nothing — it looks supported.
That is the whole lesson of `idteck.c`, which shipped an emulation nobody had ever verified
end to end and a reader that does not exist.

## Closed — do not re-open without new evidence

| | why |
|---|---|
| `LF_RSSI` / AIN0 as a signal tap | C08/C09, and §5/§6 removed the motivation entirely |
| SAADC gain | C10, same |
| Folding at 2048 samples | C14: odd parity inverts the subcarrier every frame |
| Indala parity as an acceptance gate | C19: it passed a frame wrong in 20 bits |
| A 12 kHz cutoff for the baseband filter | C16: the null at fs/2 was the point, not the cutoff |
| Placement as the explanation for emulator weakness | C72 retracted: it was the USB cable |
