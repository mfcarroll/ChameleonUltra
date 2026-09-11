# Indala PSK read on Chameleon Ultra

**The Chameleon Ultra reads Indala.** A single 300 ms capture decodes the credential.

```
lf sniff --timeout 300 --phase 24   ->   a0000000e6bd0e92
                                         Fmt 26  FC 52  Card 63612
```

43 of 160 single captures decode exactly; the empty field produced the truth 0 times in
160. No stacking, no averaging, and it works at the stock 8-bit sample width.

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
| `stack.py` | Zero-offset capture stacking and the polarity diagnostics. |
| `inputtest.py` | AIN5 vs AIN0 paired comparison. |
| `sweep.py` `phasesweep.py` `gaintest.py` `gapsweep.py` `oversample_test.py` | Per-lever sweeps. ⚠ these score the fc/2 *skirt*, which is polarity-blind — see `METHOD.md` M8. |
| `cu.py` (in `software/script/`) | Run CLI commands non-interactively. |
| `checkdocs.sh` | ⭐ Verify the notes have not drifted. Run it before committing a notes change. |

Decode the committed captures:

```bash
../../software/script/.venv/bin/python mfdemod.py --selftest && ../../software/script/.venv/bin/python mfdemod.py caps/phasebits/tag_p024_r*.bin
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
of 62.5 ns (**12–36 is the working window**; stock 0 fails) · `--input {5,0}` AIN5 or
AIN0/`LF_RSSI` (measured dead) · `--rate N` free-running kHz · `--gain N` divisor ·
`--settle N` ms
