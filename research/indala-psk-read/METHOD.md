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

**M14. ⛔ A bootstrap that resamples WITH replacement measures its own resampling.**
Testing "do two captures ever agree on a wrong answer?" by drawing from 40 capture results
with replacement reported 0.32% — but every one of those was the *same capture file drawn
twice*, which is not two captures agreeing, it is one capture counted twice. Sampled
without replacement the rate is 0 in 20000. ⇒ When the question is whether independent
trials agree, the resampling must be without replacement, or it manufactures exactly the
correlation under test.

**M15. ⚠ A decode rate is not a correct-answer rate, and the gap is large.** The offline
work reported "51 of 160 captures decode" and never asked what the OTHER frames were.
They were wrong answers, not failures: 24 wrong frames in 200 captures, one in five. A
reader built on the decode rate alone would hand back a wrong credential 20% of the time.
⇒ Always report recovered / correct / wrong as three numbers, never two.

**M20. ⛔⛔ THE SPECIMEN'S STATE IS NOT THE SAME AS THE SPECIMEN'S PLACEMENT, AND THIS
PROJECT CHECKED ONLY THE FIRST.** `README`'s opening warning — confirm what is written on
the tag before trusting any measurement — was followed scrupulously for days. Nobody ever
asked which SIDE of the reader the tag was sitting on. It was the wrong side, for the entire
investigation, and it was worth **21x — about 26 dB**, which is most of the "31.2 dB below
the Proxmark" the project was built around explaining. ⇒ Write down the physical
configuration, not just the logical one, and put a number on each part of it.

**M21. ⚠ AN ASSUMPTION IMPORTED FROM A DIFFERENT DEVICE IS STILL AN ASSUMPTION.** The tag
went on the back because that is where a Flipper Zero reads. The Chameleon is the other way
round. Nothing in the investigation ever stated this belief, so nothing could ever test it —
which is the defining property of the assumptions that cost the most.

**M22. ⭐ THE VARIABLE NOBODY VARIED IS WHERE THE ERROR IS.** Days went into sample phase,
gain, oversampling, settle, air gap, filters and stacking — every one of them a knob someone
had already thought to turn. The 26 dB was in the one degree of freedom that was never
written down as a degree of freedom at all. ⇒ When a large unexplained deficit persists
across many careful experiments, stop refining the experiments and go looking for the axis
that is not in them.

**M19. ⭐ Measure the SIGNAL, not whether it decoded.** A read is binary and conflates "the
tag was not heard" with "the tag was heard and the decoder did not lock" — and a binary
metric noisy enough to swing 6-11 out of 15 will manufacture a convincing before/after. The
subcarrier amplitude is continuous, one number per capture, and it separated those two in
thirty seconds after an hour of binary reads had separated nothing. `lfprobe.py`.

**M17. ⛔ Run the no-stressor control BEFORE claiming a regression, not after.** "5/5 before,
1/5 after" was reported as a reproduced bug. The control — same measurement, no stressor —
swung 8/15, 11/15, 6/15, and the metric then decayed to 0/15 through the control arm. The
before/after shape is the most persuasive thing a noisy metric can produce by accident,
and it is free to check: measure the probe twice with nothing in between, first.

**M18. ⚠ "Confirm the tag state" applies to the CONTROL tag too.** M-rule discipline was
applied to the Indala tag from the start and never once to the HID tag used as the probe —
which sat on the antenna, unverified, through 25 minutes of measurement whose outcome
depended entirely on it being there.

**M16. ⭐ Port it to a second implementation, and diff the outputs per input.** The C
firmware decoder and the numpy research decoder share no code — integer vs float, 3-tap
notch vs FFT — and agreeing word for word on 320 captures INCLUDING the failures is the
independent check this project's ledger asks for. Matching *counts* would not have been:
two decoders can both score 51/160 on different captures.

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
  references, renamed files, commit hashes not reachable from HEAD, and the append-only and
  frozen-banner invariants. ⚠ It cannot check whether a claim is TRUE. Only a measurement
  does that.
- ⚠ **A `LOG.md` entry cannot carry its own commit hash when it is written** — the hash does
  not exist yet. Write `this commit`, commit, then fix the pointer in a follow-up. An
  `--amend` orphans the hash you just wrote in, and until `checkdocs.sh` was taught to ask
  about *reachability* rather than existence it passed anyway.

**M29. ⛔ A CLEANUP THAT ONLY RUNS ON THE HAPPY PATH WILL CONTAMINATE THE NULL, AND THE NULL
IS THE ARM THAT MATTERS.** `flipper.py emulate` stops the Flipper in a `finally`. It was
launched under `timeout`, which sends SIGTERM — and SIGTERM kills the process without running
`finally`. The Flipper therefore went on emulating, and the captures taken next, labelled
"idle", decoded the emulated credential perfectly. For several minutes that read as "our
reader decodes a free-running source", contradicting the project's central claim on the
strength of a control that was not a control.
⇒ The failure is structural, not careless: a positive arm that is still running looks
exactly like a positive arm that worked. **Do not infer that a source stopped — stop it
explicitly and confirm, then take the null.** Here the confirmation was one ETX and the `^C`
it echoed, after which the same captures gave 0 of 4.
⭐ What saved it was running the null at all, on the same device in the same session (M26).
The real result survived and came out stronger, because the difference between the arms now
tracks the one variable that changed.

**M30. ⛔ A WORKAROUND OUTLIVES THE PROBLEM IT WORKED AROUND, AND NOBODY GOES BACK TO CHECK.**
Capture stacking was added at 04:03 on 2026-09-11 to fix a real signal deficit: 31.9% → 71.9%,
measured carefully, with a clean null at every depth. It cost two 16 KB accumulators and an
8 KB scratch buffer. At 09:53 the SAME DAY the deficit turned out to be the tag sitting on the
wrong side of the device — worth 21x — and on the correct side the rate is 68.75% at every
stacking depth INCLUDING ONE. The workaround was then carried for a further day, and its 40 KB
were what made Indala224 impossible (C94).
⇒ The fix was not wrong when it was made; it was obsolete six hours later and nothing said so.
**When a root cause is finally found, re-examine what was built to compensate for it** — every
mitigation dated before the discovery is a candidate for deletion, and the expensive ones
should be re-measured rather than assumed still to be earning their keep.
⚠ Note how invisible this is: stacking kept working perfectly. It never failed, never produced
a wrong answer, and its own measurements stayed true. A thing that still works is much harder
to notice than a thing that breaks.

**M31. ⛔ TWO INDEPENDENT INSTRUMENTS THROUGH ONE ANALYSIS ARE ONE MEASUREMENT.** I captured the
same PAC tag with the Chameleon and with the Proxmark — different antennas, different ADCs,
different firmware, no shared code — and ran BOTH through the same level-threshold script of
mine. They agreed: 99.1% and 99.0% periodicity, neither matching the tag's memory. That
agreement was written up as two independent receivers corroborating each other, and the
conclusion drawn was that the on-air bits differ from the block data.
⇒ They agreed because they shared MY BUG. The tag's blocks turned out to be a flawless PAC
frame — exact 19-bit preamble, twelve valid UART frames, correct XOR checksum — so the air data
was right all along and the demodulation was wrong.
⚠ This is M1 wearing different clothes. There, an encoder and decoder sharing a convention
agreed forever. Here, two data sources sharing an analysis did. **Independence has to hold at
the step that can be wrong**, and the step that can be wrong is usually not the one being
varied.
⭐ What caught it cost nothing: decoding the reference against the PROTOCOL'S OWN rules, in
software, with no hardware at all. A format with a checksum will tell you whether you have
read it correctly — ask it before building a theory about why it disagrees.

**M23. ⛔⛔ A SAFETY RULE MEASURED AT LOW SNR MAY NOT HOLD AT HIGH SNR, AND THE FAILURE IS
SILENT.** The two-capture agreement rule rests on "every wrong word appeared exactly once,
because bit errors land somewhere different each time." That was measured, correctly, on
160 captures — all of them 26 dB down. At that level errors ARE noise-driven and they do
scatter. With 20x the signal the decoder stops guessing and locks deterministically onto a
half-bit-offset alignment, returning the *same* wrong word on every capture. Two
independent captures then agree, and the rule reports full confidence in a wrong answer.
⇒ Improving the signal moved the failure from random to systematic. When a rule's
justification is "the errors are independent", re-test that independence at every operating
point, not just the worst one — and prefer a rule that rejects on a *mechanism* (here: an
alignment half a bit period off, at half the amplitude) over one that relies on errors
being obliging enough to disagree with each other.

**M24. ⛔ A REFERENCE LEVEL IS A PROPERTY OF THE MEASUREMENT, NOT A CONSTANT — AND A DEFAULT
THAT CROSSES CONFIGURATIONS MANUFACTURES A RATIO.** `lfprobe.py` defaulted `--band` to HID
Prox (10-18kHz) and `--floor` to 6900, the empty level for fc/2 (62.5kHz). Running it with
both defaults divided one band's energy by another band's floor and printed "896.67x" with a
full-width bar. It was reported to the user as an overwhelming signal before anyone checked
that the two numbers described the same measurement. The true ratio, once the floor was
measured in the right band, was ~90x — still the right conclusion, reached by luck.
⇒ A normalisation constant must be measured in the configuration it normalises. Where a tool
cannot know it, it must REFUSE TO DEFAULT rather than supply a plausible number: a missing
ratio is obviously missing, a wrong one is not. And the fix is cheap — the floor for a band
is one measurement with the antenna clear, which the same tool can take.

**M25. ⭐ WHEN A DIRECT MEASUREMENT IS CONFOUNDED, TEST THE THING THAT DEPENDS ON IT.** Frame
lock was asserted from a lag-0 baseband correlation, then doubted when the EMPTY field scored
higher than the tag. Both readings were right and neither settled anything, because that
correlation is dominated by a background common to every capture — it can neither prove nor
disprove alignment. What settled it took one line: stacking only works if captures are
aligned, so stack them rolled by a random offset and compare. Aligned 67%, rolled 0%.
⇒ A property that cannot be measured cleanly can often be measured through its consequences,
and the consequence test is usually both cheaper and harder to fool. Ask "what would stop
working if this were false?" before building a better instrument for the thing itself.

**M26. ⛔⛔ A POSITIVE CONTROL MUST MATCH THE GEOMETRY, NOT ONLY THE INSTRUMENT.** Four
claims were retracted at once — including the most emphatic sentence in the ledger, "PSK1
tag emulation has never worked on this device" — because the control was a real tag lying
flat on the antenna while the thing under test was a second device at an unproven distance
and orientation. Same reader, same analysis, same sample rate, same empty-field baseline:
everything matched except the one variable that mattered. The measurements were all correct;
they measured coupling.
⇒ The control must be taken in the SAME session and the SAME position, with the only change
being what is under test. An empty-field control proves the instrument hears nothing when
nothing is there; it says nothing about whether the instrument can hear THIS thing HERE.
⚠ And note what made it convincing: an independent-looking second experiment (IDTECK) agreed
precisely — because it shared the flaw. Agreement between two measurements that share a
defect is not corroboration, and it feels exactly like corroboration.

**M27. ⭐ AN ASSUMPTION THAT BUYS SIMPLICITY BUYS FRAGILITY WITH IT, AND YOU ONLY FIND OUT
WHEN YOU BUILD A SOURCE THAT VIOLATES IT.** This decoder's central insight is that Indala's
fc/2 subcarrier sits at exactly fs/2 for a carrier-locked sampler, so demodulation is
multiplication by (-1)^n — no oscillator, no phase estimate, no clock recovery. That is what
makes it small enough to run on this part in integer arithmetic, and it reads real tags
60/60. It is also the reason it is the ONLY one of three readers that cannot read our own
emulator: the assumption holds because a T5577 *divides the reader's own field* and therefore
cannot drift, and a free-running PWM offers no such guarantee.
⇒ The invariant was never written down as a dependency, because every specimen available
satisfied it for free. It only became visible when we built the first source in the project's
history that did not. ⚠ When a simplification is justified by "the physics guarantees this",
name what guarantees it — the guarantee is a dependency, and something you build later may
not provide it. ⭐ And note this is not a defect to fix: the Flipper and Proxmark are tolerant
because they demodulate generally, which costs code and cycles this device does not have to
spend on tags that are always locked.

**M28. ⛔ A COUNT THAT EXCEEDS THE NUMBER OF ATTEMPTS IS AN IMPOSSIBLE RESULT, AND IT PRINTED
PLAINLY FOR HOURS.** `nulltest.py` ran a read command five times and matched a substring in
the output. The command did not exist; the CLI printed its help, which contained the protocol
name twice. The tool reported **10/5**. Five attempts cannot produce ten successes, the "/5"
was right there in the output, and it was read as a strong pass and written into two ledger
claims as the decisive evidence.
⇒ Sanity-bound every derived number against its own denominator, and have the tool refuse
rather than print an impossible one. ⚠ And prefer matching on something the SUCCESS path
uniquely produces — a decoded value, a status code — over a protocol name that also appears
in help text, error messages and the command itself. A substring match treats the tool's own
chatter as data.
⭐ Note where it was caught: not by re-reading the numbers, but by a question about SCOPE —
"did we ever fix that decoder?". A claim can be internally consistent, cross-referenced and
still rest on a capability nobody ever built.

