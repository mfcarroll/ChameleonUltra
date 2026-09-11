## Indala on Chameleon Ultra — the receive chain loses fc/2 before the ADC

**Measured on device 2026-09-10/11.** Chameleon Ultra v3, firmware `v2.2 (v2.2.0-32-gccf6075)`,
chip id `a461ebf3b85fb19c`. Reference reads on a Proxmark3 Iceman.

Indala is **PSK1, RF/32, 64 or 224 bits** (proxmark3 `client/src/cmdlfindala.c:17`). The Chameleon
Ultra reads no PSK tag of any kind. §7 documents the firmware gap, which is certain. **§0 is why
closing it is not enough.**

### 0. ⭐⭐⭐ RESULT: the loss is analog, and it is 27 dB

A T5577's PSK carrier frequency is a field in block 0, so one tag was swept across three
subcarriers and read by BOTH instruments on identical stimulus (campaign
`campaign_20260910_194209`, `--reader chameleon --pm3-signal 0`).

| PSKCF | subcarrier | smp/cyc | Chameleon | vs empty field | Proxmark |
|---|---|---|---|---|---|
| RF/8 | 15625 Hz | 8.0 | 14.080 | 37.8x | 109.87 |
| RF/4 | 31250 Hz | 4.0 | 0.544 | 15.3x | 44.27 |
| **RF/2** | **62500 Hz** | **2.0** | **0.250** | **6.0x** | **46.15** |

Normalised to each instrument's own RF/8, which divides out the tag's behaviour:

| PSKCF | Proxmark | Chameleon | **excess loss** |
|---|---|---|---|
| RF/4 | −7.9 dB | −28.3 dB | **−20.4 dB** |
| RF/2 | −7.5 dB | −35.0 dB | **−27.5 dB** |

The Proxmark is essentially flat from 31 kHz to 62.5 kHz. ⇒ **The tag emits fc/2 at full strength;
the Chameleon's receive chain removes 27.5 dB of it.** That happens BEFORE digitisation, so no
sample-rate, sample-phase or comparator change can recover it.

⭐ **And it is not a sampling artefact.** RF/4 sits at 4 samples/cycle with no Nyquist problem and
already shows −20.4 dB of excess loss. The attenuation is present well below Nyquist. That
independently kills the retracted §1 hypothesis and its surviving "secondary contributor" caveat.

⚠ **The rolloff shape is not a linear filter.** −28.3 dB in one octave (RF/8→RF/4) then only
−6.7 dB in the next exceeds what any pole count explains, and the netlist model in §3 predicted
−2.6 dB for that first step. Consistent with the VD1 **peak detector**'s discharge time constant
setting a knee rather than an RC pole — but that mechanism is inferred, not measured.

### 0b. ⛔⛔ THE OPEN LEAD, AND IT IS BIG: every number above is 8-BIT

`lf sniff` right-shifts the 14-bit SAADC conversion by five before it leaves the device:

    val = val >> 5;  /* lf_reader_generic.c:59 */

**The protocol decoders do not.** `lf_hidprox_data.c:46` buffers `nrf_saadc_value_t` and hands
`decoder.feed()` the raw value, and `SAADC_CONFIG_RESOLUTION 3` is 14-bit.

⇒ Every measurement in this note — including the original "the tag produces nothing" — came through
a **32x truncation the real decoder path does not have.** 0.250 LSB at fc/2 is roughly **8 counts of
the actual conversion**, which is small but not obviously undemodulable once integrated over the 32
samples per bit an RF/32 frame gives.

⛔ **So "the signal is too small" is NOT established.** Measure through a full-resolution path before
concluding anything about feasibility. That is the next action, and it is a few lines of firmware
(widen or drop the shift in the sniff path), not a hardware change.

### 1. ⛔ RETRACTED: "the ADC samples at exactly Nyquist, therefore fc/2 cancels"

⛔ ~~Root cause is `lf_125khz_radio.c:113`: the SAADC sample task is PPI-triggered from the carrier
PWM's `PWMPERIODEND`, one sample per carrier cycle at fixed phase, so the fc/2 subcarrier — being
phase-locked to that same PWM — cancels deterministically.~~

Banded 2026-09-11 on the Proxmark counterexample (`fpga/lo_read.v:19`: the PM3 samples once per
carrier cycle at 125 kHz too, and reads Indala). **Now fully dead**: §0 shows the loss is already
−20.4 dB at RF/4, four samples per cycle, nowhere near Nyquist. The PPI wiring described is real and
irrelevant.

### 1b. ⛔ RETRACTED: "the Indala tag produces no detectable modulation"

⛔ ~~The indala capture is indistinguishable from an empty field — every band 0.84–1.38x, residual std
below baseline, fc/2 energy the lowest of the three. Total, deterministic silence.~~

Banded 2026-09-11. **fc/2 is visible at 6.0x the empty field** once measured properly. Two separate
errors produced the original claim:

1. **Different tag, different placement.** The original capture was the indala26 credential held by
   hand; §0 used a `spare` blank deliberately placed by the campaign harness.
2. **⭐ The analysis threw the signal away.** `sweep.py` screened out any 40-sample window swinging
   more than 60 LSB, a threshold calibrated on captures where nothing was modulating. A responding
   tag exceeds it everywhere — on PSK1-CF8 it discarded **90 windows of 90**, returned `nan`, and the
   nan propagated into a confident-looking verdict built entirely out of nan. The screen was removing
   the signal, not the noise.

⚠ **The screen is still right in `analyse.py`** and wrong in `sweep.py`: band-energy fractions really
are corrupted by broadband bursts, while a narrowband coherent DFT at one frequency barely notices
them. Same data, different statistic, opposite requirement.

⇒ Lesson worth keeping: a filter tuned on null data will silently delete the first real signal it
sees, and the failure mode is `nan`, not an error.

### 2. The tag, and what it is

    pm3 --> lf indala reader
    [+] Indala (len 64)  Raw: a0000000e6bd0e92
    [+] Fmt 26 FC: 52 Card: 63612 Parity: 11

    pm3 --> lf t55xx detect
    [=]  Chip type......... T55x7      Modulation........ PSK1      Bit rate.......... 2 - RF/32

A T5577 clone, energised and readable by other hardware, and re-programmable — which is what made
the PSKCF sweep possible.

### 3. Three architectures

| | **Chameleon Ultra** | **Flipper Zero** | **Proxmark3** |
|---|---|---|---|
| front end | LC tank → 1N4148 peak detector → 2 op-amp RC stages (GS358) | LC tank → straight into MCU comparator | LC tank → amplifier → 8-bit ADC |
| digitiser | SAADC @125kHz, PPI from carrier PWM **or** GPIO edge | **COMP1**, ½Vrefint, HIGH hysteresis → **TIM2 input capture** | ADC @125kHz → FPGA `min_max_tracker` |
| decoder input | `uint16_t` samples (**8-bit via `lf sniff`, 14-bit via `decoder.feed`**) | `(bool level, uint32_t duration)` | 8-bit buffer → software `PSKDemod` |
| reads Indala | ✗ | ✓ `protocol_indala26.c` | ✓ |

⚠ The netlist-derived poles (R9 82R/C36 33nF = 58.8 kHz; R17 4k7/C38 1nF = 33.9 kHz) predicted
−9.7 dB at fc/2. **Measured excess is −27.5 dB.** The estimate was wrong by a wide margin and the
shape is not a pole cascade; treat §3's RC arithmetic as superseded by §0.

### 4. ⚠ Glitches: real, but do not screen a narrowband measurement
Every capture, including the empty-field baseline, contains USB-transfer overrun bursts
(`lf_reader_generic.c:30`). They matter for `analyse.py`'s band fractions and must be screened
there. They do **not** matter for `sweep.py`'s single-bin DFT, and screening them there destroyed
the measurement — see §1b.

### 5. Reproducing the sweep

    # empty-field baseline, no tag anywhere near the device
    cd software/script && .venv/bin/python cu.py "hw mode -r" \
        "lf sniff --out ../../research/indala-psk-read/caps/baseline.bin"

    # program + PM3 reference + Chameleon capture, three subcarriers
    cd Momentum-Firmware
    <ChameleonUltra>/software/script/.venv/bin/python \
      T5577_block0_analysis_data/t5577_campaign.py \
      --reader chameleon --config PSK1-CF8,PSK1-CF4,PSK1 \
      --silicon spare --reads sniff --repos 1 --pm3-signal 0 --gap flat \
      --pm3 "../proxmark3/client/proxmark3 /dev/tty.usbmodemiceman1" \
      --note "PSKCF sweep"

    ./sweep.py caps/baseline.bin <campaign>/raw/*.bin <campaign>/pm3_signal/*.pm3

⭐ **Pass the `.pm3` files.** Without the Proxmark reference the sweep cannot separate the tag's own
rolloff from the Chameleon's, and that separation is the entire result.

⚠ `--pm3` is needed for a locally built client. Config order matters: ending on `PSK1` restores the
Indala word. At CF4/CF8 the harness will report `block0 IS CONFIRMED ... data blocks unconfirmed` and
default to `[p]` — that is correct for a modulation PM3 cannot read back.

### 6. Routes, re-ranked by §0

⛔ **Dead as primary fixes.** ~~A. comparator + edge timestamping. B. oversample at 200–250 kHz.
C. shift the SAADC sample phase.~~ All three act at or after digitisation, and §0 shows the signal
is already gone before it. A comparator cannot toggle on what the peak detector did not pass.

⭐ **1. Re-measure at full resolution (§0b).** Drop or widen the `>>5` in the sniff path and repeat
the sweep. This is the only step that should happen next: it costs a few lines and it decides
whether anything else is worth doing.

**2. If the signal is adequate at 14 bits** — port the decoder. `protocols.h`'s
`decoder_feed(codec, uint16_t)` already carries full-resolution samples, so an `indala.c` slots in
like `hidprox.c`, and Proxmark's `PSKDemod()`/`detectIndala()` port directly.

**3. If it is not** — the fix is analog: the peak-detector time constant and/or the filter poles.
A component change, out of scope for firmware.

### 7. Firmware state today

- Indala is a commented-out placeholder under `//////// PSK Tag-Talk-First 300` in
  `tag_base_type.h:61`, alongside Keri and NexWatch.
- Every LF reader in `reader/lf/` is ASK or FSK. No PSK demodulator exists. The only PSK source,
  `nfctag/lf/utils/psk1.c`, exports just `lf_psk1_build_sequence()` — **transmit** only, used by
  `protocols/idteck.c` for emulation.
- IDTECK, the one PSK1 type, has `write` and `econfig` but **no `read`**, and there is no
  `IDTECK_SCAN` opcode.
- Two receive architectures already coexist: **GPIO edge timing** via `LF_OA_OUT` (em410x, viking,
  jablotron, em4x05) and **SAADC sampling** (hidprox, ioprox, pac).
- No generic LF identify opcode exists, so the GUI's "generic LF read" can only be rotating the
  seven per-protocol scans.

### 8. Not tested / open

- **⭐ Whether 62.5kHz reaches `LF_OA_OUT` at all.** The one question that matters. §5.
- Whether `LF_OA_OUT` crosses the **GPIO logic threshold** at fc/2 (decides plain GPIOTE vs COMP
  with a tuned reference; COMP's programmable threshold and hysteresis make it the safer bet).
- The RC corners in §3 are netlist estimates, not a traced active-filter response.
- Whether sample phase contributes at all, now that §1 is retracted as the primary cause.
- **Keri and NexWatch**, the other two PSK placeholders — same blocker, same fix.
- Only one tag was tested, at one antenna position.

### 9. Reproducing

    ./grab.sh        # prompts through baseline / indala / control
    ../../software/script/.venv/bin/python analyse.py caps/*.bin

`grab.sh` drives the CLI through `software/script/cu.py`, added on this branch: the stock CLI has
no non-interactive mode (`chameleon_cli_main.py` calls `startCLI()` unconditionally), so `cu.py`
reuses its `exec_cmd()` the way `tests/test_ultra.py` does.

    cd software/script && .venv/bin/python cu.py "hw version" "lf em 410x read"

**One interpreter runs both halves.** ⛔ ~~Two interpreters, one per half — the toolchain python
has pyserial but no numpy, the venv has numpy but no pyserial.~~ Banded 2026-09-11: `pyserial` was
simply installed into the Chameleon venv, and the harness runs fine on the newer interpreter.

    ChameleonUltra/software/script/.venv/bin/python    # 3.14.7, numpy 2.5.3 + pyserial 3.5

Verified under 3.14.7: `py_compile` of `t5577_campaign.py` clean, `--list-configs` clean, and a
full `--dry-run --config PSK1 --pm3-signal 0` walking program → PM3 reference → read → re-verify.

⇒ Do not install numpy into `Momentum-Firmware/toolchain/` to achieve the same thing. That is a
build toolchain; add to the venv instead.

⭐ **This lowers the cost of a `--reader chameleon` backend considerably.** With one interpreter,
the seam is a single step in the campaign loop — the dry run renders it as

    [Flipper] move the 'spare' tag to the Flipper LF antenna, then Enter...
        -> rfid t5577 nativeadc   (reposition 1/1, timeout 60s)

Everything around it (config stepping, PM3 program + verify + reference capture, repositions, gap
stamping, `manifest.json`) is reader-agnostic already. A Chameleon leg replaces that one
`run_cmd(port, ...)` with a shell-out to `cu.py "lf sniff --out <capture path>"`.
