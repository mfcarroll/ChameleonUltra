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
    # ⭐ `key=float`, not `int`: K32's ladder is at 2.5 ms, so a cell key can be "52.5".
    # Strictly safer than `int` for every existing caller — float("100") sorts identically.
    return sorted((k for k in cells if k != "none" and not k.startswith("_")), key=float)


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
# ⛔⛔ V2b IS `indala`-ONLY, AND THE FIRST VERSION OF THIS BAND GOT THAT WRONG (M71). It asks
# C525's 20-30 ms region to reappear — but C525 established that region on `indala` and
# **Z1 was REFUTED for `keri`**, whose 20 ms cell was elevated and STOOD ALONE, reported and not
# counted. Applying it to `keri` gates an arm on another arm's finding, and it blocked V1 for the
# very arm a re-run existed to measure. ⚠ The correction is justified by the PRIOR record
# (C525's own refutation, published before K23 was written) and not by K23's data — but the
# caps taken under the wrong gate have been looked at, so `keri` re-runs on FRESH seeds.
K23_REGION = {"indala": ["20", "25", "30"]}   # V2b: arms with an established region there
K23_HIGH = 50.0
K23_HIGH_CELLS = 2


def _split_half(c, ladder):
    """K16's shape-agnostic drift statistic, in points. ⚠ C527 priced it: at n=88 its sd is
    10.6 points, so the 15-point gate is 1.4 sigma and fails on pure noise 14-16% of the time."""
    halves = []
    for lo, hi in ((0, 0.5), (0.5, 1.0)):
        h = n = 0
        for k in ladder:
            sc = c[k]["scores"]
            part = sc[int(len(sc) * lo):int(len(sc) * hi)]
            h += sum(1 for _, e in part if e)
            n += len(part)
        halves.append(100.0 * h / n if n else 0.0)
    return abs(halves[1] - halves[0])


def k23(paths, select=None):
    """V1/V2/V2b/V3 — where does C525's 20-30 ms region start?

    ⛔ V2 AND V2b are GATES, not findings. V2 asks the older hump to reappear and V2b asks
    C525's own region to reappear on THE ARM IT WAS MEASURED ON (M71); either failing means V1
    is not read for that arm.

    ⭐ `select` is K24's PRE-REGISTERED rule: given more than two runs, score the `select` with
    the lowest split-half |Δ|. ⚠ Not perfectly independent of V1 — the Δ comes from the same
    scores the bands read — which is why it is declared before the capture and disclosed here."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    if select and len(runs) > select:
        arms0 = sorted({a for _, d in runs for a in d})
        lad0 = [k for k in K23_LADDER if k in numeric_cells(runs[0][1][arms0[0]])]
        scored = sorted(runs, key=lambda r: min(_split_half(r[1][a], lad0) for a in r[1]))
        print("## K24 selection rule (PINNED BEFORE THE CAPTURE): score the %d of %d runs with the"
              % (select, len(runs)))
        print("   lowest split-half |delta|. ⚠ Not perfectly independent of V1 — the delta comes")
        print("   from the same scores the bands read. Declared in advance, and disclosed here.")
        for name, d in scored:
            print("   %-24s |delta| %5.1f   %s"
                  % (name, min(_split_half(d[a], lad0) for a in d),
                     "SCORED" if (name, d) in scored[:select] else "not scored"))
        print("")
        runs = scored[:select]
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
        checks = [("V2  (the k15/k16 hump)", K23_ANCHOR)]
        if arm in K23_REGION:
            checks.append(("V2b (C525's region on THIS arm)", K23_REGION[arm]))
        else:
            print("   V2b: not applicable to %s -- C525 REFUTED a region there on this arm"
                  % arm)
            print("        (its 20 ms cell stood alone), so gating it on one would be gating an")
            print("        arm on another arm's finding (M71). V2 is this arm's only continuity")
            print("        gate, and the write-up must say so.")
        for lbl, sel in checks:
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
                  " of 62% this detector fires\n      14% of the time from counting noise"
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


NOTCH_CELLS = ("15", "20", "25")     # the predicted notch and its two ladder neighbours
NOTCH_FRAME = 1.22                   # `gproxii`'s notch, in frames of its own 6144-sample frame
NOTCH_DEPTH = 25.0                   # a notch must sit this far below its neighbours' mean
# ⭐ SIMULATED (40,000 draws, n=8/cell) so the tally can be READ rather than eyeballed:
#   a real notch of `gproxii`'s depth (100/3/100) fires this detector on **100% of caps**;
#   no-notch truths matching the observed profile fire it on 0.6-12.2%, i.e. 0.1-1.3 of 11.
# ⇒ at 11 caps, 2 or fewer hits refutes a notch OF THAT DEPTH and is what noise alone gives.
NOTCH_REAL_RATE = 100.0              # % of caps a gproxii-depth notch fires on
NOTCH_NULL_MAX = 12.2                # worst-case % under a no-notch truth (a flat 76/76/76)


def notch20(paths):
    """Is there a FRAME-LOCKED notch at 20 ms on `indala` and `keri`?

    ⭐ OFFLINE. Opens no serial port and arms nothing. `gproxii`'s 5 ms notch sits at 60 ms =
    1.22 of its 6144-sample frame; 1.22 frames of the 2048-sample frame `indala` and `keri`
    share is **20.0 ms**, so the frame reading predicts a notch there on both arms.

    ⚠⚠ **THE CAPS ARE BANKED, SO THIS IS A RETRODICTION.** The criterion below is written after
    the data existed and that is disclosed rather than glossed. ⭐ **What licenses it anyway is
    the direction**: foreknowledge cannot manufacture the ABSENCE of a collapse that is not in
    the data, which is `framescale.py`'s own standing rule for banked re-analysis. ⛔ A
    SUPPORTED verdict here would be worth very little; only a REFUTED one is honestly carried.
    """
    print("## Is there a frame-locked notch at 20 ms?")
    print("   `gproxii`'s notch is %.2f frames; on a 2048-sample frame that is %.1f ms."
          % (NOTCH_FRAME, NOTCH_FRAME * 2048 * SAMPLE_US / 1000.0))
    print("   ⚠ BANKED CAPS — a retrodiction. Only a REFUTED verdict is honestly carried here.")
    print("   A notch needs the 20 ms cell at least %.0f points below the mean of 15 and 25."
          % NOTCH_DEPTH)
    print("")
    rows = []
    for p in sorted(paths):
        d = json.load(open(p))
        for arm in sorted(d):
            c = d[arm]
            ks = set(numeric_cells(c))
            if not set(NOTCH_CELLS) <= ks:
                continue
            r = [100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"]) for k in NOTCH_CELLS]
            rows.append((os.path.basename(p).replace(".json", ""), arm, r))
    if not rows:
        print("   ⛔ no banked cap carries all of %s\n" % ",".join(NOTCH_CELLS))
        return
    print("   %-26s %-7s %7s %7s %7s  notch?" % ("cap", "arm", *NOTCH_CELLS))
    hits = 0
    for name, arm, r in rows:
        nb = 0.5 * (r[0] + r[2])
        is_notch = r[1] <= nb - NOTCH_DEPTH
        hits += is_notch
        print("   %-26s %-7s %6.0f%% %6.0f%% %6.0f%%  %s"
              % (name, arm, r[0], r[1], r[2], "NOTCH" if is_notch else "no"))
    lowest = min(r[1] for _, _, r in rows)
    means = [sum(r[i] for _, _, r in rows) / len(rows) for i in range(3)]
    print("")
    print("   %d of %d independently seeded measurements show a notch at 20 ms."
          % (hits, len(rows)))
    print("   mean %s = %.0f%% / %.0f%% / %.0f%%; the 20 ms cell never falls below %.0f%%."
          % (" / ".join(NOTCH_CELLS), means[0], means[1], means[2], lowest))
    exp_real = NOTCH_REAL_RATE / 100.0 * len(rows)
    exp_null = NOTCH_NULL_MAX / 100.0 * len(rows)
    print("   ⇒ a notch of `gproxii`'s DEPTH would fire this detector on %.0f of %d caps; "
          "a no-notch" % (exp_real, len(rows)))
    print("     profile gives at most %.1f of %d from counting noise (simulated, 40,000 draws)."
          % (exp_null, len(rows)))
    if hits <= exp_null:
        print("   ⇒ **REFUTED AT `gproxii`'s DEPTH. There is no notch at 20 ms on either arm.**")
        print("     %d of %d is inside what noise alone gives and nowhere near the %.0f a real"
              % (hits, len(rows), exp_real))
        print("     notch demands — and `gproxii`'s own notch scored **2 of 72** against "
              "neighbours")
        print("     at 100%, a shape nothing here comes within reach of. ⛔ A fresh "
              "pre-registered band")
        print("     would re-measure a question the record has answered, which is C473's "
              "method.")
        print("     ⚠ It refutes a notch of THAT depth. A shallower one is not excluded, "
              "and this")
        print("     detector was never built to see one.")
    else:
        print("   ⇒ not refuted — %d hits is above the %.1f noise gives. ⛔ And a "
              "retrodiction cannot" % (hits, exp_null))
        print("     SUPPORT it either; that needs a pre-registered band on fresh seeds.")
    print("")
    print("   ⚠ POST-HOC LEAD, NOT A FINDING: the low cell in this region is 25 ms, not 20.")
    print("     25 ms is %.2f frames, which does NOT match %.2f, so even a *notch at a "
          "different frame" % (25.0 / (2048 * SAMPLE_US / 1000.0), NOTCH_FRAME))
    print("     count* reading does not fit. It needs its own pre-registered band.")
    print("")


K26_LADDER = [str(x) for x in range(10, 201, 5)]      # 39 cells — the full measured span
K26_HIGH = 25.0        # A1: a cell is elevated at median + this
K26_RUN = 3            # ⛔ THREE, not two: a 39-cell ladder gives a 2-cell run 13-26% false-fire
                       # on a flat profile depending on where the median lands on the n=8 grid;
                       # need-3 holds under 0.8% at every median from 12% to 75% (simulated first).
K26_REGIONS = {        # A2: where `indala`/`keri` are HIGH, named before this capture and
    "10-30": ("10", "30"),          # replicated across three independent seed pairs
    "50-65": ("50", "65"),
    "95-105": ("95", "105"),
    "140-150": ("140", "150"),
}
K26_REGIONS_MIN = 2    # A2: at least this many of the four must be found
# ⛔⛔ A2 MATCHES ON A MAJORITY OF A REGION'S CELLS, NOT ON ONE. The first version accepted a
# single cell of overlap, and the break-test showed a WIDE spilling run touching a named span by
# accident was then counted as a match AND escaped being an orphan: A2 false-fired 2.8% on a
# ground truth whose structure was entirely elsewhere. The majority rule takes that to 0.0%.
K26_MAJORITY = True


def k26(paths):
    """A1/A2 — does `idteck` have structure on the lead-time knob, and is it in the same places?

    ⛔ `indala224` is NOT scoreable on this knob and does not appear here: its precision is 0%
    (C502) so *exact* is a flat zero, and its marker `Indala \\(len` is the SAME one `indala`
    uses, so *decoded* cannot tell a 224-bit frame from a 64-bit one — and the demodulator
    reports a nonsense length for it (254-611 against 224, C493/C501), so no length-keyed
    marker rescues it either. There is no rate to put on the y-axis."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    print("## K26 — `idteck` on the lead-time knob")
    print("   probe `lf read -s 6144` = 3 frames, by C499's standing rule and not by a pilot.")
    print("   ⛔ A1 needs a run of %d cells, not 2: over %d cells a 2-cell run false-fires"
          % (K26_RUN, len(K26_LADDER)))
    print("   13-26%% on a FLAT profile depending on the median; need-%d holds under 0.8%%."
          % K26_RUN)
    print("")
    arms = sorted({a for _, d in runs for a in d})
    for arm in arms:
        per = [(n, d[arm]) for n, d in runs if arm in d]
        if len(per) < 2:
            print("### %s ⛔ needs two seeds — %d supplied\n" % (arm, len(per)))
            continue
        # ⭐⭐ M77: THE LADDER'S TOP IS PER ARM, so a correctly designed run may stop short of
        # 200 ms — `nexwatch`'s 98 ms probe puts a 200 ms primer over the burst and P1 failed on
        # exactly that cell, twice (C535). ⇒ accept a SHORTER ladder, but only a strict PREFIX of
        # K26's, identical across every cap, and say so out loud. ⛔ The bands are untouched:
        # A1's threshold, its run length and the both-seeds rule are exactly as committed.
        sets = [set(numeric_cells(c)) & set(K26_LADDER) for _, c in per]
        if len({frozenset(x) for x in sets}) != 1:
            print("### %s ⛔ the caps carry DIFFERENT cell sets — not scored\n" % arm)
            continue
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K26_LADDER if k in cells]
        if ladder != K26_LADDER[:len(ladder)]:
            print("### %s ⛔ the cells are not a contiguous prefix of the 10-200 ms ladder — "
                  "not scored\n" % arm)
            continue
        if len(ladder) < 20:
            print("### %s ⛔ only %d cells — too short to carry A1\n" % (arm, len(ladder)))
            continue
        if len(ladder) < len(K26_LADDER):
            print("### %s ⚠ SHORT LADDER: %d cells, %s..%s ms (of %s..%s). A1's run length was "
                  "set for\n    %d cells; over %d the false-fire rate is LOWER, not higher, so "
                  "the band is conservative\n    here. ⭐ The top was cut by M77's per-arm rule, "
                  "not by a result."
                  % (arm, len(ladder), ladder[0], ladder[-1], K26_LADDER[0], K26_LADDER[-1],
                     len(K26_LADDER), len(ladder)))
        meds, rate_ofs = [], []
        for name, c in per:
            r = lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
            rate_ofs.append(r)
            vals = sorted(r(k) for k in ladder)
            meds.append(0.5 * (vals[len(vals) // 2 - 1] + vals[len(vals) // 2])
                        if len(vals) % 2 == 0 else vals[len(vals) // 2])
        print("### %s   median %s   elevated >= %s"
              % (arm, "/".join("%.0f%%" % m for m in meds),
                 "/".join("%.0f%%" % (m + K26_HIGH) for m in meds)))
        for k in ladder:
            print("   %5s %s %s"
                  % (k, "  ".join("%-11s" % ("%.0f%%" % f(k)) for f in rate_ofs),
                     "".join("▲" if rate_ofs[i](k) >= meds[i] + K26_HIGH else "·"
                             for i in range(len(per)))))
        gated = False
        for name, c in per:
            g, line = _gate(name, c, ladder)
            print(line)
            gated = gated or not g
        if gated:
            print("   ⇒ ⛔ NO VERDICT for %s — a gate failed.\n" % arm)
            continue
        seed_runs = [_runs(ladder, rate_ofs[i], meds[i] + K26_HIGH) for i in range(len(per))]
        seed_runs = [[f for f in rs if len(f) >= K26_RUN] for rs in seed_runs]
        found = []
        for f1 in seed_runs[0]:
            for f2 in seed_runs[1]:
                if set(f1) & set(f2):
                    found.append(sorted(set(f1) | set(f2), key=int))
        print("   A1 (>= %d adjacent cells at median+%.0f, in BOTH seeds): %s"
              % (K26_RUN, K26_HIGH, [",".join(f) for f in found] or "none"))
        print("   ⇒ **A1 %s for %s** — %d region(s)%s"
              % ("FIRES" if found else "REFUTED", arm, len(found),
                 "" if found else ". ⚠ That means *no region as WIDE and as TALL as the other"
                 "\n     arms' regions* (3-5 cells at ~88%), NOT *flat*: at a 3-cell region on a"
                 "\n     38-50% median this detector has only 69-73% power."))
        if not found:
            print("")
            continue
        spans = {lbl: [k for k in ladder if int(lo) <= int(k) <= int(hi)]
                 for lbl, (lo, hi) in K26_REGIONS.items()}

        def inside(f):
            """⛔ A MAJORITY of the region's cells, not one (see K26_MAJORITY)."""
            best = max((len(set(f) & set(sp)) for sp in spans.values()), default=0)
            return (best * 2 >= len(f)) if K26_MAJORITY else (best > 0)

        named = []
        for lbl, (lo, hi) in sorted(K26_REGIONS.items(), key=lambda kv: int(kv[1][0])):
            span = spans[lbl]
            hit = [",".join(f) for f in found
                   if len(set(f) & set(span)) * 2 >= len(f) and set(f) & set(span)]
            named.append((lbl, hit))
            print("   A2 %-9s (%s): %s" % (lbl, ",".join(span), " | ".join(hit) or "no region"))
        matched = sum(1 for _, h in named if h)
        orphans = [",".join(f) for f in found if not inside(f)]
        a2 = (not orphans) and matched >= K26_REGIONS_MIN
        print("   ⇒ **A2 %s for %s** — %d of %d named regions found, %d region(s) outside them all"
              % ("FIRES" if a2 else "REFUTED" if orphans else "no verdict",
                 arm, matched, len(K26_REGIONS), len(orphans)))
        if orphans:
            print("      ⛔ outside: %s ⇒ this arm's structure is its OWN, not the other arms'."
                  % "; ".join(orphans))
        elif a2:
            print("      ⭐ A THIRD ARM in the same places ⇒ the structure is a property of the")
            print("      EMISSION rather than of one reader. ⚠ It shares the 2048-sample frame")
            print("      with both, so it says nothing about frames vs milliseconds (M65/C520).")
            print("      ⛔⛔ AND A2 FIRING MEANS *the structure found is in the named places*,")
            print("      NOT *there is no structure elsewhere*: on a ground truth carrying the")
            print("      named regions PLUS an extra one, A2 still fires ~42% of the time because")
            print("      A1 often fails to resolve the extra region separately. Stated in advance.")
        print("")
    print("⛔ Ungraded — no null sweep, no calibration row. It moves no cell.")
    print("")


INV_HIGH = 62.5       # a cell counts as HIGH at or above this (5 of 8 — the n=8 grid, not a taste)
INV_LOW = 25.0        # and LOW at or below this (2 of 8)
INV_MIN_SEEDS = 2     # a cell is only reported when this many independent caps measure it


def inventory(paths):
    """⭐ M76's FIX, AS A TOOL. Sweep every banked cap and report, per arm and per cell, how
    many independent seeds measured it HIGH and how many LOW.

    ⛔⛔ THIS EXISTS BECAUSE K26's A2 TOOK ITS REFERENCE LIST FROM THE WRITE-UPS. Four regions
    were named because a band had tested and fired on them; 180-195 ms was omitted, and
    `indala` measures 75-100% there across SIX seeds — it was never a *named feature* only
    because it is where C522's W2 failed and K20's X2 passed, two bands asking different
    questions about the same high region. A2's verdict had to be withdrawn in both directions.

    ⇒ A reference list about *where the arms are high* must be derived from every cell of every
    cap, mechanically. That is what this prints. ⛔ It is an INVENTORY and not a band: it makes
    no claim, fires nothing, and its thresholds are the n=8 grid (5/8 and 2/8) rather than a
    choice. ⚠ Cells measured by fewer than INV_MIN_SEEDS caps are printed with their count so a
    thin cell is never mistaken for a solid one."""
    import collections
    seen = collections.defaultdict(lambda: collections.defaultdict(list))
    for p in sorted(paths):
        try:
            d = json.load(open(p))
        except Exception as exc:
            print("   ⛔ %s unreadable: %s" % (os.path.basename(p), exc))
            continue
        for arm, c in d.items():
            for k in numeric_cells(c):
                sc = c[k].get("scores") or []
                if not sc:
                    continue
                # ⭐ `float`, not `int`: K32's ladder is at 2.5 ms, so a cell key can be
                # "52.5". `numeric_cells` was fixed for this and THIS LINE WAS NOT, so
                # `--inventory` raised on the full cap set — exactly the sweep M76 needs.
                seen[arm][float(k)].append(100.0 * cell_rate(sc)[0] / len(sc))
    print("## Region inventory — derived from the caps, not from the write-ups (M76)")
    print("   HIGH is >= %.1f%% (5 of 8) and LOW is <= %.1f%% (2 of 8): the n=8 grid, not a taste."
          % (INV_HIGH, INV_LOW))
    print("   ⛔ An inventory, not a band. It claims nothing and fires nothing.")
    for arm in sorted(seen):
        cells = sorted(seen[arm])
        print("\n### %s   %d cells over %d cap-measurements"
              % (arm, len(cells), sum(len(v) for v in seen[arm].values())))
        print("   %5s %6s %6s %6s  %s" % ("ms", "seeds", "high", "low", "levels"))
        runs_hi, runs_lo, cur_hi, cur_lo = [], [], [], []
        for c in cells:
            v = seen[arm][c]
            hi = sum(1 for x in v if x >= INV_HIGH)
            lo = sum(1 for x in v if x <= INV_LOW)
            mark = ""
            solid_hi = len(v) >= INV_MIN_SEEDS and hi == len(v)
            solid_lo = len(v) >= INV_MIN_SEEDS and lo == len(v)
            if solid_hi:
                mark = " ▲ HIGH in every seed"
                cur_hi.append(c)
            else:
                if len(cur_hi) >= 2:
                    runs_hi.append(list(cur_hi))
                cur_hi = []
            if solid_lo:
                mark = " ▽ LOW in every seed"
                cur_lo.append(c)
            else:
                if len(cur_lo) >= 2:
                    runs_lo.append(list(cur_lo))
                cur_lo = []
            print("   %5s %6d %6d %6d  %s%s"
                  % ("%g" % c, len(v), hi, lo, " ".join("%.0f" % x for x in v), mark))
        if len(cur_hi) >= 2:
            runs_hi.append(list(cur_hi))
        if len(cur_lo) >= 2:
            runs_lo.append(list(cur_lo))
        print("   ⇒ HIGH in EVERY seed, >= 2 adjacent cells: %s"
              % ("; ".join("-".join("%g" % x for x in (r[0], r[-1])) for r in runs_hi) or "none"))
        print("   ⇒ LOW  in EVERY seed, >= 2 adjacent cells: %s"
              % ("; ".join("-".join("%g" % x for x in (r[0], r[-1])) for r in runs_lo) or "none"))
        print("   ⚠ *adjacent* means adjacent IN THIS ARM'S MEASURED CELLS, which are not a")
        print("     uniform grid across caps — a gap of 5 ms and a gap of 20 ms both read as")
        print("     adjacent here. Use the seed counts and the ms column, never the run alone.")
    print("")


K29_ARMS = ("keri", "idteck", "nexwatch")   # decided by ARITHMETIC (C537): all three take one
                                            # forward detector; `indala`'s 75% median puts its
                                            # threshold at the ceiling, so it is scoped out (M68).
K29_LADDER = [str(x) for x in range(10, 196, 5)]        # the common ladder, topped at 195 (M77)
K29_REGIONS = [("R1", "15", "20"), ("R2", "55", "65"), ("R3", "100", "105"),
               ("R4", "140", "145"), ("R5", "180", "190")]   # derived from --inventory (M76)
K29_HIGH = 25.0        # a cell is elevated at that arm's ladder median + this
K29_CELLS = 2          # cells of a region an arm must have elevated, in BOTH seeds, to HIT it
K29_FIRE = 11          # of 15: fires on 4-5 of 5 regions shared (98.8-100%)
K29_REFUTE = 7         # of 15: refutes on <= 2 of 5 shared (100%), and 0.0% false-fire


def k29(paths):
    """B1 — are the lead-time regions the SAME regions across three arms?

    ⛔ The reference regions were derived from the banked caps by `--inventory`, so this may
    NOT be scored on any of them: fresh seeds only. ⛔ And if any arm is gated out, B1 is not
    re-scored over the remaining two — the 15-cell statistic and every simulated figure in the
    K29 section assume three arms."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    print("## K29 — are the regions SHARED across `%s`?" % "`, `".join(K29_ARMS))
    print("   reference derived by `--inventory` from the banked caps (M76), so this run must be")
    print("   on FRESH seeds. ⛔ `indala` is scoped out by arithmetic: its 75%% median puts the")
    print("   forward threshold at the ceiling (C537/M68).")
    print("   B1: total hits over 5 regions x 3 arms, of 15. >= %d fires, <= %d refutes,"
          % (K29_FIRE, K29_REFUTE))
    print("   %d-%d is NO VERDICT and MEANS *about three of the five are shared*." 
          % (K29_REFUTE + 1, K29_FIRE - 1))
    print("")
    if len(runs) != 2:
        print("   ⛔ B1 needs exactly two seeds — %d supplied\n" % len(runs))
        return
    missing = [a for a in K29_ARMS if not all(a in d for _, d in runs)]
    if missing:
        print("   ⛔ both runs must carry all three arms; missing %s\n" % ",".join(missing))
        return
    total, gated = 0, []
    for arm in K29_ARMS:
        per = [(n, d[arm]) for n, d in runs]
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K29_LADDER if k in cells]
        if ladder != K29_LADDER:
            print("### %s ⛔ not the common ladder (%d of %d cells) — not scored\n"
                  % (arm, len(ladder), len(K29_LADDER)))
            gated.append(arm)
            continue
        meds, rate_ofs, ok = [], [], True
        for name, c in per:
            g, line = _gate(name, c, ladder)
            print("   gate %-9s %s" % (arm, line.strip()))
            ok = ok and g
            r = lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
            rate_ofs.append(r)
            vals = sorted(r(k) for k in ladder)
            meds.append(0.5 * (vals[len(vals) // 2 - 1] + vals[len(vals) // 2])
                        if len(vals) % 2 == 0 else vals[len(vals) // 2])
        if not ok:
            print("   ⇒ ⛔ %s is GATED OUT\n" % arm)
            gated.append(arm)
            continue
        print("### %s   median %s   elevated >= %s"
              % (arm, "/".join("%.0f%%" % m for m in meds),
                 "/".join("%.0f%%" % (m + K29_HIGH) for m in meds)))
        for lbl, lo, hi in K29_REGIONS:
            span = [k for k in ladder if int(lo) <= int(k) <= int(hi)]
            per_seed = [[k for k in span if rate_ofs[i](k) >= meds[i] + K29_HIGH]
                        for i in range(2)]
            hit = all(len(x) >= K29_CELLS for x in per_seed)
            total += hit
            print("   %s %-9s %s ⇒ %s"
                  % (lbl, "%s-%s" % (lo, hi),
                     " | ".join(",".join(x) or "none" for x in per_seed),
                     "HIT" if hit else "no"))
        print("")
    if gated:
        print("⇒ ⛔ **NO VERDICT — %s gated out, and B1 is NOT re-scored over the rest**: its "
              "15-cell\n     statistic and every simulated figure assume three arms.\n"
              % ", ".join(gated))
        return
    if total >= K29_FIRE:
        v = ("**B1 FIRES — %d of 15. THE REGIONS ARE SHARED ACROSS ALL THREE ARMS.**\n"
             "     Simulated before the capture: %d+ occurs 98.8-100%% when 4-5 of the 5 regions "
             "are\n     genuinely shared and **0.0%%** when each arm carries independent "
             "structure of its own.\n     ⚠ `nexwatch`'s 4096-sample frame is in it, so this puts "
             "two frame lengths in the same\n     millisecond regions — which C536 already "
             "showed for two of them, so this STRENGTHENS\n     rather than establishes it. "
             "⛔ It says nothing about WHY, and it excludes `indala` by\n     construction."
             % (total, K29_FIRE))
    elif total <= K29_REFUTE:
        v = ("**B1 REFUTED — %d of 15. THE REGIONS ARE NOT SHARED.** Simulated: <= %d occurs "
             "100%%\n     when 2 or fewer of the 5 are shared, and 100%% when each arm carries 3 "
             "random regions\n     of its own.\n"
             "     ⛔⛔ BUT IT DOES NOT DISTINGUISH *each arm has structure of its OWN* from "
             "*no arm\n     has structure at all* — a flat ladder refutes it too. **The per-arm "
             "A1 verdicts are\n     what separate those** (C533 `idteck`, C536 `nexwatch`, C537 "
             "`keri` — all three FIRED),\n     so read this row beside them and never alone."
             % (total, K29_REFUTE))
    else:
        v = ("⛔ **NO VERDICT — %d of 15, inside the pre-registered %d-%d band. IT HAS A STATED\n"
             "     MEANING: about THREE of the five regions are shared** (simulated median 9 for "
             "that\n     truth). ⛔ That is not a failed band — do not re-tune the "
             "thresholds (M74)."
             % (total, K29_REFUTE + 1, K29_FIRE - 1))
    print("⇒ %s" % v)
    print("")
    print("⛔ Ungraded — no null sweep, no calibration row. It moves no cell.")
    print("")


K34_ARMS = ("keri", "idteck")
K34_LADDER = [str(x) for x in range(10, 196, 5)]   # C538's common ladder, unchanged on purpose:
                                                   # it is inside the budget at BOTH bursts (M77 —
                                                   # dec 1 tops are keri 214 / idteck 245 nominal
                                                   # at burst 500, 569 / 600 at burst 1000), so
                                                   # the comparison is like-for-like.
K34_REGIONS = list(K29_REGIONS)   # ⛔ the SAME five, derived by `--inventory` (M76). ⛔ Not re-
                                  # derived for this band: re-deriving a reference on the run that
                                  # tests it is the K26/A2 error M76 exists to stop.
K34_HIGH = 25.0        # K29's elevation rule, unchanged
K34_CELLS = 2          # K29's: cells of a region elevated, in BOTH seeds of that condition
K34_KEEP = 0.75        # D1: the fraction of the burst-500 hits that must come back
K34_NEW_MAX = 2        # D1: hits the long burst may ADD and still count as "the same regions"
K34_FLOOR = 3          # below this many burst-500 hits the CONTROL failed and nothing is judged
K34_REPS = 16          # ⛔ set by simulation, not by taste — see the K34a section in burstsync


def _k34_burst(cap_arm):
    """The burst condition a cap certifies for itself: the DEVICE's frames-per-burst.

    ⛔ Never `declared`, and never a version string — C461 is the note about believing a build
    label instead of the hardware, and this whole band is a comparison of two builds."""
    b = cap_arm.get("_burst") or {}
    return b.get("frames"), b.get("declared")


# ⭐⭐ CALIBRATED ON THE STATISTIC THIS SCORER ACTUALLY COMPUTES, WHICH IS NOT THE ONE C552's
# +60 CAME FROM. C552 derived +60 from the POOLED profile's contrast; this gate reads a PER-CAP
# maximum at reps 8, and a per-cap max is upward-biased by exactly the sampling noise pooling
# removes. Re-simulated on the right statistic (banked K34a caps, fitted cap-sigma, 800 draws):
#
#   gain   D1 power @reps16   P(gate >= +56)   >= +62   >= +68   >= +75
#   1.00        0.8%               57%           48%       6%       3%
#   1.25       14.0%               89%           79%      25%      14%
#   1.50       39.5%               98%           94%      60%      41%
#   2.00       77.2%              100%          100%      92%      69%
#
# ⛔ +60 would have passed a condition whose D1 power is 0.8% about HALF the time — it was
# calibrated on pooled profiles and is simply the wrong number here. **+68 gives 6% false-pass
# at the useless level and 92% pass where K34a would actually work**, which is the gate wanted.
# ⚠ The statistic is coarse at reps 8 (rates quantise to 1/8 and it saturates at +75), so do not
# read fine differences in it; reps 16 sharpens it (4% / 100% at +62) at double the cost, and
# was not taken because 6%/92% is already adequate.
# ⭐ Fixed BEFORE any gate cap existed, from banked caps and simulation only (M55).
GATE_MIN_ELEV = 68.0


def k34gate(paths):
    """⭐⭐⭐⭐ THE FEASIBILITY GATE FOR K34a's RE-RUN (C552/M84). ⛔ NOT A BAND.

    K34a's control failed because the bench was at a level its floor could not clear, and
    re-grounding `k34sim.py` in the day's own caps says so in advance: the floor is met 100% of
    the time at C538's level and 20.8-30.9% at K34a's, and ⛔ **more reps makes it worse and
    plateaus at 11-16% out to reps 128** — the binding variance is BETWEEN caps and reps cannot
    touch it. So the re-run must not be spent blind.

    ⭐⭐ THE STATISTIC IS PEAK ELEVATION, NOT POOLED LEVEL, AND THAT IS THE WHOLE POINT. Across
    the contrast range where D1's power runs 0.8% -> 91.2%, the pooled level moves only
    32.9% -> 38.0% while the largest region elevation moves +40.6 -> +75 points. ⇒ the pooled
    level is nearly blind to the thing that decides the band.

    ⇒ **max, over both arms and all five K29 regions, of (the region's best cell − that arm's
    own ladder median), required in BOTH caps.** ⛔ Both, not pooled and not the better one: one
    cap reaching it is exactly the single-seed evidence K29's two-seed rule exists to refuse.

    ⚠ PASS licenses spending the flash and K34a's four caps THAT SESSION, with the thresholds
    re-derived by `k34sim.py --from` these two caps. It does **not** predict K34a's verdict, it
    fires nothing, it moves no cell, and a FAIL is an honest *the bench is not at a level that
    can answer this* — not a result about the burst."""
    runs = []
    for p in paths:
        try:
            runs.append((os.path.basename(p).replace(".json", ""), json.load(open(p))))
        except Exception as exc:
            print("   ⛔ %s unreadable: %s" % (os.path.basename(p), exc))
            return
    print("## K34a's re-run — the FEASIBILITY GATE (C552/M84). ⛔ Not a band; it fires nothing.")
    print("   statistic: max over `%s` x 5 regions of (region peak − that arm's ladder median)"
          % "`, `".join(K34_ARMS))
    print("   PASS needs >= +%.0f points in BOTH caps. ⛔ Both, not pooled, not the better one."
          % GATE_MIN_ELEV)
    print("   calibrated on THIS statistic at reps 8: 6% pass where D1's power is 0.8%, 92% "
          "where it is 77%.\n")
    if len(runs) != 2:
        print("   ⛔ the gate needs exactly two caps (two fresh seeds) — %d supplied\n" % len(runs))
        return

    per_cap = []
    for name, d in runs:
        best = None
        for arm in K34_ARMS:
            if arm not in d:
                print("   ⛔ %s is missing `%s`\n" % (name, arm))
                return
            c = d[arm]
            plan = c.get("_plan") or {}
            if not plan.get("per_arm_shuffle"):
                print("   ⛔ %s/%s was captured without --per-arm-shuffle (M69)\n" % (name, arm))
                return
            cells = numeric_cells(c)
            if [k for k in K34_LADDER if k in cells] != K34_LADDER:
                print("   ⛔ %s/%s is not the common 38-cell 10-195 ladder\n" % (name, arm))
                return
            frames, declared = _k34_burst(c)
            if frames is None:
                print("   ⛔ %s/%s carries no device-measured frames-per-burst — a build label "
                      "is not evidence (C461)\n" % (name, arm))
                return
            ok, line = _gate(name, c, K34_LADDER)
            print("   gate %-7s %s" % (arm, line.strip()))
            rates = {}
            for k in K34_LADDER:
                h, n = cell_rate(c[k]["scores"])
                rates[k] = 100.0 * h / n if n else 0.0
            v = sorted(rates.values())
            med = 0.5 * (v[len(v) // 2 - 1] + v[len(v) // 2])
            for lbl, lo, hi in K34_REGIONS:
                span = [k for k in K34_LADDER if int(lo) <= int(k) <= int(hi)]
                elev = max(rates[k] for k in span) - med
                if best is None or elev > best[0]:
                    best = (elev, arm, lbl, med, frames)
            print("      %-7s median %5.1f%%   %s" % (
                arm, med, "  ".join(
                    "%s %+5.1f" % (lbl, max(rates[k] for k in K34_LADDER
                                            if int(lo) <= int(k) <= int(hi)) - med)
                    for lbl, lo, hi in K34_REGIONS)))
        per_cap.append((name, best))
        print("   ⇒ %-22s peak elevation **%+.1f** points (%s %s, median %.1f%%, %d frames)\n"
              % (name, best[0], best[1], best[2], best[3], best[4]))

    lowest = min(b[0] for _, b in per_cap)
    if lowest >= GATE_MIN_ELEV:
        print("   ✅ **PASS** — both caps reach +%.0f (weakest %+.1f). K34a's flash and its four "
              "caps are\n      licensed THIS session; re-derive the thresholds with "
              "`k34sim.py --from` these two caps\n      (M80/M83), keep the ABBA order, and "
              "⛔ do not inherit C538's numbers.\n" % (GATE_MIN_ELEV, lowest))
    else:
        print("   ⛔ **REFUSE** — weakest cap is %+.1f against +%.0f. **Do not flash and do not "
              "capture.**\n      The bench is not at a level that can answer K34a; that is a "
              "statement about the\n      bench today, ⛔ NOT a result about the burst. Say so "
              "and stop.\n" % (lowest, GATE_MIN_ELEV))


def k34(paths):
    """D1/D2/D3 — does LENGTHENING THE BURST move the lead-time regions?

    ⛔⛔ K34a IS THE CONTROL FOR K34b AND ONLY A D1 FIRE LICENSES IT. The burst is the phase
    reference this whole line measures lead time FROM (C511/C512), so a longer burst may MOVE
    the structure rather than merely reveal more of it. If it moves, that is the bigger finding
    and K34b is meaningless — see the K34 section in `burstsync.py`.

    Four caps: two seeds at burst 500 and two at burst 1000, same ladder, same arms, dec 1,
    `--per-arm-shuffle`. The caps are sorted into conditions BY WHAT THE DEVICE REPORTED."""
    runs = []
    for p in paths:
        try:
            runs.append((os.path.basename(p).replace(".json", ""), json.load(open(p))))
        except Exception as exc:
            print("   ⛔ %s unreadable: %s" % (os.path.basename(p), exc))
            return
    print("## K34a — does a LONGER BURST move the regions? (`%s`)" % "`, `".join(K34_ARMS))
    print("   reference regions from `--inventory` (M76), the same five K29 used and NOT re-derived")
    print("   here. Elevation, region width and the two-seed rule are K29's, unchanged.")
    print("   D1 SAME  : >= %.0f%% of the burst-500 hits return AND <= %d new ones"
          % (100 * K34_KEEP, K34_NEW_MAX))
    print("   D2 MOVED : <= 1 returns while the long burst still shows >= 2 regions")
    print("   D3 NONE  : the long burst shows no region at all")
    print("   ⛔ Only D1 licenses K34b.\n")

    if len(runs) != 4:
        print("   ⛔ K34a needs exactly four caps (two seeds x two bursts) — %d supplied\n"
              % len(runs))
        return
    # sort into conditions by the DEVICE's own answer
    groups = {}
    for name, d in runs:
        arm0 = next((a for a in K34_ARMS if a in d), None)
        if arm0 is None:
            print("   ⛔ %s carries neither arm\n" % name)
            return
        frames, declared = _k34_burst(d[arm0])
        if frames is None:
            print("   ⛔ %s carries no device-measured frames-per-burst — it cannot certify its\n"
                  "      own burst condition, and a build label is not evidence (C461)\n" % name)
            return
        groups.setdefault(frames, []).append((name, d, declared))
    if len(groups) != 2 or sorted(len(v) for v in groups.values()) != [2, 2]:
        print("   ⛔ the four caps must be two-and-two by MEASURED frames per burst; got %s\n"
              % ", ".join("%d frames x%d" % (k, len(v)) for k, v in sorted(groups.items())))
        return
    lo, hi = sorted(groups)
    if hi <= lo:
        print("   ⛔ the two conditions have the same frames per burst\n")
        return
    print("   conditions: **%d frames** (declared %s) vs **%d frames** (declared %s) — the "
          "device's\n   own answer, so the flash is evidenced rather than asserted"
          % (lo, groups[lo][0][2], hi, groups[hi][0][2]))
    print("")

    hits = {}
    for cond in (lo, hi):
        for arm in K34_ARMS:
            per = []
            for name, d, _ in groups[cond]:
                if arm not in d:
                    print("   ⛔ %s is missing `%s`\n" % (name, arm))
                    return
                c = d[arm]
                plan = c.get("_plan") or {}
                if not plan.get("per_arm_shuffle"):
                    print("   ⛔ %s/%s was captured without --per-arm-shuffle (M69)\n"
                          % (name, arm))
                    return
                cells = numeric_cells(c)
                if [k for k in K34_LADDER if k in cells] != K34_LADDER:
                    print("   ⛔ %s/%s is not the common ladder\n" % (name, arm))
                    return
                g, line = _gate(name, c, K34_LADDER)
                print("   gate %-7s %-5d %s" % (arm, cond, line.strip()))
                if not g:
                    print("   ⇒ ⛔ GATED OUT — K34a is NOT re-scored over what is left\n")
                    return
                per.append(c)
            got = set()
            meds = []
            for c in per:
                vals = sorted(100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
                              for k in K34_LADDER)
                meds.append(0.5 * (vals[len(vals) // 2 - 1] + vals[len(vals) // 2]))
            for lbl, l, h in K34_REGIONS:
                span = [k for k in K34_LADDER if int(l) <= int(k) <= int(h)]
                if all(sum(1 for k in span
                           if 100.0 * cell_rate(per[i][k]["scores"])[0]
                           / len(per[i][k]["scores"]) >= meds[i] + K34_HIGH) >= K34_CELLS
                       for i in (0, 1)):
                    got.add(lbl)
            hits[(cond, arm)] = got
    print("")
    h500 = {(a, r) for a in K34_ARMS for r in hits[(lo, a)]}
    h1000 = {(a, r) for a in K34_ARMS for r in hits[(hi, a)]}
    for arm in K34_ARMS:
        print("### %-7s %d frames: %-22s   %d frames: %s"
              % (arm, lo, ",".join(sorted(hits[(lo, arm)])) or "none",
                 hi, ",".join(sorted(hits[(hi, arm)])) or "none"))
    kept, new = len(h500 & h1000), len(h1000 - h500)
    print("\n   burst-%d hits %d of 10 | returned %d | new %d" % (lo, len(h500), kept, new))

    if len(h500) < K34_FLOOR:
        print("\n⇒ ⛔ **NO VERDICT — THE CONTROL FAILED.** The short burst showed only %d of 10 "
              "regions,\n     under the pre-registered floor of %d. A control that finds too "
              "little structure cannot\n     judge whether it moved. ⛔ This says nothing about "
              "burst length; it is a statement\n     about this run. K34b is NOT licensed.\n"
              % (len(h500), K34_FLOOR))
        return
    if kept >= K34_KEEP * len(h500) and new <= K34_NEW_MAX:
        print("\n⇒ ⭐⭐ **D1 FIRES — THE REGIONS DO NOT MOVE. The burst length is a REACH knob "
              "and nothing\n     else, and that is the ONLY result that licenses K34b.** "
              "Simulated before the capture on\n     the two banked C538 caps' own per-cell "
              "rates: at reps %d this fires **93.6%%** when the\n     profile is genuinely "
              "unchanged, and **0.0%%** under all three ways it could have moved\n     (scaled "
              "with the burst, shifted 20 ms, or flattened) over 10,000 draws each.\n"
              "     ⚠ It does NOT say the structure is unrelated to the burst — only that its "
              "LEAD TIMES\n     are unchanged when the burst doubles." % K34_REPS)
    elif kept <= 1 and len(h1000) >= 2:
        print("\n⇒ ⭐⭐⭐ **D2 FIRES — THE REGIONS MOVED, AND THIS IS THE BIGGER FINDING.** The "
              "structure is\n     referenced to the BURST, which is upstream of both the reader "
              "and the field — so it\n     retires the *reader-or-field* branch C538 opened. "
              "⛔⛔ **K34b is meaningless and must\n     NOT be run.**\n"
              "     ⚠ Read the SCALED check below before naming HOW it moved: this band detects "
              "that the\n     regions are elsewhere, and is only 33%% powered to fire on a "
              "scaled profile and 0.1%% on a\n     20 ms shift — most real moves land in NO "
              "VERDICT, not here.")
    elif len(h1000) == 0:
        print("\n⇒ ⛔ **D3 — NO REGION SURVIVES THE LONGER BURST.** ⚠⚠ **THIS BAND CANNOT TELL "
              "*abolished*\n     FROM *moved out of the window*, and the simulation says so: it "
              "fires 100%% when the\n     profile is genuinely flattened but also 7.5%% when it "
              "merely SCALED with the burst and\n     1.6%% on a 20 ms shift.** ⇒ report it as "
              "*moved-or-abolished*, never as *abolished*, and\n     read the SCALED check "
              "below. ⛔ K34b is NOT licensed either way.")
    else:
        print("\n⇒ ⛔ **NO VERDICT — %d of %d regions returned with %d new, outside every band. "
              "IT HAS A\n     STATED MEANING: the structure PARTLY survives** — the burst "
              "changes which regions reach\n     threshold without simply preserving or "
              "relocating them. ⛔ That is not a failed band and\n     the thresholds must not "
              "be re-tuned (M74). ⛔ **K34b is NOT licensed**: only D1 licenses it."
              % (kept, len(h500), new))

    # ⭐ DESCRIPTIVE, PRE-REGISTERED AS DESCRIPTIVE AND NOT A BAND (M74): if the structure is a
    # fixed FRACTION of the burst, every region sits at 2x its burst-500 lead time. Two of the
    # five land on this ladder. It is printed for every outcome so it can never be reached for
    # only when the band disappoints.
    print("\n   SCALED check (descriptive, NOT a band): if lead time scaled with the burst, the")
    print("   regions would sit at 2x — R1 at 30-40 and R2 at 110-130. Cells elevated at %d "
          "frames:" % hi)
    for arm in K34_ARMS:
        per = [d[arm] for _, d, _ in groups[hi]]
        meds = []
        for c in per:
            vals = sorted(100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"])
                          for k in K34_LADDER)
            meds.append(0.5 * (vals[len(vals) // 2 - 1] + vals[len(vals) // 2]))
        out = []
        for lbl, l, h in (("R1x2", 30, 40), ("R2x2", 110, 130)):
            e = [k for k in K34_LADDER if l <= int(k) <= h
                 and all(100.0 * cell_rate(per[i][k]["scores"])[0]
                         / len(per[i][k]["scores"]) >= meds[i] + K34_HIGH for i in (0, 1))]
            out.append("%s %s" % (lbl, ",".join(e) or "none"))
        print("      %-7s %s" % (arm, "   ".join(out)))
    print("")
    print("⛔ Ungraded — no null sweep, no calibration row. It moves no cell.")
    print("")


K30_LADDER = ["10", "15", "20", "25", "30", "35", "40", "45", "60", "65",
              "90", "95", "100", "105", "110", "115", "120"]
# ⛔⛔ K31 MOVED THIS FROM 62.5 TO 50.0, AND `--reps` FROM 8 TO 16 (C542/M80). 62.5 came from
# the dec-1 level; the dec-2 profile runs ~30% lower pooled, cell medians 12% and 25%, so the bar
# asked for a height this condition rarely reaches and K30 returned NO VERDICT with every gate
# passing. ⭐ Simulated against the MEASURED level, reps 16 at 50% is the only setting good in
# every row: 0.00-0.03% at those medians, 0.42% against a ONE-cell spike (at reps 8 that is 10%,
# because a 25% background reaches 50% by noise 11% of the time) and 97% power at a 75% region.
# ⛔ MORE REPS, not a lower bar, is what fixed it: the problem was quantisation at n=8.
# ⚠ K30's own caps are the PILOT and are scored at 62.5 by the git history, not by this constant.
K30_HIGH = 50.0        # ABSOLUTE — a 17-cell targeted ladder has no honest median
K30_RUN = 2            # the regions NARROW by 1.653 when decimated: 1.6-2.2 cells, so not 3
K30_ELAPSED = ["30", "35", "40"]      # R2 moved (33-39)
K30_SAMPLES = ["100", "105"]          # R3 unmoved
K30_ELAPSED2 = ["110", "115"]         # R5 moved (108.9-114.9) — supporting, not deciding
K30_AMBIG = ["105", "110"]            # one cell apart: supports neither, reported


def _runs2(cells, hot, need):
    out, cur = [], []
    for k in cells:
        if k in hot:
            cur.append(k)
        else:
            if len(cur) >= need:
                out.append(cur)
            cur = []
    if len(cur) >= need:
        out.append(cur)
    return out


def k30(paths):
    """D1 — is the lead time a DURATION or a SAMPLE COUNT?

    ⛔ The detector is ABSOLUTE and its run length is 2, not 3: decimation divides a region's
    width in nominal ms by 1.653, so the regions being looked for are 1.6-2.2 cells wide and
    K26's A1 could not see them. ⛔ Both no-verdict cases have their meaning pinned in the K30
    section of `burstsync.py`; do not invent a third."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    print("## K30 — is the lead time a DURATION or a SAMPLE COUNT?")
    print("   ELAPSED ⇒ a region at %s and NOT at %s. SAMPLES ⇒ the reverse."
          % (",".join(K30_ELAPSED), ",".join(K30_SAMPLES)))
    print("   ⛔ absolute threshold %.1f%% (5 of 8), run of %d — the regions NARROW by 1.653x "
          "when decimated." % (K30_HIGH, K30_RUN))
    print("")
    if len(runs) != 2:
        print("   ⛔ needs exactly two seeds — %d supplied\n" % len(runs))
        return
    arms = sorted({a for _, d in runs for a in d})
    for arm in arms:
        per = [(n, d[arm]) for n, d in runs if arm in d]
        if len(per) < 2:
            print("### %s ⛔ needs both seeds\n" % arm); continue
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K30_LADDER if k in cells]
        if ladder != K30_LADDER:
            print("### %s ⛔ not the K30 ladder (%d of %d) — not scored\n"
                  % (arm, len(ladder), len(K30_LADDER))); continue
        rate_ofs = [(lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"]))
                    for _, c in per]
        print("### %s" % arm)
        for k in ladder:
            print("   %5s %s %s"
                  % (k, "  ".join("%-11s" % ("%.0f%%" % f(k)) for f in rate_ofs),
                     "".join("▲" if f(k) >= K30_HIGH else "·" for f in rate_ofs)))
        bad = False
        for name, c in per:
            g, line = _gate(name, c, ladder)
            print(line)
            bad = bad or not g
        if bad:
            print("   ⇒ ⛔ NO VERDICT for %s — a gate failed.\n" % arm); continue
        hot = [{k for k in ladder if f(k) >= K30_HIGH} for f in rate_ofs]
        def region(sel):
            rs = [_runs2([k for k in ladder if k in sel], hot[i], K30_RUN) for i in (0, 1)]
            return bool(rs[0]) and bool(rs[1]), rs
        e1, re1 = region(set(K30_ELAPSED))
        s1, rs1 = region(set(K30_SAMPLES))
        e2, re2 = region(set(K30_ELAPSED2))
        amb, _ = region(set(K30_AMBIG))
        for lbl, sel, got in (("ELAPSED  (R2 moved)", K30_ELAPSED, e1),
                              ("SAMPLES  (R3 unmoved)", K30_SAMPLES, s1),
                              ("elapsed2 (R5 moved, supporting)", K30_ELAPSED2, e2)):
            print("   %-32s %s ⇒ %s"
                  % (lbl, ",".join(sel),
                     "REGION" if got else "no region"))
        if amb and not s1 and not e2:
            print("   ⚠ AMBIGUOUS: a region spans %s, one cell across the R3/R5 boundary — it "
                  "supports\n     NEITHER and is reported, not counted." % ",".join(K30_AMBIG))
        if e1 and not s1:
            v = ("**D1 — ELAPSED. The lead time is a DURATION, not a sample count.**\n"
                 "     A region appears where the *moved* prediction puts it and NOT where the "
                 "*unmoved*\n     one does. ⇒ a purely CLIENT-side reading of this line is "
                 "excluded.%s"
                 % ("\n     ⭐ And R5's moved position carries a region too, which is supporting "
                    "and was\n     not required." if e2 else ""))
        elif s1 and not e1:
            v = ("**D1 — SAMPLES. The regions sit at a fixed `-s N`, not at a fixed duration.**\n"
                 "     ⛔ This does NOT by itself make the whole line client-side: the PROBE's own "
                 "read is\n     untouched by this band, and only the primer's knob was tested.")
        elif e1 and s1:
            v = ("⛔ **NO VERDICT — regions at BOTH**, which is the pre-registered branch meaning "
                 "*the knob\n     changes something neither hypothesis describes*. Report both and "
                 "do not choose.")
        else:
            v = ("⛔ **NO VERDICT — regions at NEITHER**, the pre-registered branch meaning *the "
                 "decimated\n     profile carries nothing this detector can see*. ⚠ Read the "
                 "gates and the 60-65 cells\n     before calling it an absence: the detector needs "
                 "a region as tall as ~88%% to be 96%%\n     sure of seeing it, and only 62%% sure "
                 "at 75%%.")
        print("   ⇒ %s\n" % v)
    print("⛔ Ungraded — no null sweep, no calibration row. It moves no cell.")
    print("")


K32_LADDER = ["52.5", "55", "57.5", "60", "62.5", "65",
              "97.5", "100", "102.5", "105", "107.5", "110", "112.5", "115", "117.5"]
K32_SAMPLES = ["100", "102.5", "105"]      # R3 unmoved
K32_ELAPSED = ["110", "112.5"]             # R5 moved (108.9-114.9)
K32_SEP = "107.5"                          # empty under BOTH — the separator 5 ms lacked
K32_LOW_SAMPLES = ["55", "57.5"]           # R2 unmoved, supporting only
K32_LOW_BOTH = ["60", "62.5"]              # the R2/R3 collision, reported only


def k32(paths):
    """E1/E2 — at 2.5 ms, which prediction does the decimated profile match?

    ⛔ The threshold and reps are K31's, unchanged: the dec-2 level is measured twice and
    nothing about it moved, so re-deriving them would be fitting. ⛔ A region covering the
    separator cell closes this approach rather than leaving it open — say so, do not retry."""
    runs = [(os.path.basename(p).replace(".json", ""), json.load(open(p))) for p in paths]
    print("## K32 — the decimation test at 2.5 ms (C541's requirement, dropped by K30/K31 — M81)")
    print("   SAMPLES ⇒ a region in %s and none in %s. ELAPSED ⇒ the reverse."
          % (",".join(K32_SAMPLES), ",".join(K32_ELAPSED)))
    print("   ⛔ %s is EMPTY under BOTH predictions — a region covering it means the two are not"
          % K32_SEP)
    print("   separated even at 2.5 ms, which CLOSES this approach.")
    print("")
    if len(runs) != 2:
        print("   ⛔ needs exactly two seeds — %d supplied\n" % len(runs)); return
    for arm in sorted({a for _, d in runs for a in d}):
        per = [(n, d[arm]) for n, d in runs if arm in d]
        if len(per) < 2:
            print("### %s ⛔ needs both seeds\n" % arm); continue
        cells = numeric_cells(per[0][1])
        ladder = [k for k in K32_LADDER if k in cells]
        if ladder != K32_LADDER:
            print("### %s ⛔ not the K32 ladder (%d of %d) — not scored\n"
                  % (arm, len(ladder), len(K32_LADDER))); continue
        rate_ofs = [(lambda k, c=c: 100.0 * cell_rate(c[k]["scores"])[0] / len(c[k]["scores"]))
                    for _, c in per]
        print("### %s" % arm)
        for k in ladder:
            print("   %7s %s %s"
                  % (k, "  ".join("%-11s" % ("%.0f%%" % f(k)) for f in rate_ofs),
                     "".join("▲" if f(k) >= K30_HIGH else "·" for f in rate_ofs)))
        bad = False
        for name, c in per:
            g, line = _gate(name, c, ladder)
            print(line); bad = bad or not g
        if bad:
            print("   ⇒ ⛔ NO VERDICT for %s — a gate failed.\n" % arm); continue
        hot = [{k for k in ladder if f(k) >= K30_HIGH} for f in rate_ofs]
        regions = []
        for f1 in _runs2(ladder, hot[0], 2):
            for f2 in _runs2(ladder, hot[1], 2):
                if set(f1) & set(f2):
                    regions.append(sorted(set(f1) | set(f2), key=float))
        print("   regions (>= 2 adjacent at %.0f%% in BOTH seeds): %s"
              % (K30_HIGH, [",".join(r) for r in regions] or "none"))
        straddles = [r for r in regions if K32_SEP in r]
        s1 = [r for r in regions if set(r) & set(K32_SAMPLES) and K32_SEP not in r]
        e1 = [r for r in regions if set(r) & set(K32_ELAPSED) and K32_SEP not in r]
        lo_s = [r for r in regions if set(r) & set(K32_LOW_SAMPLES)]
        lo_b = [r for r in regions if set(r) & set(K32_LOW_BOTH)]
        if straddles:
            v = ("⛔⛔ **NO VERDICT — a region COVERS the separator %s** (%s). That is the "
                 "pre-registered\n     outcome meaning *the two predictions are not separated "
                 "even at 2.5 ms*, and it\n     **CLOSES this approach** rather than leaving it "
                 "open. ⛔ Do not retry at a finer grid." % (K32_SEP, straddles[0]))
        elif s1 and not e1:
            v = ("**E1 — SAMPLES. The regions sit at a fixed `-s N`.** ⛔ This does NOT by itself "
                 "make\n     the line client-side: only the PRIMER's knob was tested and the "
                 "probe's own read is\n     untouched.")
        elif e1 and not s1:
            v = ("**E1 — ELAPSED. The lead time is a DURATION.** ⇒ a purely CLIENT-side reading "
                 "of this\n     line is excluded.")
        elif e1 and s1:
            v = "⛔ **NO VERDICT — regions in BOTH windows**, which neither prediction allows."
        else:
            v = ("⛔ **NO VERDICT — no region in either window.** ⚠ Read the gates first: at this "
                 "n the\n     detector is 97% sure of a 2-cell region at 75% and only 65% sure "
                 "at 62%.")
        print("   ⇒ %s" % v)
        print("   E2 low zone (supporting, never deciding): R2-unmoved %s ⇒ %s | collision %s ⇒ %s"
              % (",".join(K32_LOW_SAMPLES), "region" if lo_s else "none",
                 ",".join(K32_LOW_BOTH), "region (reported, supports neither)" if lo_b else "none"))
        if lo_s and e1 and not s1:
            print("      ⚠⚠ E2 and E1 DISAGREE — E2 never overrides E1, so this is a NO VERDICT "
                  "and is\n      said so rather than resolved.")
        print("")
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
    ap.add_argument("--k32", nargs="+", metavar="CAP",
                    help="⭐ K32: two dec-2 caps on the 2.5 ms 15-cell ladder")
    ap.add_argument("--k30", nargs="+", metavar="CAP",
                    help="⭐ K30: two dec-2 caps on the targeted 17-cell ladder")
    ap.add_argument("--gate", nargs="+", metavar="CAP",
                    help="⭐ C552: K34a's re-run FEASIBILITY GATE — two caps, peak region "
                         "elevation, PASS licenses the flash. ⛔ Not a band.")
    ap.add_argument("--k34", nargs="+", metavar="CAP",
                    help="⭐ K34a: four caps — two seeds at burst 500 and two at burst 1000, "
                         "sorted into conditions by the frames-per-burst the DEVICE reported")
    ap.add_argument("--k29", nargs="+", metavar="CAP",
                    help="⭐ K29: two fresh caps, each carrying keri+idteck+nexwatch on the "
                         "common 10-195 ms ladder")
    ap.add_argument("--inventory", nargs="+", metavar="CAP",
                    help="⭐ M76: sweep every cap and report per-cell HIGH/LOW counts per arm, "
                         "so a reference list is derived from the data and not the write-ups")
    ap.add_argument("--k26", nargs="+", metavar="CAP",
                    help="⭐ K26: `idteck` over the 10-200 ms ladder, two seeds")
    ap.add_argument("--notch20", nargs="+", metavar="CAP",
                    help="⭐ offline: is there a frame-locked notch at 20 ms? (banked caps)")
    ap.add_argument("--k24", nargs="+", metavar="CAP",
                    help="⭐ K24: K23's bands over THREE seeds, scoring the two with the lowest "
                         "split-half |delta| — the rule is pinned in burstsync's docstring")
    ap.add_argument("--k22", nargs="+", metavar="CAP",
                    help="⭐ K22: the 20-80 ms ladder, two seeds, both arms")
    ap.add_argument("--k21", nargs="+", metavar="CAP",
                    help="⭐ K21: runs carrying BOTH arms, captured with --per-arm-shuffle")
    ap.add_argument("--k20", nargs="+", metavar="CAP",
                    help="⭐ score K20's X1/X2 — `indala`'s NOTCHES, the inverse detector (M68)")
    a = ap.parse_args()
    if a.gate:
        k34gate(a.gate)
    if a.k34:
        k34(a.k34)
        return 0
    if a.k32:
        k32(a.k32)
        return 0
    if a.k30:
        k30(a.k30)
        return 0
    if a.k29:
        k29(a.k29)
        return 0
    if a.inventory:
        inventory(a.inventory)
        return 0
    if a.k26:
        k26(a.k26)
        return 0
    if a.notch20:
        notch20(a.notch20)
        return 0
    if a.k24:
        k23(a.k24, select=2)
        return 0
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
