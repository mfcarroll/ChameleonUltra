#!/usr/bin/env python3
"""⛔⛔⛔ DO NOT TRUST THIS TOOL — IT MEASURES NOISE, NOT TONES (C401). RETAINED AS EVIDENCE ONLY.

It reports `ZERO long tones — this is U11's signature` on an EM410X emission that the receiving
device DECODES CORRECTLY in the same minute. Every histogram it produces decays monotonically from
the 6-sample floor instead of peaking at the tone lengths, so its "bands" are bins of a noise curve.
The cause is the threshold below: a 64-sample moving mean smears an EM410X half-bit (32 samples)
flat, and the AC-coupled baseline (~27-sample time constant, C204) finishes the job.

⚠ C391 claimed this was validated against a known-good reference. It was not — the reference output
had the same decaying shape and I read it as two bands, because I never said in advance what a PASS
would look like. That is M50 followed in form and failed in substance.

⇒ Rebuild any tone analysis on the project's PROVEN demodulators (askdemod.py, ctest/cdemod), which
are validated against real captures. Do not extend this file.

Histogram the TONE PERIODS in a reader capture — the measurement U11 turns on.

    ./tonehist.py /tmp/cap.bin [--expect 8,10]

⭐ WHY THIS EXISTS. Every FSK number this branch has taken came through the FLIPPER's raw
reader, which is a black box we infer from: it reports edge timings in its own units after its
own comparator and AGC. `rdrcap.py` hands back OUR OWN undecoded SAADC samples at a rate we set,
and this turns them into the one number U11 needs — how many tone periods are RF/8 and how many
are RF/10 (C387, C388).

⛔ THE SAMPLE RATE IS ONE PER CARRIER CYCLE, and that is what makes the answer readable without
calibration: the SAADC is PPI-triggered from the carrier PWM's PWMPERIODEND, so an RF/8 tone is
EIGHT samples and an RF/10 tone is TEN. No microseconds, no unit conversion, no scale to pin.

⚠ THE THRESHOLD IS A MOVING MEAN, NOT A FIXED LEVEL. The front end is AC-coupled with a ~27
sample time constant (C204), so a fixed threshold drifts out of the signal within one frame.
⚠ AND SHORT RUNS ARE NOISE, NOT TONES. A real tone is >= 6 samples; anything shorter is the
comparator chattering around the mean and is counted separately rather than silently dropped —
a histogram that hides its own noise floor is how a null gets read as a measurement (F05).
"""
import argparse
import struct
from collections import Counter

WINDOW = 64          # moving-mean window, in samples
MIN_TONE = 6         # below this it is chatter, not a tone


def periods(samples):
    """Rising-edge-to-rising-edge distances against a moving mean."""
    out, last, prev = [], None, None
    if len(samples) < WINDOW:
        return out
    mean = sum(samples[:WINDOW]) / float(WINDOW)
    for i, v in enumerate(samples):
        mean += (v - samples[max(0, i - WINDOW)]) / float(WINDOW)
        sign = 1 if v > mean else -1
        if prev is not None and sign == 1 and prev == -1:
            if last is not None:
                out.append(i - last)
            last = i
        prev = sign
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--expect", default="8,10",
                    help="the two tone lengths in samples, e.g. 8,10 for RF/8/RF/10")
    a = ap.parse_args()
    raw = open(a.path, "rb").read()
    s = list(struct.unpack("<%dh" % (len(raw) // 2), raw))
    p = periods(s)
    lo, hi = (int(x) for x in a.expect.split(","))
    c = Counter(p)
    noise = sum(n for v, n in c.items() if v < MIN_TONE)
    tones = sum(n for v, n in c.items() if v >= MIN_TONE)
    # A tone is "near" a length if it is the closest of the two and within one sample.
    n_lo = sum(n for v, n in c.items() if v >= MIN_TONE and abs(v - lo) <= 1)
    n_hi = sum(n for v, n in c.items() if v >= MIN_TONE and abs(v - hi) <= 1)
    print("  %s: %d samples" % (a.path, len(s)))
    print("  %d tone periods (>= %d samples), %d short runs below that = noise floor"
          % (tones, MIN_TONE, noise))
    print("  histogram: " + "  ".join("%d:%d" % (v, n)
                                      for v, n in sorted(c.items()) if v >= MIN_TONE)[:300])
    print("  near RF/%d: %d     near RF/%d: %d     ratio %s"
          % (lo, n_lo, hi, n_hi, ("%.2f" % (n_lo / float(n_hi))) if n_hi else "inf (NO LONG TONES)"))
    if tones and not n_hi:
        print("  ⛔ ZERO long tones — this is U11's signature (C387).")


if __name__ == "__main__":
    main()
