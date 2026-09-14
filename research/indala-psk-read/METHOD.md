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

**M32. ⛔ THE NOTES ARE AN INSTRUMENT TOO, AND I PROPOSED A MEASUREMENT THEY HAD ALREADY
ANSWERED.** L114 closed by pointing at carrier-edge counting for field detection — the way the
Proxmark and Flipper stay clock-locked while emulating — and I offered it as the next experiment,
costed at one capture. `FINDINGS.md`'s own **Hardware reference** already opened with
`ANT -> VD1 detector -> LF_OA`, and `lf_tag_em.c` already carried an upstream maintainer's line
saying the tag-mode antenna taps are envelope-only. Both were written before I proposed it. The
signal I planned to measure does not exist on any pin of this board, in any mode.
⇒ **Before designing a measurement, read the reference section for the subsystem it touches.**
One grep against notes I had already written would have replaced a firmware build, a flash and a
bench session.
⚠ The hazard is structural, not carelessness. A reference section is precisely where facts go to
*stop* being thought about — that is its job. The whole answer here was one word of a component
label, `detector`, and it read as scenery. ⭐ So the check cannot be "do I remember this"; it has
to be the grep, run at the moment a design is proposed rather than after it fails.

**M33. ⛔ WHEN A DECODER FAILS, GET A SCORE, NOT A VERDICT — I RAN FOUR ROUNDS OF EXPERIMENTS
THAT COULD NOT TELL ME ANYTHING.** Every PAC variant returned the same thing: `-`. Spike
capping versus interpolation, min/max versus percentile thresholds, median filters at four
widths, debouncing at three lengths, reset-versus-ignore on short intervals — thirty-two
combinations, and the output was thirty-two identical dashes. A verdict carries one bit, and
the question needed a gradient.
⇒ The tag's frame was **knowable the whole time**: write a known credential and the same
thirty-two experiments become a graded score. Scored, they separate at once — averaging a bit's
32 samples gives 8, 15 and 18 errors of 128 across three captures where a majority VOTE of the
same per-sample decisions gives 6, 8 and 6, against a random baseline of 47.
⚠ The cost was not the four rounds. It was that the dashes made the spike-handling theory look
neither confirmed nor refuted, so it stayed alive through three more rounds of variants built
on top of it.
⛔ **And the first scored run was itself wrong, in the classic way.** It reported the mean
arm flooring at 26 errors against the vote's 6 — a 4x gap — because the sweep varied threshold
for both arms but bit PHASE for only one. Given equal sweeps the gap is 1.3-3x. A graded score
does not exempt you from giving the arms equal treatment; it just makes the inequality visible
one round later instead of never.
⭐ **The rule: before varying anything, arrange for the experiment to return a NUMBER.** For a
decoder that means controlling the plaintext — write the tag rather than reading whatever is on
it. For a detector it means a rate rather than a hit. `pacber.py` exists only for this, and it
paid for itself on its first run.

**M34. ⛔ A READER IS ALSO A TRANSMITTER, AND ITS TRANSMIT SETTING IS AN INPUT TO ITS OWN
RECEIVE PATH.** PAC read 0 of 10 and the investigation spent its whole length downstream of the
ADC — spike clipping, thresholds, dead zones, debouncing, glitch policy, edge intervals versus
levels, majority votes, a comparator port. Every one of those treats the capture as given. The
capture was not given: the Chameleon **illuminates the tag**, the tag's answer scales with that
illumination, and the amplifier was being overdriven by a field the reader itself chose. One
step weaker and the same decoder reads 10 of 10.
⇒ **When a receiver fails on a STRONG signal, look at the transmitter before the demodulator.**
"Fails on loud tags" was in the section title the whole time and read as a symptom; it is the
diagnosis. A failure that gets worse as the signal gets better is almost never a sensitivity
problem, and every tool in the decoder chain is built for the opposite case.
⚠ The shape to recognise: three byte-identical tags reading 0/6, 3/6 and 7/9 (C46). That spread
is not flaky decoding, it is a monotone response to a variable nobody had written down —
coupling — with the failures at the *well-coupled* end.

**M35. ⛔⛔ AGAINST A KNOWN-INTERMITTENT SUBJECT, A SINGLE A/B IS NOT EVIDENCE — YOU NEED THE
RETURN LEG.** `lf hid prox read` scored 0 of 12. I explained it twice, confidently, and was
wrong twice: first that a geometry change had handed us the failing specimen §2 wanted, then —
after the user pointed out the geometry had not changed — that my own PWM edit had caused a
regression, which the pre-change build seemed to confirm at 12 of 12. Both stories fitted. The
third measurement killed both: the post-change build, flashed again, read **12 of 12**.
⇒ **A/B is not an experiment when the subject is noisy; A/B/A is.** The cost of the return leg
was one flash cycle. The cost of skipping it was very nearly a fabricated regression in the
notes, and a "fix" for HID that would have papered over a bug I had invented.
⛔ And the information was already written down. C45 records HID failing 15-20% of the time and
an unexplained 0/15 episode that cleared on its own. A 0/12 is *inside the documented behaviour
of that reader*. ⇒ **Before treating a failure as new, look up the subject's recorded failure
rate.** This is M32 again — the notes had the answer — but with a sharper edge: there I proposed
an experiment the notes had already answered, here I proposed a CAUSE the notes had already
explained.
⚠ The tell to recognise: a single failing run that arrives right after you changed something.
The change is the salient candidate precisely because it is yours, and salience is not evidence.

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


**M36. ⭐⭐ ENTAILMENT BEATS CAPTURES — if the code can only return X, no number of measurements
adds to that.** C269 watched a wrong frame recur at two different sample phases and concluded
that was "the shape that passes two-agreeing-stacks". C293 read the implementation and found the
comparison RESETS across phases, so cross-phase recurrence can never pass — the inference was
void. C294 then found the whole question was settled by one line: `winner_res` is assigned in
exactly one place, inside the agreement match, so every frame the reader has EVER returned
matched its predecessor at the same phase, by construction.
⇒ Three claims to reach what reading one function would have given immediately. When a question
is about what a program can do, read the program first and measure second. Measurement is for
what the world does, not for what the code permits.

**M37. ⛔ A HOST CAPTURE LOOP DOES NOT STAND IN FOR THE DEVICE'S LOOP UNLESS THE TIMING MATCHES.**
48 captures taken as separate `rdrcap.py` invocations, each re-initialising the capture path with
a 50ms gap, showed no within-phase recurrence — and were read as evidence that the device's
agreement rule could not be defeated that way. The device takes its tries BACK TO BACK inside one
read. Different timing regime, and the two were compared as though they were one measurement.
⇒ Name the regime when the tool is not the thing under test. "Captured with the same hardware"
is not "captured the same way".

**M43. ⛔ A DIFF'S SIZE IS NOT A PR'S SIZE, AND ONLY THE COMPILER KNOWS THE DIFFERENCE.**
A hunk of 4 lines was called a 4-line PR. Building it on `main` revealed that the call site needs
a guard that needs BLE connection state — **5 files and +140, thirty-five times the estimate**,
with each dependency invisible until the previous one compiled. ⇒ **Before quoting a change's
size to anyone, apply it to the target branch and BUILD IT.** `git diff --numstat` measures what
you touched, not what you need, and the gap between those is exactly where an upstreaming plan
dies on contact. ⭐ The failures are the measurement: each compile error names the next
dependency.

**M42. ⛔ A FIX THAT CHANGES WHICH CANDIDATE WINS INVALIDATES EVERY MESSAGE JUSTIFIED BY THE
OLD ORDERING — AND THE HARNESS THAT GRADED IT.**
C302 taught `unpack()` to prefer a format that can validate. Three things downstream had been
reasoned from the OLD ordering and silently became false: the CLI's corruption message ("a
genuine foreign tag never lands here"), the harness arm that justified it (which asked who
ACCEPTS rather than who WINS), and the write-side warning lists. ⚠ **Two of the three were
caught only by reading a real tag** — the harness agreed with itself because it was grading the
algorithm it had been written against. ⇒ **After changing a selection rule, grep for every
message and every test that asserts something about what the OLD rule returned**, and re-derive
each from the new one rather than re-running it and seeing green. A test that passes because it
encodes the old semantics is worse than no test.

**M41. ⚠ THE FIRMWARE BUILD NEEDS AN EXPLICIT TOOLCHAIN ROOT ON THIS MACHINE.**
`firmware/build.sh` is `#!/usr/bin/env`-broken (run it with `bash`), and the SDK's
`Makefile.posix` hardcodes `GNU_INSTALL_ROOT ?= /usr/bin/` where Homebrew puts
`arm-none-eabi-gcc` in `/opt/homebrew/bin`. ⇒ `GNU_INSTALL_ROOT=/opt/homebrew/bin/ bash ./build.sh`
compiles and links; only the final `nrfutil` packaging step fails, which a compile check does not
need. ⛔ A shipping-code change that was never compiled is not verified, and the host `ctest`
harness compiles only SOME of the files a change touches — `hidprox.c` is in none of its arms.

⛔ **AND `nrfutil` IS NOT ON `PATH` EITHER — it is `.tools/bin/nrfutil`.** C200 recorded this and
it still cost time on 2026-09-15, where `command not found` read as "no flashing possible" and
very nearly became a §5 blocker. ⚠ The DFU trigger also needs `flush()` and a short settle
before `close()`; without them it returns cleanly and the device never resets, which looks
exactly like a device that refuses DFU.

**M38. ⚠ A DOCUMENT THAT IS ONLY EVER APPENDED TO PUTS ITS OLDEST LAYER WHERE THE READER
FINISHES.** `NEXT.md` §9f grew across ten claims, each understanding added below the last. Its
TAIL still said the defect was "reachable only when something else is already perturbing the
capture" — disproved on perfectly good tags four claims earlier. A reviewer reading to the end
would have taken the weakest and oldest reading as the conclusion. The same session found two
warnings in one firmware file describing decoders that no longer existed (C292, C293).
⇒ Rewrite the conclusion, do not append to it. And when a warning is superseded, DATE it rather
than delete it — the reason it was written is often still load-bearing, as with a dead band that
is still excluded from a rotation.

**M39. ⭐ RE-RUNNING ARMS WHOSE CODE CANNOT HAVE CHANGED PROVES THE BENCH WORKS, NOT THE BUILD.**
After a day of gate changes the obvious companion to a read-arm regression pass was re-running
the emulate arms — which meant flashing the one unit untouched all session, holding a live slot,
with a known targeting hazard. A per-file `git log` over the seven files those arms depend on
showed 0 commits in range.
⇒ Ask whether the change can reach the thing before spending risk on testing it. A regression run
with no causal path is ritual.

**M40. ⛔ VERIFY THE FAILING BRANCH OF A DISPLAY FIX, NOT ONLY THE PASSING ONE.** Three times this
session a user-facing message was added and only its happy path seen: the Securakey parity line,
the IDTECK checksum verdict, and the HID relabelling warning. Each needed a deliberately
constructed input — a flipped spacer bit, a synthetic checksum, a guard-off build — before the
other branch had ever rendered. One of them, once reached, turned out to drop two fields.
⇒ A branch nobody has seen print is a branch nobody has tested, and rare branches are exactly
where a wrong f-string survives for years.

**M44 — ASK THE JUDGE TWICE BEFORE BELIEVING A FAILURE, AND TWICE BEFORE BELIEVING A BLANK.**
The independent reader that grades a write is not a perfect instrument. Measured: the Proxmark's
`lf fdxb reader` misses **12% of asks on a correctly written tag** (53 of 60), while its HID, GProxII
and AWID readers were 20 of 20 each (C346). A single ask inherits that miss rate, so a landed write
reads as a failure — which is exactly how a 25-round soak reported `24 of 25` and nearly earned FDX-B
a phantom writer defect.
⛔ The correction is direction-dependent, and getting it backwards is worse than not doing it:
  • when the expected answer is a CREDENTIAL, a miss is a false FAILURE — re-ask before recording one;
  • when the expected answer is NOTHING (a blank-tag null), a miss is a false PASS — require the
    negative answer twice, or the row passes for free.
⇒ Neither retry costs anything. Both are in `regrade.sh` and `nullmatrix.sh`.

**M45 — ASK THE HARDWARE WHAT IT IS RUNNING, NOT THE REPOSITORY.**
A clean tree, a green gate and passing notes-checks all describe the SOURCE. None of them knows what is
flashed. Firmware was changed, committed and left unflashed for a whole tick (C358) — #2 sat several
commits behind while every check reported clean, because every check looked at the repository.
⛔ A measurement taken against a stale build is unattributable and looks exactly like a good one, which is
the dangerous case: nothing fails, the numbers are plausible, and they belong to code that is not the code.
⇒ `./autopilot.sh status` now prints the device's own `get_git_version()` against HEAD every tick, and says
which of three things is true: matches HEAD, built from a dirty tree so it matches no commit, or behind the
source. ⚠ *Semantically identical, so it cannot matter* is a prediction. Flash it and run the battery.

**M46 — AN EMULATION SLOT NEEDS FOUR THINGS, AND MISSING ONE LOOKS EXACTLY LIKE A BROKEN PROTOCOL.**
A slot emits only when all four are true:
  1. `hw slot type -s N -t <Type>`   — the protocol
  2. `lf <proto> econfig -s N ...`    — the credential
  3. `hw slot enable -s N --lf`       — **the interface**
  4. `hw mode -e`                     — emulator mode
⛔ Miss the enable and `hw slot list` prints `LF: (disabled)<Type>` — type present, interface off, nothing on
the air. A working slot reads `LF: Empty <Type>`. That one word is the whole difference and it cost four rounds
of chasing the protocol (C365).
⇒ **When an emulate arm reads zero, put a KNOWN-GOOD protocol on the SAME slot before blaming the protocol.**
EM410X on the suspect slot failed too, which ruled out Indala and PSK in one step and pointed at the slot.
⚠ And read what the firmware prints: it said `WARNING: Slot LF type is not Indala` on the first attempt, and an
error filter matching `error|invalid|Traceback` threw it away. Filter for `warning` too.

**M47 — CONTEXT IS A RECURRING COST, SO COMPACT ON A THRESHOLD RATHER THAN ON INSTINCT.**
Every turn re-sends the whole window. Carrying a large context pays for it again on every subsequent turn;
compacting costs one summarisation pass and makes every later turn cheap. ⛔ *It just re-expands* is wrong —
the summary is far smaller than what it replaces, which is the point (C369).
⇒ `./autopilot.sh status` prints the figure every tick from `utility-scripts/claude/context_check.sh`, which is
local, needs no credential and costs no tokens. **≥80%: compact at the end of the tick. ≥60%: consider it.**
⚠ Carrying more context can still be the right call — mid-investigation, with state that would be expensive to
rebuild. Make it a decision, not a drift.

**M48 — COMPACTION IS DECIDED BEFORE THE SESSION STARTS, BECAUSE NOTHING CAN TRIGGER ONE FROM INSIDE.**
Cron fires, peer messages and the messaging socket all enqueue with `skipSlashCommands` on; the control protocol
has no compact verb; `Pre`/`PostCompact` hooks only observe and block (C371). The one lever is
`autoCompactWindow`, and it is read **at process start** — writing it under a running session does nothing,
measured twice (C372).
```sh
UTIL=/Users/Shared/code/personal/utility-scripts/claude
sh "$UTIL/autocompact.sh" 40 --project "$REPO"    # unattended: compact at 40%, from the NEXT session on
sh "$UTIL/autocompact.sh" off --project "$REPO"   # operator present: leave it alone, ask for /compact
```
⭐ Scope is the project (`<project>/.claude/settings.local.json`, gitignored) so sessions in other repos are
untouched. `--scope user` is global and deliberate.
⛔ **An operator-present loop cannot compact and must not pretend to.** At the threshold it commits, pushes and
asks for a manual `/compact` in one line — that is the exception to *never stop to ask*.
⚠ Every tick still ends compact-safe either way: commit, push, keep §1 current. Auto-compaction lands between
turns and keeps only what is on disk.
