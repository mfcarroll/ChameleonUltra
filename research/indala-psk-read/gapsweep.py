#!/usr/bin/env python3
"""Sweep the air gap and ask whether it changes the CHAIN'S FREQUENCY RESPONSE.

    ./gapsweep.py --gaps flat,1mm,2mm,3mm,5mm,8mm [--repeats 5] [--keep DIR]

WHY GAP COULD MATTER, MECHANISTICALLY. At zero gap the tag is tightly coupled and loads
the reader's LC tank hard. Overcoupling damps and can split the resonance, which is a
FREQUENCY-DEPENDENT effect — exactly the shape of loss this project is chasing. Backing
the tag off trades signal amplitude for a less disturbed tank. The Momentum
`t5577-deep-read` corpus found air gap the dominant variable for Flipper reads, and every
capture in this project has been taken at one position.

⭐ THE MEASURE IS A RATIO, NOT AN AMPLITUDE. Moving the tag changes how hard it couples,
so fc/2 alone conflates "the chain passes fc/2 better" with "the tag is simply louder".
Two bands are therefore measured on the same capture:

    62500 Hz  the fc/2 subcarrier — the thing that is missing
    31250 Hz  the 8th harmonic of the RF/32 bit rate (3906.25 x 8), which the front end
              passes well, so it tracks how strongly the tag is coupling

  fc2 / h8 is then the chain's frequency response with coupling divided out.

  ratio rises with gap  -> gap changes the RESPONSE. A real lever; find the optimum.
  ratio flat            -> gap only changes loudness. Nothing here for fc/2.

⚠ One empty-field pass is enough: with no tag there is nothing for the gap to change.
"""
import argparse
import os
import subprocess
import sys
import tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CU = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", "cu.py"))
PY_ = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", ".venv", "bin", "python"))
SETTLE, FS = 400, 125000.0
FC2, H8 = 62500.0, 31250.0


def capture_path(out, label, rep):
    return "%s/%s_r%d.bin" % (out, label.replace("/", "-"), rep)


def load16(path):
    raw = np.frombuffer(open(path, "rb").read(), dtype=np.uint8)
    if len(raw) < 2 or len(raw) % 2 or raw[0::2].max() > 0x3F:
        return None
    return (raw[0::2].astype(np.uint16) << 8 | raw[1::2]).astype(float)[SETTLE:]


def deglitch(x, w=50, k=4.0):
    n = len(x) // w
    if n == 0:
        return x
    pp = np.array([np.ptp(x[i * w:(i + 1) * w]) for i in range(n)])
    med = np.median(pp)
    keep = [x[i * w:(i + 1) * w] for i in range(n) if pp[i] <= k * med]
    return np.concatenate(keep) if keep else np.array([])


def sideband_rms(x, f):
    y = x - x.mean()
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w))
    fr = np.fft.rfftfreq(len(y), 1 / FS)
    m = (fr >= f - 5000) & (fr <= f - 200)
    return 2 * np.sqrt((S[m] ** 2).sum()) / w.sum()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gaps", default="flat,1mm,2mm,3mm,5mm,8mm")
    ap.add_argument("--repeats", type=int, default=5)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--settle", type=int, default=0)
    ap.add_argument("--keep")
    a = ap.parse_args()
    gaps = [g.strip() for g in a.gaps.split(",") if g.strip()]
    out = a.keep or tempfile.mkdtemp(prefix="gapsweep_")
    os.makedirs(out, exist_ok=True)

    def run(label, prompt):
        input("  %s, then press RETURN... " % prompt)
        cmds = ["hw mode -r"] + [
            "lf sniff --timeout %d --bits 16 --settle %d --out %s"
            % (a.timeout, a.settle, capture_path(out, label, r)) for r in range(a.repeats)]
        p = subprocess.run([PY_, CU] + cmds, capture_output=True, text=True)
        if p.returncode != 0:
            sys.exit("cu.py failed:\n" + (p.stdout or "") + (p.stderr or ""))

    def measure(label):
        f2, h8, dc, kept = [], [], [], []
        for r in range(a.repeats):
            fp = capture_path(out, label, r)
            if not os.path.exists(fp):
                continue
            x = load16(fp)
            if x is None:
                sys.exit("capture %s is not 16-bit — firmware current?" % fp)
            dc.append(float(np.median(x)))
            c = deglitch(x)
            kept.append(100.0 * len(c) / len(x))
            if len(c) > 300:
                f2.append(sideband_rms(c, FC2))
                h8.append(sideband_rms(c, H8))
        if not f2:
            sys.exit("⛔ no usable captures for '%s' — refusing to guess." % label)
        return (float(np.median(f2)), float(np.median(h8)),
                float(np.median(dc)), float(np.median(kept)))

    run("empty", "Remove EVERY tag from the antenna")
    for g in gaps:
        run(g, "Hold the PSK1 RF/32 tag at %s" % g)

    e2, eh, edc, _ = measure("empty")
    print("\n" + "=" * 80)
    print(" AIR-GAP SWEEP — does gap change the chain's frequency response?")
    print("=" * 80)
    print(" %-8s %8s %10s %10s %12s %10s %7s"
          % ("gap", "DC", "fc2 62.5k", "h8 31.2k", "fc2/h8", "fc2/empty", "kept"))
    print(" %-8s %8.0f %10.2f %10.2f %12s %10s %7s" % ("(empty)", edc, e2, eh, "-", "-", "-"))
    rows = []
    for g in gaps:
        f2, h8, dc, kp = measure(g)
        rows.append((g, f2, h8, f2 / h8 if h8 else float("nan"), dc))
        print(" %-8s %8.0f %10.2f %10.2f %11.4f %9.2fx %6.0f%%"
              % (g, dc, f2, h8, f2 / h8 if h8 else float("nan"),
                 f2 / e2 if e2 else float("nan"), kp))

    print("\n" + "=" * 80)
    print(" VERDICT")
    print("=" * 80)
    # ⛔ SUBTRACT THE FLOOR, AND DROP POINTS SITTING ON IT. The raw fc2/h8 ratio includes
    # the empty-field floor in both terms, so once the tag stops reaching fc/2 the ratio
    # stops measuring the tag and starts measuring noise/noise — which drifts back UP and
    # fakes a recovery at wide gaps. The first run of this script called a clean monotonic
    # decline "flat across gaps" for exactly that reason.
    usable = [(g, (f2 - e2) / (h8 - eh), f2, h8)
              for g, f2, h8, _r, _dc in [(r[0], r[1], r[2], r[3], r[4]) for r in rows]
              if f2 > 1.5 * e2 and h8 > 1.5 * eh]
    if len(usable) < 2:
        print(" ⚠ fc/2 sits on the noise floor at all but one gap — nothing to compare.")
        return
    print(" %-8s %12s %12s" % ("gap", "net fc2/h8", "fc2 absolute"))
    for g, r, f2, _h8 in usable:
        print(" %-8s %12.3f %12.2f" % (g, r, f2))
    dropped = [r[0] for r in rows if not (r[1] > 1.5 * e2 and r[2] > 1.5 * eh)]
    if dropped:
        print(" (dropped, fc/2 at the floor: %s)" % ", ".join(dropped))

    vals = [r for _g, r, _f, _h in usable]
    best_resp = usable[int(np.argmax(vals))]
    best_abs = max(usable, key=lambda u: u[2])
    declining = all(b <= a * 1.05 for a, b in zip(vals, vals[1:]))
    print("\n best RESPONSE  : %s (%.3f)" % (best_resp[0], best_resp[1]))
    print(" best ABSOLUTE  : %s (fc2 %.2f)" % (best_abs[0], best_abs[2]))
    if declining:
        print("\n ⛔ The response DECLINES monotonically as the gap opens. Tighter coupling")
        print("   is better, so the overcoupling mechanism is refuted — and the best gap is")
        print("   the one every prior measurement already used. No improvement available.")
    elif best_resp[0] != best_abs[0]:
        print("\n ⚠ Best response and best absolute signal are at DIFFERENT gaps. A better")
        print("   ratio at a wide gap is not useful if fc/2 is quieter there in absolute")
        print("   terms — a demodulator needs signal above the floor, not a favourable")
        print("   ratio between two small numbers. Trust the absolute column.")
    else:
        print("\n ⇒ %s is best on both response and absolute signal. Re-run the PSKCF sweep"
              % best_resp[0])
        print("   there before concluding anything about the front end.")
    print("\n captures in %s%s" % (out, "" if a.keep else " (temporary)"))


if __name__ == "__main__":
    main()
