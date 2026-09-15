#!/usr/bin/env python3
"""Grade every LF EMULATE arm with the FLIPPER as the reader.

⛔⛔ WHY THIS EXISTS AND emugrade.sh DOES NOT REPLACE IT. emugrade.sh reads with Chameleon #2,
and M52 forbids that against an emulation for the SAADC family — indala, gallagher, securakey,
noralsy and gproxii need a subcarrier phase-locked to the reader's carrier, which only a real tag
has. So emugrade.sh can only legally grade the GPIO family, which is less than half the column.
The Flipper is an independent commercial reader with no such restriction: it decoded GProxII and
FDX-B byte-exact (C429/C430).

⭐ THE PASS CRITERION, FIXED HERE RATHER THAN AFTER THE RUN:
  NULL   — with #1 in reader mode it emulates nothing, so the Flipper MUST read nothing. A hit
           here is ambient and every grade below it is worthless (C328/C373).
  PASS   — the decode carries the tokens of the credential we armed. ⛔⛔ THE PROTOCOL NAME IS
           NOT A GATE. M28 says match the success PATH, not the name, and C177/C178 is why: a
           working Securakey emulation was reported as a total failure for an hour because
           Momentum calls it "Radio Key", with a space, and the matcher wanted one word. This
           grader made that exact mistake on its first run. The name is printed, never tested.
  WRONG  — something decoded, but none of our tokens: a decode of something else.
  SILENT — no decode at all.
⭐ EVERY token below is sourced from a note or the firmware, NOT from a previous run of this
script — otherwise the criterion is fitted to the output it is supposed to judge.
⚠ The full decode line is printed for every arm, so a mis-grade is visible rather than hidden
behind a verdict.
"""
import argparse, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flipraw

SLOT = 8
PY = os.path.join(HERE, "../../software/script/.venv/bin/python")
CU = os.path.join(HERE, "../../software/script/cu.py")

# protocol -> (slot type, econfig args, tokens the decode must carry)
ARMS = [
    ("em410x",   "EM410X",   "lf em 410x econfig -s %d --id DEADBEEF88",                      ["DEADBEEF88"]),
    ("hidprox",  "HIDProx",  "lf hid prox econfig -s %d -f H10301 --fc 123 --cn 4567",        ["123", "4567"]),
    ("ioprox",   "ioProx",   "lf ioprox econfig -s %d --ver 1 --fc 83 --cn 1337",             ["83", "1337"]),
    ("viking",   "Viking",   "lf viking econfig -s %d --id 1A337195",                         ["1A337195"]),
    ("jablotron", "Jablotron", "lf jablotron econfig -s %d --id 1122334455",                  ["1122334455"]),
    # ⚠ PAC's credential is EIGHT ASCII CHARACTERS, not hex: `--cn 1337BEEF`. `add_card_arg`
    # registers it as --cn but stores it in args.id, which is why reading on_exec alone
    # suggests --id.
    # ⛔⛔ THE TOKEN HERE USED TO BE `CARD0001` AND IT COULD NEVER HAVE MATCHED — a criterion
    # that cannot pass is not a criterion, exactly as a control that cannot fail is not a
    # control (M52, C441). Momentum's PAC renderer is
    #     furi_string_printf(result, "CIN: %08lX", bit_lib_get_bits_32(protocol->data, 0, 32))
    # and `protocol->data` is FOUR bytes produced by `hex_chars_to_uint8(asciiCardId, ...)` —
    # so the Flipper NEVER prints the eight ASCII characters, only their hex VALUE. Worse,
    # `CARD0001` contains an `R`, which is not a hex digit, so it cannot even round-trip.
    # ⇒ the credential must be eight UPPERCASE HEX characters, and the token is that same
    # string, which is then exactly what `CIN:` carries. `1337BEEF` -> `CIN: 1337BEEF`.
    ("pac",      "PAC",      "lf pac econfig -s %d --cn 1337BEEF",                             ["1337BEEF"]),
    # ⚠ Indala's decode is a 26-bit sub-format, not our 64-bit raw: NEXT.md records this exact
    # credential reading back as `Indala26 CD7A1D30` FC 52 / Card 63612.
    ("indala",   "Indala",   "lf indala econfig -s %d --id a0000000e6bd0e92",                 ["52", "63612"]),
    ("idteck",   "IDTECK",   "lf idteck econfig -s %d --id 4944544b55667788",                 ["4944544B55667788"]),
    ("keri",     "Keri",     "lf keri econfig -s %d --id 80003039",                            ["80003039"]),
    ("nexwatch", "NexWatch", "lf nexwatch econfig -s %d --cn 87654321 -m 2",                  ["87654321"]),
    # ⚠ Gallagher and Securakey decode to FIELDS, not to our raw frame. C173 read facility 4321
    # / card 6789 off the real tag and C178 read facility 53 / card 64169 off the original, both
    # with the Proxmark as judge — so these come from a tag, not from this script.
    ("gallagher", "Gallagher", "lf gallagher econfig -s %d --raw 7feaa31e76d86c6d868cc249",   ["4321", "6789"]),
    ("securakey", "Securakey", "lf securakey econfig -s %d --raw 7fcb400001adea5344300000",   ["53", "64169"]),
    ("noralsy",  "Noralsy",  "lf noralsy econfig -s %d --raw bb0214ff0112402233670000",       ["BB0214FF0112402233670000"]),
    ("awid",     "AWID",     "lf awid econfig -s %d --raw 011d81711dd1181111111111",          ["011D81711DD1181111111111"]),
    ("gproxii",  "GProxII",  "lf gproxii econfig -s %d --raw fac2a38c2b081af0210b12c2",       ["FAC2A38C2B081AF0210B12C2"]),
    ("fdxb",     "FDXB",     "lf fdxb econfig -s %d --raw 00339a080402079f8040797788040201",  ["999", "1337"]),
]

# What the Flipper calls each one. ⚠ Its names are NOT ours.
FLIPPER_NAME = {
    "em410x": "EM4100", "hidprox": "HIDProx", "ioprox": "IoProxXSF", "viking": "Viking",
    "jablotron": "Jablotron", "pac": "PAC/Stanley", "indala": "Indala", "idteck": "IDTECK",
    "keri": "Keri", "nexwatch": "NexWatch", "gallagher": "Gallagher", "securakey": "Securakey",
    "noralsy": "Noralsy", "awid": "AWID", "gproxii": "GProxII", "fdxb": "FDX-B",
}


def cu(*cmds):
    r = subprocess.run([PY, CU] + list(cmds), capture_output=True, text=True)
    return r.stdout + r.stderr


BAD = ("unrecognized", "invalid", "usage:", "error", "need exactly", "not set to")


def arm(emu, proto, typ, ec):
    """⛔ The econfig runs in its OWN invocation and is checked on its OWN output. The first
    version of this ran all six commands together and accepted "success" from anywhere in the
    batch — so a REJECTED econfig (pac's, which takes --id and was handed --cn) still armed a
    slot with no credential and graded SILENT, which reads as a regression."""
    out = cu("hw connect -p %s" % emu, "hw slot type -s %d -t %s" % (SLOT, typ),
             "hw slot enable -s %d --lf" % SLOT)
    if "success" not in out:
        return False, "slot setup: " + out.strip()[-120:]
    eout = cu("hw connect -p %s" % emu, ec % SLOT)
    low = eout.lower()
    if any(b in low for b in BAD):
        return False, "econfig REFUSED: " + eout.strip()[-120:]
    out = cu("hw connect -p %s" % emu, "hw slot change -s %d" % SLOT, "hw mode -e")
    return "success" in out, eout


# ⛔⛔ THE FLIPPER'S rfid COMMAND IS AN EXTERNAL APP AND ITS LOADER FAILS TRANSIENTLY. When it
# does, the CLI answers "failed to load external command" and reads NOTHING — and an unguarded
# grader scores that as a protocol verdict. It did: one run reported 15 WRONG-VALUE and 1
# ARM-FAIL, a clean sweep of failures, from an instrument that never read anything at all. That
# is C373 exactly — a silent reader and a silent emulator produce identical numbers.
LOADER_FAIL = "failed to load external command"


class InstrumentDown(Exception):
    pass


def flipper_read(seconds, tries=2):
    for attempt in range(tries):
        f = flipraw.Flip()
        try:
            out = f.cmd("rfid read", seconds, etx=True)
        finally:
            f.close()
        if LOADER_FAIL not in out:
            return out
        time.sleep(3.0)
    raise InstrumentDown(LOADER_FAIL)


def decode_line(out):
    # ⚠ "rfid read" is the Flipper ECHOING our own command back. Without it in this list the
    # NULL check trips on itself and refuses to grade anything.
    skip = ("rfid read", "Reading RFID", "Press Ctrl+C", "Reading stopped", ">:")
    return " | ".join(l.strip() for l in out.splitlines()
                      if l.strip() and not any(s in l for s in skip))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("protos", nargs="*")
    ap.add_argument("--emu", default="/dev/tty.usbmodemC3A1656543DE1")
    ap.add_argument("--seconds", type=float, default=7.0)
    a = ap.parse_args()

    arms = [x for x in ARMS if not a.protos or x[0] in a.protos]

    # ⛔ NULL FIRST — a silent reader and a silent emulator give identical numbers.
    cu("hw connect -p %s" % a.emu, "hw mode -r")
    time.sleep(1.0)
    try:
        null = decode_line(flipper_read(a.seconds))
    except InstrumentDown as e:
        print("  ⛔ INSTRUMENT DOWN before the null: %s — nothing graded." % e)
        return 2
    print("  NULL (#1 in reader mode): %s" % (null or "silent"))
    if null:
        print("  ⛔ NOT SILENT — ambient pickup. Every grade below would be worthless. Stop.")
        return 1

    rows = []
    for proto, typ, ec, tokens in arms:
        ok, out = arm(a.emu, proto, typ, ec)
        if not ok:
            rows.append((proto, "ARM-FAIL", out[:150]))
            print("  %-10s ARM-FAIL     %s" % (proto, out[:110]))
            continue
        time.sleep(1.0)
        try:
            line = decode_line(flipper_read(a.seconds))
        except InstrumentDown as e:
            print("  ⛔ INSTRUMENT DOWN at %s: %s" % (proto, e))
            print("  ⛔ ABORTING — every grade in this run is void, including the ones above.")
            return 2
        up = line.upper()
        hit = [t for t in tokens if t.upper() in up]
        named = FLIPPER_NAME[proto].upper() in up
        if not line:
            verdict = "SILENT"
        elif len(hit) == len(tokens):
            verdict = "PASS" if named else "PASS*"   # * = decoded under another name
        elif hit:
            verdict = "PARTIAL"
        else:
            verdict = "WRONG-VALUE"
        rows.append((proto, verdict, line[:150]))
        print("  %-10s %-12s %s" % (proto, verdict, line[:120]))

    print("\n  %d arms: %s" % (len(rows), ", ".join(
        "%s=%d" % (v, sum(1 for r in rows if r[1] == v))
        for v in ("PASS", "PASS*", "PARTIAL", "WRONG-VALUE", "SILENT", "ARM-FAIL")
        if any(r[1] == v for r in rows))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
