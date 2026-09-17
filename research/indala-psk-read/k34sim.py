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
"""
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


def profiles():
    """True per-cell rate per arm, pooled over the two banked C538 caps (16 obs/cell)."""
    acc = {a: {c: [0, 0] for c in LADDER} for a in ARMS}
    for p in CAPS:
        d = json.load(open(p))
        for a in ARMS:
            for c in LADDER:
                sc = d[a][str(c)]["scores"]
                acc[a][c][0] += sum(1 for _, e in sc if e)
                acc[a][c][1] += len(sc)
    return {a: {c: acc[a][c][0] / acc[a][c][1] for c in LADDER} for a in ARMS}


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


def cap(prof, reps, rng):
    return {c: sum(rng.random() < prof[c] for _ in range(reps)) / reps for c in LADDER}


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
    if len(h1000) == 0 and len(h500) >= 3:
        return "D3-GONE"
    return "NO-VERDICT"


def run(reps, draws, keep_frac, new_max, floor, seed=20260917):
    prof = profiles()
    rng = random.Random(seed)
    out = {}
    for truth in "ABCD":
        tally = {}
        for _ in range(draws):
            h500, h1000 = set(), set()
            for a in ARMS:
                p5 = prof[a]
                p10 = moved(prof[a], truth)
                h500 |= {(a, r) for r in hits([cap(p5, reps, rng) for _ in (0, 1)])}
                h1000 |= {(a, r) for r in hits([cap(p10, reps, rng) for _ in (0, 1)])}
            v = verdict(h500, h1000, keep_frac, new_max, floor)
            tally[v] = tally.get(v, 0) + 1
        out[truth] = {k: 100.0 * v / draws for k, v in tally.items()}
    return out


def main():
    draws = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    prof = profiles()
    print("## K34a — power and false-fire, simulated before the capture (M70/M75)")
    print("   profiles from the two banked C538 caps: 16 obs/cell, the ladder and arms K34a runs.")
    print("   ⛔ Not a band. It fires nothing.\n")
    for a in ARMS:
        v = sorted(prof[a].values())
        med = 0.5 * (v[len(v) // 2 - 1] + v[len(v) // 2])
        print("   %-7s ladder median %.0f%%, elevated >= %.0f%%  (M73: 38-cell 10-195 ladder, "
              "dec 1, n=16 pooled)" % (a, 100 * med, 100 * (med + HIGH)))
    print("")
    for reps in (8, 12, 16):
        for keep_frac, new_max in ((0.75, 1), (0.75, 2)):
            r = run(reps, draws, keep_frac, new_max, 3)
            print("   reps %2d  keep >= %.0f%%  new <= %d" % (reps, 100 * keep_frac, new_max))
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
