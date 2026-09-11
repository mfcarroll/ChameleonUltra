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
| ~~**Stacking**~~ ✅ | **Resolved, C58.** Nothing on the front (68.75% at every depth); reinforces the straddle in the dead band; still worth 32%->72% on the back. Keep it, but it is insurance for the wrong placement, not a feature of the right one |
| ~~**Frame lock**~~ ✅ | **Resolved, C59.** The conclusion stands, the evidence does not: the empty field correlates as well as the tag, but rolled-stack decoding collapses 67%->0%, which proves alignment operationally (M25) |
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

## 3b. ⭐⭐⭐ FIX THE HID PROX AND PAC READERS — both fail on loud tags

⭐⭐ **START HERE: the SAADC readers duplicate a capture path that one of them gets right.**
The LF readers are two families, and it matters which:

| family | readers | capture |
|---|---|---|
| GPIO/comparator | em410x, jablotron, viking | `register_rio_callback`, 128-entry ring, no SAADC |
| **SAADC** | **hidprox, ioprox, pac** + lf_reader_generic | own `saadc_cb`, own 6144 ring, own field start/stop |

Only `lf_reader_generic.c` also suspends BLE advertising — and its own comment records why:
a burst collapses the 125 kHz field for ~1.6 ms and hit **4 captures in 10**. The three that
duplicate the prologue instead of sharing it are HID, ioProx and PAC, and HID and PAC are
exactly the two measured failing on loud tags. The SAADC reader that HAS the guard is Indala,
at 60/60.

⇒ `capture_begin()`/`capture_end()` already exist and are already shared by two entry points.
Moving HID onto them is a small mechanical change that also happens to be **the clean test of
C47** — same protocol, same tag, same bench, one variable.

⛔ **Do not read the em410x 95% as evidence either way.** It is on the GPIO path and never
touches the SAADC, so it cannot test this. That mistake is why C47 was wrongly weakened in
L64.


Not this project's decoder, but it is the comparison instrument for everything here and it
has cost two measurements already (L51's uninterpretable run, and C44's near-miss).

**What is established:** three tags with byte-identical memory read 0/6, 3/6 and 7/9 on the
Chameleon and 3/3 on a Proxmark (C46). The RF path is flat while reads fail (C45). So the
decoder's tolerance is narrower than the Proxmark's, and package-level differences cross it.

⚠ **`lf pac read` has the same disease**: 0/5 on one unit and 2/5 on the other while its tag
sat at 16x the empty floor and a Proxmark read it perfectly (L66). So this is not one broken
decoder — HID Prox and PAC both fail on tags that are loudly present, and `lf em 410x read`
sits at 95% (76/80) rather than 100%. ⇒ Whatever is wrong may be shared across the LF reader
family rather than specific to FSK. Fix HID first because it fails hardest (0/6, one tag
never reading at all), but measure PAC in the same session — a fix that moves both is a very
different fix from one that moves only HID.

⚠ The Indala reader is the only LF reader in this tree with its own capture path
(`raw_read_samples`, which suspends BLE and hands the decoder a whole buffer). It is also the
only one at 100% — 40/40 today across two units. That may be the cleanest clue available, or
it may be that Indala is simply the only one anybody has tuned. Do not assume which.

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

## 3c. ⛔ FIRMWARE BUG: changing a slot to a PSK1 type while emulating leaves the PWM clock wrong

**Affects IDTECK as much as Indala — pre-existing, not introduced by this work.**

`pwm_init()` picks the PWM base clock from the CURRENT tag type:

```c
cfg.base_clock = IS_PSK1_TYPE(m_tag_type) ? NRF_PWM_CLK_1MHz : NRF_PWM_CLK_125kHz;
```

but it is called only from `lf_sense_enable()`, which runs only on a
`LF_SENSE_STATE_{NONE,DISABLE} -> ENABLE` transition. Changing `m_tag_type` afterwards — which
is exactly what `hw slot type` does — never re-inits the PWM. The clock keeps whatever value
it had when sense was last enabled.

Every PWM entry is `counter_top / base_clock` with counter_top = 16:

| base clock | entry | subcarrier | bit (16 entries) | |
|---|---|---|---|---|
| 1 MHz (PSK1) | 16 µs | 62.5 kHz | 256 µs | correct — fc/2 at RF/32 |
| 125 kHz | 128 µs | 7.8 kHz | 2048 µs | **8x too slow, unrecognisable** |

Both directions are broken: select a PSK1 type while a non-PSK1 one is live and the
subcarrier is 8x slow; select a non-PSK1 type while PSK1 is live and everything ASK/FSK runs
8x fast.

⚠ **Real by inspection, NOT yet confirmed as the cause of any symptom.** It was found while
debugging silent Indala emulation, and the mode cycle that should prove it had not been run
when this was written. Do not close it by assuming it explains that; and do not assume it is
the ONLY thing wrong with PSK1 emulation — see the control test below.

**Workaround:** `hw mode -r` then `hw mode -e`, or reboot, after changing the slot type.

**Fix:** re-init the PWM when `IS_PSK1_TYPE(m_tag_type)` changes — either in the LF loadcb
when the new type's PSK1-ness differs from the live one, or by cycling sense on slot change.
⚠ `nrfx_pwm_uninit`/`init` mid-emulation needs care: `lf_sense_disable()` also releases the
HFXO request and nulls `m_pwm_seq`, so a naive disable/enable would drop the loaded sequence.

⛔ **The control test this needs, and it should have come first:** set a slot to **IDTECK**
and see whether a Proxmark reads it. IDTECK shares the entire transmit path, and nothing in
this tree records it ever having been verified end to end — `idteck.c` documents only the
READ side as unimplemented. If IDTECK is silent too, PSK1 emulation never worked and the
Indala addition inherited a broken base, which is a much larger problem than `indala.c`.

## 4. ✅ A second Chameleon — reads 20/20 at the same phase

Same firmware, same copper coin, phase 20, one capture each, no gate rejections. Only the bit
offset moves (9 -> 10), which is C51's timing showing it depends on the reader too. ⚠ The
caveat §4 was written with still stands: same batch, same revision, so this is unit-to-unit
tolerance and not design generality. A pass was always going to be weak evidence.

## 5–6. ✅ CLOSED AS UNMOTIVATED — there is no deficit left to hunt

⛔ **Read this before re-opening any of the signal-hunting work.** §2's remaining entries
(`LF_RSSI`/AIN0, SAADC gain), §5 (make the failure cheaper) and §6 (re-test the levers) all
existed for one reason: the read did not work and we were looking for missing signal. Both
causes turned out to be elsewhere — ~26 dB was the tag being on the wrong side of the device
(C36) and the rest was the decoder demodulating PSK2 against a PSK1 tag.

On hardware now: **60 of 60 reads succeeded, on two tags and two units, every one of them at
the FIRST phase in the rotation, from a single capture.** There are no failures to make
cheaper, no rotation order to tune, and no deficit for a better signal tap to close.

| | why it is closed |
|---|---|
| `LF_RSSI` / AIN0 (C08, C09) | was a search for a better signal tap. Nothing needs one |
| SAADC gain (C10) | same. The conclusion may or may not hold; it no longer matters |
| Rotation order (§5) | entry 1 wins 60/60. There is nothing to sort |
| Longer settle (§5) | paid per capture, and reads take one capture |
| Air gap / settle / oversampling (§6) | all closed against the broken decoder, all hunting the same phantom |

⚠ They become live again **only** if someone cares about the back-side case, where the read
is 32% and stacking is doing real work (C58). That is the wrong placement, so caring about it
is a product decision, not a technical one. ⇒ Do not spend a day here without that decision.

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

## 8. ✅ T5577 WRITE — works, 9 of 9 verified

`lf indala write` reads the tag before and after and reports VERIFIED / WRITE DID NOT LAND /
WRITE FAILED / WRONG DATA / CANNOT TELL. Measured by alternating between two words, so each
result proves a state change rather than a tag that already held the value: **9 consecutive
writes, all VERIFIED** (C60).

⇒ The question this section could never answer — *is the writer broken, or was the tag never
coupled well enough to be written* — resolves as **coupling**. The original failures predate
the placement discovery (L57) and were taken with the tag on the back, where writing, which
needs more field than reading, is the worst case of the worst placement.

⛔ And §8's own stated blocker was wrong. It said to add a T5577 block read before debugging
the writer. By the time that was written the verifier already existed: `lf indala read` is
60/60 across two tags and two units, and one successful read-back covers blocks 0, 1 and 2
together. A T5577 block read is still worth having for OTHER protocols — it was never on this
path.

⚠ What is NOT established: writes to a tag the reader cannot hear. Verification is only as
good as read coupling, which is why CANNOT TELL exists as a distinct verdict rather than
being folded into failure.

## 8b. ⚠ 32KB of the Indala reader serves only the wrong placement — a decision, not a bug

The reader holds **48KB static**: 8KB samples, 8KB scratch, and **32KB of stacking
accumulators**. C58 measured stacking at exactly 68.75% for N=1..5 on the front — no gain at
any depth, because a good phase decodes from one capture and a dead one never decodes — and
32% -> 72% on the back.

⇒ Two thirds of the reader's RAM is insurance for the placement users are told not to use.

| option | RAM | back-side read rate |
|---|---|---|
| as shipped | 48KB | 72% |
| int16 accumulators, cap stacking at 2 | 32KB | 52% (C58, N=2) |
| drop stacking | 16KB | 32% |

⚠ This is a product decision about whether back-side reads matter, and the numbers are here
so it can be made rather than drifted into. ⛔ Do NOT drop stacking without also re-checking
the straddle gate: C58 showed stacking REINFORCES the dead-band straddle, and the gate is
what holds it at 0 wrong — removing one without re-measuring the other is the dangerous move.

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
