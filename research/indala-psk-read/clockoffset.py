#!/usr/bin/env python3
"""Measure the emulator's frame period against the READER's clock, in ppm.

⭐ WHY. §4 left one thing unexplained: with bursts long enough that a 290 ms capture never
crosses a boundary, the Proxmark still decodes only ~131-197 ms of it (C135). Boundaries are
excluded, so the standing suspect is that our subcarrier free-runs while a T5577 divides the
reader's own carrier — two clocks, slipping.

⇒ That is measurable without any theory. An Indala frame is 64 bits at RF/32 = 2048 carrier
cycles, and the Proxmark samples one per cycle, so a LOCKED source repeats every 2048.000
samples of the Proxmark's clock by construction. Ours will not. The residual is the combined
offset of the two oscillators, in ppm, and it is the number that predicts how long a capture
can run before the phase reference is useless.

    ./clockoffset.py [--n 3] [--period 2048]

⚠ This measures the PAIR, not our crystal. It cannot say which side is off, and it does not
need to: what limits the decode is the difference.
"""
import argparse
import glob
import os
import subprocess
import sys
import numpy as np

PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
TMP = "/tmp/clockoff"


def baseband(x):
    """fc/2 down to DC, low-passed — the same treatment emutest.py's phase check uses."""
    y = np.asarray(x, dtype=float)
    y -= y.mean()
    b = y * ((-1.0) ** np.arange(len(y)))
    S = np.fft.rfft(b)
    S[np.fft.rfftfreq(len(b), 1 / 125000.0) > 12000.0] = 0
    return np.fft.irfft(S, len(b))


def offset_ppm(v):
    """Frequency offset from the SQUARED baseband, not from correlation lags.

    ⛔ THE LAG METHOD FAILED ITS OWN RESIDUAL CHECK and is kept nowhere. Indala is RF/32, so
    autocorrelation has a strong peak every 32 samples; the frame peak at 2048 sits in that
    lattice and a search near k*2048 lands on whichever neighbour wins. It reported +137 to
    +323 ppm with residuals of 27 samples against a 0.3-sample effect — two clusters about
    0.37 samples apart, which is the lattice, not the clock.

    ⭐ What works instead uses the Nyquist degeneracy rather than fighting it. fc/2 IS fs/2,
    so mixing by (-1)^n leaves a REAL baseband b[n] = A.d[n].cos(2.pi.D.n.T + phi), where
    d[n] = +-1 is the data and D is the frequency error. Squaring kills the data — d^2 = 1 —
    and leaves a tone at 2D:

        b^2 = A^2/2 . (1 + cos(4.pi.D.n.T + 2.phi))

    ⇒ Find that tone, halve it, divide by 62500. The data sits at the bit rate, 3.9 kHz,
    three orders of magnitude away from the tens of Hz this is looking for."""
    y = np.asarray(v, dtype=float)
    y -= y.mean()
    b = y * ((-1.0) ** np.arange(len(y)))
    S = np.fft.rfft(b)
    freqs = np.fft.rfftfreq(len(b), 1 / 125000.0)
    S[freqs > 12000.0] = 0
    b = np.fft.irfft(S, len(b))

    sq = b * b
    sq -= sq.mean()
    sq *= np.hanning(len(sq))
    P = np.abs(np.fft.rfft(sq))
    f = np.fft.rfftfreq(len(sq), 1 / 125000.0)
    # ⚠ Search 2..400 Hz. Below 2 Hz is the capture envelope and any burst gap; above 400 Hz
    # (= 3200 ppm) is far outside any crystal pair and would be picking up data structure.
    band = (f >= 2.0) & (f <= 400.0)
    idx = np.flatnonzero(band)
    i = idx[int(np.argmax(P[band]))]
    # parabolic interpolation on the log spectrum, for sub-bin frequency
    if 0 < i < len(P) - 1:
        y0, y1, y2 = np.log(P[i - 1] + 1e-30), np.log(P[i] + 1e-30), np.log(P[i + 1] + 1e-30)
        den = y0 - 2 * y1 + y2
        d = 0.0 if den == 0 else 0.5 * (y0 - y2) / den
    else:
        d = 0.0
    df = f[1] - f[0]
    f_tone = f[i] + d * df
    # how far the peak stands above the rest of the search band -- the confidence
    others = P[band].copy()
    others[int(np.argmax(P[band]))] = 0
    snr = float(P[i] / (np.median(others) + 1e-30))
    return f_tone / 2.0 / 62500.0 * 1e6, f_tone / 2.0, snr, df / 2.0 / 62500.0 * 1e6

def peak_lag(sig, guess, span=24):
    """Best correlation lag near `guess`, to sub-sample precision.

    ⚠ Normalised per lag. An unnormalised correlation drifts with the overlap length and
    biases every peak towards the short-lag end, which would fake a negative offset."""
    n = len(sig)
    best, lags, scores = None, [], []
    for lag in range(guess - span, guess + span + 1):
        if lag <= 0 or lag >= n:
            continue
        a, b = sig[:n - lag], sig[lag:]
        d = np.linalg.norm(a) * np.linalg.norm(b)
        s = float(np.dot(a, b) / d) if d else 0.0
        lags.append(lag)
        scores.append(s)
    if len(scores) < 3:
        return None
    i = int(np.argmax(scores))
    if i == 0 or i == len(scores) - 1:
        return float(lags[i])
    # parabolic interpolation through the peak and its two neighbours
    y0, y1, y2 = scores[i - 1], scores[i], scores[i + 1]
    denom = y0 - 2 * y1 + y2
    delta = 0.0 if denom == 0 else 0.5 * (y0 - y2) / denom
    return float(lags[i]) + delta


def capture(retries=5, min_amp=10.0):
    for _ in range(retries):
        for f in glob.glob(TMP + "*.pm3"):
            os.remove(f)
        subprocess.run([PM3, "-c", f"lf read; data save -f {TMP}"],
                       capture_output=True, text=True, timeout=200)
        found = sorted(glob.glob(TMP + "*.pm3"))
        if not found:
            return None, 0.0
        v = np.array([float(t) for t in open(found[0]).read().split() if t.strip()])
        y = v - v.mean()
        amp = float(np.sqrt(np.mean((y * ((-1.0) ** np.arange(len(y)))) ** 2)))
        if amp >= min_amp:
            return v, amp
    return v, amp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=3)
    ap.add_argument("--period", type=int, default=2048, help="nominal frame, in samples")
    ap.add_argument("--min", type=float, default=10.0)
    a = ap.parse_args()

    print(f"  nominal frame {a.period} samples = {a.period/125:.3f} ms; "
          f"a carrier-locked source sits at exactly that\n")
    for run in range(1, a.n + 1):
        v, amp = capture(min_amp=a.min)
        if v is None:
            print("  ⛔ no trace — is the Proxmark free?")
            return 2
        if amp < a.min:
            print(f"  run {run}: fc/2 {amp:.2f} — empty field, refusing to score")
            continue
        ppm, hz, snr, binppm = offset_ppm(v)
        # ⛔ REFUSE BELOW THE GATE. Validated against synthetic PSK1 at known offsets: the
        # estimator is accurate to ~1 ppm from 50 ppm upwards even at 2x noise, and at a TRUE
        # zero it returns nonsense (894, 979, 3178 ppm) — but with peak/median ~3 against >=10
        # whenever a real tone is present. So the ratio is the confidence, and a reading below
        # it means "no measurable offset", never "a large one".
        if snr < 6.0:
            print(f"  run {run}: fc/2 {amp:5.2f}   ⛔ NO TONE (peak/median {snr:4.1f}) — "
                  f"offset is below what {len(v)/125:.0f} ms can resolve, about {binppm:.0f} ppm")
            continue
        slip_ms = abs(16.0 / ppm) * 1e3 if ppm else float("inf")
        print(f"  run {run}: fc/2 {amp:5.2f}   |{ppm:7.1f}| ppm  "
              f"(tone {hz:6.2f} Hz, peak/median {snr:5.1f}, bin {binppm:.1f} ppm)   "
              f"one subcarrier period of slip after {slip_ms:6.1f} ms")
    print("\n  ⇒ 16 us of accumulated slip is one whole subcarrier cycle at fc/2.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
