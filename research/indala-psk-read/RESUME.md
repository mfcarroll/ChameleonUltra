# Resume prompt — Indala PSK read on Chameleon Ultra

*Paste this whole file into a fresh session. It is self-contained.*

---

## Task

Continue the investigation into reading Indala (PSK1, RF/32, fc/2 = 62.5 kHz subcarrier) on
a Chameleon Ultra. Work item 1 of `research/indala-psk-read/NEXT.md` unless I say otherwise.

**Read first, in this order:** `research/indala-psk-read/NEXT.md` (ranked next steps and the
method rules), then `SUMMARY.md`. `README.md` has the full working with every retraction
banded in place — consult it, don't read it front to back. `ADVERSARIAL.md` is a hostile
review prompt if you want to attack the conclusions instead of extending them.

## Where things stand, in five lines

- The **channel is measured and trustworthy**: fc/2 arrives **31.2 dB** below the Proxmark
  on identical stimulus; ~2.8 dB of that is recoverable by ADC sample phase.
- The **demodulation gap is 7.6 dB**, confirmed twice independently — spectral band SNR
  (2.30x measured vs 5.50x needed), and a ~19% bit-error rate implying ~7.7 dB.
- The signal **is detectable but not decodable**: 52/64 bits vs a 45/64 white-noise null.
- ⛔ **It is NOT established that 7.6 dB is unbridgeable.** Only sample phase and gain's
  confound test were ever measured under fully valid conditions.
- Everything is on branch `indala-psk-read` (ChameleonUltra) and `t5577-deep-read`
  (Momentum-Firmware), both pushed to `origin` = the user's own fork.

## The immediate next step

`LF_RSSI` (AIN0) taps `LF_OA` — the raw peak-detector output, **upstream of both filter
poles**. Every capture in this project sampled AIN5, downstream of both. If the deficit lives
in the filter stages, this is the only node upstream of them that reaches a pin.

In `firmware/application/src/ble_main.c`, `register_lf_adc_callback()`, change
`NRF_SAADC_INPUT_AIN5` to AIN0, rebuild, flash, and take paired empty/tag captures at the
optimal phase (32 ticks) to compare fc/2 against AIN5. `NEXT.md` §1 has the caveats — 470k
source impedance is the likely killer, which is why it needs measuring rather than arguing.

## Environment

| | |
|---|---|
| repo | `/Users/Shared/code/personal/rfid/ChameleonUltra` (branch `indala-psk-read`) |
| harness | `/Users/Shared/code/personal/rfid/Momentum-Firmware` (branch `t5577-deep-read`) |
| Proxmark | `/Users/Shared/code/personal/rfid/proxmark3`, device `/dev/tty.usbmodemiceman1` |
| python | `ChameleonUltra/software/script/.venv/bin/python` — numpy + pyserial, runs everything |
| nrfutil | `/Users/Shared/code/personal/rfid/.tools/bin/nrfutil` (8.2.1) |

⚠ If `nrfutil` is missing, reinstall it Nordic's way — **not** via Homebrew, whose cask is
disabled for a Gatekeeper failure. `curl` sets no quarantine attribute, so there is no
Gatekeeper prompt at all:

```bash
mkdir -p /Users/Shared/code/personal/rfid/.tools/bin && curl -sL -o /Users/Shared/code/personal/rfid/.tools/bin/nrfutil https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/aarch64-apple-darwin/nrfutil && chmod 755 /Users/Shared/code/personal/rfid/.tools/bin/nrfutil && PATH=/Users/Shared/code/personal/rfid/.tools/bin:$PATH nrfutil install device nrf5sdk-tools
```

## Commands that work

Build + flash (needs Docker running; `open -a Docker` first):

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/firmware && PATH=/Users/Shared/code/personal/rfid/.tools/bin:$PATH ./flash-dfu-app-macos.sh
```

Run CLI commands non-interactively (the stock client has no batch mode):

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/software/script && .venv/bin/python cu.py "hw version" "hw mode -r" "lf sniff --timeout 500 --bits 16 --phase 32 --out /tmp/x.bin"
```

`lf sniff` flags added by this work: `--bits 16` (full 14-bit, not `>>5`), `--phase N`
(0–127 ticks of 62.5 ns; **32 is the measured optimum**), `--rate N` (free-running kHz),
`--gain N` (divisor), `--settle N` (ms). A 16-bit capture returns 4096 samples = 2 Indala
frames, transferred in chunks.

Demodulate a capture:

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read && ../../software/script/.venv/bin/python mfdemod.py --selftest && ../../software/script/.venv/bin/python mfdemod.py /tmp/x.bin
```

## ⚠ Tag state — check this before trusting any measurement

The bench T5577 must carry a **real Indala credential**, not the campaign's blank payload.
Confirm with the Proxmark, and expect exactly this:

```bash
cd /Users/Shared/code/personal/rfid/proxmark3 && ./pm3 -c "lf indala reader"
```

→ `Indala (len 64) Raw: a0000000e6bd0e92`, `Fmt 26 FC: 52 Card: 63612`.

⛔ The campaign registry's `PSK1` cell writes `DEADBEEF/12345678` — correct PSK1 **air
shaping**, but not an Indala **frame**. Use the `INDALA26` cell for anything demod-related.
Misreading this invalidated a day of work.

## Method rules this project paid for — violating any of these has already cost a day

1. ⛔ **A single capture is not evidence.** Pre-fix, 41% of captures carried a field dropout
   and six consecutive captures at one setting spread **49x**. Use `--repeats`, deglitch,
   take medians, and watch the spread column.
2. ⛔ **Always run the null.** "51–54/64 bits" looked like strong detection until white noise
   scored 45/64 through the same procedure.
3. ⛔ **Confirm tag state, and understand what the confirmation says.** The harness reported
   `data blocks UNVERIFIED` and named `DEADBEEF/12345678` every run; it was read as "PM3
   cannot see them" rather than "they are not what you want".
4. ⛔ **A synthetic threshold is not a real threshold.** The matched filter's +8.2 dB held
   against white noise and vanished on real captures.
5. ⛔ **No hard-coded thresholds in verdicts.** `max ratio > 3?` got 2.90 and printed a
   conclusion its own table contradicted. Fit the shape, report the fit.
6. ⛔ **Check a measure is comparable before comparing it.** Band RMS was compared across
   bands whose noise floors differ 17x.
7. ⛔ **Don't write `<placeholder>` in a shell command** — zsh reads `<` as a redirect.

⇒ Ten conclusions in this project were wrong, every one from an artefact in the analysis
rather than the hardware. Retract in place (band the old claim, don't delete it) as
`README.md` does.

## Don't re-litigate

The firmware has no PSK demodulator (`reader/lf/*_data.c` are all ASK or FSK; `psk1.c` is
transmit-only). The device reads HID Prox, ioProx and EM410x fine. The tag works — the
Proxmark reads it. Coherent frame averaging is measured and dead. None of that is in
question.
