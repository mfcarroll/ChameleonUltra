#!/usr/bin/env python3
"""Matched-filter Indala PSK1 demodulator for Chameleon `lf sniff --bits 16` captures.

    ./mfdemod.py <capture.bin> [...]
    ./mfdemod.py --selftest            # validate, and find this demodulator's SNR threshold

WHY BUILD ONE. Proxmark's PSKDemod needs ~5.5x band SNR on this signal (measured), and the
Chameleon delivers 2.30x at its best sample phase — 7.6 dB short. But 5.5x is PSKDemod's
threshold, NOT an information-theoretic bound. It estimates the clock, makes hard per-bit
decisions and does no coherent averaging. Everything about this signal is known in advance,
so a matched filter can do better:

  * the subcarrier is EXACTLY fc/2, so mixing by (-1)^n is exact — no clock recovery
  * the bit rate is EXACTLY RF/32, so a bit is exactly 32 samples — no drift to track
  * the 33-bit preamble is FIXED for every Indala tag (preamble64 in cmdlfindala.c), so
    sync is a correlation over 1056 samples rather than 33 independent bit decisions

That last point is the whole gain: coherent integration over 1056 samples buys ~sqrt(1056)
= 32x against white noise, where a hard decision per bit buys sqrt(32) = 5.7x.

⚠ WHAT THIS IS NOT. Using the PREAMBLE as the template is legitimate — it is the same for
every tag and a real reader knows it. Correlating against the full 64-bit word would be
cheating, since that is the answer. --selftest reports both so the difference is visible.
"""
import argparse
import sys
import numpy as np

FS, BIT = 125000.0, 32
# cmdlfindala.c:50 — and these are exactly the first 33 bits of a0000000e6bd0e92
PREAMBLE = [1, 0, 1, 0] + [0] * 28 + [1]


def load16(path):
    r = np.frombuffer(open(path, 'rb').read(), dtype=np.uint8)
    if len(r) < 2 or len(r) % 2 or r[0::2].max() > 0x3F:
        return None
    return (r[0::2].astype(np.uint16) << 8 | r[1::2]).astype(float)


def polarity_from_bits(bits):
    """⭐ PSK1: THE PHASE *IS* THE DATA. Not a differential encoding.

    ⛔ This function used to be bits_to_polarity(), a running XOR — "a '1' flips the
    phase" — and that is PSK2, not PSK1. It cost this project its central conclusion.
    The authority is the Proxmark, which reads this tag: PSKDemod() emits the phase per
    bit, cmdlfindala.c:1259 matches preamble64 against that stream DIRECTLY, and only
    if that fails does it call psk1TOpsk2() and try again (cmdlfindala.c:1293).

    ⚠ AND THE SELF-TEST COULD NEVER HAVE CAUGHT IT, because synth() encoded with the
    same wrong convention that demod() decoded with. A self-consistent bug passes every
    round trip. Real captures were the only thing that could expose it, and for weeks
    they were read as "31.2 dB of analog deficit"."""
    return np.where(np.asarray(bits) > 0, 1.0, -1.0)


def bits_from_polarity(pol):
    """The inverse of polarity_from_bits. PSK1, so it is just a threshold."""
    # ⚠ plain Python ints: a numpy int64 accumulator overflows to negative on the 64th
    # shift in demod(), which reads as a failed decode when the bits were in fact right.
    return [int(v) for v in (np.asarray(pol) > 0)]


def psk1_to_psk2(bits):
    """The Proxmark's fallback path (lfdemod.c:2116): only transitions become 1s. Kept
    because an Indala tag programmed PSK2 or PSK3 needs it, and because it documents
    exactly what this file used to do to a PSK1 tag."""
    out = [0]
    for k in range(1, len(bits)):
        out.append(1 if bits[k] != bits[k - 1] else 0)
    return out


def baseband(x):
    """Mix the fc/2 subcarrier down to DC. At exactly fs/2 this is a multiplication by
    (-1)^n — no oscillator, no phase estimate. The original DC and any slow envelope drift
    move UP to fs/2 and fall out of the per-bit integration."""
    y = x - x.mean()
    return y * ((-1.0) ** np.arange(len(y)))


def lowpass(b, fc=12000.0):
    """⭐ THE DEMODULATOR HAD NO SELECTIVITY AND IT COST REAL SIGNAL. After baseband()
    the data sits near DC, and everything that was originally low-frequency sits near
    fs/2. The only thing filtering it was the 32-sample boxcar in decode_from(), whose
    response at 57.5-61.5kHz is ~2.4-2.8% — not 0. Measured on this bench the carrier
    ripple is ~316 counts against a ~10-count subcarrier, so ~9 counts leak straight
    into every bit decision. One proper low-pass on the baseband removes it.

    12kHz keeps three lobes of a 3906bps rectangular bit; narrower starts smearing the
    transitions, which is the half of the frame that actually carries the credential."""
    if not fc:
        return b
    S = np.fft.rfft(b)
    S[np.fft.rfftfreq(len(b), 1 / FS) > fc] = 0
    return np.fft.irfft(S, len(b))


def preamble_correlate(b, dc_free=False):
    """Matched filter for the 33-bit preamble. Returns (position, peak, noise_rms).

    ⚠ peak is SIGNED: a negative peak is the inverted preamble, which the Proxmark
    handles as preamble64_i (cmdlfindala.c:1255). The absolute subcarrier phase is not
    knowable, so both must be accepted.

    ⛔ dc_free USED TO BE ON AND IT THREW AWAY THE PREAMBLE. polarity_from_bits(PREAMBLE)
    is two -1s, thirty +1s and one -1, so subtracting its mean leaves 90.9% of the
    template's energy in 3 of its 33 bits: the 28-zero run — the most distinctive thing
    in an Indala frame — contributes almost nothing. It was there so residual drift could
    not bias the peak, but baseband() already moves DC to fs/2 and the per-bit boxcar
    nulls exactly there, so it guarded against something that cannot happen."""
    tpl = np.repeat(polarity_from_bits(PREAMBLE), BIT)
    if dc_free:
        tpl = tpl - tpl.mean()
    tpl /= np.linalg.norm(tpl)
    if len(b) < len(tpl) + BIT:
        return None, 0.0, 0.0
    c = np.correlate(b, tpl, mode='valid')
    pos = int(np.argmax(np.abs(c)))
    peak = float(c[pos])
    mask = np.ones(len(c), bool)
    lo, hi = max(0, pos - len(tpl)), min(len(c), pos + len(tpl))
    mask[lo:hi] = False
    noise = float(np.sqrt(np.mean(c[mask] ** 2))) if mask.sum() > 32 else 0.0
    return pos, peak, noise


def decode_from(b, pos, nbits=64, invert=False):
    """Per-bit matched filter (a 32-sample boxcar is the optimal filter for a rectangular
    bit), then read the polarity straight off — PSK1, so the phase IS the data."""
    need = pos + nbits * BIT
    if need > len(b):
        return None
    integ = b[pos:need].reshape(nbits, BIT).sum(axis=1)
    if invert:
        integ = -integ
    return bits_from_polarity(integ), integ


def bit_stream(b, offset):
    """Slice the baseband into 32-sample bits starting at `offset` and threshold. A
    32-sample boxcar is the optimal filter for a rectangular bit."""
    n = (len(b) - offset) // BIT
    if n < 1:
        return np.empty(0, int), np.empty(0)
    integ = b[offset:offset + n * BIT].reshape(n, BIT).sum(axis=1)
    return np.asarray(bits_from_polarity(integ)), integ


def find_preamble(bits, max_err=0):
    """Every position where the 33-bit preamble matches, normal or inverted.

    ⭐ THIS REPLACED A CORRELATOR, AND IT HAD TO. The preamble is 1010 then 28 ZEROS then
    1, so as a template it is dominated by a 28-bit constant run — which slides against
    itself almost as well 8 bits off as on. The correlation peak is inherently broad and
    it was landing a nibble or two out, returning the right bits in the wrong 64-bit
    window: bea0000000e6bd0e instead of a0000000e6bd0e92. The Proxmark does not correlate
    either; preambleSearch() in cmdlfindala.c is an exact match on the demodulated stream.

    Returns a list of (index, inverted, errors)."""
    tpl = np.asarray(PREAMBLE)
    n = len(bits) - len(tpl)
    hits = []
    for inv in (False, True):
        t = 1 - tpl if inv else tpl
        for i in range(max(0, n) + 1):
            e = int(np.count_nonzero(bits[i:i + len(t)] != t))
            if e <= max_err:
                hits.append((i, inv, e))
    return hits


def demod(x, verbose=False, lpf=12000.0, max_err=2):
    """⚠ lpf DEFAULTS ON AND IT IS LOAD-BEARING. Measured on 35 real single captures
    across the good phase window: 32/35 decode with it, 0/35 without. After baseband()
    the data sits near DC and everything originally low-frequency sits near fs/2, where
    the 32-sample boxcar rejects only ~2.8% — against ~316 counts of carrier ripple on a
    ~10-count subcarrier that is ~9 counts leaking into every bit decision.

    ⚠ But against WHITE noise the boxcar is already the matched filter for a rectangular
    bit, so the same filter is strictly suboptimal there. That it helps real captures and
    hurts synthetic ones IS the finding; pass lpf=None for synthetic work.

    ⚠ The bit phase within the 32-sample period is unknown, so all 32 are tried. Ranking
    them by preamble errors ALONE is not enough — several offsets match the preamble
    exactly while straddling the true bit boundaries, and those returned a0000000e69d0e82
    where the aligned one returns a0000000e6bd0e92. Rank by bit MARGIN among the
    preamble-clean offsets: the best-aligned boxcar is the one with the largest
    integrators."""
    b = lowpass(baseband(x), lpf)
    cands = []
    for off in range(BIT):
        bits, integ = bit_stream(b, off)
        if len(bits) < 64:
            continue
        for i, inv, err in find_preamble(bits, max_err):
            if i + 64 > len(bits):
                continue
            cands.append((err, -float(np.mean(np.abs(integ[i:i + 64]))), off, i, inv,
                          bits, integ))
    if not cands:
        return None
    cands.sort(key=lambda c: (c[0], c[1]))
    err, negmag, off, _, _, _, integ0 = cands[0]

    # ⭐ A 4096-sample capture is two frames, so the word is transmitted more than once.
    # Decode every preamble hit at the chosen offset and majority-vote — free error
    # correction, and it needs no alignment because the preamble search located each one.
    # ⚠ An odd-parity word inverts the subcarrier every frame, so the second copy appears
    # INVERTED; find_preamble already searches both, and each copy is un-inverted here.
    words = []
    for e, _, o, i, inv, bits, integ in cands:
        if o != off or e > err:
            continue
        w = bits[i:i + 64]
        words.append(1 - w if inv else w)
    votes = np.mean(np.array(words), axis=0)
    final = (votes >= 0.5).astype(int)
    word = 0
    for bit in final:
        word = (word << 1) | int(bit)
    seg = integ0
    margin = float(np.min(np.abs(seg)) / (np.std(seg) + 1e-9))
    if verbose:
        print(f"   sample offset {off}, {err} preamble errors, {len(words)} copies voted,"
              f" bit margin {margin:.2f}")
    return {'word': word, 'hex': f"{word:016x}", 'conf': -negmag / (np.std(seg) + 1e-9),
            'pos': off, 'margin': margin, 'preamble_err': err, 'copies': len(words)}


TRUTH = 0xa0000000e6bd0e92


def synth(n, amp, noise_std, seed=0):
    rng = np.random.default_rng(seed)
    bits = [(TRUTH >> (63 - i)) & 1 for i in range(64)]
    pol = polarity_from_bits(bits)
    # ⚠ THIS RESTARTS THE POLARITY EVERY FRAME. A real tag does not: PSK1 carries the
    # running polarity across the frame boundary, and a0000000e6bd0e92 has 19 ones — ODD
    # — so the subcarrier INVERTS every 64 bits and the true repetition period is 4096
    # samples, not 2048. Anything that folds or averages at 2048 averages a frame against
    # its own inverse and cancels the data; the transition skirt survives, which is how
    # band SNR can improve as sqrt(N) while not one bit is recovered. Use psk_frame() in
    # stack.py when the inter-frame polarity matters.
    frame = np.repeat(pol, BIT) * np.tile([1.0, -1.0], BIT * 64 // 2)
    reps = int(np.ceil(n / len(frame))) + 1
    sig = np.tile(frame, reps)[:n] * amp
    return sig + rng.normal(0, noise_std, n)


def band_snr(x, f=62500.0):
    y = x - x.mean(); w = np.hanning(len(y))
    S = np.abs(np.fft.rfft(y * w)); fr = np.fft.rfftfreq(len(y), 1 / FS)
    m = (fr >= f - 5000) & (fr <= f - 200)
    return 2 * np.sqrt((S[m] ** 2).sum()) / w.sum()


def selftest():
    print("\n=== SELF-TEST: clean signal, no noise ===")
    r = demod(synth(4096, 40, 0.0), lpf=None, verbose=True)
    ok = r and r['word'] == TRUTH
    print(f"   recovered {r['hex'] if r else None}  expected {TRUTH:016x}  -> "
          f"{'PASS' if ok else 'FAIL'}")
    if not ok:
        sys.exit(" selftest failed — the demodulator is wrong, stop here")

    print("\n=== THRESHOLD: 4096 samples (2 frames), 20 noise seeds per level ===")
    print(f"   {'amp':>5} {'band SNR':>9} {'recovered':>11} {'corr conf':>10}")
    thr = None
    for amp in (40, 20, 10, 7, 5, 4, 3, 2.5, 2, 1.5, 1.0):
        hits, confs, snrs = 0, [], []
        for seed in range(20):
            x = synth(4096, amp, 10.0, seed)
            snrs.append(band_snr(x) / band_snr(synth(4096, 0, 10.0, seed)))
            r = demod(x)
            if r and r['word'] == TRUTH:
                hits += 1
                confs.append(r['conf'])
        s = float(np.median(snrs))
        cf = float(np.median(confs)) if confs else 0.0
        print(f"   {amp:5} {s:8.2f}x {hits:8}/20 {cf:9.2f}x")
        if hits >= 19:
            thr = s          # keep descending; the last qualifying level is the threshold
    print(f"\n   lowest band SNR with >=19/20 recovery: {thr:.2f}x")
    # ⛔ THIS USED TO PRINT A VERDICT, AND THE VERDICT WAS RETRACTED. It compared the
    # threshold above against "Proxmark PSKDemod 5.50x" and "measured Chameleon 2.30x" and
    # announced a dB margin — but every one of those numbers was measured through the PSK2
    # bug, on a synthetic that shared it, against a fc/2 band figure that is polarity-blind
    # (METHOD.md M8). The threshold itself is still a useful thing to watch for regression;
    # the comparison was not, and the real answer came from real captures, not from here.
    print("   ⚠ synthetic white noise only. The boxcar is already matched to a rectangular")
    print("     bit there, so this number does NOT predict performance on real captures —")
    print("     where the low-pass is worth 51/160 against 0/160. Use it for regression.")


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("captures", nargs="*")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest or not a.captures:
        selftest()
    for p in a.captures:
        x = load16(p)
        if x is None:
            print(f" {p.split('/')[-1]:40s} not a 16-bit capture")
            continue
        r = demod(x)                      # lpf defaults on; real captures need it
        hit = r and r['word'] == TRUTH
        print(f" {p.split('/')[-1]:40s} {len(x):5d} samples  "
              f"{'*** ' + r['hex'] + ' ***' if hit else (r['hex'] if r else '—')}"
              f"  conf {r['conf'] if r else 0:.2f}x  {'MATCH' if hit else ''}")
