#!/usr/bin/env python3
"""Does LF_RSSI (AIN0) beat LF_OA_OUT (AIN5)? The only tap upstream of the filter poles.

    ./inputtest.py [--repeats 7] [--phase 32] [--keep DIR]

THE QUESTION. Every capture in this project sampled AIN5, which sits after BOTH RC poles:

    ANT -> VD1 detector -> LF_OA -> [C28 10n / R9 82 / C36 33n] -> IC1A (R17 4k7 / C38 1n)
                             |                                      -> IC1B -> AIN5 (P0.29)
                             \\-> R12 470k -> LF_RSSI -> AIN0 (P0.02)

R9/C36 = 58.8kHz and R17/C38 = 33.9kHz cost -3.3dB and -6.4dB at an fc/2 = 62.5kHz
subcarrier, so ~9.7dB of the measured 31.2dB deficit is spent before the converter. AIN0
hangs off LF_OA itself, ahead of both.

⚠ THE POLES ONLY COST SNR FOR NOISE MADE DOWNSTREAM OF THEM. They attenuate upstream noise
along with the signal. So the 9.7dB is recoverable only if the noise is added at or after
IC1A — which is exactly what comparing the two nodes' tag/empty ratios measures.

⭐ READ THE WITHIN-NODE RATIO, NEVER THE RAW COUNTS ACROSS NODES. The two taps have
different DC pedestals and different scales; comparing AIN0's absolute fc/2 against AIN5's
floor is the same error that made band RMS look comparable across bands whose floors
differ 17x. tag/empty within a node is the only figure that travels.

⚠ HEADROOM IS THE CONFOUND HERE. AIN0 is a field-strength indicator, not a signal node, and
it may sit near the rail. A capture pinned at full scale has its AC compressed away and
returns a FLAT spectrum that looks exactly like a dead node. %FS is reported per node for
that reason: read it before believing any ratio.
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
SETTLE, FS, FULL_SCALE = 400, 125000.0, 16383
INPUTS = [(5, "AIN5/LF_OA_OUT"), (0, "AIN0/LF_RSSI")]
BANDS = [(1000, 5000), (10000, 20000), (28000, 36000), (55000, 62000)]


def capture_path(out, state, ain, rep):
    """One helper for writing AND reading — in oversample_test.py two positional %d in
    the same template drifted apart and a verdict was read off nan."""
    return "%s/%s_ain%d_r%d.bin" % (out, state, ain, rep)


def load16(path):
    raw = np.frombuffer(open(path, "rb").read(), dtype=np.uint8)
    if len(raw) < 2 or len(raw) % 2 or raw[0::2].max() > 0x3F:
        return None
    return (raw[0::2].astype(np.uint16) << 8 | raw[1::2]).astype(float)[SETTLE:]


def deglitch(x, w=50, k=4.0):
    """Scale-free: drop windows whose peak-to-peak exceeds k x the median window's. A
    fixed threshold tuned on one node discarded 90 of 90 windows on another and returned
    nan."""
    n = len(x) // w
    if n == 0:
        return x
    pp = np.array([np.ptp(x[i * w:(i + 1) * w]) for i in range(n)])
    med = np.median(pp)
    keep = [x[i * w:(i + 1) * w] for i in range(n) if pp[i] <= k * med]
    return np.concatenate(keep) if keep else np.array([])


def band_rms(x, lo, hi):
    y = x - x.mean()
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w))
    fr = np.fft.rfftfreq(len(y), 1 / FS)
    m = (fr >= lo) & (fr <= hi)
    return 2 * np.sqrt((S[m] ** 2).sum()) / w.sum()


def sideband(x, f=62500.0):
    """The modulation skirt, not the subcarrier bin. BPSK suppresses its own carrier, and
    at fc/2 that bin sits exactly at Nyquist."""
    return band_rms(x, f - 5000, f - 200)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repeats", type=int, default=7)
    ap.add_argument("--phase", type=int, default=32, help="32 ticks is the measured optimum")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--keep")
    a = ap.parse_args()
    out = a.keep or tempfile.mkdtemp(prefix="inputtest_")
    os.makedirs(out, exist_ok=True)

    def run(state, label):
        input("  %s, then press RETURN... " % label)
        cmds = ["hw mode -r"]
        for ain, _ in INPUTS:
            for r in range(a.repeats):
                cmds.append("lf sniff --timeout %d --bits 16 --phase %d --input %d --out %s"
                            % (a.timeout, a.phase, ain, capture_path(out, state, ain, r)))
        p = subprocess.run([PY_, CU] + cmds, capture_output=True, text=True)
        if p.returncode != 0:
            sys.exit("cu.py failed:\n" + (p.stdout or "") + (p.stderr or ""))

    print(__doc__.split("THE QUESTION")[0])
    run("empty", "Remove EVERY tag from the antenna")
    run("tag", "Place the INDALA26 tag (a0000000e6bd0e92) on the LF antenna")

    def measure(state, ain):
        sbs, dcs, sds, bands = [], [], [], []
        for r in range(a.repeats):
            fp = capture_path(out, state, ain, r)
            if not os.path.exists(fp):
                # ⛔ Absent captures are fatal, not skipped. Silently skipping is how a
                # whole sweep once reduced to nan without saying so.
                sys.exit("⛔ missing capture %s — run aborted rather than guessed." % fp)
            x = load16(fp)
            if x is None:
                sys.exit("⛔ %s is not 16-bit — is the firmware current?" % fp)
            dcs.append(float(np.median(x)))
            c = deglitch(x)
            if len(c) < 300:
                continue
            sds.append(float(c.std()))
            sbs.append(sideband(c))
            bands.append([band_rms(c, lo, hi) for lo, hi in BANDS])
        if not sbs:
            sys.exit("⛔ no usable captures for %s on AIN%d — refusing to guess." % (state, ain))
        return (np.array(sbs), float(np.median(dcs)), float(np.median(sds)),
                np.median(np.array(bands), axis=0))

    print("\n" + "=" * 86)
    print(" SAADC INPUT NODE — is LF_RSSI (AIN0) upstream of the poles worth anything?")
    print(" phase %d ticks, %d repeats, %dms per capture" % (a.phase, a.repeats, a.timeout))
    print("=" * 86)

    res = {}
    for ain, name in INPUTS:
        print("\n %s" % name)
        print("   %-6s %8s %6s %8s   %9s %9s %9s %9s" %
              ("state", "DC", "%FS", "clean σ", "1-5k", "10-20k", "28-36k", "55-62k"))
        for state in ("empty", "tag"):
            sb, dc, sd, bd = measure(state, ain)
            res[(ain, state)] = sb
            print("   %-6s %8.0f %5.1f%% %8.1f   %9.2f %9.2f %9.2f %9.2f" %
                  (state, dc, 100.0 * dc / FULL_SCALE, sd, bd[0], bd[1], bd[2], bd[3]))
            if state == "empty":
                # rolloff shape: a node behind an RC pole falls off, a pure noise floor
                # does not. Reported, not thresholded.
                print("     rolloff 1-5k -> 55-62k: %.2fx  (%.1f dB)" %
                      (bd[0] / bd[3], 20 * np.log10(bd[0] / bd[3])))
            if 100.0 * dc / FULL_SCALE > 90.0:
                print("     ⚠ DC is %.1f%% of full scale. 1/6 is already the LOWEST gain the"
                      % (100.0 * dc / FULL_SCALE))
                print("       SAADC offers, so single-ended there is NO setting with more")
                print("       headroom. A flat spectrum here may be compression, not the node.")

    print("\n" + "=" * 86)
    print(" fc/2 SIDEBAND — within-node tag/empty, which is the only comparable figure")
    print("=" * 86)
    print(" %-16s %10s %10s %11s %10s" % ("node", "empty", "tag", "tag/empty", "spread"))
    ratios = {}
    for ain, name in INPUTS:
        e, t = res[(ain, "empty")], res[(ain, "tag")]
        em, tm = float(np.median(e)), float(np.median(t))
        ratios[ain] = tm / em if em else float("nan")
        # per-repeat spread of the RATIO's inputs, so the reader can see whether any
        # difference between nodes clears the scatter. n=1 was wrong three times here.
        spread = max(float(np.max(t) / np.min(t)), float(np.max(e) / np.min(e)))
        print(" %-16s %10.2f %10.2f %10.2fx %9.2fx" % (name, em, tm, ratios[ain], spread))

    print("\n" + "=" * 86)
    print(" VERDICT")
    print("=" * 86)
    r5, r0 = ratios[5], ratios[0]
    print(" AIN5 tag/empty %.2fx   AIN0 tag/empty %.2fx" % (r5, r0))
    if not np.isfinite(r0) or r0 <= 0:
        print(" ⛔ AIN0 produced no usable ratio.")
        return
    print(" AIN0 relative to AIN5: %+.1f dB" % (20 * np.log10(r0 / r5)))
    print("\n The two filter poles cost -9.7 dB at 62.5kHz, so that is the most AIN0 could")
    print(" return if the noise were made entirely downstream of them. Read the delta above")
    print(" against that ceiling, and against the spread column — not as a bare number.")
    print("\n captures in %s%s" % (out, "" if a.keep else " (temporary)"))


if __name__ == "__main__":
    main()
