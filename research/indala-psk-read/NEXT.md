# Next — ranked

**State:** shipped and validated on ONE tag, and the second tag is the whole story now. `lf indala read` returns the credential in
~0.5 s, 20/20 with the bench tag, 0/20 empty, 10/10 against a loud HID signal, and 36
synthetic words decode with no wrong answers (`FINDINGS.md`).

⛔ **But a second, Proxmark-verified Indala tag was completely inaudible to the Chameleon —
0 of 12, with no subcarrier present at any sample phase.** Until §1 is resolved, the honest
scope of this work is "reads the bench tag", and whether that is a position problem, a tag
problem or a reader problem is unknown.

⛔ Method rules live in `METHOD.md`, not here. Read them before adding a claim to the ledger.

---

## 1. ⛔⛔ WHY DOES STACKING NOT RESCUE THE WEAK TAG?

This is the question everything else now hangs on.

|  | single | stacked | gain |
|---|---|---|---|
| bench tag (reads) | 1.53–1.61x floor | **2.71–2.91x** | 1.74x |
| white coin (does not) | 1.04–1.12x | 0.91–1.21x | **1.00x** |

Stacking is worth 1.74x on the tag that already works and **nothing** on the tag that needs
it. Since ~1.5x reads and ~1.1x does not (C27), 1.74x applied to the white coin would land
it comfortably inside the working range — if it applied.

⭐ **And the signal is definitely there.** Stacked, the white coin yields
`a0000000e6ad0e92`, `a0000000e4bd0a92` and `a0000000e33d0e92` at three separate phases —
1, 2 and 3 bits from `a0000000e6bd0e92` — while stacked noise yields no frame at all, ever.
It is 1.4x short, not absent.

⛔ **Start by fixing the instrument, because it is broken.** C31: the lag-0 baseband
correlation used to justify frame-lock (C11) and stacking (C12) reads **0.75–0.83 on the
EMPTY field** and only 0.15–0.49 on the tags. It is measuring the field turn-on transient,
which is identical in every capture — the exact artefact that already produced one wrong
conclusion in this project (`stack.py` Control 1). ⇒ Re-derive cross-capture alignment with
the transient excluded and the correlation restricted to the fc/2 sideband, then ask:

1. Are the white coin's captures aligned to each other at all? If a different T5577 starts
   its frame at a different point after power-up, there is nothing to stack.
2. If they are not, can the alignment be *recovered* rather than assumed — cross-correlate
   and shift before adding, instead of trusting lag 0?
3. Does the polarity resolution work at this SNR? It currently correlates raw captures,
   which are dominated by ~316 counts of carrier ripple against a ~10-count subcarrier.
   ⚠ Correlating the baseband instead changes nothing (measured), which is itself
   suspicious and probably means both are keying on the transient.

## 1b. ⚠ The stacking firmware has NOT been verified on a tag that reads

It is validated offline — C29, over every combination of the committed captures, decoded by
the firmware decoder itself — and on-device it behaves correctly against a tag it cannot
read (0/12, clean timeouts, no crashes). But no successful on-device read has happened
since it landed, because the tag that reads was not on the bench. **Put the thin copper
coin back and confirm a read still takes ~2 captures and ~0.5 s.**

## 2. ⭐ Finish the loud-signal null — HID is done, the ASK tags are not

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

## 3. ⭐ A second Chameleon

⚠ Worth doing and worth not over-reading. Two units bought together are the same hardware
revision from the same batch, so this tests unit-to-unit tolerance — antenna tuning,
component spread, trimmer position — and NOT whether the design generalises to a Chameleon
Ultra in general. A pass is weak evidence; a failure would be very strong.

## 4. ⭐⭐ Make the failure cheaper, or the success more certain

A read costs a median of 2–3 captures at ~35 ms (0.08–0.22 s of device time, measured); a
failure costs the whole 500 ms timeout (0.47–0.53 s, measured).
Two things are worth measuring now that decode rate is a real metric:

- **Sort the rotation by live evidence, not by the committed sweep.** The order is
  `20, 12, 28, 36, ...`, taken from a sweep whose phase ranking has since moved.
- **Longer settle.** A T5577 charges off the field before transmitting at full amplitude,
  and 2 ms has never been varied against a working decoder (L34 invalidated the old test).
  The Indala read restarts the field for every capture, so this is paid 2–3 times per read.

## 5. ⭐ Re-test the levers closed against the broken decoder

Air gap, settle and oversampling were all closed pre-BLE-fix on a tag carrying
`DEADBEEF/12345678` (L34), and every dB measured since went through a decoder that could
not decode. Tag position looks worth ~5.7 dB but rests on n=1 from an accidental probe.

## 6. ⚠ The per-lever sweep scripts score the wrong thing

`sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py` and `oversample_test.py` all
score the fc/2 **skirt**, which is transition energy and is polarity-blind (`METHOD.md` M8).
They can rank coupling, but they cannot tell you whether something decodes. Port them to
decode rate the way `phasebits.py` was, or retire them.

## 7. ⚠ T5577 WRITE reliability on the Chameleon — backlog

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

## 8. Upstreamable?

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
