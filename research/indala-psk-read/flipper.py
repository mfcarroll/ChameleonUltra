#!/usr/bin/env python3
"""Drive the Flipper Zero's lfrfid CLI over USB serial — rig A, both directions.

    ./flipper.py read --mode both --attempts 6
    ./flipper.py emulate Indala26 a0000000 --seconds 30

⭐ WHY THIS EXISTS. Rig A's whole value is that the Flipper reads what the Chameleon emulates
(and now emulates what the Chameleon reads) with nobody pressing a button. C81 and C83 were
both taken this way from a scratch file that was never committed and is gone. See README.md
→ The bench.

⛔ MATCH THE SUCCESS PATH, NOT THE PROTOCOL NAME (METHOD.md M28). A previous tool in this
project counted substring hits of a protocol name and reported 10 successes from 5 attempts,
because the command did not exist and the CLI's own help text contained the name twice. That
impossible number was read as a strong pass and written into two ledger claims.

Reading `lfrfid_cli.c` (Momentum) gives the only line a successful read uniquely produces:

    Reading RFID...            <- printed either way
    Press Ctrl+C to abort      <- printed either way
    Indala26 A0000000          <- ONLY on success: name, space, uppercase hex, alone
    FC: 52 Card: 63612         <- optional rendered detail
    Reading stopped            <- printed either way, so NOT a success marker

So success is that anchored `^name HEX$` line and nothing else. The protocol name also appears
in the usage banner and in the "Available protocols:" listing (tab-indented, comma-separated),
which is exactly the trap M28 describes, so both are rejected structurally.

⛔ AND: `rfid read` DOES NOT TIME OUT. It loops until a tag is decoded or the next character is
ETX (0x03) — see the `cli_is_pipe_broken_or_is_etx_next_char` loop. A failed read therefore
never returns, and anything written afterwards is consumed as ordinary input while the worker
is still running. The scratch version slept and moved on, which happens to look right whenever
the read succeeds — i.e. every arm except the null, which is the arm that matters. This sends
ETX and waits for "Reading stopped" before issuing anything else.

`rfid emulate` blocks the same way, so emulation is a hold: start it, keep the port open for
the requested seconds, then ETX.
"""

import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing — use ../../software/script/.venv/bin/python")

PORT = "/dev/tty.usbmodemflip_Matthew1"
BAUD = 115200
ETX = b"\x03"

# name, single space, an even number of uppercase hex digits, end of line. The shortest
# lfrfid payload is 3 bytes, so 4 digits is a safe floor that still excludes stray words.
# ⛔⛔ THE NAME MAY CONTAIN SPACES, AND ASSUMING IT COULD NOT COST A WRONG PUBLISHED CLAIM.
# This pattern was `[A-Za-z][A-Za-z0-9]*` — one word — which cannot match Momentum's name for
# Securakey, which is "Radio Key". A working emulation therefore reported 0 of 6, and that
# number went into FINDINGS.md as an emulation defect with an isolating control beside it
# (C177). The control was sound; the instrument was not.
#
# ⚠ This is M28 biting from the other side. The rule says match the success PATH rather than
# the protocol name — and this pattern did match the path, but encoded an assumption about
# names that no reference supports. ⇒ The name is now `[A-Za-z][A-Za-z0-9 ]*?`, non-greedy so
# the hex group stays maximal. The structural rejections still hold: the "Available
# protocols:" listing is tab-indented so it fails `^[A-Za-z]`, and no banner line ends in an
# even run of hex digits.
SUCCESS = re.compile(r"^([A-Za-z][A-Za-z0-9 ]*?) ((?:[0-9A-F]{2}){2,})$")

# The CLI printed usage or a protocol listing instead of running what we asked. That means the
# command or an argument was rejected, and every count after it would be meaningless.
REJECTED = ("Available protocols:", "rfid <write | emulate>", "Unknown protocol")


class Flipper:
    def __init__(self, port=PORT, quiet=False):
        self.s = serial.Serial(port, BAUD, timeout=0.2)
        self.quiet = quiet
        time.sleep(0.4)
        self.s.reset_input_buffer()

    def close(self):
        self.s.close()

    def _drain(self, until, deadline):
        """Collect lines until `until` is seen or the deadline passes. Returns (lines, saw)."""
        buf, lines = "", []
        while time.time() < deadline:
            chunk = self.s.read(4096).decode("utf-8", "replace")
            if chunk:
                buf += chunk
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.replace("\r", "").rstrip()
                    # The CLI echoes the command and prints a ">: " prompt; neither is output.
                    if line.startswith(">") or not line.strip():
                        continue
                    lines.append(line)
                    if until in line:
                        return lines, True
            else:
                time.sleep(0.05)
        return lines, False

    def run(self, command, settle, terminator):
        """Send a command, wait `settle` seconds for `terminator`, then ETX if it never came."""
        self.s.reset_input_buffer()
        self.s.write((command + "\r\n").encode())
        lines, done = self._drain(terminator, time.time() + settle)
        if not done:
            self.s.write(ETX)
            more, _ = self._drain(terminator, time.time() + 3.0)
            lines += more
        for line in lines:
            if any(r in line for r in REJECTED):
                raise SystemExit(
                    "⛔ the Flipper REJECTED `%s` — it printed usage, not a result.\n"
                    "   Every count from here would be a phantom (METHOD.md M28). Line: %r"
                    % (command, line))
        if not self.quiet:
            for line in lines:
                print("      | " + line)
        return lines


def decode(lines):
    """The decoded credential, or None. Only the anchored success line counts.

    Everything between that line and "Reading stopped" is the protocol's own rendered
    detail — Indala26 prints FC and Card on separate lines, so taking only the next one
    silently drops the card number, which is the half that identifies the credential.
    """
    for i, line in enumerate(lines):
        m = SUCCESS.match(line.strip())
        if m:
            detail = []
            for nxt in lines[i + 1:]:
                if "Reading stopped" in nxt or SUCCESS.match(nxt.strip()):
                    break
                detail.append(nxt.strip())
            return m.group(1), m.group(2), " ".join(d for d in detail if d)
    return None


MODES = {"psk": "indala", "ask": "normal"}


def cmd_read(a):
    f = Flipper(a.port, quiet=not a.verbose)
    modes = list(MODES) if a.mode == "both" else [a.mode]
    results = {}
    try:
        for mode in modes:
            hits, seen = 0, []
            print("  --- rfid read %s (%s) x%d" % (MODES[mode], mode.upper(), a.attempts))
            for i in range(1, a.attempts + 1):
                got = decode(f.run("rfid read " + MODES[mode], a.timeout, "Reading stopped"))
                if got:
                    hits += 1
                    seen.append("%s %s" % (got[0], got[1]))
                    print("    %2d: %s %s  %s" % (i, got[0], got[1], got[2]))
                else:
                    print("    %2d: -" % i)
            # M28: a count cannot exceed its own denominator. Refuse rather than print one.
            assert hits <= a.attempts, "impossible: %d hits in %d attempts" % (hits, a.attempts)
            results[mode] = (hits, seen)
            print("    => %s: %d/%d" % (mode.upper(), hits, a.attempts))
    finally:
        f.close()

    if len(results) == 2:
        psk, ask = results["psk"][0], results["ask"][0]
        print("\n  PSK %d/%d vs ASK %d/%d" % (psk, a.attempts, ask, a.attempts))
        if psk and not ask:
            print("  ⭐ PSK hits with the ASK arm empty — the reader was looking and found a")
            print("     PSK tag, not any tag. That ASK arm IS the control (C83).")
        elif psk and ask:
            print("  ⚠ BOTH arms hit. The ASK arm is meant to be the null; something else is")
            print("     on the pad, or the modes are not exclusive here. Do not treat the PSK")
            print("     number as bracketed until this is explained.")
        elif not psk and not ask:
            print("  ⚠ Nothing on either arm. This is a null with no positive control, which")
            print("     says the Flipper heard nothing — not that the emulator is silent (F05).")
    return 0


def cmd_emulate(a):
    f = Flipper(a.port, quiet=not a.verbose)
    try:
        cmd = "rfid emulate %s %s" % (a.protocol, a.data)
        print("  %s — holding %ds" % (cmd, a.seconds))
        f.s.reset_input_buffer()
        f.s.write((cmd + "\r\n").encode())
        lines, _ = f._drain("Emulating RFID...", time.time() + 3.0)
        for line in lines:
            if any(r in line for r in REJECTED) or "needs to be" in line:
                raise SystemExit("⛔ the Flipper REJECTED it: %r" % line)
        if not any("Emulating" in line for line in lines):
            raise SystemExit("⛔ no 'Emulating RFID...' — it never started. Saw: %r" % lines)
        print("  emulating; do the read on the other device now")
        time.sleep(a.seconds)
    finally:
        f.s.write(ETX)
        time.sleep(0.3)
        f.close()
    print("  stopped")
    return 0


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--port", default=PORT)
    p.add_argument("--verbose", action="store_true", help="echo every CLI line")
    sub = p.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("read", help="read a tag or an emulator on the Flipper's pad")
    r.add_argument("--mode", choices=["psk", "ask", "both"], default="both")
    r.add_argument("--attempts", type=int, default=6)
    r.add_argument("--timeout", type=float, default=6.0, help="seconds before ETX")
    r.set_defaults(func=cmd_read)

    e = sub.add_parser("emulate", help="emulate a tag FROM the Flipper (rig A, backwards)")
    e.add_argument("protocol", help="e.g. Indala26 (4 bytes), Idteck (8 bytes)")
    e.add_argument("data", help="hex, exactly the protocol's data size")
    e.add_argument("--seconds", type=float, default=30.0)
    e.set_defaults(func=cmd_emulate)

    a = p.parse_args()
    return a.func(a)


if __name__ == "__main__":
    sys.exit(main())
