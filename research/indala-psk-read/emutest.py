#!/usr/bin/env python3
"""Test an emulating device against the Proxmark's own demodulator, bracketed.

    ./emutest.py [--frame a0000000e6bd0e92] [--min 10]

⛔ WHY THIS IS ONE COMMAND. The emulator has to be positioned by hand, and every time the
coupling was confirmed in one step and the test run in another, the device moved in between
— three times in one session. Each time the result was "fails at every length", and each
time it was an EMPTY FIELD: fc/2 amplitude 2.62, 3.06 against an empty floor of 2.99.

⇒ This checks coupling and runs the test from the SAME capture, with no chance to move
anything in between. If the capture is not coupled it refuses to score rather than reporting
a null that means nothing (FINDINGS F05).

Reference on this bench: real Indala tag 33.85, emulator best 33.87 with USB UNPLUGGED,
0.58-0.72x with the cable attached, empty field 2.99.
⚠ UNPLUG THE USB CABLE. It detunes the antenna and costs ~40% of the signal.
"""
import argparse
import glob
import os
import subprocess
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from emuprobe import load16, longest_bit_run                      # noqa: E402

PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
TMP = "/tmp/emutest"


def pm3(cmd):
    return subprocess.run([PM3, "-c", cmd], capture_output=True, text=True, timeout=200).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--frame", default="a0000000e6bd0e92")
    ap.add_argument("--min", type=float, default=10.0,
                    help="refuse to score below this fc/2 amplitude (empty field is 3.0)")
    a = ap.parse_args()

    # ⭐ WAKE THE TAG FIRST, AND DISCARD THAT READ. An emulating Chameleon is asleep until
    # a field appears; the first `lf read` wakes it but is over before it starts modulating,
    # so it captures an empty field and the bracket refuses. Two runs back to back worked
    # where one did not — the second caught the device already awake. Costs one throwaway
    # capture and removes a failure mode that looks exactly like "not coupled".
    pm3("lf read")
    for f in glob.glob(TMP + "*.pm3"):
        os.remove(f)
    pm3(f"lf read; data save -f {TMP}")
    found = sorted(glob.glob(TMP + "*.pm3"))
    if not found:
        print("  ⛔ no trace — is the Proxmark free?")
        return 2
    x = load16(found[0])
    y = x - x.mean()
    amp = float(np.sqrt(np.mean((y * ((-1.0) ** np.arange(len(y)))) ** 2)))

    print("  0. woke the tag with a throwaway read (it sleeps until a field appears)")
    print(f"  1. BRACKET — fc/2 amplitude {amp:.2f}   "
          f"(real tag 33.85, empty 2.99, threshold {a.min:.1f})")
    if amp < a.min:
        print(f"\n  ⛔ REFUSING TO SCORE. {amp:.2f} is indistinguishable from an empty field, so")
        print( "     'no decode' would say nothing about the emulator. Reposition it — face")
        print( "     down, centred — and UNPLUG THE USB, which costs about 40%.")
        return 2

    need = longest_bit_run(a.frame) * 32
    b = y * ((-1.0) ** np.arange(len(y)))
    S = np.fft.rfft(b)
    S[np.fft.rfftfreq(len(b), 1 / 125000.0) > 12000.0] = 0
    bb = np.fft.irfft(S, len(b))
    sgn = np.sign(bb)
    sgn[sgn == 0] = 1
    runs = np.diff(np.concatenate([[0], np.flatnonzero(np.diff(sgn)) + 1, [len(sgn)]]))
    print(f"  2. PHASE  — longest constant run {runs.max()} samples, frame needs {need}"
          f"   {'✓' if runs.max() > need * 0.7 else '⛔'}")

    print("  3. PROXMARK's OWN DEMOD, by capture length:")
    ok = []
    for n in (8192, 16384, 20480, 24576, 32768, len(x)):
        if n > len(x):
            continue
        p = f"/tmp/emutest-{n}.pm3"
        open(p, "w").write("\n".join(str(int(round(v))) for v in x[:n]))
        out = pm3(f"data load -f {p}; lf indala demod")
        hit = a.frame in out
        ok.append((n, hit))
        print(f"       {n / 125:6.0f} ms   {'✓ ' + a.frame if hit else '✗'}")
    best = max((n for n, h in ok if h), default=0)
    print(f"\n  ⇒ decodes up to {best / 125:.0f} ms of capture"
          if best else "\n  ⇒ no length decoded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
