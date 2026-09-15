#!/usr/bin/env python3
"""What is ACTUALLY on the air — max run length and duty — scored against the emitter's own source.

⭐ WHY THIS EXISTS. Every earlier PAC instrument asked *which of the intended runs survived*
(pacdiff.py's differential, C441's drop-only tiling, C440's positional alignment). All three
presuppose that the emission is the intended frame, degraded. This one asks the prior question —
**is the emission the frame at all** — with two numbers that need no alignment and no decode:

  MAX RUN  : the longest single level the capture carries, against the longest the frame can
             contain, computed from the emitter's own modulator.
  DUTY     : the fraction of TIME the air is high, against the frame's fraction of 1-bits.

⛔⛔ BOTH NUMBERS ARE BIASED BY THE FLIPPER'S COMPARATOR AND THE BIAS IS NOT A CONSTANT — C435
fitted 93us and 151us in two captures minutes apart. It inflates every pulse and deflates every
gap by the same b, so it cannot be ignored and it must not be guessed either. ⇒ **b is estimated
from the CONTROLS**, whose true duty is fixed at 50% by construction — Manchester and biphase
spend half of every bit at each level — and the run of controls is what licenses applying it to
the arm under test. A verdict that needs a b outside the controls' own range is not a verdict.

⛔ flipraw's second value per pair is a PERIOD, not a run (C429). A HIGH run is the pulse; the
LOW run after it is period - pulse. Getting this wrong once reported 100.0% against a true 48.8%.

⭐ PICK CONTROLS THAT COULD FAIL (M52): fdxb is PAC's exact PWM geometry — counter_top 32, one
entry per bit, a whole-entry held level via `counter_top + 1` — and gproxii holds a level for a
whole 512us bit. Both are proven byte-exact through this same reader (C429/C430), so if either
one's max run or duty comes back wrong, the instrument is wrong and nothing else here counts.

  ./airduty.py                 # pac + both controls
  ./airduty.py --arms pac
"""
import argparse, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flipraw
import pacdiff

PY = os.path.join(HERE, "../../software/script/.venv/bin/python")
CU = os.path.join(HERE, "../../software/script/cu.py")
EMU = "/dev/tty.usbmodemC3A1656543DE1"

# tag -> (slot type, econfig, headline, predicted max run us, predicted high-time fraction)
# ⚠ Every prediction is computed or read from the emitter source, never from a previous run.
#   pac.c   : PAC_RF_PER_BIT 32 -> 256us/bit; the 8-bit sync marker abuts the previous UART
#             frame's stop bit, so every frame carries a run of NINE like bits = 2304us. Its
#             duty is the frame's own 1-fraction, computed live by pacdiff.build().
#   fdxb.c  : counter_top 32, level flips every bit -> max static 384us, duty 50%.
#   gproxii.c: counter_top 64, one entry per bit -> max static 512us, duty 50%.
ARMS = {
    "pac":   ("PAC",     "lf pac econfig -s 8 --cn %s", 2304, None),
    "fdxb":  ("FDXB",    "lf fdxb econfig -s 8 --raw 00339a080402079f8040797788040201", 384, 0.5),
    "gprox": ("GProxII", "lf gproxii econfig -s 8 --raw fac2a38c2b081af0210b12c2", 512, 0.5),
}
CONTROLS = ("fdxb", "gprox")


def cu(*cmds):
    r = subprocess.run([PY, CU] + list(cmds), capture_output=True, text=True)
    return r.stdout + r.stderr


BAD = ("unrecognized", "invalid", "usage:", "error", "must be", "need exactly")


def arm(typ, econfig):
    """⛔ The econfig runs ALONE and is judged on its OWN output (C431) — and on BAD WORDS, not
    on the word `success`: several econfigs print their own confirmation line and never say
    `success`, so requiring it refuses a perfectly armed slot. That cost one run of this tool."""
    out = cu("hw connect -p %s" % EMU, "hw slot type -s 8 -t %s" % typ,
             "hw slot enable -s 8 --lf")
    if "success" not in out:
        raise SystemExit("⛔ slot prep failed: " + out[-200:])
    e = cu("hw connect -p %s" % EMU, econfig)
    if any(w in e.lower() for w in BAD):
        raise SystemExit("⛔ econfig refused: " + e[-200:])
    out = cu("hw connect -p %s" % EMU, "hw slot change -s 8", "hw mode -e")
    if "success" not in out:
        raise SystemExit("⛔ slot activate failed: " + out[-200:])


def capture(tag, seconds):
    path = "/ext/lfrfid/airduty_%s.ask.raw" % tag      # a path per arm (C432: stale captures)
    f = flipraw.Flip()
    try:
        f.raw_read(path, seconds)
        data = f.fetch(path)
    finally:
        f.close()
    pulses, durs, meta = flipraw.parse(data)
    return pulses, durs, meta


def frame_stats(card):
    """Everything the criterion needs, computed from pac.c's own bitstream builder.
    ⚠ The frame is CYCLIC — it repeats without a gap — so the first and last runs merge
    when they share a level. Forgetting that is what made C441's parity precondition
    necessary in the first place."""
    b = pacdiff.build(card)
    runs, cur = [], 1
    for i in range(1, len(b)):
        if b[i] == b[i - 1]:
            cur += 1
        else:
            runs.append((b[i - 1], cur)); cur = 1
    runs.append((b[-1], cur))
    if len(runs) > 1 and runs[0][0] == runs[-1][0]:
        runs[0] = (runs[0][0], runs[0][1] + runs[-1][1]); runs = runs[:-1]
    hi = [n for lv, n in runs if lv == 1]
    return dict(bits=len(b), ones=sum(b) / len(b), nruns=len(runs),
                himax_us=max(hi) * 256, hi3=sum(1 for n in hi if n >= 3),
                longhi=sum(n for n in hi if n >= 3) / len(b))


def report(tag, pulses, durs, meta, pred_max, pred_duty):
    n = len(pulses)
    # ⛔ An EMPTY capture is not a duty of zero and must never be averaged into anything: a
    # silent emitter and a reader that never listened produce the identical file (C373).
    if n == 0 or sum(durs) == 0:
        print("  %-6s ⛔ EMPTY CAPTURE — nothing on the air, or nothing captured. "
              "Not a measurement; do not read it as one." % tag)
        return None
    slips = meta.get("slips", 0)
    hi, tot = sum(pulses), sum(durs)
    duty = hi / tot
    # the bias b that would reconcile this capture with its own prediction
    b = (hi - pred_duty * tot) / n
    flag = " ⚠ RESYNCS OVER 1% — DISTRUST" if slips > 0.01 * n else ""
    print("  %-6s pairs=%5d resyncs=%d (%.2f%%)%s" % (tag, n, slips, 100.0 * slips / n, flag))
    print("     max HIGH run %6dus   predicted %5dus   %s"
          % (max(pulses), pred_max,
             "within" if max(pulses) <= pred_max + 300 else "⛔ OVER by %dus"
             % (max(pulses) - pred_max)))
    print("     high-time    %6.1f%%     predicted %5.1f%%   b to reconcile = %.0fus"
          % (100 * duty, 100 * pred_duty, b))
    return b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arms", nargs="*", default=["fdxb", "gprox", "pac"])
    ap.add_argument("--cards", nargs="*", default=None,
                    help="run the pac arm once per credential, to see whether the measured "
                         "duty tracks the frame instead of sitting at one value")
    ap.add_argument("--card", default="1337BEEF",
                    help="PAC credential — EIGHT UPPERCASE HEX chars, so the Flipper's "
                         "`CIN: %%08lX` renderer can round-trip it")
    ap.add_argument("--seconds", type=float, default=4.0)
    a = ap.parse_args()

    biases, pacrows = {}, []
    for tag in a.arms:
        typ, ec, pred_max, pred_duty = ARMS[tag]
        if tag != "pac":
            arm(typ, ec)
            time.sleep(1.0)
            biases[tag] = report(tag, *capture(tag, a.seconds), pred_max, pred_duty)
            continue
        for card in (a.cards or [a.card]):
            fs = frame_stats(card)
            print("\n  pac.c's frame for %s: %d bits, duty %.1f%%, %d runs, "
                  "%d HIGH runs >=3 bits, %.1f%% of the frame inside them"
                  % (card, fs["bits"], 100 * fs["ones"], fs["nruns"], fs["hi3"],
                     100 * fs["longhi"]))
            arm(typ, ec % card)
            time.sleep(1.0)
            pulses, durs, meta = capture("pac_%s" % card, a.seconds)
            b = report("pac", pulses, durs, meta, fs["himax_us"], fs["ones"])
            if b is None:
                pacrows.append((card, fs, None, None, None))
                continue
            biases["pac"] = b
            pacrows.append((card, fs, sum(pulses) / sum(durs), max(pulses), b))

    if len(pacrows) > 1:
        print("\n  ⭐ DOES THE MEASURED DUTY TRACK THE FRAME? "
              "(rows ordered by predicted time inside long HIGH runs)")
        print("     card       pred duty  pred long-HIGH  pred runs | measured duty  excess  max run")
        for card, fs, d, mx, b in sorted(pacrows, key=lambda r: r[1]["longhi"]):
            if d is None:
                print("     %-9s   %5.1f%%      %5.1f%%          %3d    |   EMPTY CAPTURE"
                      % (card, 100 * fs["ones"], 100 * fs["longhi"], fs["nruns"]))
                continue
            print("     %-9s   %5.1f%%      %5.1f%%          %3d    |   %5.1f%%      %+5.1f   %6dus"
                  % (card, 100 * fs["ones"], 100 * fs["longhi"], fs["nruns"],
                     100 * d, 100 * (d - fs["ones"]), mx))

    ctrl = [biases[c] for c in CONTROLS if biases.get(c) is not None]
    if ctrl and biases.get("pac") is not None:
        lo, hi = min(ctrl), max(ctrl)
        print("\n  controls imply b in [%.0f, %.0f]us; pac needs %.0fus -> %s"
              % (lo, hi, biases["pac"],
                 "consistent" if lo - 50 <= biases["pac"] <= hi + 50
                 else "⛔ NOT a comparator bias"))
    elif biases.get("pac") is not None:
        print("\n  ⛔ no control in this run — the bias is unconstrained and pac's number is "
              "uninterpretable. Run the controls.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
