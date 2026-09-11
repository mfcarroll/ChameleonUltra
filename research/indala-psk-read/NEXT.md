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

## 1c-follow-up. ⭐⭐⭐ THE HALF-BIT GATE — the one real hole left

In the dead band the decoder returns the SAME wrong card number every time: phase 64 gives
`a0000000b5af0b92` on 5 of 5, phase 88 `a0000000c6b90c92` on 5 of 5. Two independent
captures agree on it, so the acceptance rule passes it through. Requiring two different
*phases* to agree does not help either — `a0000000c6b90c92` is produced at both 88 and 92.

**Today this is latent**: no phase in `PHASE_ROTATION` lies in 60–92, and that is now the
only thing preventing a confidently wrong credential. ⚠ NEXT §2b as originally written —
"derive the rotation from the union of front and back working phases" — would have made it
live, because the back-side stacking table specifically credits phase 64.

⇒ **The fix should be a mechanism, not a keep-out list.** The signature is in the decoder's
own output and is not subtle: the winning alignment is `off 16`, exactly half the 32-sample
bit period, at ~half the amplitude (truth 7240–12699, wrong 5120–6833 — C43). Candidates,
in order of how much they rely on absolute level:

1. **Compare the winner against the alignment 16 offsets away.** A correctly aligned frame
   should beat its straddle by ~2x; a straddle sits between two half-strength neighbours.
   This is scale-free, which is the property a gate needs.
2. Reject when the per-bit integrator magnitudes are *bimodal* — a straddling window
   produces full-strength bits where neighbours match and near-zero where they differ.
3. ⚠ NOT an absolute amplitude threshold. It separates perfectly on this data and would
   still be wrong: amplitude scales with coupling, so a weakly-coupled tag falls under any
   fixed cut. C43.
4. ⚠ NOT Wiegand-26 parity, which C19 already rejected as a gate and which fails again
   here in the sharpest possible way: it rejects `a0000000b5af0b92` and `a0000000c6b90c92`
   but **passes `a0000000c6b90e92`** — a frame that also survives two-capture agreement and
   reports the CORRECT facility code (52) with a wrong card number. That is the exact shape
   of a credential a reader would hand over with confidence.

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

## 3. ⭐ Finish the loud-signal null — HID is done on BOTH sides, the ASK tags are not

C24 closed HID Prox on the back; C44 now closes it on the front against a tag measured at
88–100x the empty floor. That is the null the empty field cannot provide: a decoder
brute-forcing 32 offsets for a fixed pattern against a *loud* wrong signal is a different
proposition from one straining against silence.

⚠ HID Prox is FSK. EM410x, Viking, PAC and Jablotron are ASK/OOK and modulate the envelope
in a completely different way, which is what the fs/2 notch and the bit integrator actually
see. None of them are tested.

⛔ **BRACKET WITH `lfprobe.py`, NOT WITH A READ.** The old rule here said to confirm the
probe tag's coupling with a read immediately before and after. That rule is wrong and it
nearly threw away C44: six bracketing `lf hid prox read` calls failed while the tag sat at
90x the empty floor. A read conflates "not heard" with "heard but not decoded" — which is
the exact failure `lfprobe.py` exists to separate, and the reason the bracket was needed in
the first place. Measure amplitude in the interferer's own band, with a floor measured in
that same band.

```bash
# 1. floor for this tag's band, ANTENNA CLEAR
cd research/indala-psk-read && ../../software/script/.venv/bin/python lfprobe.py \
  --band 10000 18000 --monitor 5
# 2. tag on, same band — confirm it is loud, then run the null
../../software/script/.venv/bin/python lfprobe.py --band 10000 18000 --monitor 5 --floor <N>
cd ../../software/script && .venv/bin/python cu.py $(printf '"lf indala read" %.0s' {1..10})
```

⚠ EM410x/Viking/PAC/Jablotron subcarriers are NOT at 10–18kHz. Set `--band` per tag, and
measure that band's empty floor — there is no universal floor (M24).

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
