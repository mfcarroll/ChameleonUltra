# Indala PSK read on Chameleon Ultra

**The Chameleon Ultra reads Indala** — on the device, as a command.

```
lf indala read   ->   Indala PSK1
                      Raw: a0000000e6bd0e92
                      Fmt 26 FC: 52 Card: 63612 Parity: 11
```

⛔⛔ **PUT THE TAG ON THE FRONT OF THE DEVICE — the side with the buttons.** The Flipper
reads from its back; the Chameleon is the other way round, and it is worth **21x**. Every
measurement in this project before 2026-09-11 was taken on the wrong side, which is most of
the deficit it spent days trying to explain. Claims measured that way are marked ⚠B in
`FINDINGS.md` and are provisional.

On the front: **114 of 160 single captures decode (71%) and 0 of 160 empty captures produce

⭐ **Emulation works too, verified against two independent readers**: a Flipper Zero reads the
emulated credential 6 times out of 6 (`Indala26 FC 52 Card 63612`) and a Proxmark decodes it
from up to 262 ms of continuous capture. ⚠ Our own reader does NOT read our own emulator —
not a defect but a specialisation, since its exact-Nyquist demodulation assumes a subcarrier
locked to the reader's carrier, which every real tag provides and a free-running PWM does not.

⭐ **And `lf indala write` works: 9 of 9 verified writes.** It reads the tag before and after,
so it reports VERIFIED, WRITE DID NOT LAND, WRITE FAILED, WRONG DATA or CANNOT TELL rather
than claiming success it cannot know. A successful read-back covers all three T5577 blocks at
once — a tag whose config block had not landed would not be transmitting PSK1 at RF/32 and
could not be read at all.
a frame at all**, measured over 32 sample phases x 5. Sample phase turns out to be two
working bands — 0–56 and 96–124, every one of them 5/5 — split by a dead band at 60–92; the
stock phase 0 is fine on the front and useless on the back. A coil that read 0/12 on the
back reads first try. The demodulation is integer arithmetic on the nRF52840 — no float, no
FFT, 8 KB of buffer.

With no tag on the antenna it reports `LF tag not found` 20 times in 20, taking the full
500 ms budget each — the rotation exhausts rather than aborting early.

⚠ **A single decode is not trustworthy**, so the firmware returns a credential only once two
captures agree. On the back one recovered frame in five was wrong; on the front 21 of 135.

⛔ **And agreement is not sufficient on its own — it is the phase rotation that makes it
safe.** In the 60–92 dead band the decoder returns the *same* wrong card number on every
capture (phase 64 → `a0000000b5af0b92`, 5 of 5), because the winning bit alignment there is
half a bit period off and every integrator straddles a boundary. Two independent captures
agree on it. Nothing in `PHASE_ROTATION` lies in that band, and nothing may be added to it
without checking — see the ⛔⛔ block in `lf_indala_data.c`. The premise the agreement rule
was built on, that wrong words never repeat, held only because the original captures were
26 dB down: **more signal turned a random error into a systematic one.**

⛔ **This was believed impossible for most of the investigation** — "31.2 dB below the
Proxmark", "7.6 dB short", "detectable but not decodable". All retracted. The deficit was a
software bug: the demodulator was decoding **PSK2 against a PSK1 tag**, and its self-test
encoded with the same wrong convention, so it passed forever. `FINDINGS.md` has the ledger.

## Where to look

| | |
|---|---|
| **`FINDINGS.md`** | ⭐ **Start here.** Current knowledge and the claims ledger. Holds no history. |
| `NEXT.md` | Ranked next steps. |
| `METHOD.md` | The method rules — eleven wrong conclusions, and what each one taught. |
| `LOG.md` | What was learned when, indexed to git. Append-only. |
| `archive/` | The old working notes, frozen. Most of their headline numbers are retracted. |
| `ADVERSARIAL.md` | A hostile-review prompt, if you want to attack the conclusions. |

## Resuming a session

Point a fresh session at this file. Read `FINDINGS.md` then `NEXT.md`; consult `LOG.md`
only to answer "when did we learn X", and `archive/` only for reasoning, never for numbers.

⚠ **Before trusting any measurement, confirm the tag state.** The campaign registry's
`PSK1` cell writes `DEADBEEF/12345678` — correct PSK1 air shaping, but *not* an Indala
frame. Misreading that invalidated a day.

```bash
cd /Users/Shared/code/personal/rfid/proxmark3 && ./pm3 -c "lf indala reader"
```

Expect `a0000000e6bd0e92`, `Fmt 26 FC: 52 Card: 63612`.

## Tooling

| | |
|---|---|
| `mfdemod.py` | ⭐ The working decoder. `--selftest`, or pass capture files. |
| `phasebits.py` | Sweep sample phase, count decodes. `--analyse-only` re-runs on existing captures. |
| `generality.py` | ⭐ Does the decoder work on words other than the bench tag's? Synthetic, no hardware. |
| `lfprobe.py` | ⭐ Is an LF problem RF or firmware? Measures the tag's signal continuously instead of whether it decoded. |
| `stack.py` | Zero-offset capture stacking and the polarity diagnostics. |
| `inputtest.py` | AIN5 vs AIN0 paired comparison. |
| `sweep.py` `phasesweep.py` `gaintest.py` `gapsweep.py` `oversample_test.py` | Per-lever sweeps. ⚠ these score the fc/2 *skirt*, which is polarity-blind — see `METHOD.md` M8. |
| `cu.py` (in `software/script/`) | Run CLI commands non-interactively. |
| `ctest/` | ⭐ Host build of the **firmware** decoder. `make check` diffs it against `mfdemod.py` per capture. |
| `checkdocs.sh` | ⭐ Verify the notes have not drifted. Run it before committing a notes change. |

Decode the committed captures:

```bash
../../software/script/.venv/bin/python mfdemod.py --selftest && ../../software/script/.venv/bin/python mfdemod.py caps/phasebits/tag_p024_r*.bin
```

⭐ Cross-check the firmware decoder against the research one, per capture, no hardware:

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read/ctest && make check
```

Reproduce the phase sweep result from committed data (no hardware needed):

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read && ../../software/script/.venv/bin/python phasebits.py --analyse-only --keep caps/phasebits --step 4 --repeats 5
```

## Keeping these notes honest

History and current knowledge drift apart when a fact lives in two places — you update one
and the others go stale silently. So:

- **`LOG.md` is append-only.** The only permitted edit to an entry is appending a
  `⛔ retracted by L##` pointer. Never reword an entry to match what you now believe.
- **`FINDINGS.md` holds no history** and is rewritten freely. If a claim is not in its
  ledger, it is not established.
- **`archive/` is frozen.** Cite it for reasoning, never for numbers.
- A new claim enters the ledger with its `n`, its `null` and its `indep` filled in. **If a
  column would be blank, that is the finding** — say so rather than leaving it empty.

```bash
./checkdocs.sh
```

## Environment

| | |
|---|---|
| repo | `/Users/Shared/code/personal/rfid/ChameleonUltra`, branch `indala-psk-read` |
| harness | `/Users/Shared/code/personal/rfid/Momentum-Firmware`, branch `t5577-deep-read` |
| Proxmark | `/Users/Shared/code/personal/rfid/proxmark3`, device `/dev/tty.usbmodemiceman1` |
| python | `ChameleonUltra/software/script/.venv/bin/python` — numpy + pyserial |
| nrfutil | `/Users/Shared/code/personal/rfid/.tools/bin/nrfutil` (8.2.1) |

Build and flash (needs Docker running):

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/firmware && PATH=/Users/Shared/code/personal/rfid/.tools/bin:$PATH ./flash-dfu-app-macos.sh
```

⚠ If `nrfutil` is missing, install it Nordic's way — **not** Homebrew, whose cask is
disabled for a Gatekeeper failure. `curl` sets no quarantine attribute, so there is no
prompt:

```bash
mkdir -p /Users/Shared/code/personal/rfid/.tools/bin && curl -sL -o /Users/Shared/code/personal/rfid/.tools/bin/nrfutil https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/aarch64-apple-darwin/nrfutil && chmod 755 /Users/Shared/code/personal/rfid/.tools/bin/nrfutil && PATH=/Users/Shared/code/personal/rfid/.tools/bin:$PATH nrfutil install device nrf5sdk-tools
```

## `lf sniff` flags added by this work

`--bits 16` full 14-bit conversion instead of `>>5` · `--phase N` sample phase, 0–127 ticks
of 62.5 ns (**ticks 4–60 work, stock 0 fails**; the ranking inside that window moves
between sessions, so `lf indala read` rotates rather than picking one) · `--input {5,0}`
AIN5 or AIN0/`LF_RSSI` (measured dead) · `--rate N` free-running kHz · `--gain N` divisor ·
`--settle N` ms

## `lf indala read`

New command, `DATA_CMD_INDALA_SCAN` = 3033. Firmware in
`firmware/application/src/rfid/reader/lf/lf_indala_psk.{c,h}` (the demodulator, portable
integer C) and `lf_indala_data.{c,h}` (capture, phase rotation, agreement rule).
