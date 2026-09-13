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

## 1. STATE — updated 2026-09-13 01:30

- **Last landed:** U1-U4 done. **NexWatch complete**, PSK1 family closed (C164-C167), and
  C162 re-tested at n=70 with half of it retracted (C168).
- **In flight:** nothing. **Securakey is COMPLETE** — read 6/6, write 3/3, emulate 10/10
  (C176, C178). **Noralsy is next, then InstaFob.**
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
- ⭐ **Both Chameleons carry the current build.** Rig A (#1) is in emulation mode holding a
  NexWatch slot; put it back to `hw mode -r` before using it as a reader.
- **Driver:** session cron job `9530f401`, every 5 minutes at off-minutes. ⭐ Cron fires
  ONLY while the REPL is idle, so it cannot double-drive a turn that is still working —
  which is why it is both the driver and the watchdog. ⚠ It is session-only: it dies if the
  session is closed, and auto-expires after 7 days. Re-seed from §6.
- **Bench:** all four devices enumerate. T5577 holds **our own** Gallagher write —
  region 3 / facility 1111 / card 2222 / issue 5, raw `7FEAA35473ADEB0D1A8DB562`,
  ASK, block 0 `00088060`. ⚠ Rig A (Chameleon #1) is in EMULATION mode holding a Gallagher
  slot; `hw mode -r` before using it as a reader.
- **Usage at handover:** `util5=24.0 util7=2.0 mins7=9991`.
- ⚠ **Coupling watch, not a blocker:** the tag has twice stopped answering mid-session
  (C159, C163), cleared both times without diagnosis. See §3 rule 3.

---

## 2. THE QUEUE

> **EXECUTION ORDER — this overrides the numbering below.**
> **U1 → U2 → U3 → U4 → U5 → U6 → U7 → U8.**
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
6. ⚠ **One `flipper.py` emulation at a time.** `timeout` kills it with SIGTERM, skipping the
   `finally` that sends ETX, so the emulation outlives the script and the next "idle" null
   decodes (M29).
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

---

## 5. BLOCKED — needs a person

⚠ Mirror anything added here into `NEXT.md`'s **Needs hands** table, and move on.

| | why |
|---|---|
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
