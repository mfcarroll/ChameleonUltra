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

### 5. ⭐ The decisive measurement: sweep the T5577's PSK carrier

**No instrument required.** The tag under test is a T5577, and its PSK carrier frequency is a
field in block 0 — `PSKCF` (`proxmark3 include/protocols.h:818`). The Indala config decodes
exactly as

    0x00081040 = T55x7_BITRATE_RF_32 | T55x7_MODULATION_PSK1 | T55x7_PSKCF_RF_2 | (2 << 5)

so **one tag can emit a clean continuous subcarrier at three frequencies**, changing only block 0:

| PSKCF | block 0 | subcarrier | samples/cycle | netlist model |
|---|---|---|---|---|
| RF/8 | `00081840` | 15625 Hz | 8.0 | −1.1 dB |
| RF/4 | `00081440` | 31250 Hz | 4.0 | −3.8 dB |
| RF/2 | `00081040` | 62500 Hz | **2.0 — Nyquist** | −9.7 dB |

⭐ **Why this beats a scope.** RF/8 and RF/4 sit at 8 and 4 samples per cycle, free of any
sampling artefact, so they measure the **analog filter alone**. RF/2 sits at exactly Nyquist,
where fixed-phase PPI sampling could add a penalty of its own. Fit the filter on the two clean
points, extrapolate to 62.5kHz, and compare against the measured RF/2:

- measured RF/2 ≈ extrapolation ⇒ **front-end rolloff is the whole story** → hardware change
- measured RF/2 ≪ extrapolation ⇒ **sampling is adding a penalty** → §1's retracted mechanism
  returns as a real secondary effect, and route B or C fixes it in firmware

A scope at TP7 cannot separate those two, because it never sees the sampler. Sweeping one tag
also controls for modulation depth, which comparing across two different tags does not.

**Scale from the existing captures**, for calibration:

| signal | amplitude | noise | SNR |
|---|---|---|---|
| HID control @ fc/8 15625Hz | 2.06 LSB | 0.31 | **6.67x** |
| HID control @ fc/10 12500Hz | 1.36 LSB | 0.25 | **5.36x** |
| indala tag @ fc/2 62500Hz | **0.06 LSB** | 0.09 | **0.69x** |

⚠ 0.06 LSB is sub-quantisation — there is nothing there at all. Note the netlist model predicts
only −8.6dB (2.7x) between those two frequencies, while the measured gap is ~34x. Suggestive, but
**not conclusive across two different tags** with different modulation depths. That is precisely
what the single-tag sweep settles.

⭐ **Capture the PM3's view at every step too — it is not optional.** A null at RF/2 on the
Chameleon means nothing unless something confirms the tag actually emitted at that subcarrier.
Whether a given T5577 honours all three `PSKCF` values is itself unmeasured here. The Proxmark is
the reference receiver, and its capture turns each step into a controlled comparison:

| | RF/8 | RF/4 | RF/2 |
|---|---|---|---|
| PM3 sees subcarrier | reference | reference | **reference** |
| Chameleon sees it | expect yes | ? | ? |

If the PM3 sees RF/2 and the Chameleon does not, the tag is fine and the Chameleon's chain is the
subject. If neither sees it, suspect the write or the silicon, not the Chameleon.

**Reuse the existing campaign harness, do not rebuild it.**
`T5577_block0_analysis_data/t5577_campaign.py` (Momentum `t5577-deep-read`) already does the risky
half: `program_config()` writes and *verifies* block 0 against `expect_block0`, with restore files,
and `--pm3-signal` runs `lf config; lf t55xx read -b N; data save -f FILE` for exactly the reference
capture above. Its registry already carries `PSK1` at `00081040` — the Indala word.

⚠ **What it does not have is a Chameleon reader leg.** `--reads` runs *Flipper CLI commands over a
serial port* (`open_port`/`run_cmd`/`flip_read_command`), and the Chameleon speaks a framed protocol
instead, so it needs a shell-out to `cu.py` rather than a port write. That is the one seam. For a
three-point sweep it is not worth building: add the configs, let the harness program and take the
PM3 reference, and capture the Chameleon side with `cu.py` by hand. Build a `--reader chameleon`
backend only if the full matrix (PSKCF x rate x gap x repos) turns out to be worth running.

⭐ **Applied 2026-09-11**: `PSK1-CF4`/`PSK1-CF8` are in the registry and `--reader chameleon` is in
the harness (Momentum `t5577-deep-read`). `campaign-configs.py.snippet` is kept only as the record of
what was proposed. Chameleon runs default their corpus to
`<ChameleonUltra>/research/campaigns/campaign_<stamp>/` — not the Momentum T5577 tree, where the
offline T5577 harnesses glob — and `--out-dir` overrides.

⚠ **Four bugs were found running it, two of them pre-existing and reader-independent.** Recorded here
because they bite any campaign, not just this one:

| bug | whose | symptom |
|---|---|---|
| capture named *after* the read | chameleon leg | a stray `.chameleon_capture_tmp.bin` left in `raw/`, belonging to no step |
| `out_dir` derived before `--chameleon-cli` resolved | chameleon leg | `TypeError: NoneType` on every run |
| `prog` initialised inside `if not args.no_pm3:` | **pre-existing** | every `--no-pm3` run dies `UnboundLocalError` *after* taking the read — capture lost |
| verify verdict says `MISMATCH` on a dump-only failure | **pre-existing** | printed `block0=00081440 -> !! MISMATCH (expected block0 00081440)`, a value mismatching itself; cost an aborted campaign |

⇒ The last one is the one to know about: on any modulation PM3 cannot read back, a **successful** write
reports as a mismatch against itself. `res["ok"]` was left untouched — only the wording is now true.

**Procedure.** One command now drives program → PM3 reference → Chameleon capture, three times:

    cd Momentum-Firmware
    <ChameleonUltra>/software/script/.venv/bin/python \
      T5577_block0_analysis_data/t5577_campaign.py \
      --reader chameleon --config PSK1-CF8,PSK1-CF4,PSK1 \
      --silicon spare --reads sniff --repos 1 --pm3-signal 0 --gap flat \
      --pm3 "../proxmark3/client/proxmark3 /dev/tty.usbmodemiceman1" \
      --note "PSKCF sweep: Chameleon LF front-end rolloff"

    ./sweep.py caps/baseline.bin <campaign>/raw/*.bin

⚠ `--pm3` is needed whenever the Proxmark client is not the one on `PATH` — e.g. a locally built
client, which is the normal case here. ⭐ Config order matters: ending on `PSK1` leaves the tag back
at the Indala word.

`sweep.py` reads both the campaign filenames (`s01_PSK1-CF8_..._sniff_r1.bin`) and hand-taken
`psk_rf8.bin` names. An empty-field capture with `baseline` in its name is still required and the
harness does not produce one — take it separately with `./grab.sh`.

**Manual equivalent**, if the harness is not wanted (only block 0 changes; data blocks untouched):

    # capture a fresh empty-field baseline first
    ./grab.sh                                    # or just the baseline leg

    lf t55xx write -b 0 -d 00081840              # PSKCF RF/8  -> 15625 Hz
    #   then on the Chameleon:
    #   cd ../../software/script && .venv/bin/python cu.py "hw mode -r" \
    #       "lf sniff --out ../../research/indala-psk-read/caps/psk_rf8.bin"

    lf t55xx write -b 0 -d 00081440              # PSKCF RF/4  -> 31250 Hz   -> psk_rf4.bin
    lf t55xx write -b 0 -d 00081040              # PSKCF RF/2  -> 62500 Hz   -> psk_rf2.bin  (restores Indala)

    ./sweep.py caps/baseline.bin caps/psk_rf8.bin caps/psk_rf4.bin caps/psk_rf2.bin

⚠ **Use a scratch T5577, not the working credential**, if one is to hand. Writing block 0 is
how a T5577 gets locked into an unreadable configuration. The original is recoverable — block 0
`00081040`, data blocks `A0000000` / `E6BD0E92` — and the last write above restores it, but a
blank costs nothing.

### 5b. Alternative: scope TP7

If an oscilloscope is available it is a useful cross-check, though it cannot separate filter from
sampler. Probe TP7 (`LF_OA_OUT`); if hard to locate, `IC1B` pin 7 (GS358B-FR output) is the same
net. Ground to any GND test point.

⚠ **Use `./fieldhold.sh 60`, not `lf sniff`.** `lf sniff` drops the field after ~32ms once its
4000-sample buffer fills. `fieldhold.sh` loops a read that is *expected to fail*, and a failing
read holds the field for the full `g_timeout_readem_ms` = 500ms (`lf_reader_main.c:25`) — measured
~77% duty cycle.

⭐ **Run the HID Prox control BEFORE the Indala tag.** Its fc/8 at 15.6kHz must show up strongly.
If it does not, the probe is on the wrong net and a null result on Indala would mean nothing.

Settings: **AC coupling** (the net sits on `LF_VBIAS`), 10µs/div to resolve the 16µs subcarrier
period, 200mV/div to start, **FFT** if available. `LF_OA_OUT` is downstream of the VD1 peak
detector so it carries the **envelope** — 125kHz dominating there would mean the detector is not
behaving as this note assumes.

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
