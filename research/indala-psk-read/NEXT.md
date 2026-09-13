# Next — ranked

**GOAL: support as many LF encodings as the Flipper Zero does, in read, write AND emulate.**

⛔ **The rule that produced everything below, and it still binds:** fix what exists before
adding what does not. Breadth on a broken foundation multiplies the debt, and this project has
spent days on measurements that turned out to describe a broken instrument rather than the
thing under test. ⇒ Phases 1 and 2 are closed on that basis; new protocols start now, and
**by modulation family** so each one lands on a path that something has already proven.

⭐ **An unattended run is driven by `AUTOPILOT.md`**, not by this file: it carries the
ordered unit queue, the rules of engagement and the resume procedure, with `autopilot.sh`
for the heartbeat and the pre-commit secret gate. ⚠ If the two ever disagree, THIS file
wins and the autopilot contract is the one that gets corrected.

⛔ Method rules live in `METHOD.md`. Evidence lives in `FINDINGS.md` (what is believed now)
and `LOG.md` (what was believed when). This file is a PLAN — finished sections collapse to
one line. The bench layout, the device ports and the git conventions are in `README.md`
under **The bench** and **Working conventions**.

---

## ⚠ Needs hands — what is still queued

**Cleared 2026-09-13 — the bench was rebuilt and all four devices enumerate.** The two
blockers that stopped the last session are gone: Chameleon #2 and the Flipper are back on
USB, and the T5577 answers again. ⚠ Neither was diagnosed — see the watch row below.

**Cleared 2026-09-12:** both Chameleons power-cycled (rig A emulates again, 3/3); stacking
approved for removal; the T5577 may be rewritten to whatever a test needs.

| | why a person is required |
|---|---|
| ✅ **CLOSED 2026-09-13 — it was a capture taken too soon after another, and the fix is a 50ms field-off gap (C213)** | ⭐ The instrument that closed it is committed: `DATA_CMD_LF_READER_CAPTURE` + `rdrcap.py` run the READER's own capture and hand back the samples undecoded. ⚠ What holds the charge across `stop_lf_125khz_radio()` is still not established — tag storage or amplifier AC coupling — and that is a question for an oscilloscope, not this bench |
| ⛔⛔ **THE FLIPPER IS OFF USB — unplug and replug it** | Dropped 2026-09-13 11:30 during a `rfid raw_analyze` that printed 72,518 lines over the CDC link; the port now errors "Device not configured". ⚠ Probably our doing, and avoidable: use `rfid raw_read` and pull the FILE instead. ⇒ Until it is back, rig A is unavailable and NO emulation can be verified — which is U11 and U12 |
| ⛔ **NEEDS THE TWO CHAMELEONS FACING EACH OTHER: nothing here can capture rig A's EMISSION** | ⭐ The AWID emitter round-trips through our own decoder exactly and is SILENT to the Flipper 0 of 6, with a Gallagher control at 6 of 6 on the same slot minutes later (C217). So the question is whether the PWM peripheral emits what it is asked to at `counter_top` 8 and 10 — every other emitter here uses 32, 40 or 64 — and answering it needs a reader pointed at rig A's Chameleon. ⚠ The Flipper cannot: it has no raw-capture path in this harness, and its own FSK read is the thing under test. ⇒ Either the two Chameleons face each other (one emulates, one runs `lf sniff`), or a scope. This is the same request §1's status was built for |
| ⚠ **WATCH, not a blocker: the tag has twice stopped answering the Chameleon mid-session** | ⭐ **The diagnostic comes FIRST, before blaming any code.** One `lf sniff --bits 16` and the fc/2 amplitude: below ~1 means nothing is answering and no firmware change will help; ~24 and up is a healthy tag (C163). ⛔ Two episodes, two different signatures — C159 had fc/2 down to 9.6 with broadband rms UP to 1550, C163 had both at the floor — so the cause is not established and "interference" should not be quoted as settled. ⚠ Both times our own field measured healthy: rms scales with drive and `hw lfdebug` is clean. ⚠ Both times two devices had also dropped off USB, which is a lead and not a diagnosis. ⇒ If it recurs: check enumeration, take the fc/2 pair, and record it rather than working around it |
| ⚠ **The bench tag is EM410x `DEADBEEF88`** — `drivesoak.py` cycles the tag through PAC, HID, Indala and EM410x and leaves it on whichever its last round wrote. It has worn three credentials in one day: PAC (as recorded), then HID Prox (as found), then Indala for C138's control, now PAC again | ⛔ Not deliberate — it is wherever the soak left it. §2's PAC specimen is one unattended command away (`lf pac clone --cn CD4F5552`), and so is C138's Indala reference (`lf indala clone -r a0000000e6bd0e92`). ⚠ Check what is actually on the tag before running anything against it: this row has been stale twice and both times it sent experiments at the wrong specimen. ⚠ It is no longer the carrier-locked Indala reference C138 used — restoring that is one unattended command, `lf indala clone -r a0000000e6bd0e92`, and the §4/§5 work is finished with it for now. ⛔ Its previous contents are dumped to `~/lf-t55xx-1D555955-5569A9A5-55A59569-D5B2649F-B3C6AD1F-CF649393-928C14E5-dump.json` and restore with `lf t55xx restore -f <that file>`. ⚠ §2's PAC specimen is no longer on this tag — but `pactest/` reproduces that failure on the host from a committed capture, so the physical tag is not the only specimen. ⛔ Restore `0x00081040 / 0x4944544B / 0x55667788` before relying on C90-C92's regressions again |
| ⭐ **ASKED 2026-09-13, overnight: lift the T5577 out of the sandwich** — ⭐ now wanted for TWO reasons. (1) **Verify an emulation against the Proxmark rather than the Flipper.** Every emulate arm in the grid rests on one reader; a real T5577 read by the Proxmark is actual hardware behaviour and can differ at frame boundaries we do not reproduce, the sequence terminator being the known case (C179). (2) to make the clock conclusion CAUSAL | ⚠ Not urgent, and not blocking: the conclusion is recorded as *likely closed* and everything downstream of it is written that way. But the one experiment that would turn correlation into a law — detune our own subcarrier and predict the ceiling (C139, `ADVERSARIAL.md` brief 2, question 1) — needs the Proxmark seeing the emulator alone, and the tag now sits between them. **One lift, then hands off**; several builds are measured at that one geometry |
| ⛔⛔ **Chameleon #1 — the unit on the FLIPPER pad (rig A), port ending `...43DE1` — IS WEDGED — needs a power cycle** | Its port enumerates but the device answers nothing: `hw mode` returns `CMD 1035 exec timeout`, and every DFU trigger silently fails because the device is not listening. ⚠ That is why four flashes in a row landed on #2 rather than #1 — not a targeting problem, a dead unit. ⭐ C96 is the precedent: rig A's emulation once survived a forced sense re-enable, `hw slot store` AND a full DFU reflash, then came back on a USB unplug. ⇒ **Unplug and replug it.** Until then rig A is unavailable: no Flipper-emulates-to-our-reader, so FDX-A's hardware verification and every future read arm is blocked |
| ⛔ **InstaFob's WRITE arm cannot be verified on this bench** | The Proxmark has no InstaFob command at all, and the Flipper is on the other rig — so our own written tag can only be read back by our own reader, which is the self-certification this project forbids (C185). ⇒ Either the Flipper must face the T5577, or this arm stays unverified and the grid must say so |
| **A free-running source in front of a Chameleon reader** | The one case §1's status was built for. Two Chameleons must face each other and the rigs do not. The Flipper cannot stand in — it is carrier-locked and we read it 8 of 8 (C87) |
| **§7 BLE transport** | The point of it is measuring with the cable out |

⚠ **A soft reboot is not a power cycle.** Rig A's emulation died during §3 work and survived a
forced sense re-enable, `hw slot store` and a **full DFU reflash**, then came back on a USB
unplug (C96). ⇒ When emulation misbehaves in a way that makes no sense, the power cycle is a
real diagnostic step, not a superstition — and it is one only a person can take.

---

## The grid — where we stand against the Flipper

⚠ Flipper column is the **local Momentum firmware** (`/Users/Shared/code/personal/rfid/Momentum-Firmware`),
not upstream — that is what this bench actually tests against, and it carries two protocols
upstream's list does not. Every one of its 26 protocols has BOTH a decoder and an encoder.
Chameleon columns come from the CLI command table; "emulate" means an `econfig` command plus a
registered `TAG_TYPE_*`.

| protocol | read | write | emulate | Momentum |
|---|---|---|---|---|
| EM410x (+16/32, Electra) | ✓ | ✓ | ✓ | ✓ |
| HID Prox (H10301, generic, ex-generic) | ✓ **96/96 exact — C45 is CLOSED, it was a BLE advertising burst and `cf745fb`'s guard fixes it (C250)** | ✓ | ⛔ **0/6 — FSK2a emulation does not work on this device (C246)** | ✓ |
| ioProx (IOProxXSF) | ✓ | ✓ | ⛔ **0/6 — same as HID Prox (C246)** | ✓ |
| PAC/Stanley | ✓ **fixed (C144)** | ✓ | ✓ | ✓ |
| Viking | ✓ | ✓ | ✓ | ✓ |
| Jablotron | ✓ | ✓ | ✓ | ✓ |
| **Indala 64-bit** | ✓ | ✓ | ✓ | ✓ |
| **Indala 224-bit** | ✓ | ✓ **VERIFIED on tag** | ✓ **11/11 exact (C152)** | ✓ |
| **IDTECK** | ✓ | ✓ | ✓ | ✓ |
| EM4x05 | ✓ | ✗ | ✗ | — (not an lfrfid protocol) |
| **AWID** | ✓ **5/5 on a REAL TAG (C201)** | ✓ **5/5, the PROXMARK reads our write (C203)** | ⛔ **0/6 — and so are the two SHIPPED FSK2a emitters, so this is the device, not our emitter (C246)** | ✓ |
| **FDX-A** | ✓ **4/4, emulation only — pm3 has no FDX-A (C201)** | ⛔ **refused, not deferred** — unverifiable here (C185) | ✗ | ✓ |
| **FDX-B** | ✓ **6/6 on device, nulls 0/386 (C214, C215)** | ✓ **4/4, the PROXMARK reads our write (C215)** | ✗ | ✓ |
| **Paradox** | ✓ **4/4 on a REAL TAG (C201)** | ✓ **4/4, the PROXMARK reads our write (C203)** | ✗ | ✓ |
| **Pyramid** | ✓ **4/4 on a REAL TAG (C201)** | ✓ **4/4, the PROXMARK reads our write (C203)** | ✗ | ✓ |
| **Keri** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, ROTATION verified against a reference clone (C234)** | ✓ **6/6 via Flipper (C160)** | ✓ |
| **Gallagher** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag + changed credential (C233)** | ✓ **10/10 via Flipper, null 0/4 (C174)** | ✓ |
| **NexWatch** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag + changed credential (C234)** | ✓ **10/10 via Flipper, null 0/4 (C167)** | ✓ |
| **Securakey** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag (C233)** | ✓ **10/10 via Flipper, null 0/4 (C178)** | ✓ |
| **Noralsy** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag + changed credential (C233)** | ✓ **10/10 via Flipper, null 0/4 (C184)** | ✓ |
| **GProxII** | ✓ **12/12 exact on device, 0 wrong, nulls clean (C213)** | ✓ **4/4, the PROXMARK reads our write (C207)** | ⛔ **IMPOSSIBLE as designed — a biphase 0 is a HELD level and this PWM emits nothing for one (C242)** | ✓ |
| **InstaFob** | ✓ **5/5 on device, null 0/4 (C187)** | ⛔ **unverifiable here — no writer ships** | ◐ needs a terminator-aware emitter | ✓ (ASK, RF/32, **225-bit frame**) |

⇒ **Twelve protocols absent, two readers unreliable. Every Indala and IDTECK read path
works, and every one of them now emulates too.** The only Indala gap left is the 224-bit
WRITE, which is built and cannot be verified while the T5577 sits where it does (C155).

⭐ **Indala is finished except for one unattended command.** Indala224 reads (C107), emulates
6 of 6 exact (C152) and its writer is built — `lf indala write --224`, T5577 config `000820E0`,
seven data blocks. ⛔ The write has NOT been verified on a tag, and that is C155's placement
problem rather than anything about the code: the same command's 64-bit arm, verified 9 of 9 in
an earlier session, also fails to land now. ⇒ One re-seat and both verify together.

⚠ The RAM objection that used to gate this is gone. A 224-bit frame needs **3584 bytes** of PWM
buffer, not 28 KB, because all 16 entries in an RF/32 bit are identical and the sequence's
`repeats` will hold one of them (C154). The reader's 28 KB capture buffer is unchanged.

⚠ The `tag_base_type.h` placeholders for **Keri** and **NexWatch** sit in the PSK block beside
Indala and IDTECK, so those two should reuse the PSK1 path nearly whole.

## The plan — where this stands and what is next

✅ **Phases 1 and 2 are closed.** Indala, Indala224-read and IDTECK all read; the emulation
defects are fixed; the burst length is measured; the PAC reader went 0/10 to 10/10; HID turned
out never to have had the defect §2 described. Detail is one line each in **✅ Done** below, and
in full in `FINDINGS.md`.

⇒ **Two things remain, in this order:**

| | | needs |
|---|---|---|
| ✅ **A. Close the Indala family** — §1d | **DONE.** Read, write and emulate all verified on hardware | — |
| ◐ **B. New protocols, by modulation family** — §10 | ✅ **Keri complete** — read, write, emulate, all on hardware. ◐ **NexWatch designed, not started** — the layout, the config word and the acceptance rule are worked out in §10a below, from both reference implementations | ⛔ **hands** — see the rows above |

**Standing items, not blocking either:**

| | |
|---|---|
| **§5 carrier locking** | ⛔ **a person's decision, and it is now cheap.** C134 says the reader's carrier is not observable on this board, C137 sizes the cost at a ~122 ms coherent window. Recommendation: document it and move on |
| **§7 BLE transport** | ⚠ hands — the point of it is measuring with the cable out |
| **§9 upstreaming** | strip the instrumentation first; §9 carries the exact list |
| **C148 drive inertness** | ⚠ **an instrument watch, not a task.** Run `hw lfdebug` the moment a `--drive` number looks wrong, BEFORE touching anything — a reboot clears the state |

⚠ **Retired section numbers.** LOG.md is append-only and cites sections that have since moved.
`§2b`, `§3a`, `§3b`, `§3c` were folded in during the 2026-09-12 rewrite; the pre-rewrite file is
`archive/NEXT-2026-09-12-before-dedup.md`.

---

## ✅ Done — detail is in LOG.md, not here

| | outcome |
|---|---|
| Re-measure on the front (§1) | 114/160 single captures decode, 0/160 empty produce a frame. L58 |
| The straddle gate (§1c) | 110 frames on the front, **all 110 correct**. C48, L61 |
| Phase rotation (§2b) | re-derived from both placements; the "union" approach was a trap. L58 |
| Loud-signal nulls (§3) | **220 reads, 0 false positives** across HID, EM410x, IDTECK, Viking, PAC, Jablotron. L66 |
| Second Chameleon (§4) | 20/20 at the same phase. C56, L66 |
| Signal-hunting work (§5, §6) | closed as UNMOTIVATED — there was never a deficit. L69 |
| Per-lever sweep scripts (§7) | retired; the skirt does not predict decode. C38 |
| T5577 write (§8) | **9 of 9 verified writes**; the old failures were placement. C60, L68 |
| Indala emulation (§3a) | Flipper **6/6**, Proxmark to 262 ms. C81, L79 |
| Stacking and frame lock (§2) | resolved in opposite directions. C58, C59, L67 |
| Flipper read driver (§0) | `flipper.py` committed and bracketed: **PSK 4/4, ASK 0/4**. L83 |
| Undecodable-signal status (§1) | `0x43` shipped and verified on four arms, **20 reads**. C89, L85 |
| Wrong credential from IDTECK (§0) | vetoed by decoding IDTECK: **0 of 10**, was 4 of 8. C90, C91, L87 |
| IDTECK reader (§1c) | `lf idteck read`, **6/6** with two nulls. C92, L88 |
| Advertising guard shared (§2) | all four SAADC readers; effect on HID/PAC **unmeasured**. C95, L91 |
| §8 decided | drop stacking — the user's call, 2026-09-12. C94, L92 |
| Stacking removed (§8) | 48 KB → 8 KB, free RAM 49.6 → 89.8 KB, all arms pass. C97, L93 |
| Indala 224-bit reader (§1d) | **6/6** with two nulls; PSK2-only was the answer. C107, L98 |
| Undecodable-signal status (§1) | `0x43` shipped, verified on four arms. C89, L85 |
| IDTECK reader (§1c) | `lf idteck read` **6/6**, two nulls. C92, L88 |
| Emulation defects, all three (§3) | slot type changes just work now; round trip **ASK 4/4, PSK 4/4**. C132, L113 |
| Burst length (§4) | measured across **120×**; flat above ~500 ms, falls below. **Keep 500 ms.** C135, L115 |
| Carrier slip explained (§4/§5) | **131 ppm**, one subcarrier cycle per **122 ms**; locked tag decodes 290 ms 5/5 where ours manages 0-197. C137, C138, L116-117 |
| PAC reader (§2) | **0/10 → 10/10.** The reader's own field was saturating its amplifier. C140, C144, L119-120 |
| HID Prox (§2) | **no §2 defect** — 0.0% railed samples; its intermittency is separate and older. C146, C45 |
| IDTECK emulation (§6) | 4/4 |
| Stacking removed (§8) | 48 KB → 8 KB. C97, L93 |

---

## 1d. ◐ Indala 224-bit — reads, emulates; the WRITE is built and unverified
✅ **Read**: `lf indala read --224`, 6 of 6 with two nulls, PSK2-only (C107, L98).
✅ **Emulate**: `hw slot type -t Indala224` + `lf indala econfig --224`, read back by Momentum
**6 of 6 bit-for-bit** with an ASK control at 0/6 (C152, L125).
◐ **Write**: `lf indala write --224` exists — `T5577_INDALA224_CONFIG` = `000820E0`, PSK2 RF/32,
seven data blocks filling page 0. ⛔ **Not verified on a tag.** It reports CANNOT TELL, and so
does the 64-bit arm that this bench verified 9 of 9 in an earlier session, because the T5577 no
longer couples to either Chameleon (C155).

⇒ **The remaining step is one placement and two commands**, and they verify both widths at once:

```
lf indala write --224 -r 80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5
lf indala write -r a0000000e6bd0e92
```

⚠ Verify the 224 write against the **T5577 memory dump**, not against `lf indala reader`: the
Proxmark's own 224-bit read-back is right about one time in three (C153).

## 5. ⛔ Carrier locking — a decision, and it now has a MEASURED cost

⛔⛔ **2026-09-13: this is no longer theoretical, and the price is now MEASURED.** With the
T5577 lifted out, the Proxmark reads our emulation as follows (C190):

| encoding | protocols | read rate |
|---|---|---|
| ASK / Manchester | Gallagher, Securakey, Noralsy | **16 of 16** |
| PSK2 — differential | Indala224 | **14 of 16** |
| **PSK1 — absolute phase** | Indala26, NexWatch, Keri | **3 of 21** |

⇒ The cost of not carrier-locking falls entirely on **absolute-phase** encoding. Differential
and amplitude encodings are invariant to a slowly-drifting global phase and are unaffected.
⚠ The Flipper reads all of them 6/6 to 10/10, so this is invisible from rig A alone.

Our subcarrier free-runs; a T5577 divides the reader's own carrier. That is the whole
difference, and both halves of it are now measured.

| | |
|---|---|
| **What it costs** | **131 ppm**, one whole subcarrier cycle of slip every **~122 ms** (C137). A carrier-locked tag decodes the Proxmark's full 290 ms capture **5 of 5**; ours manages 0-197 ms — on *more* signal (C138) |
| **Who notices** | only a reader that demodulates one long buffer coherently. The Flipper reads our emulation 6/6 and re-acquires well inside 122 ms |
| **Whether it can be fixed** | ⛔ **not the obvious way.** Locking means recovering the reader's carrier, and every path from the antenna to the MCU passes through a detector diode — all four LF pins carry an envelope or nothing (C134). The Flipper can do it because its front end hands the carrier to a timer; ours rectifies it away in the first component |

⇒ **Recommendation: document it as a hardware-imposed property and move on.** It is not a
defect with a fix available. The question a person is actually being asked is whether anyone
wants to hunt for a second way to recover a clock this board discards — and nothing downstream
is waiting on the answer.

⚠ **The one thing that would strengthen it** is causal rather than correlational: deliberately
detune our own subcarrier and predict the ceiling (500 ppm → ~32 ms; ~30 ppm → past 290 ms).
One firmware build per point, and it needs the T5577 lifted out of the sandwich — it is on the
hands list, marked not urgent.

## 7. ⚠ Drive the Chameleon over BLE for testing

The USB cable costs **~40%** of the emulated signal (C72 retracted) and forces a physical
unplug between configuring a device and testing it — the gap that produced three empty-field
results in one session.

The firmware speaks BLE; `chameleon_com.py` is serial-only with no `bleak` anywhere, so this
is a new transport behind the existing command layer. ⚠ A live BLE connection during emulation
is itself uncharacterised, and C47 has advertising bursts collapsing the field — measure with
and without before trusting it.

## 9. Upstreamable? — assessed 2026-09-13

⚠ **This section was written when the branch was Indala-only. It is now 49 files and ~7,500
lines across NINE protocols in two modulation families**, so the question is no longer "is
this one reader upstreamable" but "what would a maintainer have to trust, and on what
evidence".

### 9a. ⭐ The evidence grade of every arm — the part a reviewer actually needs

⭐ **Grades are about WHO verified it, not how confident I feel.**
**A** = verified on hardware against an independent tool (Proxmark or Flipper) *with* a null
and a changed-plaintext control. **B** = verified on hardware against ONE independent reader.
**C** = host-only, against committed captures. **D** = not verified — none ship.

| protocol | read | write | emulate |
|---|---|---|---|
| Indala26 / Indala224 / IDTECK | **A** | **A** (pm3 reads our tag) | **B** |
| Keri | **A** 6/6 | **A** 3/3 | **B** 6/6 |
| NexWatch | **A** 6/6 | **A** 3/3 | **B** 10/10 |
| Gallagher | **A** 6/6 | **A** 3/3 | **B** 10/10 |
| Securakey | **A** 6/6 | **A** 3/3 | **B** 10/10 |
| Noralsy | **A** 6/6 | **A** 3/3 | **B** 10/10 |
| InstaFob | **B** 5/5 | ⛔ **not shipped** | ⛔ **not built** |
| AWID | **A** 5/5 real tag (C201) | **A** pm3 reads our write (C203) | ⛔ **0/6 — DO NOT SHIP (C246, §9d)** |
| Paradox | **A** 4/4 real tag (C201) | **A** 4/4 (C203) | ⛔ **not built** |
| Pyramid | **A** 4/4 real tag (C201) | **A** 4/4 (C203) | ⛔ **not built** |
| FDX-A | **B** 4/4 — ⚠ against a Flipper EMULATION, pm3 has no FDX-A (C201) | ⛔ **refused** (C185) | ⛔ **not built** |
| GProxII | **A** 12/12, 0 wrong, nulls clean (C213) | **A** pm3 reads our write (C207) | ⛔ **impossible as designed (C242)** |
| FDX-B | **A** 6/6, nulls 0/386 (C214) | **A** 4/4 (C215) | ⛔ **not built** |

⚠ **These six were missing from this grid entirely** until 2026-09-14 — it was written before the
FSK and biphase families existed and nobody widened it. A reviewer handed a grid that silently
omits a third of the protocols is worse off than one handed no grid at all.

⛔ **Why no emulate arm is grade A, and it is not modesty.** Every one is verified by a Flipper
reading rig A. A T5577 written with the same credential and read by the Proxmark is *actual
tag behaviour*, and the two can differ at the frame boundary — which is not hypothetical:
InstaFob's real frame period exceeds its nominal by 98 samples where the other three ASK tags
show zero (C188). Reading our own emulation on the Proxmark needs the tag lifted out of the
sandwich, which is in **Needs hands** (C179).

⛔ **InstaFob is deliberately partial.** Its write arm is four lines of known-good code and is
NOT shipped, because nothing on this bench can read an InstaFob tag back — the Proxmark has no
support at all. Shipping it would be self-certification, which is the `idteck.c` failure this
project exists downstream of (C185).

### 9b. What would block a PR, in order

| | |
|---|---|
| ✅ ~~**Naming, round two**~~ **DONE 2026-09-13** | ~~`lf_ask_read` was the field-strength sweep, named when three ASK protocols were its only callers — and the four FSK2a readers go through it too, so a reviewer reading `lf_fsk2a.c` call `lf_ask_read` would take it for a mistake. Renamed `lf_drive_swept_read`, which says what it does rather than who used it first. Same class as C192.~~ |
| ✅ ~~**Naming**~~ **DONE (C192)** | ~~`lf_psk1_read()` is the shared capture engine for BOTH families — it rotates sample phase, suspends BLE advertising and enforces two-agreeing-stacks, none of it PSK-specific — and the ASK readers call it. `indala_psk_result_t` is likewise shared. A reviewer will read `lf_ask_manchester.c` calling `lf_psk1_read` as a mistake. ⇒ Renamed 2026-09-13: shared things are `lf_sampled_*` / `lf_decode_*`, genuinely-PSK things keep `psk1`.~~ |
| ⛔ **Shared-struct sizing** | `LF_PSK1_MAX_FRAME_BITS` is 240, raised from Indala224's 224 because InstaFob's frame is 225 bits and would have overflowed `id[]` and `word_bits[]` by one bit's worth (C186). It costs 16 bytes in a struct there is one of, and it is load-bearing |
| ⛔ **Command-id allocation** | ⚠ **48 new ids**, and the ranges this row gave before were wrong in both directions — recounted against `main` 2026-09-14 by diffing `data_cmd.h`, not by memory. **3033-3062** (30: the scans and T5577 writers) and **5014-5031** (18: nine `SET`/`GET_EMU_ID` pairs). The previous "46 … 5016-5029" missed Indala's own pair at 5014/5015 and GProxII's at 5030/5031. ⭐ **The shippable subset is 41**: drop the three instrumentation ids (3037 `LF_EMU_DEBUG`, 3038 `LF_RADIO_DEBUG`, 3060 `LF_READER_CAPTURE`) and the four emu-id pairs belonging to the two emitters §9d marks DO NOT SHIP (AWID 5028/5029, GProxII 5030/5031). ⇒ Needs coordinating with upstream — but as 41 with a reason, not 48 with a shrug |
| ⛔ **Instrumentation has GROWN and the table below is no longer complete** | Three things were added chasing the GProxII read and none of them ship: **`DATA_CMD_LF_READER_CAPTURE` (3060)** with `lf_reader_capture_probe()`, which runs the reader's own capture and returns the samples undecoded; **the GProxII scan's failure-energy payload**, which returns 4 bytes on a FAILED read where every other scan returns none; and **`rdrcap.py`**. ⭐ They earned their place — the probe is what cracked C211 after six hypotheses had been refuted — but a reviewer must not be handed them as if they were features. ⇒ Strip all three, or land them in a separate "LF diagnostics" change with their own justification |
| ⚠ **A protocol now overrides the shared reader's parameters** | `lf_sampled_read_phases()` takes a phase list, a try count, a drive and an inter-capture gap, and GProxII passes its own for all four. ⚠ That is four new degrees of freedom on a function every LF reader calls, added for ONE protocol — and FDX-B, the second in the same family, needs none of them (C215). A reviewer will ask whether the shape is right; the honest answer is that the gap is a real hardware fix and the rest is tuning |
| ✅ ~~**Two formats have no payload check**~~ **HALF OF THIS WAS WRONG AND IS FIXED (2026-09-14)** | ~~Securakey's gate is 19 preamble bits~~ — it was, and the reason given here ("both references are the same") was not checked. It is false: `protocol_securakey_can_be_decoded` rejects any frame whose 9-bit groups do not open with a zero spacer, and we did not. `securakey_accept()` now enforces exactly those ten spacers (C253), and the reader REPORTS the Wiegand parity without gating on it, which is what the reference does (C261). ⭐ **InstaFob's `NULL` hook is confirmed CORRECT**: its reference checks the 32-bit block-1 constant and nothing else (C256) — so that half of the row stands, now measured rather than assumed. ⇒ **The lesson is the row itself**: "both references are the same" was written without reading either one's `can_be_decoded` |
| ⚠ **ASK reads sweep field strength** | `lf_ask_read` divides the caller's timeout across drive steps {4,7,6,2}. Noralsy decodes at drive 7 and NO other setting (C182), so it is required; but it changes the latency profile of every ASK read and a reviewer should be told why rather than discovering it |

### 9e. ⛔ A PRE-EXISTING DEFECT THIS BRANCH FOUND AND DID NOT CAUSE

**FSK2a emulation does not work on this device, and two of the three broken emitters are
upstream's.** `hidprox.c` and `ioprox.c` ship in this firmware; both use `counter_top` 8 and 10
with duty `top/2` and several entries per bit; both read **0 of 6** on a Flipper that reads
Gallagher **4 of 4** on the same slot seconds later (C245, C246).

⚠ **A reviewer needs this stated plainly for two reasons.** First, the AWID emitter this
branch adds sits on that broken path — which is why §9d marks it DO NOT SHIP, and why shipping
it would look like our bug. Second, upstream's own feature table claims HID Prox and ioProx
emulate; on this hardware they do not, and this branch's grid is the only place that has ever
been measured rather than inherited.

✅ **Ten hypotheses were eliminated by measurement before this was found** — counter_top
magnitude, entries per bit, AC coupling, duty shape, the emitter design (checked against
Momentum's own demodulator AND its own encoder), the buffer plumbing, the emulation engine,
held levels, the tone value, and varying counter_top per entry. The eleventh test was to try a
shipped emitter, and it should have been the first.

⚠ **What is NOT established**: no real HID or ioProx tag can be presented to the Flipper from
this bench, so its read path for those protocols is not independently confirmed. Three FSK2a
emitters failing while six non-FSK2a ones succeed is strong, and it is not proof.

### 9f. ⛔ A SECOND PRE-EXISTING DEFECT — a corrupted HID frame is reported as Indala

**`lf hid prox read` will hand the operator a confident credential of a protocol the tag is
not.** Measured on a deliberately degraded build (`235dfa3`, the BLE advertising guard off) so
that frame corruption could be produced on demand, one tag, two arms of 48 (C251):

| arm | exact | wrong | null |
|---|---|---|---|
| `format_hint = 0` — what the CLI sends by default | 40 | **7** | 1 |
| `-f H10301` — format pinned | 46 | **1** | 1 |

Six of the seven came back labelled **`Indala 26-bit`**, FC 1953-1977 / CN 471, hugging FC 1969
/ CN 471 — the ind26 reading of this tag's own **uncorrupted** frame.

⭐ **H10301's parity is not the weak link; it works.** It rejects the mangled frame, and then
`unpack()` (`wiegand.c`) walks on to the next format of the same bit length. `unpack_ind26`
checks less, accepts what H10301 refused, and `card->format` is quietly set to ind26. The CLI
prints whatever format comes back, so there is no signal that a fallback happened at all.

⛔ **Both halves are on `main`.** The walking `unpack()` is upstream's, and so is
`LFHIDProxRead` passing `format = 0` when `-f` is absent. This branch did not introduce either
and **must not "fix" it here**: the format walk is load-bearing for every reader that guesses a
format, and narrowing it is upstream's call.

⇒ **What to report, not to patch.** The smallest honest fix is at the reporting layer — when
the returned format differs from the one asked for, or when no format was asked for at all, say
so — but even that is a change to shared behaviour and belongs in its own upstream discussion.

⚠ **What is NOT established**: the guard is on in every shipping build, and with it on the same
tag reads 96/96 and 48/48 exact with zero wrong. So this is reachable only when something else
is already perturbing the capture — a BLE burst over a live link, where `lf_adv_suspend` cannot
fire by design, is the configuration to worry about, and it has not been measured here.

### 9g. ⭐ TWO AUDITS A REVIEWER CANNOT DO THEMSELVES — the frame gates, and what we PRINT

Both were run 2026-09-14 and both found something. They are here because neither is visible in
a diff: the first needs every reference implementation open beside ours, and the second needs
the hardware.

**The frame gates, each read against its own reference's `can_be_decoded` (C256):**

| | verdict |
|---|---|
| IDTECK, InstaFob, Indala224 | ✅ **match the reference exactly.** InstaFob's `.accept = NULL` looks like a gap and is not one — its reference checks the 32-bit block-1 constant and nothing else |
| Securakey | ⛔ **was a real gap, now closed (C253).** The reference checks ten 9-bit zero spacers; we checked 19 preamble bits and nothing else |
| GProxII | ⛔ **was a real gap, now closed (C255).** The reference checks Wiegand parity over the descrambled credential; we stopped at the format length |
| Indala26 | ⛔ **was a real gap, now closed (C257).** The reference requires bits 60 and 61 to be zero. ⚠ **This makes us stricter than the PROXMARK**, which reads the frames we now refuse — the two references disagree and we followed Momentum, on C205's precedent |
| Keri | ⛔ **was a real gap, now closed (C258).** The reference requires the frame twice and the same id in both; affordable for Keri's 8192-sample window and NOT for Indala26's 4096 |

⭐ **What makes the table trustworthy is `ctest`'s sweep (C252)**: one flipped frame bit, every
bit, every protocol with an emitter — 1,024 corrupted frames — with the rejected/accepted counts
**pinned**, so a gate that quietly weakens fails `make check` rather than printing a smaller
number. Its sensitivity is proven by a deliberate break: removing Gallagher's hook moved it
88/8 → 16/80 while its round-trip arm stayed ✓ exact.

⛔ **And the limit is stated rather than hidden (C254):** the sweep cannot reach `require_repeat`,
because the renderer loops the corrupted frame and a repeat check then sees two agreeing copies
of the corruption. Low numbers are a FLOOR for repeat-gated formats.

⛔⛔ **One measured caution over all of it, and it is now measured properly.** With a corrupted
tag in the field, GProxII returns a frame that passes EVERY check both references make —
**3 times in 52 reads, about 6%** (C266; C255 saw one in four and n=4 could not carry a rate).
The mechanism is exact: the tag's frame fails Wiegand parity, and a single mis-sliced bit
inside the same parity group RESTORES it. ⛔ **Every one of those false frames carried the
right card number and a wrong facility code** — which is the near-miss that matters in access
control, not a harmless garble. ⇒ A stronger gate narrows the window; it does not close it,
and a reviewer should be told that in those terms.

⭐⭐ **AND THE DEFENCE THAT DOES CLOSE MOST OF IT IS NOT A GATE (C267).** A tag holding the
CORRECT credential, captured at marginal field through the reader's own path, produced a WRONG
credential in 4 of the 14 frames it yielded — more often than the deliberately broken tag did.
**All four were distinct.** ⭐⭐ **And C268 settled that properly rather than leaving it an
inference**: across 92 marginal captures the reader's path produced 33 frames, 12 of them not
the true raw, and **all 12 were unique** — while the true frame repeated 13 times in the same
set, which is the positive control that says a repeat CAN happen. A marginal capture's error
does not reproduce, so two-agreeing-stacks rejects it. ⇒ The agreement rule is carrying this
reader, not the frame gates, and `ctest`'s sweep cannot see that at all — it feeds one perfect
rendering per trial. A reviewer weighing whether to keep the two-stack rule should be shown
these numbers rather than the sweep's.

⛔ **AND THE RULE IS NOT SUFFICIENT EITHER, which C269 pins down.** Captured the same way,
a tag whose frame is DELIBERATELY parity-broken gives 13 frames in 48 captures, **12 of 12
distinct ones wrong credentials**, and — the part that matters — **one of them recurs at two
different capture phases**. Two agreeing captures can therefore agree on a wrong frame. The
difference from marginal field is the shape of the error population: a broken frame sits one
bit from valid parity, so the nudges that restore it land in a few places and can repeat,
while marginal-field errors scatter and cannot. ⇒ **Gates narrow it, agreement narrows it
further, and neither closes it.** That is the honest thing to hand a reviewer.

⛔⛔ **AND C270 INVERTS HOW THE GATE TABLE ABOVE SHOULD BE READ.** Swept across the whole ASK
family: **Gallagher (RF/32) and Securakey (RF/40) have no marginal regime at all** — perfect at
every drive down to 2, then a cliff to nothing, **zero false frames in 40 captures each**. Only
**GProxII (RF/64)** degrades gradually into wrong answers. The front end's AC coupling has a
~27-sample time constant (C204): RF/32 and RF/40 keep their half-bit inside it, RF/64 does not.
⇒ **GProxII has the STRONGEST gate of the three and Securakey the weakest, and it is GProxII
that emits wrong credentials.** Gate strength and field robustness are independent, and the
sweep above measures only the first. A reviewer reading that table as a safety ranking would
get it backwards.

**What we PRINT, against the Proxmark, same tag, same minute (C260, C265):** thirteen protocols,
each against a Proxmark clone of a credential we chose. HID Prox, ioProx, Indala26, NexWatch,
Gallagher, Noralsy, PAC, GProxII, FDX-B and Jablotron agree field for field. **Three were not
right and all three are fixed**: Keri printed the raw id field under the label `Internal ID`
(C259); Securakey printed no derived fields at all and now prints length, FC, card and the
Wiegand word (C261); IDTECK printed the checksum BYTE with no verdict where the reference
prints both, so our output was a number and theirs a judgement (C265).

⭐ **Two of those three fixes REPORT rather than GATE**, and that is deliberate. Securakey's
Wiegand parity and IDTECK's checksum are both computed by the reference and enforced by
neither, so enforcing them here would make this reader refuse tags both references accept —
C257 shows what that costs, and it is a cost worth paying only when the evidence is strong.
⚠ IDTECK's carries the reference's own caveat: `cmdlfidteck.c` marks the check "(TBD)" and its
worked example disagrees with its code, and no real IDTECK tag exists on this bench.

⚠ **Why this had never been checked:** every read arm in this campaign scores the RAW FRAME. A
wrong number printed under a right frame was invisible by construction, in every protocol, for
the whole branch.

### 9h. ⭐ THE INSTRUMENTATION SPLIT, AS A CHECKLIST RATHER THAN AN INTENTION

§9b has said "strip all three, or land them separately" since it was written, which is a
decision and not a plan. This is the plan — every site, found by `grep` on 2026-09-14, so the
split is a mechanical operation instead of an archaeology exercise.

⛔ **Deliberately NOT executed on this branch.** Removing these would break the research tooling
the branch exists to use, and `research/` ships in no PR anyway. The checklist is the
deliverable; the cut belongs in the PR preparation.

| | firmware | host |
|---|---|---|
| **3037 `LF_EMU_DEBUG`** (`hw emudebug`) | `data_cmd.h:235`, handler `app_cmd.c:761` (5 lines), dispatch row `app_cmd.c:3856` | `chameleon_enum.py:204`, `chameleon_cmd.py:753`, `chameleon_cli_unit.py:9029` |
| **3038 `LF_RADIO_DEBUG`** (`hw lfdebug`) | `data_cmd.h:236`, handler `app_cmd.c:770` (5 lines), dispatch row `app_cmd.c:3857` | `chameleon_enum.py:205`, `chameleon_cmd.py:783`, `chameleon_cli_unit.py:9055` |
| **3060 `LF_READER_CAPTURE`** | `data_cmd.h:264`, handler `app_cmd.c:852` (39 lines), dispatch row `app_cmd.c:3853`, and `lf_reader_capture_probe()` at `lf_indala_data.c:246` / `.h:158` | `chameleon_enum.py:227` and nothing else |
| **The GProxII failure-energy payload** | `app_cmd.c:921-932` — the 4-byte payload returned on a FAILED scan | `chameleon_cmd.py:1004-1007` |

⭐ **Nothing shippable depends on any of them, and that is checked rather than hoped.**
`lf_reader_capture_probe()` has exactly one caller (`app_cmd.c:866`); the two debug handlers are
`static` and reached only through their own dispatch rows; and the GProxII energy path already
has a plain sibling in the tree — `scan_gproxii()` at `lf_reader_main.c:192` wraps
`scan_gproxii_energy()` and discards the energy. So that fourth row is two lines: call the
sibling, delete the failure branch. **The split is subtraction, not surgery.**

⭐ `rdrcap.py` needs nothing done to it. It hardcodes `CMD = 3060` and never imports the enum,
and it lives under `research/`, which is in no PR.

⚠ **What is lost is worth naming rather than quietly binning.** The capture probe is what
cracked C211 after six hypotheses had been refuted by measurement, and the failure-energy
payload is what separates "the capture was wrong" from "the decode was wrong" on a silent read —
the distinction C206 turned on. A diagnostics change should carry them with that justification.

### 9c. ⭐ Recommended shape — three PRs, not one

⭐ **10,242 lines of CODE will not be reviewed in one PR; it will be declined.** ⚠ The figure
was "7,500" until it was recounted 2026-09-13. The branch as a whole is 204,959 insertions
across 1,654 files, and saying that without splitting it would be misleading in the other
direction: **36,012 of those lines are committed CAPTURES** under `caps/` (1,409 files) and
**4,560 are the research notes**. The reviewable surface is 59 files of firmware and host code,
29 of them new.

The branch splits along its own dependency order:

1. **The shared capture engine, renamed** — `lf_psk1_read` → a modulation-neutral name, the
   result struct with it, and the sizing constants. No new protocols. Reviewable in isolation
   and everything else depends on it.
2. **The PSK1 family** — `lf_psk1_format_t` plus Keri and NexWatch. Indala and IDTECK already
   exist upstream, so this is the descriptor refactor plus two formats.
3. **The ASK/Manchester family** — `lf_ask_manchester.c`, the drive sweep, and Gallagher,
   Securakey, Noralsy. ⚠ InstaFob stays out until its write arm can be verified.
4. **The FSK2a family** — `lf_fsk2a.c` plus AWID, Paradox, Pyramid and FDX-A, and the three
   writers that go with them. ⚠ FDX-A ships READ ONLY and the PR must say why: nothing on any
   bench here can read an FDX-A tag back, so a writer would certify itself (C185).
5. **The ASK/biphase family** — `lf_ask_biphase.c` plus GProxII and FDX-B. ⭐ This one carries
   the 50ms inter-capture gap (C213), which is a HARDWARE finding rather than a protocol
   feature and is the part of this branch most worth a reviewer's attention: a capture taken
   too soon after another clips at both rails, and every decoder that thresholds on amplitude
   is exposed to it. ⚠ No emitter ships with it — see the grid.

⚠ That is FIVE PRs now, not three. The split grew because the branch did; keeping the old
number would have meant either a dishonest count or two families smuggled into someone else's
review.

### 9d. ⭐ The split at FILE level — so it can be executed rather than re-derived

61 files change. Listing them by PR is the difference between a plan and an intention, and the
shared files are the part that actually needs thought: `app_cmd.c`, `data_cmd.h`,
`lf_reader_main.c/.h`, `lf_indala_data.c/.h`, `tag_base_type.h`, `tag_emulation.c`, the
`Makefile` and all three Python files are touched by EVERY PR and must be split by hunk.

| PR | files it OWNS | notes |
|---|---|---|
| **1. Shared engine** | `lf_indala_psk.c/.h`, `lf_slicer.c/.h`, `lf_reader_generic.c/.h`, `lf_reader_data.c/.h`, `lf_125khz_radio.c/.h`, `netdata.h` | ⛔ Also carries the RENAMES (`lf_sampled_*`, `lf_drive_swept_read`) and the sizing constants. Everything below depends on it, and it touches no protocol |
| **2. PSK1 family** | `keri.c/.h`, `nexwatch.c/.h`, `psk1.c/.h`, `indala.c/.h`, `idteck.c` | The `lf_psk1_format_t` descriptor refactor plus two formats. Indala and IDTECK exist upstream, so most of this is the refactor |
| **3. ASK/Manchester family** | `lf_ask_manchester.c/.h`, `gallagher.c/.h`, `securakey.c/.h`, `noralsy.c/.h` | ⚠ Carries the drive sweep. ⛔ InstaFob stays OUT — no verifiable write arm |
| **4. FSK2a family** | `lf_fsk2a.c/.h`, `fsk2a_t55xx.c/.h`, `awid.c/.h` | ⚠ FDX-A ships READ ONLY and the PR must say why (C185). ⛔ The AWID EMITTER should not ship at all until C243's mystery is solved — it is silent on hardware and shipping it would be the self-certification this project refuses |
| **5. ASK/biphase family** | `lf_ask_biphase.c/.h`, `gproxii.c/.h` | ⭐ Carries the 50ms inter-capture gap, which is a HARDWARE finding (C213) and the part most worth a reviewer's time. ⛔ The GProxII EMITTER must not ship: a biphase 0 is a held level and this PWM emits nothing for one (C242). Delete it or land it behind a comment saying so |
| **(separate)** | `DATA_CMD_LF_READER_CAPTURE`, `lf_reader_capture_probe`, the GProxII failure-energy payload, `rdrcap.py`, `hw emudebug`, `hw lfdebug` | ⛔ INSTRUMENTATION. Strip, or land as its own "LF diagnostics" change with its own justification. It earned its place — the probe cracked C211 — but it is not a feature |

⚠ **Two emitters in that table must NOT ship**, and both for measured reasons rather than
taste. A reviewer handed a silent emitter has no way to know it is silent.

⚠ **`idteck.c` upstream ships a PSK1 emulation nobody verified end to end** (§6). Worth
reporting independently of any of this.

⛔ **REMOVE THE INSTRUMENTATION FIRST — here is the exact list.** All of it exists to answer
questions this project had, and none of it belongs in a PR:

| | what to remove |
|---|---|
| `hw emudebug` | `DATA_CMD_LF_EMU_DEBUG` (3037), `cmd_processor_lf_emu_debug`, `lf_tag_em_debug_get()` and the `m_dbg_*` counters in `lf_tag_em.c`, the `HWEmuDebug` CLI class, `lf_emu_debug()` in `chameleon_cmd.py`, the enum entry |
| `hw lfdebug` | `DATA_CMD_LF_RADIO_DEBUG` (3038), `cmd_processor_lf_radio_debug`, `lf_125khz_radio_debug_get()` and the `m_dbg_drive_at_start` / `m_dbg_ptr_at_start` / `m_dbg_starts` statics, the `HWLfRadioDebug` CLI class, `lf_radio_debug()`, the enum entry |
| `lf sniff` research flags | `--phase`, `--rate`, `--gain`, `--input`, `--settle`, `--drive` and the firmware bytes behind them. ⚠ **A decision, not a deletion** — `--bits 16` and `--drive` are genuinely useful diagnostics and `lf sniff` is already a debug command |

⭐ **What must NOT be removed:** `lf_125khz_radio_drive_set()` / `_get()` and the 1MHz/top-8 PWM
config. They are load-bearing — `pac_read()`'s drive sweep is the PAC fix (C144), not
instrumentation. ⚠ Keep `hw lfdebug` until C148 is understood; it is the only thing that can
diagnose it and the fault has never yet been seen with it armed (C151).



## 10. ⭐ The twelve missing protocols — grouped by modulation, cheapest family first
⭐ **Group by modulation, not by name.** Each family shares a capture path, a decoder shape and
a transmit path that already exist and are proven; doing one protocol from a family makes the
rest of it nearly free, and doing one from each family makes all of them expensive.

| family | protocols | the path it reuses | why this order |
|---|---|---|---|
| ~~**1. PSK1**~~ ✅ | ~~Keri~~ ✓, ~~NexWatch~~ ✓ | `lf_indala_psk.c` + `lf_indala_data.c`, parameterised by `lf_psk1_format_t` — the same descriptor that already carries Indala64, Indala224 and IDTECK | ⭐ cheapest by a wide margin. The decoder is generic already, `tag_base_type.h` reserves both in the 300 block, and the PSK1 transmit path is proven on three protocols |
| **2. ASK / biphase** | **Gallagher**, **Securakey**, **Noralsy**, **InstaFob** | the GPIO/comparator path — `register_rio_callback`, 128-entry ring, no SAADC — that em410x, Viking and Jablotron use | ⚠ the path that is **not** affected by the saturation the SAADC readers suffer (C140), so it starts from a healthy instrument |
| **3. FSK** | **AWID**, **Paradox**, **Pyramid**, **FDX-A** | ⛔ **NOT the HID/ioProx machinery any more — the SHARED `lf_sampled_read` engine plus an FSK decoder** (C193). Measured: 99% of sub-periods land exactly on RF/8 or RF/10 on our sampler | ⭐ **The deferral reason is retracted (C193).** It rested on reusing HID's per-protocol `saadc_cb` — the family C47 found MISSING the BLE guard that is the leading explanation for the intermittency. A decoder on the shared engine inherits the guard instead, and may fix C45 rather than inherit it |
| **4. biphase, long frames** | **FDX-B**, **GProxII** | closest to ASK/biphase, but 128-bit animal-ID framing and different bit rates | likely the most work; leave until a family is proven end to end |

⛔ **Each one needs a real tag to verify against, and a protocol verified only against a
specification is worth less than nothing** — it looks supported. That is `idteck.c`'s lesson:
it shipped an emulation nobody had verified end to end and a reader that did not exist.
⇒ The Proxmark can write most of these to a T5577, which makes the read side unattended. **The
write and emulate sides need a reader that is not ours** — the Proxmark or the Flipper — which
rig B and rig A already provide.

⭐ **Do a family's FIRST protocol completely — read, write, emulate, all verified — before
starting its second.** The shared path is only proven once something has gone through it end to
end, and a half-finished family is how three protocols end up sharing one untested assumption.

## 10a. ✅ NexWatch — DONE: read, write and emulate, all verified on hardware

Read 6/6 on device and 4/4 on committed captures with 508 cross-protocol nulls (C164/C165),
write read back 3/3 by the Proxmark from a wiped tag (C166), emulate 10/10 via the Flipper
with a 0/4 null (C167). ⭐ **The PSK1 family is closed** — Indala26, Indala224, IDTECK, Keri
and NexWatch all read, write and emulate through one `lf_psk1_format_t` and one modulator.
Detail in LOG.md L131-L133; the design that produced it is in the git history of this file.

## ⭐ What the saturation lever REOPENS — a review of older conclusions

⛔ **Why this section exists.** C140 established that the LF amplifier clips on a well-coupled
tag, and C144 that the reader's own field strength is the control for it. Neither existed while
most of this project's measurements were taken, so any conclusion whose evidence was *an
amplitude, a ratio, or a "the signal is present and constant"* may have been reading a railed
path rather than a healthy one. This is M30's shape: the conclusions were not wrong when made,
and nothing has said so since.

⚠ Ranked by how much the conclusion would move. **None of these is retracted here** — they are
flagged for a measurement that can now be made and could not be before.

| | why it may need another look |
|---|---|
| ⭐⭐⭐ **C45 — "the RF path is provably flat, so the HID intermittency is the DECODER"** | The evidence was a subcarrier amplitude that did not move: 6034358 / 6023801 / 6020802, "1.00x throughout", while reads went 8/8, 7/8, 7/8. ⛔ **A SATURATED path is also flat** — that is what clipping does to an amplitude measurement. So "flat" cannot distinguish a healthy path from a railed one, and the inference to "therefore the decoder" does not follow. ⚠ Partly reassured already: HID on the current bench shows **0.0% railed samples** (C146), so its path is not clipping *here*. C45 was measured on the white coin at a different coupling, so the check is to re-take it with the rail fraction recorded alongside the amplitude |
| ⭐⭐ **Every "Nx the empty floor" bracket** — C44 (88-100x), C52 (18.1-18.9x), C55 (22.65x, 16-19x), and `emutest.py`'s "real tag 33.85" reference | A ratio measured through a saturating path is **compressed**: the loud arm clips and the floor does not, so the true ratio is HIGHER than reported. ⇒ This does not weaken the nulls — a tag that loud still produced no false frame, and clipping only makes it louder than stated. But the numbers are not linear and should not be used as calibration, which is exactly what `emutest.py`'s 33.85 reference is used for |
| ⭐⭐ **C40 / C48 — the straddle gate** | C40 found that on the front (strong) the decoder returns REPEATING wrong frames where on the back (26 dB down) they scatter, and concluded "wrong words never repeat is a property of low SNR". ⛔ Deterministic wrong output at high signal is also the signature of a clipped input. C48's gate was built to reject exactly those front-side frames. ⇒ If they are a clipping artefact, a weaker drive may remove them at the source — and the gate would be a workaround outliving its problem (M30). **Cheap to test now:** re-run the front-side capture set at reduced drive and count wrong frames |
| ⭐ **C41 — the fs/2 notch reversed sign between back and front** | It helped at 26 dB down and costs decodes on the front. A parameter that reverses between weak and strong signal is what a nonlinearity looks like. Re-measure on the front at a drive where nothing rails |
| ⭐ **The Indala sample-phase window and the 43/160 rate** | "Ticks 4-60 work, stock 0 fails" is a core project finding, measured entirely at stock drive. If clipping is part of why phase matters, the window may widen, move, or stop mattering at a drive where the path is linear — and the single-capture decode rate may rise. Directly testable with `lf sniff --drive` plus the committed host decoders |
| ⛔ **`LF_RSSI` / AIN0 as a tap — TESTED, STAYS CLOSED** | I expected this to reopen: AIN0 reads `0xff-0xff` and was closed as "measured dead", which looked like saturation. It is not recoverable by weakening the field — at drives 4, 6 and 7 it reads `0xff-0xff`, `0xff-0xff` and `0xeb-0xff`, the last being 20 counts of range out of 255. Still dead. ⭐ A negative result, and the one item here that can be struck off |
| ⚠ **SAADC gain, closed as C10** | Closed because 1/6 is the LOWEST gain available, so it could not help a clipping signal. That argument only ever ran downward. At a drive weak enough that nothing rails, a HIGHER gain becomes usable and would buy resolution on weak tags — the opposite question, never asked |

## Closed — do not re-open without new evidence

| | why |
|---|---|
| `LF_RSSI` / AIN0 as a signal tap | C08/C09, and §5/§6 removed the motivation entirely |
| SAADC gain | C10, same |
| Folding at 2048 samples | C14: odd parity inverts the subcarrier every frame |
| Indala parity as an acceptance gate | C19: it passed a frame wrong in 20 bits |
| A 12 kHz cutoff for the baseband filter | C16: the null at fs/2 was the point, not the cutoff |
| Placement as the explanation for emulator weakness | C72 retracted: it was the USB cable |
