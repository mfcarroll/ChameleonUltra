#!/usr/bin/env python3
"""Score a PAC capture by BIT ERROR RATE against the tag's known frame, not pass/fail.

⭐ WHY THIS EXISTS, and it is the whole lesson. Every decoder variant tried against these
captures returned the same thing: "-". Nothing decoded, so nothing could be ranked, and four
rounds of changes to spike handling, thresholds, debouncing and glitch policy all looked
identical — because a verdict has one bit of information and the question needed more.

⇒ Write a KNOWN frame to the tag (`lf pac clone --cn CD4F5552`, raw
`FF2049906D8511C593155B56D5B2649F`) and the same experiments become a graded score. Measured
over three captures with a full threshold x bit-phase sweep for BOTH arms: averaging 32 samples
per bit gives 8, 15 and 18 errors of 128; a majority VOTE of the same per-sample decisions
gives 6, 8 and 6. A single 16380-count ringing spike moves a 32-sample mean by 512 counts and a
rank statistic does not notice it at all (M33).

⚠ **Neither decodes, and that is the finding.** PAC carries no error correction — the preamble,
twelve UART parities and an XOR checksum all have to land — so 6 errors is as useless as 60.
⛔ The random baseline for this metric is **47** errors (best window of a random bit stream), so
6 is real signal and not a scoring artefact; the information is on the air and the level path
cannot finish the job.

    ./pacber.py caps/pac/pac_r0.bin [--truth FF20...]

Capture format: `lf sniff --bits 16` — two bytes per sample, big-endian 14-bit.
"""
import argparse
import sys

import numpy as np

DEFAULT_TRUTH = "FF2049906D8511C593155B56D5B2649F"
BIT_SAMPLES = 32
FRAME_BITS = 128


def truth_bits(hexstr):
    return np.array([int(b) for b in bin(int(hexstr, 16))[2:].zfill(FRAME_BITS)], dtype=int)


def best_errors(bits, truth):
    """Fewest mismatches over window start and polarity.

    ⭐ No separate rotation loop. The frame repeats every 128 bits, so SHIFTING the window is
    already rotating it — the earlier version did both and was O(n*128) for an answer that is
    O(n). ⚠ Both polarities are still needed: which NRZ level means 1 is not fixed, and pac.c
    itself tries the frame and its inverse for exactly that reason."""
    b = np.asarray(bits, dtype=np.int8)
    if len(b) < FRAME_BITS + 1:
        return FRAME_BITS
    w = np.lib.stride_tricks.sliding_window_view(b, FRAME_BITS)
    d = np.abs(w - truth[None, :]).sum(axis=1)
    return int(min(d.min(), (FRAME_BITS - d).max() and (FRAME_BITS - d.max())))


def bits_mean(w, thr):
    """What pac.c's ancestors do: average the bit's samples, then threshold."""
    return (w.mean(axis=1) >= thr).astype(int)


def bits_vote(w, thr):
    """Per-sample decision, then majority. Immune to spikes; the mean is not."""
    return ((w >= thr).mean(axis=1) >= 0.5).astype(int)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("captures", nargs="+")
    ap.add_argument("--truth", default=DEFAULT_TRUTH)
    a = ap.parse_args()
    truth = truth_bits(a.truth)
    print(f"  truth {a.truth}  ({FRAME_BITS} bits). 0 errors = a decode.\n")
    for path in a.captures:
        x = np.fromfile(path, dtype=">u2").astype(float)
        rail_lo = float((x < 0x39).mean())
        rail_hi = float((x >= 16380).mean())
        print(f"  {path}  {len(x)} samples   "
              f"bottom rail {rail_lo:.1%}, top rail {rail_hi:.1%}")
        for name, fn in (("mean-of-32", bits_mean), ("majority vote", bits_vote)):
            best, where = FRAME_BITS, None
            for thr in (1000, 1500, 2000, 3000, 4000, 6000, 8000):
                for off in range(BIT_SAMPLES):
                    n = (len(x) - off) // BIT_SAMPLES
                    if n < FRAME_BITS + 1:
                        continue
                    w = x[off:off + n * BIT_SAMPLES].reshape(n, BIT_SAMPLES)
                    e = best_errors(fn(w, thr), truth)
                    if e < best:
                        best, where = e, (thr, off)
            print(f"      {name:14} {best:3d}/{FRAME_BITS} errors ({best / FRAME_BITS:5.1%})"
                  f"   best at threshold {where[0]}, bit phase {where[1]}"
                  + ("   ⭐ DECODES" if best == 0 else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
