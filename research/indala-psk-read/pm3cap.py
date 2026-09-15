#!/usr/bin/env python3
"""Run structure from the Proxmark's raw LF sample buffer — the comparator-free instrument.

    ./pm3cap.py --label "real PAC tag"
    ./pm3cap.py --samples 40000 --expect 256,512
    ./pm3cap.py --json

⭐ WHY THIS EXISTS. C464 established that the pm3's raw sample buffer is the only instrument on
this bench with NO comparator anywhere in the chain, and that it resolves PAC's entire static
range on a real tag (256us n=240, 512us n=31, 768us n=41, longest 2280us against a theoretical
2304us). It did that with ad-hoc shell typed at the client. Two open units — the `hw emuhold`
sweep and the like-for-like emulation-vs-tag capture — need exactly that measurement over and
over, and C465 has just retired the Flipper as a judge for PAC, so this is now the instrument of
record rather than a second opinion. ⇒ it becomes a tool.

⛔⛔ A CAPTURE IS NOT A DECODE, AND THAT IS THE ENTIRE POINT. Nothing here calls a pm3
demodulator. The 2026-09-14 "the Proxmark does not read our LF emulation" dead end ran
`lf search` and `lf indala reader` — DECODERS — and was logged as inconclusive between *pm3
cannot hear it* and *the emulation is silent*. The operator's own T5577 deep-read work killed
two further conclusions the same way (`lf t55xx detect` never reads block 0; it SYNTHESIZES the
config word from the waveform, so it is structurally blind to any modulation pm3 cannot
demodulate). A decode failure is not evidence about the signal. Amplitude is.

⭐ THE SAMPLE PERIOD IS 8 us, AND IT IS NOT TAKEN ON FAITH. `lf read` samples at 125 kHz (the
client's default LF divisor), so one sample is one 8us carrier cycle — the same tick the PWM
counter_top values are counted in, which is why a counter_top of 32 shows up here as a 256us
run. That identity is CROSS-CHECKED, not assumed: C464's capture of a real PAC tag put its
most-common run at exactly 256us, which is `pac.c`'s `PAC_RF_PER_BIT 32` at this scale. If a
future pm3 client changes the default, that peak moves and --us-per-sample is the knob.

⛔ `data save` DOES NOT OVERWRITE — it writes name.pm3, then name-001.pm3, and so on. Reading a
fixed filename reported the SAME stale capture forty times running while the device was moved
around the bench and even lifted out of the field (pm3monitor.py's docstring). Every candidate
is deleted first and whatever file appears is the one read; if none appears, that is an error
and not a silent re-report of the last good reading.
"""
import argparse
import glob
import json
import os
import subprocess
import sys
from collections import Counter

import numpy as np

PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
TMP = "/tmp/pm3cap"
US_PER_SAMPLE = 8.0


def capture(samples, timeout=180):
    """One `lf read` into the sample buffer, saved to disk and read back as signed ints."""
    for old in glob.glob(TMP + "*.pm3"):
        try:
            os.remove(old)
        except OSError:
            pass
    cmd = "lf read -s %d; data save -f %s" % (samples, TMP)
    r = subprocess.run([PM3, "-c", cmd], capture_output=True, text=True, timeout=timeout)
    found = sorted(glob.glob(TMP + "*.pm3"))
    if not found:
        return None, (r.stdout + r.stderr)[-300:]
    vals = np.array([float(t) for t in open(found[0], "rb").read().split() if t.strip()])
    return vals, None


def runs(vals, hyst=0.15, min_run=2, want_levels=False):
    """Level-hold lengths in samples, via a Schmitt trigger about the mean.

    ⛔ A bare threshold at the mean turns every noise wiggle into a transition and fills the
    histogram with 1-sample runs that drown the real structure. The hysteresis band is a
    fraction of the half peak-to-peak, so it scales with coupling instead of being a magic
    number tuned on one capture. Runs shorter than --min-run are dropped as jitter and COUNTED,
    so a capture that is mostly jitter says so rather than looking clean."""
    if len(vals) < 4:
        return [], 0, 0.0
    centre = float(vals.mean())
    ptp = float(np.ptp(vals))
    band = hyst * ptp / 2.0
    hi, lo = centre + band, centre - band
    out, dropped = [], 0
    state = None
    start = 0
    for i, v in enumerate(vals):
        if state is None:
            state = 1 if v >= hi else (0 if v <= lo else None)
            start = i
            continue
        if state == 1 and v <= lo:
            n = i - start
            if n >= min_run:
                out.append((n, 1) if want_levels else n)
            else:
                dropped += 1
            state, start = 0, i
        elif state == 0 and v >= hi:
            n = i - start
            if n >= min_run:
                out.append((n, 0) if want_levels else n)
            else:
                dropped += 1
            state, start = 1, i
    return out, dropped, ptp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", type=int, default=40000)
    ap.add_argument("--label", default="capture")
    ap.add_argument("--top", type=int, default=8)
    ap.add_argument("--hyst", type=float, default=0.15)
    ap.add_argument("--min-run", type=int, default=2, help="samples; shorter runs are jitter")
    ap.add_argument("--us-per-sample", type=float, default=US_PER_SAMPLE)
    ap.add_argument("--expect", default=None,
                    help="comma-separated run lengths in us that must each hold >= --floor "
                         "of all runs. STATE THIS BEFORE CAPTURING, derived from the emitter "
                         "source, never from a histogram you have already seen (M-rules).")
    ap.add_argument("--floor", type=float, default=0.05)
    ap.add_argument("--json", action="store_true")
    # ⛔ THE EMULATION PLAYS IN BURSTS, and across the pause between bursts the line sits static
    # for milliseconds. Those gaps are not runs of the frame and they dominate both the maximum
    # and the duty: leave them in and every capture reports a millisecond ceiling and a duty
    # pulled toward whichever level the emitter idles at. The cut is stated as a NUMBER ABOVE THE
    # FRAME'S OWN LONGEST RUN, never fitted to the data, and what it removed is always reported.
    ap.add_argument("--gap-above", type=float, default=None,
                    help="us; runs longer than this are inter-burst gaps, excluded and counted")
    ap.add_argument("--duty", action="store_true",
                    help="report the time split between the two levels. ⛔ POLARITY IS NOT KNOWN: "
                         "the pm3 samples an envelope and a Chameleon HIGH may read either way, "
                         "so BOTH shares are printed and the caller compares to a prediction and "
                         "its complement")
    a = ap.parse_args()

    vals, err = capture(a.samples)
    if vals is None:
        print("  ⛔ no trace written — is the Proxmark free? %s" % err)
        return 2
    rl, dropped, ptp = runs(vals, a.hyst, a.min_run, want_levels=True)
    levels = [lv for _, lv in rl]
    us = [r * a.us_per_sample for r, _ in rl]
    gaps = 0
    if a.gap_above is not None:
        keep = [i for i, d in enumerate(us) if d <= a.gap_above]
        gaps = len(us) - len(keep)
        us = [us[i] for i in keep]
        levels = [levels[i] for i in keep]
    hist = Counter(us)
    total = len(us)
    top = hist.most_common(a.top)

    if a.json:
        print(json.dumps({"label": a.label, "samples": len(vals), "runs": total,
                          "dropped": dropped, "ptp": ptp,
                          "longest_us": max(us) if us else 0.0,
                          "top": [[k, v] for k, v in top]}))
        return 0

    print("  %s: %d samples, %d runs (%d dropped as jitter), p-p %.0f"
          % (a.label, len(vals), total, dropped, ptp))
    if a.gap_above is not None:
        print("  excluded %d run(s) longer than %.0fus as inter-burst gaps" % (gaps, a.gap_above))
    if not total:
        print("  ⛔ NO RUN STRUCTURE — nothing is modulating this field, or the pad is not coupled.")
        return 1
    print("  top runs (us): " + ", ".join("%g:%d" % (k, v) for k, v in top))
    print("  longest run: %g us" % max(us))
    if dropped > total:
        print("  ⚠ more jitter than structure (%d dropped vs %d kept) — distrust this capture"
              % (dropped, total))
    if a.duty and us:
        hi = sum(d for d, lv in zip(us, levels) if lv == 1)
        lo = sum(d for d, lv in zip(us, levels) if lv == 0)
        if hi + lo:
            print("  level split: %.1f%% / %.1f%%   (⛔ polarity unknown — compare against BOTH "
                  "the prediction and its complement)" % (100 * hi / (hi + lo), 100 * lo / (hi + lo)))
    if a.expect:
        want = [float(x) for x in a.expect.split(",")]
        ok = True
        for w in want:
            share = hist.get(w, 0) / total
            hit = share >= a.floor
            ok = ok and hit
            print("    %s %g us -> %d runs, %.1f%% of all runs (floor %.0f%%)"
                  % ("✓" if hit else "✗", w, hist.get(w, 0), 100 * share, 100 * a.floor))
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
