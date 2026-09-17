"""⛔ INSTRUMENT CHECK, NOT A BAND. Does `lf config --dec N` survive the probe's demodulator?

If decimation breaks the probe, the decimation knob cannot separate *fixed milliseconds* from
*fixed sample count* and the design dies here, before any ladder is built. Arms cu2, runs a
handful of probes at dec 1 and dec 2, and ALWAYS disarms and resets the config in a finally.
"""
import subprocess, sys, os
sys.path.insert(0, "/Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read")
import seqdump, shortread, burstsync

ARM = "keri"
PM3 = seqdump.PM3
arm = shortread.ARMS[ARM]
probe = burstsync.PROBES[ARM] or arm.reader
demod = arm.reader.replace(" reader", " demod") if burstsync.PROBES[ARM] else arm.reader


def run(cmds, timeout=300):
    r = subprocess.run([PM3, "-c", "; ".join(cmds)], capture_output=True, text=True,
                       timeout=timeout, start_new_session=True)
    return r.stdout + r.stderr


try:
    ok, why = seqdump.arm(seqdump.CU2_PORT, arm.typ, arm.econfig)
    print("arm: %s %s" % (ok, "" if ok else why))
    if not ok:
        raise SystemExit(1)
    for dec in (1, 2):
        # ⭐ the probe is issued at a 65 ms primer, a lead time all three arms work at (C517),
        # so a failure here is decimation and not the lead time.
        out = run(["lf config --reset", "lf config --dec %d" % dec,
                   "lf read -s 8125",                 # 65 ms at dec 1
                   "lf config --dec 1",
                   probe, demod] * 3)
        hits = out.count("KERI")
        cfg = [l.strip() for l in out.splitlines() if "decimation" in l.lower()]
        print("dec %d: %d KERI markers in 3 probes | %s" % (dec, hits, cfg[:2]))
finally:
    o = seqdump.disarm(seqdump.CU2_PORT)
    print("disarm: %s" % ("ok" if "success" in o.lower() else o.strip()[-120:]))
    print(run(["lf config --reset"]).count("reset") and "lf config reset" or "lf config reset issued")
