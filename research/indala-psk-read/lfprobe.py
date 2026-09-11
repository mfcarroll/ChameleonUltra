#!/usr/bin/env python3
"""Is an LF problem RF, or firmware? Measure the tag's SIGNAL, not whether it decoded.

    ./lfprobe.py                      # 6 idle, 6 under load, 10 recovering
    ./lfprobe.py --band 10000 18000   # a different tag's subcarriers
    ./lfprobe.py --read "lf em 410x read" --band 3000 20000

⭐ WHY THIS EXISTS. A read either works or it does not, and that is a terrible instrument.
`lf hid prox read` on this bench swings 6-11 out of 15 with NOTHING changing, and it
conflates two completely different failures: the tag was not heard, and the tag was heard
but the decoder did not lock. A binary metric that noisy will produce a convincing
before/after by accident — which it did, and a regression was reported off it (M17).

A tag's subcarrier sits well inside the LF chain's passband (poles at 58.8 and 33.9kHz), so
its amplitude is directly visible in a raw capture and is a CONTINUOUS measurement. One
capture, one number, and the two failures separate:

    amplitude drops under load, recovers   -> RF: detuning, thermal, coupling, field
    amplitude FLAT while reads fail        -> firmware: the decoder or the capture path

⚠ Measured 2026-09-11 against exactly the episode that prompted it — `lf hid prox read`
sitting at 0/15 for a quarter of an hour — the answer was unambiguous:

    idle     884385 - 888402      carrier DC 5439-5442
    LOADED   887963 - 891183      carrier DC 5433-5444     ratio 1.00x
    reads failed 4 of 22, including while idle

⇒ The RF path does not degrade under heavy LF load, at all. Whatever that episode was, it
was not the antenna, the field, the coupling or thermal drift — this excludes all four in
about thirty seconds, which is why it is worth having.

Default band is HID Prox: FSK at fc/8 = 15.6kHz and fc/10 = 12.5kHz.
⚠ NOT usable for Indala as-is. Its subcarrier is at fc/2 = 62.5kHz, exactly Nyquist for the
carrier-locked sampler, where amplitude depends on the sample phase — a null there means
the phase, not the path (`METHOD.md` M8, and lf_indala_psk.h).
"""
import argparse
import os
import subprocess
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.normpath(os.path.join(HERE, "..", "..", "software", "script"))
PY = os.path.join(SCRIPT, ".venv", "bin", "python")
TMP = "/tmp/lfprobe_cap.bin"
FS = 125000.0


def cu(*cmds):
    return subprocess.run([PY, "cu.py", *cmds], capture_output=True, text=True,
                          cwd=SCRIPT).stdout


def probe(lo, hi):
    """One 300ms capture -> (subcarrier band amplitude, carrier DC)."""
    cu(f"lf sniff --timeout 300 --bits 16 --out {TMP}")
    r = np.frombuffer(open(TMP, "rb").read(), dtype=np.uint8)
    if len(r) < 4 or len(r) % 2 or r[0::2].max() > 0x3F:
        raise SystemExit("⛔ capture is not 16-bit — refusing to guess at its format")
    x = ((r[0::2].astype(np.uint16) << 8) | r[1::2]).astype(float)
    y = x - x.mean()
    S = np.abs(np.fft.rfft(y * np.hanning(len(y))))
    f = np.fft.rfftfreq(len(y), 1 / FS)
    m = (f > lo) & (f < hi)
    return float(np.sqrt((S[m] ** 2).sum())), float(x.mean())


def monitor(lo, hi, floor, n):
    """⭐ LIVE POSITIONING AID. Print the subcarrier against the empty-field floor, once a
    second, so a tag can be slid around until it peaks.

    ⚠ This exists because a tag that reads perfectly on a Proxmark can be COMPLETELY
    INAUDIBLE to a Chameleon a few millimetres out of place. Measured: a T5577 carrying
    a0000000e6bd0e92, written and verified by a Proxmark which then read it back, sat at
    0.99x the empty-field floor across twelve sample phases on the Chameleon — no signal at
    all — where the bench tag sits at 1.25-1.73x. And 1.25-1.73x IS the working range, so
    there is almost no margin to give away to position."""
    if floor:
        print(f"  empty-field floor {floor:.0f}. Slide the tag; 1.3x or better is a read.\n")
    best = 0.0
    for _ in range(n):
        amp, dc = probe(lo, hi)
        if not floor:
            print(f"  {amp:11.0f}  (no floor for this band)  DC {dc:6.0f}", flush=True)
            continue
        r = amp / floor
        best = max(best, r)
        bar = "#" * min(60, int((r - 0.8) * 80)) if r > 0.8 else ""
        print(f"  {amp:8.0f}  {r:5.2f}x  best {best:5.2f}x  DC {dc:6.0f}  {bar}", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--band", nargs=2, type=float, default=[10000.0, 18000.0],
                    help="subcarrier band in Hz (default: HID Prox fc/8 and fc/10)")
    ap.add_argument("--read", default="lf hid prox read",
                    help="the read command to score alongside the amplitude")
    ap.add_argument("--hit", default="HIDProx", help="substring marking a successful read")
    ap.add_argument("--load-hit", default="Indala",
                    help="substring marking the LOAD command finding a tag. The load arm is "
                         "a loud-signal null for the other protocol's reader, so it is "
                         "scored rather than discarded")
    ap.add_argument("--load", default="lf indala read",
                    help="the command used as the load")
    ap.add_argument("--idle", type=int, default=6)
    ap.add_argument("--loaded", type=int, default=6)
    ap.add_argument("--recover", type=int, default=10)
    ap.add_argument("--monitor", type=int, metavar="N",
                    help="live positioning: print the subcarrier N times, once a second")
    # ⛔ THE FLOOR IS PER-BAND AND THE DEFAULTS USED TO CONTRADICT EACH OTHER. --band
    # defaults to HID Prox (10-18kHz) while --floor defaulted to 6900, which is the fc/2
    # (62.5kHz) empty level. Dividing one by the other produced a ratio near 900x that
    # meant nothing, and it read like an overwhelming signal. There is no single empty
    # floor: it is a property of the band, so it must be measured in the band being used.
    # ⇒ No default. Measure it with the antenna clear, in the same band:
    #       ./lfprobe.py --band 10000 18000 --monitor 3 --floor 1
    #   then pass the number it prints as --floor.
    ap.add_argument("--floor", type=float, default=None,
                    help="empty-field level MEASURED IN THE SAME --band. No default: the "
                         "floor is band-specific. fc/2 (62.5kHz) is 6350-7900 across "
                         "phases; the 10-18kHz HID band has never been measured empty")
    a = ap.parse_args()
    lo, hi = a.band

    if a.monitor:
        if a.floor is None:
            print("  ⚠ no --floor given, printing raw band energy only. A ratio needs an\n"
                  "    empty-field level measured in this same band (see --floor).\n")
        monitor(lo, hi, a.floor, a.monitor)
        return

    print(__doc__.split("Default band is")[0])
    print(f"  band {lo:.0f}-{hi:.0f} Hz   load: {a.load!r}   read: {a.read!r}\n")
    print(f"  {'t(s)':>6} {'phase':<10} {'subcarrier':>11} {'carrier':>8}  read")
    t0 = time.time()
    rows = {}

    def sample(phase):
        amp, dc = probe(lo, hi)
        ok = a.hit in cu(a.read)
        print(f"  {time.time()-t0:6.0f} {phase:<10} {amp:11.0f} {dc:8.0f}  "
              f"{'ok' if ok else '--'}", flush=True)
        rows.setdefault(phase, []).append((amp, ok))
        return amp

    for _ in range(a.idle):
        sample("idle")
    # ⭐ SCORE THE LOAD COMMAND TOO. The load arm runs the OTHER protocol's reader against
    # this tag — which is a loud-signal null for that reader, and it was being thrown away.
    # 32 Indala reads against a 95x HID tag went unrecorded before this was added.
    load_hits, load_runs = 0, 0
    for _ in range(a.loaded):
        out = cu(*([a.load] * 4))
        load_runs += 4
        load_hits += sum(1 for ln in out.splitlines() if a.load_hit in ln)
        sample("LOADED")
    for _ in range(a.recover):
        sample("recover")

    print()
    base = float(np.median([r[0] for r in rows["idle"]]))
    for phase, rs in rows.items():
        amps = [r[0] for r in rs]
        hits = sum(r[1] for r in rs)
        print(f"  {phase:<10} median {np.median(amps):9.0f}  "
              f"{np.median(amps)/base:5.2f}x idle   reads {hits}/{len(rs)}")
    load = float(np.median([r[0] for r in rows["LOADED"]]))
    print(f"\n  ⇒ {'RF DEGRADES under load' if abs(load/base - 1) > 0.1 else 'the RF path is FLAT under load — a failing read is the decoder, not the signal'}")
    # The load arm doubles as a loud-signal null: the OTHER protocol's reader, run against
    # a tag that is loudly present and of the wrong type. It must find nothing.
    if load_runs:
        print(f"  ⇒ loud-signal null: {a.load!r} found a tag {load_hits}/{load_runs} times "
              f"against this tag" + ("  ⛔ FALSE POSITIVE" if load_hits else "  ✓ clean"))


if __name__ == "__main__":
    main()
