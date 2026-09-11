import sys, numpy as np
FS=125000.0
W=40
def clean(p):
    x = np.frombuffer(open(p,'rb').read(),dtype=np.uint8).astype(float)[400:]
    n=len(x)//W
    keep=[]
    for i in range(n):
        seg=x[i*W:(i+1)*W]
        if np.ptp(seg) <= 60:        # drop burst/glitch windows
            keep.append(seg)
    return x, np.concatenate(keep) if keep else np.array([]), len(keep), n

def spec(y):
    y=y-y.mean(); w=np.hanning(len(y)); S=np.abs(np.fft.rfft(y*w)); S[0]=0
    f=np.fft.rfftfreq(len(y),1/FS)
    return f,S

def band(f,S,lo,hi):
    m=(f>=lo)&(f<hi); return S[m].sum()

names=[]; specs={}
for p in sys.argv[1:]:
    nm=p.split('/')[-1].replace('.bin','')
    raw, y, k, n = clean(p)
    f,S = spec(y)
    names.append(nm); specs[nm]=(f,S,y)
    tot=S.sum()
    print(f"\n== {nm}: kept {k}/{n} windows ({100*k/n:.0f}% clean), {len(y)} samples ==")
    print(f"   residual std after glitch removal: {y.std():.2f}")
    for lo,hi,lbl in [(1500,2500,'EM410x bitrate fc/64 ~1953Hz'),
                      (3500,4400,'INDALA bitrate fc/32 ~3906Hz'),
                      (11500,13500,'HID FSK fc/10 =12500Hz'),
                      (14500,16500,'HID FSK fc/8  =15625Hz'),
                      (55000,62500,'INDALA subcarrier fc/2 =62500Hz')]:
        print(f"   {lbl:34s} {100*band(f,S,lo,hi)/tot:5.1f}% of energy")

# ratio vs baseline
if 'baseline' in specs:
    fb,Sb,_=specs['baseline']
    print(f"\n{'='*60}\n BAND ENERGY RATIO vs BASELINE (>1 means tag adds energy)\n{'='*60}")
    print(f" {'band':36s}" + "".join(f"{n:>12s}" for n in names if n!='baseline'))
    for lo,hi,lbl in [(1500,2500,'fc/64 ~1953Hz (EM410x)'),
                      (3500,4400,'fc/32 ~3906Hz (INDALA bit)'),
                      (11500,13500,'fc/10 =12500Hz (HID)'),
                      (14500,16500,'fc/8  =15625Hz (HID)'),
                      (55000,62500,'fc/2  =62500Hz (INDALA sub)')]:
        b=band(fb,Sb,lo,hi)/Sb.sum()
        row=f" {lbl:36s}"
        for n2 in names:
            if n2=='baseline': continue
            f2,S2,_=specs[n2]; v=band(f2,S2,lo,hi)/S2.sum()
            row+=f"{v/b:11.2f}x"
        print(row)
