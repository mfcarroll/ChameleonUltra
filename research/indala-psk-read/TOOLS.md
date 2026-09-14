# TOOLS — every executable in this directory, and the traps that cost time

⭐ **WHY THIS FILE EXISTS.** `README.md` has a Tooling table, but it is a CURATED SUBSET — 13 of the
43 scripts here, chosen for the Indala story this branch started as, and it has not grown with the
branch. Everything written in the last week (`autopilot.sh`, `emugrade.sh`, `fixcheck.sh`,
`askdemod.py`, `rdrcap.py`, `flipraw.py`, …) was missing from it. A tick that needs to drive a
Chameleon and does not know `cu.py` exists will **rediscover it**, and one did: four calls were spent
on `chameleon_cli_main.py` silently ignoring its argv before `cu.py` was found in a grep of a shell
script (C409).

⛔ **THIS FILE IS CHECKED.** `./checkdocs.sh` fails if an executable here is not listed below, so it
cannot go stale the way the README table did. Add the row when you add the tool.

---

## ⛔⛔ THE TRAPS — read these three before driving anything

| trap | what happens | the fix |
|---|---|---|
| **`chameleon_cli_main.py` IGNORES ARGV** | It takes commands from **stdin only**. Passing them as arguments prints the banner, runs **nothing**, then dies in `prompt_toolkit` with `RuntimeError: There is no current event loop`. The commands look accepted and no error names the real problem | ⭐ **Always `software/script/cu.py`.** Same syntax, non-interactive, exits after the last command: `$PY cu.py "hw connect -p $PORT" "hw slot type -s 8 -t EM410X" …` |
| **A bare `./script` run uses the SYSTEM python** | Every shebang here is `#!/usr/bin/env python3`, which has no `pyserial` — `pyserial missing — use ../../software/script/.venv/bin/python` | ⭐ Invoke through the venv explicitly: `../../software/script/.venv/bin/python tool.py`. The shell scripts all set `PY` to it |
| **`rfid raw_analyze` WEDGES THE FLIPPER** | Twice (AUTOPILOT.md §5): 72,518 lines of pulse/period pairs through the CDC link, once mid-CONTROL. Bounding the output and sending ETX did **not** prevent it | ⭐ Use `flipraw.py` — `rfid raw_read` to a file plus a binary-safe `storage read_chunks` fetch. ⚠ `raw_read` needs a **FULL PATH**: `rfid raw_read ask emctl` answers *"File is not RFID raw file"*, `rfid raw_read ask /ext/lfrfid/emctl.ask.raw` works |

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
| `flipraw.py` | ⭐⭐ What a Chameleon emulation **actually puts on the coil**, through the Flipper's raw reader. Parses the `RIFL` file (pulse/duration varints, microseconds) and applies a **peak test whose criterion is fixed before the numbers are seen** — a band counts only as a local maximum, never as a bin with counts in it (C401's exact failure) |
| `rdrcap.py` | Capture through **our own reader's** path, saved like `lf sniff --bits 16` |
| `t55rdcap.py` | Send a T5577 regular-read into a live capture and keep the raw samples |
| `pm3monitor.py` | Live fc/2 amplitude on the Proxmark, for sliding an emulating device around |
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
