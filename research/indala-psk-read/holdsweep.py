#!/usr/bin/env python3
"""Locate the knee: how long a STATIC level can this emitter hold before the air stops tracking it?

    ./holdsweep.py                 # sweep 1..9 entries, Flipper as reader
    ./holdsweep.py --max 12

⭐⭐ `hw emuhold` NOW EXISTS AND MATCHES THIS SPEC ENTRY FOR ENTRY (C468) — N=3 installs 252
entries in runs of 3 at compare 33/0, counter_top 32, and a normal re-arm puts PAC's own 128 back.
The criterion below was written BEFORE that command was built, so it cannot have been fitted to
what the device turned out to say. ⛔ THE SWEEP ITSELF IS STILL NOT IMPLEMENTED HERE: main() prints
predictions and stops. What it must do is, for each N: `hw emuhold -n N`, confirm the install count,
then capture with `pm3cap.py` and take the longest run.

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
import argparse, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

ENTRY_US = 256.0          # pac.c: counter_top 32 at the 125 kHz base clock, 8us per tick
PAC_MAX_ENTRIES = 9       # PAC's own longest static stretch: 9 bits, 2304us
SHORT_CONTROL = (1, 2)    # the run lengths every working ASK arm already emits


def predict(n):
    """The source's prediction for a run of n entries, in microseconds."""
    return n * ENTRY_US


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max", type=int, default=9, help="longest run to request, in entries")
    ap.add_argument("--port", default="/dev/tty.usbmodemC3A1656543DE1")
    ap.add_argument("--seconds", type=float, default=7.0)
    a = ap.parse_args()

    print("  ⛔ hw emuhold is not implemented yet — this tool states its criterion and stops.")
    print("  Sweep and predictions (pac.c: one entry = %.0fus):" % ENTRY_US)
    for n in range(1, a.max + 1):
        tag = ""
        if n in SHORT_CONTROL:
            tag = "   <- positive control: every working ASK arm emits this and decodes"
        if n == PAC_MAX_ENTRIES:
            tag = "   <- PAC's own longest static stretch"
        print("    N=%-3d predicted max static run %7.0f us%s" % (n, predict(n), tag))
    print("  PASS: slope 1 through the origin across the sweep.")
    print("  KNEE: tracks to K then saturates -> K*%.0fus is the limit; any K < %d explains C451."
          % (ENTRY_US, PAC_MAX_ENTRIES))
    print("  ⛔ A knee here is NOT attributed to the emitter: the Flipper's comparator is a")
    print("     suspect too (C443). Attribution needs a second, comparator-free instrument.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
