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
| **A free-running source in front of a Chameleon reader** | The one case §1's status was built for. Two Chameleons must face each other and the rigs do not. The Flipper cannot stand in — it is carrier-locked and we read it 8 of 8 (C87) |
| **§4 burst length** | The Proxmark must face the emulator, and it faces the tag |
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
| **Indala 224-bit** | ⛔ **MISSING** | ⛔ | ⛔ | ✓ |
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

⇒ **Twelve protocols absent, one read path missing (Indala224), two readers unreliable.**

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
> ✅ §0 wrong-credential fix · ✅ §1 undecodable-signal status · ✅ §1c IDTECK reader · §1d Indala224 · §4 burst length · §5 carrier locking

⭐ **Do §8 first, out of phase order.** It is the only item that changes the reader's core, it
is decided (drop stacking), and §1d is impossible until it lands. Sizing the Indala224 buffer
before removing 40 KB would mean sizing it twice.

⇒ **The order now: ~~§8~~ → §1d → §2 → §3.** Everything in it runs unattended on the bench as it
stands; §4, §5 and §7 are what remain for a person.

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
| §2 HID / PAC readers | **nothing** — the Proxmark may rewrite the T5577 freely now. ⚠ C46 used three real HID tags; a T5577 wearing HID is a different specimen, so say which was used, and restore the IDTECK contents `00081040 / 4944544B / 55667788` afterwards because C90-C92 regress against them |
| §3 PWM clock bug | **nothing** — the slot is changed over the CLI and the Flipper reads the result on rig A |
| §4 burst length | ⚠ hands — the Proxmark has to face the emulator, and it faces the tag |
| §5 carrier locking | ⛔ **a person.** A scope decision, not a task |
| §7 BLE transport | ⚠ hands — the whole point of it is testing with the cable out |
| §8 reader RAM | ✅ **done** — stacking removed, 89.8 KB free. C97 |

⇒ The unattended path through Phase 1 and Phase 2 is **§1 → §1c → §3 → §2**. Only §4
and §7 need hands, and only §5 and §8 need a decision.

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

## 1d. ⚠ Indala 224-bit — and its preamble is WEAKER than the one that already failed

✅ **§8 is settled**, so the RAM is there: 28 KB decoding in place against 89.8 KB free (C97).

⛔⛔ **Read this before trusting a preamble match.** Momentum's `protocol_indala224.c` gives
the preamble as **a 1 followed by 29 zeros** — 30 bits, of which 29 are a constant run. That
is strictly weaker than Indala26's 33-bit preamble, and Indala26's is the one a loud IDTECK
tag already forged at sample phase 28, producing a confident wrong credential (C90).

⇒ **A preamble match cannot be the acceptance test for Indala224.** It would be the C90 bug
with a larger target.

⭐ **Use periodicity instead — it is 224 bits of evidence rather than 30, and it is free.** A
224-bit frame repeats continuously, so within a two-frame capture (14336 samples) one whole
frame is guaranteed and the remaining 7168 samples are the neighbouring copies, split before
and after it. Comparing the whole frame against those two partials covers all 224 bits. A
frame that repeats exactly is a frame; noise and a wrong-protocol source do not repeat.

⚠ Momentum reaches the same conclusion by a different route: it requires two consecutive
frames and accepts the second preamble either normal OR inverted. ⇒ Do not assume the
polarity of the repeat — PSK2-style cards alternate it, and our decoder searches both
polarities already for the same reason.

⛔⛔ **AND IT IS PSK2.** A 224-bit tag written by `lf indala clone -r` leaves the T5577 config
at `000820E0` and detect reports **Modulation PSK2** — against `00081040`/PSK1 for the 64-bit
bench tag (C99). Our demodulator recovers ABSOLUTE phase, which for a PSK2 tag is the
DIFFERENTIAL of the data.

⭐ **The fix is already written down in our own decoder header.** The Proxmark matches its
preamble on the PSK1 stream first and only then calls `psk1TOpsk2()` (cmdlfindala.c:1293) —
which is XOR-ing consecutive bits. So one demodulator serves both: search the preamble in the
absolute-phase stream, and if that fails, in its differential. ⚠ The repeat gate is unaffected
— a repeating frame differentiates to a repeating stream.

⚠ **And the Proxmark cannot be the reference here.** Its own read-back of the tag it had just
written came back different, at "len 235" (C100). Use the memory dump, as IDTECK did.

**Plan, in order:**

1. ✅ Frame length parameterised by format descriptor.
2. ✅ One capture buffer at 14336 samples, capture length per protocol.
3. ✅ The periodicity check, as the acceptance test.
4. ⛔ **NEXT: the PSK2 fallback.** Search the preamble in the differential stream when the
   direct one fails, exactly as the Proxmark does.
5. Verify: `lf indala clone -r 80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5`
   writes the tag — confirmed working, and the write/restore cycle is proven: the tag was
   written, dumped, and put back to `00081040 / 4944544B / 55667788` with the IDTECK reader
   returning card 8943462 afterwards. ⛔ Always restore; C90-C92 regress against those blocks.

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

⚠ **And ioProx already argues against C47 being the whole story**: it lacked the guard too and
is not on the unreliable list. If a burst that hits 4 captures in 10 were sufficient to break
a reader, ioProx should be broken as well.

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

## 3. ⚠ PWM clock on a type change — NOT reproduced, and not refuted either

The code reading still holds: `pwm_init()` picks the base clock from `IS_PSK1_TYPE(m_tag_type)`
and runs only from `lf_sense_enable()`, so `hw slot type` changes the type with nothing
re-initialising the PWM.

⛔ **But it does not show up.** Switching slot 1 ASK↔PSK1 with the Flipper's field held
continuously kept reading 4/4, and the failure that did appear survives a full reboot — which
re-runs `pwm_init` by construction, so it cannot be the stale clock (C93, L89).

⇒ Re-open this only with a test that reads the base clock directly, or one that can show a
protocol emulating CORRECTLY before and INCORRECTLY after a type change, with a positive
control on both sides. The attempt logged in L89 had neither.

✅ **Unblocked** — rig A emulates again after the power cycle (3/3). ⭐ And that is itself a
clue worth following here: the failure survived a DFU reflash and died on a USB unplug (C96),
so whatever stuck was peripheral state that a soft reset does not clear — which is the same
family of problem §3 describes, even though §3's specific mechanism was ruled out.

## 4–5 preamble. ⭐ Burst length and carrier lock are DIFFERENT problems

They both affect emulation and they are easy to conflate, so:

| | what is wrong | affects | fixed by |
|---|---|---|---|
| **Burst length** | the emulator transmits N frames then PAUSES to check the field. A reader whose capture spans that gap sees a discontinuity | readers that demodulate a long buffer in one go — **the Proxmark** | a longer burst (fewer boundaries) |
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

## 4. ⚠ Choose the emulation burst length deliberately

`LF_TAG_FRAMES_PER_BURST` is **32** because I guessed it, and it worked (C75). The Proxmark
decodes any window inside one burst and fails across a boundary; 32 frames = 524 ms moved the
cliff from ~180 ms to ~275 ms.

⚠ It does not remove boundaries — 290 ms still fails — and it costs **field-loss latency**:
the device keeps modulating ~524 ms after the reader leaves.
⛔ Do NOT simply maximise it. `NRFX_PWM_FLAG_LOOP` removes boundaries entirely and was already
tried, breaking field detection through self-drive on LF_RSSI.

## 5. ⚠ Carrier locking — optional, and the decision belongs to a person

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
