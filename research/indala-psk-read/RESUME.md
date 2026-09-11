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

- ⭐ **It is NOT SNR.** A synthetic PSK1 frame injected into the REAL measured empty-field
  noise, at HALF the tag's own fc/2 amplitude, decodes at 0/31 credential bits from ONE
  capture. The real tag, at 4x the band SNR after stacking, gets 7/31 and never improves.
- ⇒ **The amplitude is there and the phase is not.** The question is "where does the
  polarity go?", not "how do we find 7.6 dB". `README.md` §0z.
- ⛔ Three old numbers are **retracted** (§0z2): folding at 2048 samples cancels the data
  (19 ones = odd parity, so the true period is 4096); the fc/2 band-SNR criterion is
  polarity-blind; and "52/64 bits vs a 45/64 null" was a constant preamble run scoring
  itself — on the 31 credential bits the tag gets 6/31 and the **null gets 4/31**.
- What holds: zero-offset stacking (+7.3 dB, all controls pass), the frame visible in the
  sideband envelope, and `LF_RSSI`/AIN0 **closed** (flat to 0.5 dB, alive but no bandwidth).
- Everything is on branch `indala-psk-read` (ChameleonUltra) and `t5577-deep-read`
  (Momentum-Firmware), both pushed to `origin` = the user's own fork.

## The immediate next step

**Sweep sample phase while scoring BIT RECOVERY, not sideband amplitude.** The 32-tick
optimum was found by maximising the skirt, and the skirt is transition energy —
polarity-blind. The polarity lives at 62.5 kHz = Nyquist, recovered as `2A·cos φ`, which
has a hard null the skirt does not. They have no reason to share an optimum, and 32 ticks
may sit at or near the polarity null. That one possibility explains every observation:
full skirt amplitude, frame structure visible in the envelope, and no recoverable sign.

Modify `phasesweep.py` to score `stack.py`'s data-bit errors at each phase, sweep all 128
ticks with the tag on, and take paired empty captures — the null lands around 4–5 errors
and is what makes a low count mean anything. `NEXT.md` §1.

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
cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read && ../../software/script/.venv/bin/python mfdemod.py --selftest && ../../software/script/.venv/bin/python stack.py --dir caps/inputtest
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
