#!/usr/bin/env python3
"""Analyse raw ChameleonUltra `lf sniff` captures for LF tag modulation.

Answers one question: is a tag load-modulating the 125kHz field, and at what
subcarrier? Built to test whether an Indala (PSK1, RF/32, fc/2 subcarrier) tag
is visible in the Chameleon's raw ADC stream.

    ./analyse.py caps/baseline.bin caps/indala.bin caps/control.bin

Expects a capture named 'baseline' (empty field) to use as the reference.
"""
import sys
import numpy as np

FS = 125000.0      # SAADC samples once per carrier cycle: 125kHz, 8us/sample
SETTLE = 400       # drop antenna startup ringing (firmware skips 2ms itself)
WIN = 40           # glitch-screening window
GLITCH_PTP = 60    # windows swinging more than this are USB-transfer overruns

# Subcarriers of interest, as divisors of the 125kHz carrier.
BANDS = [
    (1500, 2500, 'fc/64  ~1953Hz  EM410x bit rate'),
    (3500, 4400, 'fc/32  ~3906Hz  INDALA bit rate'),
    (11500, 13500, 'fc/10  =12500Hz HID Prox FSK lo'),
    (14500, 16500, 'fc/8   =15625Hz HID Prox FSK hi'),
]


def load(path):
    return np.frombuffer(open(path, 'rb').read(), dtype=np.uint8).astype(float)[SETTLE:]


def deglitch(x):
    """Drop windows containing USB buffer-overrun discontinuities.

    These appear in every capture including the empty-field baseline, so they
    are an artefact of the transfer, not tag signal. See firmware
    lf_reader_generic.c: 'buffer full - oldest samples dropped'.
    """
    kept = [x[i * WIN:(i + 1) * WIN] for i in range(len(x) // WIN)
            if np.ptp(x[i * WIN:(i + 1) * WIN]) <= GLITCH_PTP]
    return (np.concatenate(kept) if kept else np.array([])), len(kept), len(x) // WIN


def spectrum(y):
    y = y - y.mean()
    S = np.abs(np.fft.rfft(y * np.hanning(len(y))))
    S[0] = 0
    return np.fft.rfftfreq(len(y), 1 / FS), S


def band_frac(f, S, lo, hi):
    """Fraction of total spectral energy falling in [lo, hi]."""
    return S[(f >= lo) & (f <= hi)].sum() / S.sum()


def nyquist_frac(S):
    """Fraction of energy in the final bin = exactly fc/2 = 62500Hz.

    Read directly rather than by frequency range: rfftfreq lands the last bin
    a hair either side of 62500.0 in floating point, and a range test can miss
    it entirely. This single bin is where an Indala subcarrier would appear.
    """
    return S[-1] / S.sum()


def psk1_envelope(x):
    """Coherent PSK1 detector for an fc/2 subcarrier.

    Sampling at fc puts the fc/2 subcarrier at exactly Nyquist, so multiplying
    by (-1)^n mixes it down to DC. Low-pass over half an RF/32 bit (16 samples);
    a real Indala frame yields a bipolar envelope flipping sign at bit edges.
    """
    y = x - x.mean()
    z = y * ((-1.0) ** np.arange(len(y)))
    return np.convolve(z, np.ones(16) / 16, mode='same')


def noise_reference(x):
    """Same pipeline mixed to a frequency carrying no protocol, for SNR."""
    y = x - x.mean()
    z = y * np.cos(2 * np.pi * 0.4 * np.arange(len(y)))
    return np.convolve(z, np.ones(16) / 16, mode='same')


def main(paths):
    caps = {}
    print("\n" + "=" * 72)
    print(" PER-CAPTURE SUMMARY")
    print("=" * 72)
    for p in paths:
        name = p.split('/')[-1].replace('.bin', '')
        raw = load(p)
        clean, kept, total = deglitch(raw)
        f, S = spectrum(clean)
        caps[name] = dict(raw=raw, clean=clean, f=f, S=S)
        print(f"\n {name}")
        print(f"   deglitched     : kept {kept}/{total} windows "
              f"({100*kept/total:.0f}% clean), {len(clean)} samples")
        print(f"   residual std   : {clean.std():.2f}   "
              f"(empty field ~8.5; a modulating tag roughly doubles this)")
        for lo, hi, label in BANDS:
            print(f"   {label:32s} {100*band_frac(f, S, lo, hi):5.1f}% of energy")
        print(f"   {'fc/2   =62500Hz INDALA subcarrier':32s} "
              f"{100*nyquist_frac(S):5.2f}% of energy   "
              f"[Nyquist bin, magnitude {S[-1]:.0f}]")

    if 'baseline' not in caps:
        return
    others = [n for n in caps if n != 'baseline']

    print("\n" + "=" * 72)
    print(" BAND ENERGY RATIO vs EMPTY-FIELD BASELINE   (>1 = tag adds energy)")
    print("=" * 72)
    b = caps['baseline']
    print(f" {'band':34s}" + "".join(f"{n:>12s}" for n in others))
    for lo, hi, label in BANDS:
        ref = band_frac(b['f'], b['S'], lo, hi)
        row = f" {label:34s}"
        for n in others:
            c = caps[n]
            row += f"{band_frac(c['f'], c['S'], lo, hi)/ref:11.2f}x"
        print(row)
    ref = nyquist_frac(b['S'])
    row = f" {'fc/2   =62500Hz INDALA subcarrier':34s}"
    for n in others:
        row += f"{nyquist_frac(caps[n]['S'])/ref:11.2f}x"
    print(row)

    print("\n" + "=" * 72)
    print(" COHERENT PSK1 DETECTOR  (fc/2 mixed to DC)")
    print("=" * 72)
    print(f" {'capture':12s}{'|envelope|':>12s}{'SNR':>10s}")
    for n in ['baseline'] + others:
        x = caps[n]['raw']
        env = np.abs(psk1_envelope(x)).mean()
        nf = np.abs(noise_reference(x)).mean()
        print(f" {n:12s}{env:12.3f}{env/nf:9.2f}x")
    print("\n An Indala frame would stand well clear of baseline here.")

    if 'indala' in caps:
        print("\n" + "=" * 72)
        print(" INDALA FRAME SEARCH  (autocorrelation of the PSK envelope)")
        print("=" * 72)
        e = np.abs(psk1_envelope(caps['indala']['raw']))
        e = e - e.mean()
        e /= (np.std(e) or 1)
        ac = np.correlate(e, e, 'full')[len(e) - 1:]
        ac /= ac[0]
        for lag, note in [(32, 'one RF/32 bit'), (64, ''), (512, ''),
                          (2048, '64-bit Indala frame'), (2048 * 2, 'frame x2')]:
            if lag < len(ac):
                print(f"   lag {lag:5d} ({lag*8/1000:6.2f} ms) : {ac[lag]:+.3f}   {note}")
        print("\n A repeating frame would show a clear positive peak at lag 2048.")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
