#!/usr/bin/env python3
"""Is a failing capture bad EVERYWHERE, or bad in ONE PLACE?

emutest.py measures the longest PREFIX that decodes, so a transient anywhere early
poisons every longer length and the result reads as a length ceiling. This slides a
FIXED-length window across the same capture instead. Two different shapes of answer:

  decodes at some offsets, not others  -> localised damage (a boundary, a wake transient)
  decodes at no offset                 -> the whole capture is unusable, length is not the axis

    ./offsetsweep.py <capture.pm3> [--win 16384] [--step 2048]
"""
import argparse
import subprocess
import sys

PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
FRAME = "a0000000e6bd0e92"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("capture")
    ap.add_argument("--win", type=int, default=16384)
    ap.add_argument("--step", type=int, default=2048)
    a = ap.parse_args()

    vals = [t for t in open(a.capture).read().split() if t.strip()]
    print(f"  {len(vals)} samples = {len(vals)/125:.0f} ms, "
          f"window {a.win/125:.0f} ms, step {a.step/125:.1f} ms\n")
    hits = 0
    tried = 0
    for off in range(0, max(1, len(vals) - a.win + 1), a.step):
        p = f"/tmp/offsweep-{off}.pm3"
        open(p, "w").write("\n".join(vals[off:off + a.win]))
        out = subprocess.run([PM3, "-c", f"data load -f {p}; lf indala demod"],
                             capture_output=True, text=True, timeout=200).stdout
        hit = FRAME in out
        hits += hit
        tried += 1
        print(f"    offset {off:6d} = {off/125:6.1f} ms   {'✓' if hit else '✗'}", flush=True)
    print(f"\n  ⇒ {hits} of {tried} windows decoded")


if __name__ == "__main__":
    sys.exit(main())
