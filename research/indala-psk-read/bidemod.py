#!/usr/bin/env python3
"""Host ASK/BIPHASE demodulator — the fourth line coding on this bench.

    ./bidemod.py /tmp/gp/gp_d7.bin --preamble FAC2 --bits 96 --want fac2a38c...

⭐ WHY A SEPARATE TOOL FROM askdemod.py. Manchester and biphase differ in exactly one rule and
it is the rule that decides every bit: in Manchester a MATCHED half-bit pair is an encoding
violation, in biphase it is a legal value. Bolting a flag onto the Manchester decoder would
put that fork inside the hot loop of a decoder already verified on four protocols. This runs
first, on the host, so the air layer is established before any firmware is written — the thing
that made Keri's and NexWatch's family claims measurements rather than assumptions (C157,C164).

⛔⛔ THE FRONT END IS THE FIRMWARE'S, NOT A RESEMBLANCE OF IT — 256-sample BLOCK MEANS, copied
from lf_slicer.c, never a trailing average. `framedrift.py` used a 512-sample trailing average
where the firmware used block means and the two then disagreed about which captures contained
a frame at all (C180). A trailing average lags by half its width; block means do not lag.

⛔ THE HALF-BIT WIDTH IS SEARCHED, NOT ASSUMED, and that is deliberate rather than lazy. The
Proxmark calls GProxII "RF/64" and demods it with `ASKbiphaseDemod(0, 64, ...)`, whose pairing
step then HALVES the bit count — which cannot also give a 96-bit frame out of 96 stored T5577
bits. One of those two readings of "RF/64" is wrong and the capture is the only thing that can
say which. ⇒ sweep {16, 32, 64} and let the known credential pick.
"""
import sys

BLOCK = 256          # lf_slicer.c: LF_SLICER_BLOCK


def load16(path):
    raw = open(path, "rb").read()
    return [(raw[2 * i] << 8) | raw[2 * i + 1] for i in range(len(raw) // 2)]


def build_dc(x):
    """Per-block means, exactly lf_slicer_build_dc()."""
    dc = []
    for b in range((len(x) + BLOCK - 1) // BLOCK):
        blk = x[b * BLOCK:(b + 1) * BLOCK]
        dc.append(sum(blk) // len(blk))
    return dc


def level(x, dc, i, lp):
    """lf_slicer_level(): lp 1 is raw, 3 averages with both neighbours."""
    if lp <= 1 or i == 0 or i + 1 >= len(x):
        v = x[i]
    else:
        v = (x[i - 1] + x[i] + x[i + 1]) // 3
    return 1 if v > dc[i >> 8] else 0


def half_bits(x, dc, half, phase, lp):
    """Level at the CENTRE of each half-bit. Run lengths are never consulted — the duty
    bias that corrupts them leaves the centre level intact (C141, C145)."""
    out = []
    i = phase
    while i + half <= len(x):
        out.append(level(x, dc, i + half // 2, lp))
        i += half
    return out


def biphase(halves, pairing, inv):
    """⛔ THE ONE RULE THAT IS NOT MANCHESTER'S: a matched pair is a VALUE here, not a
    violation. Mirrors BiphaseRawDecode(): differ -> 1^invert, same -> invert."""
    bits = []
    for i in range(pairing, len(halves) - 1, 2):
        bits.append((1 ^ inv) if halves[i] != halves[i + 1] else inv)
    return bits


def transition_bits(x, phase, frac, bit=64, w=8):
    """⭐⭐ THE DECODER THAT ACTUALLY WORKS AT RF/64: ask whether the envelope STEPPED at the
    middle of each bit, never what level it is sitting at.

    ⛔ WHY THE LEVEL PATH CANNOT DO THIS, measured rather than asserted. The LF front end is
    AC-coupled with a time constant of about 27 samples: a step to 14112 decays to 4664 in 30
    samples. At RF/32 a half-bit is 16 samples, so the level is still there when you sample it
    — which is why Gallagher, Securakey and Noralsy read off `lf_ask_manchester.c`. At RF/64 a
    half-bit is 32 samples, LONGER than that time constant, so the level has decayed to the
    baseline before the half-bit ends and there is nothing left to slice. Sampling half-bit
    centres on this capture finds no frame at any phase, low-pass or inversion.

    ⭐ Biphase gives the escape: there is a transition at EVERY bit boundary by construction,
    and an extra one mid-bit for one of the two symbol values. So the boundaries are a built-in
    amplitude reference — the median boundary step is what a transition looks like in THIS
    capture — and the only question left per bit is whether the middle stepped too. One
    decision per bit instead of two, each against a self-calibrating threshold.

    ⇒ `bit = 1` iff a mid-bit transition. Exact on the bench credential, 96 of 96 (C204).
    """
    n = len(x)
    xx = x + x

    def slope(t):
        t += n
        return sum(xx[t:t + w]) / w - sum(xx[t - w:t]) / w

    nb = n // bit
    edges = sorted(abs(slope(phase + bit * k)) for k in range(nb))
    if not edges:
        return ""
    th = frac * edges[len(edges) // 2]
    return "".join("1" if abs(slope(phase + bit * k + bit // 2)) > th else "0"
                   for k in range(nb))


def find_transition_frame(x, want_bits, nbits, bit=64):
    """Sweep phase, threshold fraction and inversion; return the best (matched, hex, params)."""
    best = None
    for phase in range(bit):
        for frac in (0.2, 0.3, 0.4, 0.5, 0.6, 0.7):
            s = transition_bits(x, phase, frac, bit)
            if len(s) < nbits:
                continue
            for inv in (0, 1):
                si = "".join(str(int(c) ^ inv) for c in s)
                d = si + si
                for rot in range(len(si)):
                    cand = d[rot:rot + nbits]
                    if len(cand) < nbits:
                        break
                    m = sum(1 for a, b in zip(cand, want_bits) if a == b)
                    if best is None or m > best[0]:
                        best = (m, "%0*x" % (nbits // 4, int(cand, 2)), phase, frac, inv, rot)
    return best


def find_frame(bits, pre, nbits):
    pb = len(pre)
    for i in range(len(bits) - nbits):
        if bits[i:i + pb] == pre:
            return bits[i:i + nbits], i
    return None, None


def main():
    args = sys.argv[1:]
    pre_hex, nbits, want = "FAC2", 96, None
    if "--preamble" in args:
        pre_hex = args.pop(args.index("--preamble") + 1); args.remove("--preamble")
    if "--bits" in args:
        nbits = int(args.pop(args.index("--bits") + 1)); args.remove("--bits")
    if "--want" in args:
        want = args.pop(args.index("--want") + 1).lower(); args.remove("--want")
    pre = [(int(pre_hex, 16) >> (len(pre_hex) * 4 - 1 - i)) & 1 for i in range(len(pre_hex) * 4)]

    for path in args:
        x = load16(path)
        dc = build_dc(x)
        hits = []
        for half in (16, 32, 64):
            for lp in (1, 3, 5):
                for phase in range(0, half, max(1, half // 16)):
                    hs = half_bits(x, dc, half, phase, lp)
                    for pairing in (0, 1):
                        for inv in (0, 1):
                            bits = biphase(hs, pairing, inv)
                            frame, pos = find_frame(bits, pre, nbits)
                            if frame is None:
                                continue
                            h = "%0*x" % (nbits // 4, int("".join(map(str, frame)), 2))
                            hits.append((half, lp, phase, pairing, inv, pos, h))
        name = path.split("/")[-1]
        if not hits:
            print(f"  {name:<18} no frame")
            continue
        good = [h for h in hits if want and h[6] == want]
        show = good or hits
        half, lp, phase, pairing, inv, pos, h = show[0]
        tag = "✓ WANTED" if good else ("⚠ other  " if want else "         ")
        print(f"  {name:<18} {tag} half={half} lp={lp} ph={phase} pair={pairing} "
              f"inv={inv} pos={pos}  {h}   ({len(good)} of {len(hits)} settings exact)")


if __name__ == "__main__":
    main()
