import sys, numpy as np
for p in sys.argv[1:]:
    x = np.frombuffer(open(p,'rb').read(), dtype=np.uint8).astype(float)
    nm = p.split('/')[-1].replace('.bin','')
    # per-40-sample window: local peak-to-peak, as an activity map
    w = 40
    n = len(x)//w
    pp = np.array([np.ptp(x[i*w:(i+1)*w]) for i in range(n)])
    print(f"\n===== {nm}: local peak-to-peak per {w}-sample ({w*8}us) window =====")
    print(f"  (each col = {w} samples; row = ptp magnitude)")
    for lvl in [200,150,100,60,30,15,0]:
        line = "".join("#" if v >= lvl else " " for v in pp)
        print(f"  {lvl:3d} |{line}|")
    print(f"      +{'-'*n}+")
    print(f"      ^0{' '*(n-14)}sample {n*w}")
    # stats excluding transient
    y = x[400:]
    print(f"  steady-state (samples 400+): min={y.min():.0f} max={y.max():.0f} mean={y.mean():.1f} std={y.std():.2f}")
    lo=int((y<=1).sum()); hi=int((y>=254).sum())
    print(f"  clipped low/high in steady state: {lo} / {hi}")
