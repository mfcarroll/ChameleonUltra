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
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# `shortread.py:112` — the arm's frame in 125 kHz samples; 8 us per sample.
FRAME_SAMPLES = {"indala": 2048, "keri": 2048, "idteck": 2048,
                 "nexwatch": 4096, "gproxii": 6144, "indala224": 7168}
SAMPLE_US = 8.0
# `burstsync.K12_OVERHEAD_MS` — the measured field-up overhead a primer sits on top of.
K12_OVERHEAD_MS = 192

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
    # `_`-prefixed keys are burstsync metadata (`_plan`, M69), never ladder cells.
    return sorted((k for k in cells if k != "none" and not k.startswith("_")), key=int)


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



# ⛔ K18's ladder and bands, from `burstsync.py`'s pinned section. Wings are the four cells at the
# ladder's EDGES — fixed by position, never by outcome (M66).
K18_WINGS = ["100", "105", "165", "170"]
K18_INNER = ["110", "115", "120", "125", "130", "135", "140", "145", "150", "155", "160"]
K18_CONTROL = "65"
K18_GAP = 25.0          # (a) inner minus wings, in EACH seed
K18_RUN_CELL = 50.0     # (b) each cell of the run
K18_RUN_LEN = 2         # (b) adjacent cells, in BOTH seeds, overlapping in >= 1
K18_RUN_GAP = 30.0      # (c) the run over the wings, on the seed-pooled cells
K18_FLAT = 10.0         # V2: the window sits within this of its wings


def _runs(cells, rate_of, lo=K18_RUN_CELL, below=False):
    """Maximal contiguous stretches of `cells` whose rate is at or above `lo`.

    ⭐ `below=True` inverts it — at or BELOW `lo` — which is K20's notch detector. M68: on a
    profile sitting high the detectable feature is a notch, and the forward threshold is the
    ceiling."""
    out, cur = [], []
    for k in cells:
        if (rate_of(k) <= lo) if below else (rate_of(k) >= lo):
            cur.append(k)
        else:
            if len(cur) >= K18_RUN_LEN:
                out.append(cur)
            cur = []
    if len(cur) >= K18_RUN_LEN:
        out.append(cur)
    return out


def k18(paths):
    """V1/V2 over banked `--k12` runs carrying K18's ladder. Gates first, then the bands."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    arms = sorted({a for _, d in runs for a in d})
    print("## K18 — is there a SECOND feature above 80 ms? "
          "⛔ existence only; location is reported, never tested\n")

    for arm in arms:
        print("### %s" % arm)
        per = [(n, d[arm]) for n, d in runs if arm in d]
        cells = numeric_cells(per[0][1])
        for k in cells:
            row = []
            for _, c in per:
                h, n = cell_rate(c[k]["scores"])
                row.append("%-12s" % ("%d/%d (%.0f%%)" % (h, n, 100.0 * h / n)))
            tag = "  ← control, informational" if k == K18_CONTROL else ""
            print("   %8s %s%s" % (k, "  ".join(row), tag))

        # ⛔ Refuse a cap that does not carry K18's ladder rather than scoring a subset of it:
        # a band evaluated over whichever cells happen to be present is not the pinned band.
        missing = [k for k in K18_WINGS + K18_INNER if k not in cells]
        if missing:
            print("   ⛔ this run does not carry K18's ladder — missing %s ms. Not scored.\n"
                  % ",".join(missing))
            continue

        gated = False
        for name, c in per:
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
            arr = max(max(c[k]["arrivals"]) for k in cells)
            print("   gate %s: pooled %.1f%% of %d | split-half %.0f→%.0f (Δ %.1f) | worst P1 %.2f"
                  % (name, allr, alln, halves[0], halves[1], abs(halves[1] - halves[0]), arr))
            if allr < K17_NO_POWER:
                print("      ⛔ NO POWER"); gated = True
            if abs(halves[1] - halves[0]) > K17_DRIFT:
                print("      ⛔ DRIFTED"); gated = True
            if arr > 0.6:
                print("      ⛔ P1 — a cell reached %.2f arrivals/read" % arr)
        if gated:
            print("   ⇒ **NO VERDICT for %s — a gate failed.**\n" % arm)
            continue

        gaps, seed_runs = [], []
        for name, c in per:
            inn, _, _ = pooled(c, K18_INNER)
            wing, _, _ = pooled(c, K18_WINGS)
            gaps.append(inn - wing)
            rate_of = lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
            seed_runs.append(_runs(K18_INNER, rate_of))
            print("   (a) %-22s inner %.1f%% − wings %.1f%% = %+.1f pts | runs %s"
                  % (name, inn, wing, inn - wing,
                     [",".join(r) for r in seed_runs[-1]] or "none"))

        overlap = None
        for r1 in seed_runs[0] if seed_runs else []:
            for r2 in (seed_runs[1] if len(seed_runs) > 1 else []):
                if set(r1) & set(r2):
                    overlap = sorted(set(r1) | set(r2), key=int)
        merged = {k: sum((c[k]["scores"] for _, c in per), []) for k in cells}
        mrate = lambda ks: (100.0 * sum(cell_rate(merged[k])[0] for k in ks)
                            / sum(len(merged[k]) for k in ks))
        run_gap = (mrate(overlap) - mrate(K18_WINGS)) if overlap else None
        print("   (b) a run in BOTH seeds overlapping: %s"
              % (",".join(overlap) + " ms" if overlap else "none"))
        if overlap:
            print("   (c) that run pooled %.1f%% − wings %.1f%% = %+.1f pts"
                  % (mrate(overlap), mrate(K18_WINGS), run_gap))

        a_ok = all(g >= K18_GAP for g in gaps)
        if a_ok and overlap and run_gap >= K18_RUN_GAP:
            print("   ⇒ **V1 FIRES for %s — a second feature exists in 100-170 ms.** "
                  "⛔ Its LOCATION (%s ms) is reported, not tested.\n" % (arm, ",".join(overlap)))
        elif not a_ok and all(abs(g) <= K18_FLAT for g in gaps):
            print("   ⇒ **V2 for %s — no second feature here.** The window sits within %.0f "
                  "points of its wings in both seeds.\n" % (arm, K18_FLAT))
        else:
            print("   ⇒ **no verdict for %s** — reported as such.\n" % arm)



# ⛔ K19's ladder and bands. There are NO designated wings — M67's whole content is that a wing has
# to be justified rather than located, so the reference is the arm's own median across the ladder.
K19_LADDER = [str(x) for x in range(85, 201, 5)]
K19_CONTROL = "65"        # informational, and EXCLUDED from the median (it must not move it)
K19_HIGH = 25.0           # a cell is "elevated" at median + this
K19_LOW = 10.0            # a separating cell is at median − this
K19_RUN = 2               # adjacent elevated cells to be a feature
K19_FLOOR = 25.0          # W2: a top-four cell at median − this
K19_TOP = ["185", "190", "195", "200"]


def k19(paths):
    """W1/W2 over banked `--k12` runs carrying K19's ladder."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    arms = sorted({a for _, d in runs for a in d})
    print("## K19 — how many separated features, and does the profile return to a floor?")
    print("   ⛔ COUNT and SEPARATION only; location is reported, never tested. "
          "⛔ Periodicity is NOT testable here — the burst ceiling stops the ladder near 200 ms.\n")
    for arm in arms:
        per = [(n, d[arm]) for n, d in runs if arm in d]
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K19_LADDER if k in cells]
        partial = len(ladder) < len(K19_LADDER)
        meds, rate_ofs = [], []
        for name, c in per:
            r = lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
            rate_ofs.append(r)
            vals = sorted(r(k) for k in ladder)
            meds.append(vals[len(vals) // 2] if len(vals) % 2
                        else 0.5 * (vals[len(vals) // 2 - 1] + vals[len(vals) // 2]))
        print("### %s   median %s   %s" % (arm, "/".join("%.0f%%" % m for m in meds),
                                           "⛔ PARTIAL LADDER" if partial else ""))
        for k in ladder:
            marks = "".join("▲" if rate_ofs[i](k) >= meds[i] + K19_HIGH else
                            "▽" if rate_ofs[i](k) <= meds[i] - K19_LOW else "·"
                            for i in range(len(per)))
            print("   %5s %s %s" % (k, "  ".join("%-11s" % ("%.0f%%" % f(k)) for f in rate_ofs),
                                    marks))
        gated = False
        for (name, c), _ in zip(per, meds):
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
            arr = max(max(c[k]["arrivals"]) for k in cells)
            print("   gate %s: pooled %.1f%% of %d | split-half Δ %.1f | worst P1 %.2f"
                  % (name, allr, alln, abs(halves[1] - halves[0]), arr))
            if allr < K17_NO_POWER:
                print("      ⛔ NO POWER"); gated = True
            if abs(halves[1] - halves[0]) > K17_DRIFT:
                print("      ⛔ DRIFTED"); gated = True
            if arr > 0.6:
                print("      ⛔ P1 — a cell reached %.2f arrivals/read" % arr)
        if gated:
            print("   ⇒ **NO VERDICT for %s — a gate failed.**\n" % arm); continue

        seed_feats = [_runs(ladder, rate_ofs[i], meds[i] + K19_HIGH) for i in range(len(per))]
        feats = []
        for f1 in seed_feats[0]:
            for f2 in (seed_feats[1] if len(seed_feats) > 1 else []):
                if set(f1) & set(f2):
                    feats.append(sorted(set(f1) | set(f2), key=int))
        print("   features (>= %d adjacent cells at median+%.0f, in BOTH seeds): %s"
              % (K19_RUN, K19_HIGH, [",".join(f) for f in feats] or "none"))
        sep = []
        for a_, b_ in zip(feats, feats[1:]):
            gap = [k for k in ladder if int(k) > int(a_[-1]) and int(k) < int(b_[0])]
            quiet = [k for k in gap
                     if all(rate_ofs[i](k) <= meds[i] - K19_LOW for i in range(len(per)))]
            sep.append((a_[-1], b_[0], len(gap), len(quiet)))
            print("   separation %s→%s ms: %d cells between, %d quiet in both seeds"
                  % (a_[-1], b_[0], len(gap), len(quiet)))
        floor = [k for k in K19_TOP if k in cells
                 and all(rate_ofs[i](k) <= meds[i] - K19_FLOOR for i in range(len(per)))]
        if partial:
            print("   ⛔ partial ladder — W1/W2 not scored for %s\n" % arm); continue
        w1 = len(feats) >= 2 and any(q >= 2 for _, _, _, q in sep)
        print("   ⇒ **W1 %s for %s** — %d separated feature(s)"
              % ("FIRES" if w1 else "REFUTED" if len(feats) < 2 else "no verdict", arm, len(feats)))
        print("   ⇒ **W2 %s for %s** — top-four cells at a floor: %s ⛔ %s\n"
              % ("met" if floor else "FAILED", arm, ",".join(floor) or "none",
                 "the ladder reaches a floor, so its top edge could license a wing"
                 if floor else "THE LADDER IS STILL TOO NARROW — no wing at its top edge is "
                               "licensed for any future band"))



K20_LOW = 25.0        # a cell is "notched" at median − this
K20_BODY = 10.0       # a separating / returned cell is at median + this


def k20(paths):
    """X1/X2 — `indala`'s NOTCHES. The inverse of K19, and `indala` only (see the power table)."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    arms = sorted({a for _, d in runs for a in d})
    print("## K20 — does `indala` have separated NOTCHES in 85-200 ms?")
    print("   ⛔ COUNT and SEPARATION only. ⛔ A pre-registered REPLICATION on fresh seeds of a "
          "shape seen post-hoc in K19 — it must not be scored on K19's own caps.\n")
    for arm in arms:
        per = [(n, d[arm]) for n, d in runs if arm in d]
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K19_LADDER if k in cells]
        if len(ladder) < len(K19_LADDER):
            print("### %s ⛔ partial ladder — not scored\n" % arm); continue
        meds, rate_ofs = [], []
        for name, c in per:
            r = lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
            rate_ofs.append(r)
            vals = sorted(r(k) for k in ladder)
            meds.append(vals[len(vals) // 2] if len(vals) % 2
                        else 0.5 * (vals[len(vals) // 2 - 1] + vals[len(vals) // 2]))
        print("### %s   median %s   notch <= %s   body >= %s"
              % (arm, "/".join("%.0f%%" % m for m in meds),
                 "/".join("%.0f%%" % (m - K20_LOW) for m in meds),
                 "/".join("%.0f%%" % (m + K20_BODY) for m in meds)))
        for k in ladder:
            marks = "".join("▽" if rate_ofs[i](k) <= meds[i] - K20_LOW else
                            "▲" if rate_ofs[i](k) >= meds[i] + K20_BODY else "·"
                            for i in range(len(per)))
            print("   %5s %s %s" % (k, "  ".join("%-11s" % ("%.0f%%" % f(k)) for f in rate_ofs),
                                    marks))
        gated = False
        for name, c in per:
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
            arr = max(max(c[k]["arrivals"]) for k in cells)
            print("   gate %s: pooled %.1f%% of %d | split-half Δ %.1f | worst P1 %.2f"
                  % (name, allr, alln, abs(halves[1] - halves[0]), arr))
            if allr < K17_NO_POWER:
                print("      ⛔ NO POWER"); gated = True
            if abs(halves[1] - halves[0]) > K17_DRIFT:
                print("      ⛔ DRIFTED"); gated = True
            if arr > 0.6:
                print("      ⛔ P1 — a cell reached %.2f arrivals/read" % arr)
        if gated:
            print("   ⇒ **NO VERDICT for %s — a gate failed.**\n" % arm); continue

        seed_n = [_runs(ladder, rate_ofs[i], meds[i] - K20_LOW, below=True)
                  for i in range(len(per))]
        notches = []
        for f1 in seed_n[0]:
            for f2 in (seed_n[1] if len(seed_n) > 1 else []):
                if set(f1) & set(f2):
                    notches.append(sorted(set(f1) | set(f2), key=int))
        print("   notches (>= 2 adjacent cells at median−%.0f, in BOTH seeds): %s"
              % (K20_LOW, [",".join(f) for f in notches] or "none"))
        sep_ok = False
        for a_, b_ in zip(notches, notches[1:]):
            gap = [k for k in ladder if int(k) > int(a_[-1]) and int(k) < int(b_[0])]
            body = [k for k in gap
                    if all(rate_ofs[i](k) >= meds[i] + K20_BODY for i in range(len(per)))]
            print("   separation %s→%s ms: %d cells between, %d in the body in both seeds"
                  % (a_[-1], b_[0], len(gap), len(body)))
            sep_ok = sep_ok or len(body) >= 2
        x1 = len(notches) >= 2 and sep_ok
        print("   ⇒ **X1 %s for %s** — %d separated notch(es)"
              % ("FIRES" if x1 else "REFUTED" if len(notches) < 2 else "no verdict",
                 arm, len(notches)))
        back = [k for k in K19_TOP if k in cells
                and all(rate_ofs[i](k) >= meds[i] + K20_BODY for i in range(len(per)))]
        print("   ⇒ **X2 %s for %s** — top-four cells back in the body: %s ⛔ %s\n"
              % ("met" if back else "FAILED", arm, ",".join(back) or "none",
                 "the profile returns, so this top edge could license a wing"
                 if back else "this ladder's top edge licenses NO wing for this arm either"))


K21_LADDER = [str(x) for x in range(85, 201, 5)]
K21_R = 0.52          # Y1: the p=0.01 two-tailed permutation critical |spearman| at 24 cells
# ⛔⛔ Y2 IS WITHDRAWN AND CARRIES NO THRESHOLD (M70). The argmax-over-lags statistic false-fired
# 41% of the time against two INDEPENDENT profiles (median null gain +0.37 against a 0.30 band);
# at a 0.3% false-fire bar it needs a gain of 1.10 and then detects a real 20 ms offset 0.3% of
# the time. A fixed signed lag of +4 cells is better and still only 37% power at a 5% false-fire
# rate. ⇒ the lag table is PRINTED, labelled exploratory, and no verdict is read off it.
K21_LAGS = [-5, -4, -3, -2, -1, 1, 2, 3, 4, 5]     # cells; the ladder step is 5 ms
K21_LAG_PREREG = 4    # the direction the post-hoc observation names: `keri` 20 ms BELOW `indala`
K21_REGIONS = {                                    # Y3, named by K19/K20 BEFORE this run
    "keri":   ([["95", "100", "105"], ["140", "145", "150"]], False),
    "indala": ([["115", "120", "125", "130"], ["165", "170", "175"]], True),
}
K21_REGION_CELLS = 2   # Y3: cells of a region that must meet the detector, in BOTH seeds


def _ranks(v):
    """Average ranks, so ties are handled — every cell rate here is one of nine values."""
    idx = sorted(range(len(v)), key=lambda i: v[i])
    out = [0.0] * len(v)
    i = 0
    while i < len(idx):
        j = i
        while j + 1 < len(idx) and v[idx[j + 1]] == v[idx[i]]:
            j += 1
        avg = (i + j) / 2.0 + 1
        for k in range(i, j + 1):
            out[idx[k]] = avg
        i = j + 1
    return out


def _pearson(a, b):
    n = len(a)
    if n < 3:
        return float("nan")
    ma, mb = sum(a) / n, sum(b) / n
    sa = math.sqrt(sum((x - ma) ** 2 for x in a))
    sb = math.sqrt(sum((x - mb) ** 2 for x in b))
    if sa == 0 or sb == 0:
        return float("nan")
    return sum((a[i] - ma) * (b[i] - mb) for i in range(n)) / (sa * sb)


def _spearman(a, b):
    return _pearson(_ranks(a), _ranks(b))


def _profile(cells, ladder):
    return [100.0 * cell_rate(cells[k]["scores"])[0] / len(cells[k]["scores"]) for k in ladder]


def _gate(name, c, cells):
    """The three gates every K17+ run carries. Returns (ok, one printable line)."""
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
    bad = []
    if allr < K17_NO_POWER:
        bad.append("NO POWER")
    if drift > K17_DRIFT:
        bad.append("DRIFTED")
    if arr > 0.6:
        bad.append("P1 — a cell reached %.2f arrivals/read" % arr)
    return (not bad,
            "   gate %s: pooled %.1f%% of %d | split-half delta %.1f | worst P1 %.2f%s"
            % (name, allr, alln, drift, arr,
               "" if not bad else "   ⛔ " + "; ".join(bad)))


def k21(paths):
    """Y1/Y2/Y3 — is the `indala`/`keri` structure ONE profile or two offset ones?

    ⛔ Each path is one RUN carrying BOTH arms, and the run must have been captured with
    `--per-arm-shuffle`: without it the two arms share a cell-to-position mapping and any
    cross-arm correlation is partly manufactured (M69). This refuses such a cap by name."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    print("## K21 — do `indala` and `keri` share ONE lead-time profile?")
    print("   ⛔ *Interleaving* was read off two profiles scored with OPPOSITE detectors, and")
    print("   one shared profile reproduces that by construction. Y1 is the discriminating test.")
    print("")

    ok_runs = []
    for name, d in runs:
        arms = [a for a in ("indala", "keri") if a in d]
        if len(arms) < 2:
            print("### %s ⛔ carries %s — K21 needs BOTH arms from ONE run\n"
                  % (name, ",".join(sorted(d)) or "nothing"))
            continue
        plans = {a: d[a].get("_plan") for a in arms}
        if not all(p and p.get("per_arm_shuffle") for p in plans.values()):
            print("### %s ⛔⛔ REFUSED — captured WITHOUT `--per-arm-shuffle` (M69): both arms"
                  % name)
            print("   shared one cell order, so a cross-arm correlation is partly the shuffle")
            print("   and not the air.\n")
            continue
        cells = numeric_cells(d[arms[0]])
        ladder = [k for k in K21_LADDER if k in cells]
        if len(ladder) < len(K21_LADDER):
            print("### %s ⛔ partial ladder — not scored\n" % name)
            continue
        gates, lines = [], []
        for a in arms:
            # ⛔ THE LADDER, NOT `numeric_cells` — K21's band says the 65 ms informational cell is
            # excluded from the median, the correlations AND the gates' pooled figure alike, and
            # the gate must honour the criterion it was committed with. ⚠ K19/K20 pooled their
            # gates over the control too; that is their behaviour and is left alone.
            g, line = _gate("%s %s" % (name, a), d[a], ladder)
            gates.append(g)
            lines.append(line)
        profs = {a: _profile(d[a], ladder) for a in arms}
        print("### %s   plan seeds %s"
              % (name, ", ".join("%s=%d" % (a, plans[a]["seed"]) for a in arms)))
        for line in lines:
            print(line)
        if not all(gates):
            print("   ⇒ ⛔ NO VERDICT for this run — a gate failed.\n")
            continue
        r0 = _spearman(profs["indala"], profs["keri"])
        lagged = []
        for L in K21_LAGS:
            # ⭐ a POSITIVE lag shifts `keri` UP the ladder relative to `indala`.
            if L > 0:
                x, y = profs["indala"][L:], profs["keri"][:-L]
            else:
                x, y = profs["indala"][:L], profs["keri"][-L:]
            lagged.append((L, _spearman(x, y), len(x)))
        best = max(lagged, key=lambda t: t[1])
        preg = [t for t in lagged if t[0] == K21_LAG_PREREG][0]
        print("   Y1 cross-arm spearman at lag 0: **%+.3f** over %d cells" % (r0, len(ladder)))
        print("   ⚠ EXPLORATORY, NO VERDICT (Y2 withdrawn, M70) — lags (ms, r, n): %s"
              % "  ".join("%+d:%+.2f/%d" % (L * 5, r, n) for L, r, n in lagged))
        print("      best non-zero %+d ms at %+.3f (gain %+.3f); the pre-registered %+d ms at "
              "%+.3f (gain %+.3f)"
              % (best[0] * 5, best[1], best[1] - r0,
                 K21_LAG_PREREG * 5, preg[1], preg[1] - r0))
        ok_runs.append({"name": name, "r0": r0, "best": best, "preg": preg, "profs": profs,
                        "ladder": ladder, "d": d})
        print("")

    if len(ok_runs) < 2:
        print("⇒ ⛔ NO VERDICT — Y1 and Y2 both require BOTH runs to agree, and %d scored.\n"
              % len(ok_runs))
        return

    rs = [o["r0"] for o in ok_runs]
    if all(r >= K21_R for r in rs):
        y1 = ("**Y1 — SHARED.** Both runs at or above +%.2f: the arms track each other, and"
              "\n     *interleaving* is an artifact of opposite detectors on arms with different"
              "\n     medians. ⚠ This is the direction foreknowledge pointed at (disclosed in the"
              "\n     band), so it is the weaker of the two verdicts this design can return."
              % K21_R)
    elif all(r <= -K21_R for r in rs):
        y1 = ("**Y1 — OPPOSED.** Both runs at or below −%.2f: one arm is high where the other is"
              "\n     low. That is the strong form of the interleaving reading, and foreknowledge"
              "\n     pointed the other way." % K21_R)
    else:
        y1 = ("⛔ **Y1 — NO VERDICT.** %s against a pre-registered ±%.2f in BOTH runs."
              % (" and ".join("%+.3f" % r for r in rs), K21_R))
    print("⇒ %s" % y1)

    print("")
    print("⇒ ⛔ **Y2 — WITHDRAWN BEFORE THE CAPTURE (M70), so nothing here is a verdict.** The "
          "argmax")
    print("     statistic false-fired 41% against independent profiles; at a 0.3% bar it has "
          "0.3%")
    print("     power against a real 20 ms offset. A fixed +%d-cell lag reaches only 37%% power "
          "at a" % K21_LAG_PREREG)
    print("     5% false-fire rate. ⇒ **the offset is not measurable on this ladder at this n**, "
          "and")
    print("     a silent Y2 is NOT evidence that the arms are aligned.")
    print("     for the record: best-lag gains %s | pre-registered %+d ms gains %s"
          % (", ".join("%+.3f" % (o["best"][1] - o["r0"]) for o in ok_runs),
             K21_LAG_PREREG * 5,
             ", ".join("%+.3f" % (o["preg"][1] - o["r0"]) for o in ok_runs)))
    print("")
    print("## Y3 — do K19/K20's NAMED regions come back on fresh seeds?")
    y3 = True
    for arm, (regions, below) in sorted(K21_REGIONS.items()):
        meds, rate_ofs = [], []
        for o in ok_runs:
            c = o["d"][arm]
            r = lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
            rate_ofs.append(r)
            vals = sorted(r(k) for k in o["ladder"])
            meds.append(0.5 * (vals[len(vals) // 2 - 1] + vals[len(vals) // 2])
                        if len(vals) % 2 == 0 else vals[len(vals) // 2])
        thr = [(m - K20_LOW) if below else (m + K19_HIGH) for m in meds]
        print("   %-7s median %s   detector %s %s"
              % (arm, "/".join("%.0f%%" % m for m in meds),
                 "notch <=" if below else "high >=", "/".join("%.0f%%" % t for t in thr)))
        for reg in regions:
            per_seed = []
            for i in range(len(ok_runs)):
                if below:
                    hit = [k for k in reg if rate_ofs[i](k) <= thr[i]]
                else:
                    hit = [k for k in reg if rate_ofs[i](k) >= thr[i]]
                per_seed.append(hit)
            ok = all(len(h) >= K21_REGION_CELLS for h in per_seed)
            y3 = y3 and ok
            print("      %-18s %s ⇒ %s"
                  % (",".join(reg),
                     " | ".join("%s: %s" % (ok_runs[i]["name"].split("_")[-1],
                                            ",".join(per_seed[i]) or "none")
                                for i in range(len(per_seed))),
                     "replicates" if ok else "⛔ FALLS SHORT"))
    print("   ⇒ **Y3 %s** — all four named regions carry >= %d cells in BOTH fresh seeds%s"
          % ("FIRES" if y3 else "REFUTED", K21_REGION_CELLS,
             "" if y3 else ": at least one does not."))
    print("")
    print("⛔ Ungraded — no null sweep, no calibration row. It moves no cell.")
    print("")


K22_LADDER = [str(x) for x in range(20, 81, 5)]
K22_LOW_CELLS = ["20", "25", "30", "35"]     # Z1: the region nothing has ever measured
K22_Z1 = 40.0        # ABSOLUTE, from the 0-17% floor K15/K16 measured at 40/45 — not a median
K22_Z1_RUN = 2       # adjacent cells, in BOTH seeds, overlapping in >= 1
K22_ANCHOR = ["50", "55", "60", "65"]        # Z2: K15/K16 measured the hump here
K22_Z2 = 50.0
K22_Z2_CELLS = 2


def k22(paths):
    """Z1/Z2/Z3 — the 20-40 ms region, which no ladder has reached.

    ⛔ Z2 is a GATE on Z1, not a finding: a session that cannot reproduce the hump two prior
    runs measured is not evidence about a region nobody has measured."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    print("## K22 — is there any structure below 40 ms?")
    print("   ⭐ The reference is a FLOOR measured in two prior independent sessions on both arms")
    print("   (k15/k16, 40 and 45 ms at 0-17%%), so the %.0f%% threshold is absolute, not a "
          "median." % K22_Z1)
    print("   ⛔ The frame-locked notch this region would test is UNTESTABLE — a notch needs a")
    print("   body, and the measured body here is that floor.")
    print("")
    arms = sorted({a for _, d in runs for a in d})
    for arm in arms:
        per = [(n, d[arm]) for n, d in runs if arm in d]
        if len(per) < 2:
            print("### %s ⛔ needs two seeds — %d supplied\n" % (arm, len(per)))
            continue
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K22_LADDER if k in cells]
        if len(ladder) < len(K22_LADDER):
            print("### %s ⛔ partial ladder — not scored\n" % arm)
            continue
        rate_ofs = []
        for name, c in per:
            rate_ofs.append(lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0]
                            / len(c[k]["scores"]))
        print("### %s" % arm)
        for k in ladder:
            print("   %5s %s %s"
                  % (k, "  ".join("%-11s" % ("%.0f%%" % f(k)) for f in rate_ofs),
                     "".join("▲" if f(k) >= K22_Z1 else "·" for f in rate_ofs)))
        gated = False
        for i, (name, c) in enumerate(per):
            g, line = _gate(name, c, cells)
            print(line)
            gated = gated or not g
        if gated:
            print("   ⇒ ⛔ NO VERDICT for %s — a gate failed.\n" % arm)
            continue
        z2 = [[k for k in K22_ANCHOR if f(k) >= K22_Z2] for f in rate_ofs]
        z2_ok = all(len(h) >= K22_Z2_CELLS for h in z2)
        print("   Z2 continuity (the hump K15/K16 measured, >= %.0f%% in >= %d of %s): %s ⇒ %s"
              % (K22_Z2, K22_Z2_CELLS, ",".join(K22_ANCHOR),
                 " | ".join(",".join(h) or "none" for h in z2),
                 "the session reproduces it" if z2_ok else
                 "⛔⛔ IT DOES NOT — Z1 is NOT READ for this arm"))
        if not z2_ok:
            print("   ⇒ ⛔ NO VERDICT for %s — Z2 is a gate and it failed.\n" % arm)
            continue
        seed_runs = [_runs(K22_LOW_CELLS, f, K22_Z1) for f in rate_ofs]
        found = []
        for f1 in seed_runs[0]:
            for f2 in seed_runs[1]:
                if set(f1) & set(f2):
                    found.append(sorted(set(f1) | set(f2), key=int))
        print("   Z1 (>= %d adjacent cells of %s at >= %.0f%%, in BOTH seeds): %s"
              % (K22_Z1_RUN, ",".join(K22_LOW_CELLS), K22_Z1,
                 [",".join(f) for f in found] or "none"))
        print("   ⇒ **Z1 %s for %s**%s"
              % ("FIRES — there is structure below 40 ms" if found else "REFUTED",
                 arm,
                 "" if found else " — no elevated run below 40 ms in both seeds. ⛔ That is a"
                 " statement\n     about 20-35 ms only, and NOT about the frame-locked notch,"
                 " which has no power here."))
        bounds = []
        for f in rate_ofs:
            hi = [k for k in ladder if f(k) >= K22_Z1]
            bounds.append(hi[0] if hi else None)
        print("   Z3 reported, never tested: the lowest cell at or above %.0f%% is %s ⇒ carry the"
              % (K22_Z1, " / ".join(b or "none" for b in bounds)))
        print("      RANGE across seeds, never a single boundary (M64).\n")
    print("⛔ Ungraded — no null sweep, no calibration row. It moves no cell.")
    print("")


K23_LADDER = ["1", "5", "10", "15", "20", "25", "30", "50", "55", "60", "65"]
K23_EDGE = ["1", "5", "10"]          # V1: the cells below anything ever measured
K23_LOW = 40.0                       # V1: ABSOLUTE, carried unchanged from K22's Z1
K23_ANCHOR = ["50", "55", "60", "65"]        # V2: the hump k15/k16 measured
K23_REGION = ["20", "25", "30"]              # V2b: the region C525 measured, an hour earlier
K23_HIGH = 50.0
K23_HIGH_CELLS = 2


def k23(paths):
    """V1/V2/V2b/V3 — where does C525's 20-30 ms region start?

    ⛔ V2 AND V2b are GATES, not findings. V2 asks the older hump to reappear and V2b asks
    C525's own region to reappear; either failing means V1 is not read for that arm."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    print("## K23 — where does the 20-30 ms region start?")
    print("   ⛔ The axis floor is the design's own ~%d ms field-up overhead, NOT 1 ms: this"
          % K12_OVERHEAD_MS)
    print("   ladder spans %d-%d ms of ELAPSED time. And the no-primer control is a FRESH BURST,"
          % (1 + K12_OVERHEAD_MS, 65 + K12_OVERHEAD_MS))
    print("   not primer=0, so it is informational and is not this ladder's lowest cell.")
    print("")
    arms = sorted({a for _, d in runs for a in d})
    for arm in arms:
        per = [(n, d[arm]) for n, d in runs if arm in d]
        if len(per) < 2:
            print("### %s ⛔ needs two seeds — %d supplied\n" % (arm, len(per)))
            continue
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K23_LADDER if k in cells]
        if len(ladder) < len(K23_LADDER):
            print("### %s ⛔ partial ladder — not scored\n" % arm)
            continue
        rate_ofs = [(lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"]))
                    for _, c in per]
        print("### %s" % arm)
        for k in ladder:
            print("   %5s %s %s"
                  % (k, "  ".join("%-11s" % ("%.0f%%" % f(k)) for f in rate_ofs),
                     "".join("▽" if f(k) <= K23_LOW else "▲" if f(k) >= K23_HIGH else "·"
                             for f in rate_ofs)))
        ctl = [c["none"] for _, c in per if "none" in c]
        if ctl:
            print("   %5s %s  (informational — a FRESH burst, arrivals %s)"
                  % ("none",
                     "  ".join("%-11s" % ("%.0f%%" % (100.0 * cell_rate(x["scores"])[0]
                                                      / len(x["scores"]))) for x in ctl),
                     "/".join("%.2f" % (sum(x["arrivals"]) / len(x["arrivals"])) for x in ctl)))
        gated = False
        for name, c in per:
            g, line = _gate(name, c, ladder)
            print(line)
            gated = gated or not g
        if gated:
            print("   ⇒ ⛔ NO VERDICT for %s — a gate failed.\n" % arm)
            continue
        ok = True
        for lbl, sel in (("V2  (the k15/k16 hump)", K23_ANCHOR),
                         ("V2b (C525's own region)", K23_REGION)):
            hits = [[k for k in sel if f(k) >= K23_HIGH] for f in rate_ofs]
            good = all(len(h) >= K23_HIGH_CELLS for h in hits)
            ok = ok and good
            print("   %s: >= %.0f%% in >= %d of %s ⇒ %s ⇒ %s"
                  % (lbl, K23_HIGH, K23_HIGH_CELLS, ",".join(sel),
                     " | ".join(",".join(h) or "none" for h in hits),
                     "reproduces" if good else "⛔⛔ DOES NOT — V1 is NOT READ for this arm"))
        if not ok:
            print("   ⇒ ⛔ NO VERDICT for %s — a continuity gate failed.\n" % arm)
            continue
        edge = [[k for k in K23_EDGE if f(k) <= K23_LOW] for f in rate_ofs]
        v1 = all(e for e in edge)
        print("   V1 (any of %s at <= %.0f%% in BOTH seeds): %s"
              % (",".join(K23_EDGE), K23_LOW,
                 " | ".join(",".join(e) or "none" for e in edge)))
        if v1:
            print("   ⇒ **V1 FIRES for %s — the region HAS a bottom edge inside this ladder.**"
                  % arm)
            print("      ⛔ Its location is reported, never tested. ⛔ And a firing V1 licenses"
                  " *the profile\n      comes down*, never *it reaches a floor*: at a true level"
                  " of 62%% this detector fires\n      14%% of the time from counting noise"
                  " alone.")
        else:
            print("   ⇒ **V1 FAILS for %s — AND THAT IS THE RESULT WITH TEETH.** The profile is"
                  % arm)
            print("      elevated at every lead time the primer can reach, down to 1 ms = %d ms"
                  % (1 + K12_OVERHEAD_MS))
            print("      of elapsed against this design's own %d ms floor. ⇒ **the region's"
                  % K12_OVERHEAD_MS)
            print("      bottom is not below 20 ms; it is inside the field-up overhead, where no")
            print("      primer can go.** The next move is the overhead, not the ladder.")
        lo = [[k for k in ladder if f(k) >= K23_LOW] for f in rate_ofs]
        print("   V3 reported, never tested: the lowest cell at or above %.0f%% is %s ⇒ carry the"
              % (K23_LOW, " / ".join(x[0] if x else "none" for x in lo)))
        print("      RANGE across seeds, never a single boundary (M64).\n")
    print("⛔ Ungraded — no null sweep, no calibration row. It moves no cell.")
    print("")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--table", action="store_true",
                    help="print every banked cell in both units, then the verdict")
    ap.add_argument("--k17", nargs="+", metavar="CAP",
                    help="⭐ score K17's U1-U4 over banked `--k12` runs on `nexwatch`")
    ap.add_argument("--k18", nargs="+", metavar="CAP",
                    help="⭐ score K18's V1/V2 — is there a SECOND feature above 80 ms?")
    ap.add_argument("--k19", nargs="+", metavar="CAP",
                    help="⭐ score K19's W1/W2 — how many separated features, and is there a floor?")
    ap.add_argument("--k23", nargs="+", metavar="CAP",
                    help="⭐ K23: the 1-65 ms ladder, two seeds, both arms")
    ap.add_argument("--k22", nargs="+", metavar="CAP",
                    help="⭐ K22: the 20-80 ms ladder, two seeds, both arms")
    ap.add_argument("--k21", nargs="+", metavar="CAP",
                    help="⭐ K21: runs carrying BOTH arms, captured with --per-arm-shuffle")
    ap.add_argument("--k20", nargs="+", metavar="CAP",
                    help="⭐ score K20's X1/X2 — `indala`'s NOTCHES, the inverse detector (M68)")
    a = ap.parse_args()
    if a.k23:
        k23(a.k23)
        return 0
    if a.k22:
        k22(a.k22)
        return 0
    if a.k21:
        k21(a.k21)
        return 0
    if a.k20:
        k20(a.k20)
        return 0
    if a.k19:
        k19(a.k19)
        return 0
    if a.k18:
        k18(a.k18)
        return 0
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
