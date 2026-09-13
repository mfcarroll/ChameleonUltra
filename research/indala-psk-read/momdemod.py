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
