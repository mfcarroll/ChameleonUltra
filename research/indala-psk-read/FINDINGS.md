# Findings — current knowledge

⭐ **This file holds no history.** It is rewritten freely whenever understanding changes, and
it states only what is believed **now**. What was believed before, and when, is `LOG.md`.
If a claim is not in the ledger below, it is not established.

---

## The result

**The Chameleon Ultra reads Indala.** A single 300 ms capture at a sample phase inside the
working window decodes the credential exactly.

```
lf sniff --timeout 300 --phase 24   ->   a0000000e6bd0e92
                                         Fmt 26  FC 52  Card 63612
```

43 of 160 single captures across a 32-phase sweep decode exactly; the empty field produced
the truth **0 times in 160**. No stacking, no folding, no averaging, and it works at the
stock 8-bit sample width.

## The decode, end to end

Everything below is integer arithmetic over 4096 samples. No float, no FFT.

```
sample at 125 kHz, carrier-locked, phase in the working window
  -> mix by (-1)^n                    fc/2 is exactly fs/2, so this is the whole mixer:
                                      no oscillator, no phase estimate. DC and slow drift
                                      move UP to fs/2 and fall out.
  -> low-pass the baseband (~12 kHz)  NOT optional, see C03
  -> 32-sample boxcar per bit         the optimal filter for a rectangular bit at RF/32
  -> threshold: bit = (integrator > 0)  PSK1 — the phase IS the data, see C02
  -> search preamble64 = 1010 + 28 zeros + 1, normal and inverted
  -> read 64 bits from the preamble position
```

⚠ **Three details are each individually fatal.** Any one of them alone takes the decode
from 43/160 to 0/160 or near it: the PSK1 mapping (C02), the baseband low-pass (C03), and
**not** discarding the settle window (C04).

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
| C01 | The Chameleon Ultra decodes Indala from one 300 ms capture: 43/160 exact | 160 | emptyfield 0/160 | ✓ Proxmark reads the same tag | L44 |
| C02 | **PSK1: the phase IS the data.** Not differential — differential is `psk1TOpsk2`, the Proxmark's *fallback* | — | — | ✓ `cmdlfindala.c:1259` vs `:1293` | L44 |
| C03 | The baseband low-pass is load-bearing: **32/35** real captures decode with it, **0/35** without | 35 | — | ✓ real captures, not synthetic | L44 |
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
| C15 | Stock firmware has no PSK demodulator: `reader/lf/*_data.c` are ASK or FSK, `psk1.c` is transmit-only | — | — | ✓ source | L01 |

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

### Open / untested

| claim | status |
|---|---|
| Tag position is worth ~5.7 dB | n=1, from an accidental probe. Large, concentrated at high frequency, and plausible — but one capture |
| Settle, air gap, oversampling | closures invalid (L34); never re-measured against a working decoder |
| Is the phase window tag- or unit-specific? | untested. Check a second Indala tag and a second Chameleon before hard-coding a phase |
| Indala parity | unused. Most near-misses are 1–2 bits in the zero run; the parity would reject them |

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
