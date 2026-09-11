#!/usr/bin/env python3
"""Sweep the SAADC sample phase scoring BIT RECOVERY, not sideband amplitude.

    ./phasebits.py [--step 4] [--repeats 5] [--timeout 300] [--keep DIR]

⭐ THE HYPOTHESIS. The 32-tick "optimum" in this project was found by maximising the fc/2
modulation SKIRT. The skirt is TRANSITION energy and it is polarity-blind — it is measured
at 57.5-62.3kHz precisely to avoid the 62.5kHz bin, because BPSK suppresses its own
carrier and 62.5kHz is exactly Nyquist.

But the credential lives in the polarity, and the polarity lives in the component AT
Nyquist, recovered as 2A*cos(phi). That has a HARD NULL where the skirt has none. The two
have no reason to share an optimum — and if 32 ticks sits at or near the polarity null,
that single fact explains every observation on this bench: full skirt amplitude, frame
structure plainly visible in the envelope, and no recoverable sign.

⛔ WHAT WOULD FALSIFY IT: bit errors flat across all 128 ticks, never dropping below the
empty-field null; or a bit minimum that coincides with the skirt maximum.

⭐ WHY THIS IS WORTH BENCH TIME AT ALL. A synthetic PSK1 frame injected into the REAL
measured noise at HALF the tag's amplitude decodes at 0/31 from a SINGLE capture, while
the real tag at 4x the band SNR gets 7/31. The amplitude is there and the phase is not.
This sweep is the cheapest test of where the phase went.

⚠ READ THE EMPTY COLUMN. Brute-forcing 31 bits over 2048 positions x 64 rotations finds a
low score in pure noise — the empty field lands around 4-5 errors. A tag number only means
something against the null beside it, at the same phase.
"""
import argparse
import os
import subprocess
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mfdemod as M                                        # noqa: E402
from stack import score, sideband, load16, SETTLE          # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
CU = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", "cu.py"))
PY_ = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script", ".venv", "bin", "python"))
LPF = 12000.0


def capture_path(out, state, phase, rep):
    """One helper for writing AND reading. In oversample_test.py two positional %d in the
    same template drifted apart and a verdict was read off nan."""
    return "%s/%s_p%03d_r%d.bin" % (out, state, phase, rep)


def coherence(xs):
    """Median pairwise lag-0 baseband correlation. Stacking at zero offset is only valid
    while the captures stay frame-locked to field-on; if this collapses at some phase the
    stack for that phase is meaningless and its bit score is noise."""
    if len(xs) < 2:
        return float("nan")
    bs = [M.baseband(x) for x in xs]
    rs = [float(np.dot(bs[0], b) / np.sqrt(np.dot(bs[0], bs[0]) * np.dot(b, b)))
          for b in bs[1:]]
    return float(np.median(rs))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--step", type=int, default=4, help="phase step in ticks (4 = 11.25 deg)")
    ap.add_argument("--repeats", type=int, default=5, help="captures per phase, stacked")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--keep", default="caps/phasebits")
    ap.add_argument("--analyse-only", action="store_true",
                    help="re-analyse an existing --keep directory without capturing")
    a = ap.parse_args()
    phases = list(range(0, 128, a.step))
    out = a.keep
    os.makedirs(out, exist_ok=True)

    print(__doc__.split("⭐ THE HYPOTHESIS")[0])
    if not a.analyse_only:
        print(" %d phases x %d repeats x 2 states = %d captures, roughly %.0f min\n"
          % (len(phases), a.repeats, len(phases) * a.repeats * 2,
               len(phases) * a.repeats * 2 * 1.8 / 60))

    def run(state, label):
        input("  %s, then press RETURN... " % label)
        cmds = ["hw mode -r"]
        for p in phases:
            for r in range(a.repeats):
                cmds.append("lf sniff --timeout %d --bits 16 --phase %d --out %s"
                            % (a.timeout, p, capture_path(out, state, p, r)))
        proc = subprocess.run([PY_, CU] + cmds, capture_output=True, text=True)
        if proc.returncode != 0:
            sys.exit("cu.py failed:\n" + (proc.stdout or "") + (proc.stderr or ""))

    if not a.analyse_only:
        run("empty", "Remove EVERY tag from the antenna")
        run("tag", "Place the INDALA26 tag (a0000000e6bd0e92) on the LF antenna")

    def stack_at(state, p):
        xs = []
        for r in range(a.repeats):
            fp = capture_path(out, state, p, r)
            if not os.path.exists(fp):
                sys.exit("⛔ missing capture %s — aborted rather than guessed." % fp)
            x = load16(fp)
            if x is None:
                sys.exit("⛔ %s is not 16-bit — is the firmware current?" % fp)
            xs.append(x)
        n = min(len(x) for x in xs)
        xs = [x[:n] for x in xs]
        return np.array(xs).mean(0), xs

    print("\n" + "=" * 86)
    print(" PHASE vs BIT RECOVERY — does the credential come out anywhere?")
    print(" %d repeats stacked at zero offset per phase, LPF %gHz" % (a.repeats, LPF))
    print("=" * 86)
    print(" %6s %7s | %9s %9s %10s | %9s %10s | %6s" %
          ("ticks", "deg", "tag fc/2", "empty", "tag/empty", "tag data", "empty data", "coh"))
    rows = []
    for p in phases:
        t, txs = stack_at("tag", p)
        e, _ = stack_at("empty", p)
        st, se = sideband(t), sideband(e)
        dt, de = score(t, LPF)[1], score(e, LPF)[1]
        coh = coherence(txs)
        rows.append((p, st, se, st / se if se else float("nan"), dt, de, coh))
        print(" %6d %6.1f° | %9.2f %9.2f %9.2fx | %6d/31 %7d/31 | %6.2f"
              % (p, p * 360.0 / 128, st, se, st / se if se else float("nan"), dt, de, coh))

    print("\n" + "=" * 86)
    print(" VERDICT")
    print("=" * 86)
    ratio = np.array([r[3] for r in rows])
    tbits = np.array([r[4] for r in rows])
    ebits = np.array([r[5] for r in rows])
    ph = np.array([r[0] for r in rows])
    skirt_best = ph[int(np.argmax(ratio))]
    bits_best = ph[int(np.argmin(tbits))]
    print(" skirt maximum      at %3d ticks (%.1f°), tag/empty %.2fx"
          % (skirt_best, skirt_best * 360.0 / 128, ratio.max()))
    print(" bit-error minimum  at %3d ticks (%.1f°), %d/31 credential errors"
          % (bits_best, bits_best * 360.0 / 128, tbits.min()))
    print(" empty-field null across all phases: %d-%d errors, median %.0f"
          % (ebits.min(), ebits.max(), np.median(ebits)))
    # ⛔ No threshold. Report where things sit and how far apart, and let the reader judge.
    print("\n tag range %d-%d errors, null range %d-%d." % (tbits.min(), tbits.max(),
                                                            ebits.min(), ebits.max()))
    if len(rows) < 3:
        print("\n ⚠ %d phase(s) is not a sweep — no comparison of optima is possible."
              % len(rows))
        print("   Run without --step 128 to sweep properly.")
        print("\n captures in %s" % out)
        return
    if bits_best == skirt_best:
        print(" ⇒ The two optima COINCIDE. The hypothesis that the skirt and the polarity")
        print("   peak at different phases is not supported by this sweep.")
    else:
        print(" ⇒ The two optima are %d ticks (%.1f°) apart — the skirt and the polarity do"
              % (abs(int(bits_best) - int(skirt_best)),
                 abs(int(bits_best) - int(skirt_best)) * 360.0 / 128))
        print("   NOT share an optimum, which is what the hypothesis predicts.")
    print("\n ⚠ Neither statement is a decode. Only 0/31 is, and the null beside it at the")
    print("   same phase is the only thing that makes a low count mean anything.")
    if np.nanmin([r[6] for r in rows]) < 0.15:
        print("\n ⚠ Coherence dropped below 0.15 at some phases — the stack there is not")
        print("   valid and its bit score is noise. Check the coh column.")
    print("\n captures in %s" % out)


if __name__ == "__main__":
    main()
