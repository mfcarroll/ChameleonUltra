#!/usr/bin/env python3
"""⭐ WHAT QUANTUM IS THE EMISSION ACTUALLY BUILT ON? — a re-analysis of captures already taken.

Every model this investigation has tried asked WHICH of the intended runs survived, or whether the
duty tracks the frame. This asks the prior question about the CLOCK: are the emitted durations
integer multiples of the bit period the emitter's own source specifies?

⭐ PREDICTIONS, FROM EACH EMITTER'S MODULATOR, FIXED BEFORE THE FIT:
    pac      one entry per bit, counter_top 32 -> every run is a multiple of 256us,
             so every PERIOD is a multiple of 256us.
    fdxb     counter_top 32 with an optional mid-bit transition -> half-bit 128us.
    gproxii  counter_top 64, same idiom                        -> half-bit 256us.
⇒ pac quantising at 256us means the bit CLOCK is right and only the VALUES are wrong; any other
  quantum names the fault directly.

⛔ PERIODS ONLY, never pulses. The Flipper's comparator adds a per-capture bias to every pulse and
subtracts it from every gap, so a period is unbiased and a run is not (C435, M54).
⛔⛔ THE FIT IS A MINIMISATION, SO IT CARRIES ITS OWN NULL (M55). Uniform random durations over the
same range are fitted by the same search: if they score as well as the data, the statistic cannot
reject and no quantum here means anything.
⚠ Any divisor of the true quantum fits at least as well (128 divides 256), so the answer is the
LARGEST quantum whose residual stays under the threshold, and the whole curve is printed rather
than one winner.
"""
import argparse, os, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flipraw


def residual(durs, q):
    """Mean |distance to the nearest multiple of q|, as a fraction of q. 0 = perfect, 0.5 = none."""
    tot = 0.0
    for d in durs:
        r = d % q
        tot += min(r, q - r)
    return tot / len(durs) / q


def scan(durs, lo, hi):
    return [(residual(durs, q), q) for q in range(lo, hi + 1)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tags", nargs="*",
                    default=["fdxb", "gprox", "pac_1337BEEF", "pac_55555555"])
    ap.add_argument("--lo", type=int, default=40)
    ap.add_argument("--hi", type=int, default=1200)
    ap.add_argument("--thresh", type=float, default=0.12)
    a = ap.parse_args()

    f = flipraw.Flip()
    try:
        caps = {t: f.fetch("/ext/lfrfid/airduty_%s.ask.raw" % t) for t in a.tags}
    finally:
        f.close()

    for tag, data in caps.items():
        pulses, durs, meta = flipraw.parse(data)
        if not durs:
            print("  %-14s ⛔ empty capture" % tag)
            continue
        sc = scan(durs, a.lo, a.hi)
        best = min(sc)
        good = [q for r, q in sc if r <= a.thresh]
        # the null: uniform random durations over the same range, same search
        rng = random.Random(1234)
        null = [rng.uniform(min(durs), max(durs)) for _ in durs]
        nbest = min(scan(null, a.lo, a.hi))
        print("  %-14s n=%5d  best q=%4dus (residual %.3f)   null best %.3f at q=%dus"
              % (tag, len(durs), best[1], best[0], nbest[0], nbest[1]))
        if good:
            print("     q under %.2f: largest %dus, count %d, all: %s"
                  % (a.thresh, max(good), len(good),
                     ",".join(str(q) for q in good[-12:])))
        else:
            print("     ⛔ NO quantum under %.2f in [%d, %d]us" % (a.thresh, a.lo, a.hi))
    return 0


if __name__ == "__main__":
    sys.exit(main())
