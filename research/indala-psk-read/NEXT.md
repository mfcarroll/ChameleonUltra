# Next — ranked

⛔⛔ **EVERY MEASUREMENT IN THIS PROJECT WAS TAKEN WITH THE TAG ON THE WRONG SIDE OF THE
DEVICE.** The reading side is the FRONT. It is worth **21x** (~26 dB). See the banner at the
top of `FINDINGS.md`. The plan below is reorganised around that: re-measure first, and treat
every ⚠B claim as provisional until it is redone.

**State:** `lf indala read` works, and on the front it works far better than any number in
these notes suggests — 71% of single captures decode, against 32% on the back, and the
"phase window" turns out not to be a window.

⛔ Method rules live in `METHOD.md`, not here. Read them before adding a claim to the ledger.

---

## 1. ✅ RE-MEASURED ON THE FRONT — 1a, 1b and 1c are done

The full sweep ran (`caps/front/`, 320 captures, committed). **114 of 160 single captures
decode the truth (71%) and 0 of 160 empty captures produce a frame at all.** Phase is TWO
working bands — 0–56 and 96–124 — split by a dead band at 60–92. C36, C37, C38–C43, L58.

⛔ 1b and 1c needed no bench time: the sweep saves every capture, so both were settled
offline against the committed set. Do that first next time.

**What it settled:**

| | result |
|---|---|
| **1a** phase map | two bands, not a window. Every good phase is 5/5; the edges are cliffs, not gradients |
| **1a** the skirt | **⛔ does not predict decode at all** (C38) — peak skirt and minimum skirt both decode 5/5, the dead band sits between them. This retires §7 rather than porting it |
| **1b** C03 fs/2 notch | **reversed sign** (C41). Notch ON 114 truth / 21 wrong, OFF 121 / 26. No longer load-bearing either way — left ON, since it trades ~7 decodes for ~5 fewer wrong frames |
| **1b** C04 settle discard | **still fatal, 107 -> 0** (C42), and always was structural rather than SNR |
| **1c** agreement rule | **⛔ keep it — it is WEAKER than believed, not stronger** (C39, C40) |

## 1c-follow-up. ✅ THE STRADDLE GATE — closed

On the front the decoder now returns **110 frames, 110 of them correct**. C48, C49, L61.

Reject when a frame is BOTH loud and ragged: `mean|integ| >= 2048` AND `min|integ| * 8 <
mean|integ|`. 21 of 21 straddles rejected, 40 of 40 true frames kept at rotation phases,
back-side set untouched.

⚠ **It does not retire the 60–92 keep-out.** The amplitude term is absolute and therefore
coupling-dependent (C43). A tag coupled well enough to straddle but too weakly to clear 2048
slips through. Two layers, not one.

⚠ **Still worth doing:** the gate is tuned on 21 straddles from one tag on one unit. A
second Indala tag (§4) would be the first real test of the 2048 threshold, since it moves
with coupling. Until then treat the margin — 2.2x above the loudest back-side frame, 2.5x
below the quietest straddle — as the whole safety budget.

**1d. ✅ The loud-signal null passes.** An HID Prox tag at **88–100x the empty floor in its
own band** produced `LF tag not found` on **30 of 30** `lf indala read` attempts. C44, L59.
That is the case C24 could not test: a loud wrong signal rather than a quiet one.

⛔ It nearly went in the bin. All 6 bracketing `lf hid prox read` calls failed, which by the
old §3 rule means "the tag was not coupled, the null is uninterpretable". The tag was at 90x
the floor the whole time. **A failed read is not evidence of absence** — bracket with
`lfprobe.py`, which measures presence directly (F05, and §3 below is rewritten).

## 2. ⭐⭐ Re-open what was closed on back-side data

⚠ These were closed, some of them emphatically, on measurements now known to be ~26 dB down.

| | why it should be re-opened |
|---|---|
| **`LF_RSSI` / AIN0** (C08, C09) | closed as "carries no fc/2, flat to 0.5 dB". Measured with the tag on the back. The whole comparison was between two nodes seeing 1/20 of the available signal, and the conclusion killed an entire line of investigation |
| **Stacking** (C29, C30, C34) | worth 1.5–2.1x on the back. On the front, single captures already decode 71% of the time, so it may be solving a problem that no longer exists — and at phase 64 it demonstrably revives a *wrong* answer |
| **Frame lock** (C11, C12, C13) | already in doubt (C31, C35) — the correlation that supports them reads 0.92–0.95 on the EMPTY field. Re-derive or retract |
| **SAADC gain** (C10) | "the floor is analog-referred" may well survive, but it was measured against a signal 26 dB below what the device actually delivers |

## 2b. ✅ Phase rotation re-derived — and the union was a trap

`PHASE_ROTATION` is now `{20, 12, 28, 36, 44, 16, 112, 0}`: the first five decode on BOTH
placements (10/10 or 9/10 across the two sweeps), and the last three are insurance chosen
for spread rather than rank, including one from the upper front band. The old tail (`4`,
`56`, `0`) was weak on the back and `56` sits one step from the dead band.

⛔ The lesson is in §1c-follow-up: taking the union of "phases that ever worked" would have
imported phase 64, which returns a wrong credential 5 times out of 5. A phase that is dead
is cheap; a phase that lies is not. The rotation is now derived from phases that decode
correctly on both sides AND produce no repeatable wrong frame on either.

## 3. ✅ LOUD-SIGNAL NULLS — DONE. 220 reads, 0 false positives

| interferer | modulation | bracket | result |
|---|---|---|---|
| HID Prox, back (C24) | FSK RF/50 | a read | 10/10 not found |
| HID Prox, front (C44) | FSK RF/50 | amplitude 88–100x | 30/30 |
| EM410x (C52) | ASK RF/64 | amplitude 19x + its own reader | 20/20 |
| **IDTECK (C55)** | **PSK1 RF/32 — Indala's own config word** | **its own reader, both units** | **40/40** |
| Viking (C55) | ASK RF/32 | amplitude 18.6/19.3x | 40/40 |
| PAC (C55) | NRZ RF/32 | amplitude 16.1/15.9x | 40/40 |
| Jablotron (C55) | biphase RF/64 | amplitude 18.1/18.3x | 40/40 |

Plus 320 empty captures producing no frame at all. ⛔ Read C57 before running another one:
the default band is wrong for a PSK1 interferer, and for that case the interferer's own
reader is the better bracket, not the amplitude probe.

## 4. ✅ A second Chameleon — reads 20/20 at the same phase

Same firmware, same copper coin, phase 20, one capture each, no gate rejections. Only the bit
offset moves (9 -> 10), which is C51's timing showing it depends on the reader too. ⚠ The
caveat §4 was written with still stands: same batch, same revision, so this is unit-to-unit
tolerance and not design generality. A pass was always going to be weak evidence.

## 3b. ⭐⭐⭐ FIX THE HID PROX READER — and there is a one-line candidate to test first

Not this project's decoder, but it is the comparison instrument for everything here and it
has cost two measurements already (L51's uninterpretable run, and C44's near-miss).

**What is established:** three tags with byte-identical memory read 0/6, 3/6 and 7/9 on the
Chameleon and 3/3 on a Proxmark (C46). The RF path is flat while reads fail (C45). So the
decoder's tolerance is narrower than the Proxmark's, and package-level differences cross it.

**⭐ Test this first — it is one line and it explains an old observation.**
`advertising_stop()` appears in **1 of 15** LF reader files. Only `lf_reader_generic.c`
suspends BLE advertising; `hidprox_read()` does not. That file's comment records the
measurement that put it there: an advertising burst collapses the 125 kHz field for ~1.6 ms
and hit **4 captures in 10**. And it predicts the thing nobody could explain in L51 —
*"after connecting it to my phone and/or a reboot, it does read"* — because connecting a BLE
central is precisely what stops advertising (C47).

```bash
# the discriminating test, ~5 minutes, needs the BLUE DUAL (the 0/6 tag — the others
# have too little headroom to show an improvement)
cd software/script && for i in $(seq 1 15); do .venv/bin/python cu.py "lf hid prox read" | tail -1; done
# then connect a phone over BLE so advertising stops, and repeat
```

⚠ **If it works, resist generalising it.** It cannot explain the blue dual reading 0/6
deterministically — an intermittent field collapse does not produce a clean zero. Expect two
causes: a BLE-induced intermittency affecting every LF reader, and a per-tag waveform
tolerance in the FSK demodulator. ⇒ The fix belongs in `capture_begin()`-style shared code so
all 15 readers get it, not pasted into `hidprox_read()` alone.

⚠ **Until it is fixed, do not use HID Prox as the probe tag for a null.** Use amplitude
(`lfprobe.py`) for presence, per §3.

## 4. ⭐ A second Chameleon

⚠ Worth doing and worth not over-reading. Two units bought together are the same hardware
revision from the same batch, so this tests unit-to-unit tolerance — antenna tuning,
component spread, trimmer position — and NOT whether the design generalises to a Chameleon
Ultra in general. A pass is weak evidence; a failure would be very strong.

## 5. ⭐⭐ Make the failure cheaper, or the success more certain

A read costs a median of 2–3 captures at ~35 ms (0.08–0.22 s of device time, measured); a
failure costs the whole 500 ms timeout (0.47–0.53 s, measured).
Two things are worth measuring now that decode rate is a real metric:

- **Sort the rotation by live evidence, not by the committed sweep.** The order is
  `20, 12, 28, 36, ...`, taken from a sweep whose phase ranking has since moved.
- **Longer settle.** A T5577 charges off the field before transmitting at full amplitude,
  and 2 ms has never been varied against a working decoder (L34 invalidated the old test).
  The Indala read restarts the field for every capture, so this is paid 2–3 times per read.

## 6. ⭐ Re-test the levers closed against the broken decoder

Air gap, settle and oversampling were all closed pre-BLE-fix on a tag carrying
`DEADBEEF/12345678` (L34), and every dB measured since went through a decoder that could
not decode. Tag position looks worth ~5.7 dB but rests on n=1 from an accidental probe.

## 7. ✅ RETIRE the per-lever sweep scripts — do not port them

`sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py` and `oversample_test.py` all score
the fc/2 **skirt**. C38 measured skirt and decode on the same 160 captures and they are
**uncorrelated**: the highest skirt in the sweep (tick 44) decodes 5/5, the lowest (ticks
116–120) also decodes 5/5, and the dead band sits in the middle of the range. The earlier
hope that they "can rank coupling even if they cannot tell you whether something decodes"
does not survive that — over a sample-phase sweep the skirt is dominated by transition
energy, not coupling.

⇒ Anything worth keeping from them should be rebuilt on `phasebits.py`'s pattern: count
decodes, against an empty arm at the same setting. `lfprobe.py` remains useful because it
measures a *ratio against the live empty floor* for coupling, which is a different job.

## 8. ⚠ T5577 WRITE reliability on the Chameleon — backlog

`lf indala write` exists and is **not known to work**. Two paths were tried:

| | result |
|---|---|
| three raw `lf_t55xx_write_block` calls | **one of three blocks landed** — the Proxmark dump showed block 2 took, blocks 0 and 1 did not |
| `write_indala_to_t55xx` via the proven `write_t55xx` | spectrum unchanged; the config block did not take either |

The raw path is weaker by construction — it cycles the field per block, so each block is
written to a tag charging from cold for 1 ms, once, with no retry, where `write_t55xx`
holds the field on and writes every block twice. That difference is real and the change was
right. It did not make the write work here, so something else is wrong too.

⚠ The obvious suspect is the one that sank §1: **a tag a Proxmark writes, verifies and reads
back can be completely inaudible to the Chameleon**, and writing needs more field than
reading. Nothing so far separates "the writer is broken" from "this tag was never coupled
well enough to be written".

⛔ **The blocker is instrumentation, not code.** The Chameleon has no T5577 *read*, so every
write attempt costs a physical Proxmark round trip to verify — which is why two attempts ate
an afternoon. ⇒ Add a T5577 block read before debugging the writer: `t55xx_send_cmd`
already carries the read opcodes and the protocol decoders already recover data off the
air. That turns a round trip into one command and makes the writer debuggable at all.

## 9. Upstreamable?

Nothing in `lf_indala_psk.c` is bench-specific and it has no nRF dependency. The pieces a
PR would need beyond what is here: emulation (`lf_tag_em.c` has a transmit-only `psk1.c`
already), `lf indala write` to T5577, and the tag-type registration that
`tag_base_type.h:61` leaves as a commented-out placeholder under `//////// PSK Tag-Talk-First 300`.

## Closed — do not re-open without new evidence

| | why |
|---|---|
| `LF_RSSI` / AIN0 as a signal tap | C08/C09: no fc/2, flat to 0.5 dB, though demonstrably alive |
| SAADC gain | C10: the floor is analog-referred and tracks gain |
| Folding at 2048 samples | C14: odd parity inverts the subcarrier every frame; it cancels the data |
| Indala parity as an acceptance gate | C19: it passed a frame that was wrong in 20 bits |
| A 12 kHz cutoff for the baseband filter | C16: the cutoff was never the point; the null at fs/2 was |
