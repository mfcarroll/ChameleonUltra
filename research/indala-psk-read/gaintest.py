#!/usr/bin/env python3
"""Is the fc/2 noise floor ADC-referred or analog? Sweep SAADC gain and find out.

    ./gaintest.py [--repeats 7] [--keep DIR]

THE QUESTION. fc/2 measures ~17 counts against an empty-field floor of ~8. Two very
different worlds produce that:

  ADC-REFERRED floor (quantisation, converter noise). Raising gain multiplies the SIGNAL
  in counts while the floor stays put, so SNR improves and the subcarrier lifts clear.
  ⇒ firmware wins it.

  ANALOG-REFERRED floor (noise from the detector and op-amp chain). Gain multiplies both
  equally, SNR is unchanged, and the ADC was never the limit.
  ⇒ nothing in firmware helps.

⭐ THE DISCRIMINATOR IS THE EMPTY-FIELD FLOOR, NOT THE TAG. Watch how it scales:
  floor grows with gain      -> analog-referred
  floor flat as gain grows   -> ADC-referred

⚠ Saturation is the trap. The pedestal is ~1.2V of LF_VBIAS, so higher gain clips, and a
clipped capture still looks like a waveform. Saturated samples are counted and reported
per point; treat any point with a meaningful count as untrustworthy however good its
ratio looks.
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
GAINS = [6, 5, 4, 3]


def capture_path(out, state, gain, rep):
    """One helper for writing AND reading — see oversample_test.py, where two
    positional %d in the same template drifted apart and a verdict was read off nan."""
    return "%s/%s_g%d_r%d.bin" % (out, state, gain, rep)


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
    ap.add_argument("--repeats", type=int, default=7)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--keep")
    a = ap.parse_args()
    out = a.keep or tempfile.mkdtemp(prefix="gaintest_")
    os.makedirs(out, exist_ok=True)

    def run(state, label):
        input("  %s, then press RETURN... " % label)
        cmds = ["hw mode -r"]
        for g in GAINS:
            for r in range(a.repeats):
                cmds.append("lf sniff --timeout %d --bits 16 --gain %d --out %s"
                            % (a.timeout, g, capture_path(out, state, g, r)))
        p = subprocess.run([PY_, CU] + cmds, capture_output=True, text=True)
        if p.returncode != 0:
            sys.exit("cu.py failed:\n" + (p.stdout or "") + (p.stderr or ""))

    run("empty", "Remove EVERY tag from the antenna")
    run("tag", "Place the PSK1 RF/32 tag (block0 00081040) on the LF antenna")

    def measure(state, g):
        sbs, sats, dc = [], [], []
        for r in range(a.repeats):
            fp = capture_path(out, state, g, r)
            if not os.path.exists(fp):
                continue
            x = load16(fp)
            if x is None:
                sys.exit("capture %s is not 16-bit — firmware current?" % fp)
            sats.append(float(np.mean((x >= 16370) | (x <= 8))) * 100)
            dc.append(float(np.median(x)))
            c = deglitch(x)
            if len(c) > 300:
                sbs.append(sideband_rms(c, 62500.0))
        if not sbs:
            sys.exit("⛔ no usable captures for %s at gain 1/%d — refusing to guess." % (state, g))
        return float(np.median(sbs)), float(np.median(sats)), float(np.median(dc))

    print("\n" + "=" * 78)
    print(" SAADC GAIN SWEEP at 62500 Hz — is the floor ADC-referred or analog?")
    print("=" * 78)
    print(" %-7s %8s %10s %10s %11s %9s" % ("gain", "DC", "empty", "tag", "tag/empty", "saturated"))
    rows = []
    for g in GAINS:
        e, esat, edc = measure("empty", g)
        t, tsat, _ = measure("tag", g)
        rows.append((g, e, t, t / e if e else float("nan"), max(esat, tsat), edc))
        print(" 1/%-5d %8.0f %10.2f %10.2f %10.2fx %8.1f%%"
              % (g, edc, e, t, t / e if e else float("nan"), max(esat, tsat)))

    print("\n" + "=" * 78)
    print(" VERDICT")
    print("=" * 78)
    clean = [r for r in rows if r[4] < 2.0]
    if len(clean) < 2:
        print(" ⚠ Fewer than two unsaturated gains — cannot compare. The pedestal is")
        print("   eating the headroom; differential mode against LF_RSSI would be needed")
        print("   to subtract it before gain can be raised.")
        return
    g0, e0 = clean[0][0], clean[0][1]
    gN, eN = clean[-1][0], clean[-1][1]
    gain_ratio = g0 / gN                       # divisors: 1/6 -> 1/3 is 2x more gain
    floor_ratio = eN / e0
    print(" over the unsaturated range 1/%d -> 1/%d, gain rose %.2fx" % (g0, gN, gain_ratio))
    print(" the EMPTY-FIELD floor rose %.2fx  (analog would predict %.2fx, ADC-referred 1.00x)"
          % (floor_ratio, gain_ratio))
    snr0, snrN = clean[0][3], clean[-1][3]
    print(" tag/empty: %.2fx -> %.2fx  (%+.1f dB)" % (snr0, snrN, 20 * np.log10(snrN / snr0)))
    if floor_ratio > 0.75 * gain_ratio:
        print("\n ⛔ The floor scales with gain: it is ANALOG-referred. The ADC was never the")
        print("   limit, and no gain, resolution or sample-rate change improves fc/2.")
    elif floor_ratio < 0.3 * gain_ratio:
        print("\n ⇒ The floor barely moves: it is ADC-REFERRED. Gain is real SNR, and")
        print("   differential mode against LF_RSSI would buy considerably more.")
    else:
        print("\n ⇒ Mixed: the floor grows, but slower than gain. Both contribute, and")
        print("   there is some headroom left in the converter.")
    print("\n captures in %s%s" % (out, "" if a.keep else " (temporary)"))


if __name__ == "__main__":
    main()
