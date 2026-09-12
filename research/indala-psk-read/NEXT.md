# Next — ranked

**GOAL: support as many LF encodings as the Flipper Zero does, in read, write AND emulate.**

⛔ **The rule that produced everything below, and it still binds:** fix what exists before
adding what does not. Breadth on a broken foundation multiplies the debt, and this project has
spent days on measurements that turned out to describe a broken instrument rather than the
thing under test. ⇒ Phases 1 and 2 are closed on that basis; new protocols start now, and
**by modulation family** so each one lands on a path that something has already proven.

⛔ Method rules live in `METHOD.md`. Evidence lives in `FINDINGS.md` (what is believed now)
and `LOG.md` (what was believed when). This file is a PLAN — finished sections collapse to
one line. The bench layout, the device ports and the git conventions are in `README.md`
under **The bench** and **Working conventions**.

---

## ⚠ Needs hands — what is still queued

**Cleared 2026-09-12:** both Chameleons power-cycled (rig A emulates again, 3/3); stacking
approved for removal; the T5577 may be rewritten to whatever a test needs.

| | why a person is required |
|---|---|
| ⚠ **The bench tag is EM410x `DEADBEEF88`** — `drivesoak.py` cycles the tag through PAC, HID, Indala and EM410x and leaves it on whichever its last round wrote. It has worn three credentials in one day: PAC (as recorded), then HID Prox (as found), then Indala for C138's control, now PAC again | ⛔ Not deliberate — it is wherever the soak left it. §2's PAC specimen is one unattended command away (`lf pac clone --cn CD4F5552`), and so is C138's Indala reference (`lf indala clone -r a0000000e6bd0e92`). ⚠ Check what is actually on the tag before running anything against it: this row has been stale twice and both times it sent experiments at the wrong specimen. ⚠ It is no longer the carrier-locked Indala reference C138 used — restoring that is one unattended command, `lf indala clone -r a0000000e6bd0e92`, and the §4/§5 work is finished with it for now. ⛔ Its previous contents are dumped to `~/lf-t55xx-1D555955-5569A9A5-55A59569-D5B2649F-B3C6AD1F-CF649393-928C14E5-dump.json` and restore with `lf t55xx restore -f <that file>`. ⚠ §2's PAC specimen is no longer on this tag — but `pactest/` reproduces that failure on the host from a committed capture, so the physical tag is not the only specimen. ⛔ Restore `0x00081040 / 0x4944544B / 0x55667788` before relying on C90-C92's regressions again |
| ◐ **Lift the T5577 out of the sandwich** — to make the clock conclusion CAUSAL | ⚠ Not urgent, and not blocking: the conclusion is recorded as *likely closed* and everything downstream of it is written that way. But the one experiment that would turn correlation into a law — detune our own subcarrier and predict the ceiling (C139, `ADVERSARIAL.md` brief 2, question 1) — needs the Proxmark seeing the emulator alone, and the tag now sits between them. **One lift, then hands off**; several builds are measured at that one geometry |
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
| HID Prox (H10301, generic, ex-generic) | ⚠ **intermittent, 15-20% (C45)** | ✓ | ✓ | ✓ |
| ioProx (IOProxXSF) | ✓ | ✓ | ✓ | ✓ |
| PAC/Stanley | ✓ **fixed (C144)** | ✓ | ✓ | ✓ |
| Viking | ✓ | ✓ | ✓ | ✓ |
| Jablotron | ✓ | ✓ | ✓ | ✓ |
| **Indala 64-bit** | ✓ | ✓ | ✓ | ✓ |
| **Indala 224-bit** | ✓ | ⛔ | ⛔ | ✓ |
| **IDTECK** | ✓ | ✓ | ✓ | ✓ |
| EM4x05 | ✓ | ✗ | ✗ | — (not an lfrfid protocol) |
| AWID | ✗ | ✗ | ✗ | ✓ |
| FDX-A | ✗ | ✗ | ✗ | ✓ |
| FDX-B | ✗ | ✗ | ✗ | ✓ |
| Paradox | ✗ | ✗ | ✗ | ✓ |
| Pyramid | ✗ | ✗ | ✗ | ✓ |
| Keri | ✗ | ✗ | ✗ | ✓ |
| Gallagher | ✗ | ✗ | ✗ | ✓ |
| NexWatch | ✗ | ✗ | ✗ | ✓ |
| Securakey | ✗ | ✗ | ✗ | ✓ |
| GProxII | ✗ | ✗ | ✗ | ✓ |
| Noralsy | ✗ | ✗ | ✗ | ✓ |
| InstaFob | ✗ | ✗ | ✗ | ✓ (ASK, RF/32) |

⇒ **Twelve protocols absent, two readers unreliable. Every Indala and IDTECK READ path now works.**

⛔ **Indala is NOT finished.** Momentum implements **Indala224** as well as Indala26, and our
decoder is hard-wired to 64 bits (`INDALA_PSK_FRAME_BITS 64`). That belongs in Phase 1, and it
carries a RAM cost that collides with §8:

> A 224-bit frame at RF/32 is **7168 samples**. The two-frame rule that guarantees one whole
> frame lands inside the window (`lf_indala_psk.h`) would need **14336 samples = 28 KB** at
> 16-bit, against the 8 KB the 64-bit path uses. On a part where the reader already holds
> 48 KB, that is not a free change — and 32 KB of the current footprint is stacking that buys
> nothing at the correct placement (§8). ⇒ Decide §8 before building Indala224, not after.

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
| **A. Close the Indala family** — §1d write + emulate for 224-bit | the only in-family gap left; `--224` exists on `read` and nowhere else | **nothing** — rig B writes and verifies in both directions |
| **B. New protocols, by modulation family** — §10 | PSK1 first (Keri, NexWatch), then ASK/biphase, then FSK, then long-frame biphase | **nothing for the read side** — the Proxmark writes the tag. ⚠ write and emulate need a non-ours reader, which both rigs already provide |

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

## 1d. ◐ Indala 224-bit — reads 6/6; WRITE and EMULATE are the gap
✅ **Read works**: `lf indala read --224`, 6 of 6 with two nulls, PSK2-only (C107, L98).

⛔ **Nothing else exists.** Verified against the command table, not memory: `--224` appears on
`lf indala read` and **nowhere else** — no `lf indala write --224`, no `econfig`, and no
`TAG_TYPE_INDALA224` registered in `tag_base_type.h`. It is the only in-family gap left.

| | what it needs |
|---|---|
| **write** | the T5577 path already writes 64-bit Indala; 224 is 7 blocks instead of 2 and a different config word. ⚠ Confirm the block count and config against what `lf indala read --224` already decodes, and against a Proxmark-written tag as the independent check |
| **emulate** | register the tag type, then the PSK1 transmit path takes it — ⚠ a 224-bit frame is **57.3 ms**, so `recompute_frames_per_burst()` already handles it (C133) and the 500 ms budget gives 8 frames |
| **verify** | rig B in both directions: the Proxmark writes a 224 tag and we read it; we write one and the Proxmark reads it. Emulation goes to rig A |

⭐ **This is the cheapest remaining Phase 1 item and it unblocks nothing else** — do it first
because it closes the family, not because anything waits on it.

## 5. ⚠ Carrier locking — a decision, and it is now a cheap one

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

## 9. Upstreamable?

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


Read and write are solid and independently verified. Emulation works against two readers.
The pieces a PR would need: the tag-type registration (done here), emulation (done here), and
a decision on whether `lf indala read`'s specialisation is acceptable upstream — §1's status
message is what makes it honest.

⚠ `idteck.c` ships a PSK1 emulation nobody has verified end to end (§6). Worth reporting
upstream independently of anything here.

## 10. ⭐ The twelve missing protocols — grouped by modulation, cheapest family first
⭐ **Group by modulation, not by name.** Each family shares a capture path, a decoder shape and
a transmit path that already exist and are proven; doing one protocol from a family makes the
rest of it nearly free, and doing one from each family makes all of them expensive.

| family | protocols | the path it reuses | why this order |
|---|---|---|---|
| **1. PSK1** | **Keri**, **NexWatch** | `lf_indala_psk.c` + `lf_indala_data.c`, parameterised by `lf_psk1_format_t` — the same descriptor that already carries Indala64, Indala224 and IDTECK | ⭐ cheapest by a wide margin. The decoder is generic already, `tag_base_type.h` reserves both in the 300 block, and the PSK1 transmit path is proven on three protocols |
| **2. ASK / biphase** | **Gallagher**, **Securakey**, **Noralsy**, **InstaFob** | the GPIO/comparator path — `register_rio_callback`, 128-entry ring, no SAADC — that em410x, Viking and Jablotron use | ⚠ the path that is **not** affected by the saturation the SAADC readers suffer (C140), so it starts from a healthy instrument |
| **3. FSK** | **AWID**, **Paradox**, **Pyramid**, **FDX-A** | the HID Prox / ioProx SAADC machinery | ⛔ last of the three: HID's intermittency (C45) is unexplained and lives in exactly this path. Adding four protocols on top of an unexplained defect is what Phase 2 existed to prevent |
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
