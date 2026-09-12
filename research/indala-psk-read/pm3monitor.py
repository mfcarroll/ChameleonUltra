#!/usr/bin/env python3
"""Live fc/2 amplitude on the Proxmark, so an emulating device can be slid around.

    ./pm3monitor.py [--n 30] [--ref 33.8]

⭐ WHY. `lfprobe.py --monitor` found the single biggest error in this project — the tag was
on the wrong side of the Chameleon, worth 21x — by making coupling a CONTINUOUS number
somebody could watch while moving the tag. This is the same instrument pointed at the
Proxmark, for the question now open: Indala emulation works but delivers ~2-3x less fc/2
than a real tag, and nobody has established whether that is the firmware or simply a whole
device coupling worse than a coin lying flat on the coil.

⚠ Read it against the reference, not in isolation. Measured on this bench with a real Indala
tag on the Proxmark antenna: fc/2 amplitude **33.8**, empty field **3.0**. An emulator that
reaches ~30 is coupling as well as a tag and there is nothing to fix in firmware.
"""
import argparse
import os
import subprocess
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
TMP = "/tmp/pm3mon"


def sample():
    subprocess.run([PM3, "-c", f"lf read; data save -f {TMP}"],
                   capture_output=True, text=True, timeout=180)
    p = TMP + ".pm3"
    if not os.path.exists(p):
        return None
    vals = np.array([float(t) for t in open(p, "rb").read().split() if t.strip()])
    y = vals - vals.mean()
    # fc/2 amplitude: mix by (-1)^n and take the rms of the result. Same quantity the
    # reference numbers above were measured with.
    mixed = y * ((-1.0) ** np.arange(len(y)))
    return float(np.sqrt(np.mean(mixed ** 2))), float(np.ptp(vals))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=30)
    ap.add_argument("--ref", type=float, default=33.8,
                    help="real-tag reference on this bench (empty field is 3.0)")
    a = ap.parse_args()
    print(f"  reference: real tag {a.ref:.1f}, empty field 3.0. Slide the device; watch for a peak.\n")
    best = 0.0
    for i in range(a.n):
        r = sample()
        if r is None:
            print("  ⛔ no trace — is the Proxmark free?")
            return 2
        amp, ptp = r
        best = max(best, amp)
        frac = amp / a.ref
        bar = "#" * min(60, int(frac * 50))
        print(f"  {amp:6.2f}  {frac:5.2f}x of a real tag   best {best:6.2f}   p-p {ptp:5.0f}  {bar}",
              flush=True)
    print(f"\n  best {best:.2f} = {best / a.ref:.2f}x a real tag")


if __name__ == "__main__":
    sys.exit(main())
