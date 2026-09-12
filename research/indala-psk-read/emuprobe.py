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
EXPECT_BITS = 0
EXPECT_HEX = ""


def lowpass_baseband(y):
    """Mix fc/2 down to DC and keep the data. Same chain as the decoder."""
    b = y * ((-1.0) ** np.arange(len(y)))
    S = np.fft.rfft(b)
    S[np.fft.rfftfreq(len(b), 1 / FS) > 12000.0] = 0
    return np.fft.irfft(S, len(b))


def longest_bit_run(hexframe):
    """Longest run of identical bits in a frame — in PSK1 that IS the constant-phase run."""
    bits = bin(int(hexframe, 16))[2:].zfill(len(hexframe) * 4)
    best = run = 1
    for i in range(1, len(bits)):
        run = run + 1 if bits[i] == bits[i - 1] else 1
        best = max(best, run)
    return best


def load16(path):
    """A Chameleon `lf sniff --bits 16` capture, or a Proxmark `data save` trace.

    ⭐ BOTH SAMPLE AT ~125 kHz, ONE SAMPLE PER CARRIER CYCLE, CARRIER-LOCKED, which is the
    only property the analysis below depends on — so a Proxmark trace can be read here and
    serves as an INDEPENDENT instrument. Every measurement in L71/L72 came from a Chameleon
    reading a Chameleon: same clock architecture, same assumptions. An unexplained ~640 Hz
    beat deserves a second opinion from different silicon.

    ⚠ WHAT DOES NOT TRANSFER IS SCALE. Proxmark LF traces are 8-bit and the Chameleon's are
    14-bit, so the absolute fs/2 skirt figures quoted here (real tag ~181000, empty ~4300)
    are meaningless for a .pm3 file. The measures that DO transfer are the ones that matter:
    the constant-phase run and the frame autocorrelation are both scale-free.

    ⚠ Confirm the Proxmark is actually sampling at 125 kHz before trusting a trace:
        lf config          # expect divisor 95, i.e. a 125 kHz carrier
    """
    raw = open(path, "rb").read()
    # A .pm3 trace is text: one decimal sample per line.
    head = raw[:64]
    if all(c in b"0123456789+-.\r\n \t" for c in head):
        vals = [float(t) for t in raw.split() if t.strip()]
        if len(vals) < 64:
            raise SystemExit(f"⛔ {path}: looks like text but holds {len(vals)} samples")
        return np.array(vals, dtype=float)
    r = np.frombuffer(raw, dtype=np.uint8)
    if len(r) < 4 or len(r) % 2 or r[0::2].max() > 0x3F:
        raise SystemExit(f"⛔ {path}: not a 16-bit capture — use `lf sniff --bits 16`, "
                         f"or pass a Proxmark `data save` text trace")
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
    is_text = all(c in b"0123456789+-.\r\n \t" for c in open(path, "rb").read(64))
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
          f"(14-bit Chameleon captures: real tag ~181000, empty ~4300;\n                                     meaningless for an 8-bit Proxmark trace)")

    print("  raw peaks 500 Hz - 20 kHz :", "  ".join(
        f"{q:.0f}Hz({a:.2f})" for q, a in peaks(S, f, 500, 20000)))
    Sb = np.abs(np.fft.rfft(mixed * w))
    print("  after mixing, 300 Hz - 20k:", "  ".join(
        f"{q:.0f}Hz({a:.2f})" for q, a in peaks(Sb, f, 300, 20000)))
    print("  ⚠ RF/32 bit rate is 3906 Hz. An 8x-slow clock would put it at 488 Hz and the")
    print("    subcarrier at 7.8 kHz — but 7.8 kHz is ALSO a harmonic of a healthy 3906 Hz,")
    print("    so read the two together, never 7.8 kHz alone.")

    # ⭐ THE MEASURE THAT ACTUALLY CRACKED IT. A PSK1 frame's phase IS its bits, so a run of
    # N identical bits must appear as N*32 samples of constant phase. That test needs no
    # frequency resolution at the Nyquist edge — which is exactly where the obvious
    # measurement fails (an EMPTY field "peaks" 793 Hz off fc/2, proving the estimator
    # rather than any signal). Measured: real tag 912 samples, emulator 102, empty 34.
    b = lowpass_baseband(y)
    sgn = np.sign(b)
    sgn[sgn == 0] = 1
    ch = np.flatnonzero(np.diff(sgn)) + 1
    runs = np.diff(np.concatenate([[0], ch, [len(sgn)]]))
    print(f"  longest constant-phase run  {runs.max():6.0f} samples = "
          f"{runs.max() / 32:.1f} bits at RF/32")
    if EXPECT_BITS:
        need = EXPECT_BITS * 32
        verdict = "✓" if runs.max() > need * 0.7 else "⛔ PHASE STRUCTURE IS NOT THERE"
        print(f"    frame {EXPECT_HEX} needs a {EXPECT_BITS}-bit run = {need} samples   {verdict}")

    # A looping 64-bit frame at RF/32 repeats every 2048 samples.
    yy = b - b.mean()
    ac = np.correlate(yy, yy, "full")[len(yy) - 1:]
    ac = ac / ac[0]
    lag = 1500 + int(np.argmax(ac[1500:2600]))
    print(f"  frame autocorrelation       lag {lag} r={ac[lag]:+.2f}   (true frame = 2048)")

    # ⚠ The C harness reads only the binary 16-bit format. Printing "decoder: -" for a text
    # trace would look like a failed decode rather than a file it cannot open — say so
    # instead, because a spurious negative is worse here than no line at all.
    cdemod = os.path.join(HERE, "ctest", "cdemod")
    if is_text:
        print("  decoder: skipped — cdemod reads the binary 16-bit format, not a text trace")
    elif os.path.exists(cdemod):
        out = subprocess.run([cdemod, path], capture_output=True, text=True).stdout.strip()
        print(f"  decoder: {out.split('samples')[-1].strip() if 'samples' in out else out}")


def main():
    global EXPECT_BITS, EXPECT_HEX
    args = sys.argv[1:]
    if "--frame" in args:
        i = args.index("--frame")
        EXPECT_HEX = args[i + 1].lower()
        EXPECT_BITS = longest_bit_run(EXPECT_HEX)
        del args[i:i + 2]
    if not args:
        raise SystemExit(__doc__)
    if EXPECT_HEX:
        print(f"  expecting frame {EXPECT_HEX}: longest identical-bit run is "
              f"{EXPECT_BITS} bits = {EXPECT_BITS * 32} samples of constant phase")
    for p in args:
        report(p)


if __name__ == "__main__":
    main()
