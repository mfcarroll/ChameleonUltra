#!/usr/bin/env python3
"""Is the clock offset STABLE? — the measurement that decides whether a trim is worth building.

    ./clockdrift.py                        # indala armed, ~25 min, a reading every 90 s
    ./clockdrift.py --minutes 40 --interval 120

⭐⭐ WHY THIS AND NOT "CAN THE PWM BE SLAVED TO THE FIELD". The operator's standing architecture
question was whether the PWM clock can be locked to the received carrier. C489 answers that and
the answer is no — `VD1` rectifies at the coil so no pin can count a received carrier cycle, and
the nRF52 PWM has no external clock input to drive with one if it could. ⛔ **But PSK does not
need phase LOCK. It needs the phase to STAY PUT across one frame.** C486 measured 70-80 degrees
of rotation across a 16.4 ms Indala frame, which is ~12 Hz, ~190 ppm of the 62.5 kHz subcarrier.
Hold the rotation under ~10 degrees and the data survives; that is an offset under **~1.7 Hz, or
~27 ppm**. ⇒ **The real question is not lock, it is TRIM** — and a trim is only worth building if
the offset it corrects is the same tomorrow as today.

⭐ The clock source is already the good one. `lf_tag_em.c:250` holds HFXO precisely so the PWM
runs at **±40 ppm** rather than HFINT's ±1.5%, and that was done for the NRZ readers. So the
residual this measures is a real crystal pair at spec, not a misconfiguration — and ±40 ppm on our
side alone is already above the ~27 ppm the frame needs. **Two crystals both inside spec do not
reliably get there**, which is exactly why a per-pair trim is the thing to evaluate.

## ⛔ THE CRITERIA, WRITTEN AND COMMITTED BEFORE THE FIRST CAPTURE (M55)

**J1 — granularity, from source, and it costs no bench time.** `lf_tag_em.c:236` sets
`cfg.base_clock` to `NRF_PWM_CLK_1MHz`; the finest the peripheral offers is 16 MHz. A 62.5 kHz
half-cycle is 8 us — **8 ticks at 1 MHz, 128 at 16 MHz** — so one tick of `counter_top` is
**125,000 ppm** at the clock in use and **7,800 ppm** at the fastest available. The correction
wanted is a few tens of ppm. ⇒ **No integer `counter_top` can express it, at any prescaler, and it
is not close: the finest available step overshoots by ~300x.** A trim would therefore have to be
FRACTIONAL — dithered across entries so the AVERAGE period lands right. ⭐ The peripheral can do
that in principle (wave-form mode gives every entry its own `counter_top`, `lf_tag_em.c:540`)
⚠ but the PSK arms store one entry per BIT with `repeats` 15, so dithering inside a bit means
expanding the buffer 16x — **which is the 2,048-entry shape that was built, flashed and reverted
in C485.** ⛔ That cost is stated here so nobody discovers it after building.

**J2 — stability, and this is what the bench is for.** cu2 armed ONCE, the offset sampled
repeatedly over >= 25 minutes.
  - **STABLE**   ⇒ `(max - min) <= 30% of the mean`, over at least 10 accepted readings. A fixed
                   per-pair trim is then a meaningful object and the question goes to the operator
                   as a cost/benefit rather than a physics one.
  - **UNSTABLE** ⇒ `(max - min) >= 100% of the mean`. Nothing static can track it, and C489 says
                   there is nothing to close a loop against ⇒ **the honest outcome is a documented
                   hardware limitation**, which AUTOPILOT §2a already says is worth more than a fix
                   that keeps not arriving.
  - between      ⇒ report the numbers and record NO VERDICT.

**J3 — the estimator must not be the thing drifting.** Three guards, all of which can fail:
  - only readings passing `clockoffset.py`'s own validated `peak/median >= 6` gate are counted,
    and **the number refused is reported**. A run where most readings are refused is not a result.
  - the FFT bin width in ppm is reported beside the spread: a spread smaller than the bin is not
    a measurement of stability, it is the estimator's resolution floor.
  - a reading at the search band's edge (2 Hz or 400 Hz) is refused by name.

⛔⛔ **WHAT THE ESTIMATOR CANNOT DO, STATED BECAUSE IT BOUNDS THE CONCLUSION.** `offset_ppm`
squares the baseband to kill the data, and the squaring **destroys the sign**. So this measures
the MAGNITUDE of the offset only and cannot see a sign flip — a pair drifting through true zero
would read as a dip to zero and back, not as a reversal. Any verdict here is about magnitude.

⛔ UNGRADED. No null sweep, no calibration row, no licence; it moves no cell and is not a bench
verdict about any protocol. It is a measurement of two oscillators.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import clockoffset
import seqdump
import shortread


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arm", default="indala")
    ap.add_argument("--port", default=seqdump.CU2_PORT)
    ap.add_argument("--minutes", type=float, default=25.0)
    ap.add_argument("--interval", type=float, default=90.0)
    ap.add_argument("--min", type=float, default=10.0)
    a = ap.parse_args()

    arm = shortread.ARMS[a.arm]
    print("⛔ UNGRADED — two oscillators, not a protocol verdict. Criteria J1/J2/J3 are in this "
          "file's docstring and were committed before this run.\n")
    print("J1 (source, no bench): PWM base clock is NRF_PWM_CLK_1MHz (lf_tag_em.c:236); one "
          "counter_top\n     tick is 125,000 ppm there and 7,800 ppm at the fastest prescaler "
          "available, against a\n     wanted correction of tens of ppm. ⇒ no INTEGER trim exists; "
          "only a dither could work,\n     and for the PSK arms that means C485's 16x buffer "
          "expansion.\n")

    ok, why = seqdump.arm(a.port, arm.typ, arm.econfig)
    if not ok:
        print("⛔ ARM FAILED: %s" % why)
        return 2
    rows, refused = [], 0
    t_end = time.time() + a.minutes * 60.0
    try:
        i = 0
        while time.time() < t_end:
            i += 1
            v, amp = clockoffset.capture(min_amp=a.min)
            if v is None:
                print("  %2d  ⛔ no trace — is the Proxmark free?" % i)
                refused += 1
                continue
            ppm, hz, snr, binppm = clockoffset.offset_ppm(v)
            edge = hz <= 2.5 or hz >= 399.0
            if amp < a.min or snr < 6.0 or edge:
                refused += 1
                print("  %2d  t=%5.1f min  fc/2 %5.2f  ⛔ refused (%s)"
                      % (i, (time.time() - (t_end - a.minutes * 60.0)) / 60.0, amp,
                         "band edge" if edge else
                         "empty field" if amp < a.min else "peak/median %.1f" % snr))
            else:
                t = (time.time() - (t_end - a.minutes * 60.0)) / 60.0
                rows.append((t, ppm, hz, snr, binppm))
                print("  %2d  t=%5.1f min  fc/2 %5.2f  |%7.1f| ppm  (%6.2f Hz, p/m %5.1f, "
                      "bin %.1f ppm)" % (i, t, amp, ppm, hz, snr, binppm))
            time.sleep(max(0.0, a.interval - 8.0))
    finally:
        # ⛔ ALWAYS, NOT ONLY ON THE HAPPY PATH (AUTOPILOT §2a).
        out = seqdump.disarm(a.port)
        print("\ndisarm: %s" % ("ok" if "success" in out.lower() else out.strip()[-160:]))

    print("\n=== J2 — stability ===")
    print("accepted %d, refused %d" % (len(rows), refused))
    if len(rows) < 10:
        print("⛔ fewer than 10 accepted readings — J2 as written gives NO VERDICT, and a run "
              "where\n   most readings are refused is not a result. Report the refusals, not a "
              "stability claim.")
        return 0
    vals = [r[1] for r in rows]
    lo, hi = min(vals), max(vals)
    mean = sum(vals) / len(vals)
    spread = hi - lo
    frac = spread / mean if mean else float("inf")
    binppm = max(r[4] for r in rows)
    print("min %.1f  max %.1f  mean %.1f  spread %.1f ppm = %.0f%% of the mean"
          % (lo, hi, mean, spread, 100 * frac))
    print("largest FFT bin over the run: %.1f ppm" % binppm)
    if spread < binppm:
        print("⛔ J3: the spread is SMALLER THAN THE ESTIMATOR'S BIN — that is its resolution "
              "floor,\n   not a measurement of stability. NO VERDICT.")
    elif frac <= 0.30:
        print("⇒ **STABLE** by J2. A fixed per-pair trim is a meaningful object; whether it is "
              "worth\n   building is the operator's cost/benefit call, and J1 says the cost is "
              "C485's 16x buffer.")
    elif frac >= 1.00:
        print("⇒ **UNSTABLE** by J2. Nothing static can track it and C489 leaves nothing to close "
              "a loop\n   against ⇒ the honest outcome is a documented hardware limitation.")
    else:
        print("⇒ between J2's two bands — NO VERDICT. Report the numbers as they are.")
    print("\n⛔ Magnitude only: offset_ppm squares the baseband, which destroys the sign, so a "
          "drift\n   through true zero would read as a dip and not as a reversal.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
