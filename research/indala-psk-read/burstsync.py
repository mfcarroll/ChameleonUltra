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

## ⭐⭐⭐ K12 — IS IT TIME SINCE FIELD ARRIVAL, OR THE READ'S INDEX? (written before its capture)

C512's mechanism story is post-hoc and the finding itself says so: *the emission needs time after
field arrival before it is decodable*. Everything measured so far is consistent with it and none
of it TESTS it, because in K5-K11 **time-since-arrival and read INDEX are perfectly confounded**:
every read is the same length and issued back to back, so read k sits at
elapsed ~ k x (probe + overhead) by construction. C507's `.X.XX.` is a statement about index; the
settling story is a statement about elapsed time; no run so far can tell the two apart.

⭐⭐ **THE KNOB THAT SEPARATES THEM, AND IT IS A NEW ONE.** K6/K9/K10/K11 all varied the GAP
between reads — field DOWN — and K10 measured what that costs: past ~120 ms the burst restarts
(arrivals 0.50 → 1.00), so the gap knob moves elapsed time and burst identity TOGETHER. **This
varies the field-UP duration instead**: a PRIMER `lf read -s N` of varying length immediately
followed by the probe, with no `msleep` anywhere. The probe is then

  - always the **same command** (`lf read -s 12288` + `lf gproxii demod`),
  - always at the **same index** (1),
  - always inside **one burst** with the primer (P1 checks it),

and the only thing that moves between cells is how long the emission has been running when the
probe starts.

    elapsed(probe) = primer_duration + C

C is the client's inter-command overhead, **measured at ~80 ms before the ladder was fixed**
(`hw version` 0.60 s; +4096 samples 0.11 s for 32.8 ms of sampling; +30000 samples 0.33 s for
240 ms of sampling ⇒ ~80-90 ms of it is not sampling). It is constant across cells, so it offsets
the axis and cannot bend it.

⛔ **THE LADDER'S FLOOR IS THAT OVERHEAD, AND IT IS A REAL LIMITATION.** With any primer present
the probe cannot start earlier than ~80 ms after arrival, so the 0-80 ms region is reachable only
through the no-primer control. **If settling completes inside 80 ms this design sees a flat,
saturated profile and must report NO POWER** — which would itself locate the process below 80 ms
rather than refute it. Named here, before the run, because K8 died of exactly this.

**K12**: primer durations **20..240 ms in 20 ms steps** (`-s` = ms x 125), one session per cell
per round, **cells shuffled within every round and the seed recorded** (M60), plus a no-primer
control cell. Twelve cells x `--reps` rounds.

⛔ **THE BURST MUST NOT EXPIRE, or this measures starvation instead of settling.** 240 ms primer
+ ~80 ms overhead + 98 ms probe = **418 ms** against `LF_TAG_BURST_TARGET_MS` = 500 ms (K4). That
is why the ladder stops at 240 and not higher. ⚠ **A rate that FALLS at the longest cells is the
expiry signature** and must not be read as anything else.

  **P1 — the mechanism control, independent of the outcome.** Arrivals per read must stay
  **<= 0.6 in every one of the twelve primer cells** — one arrival for the primer+probe pair,
  i.e. the field never dropped and the two reads share a burst. ⛔ A cell climbing to >= 0.8 got
  its own burst, its elapsed axis does not exist, and it is reported rather than interpreted.
  ⚠ **The no-primer control is EXEMPT and is expected at ~1.0**: one read, its own arrival, which
  is precisely what makes it the fresh-burst cell. ⭐ P1 is also what makes K12 a different
  experiment from K9-K11: there the independent variable moved arrivals BY DESIGN; here it must
  not move them at all.

  **P2 — H_settle: the rate RISES with elapsed time.** Pre-registered: the pooled rate over the
  top three cells (200, 220, 240 ms) exceeds the pooled rate over the bottom three (20, 40, 60)
  by **>= 20 points**.

  **P3 — H_phase: the rate is PERIODIC in elapsed at C509's 121.6 ms, not monotone.** ⭐⭐ The
  ladder is built so the two cannot be confused: **phase = elapsed mod 121.6 ms is a SAWTOOTH
  over this range while elapsed is monotone**, and the six pairs (20,140) (40,160) (60,180)
  (80,200) (100,220) (120,240) sit 120 ms apart — within **1.6 ms, 4.7 degrees, of one full
  cycle**. Pre-registered: mean |rate(d) - rate(d+120)| over those six pairs **<= 15 points**
  AND the profile's own range within a half **>= 30 points**. ⛔ Without that range there is
  nothing for the pairs to match and a flat profile scores a perfect match — the K8 NO POWER
  trap, named before the run for the second time in this file.

  **P4 — H_index: flat.** All twelve cells within **15 points** of each other ⇒ elapsed time does
  not matter and C507's index effect is not a settling process.

  ⛔⛔ **IF P2 AND P3 BOTH FIRE THE VERDICT IS *NO VERDICT*** — a rising sawtooth satisfies both
  and this n cannot separate them. Written here so it cannot be settled by preference afterwards.

⛔ **THE CONTROL THAT CAN FAIL, and it is the bench-moved check.** The no-primer cell is one probe
alone in a fresh client session — a fresh burst, phase zero — which is C512's D>=600 condition and
K5's index 0. **Both measured 0% on `gproxii`, so it must come back at or below 15% here.** If it
is high the bench has moved, nothing in the run is comparable to K5-K12, and none of it gets
interpreted.

⚠ ONE ARM, and C514 is the reason it is this one: *a fresh burst scores zero* is `gproxii`-only,
so the mechanism proposed for it is tested where the effect actually exists. ⛔ **Whatever it
returns is scoped to `gproxii` until a second arm says otherwise** — the lesson C514 cost, written
before this run rather than after it.

⚠ Per-cell n is `--reps` and **no single cell's rate is claimed on its own** (M58). The statistics
that carry the verdict pool three cells (P2), six pairs (P3) or twelve (P4).


### ⛔⛔ THE LADDER WAS RE-FIXED BEFORE THE SCORING CAPTURE, AND ~80 ms WAS THE WRONG OFFSET

A reps=1 plumbing pass (13 sessions, numbers NOT interpreted — n=1 per cell) fired **P1 at the
top two cells**: arrivals 1.00 at primers 220 and 240 ms, i.e. the burst re-arms while the field
is still up. ⭐ **P1 was written as a mechanism control independent of the outcome and this is it
working**, so the ladder is re-fixed from it rather than the result being explained away.

⛔ **Re-fixed from MECHANISM data only, with no rate information involved.** Two sweeps, both with
nothing to score:

    lf read -s N                      # a bare capture — there is nothing to demodulate
    lf read -s <primer>; lf read -s 12288   # ⭐ the demod OMITTED; it raises no field, so the
                                            #   field behaviour is identical to a real probe

**Both came back perfectly deterministic, 3 of 3 at every step (seeds 5 and 9, shuffled):**

| sweep | result |
|---|---|
| one read, sampling 100→450 ms | **1 arrival**; at 500 and 600 ms, **2** |
| two reads, primer 140→200 ms | **1 arrival** (0.50/read); primer **220→280 ms, 2** (1.00/read) |

⭐⭐ **The first is `LF_TAG_BURST_TARGET_MS` = 500 measured FROM THE AIR SIDE** — the constant read
out of the firmware in K4, now confirmed by the counter, with a step sharp enough to bracket it in
one 50 ms interval.

⛔ **The second corrects this criterion's own offset, and by more than a factor of two.** From the
two thresholds: `field_up(single, N) = N + c1` bracketed at 500 ⇒ **c1 ∈ (0, 50] ms**; and
`primer + gap + 98 + c1` crossing 500 between primer 200 and 220 ⇒ **gap + c1 ∈ [182, 202) ms**.
⇒ **elapsed(probe) = primer + G with G ≈ 182-202 ms**, not the ~80 ms the wall-clock timing
suggested. ⚠ The wall-clock delta for an extra read (~80 ms) measures the CLIENT's cost; the field
stays up through USB round trips that delta does not isolate. **A number measured on the host was
not the number the air sees**, and only the arrivals counter could tell the difference.

✅ **P3 IS UNTOUCHED BY THE CORRECTION, and that is why it was specified as a difference.** The
pairs are 120 ms apart and a constant offset **cancels in a difference** — so the periodicity test
does not depend on knowing G at all. ⛔ What the correction does change is the absolute phase
column (now printed against G = 192 ms) and, more seriously, the **floor**: the probe cannot start
earlier than ~190 ms after arrival, so the unreachable region is **~190 ms, not ~80** — over 1.5
beat cycles. **The NO POWER branch is correspondingly more likely and its statement is
correspondingly weaker.** That is worse for this experiment and is recorded as such.

⇒ **THE LADDER IS NOW 20..200 ms IN 20 ms STEPS — ten cells, not twelve.** 200 + 192 + 98 = 490 ms
against the 500 ms burst, and the arrivals control confirms it directly (0.50 per read at 200,
1.00 at 220) rather than by arithmetic. **The cost is two of the six phase pairs**: the matched
set is now (20,140) (40,160) (60,180) (80,200) — **four pairs**, each still within 1.6 ms of one
full cycle. P2 becomes bottom three {20,40,60} against top three {160,180,200}, P4 spans ten
cells, and the within-half range is over {20..120}.

⚠ **What this costs in reach**: the elapsed span is now 212→392 ms, **1.48 beat cycles**, so P3
tests one lag and not a profile. A periodicity claim from four pairs at one lag is weaker than the
six this criterion first asked for, and it is not to be written up as though it were the same test.


## ⛔⛔⛔ K13 — P2's BAND FIRED AND THE PROFILE REFUTES WHAT IT WAS WRITTEN TO LICENSE

**K12's run (seed 17, reps 24, 264 sessions, `caps/k12_gproxii_s17.json`) came back saturated with
ONE deep notch, and P2's pooled contrast fired on the notch rather than on a rise:**

    none  0/24 = 0%     ⭐ the bench-moved control PASSES — C512's 0% reproduced
    20    24/24  100%   |  120  22/24   92%
    40    22/24   92%   |  140  24/24  100%
    60     2/24    8%   ⛔ THE NOTCH   |  160  20/24   83%
    80    22/24   92%   |  180  20/24   83%
    100   24/24  100%   |  200  23/24   96%

Nine of the ten cells sit in **83-100%, a range of 17 points, median 92%**. P1 held at **0.50
arrivals per read in every cell**, so one burst spanned primer and probe throughout and the
elapsed axis is real.

⛔⛔ **AND P2 IS AN ARTIFACT OF WHERE THE NOTCH FELL.** Bottom three {20,40,60} = 67% against top
three {160,180,200} = 88%, so +21 points and the band fires at >= 20. **Bottom TWO {20,40} = 96%**
— higher than the top three. The ten cells are non-decreasing in only **5 of 9** adjacent steps and
the two large steps are **-83 and +83**: the notch's own walls. **There is no rise; there is a
notch in the bottom bin.**

⭐ **THIS IS A METHOD FAILURE IN MY OWN CRITERION, NOT A BENCH RESULT** (M62). A pooled contrast
over three cells is **not robust to a single-cell outlier**, and P2 was written with no
monotonicity requirement at all — so one cell at 8% in a field of 92% is enough to manufacture a
21-point "rise". The band was pre-registered, it fired, and believing it would have been wrong.

⇒ **THE SUBSTANTIVE READING OF K12 IS ITS OWN NO POWER BRANCH.** Every reachable cell is saturated
(>= 83%) while the no-primer control is at **0%**, so the whole 0% → ~92% transition happens
**below 212 ms of elapsed time — inside the ~190 ms floor this design cannot reach.** K12 therefore
LOCATES the process below its floor and says nothing about its shape, which is exactly what the
NO POWER branch was written to say.

⭐ **The notch is silence, not a wrong answer**: marker 2/24 = exact 2/24 at that cell, against
24/24 = 24/24 at its neighbours. The demodulator goes quiet there; it does not decode and lie
(contrast C502).

**K13 — a straight replication at a fresh seed, same ten cells, same probe, nothing else changed.**

  **Q1 — the notch REPRODUCES** ⇒ cell 60 comes back **<= 25%** while the median of the other nine
  is **>= 75%**. ⇒ a real, cell-specific effect at n=48 pooled, and it needs its own explanation.
  **Q2 — it does NOT** ⇒ cell 60 comes back **>= 75%**, inside the field. ⇒ K12's cell was a fluke
  despite n=24, and P2's firing was pure artifact. Report that and claim nothing further.
  between ⇒ no verdict on the notch.

  **Q3 — P2 AMENDED, DECLARED HERE BEFORE THE RUN.** A rise is claimed only if the top-three vs
  bottom-three contrast is **>= 20 points** AND the ten cells are non-decreasing in **>= 7 of the
  9** adjacent steps. ⛔ **Pre-registered consequence: if Q1 holds and Q3 fails, K12's P2 is
  WITHDRAWN as an artifact of the notch** and H_settle is not supported by either run.
  ⚠ The amended band is applied to K13 prospectively and recomputed on K12 retrospectively; the
  retrospective figure is labelled as such wherever it appears.

  **Q4 — the NO POWER statement, and it is what the pair of runs actually establishes.** Excluding
  the notch cell, the nine remaining cells' range is **<= 20 points** with a minimum **>= 75%**,
  against a control at **<= 15%**. ⇒ the transition is entirely below the floor and the shape above
  it carries no information about it.

⛔ P1 and the bench-moved control carry over unchanged and are checked again. ⚠ Still one arm,
still `gproxii`, still ungraded — no null sweep, no calibration row, no licence, moves no cell.


## ⭐⭐⭐ K14 — HOW WIDE IS THE NOTCH? (written before its capture)

**K13 (seed 41) replicated K12 and the notch came back HARDER — 0/24 against K12's 2/24.**
Pooled over both runs:

    none  0/48   0%   |  100  48/48  100%
    20   48/48 100%   |  120  39/48   81%
    40   46/48  96%   |  140  45/48   94%
    60    2/48   4%  ⛔ |  160  44/48   92%
    80   46/48  96%   |  180  42/48   88%
                      |  200  32/48   67%

⭐⭐ **THE NOTCH IS THE FINDING: 2 of 48 at one primer length, against 92 of 96 at the two
lengths either side of it.** ⛔ And it is not the beat: **cell 60 sits at phase 8.8 ms and cell
180 at 7.2 ms** — the same phase to within 1.6 ms — yet they score **2/48 and 42/48**. P3's own
band agrees (mean |Δ| 22 and 42 points over the four pairs, against a pre-registered 15).

⛔⛔ **AND K13 TRIGGERS K12's PRE-REGISTERED CONSEQUENCE.** Q1 held (notch at 0%, median of the
other nine 100%) and **Q3 failed in both runs** — non-decreasing in 5 of 9 steps each time, and at
seed 41 the contrast did not even fire (+10 points). ⇒ **K12's P2 is WITHDRAWN as an artifact of
the notch, and H_settle is supported by neither run.** That was written down before this capture,
so it is applied rather than argued.

⚠ **Q4 HOLDS IN ONE RUN AND NOT THE OTHER, so the *saturated above the floor* claim does not
replicate**: the off-notch cells span 17 points at seed 17 and **62** at seed 41 (cell 200 falls
96% → 38%, cell 120 92% → 71%). That is C497's wander, and **M59 forbids comparing two sessions'
levels** — so the off-notch PROFILE is not a result. ⭐ **What survives M59 is exactly the notch**:
it is a zero against a non-zero, measured against neighbours **interleaved with it inside the same
rounds**, which is the one comparison M59's last paragraph says non-stationarity cannot touch.

⇒ What holds across both runs: the control at **0%**, the notch's neighbours near **100%**, and
therefore the 0% → ~100% transition still sitting **below the ~190 ms floor**. What does not hold
is any shape above that floor.

**K14 — a fine ladder around the notch: 40, 45, 50, 55, 60, 65, 70, 75, 80 ms**, plus the
no-primer control, interleaved and shuffled within every round (M60). ⛔ **P2, P3 and P4 DO NOT
APPLY to this ladder** — it spans 40 ms and carries no pair 120 ms apart, so the periodicity and
contrast bands are not the operative ones and are printed only for completeness. K14 is judged on:

  **R1 — the anchors, and it is the bench-moved control for this run.** Cells **40 and 80 must
  both come back >= 75%** (they are 46/48 and 46/48 pooled) and the no-primer control **<= 15%**.
  ⛔ If the anchors fail, the bench has moved and none of the fine cells mean anything.

  **R2 — NARROW** ⇒ cell 60 **<= 25%** while **both 50 and 70 are >= 75%**. ⇒ the notch is under
  ±10 ms wide: a sharp feature at one primer length, and the third independent replication of it.

  **R3 — BROAD** ⇒ cells 50, 55, 60, 65 and 70 **all <= 40%**. ⇒ a dip tens of ms wide, which
  would be a different kind of object and would want a different explanation.

  **R4 — IT DOES NOT REPRODUCE A THIRD TIME** ⇒ cell 60 **>= 75%**. ⛔ Two runs at 2/48 against a
  third at >= 75% would make the notch a property of something that changed between runs and not
  of the primer length. **Report it that way and withdraw the notch**, rather than running a
  fourth to break the tie.

  between ⇒ report the profile and call the width unresolved.

⚠ One coincidence, named before the run so it cannot be discovered afterwards: the phase wrap
falls between cells 50 (120.4 ms) and 55 (3.8 ms), right beside the notch. ⭐ **It is already
controlled**: if the notch were the wrap, cell 180 at phase 7.2 ms would notch too, and it is
42/48. K14 adds nothing to that question and is not read as bearing on it.

⚠ Still one arm, still `gproxii`, still ungraded — no null sweep, no calibration row, no licence.


## ⭐⭐⭐ K15 — DOES THE NOTCH GENERALISE, AND IF SO IS IT AT THE SAME LEAD TIME?

C515 is one arm. **C514 is the standing reason that is not good enough** — it cost this project its
biggest claim of the previous round, and the correction was live for an hour. So the notch gets the
same treatment, on `indala` (probe `lf read -s 4096`, ~75%) and `keri` (its own reader, ~38%):
different probe lengths, different demodulator code, different frame geometry.

⛔⛔ **THE RESOLUTION PROBLEM, AND IT DECIDES THE DESIGN.** The notch is **5 ms wide** — cells 55
and 65 are 24/24 while 60 is 0/24. **A 20 ms grid across 20..200 ms would miss a 5 ms feature about
three times in four**, and `gproxii`'s was found only because 60 happened to sit on that grid. ⇒ A
coarse ladder on a new arm cannot answer *is there a notch*. It can only answer *is there a notch
HERE*, so that is what K15 asks, at full resolution, on K14's exact ladder:
**40, 45, 50, 55, 60, 65, 70, 75, 80 ms.**

  **S1 — the notch is at the SAME lead time** ⇒ the arm's 60 ms cell is **<= 25%** absolute **AND
  <= one third** of the median of the other eight cells. ⇒ the notch belongs to the shared timing
  path — the field, the burst, the client's scheduling — and not to a protocol, a frame length or
  a demodulator. ⭐ That would be a general fact and a much bigger one than C515.
  **S1 REFUTED** ⇒ the 60 ms cell is **within 15 points** of the median of the other eight.
  between ⇒ no verdict for that arm.

⛔⛔ **WHAT K15 CANNOT DO, WRITTEN DOWN BEFORE IT RUNS.** Neither outcome decides whether the arm
has a notch **somewhere else**. *No notch at 60* is **not** *no notch* — the grid argument above
cuts both ways, and a per-arm notch at a per-arm lead time (which is what a frame-length or
read-length mechanism would predict) is exactly what this ladder is blind to. ⇒ **A null here
narrows C515 to `gproxii`'s lead time, not to `gproxii`.** Anything more needs a 5 ms grid across
the full range: 37 cells, and it is a whole tick of its own.

⚠⚠ **POWER, PER ARM, AND IT IS NOT THE SAME.** `gproxii`'s notch is near-total (**2 of 72**).
Against `indala`'s ~75% baseline a near-total notch is unmistakable. ⛔ **Against `keri`'s ~38%
baseline only a near-total notch is detectable at all, and a PARTIAL one is invisible** — so **a
null on `keri` is not evidence of absence** (M58, and this is its exact shape). Said here rather
than discovered in the write-up.

⛔⛔ **AND THE CONTROL BAND IS NOT TRANSFERABLE — THIS IS A DEFECT IN THE TOOL, FIXED BEFORE THE
RUN.** K12's no-primer cell is checked against **<= 15%** because C512 measured `gproxii` at 0% on
a fresh burst. ⭐ **C514 measured that `indala` and `keri` do NOT collapse on a fresh burst** —
50% → 67% and 33% → 54%. ⇒ On those arms the no-primer cell is expected to be *comparable to its
ladder cells*, and the inherited band would have printed **BENCH MOVED** on a perfectly healthy
run. `FRESH_BURST_ZERO` now names the one arm the band applies to, and for any other arm the
control line prints as **informational**. ⚠ A criterion carried across arms without re-deriving it
is the same error as C512's warning being carried across arms without re-measuring it.

  ⇒ **For `indala` and `keri` the bench-moved check is the ANCHORS**: cells 40 and 80 must both
  land within 20 points of the arm's own median. ⛔ If they do not, the arm's run is not
  interpretable and is reported, not read.

⛔ P1 carries over unchanged and is checked per cell. Budget: `keri`'s probe is the longer at
10,000 samples = 80 ms, so 200 + 192 + 80 = 472 ms against the 500 ms burst — inside it, and P1
is what confirms that rather than the arithmetic (C516).

⚠ Ungraded — no null sweep, no calibration row, no licence, moves no cell.


## ⛔⛔⛔ K16 — K15's CONTROL FAILED FOR THE WRONG REASON, AND WHAT IT GLIMPSED IS BIGGER THAN K15

**K15 ran (seed 91, reps 24, both arms, K14's ladder) and BOTH arms hit the anchors branch, so by
the rule written before the capture BOTH RUNS ARE UNINTERPRETABLE.** That verdict stands and is not
being argued away. ⛔ **But the control was ill-designed, and the way it failed is the finding**:

| cell | `gproxii` (K14) | `indala` (K15) | `keri` (K15) |
|---|---|---|---|
| 40 | 100% | 12% | 0% |
| 45 | 100% | 17% | 8% |
| 50 | 92% | 62% | 54% |
| 55 | 100% | 79% | 54% |
| 60 | **0%** ⛔ | 83% | 79% |
| 65 | 100% | **96%** ⭐ | **92%** ⭐ |
| 70 | 75% | 38% | 25% |
| 75 | 100% | 42% | 21% |
| 80 | 100% | 17% | 8% |

⭐ **`indala` and `keri` have near-identical HUMPS where `gproxii` has a HOLE** — a peak at 65 ms
with both wings collapsing to 0-17%, against `gproxii` sitting near 100% everywhere but 60.

⛔⛔ **M63 — A BENCH-MOVED CONTROL THAT COMPARES THE ANCHORS TO THE MEDIAN PRESUMES THE PROFILE IS
FLAT, WHICH IS THE THING UNDER TEST.** K15's control asked that cells 40 and 80 land within 20
points of the arm's median. On a hump those cells ARE the wings, so a perfectly healthy run reads
as a moved bench — **the control cannot separate *the bench moved* from *the profile has structure
at the anchors*.** It is M62's disease in a control rather than in a statistic: a criterion whose
failure mode is ambiguous tells you nothing when it fires. ⚠ And it was written in the same round
that learned M62, one unit later.

⚠⚠ **POST-HOC, AND LABELLED: THE RUN DID NOT DRIFT.** Split-half over the rounds (first 12 against
last 12): **`indala` pooled 49% → 50% (Δ +1 point)** and **`keri` 41% → 35% (Δ −6)**, with
`keri`'s pooled 38% sitting **exactly on its known ~38% baseline**. Per cell, most are within ±8
points across the halves. ⇒ the structure is probably real. ⛔ **It is a LEAD, not a finding** —
the pre-registered control failed and this split-half was not pre-registered.

⭐⭐⭐ **WHY IT MATTERS MORE THAN C515 DID.** If it replicates, a single lead time near **65 ms**
takes `gproxii` 100%, `indala` 96% and `keri` 92% — against baselines of roughly 50%, 50% and 33%
(C512, C514). That is not a notch to avoid; it is **a working point to aim at, on three arms at
once, free and reader-side.** ⛔ Which is exactly why it gets a control that can fail before it gets
believed.

**K16 — the same ladder, both arms, a FRESH seed, and two things K15 lacked.**

  ⭐ **THE CONTROL, REPLACING THE ANCHORS RULE (which is WITHDRAWN as ill-designed).** Shape-
  agnostic, so it cannot be confounded by the profile it is policing: **split-half over the rounds
  — the arm's pooled rate across the ladder must agree between the first and second halves within
  15 points, AND the peak cell's own rate within 25 points.** ⛔ Both can fail, and neither
  presumes anything about the profile's shape. ⚠ It certifies *this run did not drift*, which is
  what licenses within-run structure; it does NOT certify an absolute level, and on a bench that
  wandered 88/60/38/75% in one evening (C497) **no absolute-level control can certify a session.**

  **T1 — THE SHAPE REPRODUCES** ⇒ for each arm, the peak cell is **60 or 65 ms**, that cell is
  **>= 75%**, and **both** the 40 ms and 80 ms cells are **<= 30%**. ⇒ the hump is real and located.
  **T2 — IT DOES NOT** ⇒ either wing is **>= 50%**, or the peak cell is below 60 or above 65.
  ⇒ K15's profile was the wander after all and the lead is withdrawn.
  between ⇒ report per arm and claim no location.

  **T3 — THE CROSS-ARM CLAIM, AND IT IS THE PRACTICAL ONE.** At **65 ms** all three arms are
  **>= 75%** (`gproxii` from K14, `indala` and `keri` from this run). ⚠ M59 applies and is
  respected: this is not a comparison of the arms' rates with each other, it is each arm against
  **its own** pre-stated threshold, and `gproxii`'s number comes from a different session so it is
  quoted as a separate measurement rather than pooled in.

⛔⛔ **AND IT STILL LICENSES NOTHING IN THE HARNESS.** A fitted lead time in the graded path
re-bases every past cell, exactly as C499's read length would, and remains the operator's decision.
What this can do is hand them a number with a control attached. ⚠ Ungraded, no null sweep, no
calibration row, moves no cell.

## ⭐⭐⭐ K17 — DOES THE HUMP FOLLOW THE MODULATION OR THE FRAME? (written before its capture)

C519 refuted *the hump travels in the arm's own frame count* on `gproxii` — 80.6% and 78.5% where
the frame reading needed <= 30%, two seeds. ⛔⛔ **And M65 is the same finding's own limitation:
`gproxii` is the only arm here with a different frame AND the only ASK/biphase one**, so that
refutation cannot separate *not frames* from *it is a PSK effect*. The band named frame length and
said nothing about the other thing that moves with it.

⭐⭐ **`nexwatch` BREAKS THE CONFOUND, AND IT IS THE ONLY ARM THAT CAN.** It is PSK like `indala`
and `keri`, but its frame is **4096 samples = 32.768 ms** against their 2048 — so the two readings
put its peak **66 ms apart**:

| reading | where `nexwatch` peaks | why |
|---|---|---|
| **H_ms** the hump sits at a fixed lead time | **65 ms** | where `indala` and `keri` peak (C517) |
| **H_frame** it sits at the arm's own frame count | **130 ms** | 3.97 of ITS frames, where they peak in theirs |

⭐ The ladder carries **both windows at 5 ms resolution** plus the wings each reading needs, in the
arm's own frames alongside the milliseconds:

    40   45   55   60   65   70   75   80  |  120  125  130  135  140  160   ms
    1.22 1.37 1.68 1.83 1.98 2.14 2.29 2.44|  3.66 3.81 3.97 4.12 4.27 4.88  frames

⭐ **80 ms is the cell both readings call LOW** — H_ms's high wing and H_frame's 2.44-frame low
wing land on the same cell — so it is a shared sanity cell and discriminates nothing. The
discrimination is entirely in **55-75 ms** against **120-140 ms**.

⛔⛔ **THIS ALSO CLOSES `rfid-tools`' QUEUE UNIT 4** — `nexwatch` is unmeasured on this knob — so
one capture answers both, and the criterion is written for the discrimination rather than fitted to
whatever the arm turns out to do.

  ⭐ **THE PROBE IS FIXED BY A PRE-EXISTING RULE, NOT BY A PILOT.** C499: the window is 3-4 frames
  on every arm, carry the range not the number. `nexwatch`'s 3 frames is **`lf read -s 12288`** +
  `lf nexwatch demod`. ⛔ Choosing it from a pilot's scores would fit the instrument to the run;
  the plumbing pass checks P1 and that the demod answers, and **its rates are not interpreted**.

  **U1 — H_ms FIRES** ⇒ pooled(55,60,65,70,75) − pooled(40,45,80) **>= 30 points**, AND at least
  **3 of those 5** cells individually sit above the highest of 40, 45 and 80.
  **U2 — H_frame FIRES** ⇒ pooled(120,125,130,135,140) − pooled(80,160) **>= 30 points**, AND at
  least **3 of those 5** cells individually sit above the higher of 80 and 160.
  **U3 — BOTH fire** ⇒ ⛔ **NO VERDICT on location.** Report both; this ladder cannot separate two
  humps from one broad rise spanning them, and that is said here rather than chosen afterwards.
  **U4 — NEITHER fires** ⇒ no verdict on location, and the substantive reading is that `nexwatch`
  does not carry C517's hump at all — a limit on its generality, reported as one.

  ⛔⛔ **THE NO-POWER GATE, AND IT IS PRE-DECLARED BECAUSE IT IS THE LIKELY OUTCOME.** `nexwatch`
  is the weakest of the six — 6/24 through its own reader (C491), precision 77% (C502). **If the
  pooled rate over ALL fourteen cells is below 15%, the run is NO POWER and NEITHER U1 nor U2 is
  read**, whatever they compute to. K15 named this exact hazard for `keri` and it applies harder
  here: a rise is detectable from a low baseline, but only if the arm decodes at all.

  ⭐ **THE SHAPE CLAUSE IS A CONJUNCTION, NEVER A TRIGGER** (M62, M64). Each U is a 5-cell pooled
  gap of n=80 FIRST; the 3-of-5 clause can only make it stricter and can never fire on its own, and
  no single cell's rate is claimed (M58). ⛔ No argmax appears in either band — C504 and M64 both
  cost a verdict to that.

  ⭐ **THE DRIFT CONTROL IS K16's, WHICH SURVIVED**: split-half over the rounds, the arm's pooled
  rate across the whole ladder agreeing between first and second halves within **15 points**.
  ⛔ **NOT K15's anchors rule, which is withdrawn** — on a ladder built to have two humps and three
  wings, an anchors-against-median control presumes the flatness it is testing (M63).

  ⛔ P1 carries over and is checked per cell: arrivals **<= 0.6** in every primer cell. The top
  cell is the budget risk — 160 + ~192 overhead + 98 probe = **450 ms** against the 500 ms burst
  (K4/C516) — and **P1 is what decides that, not the arithmetic**. A cell that climbs to >= 0.8 got
  its own burst and is reported rather than interpreted.
  ⚠ The no-primer cell is **informational**: `nexwatch` is not in `FRESH_BURST_ZERO`, and C514 is
  why that set names exactly one arm.

⛔⛔ **WHAT K17 CANNOT DO.** A null in both windows is **not** *no hump on `nexwatch`* — the grid
argument cuts the same way it did for K15: a hump at some third lead time is unmeasured, and 14
cells across 40-160 ms leave most of the range untouched. ⛔ And it locates a hump; it explains
nothing. The mechanism is still open and is unit 1.

⚠ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K18 — IS THERE A SECOND FEATURE ABOVE 80 ms? (written before its capture)

C520 found a second elevated region on `nexwatch` at **135-140 ms**, reproduced in both seeds
(62-88% against 0-25% either side) — and **the band aimed at it did not fire**, so it is a lead and
nothing is claimed about it. ⛔ `indala` and `keri` have **never been measured above 80 ms**:
K14/K15/K16 all stop there. So the question the bench can actually answer is whether *anything* is
up there on the two arms whose profile below 80 ms is best known.

⛔⛔ **THIS LADDER ASKS *IS THERE A SECOND FEATURE*, NOT *WHERE IT IS*.** M64 and M66 each cost a
verdict to a location clause — one read a shape off its argmax, the other pooled a window on the
assumption the feature would fill it. ⇒ **Location is REPORTED and never tested here.** One ladder
on two arms cannot establish a period, and saying so before the capture is the only way that
sentence is worth anything afterwards.

**The ladder**: a positive-control cell at **65 ms**, then **100..170 ms in 5 ms steps** (15 cells).
Budget at the top: 170 + ~192 overhead + the probe (33 ms for `indala`, 80 ms for `keri`) = 395 and
442 ms against the 500 ms burst — ⛔ and **P1 decides that, not the arithmetic** (C516).

  ⭐ **V1 — A SECOND FEATURE EXISTS IN 100-170 ms.** Per arm, ALL THREE:
  **(a)** pooled(110..160) − pooled(100,105,165,170) >= **25 points**, in **EACH seed**;
  **(b)** a contiguous run of **>= 2 adjacent cells inside 110-160, each >= 50%**, present in
  **BOTH seeds** and overlapping in at least one cell;
  **(c)** that run's pooled rate >= **30 points** above the same four wing cells, on the cells
  pooled across both seeds.
  **V2 — IT DOES NOT** ⇒ (a) fails in both seeds *and* the window sits within **10 points** of its
  wings. between ⇒ no verdict for that arm.

  ⛔⛔ **THE WINGS ARE FIXED BY POSITION, NOT BY OUTCOME** — the four cells at the ladder's edges,
  named here before the capture. M66's disease is a window whose contents are chosen after the
  fact, and a run clause searched against outcome-defined wings would be the same thing wearing a
  run clause.

  ⚠ **AND THE >= 50% CELL THRESHOLD CANNOT CARRY THIS ALONE, WHICH IS WHY IT IS NOT ASKED TO.**
  `indala`'s baseline is ~50%, so *a cell at >= 50%* is nearly a coin flip on that arm and would
  fire on noise about two thirds of the time. **The gap clauses (a) and (c) are what discriminate**;
  the run clause only insists the rise is contiguous rather than one scattered cell (M62), and the
  both-seeds overlap is what stops eleven candidate positions being eleven chances to be wrong.

  ⛔ **Gates first, either one ends it**: pooled over every cell **< 15%** is NO POWER; K16's
  split-half over the rounds differing by **> 15 points** is a drifted run; P1 **<= 0.6** per cell.
  ⚠ **The 65 ms control cell is INFORMATIONAL and is NOT a gate.** On a bench that gave one arm
  88/60/38/75% in an evening (C497) no absolute-level control can certify a session — it is there
  so a session that has gone strange is visible, not so a band can lean on it.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K19 — HOW MANY FEATURES ARE THERE, AND DOES THE PROFILE EVER COME BACK DOWN?

K18 measured `indala` and `keri` above 80 ms and **its band mis-fired**: the wing cells it fixed at
the ladder's edges landed ON an elevated region, so it scored a three-cell feature at 140-150 ms as
*no second feature here* (C521, M67). ⛔ **The captures are good and the criterion was not**, so
this re-runs them with the reference built the way M67 prescribes.

⛔⛔ **NO DESIGNATED WINGS. THE REFERENCE IS THE LADDER'S OWN BODY.** M67's whole content is that a
wing has to be *justified*, not *located* — and 85-90 ms is NOT justified either, though it is
tempting: `indala` and `keri` are 17-25% and 8-17% at 80 ms in K15/K16, but that is one cell 5 ms
away, on profiles whose features are 5-15 ms wide. ⇒ **"elevated" here means elevated relative to
THIS arm's own median across THIS ladder**, which presumes nothing about where the quiet is. ⚠ It
buys that by measuring *relative* structure only: on an arm sitting high everywhere, a feature
means *high for this arm*, and the write-up must say so rather than implying an absolute level.

**The ladder**: an informational control at **65 ms**, then **85..200 ms in 5 ms steps** (24 cells),
one session per cell per round, shuffled within every round with the seed recorded (M60). Two seeds
per arm at `--reps 8`.

⛔⛔ **THE INSTRUMENT'S CEILING, NAMED BEFORE THE RUN SO A NULL AT THE TOP IS NOT MISREAD.** The
burst is `LF_TAG_BURST_TARGET_MS` = 500 ms (K4, confirmed from the air by C516). At a 200 ms primer
`keri` spends 200 + ~192 overhead + 80 probe = **472 ms** of it, and K12's re-fixed ladder already
saw **P1 fail at 220 and 240 ms**. ⇒ **~200 ms is the reachable maximum on this rig**, and P1 —
not this arithmetic — is what confirms each cell (C516).

  **W1 — THE PROFILE HAS MORE THAN ONE SEPARATED FEATURE.** A *feature* is a maximal contiguous
  run of **>= 2 cells each at or above (this arm's ladder median + 25 points)**, present in **BOTH
  seeds** and overlapping in at least one cell. **W1 fires** when there are **>= 2** such features
  separated by **>= 2 cells each at or below (median − 10 points) in both seeds**.
  **W1 REFUTED** ⇒ at most one such feature. between ⇒ reported, no verdict.
  ⛔ Location is REPORTED, NEVER TESTED — M64 and M66 each cost a verdict to a location clause, and
  W1 is deliberately a claim about COUNT and SEPARATION, which is what this ladder can carry.

  **W2 — DOES THE PROFILE RETURN TO A FLOOR INSIDE THE LADDER?** ⇒ at least one of the top four
  cells (185, 190, 195, 200) is at or below **(median − 25)** in **both** seeds. ⛔ **W2 FAILING IS
  A RESULT, NOT A NUISANCE**: it says the ladder is still too narrow and **no wing at its top edge
  is licensed for any future band** — which is M67 turned into something a run can actually check
  instead of a rule a designer has to remember.

  ⛔⛔ **PERIODICITY IS NOT TESTED AND CANNOT BE HERE.** If the ~55-65 and ~135-160 ms regions are
  one period apart, the next would be near 215-240 ms — **outside the burst ceiling above**. ⇒ A
  null at the top of this ladder is **not** evidence against a period, and no periodicity claim may
  be built on this run. Said before the capture because the temptation will be there afterwards.

  ⛔ Gates first, unchanged and either one ends it: pooled over every cell **< 15%** is NO POWER;
  K16's split-half over the rounds differing by **> 15 points** is a drifted run; **P1 <= 0.6** per
  cell. ⚠ The 65 ms cell is INFORMATIONAL, not a gate (C497 — no absolute-level control can certify
  a session on this bench), and it is excluded from the median so the control cannot move the
  reference the bands are read against.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K20 — `indala`'s NOTCHES, ON FRESH SEEDS (written before its capture)

K19's W1 came back REFUTED for `indala` and **the refutation was worthless**: its median is 75%, so
*median + 25* was the ceiling and the band could not have fired whatever the data did (M68). ⭐ M68's
own prescription is that **an arm should be given the detector its own level can support** — and on
a profile sitting high the detectable feature is a **notch**, not a hump.

⛔⛔ **THE POWER COMPUTATION, DONE BEFORE THE BAND AND NOT AFTER IT.** This is the step whose
absence was M68.

| arm | median (K19) | *median − 25* | *median + 10* | can this detector fire? |
|---|---|---|---|---|
| `indala` | **75%** | 50% | 85% | ⭐ **yes — both thresholds sit well inside the arm's range** |
| `keri` | 25-31% | **0-6%** | 35-41% | ⛔ **no — the notch threshold is the floor** |

⇒ **K20 is `indala` ONLY, and that is a decision made from the arithmetic rather than from a
result.** `keri` needs the forward detector, which is exactly what K19 gave it. ⚠ C514 stands: this
is one arm and nothing here generalises off it.

⚠⚠ **DISCLOSED, AND IT IS WHAT MAKES THIS WORTH RUNNING: I HAVE ALREADY SEEN K19's `indala`
PROFILE.** Its low cells sat near 90, 120-125, 165-175 and 200 — observed **post-hoc**, on caps that
have now been looked at. ⛔ **So K20 must NOT be scored on `caps/k19_indala_s{53,67}.json`**;
scoring a band on the data that suggested it is fitting, whatever the band says. ⇒ **K20 is a
PRE-REGISTERED REPLICATION on FRESH SEEDS**, which is the correct move after a post-hoc
observation and is the whole of its value.

**The ladder**: unchanged from K19 — a 65 ms informational cell excluded from the median, then
**85..200 ms in 5 ms steps**, two fresh seeds at `--reps 8`.

  **X1 — `indala` HAS MORE THAN ONE SEPARATED NOTCH.** A *notch* is a maximal contiguous run of
  **>= 2 cells each at or below (median − 25)**, present in **BOTH seeds** and overlapping in at
  least one cell. **X1 fires** with **>= 2** notches separated by **>= 2 cells each at or above
  (median + 10) in both seeds**. **X1 REFUTED** ⇒ at most one. between ⇒ reported, no verdict.
  ⛔ COUNT and SEPARATION only. Location is reported and never tested — M64 and M66 each cost a
  verdict to a location clause and K19 did not repeat it.

  **X2 — DOES THE PROFILE RETURN TO ITS BODY AT THE LADDER'S TOP?** ⇒ at least one of 185, 190,
  195, 200 at or above **(median + 10)** in **both** seeds. ⛔ **X2 failing is a result**: it says
  this ladder's top edge licenses no wing for `indala` either, the same teeth W2 had on `keri`.

  ⛔ Gates unchanged and either ends it: pooled **< 15%** is NO POWER; K16's split-half over
  **15 points** is a drifted run; **P1 <= 0.6** per cell. ⚠ The 65 ms cell is informational and
  excluded from the median.

  ⛔⛔ **PERIODICITY REMAINS UNTESTABLE** — `LF_TAG_BURST_TARGET_MS` = 500 stops the ladder near
  200 ms (C516, and K12 saw P1 fail at 220). A null at the top is not evidence against a period.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K21 — DO `indala` AND `keri` HAVE ONE PROFILE OR TWO? (written before its capture)

`rfid-tools` `QUEUE.md`, committed before this section existed, makes *a criterion for the
interleaving* the next hands-off unit: K19 put `keri`'s features at **95-105** and **140-150** ms
and K20 put `indala`'s notches at **115-130** and **165-175** ms, and the queue's own note says the
two arms' structure **interleaves** — consecutive same-type regions ~45-50 ms apart on each arm,
the arms offset from each other by ~20 ms. It is labelled post-hoc there, and this is its band.

⛔⛔ **BUT THE QUEUE'S FRAMING CONTAINS THE ERROR, AND IT IS THE M62 FAMILY AGAIN.** *Interleaving*
was read off two profiles that were scored with **OPPOSITE DETECTORS** — `keri` with a forward one
(its median is 25-31%, so only a hump is detectable) and `indala` with an inverse one (median 75%,
so only a notch is). ⭐ **If the two arms shared ONE profile, that is exactly what you would see**:
`keri`'s humps would land on the shared profile's peaks and `indala`'s notches on its troughs, and
the two sets would interleave along the ladder **by construction, with no offset in anything
physical.** The observed numbers are consistent with it — `indala`'s 115-130 notch sits inside the
110-135 gap between `keri`'s two humps. ⇒ **The question worth a capture is not *where* the regions
are. It is whether there is one profile here or two**, and *interleaved* is the reading that has to
earn its place against the simpler one.

⛔⛔⛔ **AND THE BANKED K19/K20 CAPS CANNOT ANSWER IT — A DEFECT IN THE LADDER ITSELF, FOUND BY
ASKING WHAT THE TWO ARMS SHARE (M69).** `k12()` built ONE shuffled `plan` and ran **every arm
through it**, so both arms saw the identical cell→position mapping in every round. M60 established
that **position is a variable** on this bench. ⇒ any residual position effect enters BOTH arms'
profiles **identically** and manufactures a positive cross-arm correlation out of nothing. It does
not touch K19's or K20's verdicts — those are within-arm — but it is fatal to this one.
⇒ `--per-arm-shuffle` gives each arm its own derived, recorded seed. **K21 runs with it, and no
cross-arm correlation may be computed on a cap taken without it.**

### ⛔ THE POWER COMPUTATION, DONE BEFORE THE BAND (M68), ON BANKED CAPS

⚠ Only the **per-arm reliability** is taken from banked data. ⛔ The cross-arm correlation — the
thing Y1 scores — **was not computed on them**, and must not be, for the reason above and because
it is the answer.

| arm | seed-to-seed Spearman over the 24-cell ladder | sd across cells |
|---|---|---|
| `keri` (K19 s53 vs s67) | **+0.786** | 32.4 / 35.3 |
| `indala` (K19 s53 vs s67) | **+0.817** | 32.3 / 32.7 |
| `indala` (K20 s89 vs s101) | **+0.637** | 37.0 / 28.4 |

⭐ **Both arms have a profile that replicates, and that is a new quantitative fact** — K19 and K20
only ever checked whether categorical features overlapped, never whether the whole profile did.
⚠ And it is a little WORSE than binomial noise alone allows: resampling one fixed profile twice at
n=8 gives a median Spearman of **+0.89** (`keri`) and **+0.85** (`indala`), 5th percentiles +0.80
and +0.75, so `keri`'s +0.786 sits at that floor and `indala`'s K20 pair at +0.637 is below it.
⇒ **there is session-to-session movement beyond the counting noise**, which is C497's wander
appearing in a new statistic. Disattenuation must therefore use the MEASURED reliability, not the
simulated one.

**The critical value, from a 20,000-draw permutation null on a real profile's tie structure**
(24 cells): |r| = **0.342** at p=0.10, **0.402** at p=0.05, **0.520** at p=0.01.

⇒ **What the design can detect.** With per-arm reliabilities near +0.79 and +0.82, a TRUE shared
profile would show a cross-arm Spearman of about sqrt(0.79 x 0.82) ≈ **+0.80**, and true
independence about **0**. Both pre-registered thresholds below therefore sit well inside what this
ladder can produce, which is the check whose absence WAS M68.

⇒ ⭐ **AND Y1's OWN POWER, SIMULATED BEFORE THE CAPTURE** (2,000 draws per row, two runs required
to agree, ±0.52):

| the truth | SHARED | OPPOSED | no verdict |
|---|---|---|---|
| one shared profile | **77%** | 0% | 23% |
| two independent profiles | 1.9% | 2.4% | **96%** |
| anti-aligned (`keri` = 100 − `indala`) | 0% | **77%** | 23% |
| one profile offset by 20 ms | 2.0% | 0.1% | **98%** |

⭐ **Y1 is properly powered and its errors are symmetric and small** — and the last row is the one
that makes it honest: a real 20 ms offset lands in NO VERDICT 98% of the time rather than being
dressed up as either answer. ⚠ `--reps 12` would take the two live rows to 90%, at 1.5x the
capture; **8 is kept** so the ladder matches K19/K20 and the cost stays inside one tick.

### THE BANDS

**The ladder**: unchanged from K19/K20 — a 65 ms informational cell excluded from everything, then
**85..200 ms in 5 ms steps** (24 cells), `--reps 8`, two fresh seeds (**113** and **127**; 53/67
and 89/101 are spent), **both arms in each run, shuffled independently of each other.**

  **Y1 — ONE PROFILE OR TWO.** Spearman r between `indala`'s and `keri`'s cell rates across the
  24 cells, computed **separately in each seed's run** and required to agree.
  - r >= **+0.52** in BOTH runs ⇒ **SHARED** — the arms track each other, and *interleaving* is an
    artifact of opposite detectors on arms with different medians. The regions stay where K19/K20
    put them; what dies is the claim that they are offset from each other.
  - r <= **−0.52** in BOTH runs ⇒ **OPPOSED** — one arm is high where the other is low. This is the
    strong form of the interleaving reading and it would be a real finding.
  - anything else, or the two runs disagreeing in sign ⇒ ⛔ **NO VERDICT**, reported with both
    numbers. ⚠ The threshold is the p=0.01 permutation critical value, used symmetrically on
    purpose: no mechanism predicts a sign, so neither direction gets the cheaper test.

  **Y2 — WITHDRAWN BEFORE ITS CAPTURE, AND THAT IS THE POINT (M70).** Y2 was written as
  *cross-correlate the arms at lags 0, ±5 .. ±25 ms; fire when the best NON-ZERO lag beats lag 0
  by >= 0.30 in both runs with the sign agreeing.* ⛔ **It was then simulated against a null of
  two INDEPENDENT smooth profiles and it FALSE-FIRED 41% OF THE TIME** — the median null gain is
  **+0.37**, already above the threshold, because lag 0 is one correlation and *the best of ten
  lags* is an order statistic. Raising the bar to a 0.3% false-fire rate needs a gain of **1.10**.

  ⛔⛔ **AND AT THAT BAR IT HAS NO POWER: a REAL 20 ms offset fires it 0.3% of the time.** The
  argmax version is unusable at either end. A fixed, signed, single pre-registered lag (**+4
  cells**, the direction the observation names — `keri`'s structure below `indala`'s) is better
  but still not enough: **37% power at a 5% false-fire rate, 18% at 2%.** ⇒ **A non-firing Y2
  would carry no information**, which is the K8 NO POWER trap this project has now named five
  times. ⇒ **Y2 is withdrawn as a verdict.** The lag table is still PRINTED, labelled
  exploratory, with its power beside it, so that nobody reads its silence as evidence.

  ⭐ **This is the first band in this project withdrawn BEFORE its capture rather than after it**,
  and it is the QUEUE's own prescription — *the repair is not a better rule, it is a check run
  against the design before the capture* — executed. M62, M63, M64, M66, M67 and M68 were all
  pre-registered and all six were discovered to be broken by the data they were meant to judge.
  **M70 is the first one found by the simulation instead**, and it cost no bench time at all.

  ⇒ **THE OFFSET IS THEREFORE NOT MEASURABLE ON THIS LADDER AT THIS n, AND THAT IS THE ANSWER TO
  GIVE IT.** It is not that the arms are not offset; it is that a 24-cell ladder at n=8 cannot
  tell a 20 ms shift from nothing. Reaching it needs either a finer ladder or many more reps, and
  the arithmetic for that belongs to whoever runs it next.

  **Y3 — THE REPLICATION OF THE REGIONS THEMSELVES**, carried over unchanged so the run is not
  wasted if Y1 lands in its no-verdict band. K19's W1 detector on `keri` (median + 25, runs of
  >= 2, in both seeds) and K20's X1 detector on `indala` (median − 25, inverse) re-scored on these
  fresh seeds. ⭐ Y3 is the first time either region set is tested **at a named location** — W1 and
  X1 were deliberately COUNT-and-SEPARATION only — and it is legitimate here precisely because
  K19/K20 named the cells first: `keri` high in 95-105 and 140-150, `indala` low in 115-130 and
  165-175. **Y3 fires** when each of those four regions contains >= 2 cells meeting its arm's
  detector in BOTH fresh seeds. ⛔ No single cell can decide it: four regions x two cells x two
  seeds is sixteen cell-observations, and one region falling short refutes it.

  ⛔ Gates unchanged and any one ends it: pooled over every cell **< 15%** is NO POWER; K16's
  split-half over the rounds **> 15 points** is a drifted run; **P1 > 0.6** in any cell means a
  cell restarted its burst. ⚠ The 65 ms cell is informational and is excluded from the median, the
  correlations and the gates' pooled figure alike.

  ⛔⛔ **PERIODICITY IS STILL NOT TESTED AND STILL CANNOT BE.** The ~45-50 ms spacing between each
  arm's own two regions is **not** a period claim and none may be quoted off this run: a third
  region would sit near 215-240 ms, outside `LF_TAG_BURST_TARGET_MS` = 500 (C516, and K12 saw P1
  fail at 220). ⚠ And ~49 ms is also **3 frames** of the 2048-sample frame these two arms share,
  which C520 gives no licence to invoke — an arm-independent spacing and a frame-locked one are not
  separable on two arms with the same frame.

⚠⚠ **DISCLOSED**: I have read the prose describing K19's and K20's profiles, so the region
locations in Y3 are known to me and Y3 is a REPLICATION, not a discovery. ⛔ And my own reading
before writing this — that the observed numbers look like ONE profile — means **a SHARED verdict is
the one foreknowledge points at, and is worth less than an OPPOSED one.** Said here, before the
capture, rather than in the write-up.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K22 — THE 20-40 ms REGION, WHICH NOTHING HAS EVER MEASURED (written before its capture)

`rfid-tools` `QUEUE.md`'s third hands-off unit. **K14/K15/K16 stopped at a 40 ms primer and K19/K20
started at 85, so 20-40 ms has never been looked at on either arm** — and the two ladders only just
meet. It is the last unmeasured region under the burst ceiling.

⭐⭐ **AND IT IS THE FIRST BAND IN THIS PROJECT WHOSE REFERENCE REGION IS JUSTIFIED BY INDEPENDENT
PRIOR MEASUREMENT** — which is exactly what M67 demanded and what K19 could only answer by
abandoning wings altogether. The 40 and 45 ms cells were measured in **two independent sessions on
both arms**, and all eight readings are at or below 17%:

| cap | `indala` 40 / 45 | `keri` 40 / 45 |
|---|---|---|
| `k15_indala_keri_s91` | 12% / 17% | 0% / 8% |
| `k16_indala_keri_s137` | 8% / 12% | 12% / 8% |

⇒ **a FLOOR of 0-17%, four readings per arm, from captures this band is not scored on.** That is a
wing with a warrant, not a wing at an edge.

### ⛔ THE POWER COMPUTATION, BEFORE THE BAND (M68), AND IT KILLS THE SHARPEST PREDICTION

**The frame-locked notch is the prediction this region exists to test, and it CANNOT BE TESTED.**
`gproxii`'s 5 ms notch sits at 60 ms = **1.22 of its 6144-sample frame**; 1.22 frames of the
2048-sample frame `indala` and `keri` share is **20 ms**, the bottom of this ladder. ⛔ **But a
notch needs a BODY to be notched out of, and the measured body there is the 0-17% floor above.** On
`keri`, *median − 25* would land below zero — K20's own power table, in a new place. ⇒ **the
frame-locked notch is declared UNTESTABLE here, in advance, rather than measured and mis-read as
absent.** ⚠ C520 already refuted the frame reading for the HUMP on `nexwatch`; this says the bench
cannot extend that to the NOTCH, and a null at 20 ms must not be quoted as if it had.

⭐ **The FORWARD detector does have power, and it is the floor above that gives it that.** Anything
elevated between 20 and 40 ms stands against 0-17% measured four times per arm. ⇒ K22 asks the one
question this region can answer.

⇒ ⭐ **AND Z1's OWN POWER, SIMULATED BEFORE THE CAPTURE (M70), 20,000 draws:**

| the truth in 20-35 ms | Z1 fires |
|---|---|
| the measured floor everywhere (p = 0.10) | **0.00%** |
| the floor at the top of its measured range (p = 0.17) | **0.00%** |
| ONE cell at 90%, the rest floor | **0.01%** |
| two adjacent cells at 50% | 16% |
| two adjacent cells at 60% | 47% |
| two adjacent cells at **80%** | **96%** |

⭐ **Its specificity is as good as a band here has ever had** — the absolute threshold against a
measured floor gives a false-fire rate of zero to three decimal places, and a lone 90% cell fires
it 0.01% of the time, which is M62's disease designed out rather than warned about.
⛔⛔ **BUT ITS POWER IS A CLIFF: 96% at 80%, 47% at 60%, 16% at 50%.** ⇒ **A REFUTED Z1 means
*there is no feature here as strong as the hump at 50-65 ms* (which reaches 75-96%), and it does
NOT mean the region is flat.** That sentence is the verdict's whole content and it must be carried
with it; a weaker feature would be missed more often than not.

### THE BANDS

**The ladder**: **20..80 ms in 5 ms steps** (13 cells), the top five of which (60-80) overlap
K15/K16 and act as a continuity check, plus the usual no-primer control. Two fresh seeds
(**131** and **149**), `--reps 8`, **`--per-arm-shuffle`** (M69), both arms.

  **Z1 — IS THERE ANY STRUCTURE BELOW 40 ms?** ⇒ a maximal contiguous run of **>= 2 cells among
  20,25,30,35 each at or above 40%**, present in **BOTH seeds** and overlapping in at least one
  cell. ⛔ **The 40% threshold is ABSOLUTE and is taken from the prior floor, not from this
  ladder's median** — a median reference here would be dragged up by the 50-65 ms hump the ladder
  deliberately contains, and that is M68's disease with the sign flipped. **Z1 REFUTED** ⇒ no such
  run. between ⇒ reported.

  **Z2 — THE CONTINUITY CHECK, AND IT IS A GATE ON Z1 RATHER THAN A FINDING.** The hump K15/K16
  measured must be there: **at least two of 50, 55, 60, 65 at or above 50% in both seeds.** ⛔ If
  Z2 fails, the session does not reproduce a result taken twice already and **Z1 is not read at
  all** — a new region measured on a bench that cannot repeat a known one is not evidence.
  ⚠ Z2 is NOT a claim; it is this ladder's version of the anchors rule M63 withdrew, and it is safe
  here only because the anchor cells are ones prior runs measured HIGH rather than ones this band
  assumes are quiet.

  **Z3 — WHERE THE FLOOR ACTUALLY STARTS.** Reported, never tested: the lowest cell at or above
  40% and the highest cell below it, in each seed. ⛔ **A boundary read off a ladder is an argmax
  (M64/C504), so it is carried as a RANGE across the two seeds and never as a number.**

  ⛔ Gates unchanged and any one ends it: pooled over every cell **< 15%** is NO POWER; split-half
  over the rounds **> 15 points** is a drifted run; **P1 > 0.6** in any cell means a cell restarted
  its burst. ⚠ **P1 is the one to watch at the bottom of this ladder** — a 20 ms primer is the
  shortest field-up this design has ever asked for, and if it is too short to hold the burst the
  arrivals control is what says so. **A P1 failure at 20-25 ms is a result about the instrument's
  floor**, not a nuisance, and it must be reported as the reason the region stayed unmeasured.

  ⛔⛔ **NO PERIODICITY, AGAIN.** Extending each arm's ~45-50 ms region spacing downward is
  forbidden here for the reasons K19/K20 give, and there is a new one: it does not even work
  descriptively — `indala`'s notches extrapolate down to 65-80 ms, where it is measured at
  **83-96%**. ⛔ That is a reason not to quote the spacing, not a refutation of anything, because
  no band ever tested it.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K23 — WHERE DOES THE 20-30 ms REGION START? (written before its capture)

K22's **Z3 is what forces this**: the lowest cell at or above 40% was **the ladder's FIRST cell,
20 ms, in every seed on both arms** (C525). The profile is already elevated where the measurement
begins, so the region's downward extent is unknown and **no wing at the bottom edge of any ladder
is licensed for these arms** — W2's teeth (C522) earned at the other end.

⭐ **The top edge IS known and is not what this asks about**: `indala` is 100/88/100% and
100/75/62% at 20/25/30 and 25/38% at 35, so the region ends between 30 and 35 ms. **K23 asks only
about the other side.**

### ⛔ WHAT THE INSTRUMENT ACTUALLY ALLOWS, CHECKED MECHANICALLY AND NOT ASSUMED

⭐ `lf read -s N` was run directly at N = 1250, 625, 250 and **125** with nothing armed: all four
return (`Got 125 samples`), so a primer of **1 ms** is reachable and the ladder is not bounded by
the client. ⚠ That check raised a field with nothing emulating and therefore carries **no rate
information at all** — it is an instrument check, not a pilot.

⛔⛔ **BUT THE AXIS HAS A FLOOR THAT THE PRIMER CANNOT REACH BELOW, AND IT IS NOT 1 ms.**
`K12_OVERHEAD_MS` = **192**: elapsed-at-probe ≈ primer + 192 ms of measured field-up overhead. So
this whole ladder spans **193 to 257 ms** of elapsed time, and 1 ms of primer is 193 ms of
elapsed. ⇒ **"below 20 ms" means "below 212 ms of elapsed", and the design cannot go below 192
whatever the primer is.** ⚠ Nothing has measured that overhead's JITTER, so **the step stays at
K22's 5 ms** rather than being refined — a 2.5 ms grid would presume a stability nobody has
measured, and C516 is the standing reason not to trust a host-side number about the air.

⛔⛔ **AND THE NO-PRIMER CONTROL IS NOT `primer = 0`.** It is a FRESH BURST — arrivals 1.0 against
0.50 — a different burst identity, not the bottom of this axis (C512 measured it as its own
condition). It is printed, it is informational, and **it may not be used as this ladder's lowest
cell.**

### THE BANDS

**The ladder**: **1, 5, 10, 15, 20, 25, 30** ms — the new region plus the three cells K22 measured
high, so the known part has to reappear — then **50, 55, 60, 65** as the continuity anchor, plus
the no-primer control. Two fresh seeds (**151** and **167**), `--reps 8`, `--per-arm-shuffle`
(M69), both arms. ⚠ The 1→5 ms step is 4 ms and the rest are 5; said here rather than implied.

  **V1 — DOES THE PROFILE COME DOWN INSIDE THIS LADDER?** ⇒ at least one of **1, 5, 10** ms at or
  below **40%** in **BOTH** seeds. ⛔ The 40% threshold is the ABSOLUTE one K22 used, carried
  unchanged so the two ladders are commensurable, and it is justified by the same independent
  floor (`k15`/`k16` at 40-45 ms, 0-17%).
  - **V1 fires** ⇒ the region has a bottom edge inside this design. Its LOCATION is reported and
    never tested (M64).
  - ⭐⭐ **V1 FAILS ⇒ THAT IS THE MORE INTERESTING RESULT AND IT RETIRES THIS KNOB DOWNWARD.** The
    profile would be elevated at every lead time the primer can reach, down to 1 ms — and since
    1 ms is 193 ms of elapsed against the design's own 192 ms floor, **it would mean the region's
    bottom is not below 20 ms at all but inside the field-up overhead, where no primer can go.**
    ⇒ The next move would then be to attack the overhead, not the ladder. Said before the capture
    so the null is not read as *nothing there*.

  ⛔ **V1's POWER, SIMULATED BEFORE THE CAPTURE (M70), and the relevant null is *the region
  continues at the level K22 measured*:**

  | the truth at 1, 5 and 10 ms | V1 fires |
  |---|---|
  | 100% (the ceiling `indala` shows at 20 ms) | **0.000** |
  | 95% | **0.000** |
  | 88% (`indala`'s 25 ms level) | **0.000** |
  | 75% | 0.006 |
  | 62% | 0.139 |
  | a real edge — 20% / 50% / 88% | **0.930** |
  | a real edge — 10% / 10% / 50% | **1.000** |

  ⭐ **Against the null that matters it is zero to three decimal places, and its power at a real
  edge is 93-100%.** ⚠⚠ **ITS ONE WEAKNESS, STATED RATHER THAN DISCOVERED: at a true level of 62%
  it fires 14% of the time from counting noise alone.** V1 is a claim about the **measured** level
  reaching 40%, and on a profile that has merely sagged it will sometimes say *edge*. ⇒ **A firing
  V1 licenses *the profile comes down*, never *it reaches a floor*.**

  **V2 — THE CONTINUITY GATE, K22's Z2 UNCHANGED.** At least two of 50, 55, 60, 65 at or above
  **50%** in both seeds, or **V1 is not read at all**. ⚠ Not a claim; it is safe as an anchor only
  because prior runs measured those cells HIGH (M63's distinction).

  **V2b — AND THE NEW-REGION CELLS MUST REAPPEAR TOO, ON THE ARM THEY WERE MEASURED ON.** At least
  two of **20, 25, 30** at or above **50%** in both seeds. ⛔ **If V2b fails, V1 is not read
  either**: a ladder that cannot reproduce C525's own region, measured an hour earlier, is not
  evidence about what lies below it. ⭐ This is the first continuity gate here that checks the
  finding the run is EXTENDING rather than only an older one, and K22 should have had it.

  ⛔⛔ **AMENDED AFTER ITS FIRST RUN, AND THE AMENDMENT IS A DEFECT REPORT ON ME — V2b IS
  `indala`-ONLY (M71).** The first version applied it to **both** arms. **But C525 established
  that region on `indala` and its Z1 was REFUTED for `keri`**, whose 20 ms cell was elevated and
  **stood alone**, reported and not counted. ⇒ V2b on `keri` gates an arm on **another arm's
  finding**, and on the first run it did exactly the damage that implies: `keri` passed all four
  instrument gates and V2, then had V1 **blocked** by a gate asking for a region C525 had already
  said was not there. **It blocked the very arm the re-run existed to measure.**
  ⚠ **The correction is licensed by the PRIOR record and not by the new data** — C525's refutation
  was published before K23 was written, so the error is demonstrable without looking at K23 at all.
  ⛔ **But those caps have now been looked at, so `keri` re-runs on FRESH seeds (199, 211)** and
  `caps/k23_keri_s{181,193}.json` are banked evidence, not the scored run.
  ⇒ **For `keri`, V2 — the 50-65 ms hump — is the ONLY continuity gate available**, because it is
  the only prior finding that holds on that arm, and the write-up must say so rather than implying
  two gates were passed.

  **V3 — reported, never tested**: the lowest cell at or above 40%, as a RANGE across the two
  seeds and never as a number (M64, C504).

  ⛔ Gates unchanged and any one ends it: pooled over every cell **< 15%** is NO POWER; split-half
  over the rounds **> 15 points** is a drifted run; **P1 > 0.6** in any cell. ⚠⚠ **P1 IS THE ONE
  TO WATCH AND IT IS LOAD-BEARING HERE.** A 1 ms primer is by far the shortest field-up this
  design has ever asked for, and if it is too short to keep the probe inside one burst the
  arrivals rise toward 1.0. **A P1 failure at 1 or 5 ms is a RESULT about the instrument's floor**
  — it would say the primer stops being a primer down there — and it must be reported as the
  reason those cells cannot be measured, not as a nuisance.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K24 — `keri`'s BOTTOM EDGE, WITH THREE SEEDS AND THE SELECTION RULE PINNED FIRST

`keri` has now lost **three** K23 session pairs and its bottom edge is still unmeasured. ⛔ **Not
one of those losses is evidence about `keri`**, and the reasons are on the record:

| pair | why it was not read |
|---|---|
| s151 / s167 | s167 **DRIFTED** (split-half 15.9 against a 15-point gate) |
| s181 / s193 | ⛔ **my gate was wrong** — V2b asked for a region C525 had REFUTED on this arm (M71) |
| s199 / s211 | s211 **DRIFTED** (20.5) |

⭐ **And C527 prices the first and third: the drift gate fails on pure counting noise 14-16% of the
time per run, 26-30% per pair.** Two of three pairs lost to it is unremarkable at that rate
(P(>= 2 of 6 runs over the gate) = 0.26). ⇒ **the fix is more seeds, not a different gate**, and
the gate is deliberately left alone (C527: re-pointing it is the operator's decision).

⛔⛔ **THE SELECTION RULE, PINNED BEFORE THE CAPTURE — THIS IS THE WHOLE REASON K24 EXISTS AS ITS
OWN UNIT RATHER THAN A THIRD ATTEMPT AT K23.** Three fresh seeds (**223**, **227**, **229**), one
arm, the K23 ladder and **the K23 bands unchanged**.

  **S — WHICH TWO RUNS ARE SCORED.** Of the three, score the **two with the lowest split-half |Δ|**,
  and score them under V1/V2/V3 exactly as K23 defines them. ⛔ **If fewer than two runs pass the
  gate at all, the unit returns NO VERDICT and reports how many passed** — the rule may not reach
  down to a failing run to make up a pair.
  ⚠⚠ **THE ONE DEPENDENCE, STATED RATHER THAN DISCOVERED**: the split-half Δ is computed from the
  **same scores the bands read**, so choosing runs by Δ is *not* perfectly independent of V1. A run
  with an unusually low Δ is, very weakly, one whose two halves agree — which is not the same thing
  as one whose 1 and 5 ms cells are low. ⇒ The rule is declared in advance, which is what keeps it
  honest, and **the verdict must say that two of three were selected.**
  ⛔ **The bands are NOT re-tuned.** A re-run at fresh seeds is replication; a re-run with new
  thresholds is fitting, and K23's V1 threshold, its 40% absolute level and its power table all
  stand exactly as committed.

  ⭐ **V2b does not apply to `keri` (M71)** — C525 refuted a region there on this arm — so **V2, the
  50-65 ms hump, is its only continuity gate**, and the write-up must say one gate was passed and
  not two.

⇒ **Priced: 93% chance of two usable runs against 71% for a pair**, at ~6 min per seed for one arm.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K26 — `idteck` ON THE LEAD-TIME KNOB, AND WHY `indala224` CANNOT BE (written before its capture)

The last outstanding item on the lead-time list: `idteck` and `indala224` have never been measured
on this knob. ⭐ **One of them can be and one of them cannot, and the scoping is done by arithmetic
before either is armed — the step whose absence was M68.**

### ⛔⛔ `indala224` IS NOT SCOREABLE ON THIS KNOB AT ALL, AND THAT CLOSES IT

Two independent reasons, both from the existing record:

1. **It cannot be scored on *exact*.** Its precision is **0%** — 51 decodes, none carrying our
   credential, at every read length (C502). An *exact* rate would be a flat zero and measure
   nothing.
2. ⭐⭐ **And it cannot be scored on *decoded* either, which is the part that was not obvious.**
   Its marker is `Indala \\(len` — **the SAME marker `indala` uses**, because both go through
   `lf indala demod` (C502). So a *decoded* hit cannot distinguish a 224-bit frame from a 64-bit
   one. ⛔ **And no length-specific marker can rescue it**: C493/C501 measured that the
   demodulator reports a **nonsense length for this arm — 254 to 611 against 224** — so there is
   no string in the output that identifies a 224-bit decode.

⇒ **There is no rate to put on the y-axis.** The item is closed as **not measurable with this
instrument**, not deferred. ⚠ It would become measurable with a marker that keyed on payload
LENGTH in bits rather than on the reported length, which is a `shortread.py` change and a
different unit.

### `idteck` CAN BE, AND ITS PROBE IS CHOSEN BY A PRE-EXISTING RULE RATHER THAN A PILOT

⭐ Precision **100%** — 25 of 25 decodes correct, and 0 wrong in 24 at n=24 (C503) — and a marker
of its own (`IDTECK Tag Found: Card ID`). ⭐ Probe: **`lf read -s 6144`**, which is **3 frames** of
its 2048-sample frame, by C499's standing *2-6 frames, best at 3-4* rule and **not** by a pilot on
this arm. C499 measured it at **9/24 ≈ 38%** there, which is a low baseline — so the FORWARD
detector has power and the inverse one would sit near the floor (K20's power table, a third time).

**The ladder**: **10..200 ms in 5 ms steps — 39 cells**, the full span anything has been measured
over on any arm, plus the no-primer control. One arm, two fresh seeds (**241**, **251**),
`--reps 8`, `--per-arm-shuffle` (M69). ⚠ At a 200 ms primer the burst carries 200 + ~192 overhead +
49 ms probe = **441 ms of 500**; P1, not that arithmetic, is what confirms each cell (C516).

  ⛔⛔ **A1's RUN LENGTH IS 3 AND NOT 2, AND THE REASON IS THE LADDER'S LENGTH — SIMULATED FIRST
  (M70/M73).** A 39-cell ladder gives a 2-cell run detector **many chances**, and its false-fire
  rate on a completely FLAT profile swings with where the median lands on the n=8 grid:

  | flat median | need 2 | **need 3** | need 4 |
  |---|---|---|---|
  | 12% | 1.6% | **0.0%** | 0.0% |
  | 25% | **13.2%** | **0.2%** | 0.0% |
  | 38% | 0.3% | **0.0%** | 0.0% |
  | 50% | **25.8%** | **0.8%** | 0.0% |
  | 62% | **19.0%** | **0.3%** | 0.0% |
  | 75% | 8.6% | **0.1%** | 0.0% |

  ⇒ **need 2 is unusable here at 13-26%** for medians this arm might plausibly have, and **need 3
  holds at or under 0.8% whatever the median turns out to be** — which matters because `idteck`'s
  median on this knob is not known in advance. ⛔ **This is why the same detector was fine on
  K19's 24-cell ladder and is not fine here: the number of chances is part of the detector.**

  **A1 — DOES `idteck` HAVE STRUCTURE ON THIS KNOB AT ALL?** ⇒ a maximal contiguous run of
  **>= 3 cells each at or above (this arm's ladder median + 25 points)**, present in **BOTH** seeds
  and overlapping in at least one cell. **A1 REFUTED** ⇒ no such run. between ⇒ reported.
  ⭐ **Power, simulated against the region widths actually observed on the other arms** (3-5 cells):
  a 3-cell region at 88% fires **69-99%** depending on the median, a 4-cell region **78-100%**, a
  5-cell region **87-100%**; on the full four-region truth A1 fires **95.5%** and on a completely
  flat ladder **0.2%**. ⚠ **The weak corner is a 3-cell region on a 38-50% median at 69-73%**,
  so **a REFUTED A1 means *no region as wide and as tall as the other arms' regions*, not *flat*.**
  ⭐ And the median reference survives regions occupying **36% of the ladder** — checked, because
  four named regions span 14 of 39 cells and M68 is exactly a reference dragged by its own signal.
  ⛔ A 25th-percentile reference was tried instead and is far WORSE: it false-fires **68%** on a
  flat profile, because a threshold 25 points over the quartile is reachable by noise.

  **A2 — IS ITS STRUCTURE WHERE THE OTHER ARMS' IS?** ⭐ This is the cheapest attack there is on the
  mechanism, and it is a legitimate LOCATION test because the locations were named first and have
  now replicated across **three independent seed pairs**: `indala`/`keri` are HIGH in **10-30**,
  **50-65**, **95-105** and **140-150** ms. ⇒ **A2 fires when every region A1 finds has a MAJORITY
  of its cells inside one of those four AND at least two of the four are found.** **A2 REFUTED**
  ⇒ some region A1 finds does not, i.e. `idteck`'s structure is its own.

  ⛔⛔ **A MAJORITY AND NOT ONE CELL, AND THE BREAK-TEST IS WHY.** The first version of A2 accepted a
  single cell of overlap. Simulated against a ground truth whose structure sat **entirely
  elsewhere** (70-80 and 110-120 ms), a wide spilling run touched a named span by accident, counted
  as a match **and escaped being an orphan** — **A2 false-fired 2.8%** where it should never fire.
  **The majority rule takes that to 0.0%.**

  ⭐ **A1 AND A2's POWER, SIMULATED BEFORE THE CAPTURE (2,000 draws per row, both seeds required):**

  | ground truth | A1 fires | A2 fires | wanted |
  |---|---|---|---|
  | **flat, no regions at all** | **0.2%** | **0.0%** | A1 refuted |
  | the four named regions | **95.5%** | **79.8%** | A2 fires |
  | structure entirely ELSEWHERE (70-80, 110-120) | 97.8% | **0.0%** | A2 refuted |
  | the named regions **plus** an extra one | 78.5% | **42.1%** | A2 refuted |

  ⛔⛔ **THE LAST ROW IS A REAL LIMIT AND IT IS STATED IN ADVANCE: A2 FIRING MEANS *the structure
  found is in the named places*, NOT *there is no structure elsewhere*.** On a truth carrying the
  named regions plus an extra one it still fires about 42% of the time, because A1 often fails to
  resolve the extra region as a separate run. ⇒ **A2 can establish that `idteck` shares the other
  arms' locations; it cannot establish that it has no others.**

  ⛔ **A2 tests the HIGH regions only, by arithmetic**: the other arms' LOW regions (120-130,
  165-175) need an inverse detector, and on a ~38% median *median − 25* is near the floor where it
  has no power (K20's table).
  ⚠ **`idteck` shares the 2048-sample frame with `indala` and `keri`, so it can no more separate
  frames from milliseconds than they can** (M65/C520). A2 is not asking that; it asks whether the
  structure is a property of the EMISSION or of the reader, and a third arm agreeing is the
  strongest cheap evidence for the former.

  ⛔ Gates unchanged and any one ends it: pooled **< 15%** is NO POWER; split-half over the rounds
  **> 15 points** is a drifted run — ⭐ and at 39 cells x 8 reps the n per half is **156**, so that
  gate is about **2.7σ** here and fails on noise under 1% (C530/M73, which is why its configuration
  is named beside it); **P1 > 0.6** in any cell.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐ K27 — `nexwatch`'s OWN FULL LADDER, SCORED BY K26's A1 AND NOTHING ELSE

⭐ `nexwatch` is the one arm whose frame differs — **4096 samples against the others' 2048** — so it
is the only one that can separate *the structure is a property of the emission* from *it is a
property of the frame* (M65/C520). ⛔ **But the inventory shows it with only 14 measured cells at
2 seeds**, all from K17's two narrow windows, so it cannot carry a cross-arm band yet. **This is the
capture that fixes that**: the same **10..200 ms at 5 ms, 39 cells** ladder `idteck` ran, two fresh
seeds (**257**, **263**), `--reps 8`, `--per-arm-shuffle`.

  **SCORED BY K26's A1, UNCHANGED AND ALREADY COMMITTED** — a run of **>= 3 adjacent cells at
  median + 25** present in **BOTH** seeds and overlapping. ⭐ Applying an already-pinned band to a
  new arm is replication; nothing here is re-tuned, and A1's simulated numbers carry over because
  they were computed **as a function of the median** (flat-ladder false-fire at or under 0.8% for
  every median from 12% to 75%, and 0.2% on the four-region truth's own flat control).
  ⚠ `nexwatch` is the weakest of the six arms, so its median may sit very low. **That HELPS A1**:
  the lower the median the further *median + 25* is from the ceiling (M68 inverted), and at a
  median of 0% a flat ladder cannot false-fire at all. ⛔ But it also means **a REFUTED A1 here
  must be read as *no region as wide and tall as the other arms' regions*, not as *flat*** — the
  same reading K26 committed to.

  ⛔⛔ **A2 IS NOT APPLIED TO THIS ARM, AND THAT IS DECIDED BEFORE THE CAPTURE.** A2's named-region
  list is the one C533/M76 found unsound — it came from the write-ups and omitted 180-195 ms — and
  the replacement must be derived from `--inventory`, which **this very capture changes**. ⇒ Scoring
  a cross-arm location band on the run that extends its own reference is circular. **A1 only here;
  the cross-arm band comes after, on fresh seeds, against the inventory this run has already fed.**

  ⛔ Gates unchanged: pooled **< 15%** is NO POWER — ⚠ **and that one may well fire on this arm**,
  which would be a result about `nexwatch`'s decode rate at a 3-frame read rather than about the
  knob; split-half over the rounds **> 15 points** (at 39 cells x 8 reps the n per half is **156**,
  so that gate is ~**2.7σ** here and fails on noise under 1% — C530/M73); **P1 > 0.6** in any cell.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K28 — THE COMMON LADDER, WHICH IS THE CROSS-ARM BAND'S REAL PREREQUISITE

⛔⛔ **THE FOUR ARMS HAVE NEVER BEEN MEASURED ON ONE LADDER, AND THAT IS WHY NO CROSS-ARM BAND CAN
BE WRITTEN YET.** Checking the caps mechanically (M76's habit, applied to the design rather than to
a reference): only `idteck` (K26) and `nexwatch` (K27) carry the full 10-200 ms span in a single
run. **`indala` and `keri`'s coverage is a UNION of four different ladders** — K19/K20 and K21/K25
at 85-200, K22 at 20-80, K23/K24 at 1-65 — **with different seeds, different reps, different arms
present and, in K22/K23's case, a different number of cells feeding the median every band is read
against.** ⇒ **any cross-arm comparison over those cells compares a stitched profile with a measured
one**, which is a confound of the same family as M69's shared shuffle: something differs between
the arms that is not the arms.

⭐ **K28 is the fix and it is a plain capture, not a new band**: `indala` and `keri` together over
the **same 10..195 ms at 5 ms — 38 cells** that `nexwatch` ran, two fresh seeds (**277**, **281**),
`--reps 8`, `--per-arm-shuffle`. ⛔ **Topped at 195 and not 200, by M77's per-arm rule** — and note
their probes are SHORTER than `nexwatch`'s (32.8 ms and its own reader), so the burst is not the
binding constraint for them; **195 is chosen to make the ladder COMMON, which is the whole point.**

  **SCORED BY K26's A1, UNCHANGED** — a run of **>= 3 adjacent cells at median + 25** in **BOTH**
  seeds, overlapping. ⭐ Third and fourth application of an already-pinned band to a new arm, which
  is replication; nothing is re-tuned.
  ⚠⚠ **AND IT SETTLES THE QUESTION THAT DECIDES WHETHER A CROSS-ARM BAND IS BUILDABLE AT ALL.**
  `indala`'s median on the 85-200 ladder is **75-88%**, where *median + 25* is the CEILING and the
  forward detector has no power (M68 — this is K20's power table, and it is why K20 used a notch).
  **Nobody knows its median over the FULL span**, because the 10-80 region it has never been scored
  with includes measured lows at 35-45 ms. ⇒ **If the full-span median comes out near 50%, one
  forward band can cover all four arms; if it stays near 75-88%, it cannot, and a cross-arm band
  needs two detectors — which is exactly the confound C524 identified in K21.**
  ⛔ **That question is answered by this capture and must NOT be pre-judged**: K28 makes no
  cross-arm claim and its A1 verdicts are per-arm, as committed.

  ⛔ Gates unchanged: pooled **< 15%** is NO POWER; split-half **> 15 points** (at 38 cells x 8 reps
  the n per half is 152, so ~**2.7σ** — C530/M73); **P1 > 0.6** in any cell, which M77's printed
  per-arm top should now keep clear.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐⭐ K29 — THE THREE-ARM SHARED-REGIONS BAND (written before its capture)

The unit C537 specified and the first thing on this line that could point at a **mechanism** rather
than at another profile. **The lead-time structure is already measured at fixed MILLISECONDS on four
arms and two frame lengths (C536); this asks whether the four arms' regions are THE SAME regions**,
in one pre-registered statistic instead of four per-arm ones.

⭐ **THREE ARMS, AND THE THIRD IS DECIDED BY ARITHMETIC RATHER THAN BY TASTE** (C537): medians on
the common 10-195 ladder are `keri` **25%**, `idteck` **25%**, `nexwatch` **25-38%** — all three
take one **forward** detector — while `indala`'s is **75%**, where *median + 25* is the ceiling and
the forward detector has no power at all (M68, K20's move a second time). ⭐⭐ **`nexwatch` stays in,
and that is the point: its frame is 4096 samples against the others' 2048, so it is the only frame
discriminator on the bench** (M65/C520/C536).

### ⭐ THE REFERENCE, DERIVED MECHANICALLY FROM THE CAPS AND NOT FROM ANY WRITE-UP (M76)

`./framescale.py --inventory` over every banked cap, then: a cell is HIGH for an arm when it is
**>= 62.5% (5 of 8) in EVERY cap that measured it**, with at least two caps measuring it.

    keri      15 20 60 65 105 140 145 190
    idteck    15 20 55 60 65 95 100 140 145 180 185 190
    nexwatch  10 20 55 60 65 100 105 140 180 185 190

⇒ contiguous runs of **>= 2 cells HIGH in >= 2 of the three arms**, which is the candidate set:

    R1 15-20     R2 55-65     R3 100-105     R4 140-145     R5 180-190

⚠ **Five cells — 20, 60, 65, 140, 190 — are HIGH in ALL THREE arms in every cap that measured
them.** ⛔ That is the observation this band exists to test and **not** evidence for it: the regions
come from these caps, so **K29 may not be scored on any of them.** Fresh seeds only.
⚠ **And the caps are uneven**: per-cell counts are `keri` 4-23, `nexwatch` 4-6, **`idteck` only 2**.
The derivation is therefore weakest for `idteck`, which is stated here rather than discovered later.

### THE BAND

**The ladder**: the common **10..195 ms at 5 ms, 38 cells** (M77 — topped at 195, measured twice),
`--arms keri,idteck,nexwatch` in ONE run so the arms interleave, `--reps 8`,
**`--per-arm-shuffle`** (M69, mandatory), two fresh seeds (**283**, **293**).

  **B1 — ARE THE REGIONS SHARED?** An arm **HITS** a region when **>= 2 of that region's cells are
  at or above (that arm's ladder median + 25) in BOTH seeds**. The statistic is the **total hit
  count over 5 regions x 3 arms, out of 15**.
  - **>= 11 ⇒ B1 FIRES — the regions are shared across all three arms.**
  - **<= 7 ⇒ B1 REFUTED — the regions are NOT shared.** ⛔⛔ It does NOT distinguish *each arm
    has structure of its own* from *no arm has structure at all* — a flat ladder refutes it too.
    **The per-arm A1 verdicts separate those** (C533 `idteck`, C536 `nexwatch`, C537 `keri` — all
    three FIRED), so a refutation must be read beside them and never alone.
  - **8-10 ⇒ NO VERDICT, AND IT HAS A STATED MEANING** (see below).

  ⭐⭐ **FULLY CHARACTERISED BY SIMULATION BEFORE THE CAPTURE (1,500 draws per row), AND THE
  NO-VERDICT BAND IS GIVEN A MEANING IN ADVANCE — WHICH IS M74's LESSON APPLIED AT DESIGN TIME:**

  | the truth | median hits | P(>= 11) | P(<= 7) |
  |---|---|---|---|
  | all 5 regions shared by all 3 arms | **15** | **100.0%** | 0.0% |
  | 4 of 5 shared | 12 | **98.8%** | 0.0% |
  | **3 of 5 shared** | **9** | 0.0% | 0.3% |
  | 2 of 5 shared | 6 | 0.0% | **100.0%** |
  | 1 of 5 | 3 | 0.0% | **100.0%** |
  | each arm 3 RANDOM 3-cell regions of its own | 3 | **0.0%** | **100.0%** |
  | each arm 5 RANDOM regions of its own | 4 | **0.0%** | 99.3% |
  | flat, no regions at all | 0 | 0.0% | 100.0% |

  ⇒ **it fires on 4-5 of 5 shared and refutes on 2 or fewer, with 0.0% false-fire against arms
  carrying independent structure of their own.** ⭐ **And 8-10 means specifically *about three of
  the five regions are shared*** — not *the band failed*. Say that if it lands there.

  ⛔ Gates unchanged and any one ends an arm: pooled **< 15%** is NO POWER; split-half **> 15
  points** (38 cells x 8 reps ⇒ n/half 152, so ~**2.7σ** and under 1% on noise — C530/M73);
  **P1 > 0.6** in any cell. ⚠ **If any arm is gated out, B1 is NOT re-scored over the remaining
  two** — the 15-cell statistic and every figure above assume three arms. Report NO VERDICT and
  which arm failed.

  ⛔⛔ **WHAT B1 CANNOT DO.** It tests whether the regions COINCIDE. It says nothing about WHY, it
  cannot separate a reader effect from a field effect, and **it excludes `indala` by construction**
  so it is a claim about three arms and not about the bench. ⚠ And `nexwatch`'s presence is what
  makes a FIRE interesting — it would put a 4096-sample frame and two 2048-sample frames in the
  same millisecond regions — but C536 already showed that for two of these regions, so a FIRE
  strengthens rather than establishes it.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐⭐ K30 — IS THE LEAD TIME A DURATION OR A SAMPLE COUNT? (written before its capture)

⛔⛔ **THE CONFOUND THIS CLOSES, AND NOTHING ON THIS LINE HAS EVER SEPARATED IT.** Every lead-time
result varies the primer with `lf read -s N`, and **N sets its SAMPLE COUNT and its DURATION
together**. So *fixed milliseconds* (C517/C536/C538) has always meant *fixed N*, and a purely
**client-side** reading of the whole line is not excluded. ⭐ `lf config --dec N` separates them —
verified two ways in C539 — and its stretch is **1.653x, not 2x** (C540/M79).

### ⛔ THREE ARITHMETIC RESULTS THAT SHAPED THIS BAND, ALL BEFORE ANY CAPTURE

1. **REACHABILITY** (M77 on the REAL duration): a dec-2 primer costs 1.653x its nominal ms, so the
   top is **138 ms nominal** for `keri`, 157 for `idteck`. ⇒ **R4 and R5 cannot be seen at their
   *stayed* positions**, and their absence there is uninformative.
2. **A COLLISION** (C541): *elapsed* moves R3 to **60-64**, inside R2's *stayed* **55-65**. ⇒ a
   region at 55-65 is produced by BOTH hypotheses and is **informational only**.
3. ⭐⭐ **THE REGIONS NARROW.** A width in ms divides by 1.653 too: R2's 10 ms becomes **6.0 ms**
   and R1/R3/R4's 5 ms become **3.0 ms** — **1.6 to 2.2 cells on a 5 ms grid.** ⛔ **A 3-cell run
   detector (K26's A1) CANNOT SEE THEM**, so A1 is the wrong instrument here and is not used.

### THE BAND

**The ladder is TARGETED, not a sweep** — because a 2-cell run needs few chances to stay clean:
**10,15,20,25,30,35,40,45,60,65,90,95,100,105,110,115,120 ms** (17 cells), at **`--dec 2`**,
`--arms keri,idteck`, `--reps 8`, `--per-arm-shuffle`, two fresh seeds (**307**, **311**).

  ⛔ **THE DETECTOR IS ABSOLUTE, NOT MEDIAN-RELATIVE**: a cell is elevated at **>= 62.5% (5 of 8)**,
  the `--inventory` threshold, in **BOTH** seeds, and a region is **>= 2 adjacent** such cells.
  ⭐ It is absolute because a 17-cell targeted ladder has no honest median, and it is justified by
  prior measurement: `--inventory` has `keri` **LOW at 35-45 in every seed** at dec 1.

  **THE TWO DISCRIMINATING PREDICTIONS**, each detectable at >= 2 cells:
  - **ELAPSED** ⇒ regions at **30-40** (R2 moved, 33-39) **and 110-115** (R5 moved, 108.9-114.9),
    and **NOT** at 100-105.
  - **SAMPLES** ⇒ a region at **100-105** (R3 unmoved), and **NOT** at 30-40.

  **D1 fires ELAPSED** when 30-40 carries a region **and** 100-105 does not, in both seeds.
  **D1 fires SAMPLES** when 100-105 carries a region **and** 30-40 does not, in both seeds.
  **Anything else is NO VERDICT, AND BOTH CASES HAVE A MEANING PINNED HERE** (M74):
  - **regions at BOTH** ⇒ the knob changes something neither hypothesis describes;
  - **regions at NEITHER** ⇒ the dec-2 profile carries nothing this detector can see — which the
    gates and the 60-65 informational cells separate from a dead run.
  ⚠ **AND ONE AMBIGUITY, NAMED**: R5-moved (110-115) sits one cell from R3-unmoved (105). ⇒ a
  region spanning **105-110** supports neither and is **reported as ambiguous**, not counted.

  ⭐ **POWER, SIMULATED BEFORE THE CAPTURE (20,000 draws, both seeds required):**

  | the truth at those cells | fires |
  |---|---|
  | everything at the measured low, 25% | **0.01%** |
  | everything at 38% (the top of `keri`'s measured low) | 4.7% |
  | a 2-cell region at **88%** | **96.1%** |
  | a 2-cell region at 75% | 62.4% |
  | a ONE-cell spike at 100% | **0.36%** |

  ⇒ **false-fire 0.01% against the level actually measured there, and 96% power against a region
  as tall as the ones being looked for.** ⚠ The 4.7% row is the honest worst case and the reason
  the 60-65 and 90-95 cells are in the ladder: they show what the local level actually is.

  ⛔ Gates unchanged: pooled **< 15%** NO POWER; split-half **> 15 points**; **P1 > 0.6** in any
  cell — ⚠ and P1 is load-bearing again, because a dec-2 primer eats 1.65x the burst and the
  ladder's top cell (120 nominal = ~198 ms of elapsed primer) is close to `keri`'s computed 138.

  ⛔⛔ **WHAT IT CANNOT DO**: it tests the PRIMER's knob only. A *SAMPLES* verdict would not by
  itself make the whole line client-side — the probe's own read is untouched here — and an
  *ELAPSED* verdict does not say WHICH duration (air, readback, or both) matters.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K31 — K30's RE-RUN, WITH THE THRESHOLD AND THE n TAKEN FROM THE CONDITION (M80)

K30 returned **NO VERDICT — regions at NEITHER** on both arms with every gate passing (C542). The
fault was the threshold: **62.5% came from the dec-1 level and the dec-2 profile runs ~30% lower
pooled**, with cell medians of **12% (`keri`)** and **25% (`idteck`)**. ⇒ **K30 is the PILOT and its
product is that level.** K31 is the claim, on fresh seeds, with the detector derived from it.

⛔ **THE BANDS, THE LADDER, THE ARMS AND THE PREDICTIONS ARE UNCHANGED FROM K30** — same 17-cell
targeted ladder, same `--dec 2`, same `keri`+`idteck`, same D1 with its two discriminating cells and
both no-verdict meanings. **Only the threshold and `--reps` move, and both are set by arithmetic
against the measured level.** Fresh seeds: **313**, **317**.

### ⛔ CHOOSING THEM, SIMULATED BEFORE THE CAPTURE (12,000 draws, both seeds required)

| truth | reps 8, thr 50% | reps 8, thr 56% | **reps 16, thr 50%** |
|---|---|---|---|
| `keri`'s measured median, 12% | 0.00% | 0.00% | **0.00%** |
| `idteck`'s measured median, 25% | 2.94% | 0.02% | **0.03%** |
| a uniform 38% (pessimistic) | 67.7% | 6.7% | 27.3% |
| a 2-cell region at 62% | 62.9% | 18.3% | **64.3%** |
| a 2-cell region at 75% | 91.9% | 63.3% | **97.1%** |
| ⛔ a ONE-cell spike at 100% | **10.1%** | 0.31% | **0.42%** |

⇒ ⭐⭐ **`--reps 16` with an ABSOLUTE 50% threshold**, because it is the only column that is good in
every row: **0.00-0.03% at the levels actually measured, 0.42% against a one-cell spike** (the
2-cell rule collapses at reps 8 — a 25% background reaches 50% by noise 11% of the time, so a lone
spike plus a noisy neighbour fires it) **and 97% power against a 75% region.**
⚠ **The 27% at a uniform 38% is the honest residual**, and it is pessimistic: 38% is above both
arms' measured dec-2 medians. ⭐ **The gates print the run's actual pooled level, so this row is
checkable after the fact rather than assumed.**
⛔ **More reps, not a lower threshold, is what fixed it** — the low-level condition's problem was
quantisation at n=8, not the bar's height.

⚠ Cost: 17 cells x 16 reps x 2 arms ≈ 544 sessions, ~25 min per seed.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐ K32 — THE SAME TEST AT 2.5 ms, WHICH IS WHAT C541 ASKED FOR AND K30/K31 DID NOT DO

K30 and K31 both returned **NO VERDICT — regions at NEITHER** with every gate passing, and the
elevated cells that survive both seeds landed **exactly in the two zones the arithmetic had already
called non-discriminating** (C543): `idteck` at **105 AND 110**, straddling the R3-unmoved /
R5-moved boundary, and `keri` at **60**, inside the R2/R3 collision. ⇒ **the signal is real and the
discriminating power was in the wrong cells.** ⛔ **And C541 had already said the predictions *need a
2.5 ms grid and not a 5 ms one* — the requirement was dropped when the design became a targeted
ladder (M81).** This carries it out.

### ⭐ WHAT 2.5 ms BUYS, CELL BY CELL

    high zone   R3-unmoved 100-105          R5-moved 108.9-114.9
                100  102.5  105   |  107.5 EMPTY UNDER BOTH  |  110  112.5
    low zone    R2-unmoved 55-65            R3-moved 60.5-63.5
                55  57.5  (R2 only)  |  60  62.5 (BOTH)  |  65 (R2 only)

⇒ **107.5 is the separator the 5 ms grid did not have**, and in the low zone **55-57.5 is R2-only
against 60-62.5 being shared**, which 5 ms could not split either.

**The ladder**: **52.5, 55, 57.5, 60, 62.5, 65, 97.5, 100, 102.5, 105, 107.5, 110, 112.5, 115,
117.5** (15 cells), `--dec 2`, `--arms keri,idteck`, **`--reps 16`**, `--per-arm-shuffle`, two fresh
seeds (**331**, **337**). ⛔ **The threshold stays at 50% and the reps at 16, exactly as K31 set
them** — the dec-2 level is now measured twice and nothing about it has changed, so re-deriving
them would be fitting.

  **E1 — WHICH PREDICTION DOES THE HIGH ZONE MATCH?** A region is **>= 2 adjacent cells at
  >= 50%** in **BOTH** seeds.
  - a region inside **100-105** and none inside **110-112.5** ⇒ **SAMPLES**
  - a region inside **110-112.5** and none inside **100-105** ⇒ **ELAPSED**
  - **both, or neither, or any region covering 107.5** ⇒ **NO VERDICT** — and a region covering
    107.5 is specifically *the two predictions are not separated even at 2.5 ms*, which would
    close this approach rather than leave it open.
  **E2 — THE LOW ZONE, SUPPORTING AND NOT DECIDING.** A region in **55-57.5** is R2-unmoved and so
  supports SAMPLES; one in **60-62.5** supports neither (it is the collision) and is reported.
  ⛔ E2 never overrides E1; if they disagree, that is a NO VERDICT and is said so.

  ⭐ **POWER at 15 cells, reps 16, thr 50% (15,000 draws, both seeds):** **0.02%** at the measured
  25% median, **0.37%** against a ONE-cell spike, **97.1%** against a 2-cell region at 75% and 64.8%
  at 62%. ⚠ The pessimistic uniform-38% row is **23%**, and the gates print the run's real pooled
  level so it stays checkable.
  ⛔ Gates unchanged. ⚠ P1 is comfortable here: the top cell is 117.5 nominal ≈ 194 ms of decimated
  primer, well inside `keri`'s 138 ms nominal limit... **⛔ NO — 117.5 is UNDER 138, so it is
  inside; stated explicitly because that limit is the one M77 exists for.**

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐⭐ K33-PILOT — WHERE DOES `idteck`'s BURST ACTUALLY END AT dec 2? (before K33, not after)

C544 named the successor by arithmetic: **`idteck`, R4-unmoved 140-145 against R4-moved 84.7-87.7**,
a **56 ms** separation that no skirt can bridge. ⛔ **But that test spends 89-93% of the budget**, and
the budget's top is a number this file itself calls an **UPPER BOUND**: `_top` computes `idteck` at
**157 nominal**, and the same formula put `nexwatch` at ~210 where **200 FAILED P1 twice** and 195 was
clean. ⇒ **the computed top has already been measured optimistic by ~5% once**, and K33's ladder sits
at 89-93% of it. **Running K33 without measuring the real ceiling is M81 again** — a requirement
carried into a new shape without being re-checked against it.

### ⭐⭐ THE ARITHMETIC THAT MAKES THE CEILING THE WHOLE GAME (offline, no capture)

A nominal label `L` at stretch `S` costs `L*S` of elapsed primer, so `L <= B/S` where
`B = LF_BURST_MS - K12_OVERHEAD_MS - probe_ms` = 500 - 192 - 49.2 = **258.8 ms** for `idteck`.
The moved prediction sits at `L/S`, so **separation = `L(1 - 1/S)`**, maximised at `L = B/S`:

    sep_max(S) = B(S-1)/S^2      d/dS = (2-S)/S^3      =>  PEAK AT S = 2 EXACTLY

| S | max label B/S | max separation |
|---|---|---|
| 1.653 (dec 2, measured) | 156.6 | **61.9** |
| 2.000 (the optimum) | 129.4 | **64.7** |
| 2.300 (dec 4, estimated) | 112.5 | **63.6** |
| 3.000 | 86.3 | 57.5 |

⭐⭐⭐ **SO C544's SECOND OPTION IS ANSWERED WITHOUT A CAPTURE, AND THE ANSWER IS NO.** *A higher
decimation widens the separation for the same label* is true, but the label it can afford shrinks
faster: the separation is capped at **B/4 ≈ 65 ms for ANY decimation**, dec 2 already reaches **94%**
of that cap, and dec 4 would reach 98% — **a 2.7% gain for the slope-fit M79 requires.** ⛔ **Do not
slope-fit dec 4 for this purpose.** ⚠ And the conclusion is robust to `B` being wrong: a smaller `B`
scales every row by the same factor and moves neither the peak nor the ordering.

⇒ ⭐⭐ **THE SEPARATION CANNOT BE BOUGHT — IT CAN ONLY BE SPENT AT THE TOP OF THE BUDGET.** Which
makes *where the budget really ends* the one number K33 depends on, and it has never been measured
for this arm at this decimation.

### THE PILOT

`--k12 --arms idteck --dec 2 --reps 8`, one seed (**347**), ladder
**110, 120, 130, 140, 145, 150, 155, 160, 170** (9 cells + the shuffled no-primer control), which
**brackets the computed 157 from both sides** so it can find the top either optimistic or pessimistic.
⛔ **It scores no band and claims no region** — it is an instrument measurement, like C539's knob
check and C540's timing, and its product is a ceiling and a level.

  **P1 IS THE INSTRUMENT**: the largest cell whose **arrivals per read stay <= 0.6**. Above the
  burst, arrivals go to 1.00 and the probe reads a re-armed field, which is starvation and not lead
  time.

  ⭐ **AND ITS MEANINGS ARE PINNED HERE (M74):**
  - **clean through 145 and beyond** ⇒ K33's window is inside the burst ⇒ **run K33 as C544 specified.**
  - **breaks at or below 145** ⇒ ⛔ **R4-unmoved is not observable at dec 2 at all.** With the cap
    above showing no decimation does better, that **CLOSES the decimation approach on this bench**
    rather than inviting another variant — it is a real answer, not a failed run.
  - **breaks between 145 and 157** ⇒ reachable but at the edge: K33 runs with its top clamped to the
    measured ceiling, and carries M78's note below.

  ⚠⚠ **M78 — WHICH WAY WOULD THE DEFECT PUSH?** Starvation at the top of the ladder depresses the
  **unmoved** window (140-145) and leaves the **moved** one (84.7-87.7) untouched. ⇒ it biases toward
  *no region at unmoved* ⇒ **toward ELAPSED**. So under K33 a **SAMPLES** fire is conservative and
  survives a soft ceiling, while an **ELAPSED** fire is confounded with starvation and must not be
  read as a verdict unless P1 is clean in every cell of the unmoved window. **Stated before the
  capture, not after it.**

  ⭐ **SECOND PRODUCT, AND M80 REQUIRES IT**: the decode LEVEL at 140-155, which no dec-2 run has
  ever visited — K30/K31/K32 all stopped at 120. The 50% absolute threshold was derived from cells
  <= 120, and K30's whole failure was a threshold imported from a level measured elsewhere. If the
  level at K33's window is far under 50%, **the threshold is re-derived from this pilot** and K33
  runs on fresh seeds, exactly as K31 did from K30.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐⭐ K34 — THE BURST IS OURS, AND IT IS THE ONLY LEVER LEFT (design pinned; UNRUN)

C545-C548 closed the decimation approach, and closed it **generally**: every bound on the
separation is proportional to the elapsed budget `B`, and `B = LF_TAG_BURST_TARGET_MS - overhead -
probe`. ⛔ The obvious escape (an `msleep`, which buys lead time at zero sample cost) is **already
refuted** — a gap means the field is DOWN, which K10 measured restarts the burst past ~120 ms, and
that is precisely why K12 abandoned the gap knob for the primer knob (C515).

⇒ ⭐⭐ **THE ONE TERM LEFT IS THE BURST ITSELF, AND IT IS A CONSTANT IN OUR OWN FIRMWARE** —
`LF_TAG_BURST_TARGET_MS (500)` at `lf_tag_em.c:75`, clamped to `[2, 255]` whole frames at
`lf_tag_em.c:569`.

### THE ARITHMETIC, AND IT IS THE WHOLE CASE FOR DOING IT

At **1000 ms** the budget goes from 345.7 ms of elapsed primer to 845.7 — **245 → 600 dec-1 nominal
ms** — and the reachable tops at dec 2 / 3 / 4 go from **138 / 100 / 79** to **338 / 245 / 193**:

| region | separation at dec 2 | reachable at burst 500 | at burst 1000 |
|---|---|---|---|
| R3 100-105 | 45.8 ms | yes — ⛔ **and K32 measured it BRIDGED** | yes |
| **R4 140-145** | **63.3 ms** | **NO** (top 138.2) | ⭐ **yes** |
| **R5 180-190** | **82.9 ms** | **NO** | ⭐ **yes** |

⇒ **R4 and R5 both clear the skirt with room**, and the cap `Bn/4` rises from 61.3 ms to **150 ms**.

### ⛔⛔ AND THE REASON THIS IS NOT A QUIET RE-RUN OF K33

**The burst is not a neutral instrument — it is the PHASE REFERENCE this entire line measures lead
time FROM** (C511, C512). Lengthening it may **move** the regions rather than merely reveal more of
them, and a K33 re-run that assumed otherwise would be measuring one thing and reporting another.

⇒ ⭐⭐⭐ **SO IT IS TWO STAGES, AND STAGE 1 IS THE CONTROL FOR STAGE 2 — never run stage 2 alone.**

  **K34a — DOES THE BURST'S LENGTH MOVE THE REGIONS?** The dec-1 ladder over a span both builds
  reach (say **10-190 ms**, the C538 common ladder), `idteck` + `keri`, two fresh seeds,
  `--per-arm-shuffle`, at burst **500** and burst **1000**, the two builds interleaved **by
  session and not by block** if the flashing cost allows it, and by block if it does not — ⚠ in
  which case **order is a variable (M60) and the block order must be counterbalanced.**
  - **regions at the same lead times in both** ⇒ the burst length is a **reach** knob and nothing
    else. ⭐ **That licenses K34b and is the only thing that does.**
  - **regions MOVE** ⇒ ⭐⭐ **a finding in its own right and a bigger one than K34b** — the
    structure is referenced to the burst, which is upstream of reader *and* field, and it would
    **retire the whole *reader-or-field* branch** C538 opened. ⛔ K34b is then meaningless and must
    not be run.
  - **no regions at burst 1000 at all** ⇒ the longer burst changed the emission's own behaviour;
    report it and stop, do not reach for a third build.

  **K34b — K33, FINALLY RUNNABLE.** Only if K34a says *same lead times*. `idteck`, `--dec 2`, R4
  unmoved **140-145** against moved **84.7-87.7** — ⛔ **placed with the stretch re-fitted by
  `dectime.py` on the NEW build**, never with 1.653 and never with this build's 1.775 (M79/C547: it
  is known to ~7% and a new binary is a new condition). ⭐ **The cells at 140-145 are now 41% of the
  reachable top rather than 101% of it**, so M82's failure mode is gone rather than reduced.
  ⚠ Threshold and reps to be re-derived from K34a's measured dec-2 level at the new burst (**M80** —
  the level is exactly what a longer burst might change), power and false-fire simulated first
  (M70/M75), both no-verdict branches given meanings in advance (M74).

### ⛔ THE COST, STATED PLAINLY BECAUSE IT IS A DECISION AND NOT A DETAIL

**K34 needs a cu2 FLASH.** That is permitted unattended — `enterdfu.py --port <tty> --program <zip>`
needs no bench move and verifies re-enumeration — and ⛔ **cu2 ONLY, NEVER cu1**, which is the spare
that keeps the bench alive and the successor project's writer. ⚠ `enterdfu.py` is known to fail to
**trigger** two or three times before succeeding, with nothing flashed either time, and it
distinguishes that from a flash failure — **retry it; do not go looking for a broken device.**
⚠⚠ **The honest risk**: a flash that goes wrong with nobody present costs Rig B for the rest of the
absence. It does not cost the bench — Rig A is untouched and cu1 is never flashed — but it ends the
air-side work. ⇒ ⭐ **verify the new build FUNCTIONALLY and not by version string (C461)** before
spending a capture on it: an armed Indala must show **64 entries, `seq repeats` 15**
(`hw emuseq --count 0`), `hw emuhold -n 1 --top 8` must succeed, and ⭐ **the burst change itself
must be confirmed from the AIR, not the source** — at burst 1000 a primer that broke P1 at 500
(`idteck` 140 nominal at dec 2, arrivals 0.81) must come back **clean at 0.50**. ⛔ **That check is
not optional: it is the only evidence the constant took effect**, and C461 is the note about
believing a version string instead of the device.

### ⭐⭐⭐⭐⭐ K34a PRE-REGISTERED, 2026-09-17 — THE BANDS, THEIR THRESHOLDS AND THEIR POWER

⛔⛔ **WRITTEN BEFORE THE FLASH AND BEFORE ANY CAPTURE (M55).** Every number below came out of
`k34sim.py` on the two banked C538 caps' own per-cell rates, which is the bench as it actually
behaves rather than a curve chosen to be beaten.

**THE RUN.** `keri` + `idteck`, the C538 common ladder **38 cells 10-195 ms**, **dec 1**,
`--per-arm-shuffle`, **`--reps 16`**, two fresh seeds per burst condition — four caps.
⭐ **M77 is satisfied by the SAME ladder in both conditions**, which is why it is not widened:
the dec-1 reachable tops are `keri` **214** / `idteck` **245** nominal ms at burst 500 and 569 /
600 at burst 1000, so 195 is inside the budget in both and the comparison is like-for-like. ⛔ A
ladder that reached further at burst 1000 would confound the extra reach with the thing under test.

**HOW A CAP KNOWS WHICH BUILD TOOK IT.** Each cap records `_burst.frames` — the frames-per-burst
the DEVICE reports through `hw emudebug`, with a sequence loaded — and the scorer sorts the four
caps into conditions **by that number and refuses a cap without it**. Predicted: **31 frames at
burst 500 and 62 at burst 1000** (`ceil(target / 16.384 ms)`, `lf_tag_em.c:569`). ⛔ A version
string is not evidence (C461); this is the device answering.

**THE BANDS.** `H500` and `H1000` are the sets of (arm, region) pairs HITting in each condition,
out of 2 arms x 5 regions = 10, under **K29's unchanged rule** (>= 2 cells of the region elevated
at that cap's own ladder median + 25 points, in BOTH seeds of that condition).

| band | fires when | meaning |
|---|---|---|
| **D1 SAME** | `|H500 ∩ H1000| >= 0.75·|H500|` **and** `|H1000 minus H500| <= 2` | the burst is a REACH knob ⭐ **the only band that licenses K34b** |
| **D2 MOVED** | `|H500 ∩ H1000| <= 1` and `|H1000| >= 2` | structure present but ELSEWHERE ⇒ bigger finding, K34b meaningless |
| **D3 NONE** | `|H1000| = 0` | no region survives — ⚠ *moved-or-abolished*, see below |
| **CONTROL FAILED** | `|H500| < 3` | the short burst found too little to judge replication by |
| **NO VERDICT** | anything else | ⭐ **stated in advance (M74): the structure PARTLY survives** — the burst changes which regions reach threshold without preserving or relocating them. ⛔ Not a failed band; do not re-tune |

**POWER AND FALSE-FIRE, 10,000 draws per truth, at `--reps 16`:**

| truth | D1 | D2 | D3 | no verdict |
|---|---|---|---|---|
| **A SAME** — identical profile | ⭐ **93.6%** | 0.0% | 0.0% | 6.4% |
| **B SCALED** — `p1000(x) = p500(x/2)` | **0.0%** | 34.2% | 7.2% | 58.6% |
| **C SHIFTED** — +20 ms | **0.0%** | 0.1% | 1.8% | 98.1% |
| **D FLAT** — structure abolished | **0.0%** | 0.0% | 100.0% | 0.0% |

⭐⭐ **D1's false-fire is 0.0% against all three ways the regions could have moved.** That is the
number that matters, because D1 is the only band that licenses more bench time: K34b cannot be
reached by a fluke. ⛔ `--reps 16` and not 8 is set by this table alone — at reps 8 D1's power is
**79.0%** for the same 0.0% false-fire, and K31 already paid once for a no-verdict bought by
under-powering (C542/M80).

⚠⚠ **THE TWO WEAKNESSES, STATED BEFORE THE CAPTURE AND NOT AFTER IT.**
1. ⛔ **D3 CANNOT TELL *abolished* FROM *moved out of the window*** — it fires 100% under FLAT
   but also **7.2%** under SCALED and 1.8% under SHIFTED. ⇒ a D3 result must be reported as
   **moved-or-abolished**, never as *abolished*. M78: the defect pushes a MOVED truth toward D3,
   and since neither licenses K34b the licensing decision is unaffected — only the wording is.
2. ⛔ **K34a IS POWERED TO LICENSE, NOT TO CHARACTERISE.** D2 fires on only 34% of genuinely
   scaled profiles and 0.1% of 20 ms shifts; most real moves land in NO VERDICT. ⇒ **do not read
   a NO VERDICT as evidence the regions stayed** — it is the single most likely outcome under
   both SCALED (58.6%) and SHIFTED (98.1%).

⭐ **AND ONE DESCRIPTIVE CHECK, PRE-REGISTERED AS DESCRIPTIVE SO IT CANNOT BE REACHED FOR LATER**
(M74): if lead time scaled with the burst, every region sits at **2x** its burst-500 position, and
two land on this ladder — **R1 at 30-40 and R2 at 110-130**. The scorer prints the cells elevated
there **on every outcome**, fires nothing on it, and it is the thing to read beside a D3 or a NO
VERDICT. ⛔ It is not a band and may not be upgraded into one.

⚠ **THE CONFOUNDS ENUMERATED BEFORE THE RUN**, since a longer burst changes more than reach:
- **Duty cycle: essentially unchanged.** Bursts repeat back-to-back separated by a fixed
  `ANT_NO_MOD(); bsp_delay_ms(2)` (`lf_tag_em.c:198-215`), so the emission is ~99.6% duty at
  burst 500 and ~99.8% at 1000. ⛔ Not zero, but far too small to be a mechanism.
- **The probe stays inside the FIRST burst in both conditions.** The ladder's top is 195 nominal
  = ~275 ms elapsed plus overhead and probe, against 507.9 ms of burst at 500. ⇒ the inter-burst
  boundary is never crossed on this ladder, in either build, so no region can be an artefact of it.
- ⚠ **What is NOT controlled: total emission time per burst.** That is the thing under test and
  cannot be held constant. D1 firing is what says it did not matter.

⛔ Ungraded — no null sweep, no calibration row, no licence. It moves no cell.

⛔ Ungraded, and UNRUN — a design. No null sweep, no calibration row, no licence. It moves no cell.

## ⭐⭐⭐⭐ K35 — DID THE LEVEL STEP FOLLOW THE CLOCK OR `--reps`? (pinned 2026-09-17, UNRUN)

C549/L541: the pooled level steps down between the morning block (**36.5-43.1%**, all `--reps 8`,
07:07-10:08) and the afternoon block (`keri` **32.6-35.4%**, `idteck` **22.0-33.2%**, all
`--reps 16`, 14:45-17:31), and **recovers within the afternoon** — so it is a step between
blocks, not drift through a session. ⛔⛔ **And the banked caps cannot say which, because REPS
AND TIME-OF-DAY ARE PERFECTLY CONFOUNDED across them: no cap in `caps/` is reps 8 in the
afternoon or reps 16 in the morning.** That is why this needs a measurement and not another plot.

⚠ **It matters beyond bookkeeping**: if the step follows the CLOCK, then every band on this line
needs counterbalancing against time — K34a's ABBA already had it, and the earlier A-then-B bands
did not. If it follows `--reps`, then K34a's own reps choice is implicated and its re-run changes.

**THE RUN.** `keri` alone, the C538 common **38-cell 10-195 ms** ladder, dec 1, **`--reps 8`**,
`--per-arm-shuffle`, **two fresh seeds**, burst 500 (the standing build — ⛔ **no flash**, no
bench move, ~60 min). ⭐ It is a deliberate replication of a MORNING configuration, changing only
when it runs.

**THE REFERENCES, taken from the banked caps and not from a write-up (M76):** morning `keri` pooled
**40.5, 38.2, 42.1, 41.8%** (mean **40.6**) with seed agreement **+0.827** (k28) and **+0.784**
(k29); afternoon `keri` pooled **32.6, 33.4, 32.4, 35.4%** (mean **33.5**) with agreement
**+0.573** and **+0.693**. Midpoint of the two levels: **37.0%**.

| band | fires when | reads as |
|---|---|---|
| **E1 CLOCK** | pooled **>= 37.0%** AND seed agreement **>= +0.75** | reps 8 at a non-morning hour reproduced the MORNING condition ⇒ the step is not `--reps` |
| **E2 REPS** | pooled **<= 37.0%** AND agreement **<= +0.70** | reps 8 reproduced the AFTERNOON condition ⇒ `--reps` is not the lever either, and the step follows the clock or the session |
| **NO VERDICT** | the two markers disagree | ⭐ **stated in advance (M74): level and agreement are not moving together**, which refutes the single-cause framing both bands assume. ⛔ Not a failed band — it is the answer that the two markers are separable, and the next design must treat them separately |

⚠⚠ **THE ASYMMETRY, AND IT DECIDES WHEN THIS MAY BE RUN.** Read E2's wording carefully: it is
NOT *reps caused the step*.
- ⭐ **Run at ANY hour, an E1 fire is decisive**: reps 8 away from the morning giving morning
  numbers rules `--reps` out.
- ⛔ **An E2 fire is only decisive if the cap is taken INSIDE the morning window (~07:00-10:00).**
  Outside it, E2 is consistent with *the clock did it* AND with *reps did it*, and says only that
  reps 8 is not sufficient to restore the morning condition. ⇒ **Prefer a morning tick. If the
  tick fires outside that window, run it anyway — E1 is still decisive — but record the hour in
  the same string as the verdict (M73) and ⛔ do not report an out-of-window E2 as a cause.**

⚠ **What one cap-pair cannot do**: separate *time of day* from *session boundary* (a cold start,
a fresh USB enumeration, a power cycle). They are different hypotheses and this run confounds
them by construction — say so rather than implying otherwise. ⭐ A morning run that FIRES E1
makes that the next question, and it is a cheap one.

⛔ **Gates unchanged** — the three K17+ gates on every cap, and a cap failing any of them is not
scored. ⛔ Ungraded, and UNRUN: no null sweep, no calibration row, no licence. It moves no cell.

⛔⛔ UNGRADED — no null sweep, no calibration row, no licence. It moves no cell.
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
    # ⭐ K17's arm, and the only one that can break M65's confound: PSK like `indala`/`keri` but a
    # 4096-sample frame, so H_ms and H_frame put its peak 66 ms apart. 3 frames by C499's rule
    # (the window is 3-4 frames on every arm), NOT by a pilot — see the K17 section.
    "nexwatch": "lf read -s 12288",
    # ⭐ K26's arm. 3 frames of its 2048-sample frame, by C499's standing rule and NOT by a pilot
    # on this arm. Precision 100% (C503) and a marker of its own, which is what makes it
    # scoreable where `indala224` is not — see the K26 section for why that one cannot be.
    "idteck": "lf read -s 6144",
}
# ⛔ `nexwatch` is deliberately NOT in ORDER: it is the weakest of the six and would dilute the
# default three-arm runs, whose probes were chosen for power. `--arms nexwatch` asks for it.
ORDER = ["gproxii", "indala", "keri"]

# ⛔⛔ THE ARMS WHOSE NO-PRIMER CELL IS EXPECTED AT ~0%, AND IT IS EXACTLY ONE. C512 measured
# `gproxii` at 0% on a fresh burst; C514 measured that `indala` and `keri` do NOT collapse there
# (50%→67%, 33%→54%). K12's control band was written for the first and would print BENCH MOVED on
# a healthy run of the others. ⇒ A criterion carried across arms without re-deriving it is the
# same error C514 caught in a warning carried across arms without re-measuring it.
FRESH_BURST_ZERO = frozenset({"gproxii"})


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


def session(arm_key, reads, timeout, delay=0, lead=0, primer=0, dec=1):
    """One pm3 invocation issuing `reads` IDENTICAL probes. Returns the per-read scores in order.

    ⛔ `reads == 0` is the instrument control and is not a degenerate case: the client still
    connects and disconnects, and whatever field that costs is what Δ_0 measures."""
    a = shortread.ARMS[arm_key]
    probe = PROBES[arm_key]
    cmds = []
    if lead:
        cmds.append("msleep -t %d" % lead)     # ⭐ K8: once, BEFORE the first read
    if primer:
        # ⭐ K12's independent variable, and it is the only one in this file that holds the
        # field UP rather than letting it drop: one capture of `primer` samples immediately
        # before the probe, no msleep between them, so both reads share one burst and the
        # cells differ only in how long the emission has been running.
        #
        # ⭐⭐ K30's SECOND variable, and the two `lf config` commands are issued at EVERY dec
        # INCLUDING 1 on purpose: the band compares dec 1 against dec 2 and needs the CLIENT's
        # own cost identical in both, so only the primer's AIR duration differs (C539). ⛔ The
        # reset to 1 before the probe is what keeps the demodulator working — `decprobe.py`
        # measured that path decoding 3 of 3 at both settings.
        cmds.append("lf config --dec %d" % dec)
        cmds.append("lf read -s %d" % primer)
        cmds.append("lf config --dec 1")
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


# ⭐ K12's ladder. The step is 20 ms and the top is 200 ms; both are load-bearing and the
# docstring says why (120 ms = one beat cycle to within 1.6 ms; 200+192+98 = 490 ms < the 500 ms
# burst). ⛔⛔ THE TOP WAS 240 UNTIL THE ARRIVALS CONTROL REFUTED IT — 220 and above re-arm the
# burst mid-session, measured 3/3. Do not widen it without re-running that sweep; the arithmetic
# alone said 418 ms and was wrong, because the field-up overhead is ~192 ms and not the ~80 ms
# the client's wall clock shows.
K12_PRIMERS_MS = [20, 40, 60, 80, 100, 120, 140, 160, 180, 200]
K12_CYCLE_MS = 121.6          # C509: 131.5 ppm on 62.5 kHz = 8.22 Hz. Not fitted here.
K12_PAIR_LAG_MS = 120         # the ladder's own approximation to it — 1.6 ms, 4.7 degrees
# ⛔ MEASURED, NOT ASSUMED — see the docstring. The client's wall-clock overhead for an extra
# read is ~80 ms; the FIELD stays up ~192 ms, which is what the emission actually experiences.
# Only the arrivals counter distinguishes the two, and the first version of this file used the
# host number. It shifts the phase COLUMN and the floor; P3 is a difference, so it cancels there.
K12_OVERHEAD_MS = 192
# `LF_TAG_BURST_TARGET_MS` in `lf_tag_em.c` (K4, confirmed from the air by C516).
LF_BURST_MS = 500
# ⛔ `lf_read()`'s per-command sample count for arms probed by their OWN reader — the 6x spread
# C494 measured in the pm3 client, and the burst pays for these samples too.
READER_SAMPLES = {"keri": 10000, "indala": 30000, "nexwatch": 20000, "idteck": 5000,
                  "gproxii": 10000, "indala224": 30000}
# ⭐ MEASURED, not nominal (C540/M79): decimation 2 stretches a read by 1.653x and not 2x,
# because the USB readback does not scale with it. ⚠ Unmeasured for other values.
# ⛔⛔ C548: THE STRETCH IS NOT THE WHOLE COST AND THIS TABLE IS NOT WHAT `_top` SHOULD USE.
# A read's ELAPSED is not its acquisition: C540 measured the dec-1 slope at 0.0118 ms/sample
# where 125 kHz predicts 0.0080, a ~47% USB-readback surcharge — and the old `_top` charged the
# primer at 1x nominal, so that surcharge went UNCOUNTED. It is not a constant error either:
# it scales with the primer, which is why one overhead constant could not absorb it at every
# length. ⇒ `_top` now uses MS_PER_NOMINAL below. The stretch is kept because the BAND's
# predictions are ratios (label -> label/S) and a ratio is all they need.
DEC_STRETCH = {1: 1.0, 2: 1.653}

# ⭐ dectime.py, slope-fitted over 5 sample counts x 4 decimations x 4 reps, shuffled (C546).
# elapsed = N * (a*dec + x), a = 0.00792 (acquisition, scales) + x = 0.00371 (readback, does not).
DEC_SLOPE = {1: 0.01128, 2: 0.02001, 3: 0.02762, 4: 0.03514}
# The real elapsed cost of ONE NOMINAL ms of primer (N = nominal * 125), per decimation.
MS_PER_NOMINAL = {d: 125.0 * v for d, v in DEC_SLOPE.items()}
# ⭐ The client's own per-read wall-clock cost, which is what the ~80 ms at K12_OVERHEAD_MS's
# own comment always said. The 192 was that 80 plus the uncounted readback, fitted at one
# primer length and wrong at every other. C548 fits this against idteck's measured break.
CLIENT_OVERHEAD_MS = 85.0


def k12(a, arms, primers):
    """⭐ K12. Primer LENGTH varies the field-UP time before a fixed probe at a fixed index.

    ⛔ The cell list carries `None` for the no-primer control, and it is shuffled in with the
    rest so it cannot sit at a fixed position (M60)."""
    import collections
    import random as _r
    cells = [None] + list(primers)

    def build(seed):
        rng = _r.Random(seed)
        plan = []
        for _ in range(a.reps):
            rnd = list(cells)
            rng.shuffle(rnd)                # ⭐ shuffled WITHIN each round, not once globally
            plan.extend(rnd)
        return plan

    # ⛔⛔ ONE PLAN FOR EVERY ARM IS A CROSS-ARM CONFOUND (M69, found while designing K21).
    # M60 established that POSITION is a variable on this bench. With a single plan, every arm
    # sees the identical cell→position mapping in every round, so whatever position contributes
    # enters both arms' profiles IDENTICALLY and manufactures a positive cross-arm correlation
    # out of nothing. ⚠ It does not touch a WITHIN-arm verdict (K12-K20 are all within-arm), so
    # nothing banked is retracted — but a cap taken without `--per-arm-shuffle` can never carry
    # a cross-arm correlation. The derived seeds are printed so the run is reproducible.
    seeds = {k: (a.seed + 1000 * (i + 1) if a.per_arm_shuffle else a.seed)
             for i, k in enumerate(arms)}
    plans = {k: build(seeds[k]) for k in arms}
    plan = plans[arms[0]]
    print("K12 — %d sessions over %d cells (%s ms + no-primer control), shuffled within each "
          "round (seed %d)" % (len(plan), len(cells),
                               ",".join(str(p) for p in primers), a.seed))
    if a.dec != 1:
        print("   ⭐ PRIMER DECIMATION %d \u2014 the primer's AIR duration is ~%.2fx its nominal "
              "ms (C540);\n     the probe runs at dec 1 and the two `lf config` commands are "
              "issued at EVERY dec so the\n     client's own cost is identical across the "
              "comparison." % (a.dec, 1.653 if a.dec == 2 else float(a.dec)))
    print("   cell order per arm: %s"
          % (", ".join("%s=seed %d" % (k, seeds[k]) for k in arms) if a.per_arm_shuffle
             else "⛔ ONE SHARED PLAN — this cap carries no cross-arm correlation (M69)"))
    print("   probe is fixed and at a fixed index; the primer's LENGTH is the only variable")
    # ⭐⭐ M77: THE PROBE IS INSIDE THE SAME BURST, so the reachable top is
    # `LF_TAG_BURST_TARGET_MS - overhead - probe_ms` and it is PER ARM. `nexwatch`'s 98 ms probe
    # puts a 200 ms primer over the edge (arrivals 0.94, twice) where `idteck`'s 49 ms probe
    # leaves the same cell clean — the term that was missing from K19's "~200 ms is the reachable
    # maximum", which was `keri`'s number quoted as the bench's. Printed, not enforced: P1 is the
    # gate, and a warning that could be silenced is worth less than a control that fails.
    for _k in arms:
        _p = PROBES.get(_k) or ""
        _m = re.search(r"-s\s+(\d+)", _p)
        # ⛔ An arm whose probe is its OWN reader still reads samples, and the burst pays for
        # them: `lf_read()`'s count per command is C494's 6x spread. Falling back to 0 made the
        # computed top 308 ms for `keri`, which is nonsense.
        _n = int(_m.group(1)) if _m else READER_SAMPLES.get(_k, 0)
        # ⛔⛔ C548: THE PROBE COSTS ITS ELAPSED, NOT ITS ACQUISITION. `_n * 8/1000` is the
        # acquisition time at 125 kHz and ignores the USB readback, which for a 12,288-sample
        # probe is another 40 ms. Both terms here are now the slope-fitted elapsed (C546).
        _pm = _n * DEC_SLOPE[1]
        # ⭐ AND SO DOES THE PRIMER — 1.41 ms per nominal ms at dec 1, 2.50 at dec 2, NOT the
        # 1.00 and 1.65 the old formula charged. ⇒ the corrected top reproduces BOTH measured
        # burst boundaries with one constant: `idteck` at dec 2 (clean 130, broken 140 —
        # computes to 138) and `nexwatch` at dec 1 (clean 195, failed 200 — computes to 196),
        # where the old form said 157 and 210 and was 12-18 ms optimistic on each.
        _per = MS_PER_NOMINAL.get(a.dec, 125.0 * (0.00792 * a.dec + 0.00371))
        _top = (a.burst - CLIENT_OVERHEAD_MS - _pm) / _per
        _over = [c for c in primers if c > _top]
        print("   %-9s probe %-18s = %5.1f ms  ⇒ computed primer top ~%.0f ms nominal%s"
              % (_k, _p or "(its own reader, %d samples)" % _n, _pm, _top,
                 ("   ⛔ %d cell(s) ABOVE it: %s — expect P1 to fail there (M77)"
                  % (len(_over), ",".join(str(c) for c in _over))) if _over else ""))
    # ⚠⚠ AND THE COMPUTED TOP IS OPTIMISTIC. It puts `nexwatch` at ~210 ms, and its 200 ms cell
    # FAILED P1 twice while 195 held at exactly 0.50 — so the ~192 ms overhead is an
    # underestimate, or it jitters, and nothing has measured which. ⇒ treat the number above as
    # an UPPER BOUND and P1 as the arbiter.
    # ⚠⚠ STILL NOT AN ARBITER, FOR A REASON THAT IS NOT THE OLD ONE. The corrected form lands
    # inside both measured brackets, but it cannot certify a cell within ~10 ms of the edge:
    # `keri`'s K30/K31/K32 top cells compute 4-10 ms OVER a nominal 500 and P1 measured them
    # CLEAN, and the burst is itself clamped to whole emission frames (~16.4 ms for these arms),
    # so the true ceiling is quantised at about that scale. ⇒ a design tool, P1 the arbiter.
    print("   ⚠ that top carries ~±10 ms — the burst is clamped to whole frames (~16.4 ms here)")
    print("     and `keri`'s banked top cells compute just over 500 yet measured clean. P1 decides.")
    print("   elapsed(probe) ~ primer + ~%d ms measured FIELD-UP overhead; one full beat cycle "
          "is %.1f ms\n" % (K12_OVERHEAD_MS, K12_CYCLE_MS))
    out = {}
    try:
        for key in arms:
            arm = shortread.ARMS[key]
            ok, why = seqdump.arm(a.port, arm.typ, arm.econfig)
            if not ok:
                print("%-9s ⛔ ARM FAILED: %s" % (key, why))
                continue
            sc = collections.defaultdict(list)
            arr = collections.defaultdict(list)
            for cell in plans[key]:
                before = playbacks(a.port)
                # ⛔ `reads=1` is the PROBE count. The primer is a separate `lf read` that is
                # never scored — `session()` only scores blocks labelled with the demod
                # command, and the primer is a bare capture.
                scores = session(key, 1, a.timeout,
                                 primer=(0 if cell is None else int(cell * 125)),
                                 dec=a.dec)
                after = playbacks(a.port)
                sc[cell].extend(scores)
                nreads = 1 if cell is None else 2
                if before is not None and after is not None:
                    arr[cell].append((after - before) / float(nreads))
                time.sleep(0.4)
            out[key] = {("none" if c is None else str(c)):
                        {"scores": sc[c], "arrivals": arr[c]} for c in cells}
            # ⭐ M69: the scorer must be able to REFUSE a cap that shared its plan, rather than
            # trusting whoever ran it to remember the flag.
            out[key]["_plan"] = {"seed": seeds[key],
                                 "per_arm_shuffle": bool(a.per_arm_shuffle),
                                 "arms": list(arms)}
            # ⭐⭐ K34: the cap certifies its OWN burst condition. `declared` is what the
            # operator of this run said the build is; `frames` is what the DEVICE answered with a
            # sequence loaded (`lf_tag_em.c:576` recomputes it per sequence). A scorer comparing
            # two builds must check `frames`, never `declared` and never a version string (C461).
            out[key]["_burst"] = {"declared": a.burst,
                                  "frames": seqdump.frames_per_burst(a.port)}

            def cell_rate(c):
                h, n = rate(sc[c])
                return (100.0 * h / n if n else float("nan")), h, n

            def cell_arr(c):
                v = arr[c]
                return sum(v) / len(v) if v else float("nan")

            print("\n%-9s probe %r" % (key, PROBES[key] or arm.reader))
            print("   %-8s %-9s %-7s %-9s %s"
                  % ("primer", "elapsed~", "phase", "arr/read", "rate"))
            for c in cells:
                r, h, n = cell_rate(c)
                if c is None:
                    el, ph = "0", "-"
                else:
                    el = "%d" % (c + K12_OVERHEAD_MS)
                    ph = "%.0f" % ((c + K12_OVERHEAD_MS) % K12_CYCLE_MS)
                print("   %-8s %-9s %-7s %-9.2f %d/%d=%.0f%%"
                      % ("none" if c is None else c, el, ph, cell_arr(c), h, n, r))

            # ⛔ THE BENCH-MOVED CONTROL FIRST, because nothing else is interpretable without it.
            cr, ch, cn = cell_rate(None)
            zero_expected = key in FRESH_BURST_ZERO
            if zero_expected:
                ctl_ok = cn > 0 and cr <= 15.0
                print("\n   control (no primer, fresh burst): %d/%d = %.0f%%  ⇒ %s"
                      % (ch, cn, cr, "PASSES — matches C512's D>=600 and K5's index 0"
                         if ctl_ok else
                         "⛔⛔ BENCH MOVED — C512 and K5 both measured 0% here. "
                         "Nothing below is comparable to K5-K12."))
            else:
                # ⭐ C514: this arm does not collapse on a fresh burst, so there is no 0% to
                # check against. The bench check for it is the ANCHORS, below.
                ctl_ok = True
                print("\n   control (no primer, fresh burst): %d/%d = %.0f%%  ⇒ "
                      "INFORMATIONAL — C514 measured that %s does NOT collapse on a fresh "
                      "burst,\n     so the <=15%% band is `gproxii`'s and does not apply here."
                      % (ch, cn, cr, key))
                # ⛔⛔ THE ANCHORS RULE IS WITHDRAWN (M63). It asked that the first and last
                # cells sit near the arm's median, which PRESUMES A FLAT PROFILE — the very
                # thing under test. On `indala` and `keri` those cells are the wings of a hump,
                # so a healthy K15 run read as a moved bench and both arms were declared
                # uninterpretable by a control that could not tell the two apart.
                # ⭐ Replaced by a SHAPE-AGNOSTIC drift check: does this run agree with itself
                # across its own rounds? That is what licenses within-run structure, and on a
                # bench that wandered 88/60/38/75% in one evening (C497) it is the only thing
                # any control here can honestly certify.
                def half_rate(sel):
                    v = [s for c in primers for s in sel(sc[c])]
                    h, n = rate(v)
                    return (100.0 * h / n if n else float("nan")), n

                fh, fn = half_rate(lambda v: v[:len(v) // 2])
                lh, ln = half_rate(lambda v: v[len(v) // 2:])
                pk = max(primers, key=lambda c: cell_rate(c)[0])
                pf, _ = rate(sc[pk][:len(sc[pk]) // 2])
                pl, _ = rate(sc[pk][len(sc[pk]) // 2:])
                npk = max(1, len(sc[pk]) // 2)
                pfr, plr = 100.0 * pf / npk, 100.0 * pl / npk
                ok_pool = abs(lh - fh) <= 15.0
                ok_peak = abs(plr - pfr) <= 25.0
                ctl_ok = ok_pool and ok_peak
                print("      drift (split-half, shape-agnostic): pooled %.0f%% → %.0f%% "
                      "(Δ %+.0f, n=%d/%d), peak cell %d ms %.0f%% → %.0f%% (Δ %+.0f) ⇒ %s"
                      % (fh, lh, lh - fh, fn, ln, pk, pfr, plr, plr - pfr,
                         "the run agrees with itself — within-run structure is interpretable"
                         if ctl_ok else
                         "⛔ THIS RUN DRIFTED — its structure is not interpretable"))

            # ⭐ T1/T2 — does the hump reproduce, and is it where K15 put it?
            if 60 in primers and 65 in primers and 40 in primers and 80 in primers:
                pk2 = max(primers, key=lambda c: cell_rate(c)[0])
                wings = [cell_rate(40)[0], cell_rate(80)[0]]
                if pk2 in (60, 65) and cell_rate(pk2)[0] >= 75.0 and max(wings) <= 30.0:
                    t = ("⇒ **T1 — THE HUMP REPRODUCES AND IS LOCATED** at %d ms" % pk2)
                elif max(wings) >= 50.0 or pk2 not in (60, 65):
                    t = ("⇒ **T2 — IT DOES NOT.** K15's profile was the wander and the lead is "
                         "withdrawn for this arm")
                else:
                    t = "⇒ between the bands — no location claimed"
                print("   T1/T2 hump: peak %d ms at %.0f%%, wings 40/80 ms at %.0f%%/%.0f%%  %s"
                      % (pk2, cell_rate(pk2)[0], wings[0], wings[1], t))
                print("   T3 cross-arm: this arm at 65 ms is %.0f%% (needs >= 75%%); "
                      "`gproxii` was 100%% in K14 — quoted as a SEPARATE session, not pooled (M59)"
                      % cell_rate(65)[0])

            # ⭐ S1 — is the notch at the SAME lead time as `gproxii`'s? Only asked when the
            # ladder actually contains that cell.
            if 60 in primers:
                r60 = cell_rate(60)[0]
                rest60 = sorted(cell_rate(c)[0] for c in primers if c != 60)
                m60 = rest60[len(rest60) // 2]
                if r60 <= 25.0 and r60 <= m60 / 3.0:
                    s1 = "⇒ **S1 SUPPORTED** — the notch is at the SAME lead time"
                elif abs(r60 - m60) <= 15.0:
                    s1 = ("⇒ **S1 REFUTED** — no notch at 60 ms. ⛔ That is NOT *no notch*: a "
                          "5 ms\n        feature elsewhere in 20..200 ms is invisible to any "
                          "coarse grid.")
                else:
                    s1 = "⇒ no verdict for this arm"
                print("   S1 same-lead-time: 60 ms at %.0f%% against a median of %.0f%%  %s"
                      % (r60, m60, s1))

            # P1 — the mechanism, and the control cell is exempt by construction.
            worst = max(primers, key=lambda c: (cell_arr(c) if cell_arr(c) == cell_arr(c) else 0))
            wa = cell_arr(worst)
            p1_ok = all((cell_arr(c) <= 0.6) for c in primers if cell_arr(c) == cell_arr(c))
            print("   P1 mechanism: worst primer cell is %d ms at %.2f arrivals/read  ⇒ %s"
                  % (worst, wa,
                     "one burst spans primer and probe in every cell"
                     if p1_ok else
                     "⛔ a cell restarted its burst — its elapsed axis does not exist"))
            print("      control cell arrivals %.2f (expected ~1.0 — that IS the fresh burst)"
                  % cell_arr(None))

            # P2 — H_settle.
            bot = [s for c in primers[:3] for s in sc[c]]
            top = [s for c in primers[-3:] for s in sc[c]]
            bh, bn = rate(bot)
            th, tn = rate(top)
            bp = 100.0 * bh / bn if bn else float("nan")
            tp = 100.0 * th / tn if tn else float("nan")
            rs_all = [cell_rate(c)[0] for c in primers]
            steps = [rs_all[i + 1] - rs_all[i] for i in range(len(rs_all) - 1)]
            nondec = sum(1 for s in steps if s >= 0)
            # ⛔ Q3, DECLARED IN THE DOCSTRING BEFORE K13's CAPTURE. The contrast alone is not
            # robust to a single-cell notch: K12's seed-17 run fired it at +21 points with its
            # bottom TWO cells at 96%, above the top three. A rise now needs the shape too.
            contrast = (tp - bp) >= 20.0
            mono = nondec >= 7
            p2 = contrast and mono
            print("   P2 H_settle: bottom three %d/%d=%.0f%% vs top three %d/%d=%.0f%%  "
                  "Δ %+.0f pts (contrast %s)"
                  % (bh, bn, bp, th, tn, tp, tp - bp, "fires" if contrast else "no"))
            print("      Q3 shape: steps %s ⇒ non-decreasing in %d of %d ⇒ %s"
                  % (" ".join("%+.0f" % s for s in steps), nondec, len(steps),
                     "a rise" if mono else
                     "⛔ NOT a rise — the contrast is not carried by the profile"))

            # P3 — H_phase. The pairs are one full cycle apart to within 1.6 ms.
            pairs = [(c, c + K12_PAIR_LAG_MS) for c in primers
                     if (c + K12_PAIR_LAG_MS) in primers]
            diffs = [abs(cell_rate(x)[0] - cell_rate(y)[0]) for x, y in pairs]
            md = sum(diffs) / len(diffs) if diffs else float("nan")
            half = [cell_rate(c)[0] for c in primers if c <= K12_PAIR_LAG_MS]
            hr = max(half) - min(half)
            p3 = (md <= 15.0) and (hr >= 30.0)
            if not pairs:
                print("   ⚠ this ladder carries NO pair %.0f ms apart, so P2/P3/P4 are not the "
                      "operative bands\n     for it — they are printed for completeness only."
                      % K12_PAIR_LAG_MS)
            print("   P3 H_phase: mean |Δ| over %d pairs %.0f ms apart = %.0f pts; "
                  "within-half range %.0f pts ⇒ %s"
                  % (len(pairs), K12_PAIR_LAG_MS, md, hr,
                     "PERIODIC at C509's cycle" if p3 else
                     "⛔ NO POWER — the profile is too flat for the pairs to mean anything"
                     if hr < 30.0 else "not periodic at the pre-registered 15 pts"))

            # P4 — H_index / flat.
            rs = [cell_rate(c)[0] for c in primers]
            span = max(rs) - min(rs)
            p4 = span <= 15.0
            print("   P4 H_index: range across the %d cells %.0f pts ⇒ %s"
                  % (len(rs), span, "FLAT — elapsed time does not matter"
                     if p4 else "not flat"))

            # ⭐ Q1/Q2 — the notch. Reported as a NAMED cell, not as "the minimum", so a run
            # with no notch cannot dress its lowest cell up as one.
            lo = min(primers, key=lambda c: cell_rate(c)[0])
            lo_r = cell_rate(lo)[0]
            rest = sorted(cell_rate(c)[0] for c in primers if c != lo)
            med = rest[len(rest) // 2]
            notch = lo_r <= 25.0 and med >= 75.0
            print("   Q1/Q2 notch: lowest cell is %d ms at %.0f%%, median of the other nine "
                  "%.0f%% ⇒ %s"
                  % (lo, lo_r, med,
                     "A NOTCH — cell-specific and it needs its own explanation" if notch
                     else "no notch at the pre-registered bands"))

            # ⭐ R1/R2/R3/R4 — the notch's WIDTH, read off the ladder's own neighbours so it
            # works whatever the ladder is. ⛔ The neighbours are the cells ADJACENT IN THE
            # LADDER, not a fixed 20 ms away: on K14's fine ladder they are 5 ms away.
            i = primers.index(lo)
            nb = [primers[j] for j in (i - 1, i + 1) if 0 <= j < len(primers)]
            nbr = [cell_rate(c)[0] for c in nb]
            narrow = notch and len(nbr) == 2 and min(nbr) >= 75.0
            wide = [c for c in primers if abs(c - lo) <= 2 * (primers[1] - primers[0])]
            broad = all(cell_rate(c)[0] <= 40.0 for c in wide) and len(wide) >= 3
            print("   R2/R3 width: %d ms at %.0f%%, ladder neighbours %s at %s ⇒ %s"
                  % (lo, lo_r, nb, ", ".join("%.0f%%" % x for x in nbr),
                     "NARROW — under one ladder step either side" if narrow
                     else "BROAD — the dip covers %d cells" % len(wide) if broad
                     else "width unresolved at the pre-registered bands"))

            # ⭐ Q4 — the NO POWER statement, computed with the notch cell excluded.
            nine = [cell_rate(c)[0] for c in primers if c != lo]
            q4 = (max(nine) - min(nine)) <= 20.0 and min(nine) >= 75.0 and ctl_ok
            print("   Q4 no-power: the nine cells off the notch span %.0f pts, min %.0f%%, "
                  "control %.0f%% ⇒ %s"
                  % (max(nine) - min(nine), min(nine), cr,
                     "the 0%%→~%.0f%% transition is entirely BELOW the ~%d ms floor"
                     % (med, K12_OVERHEAD_MS) if q4
                     else "the profile above the floor is not saturated-flat"))

            if not ctl_ok:
                v = ("⇒ ⛔⛔ NO VERDICT — the bench-moved control failed and that decides it "
                     "before any P does.")
            elif not p1_ok:
                v = ("⇒ ⛔ NO VERDICT — P1 failed, so at least one cell's elapsed axis does not "
                     "exist.")
            elif q4:
                v = ("⇒ **NO POWER ON THE SHAPE, AND IT LOCATES THE PROCESS** — every cell off "
                     "the notch is\n     saturated while the no-primer control is at %.0f%%, so "
                     "the whole transition happens\n     inside the ~%d ms floor this design "
                     "cannot reach. Pre-registered NO POWER branch.%s"
                     % (cr, K12_OVERHEAD_MS,
                        "\n     ⛔ AND THE NOTCH AT %d ms IS UNEXPLAINED BY EITHER HYPOTHESIS."
                        % lo if notch else ""))
            elif p4 and min(rs) >= 85.0:
                v = ("⇒ **NO POWER ON THE SHAPE, AND IT LOCATES THE PROCESS** — every primer "
                     "cell is saturated\n     while the no-primer control is at %.0f%%, so "
                     "whatever happens, happens inside the ~80 ms\n     floor this design "
                     "cannot reach. That is the pre-registered NO POWER branch." % cr)
            elif p4:
                v = ("⇒ **H_index** — twelve cells within 15 points, so elapsed time since the "
                     "field arrived is\n     not what the rate depends on.")
            elif p2 and p3:
                v = ("⇒ ⛔ **NO VERDICT** — a rising sawtooth satisfies P2 and P3 at once and "
                     "this n cannot\n     separate them. Pre-registered as no verdict.")
            elif p2:
                v = ("⇒ **H_settle SUPPORTED** — the rate rises with time since field arrival "
                     "at a fixed index.")
            elif p3:
                v = ("⇒ **H_phase SUPPORTED** — the rate is periodic in elapsed at C509's "
                     "121.6 ms, which is a\n     number from a different experiment. H_settle "
                     "is not needed to explain the profile.")
            else:
                v = "⇒ NO VERDICT — none of the four pre-registered bands fired."
            print("   %s" % v)
    finally:
        # ⛔ ALWAYS, NOT ONLY ON THE HAPPY PATH (AUTOPILOT §2a).
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
    ap.add_argument("--k12", action="store_true",
                    help="⭐ K12: sweep the PRIMER's length — field-UP time before a fixed "
                         "probe at a fixed index, separating elapsed time from read index")
    ap.add_argument("--primers", help="K12: comma-separated primer durations in ms")
    ap.add_argument("--dec", type=int, default=1,
                    help="⭐ K30/C539: LF decimation for the PRIMER only, reset to 1 before the "
                         "probe. At dec N the same `-s N` takes longer in real time with the "
                         "carrier, field and emission untouched \u2014 measured stretch 1.653x at "
                         "dec 2, NOT 2x, because the USB readback does not scale (C540/M79). "
                         "\u26d4 The two `lf config` commands are issued at EVERY dec including 1 "
                         "so the CLIENT's cost is identical across the comparison.")
    ap.add_argument("--burst", type=int, default=LF_BURST_MS,
                    help="⭐ K34: the firmware's LF_TAG_BURST_TARGET_MS for THIS build. It "
                         "does not SET anything — the constant is compiled in — it tells "
                         "`_top` which budget to compute against, and it is recorded in the cap "
                         "beside the frames-per-burst the DEVICE reports, so a cap certifies its "
                         "own burst condition instead of trusting whoever ran it (M45/C461).")
    ap.add_argument("--per-arm-shuffle", dest="per_arm_shuffle", action="store_true",
                    help="⭐ K21/M69: give each arm its OWN derived cell-order seed. Required "
                         "for any cap a CROSS-ARM correlation will be computed on; without it "
                         "every arm shares one plan and position leaks in identically.")
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
    if a.k12:
        # ⭐ K32's ladder is at 2.5 ms, so a primer may be fractional. ⛔ An INTEGRAL value must
        # still render as "100" and not "100.0", or every banked cap's cell keys and every
        # scorer's ladder matching would break.
        pr = ([(int(float(x)) if float(x).is_integer() else float(x))
               for x in a.primers.split(",")] if a.primers else K12_PRIMERS_MS)
        return k12(a, arms, pr)
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
