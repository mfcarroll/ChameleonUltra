#!/usr/bin/env python3
"""Locate the knee: how long a STATIC level can this emitter hold before the air stops tracking it?

    ./holdsweep.py                 # sweep 1..9 entries, pm3 raw buffer as reader
    ./holdsweep.py --max 12

⭐⭐ `hw emuhold` NOW EXISTS AND MATCHES THIS SPEC ENTRY FOR ENTRY (C468) — N=3 installs 252
entries in runs of 3 at compare 33/0, counter_top 32, and a normal re-arm puts PAC's own 128 back.
The criterion below was written BEFORE that command was built, so it cannot have been fitted to
what the device turned out to say. ⭐ THE SWEEP IS IMPLEMENTED BELOW.

⛔⛔ AND THE STATISTIC IS THE MODE, NOT THE MAXIMUM — DECIDED BEFORE ANY DATA WAS SEEN. The obvious
reading of "how long a level can it hold" is the LONGEST run in the capture, and that statistic is
WRONG here for a reason that has nothing to do with the emitter: the emulation plays in BURSTS
(`m_frames_per_burst`, then a pause for field detection), and across a pause the line sits static
for milliseconds. The longest run in any capture is therefore an inter-burst gap, and a sweep
scored on it would report a flat ~ms ceiling for every N and look exactly like a knee at N=1.
⇒ The buffer is pure alternating N-runs, so nearly every run inside a burst IS the run under test:
the MODE is the measurement, its SHARE says whether the peak is real, and the max is printed
alongside purely so the gap is visible rather than mistaken for a result.

⛔⛔ AND THE READER HAS CHANGED — DO NOT USE THE FLIPPER. This tool was written when rig A pointed
the Flipper at #1. The bench moved on 2026-09-15, and C465 then disqualified the Flipper for PAC
outright: it decodes nothing from a REAL PAC tag. The reader is now `pm3cap.py` against #2 on the
Proxmark's pad — raw samples, no comparator in the chain (C464), proven end to end on a known
emission (C466). ⭐ That also dissolves the attribution caveat at the bottom of this docstring: a
knee measured through a comparator-free instrument is not the instrument's comparator.

⭐ WHY A SYNTHETIC ARM IS NECESSARY, AND WHY IT IS NEW. C443 established, from source, that the
long-DC property cannot be tested with the arms that ship:
  • `jablotron` is REFUTED as a control — inverted diphase with `level = !level` at the start of
    EVERY bit, so a run can never exceed one bit period (512us) and NO credential choice changes
    that, because the boundary transition is structural rather than data-dependent.
  • `fdxb`, `gproxii` and `jablotron` all carry that same guaranteed boundary transition, and
    `em410x` toggles inside every entry. `T5577_PAC_CONFIG` is the ONLY config in `t55xx.h` built
    on `T5577_MODULATION_DIRECT`: PAC is the sole NRZ arm.
  ⇒ PAC is the only protocol here that can hold DC at all, so the property is untestable by
    choosing another protocol or another credential.
C443 concluded the control had to be a REAL T5577 PAC tag read by the Flipper — which needs the tag
on the Flipper's pad, a bench move, because the T5577 lives in the pm3 + #2 sandwich.
⭐ THAT WAS TRUE OF THE ARMS THAT EXISTED. Flashing now works (L426), so the arm can be BUILT: a
buffer of pure alternating static runs, with the run length as the swept variable. No credential, no
protocol, no bench move — rig A already points the Flipper at #1.

⭐⭐ THE PASS CRITERION, FIXED HERE AND DERIVED FROM `pac.c`, NOT FROM MEMORY:
  `pac.c:363-367` writes one entry per bit — `channel_0 = bits[i] ? (PAC_RF_PER_BIT + 1) : 0` with
  `counter_top = PAC_RF_PER_BIT`, and `PAC_RF_PER_BIT` is 32. At the 125 kHz base clock one tick is
  one 8us carrier cycle, so one entry is 256us and a run of N identical bits is a level held for
  exactly N * 256us. `hw emuhold --entries N` must build that shape and nothing else: alternating
  runs of N entries at compare 33 and N entries at compare 0, all at counter_top 32.
  ⇒ PREDICTION: measured max static run = N * 256us, a straight line of SLOPE 1 THROUGH THE ORIGIN.
  ⇒ HEALTHY: the line holds across the whole sweep.
  ⇒ KNEE: measurements track to some K and then saturate. K * 256us is then the longest level this
    path can hold. PAC's own frame needs up to 9 entries (2304us), so any K < 9 is a mechanism for
    C451's refusal: the duty pinned at 87.6% while predictions spanned 40.6-56.2%.

⭐ THE POSITIVE CONTROL IS BUILT IN AND CAN FAIL. N = 1 and N = 2 are the run lengths every WORKING
ASK arm on this bench already emits — em410x, viking, gallagher, noralsy, jablotron, gproxii, fdxb
all live there and all decode. If N = 1 or N = 2 does not track, the sweep is measuring the harness
and not the emitter, and the run is void. ⛔ A sweep that only ever reported long-run failure would
have no way to tell those apart, which is why the short end is included rather than assumed.

⛔⛔ WHAT THIS CANNOT SETTLE, STATED BEFORE IT RUNS. A knee locates a limit; it does NOT say whose.
The Flipper's comparator is itself suspected of holding high through long static and merging what
follows (C443, NEXT.md), and C452 measured its bias as PROPORTIONAL to the bit period rather than a
fixed time. So:
  • A proportional bias cannot manufacture a knee — it rescales a straight line and leaves it
    straight — so the LOCATION of a knee survives it. That is why this measures a knee rather than
    an absolute duration.
  • But a knee in THIS instrument is not yet a knee in the EMITTER. Attribution still needs a second
    instrument that does not use a comparator: either C443's real-T5577-on-the-Flipper's-pad control,
    or our own SAADC amplitude path (`rdrcap.py`), both of which need hands on the bench.
  ⇒ Report the knee and the ambiguity together. ⛔ Do not write "the emitter cannot hold DC" from
    this tool alone; C459 is what publishing a one-instrument conclusion costs.

⭐ THE FIRMWARE COMMAND THIS NEEDS — `DATA_CMD_LF_EMU_SEQHOLD`, gated by LF_RESEARCH_CMDS_ENABLED:
  request : uint16 entries_per_run (1..64)
  action  : fill a static wave-form buffer with alternating runs — `entries_per_run` at
            channel_0 = 33, then `entries_per_run` at channel_0 = 0 — every entry counter_top 32,
            channel_1 = channel_2 = 0 (pac.c writes neither, and jablotron does not either), sized
            to a whole number of run pairs, and point the emulation at it.
  response: the entry count actually installed, so the host can check it against 2 * N * pairs.
  ⚠ It must be reversible: re-arming any slot through the normal path must restore that protocol's
    own modulator, so a tick can never leave the device emitting a synthetic buffer.
  ⭐ Verify the buffer with `hw emuseq --raw` BEFORE trusting any air measurement — that command
    already reads the live buffer through the same `m_pwm_seq` pointer playback is handed (C462), so
    the shape under test is confirmed at the source rather than assumed.
"""
import argparse, os, re, subprocess, sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

ENTRY_US = 256.0          # pac.c: counter_top 32 at the 125 kHz base clock, 8us per tick
PAC_MAX_ENTRIES = 9       # PAC's own longest static stretch: 9 bits, 2304us
SHORT_CONTROL = (1, 2)    # the run lengths every working ASK arm already emits
HOLD_BUFFER_ENTRIES = 256 # lf_tag_em.h: LF_TAG_EM_HOLD_MAX_ENTRIES


def predict(n):
    """The source's prediction for a run of n entries, in microseconds."""
    return n * ENTRY_US


def sweep_one(port, n, samples, py, cu):
    """Install a run of n entries, verify the install, then measure the air."""
    import pm3cap

    want_pairs = HOLD_BUFFER_ENTRIES // (2 * n)
    want_entries = want_pairs * 2 * n

    out = subprocess.run([py, cu, "-p", port, "hw emuhold -n %d" % n],
                         capture_output=True, text=True).stdout
    m = re.search(r"entries installed:\s*(\d+)", out)
    if not m:
        return {"n": n, "error": "emuhold refused or unparsed: %s" % out.strip()[-160:]}
    got = int(m.group(1))
    if got != want_entries:
        return {"n": n, "error": "installed %d entries, spec says %d" % (got, want_entries)}

    vals, err = pm3cap.capture(samples)
    if vals is None:
        return {"n": n, "error": "no pm3 trace: %s" % err}
    rl, dropped, ptp = pm3cap.runs(vals)
    us = [r * pm3cap.US_PER_SAMPLE for r in rl]
    if not us:
        return {"n": n, "error": "no run structure — nothing modulating"}
    hist = Counter(us)
    mode, count = hist.most_common(1)[0]
    return {"n": n, "entries": got, "mode": mode, "share": count / len(us),
            "runs": len(us), "max": max(us), "ptp": ptp, "predicted": predict(n)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max", type=int, default=9, help="longest run to request, in entries")
    ap.add_argument("--port", default="/dev/tty.usbmodemF429364E46961",
                    help="the Chameleon on the PROXMARK's pad — #2 (the bench moved 2026-09-15)")
    ap.add_argument("--samples", type=int, default=40000)
    ap.add_argument("--predict-only", action="store_true")
    a = ap.parse_args()

    print("  Sweep and predictions (pac.c: one entry = %.0fus):" % ENTRY_US)
    for n in range(1, a.max + 1):
        tag = ""
        if n in SHORT_CONTROL:
            tag = "   <- positive control: every working ASK arm emits this and decodes"
        if n == PAC_MAX_ENTRIES:
            tag = "   <- PAC's own longest static stretch"
        print("    N=%-3d predicted modal run %7.0f us%s" % (n, predict(n), tag))
    print("  PASS: modal run = N * %.0fus across the sweep, slope 1 through the origin." % ENTRY_US)
    print("  KNEE: tracks to K then saturates -> K*%.0fus is the limit; any K < %d explains C451."
          % (ENTRY_US, PAC_MAX_ENTRIES))
    if a.predict_only:
        return 0

    py = os.path.join(HERE, "../../software/script/.venv/bin/python")
    cu = os.path.join(HERE, "../../software/script/cu.py")

    # ⛔ emuhold borrows the armed protocol's clock and is refused with nothing armed. PAC is the
    # right arm to borrow from: it is the 125 kHz type whose idiom the buffer imitates.
    subprocess.run([py, cu, "-p", a.port, "hw slot type -s 8 -t PAC", "hw slot enable -s 8 --lf",
                    "hw slot change -s 8", "hw mode -e"], capture_output=True, text=True)

    print("\n  measured (reader: pm3 raw sample buffer, no comparator in the chain — C464/C466):")
    rows = []
    try:
        for n in range(1, a.max + 1):
            r = sweep_one(a.port, n, a.samples, py, cu)
            rows.append(r)
            if "error" in r:
                print("    N=%-3d ⛔ %s" % (n, r["error"]))
                continue
            hit = abs(r["mode"] - r["predicted"]) <= pm3cap_tolerance()
            print("    N=%-3d predicted %7.0f   modal run %7.0f us (%4.1f%% of %d runs)   "
                  "max %8.0f   %s"
                  % (n, r["predicted"], r["mode"], 100 * r["share"], r["runs"], r["max"],
                     "✓" if hit else "✗ MISS"))
    finally:
        # ⛔ Never leave a device emitting the synthetic buffer: re-arm through the normal path
        # (which restores PAC's own modulator) and drop back to reader mode.
        subprocess.run([py, cu, "-p", a.port, "hw slot type -s 8 -t PAC", "hw slot change -s 8",
                        "hw mode -e"], capture_output=True, text=True)
        subprocess.run([py, cu, "-p", a.port, "hw mode -r"], capture_output=True, text=True)
        print("  (#2 re-armed through the normal path and returned to reader mode)")

    good = [r for r in rows if "error" not in r]
    ctrl = [r for r in good if r["n"] in SHORT_CONTROL]
    if len(ctrl) < len(SHORT_CONTROL) or not all(
            abs(r["mode"] - r["predicted"]) <= pm3cap_tolerance() for r in ctrl):
        print("  ⛔ THE POSITIVE CONTROL FAILED — N=1/N=2 are what every working ASK arm emits, so")
        print("     this run is measuring the harness and not the emitter. VOID (see the docstring).")
        return 1
    tracking = [r["n"] for r in good if abs(r["mode"] - r["predicted"]) <= pm3cap_tolerance()]
    k = max(tracking) if tracking else 0
    if k >= a.max:
        print("  ⇒ SLOPE 1 HOLDS to N=%d (%.0fus). No knee in this range." % (k, predict(k)))
    else:
        print("  ⇒ KNEE at K=%d: tracks to %.0fus and saturates. PAC needs %d entries (%.0fus)."
              % (k, predict(k), PAC_MAX_ENTRIES, predict(PAC_MAX_ENTRIES)))
    print("  ⭐ This reader has no comparator (C464/C466), so a knee here is NOT the instrument's")
    print("     — which is the caveat holdsweep.py was written with and the bench move dissolved.")
    print("  ⛔ It still LOCATES a limit rather than attributing it to a stage; do not write")
    print("     \"the emitter cannot hold DC\" from this alone.")
    return 0


def pm3cap_tolerance():
    """One pm3 sample. A run is an integer number of samples, so this is the finest the
    instrument can resolve — not a fitted fudge factor."""
    import pm3cap
    return pm3cap.US_PER_SAMPLE


if __name__ == "__main__":
    sys.exit(main())
