#!/usr/bin/env python3
"""Stack captures, and ask whether the PSK POLARITY survives to the ADC at all.

    ./stack.py [--dir caps/inputtest] [--node ain5] [--lpf 12000]

⭐ WHAT THIS FOUND. A synthetic PSK1 frame injected into the REAL measured empty-field
noise decodes at 0/31 data-bit errors — at HALF the tag's own measured fc/2 amplitude,
from a SINGLE capture. The real tag, at the same amplitude in the same noise, gets 6-7/31
and never improves with averaging. So the deficit is not SNR, not the noise, and not the
demodulator: what reaches the converter has the right fc/2 AMPLITUDE and the wrong PHASE
STRUCTURE.

⛔ THREE MEASURES THAT LOOK LIKE PROGRESS AND ARE NOT. Each cost a wrong conclusion here:

  1. FOLDING AT 2048 SAMPLES CANCELS THE DATA. a0000000e6bd0e92 has 19 ones — ODD — so
     PSK1's running polarity inverts at every frame boundary and the true repetition
     period is 4096 samples, not 2048. Folding at 2048 averages a frame against its own
     inverse. The transition SKIRT survives it (transitions fall at the same sample
     positions either way), which is why band SNR rose as sqrt(N) to 6.22x while the bit
     errors never moved a single bit.

  2. THE fc/2 BAND-SNR CRITERION IS POLARITY-BLIND. It measures the modulation skirt at
     57.5-62.3kHz, deliberately offset from the 62.5kHz bin because BPSK suppresses its
     own carrier and 62.5kHz is exactly Nyquist. Skirt energy is TRANSITION energy. It
     passed the 5.50x threshold with zero bits recovered.

  3. SCORING ALL 64 BITS CREDITS A CONSTANT RUN. Half the Indala frame is the 28-zero
     preamble — constant polarity — so matching it is free, and against 64 cyclic
     rotations up to 32 bits come for nothing. Score the 31 CREDENTIAL bits only. Doing
     that turns "52/64 vs a 45/64 null" into 6/31 for the tag against 4/31 for the null:
     the null is BETTER.

⇒ Every verdict here is scored on data bits, against the empty field at matched depth.
"""
import argparse
import glob
import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mfdemod as M                                    # noqa: E402

FS, SETTLE, BIT, FRAME = 125000.0, 400, 32, 2048
TRUTH_BITS = [(M.TRUTH >> (63 - i)) & 1 for i in range(64)]
ROTS = [tuple(TRUTH_BITS[k:] + TRUTH_BITS[:k]) for k in range(64)]
DATA_BITS = set(range(33, 64))          # the credential; 0..32 is the preamble


def load16(p):
    r = np.frombuffer(open(p, "rb").read(), dtype=np.uint8)
    if len(r) < 2 or len(r) % 2 or r[0::2].max() > 0x3F:
        return None
    return (r[0::2].astype(np.uint16) << 8 | r[1::2]).astype(float)[SETTLE:]


def lowpass(b, fc):
    """⚠ The demodulator has no selectivity of its own. After mixing by (-1)^n the data
    sits near DC and everything that was originally 1-5kHz sits at 57.5-61.5kHz, where a
    32-sample boxcar rejects only ~2.8%. At ~316 counts of carrier ripple that is ~9
    counts of leakage against a ~10-count signal."""
    if not fc:
        return b
    S = np.fft.rfft(b)
    S[np.fft.rfftfreq(len(b), 1 / FS) > fc] = 0
    return np.fft.irfft(S, len(b))


def sideband(x):
    y = x - x.mean()
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w))
    fr = np.fft.rfftfreq(len(y), 1 / FS)
    m = (fr >= 57500) & (fr <= 62300)
    return 2 * np.sqrt((S[m] ** 2).sum()) / w.sum()


def score(v, fc):
    """Brute force every bit position and every cyclic rotation, and report errors in the
    31 CREDENTIAL bits. Returns (all64, data31)."""
    b = lowpass(M.baseband(v), fc)
    best = (99, 99)
    for pos in range(max(1, len(b) - 64 * BIT)):
        pol = np.where(b[pos:pos + 64 * BIT].reshape(64, BIT).sum(1) >= 0, 1, -1)
        bits = [1] + [1 if pol[k] != pol[k - 1] else 0 for k in range(1, 64)]
        for k, r in enumerate(ROTS):
            d = sum(x != y for x, y in zip(bits, r))
            inv = d > 32
            if inv:
                d = 64 - d
            dd = sum((bits[i] ^ (1 if inv else 0)) != r[i]
                     for i in range(64) if ((i + k) % 64) in DATA_BITS)
            if (dd, d) < (best[1], best[0]):
                best = (d, dd)
    return best


def psk_frame(n, amp):
    """A continuous PSK1 stream, polarity carried ACROSS frame boundaries — which for an
    odd-parity word means it inverts every frame, exactly as the tag does."""
    pol, p, out = M.bits_to_polarity(TRUTH_BITS), None, []
    p = pol[-1]
    while len(out) < n // BIT + 64:
        for b in TRUTH_BITS:
            if b:
                p = -p
            out.append(p)
    s = np.repeat(np.array(out, float), BIT)[:n]
    return s * np.tile([1.0, -1.0], n // 2 + 1)[:n] * amp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default="caps/inputtest")
    ap.add_argument("--node", default="ain5")
    ap.add_argument("--lpf", type=float, default=12000.0)
    a = ap.parse_args()

    def grab(state):
        ps = sorted(glob.glob("%s/%s_%s_r*.bin" % (a.dir, state, a.node)))
        xs = [load16(p) for p in ps]
        if not xs or any(x is None for x in xs):
            sys.exit("⛔ %s/%s_%s_r*.bin missing or not 16-bit — refusing to guess."
                     % (a.dir, state, a.node))
        return xs

    T, E = grab("tag"), grab("empty")
    n = min(min(len(x) for x in T), min(len(x) for x in E))
    T = np.array([x[:n] for x in T])
    E = np.array([x[:n] for x in E])
    N = min(len(T), len(E))
    T, E = T[:N], E[:N]
    print(" %d tag + %d empty captures x %d samples, node %s, LPF %gHz\n"
          % (N, N, n, a.node, a.lpf))

    print("=" * 84)
    print(" CONTROL — sample-exact lock. ⚠ Skip the settle window: the field turn-on")
    print(" transient is identical in every capture and correlates at r>0.94 on its own,")
    print(" which says nothing about the signal.")
    print("=" * 84)
    for lab, A in (("tag", T), ("EMPTY", E)):
        out = []
        for i in range(1, N):
            x, y = M.baseband(A[0]), M.baseband(A[i])
            best = max(((abs(float(np.dot(x[max(L, 0):len(x) + min(L, 0)],
                                          y[max(-L, 0):len(y) - max(L, 0)])
                              / np.sqrt(np.dot(x, x) * np.dot(y, y)))), L)
                        for L in range(-8, 9)))
            out.append("%+d/%.2f" % (best[1], best[0]))
        print("  %-6s vs r0: %s" % (lab, " ".join(out)))

    print("\n" + "=" * 84)
    print(" ZERO-OFFSET STACKING — the polarity-preserving method (NOT folding at 2048)")
    print("=" * 84)
    print(" %3s | %9s %9s %10s | %9s %11s" %
          ("N", "tag fc/2", "empty", "tag/empty", "tag data", "empty data"))
    for k in range(1, N + 1):
        t, e = T[:k].mean(0), E[:k].mean(0)
        print(" %3d | %9.2f %9.2f %9.2fx | %6d/31 %8d/31"
              % (k, sideband(t), sideband(e), sideband(t) / sideband(e),
                 score(t, a.lpf)[1], score(e, a.lpf)[1]))
    print(" chance is ~15.5 errors in 31 bits; 0 is a decode.")

    print("\n" + "=" * 84)
    print(" ⭐ THE DECIDING TEST — a KNOWN PSK1 frame injected into the REAL noise")
    print("=" * 84)
    print(" Same noise, same amplitude. If this decodes and the tag does not, the")
    print(" deficit is not SNR — the phase structure is gone before the converter.")
    tag_sb = float(np.median([sideband(x) for x in T]))
    k_amp = tag_sb / sideband(psk_frame(n, 1.0))
    print(" tag fc/2 %.2f counts -> a matching synthetic frame needs amp %.2f\n" % (tag_sb, k_amp))
    print(" %-18s %8s %10s %11s %10s" % ("injected", "amp", "fc/2", "tag/empty", "data/31"))
    for mult, lab in ((1.0, "= tag"), (0.5, "0.5x tag"), (0.25, "0.25x tag")):
        for k in (1, N):
            noise = E[:k].mean(0)
            v = noise + psk_frame(n, k_amp * mult)
            d = score(v, a.lpf)[1]
            print(" %-18s %8.2f %10.2f %10.2fx %7d/31%s"
                  % ("%s N=%d" % (lab, k), k_amp * mult, sideband(v),
                     sideband(v) / sideband(noise), d, "  *** DECODE ***" if d == 0 else ""))
    print("\n real tag for comparison:")
    for k in (1, N):
        t, e = T[:k].mean(0), E[:k].mean(0)
        print(" %-18s %8s %10.2f %10.2fx %7d/31"
              % ("real tag N=%d" % k, "-", sideband(t), sideband(t) / sideband(e),
                 score(t, a.lpf)[1]))


if __name__ == "__main__":
    main()
