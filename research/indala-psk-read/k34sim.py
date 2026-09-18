#!/usr/bin/env python3
"""K34a's power and false-fire, simulated BEFORE the capture (M70/M75).

⛔⛔ THIS IS NOT A SCORER AND NOT A BAND. It fires nothing and measures nothing. It answers
one question: **would K34a's D1/D2/D3 bands tell the truth apart from the alternatives, at the
reps we can afford?** M70 is the rule that made this mandatory — Y2 was pre-registered,
replicated across two seeds, and had a 41% false-fire rate nobody had computed.

⭐ THE PROFILES ARE NOT INVENTED. Every cell's true rate is taken from the two banked C538 caps
(`k29_three_s283`, `k29_three_s293`) — the same 38-cell 10-195 ms ladder, the same two arms, the
same dec 1, the same burst 500, 16 observations per cell. ⇒ the null this simulates is the bench
as it actually behaves, not a smooth curve chosen to be beaten.

THE FOUR TRUTHS, and the third and fourth are the ones that matter:

  A  SAME      burst 1000 has the identical profile. The burst is a REACH knob and nothing else.
               ⇒ this is the truth D1 must DETECT. Its rate here is D1's POWER.
  B  SCALED    the structure is a fixed FRACTION of the burst, so a feature at 55-65 at burst
               500 sits at 110-130 at burst 1000: p1000(x) = p500(x/2).
  C  SHIFTED   the structure moves by a constant +20 ms.
  D  FLAT      the longer burst abolishes the structure inside the window.
               ⇒ B, C and D are the truths D1 must NOT fire on. Their rates are its FALSE-FIRE.

⚠ B, C and D are all *the regions moved*, which is the finding K34a would report as bigger than
K34b (the K34 section says so). So D2 is scored as well, and its power is reported per truth —
⛔ but D1 is the licensing band and D1's false-fire is the number that decides the reps.

⭐⭐⭐⭐ `--from`, ADDED 2026-09-17 AFTER K34a's CONTROL FAILED (M83). The run above put
control-failure at **0.0%** and the bench delivered it on the first attempt. The simulation was
not wrong; it was grounded in the wrong day. C538's caps pool **42.1%/43.1%** (`keri`/`idteck`,
38-cell 10-195 ladder, dec 1, n=16), and K34a's own burst-500 control pooled **33.4%/27.6%** and
**32.4%/33.1%** at n=32 — and C497 already measured that this level wanders.

⇒ **A POWER FIGURE IS A PROPERTY OF THE BAND AT THE LEVEL IT WAS GROUNDED IN, AND THE LEVEL IS
NOT A CONSTANT OF THIS BENCH.** So the grounding is now an argument (`--from`), and any re-run
must re-derive its thresholds from the level measured on the day (M80/M83) rather than inherit
these.

⚠⚠ **WHICH WAY THE REMAINING DEFECT PUSHES (M78), BECAUSE IT IS NOT SYMMETRIC.** Whatever caps
ground it, the observed per-cell rates are used AS IF they were the true ones, so the sampling
noise in them is counted as real profile structure. That inflates the spread, which inflates the
chance a region clears `median + 25 points`. ⇒ **every control-hit and D1-power figure this tool
prints is an UPPER BOUND**, and the tighter the grounding caps' own n, the tighter the bound.
C538's pair carries 16 obs/cell; K34a's carries 32, so a K34a-grounded figure is the better
bound of the two. ⛔ A floor this tool says is comfortably met may still fail on the bench; a
floor it says is marginal will not be met.
"""
import argparse
import json
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CAPS = [os.path.join(HERE, "caps", f)
        for f in ("k29_three_s283.json", "k29_three_s293.json")]
ARMS = ("keri", "idteck")
LADDER = [x for x in range(10, 196, 5)]
REGIONS = [("R1", 15, 20), ("R2", 55, 65), ("R3", 100, 105),
           ("R4", 140, 145), ("R5", 180, 190)]
HIGH = 0.25          # K29's: elevated is the cap's own ladder median + 25 points
CELLS = 2            # cells of a region elevated, in BOTH seeds, to HIT it


def profiles(caps=None):
    """True per-cell rate per arm, pooled over the grounding caps.

    ⚠ M83: the DEFAULT is C538's pair, which is the grounding K34a was pre-registered on and
    is kept so that run's numbers stay reproducible. ⛔ It is NOT the right grounding for a
    re-run — pass `--from` the caps that measured the level on the day.
    """
    caps = caps or CAPS
    acc = {a: {c: [0, 0] for c in LADDER} for a in ARMS}
    for p in caps:
        d = json.load(open(p))
        for a in ARMS:
            if a not in d:
                raise SystemExit("%s: no arm %r — a grounding cap must carry both arms "
                                 "K34a runs" % (os.path.basename(p), a))
            for c in LADDER:
                if str(c) not in d[a]:
                    raise SystemExit("%s: arm %r has no cell %d — a grounding cap must carry "
                                     "the whole 38-cell 10-195 ladder" % (
                                         os.path.basename(p), a, c))
                sc = d[a][str(c)]["scores"]
                acc[a][c][0] += sum(1 for _, e in sc if e)
                acc[a][c][1] += len(sc)
    return {a: {c: acc[a][c][0] / acc[a][c][1] for c in LADDER} for a in ARMS}


def obs_per_cell(caps):
    """Observations behind each cell of the grounding — the tightness of the upper bound."""
    d0 = json.load(open(caps[0]))
    return sum(len(json.load(open(p))[ARMS[0]][str(LADDER[0])]["scores"]) for p in caps)


def fit_cap_sigma(caps):
    """Per-cap SD implied by two same-condition caps differing by MORE than binomial.

    ⚠ The estimate is noisy: with 38 cells the variance ratio's own SD is ~sqrt(2/38) = 0.23,
    so a ratio under ~1.5 is not distinguishable from none. It is clamped at 0 and reported.
    """
    ds = [json.load(open(p)) for p in caps]
    acc = []
    for a in ARMS:
        if not all(a in d for d in ds):
            continue
        obs = exp = 0.0
        for c in LADDER:
            s1, s2 = (d[a][str(c)]["scores"] for d in ds)
            k1, n1 = sum(1 for _, e in s1 if e), len(s1)
            k2, n2 = sum(1 for _, e in s2 if e), len(s2)
            obs += (k1 / n1 - k2 / n2) ** 2
            pp = (k1 + k2) / (n1 + n2)
            exp += pp * (1 - pp) * (1.0 / n1 + 1.0 / n2)
        acc.append(max(0.0, (obs - exp) / (2.0 * len(LADDER))))
    return (sum(acc) / len(acc)) ** 0.5 if acc else 0.0


def control_only(prof, reps, draws, rng, cap_sigma=0.0):
    """P(|H500| >= k) for each k — the control's OWN distribution, which K34a never computed.

    ⭐⭐ THIS IS THE NUMBER M83 IS ABOUT. K34a's floor of 3 was carried over from a grounding
    where it was never in doubt; nobody asked what it was at the level the bench was actually
    sitting at. It is one line and it is the first thing a re-run must look at.
    """
    tally = {}
    for _ in range(draws):
        h = set()
        for a in ARMS:
            h |= {(a, r) for r in hits([cap(prof[a], reps, rng, cap_sigma) for _ in (0, 1)])}
        tally[len(h)] = tally.get(len(h), 0) + 1
    return tally


def _at(prof, x):
    """The profile's rate at an arbitrary lead time, clamped to the measured span."""
    if x <= LADDER[0]:
        return prof[LADDER[0]]
    if x >= LADDER[-1]:
        return prof[LADDER[-1]]
    lo = max(c for c in LADDER if c <= x)
    hi = min(c for c in LADDER if c >= x)
    if lo == hi:
        return prof[lo]
    f = (x - lo) / (hi - lo)
    return prof[lo] * (1 - f) + prof[hi] * f


def moved(prof, truth):
    if truth == "A":
        return dict(prof)
    if truth == "B":
        return {c: _at(prof, c / 2.0) for c in LADDER}
    if truth == "C":
        return {c: _at(prof, c - 20) for c in LADDER}
    if truth == "D":
        v = sorted(prof.values())
        med = 0.5 * (v[len(v) // 2 - 1] + v[len(v) // 2])
        return {c: med for c in LADDER}
    raise ValueError(truth)


def cap(prof, reps, rng, cap_sigma=0.0):
    """One cap's observed per-cell rates.

    ⭐⭐⭐ `cap_sigma` IS THE SECOND NOISE SOURCE, AND K34a's SIMULATION HAD ONLY THE FIRST
    (M84). Binomial draws model the variation WITHIN a cap and implicitly assert that two caps
    of the same condition sample the SAME true profile. The bench does not do that: `repro.py`
    measures seed-to-seed rank agreement between +0.57 and +0.86, and the residual variance
    between banked same-condition pairs runs 1.1-1.8x binomial. ⇒ a per-cap offset is drawn per
    cell, held across that cap's reps, and the HIT rule's requirement that BOTH seeds show the
    region is exactly what it degrades.
    """
    if cap_sigma <= 0:
        return {c: sum(rng.random() < prof[c] for _ in range(reps)) / reps for c in LADDER}
    out = {}
    for c in LADDER:
        p = min(1.0, max(0.0, prof[c] + rng.gauss(0.0, cap_sigma)))
        out[c] = sum(rng.random() < p for _ in range(reps)) / reps
    return out


def hits(caps_two):
    """The (region) set an arm HITs, under K29's rule: >= 2 elevated cells in BOTH seeds."""
    meds = []
    for cp in caps_two:
        v = sorted(cp.values())
        meds.append(0.5 * (v[len(v) // 2 - 1] + v[len(v) // 2]))
    out = set()
    for lbl, lo, hi in REGIONS:
        span = [c for c in LADDER if lo <= c <= hi]
        if all(sum(1 for c in span if caps_two[i][c] >= meds[i] + HIGH) >= CELLS
               for i in (0, 1)):
            out.add(lbl)
    return out


def verdict(h500, h1000, keep_frac, new_max, floor):
    """D1/D2/D3, exactly as K34a's section pins them."""
    if len(h500) < floor:
        return "CONTROL-FAILED"
    kept = len(h500 & h1000)
    new = len(h1000 - h500)
    if kept >= keep_frac * len(h500) and new <= new_max:
        return "D1-SAME"
    if kept <= 1 and len(h1000) >= 2:
        return "D2-MOVED"
    if len(h1000) == 0:
        # the floor was already cleared above, so |h500| is sufficient by construction;
        # ⛔ do not re-test it against a literal 3 — the floor is an argument now (M83).
        return "D3-GONE"
    return "NO-VERDICT"


def run(reps, draws, keep_frac, new_max, floor, seed=20260917, prof=None, cap_sigma=0.0):
    prof = prof or profiles()
    rng = random.Random(seed)
    out = {}
    for truth in "ABCD":
        tally = {}
        for _ in range(draws):
            h500, h1000 = set(), set()
            for a in ARMS:
                p5 = prof[a]
                p10 = moved(prof[a], truth)
                h500 |= {(a, r) for r in hits(
                    [cap(p5, reps, rng, cap_sigma) for _ in (0, 1)])}
                h1000 |= {(a, r) for r in hits(
                    [cap(p10, reps, rng, cap_sigma) for _ in (0, 1)])}
            v = verdict(h500, h1000, keep_frac, new_max, floor)
            tally[v] = tally.get(v, 0) + 1
        out[truth] = {k: 100.0 * v / draws for k, v in tally.items()}
    return out


def main():
    ap = argparse.ArgumentParser(
        description="K34a's power and false-fire. Not a scorer; fires nothing.")
    ap.add_argument("draws", nargs="?", type=int, default=4000)
    ap.add_argument("--from", dest="ground", nargs="+", metavar="CAP",
                    help="⭐ M83: ground the per-cell rates in THESE caps instead of C538's "
                         "pair. A re-run must ground in the level measured on the day — a "
                         "power figure is a property of the band at its grounding level, and "
                         "this level wanders (C497, C549). Both arms and all 38 cells "
                         "required; two caps is the shape K34a scores.")
    ap.add_argument("--cap-sigma", type=float, default=0.0, metavar="S",
                    help="⭐⭐ M84: per-cap per-cell offset SD, the second noise source K34a's "
                         "simulation did not have. 0 (the default) reproduces the "
                         "pre-registered numbers; the banked same-condition pairs support "
                         "0.07-0.09 on the arms that show any. Use --fit-sigma to read it off "
                         "the grounding caps instead of guessing.")
    ap.add_argument("--fit-sigma", action="store_true",
                    help="estimate --cap-sigma from the two grounding caps' own residual "
                         "variance in excess of binomial, per arm, and use the mean.")
    ap.add_argument("--floor", type=int, default=3,
                    help="the CONTROL floor |H500| must reach for the run to have a verdict "
                         "(K34a pre-registered 3)")
    ap.add_argument("--reps", type=int, action="append", metavar="N",
                    help="reps to report; repeatable. Default 8, 12, 16.")
    a = ap.parse_args()

    caps = a.ground or CAPS
    for p in caps:
        if not os.path.exists(p):
            raise SystemExit("no such cap: %s" % p)
    prof = profiles(caps)
    reps_list = a.reps or [8, 12, 16]
    cap_sigma = a.cap_sigma
    if a.fit_sigma:
        if len(caps) != 2:
            raise SystemExit("--fit-sigma needs exactly two grounding caps to difference")
        cap_sigma = fit_cap_sigma(caps)

    print("## K34a — power and false-fire, simulated before the capture (M70/M75)")
    print("   grounded in: %s (%d obs/cell)"
          % (", ".join(os.path.basename(p) for p in caps), obs_per_cell(caps)))
    if a.ground:
        print("   ⭐ M83: grounded on the day, NOT on C538's pair. These numbers replace the "
              "pre-registered ones for THIS run only.")
    else:
        print("   ⚠ M83: C538's pair is K34a's ORIGINAL grounding, kept reproducible. ⛔ It is "
              "not the right grounding for a re-run — pass --from the day's caps.")
    print("   ⚠ M78: observed rates are used as true ones, so every figure below is an "
          "UPPER BOUND (see the docstring).")
    if cap_sigma > 0:
        print("   ⭐⭐ cap-sigma %.3f (%.1f points)%s — the SECOND noise source (M84): two caps "
              "of one condition do not sample the same profile." %
              (cap_sigma, 100 * cap_sigma, " [fitted]" if a.fit_sigma else ""))
    else:
        print("   ⛔ cap-sigma 0: two seeds of a condition are assumed to sample the IDENTICAL "
              "profile. That is the assumption K34a's control failure refuted (M84).")
    print("   ⛔ Not a band. It fires nothing.\n")

    for arm in ARMS:
        v = sorted(prof[arm].values())
        med = 0.5 * (v[len(v) // 2 - 1] + v[len(v) // 2])
        pooled = sum(prof[arm].values()) / len(prof[arm])
        print("   %-7s pooled %.1f%%, ladder median %.0f%%, elevated >= %.0f%%  (M73: 38-cell "
              "10-195 ladder, dec 1)" % (arm, 100 * pooled, 100 * med, 100 * (med + HIGH)))
    print("")

    # ⭐⭐ THE CONTROL FIRST. K34a died here and its simulation never looked.
    print("   ⭐⭐ THE CONTROL'S OWN DISTRIBUTION — |H500|, the thing the floor tests (M83)")
    for reps in reps_list:
        t = control_only(prof, reps, a.draws, random.Random(20260917 + reps), cap_sigma)
        meets = 100.0 * sum(v for k, v in t.items() if k >= a.floor) / a.draws
        shape = " ".join("%d:%.0f%%" % (k, 100.0 * t.get(k, 0) / a.draws)
                         for k in range(0, 6) if t.get(k))
        print("      reps %2d   P(|H500| >= %d) = %5.1f%%   [%s]" % (reps, a.floor, meets, shape))
    print("")

    for reps in reps_list:
        for keep_frac, new_max in ((0.75, 1), (0.75, 2)):
            r = run(reps, a.draws, keep_frac, new_max, a.floor, prof=prof,
                    cap_sigma=cap_sigma)
            print("   reps %2d  keep >= %.0f%%  new <= %d  floor %d"
                  % (reps, 100 * keep_frac, new_max, a.floor))
            for truth, label in (("A", "A SAME   (D1 power)"), ("B", "B SCALED (D1 false)"),
                                 ("C", "C SHIFT  (D1 false)"), ("D", "D FLAT   (D1 false)")):
                t = r[truth]
                print("      %-20s D1 %5.1f%%   D2 %5.1f%%   D3 %5.1f%%   none %5.1f%%   "
                      "ctrl-fail %4.1f%%"
                      % (label, t.get("D1-SAME", 0), t.get("D2-MOVED", 0), t.get("D3-GONE", 0),
                         t.get("NO-VERDICT", 0), t.get("CONTROL-FAILED", 0)))
            print("")
    return 0


if __name__ == "__main__":
    sys.exit(main())
