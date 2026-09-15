#!/usr/bin/env python3
"""⛔⛔ REFUTED BY ITS OWN CONTROLS (C456) — KEPT SO IT IS NOT REBUILT. DO NOT "FIX" IT.

The idea was sound and the instrument will not support it. A per-run recovery needs the length of
each HIGH and each LOW separately, and the Flipper gives those only with the comparator's bias
added to every pulse and subtracted from every gap — 43-89us measured (C452) against a 128-256us
quantum. **Only a PERIOD is unbiased**, and a period tells you `n_high + n_low` without the split.
That is exactly why C455's quantum fit says "periods only" in its own first paragraph, and this
tool ignored it. M54, a third time.

The controls are what say so, not an argument: **fdxb decodes byte-exact and recovers with 52.8%
of its runs ambiguous and NO period at its predicted 256 quanta** (0.732 against a 0.523 null);
gproxii, also byte-exact, reaches only 0.836 at its predicted 192. A method that fails on
known-good arms cannot be used to judge PAC, whatever PAC scores.

⇒ The per-entry `channel_0` values are NOT recoverable from this reader and must be read off the
device. Everything below is the original design, left intact.

⭐ RECOVER THE EMITTED BITSTREAM FROM THE AIR — a recovery, not a fit.

C455 established that every emitted duration is an integer multiple of the emitter's own bit
quantum. So the levels can be READ OFF: divide each run by the quantum, round, and emit that many
bits at that level. No alignment search, no threshold to tune, no maximised score — which is why
C440's trap (M55) does not apply to the recovery itself.

⭐ PREDICTIONS FIXED BEFORE THE RUN, from each emitter's own header and modulator:
    pac      128 entries, one per bit, counter_top 32 -> quantum 256us, repeat period 128 quanta
    fdxb     128 bits, counter_top 32, half-bit       -> quantum 128us, repeat period 256 quanta
    gproxii   96 bits, counter_top 64, half-bit       -> quantum 256us, repeat period 192 quanta
⇒ three DIFFERENT predicted periods, so a tool that returns a constant cannot pass all three.

⛔ The rounding must be shown to be unambiguous or the recovery means nothing: a run landing near
a half-quantum could go either way, and the comparator's bias (43-89us measured, C452) is a
fraction of the quantum. The ambiguous fraction is printed for every capture and a high one voids
the reading rather than being explained away.
⛔ A HIGH run is the pulse; the LOW run after it is period - pulse (C429).
"""
import argparse, os, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flipraw

# tag -> (quantum us, predicted repeat period in quanta)
ARMS = {"pac_1337BEEF": (256, 128), "pac_55555555": (256, 128),
        "fdxb": (128, 256), "gprox": (256, 192)}


def levels(pulses, durs, q):
    """Alternating runs -> a level per quantum. Returns (sequence, ambiguous fraction)."""
    seq, amb, n = [], 0, 0
    for p, d in zip(pulses, durs):
        for lvl, run in ((1, p), (0, d - p)):
            x = run / q
            k = round(x)
            n += 1
            if abs(x - int(x) - 0.5) < 0.2:
                amb += 1
            seq.extend([lvl] * max(k, 0))
    return seq, (amb / n if n else 1.0)


def periodicity(seq, p):
    m = len(seq) - p
    if m <= 0:
        return 0.0
    return sum(1 for i in range(m) if seq[i] == seq[i + p]) / m


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tags", nargs="*", default=list(ARMS))
    ap.add_argument("--max-p", type=int, default=600)
    a = ap.parse_args()
    f = flipraw.Flip()
    try:
        caps = {t: f.fetch("/ext/lfrfid/airduty_%s.ask.raw" % t) for t in a.tags}
    finally:
        f.close()

    for tag, data in caps.items():
        q, want_p = ARMS[tag]
        pulses, durs, meta = flipraw.parse(data)
        if not durs:
            print("  %-14s ⛔ empty capture" % tag)
            continue
        seq, amb = levels(pulses, durs, q)
        scores = [(periodicity(seq, p), p) for p in range(1, a.max_p + 1)]
        best = max(scores)
        hits = [p for s, p in scores if s > 0.95]
        # null: the same levels in random order, same search
        rng = random.Random(7)
        sh = seq[:]
        rng.shuffle(sh)
        nbest = max((periodicity(sh, p), p) for p in range(1, a.max_p + 1))
        print("  %-14s q=%3dus  bits=%6d  ambiguous %.1f%%   predicted period %d quanta"
              % (tag, q, len(seq), 100 * amb, want_p))
        print("     at p=%d: %.3f    best p=%d at %.3f    null best %.3f"
              % (want_p, periodicity(seq, want_p), best[1], best[0], nbest[0]))
        if hits:
            print("     p over 0.95: smallest %d, all %s" % (min(hits), hits[:10]))
        else:
            print("     ⛔ no period over 0.95 in [1, %d]" % a.max_p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
