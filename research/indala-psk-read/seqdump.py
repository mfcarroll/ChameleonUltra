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

⛔⛔ "IT ARMS 1 OF 5 STEPS" IS WRONG AND IS RETRACTED HERE (2026-09-16). TOOLS.md L442 and the
work queue both said this tool ran only the econfig, and that every `VOID` it printed was the
missing four steps rather than the field. `arm()` below has done all five, in order, since it was
written — compare it with `rfid-tools benchmatrix/devices.py:589`, which is the sequence it was
accused of not following. Measured on cu2 the same day: all five steps answer `success`, and an
armed slot 8 then plays back under a Proxmark field. The diagnosis was read off the wrong thing.

⛔ WHAT WAS ACTUALLY BROKEN — and it is why a cu2 dump voided: THE ONLY FIELD SOURCE WAS THE
FLIPPER, which is on Rig A with cu1. Pointing this tool at cu2 (Rig B, whose reader is the
Proxmark) held a field on the other rig, so nothing ever reached the device being dumped. The
field now comes from the rig the port belongs to, `--field` overrides it, and `--field none` is
for an operator holding one by hand.

⭐ `playbacks started` COUNTS FIELD ARRIVALS, NOT REPEATS (measured 2026-09-16). Armed cu2 under
`lf tune`: 46 -> 47 within 4s, then 47 for the remaining 25s while the field held 19V throughout.
So the guard must straddle the field COMING UP — read it with the field down, raise the field,
read it again, which is the order below. Sampling twice under a field already up sees no rise and
would void a working arm.

⛔⛔ THE GUARD IS `playbacks` RISING, NOT THE `emulating` FLAG — and that correction came from a
measurement, not from taste. Driving a Chameleon with the Proxmark's field moved `playbacks
started` from 0 to 2 while `emulating now` read False on all three samples taken during it:
playback is BURSTY, so the instantaneous flag is False most of the time even while the arm is
working perfectly. A guard on that flag would have scored a live emulation VOID and sent the
whole unit back to the air-side routes that are already exhausted.
"""
import argparse, os, signal, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flipraw
import pacdiff

PY = os.path.join(HERE, "../../software/script/.venv/bin/python")
CU = os.path.join(HERE, "../../software/script/cu.py")
PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
SLOT = 8

# ⭐ THE RIG DECIDES THE FIELD, so a `--port` alone cannot point the tool at the wrong reader.
# Rig A is Flipper-T5577-cu1; Rig B is Proxmark-cu2 and is deliberately TAGLESS.
CU1_PORT = "/dev/tty.usbmodemC3A1656543DE1"
CU2_PORT = "/dev/tty.usbmodemF429364E46961"
FIELD_FOR_PORT = {CU1_PORT: "flipper", CU2_PORT: "pm3"}

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


# ── the PSK family ───────────────────────────────────────────
# Five of the six protocols the Proxmark will not decode from our emulation are PSK, and four of
# them reach the air through ONE builder: `utils/psk1.c:lf_psk1_build_sequence`. So the prediction
# below is shared too — if it is wrong it is wrong for all of them at once, which is worth more
# than four separately-derived guesses.
PSK1_TOP = 16       # LF_PSK1_SUBCARRIER_TOP  (psk1.h:15)
PSK1_DUTY = 8       # LF_PSK1_SUBCARRIER_DUTY (psk1.h:16)
# LF_PSK1_SEQ_REPEATS = LF_PSK1_RF32_SUBCYCLES_PER_BIT - 1 (psk1.h:56). PAC and GProxII put the
# bit in `counter_top` and repeat 0 times (pac.c:301, gproxii.c:38).
PSK1_SEQ_REPEATS = 15
PAC_GPROX_REPEATS = 0


def psk1_expect(raw, bits, differential):
    """psk1.c:25-89 — ONE ENTRY PER BIT, each a 16us subcarrier period at duty 8, carrying
    only its phase in channel_0's top bit. So every entry is 0x8008 or 0x0008 at counter_top 16
    and the whole prediction is the PHASE WALK.

    DIRECT (PSK1, Indala26/IDTECK/KERI): phase flips wherever the frame CHANGES, seeded from the
    frame's LAST bit so the buffer wraps continuously — the telescoping identity
    phase[k] = bit[k] XOR bit[N-1], which is periodic whatever the parity.

    DIFFERENTIAL (PSK2, Indala224 — and ONLY Indala224 among these arms; NexWatch is
    PSK2 in the Proxmark's naming but reaches this builder as DIRECT, `nexwatch.c:49`):
    phase flips on every 1. ⛔ AND AN ODD-PARITY FRAME
    DOES NOT REPEAT AT THE FRAME PERIOD — psk1.c:58-86 appends a whole INVERTED copy, so the
    buffer is 2N entries, not N. That is not a detail: a single-copy buffer was decoded 6 of 6 as a
    confident WRONG credential (C152). ⇒ the entry COUNT is itself a prediction here, and the
    parity of the credential decides it before the device is asked.
    """
    frame = bytes.fromhex(raw)
    if len(frame) * 8 < bits:
        raise ValueError("frame %s is %d bits, short of the %d this arm transmits"
                         % (raw, len(frame) * 8, bits))

    def bit(i):
        return (frame[i // 8] >> (7 - (i % 8))) & 1

    phase = False
    last = bit(bits - 1)
    out = []
    for i in range(bits):
        cur = bit(i)
        if differential:
            if cur:
                phase = not phase
        else:
            if cur != last:
                phase = not phase
            last = cur
        out.append(((0x8000 if phase else 0) | PSK1_DUTY, 0, 0, PSK1_TOP))
    if differential and phase:
        out += [(e[0] ^ 0x8000, 0, 0, PSK1_TOP) for e in out]
    return out


def keri_frame(internal_id_hex):
    """⛔⛔ THE BLOCK FORM GOES ON THE AIR, NOT THE READER'S FRAME VIEW — `(id << 3) | 7`,
    which is what `lf keri clone` leaves in T5577 blocks 1-2 and therefore what a real tag
    transmits. Emulating `E0000000||id` instead is the same 64-bit cycle three bits along, and it
    gave Momentum a stable WRONG credential 6 times out of 6 (C160). This is the composition
    `lf keri econfig --id` performs (chameleon_cli_unit.py), reproduced here rather than read back
    off the device, so the prediction stays independent of what the device says it holds."""
    v = int(internal_id_hex, 16)
    if not v & 0x80000000:
        raise ValueError("Keri internal id %s has its top bit clear — that bit is the last "
                         "bit of the preamble, so the frame would have none" % internal_id_hex)
    return (((v << 3) | 7) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "big").hex()


def nexwatch_frame(cn, mode, magic="nexkey"):
    """⭐ THE CLI'S OWN BUILDER, IMPORTED RATHER THAN TRANSCRIBED. The frame is a scramble, a
    parity and a vendor checksum (`_nexwatch_build_frame`); a second copy here would be a second
    thing to keep in step, and the bytes under test are precisely the ones `econfig` sends. ⭐
    No rotation, unlike Keri: NexWatch's frame begins at a T5577 block boundary, so block form and
    air frame are the same bytes — which is a measured fact about a clone's block dump, not a
    default."""
    sys.path.insert(0, os.path.join(HERE, "../../software/script"))
    import chameleon_cli_unit as cli
    return cli._nexwatch_build_frame(cn, mode, cli.NEXWATCH_MAGIC[magic]).hex()


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


class Pm3Field:
    """Hold the Proxmark's 125kHz field up for Rig B, where the Proxmark IS the reader.

    ⛔ KILL THE PROCESS GROUP, NOT THE WRAPPER. `pm3` is a shell script that execs
    `client/proxmark3`; `terminate()` reaps the script and orphans the client, which then holds
    /dev/tty.usbmodemiceman1 forever and every later pm3 command dies with `serial port is
    claimed by another process`. Measured 2026-09-16 — it stranded the port mid-round.
    """

    def __init__(self, seconds):
        self.seconds = seconds
        self.p = None
        self.out = ""

    def __enter__(self):
        n = max(20, int(self.seconds * 2))
        self.p = subprocess.Popen([PM3, "-c", "lf tune -n %d --value" % n],
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  text=True, start_new_session=True)
        time.sleep(4.0)                             # the client connects, then raises the field
        return self

    def __exit__(self, *a):
        if not self.p:
            return
        try:
            os.killpg(os.getpgid(self.p.pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError):
            pass
        try:
            self.out = self.p.communicate(timeout=20)[0] or ""
        except subprocess.TimeoutExpired:
            os.killpg(os.getpgid(self.p.pid), signal.SIGKILL)
            self.out = self.p.communicate()[0] or ""


class NoField:
    """`--field none`: the operator is holding a reader themselves. The playbacks guard still
    runs, so a dump taken with no field is still refused rather than graded."""

    def __enter__(self):
        return self

    def __exit__(self, *a):
        pass


def disarm(port):
    """⛔ ALWAYS, IN A `finally`, NEVER ONLY ON THE HAPPY PATH. A Chameleon left in emulator mode
    jams the Proxmark's pad for every later read — including its reads of a tag, measured (L442).
    A tick that crashed mid-arm used to leave the rig disabled for every tick after it."""
    return cu(port, "hw mode -r")


# ⛔⛔ THE TRANSPORT CAPS ONE READ AT 256 ENTRIES — `LF_TAG_EM_SEQ_MAX_ENTRIES`
# (lf_tag_em.h:83), because the response buffer is 2060 bytes and an entry costs 8. A longer
# buffer MUST be paged with `--start` or it comes back short, and `grade` reports that as
# "⛔ LENGTH MISMATCH — the buffer is not the one this arm should own", which reads exactly
# like an emitter defect. It nearly was recorded as one: indala224's 448-entry buffer returned 256
# on the first run of that arm, and 448-vs-256 is only a transport limit once you know the number.
SEQ_DUMP_MAX_ENTRIES = 256


def _dump_window(port, start, count):
    """One `hw emuseq` call. Returns (header dict, entry list, raw text)."""
    out = cu(port, "hw emuseq --start %d --count %d --raw" % (start, count))
    hdr, vals = {}, []
    for line in out.splitlines():
        t = line.strip()
        if ":" in t and not t[0].isdigit():
            k, _, v = t.partition(":")
            hdr[k.strip()] = v.strip()
        else:
            f = t.split()
            if len(f) == 5 and all(x.lstrip("-").isdigit() for x in f):
                vals.append(tuple(int(x) for x in f[1:]))
    return hdr, vals, out


def dump(port, entries):
    """The whole buffer, paged over the 256-entry transport limit.

    ⚠ The windows are read at different instants. That is sound only because the buffer is
    static once the slot is armed — the modulator builds it at arm time and playback only
    replays it — but it is the reason the header is kept from the FIRST window: the playbacks
    counter must be sampled at one known point, not smeared across several.
    """
    if entries <= SEQ_DUMP_MAX_ENTRIES:
        return _dump_window(port, 0, entries)
    hdr, vals, raw = {}, [], ""
    start = 0
    while start < entries:
        want = min(SEQ_DUMP_MAX_ENTRIES, entries - start)
        h, v, o = _dump_window(port, start, want)
        if not hdr:
            hdr, raw = h, o
        if not v:
            break                                   # a short window: stop, let grade say so
        vals.extend(v)
        start += len(v)
    return hdr, vals, raw


def grade(name, expect, got, hdr, repeats=None):
    print("  %s: %d entries expected, %d returned, emulating %s, playbacks %s"
          % (name, len(expect), len(got), hdr.get("emulating now", "?"),
             hdr.get("playbacks started", "?")))
    # ⭐ THE DEVICE'S OWN COUNT IS A SECOND, INDEPENDENT STATEMENT OF THE BUFFER LENGTH, and
    # for a PSK2 arm it is the one that tests the odd-parity doubling: `entries in buffer` is read
    # off the live sequence, not off however many entries this tool managed to transfer. Paging a
    # buffer short would otherwise be indistinguishable from the firmware having built a short one.
    # ⭐⭐ `repeats` IS PART OF THE PREDICTION, NOT A FIELD TO PRINT. Every entry can match
    # the source and the air still be wrong, because this is what sets the BIT PERIOD: the PSK
    # builder emits one entry per BIT and leans on the peripheral to hold each for
    # LF_PSK1_RF32_SUBCYCLES_PER_BIT periods (psk1.h:35-48), so repeats must be 15. PAC and
    # GProxII carry the bit in `counter_top` instead and need 0. A wrong value here is a
    # wrong bit rate on a byte-perfect buffer — exactly the defect an entry-by-entry
    # comparison cannot see, which is why it is graded rather than displayed.
    if repeats is not None:
        saw = hdr.get("seq repeats", "")
        if not saw.isdigit():
            print("    ⚠ the device did not report `seq repeats`, so the bit period is "
                  "UNCHECKED and a byte-perfect buffer could still play at the wrong rate.")
        elif int(saw) != repeats:
            print("    ⛔ SEQ REPEATS IS %s, THE SOURCE SAYS %d — every entry can match "
                  "and the BIT PERIOD still be wrong by a factor of %.3g." %
                  (saw, repeats, (int(saw) + 1) / float(repeats + 1)))
            return False
    held = hdr.get("entries in buffer", "").split()[0] if hdr.get("entries in buffer") else ""
    if held.isdigit() and int(held) != len(expect):
        print("    ⛔ THE DEVICE HOLDS %s ENTRIES, THE SOURCE PREDICTS %d — the buffer "
              "the firmware BUILT is the wrong length, which no transfer limit can explain."
              % (held, len(expect)))
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
    # The registry's credentials (rfid-tools benchmatrix/registry.py), so a buffer dumped here and
    # a cell graded on the bench are the same arm rather than two similar ones.
    ap.add_argument("--indala-id", default="a0000000e6bd0e92")
    ap.add_argument("--idteck-id", default="4944544b55667788")
    ap.add_argument("--indala224-id",
                    default="80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5")
    ap.add_argument("--keri-id", default="80003039")
    ap.add_argument("--nexwatch-cn", type=int, default=87654321)
    ap.add_argument("--nexwatch-mode", type=int, default=2)
    ap.add_argument("--seconds", type=float, default=12.0)
    ap.add_argument("--field", choices=("auto", "pm3", "flipper", "none"), default="auto",
                    help="reader that raises the field. `auto` takes it from the port's rig: "
                         "cu1 -> flipper (Rig A), cu2 -> pm3 (Rig B).")
    a = ap.parse_args()

    field = a.field
    if field == "auto":
        field = FIELD_FOR_PORT.get(a.port)
        if field is None:
            print("⛔ --port %s is not a rig this tool knows, so it cannot pick the field for it. "
                  "Pass --field explicitly." % a.port)
            return 2
    print("port %s, field from %s" % (a.port, field))

    def field_ctx():
        if field == "pm3":
            return Pm3Field(a.seconds)
        if field == "flipper":
            return Field(a.seconds)
        return NoField()

    # ⛔ THE ENTRY COUNT IS DERIVED FROM THE PREDICTION, NEVER KEPT BY HAND. An arm whose
    # buffer legitimately doubles (an odd-parity PSK2 frame) would otherwise be read short and
    # graded a LENGTH MISMATCH against a hand-typed constant that was simply out of date.
    # Credentials are the registry's, so a dump and a graded cell are the same arm.
    arms = {
        "pac":       ("PAC", "lf pac econfig -s %d --cn " + a.card,
                      lambda: pac_expect(a.card), PAC_GPROX_REPEATS),
        "gprox":     ("GProxII", "lf gproxii econfig -s %d --raw " + a.gprox_raw,
                      lambda: gproxii_expect(a.gprox_raw), PAC_GPROX_REPEATS),
        "indala":    ("Indala", "lf indala econfig -s %d --id " + a.indala_id,
                      lambda: psk1_expect(a.indala_id, 64, False), PSK1_SEQ_REPEATS),
        "idteck":    ("IDTECK", "lf idteck econfig -s %d --id " + a.idteck_id,
                      lambda: psk1_expect(a.idteck_id, 64, False), PSK1_SEQ_REPEATS),
        "indala224": ("Indala224", "lf indala econfig -s %d --id " + a.indala224_id + " --224",
                      lambda: psk1_expect(a.indala224_id, 224, True), PSK1_SEQ_REPEATS),
        "keri":      ("Keri", "lf keri econfig -s %d --id " + a.keri_id,
                      lambda: psk1_expect(keri_frame(a.keri_id), 64, False), PSK1_SEQ_REPEATS),
        "nexwatch":  ("NexWatch", "lf nexwatch econfig -s %%d --cn %d -m %d"
                      % (a.nexwatch_cn, a.nexwatch_mode),
                      lambda: psk1_expect(nexwatch_frame(a.nexwatch_cn, a.nexwatch_mode),
                                          96, False), PSK1_SEQ_REPEATS),
    }

    # ⭐ THE REGISTRY'S KEY IS `gproxii`; THIS TOOL'S ARM WAS ALWAYS `gprox`. A cold session
    # reads the six out of QUEUE.md and types them, so accept both rather than making the operator
    # remember which file they are quoting.
    ALIASES = {"gproxii": "gprox", "indala26": "indala"}
    protos = [ALIASES.get(n, n) for n in a.protos]

    # ⛔ VALIDATE EVERY NAME BEFORE ARMING ANYTHING. A typo in the last of seven used to be
    # discovered after six arms had been run, which wastes the bench time and — worse —
    # leaves a half-finished run that reads like a crash.
    unknown = [n for n in protos if n not in arms]
    if unknown:
        print("  unknown arm(s) %s — known: %s"
              % (", ".join(repr(u) for u in unknown), " ".join(sorted(arms))))
        return 2

    rc = 0
    for name in protos:
        typ, ec, expect_fn, repeats = arms[name]
        expect = expect_fn()
        entries = len(expect)
        try:
            ok, msg = arm(a.port, typ, ec)
            if not ok:
                print("  %s ARM-FAIL: %s" % (name, msg))
                rc = 2
                continue
            # ⭐ THE ORDER IS THE GUARD: this read is taken with the field DOWN, because
            # `playbacks started` counts field ARRIVALS and not repeats (measured 2026-09-16).
            before = dump(a.port, 0)[0].get("playbacks started", "?")
            with field_ctx():
                hdr, got, raw = dump(a.port, entries)
                after = dump(a.port, 0)[0].get("playbacks started", "?")
            moved = (before.isdigit() and after.isdigit() and int(after) > int(before))
            live = moved or hdr.get("emulating now") == "True"
            print("  %s: playbacks %s -> %s%s" % (name, before, after,
                                                  "" if moved else " (no rise)"))
            if not live:
                print("  %s: ⛔ VOID — no reader field reached the device while the buffer was "
                      "read, so this is whatever the last arm left behind (M56, C454). Not "
                      "graded." % name)
                print("     field was %s; if that is the wrong rig for %s, no field could have "
                      "reached it." % (field, a.port))
                print("     header: %s" % hdr)
                rc = 2
                continue
            if not grade(name, expect, got, hdr, repeats):
                rc = 1
        finally:
            disarm(a.port)                          # ⛔ every path, including the exception one
    return rc


if __name__ == "__main__":
    sys.exit(main())
