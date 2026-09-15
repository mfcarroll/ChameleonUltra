#!/usr/bin/env python3
"""Put ONE NAMED Chameleon into DFU mode.

    ./enterdfu.py --port /dev/tty.usbmodemC3A1656543DE1
    ./enterdfu.py --port /dev/tty.usbmodemC3A1656543DE1 --program ../../firmware/objects/ultra-dfu-app.zip

⛔⛔ WHY THIS EXISTS. `resource/tools/enter_dfu.py` walks `list_ports.comports()` and takes the
FIRST device matching the Chameleon VID/PID — it has no way to say WHICH one. With two units on
the bench that is a coin toss on enumeration order, and the wrong one goes into DFU. C363 named
this as the one still-standing objection to flashing while both devices are attached, and C364
spent two attempts on the flash it hazarded.

⭐ It also refuses to fire if a device is ALREADY in DFU, because then the port that comes back
from a `nordicDfu` scan is ambiguous and the flash could land on either.

⛔⛔ `--program` EXISTS BECAUSE THE BOOTLOADER WINDOW IS SHORTER THAN THE GAP BETWEEN TWO SHELL
COMMANDS. Trigger DFU in one call and run `nrfutil device program` in the next and the device has
already fallen back to the application — `nrfutil` then emits NO events at all and exits, and the
version string afterwards is the OLD build. That is not a flashing failure and must not be
recorded as one: it is a race, and it looks exactly like the quiet-but-successful output TOOLS.md
warns about, which is how it could be read as C459 all over again. ⇒ `--program` polls for the
bootloader's VID/PID from inside this process and invokes nrfutil the moment it appears, so the
trigger and the flash are one step. It also VERIFIES: a flash is not believed unless the device
comes back and answers `hw version` with a build string, and the string is printed for comparison
rather than judged here — a version string cannot tell two builds apart when the tree was dirty
(C461), so the FUNCTIONAL check belongs to the caller.
"""
import argparse, os, subprocess, sys, time
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
    ap.add_argument("--program", metavar="ZIP",
                    help="flash this DFU zip the moment the bootloader appears, in one step "
                         "(the window is shorter than the gap between two shell commands)")
    ap.add_argument("--nrfutil", default="/Users/Shared/code/personal/rfid/.tools/bin/nrfutil")
    ap.add_argument("--wait", type=float, default=15.0,
                    help="seconds to wait for the bootloader to enumerate")
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
    if not a.program:
        return 0

    zip_path = os.path.abspath(a.program)
    if not os.path.exists(zip_path):
        print("⛔ no such firmware zip: %s" % zip_path)
        return 1

    # ⛔ Poll from INSIDE this process. `nrfutil device list` between the trigger and the program
    # is itself slow enough to miss the window.
    deadline = time.time() + a.wait
    boot = []
    while time.time() < deadline:
        boot = [p.device for p in list_ports.comports()
                if (p.vid, p.pid) == (DFU_VID, DFU_PID)]
        if boot:
            break
        time.sleep(0.05)
    if not boot:
        print("⛔ the bootloader never enumerated within %.0fs — NOTHING was flashed, and the "
              "device is back in the application. This is a trigger failure, not a flash "
              "failure." % a.wait)
        return 1
    print("   bootloader up at %s after %.1fs — programming now"
          % (", ".join(boot), a.wait - (deadline - time.time())))

    r = subprocess.run([a.nrfutil, "device", "program", "--firmware", zip_path,
                        "--traits", "nordicDfu"], capture_output=True, text=True)
    tail = (r.stdout + r.stderr).strip().splitlines()
    print("   nrfutil exit %d%s" % (r.returncode, (": " + tail[-1][:120]) if tail else ""))
    if r.returncode != 0:
        return 1

    # ⛔ Quiet output is NOT proof (TOOLS.md). Ask the DEVICE, by name, and print what it says.
    for _ in range(40):
        time.sleep(0.5)
        if any(same_device(p.device, a.port) and (p.vid, p.pid) == (APP_VID, APP_PID)
               for p in list_ports.comports()):
            break
    else:
        print("⛔ the device did not come back in application mode — do NOT leave it like this.")
        return 1

    here = os.path.dirname(os.path.abspath(__file__))
    py = os.path.join(here, "../../software/script/.venv/bin/python")
    cu = os.path.join(here, "../../software/script/cu.py")
    v = subprocess.run([py, cu, "-p", a.port, "hw version"], capture_output=True, text=True)
    line = [l for l in v.stdout.splitlines() if "Version:" in l]
    if not line:
        print("⛔ the device is back but would not answer `hw version` — verify by hand before "
              "trusting anything it says.")
        return 1
    print("   device now reports:%s" % line[0].split("Version:")[1].rstrip())
    print("   ⚠ a version string cannot tell two builds apart (C461) — verify FUNCTIONALLY.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
