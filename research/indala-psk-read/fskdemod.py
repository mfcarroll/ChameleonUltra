#!/usr/bin/env python3
"""Host FSK2a demodulator for the AWID / Paradox / Pyramid family.

    ./fskdemod.py caps/awid-flipper/emu_drive7.bin

⭐ WHY FSK IS EASIER HERE THAN ASK WAS, which is not the order §10 expected. The data is in
the SUB-PERIOD, and our sampler takes one sample per carrier cycle, so the two tones are 8
and 10 samples — far above Nyquist and trivially separable. ⛔ The slicer's duty bias, which
wrecked the ASK edge decoder (C145) and forced the bit-centre path (C171), CANCELS here: the
8-sample period splits 3+5 and the 10-sample splits 4+6, and only the SUM is used (C193).

The scheme, from Momentum's `fsk_demod.c` with `fsk_demod_alloc(MIN_TIME, 6, MAX_TIME, 5)`:
a '0' is SIX periods of RF/8 and a '1' is FIVE periods of RF/10 — both exactly 50 carrier
cycles, which is the bit period.
"""
import sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from askdemod import load16
from framedrift import block_dc

SHORT, LONG = 8, 10          # carrier cycles per sub-period, i.e. samples
MID = 9                      # the classifier boundary
VALID_LO, VALID_HI = 6, 13   # a period outside this is not a tone at all
PULSES = {0: 6, 1: 5}        # sub-periods per bit: six RF/8, five RF/10


def periods(x):
    """Slice, then pair adjacent runs into full sub-periods.

    ⚠ Pairing is what makes the duty bias harmless — see the module note."""
    dc = block_dc(x)
    lv = [1 if x[i] > dc[i] else 0 for i in range(len(x))]
    runs, cur, n = [], lv[0], 0
    for v in lv:
        if v == cur:
            n += 1
        else:
            runs.append(n); cur, n = v, 1
    runs.append(n)
    return [runs[i] + runs[i + 1] for i in range(0, len(runs) - 1, 2)]


def bits_from(x):
    """Momentum's `fsk_demod_feed`, transcribed: classify each period, count a run of
    like periods, and on a transition emit (count+1)/pulses bits of the run's value."""
    out, count, last = [], 0, None
    for p in periods(x):
        if not (VALID_LO <= p < VALID_HI):
            count = 0                      # not a tone — drop the partial run
            continue
        pulse = 1 if p >= MID else 0
        count += 1
        if last is not None and pulse != last:
            out.extend([last] * ((count + 1) // PULSES[last]))
            count = 0
        last = pulse
    return out


def awid_frames(bits):
    """AWID: 8-bit preamble 00000001, the same preamble again 96 bits later, and ODD parity
    over every 4 bits of 8..95. All three are the reference's own gate."""
    pre = [0, 0, 0, 0, 0, 0, 0, 1]
    found = []
    for i in range(len(bits) - 104):
        if bits[i:i + 8] != pre or bits[i + 96:i + 104] != pre:
            continue
        frame = bits[i:i + 96]
        if any(sum(frame[8 + k * 4: 12 + k * 4]) % 2 == 0 for k in range(22)):
            continue                        # odd parity per nibble
        found.append((i, "".join(map(str, frame))))
    return found


def awid_payload(frame):
    """The 66 payload bits an AWID frame carries, left-aligned into 9 bytes.

    ⭐ EACH NIBBLE IS THREE DATA BITS PLUS AN ODD-PARITY LSB — that is `protocol_awid_encode`
    read forwards: it takes 3 bits of the decoded data at a time, shifts left one, and fills
    the vacated bit with odd parity. 22 nibbles x 3 = 66 bits, so the last 6 bits of the
    9-byte buffer are NOT carried on the wire and cannot be recovered.

    ⛔ The preamble is NOT part of the payload. Including those 8 bits shifted every
    subsequent bit and made the payload unrecoverable — a search over 512 strip alignments
    found nothing, because the answer was not an alignment at all (C194)."""
    bits = [int(c) for c in frame] if isinstance(frame, str) else frame
    data = []
    for i in range(22):
        data.extend(bits[8 + i * 4: 11 + i * 4])
    v = 0
    for b in data:
        v = (v << 1) | b
    return f"{v << (72 - 66):018x}"


if __name__ == "__main__":
    for path in sys.argv[1:]:
        b = bits_from(load16(path))
        ones = sum(b)
        hits = awid_frames(b)
        name = path.split("/")[-1]
        if hits:
            i, f = hits[0]
            print(f"  {name:24} {len(b):5d} bits  AWID at {i:3d}: {int(f,2):024x}\n"
                  f"  {'':24} payload {awid_payload(f)}   ({len(hits)} frames)")
        else:
            print(f"  {name:24} {len(b):5d} bits  ones {100*ones//max(len(b),1):3d}%  no AWID frame")
