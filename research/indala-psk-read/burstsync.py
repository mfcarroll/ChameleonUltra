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
decode count must be 0. ⭐ **Measured 2026-09-16 and it did BETTER than this criterion asked**:
the counter is readable while disarmed and it does not move, so the null pins BOTH halves — 0 of 12
decodes and Δ = 0 on all three arms. The prediction written here was that the counter would be
unavailable; it is not, and the run is the stronger for it.

## ⭐⭐ K5 — THE REPLICATION OF K2's SURPRISE, WRITTEN BEFORE ITS CAPTURE (added after run 1)

K2 refuted starvation and did it **in the opposite direction**: on the first armed run (n=5
sessions x 12 reads) `indala` decoded **2/10 at index 0-1 against 24/40 at index >= 4** — late is
three times better, where starvation predicts early better. `gproxii` agreed in sign (-12.5 pts)
and `keri` did not (+15.0 pts, 3/10 against 6/40, comfortably inside noise).

⚠ **That is one arm, n=10 in the early cell, p ~ 0.04.** M58 was earned on exactly this — a
`nexwatch` rate read off n=9 that moved at n=24 — so it is a hypothesis until it replicates.

**K5, balanced by construction**: `--reads 6` makes index 0-1 and index 4-5 two reads EACH per
session, so the two cells have equal n and neither is a pooled tail.
  - **replicated** ⇒ late - early >= 20 points on `indala`, same sign as run 1.
  - **it was n**  ⇒ the gap is under 10 points, or it reverses.
  - between ⇒ report as unreplicated and leave it as a hypothesis.

⭐ **AND THE CONTROL THAT CAN REFUTE THE EXPLANATION, not just the effect.** The reading that
suggests itself is a start-up latency: a burst begins when the field ARRIVES, so a read that is
over quickly catches proportionally less of it, and `indala`'s probe is 33 ms against `gproxii`'s
98 ms. ⇒ **`gproxii` must show a SMALLER gap than `indala`.** An equal or larger gap refutes the
read-duration explanation even if the effect itself replicates.
⚠ M59 forbids comparing two arms' absolute RATES measured one after the other. It does not bear
on this: early and late come from inside the SAME session, so a slow wander between the two arms'
blocks moves each arm's overall rate and not its within-session position profile.

## ⭐⭐⭐ K6 — WHOSE PATTERN IS IT? (written before its capture; the K5 run forced it)

The K5 run produced something K5 was not asking about and that matters more than K5 did.
`gproxii`, six identical reads per session, sixteen sessions: **the pattern `.X.XX.` in 16 of 16**
— index 1, 3 and 4 decoded EVERY time, index 0, 2 and 5 NEVER. `indala` in the same run: index 5
**16/16**, index 4 **0/16**, adjacent reads of the identical command. ⇒ **Within a session the
decode is not a coin, it is a function of POSITION**, and at a 50% base rate one fixed six-bit
pattern repeating sixteen times is not something a rate can produce.

⛔ **SO A "HIT RATE" MAY BE MEASURING THE HARNESS'S READ CADENCE AND NOT THE EMITTER.** Before
any of that is claimed, the pattern has to be shown to belong to the AIR rather than to the client.

  **H_beat**   — it is C486's free-running beat, sampled at the session's fixed read cadence:
                 each read lands at its own phase of a ~61-80 ms null cycle, deterministic
                 because the cadence is.
  **H_client** — the nth read of a pm3 session is intrinsically different from the (n+1)th, and
                 our emission's timing has nothing to do with it.

**K6**: repeat the session with `msleep -t D` before every read, D over a range spanning the beat
period, the DELAYS SHUFFLED across sessions so D is not confounded with time (M60).
  - H_beat ⇒ the pattern MOVES with D. **Pre-registered: at least two D values give a pattern
    other than the baseline's.**
  - H_client ⇒ every D reproduces the baseline pattern, because the nth read is still the nth.
    ⚠ That outcome would be the more alarming of the two and is the reason this runs at all.
  - patterns not repeatable WITHIN a D ⇒ no verdict; report that the determinism is itself
    D-dependent.

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
import json
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


def session(arm_key, reads, timeout, delay=0):
    """One pm3 invocation issuing `reads` IDENTICAL probes. Returns the per-read scores in order.

    ⛔ `reads == 0` is the instrument control and is not a degenerate case: the client still
    connects and disconnects, and whatever field that costs is what Δ_0 measures."""
    a = shortread.ARMS[arm_key]
    probe = PROBES[arm_key]
    cmds = []
    for _ in range(reads):
        if delay:
            # ⭐ K6's independent variable. `msleep` is AlwaysAvailable in the client
            # (cmdmain.c:365), so it costs no field and no device round trip.
            cmds.append("msleep -t %d" % delay)
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


def pattern(scores):
    """A session's outcome as one string — `X` decoded ours, `.` did not."""
    return "".join("X" if e else "." for _, e in scores)


def k6(a, arms, delays):
    """⭐ K6. Sessions grouped by delay, the DELAYS SHUFFLED so D is not confounded with time."""
    import collections
    import random as _r
    plan = [d for d in delays for _ in range(a.reps)]
    _r.Random(a.seed).shuffle(plan)
    print("K6 — %d sessions x %d reads, delays %s, shuffled (seed %d)"
          % (len(plan), a.reads, delays, a.seed))
    out = {}
    try:
        for key in arms:
            arm = shortread.ARMS[key]
            ok, why = seqdump.arm(a.port, arm.typ, arm.econfig)
            if not ok:
                print("%-9s ⛔ ARM FAILED: %s" % (key, why))
                continue
            pats = collections.defaultdict(list)
            for d in plan:
                sc = session(key, a.reads, a.timeout, delay=d)
                pats[d].append(pattern(sc))
                time.sleep(0.5)
            out[key] = {d: v for d, v in pats.items()}
            print("\n%-9s probe %r" % (key, PROBES[key] or arm.reader))
            base = None
            for d in delays:
                c = collections.Counter(pats[d])
                top, n = c.most_common(1)[0]
                if base is None:
                    base = top
                print("   msleep %-4d  modal %-10s %d/%d sessions   %s"
                      % (d, top, n, len(pats[d]),
                         "= baseline" if top == base else "⭐ MOVED"))
            moved = sum(1 for d in delays
                        if collections.Counter(pats[d]).most_common(1)[0][0] != base)
            print("   ⇒ %d of %d delays moved the pattern — H_beat needs at least 2; "
                  "0 means H_client" % (moved, len(delays) - 1))
    finally:
        o = seqdump.disarm(a.port)
        print("\ndisarm: %s" % ("ok" if "success" in o.lower() else o.strip()[-160:]))
    if a.out:
        with open(a.out, "w") as fh:
            json.dump({k: {str(d): v for d, v in x.items()} for k, x in out.items()}, fh, indent=1)
        print("raw patterns → %s" % a.out)
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arms", default=",".join(ORDER))
    ap.add_argument("--port", default=seqdump.CU2_PORT)
    ap.add_argument("--reads", type=int, default=12, help="identical probes per session (N)")
    ap.add_argument("--sessions", type=int, default=4)
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--out", help="⛔ write the per-read scores here BEFORE anything formats "
                                  "them. The first armed run lost its entire per-index "
                                  "breakdown to a format-string bug after every read had "
                                  "already been taken.")
    ap.add_argument("--delays", help="⭐ K6: comma-separated msleep values in ms, run as "
                                     "shuffled session groups")
    ap.add_argument("--reps", type=int, default=4, help="K6 sessions per delay")
    ap.add_argument("--seed", type=int, default=1, help="K6 delay-shuffle seed")
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
    if a.delays:
        return k6(a, arms, [int(x) for x in a.delays.split(",")])
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

    if a.out:
        with open(a.out, "w") as fh:
            json.dump({k: {"d0": v["d0"], "deltas": v["deltas"],
                           "per_session": v["per_session"]}
                       for k, v in results.items()}, fh, indent=1)
        print("\nraw scores → %s" % a.out)

    print("\n=== K1 — arrivals per read ===")
    if a.null:
        # ⛔ Δ IS ZERO BY CONSTRUCTION WITH NOTHING ARMED, and reading that as "the field
        # persists" would be the tool confirming its own hypothesis out of its own control. K1 is
        # only meaningful on an armed run; the null's job is K3 and nothing else.
        print("n/a on a NULL run — nothing is playing back, so \u0394=0 is the control passing "
              "and\n      carries no information about arrivals. K1 needs an armed arm.")
    for key, r in (results.items() if not a.null else []):
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
              % (key, eh, en, ep, lh, ln, lp, ep - lp))
    print("\n⇒ starvation is REFUTED by K2 if every gap is within 15 points; SUPPORTED if an arm's "
          "early rate is\n  at least twice its late rate. Read K1 as the premise and K2 as the "
          "effect.")
    if a.null:
        print("\nK3: this was the NULL run — every count above must be 0.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
