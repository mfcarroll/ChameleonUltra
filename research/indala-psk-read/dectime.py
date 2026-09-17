"""⛔ INSTRUMENT MEASUREMENT, NOT A BAND. What is `lf config --dec N`'s stretch, per N, slope-fitted?

M79 in one sentence: **measure a knob's transfer function over a RANGE and slope-fit it, then
design the band so it does not depend on the factor.** C540 did that for dec 1 and dec 2 and got
**1.653x** — and it mattered, because a band written against the obvious 2.0 would have put every
predicted cell 25-45 ms out and returned a confident wrong answer. ⛔ C540 also *timed* dec 4 but
never slope-fitted it, so M79 has stood unsatisfied for that setting ever since, and C544 named it
as the one option whose arithmetic is undone.

⭐ **THIS IS THE TOOL THAT SETTLES IT, AND IT IS WHY THE QUESTION IS CHEAP**: nothing is armed,
no credential is emitted, no demodulator runs and the bench is not touched. It times bare
`lf read -s N` against the host clock at several N and several decimations, shuffles the whole
(dec, N, rep) plan (**M60 — order is a variable on this bench**), takes a per-cell MEDIAN so one
stalled USB transfer cannot drag a slope, and least-squares fits `elapsed ~ a*N + b` per
decimation. **The stretch is the SLOPE RATIO to dec 1**, which drops the constant overhead `b`
out — the whole point of fitting rather than dividing two timings.

⚠ **WHY THE SLOPE AND NOT THE ELAPSED**: a single `-s N` timing carries the client's fixed
per-invocation cost, and that cost does not scale with the knob. C540's own decomposition is the
reason the answer is 1.653 and not 2: **acquisition doubles at dec 2 (2N raw samples for N stored)
while the USB readback does not (N is unchanged)**, so a 2x term rides on top of a 1x term. ⇒ the
model this fits is `slope(dec) = a*dec + x`, and printing `a` and `x` separately says how much of
each read is acquisition and how much is transfer.

⛔ `lf config --reset` runs in a `finally`, always — a Proxmark left decimating would silently
corrupt every later capture on this bench, and nobody is here to notice.
"""
import argparse
import os
import statistics
import subprocess
import sys
import time
import random

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import seqdump

PM3 = seqdump.PM3


def run(cmds, timeout=300):
    r = subprocess.run([PM3, "-c", "; ".join(cmds)], capture_output=True, text=True,
                       timeout=timeout, start_new_session=True)
    return r.stdout + r.stderr


def fit(xs, ys):
    """Least-squares slope and intercept of y ~ a*x + b."""
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    den = sum((x - mx) ** 2 for x in xs)
    a = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den
    return a, my - a * mx


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--decs", default="1,2,3,4")
    ap.add_argument("--counts", default="2500,5000,10000,15000,20000")
    ap.add_argument("--reps", type=int, default=4)
    ap.add_argument("--seed", type=int, default=353)
    a = ap.parse_args()
    decs = [int(v) for v in a.decs.split(",")]
    counts = [int(v) for v in a.counts.split(",")]

    plan = [(d, n) for d in decs for n in counts for _ in range(a.reps)]
    random.Random(a.seed).shuffle(plan)          # ⛔ M60 — order is a variable
    print("dectime — %d timed reads: dec %s x N %s x %d reps, SHUFFLED (seed %d)"
          % (len(plan), decs, counts, a.reps, a.seed))
    print("   ⛔ nothing armed, no demodulator, no credential — host timing only.\n")

    got = {}
    try:
        for i, (d, n) in enumerate(plan):
            t0 = time.time()
            run(["lf config --reset", "lf config --dec %d" % d, "lf read -s %d" % n])
            got.setdefault((d, n), []).append((time.time() - t0) * 1000.0)
            if (i + 1) % 20 == 0:
                print("   ... %d/%d" % (i + 1, len(plan)))
    finally:
        # ⛔ ALWAYS, NOT ONLY ON THE HAPPY PATH — a decimating Proxmark corrupts every later cap.
        print("\nlf config --reset: %s"
              % ("ok" if "decimation" in run(["lf config --reset", "lf config"]).lower()
                 else "⛔ CHECK IT"))

    print("\n   dec       N    median ms   n")
    slopes = {}
    for d in decs:
        xs, ys = [], []
        for n in counts:
            v = got.get((d, n)) or []
            if not v:
                continue
            m = statistics.median(v)
            xs.append(float(n)); ys.append(m)
            print("   %3d  %6d      %7.1f   %d" % (d, n, m, len(v)))
        if len(xs) >= 2:
            slopes[d] = fit(xs, ys)
        print("")

    base = slopes.get(1, (None, None))[0]
    print("   ⭐ SLOPE FIT — elapsed ~ a*N + b, and the STRETCH is the slope ratio to dec 1")
    print("   dec   slope ms/sample   intercept ms   stretch vs dec 1")
    for d in decs:
        if d not in slopes:
            continue
        s, b = slopes[d]
        print("   %3d        %.5f         %7.1f        %s"
              % (d, s, b, "1.000 (reference)" if d == 1 else
                 ("%.3f" % (s / base) if base else "?")))
    # ⭐ C540's decomposition: slope(dec) = a*dec + x, acquisition scaling and readback not.
    if len(slopes) >= 2:
        ds = sorted(slopes)
        aa, xx = fit([float(d) for d in ds], [slopes[d][0] for d in ds])
        print("\n   decomposition  slope(dec) = a*dec + x  ⇒  a = %.5f ms/sample (ACQUISITION, "
              "scales)\n                                             x = %.5f ms/sample "
              "(READBACK, does not)" % (aa, xx))
        if aa + xx:
            print("   ⇒ predicted stretch: " + ", ".join(
                "dec %d = %.3f" % (d, (aa * d + xx) / (aa + xx)) for d in (2, 3, 4)))
    print("\n⛔ Ungraded — an instrument measurement. It moves no cell and claims no region.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
