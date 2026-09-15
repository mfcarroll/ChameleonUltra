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
4. Read `TOOLS.md` **before driving anything** — it is the complete tool inventory and its
   trap table is three things that have each cost rework (C410). Then `FINDINGS.md` and
   `METHOD.md` if this is a fresh context. `NEXT.md` is the plan;
   this file is the queue. They must not disagree — if they do, `NEXT.md` wins and you fix
   this file.
5. Take the first unit that can be **finished**. Work it to completion, verify it on
   hardware, commit the code and its notes together, `beat`, and go to the next.
6. Never ask a question. If blocked, append the blocker to §5, add it to `NEXT.md`'s
   **Needs hands** table, and take the next unit.

---

## 1. STATE

### ✅ 2026-09-14 18:20 — BOTH DEVICES ARE ON HEAD AND THE FIX LEDGER IS RE-VERIFIED (C419)

✅ #2 was `v2.2.0-712-gba7e722`, #1 was `v2.2.0-740-g4aba1a2`; **both now run `v2.2.0-769-g3a0e687`**, `status`'s
⛔ is cleared, and `./fixcheck.sh` reports **12 pass, 0 FAIL** on that build.

⭐⭐ **THE TARGETED FLASH — THE GUARD IS THE SERIAL, NOT THE TRAIT.** Trigger DFU on the chosen `/dev/cu.` port,
then **refuse to program unless the listing shows exactly ONE DFU device AND its serial is the intended one**
(`F429364E4696` = #2, `C3A1656543DE` = #1), then `nrfutil device program --serial-number <that> --traits
nordicDfu` with `/Users/Shared/code/personal/rfid/.tools/bin/nrfutil` (⛔ the bare name is not on PATH, C200).
⚠ **§5's cross-check does not work on this host**: it says the other unit's normal port still being present
identifies the one in DFU — macOS leaves BOTH nodes listed after the trigger. The serial is the only answer.

⚠ **BUILD AFTER COMMITTING.** The first attempt flashed a `-dirty` package and `status` caught it (*built from a
DIRTY tree, so it matches no commit*), because it was built while the notes edits were still uncommitted.

---

### ⭐⭐ 2026-09-14 17:35 — THE MIXED-FRAME FAILURE IS DETERMINISTIC. F12 IS NOW A BOX (C416)

⭐ **Five repeats of the identical mixed frame**: 0.6%, 0.3%, 0.1%, 0.5%, 0.6% RF/10 — all under 1%, a spread of
half a point. **Three repeats of a pure frame** as the stability control: 73.6%, 74.7%, 71.9%, CV **0.02**.
⇒ A signal marginal enough to lose its long tones would sometimes half-succeed. Five of five do not. **The
emitter destroys tone diversity reproducibly**, which retires *is it just weak?* — live again since C401
invalidated the instrument C387/C388 used to eliminate depth.

⚠ **A criterion of mine was the wrong statistic and its number is not quoted.** I pre-registered *CV above 0.4
means marginal*; the mixed arm returns 0.47, and that is meaningless when CV divides by a mean of 0.4. The
absolute spread carries the result. ⇒ Do not pre-register a RATIO against a quantity that may be ~0.

⭐⭐ **F12 AS A BOX, EVERY SIDE MEASURED:**

| | |
|---|---|
| the sequence reaches the peripheral intact, at the right length | C414 (`SEQ[0].CNT` exact for 4 frames) |
| every encoding fails alike — not the encoding | C415 (ioProx's old encoding vs the new builder, both 0/6) |
| pure frames of EITHER tone are correct; every mixed frame collapses, flat with rate | C411 |
| the collapse is deterministic, not marginal | C416 (here) |
| the receiver is fine — it reads a real tag at 44.4% | C413 |

⛔⛔ **WHAT THIS BENCH CANNOT SETTLE.** The Flipper's raw reader returns **EDGES, not amplitude**, so *the long
tones are absent* and *the long tones are present but too shallow to cross the detector's threshold* are
**indistinguishable** to it. Everything above is consistent with either.

⇒ **THE NEXT MEASUREMENT NEEDS AMPLITUDE, AND ONLY OUR OWN SAADC PATH RETURNS IT** — `rdrcap.py` +
`DATA_CMD_LF_READER_CAPTURE`, which needs **the two Chameleons facing**. See §5. ⚠ **This is not C408 repeating
itself.** C408 was right that facing them served no open unit: it was wanted then for a DECODE, which the
phase-locked path cannot do from an emulation (M52). This asks for **raw samples**, which that path CAN return
and no other instrument here can.

---

### ⛔⭐ 2026-09-14 17:20 — THE ENCODING IS NOT THE DEFECT. TWO ENCODINGS FAIL IDENTICALLY (C415)

⭐⭐ **THE FACT, AND IT WAS ALREADY IN THE TREE**: `protocols/ioprox.c` **still runs the OLD encoding** — one entry
per tone, `counter_top` varying between `LF_FSK2a_PWM_HI_FREQ_TOP_VALUE` and `LO_FREQ_TOP_VALUE` — while `awid.c`
and `hidprox.c` run the new shared builder with `counter_top` CONSTANT and the tone in the duty. **All three
emulate 0 of 6.** ⇒ Two encodings differing in exactly the variable C382/C383 changed, producing the same
failure, so **the defect is common to both and is not the encoding**.

⛔⛔ **C414'S MECHANISM IS WITHDRAWN** (its eliminations and SEQ[0].CNT readings stand). *A real tag's mark tracks
its tone* was an over-reading of 44 hand-scanned pairs. Measured over the whole capture:

| source | mark on RF/8 periods | mark on RF/10 periods | ratio |
|---|---|---|---|
| **real tag** | 19 us (30% duty) | 50 us (63% duty) | **2.63** |
| ours, pure RF/8 | 37 us | — | — |
| ours, pure RF/10 | — | 53 us | — |

No tag emits 30% duty on one tone and 63% on the other — a subcarrier is a square wave. ⇒ **The measured pulse
width is the envelope detector's threshold, not the emitter's modulation**, so no emitter's duty can be inferred
from it, ours included. ⭐ And a real tag swings the detector's apparent duty FURTHER than we do (30->63 against
our 50->40) **and still decodes on that same receiver**, so *the DC average moves when the tone changes* cannot
by itself be fatal.

⚠ **A second assumption checked before it was written down, and also false**: PSK1 does **not** vary `counter_top`
— `utils/psk1.c` sets a constant `LF_PSK1_SUBCARRIER_TOP` — so the 6/6 PSK1 arm is no evidence that per-entry
`counter_top` reaches the air.

⇒ **WHERE F12 STANDS.** The sequence reaches the peripheral intact (C414); pure frames of EITHER tone emit
correctly and every mixed frame collapses regardless of rate (C411); the encoding is not the variable (here).
What remains common to all three encodings is **the playback path in `lf_tag_em.c` and the analog side**.
⛔⛔ **DO NOT REBUILD THE EMITTER AGAIN until those two are told apart.** Three encodings have now been written
and the air has not changed — that is the pattern this entry exists to stop.

---

### (hypothesis withdrawn by C415; its eliminations stand) 2026-09-14 17:05 — F12 NARROWED (C414)

⭐ **Four eliminations, all measured:**
1. **Truncated playback is refuted** — `ff00...` leads with eight 1-bits and still emits only 6.2% RF/10.
2. **The device loads the RIGHT LENGTH every time.** `hw lfdebug`'s `SEQ[0].CNT` reads **9216 / 9408 / 9340 /
   9600** for all-zeros, alternating, the real AWID frame and all-ones — exactly `4x(2304+k)` for each frame's
   one-bit count. **The long tones ARE in the buffer the peripheral is reading.**
3. **DECODER load is WAVEFORM, base clock 1MHz** — both correct.
4. `recompute_frames_per_burst()` was already cleared; it sums the real `counter_top`s.

⚠⚠ **NEAR-MISS WORTH KNOWING: `PWM0 COUNTERTOP` reads 1000 where ours is 16.** That looks damning. Indala PSK1
and Gallagher — **both 6/6 working arms**, same peripheral, same WAVEFORM mode — read **1000 too**. It is the
init default. ⇒ Take the control before believing a register.

⭐⭐ **WHAT IS ACTUALLY DIFFERENT, FROM THE REAL TAG'S OWN CAPTURE: ITS MARK TRACKS ITS TONE AND OURS DOES NOT.**
A real tag puts a **~50 us** mark on its 76-79 us periods and a **~20 us** mark on its 62-65 us ones — roughly
constant duty, which is what a subcarrier square wave is. We hold the mark **fixed at 4 carrier cycles**, so our
duty is **50% for RF/8 and 40% for RF/10**, and the DC average of our load modulation moves whenever the tone
changes. ⇒ That mechanism predicts **exactly** C411's cliff — a pure frame settles at one average and works in
both directions, a mixed frame never settles — and predicts its rate-independence too, since what matters is
that the average moves at all, not how often.

⛔ **HYPOTHESIS, NOT MEASUREMENT.** The tank's Q makes the same prediction and is not ours.

⇒ **THE TEST IS ALSO THE FIX, and it needs no hands**: make the mark HALF the tone period, as a real tag does —
mark 4 / gap 4 for RF/8, mark 5 / gap 5 for RF/10, constant 50% duty. `counter_top` must then divide 4 AND 5
carrier cycles, so **8 ticks rather than 16**, which doubles the worst case to **4800 entries (38,400 bytes)**.
⚠ Pin it in `ctest/roundtrip.c` BEFORE it goes near hardware — that arm has caught three wrong encodings.

---

### ✅⭐⭐ 2026-09-14 16:55 — F12 CONVICTED AGAINST A REAL TAG, AND C400 CLOSED AS NOT REPRODUCIBLE

⭐⭐⭐ **F12 IS A FIRMWARE DEFECT, NOT AN INSTRUMENT ARTEFACT (C413).** A Proxmark-written HID Prox H10301
carrying **the same credential our emulator was arming** was put on the Flipper's pad — a genuine mixed-tone
FSK2a source, so the ideal tone composition is identical and the only variable is emitter versus tag:

| source — same credential, same receiver, same pad | ASK chain | PSK chain |
|---|---|---|
| **real pm3-written tag** | **44.4%** RF/10 | 16.7% |
| **our emulation** | **7.0%** RF/10 | — |
| expected | ~45% | ~45% |

⭐ **The real tag shows TWO CLEAN PEAKS of near-equal height** — 76 us: 20,218 and 60 us: 19,417 — which is what
FSK2a should look like. Ours shows **one** peak at 60-64 plus spurious short durations (32, 36, 20 us) the real
tag never produces. ⇒ **The ASK chain is exonerated and calibrated** (44.4% against ~45% expected), so C409's
direction was right all along. ⛔ **It is the PSK chain that is the bad tone meter** — 16.7% on the tag ASK reads
at 44.4% — so every PSK number in C411, including the 2-3x that prompted its correction, is an artefact.
⚠ C409's *"absent"* stays too strong: ours is 7.0%, not zero.

⇒ **NEXT, and it needs no hands**: the cause is bounded to a sequence MIXING 4-entry and 5-entry tones (C411's
cliff — pure frames are correct in both directions, every mixed frame collapses, flat across alternation rates).
`recompute_frames_per_burst()` is already cleared. The spurious 20-36 us durations that only OUR emission
produces are the next thread: they are shorter than one tone period, so something is cutting entries short.

✅ **C400 IS CLOSED AS NOT REPRODUCIBLE (C412) — 18 of 18.** Gallagher 6/6, Securakey 6/6, Noralsy 6/6 on real
pm3-written tags with the judge reading back **every** write first. ⭐ **It was never code**: zero commits have
touched `firmware/application/src/rfid/reader/lf/` since the entry that recorded the failure, so the proposed
bisect would have found nothing. ⛔⛔ **Its controls were both on the WRONG ENGINE** — EM410X and HID Prox are
both GPIO (`lf_reader_data.h`, no swept read); Gallagher, Securakey and Noralsy are all SAADC
(`lf_drive_swept_read` in `lf_indala_data.c`). The SAADC engine had no passing arm, so *neither capture engine
is down* was never established. **M53** is the rule; §2 U5's stale claim is where the error came from and is
fixed.

⭐ `pm3written.sh` now asks pm3 to read back its own write before scoring our reader, and calls the arm **VOID**
rather than printing a reader result (M33). ⚠ **Bench during this measurement**: #1 alone and off the pad, #2 on the pm3 pad with no tag, the T5577 on the
Flipper. ✅ **Restored at 17:00 and VERIFIED by `./autopilot.sh bench`** — both rigs LIVE, `#1 ↔ #2` DEAD as it should be. ⛔ Always **run `./autopilot.sh bench`
before trusting either rig again rather than assuming it is back** (C408's whole point).

---

### ⛔⭐ 2026-09-14 16:35 — THE CLIFF IS REAL, THE MAGNITUDE WAS PARTLY MY INSTRUMENT (C411, correcting C409)

⭐⭐ **READ THIS BEFORE THE SECTION BELOW IT.** C409 said the emitter puts ONE tone on the coil and RF/10 is
absent. The direction holds; *absent* does not. The **same emission** through the Flipper's **PSK** filter
chain instead of its ASK chain returns **2-3x more RF/10** — the alternating frame 7.9% -> **21.6%**, the real
AWID frame 0.4% -> **2.3%**. Only the receiver changed. ⇒ **No single-chain number is the emitted ratio.**

⭐ **WHAT SURVIVES, AND IT IS STILL THE FINDING.** Both chains agree on direction and the shortfall is far too
large to be instrument alone:

| frame | ASK chain | PSK chain | expected |
|---|---|---|---|
| all 0s | 0.1% | 0.1% | 0% |
| real AWID frame | 0.4% | **2.3%** | **30.4%** |
| alternating every bit | 7.9% | **21.6%** | 45.5% |
| all 1s | 73.1% | **84.9%** | 100% |

⭐ **Pure frames are CORRECT IN BOTH DIRECTIONS** — so the emitter can produce either tone and both chains can
resolve either. That is the positive control at the right timescale, and it is a real one, unlike C409's
internal argument from its own peak.

⭐⭐ **IT IS A CLIFF, NOT C387'S RATE CURVE — five points, not three**: pure 73%, run-8 **6.2%**, run-4 4.1%,
run-2 5.2%, run-1 4.0%. It does NOT degrade in proportion to how often the tone changes; it falls the moment
the frame holds BOTH tone lengths and is flat thereafter. ⇒ **The cause is mixing 4-entry and 5-entry tones.**
⛔ The constant-bit-period hypothesis is dead too: `recompute_frames_per_burst()` sums the real `counter_top`s
and assumes nothing about bit length.

⛔ **BLOCKED ON CALIBRATING THE RECEIVERS, and it is ONE TAG MOVE** — see §5. Both available receivers are
envelope detectors with a duty-dependent bias, and mixing the tones IS a duty change (50% vs 40%), which is
what an AC-coupled front end with a ~27-sample time constant (C204) would distort.

---

### (corrected by C411 below) 2026-09-14 16:05 — THE FSK2a EMITTER IS SINGLE-TONE AT THE COIL (C409)

⭐⭐ **F12/U11 has its first direct measurement.** AWID and HID Prox both emit a sharp peak at **60-64 us**
— RF/8 — and **nothing at 80 us**, which is RF/10. AWID: 4081 of 16,983 pulse/duration pairs in one 4 us
bin at 60 us, against 130 at 80. HID Prox: 2502 at 64, no peak at 80. ⇒ **The long tone is not being
emitted at all**, so the defect is in the EMITTER and not the receiver. ⛔ **The cause is still open.**

⭐ **HOW IT WAS MADE TO COUNT, because the last answer to this question was retracted (C401):**
1. **The criterion was written down before any number was seen** — a band counts only as a **local
   maximum**, never as a bin with counts in it. C401 binned a decaying curve and called two bins bands.
2. **The analyser passed a known-good reference first** (M50): the EM410X control, a 6/6 arm, returned
   peaks at **512 and 1024 us** — exactly RF/64 Manchester's half-bit and full-bit. That validates the
   parser and fixes the time unit as microseconds without taking either on faith.
3. **The resolution objection was asked and answered internally.** The control sits at 512 us while the
   tones under test sit at 64-80, so a single peak could have meant *this instrument cannot resolve that
   timescale*. It can: a 4 us bin at 64 us holding 4081 counts IS a resolved feature in the band under
   test, so an RF/10 population would have formed its own peak. **That is what makes the negative
   attributable** rather than another unbracketed null.

⭐ **`flipraw.py` is the instrument** — `rfid raw_read` to a file plus a binary-safe `storage read_chunks`
fetch, parsing Flipper's `RIFL` format. ⛔ **NOT `raw_analyze`**, which wedged the Flipper twice (§5).
⚠ `raw_read` needs a **FULL PATH**: the bare name answers *"File is not RFID raw file"*.

⇒ **NEXT, and it is a narrow question now**: the shared builder holds `counter_top` at 16 ticks (2 carrier
cycles) and carries the tone in the DUTY pattern — mark 4 cycles, gap 4 for RF/8 and 6 for RF/10. The
emission says every period came out as the RF/8 shape. So the question is whether the gap-6 entries are
reaching the peripheral at all: compare `lf_fsk2a_build`'s entry counts against what `ctest/roundtrip.c`'s
`trial_fsk_duty` already pins (543 tones, mark fixed at 4 cycles, gaps 378 x4 + 165 x6) and then read back
what the device actually loaded. ⚠ **Rig A only, no hands, no bench change.**

⚠ Slot 8 on #1 is left holding HID Prox in emulator mode.

---

### ✅ 2026-09-14 17:55 — BOTH RIGS ARE LIVE AGAIN. RUN `./autopilot.sh bench` RATHER THAN ASSUMING (C408)

✅ **Rig A (Flipper + #1) and rig B (pm3 + T5577 + #2) are both restored and confirmed**: the Flipper reads
#1's Gallagher **3 of 3**, and #2 reads the tag **FC 123 / CN 4567**. Every open unit is reachable.

⭐⭐ **THIS IS THE SETUP TO KEEP.** The two rigs do not conflict. Facing the Chameleons traded them for a
single link that **no open unit needs** — asking for that was a mistake, and it cost C402/C403 (retracted).
⚠ The Flipper is a VALIDATED instrument, not a black box: `raw_analyze` decodes our Gallagher emission
byte-exact from a raw capture and produced the peaks C387's rate curve rests on.

⛔ **NEVER ASSUME THE TOPOLOGY AGAIN — `4 of 4 enumerated` says nothing about coupling.** §1 claimed rig A
worked for a whole session after the pads had changed, and a tick read the resulting expected nulls as a
four-arm regression. ⇒ **`./autopilot.sh bench`** probes all four links with EM410X (the only protocol proven
at both ends of every link, and GPIO-family so it is legal from an emulation, M52), and prints what each
missing link BLOCKS and what to ask for. It also separates *not coupled* from *the Flipper's plugin will not
load* (C377) — same null, different problems.

---

### (superseded) 2026-09-14 17:10 — THE BENCH WAS ONE LINK: #1 ↔ #2 (C405)

⭐ **THE TWO CHAMELEONS FACE EACH OTHER AND NOTHING ELSE IS ON THEIR PADS.** The T5577 sits on the
**Proxmark's** pad, completely separate, and the Flipper is not in the arrangement.

| link | status | what a null there MEANS |
|---|---|---|
| **#1 ↔ #2** | ✅ **the only live one** | #2 decodes #1's EM410X; #1 decodes #2's |
| Flipper ↔ #1 | — **not in the arrangement** | Flipper reads 0 of 3 of #1's Gallagher (6 of 6 this morning) — *the Flipper is not there* |
| Chameleon ↔ T5577 | — **no tag on those pads** | #2 gets `LF tag not found` — *the tag is not there* |

⚠ **NOTHING IS BROKEN — THIS IS A DELIBERATE REARRANGEMENT.** An earlier draft of this section called the two
links "DEAD", which would have sent a tick hunting a fault that does not exist.
⭐ **But one measurement IS load-bearing**: `flipper.py` ABORTS when its plugin will not load (C374) and it
SCORED instead, which proves the Flipper is alive and this is geometry rather than C377's heap fault
recurring. **pm3 still reads the T5577.**

⛔ **WHAT IS BLOCKED**: the **emulate column** (8 of 11 at 6/6) cannot be re-measured — it needs the Flipper as
reader; and **C400's real-tag read arms** cannot be retested — they need #2 at the T5577.
✅ **WHAT WORKS**: `./fskcap.sh` captures (#1 emulates, #2 captures raw), either Chameleon reading the other's
emulation **on the GPIO family ONLY** (M52 — never the SAADC family), and pm3 with the T5577 alone.
⚠ **THE THREE CONFIGURATIONS ARE MUTUALLY EXCLUSIVE.** The bench is a choice now, not a given, and any future
*needs hands* must say WHICH of the three it wants.

---

### ⛔⛔ 2026-09-14 16:55 — TWO READ ARMS DOWN ON REAL TAGS. THE "FOUR" WAS MY TEST METHOD (C400, C404)

⛔⛔ **READ THIS BEFORE THE BLOCK BELOW, WHICH IS RETRACTED.** C402 reported four arms down, boundaried as
*protocols this branch added*. **That was an artefact of reading one Chameleon's EMULATION with another's
SAADC capture path, which cannot work by design** — and the firmware says so itself. Probing **Indala**, the
flagship read on the same path, returns the reader's own diagnostic:

> *a tag-like subcarrier is present but no frame of the requested type could be decoded ... or an emulated
> tag whose subcarrier is not locked to the reader's own carrier*

⭐ A real tag DIVIDES the reader's carrier, so its subcarrier is inherently phase locked. An emulating
Chameleon generates PWM from its own clock, and *in reader mode the carrier is generated, not recovered —
nothing in the design ever knew a reader's carrier phase*.
⇒ **The split is real but I named it wrong**: it is **needs phase lock (SAADC: indala, gallagher, securakey,
noralsy, gproxii) vs envelope (GPIO: em410x, viking, hidprox, ioprox)**. EM410X and Viking were "controls"
that could not have failed.

⛔ **RETRACTED**: C402's attribution, its Noralsy and GProxII failures (emulation-only ⇒ **status unknown**),
and **C403 entirely** — it re-ran the same invalid configuration on pre-session firmware, so it says nothing
about either build.

✅ **WHAT STANDS IS C400 AND ONLY C400** — and it is still a real defect:
**Gallagher 0 of 6 and Securakey 0 of 5 against REAL Proxmark-written TAGS** read by #2, with **EM410X 2/2**
and **HID** passing on the SAME tag in the SAME position. A real tag is phase locked, so this is not explained
by the above.
⛔ **It can only be retested by moving #2 back to the T5577** — the rigs are mutually exclusive.
⇒ **NEXT: do NOT test a SAADC-path read arm against an emulation. Ever.** Either the bench moves back, or the
arm is left unmeasured and said to be so.

---

### (RETRACTED by C404) 2026-09-14 16:25 — FOUR READ ARMS DOWN: EVERY ASK/BIPHASE PROTOCOL THIS BRANCH ADDED (C402)

⛔ **Read this first — it supersedes the RF/32-and-RF/40 boundary below, which is REFUTED.**

| protocol | coding | #1 reads #2's emulation |
|---|---|---|
| **EM410X**, **Viking** | pre-existing upstream | **✓** control, taken before AND after |
| Gallagher RF/32, Securakey RF/40, Noralsy RF/32 | ASK, branch-added | **0 of 7** |
| GProxII RF/64 | biphase, branch-added | **0 of 2** |

⛔ **GProxII is RF/64 — the same rate as the passing control — so it is NOT bit rate.** The boundary is
*protocols this branch added* against *protocols that were already there*, on the SAME GPIO/comparator path.
⭐⭐ **AND IT IS NOT THE DECODE MATHS**: `make check` passes all four on the host against synthesized
waveforms — *Gallagher ASK RF/32 decode exact*, *Securakey*, *Noralsy*, *GProxII biphase RF/64 decode exact*.
⇒ **The device-side scan path for the branch's protocols is where to look.** EM410X and Viking reach the
capture through their own upstream scan functions.
⭐ **Two devices, two signal sources**: C400 saw the same on REAL pm3-written tags read by #2. The
bench-geometry explanation C400 could not exclude is dead.
✅ **AND IT IS NOT THIS SESSION'S DOING (C403)** — proven by flashing, not argued from the diff. Built
`940ba078`, the last firmware commit BEFORE this session, flashed it to #1 and re-ran the arms:
**Gallagher 0/3, Securakey 0/2, EM410X control reading `deadbeef88`** — identical to HEAD. The tag and
reader paths share PWM0, so my base-clock change was a live objection a diff could not answer; it is
dead now.
⚠ **Not attributed to a commit**, but the range is bounded: the fault is OLDER than `940ba078`, and it
is the branch's own arms that are down — recorded verified (`lf securakey read` 6 of 6; C343's
nineteen at 76/76). Bisect is the fallback if inspecting the scan path does not name it.

⛔⛔ **BENCH TOPOLOGY CHANGED AND IS NOW MUTUALLY EXCLUSIVE.** #2 faces #1 and **can no longer read the T5577
at all** — HID returns `LF tag not found` where it read FC 123 / CN 4567 an hour ago — while **pm3 still
reads that tag**. ⇒ **A tick that runs a read arm on #2 against the tag will get a FALSE failure.** Read arms
against a real tag now need the bench moved back; read arms against an EMULATION work as above.

---

### (superseded) 2026-09-14 15:55 — TWO ASK READ ARMS ARE DOWN, BOUNDED TO RF/32 AND RF/40 (C399, C400)

⛔ **This contradicts a headline claim and is the first thing to read.** `lf securakey read` is recorded as
returning `7fcb400001adea5344300000` **6 of 6**, and Securakey is one of C343's nineteen arms at **76 of 76**.
It now returns **`LF tag not found`** — 3 of 3 on a tag our own writer made, and **2 of 2 on one the
PROXMARK'S OWN ENCODER made**.

⭐ **pm3 reads both**: `Securakey - len: 26 FC: 0x35 Card: 64169, Raw: 7FCB400001ADEA5344300000`, and
`lf t55xx detect` says the tag is correct — **ASK, RF/40**, block 0 `000C8060`, blocks 1-3 exactly the raw.
⭐⭐ **The rig is exonerated in the same minute**: the same device, the same tag position, reads **HID FC 123 /
CN 4567** immediately afterwards. So this is protocol-specific, not coupling.
⭐ **Securakey EMULATION still scores 6/6** this session, so the frame and encoder are fine — it is the READ
path.

⛔⛔ **IT IS NOT ONE ARM — IT IS TWO, AND THE FAULT IS BOUNDED (C400).** On the same tag position, the same
device, minutes apart: **Gallagher (ASK RF/32) 0 of 6** and **Securakey (ASK RF/40) 0 of 5**, both on tags the
Proxmark's own encoder wrote and both of which pm3 reads perfectly.

⭐⭐ **AND THE CONTROLS PASS, WHICH IS WHAT BOUNDS IT.** **EM410X reads 2 of 2** — the SAME GPIO/comparator
path the ASK family uses, so that engine is alive — and **HID Prox** reads on the SAADC path. Neither capture
engine is down and neither is the bench. What fails is specifically **ASK at RF/32 and RF/40**, the two rates
that go through the decoder this branch made BIT-RATE PARAMETERISED because the family does not share one.
⚠ **The failure is TOTAL, not marginal — 0 of 11 across two protocols** — which argues against the obvious
bench explanation that faster rates need more SNR than RF/64. A marginal field gives intermittent hits.

⚠ **STILL NOT ATTRIBUTED.** Nothing changed this session touches the ASK read path, and the instrument-first
rule says do not name a regression until it is measured.
⇒ **NEXT TICK: Noralsy — a THIRD ASK RF/32 point — to confirm the boundary, then bisect the ASK decoder
against the history to find when these arms last passed.**
⛔ Until then, treat *19 read arms, 76 of 76* as **carrying two known exceptions**.

---

### ⭐⭐⭐⭐ 2026-09-14 12:15 — THE EMULATE COLUMN IS MEASURED: 8 OF 11 AT 6/6. THE GAP IS FSK2a.

⭐ **Read this first; it replaces an 11:35 heading that said the column was unmeasured and rig A needed
hands. Both halves are now resolved, and neither needed the operator.**

#### The results — the first real emulate numbers this branch has ever had (C378)

| family | protocols | score |
|---|---|---|
| **PSK1** | Indala, IDTECK, Keri, NexWatch | **6/6 each**, ASK arm 0/6 as the wrong-modulation control |
| **ASK / biphase** | Gallagher, Securakey, Noralsy, **GProxII** | **6/6 each** |
| **FSK** | HID Prox, ioProx, AWID | **0/6 each** |

⭐ **The three failures are bracketed by positives in the same run** — Noralsy 6/6 immediately before, GProxII
6/6 immediately after — with clean nulls either side of every batch. So the FSK silence is **real**, not a
dead reader. ⭐⭐ **Credentials verified, not just hit counts**: GProxII reads back
`GProxII FAC2A38C2B081AF0210B12C2` (FC 123 / Card 1337 / LEN 26), byte-identical to the raw written; Indala
reads `Indala26 CD7A1D30`, FC 52 / Card 63612. Counting hits alone would have repeated C353's front-end trap.

⭐⭐⭐⭐ **U11'S ROOT CAUSE IS FOUND AND MEASURED OFF THE AIR (C382): OUR FSK EMITTERS EMIT ONLY ONE TONE.**
Captured our own emission with the Flipper's `rfid raw_read` and histogrammed the periods:

| capture | periods in the RF/8 band (58-70 us) | in the RF/10 band (74-88 us) | `raw_analyze` verdict |
|---|---|---|---|
| **HID Prox** | **2257** | **0** | `Protocol: not found` |
| **AWID** | **3620** | **2** | `Protocol: not found` |
| **Gallagher** (control) | RF/32 half-bits at 131 and 253 us | — | `Protocol: Gallagher [12 00 10 E1 00 00 1A 85]` |

✅ **AND THE EIGHT WORKING ARMS ARE RE-VERIFIED ON THE CURRENT BUILD (C394)** — PSK1 and ASK/biphase all
**6/6** with their wrong-modulation arms at 0/6 and clean nulls, AFTER the clock macro moved to
`IS_1MHZ_PWM_TYPE` and HID/AWID moved to a shared buffer. Shared infrastructure under working
protocols, where a wrong base clock is silent rather than loud — so this was a measurement, not a
formality.

⇒ **There is no frequency modulation on the air at all**, so an FSK decoder has nothing to find. The control
pins the scale (one unit ~1 us, RF/32 = 128/256 us) and proves the capture pipeline decodes correctly.

⭐⭐ **AND IT UNIFIES THE WHOLE COLUMN.** Every emitter that WORKS uses a **single constant `counter_top`** —
Gallagher 32, Securakey 40, Noralsy 32, GProxII one constant, em410x 64, every PSK1 protocol through psk1.c's
`LF_PSK1_SUBCARRIER_TOP`. **The three FSK emitters are the only ones that VARY `counter_top` within a
sequence, and the variation is exactly what never reaches the air.**

✅✅ **MECHANISM CONFIRMED BY EXPERIMENT (C383), no longer an inference.** Both hidprox tones were forced to
`counter_top` 10, built, flashed and re-captured: **RF/10 periods 0 → 2640, RF/8 periods 2257 → 4**, the
histogram peak moving 63 → **79 us**. ⇒ The peripheral plays exactly the period it is given, **the whole
sequence at ONE period** — so the defect is specifically that a per-entry `counter_top` which VARIES within a
sequence is not applied.

⛔⛔ **THE `counter_top = 2` FIX WAS ILLEGAL AND IS CORRECTED (C385).** COUNTERTOP in WaveForm mode has a
**minimum valid value of 3** (Nordic PS, recorded in `psk1.h`), so `gcd(8, 10) = 2` cannot be used.
⭐ **The repair is the BASE CLOCK.** At the FSK types' current **125 kHz** one tick is one carrier cycle and
the tones are 8 and 10 ticks. At **1 MHz** — what every PSK1 type already uses — a carrier cycle is 8 ticks
and the tones are **64 and 80 ticks, `gcd = 16`**. ⇒ **Constant `counter_top` 16**: RF/8 = 4 entries
(2 on, 2 off), RF/10 = 5 entries (2 on, 3 off), which reproduces C226/C380's measured **4-carrier-cycle mark
= 32 us = exactly 2 entries** for free. ⭐ ioProx's tone of 11 is legal too (64 and 88 ticks, `gcd = 8`), so
that question stays open rather than blocking.
⭐⭐⭐⭐ **U11 IS RATE-DEPENDENT — READ THIS FIRST, IT SUPERSEDES THE BLOCK BELOW (C387).**
The emitter produces EITHER tone perfectly and loses the long one in proportion to how often the tone changes.
Three diagnostic frames, same emitter, same instrument:

| tone changes | periods near RF/8 | near RF/10 | mark |
|---|---|---|---|
| **never** (all ones) | 1 | **3031** | — |
| **every 4 bits** (`0xF0`) | 2459 | **261** | 28-33 us |
| **every bit** (`0xAA`) | 1078 | **0** | 7-25 us |

⇒ **The encoding, the PWM and the 2-on/3-off duty pattern are CORRECT** — a steady RF/10 comes out at 79 us
exactly as designed. What degrades is the TRANSITION between tones, monotonically with its frequency, and the
mark quality tracks the same curve against an intended 32 us. **That is a settling signature, not a digital
one.**
⭐⭐ **NOT AN INSTRUMENT ARTEFACT — the control was taken.** A genuine mixed-tone reference (the Flipper
emulating `H10301`) captured through OUR OWN reader shows **both bands, 389 near RF/8 and 79 near RF/10**.
⚠ **Cause NOT established.** Tank settling fits every number, but a real FSK tag alternates every bit and
works — it SHORTS its coil, a far larger and faster perturbation than driving a transistor across that node.
⛔ **DEPTH WAS THE NAMED LEVER AND IT IS REFUTED (C388).** Mark 4 -> 6 carrier cycles, single clean
variable, against the `0xAA` worst case: **38 periods near RF/8, still ZERO near RF/10**, capture half
the size, marks splitting into ~556 us held levels and ~10 us blips. A 6-cycle mark is **75% duty on
RF/8**, so the field is loaded most of the time and the CONTRAST a reader measures is what gets spent.
⭐ A second independent vote for C226's fixed 4-cycle mark: it is at or near OPTIMAL, not merely faithful.
⇒ **U11'S CHEAP LEVERS ARE EXHAUSTED.** Encoding, loader, playback, clock, frame arithmetic, decoder,
mark shape and depth are all eliminated by measurement. What remains is the TRANSITION between tones,
degrading monotonically with its rate — and settling that needs an instrument this bench cannot point at
its own emitter. **See §5.**
⛔ **C386's *the long tone never appears* is superseded** — it appears perfectly when not asked to alternate.

⛔⛔ **THE FIX IS BUILT AND IT DOES NOT WORK — READ THIS BEFORE TOUCHING U11 (C386).**
`lf/utils/fsk2a_mod.c` ships the constant-`counter_top` emitter: one shared buffer, top 16 at a 1 MHz clock,
the tone carried by the duty pattern, HID and AWID rewired, `IS_1MHZ_PWM_TYPE` added so the clock is one edit.
✅ **Correct at every layer that can be checked**: RAM matches C384 to the byte (+9,984 B); the device reports
**1 MHz**, sequence present, **2335 entries** (which requires 31 one-bits, so both tones ARE in it); ctest is
green with the duty pin reporting **the same shape counts as the old encoding** (378 x4 + 165 x6, mark 4).
⛔ **And the air is still one tone**: HID **1183** periods in the RF/8 band against **16** in RF/10, 0/6 on the
Flipper with Gallagher 6/6 as the control in the same run. No regression anywhere.
⇒ **C383'S MECHANISM IS NARROWER THAN IT WAS WRITTEN.** It is not simply *a varying `counter_top` is not
applied* — a CONSTANT one whose tone lives in the duty is single-toned too. What both encodings share is that
the long tone needs a **longer LOW period**, and that is exactly what never appears.
⇒ **NEXT STEP NEEDS AN INSTRUMENT WE CONTROL.** The Flipper's raw reader is a black box we are inferring from;
`rdrcap.py` + `DATA_CMD_LF_READER_CAPTURE` hand back our own undecoded samples. That needs **the two
Chameleons facing each other** — the standing bench request in `NEXT.md`, which now has a concrete question.
⛔ Do not write a third encoding before that capture exists.

⚠ **THE NEW COST IS A CLOCK GENERALISATION**: `IS_PSK1_TYPE` gates the 1 MHz choice in **three** places —
`pwm_init()`, `pwm_reinit_if_clock_changed()` and `recompute_frames_per_burst()`'s `hz`. All three must move
to one *what clock does this type need* helper; **miss the third and frames-per-burst is 8x wrong.** ⚠ **Cost is entries**: ~25 per bit against today's 5-6, so a 96-bit HID frame needs **~2,400
entries (~19 KB)** where the array is sized **576**. PSK1's 3,584-entry path is precedent that the machinery
copes, but **the RAM must be budgeted on paper first**, and ioProx's long tone is **11**, so `gcd(8, 11) = 1`
and it needs `counter_top` 1 or a corrected tone.
✅ **THE BUDGET IS NOW ON PAPER (C384), TAKEN OFF `objects/application.map`, AND U11 IS UNBLOCKED TO WRITE.**
RAM region **218,392 B**, `.data + .bss` **126,644 B**, **91,748 B free**. The three FSK buffers are **4,608 B
each (13,824 B total)**.

| option | RAM | net | share of free |
|---|---|---|---|
| three private buffers (ioProx tone 11) | 72,192 B | **+58,368 B** | **64% — REFUSED** |
| **one shared buffer** (ioProx tone 11) | 33,792 B | +19,968 B | 21.8% |
| **one shared buffer** (ioProx tone 10) | 19,200 B | **+5,376 B** | **5.9%** |

⇒ **ONE SHARED FSK BUFFER IS THE DESIGN.** It is safe because `lf_tag_data_loadcb_inner()` sets exactly one
`m_tag_type` and one `m_pwm_seq`, so the three can never be live together. **F5 is the precedent** — it fixed
this exact shape, two 28 KB capture buffers resident at once.
⚠ **ioProx's long tone of 11 is a question worth measuring, not assuming** (the Proxmark uses fc/10, and
6 x 11 = 66 against a 64-cycle bit). The shared buffer works either way, so it does not block the emitter.
⇒ **NEXT: write the shared constant-`counter_top` FSK emitter**, HID first, with a `ctest/roundtrip.c` arm
that pins the DUTY PATTERN and not just the decode — C380 is the proof that a decode-only arm cannot see a
shape defect.

⛔ **WHY NO HOST TEST COULD EVER HAVE FOUND THIS**: `ctest` decodes the SEQUENCE ARRAY, where the two tones
differ correctly. Only capturing the EMISSION separates what we intend from what we transmit — and this is
the first time anything on this bench has done that.

⭐ **Eliminated on the way, each by measurement**: the loader (C379), playback (36 bursts vs Gallagher's 12),
the frame arithmetic, our own decoder, the mark shape (C380 — fixed, still 0/6), and the whole shared air
path including our reader (C381 — the Flipper's HID emulation reads back FC 123 / CN 4567 exactly).

✅ **U11 SCOPING (C379).** All three silent protocols LOAD a sequence — `have pwm seq : True`
at the correct 125kHz, frames/burst **14 / 16 / 14**, distinct so three real waveforms were walked. ⇒ Not the
loader and not the clock: the fault is FSK2a's **encoding**. The ASK emitters set `counter_top` to the carrier
cycles per BIT; FSK2a needs 8 or 10 per TONE PERIOD. Start with a `ctest/roundtrip.c` arm per protocol (C156).

⛔⛔ **U12 / C242 IS REFUTED.** *A held level does not transmit, so GProxII cannot be emulated this way at
all* is false — it emulates byte-exact, 6 of 6. ⇒ **The whole remaining emulate gap is one family, FSK2a,
which is U11** — exactly where C217 left it, AWID silent with a Gallagher control at 6/6.

#### The blocker was the Flipper's heap, and it is now self-healing (C377)

⛔ **My own 11:35 diagnosis — an API mismatch, rebuild `lfrfid.fap` — was WRONG and is retired.** `uptime`
killed it in one command: the Flipper had **not** rebooted since C353 read 3/3 on the same firmware
(10h54m, matching C355's 00:29:36 boot), so nothing static can explain a reader that worked and then did not.
⭐ **The cause is heap FRAGMENTATION.** `rfid` is `/ext/apps/RFID/lfrfid.fap`, **66,304 bytes**, needing ONE
contiguous block: largest block ran **118,304 at boot → 63,880 after six arms, with 107,464 still free**.
⚠ **`free` is the misleading number** — plenty free, no block big enough.
✅ **Fixed without hands and made automatic**: `./flipper.py heap` exits 4 below `.fap + 12000` headroom,
`./flipper.py reboot` power-cycles over the CLI and waits (~10s), and `./emugrade.sh` preflights before the
first arm and retries once behind a reboot if one dies mid-run. **Verified: a run that died at arm 1 now
completes all five.**

#### What this cost, and the rule it earns

⛔ **U18 was never a firmware defect (C373).** `hw emudebug` said `have pwm seq : True` all along;
`playbacks started : 0` was correct for a tag no reader had energised. `hw lfdebug` was the wrong instrument
and `hw emudebug` — which answers the question directly — was never asked.
⛔ **And the guard had to be written twice (C374, C376).** `flipper.py` scored a refused plugin load as an
ordinary miss; fixing it did not protect `emugrade.sh`, which swallowed the abort in `o=$(fread)` and printed
a clean `✓ null / psk - ask - / ✓ null`. Both layers now abort. ⭐ **A clean `./flipper.py read` is rig A's
only positive control** — a silent reader and a silent emulator produce identical numbers.

---

### ✅ 2026-09-14 08:05 — READ AND WRITE ARE FINISHED AND VERIFIED BOTH WAYS. EMULATE IS THE ONLY GAP.

⭐ **Rewritten now.** What this replaces was a 02:45 heading — *the write column is measured, ASK/biphase
is next* — with twelve bullets accreted under it, and ASK/biphase had been finished hours before. §1 is
what a fresh context inherits, so it states what is true now; `LOG.md` keeps the history (C286's rule).

**THE BENCH.** Four devices enumerate. **#2 runs a clean build that contains the last `firmware/` commit —
`./autopilot.sh status` now checks that every tick and says so (C358/M45).** #1 is on old firmware. The tag
holds **HID `H10301 FC 123 / CN 4567`**, confirmed by the Proxmark. `make check` is green on all four arms.

#### What is finished, and how strongly

| | |
|---|---|
| **write, 18 arms** | 4 of 4 each — **72 writes, 0 failures**, pm3 judging (C330, C331) |
| **read, 19 arms** | **76 of 76** against tags the **Proxmark's own encoder** wrote (C343). Nine of those cross the field/frame boundary too |
| **cross-protocol nulls** | **280 reads** on 14 real tags against 20 foreign readers, **0 false positives** (C342) |
| **blank-chip null** | **105 reads**, 21 readers, **0 false positives**, blank confirmed by pm3 each run (C344) |
| **reliability** | **250 reads, 0 failures**; HID **100/100** after the capture-engine rework (C345) |
| **the judge itself** | 19 of 20 pm3 readers **15/15**; only `lf fdxb reader` is weak at **87%**, and that is the READER not our tag — pm3 reads its own clone at the same rate (C346, C347, C348) |
| **fixes** | **13 registered — 12 fixed and ALL 12 re-verified** against the flashed build by `./fixcheck.sh` — **11 pass, 0 FAIL, 0 not checkable** (C393). F10 and F11 were the standing gap and were only ever blocked by the Flipper's plugin (C377); F10 is Gallagher → Indala with no reboot, both 4/4, crossing the clock boundary, and F11 is frames-per-burst **21 vs 31**, derived rather than constant. ⛔ **F12 is registered and NOT FIXED** — FSK2a emulation emits a constant tone (C387), characterised with every cheap cause eliminated, and it carries no regression arm because an unfixed defect has nothing to regress |
| **warnings** | the branch adds **none** to the firmware and removes two; its whole debt was 2 lines, now fixed (C356, C357) |

⇒ **The loop closes in both directions**, so a shared convention error between our reader and our writer
cannot pass either — each is judged by the other project's code.

#### The only real gap, and it is ready to run

⛔ **EMULATE.** It needs **#2 on the Flipper's pad with the T5577 out** — the Proxmark cannot hear
PWM-on-the-coil at all. `./emugrade.sh` does all eleven arms in one command with C328's discipline encoded
(null first and abort on an ambient hit, wrong-modulation arm as control, null again, scratch slot 8).
⚠ **Plumbing tested, no arm ever scored — the first run is the experiment, not a regression check.**

✅ **#1 IS REFLASHED — `v2.2.0-670-g90be1e2` (C363, C364).** The *NexWatch slot* was a device-MODE reminder I
repeated into a constraint; the operator confirmed neither unit has ever held a real credential. ⇒ **Emulate
needs NO bench change** — #1 already faces the Flipper. ⚠ `./emugrade.sh` targets #1 by default now, and it
**does not yet produce a result**: slot 1 EM410X reads as a positive control, Indala on slot 8 is 0 of 6 with
the cause unreached after three script bugs of mine were fixed. Not a firmware finding — an unfinished test.

#### Compaction is decided BEFORE the session starts

⛔ **Nothing can type `/compact` from inside a session** — cron fires and peer messages hardcode
`skipSlashCommands`, the control protocol has no compact verb, and `Pre`/`PostCompact` hooks only block (C371).
⛔ **And the one lever, `autoCompactWindow`, is read at process start.** Writing it under a running session does
nothing: two trials, file in place, three turn boundaries, no compaction — while a freshly forked CLI read the
new value off the same file (C372).
⇒ **Unattended:** `sh $UTIL/autocompact.sh 40 --project $REPO` **before launching the session**, and
auto-compaction then runs itself for the whole run.
⇒ **Operator present:** leave it off. At the threshold the tick commits, pushes and **asks for a manual
`/compact` in one line** — the one sanctioned exception to *never stop to ask*.
⚠ `./autopilot.sh status` prints the project's setting. That line is information, not a control.

#### Do not re-do these

⛔ **RETIRED — C299, C305, C306, C324** were all one password-locked tag (C325), and C329 confirmed it by
restoring reads, writes and detect together. ⛔ **C297's *the Proxmark's write path stopped working*** is
part of that same family and is false.
✅ **All three former *needs hands* items are closed or bounded**, none needed hands: the Indala slot held
**Gallagher's frame** (C354), the EM410X both-arms hit is **expected** because `rfid read indala` is a
front-end not a protocol filter (C353), and the Flipper crash is time-boxed to **00:29:36** with its cause
unrecoverable (C355).

⚠ **#2 carries gated instrumentation** (`LF_RESEARCH_CMDS_ENABLED`, which this branch's `Makefile` sets to
1): a FAILED `FDXB_SCAN` returns a 12-byte counter payload instead of an empty one.

---

⚠ **THE BLOCK BELOW IS HISTORY AND CONTAINS CLAIMS SINCE RETIRED** — notably *the Proxmark's write path
stopped working* (C297) and *#1 holds the NexWatch slot* (C354). Read it for how things got here, not for
what is true.

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
> ⭐⭐ **EVERY READ AND WRITE ARM IS DONE AND VERIFIED IN BOTH DIRECTIONS.** 18 write arms judged by
> the Proxmark (72 writes, 0 failures) and 19 read arms against tags the Proxmark's own encoder wrote
> (76 of 76). Nulls, soaks and the judge's own reliability are all measured — see §1.
> ⛔ **FDX-A IS NO LONGER AN EXCEPTION.** This box used to say FDX-A *cannot be verified on this bench*.
> It can: the Proxmark has a complete FDX-A under **`lf destron`**, not `lf fdx` (C333). It now reads
> **10/10** and writes **4/4** with blocks byte-identical to pm3's own clone (C338, C340).
> ⛔ **INSTAFOB REMAINS THE ONE REAL EXCEPTION** — the Proxmark has no InstaFob command at all, so a
> writer would certify itself. That refusal stands.
>
> ⭐ **THE ONLY PROTOCOL WORK LEFT IS EMULATE, AND IT NEEDS A BENCH CHANGE.** Six protocols read and
> write but do not emulate (AWID, Paradox, Pyramid, FDX-A, GProxII, FDX-B), and three more emulate at
> 0/6. All of it needs **#2 on the Flipper's pad with the tag out** — the Proxmark cannot hear
> PWM-on-the-coil. `./emugrade.sh` is written and plumbing-tested for exactly that moment.
> ⛔⛔ **U12'S CLOSURE IS RETRACTED — AND IT CLOSES THE OTHER WAY (C378).** *A held level does not
> transmit, so GProxII cannot be emulated this way at all* (C242) is **false**: GProxII emulates
> **6/6, byte-exact** as `FAC2A38C2B081AF0210B12C2`. Every biphase emitter here works. ⭐ **U11 (FSK2a)
> IS THE WHOLE REMAINING GAP** and it is now OBSERVED rather than assumed: HID Prox, ioProx and AWID
> are the only 0/6 arms, bracketed by 6/6 positives in the same run.
>
> ✅ **U1–U10, U13, U14, U15, U16, U17 are all closed.** U16 closed as *not reproducible* rather than
> solved, and its instrument is left in place (§1).
> ⇒ **IF THE BENCH CANNOT CHANGE, THERE IS NO DEVICE UNIT LEFT IN THIS CONFIGURATION.** Say so and take
> a compute unit rather than inventing one — the protocol work is finished, and the last several ticks
> found their value in auditing the instruments rather than the firmware.

⛔⛔ **READ THE STATUS COLUMN, NOT THE DESCRIPTION.** Rows U1-U10 describe work that is **long since finished** — they are kept for the reasoning, not as a to-do list, and the EXECUTION ORDER box above is authoritative. A fresh context that skims this table and starts on *NexWatch WRITE* is reading a history section as a queue. **The only rows that are open are the ones whose *done when* column does not say ✅.**

| # | unit | needs | done when |
|---|---|---|---|
| **U1** | **NexWatch WRITE.** `T5577_NEXWATCH_CONFIG 0x00081060`, `nexwatch_t55xx_writer` (3 data blocks, frame is block-aligned — no rotation, unlike Keri C158), `DATA_CMD_NEXWATCH_WRITE_TO_T55XX`, CLI `lf nexwatch write` | device | the Proxmark reads our Chameleon-written tag back as card 12345678 / Nexkey, 3 of 3 |
| **U2** | **NexWatch EMULATE.** `protocols/nexwatch.c` (PSK1 → `lf_psk1_modulator`, 96 bits, `LF_PSK1_PHASE_DIRECT`), `TAG_TYPE_NEXWATCH` (303), econfig get/set, `Makefile` row, **and a `ctest/roundtrip.c` arm** | device | Flipper or Proxmark reads our emulation as the right credential, ≥5 of 5, with a control either side |
| **U3** | **NexWatch on-device READ.** Flash, `lf nexwatch read` against the real tag | device | 6 of 6 on device + the cross-protocol nulls re-run on the shipping build |
| **U4** | **Re-test C162** — the PSK2 `lf t55xx dump` bit-31 artefact. n=1 today. Write the Indala224 credential, dump, compare; PSK1 control from the same writer | device | either a second confirming dump (n=2) or a retraction in FINDINGS.md |
| **U5** | **Gallagher** — opens family 2 (ASK/biphase). ⛔ **THIS ROW USED TO SAY Gallagher reuses the GPIO/comparator path rather than the SAADC one. THAT IS FALSE AND IT COST A DAY (C412/M53)** — `gallagher_read` lives in `lf_indala_data.c` and goes through `lf_drive_swept_read`, the SAADC whole-capture path, as do Securakey and Noralsy. The plan-era guess was believed instead of checked, and C400 built two controls on it that could not have detected the failure they were bracketing. Start as NexWatch started: `lf gallagher clone` on the Proxmark, capture, decode on the host before writing firmware | device | read + write + emulate, all verified, nulls clean |
| **U6** | **Securakey, then Noralsy, then InstaFob** — the rest of family 2, one at a time, only after U5 is completely done | device | same bar as U5, each |
| **U7** | ✅ **DONE 2026-09-14 — `NEXT.md` §9j.** Recounted against `main`: reviewable code 10,242 → **11,584** lines, 59 → **63** files, 29 → **31** new; the branch total's growth is notes and tooling, which no PR carries. ⛔ PR 4's FDX-A read-only caveat was FALSE and is corrected at both sites — it ships read AND write now (C340). ⭐ `FIXES.md`'s seven entries are named as the PRs that go FIRST, F1 leading, since six are defects on `main` and none depends on the protocol work. §9h's gating checklist gains the new scan counters | compute | ✅ met |
| **U8** | **FSK family** (AWID, Paradox, Pyramid, FDX-A). ⛔ **LAST, deliberately.** It reuses the HID Prox/ioProx SAADC machinery, and HID's 15–20% intermittency (C45) is unexplained and lives in exactly that path. Adding four protocols on top of an unexplained defect is what Phase 2 existed to prevent | device | do not start without saying so in §4 |
| **U9** | **GProxII** — opens family 4, **ASK BIPHASE**. ⭐ Picked before FDX-B deliberately: it is RF/64, NON-inverted, standard T5577 config, 96-bit frame, 6-bit preamble, where FDX-B is inverted AND extended-mode AND 128 bits. Proving a new line coding against three changed variables at once is the n=1 generalisation this branch keeps having to retract (C169/C171/C182, C189/C190) | device | read + write verified on hardware, cross-protocol nulls clean; emulate if the emitter is honest |
| **U10** | **FDX-B** — the rest of family 4, only after U9 is completely done. ASK biphase INVERTED, RF/32, 128 bits, preamble `00000000001`, T5577 config `903F0082` (extended mode). ⚠ Not to be confused with FDX-A, which is FSK2a and already reads | device | same bar as U9 |
| **U11** | **FSK2a EMITTERS** — AWID first, then Paradox, Pyramid, FDX-A, one at a time. ⭐ The shape is known: the ASK emitters put one PWM entry per bit with `counter_top` set to the carrier cycles that bit occupies, and FSK2a only needs `counter_top` 8 or 10 per tone period instead. ⚠ A `ctest/roundtrip.c` arm per protocol, which is where three wrong encodings were caught before (C156) | device (rig A only — no tag needed) | the Flipper reads our emulation as the right credential, ≥5 of 5, with a control either side |
| **U12** | **BIPHASE EMITTERS** — GProxII then FDX-B, only after U11 is completely done. ⚠ GProxII is BIPHASE and FDX-B is DIPHASE and INVERTED; they are one bit apart in the T5577 config and must not be assumed to share an emitter until one has been through end to end | device (rig A only) | same bar as U11 |
| **U13** | ✅ **DONE 2026-09-14 (C390).** §9h re-verified against the branch: the substance was right, both its NUMBERS were wrong. The gate now measures **1,560 bytes of flash and 8,016 of RAM** against the 1,088 / 4,008 it claimed — the RAM cost **exactly doubled**. Every line number in the site table had drifted and is refreshed, the table now says the line numbers are a convenience and the contract is the `#if` blocks plus one `-D`, and the gate's own site (`application/Makefile:431`) was missing and is added. ⚠ The row that stood here — *the instrumentation list is stale, the command-id count is no longer 32* — was ITSELF stale: §9h had already covered all four sites and `rdrcap.py` since C303 | compute | ✅ met |
| **U15** | ✅ **DONE 2026-09-14 (C330) — every write arm re-graded against an unlocked tag: all eight 4 of 4, 32 writes, 0 failures, every raw byte-identical.** The old failures were the password lock (C325), not the writers. `./regrade.sh <protocol> [rounds]` reruns any row. ⛔ Indala224 and IDTECK were NOT re-graded — only Indala26 | device (sandwich) | ✅ met |
| **U18** | ⛔⛔⛔ **RETRACTED — THERE WAS NEVER A FIRMWARE DEFECT HERE (C373).** `hw emudebug` reports **`have pwm seq : True`** and `frames per burst : 21` with Gallagher loaded: the loader ran and took its branch, so *the emulator never sets `m_pwm_seq`* is false and `lf_tag_data_loadcb_inner()` needs no instrumentation. What is 0 is **`playbacks started`**, and playback starts only from `lpcomp_event_handler(UP)`, which needs a reader's field. ⛔ **The Flipper's `rfid` command was not running** — `failed to load external command`, an API mismatch between `lfrfid.fap` and the firmware on it — so every arm was scored against a reader that was never listening, and `flipper.py` reported that as a clean `0/N` (C374, now fixed to abort). ⭐ Indala PSK1, the arm §1 recorded as 6/6, scores 0/4 in the same session: not protocol-specific, and never was. ⇒ **The emulate column is UNMEASURED, not failing.** It reopens as U19 the moment the Flipper reads again | — | ✅ closed as retracted |
| **U19** | ✅ **DONE 2026-09-14 (C378) — the emulate column is measured: 8 of 11 at 6/6.** PSK1 (Indala, IDTECK, Keri, NexWatch) and ASK/biphase (Gallagher, Securakey, Noralsy, GProxII) all 6/6 with wrong-modulation controls at 0/6 and clean nulls, and **every one of the eight has had its CREDENTIAL read back and checked** (C398) — three byte-identical, three field-exact, Securakey settled by the Proxmark as a third party. ⚠ This row used to say *credentials verified byte-exact* when only TWO had been. FSK (HID Prox, ioProx, AWID) 0/6, bracketed by positives so the silence is real. ⛔ **U12/C242 refuted** — GProxII emulates exactly. ⇒ The remaining gap is one family and it is **U11** | device (rig A) | ✅ met |
| **U16** | ⛔ **CLOSED AS NOT REPRODUCIBLE (C337), NOT AS SOLVED.** FDX-B reads **15 of 15** — 6/6 CLI, 3/3 raw, 9/9 across an A/B/A — so the `0 of 4` that opened this unit is not a standing defect. ⚠ **It was real once**: the gated scan counters caught it at 17 captures kept, 8 phases, ZERO decodes, so the failure mode is a silent DECODER, not a failed agreement. Trigger unknown; drive inheritance refuted. ⇒ **If it returns**, the instrument is already in place — call `FDXB_SCAN` raw and read the 12-byte failure payload (attempts / kept / decodes / capture_failed / spliced / phases / last_phase). Do not theorise before re-running the baseline | device | ✅ closed; reopen only on a reproduction |
| **U17** | ✅ **DONE 2026-09-14 — FDX-A reads AND writes, both grade A (C338, C340).** Read 10 of 10 on a real Proxmark-written tag across two credentials; write 4 of 4 from a confirmed-blank tag with the credential alternating every round, and the stored blocks are **byte-identical to `lf destron clone`'s own**. Config `00105060` — FSK2, not the family's FSK2a. ⇒ C185's *refused, not deferred* is fully retired: it rested on looking for FDX-A under `lf fdx` instead of `lf destron` | device (sandwich) | ✅ met |
| **U14** | ✅ **DONE 2026-09-14 — `NEXT.md` §11 is complete.** The three questions the first pass left open are answered against the source: the Flipper's `validate_count` is the bar AFTER the first sighting, so the real requirement is 4 non-PSK / 7 PSK against our 2; the Proxmark takes ONE capture and stops at the first of 26 demodulators to match, so by default a shadowed protocol is never reported; and our own rule compared 8 bytes rather than the frame until C332 fixed it | compute | ✅ met |

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
| 2026-09-14 22:30 | **C368 — failure survives a reboot; three reload workarounds ruled out; loader branch guards identified** | 74 → 75 | Not a reload problem. Next step named: instrument inside `lf_tag_data_loadcb_inner`, not more black-box probing | 1 reboot, 3 reload strategies, 2 code paths read |
| 2026-09-14 21:55 | **C367 — the emulator's PWM is not playing our sequence; C366's boundary superseded** | 73 → 74 | `hw lfdebug` killed both my hypotheses in one call: not ASK-specific (EM410X fails too), not the clock (125kHz is correct). A state, not a dead protocol | 3 lfdebug dumps, 1 clean reboot, 2 control protocols |
| 2026-09-14 21:20 | **C366 — ASK/Manchester emulate arms emit nothing on current firmware; PSK1 6/6 and EM410X 3/3 as controls** | 71 → 73 | Candidate regression, not bisected; queued as U18. The emulate re-grade found exactly what it existed to find | 4 arms x 6 reads, 2 control families, device state read back |
| 2026-09-14 20:20 | **C364 — #1 reflashed, emulate unblocked with no bench change; battery still not producing a result** | 70 → 71 | Positive control passes (EM410X slot 1); Indala slot 8 is 0/6 after three of my own script bugs. `enter_dfu` needs a settle before nrfutil | 1 flash, 1 control, 3 script bugs |
| 2026-09-14 19:50 | **C363 — traced the NexWatch-slot claim: it was a device-MODE reminder I repeated into a constraint** | 69 → 70 | Origin `5cda84ab` says *put it back to `hw mode -r`*; I compressed it into the loop prompt and restated it ~20 times. Operator confirms neither unit ever held a real credential | 3 commits traced, 16 changed files counted |
| 2026-09-14 19:25 | **C362 — F9 is not standalone (needs PR 1's drive API); `FIXES.md` claimed all eleven were** | 68 → 69 | Third extraction, first failure. A wrong PREMISE rather than a wrong description — undetectable by reading. Header rewritten; 8 entries still untested | 3 extractions, 2 built, 1 failed at the first symbol |
| 2026-09-14 19:00 | **C361 — PR 1 (F8) built on `main`, 6 files +51; its file list was wrong two ways** | 67 → 68 | Omitted `ble_main.h`, listed `lf_reader_generic.c` it does not need — so F8 does NOT depend on F5. First inter-PR ordering constraint found (`lf_pac_data.c`, shared with F9) | 2 builds, one deliberate failure that pinpointed the missing extern |
| 2026-09-14 18:35 | **C360 — PR 0 written and build-tested (3 files, +19 −12 on `main`), and it corrected F1's root cause** | 66 → 67 | No PR text existed. The implementing commit does not apply to `main`; half of F1's stated cause turns out to be ours, and upstream's half is worse than described | 1 extraction, 1 build, 1 apply-check |
| 2026-09-14 18:10 | **C359 — §1 and §2 rewritten; both had drifted into saying the opposite of what is true** | 65 → 66 | The sections a fresh context inherits still announced ASK/biphase as next and FDX-A as unverifiable. Historical block fenced with its retired claims named | every heading checked against what C330-C358 established |
| 2026-09-14 17:25 | **C358 — device re-synced to source; `status` now reports firmware drift every tick** | 64 → 65 | C357's edit to `write_t55xx()` was built and never flashed; #2 sat several commits behind while every check reported clean. Verified 9/9 + 30 writes on a clean HEAD build; M45 added | the device's own `get_git_version()` against HEAD, three-way verdict, sensitivity tested |
| 2026-09-14 16:55 | **C357 — the Nordic-only files measured; the branch's whole warning debt was 2 lines, fixed** | 63 → 64 | Closes C356's stated gap. None of the new LF files warn at all; `lf_reader_main.c` 3 → 1, the survivor upstream's | full firmware build at `-Wconversion`, `git blame` per hit to separate ours from inherited |
| 2026-09-14 16:25 | **C356 — warning audit: 13 of 14 strict-built firmware files clean, all ours; upstream's `wiegand.c` improved 37 → 35** | 62 → 63 | The branch adds no warnings and removes two — a reviewer sees warnings before logic | `main` compiled the same way as the control; a zsh word-splitting bug nearly produced a false all-clear |
| 2026-09-14 15:55 | **C355 — the Flipper crash time-boxed to 00:29:36; cause unrecoverable** | 61 → 62 | `uptime` answers what the notes called unknown. All three *needs hands* items now closed or bounded, none needed hands | read-only: 1 uptime, 2 storage listings; a 00:29 commit co-timing recorded as NOT a cause |
| 2026-09-14 15:25 | **C354 — the 'bad Indala frame' is Gallagher's frame in the wrong slot; and #1 has no NexWatch slot** | 60 → 61 | Open item closed and repaired without hands. ⛔ The stated reason for never reflashing #1 is not supported by its own slot listing — reported, not acted on | 1 econfig read + repair on #2; read-only listing on #1 |
| 2026-09-14 14:55 | **C353 — the EM410X 'both arms' anomaly explained and retired; NEXT.md's stale C305 banner closed** | 59 → 60 | `rfid read indala` is a front-end, not a protocol filter — an ASK credential hitting both arms is expected. Verdict logic fixed in `flipper.py` and `emugrade.sh` | 3 reads per arm on a live EM4100 emulation, one invocation |
| 2026-09-14 14:25 | **C352 — `emugrade.sh`: the emulate column reduced to one command, plumbing tested, results not taken** | 58 → 59 | The only real gap left needs a bench change an unattended run may not make. Two of eleven entries were wrong because econfig signatures are not write signatures | 11 arms executed against #2, none scored; #2 returned to reader mode |
| 2026-09-14 13:55 | **C351 — the third audit axis: 9 of 11 fixes re-verified still-true on the live build, 0 regressed** | 57 → 58 | Completeness cannot detect a regression and a regression check cannot detect an omission. `fixcheck.sh` added and wired into FIXES.md's header | F10/F11 reported NOT CHECKED, not skipped |
| 2026-09-14 13:25 | **C350 — auditing by CLAIM found four more unregistered upstream defects; FIXES.md 7 → 11** | 56 → 57 | The file audit is blind to a fix inside an assigned file, and F10/F11 were exactly that. Neither axis would have found the other's | 33 fix-shaped claims triaged; blindness demonstrated by checking F10/F11's files ARE named in §9d |
| 2026-09-14 12:55 | **C349 — PR-split completeness audit: 10 of 63 files owned by no PR, two are unregistered UPSTREAM fixes** | 55 → 56 | F8 (the BLE guard behind C45's HID intermittency) and F9 (the PAC drive sweep) added to `FIXES.md`; the other eight assigned in §9d | files enumerated from git rather than from the notes |
| 2026-09-14 12:25 | **C348 — the control: pm3 reads its OWN FDX-B clone at the same rate as ours (35/40 vs 32/40, p = 0.55)** | 54 → 55 | Separates a weak writer from a weak reader by varying who WROTE the tag; retires any suspicion of our FDX-B writer | 40 asks of pm3's own clone against 110 of ours; block-dump difference explained as the animal bit |
| 2026-09-14 11:55 | **C347 — the other 19 judges are clean (285 asks, 0 misses); FDX-B's is uniquely weak at 86%** | 53 → 54 | Validates the write column's single-ask verdicts for 19 of 20 protocols. `judgerel.sh` added | write once then only ask, so a miss cannot be the write; per-judge limit stated, pooled bound 1.05% |
| 2026-09-14 11:20 | **C346 — the judge blinks: pm3 misses 12% of FDX-B asks; every single-ask verdict inherited it** | 52 → 53 | A write soak's lone failure in 150 cycles was the READER, not the writer. `regrade.sh` and `nullmatrix.sh` fixed in opposite directions; M44 added | 40-round probe with a second reader + 4 protocols x 20 pm3 asks |
| 2026-09-14 10:45 | **C345 — read reliability re-measured after F5/F7: 250 reads, 0 failures, HID 100/100** | 51 → 52 | C250's 96/96 predated the capture-engine rework and had never been re-taken. `readsoak.sh` added | tag written once per protocol, then only read, so a miss isolates the read path |
| 2026-09-14 10:15 | **C344 — the blank-chip null, live for the first time: 105 reads, 21 readers, 0 false positives** | 50 → 51 | Every previous empty null was a recorded capture; a wiped T5577 still modulates the field. `nullmatrix.sh blank` added | pm3 wipes and confirms blank before each of five runs |
| 2026-09-14 09:50 | **C343 extended — the six UPSTREAM protocols too; 76 of 76 across nineteen** | 50 → 50 | HID Prox, ioProx, EM410x, Viking, Jablotron and PAC verified against pm3-written tags: a regression check on the capture engine this branch rewired under them | 6 protocols x 4 reads, expectations observed not guessed |
| 2026-09-14 09:20 | **C343 — read arms verified against PROXMARK-WRITTEN tags, 52 of 52; the loop closes both ways** | 49 → 50 | Closes the self-certification hole my own instruments had: every other test writes the tag with OUR writer. `pm3written.sh` added | 13 protocols x 4 reads; 8 of 13 driven by fields, not a raw frame |
| 2026-09-14 08:50 | **C342 extended — the null matrix was asking only half the readers; 182 → 280 reads, still 0 false positives** | 49 → 49 | One list served as both tags and readers, so the GPIO/comparator, HID and ioProx readers were never asked. Lists separated | 14 tags x 20 foreign readers; bound now 1.07% |
| 2026-09-14 08:15 | **C342 — cross-protocol nulls against REAL TAGS, 182 reads, 0 false positives** | 48 → 49 | A test that needed every write arm working, so it was never runnable until tonight; FDX-A had never had one at all. `nullmatrix.sh` added | 14 written tags x 13 foreign readers, self-read as the per-row positive control |
| 2026-09-14 07:45 | **C341 — read cost measured across the sampled path; C335's field-hold lead closes negative** | 47 → 48 | Median 2 captures (the floor), 65% at it, identical aggregate over two runs; per-arm ordering shown to be noise at n=4. `capcost.sh` added | 2 runs x 14 protocols x 4 reads, plaintext written first, bench restored |
| 2026-09-14 07:15 | **U7 closed — §9 upstreaming assessment refreshed (§9j)** | 47 → 47 | Recount, PR 4's false FDX-A caveat corrected at both sites, the seven fix-PRs ordered ahead of the protocol PRs; `checkdocs.sh` taught that F-numbers live in FIXES.md | Counts from `git diff main...HEAD`; the new check proven by injecting a bogus id and watching it fail |
| 2026-09-14 06:45 | **C340 — U17 closed: the FDX-A writer ships, blocks identical to the reference clone** | 46 → 47 | A refused arm became a verified one; `roundtrip.c` pins its vectors and the complement relationship | Wiped tag, alternating credentials, Proxmark as judge, dump equals pm3's own clone |
| 2026-09-14 06:10 | **C338/C339 — FDX-A read upgraded to grade A; write constants measured** | 44 → 46 | The refusal C333 reopened is half retired: read verified on a real tag 10/10, writer fully specified off a reference clone and not yet built | pm3 `lf destron clone` as the independent writer, its own detect/dump as the source of the config word |
| 2026-09-14 05:40 | **C337 — FDX-B reads 15 of 15; the failure that drove three ticks does not reproduce** | 43 → 44 | Grid downgrade withdrawn, C215 restored, U16 closed as not-reproducible. Gated scan counters added and kept — they caught the failure mid-flight as ZERO decodes, not zero agreement | 15 device reads across two paths plus an A/B/A over the live hypothesis |
| 2026-09-14 05:05 | **C336 — retracting C335's mechanism; phase ruled out** | 42 → 43 | The *hundredfold* edge gap was `cut` slicing a number; real ranges overlap. Field-hold result survives. FDX-B decodes at all 8 rotation phases with the field held | Same files re-decoded in full, so the correction is not a new sample |
| 2026-09-14 04:45 | **C335 — FDX-B needs a held field, 16/16 vs 5/16, and that is still not the whole cause** | 41 → 42 | U16 half answered: the field effect is large and measured; the try-budget arithmetic says it cannot account for 0 of 4 on its own | 32 captures, 16 per arm, shipping decoder as judge, p = 6.8e-5 |
| 2026-09-14 04:20 | **U14 closed — the reference scan/repeat audit, and it found a defect in ours** | 38 → 41 | §11 completed; the audit produced C332 (F7), C333 (pm3 does have FDX-A) and C334 (my own retraction) | Source-traced to file and line in five trees; the F7 claim about FDX-B refuted by its own A/B |
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

### ⛔ 2026-09-14 17:35 — F12'S NEXT STEP NEEDS AMPLITUDE, WHICH MEANS THE TWO CHAMELEONS FACING (C416)

⛔ **Scope: this blocks F12's last question and nothing else.** Everything else — read, write, the other eight
emulate arms, C400 — is done or reachable on the bench as it stands.

⭐ **Why the current bench cannot answer it.** The Flipper's raw reader returns pulse/duration EDGES. *The long
tone is absent* and *the long tone is there but too shallow to cross the threshold* produce the identical edge
stream, and F12's box (C411/C413/C414/C415/C416) is consistent with both. Only **amplitude samples** separate
them, and the only instrument here that returns amplitude is **our own SAADC capture path**.

⇒ **THE ASK, IF AND WHEN IT IS WANTED**: #1 and #2 facing each other, nothing else on either pad. #1 emulates
AWID, #2 captures raw through `rdrcap.py`, and the samples go to the PROVEN demodulators — `askdemod.py` and
ctest's `cdemod` — never `tonehist.py` (C401). Then straight back to the two-rig bench.

⚠ **NOT A REPEAT OF C408'S MISTAKE, and the difference is the point.** C408 said facing them served no open
unit, and that was correct: it was wanted then for a **decode**, which the phase-locked SAADC path cannot do
against an emulation (M52). **Raw sample capture is legal on that path** — M52 forbids reading a CREDENTIAL from
an emulation, not sampling the coil. ⭐ And C401 invalidated the ANALYSIS of those captures, not the captures:
*"The captures are sound; the histograms are not evidence."*

⚠ Cost: one bench change and back. ⭐ **Nothing else waits on it**, so it is worth doing only when convenient.

### ✅ CLEARED 2026-09-14 16:55 — THE TAG MOVE WAS DONE AND IT CONVICTED THE EMITTER (C413)

✅ **The operator made the swap within minutes of it being asked for, and it settled the question**: the real tag
reads **44.4%** RF/10 on the ASK chain where our emulation reads **7.0%**. The instrument is calibrated, the
emitter is convicted, and nothing here needs hands again. Original request follows.

### (answered) 2026-09-14 16:35 — ONE TAG MOVE WOULD CALIBRATE BOTH RECEIVERS (C411)

⛔ **Scope: this blocks the last step of U11/F12 and nothing else.** The cliff is measured and the emitter is
implicated; what cannot be measured on this bench is **how much of the shortfall is ours and how much is the
receiver's**, because both chains are envelope detectors with a duty-dependent bias and they disagree 2-3x.

⇒ **THE ASK: write HID Prox to the T5577 with the Proxmark (rig B, no move needed for that), then put THAT TAG
on the FLIPPER's pad** and leave rig A otherwise as it is. A real FSK2a tag is a genuine mixed-tone source, so
reading it through both chains scores the INSTRUMENTS instead of letting the instruments score us. If a real
tag also reads ~2% RF/10 on the ASK chain, the chains are simply blind to mixed FSK2a and our emitter may be
fine; if it reads near 45%, the emitter is confirmed at fault and the cliff is ours.

⚠ **This is NOT C377's mistaken request** — it is backed by a 5-point rate sweep, a 2-chain disagreement and
pure-tone controls at both ends, and it names exactly what each outcome would settle. ⭐ Nothing else is
blocked: C400 is fully reachable on rig B as it stands.

### ✅ CLEARED 2026-09-14 16:10 — THE BENCH IS COUPLED. THE BLOCKER IS NOW MY ANALYSIS, NOT THE HARDWARE

⭐ **The operator moved the two Chameleons to face each other, and it works**: #2 decodes #1's EM410X
emulation (`EM410X/64: deadbeef88`), so `./fskcap.sh` takes real captures and no bench change is outstanding.
⛔⛔ **But the measurement is still not taken, because `tonehist.py` measures noise (C401).** It prints
`ZERO long tones — this is U11's signature` on that same known-good EM410X emission. The captures are sound;
the histograms are not evidence, and today's `fskcap.sh` numbers are void.
⇒ **NEXT: rebuild the tone analysis on the project's PROVEN demodulators** — `askdemod.py` and ctest's
`cdemod`, which are validated against real captures — then re-run `./fskcap.sh`. **No hands needed.**

### (superseded) 2026-09-14 13:35 — U11 ONLY: OUR OWN CAPTURE INSTRUMENT CANNOT HEAR OUR OWN EMITTER (C387, C388)

⛔ **Scope: this blocks U11 and nothing else.** Read, write and 8 of 11 emulate arms are finished and
verified; the bench is otherwise healthy and every other unit can proceed.

**What is known, and it is a lot.** The FSK emitter produces EITHER tone perfectly on its own — a steady
RF/10 comes out at 79 us exactly as designed — and loses the long tone in proportion to how often the tone
CHANGES: 3031 long periods when it never alternates, 261 when it alternates every 4 bits, **zero** when it
alternates every bit. Mark quality tracks the same curve. Encoding, loader, playback, clock, frame
arithmetic, our decoder, mark shape and modulation depth are each eliminated by measurement.

⭐ **It is not an instrument artefact and the control was taken**: a genuine mixed-tone reference — the
Flipper emulating `H10301` — captured through OUR OWN reader shows both bands, 389 near RF/8 and 79 near
RF/10.

⇒ **THE HANDS STEP.** Everything measured so far came through the Flipper's raw reader, a black box we are
inferring from. `rdrcap.py` + `DATA_CMD_LF_READER_CAPTURE` return our own undecoded samples at a rate we set,
and answering *what does our coil actually do when the tone changes* needs one Chameleon emulating while the
other captures. **Either the two Chameleons face each other, or a scope goes on LF_OA.**

✅ **THE MEASUREMENT IS ALREADY WRITTEN — `./fskcap.sh` (C391).** The moment the two Chameleons face
each other, that one command does everything: it checks coupling with Gallagher (a 6/6 arm, so a null
means NOT COUPLED rather than *emitter broken*), aborts in bench terms if they are not, and otherwise
captures each FSK arm through `rdrcap.py` and histograms it with `./tonehist.py`. ⭐ The analyzer is
already validated against a known-good reference — the Flipper's mixed-tone `H10301` through our own
reader gives 389 periods near RF/8 and 94 near RF/10, both bands present — and its units need no
calibration, because the SAADC samples once per carrier cycle so an RF/8 tone IS 8 samples.
⚠ **Verified uncoupled on 2026-09-14**: `./fskcap.sh` aborts on the coupling check today, as it should.

⚠ **This is NOT C377's mistaken request.** That one asked for a `.fap` rebuild on a diagnosis `uptime`
refuted in one command. This one is backed by a three-point rate curve, a positive control, and eight
eliminated hypotheses — and it names exactly which measurement is missing.


### ✅ CLEARED 2026-09-14 12:15 — RIG A NEVER NEEDED HANDS (C377)

**The entry that stood here said the Flipper's `rfid` plugin would not load and asked the operator to rebuild
`lfrfid.fap` against the running firmware. That diagnosis was wrong.** `uptime` refuted it in one command:
the device had not rebooted since C353 read 3/3 on the same firmware, so no API mismatch could explain it.

⭐ **The cause is heap fragmentation** — a 66,304-byte `.fap` needing one contiguous block, with the largest
block decaying 118,304 → 63,880 across a session while 107,464 stayed free. **A `power reboot` over the
Flipper's own CLI clears it in ~10 seconds.**

✅ **Now automatic and needs nobody**: `./flipper.py heap` / `./flipper.py reboot`, with `./emugrade.sh`
preflighting and retrying once behind a reboot. ⇒ **Nothing on rig A is blocked.**

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
