#!/usr/bin/env python3
import sys, numpy as np

FS = 125000.0  # 125 kHz, 8us/sample

def load(p):
    return np.frombuffer(open(p,'rb').read(), dtype=np.uint8).astype(float)

def report(name, x):
    print(f"\n{'='*66}\n {name}   n={len(x)}  ({len(x)*8/1000:.1f} ms)\n{'='*66}")
    print(f" min/max/mean/std : {x.min():.0f} / {x.max():.0f} / {x.mean():.1f} / {x.std():.2f}")
    # clipping
    lo = int((x<=1).sum()); hi = int((x>=254).sum())
    print(f" clipped low/high : {lo} ({100*lo/len(x):.1f}%) / {hi} ({100*hi/len(x):.1f}%)")

    # skip antenna settling
    y = x[300:]
    y = y - y.mean()

    # --- alternation test: 62.5kHz subcarrier = 2 samples/cycle (Nyquist) ---
    d1 = np.abs(np.diff(y)).mean()          # neighbour-to-neighbour
    d2 = np.abs(y[2:] - y[:-2]).mean()      # 2 apart
    print(f" mean|x[n]-x[n+1]|: {d1:.2f}")
    print(f" mean|x[n]-x[n+2]|: {d2:.2f}    ratio d1/d2 = {d1/d2 if d2 else 0:.2f}")
    print("   (ratio >>1 => strong 62.5kHz alternation, i.e. fc/2 subcarrier)")

    # --- spectrum ---
    w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y*w))
    f = np.fft.rfftfreq(len(y), 1/FS)
    S[0] = 0
    top = np.argsort(S)[::-1][:8]
    print(" top spectral peaks:")
    for i in sorted(top, key=lambda i:-S[i]):
        if f[i] <= 0: continue
        # express as RF divisor relative to 125kHz carrier
        div = FS/f[i] if f[i] else 0
        print(f"   {f[i]:8.0f} Hz  mag={S[i]:9.0f}   = carrier/{div:.1f}  ({1e6/f[i]:.1f} us period)")

    # energy in band around 62.5 kHz (Nyquist) vs total
    nyq_band = S[f > 60000].sum()
    print(f" energy >60kHz    : {100*nyq_band/S.sum():.1f}% of total")
    return S, f

files = sys.argv[1:]
data = {}
for p in files:
    nm = p.split('/')[-1].replace('.bin','')
    data[nm] = load(p)
    report(nm, data[nm])

# --- differential: does indala differ from baseline at all? ---
if 'baseline' in data and 'indala' in data:
    b, i = data['baseline'], data['indala']
    print(f"\n{'='*66}\n BASELINE vs INDALA\n{'='*66}")
    print(f" std      baseline={b.std():.2f}   indala={i.std():.2f}")
    print(f" range    baseline={b.max()-b.min():.0f}     indala={i.max()-i.min():.0f}")
