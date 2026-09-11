# Where to pick this up — ranked, with why

⭐ **Resuming in a fresh session? Paste `RESUME.md`** — self-contained context, environment and commands.

**State at 2026-09-11.** The channel is measured and trustworthy. The demodulation gap is
**7.6 dB**, confirmed twice by independent routes (spectral band SNR, and a ~19% bit-error
rate implying ~7.7 dB). The signal at fc/2 is **detectable but not decodable**: 52/64 bits
against a 45/64 white-noise null and a 42/64 empty field.

⛔ **What is NOT established is that 7.6 dB is unbridgeable.** Only two levers were ever
measured under fully valid conditions (§0b2) — sample phase, and gain's confound test.

---

## 1. ⭐⭐⭐ `LF_RSSI` (AIN0) — the upstream tap. Never tried.

    ANT -> VD1 detector -> LF_OA -> [C28 10n / R9 82 / C36 33n] -> IC1A (R17 4k7 / C38 1n)
                             |                                        -> IC1B -> LF_OA_OUT (AIN5)
                             \-> R12 470k -> LF_RSSI (AIN0)

`LF_OA` is the raw peak-detector output. **Both filter poles (≈59 kHz and ≈34 kHz) are
downstream of it.** Every capture in this entire project sampled AIN5, after both. `LF_RSSI`
hangs off `LF_OA` and lands on AIN0, which the SAADC can select.

⇒ **If the deficit lives in the filter stages, this is where it is recoverable — and it is
the only node upstream of them that reaches a pin.** Plausibly worth far more than any
software lever, because it attacks the loss rather than compensating for it.

**Do:** in `ble_main.c`, `register_lf_adc_callback()`, change the channel from
`NRF_SAADC_INPUT_AIN5` to `AIN0`. Rebuild, flash, capture paired empty/tag at the optimal
phase, compare fc/2 against AIN5.

⚠ **Reasons it may be dead, which is exactly why it must be measured and not argued:** 470k
source impedance is far above what the SAADC wants (accurate conversion would need
`ACQTIME_40US`, capping the rate near 24 kHz — and fc/2 needs ≥125 kHz); VD2 plus stray and
pin capacitance likely low-passes it hard; and the node is *designed* as a slow RSSI
indicator, which is why LPCOMP uses it for field-presence wake.
⇒ A single paired capture settles it either way. If the 470k is fatal, that is a finding.

## 2. ⭐⭐ Noise whitening in the demodulator

The matched filter's measured 2.13x threshold (+8.2 dB over PSKDemod) was against **white**
noise. This chain's noise is strongly coloured — the empty-field floor is 107 / 13 / 6.4
across the fc/8, fc/4, fc/2 bands, a 17x range. A matched filter is optimal only for white
noise; for coloured noise it needs a whitening stage first.

⇒ **This is very likely why 2.13x did not transfer to real captures.** Estimate the noise
PSD from the empty-field baselines already committed in `caps/`, whiten, then re-run
`mfdemod.py`. Costs no bench time — the captures are on disk.

## 3. ⭐ Field-drive duty — never tried

`m_lf_125khz_pwm_seq_val = {2,0,0,0}` with `top_value 4` — a hardcoded 50% duty, never
varied. It changes the **detector's operating point**, not just coupling, so the air-gap
sweep does not stand in for it (that varied coupling 3.2x without moving the response).

## 4. Re-test settle and air gap — both closures are invalid

Both were closed *before* the BLE dropout fix and on a tag carrying `DEADBEEF/12345678`
rather than an Indala frame. Both conclusions were **nulls**, and a null is exactly what
noisy data manufactures. Cheap to redo now: `INDALA26` programmed, clean firmware,
`--repeats`.

## 5. Gain — ~1–1.5 dB, measured but unclaimed

The closure survives its confound test, but the floor scales *under* the gain ratio
(≈1.4–1.75x for a 2.0x step), implying a partial ADC-referred component. Small, real, and
currently left on the table.

## 6. ⛔ Do NOT bother with coherent frame averaging

Measured and dead: 1/2/4/8/16/30 aligned frames give 51, 52, 53, 52, 53, 48 bits, where 30
frames should buy ~14.8 dB. Alignment is driven by noise — you need SNR to align frames and
alignment to gain SNR, and at 19% BER the inter-frame correlation cannot break the circle.

---

## Method rules this project paid for

1. ⛔ **A single capture is not evidence.** Pre-fix, 41% of captures carried a field
   dropout and six consecutive captures at one setting spread **49x**. Three wrong
   conclusions came from n=1 *after* that was measured and written down.
2. ⛔ **Always run the null.** The "51–54/64 bits" looked like strong detection until white
   noise scored 45/64 through the same max-over-2048 procedure.
3. ⛔ **Confirm the tag state, and understand what the confirmation says.** The harness
   reported `data blocks UNVERIFIED` and named `DEADBEEF/12345678` on every run; it was read
   as "PM3 cannot see them" rather than "they are not what you want", invalidating a day.
4. ⛔ **A synthetic threshold is not a real threshold.** The matched filter's +8.2 dB held
   against white noise and vanished on real captures.
5. ⛔ **No thresholds in verdicts.** `max ratio > 3?` got 2.90 and printed a conclusion its
   own table contradicted. Fit the shape and report the fit.
6. ⛔ **Check that a measure is comparable before comparing it.** The band-RMS measure was
   compared across bands whose noise floors differ 17x.
