# Next — ranked

**State:** shipped and working on one coil — 10/10 at 0.40–0.43 s, one capture each. The
stacking path does not regress that, and raises the offline decode rate 31.9% -> 71.9%.
⛔ A second coil with the same payload reads 0/12 and is 3.3x weaker in the fc/2 band, and
**position alone is worth more than 2x** — larger than the whole working margin. That, not
the decoder, is the open problem. `lf indala read` returns the credential in
~0.5 s, 20/20 with the bench tag, 0/20 empty, 10/10 against a loud HID signal, and 36
synthetic words decode with no wrong answers (`FINDINGS.md`).

⛔ **But a second, Proxmark-verified Indala tag was completely inaudible to the Chameleon —
0 of 12, with no subcarrier present at any sample phase.** Until §1 is resolved, the honest
scope of this work is "reads the bench tag", and whether that is a position problem, a tag
problem or a reader problem is unknown.

⛔ Method rules live in `METHOD.md`, not here. Read them before adding a claim to the ledger.

---

## 1. ⭐ REPOSITION THE WEAK COIL FIRST — position is worth more than the whole margin

The copper coil reads at **3.55–3.67x** the empty floor today, against **1.53–1.61x** in the
original sweep. Same tag, same reader, different placement. ⇒ **Position alone is worth more
than 2x**, and the entire difference between the two coils is 3.3x (C32, C33).

So before any more signal processing: put the white coil on and move it while watching.

```bash
cd research/indala-psk-read && ../../software/script/.venv/bin/python \
  lfprobe.py --band 57000 62400 --monitor 90
```

⚠ ⭐ **Recalibrate the `--floor` first.** The default 6900 came from the committed empty
captures; the copper coil now reads 3.6x, so the scale is trustworthy — but take a fresh
empty reading with no tag on the pad and pass it, rather than trusting a number from another
session. Then slide the white coil and watch for anything above ~1.5x.

If it never exceeds ~1.2x anywhere on the pad, the coil genuinely cannot couple to this
antenna and §2 is the answer. If it reaches 2x+, everything below is moot.

## 2. ⛔ Why does stacking not rescue the weak coil?

Stacking is worth 1.5–2.1x on the copper coil and **1.0x** on the white one (C34), so the
white coil's captures are not coherent with each other in the way the copper coil's are.

⭐ **Best current story, and it is testable:** a weakly-coupled T5577 charges more slowly and
more variably off the field, so its frame START JITTERS between captures. Adding captures
that are not frame-aligned averages the credential away while the noise still falls —
exactly the flat ratio observed.

⇒ **Align on the preamble, not on lag 0.** The decoder already locates the 33-bit preamble
in every capture it can decode at all; shifting each capture to a common preamble position
before adding would make stacking work regardless of when the tag woke up. For captures too
weak to find a preamble, search the shift that maximises correlation with the accumulator
*inside the fc/2 data band*.

⛔ **But fix the instrument first, because it is still broken.** Empty captures correlate at
**0.92–0.95** after mixing to baseband, band-limiting to 6 kHz, removing DC and dropping
1024 samples (C31). It is not the odd/even imbalance — that is 1.7–2.1 counts, 0.1% of the
ripple. And the lag structure is near-identical for the two tags (C35), which is the proof
it is not measuring either tag. Until that common background is identified, **no
cross-capture correlation on this bench means what it appears to mean**.

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
