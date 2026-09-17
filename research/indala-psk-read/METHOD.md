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


**M47 — AN INSTRUMENT SHARED BY TWO OWNERS MUST NAME WHICH OWNER IT IS REPORTING ON.**
`hw lfdebug` printed `⛔ PWM0 IS NOT READING OUR SEQUENCE` whenever the sequence pointer did not match. But
*ours* meant the READER's array, and `lf_tag_em.c` drives the same peripheral with its own sequence — so while
the device was emulating, that banner was the CORRECT state. U18 read it as *the emulator never sets
`m_pwm_seq`* and spent a whole unit there, while `hw emudebug` said `have pwm seq: True` and was never asked
(C373). It caught me a second time months later, printing `ours: False` beside our own buffer's address (C389).
⛔ **A ⛔ that fires in a normal state trains the reader to discount every ⛔**, which costs more than the one
wrong line. ⇒ Name the owner in the label, and alarm only when the owner named is the one that should hold it.

**M48 — SWEEP THE RATE, NOT THE ENCODING: TURN A BINARY FAILURE INTO A CURVE.**
U11 read as *FSK emulation does not work* through four re-encodings, each a build, a flash and a capture, each
answering only yes or no. Sweeping how OFTEN the tone changes — never, every four bits, every bit — gave
**3031 / 261 / 0** long tones in one cycle and named the mechanism's family: the emitter produces either tone
perfectly and degrades monotonically with the transition rate, which is a settling signature and not a digital
one (C387). ⇒ When a thing fails, look for a parameter that can be varied CONTINUOUSLY and vary it. A curve
says what kind of thing is wrong; a pass/fail only says that something is.

**M49 — A GUARD AT THE BOTTOM OF A STACK PROTECTS NOTHING UNLESS EVERY CALLER PROPAGATES IT.**
`flipper.py` was fixed to abort when its reader refused to start. `emugrade.sh` then captured that abort with
`o=$(fread)` and grepped for a score — discarding the exit status AND the message — and printed a clean
`✓ null before / psk - ask - / ✓ null after` against a reader proven dead minutes earlier (C376). The leaf was
right and the stack was not. ⇒ **Test the WRAPPER against the still-broken state**, not just the leaf. Shell's
`$(...)` silently drops exactly what a guard produces, which makes this the default outcome rather than a
freak one.

**M50 — VALIDATE AN ANALYZER ON A KNOWN-GOOD REFERENCE, AND EXERCISE THE ABORT PATH.**
A new instrument's first output is the one most likely to be believed and least likely to be checked.
`tonehist.py` was run against a reference known to be good — the Flipper's own mixed-tone emission through our
reader, giving both bands — BEFORE it was pointed at the thing under test, and `fskcap.sh` was run against the
uncoupled bench so its refusal path executed at least once (C391). ⛔ The abort path is the branch that runs
when something is wrong, which is precisely when nobody is in a position to notice it is itself broken.
⚠ And make the tool print its own noise floor: a histogram that hides its floor is how a null becomes a
measurement.

**M51 — RE-RUN THE ARMS YOU DID NOT CHANGE WHEN YOU CHANGE WHAT THEY STAND ON.**
⚠ **This is the complement of M39, not a contradiction, and the difference is causal reach.** M39 refuses a
regression run with no path from the change to the arm. Here there was a path and it was invisible: the PWM
base-clock choice moved to a new macro at three sites, two protocols moved to a shared buffer, `.bss` changed
by +9,984 B — all under protocols that were already working and none of them edited. ⛔ **A wrong base clock
is SILENT on the air, not loud** (C130), so *semantically identical* would have been believed. All eight
arms were re-graded and held at 6/6 (C394). ⇒ Ask what the arms DEPEND on, not what was edited; and note that
a change with no visible effect anywhere is the shape of one that breaks something quietly.

**M52 — A CHAMELEON CANNOT READ ANOTHER CHAMELEON'S EMULATION ON THE PHASE-LOCKED PATH, AND A NULL THERE
MEANS NOTHING.**
The SAADC whole-capture readers — indala, gallagher, securakey, noralsy, gproxii — recover a subcarrier that a
real tag produces by DIVIDING the reader's carrier, so a real tag is inherently phase locked. An emulating
Chameleon generates PWM from its own clock: *in reader mode the carrier is generated, not recovered, and
nothing in the design ever knew a reader's carrier phase.* The envelope readers on the GPIO/comparator path —
em410x, viking, hidprox, ioprox — do not care, which is why they read an emulation fine.
⛔ **So an emulation-to-reader test is valid for the GPIO family and INVALID for the SAADC family**, and it
fails silently in the direction that looks like a defect. It produced a confident four-arm regression report
and a firmware build flashed to "exonerate" a session, before `lf indala read` printed the reason in one line
(C404).
⇒ **Pick the control from the family under test.** EM410X passing proved nothing about Gallagher, because
EM410X could not have failed. A control that cannot fail is not a control.

**M53 — A CONTROL MUST EXERCISE THE SAME CAPTURE ENGINE AS THE ARM UNDER TEST, AND YOU NAME THE
ENGINE FROM THE SOURCE, NOT FROM MEMORY.**

⛔ C400 stood for a day as *two ASK read arms are down* on the strength of two passing controls,
EM410X and HID Prox, described as covering *both* capture engines. They do not. `lf_em410x_data.c`
and `lf_hidprox_data.c` both include `lf_reader_data.h` and neither calls `lf_drive_swept_read`;
`gallagher_read`, `securakey_read` and `noralsy_read` all live in `lf_indala_data.c` and all go
through it. **Both controls were the GPIO engine. Both failing arms were the SAADC engine.** The
SAADC path had no passing arm at all, so *neither capture engine is down* — the one thing the
controls existed to license — was never established, and the claim did not reproduce: 18 of 18 on
re-test (C412).

⭐ This is the same failure as M52 seen from the other side. There the trap was a control that
**could not fail**; here it is a control that **could not have detected the failure**. Both come
from naming a control by PROTOCOL rather than by the machinery it drives.

⇒ **Before a control is allowed to bracket anything, open the file and check which capture path it
enters.** One `grep` for the include and the swept-read call answers it, and it costs a minute
against the day C400 cost. ⚠ The stale claim that produced it was sitting in `AUTOPILOT.md` §2 U5
(*Gallagher reuses the GPIO/comparator path — not the SAADC capture path*), which is why it was
believed rather than checked: **a plan-era note is not a source of fact about shipped code (M45's
rule, applied to our own notes instead of to the hardware).**

**M54 — COMPARE LIKE WITH LIKE: A CORRECTED QUANTITY AGAINST AN UNCORRECTED ONE MANUFACTURES THE
ASYMMETRY YOU ARE LOOKING FOR.**

⛔ **This entry replaces a false one written in the tick before (C435, retracted by C436).** It claimed the
Flipper reports edge positions and not levels, on the strength of PAC's PERIODS landing on a multiple of the
256us bit 99.6% of the time while the HIGH and LOW runs composing them did so 0.3-2.6% of the time. The
invariant was right — NRZ holds a level for a whole number of bits, so every run must be an integer
multiple of it. The comparison was not: **the periods were bias-corrected and the runs were not.** With the
per-capture bias fitted the runs quantise at **83-89% within 0.15 bit and ~97% within 0.25 bit**. The
instrument was never the problem, and a mechanism was published on the gap between two differently-processed
numbers.

⚠ **What made it convincing is what should have made it suspect: the asymmetry was enormous** — 99.6%
against 0.3%. A real physical effect rarely arrives that clean. ⇒ When two numbers differ by two orders of
magnitude, check that the same corrections were applied to both **before** reaching for a mechanism.

⭐ The nuisance parameter behind it earns its own line: the Flipper's run bias is **not a constant**.
C429/C430 measured ~96us; fitted per capture it is **93us and 151us** in two captures taken minutes apart from
the same emitter and the same pad. ⇒ Fit it per capture, and report what changes when you do — here it
fixed the rounding and changed the recovered bits **not at all**, which is exactly how a nuisance parameter is
told apart from the defect.


**M55 — IF YOU FOUND THE FIT BY MAXIMISING, PERMUTATION-TEST THE MAXIMISATION BEFORE YOU BELIEVE IT.**

⛔ C440's alignment searched 128 cyclic offsets x 2L cycle starts for the overlap-maximising match between a
measured run sequence and a predicted frame. It returned 17 of 18, 19 of 28, 21 of 32 — numbers that read
as a good fit, and which produced a missing-position list, a clustering percentage and a
survival-by-run-length table, all ready to report. **A permutation control — same run multiset, order
shuffled, same maximisation, 300 draws — put the null mean at 17.1, 21.2 and 20.0.** The observed fit was
at or below chance in all three.

⭐ The control is cheap and mechanical: **shuffle the thing whose ORDER is supposed to carry the signal,
re-run the identical search, and compare.** If the search finds as much in the shuffle, the search is what
produced the number.

⚠ A search over N offsets gets N chances to look good, and the more thoroughly you search the better the
best result looks — so the size of the search space is itself a reason to run the control, not a reason to
trust the fit. ⇒ **Any number obtained by taking a maximum over alignments, offsets, thresholds or phases
needs its own null.** This is the same failure family as M54 (comparing quantities that were processed
differently); here the asymmetry is between a number that was optimised and a baseline that was not.

**M56 — AN EMULATION READBACK NEEDS A READER FIELD, AND A REGISTER THAT READS THE SAME FOR EVERY ARM IS
READING NOTHING.**

LF emulation is **field-driven**: the tag path only plays a burst when a reader's carrier is sensed. With a
Chameleon on the Flipper's pad and the Flipper idle, `hw mode -e` arms the slot and **nothing ever plays** —
`hw emudebug` says `emulating now: False` and `playbacks started` does not move. Every PWM0 register then
still holds whatever the READER path last left there.

⛔ That is exactly what C454's first run returned, and it looked like a clean result: `SEQ[0].CNT` read **512**
for pac, gproxii and securakey alike. 512 is pac's correct answer — so a pac-only run would have reported
*the sequence descriptor reaching the peripheral is right* from a register that had not been written since the
reader last used it. **The only reason it was caught is that gproxii and securakey must read 384 and did not.**

⇒ **Two rules.** (1) Hold a reader field up while you query — put the Flipper in `rfid read` and read the
registers during that window; `emulating now: True` and an advancing `playbacks started` are the evidence that
the field was there. (2) **Carry arms whose predicted values DIFFER**, and treat identical readings across
them as an instrument fault rather than a measurement. A register that cannot disagree is C452's *control that
cannot fail* wearing different clothes.

⚠ And not every field survives the fix: `PWM0 COUNTERTOP` still read **1000** — the reader's carrier value —
for all three arms even with the field up, because in WaveForm mode the decoder writes COUNTERTOP per entry and
a USB query lands between bursts. `SEQ[0].CNT` works, `COUNTERTOP` does not; find out which is which by making
the arms disagree, not by assuming.

**M57 — THE PSK RUN-STRUCTURE CRITERION, WRITTEN BEFORE THE CAPTURE. C471's method does not
transfer to PSK unchanged, and the arithmetic is different enough to get wrong afterwards.**

C471 predicted a run histogram from the frame's own bits and matched it, on PAC — which is NRZ, one
run per bit at `counter_top` 32 (256us). It transfers to `gproxii` directly. It does **not** transfer
to the five PSK arms, because there the subcarrier runs continuously and the data is in its PHASE.

The scale, derived rather than assumed. `lf read` samples at 125 kHz, so one sample is 8us (the
identity C464 cross-checked against a real PAC tag). The PSK arms run a 1 MHz PWM base clock with
`counter_top` 16 and duty 8 — a 62.5 kHz square, 8us high and 8us low. ⇒ **one sample per half
period: the ordinary run is ONE sample.** A bit is held for `repeats`+1 = 16 subcarrier periods,
so a bit is 256us = **32 samples**, and a 64-bit frame is 2048 samples.

⇒ **A 180-degree phase flip cannot shorten a run, it can only DOUBLE one.** `...HLHL|LHLH...` — the
two like samples either side of the flip abut into a single 16us run. So:

| what the capture shows | what it means |
|---|---|
| runs of 1 sample throughout, with N doubled runs at multiples of 32 samples | the subcarrier and its phase are both on the air |
| runs of 1 sample throughout, **no doubled runs at all** | the subcarrier is emitted and the INVERSION BIT IS INERT — the data never reaches the air |
| no 1-sample alternation | the subcarrier itself is not reaching the air; a different defect |

⛔ **N IS THE PREDICTION AND IT IS PER-CREDENTIAL, so the arms disagree and cannot cover for each
other** (M56's rule). For PSK1 the phase telescopes to `phase[k] = bit[k] XOR bit[N-1]`, so a flip
falls wherever the frame's bits change, counted cyclically:

| arm | frame | flips per frame | flip gaps (bits) |
|---|---|---|---|
| `indala` | `a0000000e6bd0e92` | **22** | 1, 2, 3, 4, 29 |
| `idteck` | `4944544b55667788` | **40** | 1, 2, 3, 4 |
| `keri` | `00000004000181cf` | **8** | 1, 2, 3, 4, 6, 17, 29 |
| `nexwatch` | `560000000012776a2f202800` | **30** | 1, 2, 3, 4, 7, 12, 36 |

⛔⛔ **`pm3cap.py --min-run` DEFAULTS TO 2 AND WOULD DELETE THIS ENTIRE SIGNAL.** The default was
right for PAC, whose runs are 32 samples; here the signal IS the 1-sample run and 2 samples is the
anomaly. Pass `--min-run 1`. A tool's defaults are calibrated to the last protocol it was pointed
at, which is not a property of the protocol in front of it.

⚠ Two samples per subcarrier period is exactly Nyquist, so the sample phase decides how cleanly the
alternation resolves and a null result needs the third row above ruled out before it is read as the
second. ⚠ And `PWM0 COUNTERTOP` reading 1000 under every arm is already explained in M56 — the
reader's carrier value, because WaveForm mode writes the top per entry. It is not evidence.

⭐ **M57 HELD, AND THE DOUBT IT RAISED IS RESOLVED (C479).** The capture showed the third row — no 1-sample alternation — and the obvious objection was that sampling a 62.5 kHz square at 8us is critically aliased, so a CORRECT signal might look the same. It does not, and the project already knew: L03 established that the pm3 samples once per carrier cycle and reads real Indala fine, so genuine PSK is demodulable at this rate. The demodulators then settled it directly — `--p1` nothing, `--nr` the credential. ⇒ When a criterion's null result has a plausible instrument explanation, the way out is an arm the instrument is known to succeed on, not more argument about the sampling.

**M58 — A SMALL-n ZERO ON AN INTERMITTENT ARM IS NOT A ZERO, AND I MADE THIS MISTAKE ONE CLAIM
AFTER DIAGNOSING IT IN SOMEONE ELSE'S WORK.**

C491 established that C488's *silent* arms were under-sampled: one capture per arm cannot tell a
silent emitter from an intermittent one. **Two claims later I recorded `nexwatch` as 0 of 9 and
built an inference on it** — C493 said the frame-length model *over-predicts nexwatch, it says
1 in 3 and the bench says 0 of 9*. At n=24 nexwatch is **6 of 24, almost exactly the 1 in 3 the
model predicted.** The model was right and my n was too small.

⇒ Before a **rate** goes in a claim, state n and the interval it supports. At p ≈ 0.3, nine trials
return zero about 4% of the time — often enough to happen, and it happened. ⛔ And a zero is the
most dangerous cell to under-sample, because it reads as a *property* (this arm does not work)
rather than as a measurement.

⭐ The cheap discipline: a rate claimed from fewer than ~20 trials is written as a rate with its n
attached, never as a capability. "0 of 9" is a number; "nexwatch does not decode" is a claim, and
only the second one was wrong.

**M59 — ON A BENCH WHOSE RATE WANDERS, COMPARE INTERLEAVED OR DO NOT COMPARE.**

C497 measured the same arm, same credential, same rig, one evening: **88%, 60%, 38%, 75%**. Not a
decline — a wander. Every cross-arm number in C491 and C494 was taken arm by arm, in sequence, and
so silently assumed a stationary bench.

⇒ **A rate measured in session A and a rate measured in session B are not comparable**, however
large n is in each. n fixes the sampling error inside a session; it does nothing about the session.

⭐ The fix costs nothing but arrangement: **round-robin the arms one capture at a time**, so
whatever is drifting moves all of them together. The indala-vs-keri result (6/10 against 0/10,
alternating) is the only cross-arm comparison in this round that is safe, and it is safe for that
reason alone.

⚠ What survives non-stationarity is a **zero against a non-zero** — *this arm decoded at all* is a
fact about the arm, not about the hour. What does not survive is *this arm decodes more often than
that one*. M58 is about n; this is about when.
**M60 — ORDER IS A VARIABLE. IF EVERY SETTING SITS AT A FIXED POSITION IN THE SEQUENCE, YOU ARE
MEASURING POSITION AND CALLING IT THE SETTING.**

M59 fixed *when* arms are measured relative to each other. This is the same disease one level down:
inside a single session, a ladder of settings run in ascending order gives each setting a **fixed
position after the arming**, so position and setting are perfectly correlated and nothing in the
result can separate them.

⭐ **It was caught by a control that could have failed, and only just.** Sweeping read length per
arm, ascending, `lf keri reader` scored **2/12** against `lf read -s 10000` + `lf keri demod`
**10/12**. But `cmdlfkeri.c:222` is `lf_read(false, 10000); demodKeri()` — the *same count* through
the *same demodulator*. The two differed in nothing but where they sat in the sequence, so a 5x gap
was position and could be nothing else. ⇒ The rung was in the ladder as a control on the reader
command's code path (a hypothesis worth testing after C487), and instead it caught the experiment.

⛔⛔ **AND THE EFFECT WAS AS BIG AS THE ONE BEING MEASURED.** Shuffled, `gproxii` went from 0/12 to
5/12 on its own reader, and its 12,288-sample rung from **0/12 to 8/12**. The ascending run's zeros
were an artifact of position, and would have been written up as a knife-edge length window —
a false mechanism, with n=12 behind it and a tidy story attached.

⭐ The fix is one line: **shuffle the ladder within each round, and record the seed.** It costs
nothing, it needs no extra captures, and it turns position from a confound into noise.

⚠ **This retroactively qualifies every fixed-order sweep in this project**, C498 included: there
the short read was the *second* command after arming every time. Its zero-versus-non-zero findings
survive (M59's last paragraph), its levels do not.

⭐⭐ **BUT IT DOES NOT BITE EVERY SWEEP EQUALLY, AND THE DISCRIMINATOR IS WORTH HAVING: WAS THE
STATISTIC PREDICTED IN ADVANCE, OR READ OFF THE SWEEP'S OWN SHAPE?** `shortread.py` was exposed
because "which length is best" is read off the shape — whatever shape the run produces becomes the
answer, so a drift with the right sign IS an answer. `holdsweep.py` sweeps in exactly the same
fixed ascending order, and **C469 is not exposed**, because its criterion was fixed in the file
before the firmware existed: modal run = N x 256us, slope 1 through the origin. A drift cannot land
on nine independently predicted values within one sample each.

✅ **CHECKED RATHER THAN ASSUMED.** `holdsweep.py --seed 7`, order **2 7 8 5 1 9 4 3 6**: every N
still on its prediction, 91.8-97.2% modal share against C469's 84.5-97.6%, slope 1, no knee. ⇒ A
shape-read statistic needs shuffling; a predicted one carries its own control. ⛔ Shuffle anyway —
it costs nothing, and the next question asked of an instrument is rarely the one it was built for.

⇒ Before any sweep: **ask what else is monotone with the variable**, and **ask whether your
criterion was written before the data**. Position, temperature, time since arming, buffer state and
sequence index are all free to masquerade as the thing being swept — and only a prediction made in
advance can tell you they did not.


**M61 — A RATE OVER BACK-TO-BACK READS IS A STATISTIC ABOUT THE SCHEDULE. SHUFFLING DOES NOT FIX
IT; SPACING DOES.**

M60 removed position as a confound by shuffling which SETTING sits at each position. ⛔ **That
leaves the position effect itself completely intact** — it converts a systematic bias into uniform
noise, which is exactly what "the hit rate wanders" looks like, so the disease survives its own
treatment looking like ordinary variance.

⭐⭐ **THE MEASUREMENT (C507).** Six IDENTICAL reads per session, one arming, sixteen sessions:
`gproxii` returned the pattern **`.X.XX.` in 16 of 16** — index 1, 3 and 4 decoding every time and
index 0, 2 and 5 **never** — and `indala` had **index 5 at 16/16 against index 4 at 0/16**,
adjacent reads of the same command. At a ~50% base rate one fixed six-bit pattern cannot repeat
sixteen times. ⇒ **Within a session the decode is not a coin; it is a function of position.**

⛔ **AND IT IS THE AIR, NOT THE CLIENT — which had to be shown and not assumed.** The rival was
*the nth read of a pm3 session is intrinsically different*. `msleep` is `AlwaysAvailable`
(`cmdmain.c:365`): it touches no device and raises no field, so it cannot change which read is the
nth. Inserting it moved the pattern on 3 of 4 delays for `gproxii` and 4 of 4 for `indala`, and
swung `gproxii`'s rate from **29.2% to 70.8%**. A pure host pause did that.

⇒ **The mechanism is C486's beat.** Each read lands at its own phase of a ~61-80 ms null cycle, and
a fixed read cadence makes that phase deterministic. So repeats issued back to back do not sample
independently — **they resample one phase, and AGREEMENT CAN BE MANUFACTURED BY THE SCHEDULE.**

⚠ **THIS IS THE DANGEROUS DIRECTION.** M58, M59 and M60 all produce noisy or shifted numbers; this
one produces *tight* ones. A `--repeat 10` run back to back would report ten agreeing reads and a
confident rate, and the confidence would be the artifact. ⛔ That is the same shape of error as
grading with no calibration row, which is the thing this project exists to prevent.

⭐ **The fix is spacing, drawn and not constant.** A constant pause is just another fixed cadence —
the very thing measured as deterministic here. `benchmatrix`' `--repeat` now waits a random
interval spanning more than one beat period between repeats (`rfid-tools` `f22feec`), with the
span, the drawing and the between-not-before placement each pinned by a break-tested test.

⇒ Before quoting any rate: **ask whether the reads that produced it were independent, or merely
consecutive.** Independence is a property of the schedule, and on this bench it has to be bought.


**M62 — A POOLED CONTRAST IS NOT ROBUST TO A SINGLE-CELL OUTLIER. PRE-REGISTER THE SHAPE AS WELL
AS THE GAP, OR ONE CELL WILL MANUFACTURE YOUR EFFECT.**

K12's P2 was written before its capture and operationalised H_settle — *the rate rises with time
since field arrival* — as a pooled contrast: **top three cells minus bottom three >= 20 points.**
It fired at **+21 points**. ⛔ **And it was an artifact of where one cell fell.**

The profile was nine cells in 83-100% with a single notch of 8% at 60 ms. That notch sat in the
bottom bin, so bottom three {20,40,60} = 67% against top three {160,180,200} = 88%. **Bottom TWO
{20,40} = 96% — higher than the top three.** The ten cells were non-decreasing in **5 of 9**
adjacent steps and the two large steps were **-83 and +83**: the notch's own walls.

⇒ **There was no rise. There was a notch in the bottom bin, and the band could not tell.**

⚠ **Pre-registration did not save it, and that is the point.** M55's discipline was followed
exactly — the criterion was committed before the capture (`d4cf4b53`) — and the criterion was
still wrong, because a three-cell mean is a statistic that a single outlier moves by a third of
its own range. **Pre-registering a bad statistic pre-registers a bad answer.**

⭐ **The fix is one extra clause and it costs nothing**: a rise is claimed only if the contrast
fires **AND** the profile is non-decreasing in >= 7 of 9 adjacent steps. Q3 added it before K13,
and K13's own data then failed it too (5 of 9 again, contrast not even firing) — so the amendment
was not fitted to the run that prompted it.

⛔ **The consequence was pre-stated so it could not be argued afterwards**: if the notch replicated
and Q3 failed, **K12's P2 is withdrawn**. Both happened, so it is withdrawn.

⇒ Before pre-registering a pooled statistic, ask: **what single cell could produce this number on
its own?** If one can, the criterion needs the shape as well as the level. ⭐ And the cheap general
form: **a contrast says how far apart two bins are; only a monotonicity check says there is a
trend.** They are different claims and the second is usually the one being made.


**M63 — A BENCH-MOVED CONTROL MUST NOT PRESUME THE SHAPE IT IS POLICING, OR A HEALTHY RUN READS AS
A BROKEN BENCH.**

K12's no-primer control is checked against **<= 15%**, because C512 measured `gproxii` at 0% on a
fresh burst. ⛔ Carrying that band to `indala` and `keri` would have been wrong on its face — C514
had already measured that those arms do **not** collapse on a fresh burst (50% → 67%, 33% → 54%) —
so K15 gave them a different check: **cells 40 and 80 must sit within 20 points of the arm's own
median.**

⛔⛔ **Both arms failed it, and both runs were declared uninterpretable by a control that was
measuring the result.** Those arms have a **hump**: the profile peaks in the middle of the ladder
and collapses at both ends, so cells 40 and 80 **are the wings** and sit 30-50 points below the
median **because the effect is real**. The control could not distinguish *the bench moved* from
*the profile has structure at the anchors*, which are opposite conclusions.

⇒ **A control has to be independent of the hypothesis.** "The ends look like the middle" is a
flatness assumption, and flatness was the null being tested.

⭐ **What replaced it, and why that one is legitimate: a SHAPE-AGNOSTIC drift check.** Does the run
agree with **itself** across its own rounds — pooled rate between the first and second halves
within 15 points, and the peak cell within 25? It can fail, it presumes nothing about the profile,
and it certifies the thing that actually licenses within-run structure. Both arms passed it in K16.

⚠ **And note what no control here can do.** On a bench that gave the same arm 88%, 60%, 38% and
75% in one evening (C497), **no absolute-level control can certify a session.** Drift within a run
is certifiable; the level is not. ⇒ Which is also why M59's interleaving is not optional here: it
is the only thing that makes a comparison survive the wander.

**M64 — A CRITERION THAT TESTS A SHAPE BY ITS ARGMAX FIRES ON ONE CELL OF NOISE. SPECIFY THE
SHAPE.**

K16's T1 required three things at once: the peak cell must be **60 or 65 ms**, that cell must be
**>= 75%**, and both wings must be **<= 30%**. `keri` met all three and fired T1. `indala` met the
second and third — its wings were **tighter** than in the run that generated the hypothesis (max
29% against 42%) — and **failed only because its argmax moved one 5 ms cell, to 55.**

⇒ **Two arms whose shape reproduced equally well got opposite verdicts on a difference of one
cell.**

⭐ **C504 had already learned this in a different variable.** Sweeping read length, a second
session moved the argmax for **four of the five** arms that decode, and the conclusion written then
was *carry the range, not the number*. T1 re-imported the error as a location clause, one round
later, in the same file.

⭐ **The fix is to say what a shape IS**: wings below X, a contiguous run of Y cells above Z, a
peak anywhere inside a named **window** rather than at a named cell. All of those reproduce when
the shape reproduces, and none of them turns on a single cell.

⚠ **M62, M63 and M64 are one disease in three places** — a statistic whose value is decided by a
single cell (M62), a control that presumes the shape it is policing (M63), and a criterion that
reads a shape off its argmax (M64). ⛔ All three were pre-registered before their captures, and
**being pre-registered did not make any of them right.** ⇒ Pre-registration protects against
fitting a criterion to the data; it does nothing about a criterion that was badly built in the
first place. **Ask of every band: what single cell could decide this?**

**M65 — A REFUTATION BUILT ON ONE CONTRAST ARM INHERITS EVERY WAY THAT ARM DIFFERS.**

C519 asked whether C517's lead-time hump travels with the protocol's own frame length. `indala`
and `keri` share a 2048-sample frame and so cannot answer it; the only arm on this bench with a
different frame is `gproxii`, and its banked K12/K13 ladders refuted the frame reading cleanly —
**80.6% and 78.5% where the hypothesis needed <= 30%**, in two independently seeded runs.

⛔ **But `gproxii` is also the only ASK/biphase arm, and the criterion named only the frame.** So
the refutation cannot separate *the hump does not travel in frames* from *the hump is a PSK
effect*. Naming frame length as the variable under test did not make it the only variable that
moved with it, and nothing in the pinned band said so.

⚠ **This is C514's lesson pointed the other way.** C514 is *a finding from one arm does not
generalise*. M65 is *a REFUTATION from one contrast arm is confounded by everything that arm
differs in*. A refutation feels like the conservative direction, which is exactly why the confound
went unwritten for a whole unit — the band was scrutinised for what it might wrongly CLAIM, and
not for what it might wrongly DENY.

⇒ **Before a cross-arm criterion is pinned, list every property that differs between the arms it
compares — not only the one under test — and say which further arm would break the confound and
what it would show.** C519's is `nexwatch`: PSK like the humped arms, but a 4096-sample frame, so
the two readings put its peak **66 ms apart** (65 ms against ~131 ms). That sentence belongs in the
criterion, not in the write-up.

**M66 — A POOLED BAND OVER A WINDOW PRESUMES THE FEATURE FILLS THE WINDOW.**

K17's U2 asked for a 30-point gap pooled over five cells, 120-140 ms, built around H_frame's
point prediction of 3.97 frames. The data put a reproducing rise at **135 and 140 ms only** —
62-88% in both seeds — and the other three cells of the window sat at 0-25%. Pooled, that is
+22.5 and +33.8 points, and U2 did not fire.

⛔ **The band did not fail because the feature was absent. It failed because the feature occupied
two cells of a five-cell window and the other three diluted it below the threshold.** U1's window
had low cells in it too (70 ms at 25%, 75 ms at 0%) and fired only because its rise was three
cells wide rather than two.

⚠ **And the window was built from the wrong statistic I already had.** `indala` and `keri` peak
at 3.97 frames but their hump SPANS 3.05-4.27 (C517). C504's rule is *carry the range, not the
number* — and U2's window was centred on the number, then pooled as though the feature would fill
it. Having the range and building the window from the point estimate is the same error C504 named,
one level up: it moved from the peak I report to the window I test.

⇒ **State the window from the known RANGE, and score a CONTIGUOUS RUN inside it rather than the
whole of it**: *k adjacent cells above the wings* fires on a narrow feature and on a broad one, and
does not reward a feature merely for being wide. ⛔ A pooled window and a run clause answer
different questions and only the run clause is robust to a feature narrower than the window.

⚠ This is the fifth member of M62's family — a statistic decided by one cell (M62), a control that
presumes the shape it polices (M63), a criterion reading a shape off its argmax (M64), a refutation
inheriting its one contrast arm's every difference (M65), and now a pooled window presuming the
feature fills it. **All five were pre-registered.**

**M67 — WINGS FIXED BY POSITION ARE ONLY SAFE IF THE POSITION IS KNOWN TO BE OUTSIDE THE
STRUCTURE.**

K18 was written to escape M66. M66's disease is a window whose contents are chosen after the data
arrives, so K18 fixed its wing cells by POSITION — the four cells at the ladder's two edges, named
in the criterion before the capture. That felt like the principled fix. It is not one.

⛔ **An edge is not outside anything. It is wherever the ladder happened to stop.** `keri`'s
100 and 105 ms cells came back at 62-88%, so the low wing landed on an elevated region, and the
band's inner-minus-wings gap went **negative** on an arm with a reproducing three-cell feature at
140-150 ms. It returned *no second feature here* about a profile that plainly has one.

⚠ **This is M63 arriving by a different route, in the round that cited M63 while writing the
band.** K15's anchors control compared the ladder's end cells to the median and failed because on
a hump those cells ARE the wings. M63's lesson was recorded as *do not presume the profile is
flat*. K18 presumed something narrower and just as unwarranted: *the profile is flat AT THE
EDGES*.

⇒ **A wing has to be justified, not merely located.** Either (i) it is low by INDEPENDENT prior
measurement — a different run, not the one being scored — or (ii) the ladder is wide enough that
the profile has demonstrably returned to a floor, which is a thing the run has to SHOW rather than
a thing the design may assume. ⛔ When neither holds, the comparison to make is not
window-against-wings at all: score the run against **the rest of the ladder excluding the run**,
and report that the reference is the ladder's own body rather than a baseline.

⚠ **And when a band misfires this way, the conjunct that would have fired is not the answer.**
K18's clause (c) fired for both arms. Promoting it after V1 failed would be M62 — a verdict chosen
from the statistic that happened to agree. The band did not fire; the finding is that the band was
mis-built, and the measurement is banked for a ladder that is not.

**M68 — A THRESHOLD OF *MEDIAN + Δ* HAS NO POWER WHEN THE MEDIAN IS WITHIN Δ OF THE CEILING. AND
IT IS M67'S OWN FIX THAT CREATED IT.**

K19 answered M67 by refusing to designate wings at all: *elevated* was defined as **median + 25
points**, referenced to the arm's own ladder, which presumes nothing about where the quiet is. On
`keri`, median 25-31%, it worked — two features found, both seeds, W1 fired.

⛔ **On `indala` the median is 75%, so the threshold is 100% — the ceiling.** A feature required
two adjacent cells at *exactly* 100% in both seeds. W1 came back REFUTED on an arm whose profile
visibly has structure (0-25% at 120-125, 12-25% at 165-175, against a body at 75-100%). **The band
could not have fired whatever the data did.**

⚠⚠ **EACH FIX INTRODUCED THE NEXT FAILURE, AND THAT IS THE PATTERN WORTH KEEPING.** M63 said a
control must not presume the profile is flat ⇒ K18 fixed its wings by position ⇒ M67, an edge is
not outside anything ⇒ K19 referenced the arm's own median ⇒ M68, a relative threshold inherits
the arm's headroom. **Three consecutive criteria, each written to repair the last, each broken in a
new place.** ⇒ The repair is not a better rule; it is a **check run against the design before the
capture**.

⇒ **Before pinning any relative threshold, compute it against each arm's KNOWN level and state
what the band can detect there.** If the threshold lands within a few points of 0% or 100% for an
arm, that arm has no power and the criterion must say so **in advance** — the way K17's high half
was declared uninformative before it ran, which is the one time this project got it right.
⭐ And when a profile is high, the detectable feature is a **notch**, not a hump: the inverse
threshold (median − Δ) has the power the forward one lacks, and an arm should be given the
detector its own level can support.

**M69 — ONE SHUFFLE FOR EVERY ARM IS A CROSS-ARM CONFOUND, AND ASKING *WHAT DO THE ARMS SHARE?*
IS WHAT FOUND IT.**

`burstsync.py`'s `k12()` built a single shuffled `plan` and ran **every arm through it**, so both
arms saw the identical cell→position mapping in every round. M60 established that **position is a
variable on this bench** — `lf keri reader` scored 2/12 ascending against 10/12 shuffled, same
count, same demodulator. ⇒ whatever position contributes enters every arm's profile **identically**,
and manufactures a positive **cross-arm** correlation out of nothing.

⚠ **It does not touch a WITHIN-arm verdict**, which is every band from K12 to K20, so nothing
banked is retracted. It is fatal only to a statistic that compares two arms cell by cell — and the
first such statistic was K21's. ⇒ `--per-arm-shuffle` derives and records a separate seed per arm,
and the scorer **REFUSES by name** a cap taken without it rather than trusting whoever ran it to
have remembered the flag.

⇒ **The general form: before comparing two arms, enumerate what the harness gives them in common.**
The shuffle was introduced as the fix for M60 and became a confound one question later.

**M70 — A BAND WITHDRAWN BY SIMULATION BEFORE ITS CAPTURE, WHICH IS THE FIRST TIME THAT HAS
HAPPENED HERE, AND THE ONLY REASON IS THAT THE POWER WAS COMPUTED.**

K21's Y2 asked whether the two arms' profiles are OFFSET: cross-correlate at lags 0, ±5..±25 ms and
fire when the best non-zero lag beats lag 0 by >= 0.30 in both runs, with the sign agreeing. It was
pre-registered, it was replicated across two seeds, and it was **broken**:

- **41% false-fire rate** against a null of two INDEPENDENT smooth profiles. The median null gain
  is **+0.37**, already above the threshold, because **lag 0 is one correlation and *the best of
  ten lags* is an order statistic.** A maximum's null is not zero.
- Raising the bar to a 0.3% false-fire rate needs a gain of **1.10**, and at that bar a **real
  20 ms offset fires it 0.3% of the time.** Unusable at either end.
- A fixed, signed, single pre-registered lag removes the argmax and is better — **37% power at a
  5% false-fire rate, 18% at 2%** — and still not enough to make a silence mean anything.

⇒ **Y2 was withdrawn before the bench was touched**, its lag table kept as labelled exploratory
output with the power printed beside it. The answer it can honestly give is *the offset is not
measurable on this ladder at this n* — which is a statement about the instrument, not about the air.

⚠⚠ **THE POINT IS NOT THE STATISTIC, IT IS WHERE THE DEFECT WAS CAUGHT.** M62, M63, M64, M66, M67
and M68 were all pre-registered and all six were found broken **by the data they were built to
judge** — six captures spent discovering that six criteria could not carry a verdict. M70 was found
by 4,000 draws of a simulated null and **cost no bench time at all**.

⇒ **Simulate every band against a null and against the effect it claims to detect, before the
capture.** Two numbers: the false-fire rate under no effect, and the power under the effect as
described. If either is unacceptable the band is not ready, and finding that out on the bench is
paying for it twice. ⭐ Y1, simulated the same way, came back at 77% power with symmetric ~2%
errors and a real 20 ms offset landing in NO VERDICT 98% of the time — so the method does not only
kill bands, it licenses them.
