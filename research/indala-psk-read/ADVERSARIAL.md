# Adversarial review prompt — Indala on Chameleon Ultra

Paste this to a fresh agent with the repo available. It is written to be hostile to its own
conclusions on purpose.

⚠ **The prior that the current conclusion is also wrong should be HIGH.** This investigation
produced **eleven** wrong conclusions before the current one, every single one from an
artefact in its own analysis rather than the hardware. Its previous "final" conclusion —
that the read was not viable, 31 dB short — was itself falsified, and the thing that
falsified it was a two-line convention bug that had been sitting in the decoder the whole
time while a self-consistent unit test reported PASS.

---

## Your task

Falsify this conclusion, or find what it is still getting wrong:

> An unmodified Chameleon Ultra **reads Indala** (PSK1, RF/32, fc/2 = 62.5 kHz subcarrier).
> 43 of 160 single 300 ms captures decode `a0000000e6bd0e92` exactly; the empty field
> produced the truth 0 times in 160. No stacking, no averaging, stock 8-bit sample width.
> The read requires a sample phase in ticks 4–60 (best 12–36; the stock phase 0 fails), a
> baseband low-pass, and **not** discarding the settle window.

Do not take the write-up at face value. Re-derive from the committed captures — every number
above is reproducible offline with no hardware.

⭐ **The sharpest questions to put to it:**

1. **Is the decode real, or is the decoder finding what it was told to look for?** It
   brute-forces sample offsets and matches a known 33-bit preamble against a known 64-bit
   answer. What is the false-positive rate of that procedure on noise? The empty-field null
   is 0/160 — verify that independently, and check whether the near-misses
   (`a0100000e6bd0e92`) are evidence of a marginal read or of a decoder straining.
2. **Does it generalise?** One tag, one unit, one coupling geometry, one session. The phase
   window in particular could be an artefact of this tag's position.
3. **Was PSK1-vs-PSK2 really the root cause, or a coincidence that happens to work here?**
   Check the claim against the ATA5577 datasheet as well as the Proxmark source.
4. **What else is a self-consistent test?** The `synth()` bug survived because the encoder
   and decoder shared a convention. Look for the same shape elsewhere in the tooling.

## Where everything is

- ⭐ Current knowledge and the claims ledger: `research/indala-psk-read/FINDINGS.md`
- Method rules, each tied to the wrong conclusion that earned it: `research/indala-psk-read/METHOD.md`
- What was learned when, indexed to git: `research/indala-psk-read/LOG.md`
- ⛔ The old working notes, **frozen and mostly retracted**: `research/indala-psk-read/archive/`
- The working decoder: `research/indala-psk-read/mfdemod.py`
- Phase sweep, 32 phases x 5 repeats x tag/empty: `research/indala-psk-read/caps/phasebits/`
- AIN5-vs-AIN0 paired captures: `research/indala-psk-read/caps/inputtest/`
- Final clean PSKCF dataset: `research/campaigns/campaign_20260910_231740/`
  — `raw/*.bin` Chameleon, 16-bit big-endian, 125 kHz, 5 repeats per config
  — `pm3_signal/*.pm3` Proxmark reference, one signed integer per line, 125 kHz
- Empty-field baseline: `research/indala-psk-read/caps/baseline16_r*.bin`

Reproduce the headline result with no hardware:

```bash
cd research/indala-psk-read && ../../software/script/.venv/bin/python phasebits.py --analyse-only --keep caps/phasebits --step 4 --repeats 5
```

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

## How this investigation went wrong eleven times — pattern-match against it

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

---

# Adversarial review prompt 2 — the clock-slip conclusion

⚠ Same standing warning as above: this investigation has produced eleven wrong conclusions,
every one from an artefact in its own analysis. Treat the prior that this one is also wrong as
HIGH. It is marked **likely closed**, not closed.

## The conclusion to falsify

> Our emulated subcarrier free-runs **131 ppm** off the reader's clock, so a whole subcarrier
> cycle of phase error accumulates every **~122 ms**. That is why the Proxmark decodes only
> 131–197 ms of a capture from our emulator but the **full 290 ms** from a T5577 playing the
> same frame in the same field — 5 of 5, on *less* signal. The burst boundary is excluded
> (C135), amplitude is excluded (C138), and the reader's demodulator is excluded (C138).

Claims: **C134, C135, C136, C137, C138, C139**. Tools: `clockoffset.py`, `emutest.py`,
`offsetsweep.py`, `burstnull.py`. Everything except the bench arms re-runs offline.

## What has already been tried, so you do not repeat it

- **Burst harmonics faking the tone.** The burst cycle is ~510 ms, its 4th harmonic ~7.9 Hz,
  the measured tone 8.20 Hz, the FFT bin 3.44 Hz — unresolvable by arithmetic. Simulated with
  zero offset and 0–8 gaps: never reaches the gate, and at 131 ppm the tone stays at
  8.17–8.21 Hz *regardless of burst length* (C139).
- **The estimator inventing tones.** At a true zero it returns nonsense, but with peak/median
  ≈3 against ≥10 for a real tone. That ratio is the gate, and it was fixed from synthetic data
  **before** the bench (C137).
- **A lag-based estimator.** Tried first, failed its own residual check, and is documented
  inside `clockoffset.py` rather than deleted.

## ⭐ The sharpest questions

1. ⛔ **IS IT CAUSAL, OR ONLY CORRELATED?** This is the biggest hole and it is admitted. Two
   sources differ in their clock AND in being locked/free-running AND in coupling, damping and
   modulation depth — and one decodes further. Nothing has ever *changed the offset and watched
   the ceiling move*. ⭐ **The experiment that would settle it: deliberately detune our own
   subcarrier and predict the ceiling quantitatively.** At 500 ppm the model says one cycle of
   slip in ~32 ms, so the ceiling should collapse to roughly a quarter of what it is now; at
   ~30 ppm it should rise past 290 ms and the emulator should behave like the tag. That is one
   firmware build per point and it turns a correlation into a law — or kills it.
2. **Whose clock is off?** 131 ppm exceeds the nRF52 HFXO spec (±40 ppm) on its own, so the
   Proxmark's sampling clock carries some of it. ⚠ If most of it is the reader's, then "122 ms"
   is a property of *this pair*, not of our emulator, and the general claim is weaker than it
   reads — against a better-matched reader the window would be longer. Separating them needs a
   third clock. Does that change any decision made on the back of it?
3. **Is `peak/median` sound on real data?** It was calibrated on synthetic noise that is white.
   The LF chain's floor spans 17× across bands. Can coloured noise inflate the ratio past 6
   without a real tone?
4. **Is the control's silence trustworthy, or just weaker coupling?** The tag read fc/2 16.0
   against the emulator's 17.9. The decode argument survives that (less signal, further decode)
   — but does the *estimator* argument? Would a genuinely offset source at 16.0 still gate in?
5. **Does the prefix bias change the SHAPE?** C136 shows `emutest.py` under-reports the
   emulator. The tag hit the ceiling of the instrument so it cannot be biased upward. Re-run
   both arms with `offsetsweep.py` and check the conclusion is not an artefact of that asymmetry.
6. **The sign.** Squaring loses it. Is there any reading in which the offset is negative and
   something else sets the ceiling?
