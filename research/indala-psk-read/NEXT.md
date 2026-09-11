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

## 1. ⭐⭐⭐ RE-MEASURE THE LOAD-BEARING CLAIMS ON THE FRONT

Roughly 26 dB was missing from every measurement. Do these in order; each is cheap and each
could collapse a pile of downstream work.

**1a. Full phase sweep with a null.** 32 phases x 5 repeats, tag and empty, on the front.
The 16x3 pilot says phases 0–48 and 96–120 give 3/3 and only 56–88 fails. Confirm it, and
note that the dead band carries the HIGHEST skirt — so it is a polarity null, and the right
model for phase is "two nulls per carrier period", not "a window".

```bash
cd research/indala-psk-read && ../../software/script/.venv/bin/python phasebits.py --step 4 --repeats 5 --keep caps/front
```

**1b. Are the three "individually fatal" details still fatal?** C03 (the fs/2 notch), C04
(not discarding the settle window) and C02 (the PSK1 mapping) were each measured at 1/20 the
signal. C02 is a convention and cannot change. The other two are SNR-dependent and may now
be merely helpful rather than load-bearing — worth knowing before anyone treats them as
sacred.

**1c. Is the two-capture agreement rule still needed?** C17 says one frame in five is wrong
and C18 builds the whole acceptance rule on it — both at 1/20 signal. If the error rate
collapses on the front, a read could return on the FIRST decode and take ~35 ms instead of
~100 ms. ⚠ Measure it, do not assume it: a wrong credential is much worse than a slow read,
so the rule stays until the data says otherwise.

**1d. Re-run the loud-signal null.** C24 (no false positive on HID Prox) was run with the
HID tag on the back — a *quiet* wrong signal, which is the easy case. On the front that
interferer is ~20x louder, which is the case the null was supposed to test.

## 2. ⭐⭐ Re-open what was closed on back-side data

⚠ These were closed, some of them emphatically, on measurements now known to be ~26 dB down.

| | why it should be re-opened |
|---|---|
| **`LF_RSSI` / AIN0** (C08, C09) | closed as "carries no fc/2, flat to 0.5 dB". Measured with the tag on the back. The whole comparison was between two nodes seeing 1/20 of the available signal, and the conclusion killed an entire line of investigation |
| **Stacking** (C29, C30, C34) | worth 1.5–2.1x on the back. On the front, single captures already decode 71% of the time, so it may be solving a problem that no longer exists |
| **Frame lock** (C11, C12, C13) | already in doubt (C31, C35) — the correlation that supports them reads 0.92–0.95 on the EMPTY field. Re-derive or retract |
| **SAADC gain** (C10) | "the floor is analog-referred" may well survive, but it was measured against a signal 26 dB below what the device actually delivers |

## 2b. ⚠ Re-derive the firmware's phase rotation

`PHASE_ROTATION` is `{20, 12, 28, 36, 44, 4, 56, 0}`, chosen from the back-side window. On
the front, 56 sits inside the dead band and 96–120 are all good and entirely absent from the
list. ⚠ The right rotation must work for a tag placed on EITHER side, because users will do
both — so derive it from the union, not from whichever placement is measured last.

## 3. ⭐ Finish the loud-signal null — HID is done, the ASK tags are not

C24 closed HID Prox: 10/10 `LF tag not found` with the tag on the antenna and its coupling
confirmed by a 5/5 HID read immediately before. That matters because it is the null the
empty field cannot provide — a decoder brute-forcing 32 offsets for a fixed pattern against
a *loud* wrong signal is a different proposition from one straining against silence.

⚠ HID Prox is FSK. EM410x, Viking, PAC and Jablotron are ASK/OOK and modulate the envelope
in a completely different way, which is what the fs/2 notch and the bit integrator actually
see. None of them are tested.

⛔ **Confirm the probe tag's coupling immediately before and after, in the same run.** Not
doing this is what made L51's measurement uninterpretable: `lf hid prox read` on this bench
is intermittent enough to sit at 0/15 for a quarter of an hour, so "the Indala read found
nothing" means nothing on its own — it has to be bracketed by proof the tag was there.

```bash
cd software/script && .venv/bin/python cu.py \
  "lf hid prox read" "lf hid prox read" \
  "lf indala read" "lf indala read" "lf indala read" \
  "lf hid prox read" "lf hid prox read"
```

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

## 7. ⚠ The per-lever sweep scripts score the wrong thing

`sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py` and `oversample_test.py` all
score the fc/2 **skirt**, which is transition energy and is polarity-blind (`METHOD.md` M8).
They can rank coupling, but they cannot tell you whether something decodes. Port them to
decode rate the way `phasebits.py` was, or retire them.

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
