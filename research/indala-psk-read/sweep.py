#!/usr/bin/env python3
"""Measure the Chameleon's LF receive rolloff by sweeping a T5577's PSK carrier,
referenced against the Proxmark3 reading the same tag.

The T5577's PSK carrier frequency is a field in block 0 (PSKCF), so one tag emits a
clean continuous subcarrier at 62500, 31250 or 15625 Hz against a fixed 125 kHz
carrier. Capture each on both instruments and the difference in rolloff is the
Chameleon's receive chain, with the tag's own behaviour divided out.

    ./sweep.py caps/baseline.bin <campaign>/raw/*.bin [<campaign>/pm3_signal/*.pm3]

⚠ EVERY CHAMELEON NUMBER HERE IS 8-BIT. `lf sniff` right-shifts the 14-bit SAADC
conversion by 5 (lf_reader_generic.c:59) before it leaves the device. The protocol
decoders do NOT -- lf_hidprox_data.c feeds decoder.feed() the raw value. So a
sub-LSB reading here is up to 32x larger on the path a real demodulator would use.
"""
import sys
import glob
import numpy as np

FS = 125000.0
SETTLE = 400
# config token -> subcarrier Hz. Check the CF cells BEFORE bare PSK1: "PSK1" is a
# substring of "PSK1-CF4", so the naive order labels everything 62500 Hz.
BY_CONFIG = [('PSK1-CF8', 15625.0), ('PSK1-CF4', 31250.0), ('PSK1', 62500.0)]
BY_NAME = {'psk_rf8': 15625.0, 'psk_rf4': 31250.0, 'psk_rf2': 62500.0}
LABEL = {15625.0: 'RF/8', 31250.0: 'RF/4', 62500.0: 'RF/2'}


def classify(path):
    stem = path.split('/')[-1].rsplit('.', 1)[0]
    if 'baseline' in stem.lower():
        return None
    if stem in BY_NAME:
        return BY_NAME[stem]
    for token, hz in BY_CONFIG:
        if ('_%s_' % token) in stem or stem.startswith(token + '_'):
            return hz
    return None


def load(path):
    """Chameleon .bin = raw uint8; Proxmark .pm3 = one signed integer per line."""
    if path.endswith('.pm3'):
        return np.array([float(l) for l in open(path) if l.strip()])
    return np.frombuffer(open(path, 'rb').read(), dtype=np.uint8).astype(float)[SETTLE:]


def amplitude_at(x, f):
    """Coherent amplitude at exactly f, via a direct DFT bin rather than an FFT grid --
    so the subcarrier is hit exactly even at Nyquist. Narrowband, so sparse USB-overrun
    bursts contribute almost nothing and no glitch screen is needed (one was tried and
    discarded real signal; see the note in the git history)."""
    y = x - x.mean()
    w = np.hanning(len(y))
    ref = np.exp(-2j * np.pi * f * np.arange(len(y)) / FS)
    return 2 * np.abs(np.sum(y * w * ref)) / w.sum()


def main(paths):
    base = None
    cham, pm3 = {}, {}
    for p in paths:
        if 'baseline' in p.split('/')[-1].lower():
            base = load(p)
            continue
        hz = classify(p)
        if hz is None:
            print(" ⚠ ignored (no PSKCF config in the name): %s" % p.split('/')[-1])
            continue
        (pm3 if p.endswith('.pm3') else cham)[hz] = load(p)

    order = [15625.0, 31250.0, 62500.0]

    print("\n" + "=" * 74)
    print(" T5577 PSK CARRIER SWEEP")
    print("=" * 74)
    print(f" {'PSKCF':6s}{'subcarrier':>12s}{'smp/cyc':>9s}{'Chameleon':>11s}{'noise':>8s}"
          f"{'SNR':>9s}{'Proxmark':>11s}")
    for hz in order:
        if hz not in cham and hz not in pm3:
            continue
        c = amplitude_at(cham[hz], hz) if hz in cham else float('nan')
        b = amplitude_at(base, hz) if base is not None else float('nan')
        p = amplitude_at(pm3[hz], hz) if hz in pm3 else float('nan')
        print(f" {LABEL[hz]:6s}{hz:11.0f}Hz{FS/hz:9.1f}{c:11.3f}{b:8.3f}{c/b:8.2f}x{p:11.2f}")

    if not (len(cham) >= 2 and len(pm3) >= 2):
        print("\n Need both instruments at >=2 subcarriers for the verdict.")
        print(" Pass the campaign's pm3_signal/*.pm3 alongside raw/*.bin.")
        return

    print("\n" + "=" * 74)
    print(" ROLLOFF FROM RF/8 — each instrument against itself")
    print("=" * 74)
    c0, p0 = amplitude_at(cham[15625.0], 15625.0), amplitude_at(pm3[15625.0], 15625.0)
    excess = {}
    print(f" {'PSKCF':6s}{'Proxmark':>12s}{'Chameleon':>12s}{'excess loss':>14s}")
    for hz in order[1:]:
        if hz not in cham or hz not in pm3:
            continue
        dp = 20 * np.log10(amplitude_at(pm3[hz], hz) / p0)
        dc = 20 * np.log10(amplitude_at(cham[hz], hz) / c0)
        excess[hz] = dc - dp
        print(f" {LABEL[hz]:6s}{dp:+11.1f}dB{dc:+11.1f}dB{dc - dp:+13.1f}dB")
    print("\n The Proxmark column is the TAG's own behaviour, common to both instruments.")
    print(" The excess is what the Chameleon's receive chain removes on top of it.")

    print("\n" + "=" * 74)
    print(" VERDICT")
    print("=" * 74)
    ex2, ex4 = excess.get(62500.0), excess.get(31250.0)
    if ex2 is None:
        return
    if ex2 > -6:
        print(" ⇒ The Chameleon tracks the Proxmark at fc/2. The receive chain is NOT the")
        print("   blocker; the gap is the missing PSK demodulator, and a firmware route works.")
    else:
        print(f" ⇒ The Chameleon loses {abs(ex2):.0f} dB at fc/2 that the Proxmark does not.")
        print("   The tag emits it; this receive chain attenuates it. That loss happens")
        print("   BEFORE digitisation, so no sample-rate, sample-phase or comparator change")
        print("   can recover it.")
        if ex4 is not None and ex4 < -6:
            print(f"\n   ⭐ And it is NOT a sampling artefact: RF/4 sits at 4 samples/cycle with no")
            print(f"      Nyquist problem and already shows {abs(ex4):.0f} dB of excess loss. The")
            print("      attenuation is present well below Nyquist.")
    print("\n ⚠ These are 8-BIT numbers. `lf sniff` right-shifts the 14-bit conversion by 5")
    print("   (lf_reader_generic.c:59); decoder.feed() does not. Before concluding the signal")
    print("   is too small to demodulate, re-measure through a full-resolution path.")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
