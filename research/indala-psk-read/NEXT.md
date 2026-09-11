# Next — ranked

**State:** solved. The Chameleon Ultra reads Indala from a single 300 ms capture
(`FINDINGS.md`). What remains is turning that into a device feature and re-testing the
levers that were closed against a decoder that could not work.

⛔ Method rules live in `METHOD.md`, not here. Read them before adding a claim to the ledger.

---

## 1. ⭐⭐⭐ Port the decoder into firmware as `lf indala read`

The whole read is integer arithmetic over 4096 samples — no float, no FFT. `FINDINGS.md`
has the chain. `lf_reader_generic.c` already captures the samples; the low-pass can be a
short FIR or a two-stage boxcar.

⚠ **Three details are each individually fatal** (C02, C03, C04): the PSK1 mapping — the
phase *is* the data; the baseband low-pass — 32/35 with it, **0/35** without; and **not**
discarding the settle window — 43/160 with the full capture, **0/160** without.

## 2. ⭐⭐ Pick the sample phase, and check the window generalises

Ticks 12–36 decode best; nothing decodes past 60. **The stock trigger is phase 0, which
decodes 0/5** — so stock firmware would fail even with a correct decoder, which is worth
knowing before anyone blames an antenna.

⚠ The window's position may be specific to this tag's coupling and this unit. Check a
second Indala tag, and ideally a second Chameleon, before hard-coding a phase. Scanning
three or four phases is cheap insurance and probably the right production behaviour.

## 3. ⭐ Check the Indala parity to reject near-misses

Most failures are one or two bits inside the zero run (`a0100000e6bd0e92` for
`a0000000e6bd0e92`). Indala carries a parity — the Proxmark prints `Parity: 11` — so
checking it would reject most near-misses and turn a 27% raw decode rate into a much higher
effective rate with retries.

## 4. ⭐ Re-test the levers closed against the broken decoder

Air gap, settle and oversampling were all closed pre-BLE-fix on a tag carrying
`DEADBEEF/12345678` (L34), and every dB measured since went through a decoder that could
not decode. There is now a real success metric — **decode rate** — so each is worth a few
minutes.

Tag position looks worth ~5.7 dB but rests on n=1 from an accidental probe. Start there.

## 5. ⚠ The per-lever sweep scripts score the wrong thing

`sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py` and `oversample_test.py` all
score the fc/2 **skirt**, which is transition energy and is polarity-blind (`METHOD.md` M8).
They can rank coupling, but they cannot tell you whether something decodes. Port them to
decode rate the way `phasebits.py` was, or retire them.

## Closed — do not re-open without new evidence

| | why |
|---|---|
| `LF_RSSI` / AIN0 as a signal tap | C08/C09: no fc/2, flat to 0.5 dB, though demonstrably alive |
| SAADC gain | C10: the floor is analog-referred and tracks gain |
| Folding at 2048 samples | C14: odd parity inverts the subcarrier every frame; it cancels the data |
