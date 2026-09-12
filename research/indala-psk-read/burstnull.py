import sys, numpy as np
sys.path.insert(0, "/Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read")
from clockoffset import offset_ppm

rng = np.random.default_rng(11)
N, T = 36295, 1 / 125000.0
print("  ADVERSARIAL: can the BURST STRUCTURE alone fake the tone?")
print("  Synthetic PSK1 with ZERO clock offset, plus a 2ms gap every `burst` ms.\n")
print(f"  {'true ppm':>8} {'burst ms':>9} {'gaps':>5} {'measured':>9} {'peak/med':>9}  verdict")
for true_ppm in (0.0, 131.0):
    for burst_ms in (507.9, 250.0, 120.0, 32.8):
        D = true_ppm * 1e-6 * 62500.0
        n = np.arange(N)
        bits = rng.integers(0, 2, N // 32 + 2) * 2 - 1
        d = np.repeat(bits, 32)[:N]
        env = np.ones(N)
        period = int((burst_ms + 2.0) / 1000.0 * 125000)
        gap = int(0.002 * 125000)
        ngaps = 0
        # ⚠ random phase, so the gap is not pinned to the record start
        start = rng.integers(0, period)
        for g0 in range(start, N, period):
            env[g0:g0 + gap] = 0.0
            if g0 < N:
                ngaps += 1
        base = d * np.cos(2 * np.pi * D * n * T + 0.7) * env
        x = ((-1.0) ** n) * base * 40.0 + rng.normal(0, 0.5 * 40.0, N) + 500.0
        ppm, hz, snr, binppm = offset_ppm(x)
        gated = snr >= 6.0
        if true_ppm == 0.0:
            ok = "✓ correctly silent" if not gated else f"⛔ FAKED A TONE at {hz:.2f} Hz"
        else:
            ok = f"✓ found it ({hz:.2f} Hz)" if gated and abs(ppm - 131) < 15 else "⛔ LOST IT"
        print(f"  {true_ppm:8.1f} {burst_ms:9.1f} {ngaps:5d} {ppm:9.1f} {snr:9.1f}  {ok}")
