#!/usr/bin/env python3
"""Cross-check the FIRMWARE decoder against the research decoder, per capture.

⭐ WHY PER-CAPTURE AND NOT PER-COUNT. Two decoders can agree on "51 of 160" while
succeeding on different captures, which would mean they are not the same algorithm and
one of them is getting lucky. The comparison below is file by file and on the full 64-bit
word, including the failures — a shared wrong answer is evidence they agree, and a
disagreement anywhere is a bug in one of them.

⚠ The two share no code. mfdemod.py is numpy floating point with an FFT brick-wall
low-pass; lf_indala_psk.c is C integer arithmetic with a [1,2,1] notch folded into the bit
integrator. They are expected to agree because the SIGNAL is the same, not because the
implementations are.
"""
import glob
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import mfdemod as M                                              # noqa: E402

TRUTH = "a0000000e6bd0e92"
TAPS = np.array([1.0, 2.0, 1.0]) / 4.0


# ⛔ The straddle gate, mirrored from lf_indala_psk.h. It lives here for the same reason
# TAPS does: this comparison is meant to catch a bad PORT, so everything that is a
# deliberate algorithm choice must be identical on both sides or every straddle shows up as
# a spurious mismatch. The gate itself is validated independently, against both placements,
# not by this file.
STRADDLE_AMP = 2048
STRADDLE_DIV = 8


def py_decode(path):
    """mfdemod's chain with the firmware's filter AND its straddle gate, so the comparison
    isolates the PORT rather than re-testing either choice."""
    x = M.load16(path)
    if x is None:
        return None
    b = np.convolve(M.baseband(x), TAPS, mode="same")
    best = None
    for off in range(M.BIT):
        bits, integ = M.bit_stream(b, off)
        if len(bits) < 64:
            continue
        for i, inv, err in M.find_preamble(bits, 0):
            if i + 64 > len(bits):
                continue
            seg = np.abs(integ[i:i + 64])
            amp = float(np.sum(seg))
            if best is None or amp > best[0]:
                w = bits[i:i + 64]
                best = (amp, "".join(str(int(v)) for v in (1 - w if inv else w)),
                        float(np.min(seg)))
    if best is None:
        return None
    # ⚠ SCALE. TAPS is [1,2,1]/4 but the firmware folds [1,2,1] into the boxcar WITHOUT
    # dividing (4*box + corrections), so every C integrator is 4x its numpy counterpart.
    # The thresholds are in firmware units, so scale up rather than editing the constant —
    # this comparison caught the mismatch when it was missed, which is the point of it.
    mean = 4.0 * best[0] / 64.0
    mn = 4.0 * best[2]
    if mean >= STRADDLE_AMP and mn * STRADDLE_DIV < mean:
        return None
    # ⭐ The reference's two zero bits, mirrored from `indala64_accept` in lf_indala_psk.c.
    # ⛔ NOT optional here: this file's whole value is that the two decoders agree word for
    # word INCLUDING their failures, so a gate added on one side and not the other shows up
    # as four per-capture mismatches — which is exactly how this line came to be written.
    if best[1][60] != "0" or best[1][61] != "0":
        return None
    return "%016x" % int(best[1], 2)


def c_decode_all(paths):
    out = subprocess.run([os.path.join(HERE, "cdemod")] + paths,
                         capture_output=True, text=True, check=True)
    res = {}
    for line in out.stdout.splitlines():
        f = line.split()
        if len(f) < 3:
            continue
        name = os.path.basename(f[0])
        if "not a 16-bit" in line:
            continue
        if f[3] == "-":
            res[name] = (None, None)
        else:
            word = f[3].strip("*") if f[3] != "***" else f[4]
            res[name] = (word, "parity-ok" in line)
    return res


def main():
    # ⚠ Defaults to the back-side set for continuity, but the front-side captures are the
    # ones current claims rest on — so the directory is an argument, not a constant.
    #     ./compare.py ../caps/front
    d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "caps", "phasebits")
    caps = sorted(glob.glob(os.path.join(d, "*.bin")))
    if not caps:
        sys.exit("no captures found")
    cres = c_decode_all(caps)

    mism, rows = [], {"tag": [0, 0, 0, 0], "empty": [0, 0, 0, 0]}
    for p in caps:
        name = os.path.basename(p)
        state = "tag" if name.startswith("tag") else "empty"
        cw, cpar = cres.get(name, (None, None))
        pw = py_decode(p)
        if cw != pw:
            mism.append((name, pw, cw))
        r = rows[state]
        r[0] += 1
        r[1] += cw is not None
        r[2] += cw == TRUTH
        r[3] += bool(cw is not None and cpar)

    print(__doc__)
    print(f"  {'':6s} {'files':>6s} {'frame':>6s} {'== truth':>9s} {'parity-ok':>10s}")
    for s, r in rows.items():
        print(f"  {s:6s} {r[0]:6d} {r[1]:6d} {r[2]:9d} {r[3]:10d}")

    n_bad_par = rows["tag"][1] - rows["tag"][3]
    n_wrong = rows["tag"][1] - rows["tag"][2]
    print(f"\n  Wiegand-26 parity as a filter: {n_wrong} of {rows['tag'][1]} frames are "
          f"wrong, {n_bad_par} fail parity")

    if mism:
        print(f"\n⛔ {len(mism)} PER-CAPTURE MISMATCHES between the two decoders:")
        for name, pw, cw in mism[:20]:
            print(f"   {name:28s} python {pw}  c {cw}")
        sys.exit(1)
    print(f"\n✓ firmware and research decoders agree on all {len(caps)} captures, "
          f"word for word")


if __name__ == "__main__":
    main()
