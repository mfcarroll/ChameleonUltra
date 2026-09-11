# Resume prompt — Indala PSK read on Chameleon Ultra

*Paste this whole file into a fresh session. It is self-contained.*

---

## Task

Indala on the Chameleon Ultra. ⭐⭐⭐ **It reads.** Work item 1 of
`research/indala-psk-read/NEXT.md` — port the decoder into firmware — unless I say otherwise.

**Read first, in this order:** `research/indala-psk-read/NEXT.md`, then `SUMMARY.md`.
`README.md` §0! has the result and §0!b the retraction table — consult it, don't read it
front to back.

## Where things stand, in five lines

- ⭐ **43 of 160 single 300ms captures decode `a0000000e6bd0e92` EXACTLY.** 5/5 at ticks
  12, 20 and 36; working window ticks 4–60. Empty field: **0 hits in 160**. No stacking,
  no folding, stock 8-bit sample width.
- ⛔ **The old "31.2 dB / 7.6 dB short" conclusion is retracted.** `mfdemod.py` was
  demodulating **PSK2 against a PSK1 tag** — in PSK1 the phase IS the data. `synth()`
  encoded with the same wrong convention, so the self-test was self-consistent and passed
  forever.
- Three things are load-bearing, each takes it to zero alone: the PSK1 mapping; the
  baseband low-pass (32/35 vs **0/35**); and **not** discarding the 400-sample settle
  window (43/160 vs **0/160**).
- Still standing: `LF_RSSI`/AIN0 dead, gain floor analog-referred, and the three real
  firmware bugs fixed (`>>5` truncation, BLE collapsing the field, DMA ring dropping 75%).
- Everything is on branch `indala-psk-read` (ChameleonUltra) and `t5577-deep-read`
  (Momentum-Firmware), both pushed to `origin` = the user's own fork.

## The immediate next step

Port the decoder to firmware as `lf indala read`. The whole read is: sample at 125kHz at a
phase in the working window → mix by `(-1)^n` → low-pass → 32-sample boxcar per bit →
threshold → search `preamble64` → read 64 bits. Integer arithmetic over 4096 samples; no
float, no FFT, and the low-pass can be a short FIR or a two-stage boxcar.

⚠ The stock trigger is phase 0, which sits at the edge of the window and decodes 0/5 — so
stock firmware would fail even with a correct decoder. Check the window on a second tag
before hard-coding a phase. `NEXT.md` §1-2.

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

`lf sniff` flags added by this work: `--input {5,0}` (5 = AIN5/LF_OA_OUT stock, 0 =
AIN0/LF_RSSI — measured dead, see `NEXT.md` §5), `--bits 16` (full 14-bit, not `>>5`), `--phase N`
(0–127 ticks of 62.5 ns; **32 is the measured optimum**), `--rate N` (free-running kHz),
`--gain N` (divisor), `--settle N` (ms). A 16-bit capture returns 4096 samples = 2 Indala
frames, transferred in chunks.

Demodulate a capture:

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read && cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read && ../../software/script/.venv/bin/python mfdemod.py --selftest && ../../software/script/.venv/bin/python mfdemod.py caps/phasebits/tag_p024_r*.bin
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
