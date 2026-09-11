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
        cs = ["hw mode -r"] + [
            "lf sniff --timeout %d --bits 16 --phase %d --out %s/p%03d.bin"
            % (a.timeout, p, sub_dir, p) for p in phases]
        r = subprocess.run([PY, CU] + cs, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit("cu.py failed:\n" + (r.stdout or "") + (r.stderr or ""))
        out = {}
        for p in phases:
            f = "%s/p%03d.bin" % (sub_dir, p)
            if not os.path.exists(f):
                continue
            x = load16(f)
            if x is None:
                sys.exit("capture at phase %d is not 16-bit — is the firmware current?" % p)
            out[p] = (amplitude_at(x, a.freq), sideband_rms(x, a.freq), x.std())
        if not out:
            sys.exit("no captures were produced")
        return out

    print(" Sweeping %d phases (step %d ticks = %.1f deg of carrier), twice."
          % (len(phases), a.step, a.step * 360.0 / 128))
    empty = run_pass("empty", "Remove EVERY tag from the antenna")
    withtag = run_pass("tag", "Place the PSK1 RF/32 tag on the LF antenna")

    print("\n%s\n PAIRED SAMPLE-PHASE SWEEP at %.0f Hz\n%s" % ("=" * 74, a.freq, "=" * 74))
    print(" %5s %8s %10s %10s %9s  %s"
          % ("ticks", "deg", "empty", "tag", "tag/empty", "tag/empty"))
    ratios = []
    for p in phases:
        if p not in empty or p not in withtag:
            continue
        e, t = empty[p][0], withtag[p][0]
        r = t / e if e else float("inf")
        ratios.append((p, r, e, t))
    peak = max((r for _, r, _, _ in ratios if np.isfinite(r)), default=1.0) or 1.0
    for p, r, e, t in ratios:
        bar = "#" * int(round(40 * min(r, peak) / peak))
        print(" %5d %7.1f° %10.2f %10.2f %8.2fx  %s" % (p, p * 360.0 / 128, e, t, r, bar))

    rr = np.array([r for _, r, _, _ in ratios if np.isfinite(r)])
    print("\n%s\n VERDICT\n%s" % ("=" * 74, "=" * 74))
    print(" tag/empty ratio: min %.2fx  max %.2fx  median %.2fx"
          % (rr.min(), rr.max(), np.median(rr)))
    best = max(ratios, key=lambda x: x[1] if np.isfinite(x[1]) else -1)
    print(" best phase: %d ticks (%.1f°) at %.2fx" % (best[0], best[0] * 360.0 / 128, best[1]))
    if rr.max() > 3 and rr.max() / max(np.median(rr), 1e-9) > 2:
        print("\n ⇒ The tag IS recoverable at some phases and not others. That is the")
        print("   cos(phi) signature, and it is a firmware fix: sample at the peak phase,")
        print("   or oversample so fc/2 is no longer at Nyquist.")
    elif rr.max() > 3:
        print("\n ⇒ The tag shows at EVERY phase. fc/2 reaches the ADC regardless, so the")
        print("   sampler was not the blocker — re-examine what the 8-bit sweep measured.")
    else:
        print("\n ⛔ The tag never rises above the empty field at any phase. The sampler is")
        print("   NOT what removes fc/2 — it never arrives. Hypothesis falsified; the loss")
        print("   is analog.")
    print("\n captures in %s%s" % (out_dir, "" if a.keep else " (temporary)"))


if __name__ == "__main__":
    main()
