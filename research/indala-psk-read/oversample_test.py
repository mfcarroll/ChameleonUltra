#!/usr/bin/env python3
"""Decide whether fc/2 is lost in the ANALOG chain or merely at the sampler.

    ./oversample_test.py [--repeats 7] [--keep DIR]

THE ARGUMENT. Sampling once per carrier period puts a tag's fc/2 subcarrier at exactly
2 samples/cycle, where recovery scales with cos(phi) for a fixed phase. Phase tuning
measured 7.5 dB of swing there — real, but far short of the ~34 dB the Chameleon loses
against a Proxmark at fc/2. So either the residue is analog, or the Nyquist sampling is
costing more than the phase sweep could show.

⇒ Free-running the trigger at 200kHz settles it. fc/2 then sits at 3.2 samples/cycle —
  comfortably oversampled, ASYNCHRONOUS to the field, no degeneracy at any phase. If the
  subcarrier is present in the analog signal, this recovers it in full.

  fc/2 jumps at 200kHz   -> the sampler was the blocker after all; firmware fixes it.
  fc/2 stays where it is -> the signal never reached the ADC. Analog, and only a
                            hardware change moves it.

RF/4 (31250 Hz) is carried through as a positive control: it is well sampled in BOTH
modes, so its ratio should barely move. If it DOES move, the two modes differ for some
reason other than Nyquist and the fc/2 comparison is not clean.
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
SETTLE = 400
MODES = [(0, 125000.0, "125kHz carrier-locked"), (200, 200000.0, "200kHz free-running")]


def capture_path(out, state, rate, rep):
    """⚠ ONE function for both writing and reading. The writer formatted
    (state, rate, rep) and the reader (state, rep, rate) — two positional %d in the same
    template, silently swapped. Every 200kHz file was written and then looked for under a
    name that did not exist, the measurement came back nan, and the verdict printed
    "fc/2 does NOT recover" off missing data."""
    return "%s/%s_r%d_%d.bin" % (out, state, rate, rep)


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


def sideband_rms(x, f, fs):
    """RMS in the modulation skirt below f. Off-Nyquist and off the suppressed carrier,
    so it is comparable between the two sample rates."""
    y = x - x.mean()
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w))
    fr = np.fft.rfftfreq(len(y), 1 / fs)
    m = (fr >= f - 5000) & (fr <= f - 200)
    return 2 * np.sqrt((S[m] ** 2).sum()) / w.sum()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repeats", type=int, default=7)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--keep")
    a = ap.parse_args()
    out = a.keep or tempfile.mkdtemp(prefix="oversample_")
    os.makedirs(out, exist_ok=True)

    def run(tag_state, label):
        input("  %s, then press RETURN... " % label)
        cmds = ["hw mode -r"]
        for rate, _, _ in MODES:
            for r in range(a.repeats):
                cmds.append("lf sniff --timeout %d --bits 16 %s--out %s"
                            % (a.timeout, ("--rate %d " % rate) if rate else "",
                               capture_path(out, tag_state, rate, r)))
        p = subprocess.run([PY_, CU] + cmds, capture_output=True, text=True)
        if p.returncode != 0:
            sys.exit("cu.py failed:\n" + (p.stdout or "") + (p.stderr or ""))

    run("empty", "Remove EVERY tag from the antenna")
    run("tag", "Place the PSK1 RF/32 tag (block0 00081040) on the LF antenna")

    print("\n" + "=" * 76)
    print(" OVERSAMPLING TEST — is fc/2 lost in the analog chain or at the sampler?")
    print("=" * 76)
    print(" %-24s %9s %10s %10s %11s" % ("mode", "freq", "empty", "tag", "tag/empty"))
    results = {}
    for rate, fs, name in MODES:
        for f, fname in ((62500.0, "fc/2"), (31250.0, "fc/4 ctrl")):
            vals = {}
            for state in ("empty", "tag"):
                got = []
                for r in range(a.repeats):
                    fp = capture_path(out, state, rate, r)
                    if not os.path.exists(fp):
                        continue
                    x = load16(fp)
                    if x is None:
                        sys.exit("capture %s is not 16-bit — firmware current?" % fp)
                    c = deglitch(x)
                    if len(c) > 300:
                        got.append(sideband_rms(c, f, fs))
                vals[state] = float(np.median(got)) if got else float("nan")
            if not np.isfinite(vals["empty"]) or not np.isfinite(vals["tag"]):
                sys.exit("⛔ no usable captures for %s at %.0f Hz.\n"
                         "   Refusing to report a verdict on missing data — that is how the\n"
                         "   nan run of 2026-09-11 concluded 'fc/2 does NOT recover' from\n"
                         "   files it had simply failed to find." % (name, f))
            ratio = vals["tag"] / vals["empty"] if vals["empty"] else float("nan")
            results[(rate, f)] = ratio
            print(" %-24s %8.0fHz %10.2f %10.2f %10.2fx"
                  % (name if f == 62500.0 else "", f, vals["empty"], vals["tag"], ratio))

    print("\n" + "=" * 76)
    print(" VERDICT")
    print("=" * 76)
    lock2, over2 = results[(0, 62500.0)], results[(200, 62500.0)]
    lock4, over4 = results[(0, 31250.0)], results[(200, 31250.0)]
    print(" fc/2  ratio: %.2fx locked -> %.2fx oversampled   (%+.1f dB)"
          % (lock2, over2, 20 * np.log10(over2 / lock2) if lock2 > 0 else float('nan')))
    print(" fc/4  ratio: %.2fx locked -> %.2fx oversampled   (%+.1f dB)  <- control"
          % (lock4, over4, 20 * np.log10(over4 / lock4) if lock4 > 0 else float('nan')))

    ctrl_moved = lock4 > 0 and abs(20 * np.log10(over4 / lock4)) > 6
    gain = 20 * np.log10(over2 / lock2) if lock2 > 0 else 0.0
    if ctrl_moved:
        print("\n ⚠ THE CONTROL MOVED. fc/4 is well sampled in both modes and should not")
        print("   change much. Something other than Nyquist differs between the modes, so")
        print("   the fc/2 comparison is not clean. Fix that before reading anything into it.")
    elif gain > 10:
        print("\n ⇒ fc/2 JUMPS when oversampled. The subcarrier was reaching the ADC all")
        print("   along and the carrier-locked trigger was destroying it. This is a")
        print("   FIRMWARE fix: oversample, and port the decoder.")
    elif over2 > 3:
        print("\n ⇒ fc/2 is present but not much better oversampled. Some of the loss is")
        print("   the sampler, most is not. Marginal — measure a real demod before")
        print("   committing to the decoder port.")
    else:
        print("\n ⛔ fc/2 does NOT recover when oversampled. The subcarrier never reaches")
        print("   the ADC: the loss is ANALOG, ahead of digitisation, and no firmware")
        print("   change can retrieve it. A front-end modification is the only path.")
    print("\n captures in %s%s" % (out, "" if a.keep else " (temporary)"))


if __name__ == "__main__":
    main()
