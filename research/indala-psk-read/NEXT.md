# Next — ranked

⛔ **DEAD END, 2026-09-14: the Proxmark does NOT read our LF emulation.** Tried #2 emulating slot 1 (Indala) on the pm3 antenna, tag removed. Control clean (`lf search` finds nothing with no emulation) and coupling confirmed (`hw tune` 18.79 V with #2 on the pad vs 21.05 V bare) — but both `lf search` and `lf indala reader` find nothing. ⚠ **Inconclusive between "pm3 cannot hear it" and "the emulation is silent"**, and the two cannot be separated without a second reader. ⇒ **§2 already said this**: *the only reader that can hear rig A is the Flipper*. Our emulators DRIVE their coil with PWM rather than load-modulating, so a reader expecting a passive tag may simply not decode them. ⭐ **The emulate-arm re-grade therefore needs the Flipper and a bench session, not a pm3 and ten minutes.** Do not retry it with the Proxmark.

✅ **CLOSED — the banner here said *OPEN DEFECT (C305): the raw-frame T5577 writers do not land*, and it was wrong in its entirety.** Nothing was wrong with those writers. Our own `lf hid prox write` had password-locked the tag with a key the other writers do not carry, so they were refused and landed nothing (C325). C305's *lands iff the config matches* rule was a pattern fitted to 14 observations of one locked tag. ⇒ The password defect is fixed (F1/C326), the write path is confirmed restored (C329), and **all 18 write arms have since been re-graded 4 of 4, 72 writes, 0 failures** (C330, C331). ⛔ Left visible rather than deleted: this was the most confidently wrong thing in these notes, and it sat at the top of the file as a warning to the next reader for a whole session after it was disproved.

⛔ **THE DUTY FIX QUEUED HERE BY C420 IS REFUTED — C421.** It had already been on the air: our emitter once spent
5 high / 5 low on the long tone (50% duty, exactly what was proposed) and emulated **0 of 6**, and C226 recorded duty
as *the fourth hypothesis to fall*. C420's composition MEASUREMENT stands; the duty MECHANISM does not. Building it
would be the fourth encoding written against unchanged air, which C415 forbade.

✅ **THE SWITCHING-RATE EXPERIMENT IS DONE AND IT ANSWERED (C422): the rate IS the cause.** Same frame, 3.3% at
RF/8+RF/10 and **36.4% with two clean peaks** at RF/32+RF/40, with ratio, duty, pulse counts and buffer all held
constant. The receiver is exonerated, so our emitter's **analog drive path** cannot switch at 15.6 kHz.

✅ **THE KNEE IS LOCATED (C423)** — 3.3% at RF/8+RF/10 with no long-tone peak, 20.2% at RF/12, 35.4% at RF/16,
36.4% at RF/32. Monotonic, saturating by RF/16, half-recovery near RF/12. AWID's required rate is ~2x beyond
where recovery begins, and the roll-off is gradual: first-order or Q-limited, not a digital cutoff.

✅ **THE ELEMENT IS NAMED (C424): the LF ANTENNA TANK.** `LF_MOD` gates Q3 switching R11 220R onto `LF_OA`,
behind series diode VD1 and tank caps C34 2.2nF + C10 5.6nF (L = 208 uH). The switch is retired at 2.2 us
against a 32 us mark; a loaded Q of 6-8 reproduces C423's curve exactly.

✅ **C425 TESTED C424 AGAINST THE RESULT THAT SHOULD HAVE KILLED IT AND IT SURVIVED.** Our reader reads real
AWID/Paradox/Pyramid byte-exact (C201) — the same RF/8 subcarriers C424 says the tank cannot pass. Resolution:
`LF_ANT_DRIVER` is the analog switch's SELECT and *swings the coil terminal between GND and 3V3*
(`lf_125khz_radio.c:41`), and tag mode **parks it LOW** (`rfid_main.c:57`). ⇒ READ drives the tank from a
low-impedance source (low loaded Q, wide bandwidth); EMULATE leaves it free-running (natural Q, narrow).

⛔ **THE RINGDOWN WAS TAKEN AND IT CANNOT MEASURE THE TANK (C426).** `rdrcap.py --settle 0` already captures the
field-start transient, but it decays with **tau = 30.5 us** on #1 and **29.3 us** on #2 — the same within 4% under
radically different antenna coupling, so it is the **FILTER CHAIN**, not the tank (the schematic's 3k/10nF second
stage is 30 us). The pre-registered criterion called >25 us inconclusive and that is honoured: the only result is
a non-discriminating bound, reader-mode Q ≤ 12. ⇒ **Every ADC path sits behind that chain, whose pole exceeds the
tank's whole predicted tau, so the quantity is masked by construction.** Measuring it needs a SCOPE on a test
point (`LF_OA_OUT`, `LF_ANT_DRV`, `LF_RSSI`, `LF_MOD`, `LF_AMP_PWR` are all brought out) — external
instrumentation this run does not have. ⛔ **Do not retry this with on-board captures.**

⛔ **PRE-EMPHASIS IS ELIMINATED TOO (C427), AND SO IS SWITCH DAMPING.** Pre-emphasis needs headroom above the
steady state; the LF section has exactly ONE binary modulation leg (R11 220R + Q3), so the deepest load is
already applied and there is no *harder*. Switch damping fails on the part: the antenna switch is SPDT with no
high-Z and its throws are GND and 3V3, both AC ground through the same Ron. ⇒ **With encoding, duty and buffer
already eliminated, the firmware surface for F12 is exhausted.**

✅⛔ **F12 IS NOW A DOCUMENTED CONSTRAINT, NOT A LIVE DEFECT — and this is the decision this file asked for.**
Cause EXPLAINED (the LF tank's bandwidth, C422-C425), not PROVEN (C426: unconfirmable with on-board
instruments), firmware surface EXHAUSTED (C427). ⇒ **U11's four FSK2a emitters — AWID, Paradox, Pyramid,
FDX-A — are RETIRED as unreachable on Ultra hw_v1.** Do not open another emitter rebuild. ⭐ **What reopens
everything**: a scope on `LF_OA_OUT` or `LF_ANT_DRV` returning a loaded Q far from 6-8.

✅ **THE LIST WAS CHECKED, NOT TRUSTED (C428).** Of AUTOPILOT § line 1170's six, **GProxII IS already built**
(`TagSpecificType.GProxII = 308`, a full emitter, an `emugrade.sh` arm) and **FDX-B genuinely is not** (command
ids 3061/3062 only, **no `TagSpecificType`**). ⇒ With the FSK2a four retired by C427, **FDX-B is the only
unbuilt emulate arm left that F12 does not block** — it is biphase, not FSK2a.

⭐⭐ **NEXT UNIT, and it is two small ones in order:**
✅ **DONE — GProxII EMITS, AND A COMMERCIAL READER DECODES IT BYTE-EXACT (C429).** The Flipper returns
`GProxII FAC2A38C2B081AF0210B12C2` FC 123 Card 1337 LEN 26, with EM410X `DEADBEEF88` as the same-session
control. **C242 is refuted**, so the emulate column above is corrected. ⚠ Two criteria in a row were wrong
here — see C429; the method that worked was three frames whose predictions span 90 points, not a peak location.

✅ **DONE — FDX-B EMULATION IS BUILT AND WORKS (C430).** The Flipper reads our emulation as
`ID: 999-000000001337`, country 999, animal yes. `TAG_TYPE_FDXB` = 309, cmds 5032/5033, `lf fdxb econfig`.
⛔ It is INVERTED relative to GProxII — a mid-bit transition means ZERO — which `lf_ask_biphase.h` already
recorded and I did not apply until the air said so.

⛔⛔ **NOT FOR THE AUTOPILOT — QUEUED FOR AN OPERATOR-PRESENT BURN WINDOW: A FULL ADVERSARIAL REVIEW OF F12.**
Requested 2026-09-14. F12 was closed as *cause EXPLAINED, not PROVEN* (C427), retiring four emitters. Before that
retirement is treated as settled, it deserves a deliberate attempt to BREAK it rather than another tick that
assumes it. ⚠ This is a reasoning audit, not a bench unit, and it should NOT be taken as an autopilot compute
unit — it needs a long uninterrupted window and a willingness to reopen a closed finding.

⭐ **The load-bearing claim, and it is a single one**: C425's assertion that READ and EMULATE see a different
loaded Q on the SAME tank — reader mode driving it from a low-impedance source, tag mode parking
`LF_ANT_DRIVER` at a rail and leaving it free-running. Everything rests on that asymmetry, because without it
C201's byte-exact reads of real AWID/Paradox/Pyramid tags at RF/8-RF/10 REFUTE the bandwidth explanation
outright. ⚠ **It has never been measured**, and C426 established it cannot be measured with anything on this
board — so it is currently believed on a structural argument from two firmware sources and a schematic.

⭐ **What the review should actually attack**, in rough order of how much it would cost us to be wrong:
  1. ⛔ **Read still works and is grade A — is that really consistent, or is it the refutation we explained
     away?** All four retired protocols read AND write at grade A against real tags with the Proxmark as
     independent judge (C201, C203, C330/C331, C338, C340). Only emulate is dead.
  2. **C423's knee is a COUNT fraction of duration bins, not an amplitude** — it was never a dB measurement,
     and C423 says so. Does a Q of 6-8 really follow, or was the curve fitted to a number chosen from it?
     C424 flags its own circularity risk explicitly; check whether the flag was honoured.
  3. **C426's ringdown control** — two devices, radically different coupling, 30.5us vs 29.3us. Is
     *filter-dominated* the only reading of that, or would a tank swamped by the SAME filter also produce it?
  4. **C427's enumeration claims the firmware surface is COMPLETE.** Completeness claims are the easiest thing
     to get wrong. Is there a modulation leg, a clock, or a drive mode the schematic walk missed?
  5. ⚠ **Three pre-registered criteria in a row were wrong (C428/C429/C430)** and each would have shipped as a
     result. The F12 chase ran on the same kind of criteria for far longer. Which of C387-C427's measurements
     would not survive the scrutiny C429/C430 got?
⭐ **What would settle it outright**: a scope on `LF_OA_OUT` or `LF_ANT_DRV` — external instrumentation this
bench does not have. If the review cannot break the argument, the honest outcome is that F12 stays closed and
the reason is written down, not that it is proven.

✅ **DONE — THE EMULATE COLUMN IS RE-GRADED (C431): 12 of 16 arms pass against the Flipper**, null silent,
via the new `flipgrade.py`. hidprox/ioprox/awid are silent as F12 predicts.

⭐⭐ **NEXT UNIT — PAC EMULATION IS SILENT AND NOTHING EXPLAINS IT (C431).**
1. **Diagnose the PAC emulate arm.** It was never in C378's 11-protocol survey, so this is an unmeasured arm,
   not a regression. ⛔ It is NOT an F12 case: `pac.c` is **NRZ at RF/32** with a full modulator — 128
   entries, one per bit, `counter_top` 32 — the same shape and rate as FDX-B, which decodes byte-exact.
   ✅ **BOTH EMISSION HYPOTHESES ARE NOW CLOSED.** (a) `pac_modulator` already uses `counter_top + 1`, so it
   is not a C242 held-level defect (C432); (b) **PAC DOES emit** — 1023 pairs, NRZ at RF/32 with every top
   period bin a whole multiple of the 256us bit (C433). ⇒ **The remaining question is DECODE, not emission**:
   why does the Flipper's PAC/Stanley decoder reject a structurally correct emission? ⭐ Look at framing and
   credential — compare our `pac_build_bitstream` output against what `lf pac read` returns off a REAL
   pm3-written PAC tag on rig B, which is the one comparison that separates "our bits are wrong" from "the
   Flipper wants something else". ⛔ Do NOT grade this with our own reader against the emulation if PAC turns
   out to be SAADC-family; check M52's list first. ⭐ Grade it the C429/C430 way: predictions from the modulator, several
   payloads whose predictions are far apart, Flipper as reader.
2. **C400** — Gallagher 0/6 and Securakey 0/5 on REAL pm3-written tags, still unattributed, retestable on rig B.

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

⚠ **ONE OPTIONAL BENCH CHANGE, AND NOTHING WAITS ON IT (C416).** #1 and #2 facing each other, nothing else on
either pad, then straight back to the two-rig bench.

⭐ **Why**: F12's box is closed on every side the Flipper can measure — sequence intact, encoding-independent,
pure-vs-mixed cliff, deterministic, receiver exonerated — but the Flipper returns **edges, not amplitude**, so
*the long tone is absent* and *it is there but too shallow to cross the threshold* are indistinguishable to it.
Only our own SAADC capture path returns amplitude. #1 emulates AWID, #2 captures via `rdrcap.py`, and the samples
go to `askdemod.py` and ctest's `cdemod` — never `tonehist.py` (C401).

⚠ **Not C408 repeating itself**: that arrangement was wanted for a DECODE, which the phase-locked path cannot do
against an emulation (M52). Raw sampling of the coil is legal on that path; M52 forbids reading a credential.

⭐ Everything else is done or reachable on the bench as it stands, so this is worth doing only when convenient.

✅ **DONE AND CLEARED — nothing here needs hands (C413).** The swap was made, the real tag read **44.4%** RF/10 against our emulation's **7.0%** on the same chain, and F12 is convicted as a firmware defect. The request that follows is kept for its reasoning only.

⛔ ~~**ONE TAG MOVE, AND IT IS THE LAST STEP OF U11/F12 (C411).**~~ Write HID Prox to the T5577 with the Proxmark
(rig B, nothing to move for that), then put **that tag on the FLIPPER's pad**, leaving rig A otherwise as it is.

⭐ **Why**: the FSK2a cliff is measured — pure frames emit either tone correctly, every mixed frame collapses —
but the two available receive chains disagree by **2-3x** on how much is lost (real AWID frame: ASK 0.4%, PSK
2.3%, expected 30.4%). Both are envelope detectors with a duty-dependent bias, and mixing the tones IS a duty
change, so neither can score itself. A **real FSK2a tag is a genuine mixed-tone source**: read it through both
chains and the instruments get scored instead of scoring us. ⇒ ~2% on a real tag means the chains are blind to
mixed FSK2a and our emitter may be fine; ~45% confirms the emitter is at fault.

⚠ Nothing else is blocked — C400 is fully reachable on rig B exactly as it stands.

✅✅ **THE EMULATE COLUMN IS DONE AND NOTHING HERE NEEDS HANDS (C378).** 8 of 11 protocols emulate **6 of 6**:
PSK1 (Indala, IDTECK, Keri, NexWatch) and ASK/biphase (Gallagher, Securakey, Noralsy, GProxII), each with its
wrong-modulation arm at 0/6 as a built-in control and clean nulls either side. **FSK — HID Prox, ioProx, AWID —
is 0/6**, bracketed by 6/6 positives in the same run, so the silence is real. Credentials verified byte-exact,
not just counted: GProxII reads back `FAC2A38C2B081AF0210B12C2`, Indala `Indala26 CD7A1D30` FC 52 / Card 63612.

⛔⛔ **U12 / C242 IS REFUTED** — *GProxII cannot be emulated this way at all* is false; it emulates exactly.
⇒ **The entire remaining emulate gap is ONE family, FSK2a, which is U11.** It is now observed rather than
inferred, and it needs no bench change.

✅ **The old blocker here — *the Flipper's `rfid` plugin will not load, rebuild the .fap* — was my own wrong
diagnosis and is retired (C377).** The cause is heap fragmentation: a 66,304-byte `.fap` needing one contiguous
block, largest block decaying 118,304 → 63,880 while 107,464 stays free. A `power reboot` over the Flipper's own
CLI clears it in ~10s, and `./flipper.py heap` / `reboot` plus an `./emugrade.sh` preflight and one retry make it
automatic. ⚠ **`free` is the misleading number** — plenty free, no block big enough.

**Cleared 2026-09-13 — the bench was rebuilt and all four devices enumerate.** The two
blockers that stopped the last session are gone: Chameleon #2 and the Flipper are back on
USB, and the T5577 answers again. ⚠ Neither was diagnosed — see the watch row below.

**Cleared 2026-09-12:** both Chameleons power-cycled (rig A emulates again, 3/3); stacking
approved for removal; the T5577 may be rewritten to whatever a test needs.

| | why a person is required |
|---|---|
| ✅ **CLOSED 2026-09-13 — it was a capture taken too soon after another, and the fix is a 50ms field-off gap (C213)** | ⭐ The instrument that closed it is committed: `DATA_CMD_LF_READER_CAPTURE` + `rdrcap.py` run the READER's own capture and hand back the samples undecoded. ⚠ What holds the charge across `stop_lf_125khz_radio()` is still not established — tag storage or amplifier AC coupling — and that is a question for an oscilloscope, not this bench |
| ✅ **CLOSED 2026-09-14 — it was never a hands item (C377)** | ⭐ The row here asked for `lfrfid.fap` to be rebuilt against the running firmware. Wrong: `uptime` showed the Flipper had not rebooted since it last read 3/3 on that same firmware, so no ABI mismatch could explain it. The cause is **heap fragmentation** — 66,304-byte `.fap`, one contiguous block, largest block decaying to 63,880 with 107,464 still free — and a `power reboot` over the CLI fixes it in ~10s. Now automatic via `./flipper.py heap`/`reboot` and an `./emugrade.sh` preflight plus one retry
| ⛔⛔ **NEEDS THE TWO CHAMELEONS FACING EACH OTHER — and U11 now has a CONCRETE question for it (C386)** | ⭐ The AWID emitter round-trips through our own decoder exactly and is SILENT to the Flipper 0 of 6, with a Gallagher control at 6 of 6 on the same slot minutes later (C217). So the question is whether the PWM peripheral emits what it is asked to at `counter_top` 8 and 10 — every other emitter here uses 32, 40 or 64 — and answering it needs a reader pointed at rig A's Chameleon. ⚠ The Flipper cannot: it has no raw-capture path in this harness, and its own FSK read is the thing under test. ⇒ Either the two Chameleons face each other (one emulates, one runs `lf sniff`), or a scope. This is the same request §1's status was built for  ⭐⭐ **THE QUESTION IS NOW SHARP.** Our FSK emission is single-toned on the air — HID 1183 periods in the RF/8 band against 16 in RF/10 — while the sequence provably contains both tones (2335 entries requires 31 one-bits) and ctest decodes it exactly. Two different encodings now fail the same way (C383, C386), so it is not the varying `counter_top`. ⛔ Everything measured so far came through the FLIPPER's raw reader, which is a black box we are inferring from. `rdrcap.py` + `DATA_CMD_LF_READER_CAPTURE` hand back OUR OWN undecoded samples at a rate we set — that is the instrument this needs, and it needs one Chameleon emulating while the other captures. ⇒ Either the two face each other, or a scope |
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
| **FDX-A** | ✓ **A — 10/10 on a REAL TAG, 2 credentials (C338)** | ✓ **A — 4/4, blocks identical to the pm3 clone (C340)** | ✗ | ✓ |
| **FDX-B** | ✓ **6/6 on device (C214, C215), re-confirmed 15/15 (C337)** — ⚠ one unreproduced failure episode | ✓ **4/4, the PROXMARK reads our write (C215)** | ✅ **the FLIPPER decodes our emulation — `ID: 999-000000001337` (C430)** | ✓ |
| **Paradox** | ✓ **4/4 on a REAL TAG (C201)** | ✓ **4/4, the PROXMARK reads our write (C203)** | ✗ | ✓ |
| **Pyramid** | ✓ **4/4 on a REAL TAG (C201)** | ✓ **4/4, the PROXMARK reads our write (C203)** | ✗ | ✓ |
| **Keri** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, ROTATION verified against a reference clone (C234)** | ✓ **6/6 via Flipper (C160)** | ✓ |
| **Gallagher** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag + changed credential (C233)** | ✓ **10/10 via Flipper, null 0/4 (C174)** | ✓ |
| **NexWatch** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag + changed credential (C234)** | ✓ **10/10 via Flipper, null 0/4 (C167)** | ✓ |
| **Securakey** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag (C233)** | ✓ **10/10 via Flipper, null 0/4 (C178)** | ✓ |
| **Noralsy** | ✓ **5/5 on a REAL TAG (C235)** | ✓ **4/4, wiped tag + changed credential (C233)** | ✓ **10/10 via Flipper, null 0/4 (C184)** | ✓ |
| **GProxII** | ✓ **12/12 exact on device, 0 wrong, nulls clean (C213)** | ✓ **4/4, the PROXMARK reads our write (C207)** | ✅ **the FLIPPER decodes our emulation byte-exact — `FAC2A38C2B081AF0210B12C2` FC 123 Card 1337 (C429)**; ⛔ C242's *impossible as designed* is REFUTED, killed by a 92-held-level frame | ✓ |
| **InstaFob** | ✓ **5/5 on device, null 0/4 (C187)** | ⛔ **unverifiable here — no writer ships** | ◐ needs a terminator-aware emitter | ✓ (ASK, RF/32, **225-bit frame**) |

⇒ **Twelve protocols absent, two readers unreliable. Every Indala and IDTECK read path
works, and every one of them now emulates too.**

⭐ **THE WRITE COLUMN ABOVE IS NO LONGER A PATCHWORK OF SESSIONS — every one of the 18 write
arms was re-measured by the same battery on 2026-09-14 and every one is 4 of 4 (C330, C331).**
72 writes, 72 independent Proxmark reads, 0 failures, every raw byte-identical to what was sent.
`./regrade.sh <protocol> [rounds]` reruns any row: four fresh writes, so the score counts writes
that landed rather than reads of one write.
⛔ **C155's Indala224 caveat is retired** — it said the 224-bit write could not be verified while
the T5577 sits where it does. It verified 4 of 4.
⚠ NexWatch and FDX-B are judged on `lf nexwatch read` / `lf fdxb reader`, not `lf search`, which
prints only a protocol name for them — a protocol-deep pass is not a credential-deep one.

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

## 11. ⭐ HOW THE REFERENCES GET ACCURACY — the scan / repeat-read audit (U14)

⭐ **Why this is here.** C268 and C269 found that our two-agreeing-stacks rule, not the frame
gates, is what makes this reader safe: marginal-field errors scatter and agreement catches all
of them, while a broken frame concentrates them and agreement sometimes cannot. So what the
references do about the same problem is load-bearing. All six trees are on disk; nothing was
fetched.

| | how accuracy is obtained | where |
|---|---|---|
| **Proxmark3** | **Structural only.** One capture, demodulate, and print **every** matching format with its parity or CRC verdict beside it. `-@` repeats the whole read and prints each result independently — there is **no agreement requirement between reads**. The operator is the filter. | `client/src/cmdlfhid.c:285-287` (the `do { lf_read(); demodHID(); } while (cm)`), `client/src/wiegand_formats.c:1708` `HIDTryUnpack`, `:1732` "found N matching M-bit formats", `client/src/cmdlf.c:1956` "Note: False Positives ARE possible" |
| **Flipper family** — official, Unleashed, RogueMaster, Momentum, Momentum-slix | **Repetition only.** N **consecutive** reads whose decoded data is **byte-identical**; any change resets the count. Reports one protocol and no alternatives. ⭐ **All four worker files differ from each other, and this rule is identical in every one.** | `lib/lfrfid/lfrfid_worker_modes.c` — official `:241`, unleashed `:263`, roguemaster `:258`, Momentum `:252`, each `protocol == last_protocol && memcmp(last_data, protocol_data, size) == 0` |
| **Here** | **Both, and stricter on one axis.** Two **consecutive** decodes at the **same sample phase** must be byte-identical — the comparison resets when the phase changes, so a wrong frame recurring at a different phase never satisfies it (C293). Plus per-format `accept` hooks and C287's alternatives. |

### ⭐⭐ The number that matters: the Flipper family doubles its count for PSK1, and only for PSK1

`validate_count` is 3 for almost every protocol and **6** for exactly six of them:

| at 6 | modulation |
|---|---|
| `indala26`, `indala224`, `keri`, `nexwatch`, `idteck` | **PSK1 — the entire PSK family** |
| `hid_generic` | FSK2a, but the **format-agnostic** HID reader |

Everything else — em4100, awid, paradox, pyramid, h10301, io_prox_xsf, fdx_a, fdx_b, gallagher,
securakey, noralsy, jablotron, viking, pac_stanley, gproxii, electra, insta_fob, hid_ex_generic
— sits at 3.

⇒ **Two independent projects reached the same conclusion about the same family.** This branch
found PSK1 the fragile one from the other end: C190 measured our PSK1 emulation read **3 of 21**
by a Proxmark where ASK read 16/16 and PSK2 14/16, because only absolute-phase encoding pays for
a free-running clock. The Flipper authors never emulated anything — they hard-coded twice the
repeats on the read side for the same five protocols.

⇒ **And `hid_generic` is C284/C285 restated by someone else.** It is the HID reader that does not
know the layout, and it is the only non-PSK protocol given 6. When the format cannot be
validated, the reference buys confidence with repetition instead — which is exactly the gap
C284 measured at 15 of 29 formats and C287 answered by naming the alternatives.

⚠ **What the fork history shows.** Official has **no `indala224` at all**; Unleashed, RogueMaster
and Momentum all add it *and* give it 6, matching its PSK1 siblings rather than the default. The
rule was applied deliberately by whoever added the protocol, not inherited by accident.

⚠ **What is NOT established.** Why 3 and 6 specifically — no comment in any tree gives a
measurement, and none of the five carries a test for it. Our own `INDALA_AGREE_COUNT` of 2 has a
bootstrap behind it (50,000 trials over 160 real captures); theirs may have none.

⭐⭐ **SO THE ANSWERABLE HALF WAS ASKED OF OUR OWN DATA INSTEAD (C292).** Current decoder over
both committed corpora: the front placement gives **110 frames and 0 wrong** in 160 captures,
and phasebits gives 13 wrong of which **every one is distinct**. ⇒ Agreement at 2 removes every
wrong frame there is; 6 would remove nothing. ⛔ It also dates our own source: the note above
`INDALA_AGREE_COUNT` warns about 3-of-21 front-side wrong frames repeating within a phase, and
those 21 are now 0 — it predates C48's straddle gate and C257's zero-bit gate. ⚠ One tag, one
unit: sufficient HERE is not the same as 6 being wrong for a Flipper on unseen tags.

### ⭐⭐ Three things the first pass missed — audited 2026-09-14 against the source, not the summary

**1. The Flipper's counts are one lower than they read.** `validate_count` is not the number of
agreeing decodes; it is the number required *after* the first. `lfrfid_worker_modes.c:240-250`
sets `last_read_count = 0` on the first sighting of a protocol+data pair, increments on each
agreeing decode, and fires at `last_read_count >= validation_count`. So the real bar is:

| | agreeing decodes actually required |
|---|---|
| Flipper, non-PSK (`validate_count` 3) | **4** |
| Flipper, PSK1 and `hid_generic` (6) | **7** |
| **Here** (`lf_indala_data.c:480`) | **2** |

⇒ The gap is wider than §11 said. Not 2 against 3 and 6 — **2 against 4 and 7**, so the
reference demands two to three and a half times the corroboration we do. C292's bootstrap still
says 2 suffices *on our corpus and our tag*; it does not make 7 excessive on unseen ones.

**2. The Proxmark's cascade is ORDERED and stops at the first match.** `CmdLFfind()`
(`cmdlf.c:1916`) takes ONE capture — `lf_read(false, 30000)` — then runs 26 demodulators in a
fixed sequence, returning at the first success unless `-c` is passed:

> EM410x → Destron → Gallagher → Noralsy → Presco → Securakey → Viking → Visa2k → FDX-B →
> Jablotron → Guard → Nedap → PAC → HID → AWID → IOProx → Pyramid → Paradox → Idteck → Keri →
> NexWatch → Indala → TI → Fermax → Trovan → COTAG

⇒ That answers *what happens when several protocols match*: by default **you are not told**. The
earliest matching demodulator wins and the rest are never run, so a false positive early in the
list silently shadows the true protocol later in it. Every PSK1 format sits in the last third,
behind all 18 ASK and FSK formats. ⭐ Our cross-protocol null battery is testing precisely the
failure this ordering hides, which is why a preamble-only match was never acceptable here.

**3. Our own rule did not do what this section claimed, until today.** §11 described it as
*"byte-identical"*. It compared `memcmp(prev_word, res.id, 8)` — the whole frame only for the
three 64-bit formats, and 8 of 28 bytes for Indala224. Fixed today (C332), and registered in `FIXES.md` as its own upstream-scoped entry; the comparison is now
the full `frame_bits` and the length with it. ⚠ Every agreement-based claim in this section that
predates 2026-09-14 was measured on a 64-bit-deep comparison, which for Indala26, IDTECK and
Keri — the formats C292's bootstrap actually used — is the whole frame and therefore unaffected.

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
| Indala26 / Indala224 / IDTECK | **A** | **A** — all three **4/4** re-graded (C330, C331) | **B** |
| Keri | **A** 6/6 | **A** **4/4** (C330) | **B** 6/6 |
| NexWatch | **A** 6/6 | **A** **4/4** (C330) — judged on `lf nexwatch read`, not `lf search` | **B** 10/10 |
| Gallagher | **A** 6/6 | **A** **4/4** (C330) | ⛔ **0/6 on current firmware — candidate regression (C366)**; was B 10/10 |
| Securakey | **A** 6/6 | **A** **4/4** (C330) | ⛔ **0/6 on current firmware (C366)**; was B 10/10 |
| Noralsy | **A** 6/6 | **A** **4/4** (C330) | ⛔ **0/6 on current firmware (C366)**; was B 10/10 |
| InstaFob | **B** 5/5 | ⛔ **not shipped** | ⛔ **not built** |
| AWID | **A** 5/5 real tag (C201) | **A** **4/4** re-graded (C330) | ⛔ **0/6 — DO NOT SHIP (C246, §9d)** |
| Paradox | **A** 4/4 real tag (C201) | **A** **4/4** re-graded (C331) | ⛔ **not built** |
| Pyramid | **A** 4/4 real tag (C201) | **A** **4/4** re-graded (C331) | ⛔ **not built** |
| FDX-A | **A** 10/10 real tag, 2 credentials (C338) | **A** 4/4, blocks identical to the reference clone (C340) | ⛔ **not built** |
| GProxII | **A** 12/12, 0 wrong, nulls clean (C213) | **A** **4/4** re-graded (C330) | **A** **byte-exact on the Flipper (C429)** — C242 refuted |
| FDX-B | **A** 6/6 (C214/C215), **re-confirmed 15 of 15 (C337)** — ⚠ one unreproduced 0-of-4 episode, instrumented and unexplained | **A** **4/4** re-graded (C331) — judged on `lf fdxb reader` | **A** **byte-exact on the Flipper (C430)** |

⭐ **The write column was re-measured wholesale on 2026-09-14 (C330), and the numbers above are that
measurement.** Every write arm had been scored while our own HID writer had the tag password-locked
(C325), so the failures recorded against them said nothing about the writers. `./regrade.sh <protocol>`
reruns any row: four fresh writes, each read back by the Proxmark, plaintext controlled and the raw
compared byte-for-byte. ⭐ **All three of that row ARE now re-graded** — Indala26, Indala224 and IDTECK, 4 of 4 each (C331),
along with every other write arm in the tree.

⭐⭐ **AND THE ENGINE REWORK COST NO RELIABILITY (C345).** 250 reads, 0 failures, across all four modulation
families; **HID Prox 100 of 100**, against C250's 96/96 taken before F5 merged the capture buffers and F7 widened
the corroboration comparison. ⚠ 0 in 250 is a **1.2%** pooled bound and HID alone **3.0%** — it cannot see a
1-in-500 failure, and the grid's per-arm n = 4-6 figures are correctness, not reliability.

⭐⭐ **AND NO READER INVENTS A CREDENTIAL FROM A BLANK CHIP (C344)** — 105 reads across 21 readers on a wiped
T5577, 0 false positives, the blank state confirmed by the Proxmark before each of the five runs. This is the
failure a reviewer should fear most, because there is no credential anywhere to misread, and it is distinct from
both the foreign-tag nulls above and the recorded empty-field captures cited per protocol.
⚠ 0 in 105 is a **2.86%** pooled upper bound; at n = 5 per reader it measures the population, not any one arm.

⭐⭐ **AND IT COMPILES CLEAN (C356).** `ctest` builds 14 firmware files at `-Wall -Wextra -Wconversion`:
**13 emit zero warnings and all 13 are this branch's own code.** The fourteenth is upstream's `wiegand.c`,
and our changes REDUCED it — HEAD **35** against `main` **37**, identical flags, 0 errors both. A reviewer
sees warnings before logic, so this is worth stating in the PR: the branch adds none and removes two.
⭐ **That gap is now closed too (C357).** The real firmware built with `-Wconversion` puts **none of this branch's
new LF files** in the warning list at all — `lf_indala_data.c` included. `lf_reader_main.c` had three and blame
splits them: one is upstream's, two were ours from the password fix and now carry explicit casts (3 → 1 verified).
⇒ **The branch's entire warning debt against the firmware was two lines, and they are fixed.**
⛔ `-Wextra` is not usable here and a PR should not propose it: 180 `unused parameter 'conn_handle'`, 132
`status`, 103 `length` — the command-dispatch signature every handler must take. An API shape, not defects.

⭐⭐⭐ **AND THE LOOP IS CLOSED IN BOTH DIRECTIONS (C343).** *We write, pm3 reads* — 18 arms, 72 writes, 0 failures
(C330, C331, C340). *pm3 writes, we read* — 19 arms, 76 reads, 76 matches (`pm3written.sh`),
six of them protocols that already ship upstream, which makes it a regression check on the shared
capture engine as well. ⛔ This is the answer to
the sharpest question a reviewer can ask of a protocol branch: **how do you know your reader and your writer are not
wrong together?** Each direction is judged by the other project's code, so a shared convention error cannot pass
either. ⭐ Nine of the nineteen go further and cross the field/frame boundary — pm3 was given `--fc`/`--cn`/`--uid`
and our reader returned the frame we predicted, so two independently written field encoders agree too.
⚠ It does NOT cover the emulate arms, which still rest on a single reader.

⭐⭐ **EVERY READ ARM HAS NOW BEEN NULLED AGAINST THIRTEEN REAL FOREIGN TAGS (C342).** `nullmatrix.sh` writes each
protocol to the T5577 and asks EVERY `lf * read` the CLI has — 20 of them, the GPIO/comparator family
included: **280 cross-protocol reads, 0 false positives, and all 14
self-reads correct**. This is the evidence a reviewer cannot reproduce from the repo — it needs the bench — and it
is distinct from the capture-based nulls already cited per protocol, which are host-side against recorded samples.
⚠ State it with its bound: n = 1 per cell, so 0 in 280 means a 95% upper bound of **1.07%** per cell, not zero.
The high-n capture nulls remain the sensitive test for any single protocol.

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

### 9f. ⛔ A PRE-EXISTING DEFECT — an unpinned HID read cannot identify half the format table

⭐ **Rewritten 2026-09-15.** This section had grown into a record of how I got here, and its
tail still said the defect was "reachable only when something else is already perturbing the
capture" — which C284 disproved on perfectly good tags. The history is in `LOG.md`; this is
what is true now.

**`lf hid prox read` returns the FIRST Wiegand layout that fits and prints it as the answer.**
`unpack()` in `wiegand.c` walks `formats[]` in table order; `LFHIDProxRead` passes
`format_hint = 0` whenever `-f` is absent. Both are on `main` — this branch introduced neither.

**The measurement to lead with (C284).** Every format the CLI accepts, cloned onto a real tag
by `lf hid clone -w <fmt> --fc 1 --cn 1` and read back unpinned: **14 round-trip exactly, 15
come back as a DIFFERENT format with a DIFFERENT credential.** A Kastle tag holding fc 1 / cn 1
reports as HID Check Point card 8389632. ⭐ **`-f KASTLE` returns fc 1 / cn 1 exactly** (3 of 3
spot-checked), so the tags are perfect and the walk is what loses the information. No
corruption is involved anywhere in that result.

**Why, and why it cannot be tweaked away (C285).** 12 of the 31 `unpack_*` functions have no (C300 measured this; C285 said 13 of 32)
rejection path at all — `ind27`/`indasc27`/`tecom27` (one shared helper), `ind29`, `adt31`,
`hcp32`, `hpp32`, `kantech`, `wie32`, `optus`, `smartpass`, `p10004`. That list predicts C284's
relabel map exactly, and the two were derived independently. The walk lands on the first format
at a given length that checks nothing. ⛔ Those 13 **cannot be hardened**: a format with no
parity or checksum has nothing to check, and the Proxmark agrees — it prints a parity verdict
beside H10306, N10002 and BQT34 and none beside HCP32 or Optus34.

⇒ **The only correct behaviours are the reference's or pinning.** `lf hid reader` prints EVERY
candidate with its parity verdict — five for the Kastle tag — and lets the operator choose.
`-f <fmt>` removes the ambiguity at the source. **Narrowing the walk is not one of the
options**, and a proposal offering it should say why it is not.

⭐ **Scope, corrected.** An earlier draft said the walk is load-bearing for "every reader that
guesses a format". A whole-tree grep finds **one caller: `hidprox.c:134`** (C276). ioProx has a
fixed XSF layout; Indala prints its own 26-bit interpretation without the table. Upstream is
being asked about one reader.

**What corruption adds (C251, C277, C279).** At 26 bits H10301 accepts all 8,658 validly packed
ind26 frames swept, so a genuine Indala-layout tag reports as H10301 and a foreign winner there
means the capture was corrupted. With the BLE guard deliberately off, an unpinned read was
wrong 7 times in 48 against 1 in 48 pinned, and 6 of the 7 came back as `Indala 26-bit`.

**What this branch shipped (C276, C280, C282, C283).** `lf hid prox read` now says an unpinned
read is the first layout that fits, names the `-f` flag, and quotes the cost. The wording is
per-length and measured: corruption at 26/32/37 bits where an HID format takes every valid
foreign frame, "the reader cannot tell" at 34 where it takes 307 of 484, and a neutral note at
27-30 bits **where no HID format exists at all** — without which the message would have fired
on every such read. All four branches are exercised on hardware. `unpack()` is untouched.

⚠ **A reviewer will also notice 42 declared formats against 31 implemented** (C281):
`card_format_t` names AVIG56, BC40, BQT38, C1K48S, CASI40, DEFCON32, H800002, IR56, ISCS,
P10001 and PW39 with no `formats[]` row, so they can never be packed or unpacked. Harmless —
the host enum has 31 and mirrors the table, so the CLI refuses them — but it is upstream's
header and it looks like a gap until someone checks.

⭐⭐ **THE ENUMERATION IS NOW BUILT (C287).** `wiegand_other_matches()` lives in the file that
owns `formats[]`; the reader reports how many other layouts fit and names up to two, in payload
bytes 13-15 that were already present and already zero. Verified against `lf hid reader` on
three tags — our counts equal its parity-PASSING candidates exactly, 3 for 3. A Kastle tag now
names Kastle among the alternatives instead of discarding it.

⚠ **What is NOT established.** W2804 and ACTPHID are missing from C284's sweep because the
Proxmark refused every credential shape tried — a writer limitation, not a reader finding. And
only two alternatives are NAMED for want of payload space; the count is exact.

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

⭐⭐ **C272 settles it with the one comparison that separates decoder from bit rate.** GProxII
and FDX-B share `lf_ask_biphase.c` WHOLE — same decoder, same acceptance machinery — and FDX-B
is RF/32 where GProxII is RF/64. Swept the same way: **56 captures, 24 frames, zero wrong
credentials.** The one differing raw is a single bit at position 113, past the 64 the CRC
covers, and the credential decodes identically. ⇒ It is the bit rate against the front end's
coupling constant, not the decoder and not the gate.

⛔ **A fragility found on the way (C273): FDX-B reads at drive 4 and at no other setting** —
16/16 at 4, 0/16 at 7, 0/16 at 6, nothing at 2 or 1. It works only because the shared ASK
reader sweeps `{4, 7, 6, 2}` with 4 FIRST, and nothing at the call site says that ordering is
load-bearing. C182 has Noralsy needing drive 7 and only 7. Two protocols, two different single
settings, one undocumented sweep — a reviewer shortening or reordering it breaks a reader with
no other symptom.

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

✅ **DONE 2026-09-15 (C303), AND NOT THE WAY THIS SECTION ORIGINALLY PLANNED IT.** §9b said
"strip all three", and the operator pushed back: stripping useful tooling is counter-intuitive,
so what is the industry-standard way of handling this? ⇒ **Compile-time gating, not deletion** —
and the tree was already doing it. `app_cmd.c` gates whole handlers AND their dispatch rows
behind `#if defined(PROJECT_CHAMELEON_ULTRA)` for the Ultra/Lite split, and this branch had
already added `#if !INDALA224_READER_TRUSTED`.

⭐ **One macro, `LF_RESEARCH_CMDS_ENABLED`, defined `0` in `data_cmd.h`.** A plain build is
therefore the SHIPPING build; `application/Makefile` sets it to `1`, and **that single line is
what an upstream PR deletes** rather than a hunt through five files.

⭐ **The cost is measured, both ways, not asserted:**

| build | text | bss |
|---|---|---|
| gated **off** (default) | 335,492 | 146,068 |
| gated **on** (this branch) | 337,052 | 154,084 |

⇒ **1,560 bytes of flash and 8,016 of RAM.**

⚠ **RE-MEASURED 2026-09-14 (C390) AND BOTH NUMBERS HAVE GROWN** — the figures this replaces were
1,088 and 4,008, taken on 2026-09-15 against a much earlier tree. The RAM cost has **exactly
doubled**, 4,008 → 8,016, which is too clean to be drift: it is two staging buffers where there
was one. ⛔ **Do not quote the old numbers to a reviewer**, and re-run `arm-none-eabi-size` on
both builds rather than trusting this table after the next round of instrumentation — the
measurement is two builds and five minutes, and it has now been wrong once. The RAM is attributed to the byte by an
`nm` symbol diff — `out.0` (4,000) + `buf.1` (4) + `nsamp.2` (4), all static locals of the
capture command's handler. ⭐ That **confirms** the source comment claiming the probe itself
costs no RAM: `m_samples` (0x7000) is present identically in BOTH builds, so the 4 KB is the
command's chunk staging buffer, which is a different thing.

⭐ **The host needs no gating at all, and that is verified on hardware.** `GET_DEVICE_CAPABILITIES`
(1035) makes the device declare which command ids it implements — #2 returns **195**, including
3037, 3038 and 3060 — and `chameleon_cmd.py` already prints a clear message for a command the
device does not understand. A gated-off build simply stops listing them.

⚠ **One of the four is not a separate command and had to be gated differently.** The GProxII
failure-energy payload changes the WIRE SHAPE of a failure reply, so gating it uses `#else` to
call `scan_gproxii()`, the plain sibling already in the tree. It must not ship on by default: a
host expecting an empty failure payload would misread four bytes of energy.

The sites, found by `grep` on 2026-09-14 and now the gated ones:

⚠ **LINE NUMBERS REFRESHED 2026-09-14 (C390); every one of them had drifted.** They are a
convenience, not the contract — the contract is the `#if LF_RESEARCH_CMDS_ENABLED` blocks and the
one `-D`. ⇒ **`grep` for the macro rather than trusting this table**, which is exactly how these
were re-found.

| | firmware | host |
|---|---|---|
| **3037 `LF_EMU_DEBUG`** (`hw emudebug`) | `data_cmd.h:258`, handler `app_cmd.c:762`, dispatch row `app_cmd.c:3968` | `chameleon_enum.py:204`, `chameleon_cmd.py:768`, `chameleon_cli_unit.py:9321` |
| **3038 `LF_RADIO_DEBUG`** (`hw lfdebug`) | `data_cmd.h:259`, handler `app_cmd.c:771`, dispatch row `app_cmd.c:3969` | `chameleon_enum.py:205`, `chameleon_cmd.py:794`, `chameleon_cli_unit.py:9348` |
| **3060 `LF_READER_CAPTURE`** | `data_cmd.h:292`, handler `app_cmd.c:855`, dispatch row `app_cmd.c:3961`, and `lf_reader_capture_probe()` at `lf_indala_data.c:270` / `.h:183` | `chameleon_enum.py:227` and nothing else |
| **The GProxII failure-energy payload** | `app_cmd.c:1016-1040` — the gated block; `scan_gproxii_energy()` at :1023, the plain sibling `scan_gproxii()` at :1033 | `chameleon_cmd.py:1049-1051` |
| **the gate itself** | `firmware/application/Makefile:431` — `CFLAGS += -DLF_RESEARCH_CMDS_ENABLED=1`, the single line an upstream PR deletes | — |

⭐ **Nothing shippable depends on any of them, and that is checked rather than hoped.**
`lf_reader_capture_probe()` has exactly one caller (`app_cmd.c:866`); the two debug handlers are
`static` and reached only through their own dispatch rows; and the GProxII energy path already
has a plain sibling in the tree — `scan_gproxii()` at `lf_reader_main.c:192` wraps
`scan_gproxii_energy()` and discards the energy. So that fourth row is two lines: call the
sibling, delete the failure branch. **The split is subtraction, not surgery.**

⭐ `rdrcap.py` needs nothing done to it. It hardcodes `CMD = 3060` and never imports the enum,
and it lives under `research/`, which is in no PR.

⭐ **Nothing is lost, which was the point of the pushback.** The capture probe is what cracked
C211 after six hypotheses had been refuted by measurement, and the failure-energy payload is what
separates "the capture was wrong" from "the decode was wrong" on a silent read — the distinction
C206 turned on. Under the original "strip it" plan both would have been deleted to ship. They now
survive in the tree at zero cost to a shipping image, and re-opening either question is one `-D`
away instead of a git-archaeology exercise.

### 9i. ⭐⭐ WHAT BECAME UPSTREAMABLE AFTER 2026-09-13 — and the one PR worth sending FIRST

⛔ **The counts in §9c are stale and are superseded here.** Recounted against `main` on
2026-09-16:

| | files | insertions |
|---|---|---|
| whole branch | 1,698 | 208,250 |
| **reviewable CODE** (`firmware/` + `software/`) | **64** | **+11,327 −154** |
| research notes and captures | 1,633 | +196,922 |

⚠ §9c said "59 files, 10,242 lines". The reviewable surface has grown by 5 files and ~1,100
lines since, which is the honest number to quote rather than the one already written down.

⭐⭐ **PR 0 — THE `unpack()` FIX, AND IT SHOULD GO FIRST BECAUSE IT DEPENDS ON NOTHING HERE.**

**3 files, +102 −16.** `wiegand.c`, `wiegand.h`, `hidprox.c` — all of which exist upstream
already, and `wiegand.h` gains **no new includes and no new externs**, so it carries none of
this branch's LF protocol work with it. It is separable today, without the five-PR sequence
below ever happening.

- **What it fixes**: `unpack()` returned the first layout of the right bit length whose
  unpacker did not refuse. On real tags that relabels **15 of 29 writable formats** (C284) — a
  Kastle tag holding fc 1 / cn 1 reports as Check Point card 8389632.
- **Why it is safe to reason about**: the table already carried `fields.has_parity`, and C300
  proved by measurement that the flag is accurate — the 12 rows flagged 0 are exactly the 12
  that accept 100% of random frames. The fix consults a column that was already correct.
- **Verified on hardware**: KASTLE now reads back as itself **4 of 4** (C304), on a tag whose
  prior build reproduced C284's failure.
- ✓✓ **SEPARABILITY IS PROVEN BY CONSTRUCTION, not by reading includes (C313).** The 163-line
  patch was applied to a clean `main` worktree: `git apply --check` passes, it **builds**, and it
  costs **+248 bytes of flash and no BSS** (297,892 → 298,140). Unpatched `main` was built first
  as the control, so a failure could not have been misattributed.
- ✓ **The `formats[]` table is BYTE-IDENTICAL to main** — zero table-row changes on this branch —
  so the fix rests entirely on `has_parity` data that already exists upstream and cannot secretly
  depend on a branch-only edit.
- ⚠ **ONE DEPENDENCY, NAMED AND BOUNDED.** `lf_hidprox_data.c` carries a **4-line** change this
  patch does not include: the C47/C250 BLE advertising guard. It affects **how often a read
  succeeds**, not which format wins — so PR 0's correctness transfers, but a maintainer testing on
  stock firmware will meet C45's old 15-20% intermittency. ⇒ **Send it as PR 0b — but it is NOT the "4 lines"
  this section first said (C314).** Compiling it on `main` found a two-step dependency chain: the
  call site needs `lf_adv_suspend()`, which needs `g_is_ble_connected`, which this branch also
  added. **The real surface is 5 files, +140 −3** — `lf_reader_data.c/.h`, `lf_hidprox_data.c`,
  `ble_main.c/.h` — and it costs **+120 bytes** on top of PR 0. Still worth sending, and still the
  difference between 96/96 and 71/80 (C250), but a plan promising a one-liner would have died on
  contact.
- ⛔ **It IS a behaviour change to shipping code and the PR must open with that**, not bury it:
  a reader that used to answer `HCP32` will now answer `KASTLE`. That is the point, and a
  maintainer must be given the chance to disagree. The measured before/after is in `ctest/ambig.c`
  and runs in `make check`.

⭐ **Three smaller things, each independently landable:**

| what | size | behaviour change? |
|---|---|---|
| **`blk_count == 0` guards** (C307) — 5 writers that shared `fsk2a_t55xx_blocks()` could write NOTHING and still return `STATUS_LF_TAG_OK` | 5 hunks | ⚠ only when the count IS 0, which never happens on a working path |
| **`t55xx.h` parameter rename** (C310) — the header called `t55xx_send_cmd()`'s third argument `data_len`; the code has always treated it as `lock_bit` | 1 line | none — documentation only |
| **`lf t55xx write`** (C307) — the firmware handler and host binding both existed with no CLI command | host only | none — new command |

⭐ **And §9h no longer describes a deletion.** The instrumentation is **compile-gated**
(C303): `LF_RESEARCH_CMDS_ENABLED`, default 0, so a plain build is the shipping build and the
upstream diff is one deleted `Makefile` line. Command **3063** and `raw_read_samples_probe()`
(C308) were added after that and are gated the same way, so they cost a shipping image nothing
and need no separate removal.

### 9j. ⭐⭐ REFRESHED 2026-09-14 — a recount, a retired caveat, and seven fix-PRs that go first

⚠ **§9c's numbers are from 2026-09-13 and the branch has moved.** Recounted against `main`:

| | 2026-09-13 | now |
|---|---|---|
| reviewable CODE (firmware + host scripts) | 10,242 lines, 59 files, 29 new | **11,584 lines, 63 files, 31 new** |
| — of which the LF firmware surface | — | 51 files, +7,320 / −132 |
| — of which host scripts | — | 4 files, +3,171 / −20 |
| committed captures under `caps/` | 36,012 lines, 1,409 files | 36,012 lines, **1,441 files** |
| research notes (`.md`) | 4,560 lines | **5,888 lines, 12 files** |
| research tooling (`.py`, `.sh`, `ctest/`) | counted with the notes | **7,178 lines, 49 files** |
| branch total | 204,959 insertions / 1,654 files | **209,204 / 1,701** |

⇒ The reviewable surface grew by ~1,300 lines, not by the 4,000 the branch total suggests: most
of the growth is notes and tooling, which no upstream PR carries.

⛔⛔ **PR 4's FDX-A CAVEAT IS FALSE AND MUST BE DELETED BEFORE ANYONE READS IT.** It says:
*"FDX-A ships READ ONLY and the PR must say why: nothing on any bench here can read an FDX-A tag
back, so a writer would certify itself (C185)."* The Proxmark has a complete FDX-A under
**`lf destron`** — demod, reader, clone and sim (C333). The refusal came from looking under
`lf fdx`, which is FDX-B, a different protocol. ⇒ **FDX-A now ships READ AND WRITE**, both grade
A: read 10 of 10 on a real Proxmark-written tag across two credentials (C338), write 4 of 4 from
a confirmed-blank tag with the credential alternating every round, and the stored blocks
byte-identical to `lf destron clone`'s own (C340). ⭐ The PR should carry the reversal rather
than quietly drop the caveat — a reviewer who sees a refusal in one commit and a writer in the
next deserves the reason.
⚠ **InstaFob's refusal is NOT retired and must stay**: the Proxmark genuinely has no InstaFob
command, so its write arm would still certify itself.

⭐⭐ **`FIXES.md` IS NOW SEVEN ENTRIES, AND THEY ARE THE PRs THAT SHOULD GO FIRST.** Each is
scoped standalone, none depends on the protocol work, and six of the seven are defects present
on `main` — so they are the cheapest possible thing for a maintainer to say yes to, and they
shrink the protocol PRs by removing arguments that do not belong in them:

| | what | ours or upstream's |
|---|---|---|
| F1 | T5577 writes silently password-protect the tag, key differs by protocol | upstream — ⛔ the worst of them |
| F2 | five writers report success having written nothing | upstream |
| F3 | header declares a lock bit as a length | upstream |
| F4 | `unpack()` relabels 15 of 29 writable HID formats | upstream |
| F5 | two 28 KB capture buffers resident at once, 22% of RAM | upstream |
| F6 | `lf hid prox write` reported success without reading back | upstream |
| F7 | the repeat-read corroboration rule compared only the first 64 bits | **ours** |

⭐⭐ **NOW ELEVEN, NOT SEVEN (C349, C350).** Four more were found in two ticks by auditing along two different axes —
by FILE (which PR owns this?) and by CLAIM (did this fix reach the register?). Neither would have found the other's:
the file audit is structurally blind to a fix inside a file that IS assigned, which is where F10 and F11 were.
⛔ None of the four was new work. All had been done, verified on hardware and written up in FINDINGS — some months
apart — and none had reached the register that decides what gets upstreamed.

| | what | ours or upstream's |
|---|---|---|
| F8 | A BLE advertising burst collapses the field mid-capture — 15-20% of HID/ioProx reads | upstream |
| F9 | `lf pac read` returns nothing: the reader's own field saturates its amplifier | upstream |
| F10 | Changing a slot's LF type silently disarms emulation until a reboot | upstream |
| F11 | The emulation burst is a frame count, so long-window readers fail at the boundary | upstream |

⛔⛔ **AND THE ORDER IS CONSTRAINED, WHICH THIS SECTION DID NOT KNOW (C362).** Extraction against `main` has been
run three times: **F1 builds alone**, **F8 builds alone**, **F9 does NOT** — it needs `lf_125khz_radio_drive_set()`,
which PR 1 introduces. So F9 lands after PR 1, not among the free-standing fixes. F8 and F9 also share
`lf_pac_data.c`, so they must be ordered relative to each other. **Eight entries remain untested — assume nothing
about their scope until each is built against `main`.**

⇒ **The order is F1 first** — it is a data-loss defect that locks a user's tag out of every
other tool, and it is the one a maintainer will care about most. F7 is ours and belongs with
the shared capture engine (PR 1), not with the others.

⚠ **§9h's instrumentation checklist still holds and has one more entry.** The
`lf_sampled_stats_t` scan counters added 2026-09-14 (C337) live behind the same
`LF_RESEARCH_CMDS_ENABLED` macro and surface on a FAILED `FDXB_SCAN`. They change the wire shape
of a failure reply, which is exactly why they are gated — and the one line in
`application/Makefile` that sets the macro to 1 is still the whole of what an upstream PR
deletes.

### 9c. ⭐ Recommended shape — three PRs, not one

⛔ **Superseded in part by §9i above**: the counts here are from 2026-09-13, and the list below
is FIVE PRs under a heading that says three. §9i adds a sixth — PR 0, the `unpack()` fix — which
depends on none of them and should go first.

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
   writers that go with them. ⛔ **FDX-A's READ-ONLY caveat here was FALSE — see §9j.** It
   ships READ **AND WRITE**, both grade A (C338, C340): the Proxmark has a complete FDX-A under
   `lf destron`, and C185 looked under `lf fdx`, which is FDX-B.
5. **The ASK/biphase family** — `lf_ask_biphase.c` plus GProxII and FDX-B. ⭐ This one carries
   the 50ms inter-capture gap (C213), which is a HARDWARE finding rather than a protocol
   feature and is the part of this branch most worth a reviewer's attention: a capture taken
   too soon after another clips at both rails, and every decoder that thresholds on amplitude
   is exposed to it. ⚠ No emitter ships with it — see the grid.

⚠ That is FIVE PRs now, not three. The split grew because the branch did; keeping the old
number would have meant either a dishonest count or two families smuggled into someone else's
review.

### 9d. ⭐ The split at FILE level — so it can be executed rather than re-derived

⛔⛔ **TESTED 2026-09-16 AND PR 1 DOES NOT BUILD AS LISTED (C315).** The table below was derived
by reading, and it is missing two things PR 1 cannot compile without: **`ble_main.c/.h`** (it
supplies `lf_adc_set_acq_fast` and `g_is_ble_connected`) and **a hunk of `app_cmd.c`** (PR 1
changes `raw_read_to_buffer`'s signature and renames `LF_SNIFF_MAX_SAMPLES`; `app_cmd.c` calls
both). ⇒ ⭐ **UPDATE (C316): PR 1 now BUILDS, and its `app_cmd.c` share is THREE LINES** — adapt the
callers to the new API, do NOT import HEAD's chunked sniff handler. Verified minimum: 10 files +
`ble_main.c/.h` + `app_cmd.c` (+4 −3). ⭐⭐ **AND C317 SAYS THE RAM BLOCKER IS FIXABLE**: the image carries TWO 28 KB capture buffers
(`m_samples` and `sniff_buf`) that are never live together. Moving one shared buffer into
`lf_reader_generic.c` returns 28,672 B — more than PR 1's whole RAM cost. ⛔⛔ **The real blocker is RAM, not packaging**: PR 1 grows
BSS by **+24,692 B**, almost all of it `sniff_buf` going from 4,000 to 28,672 bytes — ~10% of the
chip's RAM for one static buffer, which this plan has never mentioned and a reviewer certainly
will. ⇒ The old note that the hunk-split is the blocking task — the note
below says it "must be split by hunk" and it never has been, and that diff is +827 −8 across all
five PRs. ⭐ PR 0 (§9i) is unaffected: it builds alone.

⭐⭐ **COMPLETENESS AUDITED 2026-09-14 (C349), AND IT FOUND TWO UNREGISTERED UPSTREAM FIXES.** Every file changed
against `main` was checked against the table below: **63 changed, 10 named by no PR.** Two were fixes to UPSTREAM
readers that had no `FIXES.md` entry — **F8, the BLE advertising guard** (the real cause of C45's 15-20% HID
intermittency) and **F9, the PAC drive sweep** (0 of 10 → 10 of 10). Both are now registered and both go with the
fix-PRs, not the protocol PRs. The other eight are assigned below.
⛔ **Build-tested is not the same as complete.** PR 1 was proven to compile (C315-C317) and the table still had ten
orphans, because compiling proves the files you listed are sufficient — never that they are all of them.

| orphan | belongs to |
|---|---|
| `lf_hidprox_data.c`, `lf_ioprox_data.c` | **F8** — the BLE advertising guard |
| `lf_pac_data.c` | **F9** — the PAC drive sweep |
| `wiegand.c/.h`, `hidprox.c` | **F4** — the `unpack()` relabelling |
| `lf_t55xx_data.c` | **F1** — the T5577 password defect |
| `app_status.h` | **PR 1** — it adds `STATUS_LF_SIGNAL_NOT_DECODED`, used by the shared PSK1 failure path |
| `chameleon_cli_unit.py`, `chameleon_enum.py` | every PR, by hunk — covered by the note below but never named |

61 files change. Listing them by PR is the difference between a plan and an intention, and the
shared files are the part that actually needs thought: `app_cmd.c`, `data_cmd.h`,
`lf_reader_main.c/.h`, `lf_indala_data.c/.h`, `tag_base_type.h`, `tag_emulation.c`, the
`Makefile` and all three Python files are touched by EVERY PR and must be split by hunk.

| PR | files it OWNS | notes |
|---|---|---|
| **1. Shared engine** | `lf_indala_psk.c/.h`, `lf_slicer.c/.h`, `lf_reader_generic.c/.h`, `lf_reader_data.c/.h`, `lf_125khz_radio.c/.h`, `netdata.h` | ⛔ Also carries the RENAMES (`lf_sampled_*`, `lf_drive_swept_read`) and the sizing constants. Everything below depends on it, and it touches no protocol |
| **2. PSK1 family** | `keri.c/.h`, `nexwatch.c/.h`, `psk1.c/.h`, `indala.c/.h`, `idteck.c` | The `lf_psk1_format_t` descriptor refactor plus two formats. Indala and IDTECK exist upstream, so most of this is the refactor |
| **3. ASK/Manchester family** | `lf_ask_manchester.c/.h`, `gallagher.c/.h`, `securakey.c/.h`, `noralsy.c/.h` | ⚠ Carries the drive sweep. ⛔ InstaFob stays OUT — no verifiable write arm |
| **4. FSK2a family** | `lf_fsk2a.c/.h`, `fsk2a_t55xx.c/.h`, `awid.c/.h` | ⛔ **FDX-A's read-only caveat is RETIRED (§9j)** — it ships read AND write, both grade A (C338, C340). ⛔ The AWID EMITTER should not ship at all until C243's mystery is solved — it is silent on hardware and shipping it would be the self-certification this project refuses |
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
