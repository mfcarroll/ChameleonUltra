# Where to pick this up — ranked, with why

⭐ **Resuming in a fresh session? Paste `RESUME.md`** — self-contained context, environment and commands.

**State at 2026-09-11 (updated).** The framing has changed, and for the better: the problem
is **not SNR**. A synthetic PSK1 frame of `a0000000e6bd0e92`, injected into the **real
measured empty-field noise** at **half** the tag's own fc/2 amplitude, decodes at **0/31**
data-bit errors from a **single** capture. The real tag, at **four times** the band SNR
after stacking, gets **7/31** and never improves.

⇒ **What reaches the converter has the right fc/2 amplitude and the wrong phase
structure.** The question is no longer "how do we find 7.6 dB" — it is **"where does the
polarity go?"** See `README.md` §0z.

⛔ **Three numbers that used to look like progress are retracted** (`README.md` §0z2):
folding at 2048 samples *cancels* the data (19 ones = odd parity, so the true period is
4096); the fc/2 band-SNR criterion is polarity-blind; and "52/64 bits vs a 45/64 null" was
a constant preamble run scoring itself — on the 31 credential bits the tag gets 6/31 and
the **null gets 4/31**.

---

## 1. ⭐⭐⭐ Sweep sample phase while scoring BIT RECOVERY. Never done.

The 32-tick optimum was found by maximising the **sideband skirt**. The skirt is
transition energy and is **polarity-blind**. The polarity lives in the component *at*
62.5kHz = Nyquist, recovered as `2A·cos φ` — which has a **hard null** where the skirt has
none. There is no reason the two should share an optimum, and every reason they should not.

⇒ **The 32-tick "optimum" may sit at or near the polarity null.** That single possibility
explains every observation: full skirt amplitude, frame structure visible in the envelope,
and no recoverable sign.

**Do:** modify `phasesweep.py` to score `stack.py`'s data-bit errors at each phase instead
of the sideband amplitude, and sweep all 128 ticks with the tag on. Cheap, one bench run,
and it directly tests the leading hypothesis.

⚠ Take paired empty captures at each phase as usual — the null is what makes a low error
count mean anything, and at 31 bits brute-forced over 2048 positions × 64 rotations the
null lands around 4–5 errors.

## 2. ⭐⭐ Longer captures — the current 4096 samples is 1.8 frames, less than one period

The true repetition period is **4096 samples** (odd parity inverts the subcarrier every
frame). A capture of 4096 samples minus 400 settle is **0.9 super-frames** — not enough to
fold even once at the true period. `LF_SNIFF_MAX_SAMPLES` is 8192 **bytes** = 4096 samples
at 16-bit; the chunked transfer already handles the retrieval.

⇒ Raising it to 16384 bytes (8192 samples = 2 super-frames) costs 8KB more static RAM and
makes within-capture folding at 4096 possible. Worth doing before any long campaign.

## 3. ⭐ Re-test air gap — position measured ~5.7 dB, n=1

An accidental probe at a different placement gave tag/empty **3.49x** where the paired run
gave **1.80x**, with the difference concentrated at high frequency (1.08x at 1–5k rising
to 1.79x at 55–62k), so it is the tag's contribution and not the reader's ripple. `§0a3`'s
"air gap closed" was already invalid — pre-BLE-fix, wrong payload. Redo it with
`--repeats` and score bits, not just the skirt.

## 4. Re-test settle — the closure is invalid for the same reasons as air gap

Pre-BLE-fix, and on a tag carrying `DEADBEEF/12345678` rather than an Indala frame. The
conclusion was a **null**, and a null is exactly what noisy data manufactures.

## 5. ⛔ CLOSED — `LF_RSSI` (AIN0) carries no fc/2

Measured, 7 repeats, `README.md` §0z5. Within-node tag/empty **1.04x against a 1.28x
scatter**; spectrum flat to **0.5 dB** across 1–62kHz where AIN5 rolls off **31.6 dB**.
Not clipped (zero samples at full scale, 419 counts of headroom vs 18 counts of σ) and
demonstrably **alive** — the tag shifts its DC by +16 counts with ±0 spread across 7
repeats. It is a DC field-strength indicator whose corner is below 1kHz. Nothing here.

## 6. ⛔ CLOSED — gain. The floor is analog-referred

`README.md` §0a, 7 repeats, deglitched. Unchanged by today's work.

## 7. ⛔ Do NOT fold or average at 2048 samples

It averages a frame against its own inverse. The **skirt** survives, so band SNR improves
as sqrt(N) and looks like progress while not one bit is recovered. Zero-offset **stacking**
(`stack.py`) is the polarity-preserving method — validated at +7.3 dB with all three
controls — but it buys SNR and, per item 1, SNR is not what is missing.

⚠ The old ⛔ on coherent frame averaging gave the wrong REASON. The circularity argument
("you need SNR to align frames and alignment to gain SNR") does not apply: captures are
frame-locked to field-on and cross-correlate at lag 0 with one sign, so there is no
alignment step at all. The method works; it just does not solve this problem.

---

## Method rules this project paid for

1. ⛔ **A single capture is not evidence.** Pre-fix, 41% of captures carried a field
   dropout and six consecutive captures at one setting spread **49x**.
2. ⛔ **Always run the null.** And the null must be the **empty field**, not white noise —
   white noise is a far weaker control against a chain whose floor spans 17x across bands.
3. ⛔ **Confirm the tag state, and understand what the confirmation says.**
4. ⛔ **A synthetic threshold is not a real threshold.** The matched filter's +8.2 dB held
   against white noise and vanished on real captures — and the 12kHz low-pass that helps
   real captures *hurts* synthetic ones, because the boxcar is already optimal for white.
5. ⛔ **No thresholds in verdicts.** Fit the shape and report the fit.
6. ⛔ **Check that a measure is comparable before comparing it.** Band RMS across bands
   whose floors differ 17x; AIN0's absolute counts against AIN5's floor; a phase-32
   capture against a phase-0 baseline. Three separate instances of the same error.
7. ⛔ **Make sure the measure can see the thing you are claiming.** The band-SNR criterion
   is polarity-blind and a whole-frame Hamming score credits a constant run. Both passed
   while nothing was decoded.
8. ⭐ **Inject a known signal into the real noise.** One test settled what a dozen sweeps
   could not: the amplitude is there and the phase is not.
