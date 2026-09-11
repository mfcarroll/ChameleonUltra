import sys, numpy as np
FS=125000.0; W=40

def load_clean(p):
    x=np.frombuffer(open(p,'rb').read(),dtype=np.uint8).astype(float)[400:]
    return x

def nyquist_energy(x):
    y=x-x.mean(); w=np.hanning(len(y)); S=np.abs(np.fft.rfft(y*w))
    f=np.fft.rfftfreq(len(y),1/FS); S[0]=0
    tot=S.sum()
    # the single Nyquist bin = exactly 62500 Hz
    return S[-1], 100*S[-1]/tot, f[-1]

def psk1_demod(x):
    """Mix down the fc/2 subcarrier by multiplying by (-1)^n, then low-pass.
       For Indala PSK1 RF/32 one bit = 32 samples."""
    y = x - x.mean()
    z = y * ((-1.0)**np.arange(len(y)))     # shift fs/2 -> DC
    k = 16                                   # half-bit boxcar
    lp = np.convolve(z, np.ones(k)/k, mode='same')
    return lp

print(f"{'capture':10s} {'std':>7s} {'nyq bin mag':>12s} {'nyq %':>7s} {'|PSK env|':>10s} {'PSK/noise':>10s}")
print("-"*62)
res={}
for p in sys.argv[1:]:
    nm=p.split('/')[-1].replace('.bin','')
    x=load_clean(p)
    mag,pct,fny = nyquist_energy(x)
    lp=psk1_demod(x)
    env=np.abs(lp)
    # noise floor: same pipeline but mixing with a frequency that carries no data (fs/2.5)
    y=x-x.mean()
    zn=y*np.cos(2*np.pi*0.4*np.arange(len(y)))
    lpn=np.convolve(zn,np.ones(16)/16,mode='same')
    res[nm]=(env, np.abs(lpn).mean())
    print(f"{nm:10s} {x.std():7.2f} {mag:12.0f} {pct:6.2f}% {env.mean():10.3f} {env.mean()/np.abs(lpn).mean():9.2f}x")

print(f"\nNyquist bin frequency = {fny:.0f} Hz (= fc/2, the Indala subcarrier)")

# For indala: does the PSK envelope show 32-sample bit structure?
if 'indala' in res:
    env=res['indala'][0]
    print(f"\n--- Indala: autocorrelation of PSK envelope (looking for 32-sample bit period) ---")
    e=env-env.mean(); e/= (np.std(e) or 1)
    ac=np.correlate(e,e,'full')[len(e)-1:]; ac/=ac[0]
    for lag in [16,32,64,96,128,256,512,1024,2048]:
        print(f"   lag {lag:5d} ({lag*8:6d} us) : {ac[lag]:+.3f}")
