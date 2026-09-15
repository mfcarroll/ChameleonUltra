#!/usr/bin/env python3
"""Put ONE NAMED Chameleon into DFU mode.

    ./enterdfu.py --port /dev/tty.usbmodemC3A1656543DE1

⛔⛔ WHY THIS EXISTS. `resource/tools/enter_dfu.py` walks `list_ports.comports()` and takes the
FIRST device matching the Chameleon VID/PID — it has no way to say WHICH one. With two units on
the bench that is a coin toss on enumeration order, and the wrong one goes into DFU. C363 named
this as the one still-standing objection to flashing while both devices are attached, and C364
spent two attempts on the flash it hazarded.

⭐ It also refuses to fire if a device is ALREADY in DFU, because then the port that comes back
from a `nordicDfu` scan is ambiguous and the flash could land on either.
"""
import argparse, sys
import serial
import serial.tools.list_ports as list_ports

DFUCMD = b"\x11\xef\x03\xf2\x00\x00\x00\x00\x0b\x00"
APP_VID, APP_PID = 0x6868, 0x8686
DFU_VID, DFU_PID = 0x1915, 0x521F


def same_device(a, b):
    """/dev/cu.usbmodemX and /dev/tty.usbmodemX are the same physical device."""
    strip = lambda d: d.replace("/dev/cu.", "").replace("/dev/tty.", "")
    return strip(a) == strip(b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="the device to put into DFU, by tty path")
    a = ap.parse_args()

    ports = list(list_ports.comports())
    in_dfu = [p.device for p in ports if (p.vid, p.pid) == (DFU_VID, DFU_PID)]
    if in_dfu:
        print("⛔ A device is ALREADY in DFU (%s) — refusing, because a flash now could not "
              "tell the two apart. Power-cycle it first." % ", ".join(in_dfu))
        return 1

    # ⚠ macOS exposes each USB serial device TWICE — /dev/cu.X and /dev/tty.X — and
    # `comports()` reports only the cu. form while every note, script and cu.py invocation on
    # this bench names the tty. form. Matching on the literal string rejects the right device.
    match = [p for p in ports
             if same_device(p.device, a.port) and (p.vid, p.pid) == (APP_VID, APP_PID)]
    if not match:
        seen = ", ".join("%s (%04x:%04x)" % (p.device, p.vid or 0, p.pid or 0) for p in ports)
        print("⛔ %s is not an attached Chameleon in application mode. Attached: %s"
              % (a.port, seen))
        return 1

    others = [p.device for p in ports
              if (p.vid, p.pid) == (APP_VID, APP_PID) and not same_device(p.device, a.port)]
    print("   target : %s" % a.port)
    print("   leaving alone: %s" % (", ".join(others) or "(none)"))

    s = serial.Serial(port=a.port, baudrate=115200)
    try:
        s.dtr = 1
        s.timeout = 0
        s.write(DFUCMD)
    finally:
        s.close()
    print("   DFU command sent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
