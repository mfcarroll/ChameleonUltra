# AUTOPILOT — unattended LF-protocol run

⛔ **This file is the contract.** Context auto-compacts; this file plus the git history is
the only state that survives. Keep §1 and §4 current or the next session starts blind.

⭐ **Single-threaded, always.** The operator has asked for steady serial work.
⛔ **NEVER launch a workflow, and never spawn a subagent.** No `Workflow`, no `Agent`, no
fan-out of any kind. One session, one unit at a time. This is not a pacing suggestion — a
fan-out mid-flight corrupts captures and duplicates bench work on shared hardware.

| | |
|---|---|
| repo | `/Users/Shared/code/personal/rfid/ChameleonUltra` |
| branch | `indala-psk-read` — push to `origin` (= `mfcarroll/ChameleonUltra`, the operator's fork) and **nowhere else** |
| heartbeat | `/tmp/indala_autopilot.heartbeat` |
| plumbing | `research/indala-psk-read/autopilot.sh` — `beat` / `gate` / `status` |

---

## 0. ON RESUME — do exactly this

1. `cd research/indala-psk-read && ./autopilot.sh status`
   ⛔ If it says **PORT HELD**, another session is driving that device: skip every device
   unit and take a pure-compute one instead (§2 marks which are which).
   ⛔ If fewer than 4 devices enumerate, that is **not** a code problem — record it in §5
   and take a compute unit. Do not debug firmware against a bench that is not there.
2. `./autopilot.sh beat`
3. Read §1 (STATE), then §2 (QUEUE) — the **EXECUTION ORDER** box overrides the numbering.
4. Read `FINDINGS.md` and `METHOD.md` if this is a fresh context. `NEXT.md` is the plan;
   this file is the queue. They must not disagree — if they do, `NEXT.md` wins and you fix
   this file.
5. Take the first unit that can be **finished**. Work it to completion, verify it on
   hardware, commit the code and its notes together, `beat`, and go to the next.
6. Never ask a question. If blocked, append the blocker to §5, add it to `NEXT.md`'s
   **Needs hands** table, and take the next unit.

---

## 1. STATE

### ✅ 2026-09-14 02:45 — THE WRITE COLUMN IS MEASURED; THE ASK/BIPHASE FAMILY IS NEXT

⭐ **U15 IS CLOSED, AND THE WHOLE WRITE COLUMN WENT WITH IT (C330, C331).** `./regrade.sh` re-graded every write arm in the tree against an unlocked tag — all **18 protocols at 4 of 4: 72 writes, 72 independent Proxmark reads, 0 failures**, every raw frame byte-identical to what was sent. ⛔ **C155's Indala224 caveat is retired**: the 224-bit write verified 4 of 4. ⚠ NexWatch and FDX-B are judged on their own reader commands, not `lf search`, which prints only a protocol name for them. ⛔ **Nothing was ever wrong with the write arms**: C305's *0 of 9* was measuring the password lock our own HID writer had set (C325), and the password defect is fixed (F1, C326). `NEXT.md`'s grid write column is rebuilt from this run.

⭐ **NEXT IS §10's ASK/biphase family — U10 (FDX-B).** §10 is organised by modulation family and a family's protocol is finished completely before the next is started.

⛔ **EMULATION IS OFF THE TABLE while #2 is on the Proxmark.** It needs the Flipper as reader, and the Flipper faces #1 (deliberately un-reflashed, holds the NexWatch slot). The Proxmark cannot hear our PWM emulation — the dead-end note at the top of `NEXT.md`.

⚠ **Open, needs hands, not for an unattended run:** Indala emu slot 1 holds a bad frame (*not the Indala preamble*); EM410X emulation hit BOTH `flipper.py` arms, unexplained; the Flipper crash-rebooted at an unknown time.

---

### (historical) 2026-09-15 12:30 — WHERE THE READER WORK STANDS

⭐ **Rewritten today.** The paragraph this replaces was the 09:50 one, written 45 claims ago and
accurate then. `LOG.md` keeps the history; §1 is what a fresh context inherits, so it states
what is true now. Same discipline C286 applied to `NEXT.md` §9f.

**THE BENCH, verified 2026-09-15 12:30.** All four devices enumerate. **Chameleon #2 runs
`59933d8` — a clean build of HEAD (`v2.2.0-547`, no `-dirty`), reflashed 2026-09-15 21:15 and
confirmed by `hw version`; the HID read arm verifies 4 of 4 on it** — and reads the
T5577 4 of 4 after the flash. **#1 is deliberately NOT reflashed**: C289 showed by per-file diff
that nothing changed today can reach the five working emulate arms, so flashing it would have
risked the unit holding the NexWatch slot for a check that could not fail. The tag holds **HID
H10301 FC 123 / CN 4567**, `parity ( ok )` per the Proxmark — **written by OUR writer, because the
Proxmark's write path stopped working mid-session and reports success anyway (C297, §5).** `make check` is green on all four
arms — round trips, ambiguity counts, the `wiegand_other_matches` cross-check, and the
320-capture decoder comparison.

**C45 IS CLOSED (C250).** The HID reader's old 15-20% intermittency was a BLE advertising burst
collapsing the field mid-capture; `cf745fb`'s guard fixes it — 96/96 with, 71/80 without, on one
tag in one session, p = 6.4e-4.

**FOUR ACCEPTANCE GATES WERE ADDED OR CORRECTED, each verified by breaking one named bit on a
real tag**: Securakey's ten zero spacers (C253), GProxII's Wiegand parity over the descrambled
credential (C255), Indala26's bits 60 and 61 (C257, which makes us *stricter than the Proxmark* —
it reads frames we now refuse), and Keri's frame repeat (C258, affordable only because its
capture window is 8192 where Indala26's is 4096). **C288 then re-verified all twelve read arms
together — 4 of 4 each — and re-rated HID at 24/24, so the stricter gates cost nothing on real
tags.**

**THE HID FORMAT WALK IS THE BIGGEST FINDING AND IT IS UPSTREAM'S.** `unpack()` returns the
FIRST Wiegand layout that fits. On real tags **15 of 29 formats come back as a different format
with a different credential** (C284) — a Kastle tag holding fc 1 / cn 1 reports as Check Point
card 8389632 — because **12 of 31 unpackers have no rejection path at all** (C285, count corrected and MEASURED by C300). It cannot
be narrowed into correctness; only enumerating or pinning works. ⇒ We now do **both**: the
reader names the other layouts that fit (C287, `wiegand_other_matches()`, verified against the
Proxmark's parity-passing candidate list 3 for 3) and warns when no format was pinned. All of
that is `NEXT.md` §9f, rewritten strongest-first in C286.

**U14 — THE OPERATOR-REQUESTED REFERENCE AUDIT — IS DONE (C291, `NEXT.md` §11).** Three models:
the Proxmark validates structurally and never requires two reads to agree; the whole Flipper
family validates only by repetition, N consecutive byte-identical reads; we do both. ⭐ The
Flipper family uses **6 for every PSK1 protocol and 3 for everything else** — the same family
C190 found fragile from the emulation side. **C292 then showed our own count of 2 is enough**
(front corpus 0 wrong in 160 captures; phasebits 13 wrong and all distinct), and **C294 settled
the mechanism by entailment**: the reader returns only frames that matched their predecessor at
the same sample phase, so what defeats the rule is deterministic distortion, never coincidence.

⛔ **THE STANDING CAUTION, measured three ways.** Gates narrow the window, agreement narrows it
further, and **neither closes it**. A valid tag at marginal field produced 9 wrong credentials
in 92 captures (C267) — all distinct, so agreement caught them all (C268). A tag whose frame is
broken produced 12, and one recurred (C269). ⇒ Quote that to a reviewer rather than any single
number.

### ✅ 2026-09-14 07:50 — THE PASSWORD DEFECT, AND WHAT IT RETIRES

⭐⭐⭐⭐ **THE BIGGEST FINDING OF THIS BRANCH IS AN UPSTREAM DEFECT, AND IT IS FIXED (F1, C322-C326).**
**Writing any T5577 with a ChameleonUltra silently password-protected it.** All 8
`T5577_*_CONFIG` constants carried `T5577_PWD`, and the key came from two places —
`chameleon_cmd.py`'s globals (`new_key = 20206666`) for hidprox/em410x, a function default
(`51243648`) for indala/indala224/gproxii/awid — with neither documented. A tag locked by
`lf hid prox write` could not be opened by `lf gproxii write`, by the Proxmark, or by anything
else. Two tags here were unrecoverable until the key was read off a third.

**Fixed and verified on hardware.** Password protection is opt-in: `T5577_PWD` is out of the
config constants and `write_t55xx()` sets it only when a non-zero key is supplied; host `new_key`
is zero and `old_keys` is `[20206666, 51243648, 19920427]` so previously-locked tags stay
writable. A blank tag written with the fix reads `Block0 00107060`, **`Password set: No`**,
block 7 `00000000`.

⛔⛔ **WHAT THIS RETIRES — DO NOT BUILD ON ANY OF IT.**
- **C305** — *"a write lands iff the config word already matches"* — DISSOLVED. The writes that
  failed carried the wrong password. Nothing to do with config words.
- **C299 / C306** — the sandwich geometry, coil coupling, BLE bursts, *"pm3's downlink is dead"*.
  All one locked tag. pm3 was never faulty.
- **C324** — a "deterministic corruption" I invented for a constant I had not looked up.
- **C309 / C310's framing** — our read command was well-formed and *refused*, not ignored.
- The `0EAAACAA` and `80000000` constants were demodulation artefacts of a failed read, nothing
  more.

⭐ **`FIXES.md` IS NEW AND IS THE PLACE FOR THIS CLASS OF WORK.** Six defects found while doing
something else, **all six now fixed and hardware-verified**, each scoped as its own upstream PR
and each present on `main`: F1 the password defect, F2 five writers reporting success having
written nothing, F3 a header declaring a lock bit as a length, F4 `unpack()` relabelling 15 of 29
formats, F5 two 28 KB capture buffers resident at once, F6 the HID writer asserting success
without reading back. ⇒ Keep adding to it rather than folding these into the protocol work.

**THE BENCH.** All four devices enumerate. **#2 runs `9d39c15` — a clean build of HEAD
(`v2.2.0-592`, no `-dirty`)**. #1 is deliberately NOT reflashed and still holds the NexWatch
slot. Both previously-locked tags are recovered; the working tag holds **H10301 FC 42 / CN 999**.
⚠ **The sandwich is DISASSEMBLED** — it was taken apart chasing C299, which turned out to be the
password. ⚠ The Flipper reported a crash-and-reboot at an unknown time; rig A, unexamined.

⇒ **NEXT: re-grade the write arms.** GProxII, AWID, Keri, Indala, NexWatch, Gallagher, Securakey
and Noralsy were all scored against a password-locked tag and their results are void. That is the
first job, and it is pure unattended work.

### ✅ 2026-09-15 20:45 — WHAT CHANGED SINCE THE 12:30 PARAGRAPH ABOVE

⭐ **BOTH FORMER "DO NOT TOUCH" ITEMS WERE RELEASED BY THE OPERATOR and are DONE.**

**`unpack()` IS FIXED IN PLACE (C302), NOT SUPPLEMENTED.** The walk makes two passes and
prefers a format that can VALIDATE; the card it returns carries `matches`, `verified` and
`others`, and `wiegand_other_matches()` is deleted so the count cannot drift from the winner.
⭐ The key was already in the table — `fields.has_parity`, which C300 proved accurate by
measurement: the 12 rows flagged 0 are exactly the 12 that accept 100% of random frames. ⇒
Never-reads-back-as-itself went **14 → 12**, and **KASTLE — C284's headline failure — now reads
back as itself 4 of 4 on hardware** (C304). ⛔ Not a cure: where nothing validates there is
nothing to prefer.

**THE INSTRUMENTATION IS COMPILE-GATED, NOT STRIPPED (C303).** `LF_RESEARCH_CMDS_ENABLED`,
default **0** in `data_cmd.h`, set to 1 by one `application/Makefile` line that an upstream PR
deletes. Cost measured both ways: **1,088 B flash + 4,008 B RAM**, the RAM attributed to the
byte by `nm`. ⚠ The GProxII failure-energy payload needed `#else` rather than a plain gate —
it changes a failure reply's WIRE SHAPE.

**THE HARNESS GREW TWO ARMS** — `ambig` now measures per-format selectivity through the
shipping `unpack()`, and new `gates.c` measures every reader acceptance gate in bits (C301).
⚠ **C301 found 4 of 11 formats carry no gate**, and INSTAFOB and IDTECK carry no
`require_repeat` either — 32 preamble bits and nothing else.

⛔⛔ **THE BENCH HAS A NEW AND SERIOUS CONSTRAINT (C305): IT CANNOT CHANGE THE TAG'S PROTOCOL.**
A write lands **iff the config word it writes already matches the tag's**. `lf hid prox write`
5 of 5 (it writes the config already there); GProxII, AWID, Keri and Indala **0 of 9**. ⭐ HID and
GProxII write the SAME 4 blocks to the SAME addresses through the SAME function with the SAME
password, and differ ONLY in the 32-bit values (L273). After
all nine failures pm3 read the credential bit-identical. ⇒ **Any unit needing a non-HID
credential on the T5577 is blocked.** ⛔ The mechanism is NOT established — it is a rule fitted
to 14 observations. The leading candidate is block 0 being locked *if* a rejected write also
aborts the session; `blk_count == 0` is dead (all writers return `LF_TAG_OK`, never `PAR_ERR`).

⭐ **pm3'S DOWNLINK IS BROKEN WHILE ITS LISTENING IS INTACT (C306)** — `lf search` is perfect on
every attempt, but `lf t55xx detect` fails and `lf t55xx read` returns ONE IDENTICAL WORD for
every block, which cannot be real data. A second, independent symptom class for C297/C299.

⭐ **`./benchab.sh <label>` IS THE BENCH PROCEDURE — run it, do not improvise.** One battery,
six sub-tests, self-verifying restore, appending comparable blocks to `bench-ab.log`. The
`before` baseline is already captured (C311). ⛔ **Order: `before` → `nochamp2` → `opened`.**
`nochamp2` means power down Chameleon #2 and change NOTHING else, and it MUST run before the
stack is opened — that one action is the whole C299 test and opening the stack destroys it.

⇒ **NEXT.** Two things need HANDS and neither has moved: **power down Chameleon #2 and retry
`lf hid clone`** (one action, no disassembly — settles C299), then **lift the T5577** for the
emulate-arm re-grade. ⭐ The one compute unit that would unblock C305 is a **T5577 block READ**:
the firmware has none, but `t55xx_send_cmd()` already expresses a direct read — its own comment
documents `2op(1+bck) 1(0) 3addr`, which is `data = NULL`. Reading block 0 would settle the lock
question outright.

### ✅ 2026-09-14 01:30 — WHERE THE EMULATOR WORK STANDS, IN ONE PARAGRAPH

Every protocol Momentum carries READS and WRITES here except FDX-A and InstaFob, which nothing
on this bench can verify. All ten write arms and every read arm that CAN face a real tag now
do. Five emulate arms work and were re-verified on the current build (C241).
**Two emulators are silent, and one of them is now explained.**

✅ **GProxII: SOLVED, and it is not fixable as designed.** A held-level PWM entry emits
NOTHING — proved by making two of em410x's 64 entries held, watching a 4/4 arm go to 0/4, and
restoring it (C242). A biphase 0 IS a held level, so half the frame is silence.
⇒ biphase emulation needs a different mechanism entirely, not a better emitter.

⭐⭐ **C45 IS CLOSED (C250).** The HID reader's 15-20% was a BLE advertising burst collapsing the
field mid-capture, and `cf745fb`'s guard fixes it: 96/96 with the guard, 71/80 without it on one
tag in one session. ⛔ **The finding that outlives it is that an unguarded read returned FIVE WRONG
CREDENTIALS as successes — past a parity check that was working.** `unpack_h10301()` checks both
bits and the reader does reach it; my first write-up said otherwise and was wrong. ⭐⭐ **SEPARATED (C251): it is mostly the relabelling.** Six of seven wrong reads came back as
`Indala 26-bit` hugging this tag's own ind26 reading; `-f H10301` cuts it 7/48 → 1/48. Both halves
are on `main`, so it is UPSTREAM'S defect to report, not ours to patch quietly. ✅ **Written up as `NEXT.md` §9f.** ⇒ **Next: back to the queue.** ⛔ Do not "fix" `unpack()` on this branch — the
format walk is load-bearing for every reader that guesses a format, and changing it is a decision
for upstream, not a side effect of an LF research branch.

⛔ **AWID: STILL OPEN.** Every one of its entries is a 50% square, so C242 does not touch it.
Eight explanations are dead by measurement: counter_top magnitude, entries per bit, AC
coupling, duty shape, the design itself (checked against Momentum's own demodulator AND its
own encoder), the buffer plumbing, the emulation engine, and now held levels.
⇒ **The only reader that can hear rig A's emission is the Flipper, and it is the thing under
test.** Either the T5577 comes out of the sandwich so the Proxmark can hear Chameleon #2, or
the two Chameleons face each other.

 — updated 2026-09-13 01:30

- **Last landed:** U1-U4 done. **NexWatch complete**, PSK1 family closed (C164-C167), and
  C162 re-tested at n=70 with half of it retracted (C168).
- **In flight:** nothing. **InstaFob READS on device** (C186, C187), ships read-only, and its
  terminator is now MEASURED at 98-99 samples of frame-boundary excess (C188).
  ⛔ **Its emulate arm is deliberately NOT attempted**: the gap does not decompose into clean
  Manchester runs and Momentum decodes it with a six-state machine, so an emitter would be a
  fitted guess against a shape measurable only approximately. Revisit only with better
  captures or a reading of that state machine's transitions.
- **U7 is DONE** — §9 of NEXT.md now carries an evidence grade per arm, the three blockers a
  reviewer hits before the protocols, and a three-PR split in dependency order (L151).
- ⭐⭐ **THE TAG IS OUT (operator lifted it 2026-09-13). Rig B is now Proxmark <-> Chameleon #2.**
  ⛔ **Its measurements found C190: our emulation is read 16/16 (ASK), 14/16 (PSK2) and
  3/21 (PSK1) by the Proxmark.** The cost of our free-running clock falls ONLY on
  absolute-phase encoding; differential and amplitude are invariant to phase drift (C74).
  ⇒ **DO NOT re-grade the PSK1 emulate arms to A** — the honest grade is "A against a Flipper,
  fails against a Proxmark". Indala224 and the ASK protocols DO deserve A.
  ⚠ C189 said this was an ASK-vs-PSK split; it was published from n=2 and is corrected.
  ⚠ A slot's LF TYPE CHANGE needs `hw mode -r` then `-e` before it takes effect.
- ⚠ Rig A stays as it is — the operator confirmed the Chameleon there CANNOT reach a tag, so a
  tag on that pad would only be readable by the Flipper and would corrupt the emulation path.
- **(historical) The lift was requested for this reason:**
  ⇒ **On resume, CHECK WHETHER IT IS OUT** — one `lf search` on the Proxmark: no tag means it
  was lifted. If it is out, the highest-value unit is **upgrading every emulate arm from grade
  B to grade A** by having the PROXMARK read our emulation on Chameleon #2 (Indala224, Keri,
  NexWatch, Gallagher, Securakey, Noralsy). That answers C179 directly and is why the lift was
  requested. ⛔ With the tag out, U8 and any read/write work are impossible — do the emulation
  sweep and the compute units instead.
- ⭐ **IF THE TAG IS STILL IN, the order is: compute units FIRST, U8 LAST.** The compute queue
  is better work than FSK and needs no bench: (1) ~~the `lf_psk1_read` rename~~ **DONE (C192)**; (2) ~~roundtrip arms for the ASK protocols~~ **DONE (C191)** — three arms added, sensitivity
  proven by a deliberate break, and `make check` now rebuilds from scratch after a false-pass
  bug was found in it; (3) InstaFob's emitter rebuilt from Momentum's
  six-state terminator machine rather than from an approximate capture (C188).
- ⭐⭐ **U8 (FSK) IS NOW THE NEXT UNIT, and its deferral reason is retracted (C193).** The
  physical layer is confirmed at 99% on our sampler, and the read arms need NO tag — the
  Flipper emulates AWID/Paradox/Pyramid into Chameleon #1 exactly as it did for InstaFob.
  ⭐⭐ **AWID READS ON DEVICE — 5/5, null 0/3, 4/4 on a changed payload (C196).** The FSK
  family is open and was the CHEAPEST of the three attempted, not the most expensive.
  ⭐⭐ **THREE FSK PROTOCOLS READ ON DEVICE** — AWID, Paradox, Pyramid (C196, C198).
  ⭐⭐ **THE T5577 GOES BACK INTO RIG B (the Proxmark sandwich) for the day, 2026-09-13.**
  ✅ **The tag is back and the READ upgrade is DONE (C201)** — AWID, Paradox and Pyramid all
  read real tags byte-exactly. ⛔ FDX-A could not be upgraded: pm3 has no FDX-A clone.
  ✅✅ **THE FSK WRITE ARMS ARE DONE AND VERIFIED THE STRONG WAY (C202, C203).** We write the
  T5577 from Chameleon #2, the **Proxmark** reads it: AWID 5/5, Paradox 4/4, Pyramid 4/4, every
  block dump identical to the Proxmark's own clone of the same credential, every credential
  changed from C201's, every tag wiped first with the null confirmed. ⭐ `ctest/roundtrip.c`
  also pins the three block vectors — the first host coverage any T5577 writer here has had.
  ⛔ **There is no FDX-A writer and there must not be one**: `lf fdx` is FDX-B, so nothing on
  this bench can read an FDX-A tag we wrote. That is a refusal, like InstaFob's (C185).
  ⇒ **Next: see §2's EXECUTION ORDER box.** The FSK family is complete on both read and write
  for everything verifiable, so the queue moves on.
  ⚠ With the tag IN, the Proxmark can no longer read our emulation — but that sweep is
  COMPLETE (C190) and the four FSK protocols have no emulator, so nothing is lost.
  ✅ **FDX-A reads on device 4/4 (C199). All four FSK protocols read on hardware.**
  (see §5). It was NOT a flash-targeting problem — the device answers nothing, so every DFU
  trigger was a no-op and only the script's untargeted one ever worked. ⇒ When #1 is back:
  trigger DFU on its `/dev/cu.` port, confirm `nrfutil device list` shows exactly one
  nordicDfu, then run `nrfutil device program --firmware objects/ultra-dfu-app.zip --traits
  nordicDfu` DIRECTLY — never the script, whose own trigger picks the wrong unit.
  **Then: FDX-A on hardware, and the FSK writers when the tag returns.**
  ⚠ **After a flash, wait ~15s before querying capabilities** — three "failed" flashes on
  2026-09-13 were the check racing re-enumeration at 6s while the script reported success
  each time (L162). Read the flash script's OUTPUT rather than discarding it.
  ⚠ AWID's WRITER is deferred until the tag is back — the Proxmark can verify it then.
  ⚠ Paradox and Pyramid share the tones and the pulse counts — only preamble and payload
  layout differ, so they should be format entries rather than new decoders.
  ⚠ Only the WRITE arms need the tag back in the sandwich.
- **(superseded) U8 was deferred because it reused the HID machinery** — it no longer does.
  ⛔ Deliberately last regardless: it reuses the HID Prox/ioProx SAADC machinery and HID's
  15-20% intermittency (C45) is unexplained and lives in exactly that path. Do not start it
  without saying so in §4 first.
- ⭐ Also open and cheap: the ASK protocols have no `ctest/roundtrip.c` arms — the PSK ones all
  do. That harness exists because three wrong encodings shipped in one session (C156).
- **Then:** U7 (upstreaming prep, pure compute) and U8 (FSK, deliberately last).
- ⛔⛔ **The ASK readers now SWEEP DRIVE (C182)** — Noralsy decodes at drive 7 and at no other
  setting, including stock. Every ASK capture in this campaign was taken at `--drive 7`, so
  a host decode does NOT predict a device read unless the reader sweeps too.
- ⛔⛔ **C177 was RETRACTED: `flipper.py`'s success matcher could not express a protocol name
  containing a space, so a working emulation reported 0 of 6.** Momentum calls Securakey
  "Radio Key". ⇒ When a NEW protocol's emulation reads zero, check what Momentum NAMES it
  before believing the number.
- ⚠ **Standing limitation (C179):** every emulate arm here is verified by the Flipper alone.
  A T5577 read by the Proxmark is actual hardware behaviour and can differ at frame
  boundaries we do not emit. The write arms carry that stronger evidence; the emulate arms
  need the tag lifted out of the sandwich.
  ⭐ **Partly answered for Gallagher (C180)**: the real tag's frame period is exactly nominal,
  12 intervals at 3072 ± 1 sample, so its sequence terminator causes no displacement and our
  plain loop matches at the boundary. `framedrift.py` does this with no hands.
  ⭐ **Now answered for BOTH ASK protocols (C180): 20 frame-to-frame periods, none showing a
  terminator gap.** `framedrift.py` agrees with the shipping decoder on every capture.
  ⛔ **The fix was NOT the low-pass sweep I predicted** — it was the slicing reference: a
  trailing moving average lags where the firmware's block means do not. ⇒ Any tool reasoning
  about a decoder must SHARE its front end, not resemble it.
- ⭐ **Chameleon #2 carries `f536b44`** (Securakey gate, confirmed by `hw version`); ⚠ **the T5577
  now holds HID `H10301 --fc 123 --cn 4567`, and **Chameleon #2 is on HEAD** (`656cc61`) — the audits and
  field sweeps cycled it through seventeen credentials in one session; before that Securakey, IDTECK, FDX-B, Keri, Indala, GProxII; earlier today it held Securakey `7FCB400001ADEA5344300000`, not the Pyramid credential §1 used to name.**
- ⭐ **(superseded) #2 carried `a049bc6`** (guard restored, confirmed by `hw version`); #1 was not
  reflashed today and still carries the pre-probe build. ⭐ **Both Chameleons carry the current build.** Rig A (#1) is in emulation mode holding a
  NexWatch slot; put it back to `hw mode -r` before using it as a reader.
- **Driver:** session cron job `9530f401`, every 5 minutes at off-minutes. ⭐ Cron fires
  ONLY while the REPL is idle, so it cannot double-drive a turn that is still working —
  which is why it is both the driver and the watchdog. ⚠ It is session-only: it dies if the
  session is closed, and auto-expires after 7 days. Re-seed from §6.
- **Bench, RE-CHECKED 2026-09-13 09:17 and every pairing confirmed live:** all four devices
  enumerate; both Chameleons answer `hw version` and are in **Tag Reader** mode (no `hw mode -r`
  needed today); the Proxmark reads the tag and so does Chameleon #2, byte-identically.
  T5577 holds C201's last write — **Pyramid FC 123 card 11223**, raw
  `00010101010101010101016eb35e5da4`, FSK2a, block 0 `00107080`.
  ⭐⭐ **THE OPERATOR IS AWAY FOR THE DAY AND THE SANDWICH STAYS — this is the RIGHT
  geometry for the queued unit and no adjustment was asked for.** The FSK write arms need
  exactly it: Chameleon #2 writes the tag, the Proxmark reads it back, and those are the two
  faces of the sandwich. ⛔ Do NOT record "lift the tag" as a blocker today — the lift buys
  only the emulate-arm re-grade, C190 already answered that at the modulation level, and the
  four FSK protocols have no emitter for it to grade.
- ⭐⭐ **THE FSK WRITERS' TWO CONSTANTS ARE NOW BOTH MEASURED, from real clones' own block
  dumps (2026-09-13 09:2x) — not read off a header and not assumed from a neighbour.**

  | | block 0 | blocks 1..n | our reader's raw |
  |---|---|---|---|
  | AWID | `00107060` | `011D8171 1DD11811 11111111` | `011d81711dd1181111111111` |
  | Paradox | `00107060` | `0F555556 95596A6A 9999A59A` | `0f55555695596a6a9999a59a` |
  | Pyramid | `00107080` | `00010101 01010101 0101016E B35E5DA4` | `00010101010101010101016eb35e5da4` |

  ⭐ **All three block forms are BYTE-IDENTICAL to the air frame our decoder reports** — so the
  writer is a straight big-endian transcription, Gallagher's case (C171), NOT Keri's. ⛔ Keri's
  block form is `(id << 3) | 7`, three bits out of phase with its air frame, and emitting the
  wrong one of the two gave a stable WRONG credential 6 of 6 (C160). ⇒ This table is why that
  cannot happen here: the question was MEASURED for each protocol separately.
  ⚠ Each block 0 also read back from `lf t55xx detect` as FSK2a / RF/50, and agrees with pm3's
  `client/src/cmdlft55xx.h:58-60` — two independent sources for the same word.
  ⚠ AWID and Paradox share `00107060` exactly; only Pyramid differs, and only in the block
  count field (4 blocks, its frame being 128 bits rather than 96).
- **Usage at handover:** `util5=7.0 util7=7.0 mins7=9524`.
- ⚠ **Coupling watch, not a blocker:** the tag has twice stopped answering mid-session
  (C159, C163), cleared both times without diagnosis. See §3 rule 3.

---

## 2. THE QUEUE

> **EXECUTION ORDER — this overrides the numbering below.**
> ✅ **U1–U10 ARE ALL COMPLETE (2026-09-13).** Every protocol Momentum carries now READS and
> WRITES here except the two that cannot be verified on this bench (FDX-A and InstaFob).
> ⛔ **The queue does not end there either, and the grid says why: SIX protocols read and
> write but do not EMULATE** — AWID, Paradox, Pyramid, FDX-A, GProxII, FDX-B.
> ⛔ **U11 AND U12 ARE BLOCKED, NOT PENDING, AND FOR DIFFERENT REASONS.** U12 (biphase
> emitters) is CLOSED: a held level does not transmit, so GProxII cannot be emulated this way
> at all (C242) — do not write another biphase emitter. U11 (FSK2a emitters) is open but
> unobservable: AWID's emitter is correct by every check available and silent anyway, and the
> only reader that can hear rig A is the Flipper, which is under test.
> ✅ **U14 IS LARGELY DONE — `NEXT.md` §11 (C291).** What remains is the per-protocol
> *reasoning*, which no tree documents. ⭐ **(historical) U14 IS OPEN AND OPERATOR-REQUESTED (2026-09-15): audit the references' scan / repeat-read
> discipline**, Flipper variants included. It is a COMPUTE unit — every tree is already on
> disk — and it is the natural companion to C268/C269, which found the agreement rule rather
> than the gates is what makes this reader safe.
> ✅ **U13 is DONE.** ⭐ Everything else finished this session is in §4 and the grid.
> **(historical) U1 → U2 → U3 → U4 → U5 → U6 → U7 → U8.**
> ⛔ §10 of `NEXT.md` is organised **by modulation family**, and a family's FIRST protocol
> must be finished completely — read, write, emulate, all verified on hardware — before its
> second is started. A shared path is only proven once something has been through it end to
> end. U1–U3 close the PSK1 family; U5 opens ASK/biphase and U6 only follows once U5 is done.

| # | unit | needs | done when |
|---|---|---|---|
| **U1** | **NexWatch WRITE.** `T5577_NEXWATCH_CONFIG 0x00081060`, `nexwatch_t55xx_writer` (3 data blocks, frame is block-aligned — no rotation, unlike Keri C158), `DATA_CMD_NEXWATCH_WRITE_TO_T55XX`, CLI `lf nexwatch write` | device | the Proxmark reads our Chameleon-written tag back as card 12345678 / Nexkey, 3 of 3 |
| **U2** | **NexWatch EMULATE.** `protocols/nexwatch.c` (PSK1 → `lf_psk1_modulator`, 96 bits, `LF_PSK1_PHASE_DIRECT`), `TAG_TYPE_NEXWATCH` (303), econfig get/set, `Makefile` row, **and a `ctest/roundtrip.c` arm** | device | Flipper or Proxmark reads our emulation as the right credential, ≥5 of 5, with a control either side |
| **U3** | **NexWatch on-device READ.** Flash, `lf nexwatch read` against the real tag | device | 6 of 6 on device + the cross-protocol nulls re-run on the shipping build |
| **U4** | **Re-test C162** — the PSK2 `lf t55xx dump` bit-31 artefact. n=1 today. Write the Indala224 credential, dump, compare; PSK1 control from the same writer | device | either a second confirming dump (n=2) or a retraction in FINDINGS.md |
| **U5** | **Gallagher** — opens family 2 (ASK/biphase). ⭐ Reuses the **GPIO/comparator** path (`register_rio_callback`, 128-entry ring, no SAADC) that em410x/Viking/Jablotron use — **not** the PSK capture path. Start as NexWatch started: `lf gallagher clone` on the Proxmark, capture, decode on the host before writing firmware | device | read + write + emulate, all verified, nulls clean |
| **U6** | **Securakey, then Noralsy, then InstaFob** — the rest of family 2, one at a time, only after U5 is completely done | device | same bar as U5, each |
| **U7** | **§9 upstreaming prep** — strip instrumentation, review what is upstreamable. Pure compute, no device | compute | a written assessment in NEXT.md §9 |
| **U8** | **FSK family** (AWID, Paradox, Pyramid, FDX-A). ⛔ **LAST, deliberately.** It reuses the HID Prox/ioProx SAADC machinery, and HID's 15–20% intermittency (C45) is unexplained and lives in exactly that path. Adding four protocols on top of an unexplained defect is what Phase 2 existed to prevent | device | do not start without saying so in §4 |
| **U9** | **GProxII** — opens family 4, **ASK BIPHASE**. ⭐ Picked before FDX-B deliberately: it is RF/64, NON-inverted, standard T5577 config, 96-bit frame, 6-bit preamble, where FDX-B is inverted AND extended-mode AND 128 bits. Proving a new line coding against three changed variables at once is the n=1 generalisation this branch keeps having to retract (C169/C171/C182, C189/C190) | device | read + write verified on hardware, cross-protocol nulls clean; emulate if the emitter is honest |
| **U10** | **FDX-B** — the rest of family 4, only after U9 is completely done. ASK biphase INVERTED, RF/32, 128 bits, preamble `00000000001`, T5577 config `903F0082` (extended mode). ⚠ Not to be confused with FDX-A, which is FSK2a and already reads | device | same bar as U9 |
| **U11** | **FSK2a EMITTERS** — AWID first, then Paradox, Pyramid, FDX-A, one at a time. ⭐ The shape is known: the ASK emitters put one PWM entry per bit with `counter_top` set to the carrier cycles that bit occupies, and FSK2a only needs `counter_top` 8 or 10 per tone period instead. ⚠ A `ctest/roundtrip.c` arm per protocol, which is where three wrong encodings were caught before (C156) | device (rig A only — no tag needed) | the Flipper reads our emulation as the right credential, ≥5 of 5, with a control either side |
| **U12** | **BIPHASE EMITTERS** — GProxII then FDX-B, only after U11 is completely done. ⚠ GProxII is BIPHASE and FDX-B is DIPHASE and INVERTED; they are one bit apart in the T5577 config and must not be assumed to share an emitter until one has been through end to end | device (rig A only) | same bar as U11 |
| **U13** | **§9 REFRESH** — pure compute. The instrumentation list is stale: `DATA_CMD_LF_READER_CAPTURE`, the GProxII failure-energy reporting and `rdrcap.py` have all been added since it was written, and the command-id count is no longer 32. ⚠ The three-PR split also predates the biphase family | compute | §9 and §9b match the branch again |
| **U15** | ✅ **DONE 2026-09-14 (C330) — every write arm re-graded against an unlocked tag: all eight 4 of 4, 32 writes, 0 failures, every raw byte-identical.** The old failures were the password lock (C325), not the writers. `./regrade.sh <protocol> [rounds]` reruns any row. ⛔ Indala224 and IDTECK were NOT re-graded — only Indala26 | device (sandwich) | ✅ met |
| **U14** | ⭐ **HOW THE REFERENCES GET ACCURACY — audit every Flipper variant and the Proxmark for their scan / repeat-read discipline.** Operator-requested 2026-09-15. Today's C268/C269 found that our two-agreeing-stacks rule, not the frame gates, is what carries this reader — so what the references do about the same problem is directly load-bearing and has never been compared side by side. ⚠ **Include the variants, not just Momentum**: `flipperzero-firmware`, `Momentum-Firmware`, `Momentum-Firmware-slix`, `unleashed-firmware`, `roguemaster` and `proxmark3` are all on disk, so this needs no fetching. Questions: how many reads before reporting; whether agreement is on the PROTOCOL or on the DECODED DATA; per-protocol counts and why they differ; what happens when several protocols match; and what is GATED versus merely reported | compute | a side-by-side table in `NEXT.md` with each claim traced to the file and line that implements it, and our own rule placed against them |

⛔ **Not yours to decide** — leave these alone and do not "make progress" on them:
§5 carrier-locking (a person's call; the recommendation is written), §7 BLE transport (needs
the cable out), anything in `NEXT.md`'s **Needs hands** table.

---

## 3. RULES OF ENGAGEMENT

**Pacing**

1. At the start of every turn: `sh /Users/Shared/code/personal/rfid/Momentum-Firmware/T5577_block0_analysis_data/usage_check.sh`.
   ⛔ **If `util7 > 30`, stop and report, naming `mins7`.** That is the operator's number,
   set deliberately to spread a weekly budget across a multi-day run — it is the one dial to
   change if a harder burn is wanted. If the script exits non-zero, pace blindly.
2. **Never idle below that ceiling.** A mid-turn kill costs one turn and nothing more,
   provided you commit every 20 minutes. Do not stop early to be tidy.
3. `./autopilot.sh beat` at every unit boundary and before anything long (a flash, a soak,
   a capture sweep). This is what stops the watchdog firing on top of you.

**The bench**

4. ⛔ **Check what is on the T5577 before running anything against it** — one `lf search`.
   It has worn seven credentials in two days and a stale assumption has twice sent
   experiments at the wrong specimen.
5. ⛔ **FIRST DIAGNOSTIC on any read failure (0x43, "not found"): are all four devices
   enumerated, and what is the fc/2 amplitude from one `lf sniff --bits 16`?** Below ~1
   means nothing is answering and **no firmware change will help**; ~24 and up is a healthy
   tag (C163). Do not debug code until that number is healthy. A whole session was spent
   debugging a reader against a bench that was not answering.
6. ⛔⛔ **One `flipper.py` emulation at a time, AND STOP IT EXPLICITLY.** `timeout` kills it
   with SIGTERM, skipping the `finally` that sends ETX, so the emulation outlives the script
   and the next "idle" null decodes (M29). ⚠ **`wait` DOES NOT HELP** — every command here
   runs in a fresh shell, so a job backgrounded in an earlier call is invisible to it. This
   rule was in this file and still produced a false null on 2026-09-13 (C187). ⇒ `pkill -f
   "flipper.py emulate"`, then send a literal ETX (`b'\x03'`) to the Flipper's port, then
   CONFIRM the null is empty before believing any return leg.
7. ⚠ **The flash script does NOT choose which Chameleon it flashes**, and `hw version`
   cannot settle it (`GIT_VERSION` arrives as a `-D` flag, so the object is not rebuilt).
   Trigger DFU on the port you want, then **ask the device what command ids it declares** —
   see README.md **Working conventions** for both commands.
8. ⚠ **`lf t55xx dump` NEEDS `lf t55xx detect` IN THE SAME `pm3 -c` INVOCATION.** It reads
   the chip config that `detect` caches, and every `./pm3 -c` is a fresh session — without it
   the dump comes back completely EMPTY, which looks exactly like a dead tag and nearly got
   written up as a fourth coupling failure (L134).
9. ⭐ **Build:** `cd firmware && docker compose up --pull=always build-ultra`.
   ⚠ `./build.sh` does **not** work on this host — bad interpreter, and the SDK expects the
   ARM toolchain at `/usr/bin` where Homebrew puts it in `/opt/homebrew/bin`. Do not spend
   time fixing that; Docker carries the pinned toolchain and is what the flash script uses.

**Evidence — `METHOD.md` binds**

10. Nothing enters `FINDINGS.md` without its **n**, its **null** and its **independent
   check**. ⭐ A blank column IS the finding — say so. (The NexWatch parity gate's null came
   back blank and that is recorded as a blank, not dressed up.)
11. ⛔ **A new format is not done until its cross-protocol nulls pass** — against the
    committed Indala26, IDTECK, Indala224, Keri, NexWatch and empty captures, **both
    directions**. The Keri veto was refuted into existence in ten minutes by exactly that
    (C157). A preamble-only match is never acceptable.
12. ⛔ **Emulate what the tag puts on the wire, not the reader's frame view.** They differ by
    a rotation and it cost a stable wrong credential 6/6 (C160). Verify against a real
    clone's own block dump.
13. ⛔ **Capture length is per-protocol and MEASURED by truncating one good capture** — never
    guessed from the frame length (C161, C165). NexWatch's real threshold was 3456 where the
    design guessed 12288.
14. ⛔ **Control the plaintext**: write a KNOWN credential and score **bit errors**, not
    pass/fail (M33).
15. ⛔ Against anything intermittent, **A/B is not an experiment — A/B/A is** (M35).
16. ⛔ Before designing a measurement, **grep `FINDINGS.md` for the subsystem it touches**
    (M32). The answer has been sitting there more than once.
17. ⛔ **Do NOT verify a PSK2 write against `lf t55xx dump`, and do NOT trust a clean one
    either.** Measured at n=70: **33% of PSK2 block reads have bit 31 wrong**, zero errors in
    any other bit, and only **1 dump in 10 is completely clean** — so a single good dump is
    not evidence any more than a bad one is (C168). The PSK1 control is 40/40 exact. Use two
    independent readers.
18. ⭐ `cd ctest && make check` runs the 320-capture cross-check **and** the emitter round
    trip. **A new protocol gets a `roundtrip.c` arm; a new reader gets its captures
    committed under `caps/`.** That harness exists because a bug shipped three times.

**Git**

19. ⛔ **`./autopilot.sh gate` before every commit.** It scans the **staged** diff and exits
    non-zero on a match. Verified to fire in both directions on 2026-09-13 — do not "fix" it
    into a `||` chain, which would invert it into a rubber stamp.
20. ⛔ **Always `git commit --no-gpg-sign`.** 1Password is locked overnight; a plain commit
    blocks on an unlock prompt or fails. Use a **quoted** heredoc (`-F - <<'MSG'`) so the
    body is not command-substituted.
21. `./checkdocs.sh` passes before every commit — run it **WITHOUT a pipe**.
22. ⛔ **Never `--amend`, never `--force`.** `LOG.md` cites hashes and `checkdocs.sh` asks
    whether each is reachable from HEAD. Land it, then fix it forward.
23. ⭐ **A change and the note describing it belong in the same commit.** Write the LOG entry
    with `` `this commit` ``, then point it at its own hash in a follow-up commit.
24. ⛔ **Push to `origin indala-psk-read` only.** Never `upstream`, never `main`, no PR, no
    upstream comment, no public post. ⚠ If `indala-psk-read` ever becomes the head of an
    open PR, **stop pushing and ask** — that is a standing rule from the operator's global
    config and it outranks this file.
25. ⚠ `NEXT.md` is a plan, not a journal. Finished sections collapse to one line; history
    goes to `LOG.md`; what is believed now goes to `FINDINGS.md`.

**Stop conditions** — report and halt, do not work around:

26. The build stays broken after one honest attempt; a device stops enumerating and stays
    gone; `checkdocs.sh` fails without an obvious fix; `util7 > 30`; or everything
    remaining needs hands.

---

## 4. LOG — one row per unit

| when | unit | util5 before → after | what landed | what verified it |
|---|---|---|---|---|
| 2026-09-14 03:05 | **C331 — the whole write column re-measured: 18 protocols, 4 of 4, 72 writes** | 37 → 38 | `regrade.sh` extended past U15's eight; C155's Indala224 gap closed; NexWatch and FDX-B judges tightened from protocol-deep to credential-deep | Proxmark as independent judge, fresh write every  round, raw byte-for-byte, bench restored |
| 2026-09-14 02:45 | **C330 — U15 CLOSED: all eight write arms 4 of 4 against an unlocked tag** | 36 → 37 | 32 writes, 32 independent pm3 reads, 0 failures; the grid's write column rebuilt; `regrade.sh` added so any row reruns | Proxmark as the independent judge, fresh write every round, raw compared byte-for-byte, bench restored |
| 2026-09-14 08:15 | **C329 — GProxII write lands; sandwich rebuilt** | 35 → 36 | The password fix restores block-0 writes AND pm3's detect/reads together. U15 now 1 of 8 | Pre-registered criterion in `benchab.sh`, same script and rig as the pre-fix run |
| 2026-09-14 08:00 | **C328 — emulate arms re-graded, §5 item closed** | 34 → 35 | IDTECK 4/4, EM410X 4/4 on the fixed firmware, with the Flipper as reader. Indala slot 1's null is bad slot data (`not the Indala preamble`), not code | Positive control and null both run first; EM410X's both-arms hit left explicitly unexplained |
| 2026-09-14 07:50 | **§1 and §5 rewritten for the password defect** | 34 → 34 | §5's "bench cannot change tag protocol" blocker CLEARED — it was the password, not the bench. §1 now names what C325 retires so a fresh context cannot build on C299/C305/C306/C324 | #2 reflashed clean at `v2.2.0-592-g9d39c15`; notes consistent |
| 2026-09-16 04:30 | **C318 — capture buffers shared, 28,672 B returned** | 33 → 34 | BSS 172,760 → 144,088; one 0x7000 symbol where there were two. Removes PR 1's RAM objection | Predicted then measured to the byte; three hardware paths + 4-arm harness |
| 2026-09-16 03:55 | **C317 — 57 KB in two capture buffers, never live together** | 32 → 33 | `m_samples` and `sniff_buf` both 28,672 B and both static. Sharing one returns more than PR 1's entire RAM cost. Belongs in `lf_reader_generic.c`, not reached into from a protocol file | `nm` on the shipping image; usage traced across the tree. Not implemented — needs its own verified unit |
| 2026-09-16 03:20 | **C316 — PR 1 builds; the real blocker is RAM** | 31 → 32 | Minimum is 13 files with a 3-line `app_cmd.c` hunk. Flash +1,024 B but BSS +24,692 B, almost all one buffer going 4,000 → 28,672 | `nm` attributes the cost to a named symbol, so the reviewer objection can be answered rather than just reported |
| 2026-09-16 02:45 | **C315 — PR 1 does not build as specified** | 30 → 31 | Missing `ble_main.c/.h` and an `app_cmd.c` hunk. The hunk-split (+827 −8 across all five PRs) is the blocking task for the whole sequence. PR 0 unaffected | 3 builds, isolating one dependency class at a time so the remaining gap is bounded, not guessed |
| 2026-09-16 02:05 | **C314 — PR 0b is 5 files, not 4 lines** | 29 → 30 | Compiling on `main` exposed a two-step dependency chain: call site → guard → BLE connection state. PR 0 unchanged | 4 builds, 3 failing in sequence; each compile error named the next dependency |
| 2026-09-16 01:30 | **C313 — PR 0 separability proven by construction** | 28 → 29 | Patch applies to clean `main` and builds there: +248 B flash, no new BSS. `formats[]` byte-identical. One 4-line dependency (BLE guard) named as PR 0b | Unpatched control built first so +248 B is a measurement, not a cross-tree subtraction |
| 2026-09-16 00:55 | **C312 — U7 refresh; `unpack()` fix is separable today** | 27 → 28 | 3 files, +102 −16, no new includes or externs, all three already upstream. Reviewable surface recounted at 64 files / +11,327 | Separability measured via the header's include/extern lines, not eyeballed; the honest recount went UP and is quoted that way |
| 2026-09-16 00:20 | **C311 — bench A/B battery built and baselined** | 26 → 27 | `benchab.sh` runs the six-test battery identically each time; `before` captured with the sandwich assembled. Run order `before → nochamp2 → opened` is mandatory | Baseline independently reproduces C305, C306 and the 4-of-4 reader in one pass; ends by asserting the restore |
| 2026-09-15 23:50 | **C310 — the read command is well-formed; the tag ignores it** | 25 → 26 | Decoded our own downlink off the capture: ONE,ZERO,ZERO,ZERO,ZERO,ZERO = opcode 10, lock 0, addr 000. Transmitter exonerated | The frame decoded off air matches the frame the source builds — two independent derivations; absolute timing left blank as unresolvable |
| 2026-09-15 23:15 | **C309 — the tag never enters read mode** | 24 → 25 | Autocorrelation: HID frame present at lag 4800, 32-bit block absent at 1600, with and without password. Corrects C308's claim that the tag's output changed | Modulation-agnostic method finds the frame that IS there, so the negative cannot be blamed on the demodulator |
| 2026-09-15 22:40 | **C308 — T5577 block read built; mechanism proved, block not decoded** | 23 → 24 | Command 3063 transmits a read INTO a live capture. The tag's 96-bit frame disappears after it (1.000 → 0.512), so the command lands; no 32-bit periodicity recovered | The control recovers a perfect period-96 frame with the same demodulator, so the failure is in the signal, not the tool |
| 2026-09-15 21:55 | **L274 — C305 narrowed to block 0's value** | 23 → 23 | Five successful writes carried five different payloads, so the data words are not the gate; block 0's value is the only variable left, and no simple mechanism explains it | Entailment from measurements already taken (M36), no new bench time |
| 2026-09-15 21:40 | **L273 — corrected C305's block counts** | 23 → 23 | HID writes 4, not 3. Same count as GProxII, opposite outcome, so block count is refuted more cleanly and the two differ only in the values written | Counts read from the source constants for all five writers |
| 2026-09-15 21:10 | **C307 — silent-success hole closed; `lf t55xx write` added** | 22 → 23 | 5 of 16 writers could report success having written nothing; all now return PAR_ERR. Raw block writer exposed as a CLI command for the C305 bench diagnosis | Audit covered all 16 writers, not the suspected ones; firmware rebuilt and flashed; HID arm re-verified |
| 2026-09-15 20:30 | **C306 — pm3's downlink broken, listening intact** | 21 → 22 | `lf t55xx read` returns one identical word for every block while `lf search` is perfect. Second symptom class for C299. Also withdrew L270's over-strong refutation of *block 0 locked* | Identical-across-blocks is self-refuting as data; `lf search` is the null and passes throughout |
| 2026-09-15 19:55 | **C305 — writes land iff the config already matches** | 21 → 21 | 5 of 5 with the tag's own config, 0 of 9 with any other, across four protocols. Block-0-locked and blk_count==0 both refuted | pm3 read the credential bit-identical after all nine failures; direct status queries returned LF_TAG_OK not PAR_ERR |
| 2026-09-15 19:15 | **C305 — only the HID writer lands** | 21 → 21 | GProxII, AWID, Keri and a raw single-block write all fail on a tag provably taking HID writes. Block count, time-ordering, password, preceding-read and silent-no-op all refuted | 10 attempts across 5 paths, every before/after read by pm3; two candidates survive and neither fits all ten results |
| 2026-09-15 18:05 | **C304 — C302/C303 verified on hardware** | 20 → 21 | Flashed #2 to HEAD. KASTLE now reads back as itself 4/4 (was C284's headline failure). ⛔ The read also caught the corruption message lying about correct numbers — fixed, 32 bits moved to "cannot tell" | Bench restored and re-verified; pm3 corroborates `matches = 2`; the harness's own example named the same credential |
| 2026-09-15 17:20 | **C303 — instrumentation compile-gated** | 19 → 20 | `LF_RESEARCH_CMDS_ENABLED`, default 0; one Makefile line enables it. Cost measured both ways: 1,088 B flash + 4,008 B RAM | Two builds per configuration; `nm` attributes the RAM to the byte and confirms an existing source comment; capability query verified on #2 |
| 2026-09-15 16:45 | **C302 — `unpack()` fixed in place** | 19 → 19 | Two-pass walk preferring a format that can validate; ambiguity reported on the card; `wiegand_other_matches()` deleted. KASTLE repaired, never-self 14 → 12 | 1,519-frame cross-check 0 disagree; `has_parity` agrees with C300's behavioural sweep on all 31 rows; firmware links; `make check` green |
| 2026-09-15 16:10 | **C301 — every reader gate measured in bits** | 18 → 19 | 200,000 random frames per format through the exported descriptors. 4 of 11 have no gate; INSTAFOB and IDTECK have no repeat requirement either. My GProxII prediction was wrong, the code was right | Predictions written before the run and pinned; the two zero-rate gates decomposed so they cannot be confused with always-false |
| 2026-09-15 15:40 | **C300 — the check-less formats measured, not read** | 17 → 18 | 4096 random frames per format through the shipping `unpack()`: 12 accept 100%, 19 reject some. `unpack()` cannot be repaired by implementing the missing checks — there are none. Corrects C285 to 12 of 31 | Null printed and pinned: H10301's two parity bits admit 26%, so the sweep reaches the checks; the 12 reproduce C285's list name for name |
| 2026-09-15 14:55 | **C299 — five causes eliminated for C297** | 16 → 17 | Password, all four downlink modes, pm3's antenna, the tag and the field all ruled out by direct test. The hands-needed test is now one action: power down #2 and retry | Destructive block-2 writes so "no change" cannot be a false negative; `hw tune` for the antenna |
| 2026-09-15 14:25 | **C298 — C297 is not a password** | 16 → 16 | Tested the obvious hypothesis and refuted it; C297's downlink account stands. Side finding: every write here sets the card's password `51243648`, previously undocumented | The test is destructive by design — block 2 would have wrecked the credential, so "air unchanged" cannot be a false negative |
| 2026-09-15 13:55 | **C297 — pm3 cannot write the T5577 any more** | 16 → 16 | 4 pm3 writes reported `Done!` and none landed; `lf t55xx detect` fails. Our writer recovered the bench first time. Recorded in §5 as a bench condition, not a blocker | Field measured healthy (13908 p-p, 3778 drops) to separate "tag gone" from "cannot write"; our writer on the same tag in the same minute is the control |
| 2026-09-15 13:25 | **C296 — M36-M40 added to METHOD.md** | 16 → 16 | Five transferable lessons lifted out of the claims: entailment over captures, host-vs-device timing, append-only documents, ritual regression runs, failing branches of display fixes | Each traced to the specific failure that produced it; the caveat that method has no experimental test is stated |
| 2026-09-15 12:55 | **C295 — the writer warns too** | 15 → 16 | 14 formats never read back as themselves, 3 sometimes; `lf hid prox write` now says so and names which format will be reported | Prediction tested on a real tag, not just the message's appearance; both lists pinned in `make check` |
| 2026-09-15 12:30 | **§1 rewritten, #2 reflashed from a clean HEAD** | 15 → 15 | `hw version` now reads `96c9d1c` = HEAD with no `-dirty`, where it had been a mid-edit build matching no commit. §1 was 45 claims stale | Post-flash read check 4 of 4; every claim in the new §1 cites the claim that established it |
| 2026-09-15 12:10 | **C294 — C266's survivors were never unexplained** | 15 → 15 | Entailed: the reader returns only frames that matched their predecessor at the same phase, so those 3 recurred within a phase by construction. C207 confirmed | One assignment site and one return path; the host-vs-device timing trap named |
| 2026-09-15 11:35 | **C293 — C269 corrected, and a second stale warning** | 15 → 15 | Our agreement needs two CONSECUTIVE decodes at the SAME phase and resets across phases, so C269's cross-phase explanation is void. `gproxii_read` still said NOT VERIFIED after C213 fixed it | Read the implementation rather than describing it; the right mechanism was already in our own C207 comment |
| 2026-09-15 10:55 | **C292 — is our agree count of 2 enough?** | 15 → 15 | Yes: front 0 wrong in 160 captures, phasebits 13 wrong and all distinct. The `INDALA_AGREE_COUNT` note predates two gates and is now labelled history | The front corpus is the control and has no wrong frames left to count; "all distinct" is a uniqueness check, not an inference |
| 2026-09-15 10:20 | **U14 / C291 — the reference scan-accuracy audit** | 14 → 15 | Six trees audited, `NEXT.md` §11 written. Three models; the Flipper family doubles its repeat count for the whole PSK1 family and for `hid_generic` alone besides | Every claim cites the implementing line; the four worker loops md5-compared before calling their rule identical |
| 2026-09-15 09:40 | **C290 — the new API pinned in ctest** | 14 → 14 | `wiegand_other_matches()` cross-checked against an independent enumeration: 1,519 frames, 0 disagreements | Break test moved it to 1,519 of 1,519 and failed `make check`; binary deleted before rebuilding |
| 2026-09-15 09:05 | **C289 — emulate arms provably untouched** | 14 → 14 | Seven firmware files changed since C241's build; the seven the working arms depend on have 0 commits between them. No flash of #1 taken | Per-file `git log` rather than a diff summary; the caveat that this reasons at file level is recorded |
| 2026-09-15 08:35 | **C288 — regression pass, 12 read arms** | 14 → 14 | All 4/4 on `5a5fdf7c` after four gate changes and a payload extension. HID re-rated 24/24 against C248's 48/48 | Every credential Proxmark-written from arguments we chose; the one change with a plausible cost measured against its own pre-change number |
| 2026-09-15 07:55 | **C287 — the reader names the other layouts** | 13 → 14 | `wiegand_other_matches()` in the file that owns the table; count + 2 names in payload bytes 13-15 that were already zero. Flashed to #2 | Counts equal the Proxmark's parity-passing candidates exactly on 3 tags; harness green; bench restored 4/4 |
| 2026-09-15 07:10 | **C286 — §9f rewritten** | 13 → 13 | Ten claims had been appended in order; the tail still said the defect needs a perturbed capture, which C284 disproved on valid tags. Restated strongest-first | Three contradicted statements each named with the claim that refutes them; nothing dropped, history left in LOG.md |
| 2026-09-15 06:35 | **C285 — the walk cannot be made correct** | 13 → 13 | 13 of 32 unpackers check nothing ⛔ (corrected to 12 of 31 by C300), and that list predicts C284's relabel map exactly. The Proxmark prints every candidate with a parity verdict; we pick the first | The check-less list and the relabel map were derived independently and agree; pm3's parity column confirms the 13 have nothing to check |
| 2026-09-15 05:55 | **C284 — half the format table fails an unpinned read** | 13 → 13 | 29 formats written to a real tag: 14 exact, **15 relabelled with a wrong credential**. Pinning recovers the written value exactly | 3 pinned controls separate "the walk loses it" from "the tag is wrong"; every credential fc 1 / cn 1 so a relabel is obvious on sight |
| 2026-09-15 05:15 | **C283 — all four message branches on hardware** | 13 → 13 | C282 said three were unreachable; the Proxmark writes those formats. ind27 and Optus34 cloned onto the HID tag exercised the neutral note and the hedge | Cloned credentials read back exactly; `ambig` named the Optus34 frame in the 177 no HID format takes; bench restored 4/4 |
| 2026-09-15 04:35 | **C281/C282 — the sweep was complete; the message dropped fields** | 13 → 13 | `formats[]` stops at 37 bits so C280's table covers everything; the predicted zero-write defect is refused by the CLI. ⛔ And my own C280 branch skipped IL/OEM — restructured | Sets proved disjoint and the fall-through set matches C280's partial length exactly; `checkdocs.sh` gained a trailing `\b` after `C1K48S` read as claim C1 |
| 2026-09-15 02:40 | **C279 — the relabelling message, seen to render** | 13 → 13 | `512a7be` (probe) then `656cc61` (restore, = HEAD). Read 29 of 40 walked the branch and printed it correctly; corrupted credential FC 1973 / card 471 against a true 1969 / 471 | A/B/A: 12/12 correct on the restored build. ⭐ #2 is now on HEAD firmware — no drift for the operator's return |
| 2026-09-15 00:30 | **C276 — the unpinned HID read says it is a guess** | 19 → 20 | Operator called the `unpack()` relabelling ours to work on. `lf hid prox read` now flags an unpinned read, names the `-f` flag and quotes C251's 7-in-48. ⭐ And §9f overstated the risk: the walk has ONE caller, `hidprox.c:134` | Both branches verified on a real H10301 clone — unpinned warns, pinned is silent; the caller count is a whole-tree grep |
| 2026-09-14 23:20 | **C274 — the drive sweep's comment was wrong** | 19 → 19 | It claimed step order affects latency not correctness; two protocols read at one drive each, and the table truncates under a short timeout. No live bug — every caller passes 3000 ms — but the margin is load-bearing and now says so | Worked through the step arithmetic at five timeout values against the constant callers actually pass; comment-only, firmware rebuilt clean |
| 2026-09-14 22:50 | **C272/C273 — FDX-B settles C270** | 18 → 19 | Same biphase decoder as GProxII, RF/32 not RF/64: 56 captures, 24 frames, **zero wrong credentials** — so it is the bit rate, not the decoder. ⛔ And FDX-B reads at drive 4 and NO other setting, working only because the shared sweep tries 4 first | Two passes at drives 7 and 6 to rule out a fluke; drive 4's 16/16 in the same sweeps is the positive control |
| 2026-09-14 22:00 | **C270/C271 — field sweep across the ASK family** | 18 → 18 | Gallagher (RF/32) and Securakey (RF/40) have NO marginal regime — perfect, then a cliff to nothing, 0 false in 40 captures each. Only GProxII (RF/64) degrades into wrong answers, and it has the strongest gate of the three | 120 captures × 5 drives with drive 7 as the in-sweep control; the drive-1 cliff reproduces on both protocols and on two passes |
| 2026-09-14 21:05 | **C269 — why the two failure modes differ** | 17 → 18 | Broken tag captured C268's way: 48 captures, 13 frames, 12 distinct, 12/12 wrong credentials, on-tag frame returned 0 times. **One frame recurs at two phases** — that is what passes two-agreeing-stacks, and it explains C266 | A/B/A: restored tag reads 8/8 exact; the two recurring captures committed under `caps/gproxii-broken/` and reproduce it off-bench |
| 2026-09-14 20:20 | **C268 — do marginal errors repeat?** | 17 → 17 | No: 92 captures, 33 frames, 12 odd raws, all 12 unique, while the true frame repeated 13 times in the same set. Two-agreeing-stacks rejects marginal-field errors — measured, not inferred | The repeating TRUE frame is the positive control: the pipeline can produce a repeat, so "none repeated" is not an artefact of never seeing one |
| 2026-09-14 19:40 | **C267 — marginal field on a VALID tag** | 16 → 17 | 4 wrong credentials in 14 frames, worse than C266's broken tag — but all four DISTINCT, so two-agreeing-stacks would reject them all. The agreement rule, not the frame gates, is what carries this reader | 60 captures at 4 drives × 6 phases through the reader's own path; drive 7 control 6/6 exact; 8 committed under `caps/gproxii-marginal/` |
| 2026-09-14 18:50 | **C266 — the false frame, measured** | 15 → 15 | C255's "one in four" is n=4; the rate is 3 in 52 (~6%) and the mechanism is a mis-sliced bit restoring the parity the tag's frame breaks. All three false frames: right card number, wrong facility code | A/B/A: valid 12/12 before, 8/8 after; each false frame re-run through the full gate offline and it passes on its own merits |
| 2026-09-14 18:00 | **C265 — the display audit finished properly** | 14 → 15 | C260 missed three protocols. FDX-B and Jablotron agree with the Proxmark; IDTECK printed the checksum byte with no verdict and now prints both | Verified on the tag in BOTH directions — a constructed failing frame and a constructed passing one, each matching pm3's word |
| 2026-09-14 17:05 | **C264 — the `require_repeat` instrument, second attempt** | 12 → 14 | Built to C254's design with a sanity check and a control, and BACKED OUT: toggling the flag on the format under test moved nothing. Two instruments, two failures, both recorded | The disproof is a flag toggle plus a forced clean rebuild, not an argument about why it should have worked |
| 2026-09-14 16:20 | **C263 — the instrumentation split, as a checklist** | 12 → 12 | §9h: four items, every call site grepped. Nothing shippable depends on any of them — the probe has one caller, the debug handlers none beyond their dispatch rows, and GProxII's is two lines because `scan_gproxii()` was already in the tree | Pure compute; the "nothing depends on them" claim is a caller count, and deliberately NOT executed — it would break this branch's own tooling |
| 2026-09-14 15:45 | **C262 — §9 refresh** | 12 → 12 | Command ids recounted from a `data_cmd.h` diff: 48 new, 41 shippable. Three stale rows fixed — HID Prox's "intermittent 15-20%", §9a's grid omitting six protocols, and a blocker arguing from an unread reference. New §9g carries the gate and display audits | Pure compute: every correction is contradicted by a claim already in the ledger, and the count is a `comm` between two checked-out headers |
| 2026-09-14 14:40 | **C261 — Securakey's display** | 11 → 12 | `_securakey_fields()` in the CLI: length, FC, card and the Wiegand word, matching the Proxmark field for field. The parity is REPORTED, not gated — the third option C253 did not list | A/B/A on the tag: valid fields 4/4 matching pm3, bit 38 flipped read 4/4 with `PARITY FAILS` flagged (the gate cannot be what changed), restored 4/4 |
| 2026-09-14 13:55 | **C259/C260 — the display layer** | 11 → 11 | Keri's `Internal ID` fixed (it printed the raw field under the reference's label); the other half of C259 retracted as my own comparison error. Then all ten displays audited: 8 clean, 1 fixed, 1 gap — Securakey prints nothing derived | Each protocol cloned by the Proxmark with a credential WE chose, read by both tools in the same minute |
| 2026-09-14 12:35 | **C258/C259 — Keri's repeat check** | 11 → 11 | `57aa2f9` flashed to #2. Keri `require_repeat` on (3/4 either way on captures), Indala26 refused it (51→43 true frames). ⚠ C259 found: our Keri reader's `Internal ID` and MS card number disagree with the Proxmark's on an identical raw frame | Hardware 8/8 with the Proxmark reading the same tag 3/3, raw byte-identical; the control is Indala26 under the same flag, and it moves |
| 2026-09-14 11:40 | **C257 — Indala26's two zero bits** | 11 → 11 | `9bd111d` flashed to #2. Corpus 68→64 frames, 51 true kept, 4 wrong dropped; `compare.py` mirrored because the harness caught the one-sided gate | Hardware A/B/A: valid 8/8, bit 61 set 0/4 **while the Proxmark reads that tag perfectly**, restored 6/6 |
| 2026-09-14 10:45 | **C255/C256 — gate audit, GProxII closed** | 8 → 11 | `3f46af2` flashed to #2. GProxII gains the reference's Wiegand parity check (sweep 36→64 rejected); every other format audited against its reference — IDTECK, InstaFob and Indala224 MATCH, Indala26 and Keri have open gaps | Hardware A/B/A: valid 8/8, one named bit broken 0 exact/3 silent/**1 self-consistent false frame**, restored 8/8 |
| 2026-09-14 08:20 | **C252 — the error-detection sweep** | 10 → 10 | `6b51ccb`. 1,024 corrupted frames through emitter and decoder, 10 protocols. Rejected/wrong runs AWID 96/0 to Indala224 28/196. Counts PINNED so a weakened gate fails `make check` | Sensitivity by deliberate break: Gallagher's accept hook removed moved it 88/8 → 16/80 while its round trip stayed ✓ exact |
| 2026-09-14 09:15 | **C253 — Securakey's missing gate** | 10 → 10 | `f536b44` flashed to #2. `securakey_accept()` enforces the reference's ten zero spacers; sweep 19→28 caught | Hardware A/B/A: valid 8/8, spacer bit 46 flipped 0/4 with the field loud and pm3's readback proving the tag held it, restored 4/4 |
| 2026-09-14 07:30 | **C251 — the wrong credentials are Indala** | 10 → 10 | `235dfa3` (probe #2) then `a049bc6` (restore). Guard OFF: hint 0 → 7 wrong of 48, six of them relabelled `Indala 26-bit`; `-f H10301` → 1 wrong of 48. The four H10301-labelled wrongs are two flips inside one parity group | A/B/A, closing arm 48/48 on the restored build; each arm confirmed on the device by `hw version`'s git hash |
| 2026-09-14 06:10 | **C47's paired test — C45 EXPLAINED** | 10 → 10 | `d37b450` (probe, guard 0) then `ee59b45` (restore, guard 1). Guard ON 96/96 exact, guard OFF 71/80 with **5 wrong credentials**, p = 6.4e-4 (C250). C45's headline corrected: the fault is the capture, not the decoder | A/B/A across three flashed builds, each confirmed on the device by `hw version`'s git hash before any read |
| 2026-09-14 04:40 | **C45 on the current build** | 10 → 10 | Nothing in firmware — a measurement. A/B/A on the rig-B T5577: legacy HID 32/32 exact, shared-engine AWID 32/32 exact, legacy HID 16/16 after rewriting the credential (C248). ⚠ C249: chained `pm3 -c` reported a pre-wipe credential from a tag that had just been wiped | 80 credential-scored reads + 24 null reads, four blank columns all silent; the closing HID arm rules out drift |
| 2026-09-13 01:30 | — | 17 → 24 | NexWatch reader (`c5ffd94`, `e0eb44b`) | 4/4 exact on real-tag captures, 508 nulls clean, `make check` green |
| 2026-09-13 02:10 | U1 + U3 | 25 → 29 | NexWatch write + read commands, CLI, T5577 config `00081060` | read 6/6 on device; write read back 3/3 by the Proxmark from a wiped tag, all three fields changed |
| 2026-09-13 02:45 | U2 | 29 → 31 | NexWatch emulation: protocol struct, `TAG_TYPE_NEXWATCH`, econfig, 2 roundtrip arms | Flipper 6/6, null 0/4, return leg 4/4 — A/B/A |
| 2026-09-13 03:20 | U4 | 27 → 30 | C162 re-tested at n=70; C168 added, C162 corrected in place | 23/70 PSK2 block reads wrong at bit 31, 0/2170 elsewhere; PSK1 control 40/40 exact |
| 2026-09-13 04:05 | U5 (part) | 28 → 31 | Gallagher characterised, `askdemod.py`, 8 captures | saturation found at every drive but 7 (C169); decode still open (C170) |
| 2026-09-13 04:35 | U5 (part) | 30 → 32 | bit-centre decoder; C171 added, C169 corrected and its design rule withdrawn | 4/4 exact against the pm3 raw, 17 nulls clean, edge decoder 0/4 |
| 2026-09-13 05:05 | U5 (part) | 32 → 34 | `lf_ask_manchester.c/h` shipping decoder, ctest arm, CRC and capture length both corrected | 4/4 host-compiled exact, 17 nulls clean; CRC 0x07/0x2C verified, capture threshold 10240 measured |
| 2026-09-13 05:45 | U5 (read+write) | 33 → 36 | Gallagher device read + write, config `00088060`, CLI with host descramble | read 6/6 on device; write read back 3/3 by the Proxmark from a wiped tag, all four fields changed |
| 2026-09-13 06:20 | U5 (emulate) — DONE | 34 → 37 | Gallagher ASK emitter, `TAG_TYPE_GALLAGHER`, econfig | Flipper 6/6, null 0/4, return leg 4/4 with a CHANGED credential it tracked |
| 2026-09-13 06:55 | U6 (part) | 38 → 40 | Securakey decoder; `lf_ask_format_t` parameterised by bit rate | 4/4 exact at RF/40, Gallagher unregressed, 21 nulls clean incl. same-family both ways |
| 2026-09-13 07:35 | U6 (part) | 39 → 42 | Securakey device read + write + emitter, `TAG_TYPE_SECURAKEY`, CLI | read 6/6, write 3/3 via pm3; **emulate 0/6** with Gallagher 4/4 as the same-rig control |
| 2026-09-13 08:05 | U6 — DONE | 41 → 43 | C177 retracted; `flipper.py` matcher widened for multi-word names | emulate 10/10, null 0/4, return leg tracked a changed credential |
| 2026-09-13 08:30 | C179 follow-up | 42 → 44 | `framedrift.py`; C180 | Gallagher real-tag frame period 3072 ± 1 over 12 intervals; Securakey withheld — tool disagrees with shipping decoder |
| 2026-09-13 08:55 | C180 completed | 42 → 43 | `framedrift.py` DC estimator now mirrors the firmware | Securakey 8 intervals all exactly 3840; 20 periods total, no terminator gap on either protocol |
| 2026-09-13 09:45 | Noralsy (read) | 0 → 8 | Noralsy decoder + device arm; ASK drive sweep; BCD field fix | read 0/6 → 6/6 with the sweep; card 112233 year 2024 matching pm3 |
| 2026-09-13 10:20 | Noralsy — DONE | 3 → 6 | Noralsy write + emulate verified; drive-sweep regression | write 3/3 pm3, emulate 10/10 null 0/4; Gallagher 4/4 and Securakey 4/4 after the shared change |
| 2026-09-13 10:55 | InstaFob scoping | 3 → 5 | C185; 2 committed captures | Flipper emulation audible at fs/2 39314 vs 15287 null; write arm unverifiable, in Needs hands |
| 2026-09-13 11:35 | InstaFob decoder | 4 → 7 | InstaFob format; shared frame bound 224 → 240 | 2 payloads tracked across a change, 24 nulls clean, 320-capture regression holds |
| 2026-09-13 12:15 | InstaFob read arm | 4 → 8 | device read arm, read-only by design; M29 rule hardened | 5/5 on device, null 0/4 after an explicit stop, 4/4 on a changed payload |
| 2026-09-13 12:45 | InstaFob terminator | 7 → 9 | C188; `framedrift.py` InstaFob arm | 98-99 samples excess vs 0 across 20 control intervals; emitter deliberately not attempted |
| 2026-09-13 13:15 | U7 — DONE | 8 → 10 | §9 rewritten: evidence grades, blockers, three-PR split | static assessment; no hardware claim |
| 2026-09-13 14:05 | emulate sweep on rig B | 9 → 13 | C189; §5 upgraded to a measured decision | ASK 16/16, PSK 1/13 to the Proxmark — a clean modulation split |
| 2026-09-13 14:40 | sweep extended to n=7 | 10 → 12 | C190; C189 corrected | ASK 16/16, PSK2 14/16, PSK1 3/21 — the line is absolute phase, not modulation family |
| 2026-09-13 15:15 | ASK roundtrip arms | 11 → 13 | 3 arms on the shipping emitters; `make check` false-pass fixed | 10/10 arms exact; deliberate break fails the suite |
| 2026-09-13 15:50 | §9 blocker #1 | 11 → 14 | shared engine renamed off `psk1` (~120 refs) | builds; 10/10 arms; 320 captures; reader 4/4 on hardware, null clean |
| 2026-09-13 16:25 | FSK feasibility | 12 → 14 | C193; 2 committed captures; §10's FSK row rewritten | 1650/1655 sub-periods exactly RF/8 or RF/10; null has no structure |
| 2026-09-13 17:00 | AWID frames | 12 → 15 | `fskdemod.py`; C194; 3 committed captures | 2 payloads → 2 distinct gated frames; null returns 0 bits; field mapping unsolved |
| 2026-09-13 17:30 | AWID payload | 13 → 15 | payload decode added; C194 amended | both payloads exact; the earlier failure was my own preamble-shift bug |
| 2026-09-13 18:05 | FSK in firmware | 13 → 16 | `lf_fsk2a.c/h`, `lf_slicer.c/h` shared, ctest arm | shipping decoder matches host byte for byte; 22 nulls; ASK unregressed |
| 2026-09-13 18:40 | AWID on device | 14 → 17 | `awid_read`, `scan_awid`, `DATA_CMD_AWID_SCAN`, CLI | 5/5 on device, null 0/3, 4/4 on a changed payload |
| 2026-09-13 19:15 | Paradox + Pyramid | 14 → 17 | 2 format entries, 2 committed captures | both decode first try; 26 nulls clean; Pyramid's CRC verified on a real signal |
| 2026-09-13 19:50 | both on device | 15 → 18 | device arms, commands 3052/3053, CLI | Paradox 4/4 null 0/2; Pyramid 4/4 then 3/3 on a changed payload |
| 2026-09-13 20:25 | FDX-A | 15 → 18 | format + device arm + CLI; C199 | decodes host-side, 26 nulls; hardware pending on a misdirected flash |
| 2026-09-14 09:25 | FDX-A hardware + C200 | 5 → 6 | targeted flash proven; nrfutil PATH root cause | FDX-A 4/4 on device, null 0/2 |
| 2026-09-14 09:45 | real-tag read upgrade | 6 → 7 | C201, no new code | AWID 5/5, Paradox 4/4, Pyramid 4/4 on real T5577s, all byte-exact |

---

## 5. BLOCKED

✅ **CLEARED 2026-09-14 — the entry that stood here was wrong.** It said the bench could not
change the tag's protocol (C305). It could; the tag was **password-locked by our own writer**,
and C325 dissolved that claim entirely. ⇒ **Nothing about the bench is blocked.** The sandwich,
the geometry, the coil coupling and the "pm3 downlink is dead" account (C299/C306) were all the
same one cause and are all retired. ⚠ The write arms for GProxII / AWID / Keri / Indala were
scored as FAILING against a locked tag and **must be re-graded** — that is a queue item, not a
blocker.

### ⚠ 2026-09-15 13:55 — THE PROXMARK CANNOT WRITE THE T5577 ANY MORE. OURS CAN.

**Not blocking — there is a working path — but the operator should know before reaching for
`lf hid clone`.** pm3's writes stopped landing mid-session and it reports `Done!` regardless:
4 attempts, 0 landed, each checked with an independent read. `lf t55xx detect` now fails
outright, and the all-ones `lf t55xx dump` and empty `lf t55xx read -b 2` logged earlier
today are the same fault — pm3's DOWNLINK. Its air reads are unaffected and correct.

⭐ **The workaround is our own writer**, which landed first time and was confirmed by both
readers: `lf hid prox write -f H10301 --fc 123 --cn 4567`. Every protocol here has one.

⛔ **NOT a password — tested and refuted (C298).** Our writers DO set one (`51243648`, via
`try_reset_t55xx_passwd`) and the timeline fitted, but a pm3 write authenticated with it left
the air unchanged. ⭐ Worth knowing regardless: **every write this project performs sets the
card's password**, which was undocumented.

⭐⭐ **FIVE CAUSES ELIMINATED (C299), and the test is now ONE ACTION rather than a disassembly.**
Not the password, not any of pm3's four downlink modes, not pm3's antenna (`hw tune`: 21.05 V at
125 kHz, *LF antenna ok*), not the tag, not the field. ⇒ What fits every observation is the
second antenna smearing pm3's transmit gaps while leaving the tag's uplink untouched — pm3 hears
perfectly and cannot talk — which also explains why OUR writer, the near antenna, still works.

⛔ **THE TEST: power down Chameleon #2 and retry `lf hid clone`.** No disassembly. If it lands,
the sandwich is the cause and neither tool is faulty. ⚠ That account is a hypothesis that fits, not
a measurement.

### ⛔⛔ 2026-09-13 11:40 — STOP USING `rfid raw_analyze`. IT HAS WEDGED THE FLIPPER TWICE.

First time it dropped the port entirely; second time the CLI went silent while still
enumerating, mid-way through taking a CONTROL that would have made the measurement usable.
⚠ Bounding the output and sending ETX did not prevent it.

⇒ If rig A's emission has to be measured again, the route is `rfid raw_read` to a FILE and
`storage read` to fetch it — or better, two Chameleons facing each other so our own sampler
can see it. Do not reach for `raw_analyze` a third time.

### ⛔ 2026-09-13 11:30 — THE FLIPPER DROPPED OFF USB during `rfid raw_analyze`

3 of 4 devices enumerate; `/dev/cu.usbmodemflip_Matthew1` is gone and the port errors with
"Device not configured". ⚠ **Probable cause is ours**: `rfid raw_analyze` on a 14336-sample
capture printed **72,518 lines** over the CDC link, and the CLI stopped answering immediately
afterwards — two further commands returned zero lines before the port vanished entirely.

⇒ **Needs a person to unplug and replug the Flipper.** Until then rig A is unavailable: no
emulation verification, which is exactly what U11 and U12 need.

⭐ **The one datum the successful run DID give, stated at its real strength:** the first ~24
pulse/period pairs of our AWID emission were ALL 62-64 units with no second value anywhere
among them. A real FSK2a emission alternates between two periods in a 10:8 ratio. That is a
LEAD, not a measurement — 24 pairs of 72,518 — but it is the first evidence about what the
peripheral actually emits, and it points at a single tone rather than two.

⚠ **Next time use `raw_read` and pull the FILE**, not `raw_analyze`. The analyze command is
built for a human watching a few screens of output, and 72,518 lines through a CLI is an abuse
of it. The file route also gives samples our own decoder can read.

⚠ Mirror anything added here into `NEXT.md`'s **Needs hands** table, and move on.

| | why |
|---|---|
| ✅ ~~Chameleon #1 wedged~~ **CLEARED 2026-09-13 09:10 by a power cycle.** ⭐ To flash ONE named unit: trigger DFU on its `/dev/cu.` port, confirm exactly one nordicDfu via `/Users/Shared/code/personal/rfid/.tools/bin/nrfutil device list` (⛔ the bare name is NOT on PATH — that read as "no DFU device" for four hours, C200), then `nrfutil device program --firmware firmware/objects/ultra-dfu-app.zip --traits nordicDfu`. Never the script: its own trigger picks whichever port the OS lists first | `hw mode` times out (CMD 1035) and DFU triggers do nothing, because the device is not listening at all. Four flashes landed on #2 as a result. C96's precedent says only a USB unplug clears this. ⇒ **Rig A is unavailable until then** — no Flipper-emulates-to-our-reader, so FDX-A's hardware arm and any new read arm are blocked |
| Coupling intermittent (C159, C163) | ⚠ **A watch, not a blocker.** Two episodes, two different signatures, both cleared without diagnosis. If it recurs: check enumeration, take the fc/2 pair, **record it** rather than working around it |
| A free-running source in front of a Chameleon reader | Two Chameleons must face each other; the rigs do not. The Flipper cannot stand in — it is carrier-locked (C87) |
| Lift the T5577 out of the sandwich | Would make the clock conclusion causal (C139). Not urgent, not blocking |
| §7 BLE transport | The point of it is measuring with the cable out |

---

## 6. THE WATCHDOG PROMPT

⭐ Re-seed this in a fresh session if the run dies and the cron is gone with it.

```
You are a watchdog for an unattended LF-protocol research session. You have no memory of
the conversation that created you, and that is expected — everything you need is on disk.

REPO: /Users/Shared/code/personal/rfid/ChameleonUltra   (branch indala-psk-read)

STEP 1 — DECIDE WHETHER TO ACT. Run:
    cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read && ./autopilot.sh status

Take over ONLY if BOTH are true: the heartbeat is missing or more than 15 minutes old, AND
the last commit is more than 30 minutes old. If either is fresh, a session is alive and
working — reply with ONE short line saying you exited, and stop. Do not read further, do
not commit, do not start a unit. Two sessions on this bench at once corrupts captures and
duplicates work.

STEP 2 — IF BOTH ARE STALE, the primary session stalled or was cut off. Take over: read
research/indala-psk-read/AUTOPILOT.md and follow section 0 exactly. That file is the whole
contract — 0 resume, 1 state, 2 the queue (with an EXECUTION ORDER box that overrides the
numbering), 3 rules of engagement, 4 the log, 5 blocked.

CRITICAL, because the operator is asleep and will not answer:
- SINGLE-THREADED. Never launch a workflow, never spawn a subagent, never fan out.
- USE THE CAPACITY, NEVER IDLE, but stop and report if util7 > 30.
- ./autopilot.sh beat at every unit boundary and before anything long.
- ./autopilot.sh gate before every commit; it exits non-zero on a match — never commit past it.
- ALWAYS git commit --no-gpg-sign, with a quoted heredoc for the message.
- NEVER stop to ask a question. If blocked, append it to section 5 and take the next unit.
- Push to origin indala-psk-read only. No PR, no upstream, no force, no amend.
- Before device work, ./autopilot.sh status reports whether the serial ports are held. If
  they are, another session is driving the CLI — do a compute unit instead.
- Keep sections 1 and 4 current, including util5 before/after, because context compacts and
  that file plus git history is the only state that survives.
```
