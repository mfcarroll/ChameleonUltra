import sys, numpy as np
for p in sys.argv[1:]:
    x = np.frombuffer(open(p,'rb').read(), dtype=np.uint8)
    nm = p.split('/')[-1]
    print(f"\n===== {nm} : samples 400-520 =====")
    for i in range(400, 520, 20):
        row = x[i:i+20]
        print(f" {i:4d}  " + " ".join(f"{b:02x}" for b in row))
    # ascii waveform, 1 col per sample, 400..760
    print(f"  --- waveform 400..760 (36 rows of 10? no: 3 blocks of 120) ---")
    for blk in range(3):
        s = 400 + blk*120
        seg = x[s:s+120]
        for lvl in range(7, -1, -1):
            hi = (lvl+1)*32; lo = lvl*32
            line = "".join("#" if lo <= v < hi else " " for v in seg)
            print(f"  {lo:3d} |{line}|")
        print(f"      +{'-'*120}+  (start {s})")
