## Indala on Chameleon Ultra — not readable, and not a configuration problem

**Measured on device 2026-09-10.** Chameleon Ultra v3, firmware `v2.2 (v2.2.0-32-gccf6075)`,
chip id `a461ebf3b85fb19c`, on `/dev/tty.usbmodemF429364E46961`. Repo at `ccf6075`
(= `origin/main` that day, 32 commits past the `v2.2.0` release).

Indala is **PSK1, RF/32, 64 or 224 bits** (proxmark3 `client/src/cmdlfindala.c:17`). The
Chameleon Ultra can *transmit* PSK1 but has **no PSK demodulator at all**, so no PSK tag
of any kind can be read. An indala26 tag reads as silence.

### Three independent confirmations in the firmware

**1. Indala is an explicit unimplemented placeholder.**
`firmware/application/src/rfid/nfctag/tag_base_type.h:61`

    //////// PSK Tag-Talk-First   300
    // Indala
    // Keri
    // NexWatch
    TAG_TYPE_IDTECK = 310,

**2. Every LF reader is ASK or FSK.** `firmware/application/src/rfid/reader/lf/` contains
`lf_em410x_data.c`, `lf_em4x05_data.c`, `lf_hidprox_data.c`, `lf_ioprox_data.c`,
`lf_jablotron_data.c`, `lf_pac_data.c`, `lf_t55xx_data.c`, `lf_viking_data.c` — and nothing
else. The only PSK source in the tree is `nfctag/lf/utils/psk1.c`, whose sole export is
`lf_psk1_build_sequence()`, a **transmit** waveform builder. It is included by exactly one
file, `protocols/idteck.c`, for emulation.

**3. The one PSK protocol is write-only.** `lf idteck` offers `write` and `econfig` but no
`read` — the only LF protocol in the CLI missing a read. There is no `IDTECK_SCAN` opcode;
the LF wire protocol (`software/script/chameleon_enum.py:85`) exposes only `EM410X_SCAN`,
`HIDPROX_SCAN`, `VIKING_SCAN`, `PAC_SCAN`, `IOPROX_SCAN`, `JABLOTRON_SCAN`, `EM4X05_SCAN`.

⇒ There is also **no generic LF identify command in the firmware**. The GUI's "generic LF
read" can only be rotating through those seven per-protocol scans. The CLI is not missing
a feature.

### Measured: the tag produces no detectable modulation

Three raw `lf sniff` captures (4000 samples = 32ms each, 125kHz, 8us/sample), analysed with
`analyse.py`. The control was an HID Prox card that reads fine.

| capture | residual std | fc/8 vs baseline | fc/10 vs baseline | fc/2 vs baseline |
|---|---|---|---|---|
| baseline (empty field) | 8.56 | 1.00x | 1.00x | 1.00x |
| **indala26** | **8.15** | 0.99x | 0.84x | **0.87x** |
| control (HID Prox) | 16.24 | **4.25x** | **3.19x** | 1.02x |

The indala capture is **indistinguishable from an empty field** — every band between 0.84x
and 1.38x, and its residual std is *below* baseline. Its energy at fc/2 = 62500Hz, the
Indala subcarrier, is the lowest of the three (Nyquist-bin magnitude 41, vs 68 for an
empty field).

Two cross-checks rule out a broken measurement:

- **The method works.** The control lights up at fc/8 (15625Hz) and fc/10 (12500Hz) at
  4.25x and 3.19x baseline — textbook HID Prox FSK, caught cleanly by the same pipeline.
- **No frame structure.** A 64-bit Indala at RF/32 repeats every 2048 samples (16.4ms).
  Autocorrelation of the coherently-demodulated envelope at that lag: **-0.057**. Nothing.
  A dedicated PSK1 detector (multiply by `(-1)^n` to mix fc/2 to DC, low-pass over half a
  bit) scored SNR 1.62x on indala versus **1.71x on the empty field** — no discrimination.

### ⚠ Every capture contains glitches — a baseline is mandatory
All three captures, **including the empty-field baseline**, contain large amplitude bursts
(peak-to-peak > 200). These are USB-transfer buffer overruns, not tag signal — see
`lf_reader_generic.c:30` ("buffer full — oldest samples dropped"). Mistaking them for
modulation is the easy error here; `analyse.py` drops any 40-sample window swinging more
than 60 LSB before it measures anything. Without an empty-field reference you cannot tell
a burst from a tag.

### Why offline demodulation of the raw capture will not rescue this
`lf sniff` gives the same 125kHz envelope stream the Proxmark demodulates Indala from, so
it looks tempting. Two things kill it:

- **Analog bandwidth.** The captures show the 125kHz LC tank passing HID's 12.5–15.6kHz
  sidebands strongly. Indala's subcarrier sits at **62.5kHz, four times further out** —
  far outside the tank's passband.
- **No sampling margin.** The SAADC samples once per carrier cycle, so 62.5kHz is *exactly*
  Nyquist. The whole subcarrier collapses into a single FFT bin.

⇒ Indala needs a demodulator the firmware does not have, fed by analog bandwidth the
hardware does not deliver. **Use the Proxmark3 for Indala.**

### Not tested / open
- **Whether the tag was energised at all.** This data cannot separate "tag never coupled"
  from "front end cannot pass fc/2" — both predict identical silence. Settle it with
  `lf indala reader` on a Proxmark3; if that reads the tag and the Chameleon still sees
  nothing, the conclusion above is complete.
- **Indala *emulation*.** Much smaller gap than reading: `psk1.c` already builds RF/32 PSK1
  waveforms for IDTECK, so a `TAG_TYPE_INDALA` would mostly be frame construction. Untried.
- **Keri and NexWatch**, the other two PSK placeholders — same blocker, not measured.
- Only one indala26 tag was tested, at one position on the antenna.

### Reproducing

    ./grab.sh        # prompts through baseline / indala / control
    ../../software/script/.venv/bin/python analyse.py caps/*.bin

`grab.sh` drives the CLI through `software/script/cu.py`, added on this branch: the stock
CLI has no non-interactive mode (`chameleon_cli_main.py` calls `startCLI()` unconditionally),
so `cu.py` reuses its `exec_cmd()` the way `tests/test_ultra.py` does.

    cd software/script && .venv/bin/python cu.py "hw version" "lf em 410x read"

Analysis needs `numpy` in `software/script/.venv` (2.5.3 used here).
