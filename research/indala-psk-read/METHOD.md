# Method rules

⛔ **APPEND-ONLY.** Every rule here was paid for with a wrong conclusion. Add rules; do not
quietly drop them because they feel obvious now. Each one felt obvious *after*.

**Eleven conclusions in this project were wrong. Every one came from an artefact in the
analysis, not from the hardware.** That is the single most useful fact about this bench.

---

## The three that cost the most

**M1. ⭐⭐⭐ A round-trip self-test proves the encoder and decoder agree with each other, not
with the world.** `synth()` encoded PSK with the same running XOR `decode_from()` inverted.
A self-consistent bug passes every round trip, at every SNR, forever. It reported PASS,
produced a "+8.2 dB over PSKDemod" figure, and that figure was used to conclude the
hardware was 31 dB short for a day and a half.
⇒ **Generate test vectors from an independent source.** Here: the Proxmark's own
`preamble64`, and a real capture.

**M2. ⭐⭐ When a synthetic beats reality, suspect the synthetic.** A synthetic frame at half
the tag's measured amplitude decoded in the tag's own recorded noise, while the tag did
not. That was read as proof the hardware destroyed the phase. It was proof the synthetic
shared the decoder's bug.

**M3. ⭐⭐ Check the reference implementation's ORDER, not just that it exists.** The
Proxmark has both `psk1TOpsk2()` and a direct preamble match. Which one is primary and
which is the fallback *was the entire answer*, and it was two greps away for a day.

## Measurement

**M4. ⛔ A single capture is not evidence.** Pre-fix, 41% of captures carried a field
dropout and six consecutive captures at one setting spread **49x**. Use repeats, deglitch,
take medians, watch the spread column. Three wrong conclusions came from n=1 *after* this
was measured and written down.

**M5. ⛔ Always run the null, and make it the empty field.** White noise is a far weaker
control against a chain whose floor spans 17x across bands. "51–54/64 bits" looked like
strong detection until white noise scored 45/64 through the same procedure — and the
empty field scored better still.

**M6. ⛔ Confirm the tag state, and understand what the confirmation says.** The harness
reported `data blocks UNVERIFIED` and named `DEADBEEF/12345678` on every run; it was read
as "the Proxmark cannot see them" rather than "they are not what you want".

**M7. ⛔ Check that a measure is comparable before comparing it.** Three separate instances
here: band RMS across bands whose floors differ 17x; AIN0's absolute counts against AIN5's
floor; a phase-32 capture against a phase-0 baseline.

**M8. ⛔ Make sure the measure can SEE the thing you are claiming.** The fc/2 band-SNR
criterion is polarity-blind — it measures the modulation skirt, which is transition energy
— and it sailed past its threshold with zero bits recovered. A whole-frame Hamming score
credits a 28-bit constant run with up to 32 free bits. Both passed while nothing decoded.

## Reporting

**M9. ⛔ No hard-coded thresholds in verdicts.** `max ratio > 3?` got 2.90 and printed a
conclusion its own table flatly contradicted. Fit the shape and report the fit. A verdict
that compares two argmins without checking either is meaningful is the same bug wearing a
different hat — `phasebits.py` shipped with exactly that and had to be corrected.

**M10. ⛔ Retract in place.** Band the old claim, do not delete it. The reasoning is the
reusable part; on this bench the artefacts have outlasted the dB figures.

**M11. ⚠ Question the conventions, including the ones that look like hygiene.** Discarding
400 "settle" samples was never once questioned. It alone took the decode from 43/160 to
**0/160**.

**M12. ⚠ Absent inputs are fatal, not skippable.** A whole sweep once reduced to `nan`
without saying so, and a verdict was read off it. Missing captures abort the run.

**M13. ⛔ Don't write `<placeholder>` in a shell command** — zsh reads `<` as a redirect.

---

## Keeping these notes honest

- `LOG.md` is **append-only**. The only permitted edit to an entry is appending a
  `⛔ retracted by L##` pointer. Never reword an entry to match what you now believe.
- `FINDINGS.md` holds **no history** and is rewritten freely. If a claim is not in its
  ledger, it is not established.
- **Nothing states a fact in two places.** Duplication is the mechanism by which history
  and current knowledge drift apart: you update one copy and the others go stale silently.
  That is why `SUMMARY.md` and `RESUME.md` no longer exist.
- A new claim goes in the ledger with its `n`, its `null` and its `indep` filled in. **If a
  column would be blank, that is the finding** — say so rather than leaving it empty.
- `./checkdocs.sh` verifies all of the above mechanically: dangling `L##`/`C##`/`M##`
  references, renamed files, commit hashes that no longer resolve, and the append-only and
  frozen-banner invariants. ⚠ It cannot check whether a claim is TRUE. Only a measurement
  does that.
