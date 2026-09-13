#!/usr/bin/env python3
"""Capture through the READER's own path and save it like `lf sniff --bits 16`.

    ./rdrcap.py --out /tmp/r.bin [--samples 14336] [--drive 7] [--phase 112]

⛔ WHY THIS IS NOT `lf sniff`. It is the same antenna, the same ADC and the same
`capture_begin`, but a DIFFERENT capture function called from a different command — and that
difference is the whole of C209: on the same tag in the same minute the reader path measures a
mean bit-boundary step of ~5070 where sniff measures 2819, and the sniff capture decodes
exactly while the reader finds nothing. Every setting the two paths differ in has been swept
without reproducing it. ⇒ The only measurement left was the reader's own samples, and until
this existed nothing could take it.

⚠ Instrumentation. It exists to answer one question and should go when that is answered.
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "software", "script"))

import chameleon_com                       # noqa: E402
import chameleon_cmd                       # noqa: E402
from chameleon_enum import Status          # noqa: E402

CMD = 3060
PORT = "/dev/tty.usbmodemF429364E46961"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    p.add_argument("--samples", type=int, default=14336)
    p.add_argument("--drive", type=int, default=7)
    p.add_argument("--phase", type=int, default=0)
    p.add_argument("--port", default=PORT)
    a = p.parse_args()

    d = chameleon_com.ChameleonCom()
    d.open(a.port)
    chameleon_cmd.ChameleonCMD(d).set_device_reader_mode(True)

    blob = b""
    for chunk in range(32):
        payload = struct.pack(">HBBB", a.samples, a.drive, a.phase, chunk)
        r = d.send_cmd_sync(CMD, payload, timeout=20)
        if r.status != Status.LF_TAG_OK:
            sys.exit(f"chunk {chunk}: status {r.status}")
        if not r.data:
            break
        blob += r.data
    d.close()

    open(a.out, "wb").write(blob)
    print(f"  {len(blob)} bytes = {len(blob)//2} samples -> {a.out}")


if __name__ == "__main__":
    main()
