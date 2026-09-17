#!/usr/bin/env python3
"""Is the intermittency the finite per-field-arrival BURST, out of step with the reader?

    ./burstsync.py                          # the three probe arms, 4 sessions of 12 reads each
    ./burstsync.py --arms keri --sessions 6
    ./burstsync.py --null                   # the control: nothing armed, every count must be 0

⭐⭐ THE QUESTION (`rfid-tools` QUEUE item 9, the last hands-off item on that list). Five of the
six emulate arms decode INTERMITTENTLY through the Proxmark — 1-in-9 to 1-in-3 (C491), wandering
88/60/38/75% across one evening (C497). C486's beat explains WHY a frame can be cut; it does not
explain why the rate wanders between sessions. Item 9's rival hypothesis is the emitter's own
duty cycle: `lf_tag_em.c` plays a FINITE burst per field arrival — `recompute_frames_per_burst()`
sizes it to `LF_TAG_BURST_TARGET_MS` (500 ms), clamped to [2, 255] frames — and `playbacks
started` counts ARRIVALS, not repeats (C475). If the field comes up once and stays up, the
emission dies 500 ms in and every later read of that session captures DEAD AIR.

⭐⭐⭐ WHY IT WOULD MATTER FAR BEYOND ITEM 9. `shortread.py` issues every rung of a ladder inside
ONE pm3 session. If the burst dies after the first 500 ms of that session, then C498/C499-C504
were scoring mostly-dead reads, and M60's position effect — `lf keri reader` 2/12 ascending
against 10/12 for the SAME count through the SAME demodulator — would have its mechanism.
⛔ Shuffling does not rescue that: it converts a systematic starvation into uniform dilution, so
the ladders would still be comparable with each other and still all be understated. That is
exactly the shape of a defect a shuffle hides.

## ⛔⛔ THE CRITERIA, WRITTEN AND COMMITTED BEFORE THE FIRST CAPTURE (M55)

**K4 — arithmetic, and it costs no bench time at all.** `recompute_frames_per_burst()` targets
500 ms and floors at 2 frames, so for every arm whose frame is under 250 ms — all six; the longest
is `indala224` at 57.3 ms — the burst is at least 500 ms. The longest read the matrix grades on is
`lf indala reader`'s 30,000 samples = 240 ms. ⇒ **A burst cannot run out inside a single read.**
Item 9 survives only in its between-reads form, which is what K1/K2 measure.

**K1 — arrivals per read.** r = (Δ_N − Δ_0) / N, where Δ_N is the advance of `playbacks started`
across a session of N reads and Δ_0 the advance across a session of ZERO reads (the instrument
control: whatever the client's own connect and disconnect cost).
  - r ≥ 0.8  ⇒ each read is its own field arrival, so each read gets a fresh 500 ms burst
               ⇒ **starvation REFUTED.**
  - r ≤ 0.3  ⇒ the field persists across a session ⇒ **SUPPORTED** — go to K2.
  - between  ⇒ no verdict. Report the number and say it is between.

**K2 — decode rate by POSITION in the session, and this is the one that carries the verdict.**
Every read in a session is the IDENTICAL command, so index is the only variable — M60 inverted on
purpose: there, position was the confound to remove; here it is the subject. Pooled per arm,
compare index 0-1 against index >= 4.
  - starvation real ⇒ early >= 2x late, on `gproxii` above all: it sits at ~100% through a fitted
    read (C498, 8/8), and a ceiling is the most sensitive detector there is of reads going dead.
  - **early and late within 15 points on every arm ⇒ starvation REFUTED as an explanation of the
    intermittency, whatever K1 says.** K1 measures the premise; K2 measures the effect.

**K3 — null control.** `--null`, nothing armed, the same commands on the same tagless rig: every
decode count must be 0. ⚠ The playbacks counter is not readable with no slot armed, so K3's
counter half is reported as unavailable rather than as a zero.

⛔ THE PROBE COMMAND PER ARM IS FIXED AND CHOSEN FOR POWER, not for being the graded one: an arm
at 0% cannot show a decline and an arm at 100% cannot show a rise. `gproxii` at ~100% (fitted
`-s 12288`), `indala` at ~75% (`-s 4096`), `keri` at ~38% (its own reader). Between them they can
detect a fall, a rise, or neither.

⛔⛔ THIS GRADES NOTHING. No null sweep, no calibration row, no licence — a manual observation of
the same class as C487/C488/C490. It cannot move a cell in the matrix and must never be reported
as if it had. What it can do is tell us whether an instrument artifact has been diluting every
read-length figure this round produced.
"""
import argparse
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import seqdump
import shortread

PM3 = shortread.PM3

# (arm key, probe command) — see the docstring: chosen so a fall and a rise are both detectable.
PROBES = {
    "gproxii": "lf read -s 12288",      # ~100% in C498 — the ceiling that a dead read must break
    "indala": "lf read -s 4096",        # ~75%
    "keri": None,                       # its own reader, ~38%
}
ORDER = ["gproxii", "indala", "keri"]


def playbacks(port):
    """`playbacks started` from the sequence header, or None when no slot is armed."""
    try:
        hdr = seqdump.dump(port, 0)[0]
    except Exception:
        return None
    v = hdr.get("playbacks started")
    if v is None:
        return None
    try:
        return int(v)
    except ValueError:
        return None


def session(arm_key, reads, timeout):
    """One pm3 invocation issuing `reads` IDENTICAL probes. Returns the per-read scores in order.

    ⛔ `reads == 0` is the instrument control and is not a degenerate case: the client still
    connects and disconnects, and whatever field that costs is what Δ_0 measures."""
    a = shortread.ARMS[arm_key]
    probe = PROBES[arm_key]
    cmds = []
    for _ in range(reads):
        if probe is None:
            cmds.append(a.reader)
        else:
            cmds.append(probe)
            cmds.append(a.reader.replace(" reader", " demod"))
    if not cmds:
        cmds = ["hw version"]           # connect and leave; no LF field raised
    r = subprocess.run([PM3, "-c", "; ".join(cmds)], capture_output=True, text=True,
                       timeout=timeout, start_new_session=True)
    out = r.stdout + r.stderr
    blocks, cur, label = [], [], None
    for ln in out.splitlines():
        m = re.search(r"pm3 --> (.+)$", ln)
        if m:
            if label is not None:
                blocks.append((label, "\n".join(cur)))
            label, cur = m.group(1).strip(), []
        elif label is not None:
            cur.append(ln)
    if label is not None:
        blocks.append((label, "\n".join(cur)))

    judged = a.reader if probe is None else a.reader.replace(" reader", " demod")
    scores = []
    for label, body in blocks:
        if label == judged:
            scores.append(shortread.score(body, a.marker, a.expect))
    return scores


def rate(pairs):
    """(exact hits, n) over a list of (marker, exact) scores."""
    return sum(1 for _, e in pairs if e), len(pairs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arms", default=",".join(ORDER))
    ap.add_argument("--port", default=seqdump.CU2_PORT)
    ap.add_argument("--reads", type=int, default=12, help="identical probes per session (N)")
    ap.add_argument("--sessions", type=int, default=4)
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--null", action="store_true",
                    help="⛔ K3: nothing armed. Every decode count must be 0.")
    a = ap.parse_args()

    arms = [x.strip() for x in a.arms.split(",") if x.strip()]
    bad = [x for x in arms if x not in PROBES]
    if bad:
        print("unknown arm(s): %s\nknown: %s" % (", ".join(bad), ", ".join(ORDER)))
        return 2

    print("⛔ UNGRADED — a manual observation, no null sweep and no calibration row. "
          "It moves no cell.")
    print("criteria K1/K2/K3/K4 are in this file's docstring and were committed before this run.\n")

    # K4 costs nothing and is stated whatever else happens.
    print("K4 (arithmetic, no bench): LF_TAG_BURST_TARGET_MS=500, floor 2 frames; the longest arm "
          "frame is\n     indala224 at 57.3 ms ⇒ every burst ≥ 500 ms, against a longest graded "
          "read of 240 ms\n     (lf indala reader, 30,000 samples). ⇒ a burst cannot expire "
          "INSIDE one read.\n")

    results = {}
    try:
        for key in arms:
            arm = shortread.ARMS[key]
            if a.null:
                seqdump.disarm(a.port)
            else:
                ok, why = seqdump.arm(a.port, arm.typ, arm.econfig)
                if not ok:
                    print("%-9s ⛔ ARM FAILED: %s" % (key, why))
                    continue
            probe = PROBES[key] or arm.reader
            print("%-9s probe %r, %d sessions x %d reads%s"
                  % (key, probe, a.sessions, a.reads, "  [NULL]" if a.null else ""))

            # Δ_0 — the instrument control: a session that issues no read at all.
            b0 = playbacks(a.port)
            session(key, 0, a.timeout)
            b1 = playbacks(a.port)
            d0 = None if (b0 is None or b1 is None) else b1 - b0

            per_session, by_index = [], {}
            deltas = []
            for s in range(a.sessions):
                before = playbacks(a.port)
                scores = session(key, a.reads, a.timeout)
                after = playbacks(a.port)
                d = None if (before is None or after is None) else after - before
                deltas.append(d)
                per_session.append(scores)
                for i, sc in enumerate(scores):
                    by_index.setdefault(i, []).append(sc)
                hits, n = rate(scores)
                print("   session %d: %2d/%2d exact   Δplaybacks %s"
                      % (s, hits, n, "n/a" if d is None else d))
                time.sleep(1.0)

            results[key] = {"d0": d0, "deltas": deltas, "by_index": by_index,
                            "per_session": per_session}
    finally:
        # ⛔ ALWAYS, NOT ONLY ON THE HAPPY PATH — an armed Chameleon jams the Proxmark's pad for
        # every later tick (AUTOPILOT §2a).
        out = seqdump.disarm(a.port)
        print("\ndisarm: %s" % ("ok" if "success" in out.lower() else out.strip()[-160:]))

    print("\n=== K1 — arrivals per read ===")
    for key, r in results.items():
        ds = [d for d in r["deltas"] if d is not None]
        if not ds or r["d0"] is None:
            print("%-9s counter unavailable" % key)
            continue
        per = [(d - r["d0"]) / float(a.reads) for d in ds]
        mean = sum(per) / len(per)
        verdict = ("REFUTED (each read is its own arrival)" if mean >= 0.8
                   else "SUPPORTED (the field persists)" if mean <= 0.3
                   else "between — no verdict")
        print("%-9s Δ0=%-3s Δ=%-18s r=%.2f  ⇒ %s"
              % (key, r["d0"], ",".join(str(d) for d in ds), mean, verdict))

    print("\n=== K2 — decode rate by position (the verdict) ===")
    print("%-9s %-14s %-14s %s" % ("arm", "index 0-1", "index >=4", "gap"))
    for key, r in results.items():
        early = [sc for i, v in r["by_index"].items() if i <= 1 for sc in v]
        late = [sc for i, v in r["by_index"].items() if i >= 4 for sc in v]
        eh, en = rate(early)
        lh, ln = rate(late)
        if not en or not ln:
            print("%-9s too few reads to split" % key)
            continue
        ep, lp = 100.0 * eh / en, 100.0 * lh / ln
        print("%-9s %2d/%-2d %5.1f%%   %2d/%-2d %5.1f%%   %+.1f pts"
              % (key, eh, en, ep, lh, ln, ep - lp))
    print("\n⇒ starvation is REFUTED by K2 if every gap is within 15 points; SUPPORTED if an arm's "
          "early rate is\n  at least twice its late rate. Read K1 as the premise and K2 as the "
          "effect.")
    if a.null:
        print("\nC3: this was the NULL run — every count above must be 0.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
