## Indala on Chameleon Ultra — a firmware limit, not a hardware one

**Measured on device 2026-09-10/11.** Chameleon Ultra v3, firmware `v2.2 (v2.2.0-32-gccf6075)`,
chip id `a461ebf3b85fb19c`. Reference reads on a Proxmark3 Iceman.

Indala is **PSK1, RF/32, 64 or 224 bits** (proxmark3 `client/src/cmdlfindala.c:17`). The
Chameleon Ultra cannot read it today. The interesting part is **why**: the analog front end
passes the signal fine, and the blocker is a two-line PPI assignment in the sample clock.

⇒ **This is portable.** See *Porting routes* below.

### The tag works, and it is a T5577 clone

    pm3 --> lf indala reader
    [+] Indala (len 64)  Raw: a0000000e6bd0e92
    [+] Fmt 26 FC: 52 Card: 63612 Parity: 11

    pm3 --> lf t55xx detect
    [=]  Chip type......... T55x7
    [=]  Modulation........ PSK1
    [=]  Bit rate.......... 2 - RF/32

So the tag is energised and readable by other hardware, and it is re-programmable — the
Chameleon can already *write* it (`lf clone`, `LF_T55XX_WRITE`), just not read it back.

### Root cause: the ADC samples at exactly Nyquist, at a fixed phase

`lf_125khz_radio.c:113` wires the ADC sample trigger straight to the carrier PWM:

    nrfx_ppi_channel_assign(
        m_pwm_saadc_sample_ppi_channel,
        nrfx_pwm_event_address_get(&m_pwm, NRF_PWM_EVENT_PWMPERIODEND),
        nrf_saadc_task_address_get(NRF_SAADC_TASK_SAMPLE));

One sample per carrier cycle — 125kHz — **at the same phase of every cycle**. Now note that
a T5577 derives its PSK subcarrier by dividing the field carrier by two. That subcarrier is
therefore at exactly fc/2 = 62.5kHz **and phase-locked to the very PWM that clocks the ADC**.

Sampling a 62.5kHz tone at exactly 125kHz recovers an amplitude proportional to `cos(φ)`,
where φ is the fixed sampling phase offset. φ is a constant set by circuit delay, so:

⚠ **The cancellation is deterministic, not noisy.** If φ lands near 90° the subcarrier is
invisible no matter how strong it is, on every read, forever. Averaging, longer captures and
a better antenna position all change nothing. This matches the measurement exactly — see below.

### The analog front end is *not* the blocker

From `hardware/ultra/Chameleon_nrf52_ultra_V1.0.pdf`, the LF receive chain is a 1N4148 peak
detector into two RC-shaped op-amp stages (GS358). First-order estimate from the RC values:

| pole | components | corner |
|---|---|---|
| envelope detect | R9 82R / C36 33nF | 58.8 kHz |
| 1st filter | R17 4k7 / C38 1nF | 33.9 kHz |

| subcarrier | attenuation |
|---|---|
| HID fc/10 = 12500Hz | −0.7 dB |
| HID fc/8 = 15625Hz | −1.1 dB |
| **Indala fc/2 = 62500Hz** | **−9.7 dB** |

Indala's subcarrier arrives roughly **8.6 dB weaker than HID's — attenuated, but present**.
The chain is tuned for the 12–16kHz ASK/FSK band, not walled off above it. Nothing here
justifies "the hardware can't see Indala".

⇒ Not a drive/power problem either: the field is identical for every protocol, and the same
device reads HID Prox through the same chain.

### Measured: consistent with sample-phase cancellation

Three raw 32ms `lf sniff` captures (`caps/`), analysed by `analyse.py`. Control was an HID
Prox card that reads fine.

| capture | residual std | fc/8 vs baseline | fc/10 vs baseline | fc/2 vs baseline |
|---|---|---|---|---|
| baseline (empty field) | 8.56 | 1.00x | 1.00x | 1.00x |
| **indala** | **8.15** | 0.99x | 0.84x | **0.87x** |
| control (HID Prox) | 16.24 | **4.25x** | **3.19x** | 1.02x |

The indala capture is indistinguishable from an empty field — every band 0.84–1.38x, residual
std *below* baseline, and fc/2 energy the lowest of the three. Meanwhile the control lights up
at 4.25x and 3.19x through the identical pipeline, so the method plainly sees a responding tag.
A coherent PSK1 detector (mix fc/2 to DC with `(-1)^n`, low-pass over half a bit) scored SNR
1.62x on indala versus **1.71x on the empty field**, and autocorrelation at the 2048-sample
(16.4ms) 64-bit frame period was **−0.057**. Total, deterministic silence — exactly what
`cos(φ) ≈ 0` predicts, and *not* what mere attenuation would look like.

### ⚠ Every capture contains glitches — a baseline is mandatory
All three captures, **including the empty-field baseline**, contain amplitude bursts with
peak-to-peak > 200. These are USB-transfer buffer overruns, not tag signal
(`lf_reader_generic.c:30`, "buffer full — oldest samples dropped"). Mistaking them for
modulation is the easy error here; `analyse.py` drops any 40-sample window swinging more than
60 LSB before measuring. Without an empty-field reference you cannot tell a burst from a tag.

### Firmware state today

- Indala is a commented-out placeholder under `//////// PSK Tag-Talk-First 300` in
  `tag_base_type.h:61`, alongside Keri and NexWatch.
- Every LF reader in `reader/lf/` is ASK or FSK. There is no PSK demodulator. The only PSK
  source, `nfctag/lf/utils/psk1.c`, exports just `lf_psk1_build_sequence()` — **transmit**
  only, used by `protocols/idteck.c` for emulation.
- IDTECK, the one PSK1 type, has `write` and `econfig` but **no `read`**, and there is no
  `IDTECK_SCAN` opcode.
- Two receive architectures already coexist: **GPIO edge timing** via `LF_OA_OUT`
  (em410x, viking, jablotron, em4x05) and **SAADC sampling** (hidprox, ioprox, pac).
- There is no generic LF identify opcode at all, so the GUI's "generic LF read" can only be
  rotating the seven per-protocol scans.

### Porting routes

The decoder side is the easy half. `protocols.h` defines a streaming interface —
`decoder_start(codec, format)` / `decoder_feed(codec, uint16_t sample)` — so an `indala.c`
slots in exactly the way `hidprox.c` does, and `psk1.c` already supplies the modulator for
emulation. Proxmark's `PSKDemod()` / `psk1TOpsk2()` (`common/lfdemod.c`) and `detectIndala()`
(~148 lines, `cmdlfindala.c`) are plain C over a sample buffer and port directly.

Getting a usable signal to that decoder is the real work. In rough order of effort:

**A. Shift the sample phase (smallest possible change, best first experiment).**
Keep 125kHz, but move φ off the cancellation null. Drive `NRF_SAADC_TASK_SAMPLE` from a
spare TIMER's COMPARE running at 125kHz with a programmable offset, instead of from
`PWMPERIODEND`. At 16MHz the offset resolution is 62.5ns, i.e. ~1.4° of subcarrier phase.
**Diagnostic value:** sweep φ across 0–360° and re-run `analyse.py`. If fc/2 energy rises and
falls as a cosine, sample-phase cancellation is confirmed and the whole diagnosis holds.

**B. Oversample (robust fix).** Point the same PPI at a TIMER at 200–250kHz for 3.2–4 samples
per subcarrier cycle — full I/Q recovery, no phase sensitivity at all. nRF52840 SAADC is
spec'd to 200ksps. Note a 64-bit frame is 16.4ms = ~4100 samples at 250kHz, over the current
`LF_SNIFF_MAX_SAMPLES` of 4000, so decode incrementally through `decoder.feed()` the way
`hidprox_read()` does rather than buffering a whole frame.

**C. GPIOTE edge timing.** `LF_OA_OUT` already feeds GPIOTE, and `lf_reader_data.c` already
captures a timer on it. A 62.5kHz square gives an edge every 8µs; phase reversals show up as
a stretched interval. Hardware timestamping via PPI → TIMER CAPTURE is phase-immune and cheap,
but 125k edges/s is ~512 CPU cycles per edge — the tightest of the three.

⇒ Start with **A**: it is a handful of lines, and it either confirms the diagnosis outright or
falsifies it cleanly.

### Not tested / open
- **Whether φ is actually near the null.** This is the one load-bearing inference in the
  diagnosis. Route A's phase sweep settles it directly. Everything else here is measured.
- **Whether −9.7dB leaves enough amplitude** once φ is corrected, and whether `LF_OA_OUT`
  still crosses the GPIO logic threshold at fc/2 (matters for route C, not for A or B).
- The RC corners are first-order estimates from netlist values, not a traced active-filter
  response.
- **Keri and NexWatch**, the other two PSK placeholders — same blocker, same fix.
- Only one tag was tested, at one antenna position.

### Reproducing

    ./grab.sh        # prompts through baseline / indala / control
    ../../software/script/.venv/bin/python analyse.py caps/*.bin

`grab.sh` drives the CLI through `software/script/cu.py`, added on this branch: the stock CLI
has no non-interactive mode (`chameleon_cli_main.py` calls `startCLI()` unconditionally), so
`cu.py` reuses its `exec_cmd()` the way `tests/test_ultra.py` does.

    cd software/script && .venv/bin/python cu.py "hw version" "lf em 410x read"

Analysis needs `numpy` in `software/script/.venv` (2.5.3 used here).
