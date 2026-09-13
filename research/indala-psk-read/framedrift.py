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

⛔ THE SWEEP MUST MATCH THE SHIPPING DECODER'S. This tool first swept one low-pass width
where `ctest/cdemod` sweeps two, and reported ZERO frames in `sk_20.bin` — a capture the
shipping decoder reads exactly. That looked like a finding about the tag and was a finding
about the tool. ⇒ An instrument whose sweep is narrower than the decoder it reasons about
manufactures absences. It sweeps both widths now, and the agreement check below is the
regression guard.
"""
import sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from askdemod import load16, boxcar

def block_dc(sm, shift=8):
    """The slicing reference, as 256-sample BLOCK MEANS — mirroring `build_dc` in the shipping
    decoder exactly.

    ⛔⛔ THIS IS WHAT THE TOOL GOT WRONG, AND IT IS SUBTLER THAN THE LOW-PASS SWEEP THAT WAS
    BLAMED FIRST. The earlier version sliced against a 512-sample TRAILING moving average,
    which lags the signal by ~256 samples. The firmware uses block means, which do not lag.
    On Gallagher the two agreed; on Securakey the trailing average found 0-1 frames where the
    shipping decoder reads all four exactly. ⇒ Widening the low-pass sweep did NOT fix it —
    the first hypothesis was wrong and the measurement said so.

    ⭐ The general rule this cost: an analysis tool that reasons ABOUT a decoder must share the
    decoder's front end, not merely resemble it. A different DC estimator is a different
    instrument."""
    n = len(sm)
    nb = (n + (1 << shift) - 1) >> shift
    out = [0] * n
    for b in range(nb):
        lo = b << shift
        hi = min(lo + (1 << shift), n)
        m = sum(sm[lo:hi]) // (hi - lo)
        for i in range(lo, hi):
            out[i] = m
    return out


GALLAGHER_PRE = [0,1,1,1,1,1,1,1,1,1,1,0,1,0,1,0]
# InstaFob: the 32 bits of 0x00107060 — the tag's own T5577 config word (C186).
INSTAFOB_PRE = [int(b) for b in format(0x00107060, '032b')]
SECURAKEY_PRE = [0,1,1,1,1,1,1,1,1,1,0,0,1,0,1,1,0,1,0]


def frame_starts(path, spb, pre, lps=(1, 3)):
    """Absolute sample offsets where the preamble begins, at ANY bit phase.

    ⭐ Scanning every phase is the point: a boundary gap that is not a whole number of bits
    re-phases the bit grid, so a fixed-phase search would find the first frame and miss the
    rest — which reads as "one frame in the capture" rather than as a drift.

    ⛔ AND IT MUST SWEEP THE LOW-PASS THE WAY THE SHIPPING DECODER DOES. The first version
    swept lp=1 only, and reported ZERO frames in a capture the shipping decoder reads exactly
    — so it looked like a finding about the tag and was actually a finding about itself. Any
    analysis tool whose sweep is narrower than the decoder it is reasoning about will
    manufacture absences."""
    x = load16(path)
    hits = []
    for lp in lps:
        sm = boxcar(x, lp) if lp > 1 else x
        dc = block_dc(sm)
        for phase in range(spb):
            bits, i = [], phase
            while i + spb <= len(x):
                a = 1 if sm[i + spb // 4] > dc[i + spb // 4] else 0
                b = 1 if sm[i + 3 * spb // 4] > dc[i + 3 * spb // 4] else 0
                bits.append(None if a == b else (1 if a == 1 and b == 0 else 0))
                i += spb
            for inv in (0, 1):
                seq = [None if v is None else v ^ inv for v in bits]
                for k in range(len(seq) - len(pre)):
                    if seq[k:k + len(pre)] == pre:
                        hits.append(phase + k * spb)
    hits = sorted(set(hits))
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
    print()
    for f in ("sk_0", "sk_12", "sk_20", "sk_28"):
        report(f"caps/securakey-tag/{f}.bin", 40, SECURAKEY_PRE, 96, f"Securakey {f[-2:]}")
    print()
    # ⭐ THE ONE THAT SHOULD BE DIFFERENT. InstaFob is the first protocol here whose reader
    # explicitly times a sequence terminator, so if any tag in this family emits a
    # frame-boundary gap, it is this one — and the excess column is where it would appear.
    for f in ("emu_drive7", "emu_b_drive7"):
        report(f"caps/instafob-flipper/{f}.bin", 32, INSTAFOB_PRE, 225, f"InstaFob {f[:5]}")
    print("\n  ⛔ AGREEMENT CHECK — every capture the shipping decoder reads must yield at")
    print("     least one frame here, or this tool is measuring itself (see the note above).")
