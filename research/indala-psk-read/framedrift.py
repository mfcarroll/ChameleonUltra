#!/usr/bin/env python3
"""Measure a REAL T5577's frame-to-frame period against its nominal frame length.

    ./framedrift.py                 # the committed Gallagher and Securakey captures

⭐ WHY THIS EXISTS. Every emulate arm in this project is verified by a Flipper reading rig A.
That is a genuine independent decoder, and it is NOT the same evidence as a real tag (C179):
a T5577 may do something at the frame boundary that our plain looping emission does not
reproduce, and one reader that happens not to care cannot detect it. The sequence terminator
is the known candidate — Gallagher's and Securakey's configs both set the ST bit.

⛔ THE QUESTION THIS ANSWERS, AND THE ONE IT DOES NOT. It measures where successive frame
preambles START, in samples, on a capture of the real tag. If the spacing equals the nominal
frame length exactly, the tag emits no extra gap and a plain looping emission matches it
there. It does NOT prove the waveforms are identical — only that the frame boundary carries
no displacement.

⚠ KNOWN LIMITATION, AND IT IS WHY THIS FILE DOES NOT REPORT A SECURAKEY NUMBER: on
caps/securakey-tag/sk_20.bin this finds no frame at all, while the SHIPPING decoder
(ctest/cdemod --securakey) reads it exactly. The shipping decoder sweeps two low-pass widths
and this sweeps one, which is the likely cause but is not confirmed. ⇒ An instrument that
disagrees with a known-good result on a known-good input is not evidence about the subject,
it is evidence about itself. Fix the disagreement before quoting any Securakey period.
"""
import sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from askdemod import load16, boxcar

GALLAGHER_PRE = [0,1,1,1,1,1,1,1,1,1,1,0,1,0,1,0]
SECURAKEY_PRE = [0,1,1,1,1,1,1,1,1,1,0,0,1,0,1,1,0,1,0]


def frame_starts(path, spb, pre):
    """Absolute sample offsets where the preamble begins, at ANY bit phase.

    ⭐ Scanning every phase is the point: a boundary gap that is not a whole number of bits
    re-phases the bit grid, so a fixed-phase search would find the first frame and miss the
    rest — which reads as "one frame in the capture" rather than as a drift."""
    x = load16(path)
    dc = boxcar(x, 512)
    hits = []
    for phase in range(spb):
        bits, i = [], phase
        while i + spb <= len(x):
            a = 1 if x[i + spb // 4] > dc[i + spb // 4] else 0
            b = 1 if x[i + 3 * spb // 4] > dc[i + 3 * spb // 4] else 0
            bits.append(None if a == b else (1 if a == 1 and b == 0 else 0))
            i += spb
        for inv in (0, 1):
            seq = [None if v is None else v ^ inv for v in bits]
            for k in range(len(seq) - len(pre)):
                if seq[k:k + len(pre)] == pre:
                    hits.append(phase + k * spb)
    hits.sort()
    merged = []
    for s in hits:
        if merged and s - merged[-1] < spb // 2:
            continue
        merged.append(s)
    return merged


def report(path, spb, pre, frame_bits, label):
    starts = frame_starts(path, spb, pre)
    nominal = frame_bits * spb
    if len(starts) < 2:
        print(f"  {label:16} {len(starts)} frame start(s) — NOT ENOUGH TO MEASURE A PERIOD")
        return
    gaps = [starts[i + 1] - starts[i] for i in range(len(starts) - 1)]
    excess = [g - nominal for g in gaps]
    print(f"  {label:16} starts {starts[:5]}")
    print(f"  {'':16} spacing {gaps[:4]} samples   nominal {nominal}   excess {excess[:4]}")


if __name__ == "__main__":
    print("REAL T5577 FRAME PERIOD vs NOMINAL (C179)")
    for f in ("gal_d7_0", "gal_d7_12", "gal_d7_20", "gal_d7_28"):
        report(f"caps/gallagher-tag/{f}.bin", 32, GALLAGHER_PRE, 96, f"Gallagher {f[-2:]}")
    print("\n  ⚠ Securakey omitted — see the limitation note at the top of this file.")
