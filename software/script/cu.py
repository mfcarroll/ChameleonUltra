#!/usr/bin/env python3
"""Run ChameleonUltra CLI commands non-interactively.

Usage:
    cu.py "hw version"                       # single command
    cu.py -p /dev/tty.usbmodemXXX "hw version"   # pick the device
    cu.py "hw connect" "lf em 410x read"     # several, in one session
    echo "hw version" | cu.py -              # read commands from stdin

'hw connect' is issued automatically before your commands unless you
pass one yourself or use --no-connect. With -p/--port it becomes
'hw connect -p <port>'.

⛔⛔ `-p` WAS ADDED BECAUSE ITS ABSENCE COST A PUBLISHED FINDING (C459, retracted). Every argument
used to be treated as a COMMAND, so `cu.py -p /dev/tty.X "hw version"` printed the help twice, then
auto-connected with a bare `hw connect` and answered from WHICHEVER Chameleon enumerated first — no
crash, no error, no wrong exit code, just a confident reading from the other unit. A correctly
flashed device was graded through its neighbour twice and the flashing tool was blamed.
⇒ Unknown options are now REFUSED rather than silently reinterpreted as commands. With two devices
on the bench, a wrong-device answer is worse than no answer.
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import colorama                       # noqa: E402
import chameleon_cli_unit             # noqa: E402
from chameleon_cli_main import ChameleonCLI  # noqa: E402


def main():
    argv = sys.argv[1:]
    auto_connect = True
    port = None
    if "--no-connect" in argv:
        auto_connect = False
        argv.remove("--no-connect")
    for flag in ("-p", "--port"):
        while flag in argv:
            i = argv.index(flag)
            if i + 1 >= len(argv):
                sys.exit("cu.py: %s needs a port, e.g. -p /dev/tty.usbmodemXXX" % flag)
            port = argv[i + 1]
            del argv[i:i + 2]

    # ⛔ Refuse anything else that looks like an option. A CLI command never starts with "-",
    # so this can only be a flag we do not implement — and silently running it as a command is
    # how a wrong-device reading gets produced with no error at all (see the module docstring).
    stray = [a for a in argv if a.startswith("-") and a != "-"]
    if stray:
        sys.exit("cu.py: unknown option(s): %s\n"
                 "       commands go in quotes; the device is chosen with -p/--port."
                 % " ".join(stray))

    if argv == ["-"] or not argv:
        cmds = [ln.strip() for ln in sys.stdin if ln.strip()]
    else:
        cmds = argv

    if port is not None and any(c.startswith("hw connect") for c in cmds):
        sys.exit("cu.py: pass the port with -p OR an explicit 'hw connect ...', not both.")
    if auto_connect and not any(c.startswith("hw connect") for c in cmds):
        cmds.insert(0, "hw connect" + (" -p %s" % port if port else ""))

    colorama.init(autoreset=True)
    chameleon_cli_unit.check_tools()
    cli = ChameleonCLI()
    try:
        for c in cmds:
            cli.exec_cmd(c)
    finally:
        try:
            cli.exec_cmd("hw disconnect")
        except Exception:
            pass


if __name__ == "__main__":
    main()
