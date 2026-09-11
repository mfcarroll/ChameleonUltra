#!/usr/bin/env python3
"""What is an emulating tag ACTUALLY transmitting? Report the spectrum, not a verdict.

    ./emuprobe.py capture.bin [more.bin ...]

⭐ WHY THIS EXISTS. "The Proxmark reads nothing" is the same useless binary metric that F05
and M17 were written about: it cannot separate "transmitting nothing" from "transmitting the
wrong thing" from "transmitting the right thing too quietly".

⛔ AND THE OBVIOUS VERSION OF THIS TOOL WAS WRONG. The first draft looked for a ~62.5 kHz
peak and declared the carrier correct or 8x slow. It reported "matches no expected value" on
a capture from a real Indala tag that decodes 5/5, for two reasons worth keeping written
down:

  1. fc/2 IS fs/2. The subcarrier sits at exactly Nyquist for the 125 kHz sampler, so it
     never appears as a peak — it appears as an alternating sign, i.e. energy in the LAST
     FFT bin, and its sidebands fold down to baseband. There is no 62.5 kHz hump to find.
  2. 7812 Hz — the "8x slow" signature — measures 0.53 of peak in a KNOWN-GOOD capture,
     because it is a harmonic of the 3906 Hz bit rate. Testing for it alone fires on
     healthy signals.

⇒ This prints measurements and leaves the reading to a human. The one line that is a real
verdict is the decoder's, because that one cannot be fooled by a harmonic.

Capture with the OTHER Chameleon in reader mode — it generates the field the emulating tag
modulates:

    lf sniff --timeout 500 --bits 16 --out emu.bin
"""
import os
import subprocess
import sys
import numpy as np

FS = 125000.0
HERE = os.path.dirname(os.path.abspath(__file__))


def load16(path):
    r = np.frombuffer(open(path, "rb").read(), dtype=np.uint8)
    if len(r) < 4 or len(r) % 2 or r[0::2].max() > 0x3F:
        raise SystemExit(f"⛔ {path}: not a 16-bit capture — use `lf sniff --bits 16`")
    return ((r[0::2].astype(np.uint16) << 8) | r[1::2]).astype(float)


def band(S, f, lo, hi):
    m = (f > lo) & (f < hi)
    return float(np.sqrt((S[m] ** 2).sum())) if m.any() else 0.0


def peaks(S, f, lo, hi, n=4, sep=800):
    m = (f > lo) & (f < hi)
    fs_, Ss = f[m], S[m]
    out = []
    for i in np.argsort(Ss)[::-1]:
        if any(abs(fs_[i] - q) < sep for q, _ in out):
            continue
        out.append((float(fs_[i]), float(Ss[i] / Ss.max())))
        if len(out) >= n:
            break
    return out


def report(path):
    x = load16(path)
    y = x - x.mean()
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w))
    f = np.fft.rfftfreq(len(y), 1 / FS)

    print(f"\n=== {os.path.basename(path)}")
    print(f"  {len(x)} samples, {len(x) / FS * 1000:.1f} ms   mean {x.mean():.0f}   "
          f"peak-to-peak {x.max() - x.min():.0f}")

    # ⭐ THE TWO NUMBERS THAT ACTUALLY DISCRIMINATE, calibrated on committed captures:
    #
    #                       fs/2 skirt    3906 Hz peak     decoder
    #     real Indala tag       181117    0.97 of peak     decodes
    #     empty field             4347    absent           nothing
    #
    # The skirt is 41x apart between them, which makes it a presence test. 3906 Hz is the
    # RF/32 bit rate and confirms the TIMING is right, which the skirt alone cannot.
    #
    # ⚠ Do NOT try to measure the subcarrier as the DC of the (-1)^n-mixed signal. It reads
    # ~1 for a real tag and ~1 for an empty field, because the subcarrier is PHASE-MODULATED
    # — the exact-fs/2 component averages to nothing over a frame. The energy is in the
    # sidebands, which is what the band measure below catches and that one does not.
    mixed = y * ((-1.0) ** np.arange(len(y)))
    print(f"  fs/2 skirt 60-65kHz        {band(S, f, 60000, 65000):8.0f}   "
          f"(real tag ~181000, empty ~4300)")

    print("  raw peaks 500 Hz - 20 kHz :", "  ".join(
        f"{q:.0f}Hz({a:.2f})" for q, a in peaks(S, f, 500, 20000)))
    Sb = np.abs(np.fft.rfft(mixed * w))
    print("  after mixing, 300 Hz - 20k:", "  ".join(
        f"{q:.0f}Hz({a:.2f})" for q, a in peaks(Sb, f, 300, 20000)))
    print("  ⚠ RF/32 bit rate is 3906 Hz. An 8x-slow clock would put it at 488 Hz and the")
    print("    subcarrier at 7.8 kHz — but 7.8 kHz is ALSO a harmonic of a healthy 3906 Hz,")
    print("    so read the two together, never 7.8 kHz alone.")

    cdemod = os.path.join(HERE, "ctest", "cdemod")
    if os.path.exists(cdemod):
        out = subprocess.run([cdemod, path], capture_output=True, text=True).stdout.strip()
        print(f"  decoder: {out.split('samples')[-1].strip() if 'samples' in out else out}")


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for p in sys.argv[1:]:
        report(p)


if __name__ == "__main__":
    main()
