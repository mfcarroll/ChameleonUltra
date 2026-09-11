#!/usr/bin/env python3
"""Measure the Chameleon's LF receive rolloff by sweeping a T5577's PSK carrier.

The T5577's PSK carrier frequency is a field in block 0 (PSKCF: RF/2, RF/4,
RF/8), so one tag can emit a clean continuous subcarrier at 62500, 31250 or
15625 Hz. Capturing each with `lf sniff` measures the receive chain's response
at three points, using the device's own ADC and no instruments.

    ./sweep.py caps/baseline.bin caps/psk_rf8.bin caps/psk_rf4.bin caps/psk_rf2.bin

Why this separates the two competing explanations:

  RF/8 and RF/4 land at 8 and 4 samples per subcarrier cycle, so they are free
  of any sampling artefact and measure the ANALOG FILTER alone. RF/2 lands at
  exactly 2 samples/cycle — Nyquist — where the fixed-phase PPI sampling could
  add a penalty of its own. Fit the filter on the first two, extrapolate to
  62.5kHz, and compare: a shortfall at RF/2 beyond the fitted curve is the
  sampling effect, not the filter.
"""
import sys
import math
import numpy as np

FS = 125000.0
SETTLE, WIN, GLITCH_PTP = 400, 40, 60
SUBCARRIER = {'psk_rf8': 15625.0, 'psk_rf4': 31250.0, 'psk_rf2': 62500.0}
# First-order estimate from schematic netlist values, for comparison only.
MODEL_POLES = [1 / (2 * math.pi * 82 * 33e-9), 1 / (2 * math.pi * 4700 * 1e-9)]


def model_db(f):
    return sum(20 * math.log10(1 / math.sqrt(1 + (f / p) ** 2)) for p in MODEL_POLES)


def load_clean(path):
    x = np.frombuffer(open(path, 'rb').read(), dtype=np.uint8).astype(float)[SETTLE:]
    kept = [x[i * WIN:(i + 1) * WIN] for i in range(len(x) // WIN)
            if np.ptp(x[i * WIN:(i + 1) * WIN]) <= GLITCH_PTP]
    return np.concatenate(kept) if kept else np.array([])


def amplitude_at(x, f_target):
    """Coherent amplitude at f_target, in LSB. Uses a direct DFT bin rather than
    an FFT grid so the exact subcarrier frequency is hit even at Nyquist."""
    y = x - x.mean()
    n = np.arange(len(y))
    w = np.hanning(len(y))
    ref = np.exp(-2j * np.pi * f_target * n / FS)
    return 2 * np.abs(np.sum(y * w * ref)) / w.sum()


def main(paths):
    caps = {p.split('/')[-1].replace('.bin', ''): load_clean(p) for p in paths}
    if 'baseline' not in caps:
        sys.exit("need a baseline.bin (empty field) capture")
    base = caps['baseline']

    print(f"\n{'='*74}")
    print(" T5577 PSK CARRIER SWEEP — Chameleon LF receive response")
    print(f"{'='*74}")
    print(f" {'config':9s}{'subcarrier':>12s}{'smp/cyc':>9s}{'tag':>9s}{'noise':>9s}"
          f"{'SNR':>8s}{'dB rel RF/8':>13s}")

    measured = {}
    for name, f in SUBCARRIER.items():
        if name not in caps:
            continue
        a_tag = amplitude_at(caps[name], f)
        a_base = amplitude_at(base, f)
        measured[name] = (f, a_tag, a_base)
        print(f" {name:9s}{f:11.0f}Hz{FS/f:9.1f}{a_tag:9.2f}{a_base:9.2f}"
              f"{a_tag/a_base:7.2f}x", end="")
        if 'psk_rf8' in measured:
            ref = measured['psk_rf8'][1]
            print(f"{20*math.log10(a_tag/ref):12.1f}", end="")
        print()

    if 'psk_rf8' not in measured:
        return
    ref = measured['psk_rf8'][1]

    print(f"\n{'='*74}")
    print(" FILTER FIT  (RF/8 and RF/4 only — both free of sampling artefacts)")
    print(f"{'='*74}")
    if 'psk_rf4' not in measured:
        print(" need psk_rf4 to fit"); return

    # Single-pole fit through the two clean points, relative to RF/8.
    f8, a8, _ = measured['psk_rf8']
    f4, a4, _ = measured['psk_rf4']
    ratio = a4 / a8
    # solve 1/sqrt(1+(f4/fc)^2) / (1/sqrt(1+(f8/fc)^2)) = ratio  for fc
    r2 = ratio ** 2
    num = f4 ** 2 - r2 * f8 ** 2
    den = r2 - 1
    fc = math.sqrt(num / den) if den != 0 and num / den > 0 else float('inf')
    print(f" measured RF/4 / RF/8 : {ratio:.3f}  ({20*math.log10(ratio):+.1f} dB)")
    print(f" implied 1-pole corner: {fc/1000:.1f} kHz"
          if fc != float('inf') else " implied corner: flat (no rolloff detected)")
    print(f" netlist 2-pole model : {model_db(f4)-model_db(f8):+.1f} dB for the same step")

    if 'psk_rf2' not in measured:
        print("\n need psk_rf2 for the verdict"); return
    f2, a2, _ = measured['psk_rf2']
    pred = a8 * (1 / math.sqrt(1 + (f2 / fc) ** 2)) / (1 / math.sqrt(1 + (f8 / fc) ** 2)) \
        if fc != float('inf') else a8
    print(f"\n{'='*74}")
    print(" VERDICT AT 62.5kHz")
    print(f"{'='*74}")
    print(f" predicted from filter fit : {pred:8.2f} LSB")
    print(f" actually measured         : {a2:8.2f} LSB")
    shortfall = 20 * math.log10(a2 / pred) if pred > 0 and a2 > 0 else float('-inf')
    print(f" shortfall beyond filter   : {shortfall:+8.1f} dB")
    print()
    if a2 / measured['psk_rf2'][2] < 1.5:
        print(" ⇒ fc/2 is NOT VISIBLE above the noise floor.")
        if shortfall < -6:
            print("   The filter fit predicts it should be. Something beyond the filter")
            print("   is removing it — consistent with fixed-phase sampling at Nyquist.")
        else:
            print("   The filter fit already predicts this. Front-end rolloff is")
            print("   sufficient explanation; a hardware change is needed.")
    else:
        print(" ⇒ fc/2 IS VISIBLE. The receive chain delivers the Indala subcarrier;")
        print("   the gap is purely the missing PSK demodulator. Comparator or ADC")
        print("   route will both work.")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
