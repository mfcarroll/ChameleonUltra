#!/usr/bin/env python3
"""Is C486's beat the WHOLE story? Demodulate a single null-free window and see.

    ./nullframe.py                      # indala, with pac as the machinery control
    ./nullframe.py --proto keri --json

⭐⭐ THE CRITERION, WRITTEN BEFORE THE CAPTURE (M55). C486 measured that our subcarrier
free-runs against the reader's clock and beats through nulls: a 9.0x amplitude swing with a deep
null roughly every 80 ms. It concluded that PSK cannot decode. ⚠ BUT A BEAT IS NOT A CONSTANT
IMPAIRMENT — between two nulls there is a lobe tens of ms wide where the subcarrier is strong and
its phase is merely rotating slowly. An Indala frame is 16.4 ms. So C486's own numbers predict
that SOME frames land in a clean lobe, and the question this tool asks is whether such a frame
decodes when it is handed to a demodulator ON ITS OWN.

    P1 — C486 IS SUFFICIENT. A null-free window exists AND at least one of them yields the
         armed credential from `lf <proto> demod` or `data rawdemod --p1`. ⇒ the emitter is
         fine inside a lobe, the defect is entirely the beat, and what kills the LIVE reader is
         that it estimates clock and threshold across a whole buffer that swings 9x.

    P0 — C486 IS NECESSARY BUT NOT SUFFICIENT. Null-free windows exist, the CONTROL arm decodes
         from an equally short window, and NO target window decodes. ⇒ something besides the
         beat is also wrong, and the six-gap story is incomplete.

    INCONCLUSIVE — no null-free window exists in the capture, or the control does not decode
         from its own window. The second is the one that matters: without it, "no window
         decoded" measures the window LENGTH and this tool's own file handling, not the air.

⛔ THE CONTROL IS NOT OPTIONAL AND IT CAN FAIL. A 6144-sample window is ~1.5 frames; a demod
that needs a preamble plus margin may simply refuse it. `pac` is armed, captured, windowed, saved
and loaded through the IDENTICAL code path, and if its window does not decode then neither
verdict above is available. ⭐ pac decodes both live and after save/load (C488), so it is a
control that could have failed and is expected to pass.

⛔⛔ THIS GRADES NOTHING. No null sweep, no calibration row, no licence — it is a capture and a
host demod, exactly like C487/C488, and it makes no claim about any cell in the matrix.

HOW THE ENVELOPE IS TAKEN. The pm3 samples once per 8 us carrier cycle, so an fc/2 subcarrier
sits at exactly fs/2 and appears as sample-to-sample alternation. `|x[n] - x[n+1]|/2` is that
alternation with DC and any slow drift differenced away, and a 256-sample (2 ms) moving average
of it is the beat envelope — the same 2 ms window C486 used. ⚠ Phase FLIPS (the data) do not
disturb it because the magnitude is taken; only the beat moves it.
"""
import argparse
import glob
import json
import os
import subprocess
import sys

import numpy as np

import pm3cap
import seqdump

HERE = os.path.dirname(os.path.abspath(__file__))
PM3 = pm3cap.PM3
OUT = "/tmp/nullframe"

# name -> (slot type, econfig, marker that must appear in the demod output)
ARMS = {
    "indala":   ("Indala", "lf indala econfig -s %d --id a0000000e6bd0e92",
                 "a0000000e6bd0e92", "lf indala demod"),
    "keri":     ("Keri", "lf keri econfig -s %d --id 80003039",
                 None, "lf keri demod"),
    "idteck":   ("IDTECK", "lf idteck econfig -s %d --id 4944544b55667788",
                 None, "lf idteck demod"),
    "pac":      ("PAC", "lf pac econfig -s %d --cn 1337BEEF",
                 None, "lf pac demod"),
}


def pm3(cmd, timeout=120):
    r = subprocess.run([PM3, "-c", cmd], capture_output=True, text=True, timeout=timeout)
    return r.stdout + r.stderr


def envelope(vals, smooth):
    """Beat envelope: the fs/2 alternation magnitude, 2 ms moving average."""
    alt = np.abs(np.diff(vals)) / 2.0
    k = np.ones(smooth) / smooth
    return np.convolve(alt, k, mode="same")


def windows(env, width, stride, null_frac):
    """Every window start whose worst envelope sample clears null_frac of the global peak."""
    peak = float(env.max())
    floor = null_frac * peak
    out = []
    for s in range(0, len(env) - width, stride):
        m = float(env[s:s + width].min())
        out.append((s, m, m >= floor))
    return out, peak


def save_window(vals, path):
    """Write a slice in `data save`'s own format so `data load` takes it back unchanged."""
    with open(path, "w") as f:
        f.write("\n".join(str(int(round(v))) for v in vals) + "\n")
    return path


def demod(path, cmd):
    return pm3("data load -f %s; %s; data rawdemod --p1" % (path, cmd))


def capture_arm(port, name, samples, seconds_note):
    typ, ec, marker, cmd = ARMS[name]
    ok, why = seqdump.arm(port, typ, ec)
    if not ok:
        return None, "ARM FAILED: " + why
    vals, err = pm3cap.capture(samples)
    if vals is None:
        return None, "CAPTURE FAILED: " + str(err)
    return vals, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--proto", default="indala", choices=sorted(ARMS))
    ap.add_argument("--control", default="pac", choices=sorted(ARMS))
    ap.add_argument("--port", default=seqdump.CU2_PORT)
    ap.add_argument("--samples", type=int, default=40000)
    ap.add_argument("--window", type=int, default=6144,
                    help="samples; 6144 = 49 ms = 1.5 Indala frames")
    ap.add_argument("--stride", type=int, default=64)
    ap.add_argument("--smooth", type=int, default=256, help="samples; 256 = 2 ms, as C486")
    ap.add_argument("--null-frac", type=float, default=0.25,
                    help="a window is null-free if its worst sample clears this x global peak")
    ap.add_argument("--tries", type=int, default=3, help="best null-free windows to demodulate")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    os.makedirs(OUT, exist_ok=True)
    report = {"proto": a.proto, "control": a.control, "window": a.window,
              "null_frac": a.null_frac, "verdict": None}

    try:
        for role, name in (("target", a.proto), ("control", a.control)):
            marker = ARMS[name][2]
            cmd = ARMS[name][3]
            print("\n=== %s: %s ===" % (role, name))
            vals, err = capture_arm(a.port, name, a.samples, None)
            if vals is None:
                print("  " + err)
                report[role] = {"error": err}
                continue
            env = envelope(vals, a.smooth)
            wins, peak = windows(env, a.window, a.stride, a.null_frac)
            clean = sorted([w for w in wins if w[2]], key=lambda w: -w[1])
            worst = sorted(wins, key=lambda w: w[1])
            swing = peak / max(float(env.min()), 1e-9)
            print("  %d samples, envelope peak %.1f min %.1f (swing %.1fx)"
                  % (len(vals), peak, float(env.min()), swing))
            print("  %d of %d windows are null-free at %.2f x peak"
                  % (len(clean), len(wins), a.null_frac))

            rec = {"samples": len(vals), "peak": peak, "env_min": float(env.min()),
                   "swing": swing, "clean": len(clean), "total": len(wins), "hits": []}

            full = save_window(vals, os.path.join(OUT, "%s_full.pm3" % name))
            out = demod(full, cmd)
            rec["full_decodes"] = bool(marker and marker.lower() in out.lower())
            print("  whole capture: %s" % ("DECODES" if rec["full_decodes"] else "silent"))

            for i, (s, m, _) in enumerate(clean[:a.tries]):
                p = save_window(vals[s:s + a.window], os.path.join(OUT, "%s_w%d.pm3" % (name, i)))
                out = demod(p, cmd)
                hit = bool(marker and marker.lower() in out.lower())
                rec["hits"].append({"start": s, "worst_env": m, "decodes": hit,
                                    "tail": out.strip()[-400:]})
                print("  window @%d (worst env %.1f): %s" % (s, m, "DECODES" if hit else "silent"))
                if not marker:
                    print("    ---- output, no marker registered for this arm ----")
                    print("    " + out.strip()[-400:].replace("\n", "\n    "))

            if worst:
                s, m, _ = worst[0]
                p = save_window(vals[s:s + a.window], os.path.join(OUT, "%s_null.pm3" % name))
                out = demod(p, cmd)
                rec["null_window"] = {"start": s, "worst_env": m,
                                      "decodes": bool(marker and marker.lower() in out.lower())}
                print("  NULL window @%d (worst env %.1f): %s"
                      % (s, m, "DECODES" if rec["null_window"]["decodes"] else "silent"))
            report[role] = rec
    finally:
        print("\ndisarming cu2 ...")
        seqdump.disarm(a.port)

    t, c = report.get("target", {}), report.get("control", {})
    if not c.get("hits") or not any(h["decodes"] for h in c.get("hits", [])):
        report["verdict"] = ("INCONCLUSIVE — the control did not decode from a %d-sample window, "
                             "so a silent target window measures the window length or this "
                             "tool, not the air." % a.window)
    elif not t.get("clean"):
        report["verdict"] = "INCONCLUSIVE — no null-free window in the target capture."
    elif any(h["decodes"] for h in t.get("hits", [])):
        report["verdict"] = ("P1 — C486 IS SUFFICIENT. A null-free window decodes on its own, so "
                             "the emission inside a lobe is good and the beat is the whole defect.")
    else:
        report["verdict"] = ("P0 — C486 IS NECESSARY BUT NOT SUFFICIENT. Null-free windows exist, "
                             "the control decodes from one, and the target does not. Something "
                             "besides the beat is also wrong.")
    print("\n" + report["verdict"])
    if a.json:
        print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
