# Findings — current knowledge

⭐ **This file holds no history.** It is rewritten freely whenever understanding changes, and
it states only what is believed **now**. What was believed before, and when, is `LOG.md`.
If a claim is not in the ledger below, it is not established.

---

## ⛔⛔ PLACEMENT: READ THIS BEFORE TRUSTING ANY NUMBER BELOW

**Every measurement in this project up to 2026-09-11 was taken with the tag on the BACK of
the Chameleon Ultra. The reading side is the FRONT, where the buttons are. It is worth
21x.**

That is not a caveat, it is the dominant term. The whole investigation ran at roughly 1/20
of the available signal — about 26 dB — which is most of the "31.2 dB below the Proxmark"
the project spent days trying to explain. The PSK1/PSK2 decoder bug was one cause of that
figure; the tag being on the wrong side of the device was the other, and larger, one.

⚠ The Flipper Zero reads from its back, which is where the assumption came from. The
Chameleon is the other way round.

| | tag on the BACK | tag on the FRONT |
|---|---|---|
| fc/2 skirt over the empty floor | 1.0–3.7x | **20–34x** |
| single-capture decode rate | 51/160 = 32% | **34/48 = 71%** |
| sample phase 0 (the stock trigger) | 0/5 | **3/3** |
| sample phases 96–120 | 0/5 | **3/3** |
| the weak "white" coil | 0/12, never read | **reads first try** |

⇒ Claims measured on the back are marked **⚠B**. They are not automatically wrong — many
are relative comparisons that survive a common scale factor — but none of them has been
confirmed at the signal level the device actually delivers, and several are now known to be
artefacts. `NEXT.md` §1 is the re-measurement.

---

## The result

**The Chameleon Ultra reads Indala, on the device, as a command** — with the tag on the
**front**, 71% of single captures decode and every sample phase outside 56–88 works,
including the stock phase 0.

```
lf indala read   ->   Indala PSK1
                      Raw: a0000000e6bd0e92
                      Fmt 26 FC: 52 Card: 63612 Parity: 11
```

20 of 20 consecutive reads returned the credential, in 0.41–0.55 s each. The whole
demodulation is integer arithmetic on the nRF52840 — no float, no FFT, 8 KB of buffer.

Offline, the same decoder recovers the credential from 51 of 160 single 300 ms captures,
and the empty field produced a frame **0 times in 160**. No stacking, no folding, no
averaging.

## The decode, end to end

Everything below is integer arithmetic over 4096 samples. No float, no FFT.

```
sample at 125 kHz, carrier-locked, phase in the working window
  -> mix by (-1)^n                    fc/2 is exactly fs/2, so this is the whole mixer:
                                      no oscillator, no phase estimate. DC and slow drift
                                      move UP to fs/2 and fall out.
  -> [1,2,1] notch at fs/2            NOT optional, see C03. And it is a NOTCH, not a
                                      cutoff: its job is to remove what the mix just
                                      moved up there, see C16.
  -> 32-sample boxcar per bit         the optimal filter for a rectangular bit at RF/32
  -> threshold: bit = (integrator > 0)  PSK1 — the phase IS the data, see C02
  -> search preamble64 = 1010 + 28 zeros + 1, normal and inverted
  -> read 64 bits from the preamble position
```

⚠ **Three details are each individually fatal.** Any one of them alone takes the decode
from 51/160 to 0/160 or near it: the PSK1 mapping (C02), the baseband filter (C03), and
**not** discarding the settle window (C04).

⚠ **And a fourth detail is fatal to trusting the answer rather than getting one.** One in
five recovered frames is WRONG (C17). A reader that returns the first frame it decodes
returns a wrong credential about 20% of the time, so the firmware requires two captures to
agree (C18).

## Claims ledger

Every load-bearing claim, with the evidence that supports it. **`indep` is the column this
project learned the hard way**: whether the claim was checked against something that does
not share its own code path. The retracted `+8.2 dB` had n=20 and a null — and no
independent check, which is exactly why it survived for a day.

`null` — was a control run, and what kind. `emptyfield` is the real control here; `white`
noise is a much weaker one against a chain whose floor spans 17x across bands.

### Holds

| id | claim | n | null | indep | ref |
|---|---|---|---|---|---|
| C01 ⚠B | The Chameleon Ultra decodes Indala from one 300 ms capture: 51/160 exact | 160 | emptyfield 0/160 | ✓ Proxmark reads the same tag | L44, L46 |
| C02 | **PSK1: the phase IS the data.** Not differential — differential is `psk1TOpsk2`, the Proxmark's *fallback* | — | — | ✓ `cmdlfindala.c:1259` vs `:1293` | L44 |
| C03 ⚠B | The baseband filter is load-bearing: **51/160** real captures decode with it, **0/160** without | 160 | — | ✓ real captures, not synthetic | L44, L46 |
| C04 ⚠B | Discarding the 400-sample settle window is fatal: **43/160** with the full capture, **0/160** without | 160 | — | ✓ real captures | L44 |
| C05 | The preamble needs an exact search, not a correlator — the template is dominated by a 28-bit constant run and the peak lands a nibble out | — | — | ✓ Proxmark `preambleSearch()` | L44 |
| C06 ⚠B | Working phase window is ticks 4–60; best 12–36. **Stock phase 0 decodes 0/5** | 5/phase | emptyfield | — | L44 |
| C07 ⚠B | Works at the stock 8-bit sample width; the 16-bit path is not required for the read | 1 | — | — | L44 |
| C08 ⚠B | `LF_RSSI`/AIN0 carries no fc/2: tag/empty 1.04x inside a 1.28x scatter, flat to 0.5 dB across 1–62 kHz vs AIN5's 31.6 dB rolloff. Not clipped | 7 | emptyfield | — | L39 |
| C09 ⚠B | …but AIN0 is alive: the tag shifts its DC by +16 counts with **±0 spread over 7 repeats** | 7 | emptyfield | — | L39 |
| C10 ⚠B | The fc/2 noise floor is analog-referred, not ADC-referred — it tracks gain at or above the gain ratio | 7 | emptyfield | — | L22 |
| C11 ⚠B | Captures are frame-locked to field-on: they cross-correlate at lag 0 with one sign; empty captures scatter with mixed signs | 7 | emptyfield | — | L40 |
| C12 ⚠B | Zero-offset cross-capture stacking gives √N: +7.3 dB at N=7, empty floor falling exactly 2.65x | 7 | emptyfield + arithmetic null | — | L40 |
| C13 ⚠B | The frame is visible in the sideband envelope — the 28-zero run as a reproducible null at the 2048-sample period | 7 | emptyfield | — | L40 |
| C14 | The word has **19 ones — odd parity** — so the subcarrier inverts every frame and the true repetition period is 4096 samples, not 2048 | — | — | ✓ arithmetic on the known word | L41 |
| C15 | Stock firmware *had* no PSK demodulator: `reader/lf/*_data.c` were all ASK or FSK, `psk1.c` transmit-only. `lf_indala_psk.c` is the first | — | — | ✓ source | L01, L47 |
| C16 ⚠B | **The filter's job is a NULL AT fs/2, not a low cutoff.** [1,2,1] beats the 12 kHz FFT brick wall 51 vs 43, strictly (McNemar b=0 c=8, p=0.008) — and [1,1,1], which smooths as hard but nulls at fs/3, is the worst of the set at 31/160 | 160 paired | emptyfield 0/160 at every variant | ✓ mechanism control: same smoothing, wrong null | L46 |
| C17 ⚠B | **One recovered frame in five is WRONG.** 24 bad frames from 200 captures across two sessions — and all 24 were DISTINCT, while the truth recurred 77 times | 200 | emptyfield: no frame at all, 0/160 | ✓ two sessions, two geometries | L46, L47 |
| C18 ⚠B | Requiring two captures to agree removes them: **0 wrong in 40000** resampling trials on either pool, at a median of 2–3 captures | 2x20000 | ✓ vs need=1 at 20/22% wrong | ✓ replicated on live captures taken after the design was fixed | L47 |
| C19 ⚠B | The Wiegand-26 parity is **not** a sufficient gate: it rejects 13 of 17 bad frames but passed `a0000000b9be47a4`, which is wrong in 20 bits | 200 | — | ✓ a counter-example, not a rate | L47 |
| C20 | The firmware read works: **20/20** consecutive `lf indala read`, 0.41–0.55 s each, integer-only on the nRF52840 | 20 | emptyfield **0/20**, on the device | ✓ agrees with the Proxmark's read of the tag | L47, L48 |
| C26 ⚠B | A second, Proxmark-verified Indala tag reads **0/12** on the Chameleon. Its fc/2 skirt sits at 1.04–1.12x the empty-field floor where the bench tag sits at 1.53–1.61x. ⚠ **Revised: "inaudible" was too strong** — see C28 | 12 phases | ✓ empty-field captures at the *same* phases | ✓ the Proxmark wrote, verified and read the tag back; the Flipper reads both tags | L53, L55 |
| C36 | ⭐⭐ **The reading side is the FRONT and it is worth 21x.** Same coil, same session: 1.0–1.1x the empty floor on the back, 20–34x on the front. The coil that read 0/12 reads first try | 48 front + 32 back | ✓ empty floor unchanged (no tag either way) | ✓ found by an instrument built for something else, then confirmed by a read | L57 |
| C37 | ⭐ **On the front the phase structure is TWO WORKING BANDS, not a window.** Phases 0–56 and 96–124 decode 5/5; 60–92 is dead. 114 of 160 single captures return the truth (71%) and **0 of 160 empty captures produce a frame at all** | 32 phases x 5, tag and empty, shipping C decoder | ✓ empty arm at every phase, same session | ✓ numpy decoder agrees on the band structure (107/160, dead band 60–96) | L57, L58 |
| C32 ⚠B | ⭐ **The two coils differ by 3.3x in the fc/2 band** — copper 3.55–3.67x the empty floor, white 1.04–1.12x — same session, same geometry, same payload, same phases | 8 x 4 phases each | ✓ empty-field reference per phase | ✓ both read on a Flipper; a Proxmark wrote and verified both | L56 |
| C33 ⚠B | ⚠ **Position is worth more than 2x.** The copper coil reads 3.55–3.67x now against 1.53–1.61x in the original sweep — same tag, same reader, different placement. That is larger than the whole working margin | 8 x 4 | ✓ same empty reference | — | L56 |
| C34 ⚠B | Stacking gain is **1.5–2.1x on the copper coil and 1.0x on the white one**, so the white coil's captures are not coherent with each other in the way the copper coil's are | 8 x 4 | ✓ ratio is against a stacked empty, so a deterministic background cancels | — | L56 |
| C35 ⚠B | ⛔ **The lag structure is the same for both tags** — 4 of 7 captures at lag 0, 3 at ≈−47 samples, near-identical for copper and white. So the cross-capture correlation is a property of the CAPTURE PATH, not of either tag's frame | 7 pairs x 2 tags | ✓ the two tags are the control for each other | — | L56 |
| C28 ⚠B | ⭐ **That tag IS heard — the band ratio is simply not sensitive enough to see it.** Stacking 8 captures yields `a0000000e6ad0e92` (1 bit out), `a0000000e4bd0a92` (2), `a0000000e33d0e92` (3) at three separate phases, against a truth of `a0000000e6bd0e92`. Stacked EMPTY captures produce no frame at all, ever | 4 phases x 8 | ✓ emptyfield 0 frames at every stack depth | ✓ the exact 33-bit preamble is what noise never produces (0/160) | L55 |
| C29 ⚠B | ⭐ **Cross-capture stacking works: 31.9% -> 71.9% correct**, over every combination of the committed captures, with **0 empty-field frames at every depth**. It revives sample phase 0 — the stock trigger, 0/5 singly — and phase 64, outside the single-capture window | 160–320 per depth | ✓ emptyfield 0 at N=1..5 | ✓ decoded by the C firmware decoder, not the Python one | L55 |
| C30 ⚠B | ⛔ **…but it does nothing for the weak tag**: bench 1.56x -> 2.71x stacked, white coin 1.08x -> 1.05x. The cross-capture alignment stacking depends on is not holding for that tag | 4 phases x 8 | ✓ same treatment, same phases | — | L55 |
| C31 ⚠B | ⛔ **C11/C12 need re-examining: the EMPTY field correlates better than either tag.** Median lag-0 baseband \|r\| is 0.75–0.83 empty, 0.18–0.49 bench, 0.15–0.22 white — so that correlation is measuring the field turn-on transient, not frame lock | 10–28 pairs x 4 phases | ✓ empty is the control and it *wins*, which is the finding | — | L55 |
| C27 ⚠B | ⇒ **The working margin is tiny: ~1.5x over the floor reads, ~1.1x does not.** There is almost nothing between "works every time" and "never" | 12 | ✓ per-phase empty reference | — | L53 |
| C25 ⚠B | **The RF path does not degrade under heavy LF load.** An HID tag's fc/8+fc/10 amplitude is flat to **1.00x** across idle, sustained load and recovery, with the carrier DC flat to 0.3% | 22 | ✓ idle arms before and after the loaded one | ✓ a continuous measurement, not the binary read whose swings prompted it | L52 |
| C24 ⚠B | **No false positive on a non-Indala tag**: an HID Prox 36-bit tag on the antenna gives `LF tag not found` 10 times in 10, with coupling confirmed by a 5/5 HID read immediately before | 10 | ✓ the loud-signal null, which the empty field does not test | ✓ a second run of 10 by the user, same result | L50 |
| C23 | The decoder is **word-agnostic**: 36 synthetic words all decode, no wrong answers at any amplitude. Odd-parity mean threshold 23.5, even 24.5 — one ladder rung apart | 36 words x 20 seeds | ✓ control = the bench word, generated by the same code | ✓ the generator is written from the physics and validated by the C decoder, which was validated on 320 real captures | L49 |
| C22 | The empty-field failure is genuine **timeout exhaustion**, not an early abort: 0.47–0.53 s of device time against a 500 ms budget, where a success takes 0.08–0.22 s | 5 vs 20 | ✓ the 0.33 s host floor measured separately and subtracted | ✓ timing, independent of the decoder's own verdict | L48 |
| C21 | The C decoder and the numpy decoder agree **word for word on all 320 committed captures**, including the failures | 320 | — | ✓ *this is the independent check* — integer vs float, notch vs FFT, no shared code | L47 |

### Firmware bugs found and fixed

| id | bug | effect | ref |
|---|---|---|---|
| F01 | The 8-bit debug path did `14-bit >> 5` | discarded 5 bits **and distorted the rolloff shape** | L10 |
| F02 | BLE advertising collapsed the LF field during capture | 41% of captures corrupted → 0% | L26 |
| F03 | The DMA ring dropped 75% of every batch (`CIRCULAR_BUFFER_SIZE` 512 vs `ADC_BUF_SIZE` 2048) | captures were islands of 512 samples spanning 64 ms, not 16 ms | L01 |

### Retracted

| claim | why it was wrong | retracted by |
|---|---|---|
| **"the working phase window is ticks 4–60; the stock phase 0 decodes 0/5"** (C06) | ⛔ a placement artefact. With the tag on the FRONT, phase 0 decodes 3/3 and so do phases 96–120. The surviving dead zone is 56–88, and it carries the HIGHEST skirt of the sweep — so it is a genuine polarity null, not weak signal | L57 |
| "the working margin is tiny — ~1.5x over the floor reads, ~1.1x does not" (C27) | ⛔ measured entirely on the back. On the front the same coils sit at 20–34x | L57 |
| "a second Indala coil is inaudible / 3.3x weaker" (C26, C32) | ⛔ both coils were on the back. On the front the weak one reads first try | L57 |
| "tag position is worth ~5.7 dB" | ⛔ understated by a factor of ten. Side of the device alone is 21x, ~26 dB | L57 |
| "fc/2 sits at Nyquist, therefore it cancels" | the Proxmark samples once per carrier cycle too, and reads Indala | L03 |
| "the Indala tag produces no detectable modulation" | measured through the `>>5` truncation | L10 |
| "phase sweep shows 659x" | tracking overrun bursts; small denominator | L15 |
| "31.2 dB below the Proxmark ⇒ not viable" | the **measurement** stands; the conclusion assumed a decoder that could not work | L44 |
| "7.6 dB demodulation gap, confirmed twice" | both routes measured that broken decoder | L44 |
| "detectable but not decodable, 52/64 vs a 45/64 null" | a constant preamble run scoring itself — and the wrong decoder besides | L42, L44 |
| "matched filter worth +8.2 dB over PSKDemod" | measured on a synthetic that shared the decoder's own bug | L44 |
| "the phase is gone before the ADC" | the injected synthetic shared the decoder's convention; it was testing self-consistency | L44 |
| "coherent frame averaging is dead" | the circularity argument was wrong — captures are frame-locked, so there is no alignment step | L40 |
| "folding at 2048 samples improves things" | it averages a frame against its own inverse; only the polarity-blind skirt survives | L41 |
| "settle has no effect" / "air gap is flat" | both measured pre-BLE-fix on a tag carrying `DEADBEEF/12345678` | L34 |
| "the baseband low-pass needs a ~12 kHz cutoff" | the cutoff was incidental. What the filter must do is NULL fs/2, where the mixer just put the carrier ripple; a 3-tap notch does it better and cheaper (C16) | L46 |

### Open / untested

| claim | status |
|---|---|
| Tag position is worth ~5.7 dB | n=1, from an accidental probe. Large, concentrated at high frequency, and plausible — but one capture |
| Settle, air gap, oversampling | closures invalid (L34); never re-measured against a working decoder |
| Does a second **physical** tag read? | ⛔ **NO** — C26, and it is silicon not payload: both tags carry the same word, both read on a Flipper, the thin copper coin reads on the Chameleon and the white coin does not. C28 shows its signal is present but 1.4x weaker, which on a ~1.5x margin is the whole difference |
| Why does stacking not rescue the weak coil? | ⛔ open (C34). Best current story: a weakly-coupled T5577 charges more slowly and more variably, so its frame start jitters between captures and there is nothing to add coherently. ⇒ align on the PREAMBLE the decoder already finds, rather than assuming lag 0 |
| What is the common background? | ⛔ open and it defeats every filter tried (C31, C35). Empty captures correlate at 0.92–0.95 after mixing, band-limiting to 6 kHz, removing DC and dropping 1024 samples. Not the odd/even imbalance (1.7–2.1 counts, 0.1% of the ripple). Until it is identified, no cross-capture correlation on this bench means what it appears to mean |
| Can the Chameleon WRITE a T5577 reliably? | ⛔ not known to work — one of three raw blocks landed, and the proven multi-block path changed nothing. Undiagnosable without a T5577 block read, which the device does not have. `NEXT.md` §7 |
| C38 | ⛔⛔ **THE SKIRT DOES NOT PREDICT DECODE — it is anti-correlated with it over half the sweep.** The fc/2 skirt is a smooth single-peaked curve: **highest at tick 44 (212), which decodes 5/5**, lowest at 116–120 (134), which also decodes 5/5. The dead band 60–92 sits in the MIDDLE of the skirt range (145–200). ⇒ the sweep scripts in NEXT §7 score a quantity that cannot rank what they were used to rank | 32 phases x 5, skirt and decode measured on the same captures | ✓ decode and skirt come from the identical capture set | ✓ predicted by M8 (the skirt is polarity-blind); this is the direct test | L58 |
| C39 | ⛔⛔ **IN THE DEAD BAND THE DECODER RETURNS A DETERMINISTIC WRONG FRAME, WHICH DEFEATS THE TWO-CAPTURE AGREEMENT RULE.** Phase 64 returns `a0000000b5af0b92` on **5 of 5** captures, phase 88 `a0000000c6b90c92` on **5 of 5**, phase 92 `a0000000c6b90e92` on 4 of 5. Two independent captures therefore AGREE on a wrong credential and `indala_read()` returns it. Mechanism: the winning bit alignment is **off 16 — exactly half the 32-sample bit period** — so every integrator straddles a bit boundary and blends two adjacent bits, at ~half the amplitude ⚠ Wiegand-26 parity rejects two of the three but **passes `a0000000c6b90e92`**, which reports the CORRECT facility code 52 with a wrong card number | 5 captures at each of 32 phases | ✓ the 22 good phases produce 0 wrong frames in 114 decodes | ✓ mfdemod.py independently produces wrong words at the same phases | L58 |
| C40 | ⭐⭐ **MORE SIGNAL MADE THE FAILURE MODE WORSE, NOT BETTER.** On the back (26 dB down) all 17 wrong frames were DISTINCT — errors were noise-driven, so they scattered, which is exactly what C17/C18 measured and built the acceptance rule on. On the front, 3 wrong words repeat within a phase, two of them 5/5. ⇒ **"wrong words never repeat" is a property of low SNR, not of the decoder**, and the rule it justifies does not cover the strong-signal case | 160 back + 160 front tag captures, same decoder | ✓ same tag, same decoder, same capture length; only placement differs | ✓ the two capture sets were taken weeks apart | L58 |
| C41 | ⚠ **C03 HAS REVERSED SIGN: the fs/2 notch now costs decodes.** Shipping C decoder on the front: notch ON 114 truth / 21 wrong, notch OFF (plain boxcar) **121 truth** / 26 wrong; empty stays 0/160 both ways. On the back it was 43->51 and strictly dominant. The mechanism it was justified by — ~9 counts of carrier ripple leaking into a ~10-count subcarrier — is gone at 20x the subcarrier. ⇒ no longer load-bearing; it now trades ~7 decodes for ~5 fewer wrong frames | 160 front captures, both builds | ✓ same captures, same binary except the 3-tap fold | ✓ numpy decoder reproduces the reversal (107 with lpf, 121 without) | L58 |
| C42 | ⭐ **C04 SURVIVES INTACT AND IT WAS NEVER ABOUT SNR.** Discarding a 400-sample settle window from the capture still takes the decode from 107/160 to **0/160** on the front, unchanged from the back. It is structural: 400 samples removes the first frame's preamble and leaves too few bits after the second. ⇒ the only one of the three "individually fatal" details that is fatal for a reason 26 dB cannot touch | 160 front captures, cut 0 vs cut 400 | ✓ same captures, only the cut differs | ✓ same result as the back-side measurement | L58 |
| C43 | ⚠ **Integrator amplitude separates truth from wrong with no overlap here — but it is not a gate.** Truth 7240–12699, wrong 5120–6833. The gap is real and matches the half-bit mechanism (a straddling integrator recovers ~half the energy), but amplitude scales with coupling, so a weakly-coupled tag would fall under any fixed threshold. ⇒ a diagnostic, not an acceptance test | 135 frames from 160 front captures | ✓ both classes from the same sweep | — | L58 |
| The `lf hid prox read` 0/15 episode | ⛔ **unexplained, and not reproducible.** It sat at 0/15 for ~15 minutes, survived 150 s of rest, then cleared on its own. C25 excludes the antenna, the field, coupling and thermal drift; the control arm excludes `lf indala read`. What remains is the hidprox decoder or something above it. `lfprobe.py` will catch it in the act if it recurs |
| `lf hid prox read` lock rate | fails ~15-20% on a signal C25 shows is present and constant — an existing decoder issue, unrelated to this work, and a poor instrument to measure anything else with |
| Does `lf indala read` false-positive on a **non-Indala tag**? | HID Prox closed (C24). ⚠ EM410x, ioProx, Viking, PAC and Jablotron are all still untested, and ASK/OOK tags modulate the envelope differently from HID's FSK |
| Is the phase window tag- or unit-specific? | ⚠ worse than that — it is **not stable across sessions on the same tag and unit**. Phase 12 was 5/5 in the sweep and 4/10 correct a day later; phase 28 went the other way. ⇒ do not hard-code a phase, rotate. Still untested on a second tag or unit |
| Indala parity | reported, deliberately **not** a gate — see C19. It only covers format 26, and a badly wrong frame passed it |

## What runs on the device

| | |
|---|---|
| `rfid/reader/lf/lf_indala_psk.c` | the demodulator. Pure integer, no nRF dependency, so it host-compiles for `research/indala-psk-read/ctest/` |
| `rfid/reader/lf/lf_indala_data.c` | capture, phase rotation, the two-capture agreement rule |
| `rfid/reader/lf/lf_reader_generic.c` | `capture_begin`/`capture_end` extracted so `lf sniff` and the Indala read share one copy of the BLE suspend, the ring and the settle discard |
| `DATA_CMD_INDALA_SCAN` = 3033 | `lf indala read` |

Cost: 8 KB of `.bss` for the sample buffer, ~1 KB of stack, 92 KB of RAM still free.
The decode is ~32 x 4096 adds and runs inside the inter-capture gap.

⭐ **The filter is free.** Summing a [1,2,1]-filtered signal over a 32-sample window is
identically a weighted sum of the *unfiltered* signal over 34 samples with weights
`1,3,4,4,...,4,3,1`, so the notch folds into the bit integrator as

```
integrator = 4*sum(y[a..a+31]) + y[a-1] - y[a] - y[a+31] + y[a+32]
```

— four extra adds per bit rather than three per sample, and no second buffer.

## Hardware reference

```
ANT -> VD1 detector -> LF_OA -> [C28 10n / R9 82 / C36 33n] -> IC1A (R17 4k7 / C38 1n)
                         |                                      -> IC1B -> AIN5 (P0.29)
                         \-> R12 470k -> LF_RSSI -> AIN0 (P0.02)
```

- Filter poles: R9/C36 = **58.8 kHz**, R17/C38 = **33.9 kHz**. At fc/2 = 62.5 kHz they cost
  −3.3 dB and −6.4 dB.
- `READER_POWER` (P1.15) is the schematic's `LF_AMP_PWR`; it feeds the bias divider
  R8 4k7 / R10 3k → LF_VBIAS ≈ 1.2 V.
- SAADC: 14-bit, PPI-triggered from the carrier PWM `PWMPERIODEND`, `GAIN1_6` against the
  internal 0.6 V reference → 3.6 V full scale, `ACQTIME_5US`.
- AIN5 sits at ~1.2 V (33% FS). AIN0 sits at **3.51 V (97% FS)** — 1/6 is already the
  lowest gain the SAADC offers, so single-ended there is no setting with more headroom.

## The tag

`a0000000e6bd0e92` — Indala, 64-bit, Fmt 26, FC 52, Card 63612. PSK1, RF/32, subcarrier at
fc/2 = 62.5 kHz. T5577 block 0 = `00081040`.

⛔ **The campaign registry's `PSK1` cell writes `DEADBEEF/12345678`** — correct PSK1 air
shaping, but not an Indala frame. Use the `INDALA26` cell for anything demod-related;
misreading this invalidated a day (L31).

Confirm before trusting any measurement:

```bash
cd /Users/Shared/code/personal/rfid/proxmark3 && ./pm3 -c "lf indala reader"
```
