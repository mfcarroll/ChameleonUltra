# Where to pick this up — ranked, with why

⭐ **Resuming in a fresh session? Paste `RESUME.md`** — self-contained context, environment and commands.

**State at 2026-09-11 (final).** ⭐⭐⭐ **THE CHAMELEON ULTRA READS INDALA.** 43 of 160
single 300ms captures decode `a0000000e6bd0e92` exactly — 5/5 at ticks 12, 20 and 36,
across a working window of ticks 4–60. The empty field produced the truth **0 times in
160**. No stacking, no folding, and it works at the **stock 8-bit** sample width.

⛔ **The deficit was a software bug.** `mfdemod.py` demodulated **PSK2 against a PSK1
tag** — in PSK1 the phase *is* the data, and it was differential-decoding, which is the
Proxmark's `psk1TOpsk2()` **fallback** path applied as the primary. `synth()` encoded with
the same wrong convention, so the self-test was self-consistent and passed forever. See
`README.md` §0! and §0!a.

⇒ **"31.2 dB short", "7.6 dB demodulation gap", "detectable but not decodable" and "the
phase is gone before the ADC" are all retracted.** `README.md` §0!b has the full table of
what stands and what does not.

---

## 1. ⭐⭐⭐ Port the decoder into firmware — this is now a feature, not a research question

The whole read is: sample at 125kHz with a phase in the working window → mix by `(-1)^n` →
low-pass → 32-sample boxcar per bit → threshold → search `preamble64` → read 64 bits.

**Everything it needs already exists on the device.** `lf_reader_generic.c` captures the
samples; the rest is integer arithmetic over 4096 samples. No float, no FFT — the low-pass
can be a short FIR or a two-stage boxcar.

⚠ **Three things are load-bearing and each one alone takes it to zero:**
- **the PSK1 mapping** — the phase IS the data (`mfdemod.py:polarity_from_bits`);
- **the baseband low-pass** — 32/35 with it, **0/35** without;
- **NOT discarding the settle window** — 43/160 with the full capture, **0/160** with the
  400-sample discard this project used throughout.

Then `lf indala read` alongside `lf em410x read` and the rest.

## 2. ⭐⭐ Set the sample phase, and check it on a second tag

Ticks 12–36 decode best, and nothing decodes past tick 60. The stock trigger is phase 0,
which sits at the edge of the window (0/5) — so **the stock firmware would fail even with
a correct decoder**, which is worth knowing before blaming an antenna.

⚠ The window's position may be specific to this tag's coupling and this unit. Check a
second Indala tag and, ideally, a second Chameleon before hard-coding a phase. A short
scan across three or four phases is cheap insurance.

## 3. ⭐ Check the Indala parity to reject near-misses

Most failures are one or two bits in the zero run (`a0100000e6bd0e92` for
`a0000000e6bd0e92`). Indala carries a parity — the Proxmark prints `Parity: 11` — so
checking it would reject most near-misses and turn a 27% raw decode rate into a much
higher effective rate with retries.

## 4. ⭐ Re-test the levers that were closed against a broken decoder

Air gap and settle were closed pre-BLE-fix on a tag carrying `DEADBEEF/12345678`, and every
"dB" measured since was measured through a decoder that could not decode. Now that there
is a real success metric — **decode rate** — they are worth a few minutes each.

## 5. ⛔ CLOSED — `LF_RSSI` (AIN0) carries no fc/2

`README.md` §0z5, 7 repeats. Within-node tag/empty **1.04x against a 1.28x scatter**;
spectrum flat to **0.5 dB** across 1–62kHz where AIN5 rolls off 31.6 dB. Not clipped, and
demonstrably alive — the tag shifts its DC by +16 counts with ±0 spread. A DC
field-strength indicator, corner below 1kHz. Independent of the decoder, so it stands.

## 6. ⛔ CLOSED — gain. The floor is analog-referred

`README.md` §0a, 7 repeats, deglitched. Also independent of the decoder.

---

## Method rules this project paid for

1. ⭐⭐⭐ **A round-trip self-test proves the encoder and decoder agree with each other,
   not with the world.** `synth()` encoded with the same wrong convention `demod()`
   inverted. It passed at every SNR, produced a "+8.2 dB" figure, and that figure was used
   to conclude the hardware was 31 dB short. Generate test vectors from an INDEPENDENT
   source — here, the Proxmark's own `preamble64` and a real capture.
2. ⭐⭐ **When a synthetic beats reality, suspect the synthetic.** A synthetic frame at
   half the tag's amplitude decoded while the tag did not. That was read as proof the
   hardware destroyed the phase. It was proof the synthetic shared the decoder's bug.
3. ⭐⭐ **Check the reference implementation's ORDER, not just its existence.** The
   Proxmark has both `psk1TOpsk2()` and a direct preamble match. Reading which one is the
   primary and which the fallback was the entire answer, and it was two greps away.
4. ⛔ **A single capture is not evidence** — pre-fix, 41% carried a field dropout and six
   consecutive captures spread 49x.
5. ⛔ **Always run the null, and make it the empty field**, not white noise.
6. ⛔ **Confirm the tag state, and understand what the confirmation says.**
7. ⛔ **No thresholds in verdicts.** Fit the shape and report the fit.
8. ⛔ **Check that a measure is comparable before comparing it.** Band RMS across bands
   whose floors differ 17x; AIN0's counts against AIN5's floor; a phase-32 capture against
   a phase-0 baseline.
9. ⛔ **Make sure the measure can see the thing you are claiming.** The fc/2 band-SNR
   criterion is polarity-blind, and a whole-frame Hamming score credits a constant run.
   Both passed while nothing was decoded.
10. ⚠ **Question the conventions, including the ones that look like hygiene.** Discarding
    400 "settle" samples was never questioned and it alone took the decode from 43/160
    to 0/160.
