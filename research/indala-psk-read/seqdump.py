#!/usr/bin/env python3
"""Read the LIVE PWM wave-form entries off an emulating Chameleon and compare them, entry by
entry, with what the emitter's SOURCE says they must be.

    ./seqdump.py pac gprox            # the unit and its control
    ./seqdump.py pac --card EEEEEEEE  # the credential that emits nothing at all

⭐ WHY THIS EXISTS. Every other step of the PAC emulation is verified on hardware and the
emission is still wrong. The frame is byte-exact on a T5577 (C436) and the slot contents are
byte-exact against the host mirror (M45); the descriptor reads back right on the device —
SEQ[0].CNT 512, with gproxii 384 and securakey 384 as controls that had to differ and did
(C454); the base clock is 125 kHz, the burst arithmetic is ceil(500000/(entries*top*8)), and
the emitted bit PERIOD is quantised at exactly 256us with a residual tighter than the gproxii
control's (C455). Yet the duty sits at 87.6% +/- 1.91 across 14 credentials whose predictions
span 40.6-56.2%, r^2 = 0.002 (C451), PAC stands +34.5 points clear of eight ASK arms that fall
in a 3.0-point band (C452), and `EEEEEEEE` emits nothing at all.

⛔ FOUR AIR-SIDE ROUTES HAVE BEEN REFUTED, EACH BY ITS OWN CONTROL: positional alignment
(C440), drop-only tiling (C441), the one-count law (C451, whose +-5us fit missed three
pre-registered predictions by 5000-28000us), bitstream recovery (C456, 52.8% of runs ambiguous
on a byte-exact control). They fail for the same reason: the air carries what the PERIPHERAL
made of the buffer, never the buffer. So this asks the buffer.

⭐ THE PASS CRITERION, FIXED HERE AND DERIVED FROM THE EMITTER SOURCE RATHER THAN FROM MEMORY:

  pac.c:365     channel_0 = bits[i] ? (PAC_RF_PER_BIT + 1) : 0;  counter_top = PAC_RF_PER_BIT
                PAC_RF_PER_BIT is 32, so entry i is (33, 0, 0, 32) or (0, 0, 0, 32), and
                bits[] is pacdiff.build(card) — itself already verified byte-exact against the
                device for EEEEEEEE (C436).
  gproxii.c:91  one entry per bit at counter_top 64: a 1 is a half-bit square carrying its
                phase in channel_0's inversion bit (0x8000 | 32, or 32), a 0 is held for the
                whole bit (65 when high, 0 when low), and the level flips at every bit
                boundary and again in the middle of every 1.

  ALL entries match  => the buffer is right and the fault is in playback or downstream.
  ANY entry differs  => the fault is located, byte-exact, in the modulator.

⛔⛔ THE CONTROL IS NOT OPTIONAL AND IT MUST BE ABLE TO FAIL. gproxii goes through the SAME
firmware command and the SAME host parser; its buffer is 96 entries against pac's 128 and its
counter_top 64 against 32, so a command that returns a constant, a stale buffer, or one arm's
answer for every arm cannot pass it. That is the exact failure C454 caught.

⛔⛔ AND THE DUMP IS VOID WITHOUT A READER FIELD (M56). The modulator runs on field detection,
so with the field down this reads whatever the last arm left behind. The Flipper is held in
`rfid read` for the whole query and the field is verified from the device's own counters before
a single entry is compared.

⛔⛔ THE GUARD IS `playbacks` RISING, NOT THE `emulating` FLAG — and that correction came from a
measurement, not from taste. Driving a Chameleon with the Proxmark's field moved `playbacks
started` from 0 to 2 while `emulating now` read False on all three samples taken during it:
playback is BURSTY, so the instantaneous flag is False most of the time even while the arm is
working perfectly. A guard on that flag would have scored a live emulation VOID and sent the
whole unit back to the air-side routes that are already exhausted.
"""
import argparse, os, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flipraw
import pacdiff

PY = os.path.join(HERE, "../../software/script/.venv/bin/python")
CU = os.path.join(HERE, "../../software/script/cu.py")
SLOT = 8

# name -> (slot type, econfig, expected-entry builder)
ARMS = {}


def pac_expect(card):
    """pac.c:363-367 — one entry per bit, compare 33 or 0, counter_top 32."""
    return [((33 if b else 0), 0, 0, 32) for b in pacdiff.build(card)]


def gproxii_expect(raw):
    """gproxii.c:91-123 — one entry per bit at counter_top 64; level flips every boundary and
    again mid-bit on a 1; the inversion bit (0x8000) carries which half of a 1 is high."""
    data = bytes.fromhex(raw)
    out, level = [], False
    for i in range(96):
        bit = (data[i // 8] >> (7 - (i % 8))) & 1
        level = not level
        if bit:
            ch0 = (0 if level else 0x8000) | 32
        else:
            ch0 = 65 if level else 0
        out.append((ch0, 0, 0, 64))
        if bit:
            level = not level
    return out


def cu(port, *cmds):
    """⛔⛔ THE PORT IS SELECTED WITH `hw connect -p`, NOT A `-p` FLAG ON cu.py — cu.py HAS NO SUCH
    FLAG. Passing `-p <path>` hands cu.py two unparseable COMMANDS: it prints its help for each,
    then auto-connects with a bare `hw connect`, which takes whichever Chameleon it finds first.
    That is not a crash and not an error — it is a silent wrong-device read, and it cost a whole
    finding (C459 retracted): a correctly flashed #1 was graded through #2, twice, and the flashing
    tool was blamed for reporting a success it had actually earned."""
    r = subprocess.run([PY, CU, "hw connect -p %s" % port] + list(cmds),
                       capture_output=True, text=True)
    out = r.stdout + r.stderr
    if "Chameleon Ultra connected" not in out and "connected" not in out.lower():
        return out + "\n⛔ NO CONNECT LINE — the device was not reached."
    return out


BAD = ("unrecognized", "invalid", "usage:", "error", "need exactly", "not set to")


def arm(port, typ, ec):
    """⛔ The econfig is judged on BAD WORDS, not on the word "success": several econfigs
    print their own confirmation line instead, and requiring "success" rejected a valid fdxb
    arm for a whole run (airduty.py, same week)."""
    out = cu(port, "hw slot type -s %d -t %s" % (SLOT, typ), "hw slot enable -s %d --lf" % SLOT)
    if "success" not in out:
        return False, "slot setup: " + out.strip()[-160:]
    eout = cu(port, ec % SLOT)
    if any(b in eout.lower() for b in BAD):
        return False, "econfig REFUSED: " + eout.strip()[-160:]
    out = cu(port, "hw slot change -s %d" % SLOT, "hw mode -e")
    return "success" in out, eout.strip()[-160:]


class Field:
    """Hold the Flipper in `rfid read` so the emulation is actually running while we query.

    ⚠ The Flipper's `rfid` is an external app whose loader fails transiently, and an unguarded
    caller scores that as a protocol verdict (C373, flipgrade.py). Here it cannot: a failed
    load simply means no field, the device's own `emulating` flag goes false, and the run is
    abandoned rather than graded."""

    def __init__(self, seconds):
        self.seconds = seconds
        self.out = ""
        self.t = threading.Thread(target=self._run, daemon=True)

    def _run(self):
        f = flipraw.Flip()
        try:
            self.out = f.cmd("rfid read", self.seconds, etx=True)
        except Exception as e:                      # noqa: BLE001 - reported, never swallowed
            self.out = "FIELD THREAD FAILED: %r" % (e,)
        finally:
            f.close()

    def __enter__(self):
        self.t.start()
        time.sleep(2.0)                             # let the reader come up before we query
        return self

    def __exit__(self, *a):
        self.t.join(timeout=self.seconds + 10)


def dump(port, entries):
    """Read the whole buffer as `hw emuseq --raw` and parse the header plus entries."""
    out = cu(port, "hw emuseq --count %d --raw" % entries)
    hdr, vals = {}, []
    for line in out.splitlines():
        s = line.strip()
        if ":" in s and not s[0].isdigit():
            k, _, v = s.partition(":")
            hdr[k.strip()] = v.strip()
        else:
            p = s.split()
            if len(p) == 5 and all(x.lstrip("-").isdigit() for x in p):
                vals.append(tuple(int(x) for x in p[1:]))
    return hdr, vals, out


def grade(name, expect, got, hdr):
    print("  %s: %d entries expected, %d returned, emulating %s, playbacks %s"
          % (name, len(expect), len(got), hdr.get("emulating now", "?"),
             hdr.get("playbacks started", "?")))
    if len(got) != len(expect):
        print("    ⛔ LENGTH MISMATCH — the buffer is not the one this arm should own.")
        return False
    bad = [(i, e, g) for i, (e, g) in enumerate(zip(expect, got)) if e != g]
    if not bad:
        print("    ✅ ALL %d ENTRIES MATCH THE SOURCE PREDICTION." % len(expect))
        return True
    print("    ⛔ %d of %d ENTRIES DIFFER. First 12:" % (len(bad), len(expect)))
    for i, e, g in bad[:12]:
        print("       entry %3d  expected %-22s got %s" % (i, e, g))
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("protos", nargs="*", default=["pac", "gprox"])
    ap.add_argument("--port", default="/dev/tty.usbmodemC3A1656543DE1")
    ap.add_argument("--card", default="1337BEEF",
                    help="PAC credential: eight ASCII chars (%%08lX hex is what a Flipper renders)")
    ap.add_argument("--gprox-raw", default="fac2a38c2b081af0210b12c2")
    ap.add_argument("--seconds", type=float, default=12.0)
    a = ap.parse_args()

    arms = {
        "pac":   ("PAC", "lf pac econfig -s %d --cn " + a.card, 128,
                  lambda: pac_expect(a.card)),
        "gprox": ("GProxII", "lf gproxii econfig -s %d --raw " + a.gprox_raw, 96,
                  lambda: gproxii_expect(a.gprox_raw)),
    }

    rc = 0
    for name in a.protos:
        if name not in arms:
            print("  unknown arm %r" % name)
            return 2
        typ, ec, entries, expect_fn = arms[name]
        ok, msg = arm(a.port, typ, ec)
        if not ok:
            print("  %s ARM-FAIL: %s" % (name, msg))
            rc = 2
            continue
        before = dump(a.port, 0)[0].get("playbacks started", "?")
        with Field(a.seconds):
            hdr, got, raw = dump(a.port, entries)
            after = dump(a.port, 0)[0].get("playbacks started", "?")
        moved = (before.isdigit() and after.isdigit() and int(after) > int(before))
        live = moved or hdr.get("emulating now") == "True"
        print("  %s: playbacks %s -> %s%s" % (name, before, after, "" if moved else " (no rise)"))
        if not live:
            print("  %s: ⛔ VOID — no reader field reached the device while the buffer was read, "
                  "so this is whatever the last arm left behind (M56, C454). Not graded." % name)
            print("     header: %s" % hdr)
            rc = 2
            continue
        if not grade(name, expect_fn(), got, hdr):
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
