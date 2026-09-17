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
}
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


def session(arm_key, reads, timeout, delay=0, lead=0, primer=0):
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
        cmds.append("lf read -s %d" % primer)
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


def k12(a, arms, primers):
    """⭐ K12. Primer LENGTH varies the field-UP time before a fixed probe at a fixed index.

    ⛔ The cell list carries `None` for the no-primer control, and it is shuffled in with the
    rest so it cannot sit at a fixed position (M60)."""
    import collections
    import random as _r
    rng = _r.Random(a.seed)
    cells = [None] + list(primers)
    plan = []
    for _ in range(a.reps):
        rnd = list(cells)
        rng.shuffle(rnd)                    # ⭐ shuffled WITHIN each round, not once globally
        plan.extend(rnd)
    print("K12 — %d sessions over %d cells (%s ms + no-primer control), shuffled within each "
          "round (seed %d)" % (len(plan), len(cells),
                               ",".join(str(p) for p in primers), a.seed))
    print("   probe is fixed and at a fixed index; the primer's LENGTH is the only variable")
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
            for cell in plan:
                before = playbacks(a.port)
                # ⛔ `reads=1` is the PROBE count. The primer is a separate `lf read` that is
                # never scored — `session()` only scores blocks labelled with the demod
                # command, and the primer is a bare capture.
                scores = session(key, 1, a.timeout,
                                 primer=(0 if cell is None else int(cell * 125)))
                after = playbacks(a.port)
                sc[cell].extend(scores)
                nreads = 1 if cell is None else 2
                if before is not None and after is not None:
                    arr[cell].append((after - before) / float(nreads))
                time.sleep(0.4)
            out[key] = {("none" if c is None else str(c)):
                        {"scores": sc[c], "arrivals": arr[c]} for c in cells}

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
                med_all = sorted(cell_rate(c)[0] for c in primers)[len(primers) // 2]
                anch = [c for c in (primers[0], primers[-1])]
                ok_a = all(abs(cell_rate(c)[0] - med_all) <= 20.0 for c in anch)
                print("      anchors %s at %s against a median of %.0f%% ⇒ %s"
                      % (anch, ", ".join("%.0f%%" % cell_rate(c)[0] for c in anch), med_all,
                         "the bench-moved check PASSES"
                         if ok_a else "⛔ anchors off the median — this arm's run is NOT "
                                      "interpretable"))
                ctl_ok = ok_a

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
        pr = ([int(x) for x in a.primers.split(",")] if a.primers else K12_PRIMERS_MS)
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
