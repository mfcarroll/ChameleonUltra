# Adversarial review prompt — Indala on Chameleon Ultra

Paste this to a fresh agent with the repo available. It is written to be hostile to its own
conclusions on purpose: the investigation produced **nine** wrong conclusions before the
final one, every single one from an artefact in its own analysis rather than the hardware,
so the prior that the tenth is also wrong should be high.

---

## Your task

Falsify this conclusion, or find the lever it missed:

> Reading Indala (PSK1, RF/32, fc/2 = 62.5 kHz subcarrier) on an unmodified Chameleon Ultra
> is not viable. The firmware has no PSK demodulator, but that is not the binding
> constraint: the receive chain loses ~31 dB at fc/2 relative to a Proxmark3 on identical
> stimulus, of which ~7.5 dB is recoverable by ADC sample phase and **~24 dB is analog,
> ahead of the ADC**. At fc/2 the tag sits 2.8x above the empty-field noise floor, where
> RF/4 — which this device reads without trouble — sits at 58x.

Do not take the write-up at face value. Re-derive from the captures.

## Where everything is

- Findings, with every retraction banded in place: `research/indala-psk-read/README.md`
- Condensed: `research/indala-psk-read/SUMMARY.md`
- Final clean dataset: `research/campaigns/campaign_20260910_231740/`
  — `raw/*.bin` Chameleon, 16-bit big-endian, 2000 samples @125 kHz, 5 repeats per config
  — `pm3_signal/*.pm3` Proxmark reference, one signed integer per line, 125 kHz
- Empty-field baseline: `research/indala-psk-read/caps/baseline16_r*.bin` (5 captures)
- Analysis: `sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py`, `oversample_test.py`
- Firmware knobs added: `lf sniff --bits 16 --phase N --rate N --gain N --settle N`
- Schematic: `hardware/ultra/Chameleon_nrf52_ultra_V1.0.pdf` (LF is sheet 2)
- Build + flash on macOS: `firmware/flash-dfu-app-macos.sh`

Signal chain, from the schematic:

    ANT -> VD1 detector -> LF_OA -> [C28 10n / R9 82 / C36 33n] -> IC1A (R17 4k7 / C38 1n)
                             |                                        -> IC1B -> LF_OA_OUT (AIN5)
                             \-> R12 470k -> LF_RSSI (AIN0)

---

## Ranked attack surface — start at the top

### 1. ⭐⭐⭐ `LF_RSSI` taps UPSTREAM of both filter stages, and was never measured

`LF_OA` is the raw peak-detector output. Both filter poles (≈59 kHz and ≈34 kHz) sit
*after* it. `LF_RSSI` hangs off `LF_OA` through R12 470k and lands on **AIN0** — a pin the
SAADC can sample.

⇒ If the ~24 dB is lost in the filter stages, `LF_RSSI` should carry materially more fc/2
than `LF_OA_OUT` does. **Nobody has looked.** The whole investigation sampled AIN5 only.

The write-up dismissed this route once, for the wrong reason — it argued differential mode
against `LF_RSSI` was pointless because the noise floor is analog-referred. That answers a
*gain* question. It says nothing about whether `LF_RSSI` is a better **tap**.

⚠ Reasons it may still be dead, which is why it needs measuring and not arguing:
- 470k source impedance is far above what the SAADC wants for a short acquisition window;
  accurate conversion would need `ACQTIME_40US`, capping the rate near 24 kHz.
- VD2 plus stray/pin capacitance likely low-passes the node hard.
- It is *designed* as a slow RSSI indicator — LPCOMP uses it for field-presence wake.

**Test:** point the LF SAADC channel at `NRF_SAADC_INPUT_AIN0` (`ble_main.c`,
`register_lf_adc_callback`), rebuild, capture with and without the tag, compare fc/2 against
AIN5. One firmware change and one paired capture pass.

### 2. ⭐⭐⭐ Nobody ever attempted an actual demodulation

Every conclusion rests on **band energy**, never on recovering a bit. "2.8x above the floor"
is a spectral ratio, not a demod result. A matched filter coherently integrating over a
2048-sample frame could plausibly do far better than a band-power ratio suggests.

⚠ And there is a hard obstacle the write-up never states: **a 64-bit Indala frame is exactly
2048 samples at 125 kHz, and the 16-bit captures are 2000.** *No full-resolution capture in
this investigation contains one complete frame.* 8-bit captures hold 1.95 frames at 32x
worse resolution. `LF_SNIFF_MAX_SAMPLES` is 4000 **bytes**.

**Test:** run Proxmark's `PSKDemod()` / `detectIndala()` (`proxmark3/common/lfdemod.c`,
`client/src/cmdlfindala.c`) over `campaign_20260910_231740/raw/s03_PSK1_*.bin`. If it
recovers `a0000000e6bd0e92` (FC 52, card 63612 — the Proxmark's own read of this tag) the
headline conclusion is simply wrong. If it does not, raise `LF_SNIFF_MAX_SAMPLES` for a
multi-frame 16-bit capture and try again before believing it.

### 3. ⭐⭐ The 7.5 dB phase figure and the 31.2 dB loss come from different eras

`31.2 − 7.5 ≈ 24` mixes measurements. The 31.2 dB is from clean captures (post BLE fix); the
7.5 dB phase sweep predates it, when 41% of captures carried a field dropout. Re-run
`phasesweep.py` on the current firmware. If phase is worth materially more than 7.5 dB on
clean data, the residue shrinks and the conclusion weakens.

### 4. ⭐⭐ Is band RMS comparable across three different frequencies?

`sideband_rms()` integrates `f−5000 .. f−200`. The empty-field floor in those bands is
**107.36 / 13.10 / 6.37** — a 17x range. The method then normalises each instrument to its
own RF/8 and subtracts. Attack whether that cancels anything, given the bands differ that
much in noise content and in how PSK1's spectrum falls inside them. A wrong answer here
moves every dB figure in the write-up.

### 5. ⭐⭐ Is the Proxmark a valid reference?

The method divides out "the tag's own rolloff" by normalising each instrument to its own
RF/8. That assumes the tag emits the same spectrum into both readers. They have different
antennas, tuning and coupling, and a T5577's output depends on how hard it is driven.
Note the Proxmark *also* shows −9.4 dB at fc/2. If part of that is the PM3's own chain
rather than the tag, the "excess" attribution shifts.

### 6. ⭐ Could the deglitch screen be eating fc/2?

`deglitch()` drops 50-sample windows whose peak-to-peak exceeds 4x the median window's. fc/2
is the smallest signal in the dataset. Show whether the screen is frequency-selective. It
was validated only by "the tag/empty ratio does not change much".

### 7. ⭐ Untried knobs

- **Field-drive duty**: `m_lf_125khz_pwm_seq_val = {2,0,0,0}`, `top_value 4` — hardcoded
  50%, never varied. Argued down because the gap sweep moved coupling 3.2x without moving
  the response, but duty also changes the *detector's operating point*, which gap does not.
- **`LF_SNIFF_MAX_SAMPLES`** = 4000 bytes. Raising it buys longer coherent integration and
  full frames at 16-bit. Why 4000? Is the "USB frame limit" comment actually a limit?
- **SAADC hardware oversample/burst** (`NRFX_SAADC_CONFIG_OVERSAMPLE`) — dismissed as
  low-pass, never measured.
- **`READER_POWER` / `LF_AMP_PWR`** (P1.15) is a plain GPIO feeding the op-amp bias divider
  (R8 4k7 / R10 3k → `LF_VBIAS` ≈ 1.2 V). PWM'ing it moves the amplifier's operating point.
  Dismissed as "probably shifts DC, not bandwidth" — not measured.

### 8. Claims that should be easy to confirm, so confirm them and move on

- No PSK demodulator exists in firmware; every `reader/lf/*_data.c` is ASK or FSK; `psk1.c`
  is transmit-only and used solely by `idteck.c`; `lf idteck` has no `read`.
- Indala is a commented-out placeholder under `//////// PSK Tag-Talk-First 300`
  (`tag_base_type.h:61`).
- No generic LF identify opcode exists, so the GUI's "generic LF read" can only be rotating
  the per-protocol scans.

---

## How this investigation went wrong nine times — pattern-match against it

1. Reasoned from one device's architecture without checking a device that already does the thing.
2. A glitch screen tuned on null data discarded **90 of 90** windows of a strong capture and returned `nan`.
3. Measured at a spectral bin that is degenerate at Nyquist and where BPSK suppresses its own carrier.
4. Built a diagnosis on **one** capture from a device whose repeats spread **49x**. Three separate times.
5. A hard-coded threshold (`max ratio > 3?`) got 2.90 and printed a verdict its own table contradicted.
6. Writer and reader formatted one filename template with two `%d` swapped; a verdict was read off `nan`.
7. A ratio carrying the noise floor in both terms faked a recovery once the signal vanished.

⛔ **If you find yourself about to report a conclusion from a single capture, stop.** Before
the BLE-advertising fix 41% of captures carried a field dropout; six consecutive captures at
one fixed setting spread 49x. Use `--repeats`, deglitch, take medians, and check the
`spread` column.

## What a real refutation looks like

- A demodulated Indala ID from a Chameleon capture. Decisive.
- fc/2 materially stronger on AIN0 than AIN5, paired, with repeats.
- A phase sweep on clean firmware showing much more than 7.5 dB.
- A demonstration that `sideband_rms` is not comparable across the three bands, with a
  measure that is, changing the dB figures.

## What does NOT need re-litigating

The firmware gap is real and documented. The device reads HID Prox, ioProx, EM410x fine, so
the LF path works. The tag works — the Proxmark reads it as `a0000000e6bd0e92`, FC 52, card
63612, and identifies it as `T55x7, PSK1, RF/32`. None of that is in question.
