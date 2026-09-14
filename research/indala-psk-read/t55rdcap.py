#!/usr/bin/env python3
"""Send a T5577 regular-read for one block INTO a live capture and save the raw samples.

⭐ The point of this tool is C305: a write lands only when block 0's value already matches
the tag's, and NOTHING on this bench can read block 0 to see what that value is — pm3's
downlink is broken (C306) and this firmware had no block read at all until now.

⚠ The tag answers in whatever modulation its CONFIG selects, which for block 0 is the very
thing being asked. So this returns samples, not a word, and the demodulation is a separate
step that has to be told (or made to guess) the modulation.

    ./t55rdcap.py --block 0 --out caps/t55/blk0.bin
    ./t55rdcap.py --block 0 --out caps/t55/ctrl.bin --no-read   # the control
"""
import argparse
import struct
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "software", "script"))
import chameleon_com          # noqa: E402
import chameleon_cmd          # noqa: E402
from chameleon_enum import Status  # noqa: E402

PORT = "/dev/tty.usbmodemF429364E46961"
CMD_T55_READ = 3063
CMD_PLAIN = 3060


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    p.add_argument("--block", type=int, default=0)
    p.add_argument("--samples", type=int, default=8192)
    p.add_argument("--settle", type=int, default=0)
    p.add_argument("--pwd", default=None, help="8 hex digits; sends an authenticated read")
    p.add_argument("--page1", action="store_true")
    p.add_argument("--no-read", action="store_true",
                   help="THE CONTROL: same capture, no read command sent (uses 3060)")
    p.add_argument("--port", default=PORT)
    a = p.parse_args()

    d = chameleon_com.ChameleonCom()
    d.open(a.port)
    chameleon_cmd.ChameleonCMD(d).set_device_reader_mode(True)

    blob = b""
    for chunk in range(32):
        if a.no_read:
            # ⭐ The control is the PLAIN capture command, not this one with the read
            # suppressed — a flag that skips the send would still be this code path, and
            # the whole question is whether this code path changes the air.
            payload = struct.pack(">HBBBBBB", a.samples, 7, 0, chunk, 1, a.settle, 0)
            cmd = CMD_PLAIN
        else:
            flags = (1 if a.pwd else 0) | (2 if a.page1 else 0)
            pwd = int(a.pwd, 16) if a.pwd else 0
            payload = struct.pack(">BBIHBB", a.block, flags, pwd, a.samples, chunk, a.settle)
            cmd = CMD_T55_READ
        r = d.send_cmd_sync(cmd, payload, timeout=20)
        if r.status != Status.LF_TAG_OK:
            sys.exit(f"chunk {chunk}: status 0x{r.status:02x}")
        if not r.data:
            break
        blob += r.data
    d.close()

    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    open(a.out, "wb").write(blob)
    what = "control (no read cmd)" if a.no_read else f"block {a.block}"
    print(f"  {len(blob)} bytes = {len(blob)//2} samples, {what} -> {a.out}")


if __name__ == "__main__":
    main()
