# Indala on Chameleon Ultra — investigation summary

**2026-09-10/11.** Chameleon Ultra v3 (chip `a461ebf3b85fb19c`), Proxmark3 Iceman as
reference, T5577 as the tag under test. Branch `indala-psk-read` on
`mfcarroll/ChameleonUltra`. Full working and every retraction: `README.md`.

---

## The question

A newly bought Chameleon Ultra reads some LF tags but not an indala26. Is the tag
unsupported, or was something being done wrong?

## The answer

**Unsupported, and it is the analog receive chain — not the missing PSK demodulator.**

Indala is PSK1, RF/32, with its subcarrier at fc/2 = 62.5 kHz. The firmware has no PSK
demodulator, which is real and easy to find. But closing that gap would not help: the
subcarrier does not arrive in usable condition.

Measured against the Proxmark on identical stimulus, 5 repeats per point, clean captures,
verified empty-field baseline (`campaign_20260910_231740`):

| PSKCF | subcarrier | smp/cyc | Chameleon | empty | **SNR** | Proxmark | **excess loss** |
|---|---|---|---|---|---|---|---|
| RF/8 | 15625 Hz | 8.0 | 1909.60 | 107.36 | 17.8x | 73.95 | ref |
| RF/4 | 31250 Hz | 4.0 | 764.13 | 13.10 | **58.3x** | 44.68 | −3.6 dB |
| RF/2 | 62500 Hz | 2.0 | 17.86 | 6.37 | **2.8x** | 25.15 | **−31.2 dB** |

Repeat spreads 1.00x / 1.00x / 1.12x.

**Budget at fc/2:** 31.2 dB excess over the Proxmark, of which ~7.5 dB is recoverable by
sample phase, leaving **~24 dB the front end owns**.

⇒ The deciding figure is the SNR column. fc/2 sits **2.8x** above the noise floor where
RF/4 — which this device reads without trouble — sits at **58x**. A ~20x SNR deficit,
entirely ahead of the ADC.

## ⛔⛔ STATUS 2026-09-11: the demod test is retracted, and the tag state was wrong

Found by the operator: `lf indala reader` failed on the bench tag and it had to be rewritten
with the Indala config before the Proxmark would read it.

Every PSK1 cell in the campaign registry writes `DEADBEEF`/`12345678`. Block 0 is the
correct Indala **air shaping**; the data blocks are not an Indala **frame**. So from the
first campaign onward the tag broadcast PSK1 RF/32 with a payload no Indala demodulator
accepts — and the offline PSKDemod negatives say nothing about the Chameleon.

⚠ The harness reported this on every run ("data blocks UNVERIFIED", plus a ROT-FIX reverify
naming `DEADBEEF`/`12345678`). It was misread as *PM3 cannot see them* rather than *they are
not what you want*.

| | |
|---|---|
| demod attempt | **void** |
| absolute SNR vs threshold | **biased +1.9 dB** — the campaign payload is bit-dense and puts 1.24x more energy in the measured skirt than `a0000000e6bd0e92`, which has a 32-bit zero run |
| instrument-relative loss (−3.6 dB RF/4, −31.2 dB fc/2) | **survives** — both instruments saw the same tag and payload |
| lever closures, firmware bugs | **survive** — relative, and payload-independent |

⛔ **No measurement in this project was ever taken on a correctly-configured Indala tag at
full resolution on clean captures.** Two tags were involved and neither gave one.

Fixed at the root: an `INDALA26` registry cell now carries the real credential with block 0
identical to `PSK1`. See `README.md` §0c and §6 for the re-measurement plan.

## Levers tried, and closed by measurement## Levers tried, and closed by measurement

| lever | result | where |
|---|---|---|
| 8-bit truncation in the sniff path | **real bug, fixed.** `>>5` discarded 5 of 14 ADC bits and *distorted the rolloff shape*, not just the level | §0b |
| SAADC sample phase | real, one-cycle `cos φ` at R²=0.90, worth **7.5 dB**, never nulls | §0a |
| oversampling at 200 kHz | **nothing.** Removing the Nyquist degeneracy entirely recovers none of it | §0 |
| SAADC gain 1/6 → 1/3 | **nothing.** The noise floor tracks gain, so it is analog-referred and the ADC was never the limit | §0a |
| differential vs `LF_RSSI` | dead by implication — gain cannot beat an analog floor | §0a |
| settle 2–250 ms | **nothing.** Flat within 8% across a 125x range | §0a2 |
| air gap flat–8 mm | **nothing useful.** Response *declines* as the gap opens; flat is already optimal | §0a3 |
| field dropouts ("overruns") | **real bug, fixed.** BLE advertising was collapsing the field; 41% of captures corrupted → 0% | §0a4 |
| DMA ring 512 vs 2048 batch | **real bug, fixed.** 75% of every batch dropped; captures were islands spanning 64 ms, not 16 ms | §0a4 |

Untried: field-drive duty (hardcoded 50%). The gap sweep argues against it — varying
coupling 3.2x did not move the response, and drive varies much the same thing.

## What was built along the way

**Firmware** (all flashed and verified on device):
- `lf sniff --bits 16` — full 14-bit conversion instead of `>>5`. Generally useful.
- `lf sniff --phase N` — programmable ADC sample phase via TIMER3, 62.5 ns resolution.
- `lf sniff --rate N` — free-running oversampled trigger, decoupled from the carrier.
- `lf sniff --gain N` — SAADC input gain.
- `lf sniff --settle N` — field-on time before the capture window.
- BLE advertising suspended during capture.
- DMA ring sized to a whole batch; heap raised to cover it.
- Deduplicated `raw_read_to_buffer`'s prototype, which went stale on both signature changes.

**Tooling:**
- `software/script/cu.py` — non-interactive CLI runner (the stock client has no batch mode).
- `firmware/flash-dfu-app-macos.sh` — Docker build + `nrfutil` flash on macOS.
- `--reader chameleon` and the `PSK1-CF4`/`PSK1-CF8` cells in the Momentum `t5577_campaign.py`
  harness, plus two pre-existing harness bugs fixed (a `--no-pm3` crash, and a verify
  verdict that reported a successful write as a mismatch against itself).
- `sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py`, `oversample_test.py`,
  `analyse.py`, `grab.sh`, `fieldhold.sh`.

## Retraction log — nine wrong conclusions, all mine

Recorded because the artefacts are the reusable part.

| # | claim | why it was wrong |
|---|---|---|
| 1 | "fc/2 cancels at Nyquist — that is the root cause" | The Proxmark samples at 125 kHz synchronously too, and reads Indala |
| 2 | "The tag produces no detectable modulation" | Different tag, hand-held; and my glitch screen discarded 90 of 90 windows and returned `nan` |
| 3 | "−27.5 dB analog loss" | Measured at the exact subcarrier bin: BPSK suppresses its own carrier there, and at fc/2 the bin is degenerate (reads 0–2x the truth by phase) |
| 4 | "The sampler is the blocker" | Rested on RF/4 showing +0.5 dB, which was one glitchy capture; clean it is −3.6 dB |
| 5 | "659x at 44 ticks — `cos φ` confirmed" | One capture per phase, tracking dropout bursts; the 659x was a small denominator |
| 6 | "The tag never rises above the empty field at any phase" | Contradicted by its own table — the tag led at all 32 phases. A hard-coded `> 3` threshold got 2.90 and fell through |
| 7 | "fc/2 does not recover when oversampled" | Right answer, **read off `nan`** — writer and reader formatted the same filename with two `%d` swapped |
| 8 | "Settle converges the capture" | Three single captures; with repeats the same measure is random |
| 9 | "Gap is flat, nothing here" | Right conclusion, wrong description — the raw ratio carried the noise floor in both terms and faked a recovery at wide gaps, hiding a monotonic decline |

⛔⛔ **The one principle worth carrying:** on this device a single capture is not evidence of
anything. Before the BLE fix, 41% of captures carried a field dropout, and six consecutive
captures at a fixed setting spread **49x**. Three of the nine retractions above are
single-capture conclusions drawn *after* that had been measured and written down.

## What would move it

Only hardware. The 1st filter pole is R17 4k7 / C38 1nF ≈ 34 kHz; `LF_OA` (the raw
peak-detector output, upstream of both filter stages) is the only node with more fc/2
content, and it reaches a pin only through `LF_RSSI`'s 470k. See `ADVERSARIAL.md` §2 — that
tap has never been measured, and it is the strongest thing this investigation may have
missed.
