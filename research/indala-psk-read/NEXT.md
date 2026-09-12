# Next — ranked

**State:** `lf indala read`, `lf indala write` and Indala tag emulation all work and are
verified against independent hardware. What remains is a handful of real defects, one design
decision, and upstreaming.

⛔ Method rules live in `METHOD.md`, not here. Read them before adding a claim to the ledger.
⛔ Evidence lives in `FINDINGS.md` (what is believed now) and `LOG.md` (what was believed
when). This file is a PLAN — if a section is finished it collapses to one line below.

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

---

## 1. ⭐⭐⭐ Tell the user WHY a read failed — the undecodable-signal status

Our reader cannot decode a non-carrier-locked source, so a Flipper or Proxmark emulating
Indala reads as `LF tag not found` — **the same message as an empty antenna** (C79, C81).
That is a trap: we fell into it ourselves with far better instruments than a user will have.

⚠ This is a genuine limitation and the fix is to REPORT it, not to hide it. See §5 for why
tolerating unlocked sources is not worth the complexity.

**The framework already exists.** `app_status.h` defines `STATUS_LF_*` codes in the 0x40
block, and `chameleon_enum.py`'s `Status` is an `IntEnum` whose `__str__` carries the human
message. Every client — CLI, mobile app, any GUI — reads the same status byte, so one new
code reaches all of them.

```
firmware/application/src/app_status.h
    STATUS_LF_TAG_OK              0x40
    STATUS_LF_TAG_NO_FOUND        0x41
    STATUS_LF_TAG_LOGIN_REQUIRED  0x42
    STATUS_LF_SIGNAL_NOT_DECODED  0x43   <- new
```

**Design:**

1. In `indala_read()`, when no capture yields a frame, compute one cheap integer measure of
   fc/2 energy over the last capture — the mean absolute value of the `(-1)^n`-mixed buffer
   is a single pass and needs no FFT. Calibrated figures are in `emuprobe.py`.
2. Strong energy but no frame ⇒ return `STATUS_LF_SIGNAL_NOT_DECODED` instead of
   `STATUS_LF_TAG_NO_FOUND`.
3. Add it to `Status` in `chameleon_enum.py` with a message naming the likely cause:
   *"An Indala-like subcarrier is present but could not be decoded. This usually means an
   emulated tag (Flipper, Proxmark, another Chameleon) rather than a real one."*

⛔ **Name the status for what is MEASURED, not what is inferred.** We observe "fc/2 energy
present, no frame recovered". "It is an emulator" is the likely cause, not the observation —
a detuned real tag or a damaged one could present the same way. The message may offer the
inference; the status code must not encode it.

⚠ **Pick the threshold against both arms.** An empty field must never produce this status, or
it becomes noise. `emuprobe.py` has the calibration: on a Chameleon capture a real tag reads
~181000 in the fc/2 band and an empty field ~4300.

⚠ **While in there:** `STATUS_LF_TAG_LOGIN_REQUIRED (0x42)` exists in the firmware and is
MISSING from the host `Status` enum, so any client hitting it today gets a bare number. Fix
in the same pass.

## 1b. ✅ IDTECK null re-run and bracketed — passes

Bracketed at **22.65x the empty floor at 60–65 kHz** (the band a PSK1 subcarrier occupies,
where the original used 500–20000 Hz and measured leakage), then **20/20 not found, 0 frames**
(C55 repaired, C85, L82). The phantom-reader leg correctly reported that it contributes
nothing.

⇒ Every loud-signal null in the project now rests on a valid bracket.

## 2. ⭐⭐⭐ Fix the HID Prox and PAC readers — both fail on loud tags

Three tags with byte-identical memory read **0/6, 3/6 and 7/9** on the Chameleon and 3/3 on a
Proxmark (C46). The RF path is flat while reads fail (C45). `lf pac read` scored 0/5 and 2/5
with its tag at 16x the empty floor (L66). `lf em 410x read` sits at 95%, not 100%.

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

## 3. ⛔ Firmware bug: changing a slot to a PSK1 type while emulating leaves the PWM clock wrong

Affects IDTECK identically — pre-existing, not introduced here.

`pwm_init()` picks `IS_PSK1_TYPE(m_tag_type) ? 1MHz : 125kHz`, but runs only from
`lf_sense_enable()`, which fires only on a sense DISABLE→ENABLE transition. `hw slot type`
changes `m_tag_type` long afterwards and nothing re-inits.

| base clock | entry | subcarrier | |
|---|---|---|---|
| 1 MHz (PSK1) | 16 µs | 62.5 kHz | correct |
| 125 kHz | 128 µs | 7.8 kHz | **8x slow, unrecognisable** |

Broken both ways. **Workaround:** `hw mode -r` then `-e`, or reboot, after changing type.
**Fix:** re-init when `IS_PSK1_TYPE(m_tag_type)` changes. ⚠ `lf_sense_disable()` also releases
the HFXO request and nulls `m_pwm_seq`, so a naive disable/enable drops the loaded sequence.

⚠ Real by inspection; never confirmed as the cause of any symptom (L70).

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

## 8. ⚠ 32KB of the Indala reader serves only the wrong placement — a decision, not a bug

48KB static: 8KB samples, 8KB scratch, **32KB of stacking accumulators**. Stacking is worth
exactly nothing on the front (68.75% at every depth) and 32%→72% on the back (C58).

| option | RAM | back-side rate |
|---|---|---|
| as shipped | 48KB | 72% |
| int16 accumulators, cap stacking at 2 | 32KB | 52% |
| drop stacking | 16KB | 32% |

⛔ Do not drop stacking without re-checking the straddle gate: stacking REINFORCES the
dead-band straddle, and the gate is what holds it at 0 wrong.

## 9. Upstreamable?

Read and write are solid and independently verified. Emulation works against two readers.
The pieces a PR would need: the tag-type registration (done here), emulation (done here), and
a decision on whether `lf indala read`'s specialisation is acceptable upstream — §1's status
message is what makes it honest.

⚠ `idteck.c` ships a PSK1 emulation nobody has verified end to end (§6). Worth reporting
upstream independently of anything here.

## Closed — do not re-open without new evidence

| | why |
|---|---|
| `LF_RSSI` / AIN0 as a signal tap | C08/C09, and §5/§6 removed the motivation entirely |
| SAADC gain | C10, same |
| Folding at 2048 samples | C14: odd parity inverts the subcarrier every frame |
| Indala parity as an acceptance gate | C19: it passed a frame wrong in 20 bits |
| A 12 kHz cutoff for the baseband filter | C16: the null at fs/2 was the point, not the cutoff |
| Placement as the explanation for emulator weakness | C72 retracted: it was the USB cable |
