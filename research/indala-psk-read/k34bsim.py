#!/usr/bin/env python3
"""K34b's band, its power and its false-fire, computed BEFORE the flash (M70/M75).

⛔⛔ THIS IS NOT A SCORER AND NOT A BAND. It fires nothing and measures nothing on the air. It
answers one question: **can K34b's two windows tell ELAPSED from SAMPLES at the dec-2 level this
bench has actually been measured at, at the reps we can afford?** `k34sim.py` is the same job for
K34a; this is its sibling and it exists for the same reason — K34a was flashed on a power figure
grounded in the wrong day (M83) and refused on its own arithmetic afterwards (C552/C553).

⭐⭐⭐⭐ WHAT IT FOUND, AND IT IS A DESIGN FAULT IN THE PINNED K34b, NOT A POWER FIGURE.
K34b is pinned as `idteck`, `--dec 2`, R4 **unmoved 140-145** against **moved 84.7-87.7**, scored
by K29's HIT rule (>= 2 cells of the window elevated over the cap's own ladder median + 25 points,
in BOTH seeds). ⛔ **Those two windows are not comparable, and the asymmetry is the stretch's:**

    SAMPLES  the region sits at nominal 140-145.  Width 5.0 ms, position known EXACTLY --
             no stretch enters, because a sample count is what the ladder asks for.
    ELAPSED  the region sits at nominal 140/S .. 145/S.  Width 5/S = 2.7-3.2 ms, and its
             POSITION is uncertain over 76.4-93.0 because S is 1.559-1.833 (C556's two 95%
             intervals, unioned).

⇒ ⛔ **Under ELAPSED the region is ~3 ms wide and we are ignorant of where it sits to +/-8 ms —
nearly THREE TIMES its own width.** Two consequences, and the first is fatal on its own:

1. ⛔⛔ **ON THE PINNED 5 ms GRID THE ELAPSED REGION OCCUPIES 0 OR 1 CELLS — it can fall between
   two ladder points and miss the ladder entirely — so `>= 2 elevated cells` CANNOT FIRE ON
   ELAPSED AT ALL.** The band could only ever return SAMPLES or NO VERDICT. **A band that can
   return only one of its two answers is not an experiment**, and it would have cost a flash and
   two caps to find that out on the bench.
   ⛔ **AND 2.5 ms IS WORSE THAN IT LOOKS, WHICH IS THE SUBTLER HALF:** occupancy is 1-2 cells,
   `>= 2` only **19.1%** of the time — so `k_E = 2` is met, 80% of the time, by **one region cell
   plus one baseline cell that happened to clear**. It simulates at 76.4% power (reps 32) and
   that power is mostly the noise term. ⇒ ⭐⭐ **A WINDOW'S THRESHOLD MUST NOT EXCEED WHAT THE
   REGION CAN SUPPLY THERE** (`occupancy()`), or the marginal cell is noise and the fire is not
   diagnostic. A power figure alone does not reveal this; the occupancy does.
2. ⚠ **The two windows then differ in CELL COUNT** (ELAPSED is wide because we are ignorant;
   SAMPLES is narrow because we are not), so one threshold over both windows hands SAMPLES a
   lower false-fire for free. ⇒ **each window needs its own threshold, set so the two false-fire
   rates match.** That is what `--equalise` does.

✅✅ **AND A DESIGN THAT SURVIVES, WHICH IS THE POINT OF RUNNING THIS BEFORE THE FLASH.**
`--fine-grid 1.0 --k-elapsed 3 --k-samples 3 --reps 32`, background 5 ms over 70-150 (39 cells):
occupancy is **2-4, median 3**, so `k_E = 3` is supplied by the region itself 92% of the time.

    region height   FALSE-FIRE E / S   POWER E-truth / S-truth   no-verdict
    0.725 (measured)   2.4% / 0.0%        95.6% / 99.5%           4.4% / 0.5%
    0.65               2.4% / 0.0%        91.5% / 99.5%           8.5% / 0.5%
    0.60               2.4% / 0.0%        83.9% / 98.8%          16.1% / 1.2%
    0.55  ⭐ the bar   2.4% / 0.0%        70.5% / 94.6%          29.5% / 5.4%
    0.50               2.4% / 0.0%    ⛔  53.0% / 77.9%          47.0% / 21.9%
    0.45               2.4% / 0.0%    ⛔  33.8% / 45.3%          66.2% / 54.5%

⛔ **THESE FIGURES REPLACE THE FIRST SET PUBLISHED (C557), WHICH WERE COMPUTED ON A MIS-ANCHORED
LADDER — see `ladder()`. The direction matters: the cliff is STEEPER than the first set said, and
the gate's bar is the region height 0.55, not 0.50** (C558).

⭐ 35 cells x 32 reps x 2 seeds x 1 arm = **2,240 reads, under half of K34a's 4,864** — the finer
grid is paid for by dropping `keri` and the second condition, both of which K34b does not need.
⛔ **The residual false-fire asymmetry (5.5% vs 0.0%) CANNOT be equalised away**: raising `k_E` to
4 exceeds the region's median occupancy and re-creates fault 1. It is structural — ELAPSED's window
is 3.3x wider **because we are ignorant of S**, not because the physics is broader — and the band
must state it rather than hide it.

⛔⛔ **AND THE POWER CLIFF IS THE GATE (M85's lesson, applied before the flash rather than after):
everything turns on the region's HEIGHT at dec 2 on the new build, which falls off a cliff between
0.50 (71%) and 0.40 (26%).** That quantity is measurable from a cheap COARSE pair on the flashed
build — 5 ms over 70-150, reps 16, 336 reads — and `--from` reads it directly. ⇒ the sequence is
**flash · verify from the air (C461) · re-fit S with `dectime.py --interval` (the window is a
FUNCTION of it) · coarse gate pair · `k34bsim.py --from` · only then the 1 ms pair.** ⛔ Never the
same caps for the gate and the verdict. ⭐ **The gate's bar: the region's height >= 0.55** (the
banked burst-500 value is 0.725, so it is the BUILD that is in question, not the bench).

⭐ NEITHER POINT REOPENS THE DECIMATION LINE (closed twice, C545-C548 and C556). It is the same
quantity — the stretch's imprecision — biting K34b through the WINDOW rather than through an
alignment, and QUEUE item 4's *K34b keeps a threshold band* was written as though a band escaped
C556's arithmetic. It does not escape it; it inherits it somewhere else.

⭐⭐ THE GROUNDING IS MEASURED, NOT INVENTED -- all of it from the six banked dec-2 caps
(`k30_dec2_s{307,311}`, `k31_dec2_s{313,317}`, `k32_dec2_s{331,337}`), which are the only dec-2
ladders this bench has ever run:

    baseline cell rate    mean 0.19, true SD 0.115 (observed 0.154 with the binomial part removed)
    a region's height     0.725 (`idteck` nominal 110, pooled 58/80 over all six caps -- the one
                          cell that replicated in EVERY pair, so it is a feature and not a pick)
    per-cap offset SD     0.071 (same-condition pairs, excess over binomial -- `cap_sigma`)

⚠⚠ WHICH WAY EACH DEFECT PUSHES (M78), BECAUSE THEY DO NOT PUSH THE SAME WAY:
  - ⚠ The region's height is transferred from nominal **110** to R4's cells. It is a different
    region. If R4 is weaker at dec 2 than 110 is, every power figure here is optimistic --
    `--region-rate` is exposed for exactly that reason and 0.50 is reported beside 0.725.
  - ⭐ The baseline SD is binomial-**deconvolved**, so unlike `k34sim.py`'s empirical pool it does
    NOT count sampling noise as real structure. That removes the upper-bound caveat in the
    false-fire direction and replaces it with an ordinary modelling assumption (Beta).
  - ⛔ Everything is measured at **burst 500**, and K34b runs at burst **1000**. M80 is explicit
    that the level is exactly what a longer burst may change. ⇒ **every figure here is a
    statement about the bench at burst 500** and must be re-grounded on the day from the new
    build's own caps. What DOES transfer is the geometry: the widths and the grid arithmetic are
    properties of the stretch and the ladder, not of the level.
"""
import argparse
import json
import math
import os
import random
import statistics as st

HERE = os.path.dirname(os.path.abspath(__file__))
DEC2_CAPS = ["k30_dec2_s307.json", "k30_dec2_s311.json", "k31_dec2_s313.json",
             "k31_dec2_s317.json", "k32_dec2_s331.json", "k32_dec2_s337.json"]
HIGH = 0.25                 # K29's: elevated is the cap's own ladder median + 25 points
R4 = (140.0, 145.0)         # the dec-1 region K34b places, per the pinned design
S_LO, S_HI = 1.559, 1.833   # C556's two 95% intervals, unioned -- the stretch's honest range
REGION_CELL = 110.0         # the dec-2 cell that replicated in every banked pair


def grounding(arm, caps=None):
    """Baseline mean, baseline true SD, and the per-cap offset SD, from the banked dec-2 caps."""
    caps = caps or [os.path.join(HERE, "caps", f) for f in DEC2_CAPS]
    base, per_cap = [], []
    for p in caps:
        d = json.load(open(p))
        if arm not in d:
            raise SystemExit("%s: no arm %r" % (os.path.basename(p), arm))
        r = {float(k): (sum(1 for _, e in v["scores"] if e), len(v["scores"]))
             for k, v in d[arm].items() if k != "none" and not k.startswith("_")}
        med = st.median(sorted(k / n for k, n in r.values()))
        per_cap.append(r)
        base += [(k, n) for k, n in r.values() if k / n < med + HIGH]
    ps = [k / n for k, n in base]
    mean = sum(ps) / len(ps)
    excess = max(0.0, st.pvariance(ps) - sum(p * (1 - p) / n
                                             for p, (_, n) in zip(ps, base)) / len(base))
    # the region's measured height: one cell, pooled over every cap that carries it
    k = n = 0
    for r in per_cap:
        if REGION_CELL in r:
            k += r[REGION_CELL][0]
            n += r[REGION_CELL][1]
    return {"base_mean": mean, "base_sd": excess ** 0.5, "region_rate": k / n,
            "region_obs": (k, n), "cap_sigma": _cap_sigma(per_cap), "base_n": len(ps)}


def _cap_sigma(per_cap):
    """Per-cap offset SD: the excess over binomial between two caps of one condition."""
    acc = []
    for i in (0, 2, 4):
        r1, r2 = per_cap[i], per_cap[i + 1]
        cells = sorted(set(r1) & set(r2))
        obs = exp = 0.0
        for c in cells:
            k1, n1 = r1[c]
            k2, n2 = r2[c]
            obs += (k1 / n1 - k2 / n2) ** 2
            pp = (k1 + k2) / (n1 + n2)
            exp += pp * (1 - pp) * (1.0 / n1 + 1.0 / n2)
        acc.append(max(0.0, (obs - exp) / (2.0 * len(cells))))
    return (sum(acc) / len(acc)) ** 0.5


def occupancy(grid, lo, hi, draws=2001):
    """Cells of a `grid`-spaced ladder the region can occupy, over the stretch's whole range.

    ⛔⛔ THIS IS THE NUMBER THAT KILLS THE PINNED DESIGN AND IT IS ARITHMETIC, NOT SIMULATION.
    A region 5/S = 2.7-3.2 ms wide, on a 5 ms grid, occupies **0 or 1** cells -- it can fall
    between two ladder points and miss the ladder entirely. ⇒ `>= 2 elevated cells` is not
    merely unlikely on ELAPSED, it is **impossible**, and a threshold above what the region can
    supply is met by BASELINE cells: the fire is then noise-assisted and says nothing.
    """
    out = []
    for i in range(draws):
        s = S_LO + (S_HI - S_LO) * i / (draws - 1)
        rlo, rhi = R4[0] / s, R4[1] / s
        out.append(max(0, math.floor(rhi / grid) - math.ceil(rlo / grid) + 1))
    return out


def ladder(grid, span, fine, fine_grid):
    """The dec-2 nominal ladder: a coarse background plus a fine grid inside the windows."""
    lo, hi = span
    cells = {round(lo + i * grid, 3) for i in range(int((hi - lo) / grid) + 1)}
    # ⛔⛔ THE FINE GRID IS ANCHORED ON ROUND MULTIPLES OF `fine_grid`, NOT ON THE WINDOW'S EDGE.
    # Anchoring it at `wlo` produced cells like 76.4, 81.4, 86.4 — nominal values no ladder
    # would actually be asked for — and, worse, it made `occupancy()` (which assumes a
    # 0-anchored grid) describe a DIFFERENT ladder from the one simulated. At 5 ms the two
    # disagreed on whether ELAPSED could occupy 2 cells at all. Caught by cross-checking the
    # two against each other rather than by either one alone.
    for wlo, whi in fine:
        i0 = int(math.ceil(round(wlo / fine_grid, 6)))
        i1 = int(math.floor(round(whi / fine_grid, 6)))
        cells |= {round(i * fine_grid, 3) for i in range(i0, i1 + 1)}
    return sorted(c for c in cells if lo - 1e-9 <= c <= hi + 1e-9)


def windows(grid_e):
    """The two windows, in dec-2 nominal ms. ⛔ Their widths are NOT a design choice."""
    return {"ELAPSED": (R4[0] / S_HI, R4[1] / S_LO), "SAMPLES": R4}


def beta_ab(mean, sd):
    v = max(1e-9, sd * sd)
    k = mean * (1 - mean) / v - 1
    if k <= 0:
        return None
    return mean * k, (1 - mean) * k


def draw_cap(cells, truth, g, rng, reps, cap_sigma, region_rate, s_true):
    """One cap's observed per-cell rates under one truth."""
    ab = beta_ab(g["base_mean"], g["base_sd"])
    if truth == "ELAPSED":
        rlo, rhi = R4[0] / s_true, R4[1] / s_true
    elif truth == "SAMPLES":
        rlo, rhi = R4
    else:
        rlo, rhi = None, None
    out = {}
    for c in cells:
        if rlo is not None and rlo - 1e-9 <= c <= rhi + 1e-9:
            p = region_rate
        elif ab:
            p = rng.betavariate(*ab)
        else:
            p = g["base_mean"]
        p = min(1.0, max(0.0, p + rng.gauss(0.0, cap_sigma)))
        out[c] = sum(rng.random() < p for _ in range(reps)) / reps
    return out


def fires(two_caps, win, k):
    """K29's HIT rule, per window: >= k cells of the window elevated in BOTH seeds."""
    lo, hi = win
    for cp in two_caps:
        med = st.median(sorted(cp.values()))
        span = [c for c in cp if lo - 1e-9 <= c <= hi + 1e-9]
        if sum(1 for c in span if cp[c] >= med + HIGH) < k:
            return False
    return True


def run(g, cells, wins, reps, draws, ks, region_rate, rng, truth):
    """Verdict rates under one truth, integrating over the stretch's own uncertainty."""
    tally = {"ELAPSED": 0, "SAMPLES": 0, "BOTH": 0, "NEITHER": 0}
    for _ in range(draws):
        s = rng.uniform(S_LO, S_HI)
        two = [draw_cap(cells, truth, g, rng, reps, g["cap_sigma"], region_rate, s)
               for _ in (0, 1)]
        e = fires(two, wins["ELAPSED"], ks["ELAPSED"])
        sm = fires(two, wins["SAMPLES"], ks["SAMPLES"])
        tally["BOTH" if e and sm else "ELAPSED" if e else "SAMPLES" if sm else "NEITHER"] += 1
    return {k: v / draws for k, v in tally.items()}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("draws", nargs="?", type=int, default=2000)
    ap.add_argument("--arm", default="idteck", help="the pinned K34b arm")
    ap.add_argument("--from", dest="frm", nargs="+",
                    help="⭐ ground it in caps measured ON THE DAY (M80/M83), not these")
    ap.add_argument("--reps", type=int, nargs="+", default=[8, 16, 32])
    ap.add_argument("--fine-grid", type=float, nargs="+", default=[5.0, 2.5, 1.0],
                    help="grid INSIDE the windows; the >=2-cell rule needs <= 5/S = 2.7 ms")
    ap.add_argument("--grid", type=float, default=5.0, help="background grid, for the median")
    ap.add_argument("--span", type=float, nargs=2, default=[70.0, 150.0])
    ap.add_argument("--region-rate", type=float, nargs="+", default=[0.725, 0.50],
                    help="⚠ transferred from nominal 110; 0.50 is the pessimistic reading")
    ap.add_argument("--cells", type=int, default=2, help="K29's >=2 elevated cells")
    ap.add_argument("--k-elapsed", type=int, help="override the ELAPSED window's threshold")
    ap.add_argument("--k-samples", type=int, help="override the SAMPLES window's threshold")
    ap.add_argument("--equalise", action="store_true",
                    help="raise the wide window's threshold until its false-fire matches")
    ap.add_argument("--seed", type=int, default=20260917)
    a = ap.parse_args()

    caps = a.frm or None
    if caps:
        caps = [c if os.path.sep in c else os.path.join(HERE, "caps", c) for c in caps]
    g = grounding(a.arm, caps)
    wins = windows(None)
    print("⭐ GROUNDING — arm %s, %s" % (
        a.arm, "caps given with --from" if caps else "the six banked dec-2 caps, BURST 500"))
    print("   baseline mean %.3f  true SD %.3f (%d cells)   cap_sigma %.3f" % (
        g["base_mean"], g["base_sd"], g["base_n"], g["cap_sigma"]))
    print("   a region's height %.3f  (nominal %g, %d/%d pooled)" % (
        g["region_rate"], REGION_CELL, *g["region_obs"]))
    print("\n⛔ THE TWO WINDOWS ARE NOT COMPARABLE, AND THAT IS THE STRETCH'S DOING")
    print("   stretch %.3f-%.3f (C556, two 95%% intervals unioned)" % (S_LO, S_HI))
    for name, (lo, hi) in wins.items():
        print("   %-8s nominal %6.1f-%6.1f   window %4.1f ms   region itself %.1f ms" % (
            name, lo, hi, hi - lo,
            5.0 / ((S_LO + S_HI) / 2) if name == "ELAPSED" else 5.0))
    print("   ⇒ ELAPSED's region is %.1f ms wide inside a %.1f ms window: we are ignorant of its\n"
          "     position by %.1fx its own width, and that is what the grid must beat." % (
              5.0 / ((S_LO + S_HI) / 2), wins["ELAPSED"][1] - wins["ELAPSED"][0],
              (wins["ELAPSED"][1] - wins["ELAPSED"][0]) / (5.0 / ((S_LO + S_HI) / 2))))

    for fine in a.fine_grid:
        cells = ladder(a.grid, a.span, list(wins.values()), fine)
        ne = len([c for c in cells if wins["ELAPSED"][0] - 1e-9 <= c <= wins["ELAPSED"][1] + 1e-9])
        ns = len([c for c in cells if wins["SAMPLES"][0] - 1e-9 <= c <= wins["SAMPLES"][1] + 1e-9])
        occ = occupancy(fine, *wins["ELAPSED"])
        occ_s = math.floor(5.0 / fine) + 1
        occ_med = sorted(occ)[len(occ) // 2]
        print("\n" + "=" * 78)
        print("FINE GRID %.1f ms — ladder %d cells (%g-%g, background %g ms)" % (
            fine, len(cells), a.span[0], a.span[1], a.grid))
        print("  window cells: ELAPSED %d, SAMPLES %d   ⇒ %s" % (
            ne, ns, "the wide window has %.1fx the cells" % (ne / ns)))
        print("  cells the ELAPSED region can OCCUPY here: min %d median %d max %d  "
              "P(>=2) %.1f%%  P(>=3) %.1f%%" % (
                  min(occ), occ_med, max(occ),
                  100 * sum(1 for o in occ if o >= 2) / len(occ),
                  100 * sum(1 for o in occ if o >= 3) / len(occ)))
        print("  cells the SAMPLES region occupies here: %d (exactly — no stretch enters)" % occ_s)
        ke = a.k_elapsed or a.cells
        kss = a.k_samples or a.cells
        if ke > max(occ):
            print("  ⛔ k_E=%d EXCEEDS what the region can EVER supply (%d) — this grid is DEAD "
                  "for ELAPSED: the band could only ever return SAMPLES or NO VERDICT." % (
                      ke, max(occ)))
            continue
        if ke > occ_med:
            print("  ⛔ k_E=%d exceeds the region's MEDIAN occupancy (%d) — a fire here needs "
                  "%d region cell(s) PLUS a baseline cell, so it is NOISE-ASSISTED and its "
                  "power figure is mostly the noise term. Read the false-fire beside it." % (
                      ke, occ_med, occ_med))
        for reps in a.reps:
            rng = random.Random(a.seed)
            ks = {"ELAPSED": ke, "SAMPLES": kss}
            flat = run(g, cells, wins, reps, a.draws, ks, g["region_rate"], rng, "FLAT")
            if a.equalise:
                while flat["ELAPSED"] + flat["BOTH"] > flat["SAMPLES"] + flat["BOTH"] + 0.01 \
                        and ks["ELAPSED"] < max(occ):
                    ks["ELAPSED"] += 1
                    rng = random.Random(a.seed)
                    flat = run(g, cells, wins, reps, a.draws, ks, g["region_rate"], rng, "FLAT")
            for rr in a.region_rate:
                rng = random.Random(a.seed + 1)
                pe = run(g, cells, wins, reps, a.draws, ks, rr, rng, "ELAPSED")
                rng = random.Random(a.seed + 2)
                psm = run(g, cells, wins, reps, a.draws, ks, rr, rng, "SAMPLES")
                print("  reps %-3d k=(E%d,S%d) region %.3f | "
                      "FALSE-FIRE E %.1f%% S %.1f%% | POWER  E-truth %.1f%%  S-truth %.1f%% | "
                      "no-verdict %.1f%%/%.1f%%" % (
                          reps, ks["ELAPSED"], ks["SAMPLES"], rr,
                          100 * flat["ELAPSED"], 100 * flat["SAMPLES"],
                          100 * pe["ELAPSED"], 100 * psm["SAMPLES"],
                          100 * (pe["NEITHER"] + pe["BOTH"]),
                          100 * (psm["NEITHER"] + psm["BOTH"])))
    print("\n⚠ Every figure is the bench at BURST 500 (M80) and a statement about this grounding,\n"
          "  never about the burst. ⛔ Re-ground with --from on the day before any flash.")


if __name__ == "__main__":
    main()
