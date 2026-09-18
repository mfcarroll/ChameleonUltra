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


def fit_se(xs, ys):
    """Slope, intercept, and the slope's STANDARD ERROR from the fit's own residuals.

    ⭐⭐ THIS REPLACED A BOOTSTRAP THAT GAVE A NONSENSE ANSWER, AND THE REASON IS WORTH KEEPING.
    The first version resampled the replicate timings inside each (dec, N) cell and refit. It
    returned a 95% interval of **−1.545 .. 1.854** for dec 2 — a negative stretch, which is
    impossible — while the point estimate was a clean 1.739 off medians that are monotone to
    within 3 ms.

    ⛔ The bootstrap was not measuring the stretch's precision, it was measuring the OUTLIER RATE.
    A per-cell median over 10 timings is robust to the occasional multi-second pm3 client hiccup;
    a bootstrap resample of those same 10 is not, because a draw can pick the outliers several
    times over and move that cell's median by hundreds of ms. One distorted cell out of five is
    enough to flip a fitted slope through zero, and a ratio with a near-zero denominator explodes.
    ⇒ **resampling a statistic chosen for being robust destroys the robustness that justified it.**

    The residual-based SE is the right estimator here: it runs on the five MEDIAN points, which
    are what the fit actually uses, and their residuals (<= 3.1 ms on a 196 ms span) are what the
    fit's uncertainty actually is.
    """
    n = len(xs)
    a, b = fit(xs, ys)
    mx = sum(xs) / n
    den = sum((x - mx) ** 2 for x in xs)
    resid = [y - (a * x + b) for x, y in zip(xs, ys)]
    s2 = sum(r * r for r in resid) / (n - 2) if n > 2 else 0.0
    return a, b, (s2 / den) ** 0.5 if den else 0.0, max((abs(r) for r in resid), default=0.0)


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
    ap.add_argument("--interval", action="store_true",
                    help="⭐ a 95%% CONFIDENCE INTERVAL on each stretch, from the fit's own "
                         "residuals. C547 called the stretch *known to ~7%%* without ever "
                         "measuring it, and an alignment design's usable resolution follows "
                         "directly from that figure.")
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
    slopes, ses, resids = {}, {}, {}
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
            sl, _, se, worst = fit_se(xs, ys)
            ses[d] = (sl, se)
            resids[d] = worst
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
    if a.interval and 1 in ses and ses[1][0]:
        # t(0.975) by residual df = len(counts) - 2
        TT = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 8: 2.306}
        df = max(1, len(counts) - 2)
        t = TT.get(df, 1.96)
        print("\n   ⭐⭐ 95%% CONFIDENCE INTERVAL ON THE STRETCH (from the fit's residuals, "
              "t=%.3f, %d df)" % (t, df))
        a1, se1 = ses[1]
        print("   dec   stretch   95% CI            +/-      worst residual")
        for d in decs:
            if d == 1 or d not in ses:
                continue
            a2, se2 = ses[d]
            if not a1 or not a2:
                continue
            ratio = a2 / a1
            rel = ((se1 / a1) ** 2 + (se2 / a2) ** 2) ** 0.5
            print("   %3d    %.3f    %.3f .. %.3f    +/- %.2f%%   %.1f ms"
                  % (d, ratio, ratio * (1 - t * rel), ratio * (1 + t * rel), 100 * t * rel,
                     resids.get(d, 0.0)))
            print("      ⇒ a dec-%d cell's ELAPSED position is uncertain by %s"
                  % (d, ", ".join("+/-%.2f cells at %d ms nominal"
                                  % (x * ratio * t * rel / 5.0, x) for x in (50, 100, 150))))
        print("   ⚠ Fit uncertainty only. It does NOT cover a systematic that moves every count\n"
              "     together, and it does NOT transfer to another build (M79/C547: a new binary\n"
              "     is a new condition).")
    print("\n⛔ Ungraded — an instrument measurement. It moves no cell and claims no region.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
