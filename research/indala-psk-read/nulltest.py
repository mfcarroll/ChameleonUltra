#!/usr/bin/env python3
"""Run a LOUD-SIGNAL NULL: prove a wrong tag is present, then prove `lf indala read` ignores it.

    ./nulltest.py --floor 470000 --read "lf viking read" --hit Viking
    ./nulltest.py --floor 470000 --read "lf idteck read" --hit IDTECK --n 30

⭐ WHY THIS IS A SCRIPT AND NOT A PASTED COMMAND. The null only means something if the
interferer was demonstrably on the antenna while the reads failed, and the obvious way to
show that — read the interferer — is the one that does not work. Six bracketing
`lf hid prox read` calls failed while that tag sat at 90x the empty floor (FINDINGS F05),
and following that rule would have thrown away a valid measurement. A read conflates "not
heard" with "heard but not decoded". Amplitude does not.

⇒ The bracket here is AMPLITUDE, measured in the interferer's own band against a floor
measured in THAT SAME BAND with the antenna clear. There is no universal floor (METHOD M24):
`--floor` has no default and this refuses to run without one.

    ./lfprobe.py --band 500 20000 --monitor 5      # ANTENNA CLEAR -> the floor
    ./nulltest.py --floor <that number> --read "lf pac read" --hit PAC

⚠ The interferer's own reader is still run, but only as a BONUS. It cannot fail the test,
because a broken decoder for the interferer says nothing about the Indala reader.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lfprobe import cu, probe                                   # noqa: E402

HIT_INDALA = "Indala"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--floor", type=float, required=True,
                    help="empty-field level in --band, antenna CLEAR. No default: M24.")
    ap.add_argument("--band", nargs=2, type=float, default=[500.0, 20000.0],
                    help="band to measure the interferer in (default 500-20000, wide "
                         "enough for every LF envelope modulation on this bench)")
    ap.add_argument("--read", required=True, help="the interferer's own read command")
    ap.add_argument("--hit", required=True, help="substring marking that read succeeding")
    ap.add_argument("--n", type=int, default=20, help="Indala read attempts (default 20)")
    ap.add_argument("--min-ratio", type=float, default=3.0,
                    help="refuse to score the null below this ratio (default 3.0)")
    a = ap.parse_args()
    lo, hi = a.band

    print(f"  band {lo:.0f}-{hi:.0f} Hz   floor {a.floor:.0f}   interferer {a.read!r}\n")
    print("  1. BRACKET — is the interferer actually on the antenna?")
    amps = []
    for _ in range(5):
        amp, dc = probe(lo, hi)
        amps.append(amp)
        print(f"       {amp:11.0f}   {amp / a.floor:6.2f}x   carrier DC {dc:.0f}")
    ratio = sorted(amps)[len(amps) // 2] / a.floor
    print(f"     median {ratio:.2f}x the empty floor")
    if ratio < a.min_ratio:
        print(f"\n  ⛔ REFUSING TO SCORE. {ratio:.2f}x is not convincing presence, so a null "
              f"here\n     would be indistinguishable from an empty antenna. Reposition the "
              f"tag, or\n     check --floor was measured in THIS band with the antenna clear.")
        return 2

    print(f"\n  2. THE NULL — {a.n} x 'lf indala read' against it")
    out = cu(*(["lf indala read"] * a.n))
    hits = [m.group(1) for m in re.finditer(r"Raw:\s*([0-9a-f]{16})", out)]
    notfound = len(re.findall(r"LF tag not found", out))
    print(f"     not found : {notfound}/{a.n}")
    print(f"     FRAMES    : {len(hits)}" + ("   ⛔ " + " ".join(sorted(set(hits))) if hits else "   ✓ none"))

    print(f"\n  3. bonus — does {a.read!r} itself work? (cannot fail this test)")
    out2 = cu(*([a.read] * 5))
    own = len(re.findall(re.escape(a.hit), out2))
    print(f"     {own}/5 " + ("✓" if own else "— broken or absent; the bracket above still stands"))

    ok = not hits and notfound == a.n
    print(f"\n  ⇒ {'✓ NULL PASSES' if ok else '⛔ NULL FAILED — the Indala reader produced a frame'}"
          f"  ({notfound}/{a.n} not found, interferer at {ratio:.1f}x)")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
