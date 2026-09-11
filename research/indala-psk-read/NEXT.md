# Next — ranked

**State:** shipped and validated both ways. `lf indala read` returns the credential in
~0.5 s, 20/20 with the tag and 0/20 without (`FINDINGS.md`). What remains is generality,
then re-testing the levers that were closed against a decoder that could not work.

⛔ Method rules live in `METHOD.md`, not here. Read them before adding a claim to the ledger.

---

## 1. ⭐⭐ Check a second Indala tag, and a second Chameleon

The phase window is **not stable even across sessions on the same tag and unit** — phase 12
went from 5/5 to 4/10 correct overnight while phase 28 went the other way. The rotation
absorbs that, which is why it is a rotation. What is still untested is whether a different
tag or a different unit lands *outside* ticks 4–60 entirely, which the rotation would not
absorb.

⚠ A second tag also tests something the parity cannot: `descramble26()` assumes format 26.
A 29-bit or other-format Indala tag should still return a raw frame, with the Wiegand-26
parity reported as failing — check it does not do something worse.

## 2. ⭐⭐ Make the failure cheaper, or the success more certain

A read costs a median of 2–3 captures at ~35 ms (0.08–0.22 s of device time, measured); a
failure costs the whole 500 ms timeout (0.47–0.53 s, measured).
Two things are worth measuring now that decode rate is a real metric:

- **Sort the rotation by live evidence, not by the committed sweep.** The order is
  `20, 12, 28, 36, ...`, taken from a sweep whose phase ranking has since moved.
- **Longer settle.** A T5577 charges off the field before transmitting at full amplitude,
  and 2 ms has never been varied against a working decoder (L34 invalidated the old test).
  The Indala read restarts the field for every capture, so this is paid 2–3 times per read.

## 3. ⭐ Re-test the levers closed against the broken decoder

Air gap, settle and oversampling were all closed pre-BLE-fix on a tag carrying
`DEADBEEF/12345678` (L34), and every dB measured since went through a decoder that could
not decode. Tag position looks worth ~5.7 dB but rests on n=1 from an accidental probe.

## 4. ⚠ The per-lever sweep scripts score the wrong thing

`sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py` and `oversample_test.py` all
score the fc/2 **skirt**, which is transition energy and is polarity-blind (`METHOD.md` M8).
They can rank coupling, but they cannot tell you whether something decodes. Port them to
decode rate the way `phasebits.py` was, or retire them.

## 5. Upstreamable?

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
