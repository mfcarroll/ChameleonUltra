## Indala on Chameleon Ultra — why it cannot read PSK, and the cheapest way to find out

**Measured on device 2026-09-10/11.** Chameleon Ultra v3, firmware `v2.2 (v2.2.0-32-gccf6075)`,
chip id `a461ebf3b85fb19c`. Reference reads on a Proxmark3 Iceman.

Indala is **PSK1, RF/32, 64 or 224 bits** (proxmark3 `client/src/cmdlfindala.c:17`). The Chameleon
Ultra cannot read it, and no PSK tag of any kind is readable. The firmware gap is certain and
documented below. **Which physical layer causes it is not yet settled** — §4 is one scope probe.

⇒ **Next action: put a scope on TP7. See §5.** Zero firmware work, and it decides the project.

---

### 1. ⛔ RETRACTED: "the ADC samples at exactly Nyquist, therefore fc/2 cancels"

⛔ ~~Root cause is `lf_125khz_radio.c:113`: the SAADC sample task is PPI-triggered from the carrier
PWM's `PWMPERIODEND`, one sample per carrier cycle at fixed phase. A T5577 derives its PSK
subcarrier as fc/2, phase-locked to that same PWM, so recovered amplitude is proportional to
`cos(φ)` for constant φ and the subcarrier cancels deterministically.~~

**Banded 2026-09-11. The Proxmark3 is a direct counterexample.** `fpga/lo_read.v:19` —
*"we are generating the unmodulated low frequency carrier. The A/D samples at that same rate"* —
so the PM3 samples **one sample per carrier cycle at 125kHz, synchronously, exactly like the
Chameleon**, and reads Indala fine. 125kHz synchronous sampling is therefore *sufficient* for
fc/2, and cannot be what distinguishes the two devices.

⚠ **What survives the retraction:** the PPI wiring described is real, and fixed-phase sampling at
exactly Nyquist remains a plausible *secondary* contributor. It is no longer the headline, and
the phase-sweep experiment it motivated is no longer the first thing to try.

⇒ The load-bearing error was reasoning from one device's architecture without checking it against
a device that already does the thing.

### 2. The tag works, and it is a T5577 clone

    pm3 --> lf indala reader
    [+] Indala (len 64)  Raw: a0000000e6bd0e92
    [+] Fmt 26 FC: 52 Card: 63612 Parity: 11

    pm3 --> lf t55xx detect
    [=]  Chip type......... T55x7
    [=]  Modulation........ PSK1
    [=]  Bit rate.......... 2 - RF/32

Energised and readable by other hardware, and re-programmable — the Chameleon can already *write*
it (`lf clone`, `LF_T55XX_WRITE`), just not read it back.

### 3. Three architectures, and what actually differs

| | **Chameleon Ultra** | **Flipper Zero** | **Proxmark3** |
|---|---|---|---|
| front end | LC tank → 1N4148 peak detector → **2 op-amp RC stages** (GS358) | LC tank → straight into MCU comparator | LC tank → amplifier → 8-bit ADC |
| digitiser | SAADC @125kHz, PPI from carrier PWM **or** plain GPIO edge | **COMP1**, ½Vrefint, HIGH hysteresis → **TIM2 input capture** | ADC @125kHz → FPGA `min_max_tracker` adaptive threshold |
| decoder input | `uint16_t` envelope samples | **`(bool level, uint32_t duration)`** | 8-bit buffer → software `PSKDemod` |
| reads Indala | ✗ | ✓ `lib/lfrfid/protocols/protocol_indala26.c` | ✓ `lf indala reader` |

⭐ **The Chameleon is the only one of the three that shapes the signal for the ASK/FSK band before
digitising.** The PM3 digitises wide and sorts it out in FPGA + software; the Flipper digitises
with a hysteretic comparator and never forms a waveform at all. First-order estimate of the
Chameleon's two poles, from schematic netlist values:

| pole | components | corner |
|---|---|---|
| envelope detect | R9 82R / C36 33nF | 58.8 kHz |
| 1st filter | R17 4k7 / C38 1nF | 33.9 kHz |

| subcarrier | estimated attenuation |
|---|---|
| HID fc/10 = 12500Hz | −0.7 dB |
| HID fc/8 = 15625Hz | −1.1 dB |
| **Indala fc/2 = 62500Hz** | **−9.7 dB** |

⚠ **This is the leading hypothesis, not a result.** The corners are first-order RC estimates read
off a netlist text dump, not a traced active-filter response, and −9.7dB alone does not obviously
explain *total* silence. §5 measures it directly instead of arguing about it.

⇒ Not a drive/power problem under any hypothesis: the field is identical for every protocol, and
the same device reads HID Prox through the same chain.

### 4. Measured: the tag produces nothing the Chameleon can see

Three raw 32ms `lf sniff` captures (`caps/`), analysed by `analyse.py`. Control was an HID Prox
card that reads fine.

| capture | residual std | fc/8 vs baseline | fc/10 vs baseline | fc/2 vs baseline |
|---|---|---|---|---|
| baseline (empty field) | 8.56 | 1.00x | 1.00x | 1.00x |
| **indala** | **8.15** | 0.99x | 0.84x | **0.87x** |
| control (HID Prox) | 16.24 | **4.25x** | **3.19x** | 1.02x |

The indala capture is indistinguishable from an empty field — every band 0.84–1.38x, residual std
*below* baseline, fc/2 energy the lowest of the three. The control lights up at 4.25x and 3.19x
through the identical pipeline, so the method plainly sees a responding tag. A coherent PSK1
detector (mix fc/2 to DC with `(-1)^n`, low-pass over half a bit) scored SNR 1.62x on indala
versus **1.71x on an empty field**, and autocorrelation at the 2048-sample (16.4ms) 64-bit frame
period was **−0.057**.

⚠ This measures *the ADC path only*, and cannot distinguish a signal absent at `LF_OA_OUT` from
one present but invisible to this sampler. That is exactly what §5 resolves.

### ⚠ Every capture contains glitches — a baseline is mandatory
All three captures, **including the empty-field baseline**, contain amplitude bursts with
peak-to-peak > 200. These are USB-transfer buffer overruns, not tag signal
(`lf_reader_generic.c:30`, "buffer full — oldest samples dropped"). `analyse.py` drops any
40-sample window swinging more than 60 LSB before measuring. Without an empty-field reference you
cannot tell a burst from a tag.

### 5. ⭐ The decisive measurement: scope TP7

The V1.0 schematic brings `LF_OA_OUT` out to test point **TP7**. That node is the last point in
the analog chain before the MCU, and it is what *both* firmware read paths see.

**Probe TP7, ground to any GND test point, device in reader mode.** If TP7 is hard to locate on
the board, `IC1B` pin 7 (GS358B-FR output) is the same net.

⚠ **Use `./fieldhold.sh 60`, not `lf sniff`.** `lf sniff` drops the field after ~32ms once its
4000-sample buffer fills. `fieldhold.sh` loops a read that is *expected to fail*, and a failing
read holds the field for the full `g_timeout_readem_ms` = 500ms (`lf_reader_main.c:25`) — measured
~77% duty cycle, which is a comfortable scope target.

⭐ **Run the HID Prox control BEFORE the Indala tag.** Its fc/8 at 15.6kHz must show up strongly.
If it does not, the probe is on the wrong net and a null result on Indala would mean nothing.

Settings: **AC coupling** (the net sits on `LF_VBIAS`), 10µs/div to resolve the 16µs subcarrier
period, 200mV/div to start. Use **FFT** if the scope has it — a peak at 62.5kHz is the whole
question. Also sweep out to 50µs/div for the 256µs bit period and 2ms/div for the 16.4ms frame.

⚠ `LF_OA_OUT` is downstream of the VD1 peak detector, so it carries the **envelope**. Seeing
125kHz dominate there would mean the detector is not behaving as this note assumes.

| result at TP7 | meaning | next step |
|---|---|---|
| 62.5kHz present, volts-scale | front end passes it; purely a firmware gap | comparator route, §6 |
| 62.5kHz present but tens of mV | marginal; below GPIO logic threshold but fine for COMP | comparator route with a low reference |
| nothing at 62.5kHz | R17/C38 is eating it | component change, not firmware |

Take the same trace with the **HID Prox control** for scale — its fc/8 at 15.6kHz should be
strong, and the ratio between the two is the measured version of the −9.7dB estimate in §3.

⇒ This is one probe and it forks the whole project. Do it before writing code.

### 6. Porting routes, cheapest first

**⭐ A. Comparator + hardware edge timestamping (the Flipper model).** `LF_OA_OUT` is **P0.29**
(`hw_connect.c:136`) = **AIN5**, a COMP-capable input. The nRF52840 **COMP** peripheral is
entirely unused — LPCOMP is already on the LF path but on AIN0/`LF_RSSI` for field-detect wake
(`lf_tag_em.c:58`), so it is not in the way. COMP → PPI → TIMER CAPTURE gives 16MHz edge
timestamps with **zero CPU per edge**.

⭐ This is strictly better than the Flipper's own implementation. `FLIPPER_HAL_RFID_CAPTURE_FINDINGS.md`
(Momentum `t5577-deep-read`) documents that the Flipper writes its capture origin in software
inside the ISR with slave-mode reset disabled, so interrupt latency contaminates two of three
derivable quantities. A PPI-driven design has that designed out from the start.

**B. Oversample the ADC.** Point the existing PPI at a TIMER at 200–250kHz for 3.2–4 samples per
subcarrier cycle. nRF52840 SAADC is spec'd to 200ksps. A 64-bit frame is 16.4ms = ~4100 samples at
250kHz, over the current `LF_SNIFF_MAX_SAMPLES` of 4000, so decode incrementally through
`decoder.feed()` the way `hidprox_read()` does.

**C. Shift the SAADC sample phase.** ⛔ Demoted from first choice by §1. Still a cheap
discriminator — flat response at *all* phases would independently implicate the filter — but the
scope answers the same question faster.

**Decoder side, any route.** `protocols.h` defines a streaming interface —
`decoder_start(codec, format)` / `decoder_feed(codec, uint16_t sample)` — so an `indala.c` slots
in the way `hidprox.c` does, and `psk1.c` already supplies the modulator for emulation. Two
sources to port from: Flipper's `protocol_indala26.c` consumes `(bool level, uint32_t duration)`
and suits route A directly; Proxmark's `PSKDemod()`/`psk1TOpsk2()` (`common/lfdemod.c`) and
`detectIndala()` (~148 lines) consume a sample buffer and suit route B.

⚠ Route A needs the decoder interface widened — `decoder_feed(void*, uint16_t)` carries one ADC
sample and cannot express `(level, duration)`.

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

Analysis needs `numpy` in `software/script/.venv` (2.5.3 used here).
