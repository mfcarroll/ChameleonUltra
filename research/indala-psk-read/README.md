## Indala on Chameleon Ultra — the sampler, not the front end

**Measured on device 2026-09-10/11.** Chameleon Ultra v3, firmware `v2.2 (v2.2.0-32-gccf6075)`,
chip id `a461ebf3b85fb19c`. Reference reads on a Proxmark3 Iceman.

Indala is **PSK1, RF/32, 64 or 224 bits** (proxmark3 `client/src/cmdlfindala.c:17`). The Chameleon
Ultra reads no PSK tag of any kind. §7 documents the firmware gap, which is certain. **§0 is what
else stands in the way, and how much of that is fixable in firmware.**

### 0. ⭐⭐⭐ RESULT AT FULL RESOLUTION: the front end is fine; the SAMPLER is the blocker

Measured at 14 bits (`lf sniff --bits 16`, firmware `v2.2.0-43-g69a3d54`) against the
Proxmark on identical stimulus, campaign `campaign_20260910_202715`. Normalised to each
instrument's own RF/8, which divides out the tag's own rolloff:

| PSKCF | subcarrier | smp/cyc | Proxmark | Chameleon | **excess loss** |
|---|---|---|---|---|---|
| RF/4 | 31250 Hz | 4.0 | −4.1 dB | −3.6 dB | **+0.5 dB** |
| RF/2 | 62500 Hz | **2.0** | −9.3 dB | −43.5 dB | **−34.2 dB** |

⭐⭐ **The Chameleon tracks the Proxmark exactly at 31 kHz, then falls off a cliff at
62.5 kHz.** That shape is the whole finding, and it indicts the sampler:

- A filter that is **flat at 31 kHz cannot lose 34 dB by 62.5 kHz** — one octave — without
  something like six poles. This chain is a diode detector and two RC op-amp stages.
- **RF/2 is exactly 2 samples per cycle**, and the SAADC is PPI-triggered from the very PWM
  that generates the field (`lf_125khz_radio.c:113`), so the sampling phase φ is a constant.
  Recovered amplitude goes as `cos φ`, which can null a tone to **any** depth.
- RF/4 is 4 samples/cycle, where no such degeneracy exists — and there the loss is zero.

⇒ **This reverses §1, which I retracted twice.** The Nyquist mechanism was right; the earlier
8-bit data could not see it because truncation was manufacturing apparent loss at RF/4 too
(it read −6.7 dB there, against +0.5 dB measured properly), which made the rolloff look
progressive and analog. At full resolution the loss is confined to exactly the frequency
where sampling degenerates.

⚠ **Still an inference, and a directly testable one.** Sweep the SAADC sample phase, or
sample at 200–250 kHz so fc/2 is no longer at Nyquist. If fc/2 reappears, it is settled.

### 0b. ✅ RESOLVED: the 8-bit truncation

`lf_reader_generic.c:59` right-shifted the 14-bit conversion by five, discarding ~30 dB of
dynamic range on the sniff path only — the protocol decoders always received the full value
(`lf_hidprox_data.c:46`).

⭐ Fixed and flashed. `lf sniff --bits 16` returns the full conversion, 2 bytes/sample
big-endian; 8-bit remains the default so existing hosts and captures are unaffected. Costs
half the duration — the 4000-byte frame limit counts bytes, so 2000 samples / 16 ms.

⇒ It mattered more than expected: **it was not merely hiding signal, it was distorting the
shape of the rolloff** and pointing the diagnosis at the wrong subsystem.

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

⭐ **1. Sweep the SAADC sample phase, or oversample.** §0 makes this the decisive experiment,
not a fallback. Drive `NRF_SAADC_TASK_SAMPLE` from a spare TIMER COMPARE with a programmable
offset instead of `PWMPERIODEND` (62.5 ns resolution ≈ 1.4° of subcarrier phase), or run that
timer at 200–250 kHz so fc/2 stops being Nyquist. If fc/2 reappears, the diagnosis is proved
and the rest follows.

**2. Port the decoder.** `decoder_feed(codec, uint16_t)` already carries full-resolution
samples, so an `indala.c` slots in like `hidprox.c`; Proxmark's `PSKDemod()`/`detectIndala()`
port directly. ⚠ At 250 kHz a 64-bit frame is ~4100 samples, over `LF_SNIFF_MAX_SAMPLES`, so
decode incrementally through `decoder.feed()` the way `hidprox_read()` does.

⛔ **3. Analog change.** ~~The fix is the peak-detector time constant and/or the filter poles.~~
Struck: §0 measures the front end as **flat to 31 kHz with zero excess loss**. There is nothing
to fix there until an experiment says otherwise.

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
