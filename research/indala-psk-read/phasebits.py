#!/usr/bin/env python3
"""Sweep the SAADC sample phase and count DECODES. The Chameleon Ultra reads Indala.

    ./phasebits.py [--step 4] [--repeats 5] [--timeout 300] [--keep DIR]

⭐ WHAT THIS ESTABLISHED, with the tag on the FRONT (the reading side — see the placement
banner in FINDINGS.md). 114 of 160 single 300ms captures decode a0000000e6bd0e92 EXACTLY
(71%), and the empty field produced a frame AT ALL 0 times in 160. No stacking, no folding.
Phase is TWO WORKING BANDS — ticks 0-56 and 96-124, every one of them 5/5 — split by a dead
band at 60-92. On the back the same sweep gives 51/160 and only one band.

⛔ DO NOT SCORE THIS SWEEP BY THE fc/2 SKIRT. Measured on these same captures, the skirt is
UNCORRELATED with decoding: it peaks at tick 44 (212, decodes 5/5) and bottoms at ticks
116-120 (134, also 5/5), while the dead band sits in the middle of that range. Over a phase
sweep the skirt is transition energy, not coupling. This is why the other sweep scripts in
this directory were retired rather than ported (FINDINGS C38).

⛔⛔ THE DEAD BAND LIES — it does not merely fail. At ticks 60-92 the decoder returns the
SAME wrong card number on every capture: tick 64 gives a0000000b5af0b92 5/5, tick 88 gives
a0000000c6b90c92 5/5. Those frames are not aligned to the data and their weakest bit
integrator cancels to near zero, which is what the firmware's straddle gate rejects. Two
independent captures AGREE on the wrong word, so the acceptance rule cannot catch it, and no
phase in 60-92 may ever enter PHASE_ROTATION (FINDINGS C39, C40, C48).
⛔ Do not read the OFFSET column as a signature: these won at 16 and 22 while true frames won
at 9, 10 and 12, but a second Indala tag decodes correctly at offset 22 on hardware. The
offset is the tag's frame timing, not a correctness signal (C51).

⛔ THE ORIGINAL HYPOTHESIS HERE WAS WRONG, AND SO WAS EVERYTHING IT WAS BUILT ON. This
script was written to test whether the sample phase sat at a polarity null. It does not.
The reason nothing decoded was that mfdemod.py demodulated PSK2 against a PSK1 tag — in
PSK1 the phase IS the data, and it was differential-decoding. See mfdemod.py
polarity_from_bits(). "31.2 dB of analog deficit" and "7.6 dB short" were never real; about
26 dB of that was the tag being on the wrong side of the device.

⚠ Load captures with mfdemod.load16, NOT stack.load16. The latter drops the first 400
samples as "settle", and that discard alone takes this from 107/160 to 0/160: it removes
the first frame's preamble and leaves too few bits after the second. The turn-on transient
needs no discarding here — baseband() moves it to fs/2 and the low-pass removes it.

⚠ READ THE EMPTY COLUMN. Brute-forcing 31 bits over 2048 positions x 64 rotations finds a
low score in pure noise — the empty field lands around 4-5 errors. A tag number only means
something against the null beside it, at the same phase.
"""
import argparse
import os
import subprocess
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mfdemod as M                                        # noqa: E402
from stack import score, sideband, load16, SETTLE          # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
CU = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", "cu.py"))
PY_ = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", ".venv", "bin", "python"))
LPF = 12000.0


def capture_path(out, state, phase, rep):
    """One helper for writing AND reading. In oversample_test.py two positional %d in the
    same template drifted apart and a verdict was read off nan."""
    return "%s/%s_p%03d_r%d.bin" % (out, state, phase, rep)


def coherence(xs):
    """Median pairwise lag-0 baseband correlation. Stacking at zero offset is only valid
    while the captures stay frame-locked to field-on; if this collapses at some phase the
    stack for that phase is meaningless and its bit score is noise."""
    if len(xs) < 2:
        return float("nan")
    bs = [M.baseband(x) for x in xs]
    rs = [float(np.dot(bs[0], b) / np.sqrt(np.dot(bs[0], bs[0]) * np.dot(b, b)))
          for b in bs[1:]]
    return float(np.median(rs))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--step", type=int, default=4, help="phase step in ticks (4 = 11.25 deg)")
    ap.add_argument("--repeats", type=int, default=5, help="captures per phase, stacked")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--keep", default="caps/phasebits")
    ap.add_argument("--analyse-only", action="store_true",
                    help="re-analyse an existing --keep directory without capturing")
    a = ap.parse_args()
    phases = list(range(0, 128, a.step))
    out = a.keep
    os.makedirs(out, exist_ok=True)

    print(__doc__.split("⭐ THE HYPOTHESIS")[0])
    if not a.analyse_only:
        print(" %d phases x %d repeats x 2 states = %d captures, roughly %.0f min\n"
          % (len(phases), a.repeats, len(phases) * a.repeats * 2,
               len(phases) * a.repeats * 2 * 1.8 / 60))

    def run(state, label):
        input("  %s, then press RETURN... " % label)
        cmds = ["hw mode -r"]
        for p in phases:
            for r in range(a.repeats):
                cmds.append("lf sniff --timeout %d --bits 16 --phase %d --out %s"
                            % (a.timeout, p, capture_path(out, state, p, r)))
        proc = subprocess.run([PY_, CU] + cmds, capture_output=True, text=True)
        if proc.returncode != 0:
            sys.exit("cu.py failed:\n" + (proc.stdout or "") + (proc.stderr or ""))

    if not a.analyse_only:
        run("empty", "Remove EVERY tag from the antenna")
        run("tag", "Place the INDALA26 tag (a0000000e6bd0e92) on the LF antenna")

    def stack_at(state, p):
        xs = []
        for r in range(a.repeats):
            fp = capture_path(out, state, p, r)
            if not os.path.exists(fp):
                sys.exit("⛔ missing capture %s — aborted rather than guessed." % fp)
            x = load16(fp)
            if x is None:
                sys.exit("⛔ %s is not 16-bit — is the firmware current?" % fp)
            xs.append(x)
        n = min(len(x) for x in xs)
        xs = [x[:n] for x in xs]
        return np.array(xs).mean(0), xs

    print("\n" + "=" * 86)
    print(" PHASE vs DECODE RATE — single captures, no stacking")
    print(" ⛔ The empty column is the null: a single hit there sinks the result.")
    print("=" * 86)
    print(" %6s %7s | %-9s %-9s | %9s | %s" %
          ("ticks", "deg", "tag", "empty", "tag fc/2", "near-misses"))
    rows, th, eh, tn, en = [], 0, 0, 0, 0

    def load_raw(state, p, r):
        fp = capture_path(out, state, p, r)
        if not os.path.exists(fp):
            sys.exit("⛔ missing capture %s — aborted rather than guessed." % fp)
        x = M.load16(fp)          # ⚠ M.load16, NOT stack.load16: see below
        if x is None:
            sys.exit("⛔ %s is not 16-bit — is the firmware current?" % fp)
        return x

    for p in phases:
        h, bad = 0, []
        for r in range(a.repeats):
            d = M.demod(load_raw("tag", p, r))
            tn += 1
            if d and d["word"] == M.TRUTH:
                h += 1
                th += 1
            elif d:
                bad.append(d["hex"])
        e = 0
        for r in range(a.repeats):
            d = M.demod(load_raw("empty", p, r))
            en += 1
            if d and d["word"] == M.TRUTH:
                e += 1
                eh += 1
        sk = sideband(np.array([load_raw("tag", p, r)[SETTLE:]
                                for r in range(a.repeats)]).mean(0))
        rows.append((p, h, e))
        print(" %6d %6.1f° | %-9s %-9s | %9.2f | %s" %
              (p, p * 360.0 / 128, "%d/%d" % (h, a.repeats), "%d/%d" % (e, a.repeats),
               sk, " ".join(bad[:2])))

    print("\n" + "=" * 86)
    print(" VERDICT")
    print("=" * 86)
    print(" TAG   : %d / %d single captures decoded %016x EXACTLY (%.0f%%)"
          % (th, tn, M.TRUTH, 100.0 * th / max(tn, 1)))
    print(" EMPTY : %d / %d  <- the null" % (eh, en))
    if eh:
        print("\n ⛔ THE NULL FIRED. The empty field produced the truth, so the decoder is")
        print("   finding it in noise and every tag number above is worthless. Stop here.")
        return
    win = [p for p, h, e in rows if h >= 1]
    best = [p for p, h, e in rows if h >= max(1, a.repeats - 1)]
    if not win:
        print("\n ⇒ No phase decoded. Check the tag is present and carries the Indala")
        print("   credential (lf indala reader -> a0000000e6bd0e92).")
        return
    print("\n best phases (>=%d/%d): %s" % (a.repeats - 1, a.repeats,
                                            ", ".join(str(p) for p in best)))
    print(" any decode at all   : %d..%d ticks (%.0f-%.0f deg), %.0f%% of the range"
          % (min(win), max(win), min(win) * 360.0 / 128, max(win) * 360.0 / 128,
             100.0 * (max(win) - min(win)) / 128))
    print("\n ⚠ Near-misses are usually one or two bits in the zero run. Indala carries a")
    print("   parity (the Proxmark prints it), so checking it would reject most of them.")
    print("\n captures in %s" % out)


if __name__ == "__main__":
    main()
