#!/usr/bin/env python3
"""Run ChameleonUltra CLI commands non-interactively.

Usage:
    cu.py "hw version"                  # single command
    cu.py "hw connect" "lf em 410x read"  # several, in one session
    echo "hw version" | cu.py -          # read commands from stdin

'hw connect' is issued automatically before your commands unless you
pass one yourself or use --no-connect.
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
    if "--no-connect" in argv:
        auto_connect = False
        argv.remove("--no-connect")

    if argv == ["-"] or not argv:
        cmds = [ln.strip() for ln in sys.stdin if ln.strip()]
    else:
        cmds = argv

    if auto_connect and not any(c.startswith("hw connect") for c in cmds):
        cmds.insert(0, "hw connect")

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
