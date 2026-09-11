#!/usr/bin/env python3
"""Walk the SAADC sample phase and watch what it does to an fc/2 subcarrier.

    ./phasesweep.py [--step 4] [--timeout 300] [--freq 62500] [--keep DIR]

THE HYPOTHESIS THIS TESTS. The SAADC sample task is PPI-triggered from the carrier
PWM, so it fires once per 8us carrier period at one fixed phase. A T5577 derives its
PSK subcarrier by dividing the same field, so at PSKCF RF/2 the subcarrier is 62.5kHz
— exactly 2 samples/cycle — AND phase-locked to that trigger. Recovered amplitude then
goes as cos(phi) for a constant phi, and an unlucky phi nulls it to any depth.

⇒ PREDICTION, and it is a sharp one: sweep phi and the fc/2 amplitude should trace a
  |cos| — deep nulls twice per carrier period, maxima between them. 128 ticks span one
  carrier period, which is half a subcarrier cycle, so expect ONE null and ONE peak
  across the range.

⛔ WHAT WOULD FALSIFY IT: the TAG-TO-EMPTY RATIO staying flat across phase.

⚠⚠ A BARE SWEEP PROVES NOTHING, AND ALMOST FOOLED THIS SCRIPT. Measured with NO TAG on
the antenna, fc/2 amplitude still varied 3907x across phase (0.01 to 29.88) and the
capture std ranged 293 to 2629. Of course it does: the SAADC samples the peak
detector's own ripple, and which point of that ripple you land on IS the phase. So
"amplitude at 62.5kHz moves with phase" is true of an EMPTY FIELD and says nothing
about a tag. An earlier version of this script printed a confident cos(phi) verdict on
exactly that no-tag run.

⇒ So the experiment is PAIRED: sweep once with nothing on the antenna, once with the
  tag, and read the RATIO. Only the ratio carries the tag.

Run with a PSK1 RF/32 tag (block0 00081040) — that is what the campaign leaves behind.
"""
import argparse
import os
import subprocess
import sys
import tempfile
import numpy as np

FS = 125000.0
SETTLE = 400
HERE = os.path.dirname(os.path.abspath(__file__))
CU = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", "cu.py"))
PY = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", ".venv", "bin", "python"))


def deglitch(x, w=50, k=4.0):
    """Drop windows whose peak-to-peak is far above the median window's.

    ⛔⛔ WITHOUT THIS THE SWEEP MEASURES NOTHING. `lf sniff` captures land randomly in
    two states: clean, or carrying a USB-transfer buffer overrun (lf_reader_generic.c:30
    — "buffer full, oldest samples dropped"). An overrun is a FULL-SCALE discontinuity
    (measured: single-sample jumps of 16380 of 16383) and it dumps broadband energy into
    every bin, fc/2 included. Measured at a FIXED phase with the tag untouched, the fc/2
    sideband varied 49x run to run (15.3 to 752.7) — as much as the whole phase sweep.
    Deglitched, the same six repeats spread 1.1x.
    ⇒ Any single-capture number from this device is suspect. Repeat and take medians.

    Scale-free on purpose: a fixed LSB threshold tuned on 8-bit captures silently
    discarded 100% of a strong 14-bit one earlier in this project."""
    n = len(x) // w
    if n == 0:
        return x
    pp = np.array([np.ptp(x[i * w:(i + 1) * w]) for i in range(n)])
    med = np.median(pp)
    keep = [x[i * w:(i + 1) * w] for i in range(n) if pp[i] <= k * med]
    return np.concatenate(keep) if keep else np.array([])


def load16(path):
    raw = np.frombuffer(open(path, "rb").read(), dtype=np.uint8)
    if len(raw) < 2 or len(raw) % 2 or raw[0::2].max() > 0x3F:
        return None                      # not a 16-bit capture
    return (raw[0::2].astype(np.uint16) << 8 | raw[1::2]).astype(float)[SETTLE:]


def amplitude_at(x, f):
    y = x - x.mean()
    w = np.hanning(len(y))
    return 2 * np.abs(np.sum(y * w * np.exp(-2j * np.pi * f * np.arange(len(y)) / FS))) / w.sum()


def sideband_rms(x, f):
    y = x - x.mean()
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w))
    fr = np.fft.rfftfreq(len(y), 1 / FS)
    m = (fr >= f - 5000) & (fr <= f - 200)
    return 2 * np.sqrt((S[m] ** 2).sum()) / w.sum()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--step", type=int, default=4, help="phase step in ticks (default 4 = 11.25 deg)")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--freq", type=float, default=62500.0)
    ap.add_argument("--keep", help="directory to keep the captures in")
    ap.add_argument("--repeats", type=int, default=5,
                    help="captures per phase; the median is reported (default 5). "
                         "Captures return when the buffer fills (~16ms), NOT after "
                         "--timeout, so repeats are nearly free.")
    ap.add_argument("--yes", action="store_true", help="skip the place/remove prompts")
    a = ap.parse_args()

    phases = list(range(0, 128, a.step))
    out_dir = a.keep or tempfile.mkdtemp(prefix="phasesweep_")
    os.makedirs(out_dir, exist_ok=True)

    # ⭐ ONE cu.py session for the whole sweep. Per-phase invocation costs ~1s of
    # connect/disconnect each, which dwarfs the 16ms capture and makes a 32-point
    # sweep a minute of waiting for nothing.
    cmds = ["hw mode -r"]
    for p in phases:
        cmds.append("lf sniff --timeout %d --bits 16 --phase %d --out %s/p%03d.bin"
                    % (a.timeout, p, out_dir, p))
    def run_pass(tag, label):
        sub_dir = os.path.join(out_dir, tag)
        os.makedirs(sub_dir, exist_ok=True)
        if not a.yes:
            input("  %s, then press RETURN... " % label)
        cs = ["hw mode -r"]
        for rep in range(a.repeats):
            for p in phases:
                cs.append("lf sniff --timeout %d --bits 16 --phase %d --out %s/p%03d_r%d.bin"
                          % (a.timeout, p, sub_dir, p, rep))
        r = subprocess.run([PY, CU] + cs, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit("cu.py failed:\n" + (r.stdout or "") + (r.stderr or ""))
        out = {}
        for p in phases:
            sbs, bins, kept = [], [], []
            for rep in range(a.repeats):
                f = "%s/p%03d_r%d.bin" % (sub_dir, p, rep)
                if not os.path.exists(f):
                    continue
                x = load16(f)
                if x is None:
                    sys.exit("capture at phase %d is not 16-bit — is the firmware current?" % p)
                c = deglitch(x)
                kept.append(len(c) / max(len(x), 1))
                if len(c) < 300:            # too little left to transform meaningfully
                    continue
                sbs.append(sideband_rms(c, a.freq))
                bins.append(amplitude_at(c, a.freq))
            if sbs:
                out[p] = (float(np.median(bins)), float(np.median(sbs)),
                          float(np.median(kept)), len(sbs))
        if not out:
            sys.exit("no usable captures were produced")
        return out

    print(" Sweeping %d phases x %d repeats (step %d ticks = %.1f deg of carrier), twice."
          % (len(phases), a.repeats, a.step, a.step * 360.0 / 128))
    empty = run_pass("empty", "Remove EVERY tag from the antenna")
    withtag = run_pass("tag", "Place the PSK1 RF/32 tag on the LF antenna")

    print("\n%s\n PAIRED SAMPLE-PHASE SWEEP at %.0f Hz\n%s" % ("=" * 74, a.freq, "=" * 74))
    print(" (medians of %d deglitched captures; sideband measure, not the Nyquist bin)"
          % a.repeats)
    print(" %5s %8s %10s %10s %10s %7s  %s"
          % ("ticks", "deg", "empty", "tag", "tag/empty", "kept", "tag/empty"))
    ratios = []
    for p in phases:
        if p not in empty or p not in withtag:
            continue
        e, t = empty[p][1], withtag[p][1]        # [1] = sideband, the robust measure
        r = t / e if e else float("inf")
        ratios.append((p, r, e, t, withtag[p][2]))
    peak = max((r for _, r, _, _, _ in ratios if np.isfinite(r)), default=1.0) or 1.0
    for p, r, e, t, kp in ratios:
        bar = "#" * int(round(40 * min(r, peak) / peak))
        print(" %5d %7.1f° %10.2f %10.2f %9.2fx %6.0f%%  %s"
              % (p, p * 360.0 / 128, e, t, r, kp * 100, bar))

    rr = np.array([r for _, r, _, _, _ in ratios if np.isfinite(r)])
    print("\n%s\n VERDICT\n%s" % ("=" * 74, "=" * 74))
    print(" tag/empty ratio: min %.2fx  max %.2fx  median %.2fx"
          % (rr.min(), rr.max(), np.median(rr)))

    # ⛔ NO THRESHOLDS. An earlier version asked `max ratio > 3?`, got 2.90, and printed
    # "the tag never rises above the empty field at any phase" -- flatly contradicted by
    # its own table, where the tag led the empty field at all 32 phases. Fit the shape
    # instead and report what the fit says.
    #
    # The prediction is one cycle of cos() per CARRIER period, because that is the span
    # the phase knob covers. Fit it to the tag's own contribution (tag minus empty), so
    # a gain change that moves both is not mistaken for signal.
    ph = np.array([p * 360.0 / 128 for p, _, _, _, _ in ratios])
    tag_only = np.array([t - e for _, _, e, t, _ in ratios])
    X = np.c_[np.ones_like(ph), np.cos(np.deg2rad(ph)), np.sin(np.deg2rad(ph))]
    coef, *_ = np.linalg.lstsq(X, tag_only, rcond=None)
    resid = tag_only - X @ coef
    denom = ((tag_only - tag_only.mean()) ** 2).sum()
    r2 = 1 - (resid ** 2).sum() / denom if denom else 0.0
    amp, off = float(np.hypot(coef[1], coef[2])), float(coef[0])
    peak_deg = float(np.rad2deg(np.arctan2(coef[2], coef[1])) % 360)
    tags = np.array([t for _, _, _, t, _ in ratios])
    swing_db = 20 * np.log10(tags.max() / tags.min()) if tags.min() > 0 else float("inf")

    print(" one-cycle sinusoid fit to (tag - empty): R^2 = %.3f, peak at %.0f deg" % (r2, peak_deg))
    print(" offset %.2f, amplitude %.2f  =>  %s" % (off, amp,
          "modulated but never nulled" if amp < off else "reaches zero (true null)"))
    print(" moving the phase is worth %.1f dB" % swing_db)

    if r2 < 0.5:
        print("\n ⛔ No coherent phase dependence. The sampler is not what removes fc/2.")
    elif amp >= off:
        print("\n ⇒ cos(phi) CONFIRMED and it nulls completely. Sample at the peak phase,")
        print("   or oversample so fc/2 is no longer at Nyquist.")
    else:
        print("\n ⇒ PARTLY confirmed: the phase dependence is real and smooth, but SHALLOW.")
        print("   It modulates the subcarrier without ever nulling it, so a fixed unlucky")
        print("   phase is NOT the explanation — it is worth %.1f dB, not the whole loss." % swing_db)
        print("   ⚠ Compare that against the total fc/2 deficit before calling it a fix.")
    print("\n captures in %s%s" % (out_dir, "" if a.keep else " (temporary)"))


if __name__ == "__main__":
    main()
