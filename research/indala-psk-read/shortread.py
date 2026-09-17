#!/usr/bin/env python3
"""Does C490 generalise? Each emulate arm read TWICE — the reader's own way, and with a short read.

    ./shortread.py                       # all six silent arms
    ./shortread.py indala keri --repeat 5

⭐⭐ WHY. C490 measured that `lf indala reader` is silent on our emulation while `lf read -s 4096`
followed by `lf indala demod` returns the armed credential byte-exact, 4 of 4, in the same session
and the same field. The reader's own source says why: `cmdlfindala.c:633` is `lf_read(false,
30000)` — 240 ms, against the short read's 33 ms — and a span that long carries enough of C486's
beat to defeat the demodulator. ⇒ **The obvious question is whether the other five silent arms are
the same story**, and that is what this answers.

⛔⛔ THE A/B IS THE POINT AND IT MUST BE INTERLEAVED. Both reads run in ONE pm3 session, alternating,
so field strength, coupling, temperature and the arm itself are identical between them. The ONLY
variable is how many samples the client asks for. A run that did all the long reads and then all
the short ones would confound the answer with drift.

⛔ TWO SCORES, NOT ONE, because they answer different questions:
  - `marker` — the registry's own decode marker matched PER LINE (C488: a marker is matched
    against a LINE; `re.search` over a blob measures the regex, not the signal). It says a frame
    of this protocol was found.
  - `exact`  — the armed credential appears in the output. It says the frame was OURS, byte for
    byte. ⭐ A marker without an exact is a decoded frame carrying the WRONG payload, which is a
    result in its own right and is reported as such: C490 saw exactly that from a one-frame window
    (`a0000000e4000000` for `a0000000e6bd0e92`), confidently and with no warning.

⛔ THE SHORT READ IS SIZED PER ARM at roughly two frames, because one frame is demonstrably enough
to produce a confident wrong answer and not enough to check itself. Frame lengths are the arm's
own: 64 bits x RF/32 for the PSK64 arms, 128 x RF/32 for NexWatch, 224 x RF/32 for Indala224,
96 x RF/64 for GProxII.

⛔⛔ THIS GRADES NOTHING. No null sweep, no calibration row, no licence — a manual observation
like C487/C488/C490. It cannot move a cell in the matrix and must never be reported as if it had.

⚠ The arm table is TRANSCRIBED from `rfid-tools`' `benchmatrix/registry.py` (the same `pm3_read`,
`pm3_decode_marker`, `expect` and `cu_emulate` the graded matrix uses) rather than imported across
repos. If one drifts from the other that is a thing to report, not to paper over.
"""
import argparse
import json
import re
import subprocess
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import seqdump

PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"

# key -> (cu slot type, econfig, pm3 reader cmd, decode marker regex, expected raw, short samples)
ARMS = {
    "indala": ("Indala", "lf indala econfig -s %d --id a0000000e6bd0e92",
               "lf indala reader", r"Indala \(len", "a0000000e6bd0e92", 4096),
    "keri": ("Keri", "lf keri econfig -s %d --id 80003039",
             "lf keri reader", r"KERI - Internal ID|Descrambled MS - FC:|probably KERI",
             "80003039", 4096),
    "idteck": ("IDTECK", "lf idteck econfig -s %d --id 4944544b55667788",
               "lf idteck reader", r"IDTECK Tag Found: Card ID", "4944544B55667788", 4096),
    "nexwatch": ("NexWatch", "lf nexwatch econfig -s %d --cn 87654321 -m 2",
                 "lf nexwatch reader", r"NexWatch raw id|88bit id", "87654321", 8192),
    "indala224": ("Indala224",
                  "lf indala econfig -s %d --id "
                  "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5 --224",
                  "lf indala reader", r"Indala \(len",
                  "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5", 16384),
    "gproxii": ("GProxII", "lf gproxii econfig -s %d --raw fac2a38c2b081af0210b12c2",
                "lf gproxii reader", r"G-Prox-II - (Unknown )?[Ll]en:",
                "fac2a38c2b081af0210b12c2", 12288),
}
ORDER = ["indala", "keri", "idteck", "nexwatch", "indala224", "gproxii"]


def score(block, marker, expect):
    """⛔ PER LINE (C488). `^`-anchored markers never match inside a blob."""
    rx = re.compile(marker)
    hit = any(rx.search(ln) for ln in block.splitlines())
    ex = expect.lower() in block.lower()
    return hit, ex


def run(port, key, repeat, timeout, null=False):
    """⛔ `null=True` RUNS THE IDENTICAL COMMANDS WITH NOTHING ARMED. Rig B is tagless, so every
    count must be zero; a single hit there means the reads are picking up something that is not
    our emission, and no armed figure in the same run can be believed."""
    typ, ec, rd, marker, expect, n = ARMS[key]
    demod = rd.replace(" reader", " demod")
    if null:
        seqdump.disarm(port)
    else:
        ok, why = seqdump.arm(port, typ, ec)
        if not ok:
            return {"arm": key, "error": "ARM FAILED: " + why}
    cmds = []
    for _ in range(repeat):
        cmds.append(rd)
        cmds.append("lf read -s %d" % n)
        cmds.append(demod)
    r = subprocess.run([PM3, "-c", "; ".join(cmds)], capture_output=True, text=True,
                       timeout=timeout)
    out = r.stdout + r.stderr
    # Split the transcript on the client's own echo of each command.
    blocks, cur, label = [], [], None
    for ln in out.splitlines():
        m = re.search(r"pm3 --> (.+)$", ln)
        if m:
            if label is not None:
                blocks.append((label, "\n".join(cur)))
            label, cur = m.group(1).strip(), []
        elif label is not None:
            cur.append(ln)
    if label is not None:
        blocks.append((label, "\n".join(cur)))

    res = {"arm": key, "short_samples": n, "long": [], "short": []}
    for label, body in blocks:
        if label == rd:
            res["long"].append(score(body, marker, expect))
        elif label == demod:
            res["short"].append(score(body, marker, expect))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("arms", nargs="*", default=ORDER)
    ap.add_argument("--port", default=seqdump.CU2_PORT)
    ap.add_argument("--repeat", type=int, default=3)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--null", action="store_true",
                    help="run every read with NOTHING armed; every count must be zero")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    bad = [x for x in a.arms if x not in ARMS]
    if bad:
        print("unknown arm(s): %s\nknown: %s" % (", ".join(bad), ", ".join(ORDER)))
        return 2

    out = []
    try:
        for key in a.arms:
            r = run(a.port, key, a.repeat, a.timeout, null=a.null)
            out.append(r)
            if "error" in r:
                print("%-10s %s" % (key, r["error"]))
                continue
            lm = sum(1 for h, _ in r["long"] if h)
            lx = sum(1 for _, e in r["long"] if e)
            sm = sum(1 for h, _ in r["short"] if h)
            sx = sum(1 for _, e in r["short"] if e)
            print("%-10s  %-22s marker %d/%d  exact %d/%d   |   short -s %-5d marker %d/%d  "
                  "exact %d/%d"
                  % (key, ARMS[key][2], lm, len(r["long"]), lx, len(r["long"]),
                     r["short_samples"], sm, len(r["short"]), sx, len(r["short"])))
    finally:
        print("\ndisarming cu2 ...")
        seqdump.disarm(a.port)

    if a.null:
        tot = sum(sum(1 for h, _ in r.get("long", []) if h)
                  + sum(1 for h, _ in r.get("short", []) if h) for r in out)
        print("\n%s NULL: %d marker hits across every arm with NOTHING armed"
              % ("✅" if tot == 0 else "⛔⛔", tot))
    print("\n⛔ Ungraded: no null sweep, no calibration row, no licence. Moves no cell.")
    if a.json:
        print(json.dumps(out, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
