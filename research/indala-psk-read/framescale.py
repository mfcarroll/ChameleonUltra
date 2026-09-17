#!/usr/bin/env python3
"""Is the lead-time hump fixed in MILLISECONDS or in the arm's own FRAME COUNT?

    ./framescale.py                 # the verdict, from banked caps only — no device, no bench
    ./framescale.py --table         # every banked cell, in both units

⭐ **OFFLINE RE-ANALYSIS. It opens no serial port, arms nothing and moves no cell.** Every number
comes out of `caps/k12_gproxii_s17.json`, `k13_gproxii_s41.json`, `k14_gproxii_s73.json`,
`k15_indala_keri_s91.json` and `k16_indala_keri_s137.json`, which `burstsync.py` wrote before
anything formatted them.

## ⭐⭐ THE QUESTION, AND IT WAS PRE-REGISTERED BY THE ROUND THAT RAISED IT

`rfid-tools` `QUEUE.md`, committed `599b0b9` at 2026-09-17 02:05, lists the next hands-off units
and this is its second: *"Is the hole/hump fixed in lead time or in the arm's own frame count? —
the same arithmetic that turned C494's 6x spread into frames (C499). Cheap, and it would explain
the sign difference if it works."* ⭐ **The hypothesis is therefore pre-registered in the strict
sense — it was written down, in a commit, before this file existed.** What is written below is its
OPERATIONALISATION, and that is pinned here before the analysis runs.

⚠⚠ **THE DISCLOSURE THAT MATTERS, AND IT IS NOT SMALL.** These captures are BANKED, so this is a
retrodiction, not a blind test, and I had already read the prose describing K12 as *"saturated with
every reachable cell"* (C513) before writing this band. ⛔ **So a SUPPORTED verdict here would be
worth very little** — I cannot certify that a band I wrote was uninfluenced by a headline I had
read. A REFUTED verdict is the one this file can honestly carry, because foreknowledge of the
saturation cannot manufacture a collapse that is not in the data. Said here, before the run,
rather than in the write-up.

## THE ARITHMETIC (`shortread.py:112`, and none of it is new)

Each arm's frame, in 125 kHz samples and in milliseconds at 8 us/sample:

    indala  2048 = 16.384 ms      nexwatch   4096 = 32.768 ms
    keri    2048 = 16.384 ms      gproxii    6144 = 49.152 ms
    idteck  2048 = 16.384 ms      indala224  7168 = 57.344 ms

⛔ **`indala` AND `keri` CANNOT DISCRIMINATE — THEY SHARE A FRAME.** Both are 2048 samples, so
their near-identical humps at the same 65 ms (K15, K16) are exactly what BOTH hypotheses predict.
The whole question turns on `gproxii`, whose frame is 3x theirs, and the banked K12/K13 ladders are
the only thing on the bench that spans enough of its frame axis to answer it.

    lead time in frames = lead_ms / frame_ms

`indala` and `keri` peak at 65 ms = **3.97 frames**, with wings at 40 ms = 2.44 f and 80 ms =
4.88 f. `gproxii`'s notch is at 60 ms = **1.22 frames**, and its K12/K13 ladder (20..200 ms in 20 ms
steps) spans **0.41 .. 4.07 frames** — so it covers the predicted peak (3.97 f = 195 ms, the 200 ms
cell) and the whole of the predicted low wing.

## ⛔⛔ THE CRITERION, PINNED BEFORE THE ANALYSIS RUNS (M55)

**H_frame** — the hump is a feature of the arm's own frame count: high at ~3-4.3 frames, wings
collapsing at <= 2.44 f and >= 4.88 f, on every arm.
**H_ms** — the features sit at fixed millisecond lead times and the frame length is irrelevant.

⭐ **F1 — THE LOW WING IS THE HALF THAT CARRIES, AND THE HIGH HALF IS DECLARED USELESS HERE.**
`gproxii` sits near 100% across its ladder, so *"high at 3.97 f"* would pass on a saturated ladder
without evidence — the K8 NO POWER trap, named for the fourth time in this project. ⛔ **The high
half is reported and NOT counted.** The low wing is where the ladder has power: `gproxii`'s 60 ms
notch proves this ladder can show a total collapse when one is there.

  Pooled over `gproxii`'s cells at **<= 2.44 frames** — 20, 40, 60, 80, 100, 120 ms — in K12 and
  K13 **separately**, so the verdict has to survive two seeds:

  - pooled rate **<= 30%** in BOTH runs ⇒ **H_frame SUPPORTED**: the wing is there, moved to where
    the frame count puts it.
  - pooled rate **>= 60%** in BOTH runs ⇒ **H_frame REFUTED**: at the frame count where `indala`
    and `keri` collapse to 0-17%, `gproxii` is fine, so the feature does not travel in frames.
  - the two runs disagree, or either lands between ⇒ **no verdict**, reported as such.

  ⚠ **M59 IS RESPECTED AND THIS IS HOW.** No arm's rate is compared with another arm's. Each is
  measured against a threshold stated here before the numbers were read, which is K16's T3
  construction. The 30/60 points come from `indala`'s and `keri`'s OWN wing values (0-17%) and
  their own peaks (92-96%) as reported in C517 — i.e. from the shape H_frame says travels, not
  from `gproxii`'s ladder.

  ⛔ **THE NOTCH CELL IS INCLUDED, DELIBERATELY.** 60 ms is `gproxii`'s known 0% hole (C515,
  reproduced three times), and dropping it would RAISE the pool and make refutation easier. It
  stays in for the verdict; the pool without it is printed as a sensitivity check only.

  ⛔ **NO SINGLE CELL CAN DECIDE THIS** (M62, M64). The pool is six cells wide, it must agree
  across two independently seeded runs, and the sensitivity check says what the one anomalous cell
  is worth.

⭐ **F2 — THE SHAPE CHECK, and it is subordinate to F1.** Under H_frame `gproxii`'s ladder should
have RANGE: the six low cells pooled must sit at least **30 points** below the top three (160, 180,
200 ms). A ladder whose range is under 15 points has no shape for either hypothesis to own, and F2
reports **FLAT** — which is not a verdict for H_ms either.

⛔⛔ **WHAT THIS CANNOT DO, WRITTEN DOWN BEFORE IT RUNS.**
1. It cannot test H_frame's *hole* reading. `gproxii`'s notch is at 1.22 frames, which on `indala`
   and `keri` is **20 ms** — below K14/K15/K16's 40 ms floor. **Unmeasured, and this file does not
   guess at it.** A 20 ms cell on those two arms is a real, cheap, hands-off capture and is the
   follow-up regardless of which way F1 lands.
2. ⛔ **H_frame does not explain the sign difference even if it is SUPPORTED.** A hump at 3.97 f
   and a hole at 1.22 f are two different features at two different frame counts; frame-scaling
   relocates the disagreement, it does not dissolve it. `QUEUE.md`'s *"it would explain the sign
   difference if it works"* is too generous to the hypothesis and this file does not inherit it.
3. Ungraded throughout — no null sweep, no calibration row, no licence. **It moves no cell**, and a
   re-analysis is not a bench result (AUTOPILOT §4).

## ⭐⭐⭐ K17's BANDS IN CODE, WRITTEN BEFORE THE SCORING CAPTURE

`burstsync.py`'s K17 section (pinned `fdd31f49`) asks the question C519 could not: `nexwatch` is
PSK like `indala`/`keri` but its frame is 4096 samples, so **H_ms puts its peak at 65 ms and
H_frame at 130 ms**. ⛔ **The bands are implemented HERE, before the capture, because C519's own
slip was a prose band and a rounded constant disagreeing** — six wing cells named in the text,
five selected by the code. A criterion that exists only in prose is not yet a criterion.

  **U1 — H_ms** ⇒ pooled(55,60,65,70,75) − pooled(40,45,80) >= **30 points**.
  **U2 — H_frame** ⇒ pooled(120,125,130,135,140) − pooled(80,160) >= **30 points**.
  Each also needs its shape: **>= 3 of its 5 window cells above the highest of its own wings.**
  **U3** both fire ⇒ no verdict on location. **U4** neither ⇒ no verdict on location, and the
  substantive reading is that `nexwatch` does not carry the hump.

⛔ **THE GATES RUN FIRST AND EITHER ONE ENDS IT.** Pooled over all fourteen cells **< 15%** ⇒
**NO POWER**, neither U is read. Split-half over the rounds disagreeing by **> 15 points** ⇒ the
run drifted and its structure is not interpretable (K16's control, which survived; ⛔ not K15's
anchors rule, withdrawn as M63).

⚠⚠ **AMENDED BEFORE ANY SCORING DATA EXISTED, AND FOR A BENCH-TIME REASON.** K17 as pinned implied
one run. Fifteen sessions per round against the tick's budget makes **two runs of `--reps 8` at
different seeds** the affordable shape, and it is the better one — it is C519's own construction,
where a verdict had to survive two independent seeds. ⇒ **The 30-point gap must hold in EACH seed
separately**; the 3-of-5 shape clause is evaluated on the cells **pooled across both**, because at
n=8 a single cell cannot carry it (M58). Recorded here rather than in the write-up.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# `shortread.py:112` — the arm's frame in 125 kHz samples; 8 us per sample.
FRAME_SAMPLES = {"indala": 2048, "keri": 2048, "idteck": 2048,
                 "nexwatch": 4096, "gproxii": 6144, "indala224": 7168}
SAMPLE_US = 8.0

RUNS = [("k12_gproxii_s17", 17), ("k13_gproxii_s41", 41), ("k14_gproxii_s73", 73),
        ("k15_indala_keri_s91", 91), ("k16_indala_keri_s137", 137)]

# F1's pool: the cells at or below `indala`/`keri`'s own low wing. ⛔ DERIVED, not typed: the
# criterion names their 40 ms cell, and 40/16.384 is 2.44140625, so the rounded 2.44 in the prose
# excluded `gproxii`'s 120 ms cell (120/49.152 = 2.44140625) that the same prose lists by name.
# Caught on the first run and fixed before the verdict was written up; it moved neither pool's band.
WING_FRAMES = 40.0 / (2048 * SAMPLE_US / 1000.0)
F1_SUPPORT, F1_REFUTE = 30.0, 60.0
F2_RANGE, F2_FLAT = 30.0, 15.0


def frame_ms(arm):
    return FRAME_SAMPLES[arm] * SAMPLE_US / 1000.0


def load(name):
    with open(os.path.join(HERE, "caps", name + ".json")) as fh:
        return json.load(fh)


def cell_rate(scores):
    """(exact hits, n) — `burstsync.rate()`'s definition, the one every K table quotes."""
    return sum(1 for _, exact in scores if exact), len(scores)


def pooled(cells, keys):
    hits = sum(cell_rate(cells[k]["scores"])[0] for k in keys)
    n = sum(cell_rate(cells[k]["scores"])[1] for k in keys)
    return (100.0 * hits / n if n else 0.0), hits, n


def numeric_cells(cells):
    return sorted((k for k in cells if k != "none"), key=int)


def table():
    for name, seed in RUNS:
        for arm, cells in load(name).items():
            fms = frame_ms(arm)
            print("\n%s  %s  (seed %d)  frame %d samples = %.3f ms"
                  % (name, arm, seed, FRAME_SAMPLES[arm], fms))
            print("   %8s %8s %8s" % ("lead ms", "frames", "exact"))
            for k in numeric_cells(cells):
                hits, n = cell_rate(cells[k]["scores"])
                print("   %8s %8.2f %8s" % (k, int(k) / fms, "%d/%d (%.0f%%)"
                                            % (hits, n, 100.0 * hits / n)))
            hits, n = cell_rate(cells["none"]["scores"])
            print("   %8s %8s %8s" % ("none", "-", "%d/%d" % (hits, n)))


def f1():
    """The verdict. `gproxii`'s cells at <= 2.44 frames, K12 and K13 separately."""
    print("## F1 — the low wing, where H_frame says `gproxii` must collapse\n")
    fms = frame_ms("gproxii")
    verdicts = []
    for name, seed in [("k12_gproxii_s17", 17), ("k13_gproxii_s41", 41)]:
        cells = load(name)["gproxii"]
        wing = [k for k in numeric_cells(cells) if int(k) / fms <= WING_FRAMES]
        top = numeric_cells(cells)[-3:]
        rate, hits, n = pooled(cells, wing)
        sens, _, sn = pooled(cells, [k for k in wing if k != "60"])
        trate, _, tn = pooled(cells, top)
        print("%s (seed %d)" % (name, seed))
        print("   wing cells %s = %.2f..%.2f frames"
              % (",".join(wing), int(wing[0]) / fms, int(wing[-1]) / fms))
        print("   pooled EXACT %d/%d = %.1f%%   [sensitivity, 60 ms dropped: %.1f%% of %d]"
              % (hits, n, rate, sens, sn))
        print("   top three %s = %.1f%% of %d  (reported, NOT counted — no power at a ceiling)"
              % (",".join(top), trate, tn))
        v = ("SUPPORTED" if rate <= F1_SUPPORT else
             "REFUTED" if rate >= F1_REFUTE else "between")
        print("   ⇒ %s (<= %.0f support, >= %.0f refute)\n" % (v, F1_SUPPORT, F1_REFUTE))
        verdicts.append((v, rate, trate - rate))
    vs = {v for v, _, _ in verdicts}
    if vs == {"SUPPORTED"}:
        out = "H_frame SUPPORTED in both seeds"
    elif vs == {"REFUTED"}:
        out = "H_frame REFUTED in both seeds"
    else:
        out = "NO VERDICT — the two seeds do not agree, or a pool landed between the bands"
    print("⇒ **F1: %s**\n" % out)

    print("## F2 — is there any shape for either hypothesis to own?\n")
    for (name, _), (_, rate, span) in zip([("k12_gproxii_s17", 17), ("k13_gproxii_s41", 41)],
                                          verdicts):
        shape = ("RANGE" if span >= F2_RANGE else "FLAT" if abs(span) < F2_FLAT else "between")
        print("   %s  top3 - wing = %+.1f points ⇒ %s" % (name, span, shape))
    return out



# ⛔ K17's ladder, exactly as `burstsync.py`'s pinned section lists it. Cells are strings because
# that is how `burstsync --out` keys them.
K17_MS_WINDOW = ["55", "60", "65", "70", "75"]      # H_ms: where indala and keri peak
K17_MS_WINGS = ["40", "45", "80"]
K17_FRAME_WINDOW = ["120", "125", "130", "135", "140"]   # H_frame: 3.66..4.27 of nexwatch's frame
K17_FRAME_WINGS = ["80", "160"]
K17_GAP = 30.0          # points, and it must hold in EACH seed
K17_SHAPE = 3           # of 5 window cells above the highest wing, on the seed-pooled cells
K17_NO_POWER = 15.0     # pooled over every cell, below which neither U is read
K17_DRIFT = 15.0        # split-half over rounds


def k17(paths):
    """U1-U4 over one or more banked `--k12` runs on `nexwatch`. Gates first, then the bands."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))["nexwatch"])
            for p in paths]
    fms = frame_ms("nexwatch")
    print("## K17 — does `nexwatch`'s hump sit at H_ms's 65 ms or H_frame's %.0f ms?\n"
          % (3.97 * fms))

    print("   %8s %8s %s" % ("lead ms", "frames", "  ".join("%-12s" % n for n, _ in runs)))
    cells = numeric_cells(runs[0][1])
    for k in cells:
        row = []
        for _, c in runs:
            h, n = cell_rate(c[k]["scores"])
            row.append("%-12s" % ("%d/%d (%.0f%%)" % (h, n, 100.0 * h / n)))
        print("   %8s %8.2f %s" % (k, int(k) / fms, "  ".join(row)))

    print("\n### gates")
    gated = False
    for name, c in runs:
        allr, _, alln = pooled(c, cells)
        halves = []
        for lo, hi in ((0, 0.5), (0.5, 1.0)):
            h = n = 0
            for k in cells:
                sc = c[k]["scores"]
                part = sc[int(len(sc) * lo):int(len(sc) * hi)]
                h += sum(1 for _, e in part if e)
                n += len(part)
            halves.append(100.0 * h / n if n else 0.0)
        drift = abs(halves[1] - halves[0])
        arr = max(max(c[k]["arrivals"]) for k in cells)
        print("   %s  pooled %.1f%% of %d | split-half %.0f%% → %.0f%% (Δ %.1f) | worst P1 %.2f"
              % (name, allr, alln, halves[0], halves[1], drift, arr))
        if allr < K17_NO_POWER:
            print("      ⛔ NO POWER — pooled below %.0f%%; neither U is read" % K17_NO_POWER)
            gated = True
        if drift > K17_DRIFT:
            print("      ⛔ DRIFTED — split-half over %.0f points; structure not interpretable"
                  % K17_DRIFT)
            gated = True
        if arr > 0.6:
            print("      ⛔ P1 — a cell reached %.2f arrivals/read; reported, not interpreted" % arr)
    if gated:
        print("\n⇒ **K17: NO VERDICT — a gate failed, and that decides it before any U does.**")
        return

    print("\n### U1 / U2 — the 30-point gap, in EACH seed")
    fired = {}
    for tag, win, wings in (("U1 H_ms", K17_MS_WINDOW, K17_MS_WINGS),
                            ("U2 H_frame", K17_FRAME_WINDOW, K17_FRAME_WINGS)):
        gaps = []
        for name, c in runs:
            w, _, _ = pooled(c, win)
            g, _, _ = pooled(c, wings)
            gaps.append(w - g)
            print("   %-11s %-22s window %.1f%% − wings %.1f%% = %+.1f pts"
                  % (tag, name, w, g, w - g))
        # the shape clause, on the cells pooled across every seed (M58 — n=8 cannot carry a cell)
        merged = {k: sum((c[k]["scores"] for _, c in runs), []) for k in cells}
        wing_max = max(100.0 * cell_rate(merged[k])[0] / len(merged[k]) for k in wings)
        above = [k for k in win
                 if 100.0 * cell_rate(merged[k])[0] / len(merged[k]) > wing_max]
        ok = all(g >= K17_GAP for g in gaps) and len(above) >= K17_SHAPE
        print("   %-11s shape: %d of 5 above the highest wing (%.1f%%) — need %d ⇒ %s\n"
              % (tag, len(above), wing_max, K17_SHAPE, "FIRES" if ok else "does not fire"))
        fired[tag] = ok

    a, b = fired["U1 H_ms"], fired["U2 H_frame"]
    if a and not b:
        out = ("**U1 — H_ms.** The hump sits at the same lead time on an arm whose frame is twice "
               "as long ⇒ it does not travel with the frame, and M65's confound is broken")
    elif b and not a:
        out = ("**U2 — H_frame.** The hump moved to `nexwatch`'s own 3.97 frames ⇒ it travels with "
               "the frame after all, and C519's refutation was the modulation confound")
    elif a and b:
        out = ("**U3 — BOTH FIRED, NO VERDICT ON LOCATION.** This ladder cannot separate two humps "
               "from one broad rise spanning them, and K17 said so before the capture")
    else:
        out = ("**U4 — NEITHER FIRED.** No verdict on location; the substantive reading is that "
               "`nexwatch` does not carry C517's hump. ⛔ Not the same as *no hump anywhere* — "
               "14 cells across 40-160 ms leave most of the range untouched")
    print("⇒ **K17: %s**" % out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--table", action="store_true",
                    help="print every banked cell in both units, then the verdict")
    ap.add_argument("--k17", nargs="+", metavar="CAP",
                    help="⭐ score K17's U1-U4 over banked `--k12` runs on `nexwatch`")
    a = ap.parse_args()
    if a.k17:
        k17(a.k17)
        return 0
    if a.table:
        table()
        print()
    f1()
    return 0


if __name__ == "__main__":
    sys.exit(main())
