# TOOLS — every executable in this directory, and the traps that cost time

⭐ **WHY THIS FILE EXISTS.** `README.md` has a Tooling table, but it is a CURATED SUBSET — 13 of the
43 scripts here, chosen for the Indala story this branch started as, and it has not grown with the
branch. Everything written in the last week (`autopilot.sh`, `emugrade.sh`, `fixcheck.sh`,
`askdemod.py`, `rdrcap.py`, `flipraw.py`, …) was missing from it. A tick that needs to drive a
Chameleon and does not know `cu.py` exists will **rediscover it**, and one did: four calls were spent
on `chameleon_cli_main.py` silently ignoring its argv before `cu.py` was found in a grep of a shell
script (C409).

⛔⛔⛔ **READ THIS WHOLE FILE, TOP TO BOTTOM, BEFORE ACTING ON ANY SUSPECTED HARDWARE PROBLEM.** It is
a couple of hundred lines and it is FASTER than the alternative, which has now happened three times:
C409 (four calls spent on `chameleon_cli_main.py` silently ignoring its argv), C457 (a published
blocker on a missing `nrfutil` that this file already gave the command for) and C459 (a published
blocker on a flashing tool that had actually worked, because the verification read the wrong device
through a flag `cu.py` does not have — and the row for that was about to be written here). ⛔ **A
device that "does not answer", a tool that "does nothing", a reading that "makes no sense" is a
TOOLING SYMPTOM until this file says otherwise.** Suspect the instrument first, and this file is the
instrument's documentation. Do not skim it for the one row you expect to need: the trap that costs
the session is the one you did not know to look for.

⛔ **THIS FILE IS CHECKED.** `./checkdocs.sh` fails if an executable here is not listed below, so it
cannot go stale the way the README table did. Add the row when you add the tool.

---

## ⛔⛔ THE TRAPS — read these three before driving anything

| trap | what happens | the fix |
|---|---|---|
| ⛔⛔⛔ **`autoCompactWindow` IS A WINDOW SIZE, NOT A TRIGGER PERCENTAGE** | `.claude/settings.local.json`'s `autoCompactWindow: 400000` sets the EFFECTIVE CONTEXT WINDOW, and `status`'s context percentage is a fraction **of that same number** — so the two can never be compared as threshold and reading. The old gate stopped the loop at 40% saying *autocompact should have taken it already* while 172k tokens were free, and two firmware units were declined for it (C467). Compaction fires when `free` runs down to the reserved buffer (~92%), not at the setting's numeric value. ⇒ read `window=` and `free=` from `context_check.sh`, never a bare percentage |
| **`chameleon_cli_main.py` IGNORES ARGV** | It takes commands from **stdin only**. Passing them as arguments prints the banner, runs **nothing**, then dies in `prompt_toolkit` with `RuntimeError: There is no current event loop`. The commands look accepted and no error names the real problem | ⭐ **Always `software/script/cu.py`.** Same syntax, non-interactive, exits after the last command: `$PY cu.py "hw connect -p $PORT" "hw slot type -s 8 -t EM410X" …` |
| **A bare `./script` run uses the SYSTEM python** | Every shebang here is `#!/usr/bin/env python3`, which has no `pyserial` — `pyserial missing — use ../../software/script/.venv/bin/python` | ⭐ Invoke through the venv explicitly: `../../software/script/.venv/bin/python tool.py`. The shell scripts all set `PY` to it |
| **`./autopilot.sh gate` DOES NOT BUILD — it is a SECRET SCAN** | Every tick read `✓ gate clean` as *this compiles*. It never meant that, and F13 reached origin having never been through a compiler: `lf_tag_em.c` is compiled by **no host test** — `ctest/` builds the decoders and emitters, not the tag-emulation driver (C418) | ⭐ Fixed: the gate now builds `firmware/` whenever the staged diff touches it, and notes-only commits stay instant |
| ⭐⭐ **`nrfutil` SHIPS IN THE PROJECT'S OWN DOCKER IMAGE — IT IS OFFICIAL CHAMELEON BUILD TOOLING, NOT SOMETHING TO INSTALL BY HAND.** `firmware/Dockerfile` does `curl -sLo /usr/bin/nrfutil ...; nrfutil install device nrf5sdk-tools`, so the container needs nothing from the host. ⛔ **A tick reported the firmware route BLOCKED on a missing nrfutil (C457, retracted)** after running `which nrfutil` and looking in `~/bin` — while the row below already named the host copy and this row's container had it too. | ⭐ `cd firmware && docker compose up --pull=always build-ultra` → DFU zips in `firmware/objects/`. ⚠ The FLASH still runs on the host (Docker Desktop on macOS passes no USB), and the host binary is **`/Users/Shared/code/personal/rfid/.tools/bin/nrfutil`**, nrfutil 8.2.1 — verified 2026-09-15 producing `ultra-dfu-app.zip` and `ultra-dfu-full.zip` |
| **`firmware/build.sh` DELETES `objects/` FIRST, THEN FAILS** | Its first act is `rm -rf objects`, so a build that dies on a missing tool leaves you with **no flashable DFU zip at all** — which is what happened here. It also needs two things the shell does not give it | ⭐ `cd firmware && PATH=/Users/Shared/code/personal/rfid/.tools/bin:$PATH GNU_INSTALL_ROOT=/opt/homebrew/bin/ GNU_VERSION=$(arm-none-eabi-gcc -dumpversion) bash build.sh`. ⚠ `Makefile.posix` names `/usr/bin/`, which is wrong here; `nrfutil` is not on PATH (C200's trap again); and the final `mergehex` step fails harmlessly — it is the SWD artifact, written after the DFU zips |
| **`rfid raw_analyze` WEDGES THE FLIPPER** | Twice (AUTOPILOT.md §5): 72,518 lines of pulse/period pairs through the CDC link, once mid-CONTROL. Bounding the output and sending ETX did **not** prevent it | ⭐ Use `flipraw.py` — `rfid raw_read` to a file plus a binary-safe `storage read_chunks` fetch. ⚠ `raw_read` needs a **FULL PATH**: `rfid raw_read ask emctl` answers *"File is not RFID raw file"*, `rfid raw_read ask /ext/lfrfid/emctl.ask.raw` works |
| ⛔⛔ **A FAILED `make` STILL LEAVES THE OLD BINARY RUNNABLE — AND IT WILL PASS** | `make roundtrip` died on the Xcode licence, `./roundtrip` ran the binary from 53 minutes earlier and printed `✓ all round trips exact`. That pass said nothing about the edit | ⭐ `rm -f roundtrip` first, or check `ls -l roundtrip roundtrip.c`. Never read a ctest pass without confirming the build succeeded |
| ⛔⛔⛔ **THE WHOLE APPLE TOOLCHAIN — INCLUDING `git` — CAN DIE MID-SESSION ON AN XCODE LICENCE PROMPT** | Appeared partway through 2026-09-14. `/usr/bin/git` and `cc` both shim through Xcode, so `clang`, `git`, `./checkdocs.sh`, `./autopilot.sh gate`, `git commit` and `git push` all fail at once with *You have not agreed to the Xcode license agreements*. ⚠⚠ **`checkdocs.sh` then reports EVERY LOG.md hash as `⛔ hash does not resolve`, including commits made minutes earlier** — which reads like catastrophic notes corruption and is nothing of the kind. The real fix needs `sudo`, which an autopilot run cannot do | ⭐ **`export DEVELOPER_DIR=/Library/Developer/CommandLineTools`** — restores git and clang with no sudo, and checkdocs goes straight back to `✓ notes consistent`. ⛔ If every hash fails at once, suspect the toolchain, never the notes |
| ⛔⛔ **`rdrcap.py` WRITES BIG-ENDIAN int16, AND A LITTLE-ENDIAN PARSE LOOKS LIKE PLAUSIBLE DATA** | Parsed `<h` the samples ran -32764..31766 and looked like believable rail-to-rail noise; parsed `>h` the SAME bytes are 812..8560 and monotonic. The wrong reading does not error, it just lies | ⭐ Unpack `>%dh`. Sanity check: SAADC values are POSITIVE and well inside full scale; if a capture looks like full-scale noise, try the other byte order before believing it |

---

## Entry points — what a session actually runs

| | |
|---|---|
| `autopilot.sh` | ⭐⭐ The loop's control script. `status` (usage, context, devices, **and what firmware the hardware is really running**, M45), `beat`, `gate` (build + tests, run before every commit), `bench` (⭐ probes all four bench links with EM410X and says what a missing one BLOCKS — never infer the topology from a silent null, C408) |
| `checkdocs.sh` | ⭐ The notes-drift checker: dead cross-references, missing files, unreachable commit hashes, duplicated sections, stale counts. Run it **without a pipe** before committing a notes change |
| `fixcheck.sh` | ⭐ Regression-tests every FIXED entry in `FIXES.md`. A regression would silently turn a ledger entry into a false claim |
| `emugrade.sh` | ⭐ Grades the whole **emulate** column on rig A, Flipper as reader. Aborts if the reader is not proven alive first (C373/C374 — a silent reader and a silent emulter give identical numbers) |
| `nullmatrix.sh` | Cross-protocol nulls. A format is not done until its nulls pass (METHOD.md) |
| `regrade.sh` / `pm3written.sh` | Re-grade write arms, and read arms against tags the **Proxmark's own encoder** wrote |
| `readsoak.sh` / `judgerel.sh` | Reliability rather than correctness: how often does it work, and how reliable is the **judge** (M44) |
| `capcost.sh` / `benchab.sh` / `grab.sh` / `fieldhold.sh` | Held-vs-cycled field cost; the A/B bench comparison; a guided three-capture run; hold the field up for a scope |
| `fskcap.sh` | ⚠ **Its analysis is void** — it histograms with `tonehist.py`. The captures are sound; the verdict is not (C401) |

## Instrument drivers — talking to the four devices

| | |
|---|---|
| `cu.py` *(in `software/script/`)* | ⭐⭐ **The only way to drive a Chameleon from a script.** See the trap table |
| `flipper.py` | ⭐ The Flipper's `lfrfid` CLI: `read` (with its ASK control), `emulate`, `heap`, `reboot`. ⛔ It **aborts** rather than scoring 0 when the `rfid` plugin will not load — that refusal cost a whole unit (C373) |
| `flipraw.py` ⭐ **`--raw <12 hex bytes> --frac` scores the captured RF/10 share against the share the frame's OWN BITS imply**, instead of against a remembered expectation — which is how C414 and C411 came to report the same 6.2% for two different frames (C420). Use it for any tone-composition question. | ⭐⭐ What a Chameleon emulation **actually puts on the coil**, through the Flipper's raw reader. Parses the `RIFL` file (pulse/duration varints, microseconds) and applies a **peak test whose criterion is fixed before the numbers are seen** — a band counts only as a local maximum, never as a bin with counts in it (C401's exact failure) |
| `rdrcap.py` | Capture through **our own reader's** path, saved like `lf sniff --bits 16` |
| `t55rdcap.py` | Send a T5577 regular-read into a live capture and keep the raw samples |
| `pm3monitor.py` | Live fc/2 amplitude on the Proxmark, for sliding an emulating device around |
| ⭐⭐ `pm3cap.py` | **The comparator-free instrument of record (C464/C466).** `lf read -s N` + `data save` into a run histogram: level holds in us, top bins, longest run, and `--expect` scored against a floor you state FIRST. ⛔ It never calls a pm3 demodulator — that is the point: a decode failure is not evidence about a signal (C465, and the operator's T5577 work on `lf t55xx detect`). One sample = 8us = one carrier cycle = one PWM `counter_top` tick, which is why a `counter_top` of 32 reads back as a 256us run |
| `emuprobe.py` / `emutest.py` | What is an emulating tag transmitting (spectrum, not a verdict); and test it against the Proxmark's own demodulator, bracketed |
| `nulltest.py` | A **loud-signal** null: prove a wrong tag is present, then prove the reader ignores it |

## Host demodulators — no hardware, and the ones that are trusted

| | |
|---|---|
| `mfdemod.py` | ⭐ The working Indala PSK1 matched-filter decoder. `--selftest` |
| `askdemod.py` | ⭐ ASK/Manchester, the biphase family (Gallagher first) |
| `bidemod.py` | ⭐ ASK/**biphase**, the fourth line coding on this bench |
| `fskdemod.py` | FSK2a for AWID / Paradox / Pyramid. ⚠ Goertzel bins for fc/8 and fc/10 only — ioProx's tone of 11 is not in it |
| `momdemod.py` | Momentum's own FSK demodulator in Python, run over our emitter's ideal output |
| `generality.py` | ⭐ Does the firmware decoder generalise past the one word this bench has ever seen? Synthetic |
| `ctest/` | ⭐⭐ Host build of the **firmware** decoder. `make check` diffs it against `mfdemod.py` per capture **and runs the emitter round trip** (`roundtrip.c`) |
| `recover.py` ⛔⛔ **REFUTED BY ITS OWN CONTROLS (C456) — kept so it is not rebuilt, do NOT fix it into passing.** Reads the emitted bitstream off the air by dividing each run by the bit quantum. The idea is sound and the instrument will not support it: a recovery needs each HIGH and LOW separately, and the Flipper adds the comparator bias to every pulse and subtracts it from every gap (43-89us against a 128-256us quantum). **Only a period is unbiased**, and a period gives `n_high + n_low` without the split. | ⭐ What it actually shows: **fdxb, byte-exact on the air, recovers with 52.8% of runs ambiguous and no period at its predicted 256 quanta** — the control that voids the method before any PAC number can be believed (C456) |
| `quantum.py` ⭐⭐ **IS THE EMISSION BUILT ON THE CLOCK ITS SOURCE SPECIFIES?** Fits the largest quantum whose multiples explain the measured durations, against a prediction read from each emitter's modulator. ⛔ **Periods only** — a pulse carries the comparator's per-capture bias and a period does not (C435, M54). ⛔⛔ The fit is a minimisation, so **every run carries its own null** (M55): uniform random durations over the same range, same search, scoring 0.234-0.239 against the data's 0.018-0.056. ⚠ Any divisor of the true quantum fits at least as well and a loose band of large q passes at small n — **the minimum is the reading, not the band**. | ⭐⭐ Whether a failing emitter's CLOCK is wrong or only its data. PAC: 256us exactly, tighter than the gproxii control at the same quantum (C455) |
| `airduty.py` ⭐⭐ **WHAT IS ACTUALLY ON THE AIR — MAX RUN AND DUTY, SCORED AGAINST THE EMITTER'S OWN SOURCE.** Every other PAC instrument here asks *which of the intended runs survived* and so presupposes the emission is the frame, degraded; this one asks whether it is the frame at all, with two numbers that need no alignment and no decode. ⛔ The Flipper's comparator bias is **not a constant** (C435), so it is **estimated from the controls** — fdxb and gproxii, whose true duty is fixed at 50% by construction — and a verdict needing a bias outside their range is not a verdict. ⛔ Controls that CAN fail (M52): fdxb is PAC's exact geometry, both are proven byte-exact through this reader. ⭐ `--cards A B C` runs the pac arm once per credential so the question becomes *does the measured duty TRACK the frame* — a data-independent emission cannot follow three data-matched answers. `frame_stats()` prints predicted duty, run count, long-HIGH count and time-inside-long-HIGH so the two can be varied independently; C450's sweep confounded them and says so. ⚠ The pair count is **block-quantised** (2048-byte RIFL blocks, ~512 pairs each) — it is not a run count and must never be compared with one; a **zero-block, 20-byte file** is unambiguous and is what `EEEEEEEE` returns. | ⭐⭐ Whether an emitter is emitting its frame at all. PAC: predicted 2304us / 45.3%, measured **6021us / 85.7%**, against controls landing on 364us and 661us (C449); duty pinned at 83-88% across four credentials predicting 40.6-53.1% (C450) |
| `pacdiff.py` ⭐ **A DIFFERENTIAL DECODER VALIDATOR, AND IT RETIRED THE NUMBER IT WAS BUILT TO CONFIRM.** Emits two PAC credentials and checks the recovered bits change EXACTLY as `pac_build_bitstream` says they must. Chooses a control that CAN fail: `0000AAAA` vs `CARD0001` differs in 38 positions with near-equal total ones, so no level-counting decoder passes by luck. ⛔ It fails today, and that is the finding (C435) — do not 'fix' it into passing. | ⭐⭐ Whether a decoder tracks the DATA, without needing it to be absolutely right. Prints the invariant-prefix violations separately: PAC's payload bytes 0..2 are constants, so any recovered difference in bits 0..37 is the decoder's and nothing else's |
| `tonehist.py` | ⛔⛔⛔ **DO NOT TRUST — IT MEASURES NOISE (C401).** It printed *"ZERO long tones"* for an emission the receiver decoded correctly. Retained as evidence only. ⇒ Use `flipraw.py` |

## Measurement and sweeps — mostly historical, mostly Indala-era

| | |
|---|---|
| `analyse.py` | Analyse raw `lf sniff` captures for LF tag modulation |
| `phasebits.py` | ⭐ Sweep the SAADC sample phase and count **decodes**. `--analyse-only` re-runs on committed captures |
| `phasesweep.py` `sweep.py` `gaintest.py` `gapsweep.py` `oversample_test.py` `offsetsweep.py` | Per-lever sweeps. ⚠ these score the fc/2 **skirt**, which is polarity-blind — METHOD.md M8 |
| `lfprobe.py` | ⭐ Is an LF problem RF or firmware? Measures the tag's **signal**, not whether it decoded |
| `stack.py` | Capture stacking, and whether PSK **polarity** survives to the ADC at all |
| `inputtest.py` | AIN5 vs AIN0, the only tap upstream of the filter poles |
| `clockoffset.py` / `framedrift.py` | The emulator's frame period against the reader's clock (ppm); a real T5577's frame-to-frame drift |
| `drivesoak.py` | Soak the LF reader until the drive control goes inert |
| `pacber.py` / `burstnull.py` | Score a PAC capture by **bit error rate** rather than pass/fail; burst nulls |

## ⛔ A trap that produced a confident false pass (C429)

**RIFL's second value per pair is the PERIOD — a high run PLUS the low run after it — not one run.** Check it
on the bytes: `587+156=743`, `354+157=511`. A criterion written in RUN lengths scores the wrong quantity and
reported **100.0% against a true 48.8%**, raising no error at all. `flipraw.py --biphase` now works in periods.

**And a peak LOCATION may discriminate nothing.** `em410x.c` and `gproxii.c` are both one PWM entry per bit at
`counter_top` 64, so both emit 256us and 512us quanta. Twice in a row a criterion was written as *look for a
peak at X* and twice it was wrong. ⭐ What works instead: compute the expected distribution from the modulator
source, and pick SEVERAL payloads whose predictions are far apart — a constant tone cannot track three
different data-matched distributions, and no single capture can rule one out.

## ⛔⛔ The SECOND flipraw.py parser trap, and it corrupted 40% of a capture (C430)

**RIFL's pair stream is continuous across blocks, and one inserted or lost varint at a block boundary swaps
pulse and duration for EVERY pair after it.** No error is raised and the histogram still looks plausible.

⭐ **The invariant that catches it**: a pulse is the HIGH part of its own period, so `pulse < duration`
ALWAYS. One FDX-B capture had **4127 of 10296 pairs** violating that. `parse()` now resyncs on it and reports
the count; a healthy capture needs ~1% of pairs dropped.

⛔ **Parsing each block independently is WRONG** — it was tried first and made previously-clean captures
57-78% violating. That is also the evidence the stream really is continuous: a fix that only helps the broken
case is not a diagnosis.

⚠ **And pairing runs into periods has TWO phases.** A period is a HIGH run plus the LOW run after it, so the
pairing depends on the modulator's polarity. Only MIXED-run frames distinguish them — which is why extreme
frames can pass while the realistic one fails. `--biphase` reports both and names the one it used.

## ⭐ `flipgrade.py` — grade every LF EMULATE arm with the Flipper as reader (C431)

`./flipgrade.py [proto ...]`. ⛔ **Use this, not `emugrade.sh`, for the SAADC family** — emugrade reads with
Chameleon #2 and M52 forbids that against an emulation, which left half the emulate column ungradeable.

⛔ **Three traps it was built with and had to have removed — do not reintroduce them:**
| | |
|---|---|
| Gating PASS on the protocol NAME | Securakey reads as *Radio Key* and scored a total failure. C177/C178's exact bug; **M28 says match the success PATH**. The credential is the gate; the name is printed, never tested |
| Trusting the Flipper to answer | `failed to load external command` is a transient loader failure that reads NOTHING — scored as 15 protocol verdicts once. It now retries twice, then **voids the whole run** (C373) |
| Trusting `success` from a command batch | A REFUSED econfig still left a slot armed with no credential, grading SILENT like a dead emitter. The econfig runs alone and is judged on its own output |

## ⛔⛔ The Flipper's `rfid` app loader fails transiently — and a silent failure serves STALE data (C432)

`failed to load external command` means the app never started and **nothing was captured**. Both entry points
now guard it, because the two failure modes differ in severity:

| | |
|---|---|
| `flipgrade.py` (`rfid read`) | an unguarded failure becomes a wrong VERDICT — once, 15 protocol failures from a reader that read nothing (C431) |
| `flipraw.py` (`rfid raw_read`) | ⚠ **worse**: it returned normally and `fetch()` served whatever already sat at that path — a wrong verdict backed by a real, plausible histogram **belonging to another protocol** (C432) |

⭐ **The guard that actually catches it**: copy a known file to the target, then compare the file's SIZE
before and after the read. A read that did not raise is NOT evidence that it read anything. Run the control
emitter through the same guard — an unchanged size on a KNOWN-GOOD arm is what convicts the instrument rather
than the protocol.

## ⭐ `failed to load external command` is HEAP FRAGMENTATION — reboot the Flipper (C433)

Three incidents before it was root-caused. It is **not** storage: `/ext` had 60GB free. `free` shows the real
cause — plenty of free heap but no contiguous block big enough for the CLI plugin:

| | max contiguous block |
|---|---|
| fragmented (loader failing) | **59976** of 127504 free |
| after `power reboot` | **132176** |
| ⭐ **`seqdump.py` — read the PWM wave-form entries off the device and grade them against the emitter SOURCE** | The one step of the PAC path no air-side measurement can reach: four air routes are refuted, each by its own control (C440/C441/C451/C456), because the air carries what the PERIPHERAL made of the buffer, never the buffer. ⛔ Needs firmware carrying `DATA_CMD_LF_EMU_SEQDUMP` (3065) — **not yet flashable, see C459** | `./seqdump.py pac gprox` — gproxii goes through the SAME command (96 entries at counter_top 64 vs pac's 128 at 32) so a constant or stale answer cannot pass. ⛔ Guards on `playbacks` RISING, not the `emulating` flag (C460) |
| ⭐ **`enterdfu.py` — put ONE NAMED Chameleon into DFU** | `resource/tools/enter_dfu.py` walks `comports()` and takes the FIRST Chameleon it finds, with no way to say which — a coin toss with two on the bench, and the C363 hazard C364 paid for | ⭐⭐ **`./enterdfu.py --port <tty> --program firmware/objects/ultra-dfu-app.zip` — ONE STEP, and use it rather than two commands.** The bootloader window is shorter than the gap between two shell invocations: trigger in one and `nrfutil device program` in the next and the device has already fallen back to the application, nrfutil emits NO events and exits, and the version afterwards is the OLD build. ⛔ That is a RACE, not a flash failure, and it looks exactly like the quiet-but-successful output this file warns about — C459's trap with the sign flipped (C468). `--program` polls the bootloader VID/PID from inside the process, flashes the instant it appears (1.1s measured), waits for re-enumeration and prints what the device answers to `hw version`. Still refuses if any device is ALREADY in DFU, and still prints what it is leaving alone |
| ⭐ **`cu.py -p /dev/tty.X "hw version"` — FIXED 2026-09-15, and the fix is the lesson** | ⛔ **Until today cu.py had no `-p`.** It took COMMANDS, so `-p` and the path were each run as one, the help was printed for each, and it auto-connected with a bare `hw connect` — **whichever Chameleon enumerated first**. No crash, no error, no wrong exit code: a confident answer from the other unit. **It cost a published finding** (C459, retracted): #1 was flashed correctly, verified twice through #2, and `nrfutil` was blamed for reporting a success it had earned. Both *independent* signals agreed because both came from the same wrong device. The giveaway sat in the output — cu.py's help dump above every reading, and `playbacks started: 2`, #2's counter from an experiment #1 never ran | ⭐ **`-p/--port` now exists** and unknown options are REFUSED rather than reinterpreted as commands, because with two devices on the bench a wrong-device answer is worse than no answer. `"hw connect -p /dev/tty.X"` inside the command string also works and is what `flipgrade.py` always did. ⛔ Passing both is refused |
| ⭐ **FLASHING WORKS — `nrfutil device program` is fine, and C459 blamed it for a reading error (retracted)** | The route is `enterdfu.py --port <tty>` then `nrfutil device program --firmware firmware/objects/ultra-dfu-app.zip --traits nordicDfu`. In a terminal it draws a progress bar and ends `100% [1/1 <serial>] Programmed`; captured non-interactively it prints only the unrelated JLink warning, and `--json` gives 85 progress records ending `result=success, message=Programmed`. ⚠ **Quiet output is NOT failure** — verify by asking the DEVICE, with the correct connect idiom | ⭐ `enterdfu.py` targets ONE named port; `resource/tools/enter_dfu.py` takes whichever Chameleon enumerates first (the C363 hazard). ⛔ Still do not use `nrf5sdk-tools dfu usb-serial` — that is the 6.1.7 path recorded above as never completing this bootloader's handshake |
| ⭐ **`holdsweep.py` — the criterion for the long-DC question, written before the instrument exists** | C443 showed the property is untestable with the arms that ship: jablotron's `level = !level` is structural so no credential reaches a long run, every other arm is transition-guaranteed, and PAC is the only NRZ config in `t55xx.h`. ⭐ Flashing now works (L426), so the arm can be BUILT — a buffer of pure alternating static runs with the run length as the swept variable, which needs no credential, no protocol and no bench move | ⭐ **`hw emuhold` NOW EXISTS (C468)** — built to this file's spec and verified entry for entry, so the sweep itself is the open step. `./holdsweep.py` still only prints predictions. Prediction is slope 1 through the origin at 256us/entry; N=1 and N=2 are a positive control that can fail, being what every working ASK arm already emits. ⭐⭐ **AND ITS ATTRIBUTION PROBLEM IS SOLVED**: it was written when the Flipper faced #1 and could not attribute a knee, because the Flipper's comparator is a suspect too (C443). The reader is now `pm3cap.py` against #2 — no comparator anywhere in the chain (C464), proven end to end (C466) — so the sweep can locate a knee AND say it is not the instrument's. ⛔ Re-derive the criterion for that reader before running it |

⚠ **Repeated app load/unload cycles fragment it**, which is exactly what a grading pass over 16 arms does —
so expect it after `flipgrade.py` and reboot before trusting a later capture. ⭐ After any reboot, re-run the
CONTROL arm before the unknown one, so the result is taken on a bench proven live.

## ⛔⛔ THE FLIPPER'S RUN BIAS IS NOT A CONSTANT — FIT IT PER CAPTURE (C436/M54)

C429/C430 measured the comparator bias at ~96us: HIGH runs long by that, LOW runs short by it, PERIODS
unbiased. **The magnitude is not stable.** Fitted per capture by minimising rounding residual against the bit
period, it is **93us in one capture and 151us in another taken minutes apart** from the same emitter and the
same pad.

✅ Fitting it matters and is cheap: for PAC it took the runs from essentially never landing on a multiple
of the 256us bit to **83-89% within 0.15 bit and ~97% within 0.25 bit**. ⚠ It also changed the recovered
bits **not at all**, which is how you tell a nuisance parameter from the defect — report both.

⛔⛔ **AN EARLIER VERSION OF THIS SECTION SAID THE FLIPPER CANNOT MEASURE NRZ LEVELS AT ALL. THAT WAS
FALSE AND IS RETRACTED (C435 -> C436).** It compared bias-CORRECTED periods (99.6% quantised) against
bias-UNCORRECTED runs (0.3-2.6%) and read the gap as a property of the instrument. The invariant was sound
— NRZ holds a level a whole number of bits — but the two sides had different corrections applied.
⭐ The lesson is M54: when two numbers differ by two orders of magnitude, check they were processed the
same way before reaching for a mechanism.
