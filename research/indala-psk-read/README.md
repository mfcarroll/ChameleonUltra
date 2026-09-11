## Indala on Chameleon Ultra — ~27 dB of analog loss, not a firmware gap

**Measured on device 2026-09-10/11.** Chameleon Ultra v3, firmware `v2.2 (v2.2.0-32-gccf6075)`,
chip id `a461ebf3b85fb19c`. Reference reads on a Proxmark3 Iceman.

Indala is **PSK1, RF/32, 64 or 224 bits** (proxmark3 `client/src/cmdlfindala.c:17`). The Chameleon
Ultra reads no PSK tag of any kind. §7 documents the firmware gap, which is certain. **§0 is what
else stands in the way, and how much of that is fixable in firmware.**

### 0. ⭐⭐⭐ RESULT: ~27 dB of analog loss that firmware cannot reach

Measured at 14 bits, **5 repeats per point, each deglitched, medians reported**
(`campaign_20260910_205456`, `--reads "sniff16 x5" --pm3-signal 0`), against the Proxmark
on identical stimulus. Normalised to each instrument's own RF/8, which divides out the
tag's own rolloff:

| PSKCF | subcarrier | smp/cyc | Proxmark | Chameleon | **excess loss** | spread |
|---|---|---|---|---|---|---|
| RF/4 | 31250 Hz | 4.0 | −4.3 dB | −8.4 dB | **−4.1 dB** | 1.02x |
| RF/2 | 62500 Hz | **2.0** | −9.3 dB | −43.8 dB | **−34.5 dB** | 1.28x |

⭐ Repeat spreads of 1.02x / 1.02x / 1.28x — after eight weeks of chasing artefacts, these
are the first numbers in this note that are simply **stable**.

**Splitting the 34.5 dB at fc/2:**

| | |
|---|---|
| sample phase (`phasesweep.py`, R²=0.90 one-cycle fit) | **~7.5 dB** — firmware can reach this |
| residue | **~27 dB** — firmware cannot |

⇒ **The analog front end is the blocker, and it is not close.** The Chameleon already loses
4.1 dB at 31 kHz, where sampling is safe at 4 samples/cycle, so the rolloff is genuine and
starts well below fc/2 — about 30 dB per octave across that span. Optimising the sample
phase is worth a real 7.5 dB and no more.

⇒ **Reading Indala on an unmodified Chameleon Ultra is not viable.** At the best phase the
fc/2 sideband reaches ~20 against an empty-field floor of ~7 — roughly 3x, where a
demodulator wants an order of magnitude. The missing ~27 dB is in the receive chain ahead
of the ADC, so no sample-rate, sample-phase or comparator change recovers it.

⚠ **One cheap confirmation is still outstanding.** Sampling at 200–250 kHz removes the
Nyquist degeneracy entirely rather than merely moving it. If fc/2 is *still* ~27 dB down
there, the analog attribution is proved rather than inferred. That is the one experiment
left worth running.

### 0a. ⛔ GAIN CLOSED: the noise floor is ANALOG-referred, so the ADC was never the limit

`gaintest.py --repeats 7`, empty-then-tag, four gains, deglitched medians. The question was
whether the fc/2 floor is quantisation/converter noise (which gain would lift the signal
clear of) or analog noise from the detector and op-amp chain (which gain multiplies along
with the signal).

| gain | gain vs 1/6 | empty-field std | std vs 1/6 | empty sb | tag sb | tag/empty |
|---|---|---|---|---|---|---|
| 1/6 | 1.00x | 357 | 1.00x | 7.18 | 13.85 | 1.93x |
| 1/5 | 1.20x | 436 | 1.22x | 8.91 | 17.15 | 1.93x |
| 1/4 | 1.50x | 635 | 1.78x | 12.87 | 20.33 | 1.58x |
| 1/3 | 2.00x | 830 | 2.33x | 11.61 | 27.78 | 2.39x |

⇒ **The floor tracks gain at every point** — if anything slightly faster than gain. Signal
and floor rise together, so **tag/empty is flat**: 1.93, 1.93, 1.58, 2.39 is non-monotonic
with ±25% scatter, not a trend.

⛔ **So the ADC was never the limit, and differential mode against `LF_RSSI` is dead too** —
subtracting the DC pedestal would allow more gain, but more gain amplifies an analog floor
along with the signal. Nothing in the converter's configuration helps.

⚠ **Two reading traps this run exposed**, both now handled in the script:
- **Endpoints lied.** First-to-last gave "+1.9 dB" from 1.93x → 2.39x, while the full
  series is non-monotonic. The verdict now reads the whole sequence and says whether it is
  monotonic.
- **"Saturated" was measuring glitches, not clipping.** Per-repeat at a FIXED gain it swings
  0% to 7%, and 1/3 can read lower than 1/4 despite sitting nearer the rail. Those are
  USB-overrun bursts hitting the rails. Relabelled, and flagged as a glitch indicator.

### 0b. ⛔⛔ THE MEASUREMENT TRAP THAT INVALIDATED TWO SWEEPS

`lf sniff` captures land randomly in one of two states: clean, or carrying a **USB-transfer
buffer overrun** (`lf_reader_generic.c:30`). An overrun is a full-scale discontinuity —
measured single-sample jumps of **16380 of 16383** — in a localised burst, and it dumps
broadband energy into every bin including fc/2. Capture std is bimodal, ~350 against ~2500.

Measured at a **fixed** phase, tag untouched, six consecutive captures:

    raw        752.7   15.3   53.1  539.1  469.0   17.0     <- 49x spread
    deglitched  15.5   15.3   17.0   16.3   17.4   17.0     <- 1.1x

⇒ **Any single-capture number from this device is suspect**, including the PSKCF sweep that
produced the −34.2 dB figure §0 now supersedes. The fix is cheap: a capture returns when the
buffer fills (~16 ms), **not** after `--timeout`, so repeats cost almost nothing. Every
measurement here is now a median of 5 deglitched captures.

⚠ The screen is **scale-free** (drop 50-sample windows whose peak-to-peak exceeds 4x the
median window's). A fixed LSB threshold tuned on 8-bit captures once discarded 100% of a
strong 14-bit capture and returned `nan`.

### 0c. ⛔ TWO VERDICTS RETRACTED, BOTH FROM THIS SCRIPT

1. **"659x at 44 ticks — cos(phi) confirmed."** Withdrawn: that sweep was one capture per
   phase and was tracking overrun bursts, not phase. The 659x was a small denominator.
2. **"The tag never rises above the empty field at any phase."** Withdrawn, and it was
   contradicted by its own table — the tag led at all 32 phases. The verdict asked
   `max ratio > 3?`, got **2.90**, and fell through to the falsified branch. Arbitrary
   threshold, wrong answer. `phasesweep.py` now fits the shape and reports the fit.

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

### 6. Routes — what is closed, and what is left

⛔ **Closed by measurement, all firmware-side:**

| lever | result | where |
|---|---|---|
| 8-bit truncation | fixed (`--bits 16`); it was distorting the rolloff shape | §0b |
| sample phase | real but worth only **7.5 dB**, never nulls | §0 |
| oversampling at 200kHz | recovers **nothing**; control moved as much | §0 |
| ADC gain | floor is **analog-referred**; SNR flat across 2x gain | §0a |
| differential vs `LF_RSSI` | dead by implication — gain cannot beat an analog floor | §0a |

⇒ Nothing in the converter, its clock, its phase or its gain moves fc/2. The limit is in
the analog chain ahead of it.

⭐ **Still open, and worth trying in this order:**

1. **Settle / field-on time.** `raw_read_to_buffer` waits a hardcoded `bsp_delay_ms(2)` and
   then captures 10–16ms. A T5577 must charge before it transmits at full amplitude, and
   this has never been varied. ⭐ This is the direct analogue of the Momentum `t5577-deep-read`
   finding where apparent air-gap effects turned out to be settle — same shape of confound,
   and every measurement in this note was taken at ONE settle.
2. **Air gap.** Every capture here is `--gap flat`. The harness already stamps the axis.
3. **Field drive.** `m_lf_125khz_pwm_seq_val = {2,0,0,0}` with `top_value 4` — a hardcoded
   50% duty. Changes tag power and the detector's operating point.
4. **Fix the overruns.** 10–20% of windows are discarded, and the bursts have produced three
   false verdicts in this investigation. Worth doing for the measurement quality alone.

⚠ **And the honest possibility**: if settle, gap and drive all come back flat, the answer is
the front end's bandwidth and only a component change moves it.

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
