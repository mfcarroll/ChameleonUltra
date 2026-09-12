# Next — ranked

⛔⛔ **EVERY MEASUREMENT IN THIS PROJECT WAS TAKEN WITH THE TAG ON THE WRONG SIDE OF THE
DEVICE.** The reading side is the FRONT. It is worth **21x** (~26 dB). See the banner at the
top of `FINDINGS.md`. The plan below is reorganised around that: re-measure first, and treat
every ⚠B claim as provisional until it is redone.

**State:** `lf indala read` works, and on the front it works far better than any number in
these notes suggests — 71% of single captures decode, against 32% on the back, and the
"phase window" turns out not to be a window.

⛔ Method rules live in `METHOD.md`, not here. Read them before adding a claim to the ledger.

---

## 1. ✅ RE-MEASURED ON THE FRONT — 1a, 1b and 1c are done

The full sweep ran (`caps/front/`, 320 captures, committed). **114 of 160 single captures
decode the truth (71%) and 0 of 160 empty captures produce a frame at all.** Phase is TWO
working bands — 0–56 and 96–124 — split by a dead band at 60–92. C36, C37, C38–C43, L58.

⛔ 1b and 1c needed no bench time: the sweep saves every capture, so both were settled
offline against the committed set. Do that first next time.

**What it settled:**

| | result |
|---|---|
| **1a** phase map | two bands, not a window. Every good phase is 5/5; the edges are cliffs, not gradients |
| **1a** the skirt | **⛔ does not predict decode at all** (C38) — peak skirt and minimum skirt both decode 5/5, the dead band sits between them. This retires §7 rather than porting it |
| **1b** C03 fs/2 notch | **reversed sign** (C41). Notch ON 114 truth / 21 wrong, OFF 121 / 26. No longer load-bearing either way — left ON, since it trades ~7 decodes for ~5 fewer wrong frames |
| **1b** C04 settle discard | **still fatal, 107 -> 0** (C42), and always was structural rather than SNR |
| **1c** agreement rule | **⛔ keep it — it is WEAKER than believed, not stronger** (C39, C40) |

## 1c-follow-up. ✅ THE STRADDLE GATE — closed

On the front the decoder now returns **110 frames, 110 of them correct**. C48, C49, L61.

Reject when a frame is BOTH loud and ragged: `mean|integ| >= 2048` AND `min|integ| * 8 <
mean|integ|`. 21 of 21 straddles rejected, 40 of 40 true frames kept at rotation phases,
back-side set untouched.

⚠ **It does not retire the 60–92 keep-out.** The amplitude term is absolute and therefore
coupling-dependent (C43). A tag coupled well enough to straddle but too weakly to clear 2048
slips through. Two layers, not one.

⚠ **Still worth doing:** the gate is tuned on 21 straddles from one tag on one unit. A
second Indala tag (§4) would be the first real test of the 2048 threshold, since it moves
with coupling. Until then treat the margin — 2.2x above the loudest back-side frame, 2.5x
below the quietest straddle — as the whole safety budget.

**1d. ✅ The loud-signal null passes.** An HID Prox tag at **88–100x the empty floor in its
own band** produced `LF tag not found` on **30 of 30** `lf indala read` attempts. C44, L59.
That is the case C24 could not test: a loud wrong signal rather than a quiet one.

⛔ It nearly went in the bin. All 6 bracketing `lf hid prox read` calls failed, which by the
old §3 rule means "the tag was not coupled, the null is uninterpretable". The tag was at 90x
the floor the whole time. **A failed read is not evidence of absence** — bracket with
`lfprobe.py`, which measures presence directly (F05, and §3 below is rewritten).

## 2. ⭐⭐ Re-open what was closed on back-side data

⚠ These were closed, some of them emphatically, on measurements now known to be ~26 dB down.

| | why it should be re-opened |
|---|---|
| **`LF_RSSI` / AIN0** (C08, C09) | closed as "carries no fc/2, flat to 0.5 dB". Measured with the tag on the back. The whole comparison was between two nodes seeing 1/20 of the available signal, and the conclusion killed an entire line of investigation |
| ~~**Stacking**~~ ✅ | **Resolved, C58.** Nothing on the front (68.75% at every depth); reinforces the straddle in the dead band; still worth 32%->72% on the back. Keep it, but it is insurance for the wrong placement, not a feature of the right one |
| ~~**Frame lock**~~ ✅ | **Resolved, C59.** The conclusion stands, the evidence does not: the empty field correlates as well as the tag, but rolled-stack decoding collapses 67%->0%, which proves alignment operationally (M25) |
| **SAADC gain** (C10) | "the floor is analog-referred" may well survive, but it was measured against a signal 26 dB below what the device actually delivers |

## 2b. ✅ Phase rotation re-derived — and the union was a trap

`PHASE_ROTATION` is now `{20, 12, 28, 36, 44, 16, 112, 0}`: the first five decode on BOTH
placements (10/10 or 9/10 across the two sweeps), and the last three are insurance chosen
for spread rather than rank, including one from the upper front band. The old tail (`4`,
`56`, `0`) was weak on the back and `56` sits one step from the dead band.

⛔ The lesson is in §1c-follow-up: taking the union of "phases that ever worked" would have
imported phase 64, which returns a wrong credential 5 times out of 5. A phase that is dead
is cheap; a phase that lies is not. The rotation is now derived from phases that decode
correctly on both sides AND produce no repeatable wrong frame on either.

## 3. ✅ LOUD-SIGNAL NULLS — DONE. 220 reads, 0 false positives

| interferer | modulation | bracket | result |
|---|---|---|---|
| HID Prox, back (C24) | FSK RF/50 | a read | 10/10 not found |
| HID Prox, front (C44) | FSK RF/50 | amplitude 88–100x | 30/30 |
| EM410x (C52) | ASK RF/64 | amplitude 19x + its own reader | 20/20 |
| **IDTECK (C55)** | **PSK1 RF/32 — Indala's own config word** | **its own reader, both units** | **40/40** |
| Viking (C55) | ASK RF/32 | amplitude 18.6/19.3x | 40/40 |
| PAC (C55) | NRZ RF/32 | amplitude 16.1/15.9x | 40/40 |
| Jablotron (C55) | biphase RF/64 | amplitude 18.1/18.3x | 40/40 |

Plus 320 empty captures producing no frame at all. ⛔ Read C57 before running another one:
the default band is wrong for a PSK1 interferer, and for that case the interferer's own
reader is the better bracket, not the amplitude probe.

## 3b. ⭐⭐⭐ FIX THE HID PROX AND PAC READERS — both fail on loud tags

⭐⭐ **START HERE: the SAADC readers duplicate a capture path that one of them gets right.**
The LF readers are two families, and it matters which:

| family | readers | capture |
|---|---|---|
| GPIO/comparator | em410x, jablotron, viking | `register_rio_callback`, 128-entry ring, no SAADC |
| **SAADC** | **hidprox, ioprox, pac** + lf_reader_generic | own `saadc_cb`, own 6144 ring, own field start/stop |

Only `lf_reader_generic.c` also suspends BLE advertising — and its own comment records why:
a burst collapses the 125 kHz field for ~1.6 ms and hit **4 captures in 10**. The three that
duplicate the prologue instead of sharing it are HID, ioProx and PAC, and HID and PAC are
exactly the two measured failing on loud tags. The SAADC reader that HAS the guard is Indala,
at 60/60.

⇒ `capture_begin()`/`capture_end()` already exist and are already shared by two entry points.
Moving HID onto them is a small mechanical change that also happens to be **the clean test of
C47** — same protocol, same tag, same bench, one variable.

⛔ **Do not read the em410x 95% as evidence either way.** It is on the GPIO path and never
touches the SAADC, so it cannot test this. That mistake is why C47 was wrongly weakened in
L64.


Not this project's decoder, but it is the comparison instrument for everything here and it
has cost two measurements already (L51's uninterpretable run, and C44's near-miss).

**What is established:** three tags with byte-identical memory read 0/6, 3/6 and 7/9 on the
Chameleon and 3/3 on a Proxmark (C46). The RF path is flat while reads fail (C45). So the
decoder's tolerance is narrower than the Proxmark's, and package-level differences cross it.

⚠ **`lf pac read` has the same disease**: 0/5 on one unit and 2/5 on the other while its tag
sat at 16x the empty floor and a Proxmark read it perfectly (L66). So this is not one broken
decoder — HID Prox and PAC both fail on tags that are loudly present, and `lf em 410x read`
sits at 95% (76/80) rather than 100%. ⇒ Whatever is wrong may be shared across the LF reader
family rather than specific to FSK. Fix HID first because it fails hardest (0/6, one tag
never reading at all), but measure PAC in the same session — a fix that moves both is a very
different fix from one that moves only HID.

⚠ The Indala reader is the only LF reader in this tree with its own capture path
(`raw_read_samples`, which suspends BLE and hands the decoder a whole buffer). It is also the
only one at 100% — 40/40 today across two units. That may be the cleanest clue available, or
it may be that Indala is simply the only one anybody has tuned. Do not assume which.

**⭐ Test this first — it is one line and it explains an old observation.**
`advertising_stop()` appears in **1 of 15** LF reader files. Only `lf_reader_generic.c`
suspends BLE advertising; `hidprox_read()` does not. That file's comment records the
measurement that put it there: an advertising burst collapses the 125 kHz field for ~1.6 ms
and hit **4 captures in 10**. And it predicts the thing nobody could explain in L51 —
*"after connecting it to my phone and/or a reboot, it does read"* — because connecting a BLE
central is precisely what stops advertising (C47).

```bash
# the discriminating test, ~5 minutes, needs the BLUE DUAL (the 0/6 tag — the others
# have too little headroom to show an improvement)
cd software/script && for i in $(seq 1 15); do .venv/bin/python cu.py "lf hid prox read" | tail -1; done
# then connect a phone over BLE so advertising stops, and repeat
```

⚠ **If it works, resist generalising it.** It cannot explain the blue dual reading 0/6
deterministically — an intermittent field collapse does not produce a clean zero. Expect two
causes: a BLE-induced intermittency affecting every LF reader, and a per-tag waveform
tolerance in the FSK demodulator. ⇒ The fix belongs in `capture_begin()`-style shared code so
all 15 readers get it, not pasted into `hidprox_read()` alone.

⚠ **Until it is fixed, do not use HID Prox as the probe tag for a null.** Use amplitude
(`lfprobe.py`) for presence, per §3.

## 3a. ✅ INDALA EMULATION WORKS — and four claims had to be retracted to find out

Proxmark capture, emulating Chameleon placed exactly where a real tag had just been proven to
couple (C68, C69, L73, M26):

| | peak-to-peak | fs/2 skirt | constant-phase run | our decoder |
|---|---|---|---|---|
| real tag | 239 | 415835 | 929 (needs 928) | **12/12 windows** |
| **emulator** | 250 | 78782 | **931 (needs 928)** ✓ | **10/12 windows** |
| empty | 22 | 3426 | 49 | — |

⛔ **C63, C66 and C67 are retracted** — including "PSK1 tag emulation has never worked on this
device", which was the most emphatic claim in the ledger. C64 is provisional.

**What went wrong:** every retracted measurement was Chameleon-to-Chameleon, comparing an
emulating Chameleon — a second device at an unproven distance and orientation — against
committed captures of a real tag lying flat on the antenna. Same reader, same analysis, same
empty-field baseline; different geometry. The emulator was coupling badly and every number
downstream described that. The IDTECK "control" shared the flaw, which is exactly why it
agreed so convincingly (M26).

**What actually remains, and it is real:**

⚠ **Emulation is ~5x weaker than a real tag** — skirt 78782 vs 415835, integrator amplitude
~11000 vs ~55900 — and **the Proxmark's own `lf indala reader` will not lock onto it** even
though our decoder reads it 10 times in 12. That is a margin problem, and it is the thing
that matters for anyone who wants to use this against a real reader.

⚠ A clock drift exists and is far smaller than C67 claimed: the winning bit offset slides
12 -> 8 over 180 ms of capture and the polarity flag flips, roughly 180 ppm. Harmless inside
one 4096-sample window, and consistent with a PWM free-running against the reader's clock.

**Next, in order:**

⭐ **The cause is clock drift, and the threshold is measured.** Proxmark's own demodulator
reads the emulated signal at 131 ms and 163 ms of capture and fails at 196 ms and beyond
(C70). A real tag's frame period is exactly 2048.000 samples with zero slip because it
DIVIDES the reader's carrier (C71); a free-running PWM cannot. Everything else is eliminated:
placement (C72 — and this is the first time in this project placement was not the answer),
modulation depth, transitions, duty cycle, and the low-frequency burst envelope (C73).

1. ⭐ **Try shortening the burst first — it is a one-constant change and the numbers point
   straight at it.** `lf_tag_em.c` plays `nrfx_pwm_simple_playback(..., 10, FLAG_STOP)` — ten
   frames, **164 ms**, sitting exactly on the 163/196 ms threshold. ⚠ But think before
   changing it: a shorter burst restarts the sequence more often, and each restart is a phase
   DISCONTINUITY in the middle of a long reader capture. That could easily be worse, not
   better. Measure with `lf indala demod` on a full-length trace, not on the burst alone.
2. **Establish what a real reader actually needs.** The Proxmark demods a 290 ms buffer,
   which may be far longer than a door reader's window. If real readers sample ~50 ms, this
   emulation may already work in the field and only fail against the Proxmark. ⚠ Nobody has
   tested it against an actual access-control reader, and that is the bar that matters.
3. ⭐⭐ **The complete fix is to lock the subcarrier to the reader's carrier, and the Flipper
   Zero shows it is the right answer** (C74). Its emulation timer is clocked from the
   EXTERNAL TRIGGER — the reader's carrier through the comparator — so it counts carrier
   cycles and divides them, exactly as a T5577 does:

   ```c
   LL_TIM_SetClockSource(FURI_HAL_RFID_EMULATE_TIMER, LL_TIM_CLOCKSOURCE_EXT_MODE2);
   LL_TIM_ConfigETR(FURI_HAL_RFID_EMULATE_TIMER, LL_TIM_ETR_POLARITY_INVERTED, ...);
   ```

   ⛔ **IT IS NOT A PORT.** The STM32's TIM2 accepts an external clock on ETR. The nRF52's
   PWM is clocked only from PCLK16M and has no equivalent input, so no amount of tuning the
   sequence-playback path can lock it. ⚠ Confirm that against the nRF52840 PS before
   building — it is the assumption the whole design rests on.

   **The nRF equivalent, built from pieces this device already uses in READER mode:**

   | piece | already exists? |
   |---|---|
   | `m_pwm_timer_counter` = TIMER2 in `NRF_TIMER_MODE_COUNTER` | ✓ `lf_125khz_radio.c` |
   | PPI into `NRF_TIMER_TASK_COUNT` | ✓ — but keyed off `PWMPERIODEND`, which only exists when WE generate the carrier |
   | an event per RECEIVED carrier cycle | ⚠ needed. LPCOMP (already initialised for field detect) or the RIO GPIOTE edge the ASK readers use |
   | TIMER COMPARE -> PPI -> GPIOTE toggle on `LF_MOD` | ⚠ needed |

   That gives an fc/2 subcarrier locked to the carrier, with the CPU involved only once per
   bit (256 us) to express a phase reversal as a skipped toggle. ⚠ Still do 1 and 2 first:
   they are hours against days, and 2 may show this is not needed in the field at all.

⚠ **What NOT to do:** chase the 0.6x amplitude. Both traces are near the ADC's full scale
(peak-to-peak 250 emulator, 239 real tag), so the emulator is not quiet — its energy is
distributed differently, and amplifying is neither possible nor the point.

## 3b. ⭐⭐⭐ FIX THE HID PROX AND PAC READERS — both fail on loud tags

⭐⭐ **START HERE: the SAADC readers duplicate a capture path that one of them gets right.**
The LF readers are two families, and it matters which:

| family | readers | capture |
|---|---|---|
| GPIO/comparator | em410x, jablotron, viking | `register_rio_callback`, 128-entry ring, no SAADC |
| **SAADC** | **hidprox, ioprox, pac** + lf_reader_generic | own `saadc_cb`, own 6144 ring, own field start/stop |

Only `lf_reader_generic.c` also suspends BLE advertising — and its own comment records why:
a burst collapses the 125 kHz field for ~1.6 ms and hit **4 captures in 10**. The three that
duplicate the prologue instead of sharing it are HID, ioProx and PAC, and HID and PAC are
exactly the two measured failing on loud tags. The SAADC reader that HAS the guard is Indala,
at 60/60.

⇒ `capture_begin()`/`capture_end()` already exist and are already shared by two entry points.
Moving HID onto them is a small mechanical change that also happens to be **the clean test of
C47** — same protocol, same tag, same bench, one variable.

⛔ **Do not read the em410x 95% as evidence either way.** It is on the GPIO path and never
touches the SAADC, so it cannot test this. That mistake is why C47 was wrongly weakened in
L64.


Not this project's decoder, but it is the comparison instrument for everything here and it
has cost two measurements already (L51's uninterpretable run, and C44's near-miss).

**What is established:** three tags with byte-identical memory read 0/6, 3/6 and 7/9 on the
Chameleon and 3/3 on a Proxmark (C46). The RF path is flat while reads fail (C45). So the
decoder's tolerance is narrower than the Proxmark's, and package-level differences cross it.

⚠ **`lf pac read` has the same disease**: 0/5 on one unit and 2/5 on the other while its tag
sat at 16x the empty floor and a Proxmark read it perfectly (L66). So this is not one broken
decoder — HID Prox and PAC both fail on tags that are loudly present, and `lf em 410x read`
sits at 95% (76/80) rather than 100%. ⇒ Whatever is wrong may be shared across the LF reader
family rather than specific to FSK. Fix HID first because it fails hardest (0/6, one tag
never reading at all), but measure PAC in the same session — a fix that moves both is a very
different fix from one that moves only HID.

⚠ The Indala reader is the only LF reader in this tree with its own capture path
(`raw_read_samples`, which suspends BLE and hands the decoder a whole buffer). It is also the
only one at 100% — 40/40 today across two units. That may be the cleanest clue available, or
it may be that Indala is simply the only one anybody has tuned. Do not assume which.

**⭐ Test this first — it is one line and it explains an old observation.**
`advertising_stop()` appears in **1 of 15** LF reader files. Only `lf_reader_generic.c`
suspends BLE advertising; `hidprox_read()` does not. That file's comment records the
measurement that put it there: an advertising burst collapses the 125 kHz field for ~1.6 ms
and hit **4 captures in 10**. And it predicts the thing nobody could explain in L51 —
*"after connecting it to my phone and/or a reboot, it does read"* — because connecting a BLE
central is precisely what stops advertising (C47).

```bash
# the discriminating test, ~5 minutes, needs the BLUE DUAL (the 0/6 tag — the others
# have too little headroom to show an improvement)
cd software/script && for i in $(seq 1 15); do .venv/bin/python cu.py "lf hid prox read" | tail -1; done
# then connect a phone over BLE so advertising stops, and repeat
```

⚠ **If it works, resist generalising it.** It cannot explain the blue dual reading 0/6
deterministically — an intermittent field collapse does not produce a clean zero. Expect two
causes: a BLE-induced intermittency affecting every LF reader, and a per-tag waveform
tolerance in the FSK demodulator. ⇒ The fix belongs in `capture_begin()`-style shared code so
all 15 readers get it, not pasted into `hidprox_read()` alone.

⚠ **Until it is fixed, do not use HID Prox as the probe tag for a null.** Use amplitude
(`lfprobe.py`) for presence, per §3.

## 3a. ✅ INDALA EMULATION WORKS — and four claims had to be retracted to find out

Proxmark capture, emulating Chameleon placed exactly where a real tag had just been proven to
couple (C68, C69, L73, M26):

| | peak-to-peak | fs/2 skirt | constant-phase run | our decoder |
|---|---|---|---|---|
| real tag | 239 | 415835 | 929 (needs 928) | **12/12 windows** |
| **emulator** | 250 | 78782 | **931 (needs 928)** ✓ | **10/12 windows** |
| empty | 22 | 3426 | 49 | — |

⛔ **C63, C66 and C67 are retracted** — including "PSK1 tag emulation has never worked on this
device", which was the most emphatic claim in the ledger. C64 is provisional.

**What went wrong:** every retracted measurement was Chameleon-to-Chameleon, comparing an
emulating Chameleon — a second device at an unproven distance and orientation — against
committed captures of a real tag lying flat on the antenna. Same reader, same analysis, same
empty-field baseline; different geometry. The emulator was coupling badly and every number
downstream described that. The IDTECK "control" shared the flaw, which is exactly why it
agreed so convincingly (M26).

**What actually remains, and it is real:**

⚠ **Emulation is ~5x weaker than a real tag** — skirt 78782 vs 415835, integrator amplitude
~11000 vs ~55900 — and **the Proxmark's own `lf indala reader` will not lock onto it** even
though our decoder reads it 10 times in 12. That is a margin problem, and it is the thing
that matters for anyone who wants to use this against a real reader.

⚠ A clock drift exists and is far smaller than C67 claimed: the winning bit offset slides
12 -> 8 over 180 ms of capture and the polarity flag flips, roughly 180 ppm. Harmless inside
one 4096-sample window, and consistent with a PWM free-running against the reader's clock.

**Next, in order:**
1. **Find the 5x.** Modulation depth is the obvious suspect — `LF_PSK1_SUBCARRIER_DUTY` is 8
   of a 16 counter_top, i.e. 50%. A real T5577 shunts harder. ⚠ Measure before changing it.
2. **Re-test whether a real reader locks on** once the margin improves. `lf indala reader` on
   the Proxmark is the honest bar, not our own decoder.
3. **Redo C64's front/back question** on the Proxmark, since it was measured on the
   discredited instrument.
4. **Re-test IDTECK** the same way. Its retraction was collateral; nobody has yet shown
   IDTECK emulation working OR broken on a trustworthy instrument.

## 3b. ⭐⭐⭐ FIX THE HID PROX AND PAC READERS — both fail on loud tags

⭐⭐ **START HERE: the SAADC readers duplicate a capture path that one of them gets right.**
The LF readers are two families, and it matters which:

| family | readers | capture |
|---|---|---|
| GPIO/comparator | em410x, jablotron, viking | `register_rio_callback`, 128-entry ring, no SAADC |
| **SAADC** | **hidprox, ioprox, pac** + lf_reader_generic | own `saadc_cb`, own 6144 ring, own field start/stop |

Only `lf_reader_generic.c` also suspends BLE advertising — and its own comment records why:
a burst collapses the 125 kHz field for ~1.6 ms and hit **4 captures in 10**. The three that
duplicate the prologue instead of sharing it are HID, ioProx and PAC, and HID and PAC are
exactly the two measured failing on loud tags. The SAADC reader that HAS the guard is Indala,
at 60/60.

⇒ `capture_begin()`/`capture_end()` already exist and are already shared by two entry points.
Moving HID onto them is a small mechanical change that also happens to be **the clean test of
C47** — same protocol, same tag, same bench, one variable.

⛔ **Do not read the em410x 95% as evidence either way.** It is on the GPIO path and never
touches the SAADC, so it cannot test this. That mistake is why C47 was wrongly weakened in
L64.


Not this project's decoder, but it is the comparison instrument for everything here and it
has cost two measurements already (L51's uninterpretable run, and C44's near-miss).

**What is established:** three tags with byte-identical memory read 0/6, 3/6 and 7/9 on the
Chameleon and 3/3 on a Proxmark (C46). The RF path is flat while reads fail (C45). So the
decoder's tolerance is narrower than the Proxmark's, and package-level differences cross it.

⚠ **`lf pac read` has the same disease**: 0/5 on one unit and 2/5 on the other while its tag
sat at 16x the empty floor and a Proxmark read it perfectly (L66). So this is not one broken
decoder — HID Prox and PAC both fail on tags that are loudly present, and `lf em 410x read`
sits at 95% (76/80) rather than 100%. ⇒ Whatever is wrong may be shared across the LF reader
family rather than specific to FSK. Fix HID first because it fails hardest (0/6, one tag
never reading at all), but measure PAC in the same session — a fix that moves both is a very
different fix from one that moves only HID.

⚠ The Indala reader is the only LF reader in this tree with its own capture path
(`raw_read_samples`, which suspends BLE and hands the decoder a whole buffer). It is also the
only one at 100% — 40/40 today across two units. That may be the cleanest clue available, or
it may be that Indala is simply the only one anybody has tuned. Do not assume which.

**⭐ Test this first — it is one line and it explains an old observation.**
`advertising_stop()` appears in **1 of 15** LF reader files. Only `lf_reader_generic.c`
suspends BLE advertising; `hidprox_read()` does not. That file's comment records the
measurement that put it there: an advertising burst collapses the 125 kHz field for ~1.6 ms
and hit **4 captures in 10**. And it predicts the thing nobody could explain in L51 —
*"after connecting it to my phone and/or a reboot, it does read"* — because connecting a BLE
central is precisely what stops advertising (C47).

```bash
# the discriminating test, ~5 minutes, needs the BLUE DUAL (the 0/6 tag — the others
# have too little headroom to show an improvement)
cd software/script && for i in $(seq 1 15); do .venv/bin/python cu.py "lf hid prox read" | tail -1; done
# then connect a phone over BLE so advertising stops, and repeat
```

⚠ **If it works, resist generalising it.** It cannot explain the blue dual reading 0/6
deterministically — an intermittent field collapse does not produce a clean zero. Expect two
causes: a BLE-induced intermittency affecting every LF reader, and a per-tag waveform
tolerance in the FSK demodulator. ⇒ The fix belongs in `capture_begin()`-style shared code so
all 15 readers get it, not pasted into `hidprox_read()` alone.

⚠ **Until it is fixed, do not use HID Prox as the probe tag for a null.** Use amplitude
(`lfprobe.py`) for presence, per §3.

## 3a. ⛔⛔⛔ PSK1 EMULATION HAS NEVER WORKED — IDTECK IS BROKEN TOO, AND IT PREDATES US

Paired test, same frame `4944544b00000000`, same reader, same session (C66, C67, L72):

| | fc/2 skirt | longest constant-phase run | frame autocorrelation |
|---|---|---|---|
| **real IDTECK tag** | 80165 | **1042 samples** (needs 1024) ✓ | lag 2048, r=+0.38 |
| emulated IDTECK | 70183 | **100 samples** ⛔ | lag 2071, r=+0.18 |
| emulated Indala | 62524 | **102 samples** ⛔ | lag 2070, r=+0.34 |
| empty | 4591 | 34 | lag 1520, r=+0.01 |

⇒ **It is not `indala.c`.** The Indala emulator inherited a transmit path that was already
broken in a protocol nobody had verified. Both emulators modulate strongly and neither
carries phase, in numerically identical ways.

⭐ The real tag is also the control on the INSTRUMENT: 1042 against 1024 expected. The
measurement is sound, which was worth establishing before building on it.

**Ruled out, with evidence:**
- the modulator — host test gives polarity-per-bit identical to the frame (C63)
- the data path — `econfig` reads the frame back
- NEXT §3c stale clock — survives a power cycle, and 8x slow would show a 7.8 kHz subcarrier

⚠ **Mechanism still OPEN, and the obvious answer does not add up.** The mixed baseband peaks
at **641 Hz** (IDTECK) and **671 Hz** (Indala) with nothing comparable on an empty field, and
a ~640 Hz beat flips the apparent phase every ~98 samples — matching the observed 100–102
exactly. But 640 Hz is **1.03% of 62.5 kHz**, and two independent crystals at 20 ppm beat at
~2.5 Hz. So "the PWM is not phase-locked to the reader" predicts the right SHAPE and the
wrong SIZE by three orders of magnitude. Something is setting the subcarrier ~1% off, or the
beat is not a clock beat at all.

⭐ **THE NEXT EXPERIMENT — emulate a frame with NO phase transitions.**

```bash
.venv/bin/python cu.py "lf idteck econfig --id 0000000000000000"   # warns about the preamble; allow it
```

An all-zero frame makes `lf_psk1_build_sequence` emit one constant polarity for all 1024
entries — a pure unmodulated 62.5 kHz subcarrier with no data on it. Then any phase rotation
in the capture is the clock offset and nothing else:

- a clean ~640 Hz sinusoid in the mixed baseband ⇒ the subcarrier really is ~1% off fs/2, and
  the question becomes WHY (PWM period, prescaler, or the reader's own carrier)
- constant phase ⇒ the subcarrier IS locked, the beat came from the modulation itself, and
  the polarity bit is not doing what `lf_psk1_build_sequence` assumes

⛔ Do not start writing a fix before that distinction. They lead to completely different
repairs, and one of them means the shipping `psk1.c` is wrong rather than merely unlocked.

## 3a-note. Upstream impact

`idteck.c` ships a PSK1 emulation that does not work. That is worth reporting upstream
independently of anything here, and it means **emulation must not be part of an Indala PR**
until the transmit path is fixed — see §9.

## 3b. ⭐⭐⭐ FIX THE HID PROX AND PAC READERS — both fail on loud tags

⭐⭐ **START HERE: the SAADC readers duplicate a capture path that one of them gets right.**
The LF readers are two families, and it matters which:

| family | readers | capture |
|---|---|---|
| GPIO/comparator | em410x, jablotron, viking | `register_rio_callback`, 128-entry ring, no SAADC |
| **SAADC** | **hidprox, ioprox, pac** + lf_reader_generic | own `saadc_cb`, own 6144 ring, own field start/stop |

Only `lf_reader_generic.c` also suspends BLE advertising — and its own comment records why:
a burst collapses the 125 kHz field for ~1.6 ms and hit **4 captures in 10**. The three that
duplicate the prologue instead of sharing it are HID, ioProx and PAC, and HID and PAC are
exactly the two measured failing on loud tags. The SAADC reader that HAS the guard is Indala,
at 60/60.

⇒ `capture_begin()`/`capture_end()` already exist and are already shared by two entry points.
Moving HID onto them is a small mechanical change that also happens to be **the clean test of
C47** — same protocol, same tag, same bench, one variable.

⛔ **Do not read the em410x 95% as evidence either way.** It is on the GPIO path and never
touches the SAADC, so it cannot test this. That mistake is why C47 was wrongly weakened in
L64.


Not this project's decoder, but it is the comparison instrument for everything here and it
has cost two measurements already (L51's uninterpretable run, and C44's near-miss).

**What is established:** three tags with byte-identical memory read 0/6, 3/6 and 7/9 on the
Chameleon and 3/3 on a Proxmark (C46). The RF path is flat while reads fail (C45). So the
decoder's tolerance is narrower than the Proxmark's, and package-level differences cross it.

⚠ **`lf pac read` has the same disease**: 0/5 on one unit and 2/5 on the other while its tag
sat at 16x the empty floor and a Proxmark read it perfectly (L66). So this is not one broken
decoder — HID Prox and PAC both fail on tags that are loudly present, and `lf em 410x read`
sits at 95% (76/80) rather than 100%. ⇒ Whatever is wrong may be shared across the LF reader
family rather than specific to FSK. Fix HID first because it fails hardest (0/6, one tag
never reading at all), but measure PAC in the same session — a fix that moves both is a very
different fix from one that moves only HID.

⚠ The Indala reader is the only LF reader in this tree with its own capture path
(`raw_read_samples`, which suspends BLE and hands the decoder a whole buffer). It is also the
only one at 100% — 40/40 today across two units. That may be the cleanest clue available, or
it may be that Indala is simply the only one anybody has tuned. Do not assume which.

**⭐ Test this first — it is one line and it explains an old observation.**
`advertising_stop()` appears in **1 of 15** LF reader files. Only `lf_reader_generic.c`
suspends BLE advertising; `hidprox_read()` does not. That file's comment records the
measurement that put it there: an advertising burst collapses the 125 kHz field for ~1.6 ms
and hit **4 captures in 10**. And it predicts the thing nobody could explain in L51 —
*"after connecting it to my phone and/or a reboot, it does read"* — because connecting a BLE
central is precisely what stops advertising (C47).

```bash
# the discriminating test, ~5 minutes, needs the BLUE DUAL (the 0/6 tag — the others
# have too little headroom to show an improvement)
cd software/script && for i in $(seq 1 15); do .venv/bin/python cu.py "lf hid prox read" | tail -1; done
# then connect a phone over BLE so advertising stops, and repeat
```

⚠ **If it works, resist generalising it.** It cannot explain the blue dual reading 0/6
deterministically — an intermittent field collapse does not produce a clean zero. Expect two
causes: a BLE-induced intermittency affecting every LF reader, and a per-tag waveform
tolerance in the FSK demodulator. ⇒ The fix belongs in `capture_begin()`-style shared code so
all 15 readers get it, not pasted into `hidprox_read()` alone.

⚠ **Until it is fixed, do not use HID Prox as the probe tag for a null.** Use amplitude
(`lfprobe.py`) for presence, per §3.

## 3a. ⛔⛔ INDALA EMULATION TRANSMITS BUT ITS PHASE IS DESTROYED — and the modulator is innocent

Sniffed from the second Chameleon in reader mode, against an empty control (C63, C64, C65, L71).

| | fc/2 skirt | longest constant-phase run | frame autocorrelation |
|---|---|---|---|
| real tag | 201450 | **912 samples** (28.5 bits) | lag 2048, r=+0.41 |
| **emulator, front** | 62524 | **102 samples** (3.2 bits) | lag 2070, r=+0.34 |
| emulator, other side | ~5000 | 45 | r=+0.02 |
| empty | 6399 | 34 | r=+0.01 |

It IS modulating, at very nearly the right frame rate. The 28-zero preamble needs a 896-sample
constant-phase run and there is nothing longer than 102.

**Ruled out:**
- ⭐ **The modulator.** Host-tested in the `ctest` style: `lf_psk1_rf32_modulator` on
  `a0000000e6bd0e92` gives polarity-per-bit IDENTICAL to the frame, all 16 entries per bit
  agreeing, counter_top 16, duty 8. It is correct.
- **The data path.** `lf indala econfig` reads the frame back off the device.
- **NEXT §3c** (stale PWM clock) — a power cycle did not fix it, and an 8x-slow clock would
  show a 7.8 kHz subcarrier, which is not what the spectrum shows.

⚠ **LEADING HYPOTHESIS, UNTESTED: a PWM cannot emulate PSK1 because it is not phase-locked to
the reader.** A real T5577 *divides the reader's own field* to make its fc/2 subcarrier, so
the subcarrier is coherent with the carrier by construction — and that coherence is the whole
premise of this project's decoder: mix by (-1)^n, no oscillator, no phase estimate, because
fc/2 IS fs/2. A PWM free-running from the emulator's own crystal has no such relationship, so
the phase rotates and the data is destroyed while the envelope timing survives. Which is
exactly the measured shape.

⛔ **TEST IT WITH IDTECK BEFORE BELIEVING IT, AND BEFORE WRITING ANY CODE.** IDTECK shares the
entire transmit path, and nothing in this tree records it ever being verified end to end. If
IDTECK emulates correctly, the hypothesis is wrong and something Indala-specific is at fault.
If IDTECK is equally broken, PSK1 emulation has never worked on this device and the fix is a
design change — deriving the subcarrier from the field — not a patch.

⚠ Do not skip to "make the PWM coherent". Nothing has yet established that the nRF52 PWM
*can* be locked to the recovered carrier, and the LF path may not even expose a usable clock
to lock to. Establish the failure first.

## 3c. ⛔ FIRMWARE BUG: changing a slot to a PSK1 type while emulating leaves the PWM clock wrong

**Affects IDTECK as much as Indala — pre-existing, not introduced by this work.**

`pwm_init()` picks the PWM base clock from the CURRENT tag type:

```c
cfg.base_clock = IS_PSK1_TYPE(m_tag_type) ? NRF_PWM_CLK_1MHz : NRF_PWM_CLK_125kHz;
```

but it is called only from `lf_sense_enable()`, which runs only on a
`LF_SENSE_STATE_{NONE,DISABLE} -> ENABLE` transition. Changing `m_tag_type` afterwards — which
is exactly what `hw slot type` does — never re-inits the PWM. The clock keeps whatever value
it had when sense was last enabled.

Every PWM entry is `counter_top / base_clock` with counter_top = 16:

| base clock | entry | subcarrier | bit (16 entries) | |
|---|---|---|---|---|
| 1 MHz (PSK1) | 16 µs | 62.5 kHz | 256 µs | correct — fc/2 at RF/32 |
| 125 kHz | 128 µs | 7.8 kHz | 2048 µs | **8x too slow, unrecognisable** |

Both directions are broken: select a PSK1 type while a non-PSK1 one is live and the
subcarrier is 8x slow; select a non-PSK1 type while PSK1 is live and everything ASK/FSK runs
8x fast.

⚠ **Real by inspection, NOT yet confirmed as the cause of any symptom.** It was found while
debugging silent Indala emulation, and the mode cycle that should prove it had not been run
when this was written. Do not close it by assuming it explains that; and do not assume it is
the ONLY thing wrong with PSK1 emulation — see the control test below.

**Workaround:** `hw mode -r` then `hw mode -e`, or reboot, after changing the slot type.

**Fix:** re-init the PWM when `IS_PSK1_TYPE(m_tag_type)` changes — either in the LF loadcb
when the new type's PSK1-ness differs from the live one, or by cycling sense on slot change.
⚠ `nrfx_pwm_uninit`/`init` mid-emulation needs care: `lf_sense_disable()` also releases the
HFXO request and nulls `m_pwm_seq`, so a naive disable/enable would drop the loaded sequence.

⛔ **The control test this needs, and it should have come first:** set a slot to **IDTECK**
and see whether a Proxmark reads it. IDTECK shares the entire transmit path, and nothing in
this tree records it ever having been verified end to end — `idteck.c` documents only the
READ side as unimplemented. If IDTECK is silent too, PSK1 emulation never worked and the
Indala addition inherited a broken base, which is a much larger problem than `indala.c`.

## 4. ✅ A second Chameleon — reads 20/20 at the same phase

Same firmware, same copper coin, phase 20, one capture each, no gate rejections. Only the bit
offset moves (9 -> 10), which is C51's timing showing it depends on the reader too. ⚠ The
caveat §4 was written with still stands: same batch, same revision, so this is unit-to-unit
tolerance and not design generality. A pass was always going to be weak evidence.

## 5–6. ✅ CLOSED AS UNMOTIVATED — there is no deficit left to hunt

⛔ **Read this before re-opening any of the signal-hunting work.** §2's remaining entries
(`LF_RSSI`/AIN0, SAADC gain), §5 (make the failure cheaper) and §6 (re-test the levers) all
existed for one reason: the read did not work and we were looking for missing signal. Both
causes turned out to be elsewhere — ~26 dB was the tag being on the wrong side of the device
(C36) and the rest was the decoder demodulating PSK2 against a PSK1 tag.

On hardware now: **60 of 60 reads succeeded, on two tags and two units, every one of them at
the FIRST phase in the rotation, from a single capture.** There are no failures to make
cheaper, no rotation order to tune, and no deficit for a better signal tap to close.

| | why it is closed |
|---|---|
| `LF_RSSI` / AIN0 (C08, C09) | was a search for a better signal tap. Nothing needs one |
| SAADC gain (C10) | same. The conclusion may or may not hold; it no longer matters |
| Rotation order (§5) | entry 1 wins 60/60. There is nothing to sort |
| Longer settle (§5) | paid per capture, and reads take one capture |
| Air gap / settle / oversampling (§6) | all closed against the broken decoder, all hunting the same phantom |

⚠ They become live again **only** if someone cares about the back-side case, where the read
is 32% and stacking is doing real work (C58). That is the wrong placement, so caring about it
is a product decision, not a technical one. ⇒ Do not spend a day here without that decision.

## 7. ✅ RETIRE the per-lever sweep scripts — do not port them

`sweep.py`, `phasesweep.py`, `gaintest.py`, `gapsweep.py` and `oversample_test.py` all score
the fc/2 **skirt**. C38 measured skirt and decode on the same 160 captures and they are
**uncorrelated**: the highest skirt in the sweep (tick 44) decodes 5/5, the lowest (ticks
116–120) also decodes 5/5, and the dead band sits in the middle of the range. The earlier
hope that they "can rank coupling even if they cannot tell you whether something decodes"
does not survive that — over a sample-phase sweep the skirt is dominated by transition
energy, not coupling.

⇒ Anything worth keeping from them should be rebuilt on `phasebits.py`'s pattern: count
decodes, against an empty arm at the same setting. `lfprobe.py` remains useful because it
measures a *ratio against the live empty floor* for coupling, which is a different job.

## 8. ✅ T5577 WRITE — works, 9 of 9 verified

`lf indala write` reads the tag before and after and reports VERIFIED / WRITE DID NOT LAND /
WRITE FAILED / WRONG DATA / CANNOT TELL. Measured by alternating between two words, so each
result proves a state change rather than a tag that already held the value: **9 consecutive
writes, all VERIFIED** (C60).

⇒ The question this section could never answer — *is the writer broken, or was the tag never
coupled well enough to be written* — resolves as **coupling**. The original failures predate
the placement discovery (L57) and were taken with the tag on the back, where writing, which
needs more field than reading, is the worst case of the worst placement.

⛔ And §8's own stated blocker was wrong. It said to add a T5577 block read before debugging
the writer. By the time that was written the verifier already existed: `lf indala read` is
60/60 across two tags and two units, and one successful read-back covers blocks 0, 1 and 2
together. A T5577 block read is still worth having for OTHER protocols — it was never on this
path.

⚠ What is NOT established: writes to a tag the reader cannot hear. Verification is only as
good as read coupling, which is why CANNOT TELL exists as a distinct verdict rather than
being folded into failure.

## 8b. ⚠ 32KB of the Indala reader serves only the wrong placement — a decision, not a bug

The reader holds **48KB static**: 8KB samples, 8KB scratch, and **32KB of stacking
accumulators**. C58 measured stacking at exactly 68.75% for N=1..5 on the front — no gain at
any depth, because a good phase decodes from one capture and a dead one never decodes — and
32% -> 72% on the back.

⇒ Two thirds of the reader's RAM is insurance for the placement users are told not to use.

| option | RAM | back-side read rate |
|---|---|---|
| as shipped | 48KB | 72% |
| int16 accumulators, cap stacking at 2 | 32KB | 52% (C58, N=2) |
| drop stacking | 16KB | 32% |

⚠ This is a product decision about whether back-side reads matter, and the numbers are here
so it can be made rather than drifted into. ⛔ Do NOT drop stacking without also re-checking
the straddle gate: C58 showed stacking REINFORCES the dead-band straddle, and the gate is
what holds it at 0 wrong — removing one without re-measuring the other is the dangerous move.

## 9. Upstreamable?

Nothing in `lf_indala_psk.c` is bench-specific and it has no nRF dependency. The pieces a
PR would need beyond what is here: emulation (`lf_tag_em.c` has a transmit-only `psk1.c`
already), `lf indala write` to T5577, and the tag-type registration that
`tag_base_type.h:61` leaves as a commented-out placeholder under `//////// PSK Tag-Talk-First 300`.

## Closed — do not re-open without new evidence

| | why |
|---|---|
| `LF_RSSI` / AIN0 as a signal tap | C08/C09: no fc/2, flat to 0.5 dB, though demonstrably alive |
| SAADC gain | C10: the floor is analog-referred and tracks gain |
| Folding at 2048 samples | C14: odd parity inverts the subcarrier every frame; it cancels the data |
| Indala parity as an acceptance gate | C19: it passed a frame that was wrong in 20 bits |
| A 12 kHz cutoff for the baseband filter | C16: the cutoff was never the point; the null at fs/2 was |
