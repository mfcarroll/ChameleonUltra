#!/usr/bin/env python3
"""Momentum's own FSK demodulator, in Python, over OUR emitter's ideal output.

    ./momdemod.py

⛔ WHY THIS EXISTS. Our AWID emulation round-trips exactly through our own decoder and is read
0 of 6 by the Flipper, with a Gallagher control at 6 of 6 on the same slot (C217). Two
explanations remained and no instrument on this bench separates them:

    (a) the PWM does not emit what the emitter asks for, or
    (b) what the emitter asks for is not what Momentum's demodulator accepts.

⭐ (b) is decidable WITHOUT hardware. Render the emitter's IDEAL output — the level/duration
pairs the PWM entries describe, with no hardware artefacts at all — and run Momentum's own
algorithm over it. If the ideal emission decodes, (b) is dead and the fault is the peripheral.
If it does not, the emitter is wrong and no amount of hardware chasing would have found it.

⚠ THE ALGORITHM IS A TRANSCRIPTION, NOT A RESEMBLANCE — `fsk_demod_feed` in
lib/lfrfid/tools/fsk_demod.c, with AWID's own `fsk_demod_alloc(64-20, 6, 80+20, 5)` from
protocol_awid.c. A demodulator that merely resembles the reference proves nothing about it;
that is the same lesson `framedrift.py` cost a day to learn (C180).
"""

# protocol_awid.c: MIN_TIME (64 - JITTER_TIME), MAX_TIME (80 + JITTER_TIME), JITTER_TIME 20
LOW_TIME, LOW_PULSES = 64 - 20, 6
HI_TIME, HI_PULSES = 80 + 20, 5
MID_TIME = (HI_TIME - LOW_TIME) // 2 + LOW_TIME

# Carrier cycles per tone period, and the microseconds one carrier cycle takes.
SHORT_CYCLES, LONG_CYCLES, US_PER_CYCLE = 8, 10, 8


def emit_ideal(frame_bits):
    """(level, duration) pairs for our emitter's PWM entries, with NO hardware artefacts.

    ⚠ One entry is one whole tone cycle: half high, half low. The renderer in
    ctest/roundtrip.c models exactly this, which is why the round trip there passes."""
    out = []
    for bit in frame_bits:
        cycles = LONG_CYCLES if bit else SHORT_CYCLES
        pulses = HI_PULSES if bit else LOW_PULSES
        half = cycles * US_PER_CYCLE // 2
        for _ in range(pulses):
            out.append((True, half))
            out.append((False, half))
    return out


def fsk_demod(pairs):
    """fsk_demod_feed(), transcribed. Returns the bit stream it produces."""
    time_acc = 0
    count = 0
    last_pulse = None
    bits = []
    for level, duration in pairs:
        if level:
            time_acc = duration          # polarity true: accumulate time = time
            continue
        time_acc += duration
        if LOW_TIME <= time_acc < HI_TIME:
            pulse = time_acc >= MID_TIME
            count += 1
            if last_pulse is not None and pulse != last_pulse:
                data_count = count + 1
                data_count //= HI_PULSES if last_pulse else LOW_PULSES
                bits.extend([1 if last_pulse else 0] * data_count)
                count = 0
            last_pulse = pulse
        else:
            count = 0
    return bits


def main():
    frame_hex = "011db218271bd81111111111"
    want = [int(c) for c in bin(int(frame_hex, 16))[2:].zfill(96)]
    pairs = emit_ideal(want * 4)          # four frames, as the emulator loops
    got = fsk_demod(pairs)
    s = "".join(map(str, got))
    w = "".join(map(str, want))
    print(f"  window [{LOW_TIME}, {HI_TIME})  mid {MID_TIME}")
    print(f"  ideal emission -> {len(got)} bits")
    print(f"  first 96: {s[:96]}")
    print(f"  wanted  : {w}")
    print(f"  frame present anywhere: {'YES' if w in s else 'NO'}")


if __name__ == "__main__":
    main()


# ─────────────────────────────────────────────────────────────────────────────────────────
# ⭐⭐ WHAT AN AC-COUPLED FRONT END DOES TO EACH EMITTER
#
# ⛔ WHY. Three hypotheses for the silent emulations are dead — counter_top magnitude,
# entries per bit, and sequence length — and the one that survives is that every emitter the
# Flipper reads emits a 50% SQUARE in every entry, while GProxII holds a level (C223). The
# mechanism proposed for that is AC coupling: a level that never changes cannot modulate a
# load-switching emulator seen through a high-pass. AWID, whose entries ARE all squares,
# stays unexplained.
#
# ⇒ This sweeps the high-pass time constant and asks, for each emitter, where its emission
# stops decoding. If AWID dies at a far shorter time constant than Gallagher, one mechanism
# covers both failures. If they die together, it covers neither.
#
# ⚠ THE TIME CONSTANT IS NOT MEASURED FOR THIS FRONT END. C204 measured ~27 samples at the
# CHAMELEON's receiver; the Flipper's is a different circuit and nothing here has measured
# it. So this is a sensitivity analysis over a plausible range, not a prediction — and the
# useful output is the ORDERING of the protocols, which does not depend on the exact value.
# ─────────────────────────────────────────────────────────────────────────────────────────

def render_samples(pairs, us_per_sample=1):
    """Level/duration pairs -> a sample train, +1 high and -1 low."""
    out = []
    for level, duration in pairs:
        out.extend([1.0 if level else -1.0] * (duration // us_per_sample))
    return out


def highpass(x, tau):
    """One-pole high-pass, the shape an AC-coupled front end has. tau in samples."""
    if tau <= 0:
        return x[:]
    a = 1.0 - 1.0 / tau
    out, prev_in, prev_out = [], 0.0, 0.0
    for v in x:
        prev_out = a * (prev_out + v - prev_in)
        prev_in = v
        out.append(prev_out)
    return out


def to_pairs(x):
    """Threshold at zero and return (level, duration) runs — what an edge detector sees."""
    pairs, cur, n = [], x[0] > 0, 0
    for v in x:
        lvl = v > 0
        if lvl == cur:
            n += 1
        else:
            pairs.append((cur, n)); cur, n = lvl, 1
    pairs.append((cur, n))
    return pairs


def gallagher_ideal(frame_bits):
    """Gallagher's emitter: ONE entry per bit, always a 50% square, polarity = the data."""
    out = []
    for bit in frame_bits:
        half = 32 * 8 // 2      # RF/32 bit = 32 carrier cycles = 256us; half is 128us
        if bit:
            out.append((True, half)); out.append((False, half))
        else:
            out.append((False, half)); out.append((True, half))
    return out


def gproxii_ideal(frame_bits):
    """GProxII's emitter: one entry per bit, a square for a 1 and a HELD level for a 0."""
    out, level = [], False
    for bit in frame_bits:
        level = not level
        if bit:
            out.append((level, 64 * 8 // 2))
            level = not level
            out.append((level, 64 * 8 // 2))
        else:
            out.append((level, 64 * 8))
    return out


# ⛔⛔ THE LIMIT OF THIS MODEL, BEFORE ANYONE READS A RESULT OFF IT. `highpass` decays toward
# ZERO and `to_pairs` thresholds AT zero, so a held level decays asymptotically and never
# crosses — it survives as a clean long run no matter how short the time constant is. A real
# comparator has hysteresis and a baseline of its own, and a decayed level DOES vanish into it.
# ⇒ This model can say something about emissions made of SQUARES and nothing at all about
# emissions containing HELD LEVELS. The GProxII row below is printed for completeness and must
# not be read as evidence either way.
def sweep():
    awid = [int(c) for c in bin(int("011db218271bd81111111111", 16))[2:].zfill(96)]
    gal = [int(c) for c in bin(int("7feaa31e76d86c6d868cc249", 16))[2:].zfill(96)]
    arms = [
        ("AWID     FSK2a  squares", emit_ideal(awid * 4)),
        ("Gallagher ASK   squares", gallagher_ideal(gal * 4)),
        ("GProxII  biphase HELD  ", gproxii_ideal(gal * 4)),
    ]
    want = "".join(map(str, awid))
    print("\n  tau (samples, 1us each) at which each emission stops decoding as AWID bits")
    print("  ⚠ only the AWID arm can be scored against a frame; the others are scored on")
    print("     whether their EDGE STRUCTURE survives at all (run lengths still quantised).")
    for name, pairs in arms:
        row = []
        for tau in (0, 2000, 1000, 500, 200, 100, 50, 20):
            x = render_samples(pairs)
            got = to_pairs(highpass(x, tau)) if tau else pairs
            if "AWID" in name:
                bits = "".join(map(str, fsk_demod(got)))
                row.append(f"{tau}:{'Y' if want in bits else 'n'}")
            else:
                runs = [d for _, d in got]
                longest = max(runs) if runs else 0
                row.append(f"{tau}:{longest}")
        print(f"  {name}  " + "  ".join(row))


if __name__ == "__main__":
    sweep()
