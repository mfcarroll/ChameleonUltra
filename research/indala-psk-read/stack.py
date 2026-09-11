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


# ⭐ Precomputed rotation tables. Scoring is a 2048 x 64 x 64 comparison and the loop
# version took seconds per capture, which is unaffordable once a phase sweep calls it 64
# times. Vectorised here, and checked against the loop version on real captures.
_ROTS = np.array(ROTS, dtype=np.int8)                                   # 64 rotations x 64 bits
_DATA_MASK = np.array([[((i + k) % 64) in DATA_BITS for i in range(64)]
                       for k in range(64)])                             # which bits are credential
_N_DATA = _DATA_MASK.sum(1)                                             # 31 for every rotation


def score(v, fc):
    """Brute force every bit position and every cyclic rotation, and report errors in the
    31 CREDENTIAL bits. Returns (all64, data31).

    ⛔ SCORE THE CREDENTIAL, NOT THE FRAME. Bits 0-32 are the preamble, and 28 of them are
    a constant-polarity run — matching a constant is free, and against 64 rotations that
    is up to 32 bits of unearned score. Reading the all-64 number is how "52/64 against a
    45/64 null" got written down as detection when the null was actually winning."""
    b = lowpass(M.baseband(v), fc)
    n_pos = len(b) - 64 * BIT
    if n_pos < 1:
        return (99, 99)
    # sliding 32-sample boxcar at every offset, then gather the 64 bit-integrators per
    # position in one indexing step
    c = np.concatenate([[0.0], np.cumsum(b)])
    box = c[BIT:] - c[:-BIT]                                   # box[i] = sum(b[i:i+BIT])
    idx = np.arange(n_pos)[:, None] + BIT * np.arange(64)[None, :]
    pol = np.where(box[idx] >= 0, 1, -1)                       # n_pos x 64
    bits = np.empty_like(pol)
    bits[:, 0] = 1
    bits[:, 1:] = (pol[:, 1:] != pol[:, :-1]).astype(pol.dtype)
    ne = bits[:, None, :] != _ROTS[None, :, :]                 # n_pos x 64 x 64
    d_all = ne.sum(-1)
    inv = d_all > 32                                           # differential decode is
    d_all = np.where(inv, 64 - d_all, d_all)                   # polarity-blind
    d_dat = (ne & _DATA_MASK[None, :, :]).sum(-1)
    d_dat = np.where(inv, _N_DATA[None, :] - d_dat, d_dat)
    # rank by credential errors first, then by the whole frame
    flat = np.argmin(d_dat * 100 + d_all)
    i, j = np.unravel_index(flat, d_dat.shape)
    return (int(d_all[i, j]), int(d_dat[i, j]))


def psk_frame(n, amp):
    """A continuous PSK1 stream: the subcarrier PHASE is the bit, carried across frame
    boundaries exactly as the tag sends it.

    ⛔ THIS FUNCTION WAS BROKEN AND CARRYING THE RETRACTED BUG. Until it was fixed it read
    `M.bits_to_polarity(...)` — a name deleted when that running XOR was identified as PSK2
    rather than PSK1 — and its body still flipped the polarity on every '1'. So it raised
    AttributeError on every call, which is the lucky outcome: had the name survived the
    rename it would have gone on silently generating a PSK2 stream for a PSK1 decoder, in
    the one file whose job is to INJECT a known signal into real noise. An injection
    control that injects the wrong modulation cannot fail visibly — it just reports that
    the thing is undetectable.

    ⭐ The parity effect is real and is what the tile below preserves: a0000000e6bd0e92 has
    19 ones, so under PSK1 the running polarity arrives at the next frame inverted and the
    true repetition period is 4096 samples, not 2048 (C14). Nothing here restarts per
    frame. generality.py's psk1_stream() is the same construction, parameterised by word."""
    reps = int(np.ceil(n / (64 * BIT))) + 2
    pol = np.where(np.tile(np.asarray(TRUTH_BITS), reps) > 0, 1.0, -1.0)
    s = np.repeat(pol, BIT)[:n]
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
