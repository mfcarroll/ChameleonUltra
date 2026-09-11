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


WIDTH = {}          # path -> 8 or 16, for the mixing guard


def load(path):
    """Proxmark .pm3 = one signed integer per line. Chameleon .bin = either the 8-bit
    format or, from `lf sniff --bits 16`, two bytes per sample big-endian.

    ⭐ The two are told apart by content, not by name: a 14-bit conversion can never put
    more than 0x3F in its high byte, so any even-offset byte above that proves the file
    is 8-bit. This is the same invariant the firmware guarantees by masking to 0x3FFF."""
    if path.endswith('.pm3'):
        return np.array([float(l) for l in open(path) if l.strip()])
    raw = np.frombuffer(open(path, 'rb').read(), dtype=np.uint8)
    if len(raw) % 2 == 0 and len(raw) > 1 and raw[0::2].max() <= 0x3F:
        WIDTH[path] = 16
        x = (raw[0::2].astype(np.uint16) << 8 | raw[1::2]).astype(float)
        return x[SETTLE:]                       # SETTLE is in SAMPLES, not bytes
    WIDTH[path] = 8
    return raw.astype(float)[SETTLE:]


def sideband_rms(x, f):
    """RMS in the lower modulation skirt of a PSK1 subcarrier at f. THIS is the primary
    estimator, for two reasons the exact-bin figure gets wrong:

    ⛔ BPSK SUPPRESSES ITS OWN CARRIER. With balanced data almost nothing sits AT f --
    what is there is residual imbalance, so a bin measurement at f compares two
    instruments on the weakest part of the signal and the ratio is noise.

    ⛔ AND AT f = fc/2 THE BIN IS DEGENERATE. 62500 Hz sampled at 125000 is exactly
    Nyquist: the recovered amplitude is 2*A*|cos(phi)| for a fixed sampling phase phi,
    i.e. anywhere from 0 to twice the truth. Verified in test: an injected cosine reads
    2x, the same signal in quadrature reads ~0.

    The skirt below f is off Nyquist and carries the actual modulation energy, so it is
    both phase-safe and representative. The upper skirt aliases back onto the lower one
    here, so one band catches both."""
    y = x - x.mean()
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w))
    freqs = np.fft.rfftfreq(len(y), 1 / FS)
    m = (freqs >= f - 5000) & (freqs <= f - 200)
    return 2 * np.sqrt((S[m] ** 2).sum()) / w.sum()


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
    seen = {}                       # (instrument, subcarrier) -> path, for the duplicate guard
    for p in paths:
        if 'baseline' in p.split('/')[-1].lower():
            base = load(p)
            continue
        hz = classify(p)
        if hz is None:
            print(" ⚠ ignored (no PSKCF config in the name): %s" % p.split('/')[-1])
            continue
        inst = 'pm3' if p.endswith('.pm3') else 'cham'
        target = pm3 if inst == 'pm3' else cham
        # ⛔ A GLOB OVER campaigns/*/raw/*.bin MATCHES EVERY CAMPAIGN. Two runs of the
        # same sweep then collide on the same subcarrier key and the later silently
        # wins -- which happened, mixing an 8-bit run with a 14-bit one and producing
        # a plausible-looking table from two different instruments-worth of data.
        if (inst, hz) in seen:
            sys.exit("⛔ two %s captures both claim %s:\n  %s\n  %s\n"
                     "Point at ONE campaign, not a glob across all of them."
                     % (inst, LABEL[hz], seen[(inst, hz)], p))
        seen[(inst, hz)] = p
        target[hz] = load(p)

    widths = {WIDTH[p] for p in WIDTH if not p.endswith('.pm3')}
    if len(widths) > 1:
        sys.exit("⛔ mixed 8-bit and 16-bit Chameleon captures in one run. They are not\n"
                 "   comparable: the 8-bit path discards 5 bits. Use one campaign's\n"
                 "   captures and a baseline of the SAME width.")
    bits = widths.pop() if widths else 8
    if base is not None and WIDTH.get([p for p in WIDTH if 'baseline' in p][0]) != bits:
        sys.exit("⛔ baseline is %d-bit but the captures are %d-bit. Take a matching one."
                 % (WIDTH[[p for p in WIDTH if 'baseline' in p][0]], bits))
    print("\n Sample width: %d-bit" % bits)

    order = [15625.0, 31250.0, 62500.0]

    print("\n" + "=" * 74)
    print(" T5577 PSK CARRIER SWEEP")
    print("=" * 74)
    print(f" {'PSKCF':6s}{'subcarrier':>11s}{'smp/cyc':>8s}"
          f"{'CHAM band':>11s}{'PM3 band':>10s}{'CHAM bin':>10s}{'PM3 bin':>9s}")
    for hz in order:
        if hz not in cham and hz not in pm3:
            continue
        cb_ = sideband_rms(cham[hz], hz) if hz in cham else float('nan')
        pb_ = sideband_rms(pm3[hz], hz) if hz in pm3 else float('nan')
        c = amplitude_at(cham[hz], hz) if hz in cham else float('nan')
        p = amplitude_at(pm3[hz], hz) if hz in pm3 else float('nan')
        deg = " <- degenerate" if hz == 62500.0 else ""
        print(f" {LABEL[hz]:6s}{hz:10.0f}Hz{FS/hz:8.1f}{cb_:11.3f}{pb_:10.2f}"
              f"{c:10.3f}{p:9.2f}{deg}")
    print("\n band = PSK modulation skirt (primary). bin = exact subcarrier: BPSK suppresses")
    print(" its own carrier there, and at fc/2 the bin is at Nyquist and reads 0-2x the truth.")

    if not (len(cham) >= 2 and len(pm3) >= 2):
        print("\n Need both instruments at >=2 subcarriers for the verdict.")
        print(" Pass the campaign's pm3_signal/*.pm3 alongside raw/*.bin.")
        return

    print("\n" + "=" * 74)
    print(" ROLLOFF FROM RF/8 — each instrument against itself")
    print("=" * 74)
    c0, p0 = sideband_rms(cham[15625.0], 15625.0), sideband_rms(pm3[15625.0], 15625.0)
    excess = {}
    print(f" {'PSKCF':6s}{'Proxmark':>12s}{'Chameleon':>12s}{'excess loss':>14s}")
    for hz in order[1:]:
        if hz not in cham or hz not in pm3:
            continue
        dp = 20 * np.log10(sideband_rms(pm3[hz], hz) / p0)
        dc = 20 * np.log10(sideband_rms(cham[hz], hz) / c0)
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
    # ⭐ THE SHAPE IS THE EVIDENCE, NOT THE DEPTH. RF/4 sits at 4 samples/cycle where
    # sampling is safe, RF/2 at exactly 2 where a fixed-phase sampler can null a tone
    # completely. So a chain that tracks the reference at RF/4 and then collapses at
    # RF/2 is pointing at the SAMPLER; one that degrades progressively across both is
    # pointing at the analog front end. Earlier versions of this script asserted the
    # latter unconditionally, which inverted the reading once the 14-bit data arrived.
    if ex4 is not None and ex4 > -3 and ex2 < -20:
        print(f" ⇒ The chain TRACKS the Proxmark at RF/4 ({ex4:+.1f} dB) and then collapses")
        print(f"   at fc/2 ({ex2:+.1f} dB). That shape indicts the SAMPLER, not the front end.")
        print("   A filter flat at 31 kHz cannot lose 34 dB by 62.5 kHz — that is >6 poles,")
        print("   and this chain has a diode detector and two RC stages. Sampling at exactly")
        print("   2 samples/cycle at a FIXED phase can null a tone to any depth: the")
        print("   recovered amplitude is proportional to cos(phi), and phi is constant")
        print("   because the SAADC is PPI-triggered from the same PWM that makes the field.")
        print("\n   ⇒ TESTABLE, AND IN FIRMWARE: sweep the sample phase, or sample at")
        print("     200-250 kHz so fc/2 is no longer at Nyquist. If fc/2 reappears, this")
        print("     is settled and the decoder port is worth doing.")
    elif ex2 > -10:
        print(" ⇒ The Chameleon tracks the Proxmark at fc/2. The receive chain is NOT the")
        print("   blocker; the gap is the missing PSK demodulator, and a firmware route works.")
    else:
        print(f" ⇒ The Chameleon loses {abs(ex2):.1f} dB at fc/2 that the Proxmark does not,")
        print("   and that loss is analog — it happens before digitisation.")
        if ex4 is not None:
            print(f"   RF/4 already shows {abs(ex4):.1f} dB at 4 samples/cycle, so it is a genuine")
            print("   front-end rolloff and not a sampling artefact.")
        print(f"\n   ⚠ But weigh it against the 8-bit truncation below: >>5 discards ~30 dB of")
        print(f"      dynamic range, which is MORE than this {abs(ex2):.1f} dB. Resolution is the")
        print("      larger handicap, and unlike the front end it is a firmware fix.")
    if bits == 8:
        print("\n ⚠ These are 8-BIT numbers. `lf sniff` right-shifts the 14-bit conversion by")
        print("   5 (lf_reader_generic.c:59); decoder.feed() does not. Re-measure with")
        print("   --bits 16 before concluding anything about demodulability.")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
