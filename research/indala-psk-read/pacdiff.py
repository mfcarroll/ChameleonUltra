#!/usr/bin/env python3
"""PAC emulate: VALIDATE THE DECODER BEFORE BLAMING THE FIRMWARE (C434 -> this unit).

⛔⛔ C434 decoded one PAC emission at 97/128 and REFUSED to call that a firmware defect, because
the decoder had no control. This is that control, and it is the C429 differential method.

`pac_build_bitstream` is a pure function of the 8-byte ASCII card ID, and payload bytes 0..2 are
the constants STX/'2'/'0'. So for ANY two credentials:

  * bits 0..37 (sync + the three fixed UART frames) are IDENTICAL — an invariant prefix that the
    data cannot reach. Any recovered difference in that window is the decoder's, full stop.
  * D = {i : E_A[i] != E_B[i]} lies entirely in bits 38..127.

PASS = the recovered difference set R equals D EXACTLY. Only then does an absolute mismatch mean
anything about the emitter. FAIL = R != D; the decoder does not track the data and no firmware
claim is available at any score.

⚠ THE BIAS (C429/C430): the Flipper's comparator stretches every HIGH run by ~96us and shortens
every LOW run by the same; only PERIODS are unbiased. NRZ decoding is necessarily run-level, so it
sits squarely on the biased quantity — hence the explicit correction here AND the residual report,
which is what tells you whether the correction is real or a fudge.
"""
import argparse, os, subprocess, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from flipraw import Flip, parse

BIT_US   = 256.0   # RF/32 at 125 kHz
RUN_BIAS = 96.0    # C429/C430: high runs long by this, low runs short by it


def build(card):
    """Mirror of pac_build_bitstream() in firmware/.../protocols/pac.c."""
    b = card.encode("ascii")
    if len(b) != 8:
        raise SystemExit("card id must be exactly 8 ASCII chars")
    payload = [0x02, ord("2"), ord("0")] + list(b)
    x = 0
    for i in range(3, 11):
        x ^= payload[i]
    payload.append(x)
    bits = [1] * 8                       # 8-bit sync marker 0xFF
    for byte in payload:                 # 12 UART frames
        bits.append(0)                   # start
        ones = 0
        for i in range(7):               # 7 data bits, LSB first
            v = (byte >> i) & 1
            bits.append(v)
            ones += v
        bits.append(0 if (ones & 1) else 1)   # odd parity
        bits.append(1)                        # stop
    assert len(bits) == 128
    return bits


def runs_to_bits(pulses, durs):
    """(pulse, period) pairs -> NRZ bit stream, with the comparator bias removed.

    A pair is one HIGH run of `pulse` and one LOW run of `period - pulse`. Returns the bit list
    and the rounding residuals, in units of a bit period — the residuals are the decode's own
    health check, not decoration."""
    bits, resid = [], []
    for p, d in zip(pulses, durs):
        for run, level in ((p - RUN_BIAS, 1), (d - p + RUN_BIAS, 0)):
            n = run / BIT_US
            k = int(round(n))
            if k < 1:
                k = 1
            resid.append(n - k)
            bits.extend([level] * k)
    return bits, resid


def align(bits, invert):
    """Frame starts 8 bits before the END of a >=8 long sync run.

    ⚠ On air the run is NINE: the previous frame's stop bit abuts the sync marker. Aligning on the
    START of the run therefore lands one bit early and corrupts everything after — C434's first
    attempt did exactly that."""
    want = 0 if invert else 1
    out, i, n = [], 0, len(bits)
    while i < n:
        if bits[i] != want:
            i += 1
            continue
        j = i
        while j < n and bits[j] == want:
            j += 1
        if j - i >= 8 and j - 8 + 128 <= n:
            out.append(j - 8)
        i = j
    return out


def best_frame(bits, expected, invert):
    """The frame at the alignment that best matches `expected`. Score is reported, never gated on."""
    best = (-1, None, None)
    for s in align(bits, invert):
        f = [b ^ (1 if invert else 0) for b in bits[s:s + 128]]
        sc = sum(1 for a, b in zip(f, expected) if a == b)
        if sc > best[0]:
            best = (sc, s, f)
    return best


def capture(emu, card, path, seconds, port):
    cmds = ["hw connect -p %s" % emu, "hw slot type -s 8 -t PAC", "hw slot enable -s 8 --lf"]
    r = subprocess.run([PY, CU] + cmds, capture_output=True, text=True)
    if "success" not in r.stdout:
        raise SystemExit("⛔ slot prep failed: %s" % (r.stdout + r.stderr)[-300:])
    # ⛔ the econfig runs ALONE and is judged on its OWN output (C431): a refused econfig inside a
    # batch still leaves the slot armed, credential-less, and the batch still says "success".
    e = subprocess.run([PY, CU, "hw connect -p %s" % emu,
                        "lf pac econfig -s 8 --cn %s" % card], capture_output=True, text=True)
    low = (e.stdout + e.stderr).lower()
    if "success" not in low or any(w in low for w in
                                  ("unrecognized", "invalid", "usage:", "error", "must be")):
        raise SystemExit("⛔ econfig %s refused: %s" % (card, (e.stdout + e.stderr)[-300:]))
    r = subprocess.run([PY, CU, "hw connect -p %s" % emu,
                        "hw slot change -s 8", "hw mode -e"], capture_output=True, text=True)
    if "success" not in r.stdout:
        raise SystemExit("⛔ slot activate failed: %s" % (r.stdout + r.stderr)[-300:])
    time.sleep(1.0)
    f = Flip(port) if port else Flip()
    try:
        f.raw_read(path, seconds)
        data = f.fetch(path)
    finally:
        f.close()
    return data


HERE = os.path.dirname(os.path.abspath(__file__))
PY = os.path.join(HERE, "../../software/script/.venv/bin/python")
CU = os.path.join(HERE, "../../software/script/cu.py")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", default="CARD0001")
    ap.add_argument("--b", default="0000AAAA")
    ap.add_argument("--emu", default="/dev/tty.usbmodemC3A1656543DE1")
    ap.add_argument("--port", default=None)
    ap.add_argument("--seconds", type=float, default=4.0)
    ap.add_argument("--outdir", default="/tmp")
    a = ap.parse_args()

    exp = {}
    for c in (a.a, a.b):
        exp[c] = build(c)
    D = [i for i in range(128) if exp[a.a][i] != exp[a.b][i]]
    print("  CRITERION (stated in advance, derived from pac_build_bitstream):")
    print("    invariant prefix bits 0..37 must NOT differ")
    print("    D = %d positions, span %d..%d   ones: %s=%d %s=%d"
          % (len(D), D[0], D[-1], a.a, sum(exp[a.a]), a.b, sum(exp[a.b])))
    print("    PASS iff recovered R == D exactly\n")

    frames = {}
    for tag, card in (("A", a.a), ("B", a.b)):
        # ⛔ a path PER CREDENTIAL: a shared path is how a stale capture masquerades as fresh (C432).
        path = "/ext/lfrfid/pacdiff%s.ask.raw" % tag
        data = capture(a.emu, card, path, a.seconds, a.port)
        out = os.path.join(a.outdir, "pacdiff_%s.raw" % card)
        open(out, "wb").write(data)
        pulses, durs, hdr = parse(data)
        if not durs:
            raise SystemExit("⛔ %s captured NOTHING — emitter silent or pad uncoupled." % card)
        bits, resid = runs_to_bits(pulses, durs)
        bad = sum(1 for r in resid if abs(r) > 0.25)
        print("  %s %-8s %d bytes, %d pairs, %d resyncs, %d bits, "
              "residual>0.25bit: %d/%d (%.1f%%)"
              % (tag, card, len(data), len(durs), hdr["slips"], len(bits),
                 bad, len(resid), 100.0 * bad / len(resid)))
        for inv in (False, True):
            sc, st, fr = best_frame(bits, exp[card], inv)
            print("      %-9s best %3d/128 at offset %s"
                  % ("inverted" if inv else "as-captured", sc, st))
            frames[(tag, inv)] = fr

    print()
    for inv in (False, True):
        fa, fb = frames[("A", inv)], frames[("B", inv)]
        if fa is None or fb is None:
            print("  %-11s no alignment found in one of the captures" %
                  ("inverted" if inv else "as-captured"))
            continue
        R = [i for i in range(128) if fa[i] != fb[i]]
        pre = [i for i in R if i < 38]
        extra = sorted(set(R) - set(D))
        missing = sorted(set(D) - set(R))
        verdict = "PASS — decoder validated" if not extra and not missing else "FAIL"
        print("  %-11s |R|=%3d  prefix-violations=%d  extra=%d  missing=%d   %s"
              % ("inverted" if inv else "as-captured", len(R), len(pre),
                 len(extra), len(missing), verdict))
    return 0


if __name__ == "__main__":
    sys.exit(main())
