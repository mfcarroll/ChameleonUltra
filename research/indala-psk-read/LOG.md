# Log — what was learned, when, in order

⛔ **APPEND-ONLY.** The only permitted edit to an existing entry is appending a
`⛔ retracted by L##` pointer. Never reword an entry to match what you now believe — the
whole point of this file is that it records what was believed at the time.

**The detailed log is the git history.** Every entry below names its commit; `git show <hash>`
gives the full reasoning, the numbers and the controls. This file is the index: one line per
finding so the arc is readable without `git log`, and so retractions are visible at a glance.

**Current knowledge is `FINDINGS.md`** — that file is rewritten freely and holds no history.
Nothing here should be read as current unless `FINDINGS.md` also says so.

---

## Day 1 — 2026-09-10

| # | time | commit | finding |
|---|---|---|---|
| L01 | 18:28 | `e56692d` | Indala unreadable on Chameleon Ultra. Firmware has no PSK demodulator — `reader/lf/*_data.c` are all ASK or FSK, `psk1.c` is transmit-only. |
| L02 | 18:36 | `446622b` | Root-caused to the ADC sample clock: fc/2 sits at exactly Nyquist. ⛔ retracted by L03 |
| L03 | 18:47 | `c17fda7` | Nyquist diagnosis retracted — `lo_read.v:19` shows the Proxmark also samples once per carrier cycle and reads Indala fine. Front-end filtering is the leading cause. |
| L04 | 18:48 | `6a86533` | `fieldhold.sh` + a TP7 scope protocol. Unused — no scope available. |
| L05 | 18:57 | `13ead89` | A no-instrument T5577 PSKCF sweep designed as the decisive test. |
| L06 | 18:59 | `e1a07d0` | Reused the existing `t5577_campaign.py` harness; added a Proxmark reference leg. |
| L07 | 19:01 | `82b48c1` | One venv runs both halves; the two-interpreter claim (L07-pre) banded. |
| L08 | 19:25 | `c8080a7` | Harness leg landed, and four bugs it exposed along the way. |
| L09 | 19:50 | `4f6d8c4` | Sweep measured: the loss is analog, ~27 dB, and before the ADC. ⛔ superseded by L44 |
| L10 | 19:59 | `69a3d54` | ⭐ **FIRMWARE BUG:** the 8-bit debug path did `14-bit >> 5`, discarding 5 bits and distorting the rolloff shape. `--bits 16` added. |
| L11 | 20:16 | `ad47e4b` | macOS build+flash script. Homebrew's nrfutil cask is disabled for a Gatekeeper failure. |
| L12 | 20:23 | `69c50b2` | macOS flashing works via Nordic's direct download, as the wiki says. |
| L13 | 20:33 | `56143c9` | At 14 bits the front end is flat and the SAMPLER is the blocker. ⛔ superseded by L44 |
| L14 | 20:40 | `ee55356` | `lf sniff --phase`: programmable SAADC sample phase via TIMER3. |
| L15 | 20:47 | `a0d4c35` | Phase-sweep verdict retracted — it was tracking buffer-overrun bursts, and "659x" was a small denominator. |
| L16 | 20:51 | `91736c7` | Phase sweep lands properly: the sampler is real but worth ~7.5 dB of ~44. |
| L17 | 20:54 | `0f3c56b` | `sweep.py` aggregates repeats; deglitching alone revises RF/4 by 9 dB. |
| L18 | 20:58 | `d7aa812` | Repeated sweep: ~27 dB analog, ~7.5 dB firmware. ⛔ superseded by L44 |
| L19 | 21:04 | `9ce378e` | `lf sniff --rate`: free-running oversampled trigger. |
| L20 | 21:09 | `77d0d49` | Oversampling recovers nothing — but the first verdict was read off a `nan`. ⚠ measured on the wrong tag payload; see L33 |
| L21 | 21:16 | `4cef14b` | `lf sniff --gain`: SAADC gain control. |
| L22 | 22:29 | `6165462` | ⭐ **Gain closed:** the noise floor is analog-referred, so the ADC was never the limit. n=7, deglitched. Still holds. |
| L23 | 22:34 | `6edbf30` | `lf sniff --settle`; sweep says no effect on fc/2. ⚠ invalid conditions, see L36 |
| L24 | 22:36 | `29453ae` | Air-gap sweep, measured as response rather than raw amplitude. |
| L25 | 22:47 | `ddf0325` | Air gap closed — flat is already the optimum. ⚠ invalid conditions, see L36 |
| L26 | 23:01 | `e1a336d` | ⭐ **FIRMWARE BUG:** the "overruns" were BLE advertising collapsing the LF field. 41% of captures corrupted → 0%. |
| L27 | 23:06 | `370479c` | `sweep.py` takes baseline repeats and reports SNR against the floor. |
| L28 | 23:25 | `e3a15e3` | The clean PSKCF measurement: 31.2 dB excess at fc/2. ⛔ the measurement stands, the conclusion drawn from it is retracted by L44 |
| L29 | 23:35 | `62c3e4a` | Investigation summary and an adversarial review prompt. |
| L30 | 23:43 | `64cfefb` | Demod attempted — "~1.5 dB short of a working read". ⛔ retracted by L31 |
| L31 | 23:55 | `d159b15` | ⛔ **Demod result retracted — the tag was never transmitting Indala.** The campaign's `PSK1` cell writes `DEADBEEF/12345678`: correct PSK1 air shaping, not an Indala frame. Invalidated everything since the last campaign run. |

## Day 2 — 2026-09-11

| # | time | commit | finding |
|---|---|---|---|
| L32 | 00:05 | `d6edd2e` | Chunked retrieval — `NETDATA_MAX_DATA_LENGTH` is a hard 4096 and raising it faults the device. Captures now hold the two frames a demod needs. |
| L33 | 00:11 | `2d4a225` | The decisive test re-run on a correctly programmed tag: negative by 7.6 dB. ⛔ retracted by L44 |
| L34 | 00:16 | `7567f2d` | ⚠ The lever closures are not safe — settle, air gap and oversampling were all measured before the BLE fix and on the wrong payload. |
| L35 | 00:24 | `fe6c7ed` | Matched-filter demodulator: "+8.2 dB on white noise, no decode on real data". ⛔ retracted by L44 |
| L36 | 00:25 | `6a3f7a8` | `NEXT.md` — ranked next steps. |
| L37 | 00:28 | `8618110` | `RESUME.md` — session resume prompt. Later folded into `README.md`. |
| L38 | 00:41 | `fceeca4` | `lf sniff --input`: select AIN0/`LF_RSSI`, the only tap upstream of the filter poles. |
| L39 | 01:07 | `7f03d7e` | ⭐ **AIN0 closed:** carries no fc/2 (tag/empty 1.04x inside a 1.28x scatter; flat to 0.5 dB across 1–62kHz) but is demonstrably alive (+16-count DC shift, ±0 over 7 repeats). Still holds. |
| L40 | 01:07 | `7f03d7e` | ⭐ Zero-offset cross-capture stacking works: +7.3 dB at N=7, empty floor falling exactly √7. Captures are frame-locked to field-on. Still holds — but superseded in usefulness, the read needs no averaging. |
| L41 | 01:07 | `7f03d7e` | ⛔ Folding at 2048 samples **cancels the data** — the word has 19 ones, odd parity, so the subcarrier inverts every frame and the true period is 4096. |
| L42 | 01:07 | `7f03d7e` | ⛔ "52/64 bits vs a 45/64 null" retracted — half the frame is a constant preamble run and scoring it against 64 rotations gives up to 32 bits free. |
| L43 | 01:15 | `bd9a351` | `phasebits.py` — sweep phase scoring bit recovery rather than skirt amplitude. |
| L44 | 01:36 | `d4b2b4b` | ⭐⭐⭐ **SOLVED. The Chameleon Ultra reads Indala.** 43/160 single captures decode `a0000000e6bd0e92` exactly; empty field 0/160. Root cause: `mfdemod.py` demodulated **PSK2 against a PSK1 tag**, and `synth()` encoded with the same wrong convention so the self-test was self-consistent and passed forever. Retracts L09, L13, L18, L28-conclusion, L33, L35 and the whole "analog deficit" framing. |
| L45 | — | this commit | Notes restructured: `README` / `FINDINGS` / `LOG` / `METHOD` / `NEXT`, with the old working notes frozen under `archive/`. |
| L46 | 02:05 | `84b99a7` | ⭐ **The baseband filter's job is a NULL at fs/2, not a low cutoff.** A 3-tap [1,2,1] beats the 12 kHz FFT brick wall **51/160 vs 43/160**, and strictly — McNemar b=0, c=8, p=0.008. [1,1,1], which smooths as hard but nulls at fs/3, is the worst of the set at 31/160, which is the mechanism showing itself. Revises C03's numbers; the claim that the filter is load-bearing is unchanged (0/160 with none). |
| L47 | 02:40 | `84b99a7` | ⭐⭐⭐ **`lf indala read` runs on the device.** 20/20 consecutive reads of `a0000000e6bd0e92`, 0.41–0.55 s each, integer-only. The C decoder agrees with `mfdemod.py` word for word on all 320 committed captures. ⚠ And 40 fresh captures taken today exposed what the offline work had not: **one recovered frame in five is WRONG**, 24 of them across 200 captures and all 24 distinct — so the firmware returns a credential only when two captures agree. ⚠ The on-device empty-field null has NOT been run. |
| L48 | — | `3ea8d3e` | ⭐ **On-device empty-field null passed: 0 reads in 20**, closing C20. ⚠ And the timing had to be measured rather than eyeballed — a failed read and a successful one both land near 0.5 s of wall clock. Subtracting the 0.33 s host floor separates them: a failure spends 0.47–0.53 s on the device, the whole 500 ms budget, where a success spends 0.08–0.22 s. The rotation walks the full window and exhausts rather than aborting early. |
| L49 | — | `df463d6` | ⭐ **The decoder is word-agnostic** — 36 synthetic words, every one decodes, no wrong answer at any amplitude. Closes the structural half of generality: EVEN parity (never tested — C14's frame inversion only happens for odd), payloads of 31 zeros and 31 ones, and a preamble reproduced inside the payload. `generality.py`, written from the physics and validated by the C decoder rather than by a round trip. ⛔ Also fixed `stack.py`'s `psk_frame()`, which was dead — it called a name deleted in the PSK1 rename, and its body still ran the retracted PSK2 running XOR. An injection control that injects the wrong modulation reports "undetectable" and cannot fail visibly. |
| L50 | — | `d8fd7bd` | ⭐ **No false positive on a loud wrong signal.** An HID Prox tag on the antenna gives `LF tag not found` 10/10, with coupling confirmed by a 5/5 HID read immediately before, plus 10/10 from a separate run. Closes NEXT §1 for HID. |
| L51 | — | `d8fd7bd` | ⛔ **A claimed regression that was not one, and the rule it broke.** `lf hid prox read` went 5/5 before a burst of `lf indala read` and 1/5 after, and that was reported as a reproduced state-corruption bug. It was not: a **no-stressor control** swung 8/15, 11/15 and 6/15, and over 25 minutes the HID rate declined monotonically to 0/15 **through the control arm**, then did not recover after 150 s of rest while the field amplitude stayed put (mean 5447 vs 5438). ⇒ The physical tag state was never confirmed and the whole sequence is uninterpretable. `README`'s first warning says to confirm the tag state before trusting any measurement; it was written after this exact mistake cost a day, and it was not followed. |
| L52 | — | `ac3c84b` | ⭐ **The RF path does not degrade under load** — an HID tag's fc/8+fc/10 amplitude is flat to 1.00x across idle, sustained `lf indala read` load and recovery, carrier DC flat to 0.3%. That excludes detuning, thermal, coupling and field collapse for the L51 episode in about thirty seconds, and it is the measurement that should have been made first. `lfprobe.py`. ⚠ The 0/15 episode itself stays unexplained: not RF, not attributable to this work, self-clearing, not reproducible. `lf hid prox read` also fails ~15-20% on a signal that is demonstrably present and constant. |
| L53 | — | `ce1f9a2` | ⛔⛔ **A second Indala tag is inaudible to the Chameleon.** A spare T5577 written with `a0000000e6bd0e92` by a Proxmark — "Data written and verified", then read back as Fmt 26 FC 52 Card 63612 — decoded **0 of 12** on the Chameleon, and its fc/2 skirt sits at 0.89–1.11x the empty-field floor across twelve sample phases where the bench tag sits at 1.25–1.73x. The subcarrier is not there at all. ⚠ Cause unknown (position / tag / reader) and the control has not been run. ⇒ The working margin is only 1.25–1.73x over the floor, so anything worth more than ~1.3x makes a tag unreadable. |
| L54 | — | `ce1f9a2` | ⚠ **The Chameleon's T5577 writer is not known to work.** Three raw `lf_t55xx_write_block` calls landed exactly one block; routing through the proven `write_t55xx` (field held on, every block written twice) changed the spectrum not at all. Not diagnosable from this device — it has no T5577 read, so every attempt costs a Proxmark round trip. `NEXT.md` §7. |
