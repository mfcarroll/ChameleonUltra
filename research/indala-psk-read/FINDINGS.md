# Findings — current knowledge

⭐ **This file holds no history.** It is rewritten freely whenever understanding changes, and
it states only what is believed **now**. What was believed before, and when, is `LOG.md`.
If a claim is not in the ledger below, it is not established.

---

## The result

**The Chameleon Ultra reads Indala, on the device, as a command.**

```
lf indala read   ->   Indala PSK1
                      Raw: a0000000e6bd0e92
                      Fmt 26 FC: 52 Card: 63612 Parity: 11
```

20 of 20 consecutive reads returned the credential, in 0.41–0.55 s each. The whole
demodulation is integer arithmetic on the nRF52840 — no float, no FFT, 8 KB of buffer.

Offline, the same decoder recovers the credential from 51 of 160 single 300 ms captures,
and the empty field produced a frame **0 times in 160**. No stacking, no folding, no
averaging.

## The decode, end to end

Everything below is integer arithmetic over 4096 samples. No float, no FFT.

```
sample at 125 kHz, carrier-locked, phase in the working window
  -> mix by (-1)^n                    fc/2 is exactly fs/2, so this is the whole mixer:
                                      no oscillator, no phase estimate. DC and slow drift
                                      move UP to fs/2 and fall out.
  -> [1,2,1] notch at fs/2            NOT optional, see C03. And it is a NOTCH, not a
                                      cutoff: its job is to remove what the mix just
                                      moved up there, see C16.
  -> 32-sample boxcar per bit         the optimal filter for a rectangular bit at RF/32
  -> threshold: bit = (integrator > 0)  PSK1 — the phase IS the data, see C02
  -> search preamble64 = 1010 + 28 zeros + 1, normal and inverted
  -> read 64 bits from the preamble position
```

⚠ **Three details are each individually fatal.** Any one of them alone takes the decode
from 51/160 to 0/160 or near it: the PSK1 mapping (C02), the baseband filter (C03), and
**not** discarding the settle window (C04).

⚠ **And a fourth detail is fatal to trusting the answer rather than getting one.** One in
five recovered frames is WRONG (C17). A reader that returns the first frame it decodes
returns a wrong credential about 20% of the time, so the firmware requires two captures to
agree (C18).

## Claims ledger

Every load-bearing claim, with the evidence that supports it. **`indep` is the column this
project learned the hard way**: whether the claim was checked against something that does
not share its own code path. The retracted `+8.2 dB` had n=20 and a null — and no
independent check, which is exactly why it survived for a day.

`null` — was a control run, and what kind. `emptyfield` is the real control here; `white`
noise is a much weaker one against a chain whose floor spans 17x across bands.

### Holds

| id | claim | n | null | indep | ref |
|---|---|---|---|---|---|
| C01 | The Chameleon Ultra decodes Indala from one 300 ms capture: 51/160 exact | 160 | emptyfield 0/160 | ✓ Proxmark reads the same tag | L44, L46 |
| C02 | **PSK1: the phase IS the data.** Not differential — differential is `psk1TOpsk2`, the Proxmark's *fallback* | — | — | ✓ `cmdlfindala.c:1259` vs `:1293` | L44 |
| C03 | The baseband filter is load-bearing: **51/160** real captures decode with it, **0/160** without | 160 | — | ✓ real captures, not synthetic | L44, L46 |
| C04 | Discarding the 400-sample settle window is fatal: **43/160** with the full capture, **0/160** without | 160 | — | ✓ real captures | L44 |
| C05 | The preamble needs an exact search, not a correlator — the template is dominated by a 28-bit constant run and the peak lands a nibble out | — | — | ✓ Proxmark `preambleSearch()` | L44 |
| C06 | Working phase window is ticks 4–60; best 12–36. **Stock phase 0 decodes 0/5** | 5/phase | emptyfield | — | L44 |
| C07 | Works at the stock 8-bit sample width; the 16-bit path is not required for the read | 1 | — | — | L44 |
| C08 | `LF_RSSI`/AIN0 carries no fc/2: tag/empty 1.04x inside a 1.28x scatter, flat to 0.5 dB across 1–62 kHz vs AIN5's 31.6 dB rolloff. Not clipped | 7 | emptyfield | — | L39 |
| C09 | …but AIN0 is alive: the tag shifts its DC by +16 counts with **±0 spread over 7 repeats** | 7 | emptyfield | — | L39 |
| C10 | The fc/2 noise floor is analog-referred, not ADC-referred — it tracks gain at or above the gain ratio | 7 | emptyfield | — | L22 |
| C11 | Captures are frame-locked to field-on: they cross-correlate at lag 0 with one sign; empty captures scatter with mixed signs | 7 | emptyfield | — | L40 |
| C12 | Zero-offset cross-capture stacking gives √N: +7.3 dB at N=7, empty floor falling exactly 2.65x | 7 | emptyfield + arithmetic null | — | L40 |
| C13 | The frame is visible in the sideband envelope — the 28-zero run as a reproducible null at the 2048-sample period | 7 | emptyfield | — | L40 |
| C14 | The word has **19 ones — odd parity** — so the subcarrier inverts every frame and the true repetition period is 4096 samples, not 2048 | — | — | ✓ arithmetic on the known word | L41 |
| C15 | Stock firmware *had* no PSK demodulator: `reader/lf/*_data.c` were all ASK or FSK, `psk1.c` transmit-only. `lf_indala_psk.c` is the first | — | — | ✓ source | L01, L47 |
| C16 | **The filter's job is a NULL AT fs/2, not a low cutoff.** [1,2,1] beats the 12 kHz FFT brick wall 51 vs 43, strictly (McNemar b=0 c=8, p=0.008) — and [1,1,1], which smooths as hard but nulls at fs/3, is the worst of the set at 31/160 | 160 paired | emptyfield 0/160 at every variant | ✓ mechanism control: same smoothing, wrong null | L46 |
| C17 | **One recovered frame in five is WRONG.** 24 bad frames from 200 captures across two sessions — and all 24 were DISTINCT, while the truth recurred 77 times | 200 | emptyfield: no frame at all, 0/160 | ✓ two sessions, two geometries | L46, L47 |
| C18 | Requiring two captures to agree removes them: **0 wrong in 40000** resampling trials on either pool, at a median of 2–3 captures | 2x20000 | ✓ vs need=1 at 20/22% wrong | ✓ replicated on live captures taken after the design was fixed | L47 |
| C19 | The Wiegand-26 parity is **not** a sufficient gate: it rejects 13 of 17 bad frames but passed `a0000000b9be47a4`, which is wrong in 20 bits | 200 | — | ✓ a counter-example, not a rate | L47 |
| C20 | The firmware read works: **20/20** consecutive `lf indala read`, 0.41–0.55 s each, integer-only on the nRF52840 | 20 | ⚠ **the on-device empty-field null has not been run** | ✓ agrees with the Proxmark's read of the tag | L47 |
| C21 | The C decoder and the numpy decoder agree **word for word on all 320 committed captures**, including the failures | 320 | — | ✓ *this is the independent check* — integer vs float, notch vs FFT, no shared code | L47 |

### Firmware bugs found and fixed

| id | bug | effect | ref |
|---|---|---|---|
| F01 | The 8-bit debug path did `14-bit >> 5` | discarded 5 bits **and distorted the rolloff shape** | L10 |
| F02 | BLE advertising collapsed the LF field during capture | 41% of captures corrupted → 0% | L26 |
| F03 | The DMA ring dropped 75% of every batch (`CIRCULAR_BUFFER_SIZE` 512 vs `ADC_BUF_SIZE` 2048) | captures were islands of 512 samples spanning 64 ms, not 16 ms | L01 |

### Retracted

| claim | why it was wrong | retracted by |
|---|---|---|
| "fc/2 sits at Nyquist, therefore it cancels" | the Proxmark samples once per carrier cycle too, and reads Indala | L03 |
| "the Indala tag produces no detectable modulation" | measured through the `>>5` truncation | L10 |
| "phase sweep shows 659x" | tracking overrun bursts; small denominator | L15 |
| "31.2 dB below the Proxmark ⇒ not viable" | the **measurement** stands; the conclusion assumed a decoder that could not work | L44 |
| "7.6 dB demodulation gap, confirmed twice" | both routes measured that broken decoder | L44 |
| "detectable but not decodable, 52/64 vs a 45/64 null" | a constant preamble run scoring itself — and the wrong decoder besides | L42, L44 |
| "matched filter worth +8.2 dB over PSKDemod" | measured on a synthetic that shared the decoder's own bug | L44 |
| "the phase is gone before the ADC" | the injected synthetic shared the decoder's convention; it was testing self-consistency | L44 |
| "coherent frame averaging is dead" | the circularity argument was wrong — captures are frame-locked, so there is no alignment step | L40 |
| "folding at 2048 samples improves things" | it averages a frame against its own inverse; only the polarity-blind skirt survives | L41 |
| "settle has no effect" / "air gap is flat" | both measured pre-BLE-fix on a tag carrying `DEADBEEF/12345678` | L34 |
| "the baseband low-pass needs a ~12 kHz cutoff" | the cutoff was incidental. What the filter must do is NULL fs/2, where the mixer just put the carrier ripple; a 3-tap notch does it better and cheaper (C16) | L46 |

### Open / untested

| claim | status |
|---|---|
| Tag position is worth ~5.7 dB | n=1, from an accidental probe. Large, concentrated at high frequency, and plausible — but one capture |
| Settle, air gap, oversampling | closures invalid (L34); never re-measured against a working decoder |
| Is the phase window tag- or unit-specific? | ⚠ worse than that — it is **not stable across sessions on the same tag and unit**. Phase 12 was 5/5 in the sweep and 4/10 correct a day later; phase 28 went the other way. ⇒ do not hard-code a phase, rotate. Still untested on a second tag or unit |
| On-device empty-field null | ⛔ **not run.** The offline null is strong (no frame in 160) but the firmware adds a phase rotation and an agreement rule, and neither has been exercised against an empty field. C20 is incomplete until it is |
| Indala parity | reported, deliberately **not** a gate — see C19. It only covers format 26, and a badly wrong frame passed it |

## What runs on the device

| | |
|---|---|
| `rfid/reader/lf/lf_indala_psk.c` | the demodulator. Pure integer, no nRF dependency, so it host-compiles for `research/indala-psk-read/ctest/` |
| `rfid/reader/lf/lf_indala_data.c` | capture, phase rotation, the two-capture agreement rule |
| `rfid/reader/lf/lf_reader_generic.c` | `capture_begin`/`capture_end` extracted so `lf sniff` and the Indala read share one copy of the BLE suspend, the ring and the settle discard |
| `DATA_CMD_INDALA_SCAN` = 3033 | `lf indala read` |

Cost: 8 KB of `.bss` for the sample buffer, ~1 KB of stack, 92 KB of RAM still free.
The decode is ~32 x 4096 adds and runs inside the inter-capture gap.

⭐ **The filter is free.** Summing a [1,2,1]-filtered signal over a 32-sample window is
identically a weighted sum of the *unfiltered* signal over 34 samples with weights
`1,3,4,4,...,4,3,1`, so the notch folds into the bit integrator as

```
integrator = 4*sum(y[a..a+31]) + y[a-1] - y[a] - y[a+31] + y[a+32]
```

— four extra adds per bit rather than three per sample, and no second buffer.

## Hardware reference

```
ANT -> VD1 detector -> LF_OA -> [C28 10n / R9 82 / C36 33n] -> IC1A (R17 4k7 / C38 1n)
                         |                                      -> IC1B -> AIN5 (P0.29)
                         \-> R12 470k -> LF_RSSI -> AIN0 (P0.02)
```

- Filter poles: R9/C36 = **58.8 kHz**, R17/C38 = **33.9 kHz**. At fc/2 = 62.5 kHz they cost
  −3.3 dB and −6.4 dB.
- `READER_POWER` (P1.15) is the schematic's `LF_AMP_PWR`; it feeds the bias divider
  R8 4k7 / R10 3k → LF_VBIAS ≈ 1.2 V.
- SAADC: 14-bit, PPI-triggered from the carrier PWM `PWMPERIODEND`, `GAIN1_6` against the
  internal 0.6 V reference → 3.6 V full scale, `ACQTIME_5US`.
- AIN5 sits at ~1.2 V (33% FS). AIN0 sits at **3.51 V (97% FS)** — 1/6 is already the
  lowest gain the SAADC offers, so single-ended there is no setting with more headroom.

## The tag

`a0000000e6bd0e92` — Indala, 64-bit, Fmt 26, FC 52, Card 63612. PSK1, RF/32, subcarrier at
fc/2 = 62.5 kHz. T5577 block 0 = `00081040`.

⛔ **The campaign registry's `PSK1` cell writes `DEADBEEF/12345678`** — correct PSK1 air
shaping, but not an Indala frame. Use the `INDALA26` cell for anything demod-related;
misreading this invalidated a day (L31).

Confirm before trusting any measurement:

```bash
cd /Users/Shared/code/personal/rfid/proxmark3 && ./pm3 -c "lf indala reader"
```
