#!/usr/bin/env python3
"""Host ASK/Manchester demodulator for the biphase family — Gallagher first.

    ./askdemod.py caps/gallagher-tag/gal_*.bin --preamble 7FEA --bits 96

⭐ WHY THIS EXISTS, AND WHY IT IS NOT THE PSK HARNESS. `ctest/cdemod` compiles the shipping
PSK decoder; there is no shipping ASK decoder yet, and the whole point of running the host
first is to establish the AIR LAYER before any firmware is written. That is what made Keri's
and NexWatch's family claims measurements rather than assumptions (C157, C164).

⛔ ASK IS A DIFFERENT INSTRUMENT FROM PSK AND THE PROJECT'S SCARS DO NOT ALL TRANSFER.
PSK1 here rides a constant-amplitude fc/2 subcarrier, so amplitude carries nothing and the
mixer is the whole front end. ASK puts the data IN the amplitude, which means:
  - the saturation that destroys PAC (C140) is a live threat here, not a hypothetical;
  - but Manchester guarantees a transition every bit, so no run lasts long enough for the
    AC coupling to decay — which is exactly why em410x, Viking and Jablotron work and PAC
    does not (C124).
⚠ C145 refuted the comparator/edge route for PAC on this front end (77-92% sub-bit glitches).
That was measured on NRZ; this decoder therefore low-passes BEFORE slicing rather than
trusting raw edges, which is the one thing C145 found did fix the glitches.
"""
import sys

def load16(path):
    raw = open(path, "rb").read()
    return [(raw[2*i] << 8) | raw[2*i+1] for i in range(len(raw)//2)]

def boxcar(x, k):
    if k <= 1:
        return x[:]
    out, acc = [], 0
    for i, v in enumerate(x):
        acc += v
        if i >= k:
            acc -= x[i-k]
        out.append(acc / min(i+1, k))
    return out

def runs_from(x, k_lp):
    """Low-pass, slice at the running median, and return (level, length) runs."""
    sm = boxcar(x, k_lp)
    # ⚠ A GLOBAL threshold is wrong on a capture with drift; a long boxcar is the local DC.
    dc = boxcar(sm, 512)
    lv = [1 if sm[i] > dc[i] else 0 for i in range(len(sm))]
    out, cur, n = [], lv[0], 0
    for v in lv:
        if v == cur:
            n += 1
        else:
            out.append((cur, n)); cur, n = v, 1
    out.append((cur, n))
    return out

def manchester_bits(runs, spb):
    """Manchester: a run is half a bit or a whole bit. Emit the half-bit stream, then pair.

    ⭐ Decoding the HALF-BIT stream first and pairing afterwards is what makes this robust to
    the run-length bias C145 measured: a mis-sized run costs one half-bit, not a resync."""
    half = spb / 2.0
    halves = []
    for lvl, n in runs:
        q = int(round(n / half))
        if q < 1:
            q = 1
        if q > 4:                      # a run longer than two bits is not Manchester
            halves.extend([lvl] * 4)
            continue
        halves.extend([lvl] * q)
    return halves


def pair_halves(halves, phase):
    """Pair adjacent half-bits into bits: 10 -> 1, 01 -> 0, and a matched pair is illegal.

    ⛔ THE PAIRING PHASE MUST BE SEARCHED, NOT ASSUMED. The half-bit stream begins wherever
    the capture begins, which is mid-bit half the time; pairing from index 0 regardless then
    makes EVERY pair a matched (illegal) pair and the decode fails completely rather than
    degrading. That is exactly how this decoder failed on its first run against a capture
    whose run lengths were already measurably correct."""
    bits = []
    for i in range(phase, len(halves) - 1, 2):
        a, b = halves[i], halves[i+1]
        bits.append(None if a == b else (1 if a == 1 and b == 0 else 0))
    return bits

def find_frame(bits, preamble_bits, frame_bits):
    pb = len(preamble_bits)
    for inv in (0, 1):
        seq = [None if b is None else b ^ inv for b in bits]
        for i in range(len(seq) - frame_bits):
            if seq[i:i+pb] == preamble_bits and None not in seq[i:i+frame_bits]:
                return seq[i:i+frame_bits], i, inv
    return None, None, None

def main():
    args = [a for a in sys.argv[1:]]
    pre_hex, nbits = "7FEA", 96
    if "--preamble" in args:
        pre_hex = args.pop(args.index("--preamble")+1); args.remove("--preamble")
    if "--bits" in args:
        nbits = int(args.pop(args.index("--bits")+1)); args.remove("--bits")
    pre = [(int(pre_hex, 16) >> (len(pre_hex)*4-1-i)) & 1 for i in range(len(pre_hex)*4)]

    for path in args:
        x = load16(path)
        best = None
        # ⭐ SWEEP the low-pass, because the right width is a property of the front end and
        # not knowable a priori. A blank column across the whole sweep is itself a finding.
        for k in (1, 3, 5, 7, 9, 13, 17, 25):
            runs = runs_from(x, k)
            halves = manchester_bits(runs, 32)
            for phase in (0, 1):
                bits = pair_halves(halves, phase)
                frame, pos, inv = find_frame(bits, pre, nbits)
                if frame:
                    h = "".join(str(b) for b in frame)
                    hexs = "%0*x" % (nbits//4, int(h, 2))
                    best = (k, hexs, pos, inv)
                    break
            if best:
                break
        name = path.split("/")[-1]
        if best:
            k, hexs, pos, inv = best
            print(f"  {name:<28} lp={k:<3} pos={pos:<5} {'inv' if inv else '   '}  {hexs}")
        else:
            runs = runs_from(x, 9)
            print(f"  {name:<28} -  no frame   ({len(runs)} runs, median "
                  f"{sorted(n for _, n in runs)[len(runs)//2]} samples)")

if __name__ == "__main__":
    main()
