#!/usr/bin/env python3
"""Capture what a Chameleon emulation actually puts on the coil, via the Flipper's raw reader.

    ./flipraw.py capture awid  --emu /dev/tty.usbmodemC3A1656543DE1
    ./flipraw.py analyse /tmp/awid.raw --expect 64,80

⭐ WHY THIS EXISTS, AND WHY IT IS NOT `tonehist.py`. U11 asks one question — does our FSK2a
emitter put TWO tones on the air or one — and the previous answer to it was withdrawn. C401:
`tonehist.py` printed `ZERO long tones — this is U11's signature` for an EM410X emission that
the receiving reader DECODED CORRECTLY. It binned a decaying curve and read it as two bands.
⇒ The lesson is not "write another histogram". It is that a histogram with no PEAK STRUCTURE
is not evidence of anything, and that the analyser must be made to FAIL on a known-good
reference before its verdict on the unknown is allowed to count (METHOD.md M50).

⛔⛔ `rfid raw_analyze` IS NOT THE ROUTE. It wedged the Flipper twice (AUTOPILOT.md §5) —
72,518 lines of pulse/period pairs through a CDC link, and the second time it died mid-CONTROL.
The sanctioned route is `rfid raw_read` to a FILE plus a binary-safe fetch, which is what this
does. ⚠ `raw_read` takes a FULL PATH: `rfid raw_read ask emctl` answers "File is not RFID raw
file", but `rfid raw_read ask /ext/lfrfid/emctl.ask.raw` works.

⭐ THE FILE FORMAT, READ OFF THE WIRE RATHER THAN OUT OF A HEADER WE DO NOT HAVE:
    "RIFL" | u32 version=1 | f32 frequency=125000.0 | f32 duty=0.5 | u32 max_block=2048
    then blocks: u32 length, then LEB128 varints, alternating PULSE, DURATION, in MICROSECONDS.
The unit is not assumed — it is CONFIRMED by the control, whose durations land on 512 and 1024,
which is exactly EM410X's RF/64 Manchester half-bit and full-bit at 125 kHz.
"""

import argparse
import struct
import sys
import time
from collections import Counter

try:
    import serial
except ImportError:
    sys.exit("pyserial missing — use ../../software/script/.venv/bin/python")

FLIPPER = "/dev/tty.usbmodemflip_Matthew1"
BAUD = 115200
MAGIC = b"RIFL"


def varints(blob):
    """LEB128 stream -> values. Stops cleanly on a truncated tail."""
    out, i, n = [], 0, len(blob)
    while i < n:
        val, shift, ok = 0, 0, False
        while i < n:
            b = blob[i]; i += 1
            val |= (b & 0x7F) << shift
            if not (b & 0x80):
                ok = True
                break
            shift += 7
            if shift > 28:
                return out
        if not ok:
            break
        out.append(val)
    return out


def parse(raw):
    """(pulses, durations) in microseconds, or SystemExit if this is not a RIFL file."""
    if raw[:4] != MAGIC:
        raise SystemExit("⛔ not a Flipper RIFL raw file (got %r)" % raw[:4])
    ver, freq, duty, maxblk = struct.unpack("<IffI", raw[4:20])
    vals, i = [], 20
    while i + 4 <= len(raw):
        (blen,) = struct.unpack("<I", raw[i:i + 4]); i += 4
        if blen == 0 or i + blen > len(raw):
            vals += varints(raw[i:])
            break
        vals += varints(raw[i:i + blen]); i += blen
    # ⛔⛔ DO NOT SPLIT BY GLOBAL PARITY. The pair stream runs continuously across blocks, but
    # a value is occasionally inserted or lost at a block boundary, and a SINGLE such slip swaps
    # pulse and duration for EVERYTHING after it. On one FDX-B capture that corrupted 40% of the
    # pairs and the scorer reported a confident wrong number (C430).
    #
    # ⭐ The invariant that heals it: a pulse is the HIGH part of its own period, so pulse <
    # duration always. Walk the stream and resync on that. Parsing each block independently was
    # tried and is WRONG — it breaks captures that were previously fine, which is how we know
    # the stream really is continuous.
    pulses, durs, slips = [], [], 0
    i = 0
    while i + 1 < len(vals):
        a, b = vals[i], vals[i + 1]
        if a < b:
            pulses.append(a); durs.append(b); i += 2
        else:
            slips += 1; i += 1
    return pulses, durs, dict(version=ver, freq=freq, duty=duty, block=maxblk, slips=slips)


# ⭐⭐ THE PASS CRITERION, FIXED BEFORE ANY NUMBER IS LOOKED AT (this is the whole point).
# A band counts as PRESENT only if it is a genuine LOCAL MAXIMUM of the histogram, not merely
# a bin with counts in it. C401's retracted analysis failed exactly here: every bin of a
# monotonically decaying curve has counts in it, and two of them were called "bands".
def peaks(durations, expect, tol=0.15, floor=0.02):
    """For each expected period, is there a local-maximum bin within tol of it?"""
    hist = Counter(durations)
    total = len(durations) or 1
    # 1 us bins are too fine for jitter; 4 us is well under the 16 us that separates 64 from 80.
    binned = Counter((d // 4) * 4 for d in durations)
    found = {}
    for e in expect:
        lo, hi = e * (1 - tol), e * (1 + tol)
        cands = [(c, b) for b, c in binned.items() if lo <= b <= hi]
        if not cands:
            found[e] = (0, None, "no bin in range")
            continue
        c, b = max(cands)
        # local maximum: strictly greater than the bins one step either side
        left, right = binned.get(b - 4, 0), binned.get(b + 4, 0)
        is_peak = c > left and c > right
        share = c / total
        why = ("peak" if is_peak else "shoulder of a slope (left %d, this %d, right %d)"
               % (left, c, right))
        if is_peak and share < floor:
            why = "peak but only %.1f%% of samples" % (100 * share)
            is_peak = False
        found[e] = (c, b if is_peak else None, why)
    return found, hist


# ⭐⭐ THE SECOND PASS CRITERION, ALSO FIXED BEFORE ANY NUMBER IS LOOKED AT.
# Every earlier RF/10 measurement on this question was scored against a REMEMBERED expectation
# ("the real AWID frame should be 30.4%"). That is how C414 and C411 came to report the same
# 6.2% for what may have been two different frames, with no way to tell afterwards which frame
# produced it. ⇒ The expected fraction is COMPUTED FROM THE FRAME BITS here, so a capture
# carries its own expectation and no run can be confused with another.
#
# ⚠ The bands do not overlap and are not tuned: RF/8 is 64 us and RF/10 is 80 us, so the
# boundary is their midpoint, 72. Anything outside 54..92 is neither tone and is not counted.
TONE_LO, TONE_SPLIT, TONE_HI = 54, 72, 92


def frac_expected(hexframe, short_pulses=6, long_pulses=5):
    """RF/10 share this frame SHOULD emit: a 1 bit is five long tones, a 0 bit six short ones."""
    bits = bin(int(hexframe, 16))[2:].zfill(len(hexframe) * 4)
    ones, zeros = bits.count("1"), bits.count("0")
    lng, sht = ones * long_pulses, zeros * short_pulses
    return (lng / (lng + sht) if (lng + sht) else 0.0), ones, zeros


# ⛔ RIFL's second value is the PERIOD — a high run PLUS the low run after it — not one run.
# Checked on real data: 587+156=743 and 354+157=511 are the recorded durations themselves (C429).
# A criterion written in run lengths scores the wrong quantity and reads 100% against a true 49%.
# ⭐ Parameterised by the HALF-BIT, because the two biphase emitters differ only in scale:
# GProxII RF/64 -> half-bit 256us -> periods 512/768/1024; FDX-B RF/32 -> 128us -> 256/384/512.
BIPHASE_HALFBIT = 256


def biphase_periods(halfbit=BIPHASE_HALFBIT):
    return (2 * halfbit, 3 * halfbit, 4 * halfbit)


BIPHASE_PERIODS = biphase_periods()


def periods_expected_biphase(hexframe, reps=40, halfbit=BIPHASE_HALFBIT, invert=False,
                             phase=0):
    """Biphase (gproxii): EVERY bit carries a boundary transition and a 1 bit adds a mid-bit one,
    so a 0 bit is ONE 512 us run and a 1 bit TWO 256 us runs. Derived from gproxii_modulator,
    not from the protocol's name. Pairing consecutive runs into periods gives 512/768/1024."""
    bits = bin(int(hexframe, 16))[2:].zfill(len(hexframe) * 4)
    runs = []
    for _ in range(reps):
        for b in bits:
            held = (b == "1") if invert else (b == "0")
            runs += [2 * halfbit] if held else [halfbit, halfbit]
    # ⚠ PHASE IS A PROPERTY OF THIS MODEL, NOT OF THE EMITTER. A period is a HIGH run plus the
    # LOW run after it; which run is high depends on the polarity the modulator picks, so there
    # are two pairings and only frames with MIXED run lengths can tell them apart. Both are
    # reported (C430) rather than one being chosen to fit.
    per = [runs[i] + runs[i + 1] for i in range(phase, len(runs) - 1, 2)]
    n = len(per) or 1
    ks = biphase_periods(halfbit)
    return {k: per.count(k) / n for k in ks}, bits.count("1"), bits.count("0")


# ⚠ tol 0.12, not 0.15: at 0.15 the 3h and 4h bands OVERLAP (384*1.15 > 512*0.85) and a
# period lands in two bands at once, pushing coverage over 100%.
def periods_measured(durations, halfbit=BIPHASE_HALFBIT, tol=0.12):
    n = len(durations) or 1
    out = {}
    for k in biphase_periods(halfbit):
        lo, hi = k * (1 - tol), k * (1 + tol)
        out[k] = sum(1 for d in durations if lo <= d <= hi) / n
    return out


def frac_measured(durations, bands=None):
    """RF/10 share actually captured, counting only durations that are one tone or the other."""
    lo, split, hi = bands or (TONE_LO, TONE_SPLIT, TONE_HI)
    sht = sum(1 for d in durations if lo <= d < split)
    lng = sum(1 for d in durations if split <= d <= hi)
    return (lng / (lng + sht) if (lng + sht) else 0.0), sht, lng


class Flip:
    def __init__(self, port=FLIPPER):
        self.s = serial.Serial(port, BAUD, timeout=0.3)
        time.sleep(0.4); self.s.reset_input_buffer()

    def close(self):
        self.s.close()

    def cmd(self, c, wait, etx=False):
        self.s.reset_input_buffer(); self.s.write((c + "\r\n").encode())
        buf, t = b"", time.time() + wait
        while time.time() < t:
            ch = self.s.read(4096)
            if ch: buf += ch
            else: time.sleep(0.05)
        if etx:
            self.s.write(b"\x03"); t = time.time() + 3
            while time.time() < t:
                ch = self.s.read(4096)
                if ch: buf += ch
                else: time.sleep(0.05)
        return buf.decode("utf-8", "replace")

    def raw_read(self, path, seconds):
        out = self.cmd("rfid raw_read ask %s" % path, seconds, etx=True)
        if "not RFID raw file" in out or "Usage" in out:
            raise SystemExit("⛔ the Flipper refused raw_read: %r" % out[:200])

    def fetch(self, path):
        """storage read_chunks, binary-safe. It prints `Ready?` and waits for ONE char per chunk."""
        self.s.reset_input_buffer()
        self.s.write(("storage read_chunks %s 4096\r\n" % path).encode())
        buf, t = b"", time.time() + 5
        while b"Ready?" not in buf and time.time() < t:
            buf += self.s.read(4096)
        if b"Size:" not in buf:
            raise SystemExit("⛔ read_chunks gave no size: %r" % buf[:200])
        size = int(buf.split(b"Size:")[1].split(b"\r\n")[0])
        data = buf.split(b"Ready?\r\n", 1)[1] if b"Ready?\r\n" in buf else b""
        while len(data) < size:
            chunk, t = b"", time.time() + 5
            while time.time() < t and len(data) + len(chunk) < size:
                c = self.s.read(4096)
                if c:
                    chunk += c; t = time.time() + 1.0
                else:
                    self.s.write(b"\r"); time.sleep(0.1)
            if not chunk:
                break
            data += chunk
        # the tail carries the CLI prompt back; the block walker stops on a short block anyway
        return data[:size]


def arm(emu, proto, slot=8, raw=None):
    """Put ONE protocol on the emulator, all four things M46 requires."""
    import subprocess, os
    here = os.path.dirname(os.path.abspath(__file__))
    py = os.path.join(here, "../../software/script/.venv/bin/python")
    cu = os.path.join(here, "../../software/script/cu.py")
    ECFG = {
        "em410x":  ("EM410X",  "lf em 410x econfig -s %d --id DEADBEEF88" % slot),
        "awid":    ("AWID",    "lf awid econfig -s %d --raw 011d81711dd1181111111111" % slot),
        "hidprox": ("HIDProx", "lf hid prox econfig -s %d -f H10301 --fc 123 --cn 4567" % slot),
        "ioprox":  ("IOProx",  "lf ioprox econfig -s %d --ver 1 --fc 83 --cn 1337" % slot),
        "gproxii": ("GProxII", "lf gproxii econfig -s %d --raw f84602a46119d4a114211046" % slot),
        "fdxb":    ("FDXB",    "lf fdxb econfig -s %d --raw 00339a080402079f8040797788040201" % slot),
    }
    if proto not in ECFG:
        raise SystemExit("no econfig for %s" % proto)
    t, ec = ECFG[proto]
    if raw is not None:
        if proto not in ("awid", "gproxii", "fdxb"):
            raise SystemExit("--raw is only wired for awid, gproxii and fdxb")
        ec = "lf %s econfig -s %d --raw %s" % (proto, slot, raw)
    cmds = ["hw connect -p %s" % emu, "hw slot type -s %d -t %s" % (slot, t),
            "hw slot enable -s %d --lf" % slot, ec,
            "hw slot change -s %d" % slot, "hw mode -e"]
    r = subprocess.run([py, cu] + cmds, capture_output=True, text=True)
    if "success" not in r.stdout:
        raise SystemExit("⛔ arming %s failed: %s" % (proto, (r.stdout + r.stderr)[-300:]))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("capture")
    c.add_argument("proto")
    c.add_argument("--emu", default="/dev/tty.usbmodemC3A1656543DE1")
    c.add_argument("--seconds", type=float, default=4.0)
    c.add_argument("--out", default=None)
    c.add_argument("--expect", default=None, help="comma-separated periods in us")
    c.add_argument("--no-arm", action="store_true")
    c.add_argument("--raw", default=None, help="12-byte AWID frame in hex, overrides the econfig")
    c.add_argument("--bands", default=None,
                   help="lo,split,hi in us for --frac; default 54,72,92 (RF/8 vs RF/10)")
    c.add_argument("--halfbit", type=int, default=BIPHASE_HALFBIT,
                   help="biphase half-bit in us: 256 for gproxii (RF/64), 128 for fdxb (RF/32)")
    c.add_argument("--invert", action="store_true",
                   help="fdxb's sense: a mid-bit transition means ZERO, not one")
    c.add_argument("--biphase", action="store_true",
                   help="score the measured whole-bit share against the share the frame implies")
    c.add_argument("--frac", action="store_true",
                   help="score measured RF/10 share against the share the frame implies")
    a = ap.parse_args()

    path = "/ext/lfrfid/%s.ask.raw" % a.proto
    if not a.no_arm:
        arm(a.emu, a.proto, raw=a.raw)
        time.sleep(1.0)
    f = Flip()
    try:
        f.raw_read(path, a.seconds)
        data = f.fetch(path)
    finally:
        f.close()
    out = a.out or "/tmp/%s.ask.raw" % a.proto
    open(out, "wb").write(data)
    pulses, durs, hdr = parse(data)
    print("  %s: %d bytes, %d pulse/duration pairs, carrier %.0f Hz, %d resyncs"
          % (a.proto, len(data), len(durs), hdr["freq"], hdr["slips"]))
    if not durs:
        print("  ⛔ NOTHING CAPTURED — the emitter was silent or the pad is not coupled.")
        return 1
    top = Counter((d // 4) * 4 for d in durs).most_common(8)
    print("  top duration bins (us): " + ", ".join("%d:%d" % (b, c) for b, c in top))
    if a.biphase:
        if not a.raw:
            raise SystemExit("--biphase needs --raw: the expectation comes from the frame bits")
        got = periods_measured(durs, halfbit=a.halfbit)
        best = None
        for ph in (0, 1):
            exp, ones, zeros = periods_expected_biphase(a.raw, halfbit=a.halfbit,
                                                        invert=a.invert, phase=ph)
            worst = max(abs(got[k] - exp[k]) for k in biphase_periods(a.halfbit))
            if best is None or worst < best[0]:
                best = (worst, ph, exp, ones, zeros)
        worst, ph, exp, ones, zeros = best
        print("    frame %s -> %d one-bits, %d zero-bits%s"
              % (a.raw, ones, zeros, " (inverted sense)" if a.invert else ""))
        for k in biphase_periods(a.halfbit):
            print("    %4d us: expected %5.1f%%   measured %5.1f%%   (%+.1f points)"
                  % (k, 100 * exp[k], 100 * got[k], 100 * (got[k] - exp[k])))
        print("    covered %.1f%%; worst band error %.1f points (model pairing phase %d)"
              % (100 * sum(got.values()), 100 * worst, ph))
    if a.frac:
        if not a.raw:
            raise SystemExit("--frac needs --raw: the expectation comes from the frame bits")
        exp, ones, zeros = frac_expected(a.raw)
        bands = tuple(int(x) for x in a.bands.split(",")) if a.bands else None
        got, sht, lng = frac_measured(durs, bands)
        print("    frame %s -> %d one-bits, %d zero-bits" % (a.raw, ones, zeros))
        print("    RF/10 share: expected %.1f%%   measured %.1f%%   (%d long, %d short in band)"
              % (100 * exp, 100 * got, lng, sht))
        print("    verdict: %s" % ("WITHIN 3 points" if abs(got - exp) <= 0.03
                                   else "SHORT BY %.1f points" % (100 * (exp - got))))
    if a.expect:
        exp = [int(x) for x in a.expect.split(",")]
        found, _ = peaks(durs, exp)
        for e in exp:
            cnt, at, why = found[e]
            mark = "✓" if at else "✗"
            print("    %s %d us -> %s" % (mark, e, why))
        return 0 if all(v[1] for v in found.values()) else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
