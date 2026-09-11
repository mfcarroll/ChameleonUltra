#!/usr/bin/env python3
"""Does the firmware decoder generalise past the one word this project has ever seen?

    ./generality.py                 # threshold sweep over structurally-chosen words
    ./generality.py --words 200     # plus random payloads

⭐ WHAT THIS IS FOR. Every capture in this project carries `a0000000e6bd0e92` and nothing
else. Three structural properties of that ONE word have never been varied, and each has a
plausible mechanism for breaking a decoder that works on it:

  1. ⚠ IT HAS 19 ONES — ODD PARITY. PSK1 carries the subcarrier polarity across the frame
     boundary, so an odd-parity word INVERTS every frame and repeats at 4096 samples
     (C14). An EVEN-parity word does not invert and repeats at 2048. Every capture, every
     synthetic, and every decode in this project is the inverting case.
  2. ⚠ A FALSE PREAMBLE CAN LIVE IN THE DATA. The preamble is 1010 + 28 zeros + 1, and the
     search is exact with zero tolerance. A payload that happens to reproduce it — or that
     reproduces it ACROSS the wrap from one frame into the next — gives a second, wrong
     lock position that scores on magnitude like a real one.
  3. ⚠ MAGNITUDE RANKING ASSUMES THE WORD IS MIXED. Offsets are ranked by mean |integrator|
     over the 64 bits, which is what rejects a boxcar straddling the bit boundaries. A word
     with long constant runs has systematically larger integrators at EVERY offset,
     because a straddling boxcar inside a run does not cancel.

⚠ THE GENERATOR MUST NOT SHARE THE DECODER'S CONVENTIONS — that is precisely the trap that
cost this project a fortnight (METHOD M1, M16). So it is written from the physical
description, and the thing that validates it is not a round trip: it is that the C decoder,
already checked word for word against 320 REAL captures, reads what this produces. If the
generator had the wrong convention the C decoder would fail on the known word, which is
the first row of the table and is there as the control.

Reported per word: the lowest subcarrier amplitude at which >=19 of 20 noise seeds decode
EXACTLY. A structural problem shows up as a threshold far off the control's, or as a wrong
answer at high amplitude where noise cannot be the explanation.
"""
import argparse
import os
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CDEMOD = os.path.join(HERE, "ctest", "cdemod")

FS, BIT = 125000.0, 32
PREAMBLE = [1, 0, 1, 0] + [0] * 28 + [1]
TRUTH = 0xa0000000e6bd0e92

# Measured from caps/phasebits/tag_p024_r0.bin: the DC pedestal the SAADC actually sits on
# and the carrier ripple riding it. The ripple matters — it is what the fs/2 notch removes,
# and a synthetic without it tests a filter that has nothing to do.
PEDESTAL, RIPPLE = 13600.0, 316.0


def word_bits(w):
    return [(w >> (63 - i)) & 1 for i in range(64)]


def bits_word(bits):
    v = 0
    for b in bits:
        v = (v << 1) | int(b)
    return v


def psk1_stream(bits, n):
    """⭐ PSK1 FROM THE PHYSICS, NOT FROM THE DECODER.

    A T5577 in PSK1 transmits a subcarrier at fc/2 whose PHASE is the data: bit 1 is one
    phase, bit 0 the other, for 32 carrier cycles each. The tag does not restart at a frame
    boundary — it just keeps sending bits — so the polarity simply continues into the next
    copy of the word. That continuation is the whole of the odd/even parity effect, and it
    is why this does `np.tile(bits, ...)` rather than building one frame and repeating it.

    Sampled once per carrier cycle, a subcarrier at exactly fs/2 alternates sign every
    sample, which is the (-1)^n below.
    """
    reps = int(np.ceil(n / (64 * BIT))) + 2
    stream = np.tile(np.asarray(bits, float), reps)
    pol = np.where(stream > 0, 1.0, -1.0)                 # PSK1: the phase IS the bit
    sig = np.repeat(pol, BIT)[:n]
    return sig * np.tile([1.0, -1.0], n // 2 + 1)[:n]


def synth_capture(bits, n, amp, noise, seed, start=0):
    rng = np.random.default_rng(seed)
    t = np.arange(n)
    # A slow envelope drift plus carrier ripple: both originally low-frequency, both moved
    # to fs/2 by the mixer, which is exactly what the notch has to remove.
    ripple = RIPPLE * np.sin(2 * np.pi * 1800.0 * t / FS + 0.7)
    x = PEDESTAL + ripple + amp * psk1_stream(bits, n + start)[start:start + n]
    x = x + rng.normal(0, noise, n)
    return np.clip(np.round(x), 0, 16383).astype(np.uint16)


def write16(path, x):
    b = bytearray()
    for v in x:
        b += bytes(((int(v) >> 8) & 0x3F, int(v) & 0xFF))
    open(path, "wb").write(bytes(b))


def cdemod(paths):
    out = subprocess.run([CDEMOD] + paths, capture_output=True, text=True, check=True)
    res = {}
    for line in out.stdout.splitlines():
        f = line.split()
        if len(f) < 4:
            continue
        res[os.path.basename(f[0])] = None if f[3] == "-" else (f[4] if f[3] == "***" else f[3])
    return res


def threshold(bits, noise, seeds, amps, tmp):
    """Lowest amp with >=19/20 exact recoveries. Also reports whether any failure was a
    WRONG ANSWER rather than no answer — at high amplitude that cannot be noise."""
    want = "%016x" % bits_word(bits)
    thr, worst_wrong = None, None
    for amp in amps:
        paths = []
        for s in range(seeds):
            p = os.path.join(tmp, "w_%d_%d.bin" % (amp, s))
            # ⚠ a random start phase within the frame: the real capture has no idea where
            # in the word the field came up, and a decoder that only works from bit 0 would
            # otherwise pass.
            write16(p, synth_capture(bits, 4096, amp, noise, s,
                                     start=int(np.random.default_rng(s).integers(0, 2048))))
            paths.append(p)
        res = cdemod(paths)
        hits = sum(1 for p in paths if res.get(os.path.basename(p)) == want)
        wrong = [res[os.path.basename(p)] for p in paths
                 if res.get(os.path.basename(p)) not in (None, want)]
        if wrong and amp >= amps[0]:
            worst_wrong = worst_wrong or (amp, wrong[0])
        if hits >= seeds - 1:
            thr = amp
    return thr, worst_wrong


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--noise", type=float, default=40.0)
    ap.add_argument("--seeds", type=int, default=20)
    ap.add_argument("--words", type=int, default=0, help="extra random payloads")
    a = ap.parse_args()
    if not os.path.exists(CDEMOD):
        sys.exit("build the decoder first:  cd ctest && make")

    print(__doc__.split("Reported per word")[0])
    amps = [120, 80, 60, 40, 30, 24, 20, 16, 13, 10, 8, 6, 5, 4]

    cases = []
    tb = word_bits(TRUTH)
    cases.append(("bench tag (CONTROL)", tb))

    # even parity: flip one payload bit of the control, nothing else changes
    eb = list(tb); eb[63] ^= 1
    cases.append(("even parity", eb))

    # all-zero and all-one payloads: the longest possible constant runs
    cases.append(("payload all zeros", PREAMBLE + [0] * 31))
    cases.append(("payload all ones", PREAMBLE + [1] * 31))
    # ⚠ a payload whose tail plus the next frame's head reproduces the preamble
    cases.append(("preamble in payload", PREAMBLE + [1, 0, 1, 0] + [0] * 27))
    cases.append(("alternating payload", PREAMBLE + [i % 2 for i in range(31)]))

    rng = np.random.default_rng(4)
    for i in range(a.words):
        cases.append(("random %d" % i, PREAMBLE + list(rng.integers(0, 2, 31))))

    print(f"  noise sd {a.noise:.0f} counts, {a.seeds} seeds, random start phase per seed\n")
    print(f"  {'word':20s} {'hex':>17s} {'ones':>5s} {'par':>4s} {'threshold':>10s}  note")
    with tempfile.TemporaryDirectory() as tmp:
        ctrl = None
        for name, bits in cases:
            w = bits_word(bits)
            thr, wrong = threshold(bits, a.noise, a.seeds, amps, tmp)
            ones = sum(bits)
            if ctrl is None:
                ctrl = thr
            note = ""
            if thr is None:
                note = "⛔ NEVER decoded"
            elif ctrl and thr > ctrl * 2:
                note = "⛔ %.1fx worse than the control" % (thr / ctrl)
            if wrong:
                note += "  ⛔ WRONG ANSWER at amp %d: %s" % wrong
            print(f"  {name:20s} {w:016x} {ones:5d} {'odd' if ones % 2 else 'even':>4s} "
                  f"{str(thr):>10s}  {note}")


if __name__ == "__main__":
    main()
