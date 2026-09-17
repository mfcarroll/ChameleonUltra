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

## ⭐⭐ K7 — WHY WAS K5 DETERMINISTIC AND K6 NOT? (written before its capture)

K5 got `gproxii`'s `.X.XX.` in **16 of 16** sessions. K6, same arm and same six identical reads,
got its modal pattern in only **2 of 4** at the same D=0. ⛔ By K6 as written that is the
*no verdict* branch for the determinism, and the difference between the two runs must not be
explained by staring at them. **The one structural difference**: K5 spaced every session
identically (a fixed pause, and every session the same length), while K6's sessions differ in
length because their delays differ — so the PHASE AT SESSION START was repeatable in K5 and was
not in K6.

**H_phase** — the pattern is deterministic when the session's START PHASE is repeatable; the
cadence then fixes every later read's phase off it.
**H_other** — the K5/K6 difference is something else, and the determinism is not about spacing.

**K7**, one arm, identical session content throughout, two blocks INTERLEAVED so drift moves both:
  - **fixed** spacing — the same pause between every session
  - **random** spacing — a pause drawn uniformly over a span wider than the beat period
  - H_phase ⇒ **fixed reaches a modal pattern in >= 70% of its sessions and random in <= 40%.**
  - both high ⇒ spacing is not what sets the phase; H_phase refuted and the K5/K6 difference is
    still open.
  - both low ⇒ K5's 16/16 did not reproduce at all, and the determinism claim of C507 weakens
    further rather than being rescued. ⚠ **That outcome must be reported as such**, not retried
    until it comes out.
  - otherwise ⇒ no verdict.

⚠ This tests the DETERMINISM half only. K6 already refuted H_client on its own evidence — a
host-side `msleep` moved the outcome — and nothing here revisits that.

## ⭐⭐⭐ K8 — THE PERIODICITY TEST, WITH ITS NUMBER TAKEN FROM A DIFFERENT EXPERIMENT

C510 left exactly one thing standing and flagged it as post-hoc: **ms between READS move the
pattern (K6) while 8 s between SESSIONS do not (K7)** ⇒ the phase is re-established at each
session and only a read's offset WITHIN the session matters. If that is right, a `msleep -t D`
placed as a **LEAD-IN**, before the first read, should slide the whole pattern through the beat.

⭐⭐ **AND THE PERIOD IS NOT A FREE PARAMETER — IT COMES FROM C509, WHICH MEASURED SOMETHING
ELSE.** The offset is **131.5 ppm on 62.5 kHz = 8.22 Hz**, so the full phase cycle is
**121.6 ms**. ⭐ The amplitude nulls sit at HALF that, 60.8 ms — and C493 independently measured
a null spacing of **60.8 ms**. Two experiments that never shared a method already agree, so the
prediction below is not fitted to anything.

**K8**: lead-in `msleep -t D` before the first read, D = 0..240 ms in 15 ms steps, reps shuffled.
  - **primary prediction**: `pattern(D)` matches `pattern(D + 122 ms)` BETTER than `pattern(D +
    61 ms)`, because 122 ms is a full phase cycle and 61 ms is a half cycle (an inversion).
  - **SUPPORTED** ⇒ mean Hamming similarity at lag ~122 exceeds that at lag ~61 by **>= 1 bit of
    6**, and lag-122 similarity is **>= 5/6**.
  - **REFUTED** ⇒ lag-122 is not greater than lag-61, or lag-122 is below 4/6.
  - between ⇒ no verdict.
  ⛔ **THE CONTROL THAT CAN KILL IT FOR LACK OF POWER, and it is the likely failure**: if the
  similarity is high at EVERY lag, the lead-in simply does not move the pattern and the test
  proves nothing. The mean over all lags is reported first for exactly that reason, and a flat
  profile is reported as **NO POWER**, not as support.

## ⭐⭐⭐ K9 — GIVE EVERY READ ITS OWN BURST, AND SEE IF THE POSITION DEPENDENCE GOES AWAY

C511's consolidated picture is post-hoc and says so: **the phase reference is the BURST's start,
and only the inter-read cadence matters.** It needs K1's measurement to work at all — a burst
SPANS reads (`gproxii` takes ~1 field arrival per 2.4 reads), so successive reads sit at
successive phases of ONE burst. ⭐ **That makes a sharp prediction with a real payoff**: put a gap
between reads longer than the burst itself (`LF_TAG_BURST_TARGET_MS` = 500 ms) and every read
starts a FRESH burst ⇒ every read sits at phase zero ⇒ **the position dependence should vanish.**

**K9**: between-read gaps D ∈ {0, 300, 600, 900} ms, sessions interleaved and shuffled, with the
playbacks counter read around each session.

  - **P1, the MECHANISM, and it is independent of the outcome.** Arrivals per read should climb
    towards 1.0 as D passes ~500 ms, from K1's measured 0.42 at D=0. **P1 is what makes P2 and P3
    interpretable**: without it, a change at large D could be anything.
  - **P2, position dependence.** At D >= 600 the per-index hit rates should become EQUAL — spread
    across the six indices <= 1 of `reps`, against the 16-of-16 alternation seen at D=0.
  - **P3, the rate, and either direction is a result.** If every read lands at phase zero the rate
    goes to ~100% (phase zero is favourable) or ~0% (it is not). ⛔ **A rate that stays near 50%
    WITH structure refutes the whole picture**, and that is the outcome to watch for.

⛔ **THE CONTROL THAT CAN FAIL**: D=0 runs in the same shuffled set and must still reproduce the
`.X.XX.`-family pattern. If it does not, the bench moved and nothing in the run is comparable to
K5-K8 — report that, do not interpret the rest.

⚠ **IF P2 AND P3 BOTH LAND, IT IS STILL NOT A HARNESS CHANGE TO MAKE HERE.** A reliable read is
an operator decision, because the graded read path re-bases every past cell. What this run can do
is hand them the number.

## ⛔⛔⛔ K10 — K9 IMPLICATES A HARNESS CONSTANT I ALREADY SHIPPED, SO THIS DECIDES IT

K9 measured `gproxii` at **50% with D=0, 12% at D=300 ms, 0% at D=600 and 900** — with arrivals
per read going 0.50 → 1.00, so past the burst every read starts a fresh one and **a fresh burst
does not decode at all.**

⛔⛔ **THAT BEARS DIRECTLY ON `benchmatrix`' `--repeat`, WHICH I CHANGED EARLIER TONIGHT** on
C507's evidence, adding a random pause of **[0, 250) ms** between repeats so they would not
resample one phase. K9 says a pause is **not a neutral randomiser**: it systematically collapses
the rate. A 250 ms span reaches into the region where these arms stop decoding, so the fix may be
trading a correlated rate for a suppressed one.

**K10** sweeps D ∈ {0, 40, 80, 120, 160, 200, 250, 300} ms, shuffled, with arrivals and rate.
  - **the number wanted**: the largest D at which arrivals/read stays **<= 0.6** (bursts still
    span reads) AND the rate stays **>= 40%** (within noise of D=0's 50%).
  - **the harness rule**: the jitter span must be **<= that D** and **>= 121.6 ms** (C509's full
    phase cycle) to decorrelate phase at all.
  - ⛔⛔ **AND IT IS ALLOWED TO CONCLUDE THAT NO SUCH SPAN EXISTS.** If nothing satisfies both,
    then **spacing cannot decorrelate phase without collapsing the rate**, and `--repeat` simply
    cannot produce independent samples of these arms by spacing alone. That outcome goes in the
    flag's own help text — it does not get papered over with a number that looks defensible.

## ⭐⭐ K11 — DOES C512 HOLD ON A SECOND ARM, OR IS THE WARNING `gproxii`-SHAPED?

⛔ **K5 through K10 all used ONE arm.** C512's conclusion — *a fresh burst scores zero, so never
add settling between graded reads* — is the round's most consequential warning and it rests
entirely on `gproxii`. A warning that general has to be shown to be that general.

**K11**: K9's design (gaps 0 and 600 ms, shuffled, arrivals counted) on **`indala`** and
**`keri`**, whose probes are different lengths and whose demodulators are different code.
  - **generalises** ⇒ both arms lose at least **half** their D=0 rate at D=600, and arrivals per
    read climb to **>= 0.8** on both. The warning stands as written.
  - **`gproxii`-shaped** ⇒ either arm holds its rate at D=600 while its arrivals still climb.
    ⛔ Then **C512 must be re-scoped to the one arm** and the harness warning narrowed with it.
  - split ⇒ report per arm and narrow the claim to the arms that showed it.
  ⚠ The arrivals half is the control: if arrivals do NOT climb on an arm, its burst never
  restarted and its rate says nothing about fresh bursts.
⚠ Each arm's D=0 rate is its own baseline — M59 forbids comparing the arms' absolute rates with
each other, and nothing here does: the comparison is within an arm, across two gaps.

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


def session(arm_key, reads, timeout, delay=0, lead=0):
    """One pm3 invocation issuing `reads` IDENTICAL probes. Returns the per-read scores in order.

    ⛔ `reads == 0` is the instrument control and is not a degenerate case: the client still
    connects and disconnects, and whatever field that costs is what Δ_0 measures."""
    a = shortread.ARMS[arm_key]
    probe = PROBES[arm_key]
    cmds = []
    if lead:
        cmds.append("msleep -t %d" % lead)     # ⭐ K8: once, BEFORE the first read
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


def k7(a, arms):
    """⭐ K7. Fixed vs randomised inter-session spacing, interleaved, identical sessions."""
    import collections
    import random as _r
    rng = _r.Random(a.seed)
    plan = [m for _ in range(a.reps) for m in ("fixed", "random")]
    rng.shuffle(plan)
    print("K7 — %d sessions x %d reads, fixed pause %.1fs vs random over [%.1f, %.1f]s, "
          "interleaved (seed %d)" % (len(plan), a.reads, a.fixed_pause, a.fixed_pause,
                                     a.fixed_pause + a.jitter, a.seed))
    out = {}
    try:
        for key in arms:
            arm = shortread.ARMS[key]
            ok, why = seqdump.arm(a.port, arm.typ, arm.econfig)
            if not ok:
                print("%-9s ⛔ ARM FAILED: %s" % (key, why))
                continue
            pats = collections.defaultdict(list)
            for mode in plan:
                sc = session(key, a.reads, a.timeout)
                pats[mode].append(pattern(sc))
                time.sleep(a.fixed_pause if mode == "fixed"
                           else a.fixed_pause + rng.uniform(0.0, a.jitter))
            out[key] = dict(pats)
            print("\n%-9s probe %r" % (key, PROBES[key] or arm.reader))
            share = {}
            for mode in ("fixed", "random"):
                c = collections.Counter(pats[mode])
                top, n = c.most_common(1)[0]
                share[mode] = n / float(len(pats[mode]))
                print("   %-7s modal %-10s %d/%d = %.0f%%   %s"
                      % (mode, top, n, len(pats[mode]), 100 * share[mode],
                         " ".join(pats[mode])))
            f, r = share["fixed"], share["random"]
            if f >= 0.70 and r <= 0.40:
                v = "⇒ H_phase SUPPORTED — the start phase is what makes it deterministic"
            elif f >= 0.70 and r >= 0.70:
                v = ("⇒ BOTH HIGH — spacing is not what sets the phase. H_phase refuted and the "
                     "K5/K6 difference is still open")
            elif f <= 0.40 and r <= 0.40:
                v = ("⇒ BOTH LOW — K5's 16/16 did not reproduce at all. C507's determinism half "
                     "weakens rather than being rescued; report it, do not retry it")
            else:
                v = "⇒ no verdict under K7 as written"
            print("   %s" % v)
    finally:
        o = seqdump.disarm(a.port)
        print("\ndisarm: %s" % ("ok" if "success" in o.lower() else o.strip()[-160:]))
    if a.out:
        with open(a.out, "w") as fh:
            json.dump(out, fh, indent=1)
        print("raw patterns → %s" % a.out)
    return 0


def _sim(a, b):
    """Hamming similarity of two equal-length patterns, in bits."""
    return sum(1 for x, y in zip(a, b) if x == y)


def k8(a, arms, leads):
    """⭐ K8. A LEAD-IN delay before the first read, swept across the beat. See the docstring."""
    import collections
    import random as _r
    rng = _r.Random(a.seed)
    plan = [d for d in leads for _ in range(a.reps)]
    rng.shuffle(plan)
    print("K8 — %d sessions x %d reads, lead-in %s ms, shuffled (seed %d)"
          % (len(plan), a.reads, leads, a.seed))
    print("   period from C509: 131.5 ppm x 62.5 kHz = 8.22 Hz ⇒ full cycle 121.6 ms, "
          "nulls at 60.8 ms\n")
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
                # ⛔ The lead-in is ONE msleep before the first read and nothing between them:
                # K6 already varied the between-read spacing, and varying both would confound
                # the lead-in with the cadence.
                sc = session(key, a.reads, a.timeout, delay=0, lead=d)
                pats[d].append(pattern(sc))
                time.sleep(0.4)
            out[key] = {d: v for d, v in pats.items()}
            modal = {}
            print("\n%-9s probe %r" % (key, PROBES[key] or arm.reader))
            for d in leads:
                c = collections.Counter(pats[d])
                top, n = c.most_common(1)[0]
                modal[d] = top
                print("   lead %-4d  modal %-10s %d/%d" % (d, top, n, len(pats[d])))

            step = leads[1] - leads[0] if len(leads) > 1 else 1
            def profile(lag_ms):
                k = int(round(lag_ms / float(step)))
                ps = [(modal[d], modal[d + k * step])
                      for d in leads if (d + k * step) in modal]
                return (sum(_sim(x, y) for x, y in ps) / float(len(ps)), len(ps)) if ps else (0, 0)

            print("\n   similarity by lag (bits of %d):" % a.reads)
            allsims = []
            for lag in range(step, 241, step):
                m, n = profile(lag)
                if n:
                    allsims.append(m)
                    mark = ""
                    if abs(lag - 122) <= step / 2.0:
                        mark = "  ⭐ full cycle (C509)"
                    elif abs(lag - 61) <= step / 2.0:
                        mark = "  ← half cycle"
                    print("     lag %3d ms  %.2f  (n=%d)%s" % (lag, m, n, mark))
            s122, _ = profile(122)
            s61, _ = profile(61)
            flat = (max(allsims) - min(allsims)) if allsims else 0.0
            print("\n   lag 122 = %.2f, lag 61 = %.2f, spread across all lags = %.2f"
                  % (s122, s61, flat))
            if flat < 0.5:
                print("   ⇒ **NO POWER** — similarity is flat across every lag, so the lead-in "
                      "does not move\n     the pattern at all. This proves nothing either way.")
            elif s122 - s61 >= 1.0 and s122 >= a.reads * 5.0 / 6.0:
                print("   ⇒ **SUPPORTED** — the pattern repeats at C509's full phase cycle, a "
                      "number that\n     came from a different experiment entirely.")
            elif s122 <= s61 or s122 < a.reads * 4.0 / 6.0:
                print("   ⇒ **REFUTED** — no repeat at the predicted period.")
            else:
                print("   ⇒ between the bands — NO VERDICT.")
    finally:
        o = seqdump.disarm(a.port)
        print("\ndisarm: %s" % ("ok" if "success" in o.lower() else o.strip()[-160:]))
    if a.out:
        with open(a.out, "w") as fh:
            json.dump({k: {str(d): v for d, v in x.items()} for k, x in out.items()}, fh, indent=1)
        print("raw patterns → %s" % a.out)
    return 0


def k9(a, arms, gaps):
    """⭐ K9. Between-read gaps spanning the burst, with the arrivals counter as the mechanism."""
    import collections
    import random as _r
    rng = _r.Random(a.seed)
    plan = [d for d in gaps for _ in range(a.reps)]
    rng.shuffle(plan)
    print("K9 — %d sessions x %d reads, between-read gaps %s ms, shuffled (seed %d)"
          % (len(plan), a.reads, gaps, a.seed))
    print("   burst is LF_TAG_BURST_TARGET_MS = 500 ms; K1 measured 0.42 arrivals/read at D=0\n")
    out = {}
    try:
        for key in arms:
            arm = shortread.ARMS[key]
            ok, why = seqdump.arm(a.port, arm.typ, arm.econfig)
            if not ok:
                print("%-9s ⛔ ARM FAILED: %s" % (key, why))
                continue
            pats = collections.defaultdict(list)
            arr = collections.defaultdict(list)
            for d in plan:
                before = playbacks(a.port)
                sc = session(key, a.reads, a.timeout, delay=d)
                after = playbacks(a.port)
                pats[d].append(pattern(sc))
                if before is not None and after is not None:
                    arr[d].append((after - before) / float(a.reads))
                time.sleep(0.4)
            out[key] = {str(d): {"patterns": pats[d], "arrivals": arr[d]} for d in gaps}
            print("\n%-9s probe %r" % (key, PROBES[key] or arm.reader))
            print("   %-6s %-10s %-9s %-24s %s"
                  % ("gap", "arr/read", "rate", "per-index hits", "patterns"))
            for d in gaps:
                ps = pats[d]
                per = [sum(1 for p in ps if p[i] == "X") for i in range(a.reads)]
                hits = sum(p.count("X") for p in ps)
                n = sum(len(p) for p in ps)
                am = sum(arr[d]) / len(arr[d]) if arr[d] else float("nan")
                print("   %-6d %-10.2f %-9s %-24s %s"
                      % (d, am, "%d/%d=%.0f%%" % (hits, n, 100.0 * hits / n),
                         " ".join(str(x) for x in per), " ".join(ps)))
            # P1
            # ⛔ DEFINED FROM THE BURST, NOT HARDCODED. The first version tested `d >= 600`,
            # which is empty for any sweep that stops below it — K10's did, and the summary
            # then reported "0.00 arrivals at D>=600" and declared the run uninterpretable while
            # the table above it was perfectly good. A guard that fires on the absence of data
            # reads exactly like a guard that fires on bad data.
            big = [d for d in gaps if d >= 600] or [max(gaps)]
            a0 = sum(arr[gaps[0]]) / len(arr[gaps[0]]) if arr[gaps[0]] else 0.0
            ab = ([sum(arr[d]) / len(arr[d]) for d in big if arr[d]] or [0.0])
            abm = sum(ab) / len(ab)
            print("\n   P1 mechanism: arrivals/read %.2f at D=%d → %.2f at D=%s  ⇒ %s"
                  % (a0, gaps[0], abm, ",".join(str(d) for d in big),
                     "burst restarts per read" if abm >= 0.8
                     else "it does NOT restart per read — P2/P3 are uninterpretable"))
            # P2
            for d in big:
                ps = pats[d]
                per = [sum(1 for p in ps if p[i] == "X") for i in range(a.reads)]
                spread = max(per) - min(per)
                print("   P2 at D=%d: per-index spread %d of %d ⇒ %s"
                      % (d, spread, len(ps),
                         "position dependence GONE" if spread <= 1 else "structure REMAINS"))
            # P3
            for d in big:
                ps = pats[d]
                hits = sum(p.count("X") for p in ps)
                n = sum(len(p) for p in ps)
                r = 100.0 * hits / n
                print("   P3 at D=%d: rate %.0f%% ⇒ %s"
                      % (d, r, "near-deterministic" if r >= 85 or r <= 15
                         else "⛔ still mid-range — this REFUTES the picture"))
    finally:
        o = seqdump.disarm(a.port)
        print("\ndisarm: %s" % ("ok" if "success" in o.lower() else o.strip()[-160:]))
    if a.out:
        with open(a.out, "w") as fh:
            json.dump(out, fh, indent=1)
        print("raw → %s" % a.out)
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
    ap.add_argument("--gaps", help="K9/K10: comma-separated between-read gaps in ms")
    ap.add_argument("--k9", action="store_true",
                    help="⭐ K9: between-read gaps spanning the 500 ms burst, with arrivals")
    ap.add_argument("--k8", action="store_true",
                    help="⭐ K8: sweep a LEAD-IN delay before the first read, across the beat")
    ap.add_argument("--k7", action="store_true",
                    help="⭐ K7: fixed vs randomised inter-session spacing, interleaved")
    ap.add_argument("--fixed-pause", dest="fixed_pause", type=float, default=1.0)
    ap.add_argument("--jitter", type=float, default=8.0,
                    help="K7: the random block's extra pause span, wider than the beat period")
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
    if a.k7:
        return k7(a, arms)
    if a.k8:
        return k8(a, arms, list(range(0, 241, 15)))
    if a.k9:
        gaps = ([int(x) for x in a.gaps.split(",")] if a.gaps else [0, 300, 600, 900])
        return k9(a, arms, gaps)
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
