#!/usr/bin/env python3
"""Soak the LF reader until the drive control goes inert, with the trap armed.

⭐ WHY. C148: `lf sniff --drive N` works from a fresh boot and stops having any effect
partway through a session; a reboot restores it exactly. It silently invalidated a PAC
result and cost four wrong explanations, because every symptom is downstream of a knob
that had quietly stopped working. `hw lfdebug` (C149) can now say WHICH failure it is —
but only while the device is still in the bad state, and a reboot clears it.

⇒ This hammers the device with the mix of operations the session was doing when it broke,
checking after each one, and STOPS the moment it catches it so the state survives to be
read.

    ./drivesoak.py [--rounds 6]

⚠ The detector is the RATIO drive7/drive4 measured against THIS TAG'S OWN BASELINE, and the
first version got that wrong. A fixed threshold of 0.80 fired on op 13 the moment the soak
rewrote the tag to Indala — but the drive was working perfectly: drive 4 had fallen from
3100 to 1358 because Indala is PSK1 and constant-amplitude, so its envelope signal is far
weaker and the ratio compresses toward 1 on its own. The trap said so directly, reporting
`at last start: drive 7, seq ptr ours True` with every PWM register correct.

⇒ Live ratios are per-protocol — about 0.35 on HID, 0.59 on PAC, 0.82 on Indala — so the
soak re-baselines after every tag rewrite and flags only a move away from that tag's own
figure. ⛔ A fixed threshold across changing tags measures the tag, not the fault.
"""
import argparse
import subprocess
import sys
import time

import numpy as np

CU = "/Users/Shared/code/personal/rfid/ChameleonUltra/software/script/cu.py"
PY = "/Users/Shared/code/personal/rfid/ChameleonUltra/software/script/.venv/bin/python"
PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
PORT = "/dev/tty.usbmodemF429364E46961"
# How far the ratio must climb ABOVE this tag's own baseline to count as inert. The real
# fault takes it from 0.59 to 1.00 on an unchanged tag, so 0.20 is comfortably inside that
# and well outside the run-to-run scatter, which is under 0.03.
INERT_MARGIN = 0.20


def cu(*cmds, timeout=180):
    args = [PY, CU, f"hw connect -p {PORT}"] + list(cmds)
    try:
        return subprocess.run(args, capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return "<timeout>"


def pm3(cmd, timeout=220):
    try:
        return subprocess.run([PM3, "-c", cmd], capture_output=True, text=True,
                              timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return "<timeout>"


def rms(path):
    x = np.fromfile(path, dtype=">u2").astype(float)
    if len(x) == 0:
        return None
    return float(np.sqrt(np.mean((x - x.mean()) ** 2)))


def check(tag):
    """One drive-4 and one drive-7 capture. Returns (ratio, r4, r7)."""
    cu("hw mode -r")
    out = []
    for d in (4, 7):
        p = f"/tmp/soak_d{d}.bin"
        cu(f"lf sniff --bits 16 --drive {d} --timeout 400 --out {p}")
        out.append(rms(p))
    r4, r7 = out
    if not r4 or not r7:
        return None, r4, r7
    return r7 / r4, r4, r7


# The operation mix, drawn from what the session was actually doing when it broke.
OPS = [
    ("sniff sweep 1-7",   lambda: [cu(f"lf sniff --drive {d} --timeout 300") for d in range(1, 8)]),
    ("sniff AIN0",        lambda: cu("lf sniff --input 0 --timeout 300")),
    ("sniff phases",      lambda: [cu(f"lf sniff --phase {p} --timeout 300") for p in (0, 20, 40, 60)]),
    ("sniff gain+settle", lambda: cu("lf sniff --gain 3 --timeout 300", "lf sniff --settle 20 --timeout 300")),
    ("sniff free-run",    lambda: cu("lf sniff --rate 150 --timeout 300")),
    ("mode cycle x3",     lambda: [cu("hw mode -e", "hw mode -r") for _ in range(3)]),
    ("emudebug in tag",   lambda: cu("hw mode -e", "hw emudebug", "hw mode -r")),
    ("slot change",       lambda: cu("hw slot change -s 2", "hw slot change -s 1")),
    ("slot type flip",    lambda: cu("hw slot type -s 1 -t EM410X", "hw slot type -s 1 -t Indala")),
    ("reader reads",      lambda: cu("lf pac read", "lf indala read", "lf em 410x read", "lf hid prox read")),
    ("pm3 rewrite PAC",   lambda: pm3("lf pac clone --cn CD4F5552")),
    ("pm3 rewrite HID",   lambda: pm3("lf hid clone -w H10301 --fc 118 --cn 1603")),
    ("pm3 rewrite Indala", lambda: pm3("lf indala clone -r a0000000e6bd0e92")),
    ("pm3 rewrite EM",    lambda: pm3("lf em 410x clone --id DEADBEEF88")),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=6)
    a = ap.parse_args()

    print("  baseline:", flush=True)
    baseline, r4, r7 = check("start")
    print(f"    drive4 {r4:.0f}  drive7 {r7:.0f}  ratio {baseline:.2f}   ✓ live\n", flush=True)

    n = 0
    t0 = time.time()
    for rnd in range(1, a.rounds + 1):
        for name, fn in OPS:
            n += 1
            fn()
            ratio, r4, r7 = check(name)
            mins = (time.time() - t0) / 60
            if ratio is None:
                print(f"  [{mins:5.1f}m] op {n:3d} {name:22} ⛔ capture failed", flush=True)
                continue
            # ⭐ A tag rewrite changes the signal, not the drive — re-baseline instead of
            # firing. This is the false positive the first run produced.
            if name.startswith("pm3 rewrite"):
                baseline = ratio
                print(f"  [{mins:5.1f}m] round {rnd} op {n:3d} {name:22} "
                      f"d4 {r4:6.0f}  d7 {r7:6.0f}  ratio {ratio:.2f}   (re-baselined)",
                      flush=True)
                continue
            inert = ratio > baseline + INERT_MARGIN
            flag = "" if not inert else "   ⛔⛔ CAUGHT IT"
            print(f"  [{mins:5.1f}m] round {rnd} op {n:3d} {name:22} "
                  f"d4 {r4:6.0f}  d7 {r7:6.0f}  ratio {ratio:.2f} "
                  f"(base {baseline:.2f}){flag}", flush=True)
            if inert:
                print("\n  ⛔⛔ INERT — state dump BEFORE anything else touches the device:\n",
                      flush=True)
                print(cu("hw lfdebug"), flush=True)
                print(cu("hw mode -e", "hw emudebug"), flush=True)
                print(f"  ⇒ caught after {n} operations, {mins:.1f} minutes, "
                      f"triggered by: {name}", flush=True)
                return 2
    print(f"\n  ⇒ {n} operations, {(time.time()-t0)/60:.1f} minutes, never went inert.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
