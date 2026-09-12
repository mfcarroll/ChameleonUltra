# Indala PSK read on Chameleon Ultra

**The Chameleon Ultra reads Indala** — on the device, as a command.

```
lf indala read   ->   Indala PSK1
                      Raw: a0000000e6bd0e92
                      Fmt 26 FC: 52 Card: 63612 Parity: 11
```

⛔⛔ **PUT THE TAG ON THE FRONT OF THE DEVICE — the side with the buttons.** The Flipper
reads from its back; the Chameleon is the other way round, and it is worth **21x**. Every
measurement in this project before 2026-09-11 was taken on the wrong side, which is most of
the deficit it spent days trying to explain. Claims measured that way are marked ⚠B in
`FINDINGS.md` and are provisional.

⛔ **The back is not a supported placement, decided 2026-09-12** — not merely worse. The
reader is tuned for the front and stacking, which was the only thing propping the back side
up, was removed for the RAM (C97, C98). A tag held the Flipper's way will often read as
"a subcarrier is present but no frame could be decoded", which is the signal to move it.

On the front: **114 of 160 single captures decode (71%) and 0 of 160 empty captures produce

⭐ **Emulation works too, verified against two independent readers**: a Flipper Zero reads the
emulated credential 6 times out of 6 (`Indala26 FC 52 Card 63612`) and a Proxmark decodes it
from up to 262 ms of continuous capture. ⚠ Our own reader does NOT read our own emulator —
not a defect but a specialisation, since its exact-Nyquist demodulation assumes a subcarrier
locked to the reader's carrier, which every real tag provides and a free-running PWM does not.

⭐ **And `lf indala write` works: 9 of 9 verified writes.** It reads the tag before and after,
so it reports VERIFIED, WRITE DID NOT LAND, WRITE FAILED, WRONG DATA or CANNOT TELL rather
than claiming success it cannot know. A successful read-back covers all three T5577 blocks at
once — a tag whose config block had not landed would not be transmitting PSK1 at RF/32 and
could not be read at all.
a frame at all**, measured over 32 sample phases x 5. Sample phase turns out to be two
working bands — 0–56 and 96–124, every one of them 5/5 — split by a dead band at 60–92; the
stock phase 0 is fine on the front and useless on the back. A coil that read 0/12 on the
back reads first try. The demodulation is integer arithmetic on the nRF52840 — no float, no
FFT, 8 KB of buffer.

With no tag on the antenna it reports `LF tag not found` 20 times in 20, taking the full
500 ms budget each — the rotation exhausts rather than aborting early.

⚠ **A single decode is not trustworthy**, so the firmware returns a credential only once two
captures agree. On the back one recovered frame in five was wrong; on the front 21 of 135.

⛔ **And agreement is not sufficient on its own — it is the phase rotation that makes it
safe.** In the 60–92 dead band the decoder returns the *same* wrong card number on every
capture (phase 64 → `a0000000b5af0b92`, 5 of 5), because the winning bit alignment there is
half a bit period off and every integrator straddles a boundary. Two independent captures
agree on it. Nothing in `PHASE_ROTATION` lies in that band, and nothing may be added to it
without checking — see the ⛔⛔ block in `lf_indala_data.c`. The premise the agreement rule
was built on, that wrong words never repeat, held only because the original captures were
26 dB down: **more signal turned a random error into a systematic one.**

⛔ **This was believed impossible for most of the investigation** — "31.2 dB below the
Proxmark", "7.6 dB short", "detectable but not decodable". All retracted. The deficit was a
software bug: the demodulator was decoding **PSK2 against a PSK1 tag**, and its self-test
encoded with the same wrong convention, so it passed forever. `FINDINGS.md` has the ledger.

## Where to look

| | |
|---|---|
| **`FINDINGS.md`** | ⭐ **Start here.** Current knowledge and the claims ledger. Holds no history. |
| `NEXT.md` | Ranked next steps. |
| `METHOD.md` | The method rules — eleven wrong conclusions, and what each one taught. |
| `LOG.md` | What was learned when, indexed to git. Append-only. |
| `archive/` | The old working notes, frozen. Most of their headline numbers are retracted. |
| `ADVERSARIAL.md` | A hostile-review prompt, if you want to attack the conclusions. |

## Resuming a session

Point a fresh session at this file. Read `FINDINGS.md` then `NEXT.md`; consult `LOG.md`
only to answer "when did we learn X", and `archive/` only for reasoning, never for numbers.

⚠ **Before trusting any measurement, confirm the tag state.** The campaign registry's
`PSK1` cell writes `DEADBEEF/12345678` — correct PSK1 air shaping, but *not* an Indala
frame. Misreading that invalidated a day.

```bash
cd /Users/Shared/code/personal/rfid/proxmark3 && ./pm3 -c "lf indala reader"
```

Expect `a0000000e6bd0e92`, `Fmt 26 FC: 52 Card: 63612`.

## Tooling

| | |
|---|---|
| `mfdemod.py` | ⭐ The working decoder. `--selftest`, or pass capture files. |
| `phasebits.py` | Sweep sample phase, count decodes. `--analyse-only` re-runs on existing captures. |
| `generality.py` | ⭐ Does the decoder work on words other than the bench tag's? Synthetic, no hardware. |
| `lfprobe.py` | ⭐ Is an LF problem RF or firmware? Measures the tag's signal continuously instead of whether it decoded. |
| `stack.py` | Zero-offset capture stacking and the polarity diagnostics. |
| `inputtest.py` | AIN5 vs AIN0 paired comparison. |
| `sweep.py` `phasesweep.py` `gaintest.py` `gapsweep.py` `oversample_test.py` | Per-lever sweeps. ⚠ these score the fc/2 *skirt*, which is polarity-blind — see `METHOD.md` M8. |
| `cu.py` (in `software/script/`) | Run CLI commands non-interactively. |
| `ctest/` | ⭐ Host build of the **firmware** decoder. `make check` diffs it against `mfdemod.py` per capture. |
| `flipper.py` | ⭐ Drive the Flipper's lfrfid CLI — `read` (with its ASK control) and `emulate`. Rig A, both directions. |
| `checkdocs.sh` | ⭐ Verify the notes have not drifted. Run it before committing a notes change. |

Decode the committed captures:

```bash
../../software/script/.venv/bin/python mfdemod.py --selftest && ../../software/script/.venv/bin/python mfdemod.py caps/phasebits/tag_p024_r*.bin
```

⭐ Cross-check the firmware decoder against the research one, per capture, no hardware:

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read/ctest && make check
```

Reproduce the phase sweep result from committed data (no hardware needed):

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/research/indala-psk-read && ../../software/script/.venv/bin/python phasebits.py --analyse-only --keep caps/phasebits --step 4 --repeats 5
```

## Keeping these notes honest

History and current knowledge drift apart when a fact lives in two places — you update one
and the others go stale silently. So:

- **`LOG.md` is append-only.** The only permitted edit to an entry is appending a
  `⛔ retracted by L##` pointer. Never reword an entry to match what you now believe.
- **`FINDINGS.md` holds no history** and is rewritten freely. If a claim is not in its
  ledger, it is not established.
- **`archive/` is frozen.** Cite it for reasoning, never for numbers.
- A new claim enters the ledger with its `n`, its `null` and its `indep` filled in. **If a
  column would be blank, that is the finding** — say so rather than leaving it empty.

```bash
./checkdocs.sh
```

## The bench

⛔ **Two fixed rigs, and nothing on them moves unless a person moves it.** That is the
constraint that matters for unattended work: a test must not assume a placement it cannot
make.

| rig | layout | what it drives |
|---|---|---|
| **A — emulate** | Chameleon #1 alone on the Flipper pad, its face towards the Flipper's back | the Chameleon emulates, the Flipper reads over its serial CLI |
| **B — the sandwich** | ⭐ **Proxmark, T5577, Chameleon #2, in that order** — re-aimed 2026-09-12 so the two devices face each other with the tag between them | ⭐ strictly better than the old layout: it runs **all three** pairings. Proxmark↔tag (write and verify), Chameleon↔tag (reads it 4/4), and Proxmark↔Chameleon for emulation work — that last one by lifting the tag out, since otherwise both sources modulate the same field. ⭐ The converse needs NO hands: to measure the tag alone, put Chameleon #2 into reader mode over USB (`hw mode -r`) and it stops emulating — which is how C138's control was taken |

| device | port |
|---|---|
| Chameleon #1 | `/dev/tty.usbmodemC3A1656543DE1` |
| Chameleon #2 | `/dev/tty.usbmodemF429364E46961` |
| Flipper Zero | `/dev/tty.usbmodemflip_Matthew1` |
| Proxmark3 | `/dev/tty.usbmodemiceman1` |

⭐ **Between them the two rigs close both loops with no hands.** Rig B is a complete
read-path bench: the Proxmark writes *any* protocol onto the T5577 and the Chameleon reads
it back, so a new reader can be exercised end to end against an independently-written tag.
Rig A is the same for the emulate path. Most of `NEXT.md` needs neither a person nor a
placement change.

⭐ **Rig A can also run BACKWARDS, which is what §1 needs.** The Flipper emulates over the
same CLI — `rfid emulate Indala26 <4 bytes>`, `rfid emulate Idteck 4944544B00000000` — into
Chameleon #1, which already sits in the front-to-back geometry C81 used in the other
direction. That is a non-carrier-locked PSK1 source in front of our own reader, with no hands.
⭐ **Tested, and it works** — 8 of 8, with a paired null at 0 of 4 (C87). ⚠ Stop the Flipper
explicitly before taking that null: `timeout` kills `flipper.py` with SIGTERM, which skips the
`finally` that sends ETX, so the emulation outlives the script and the "idle" arm decodes
(M29).

⭐ **And it works — our reader decodes the Flipper's emulation 8 of 8** (C87). That is not a
contradiction of M27/C82: the Flipper clocks its emulation from the reader's own carrier
(C74), so it is carrier-locked exactly as a T5577 is. ⇒ The limitation is **free-running**
sources, and the only one known is our own PWM.

⛔ **What genuinely needs a person:** a second *Chameleon* emulating in front of a Chameleon
reader — the only free-running source on this bench, and the two rigs do not face each other;
lifting the T5577 out of the sandwich when the Proxmark must see the emulator alone; and
anything measured with the cable out.

⭐ **COUPLING IS A SETTABLE VARIABLE: paper spacers give an air gap in 1 mm steps, 1-12 mm.**
That matters more than it sounds. Several claims here are about behaviour at MARGINAL signal —
C47 predicts an effect only where reads are already failing — and a tag either couples well or
it does not, so those claims were untestable while the only specimens were "on the pad" and
"not on the pad". A gap turns that into a dial: find the distance where reads sit near 50% and
the margin is where the experiment wants it.

⚠ It needs a person, but only to place the spacer. Everything after that — including
reflashing between two builds — leaves the geometry untouched, so a paired test at a fixed gap
is one placement and any number of measurements.

⚠ **Both Chameleons are cabled, and the cable costs ~40% of the coupling by detuning the
antenna (C72).** Emulation is still read reliably by the Flipper in this state, so functional
pass/fail tests are valid as they stand — but an amplitude measured cabled is not comparable
to one measured uncabled. Re-take anything that enters the ledger as a *level* rather than as
a pass with the cable out. §7 exists to remove this confound.

⭐ **`flipper.py` drives both directions** — `flipper.py read --mode both` runs the PSK arm
and its ASK control, `flipper.py emulate Indala26 <4 bytes>` holds an emulation for the
Chameleon to read. ⚠ It sends ETX after its timeout because `rfid read` does **not** time out:
it loops until a tag decodes or the next character is ETX, so a failed read otherwise leaves
the worker running and swallows the next command — invisible on a passing arm, wrong on the
null (L83).

⚠ **The unit numbering above was established by evidence, not by assumption:** only the unit
on `/dev/tty.usbmodemC3A1656543DE1` has an active slot emulating Indala, and the Flipper reads
Indala from its pad. Re-check that after anything is unplugged — the ports are stable, but
which unit sits where is not knowable from software alone.

⚠ Keep those serials inside a full `/dev/...` path. Written bare, a serial starting with `C`
and a digit parses as a claim citation to `checkdocs.sh` and fails the run — as the first
draft of this very paragraph did. ⛔ The same bites **T5577 block values**: write them with an
`0x` prefix, because a block beginning with `C` and a digit reads, at a word boundary, as a
citation of a claim number.

⛔ **Run `./checkdocs.sh` WITHOUT a pipe when you are using it as a gate.** `./checkdocs.sh |
tail -1 && git commit ...` always commits: a pipeline's exit status is the last command's, so
`tail` returning 0 masks the failure. That is not hypothetical — it let a broken
cross-reference through on this branch, and the shell reported success the whole way.

## Working conventions

| | |
|---|---|
| branch | `indala-psk-read` on `origin` = `mfcarroll/ChameleonUltra`, the fork — pushing research branches there is expected |
| ⛔ never | `upstream`, `main`, or `--force` / `--force-with-lease` on anything |
| signing | commits are signed through 1Password's `op-ssh-sign`. **If 1Password is locked, `git commit --no-gpg-sign` and carry on** |

⛔ **Do not go back and sign an unsigned commit, and do not `--amend` one that exists.** Both
rewrite the hash, `LOG.md` cites hashes, and `checkdocs.sh` asks whether each is reachable
from HEAD. The rule already written for LOG pointers covers signatures too: land it, then fix
it forward in a follow-up commit.

⛔ **Confirm the firmware version after every flash — the DFU trigger fails silently.**
`hw version` carries the build's `git describe`, so a stale build is visible in one command.
The device does not always enter DFU on the first trigger; `nrfutil` then reports "No devices
with requested serial number(s) or trait(s) found" and the old firmware keeps running. A
regression test against that is a test of the previous build, and it looks exactly like a
passing one — it happened here and the version string is what caught it. ⇒ Retry the trigger
until `nrfutil device list` shows `nordicDfu`, then check the version afterwards.

⛔ **CHECK WHAT IS ACTUALLY ON THE T5577 BEFORE RUNNING ANYTHING AGAINST IT.** One
`lf search` on the Proxmark. The tag has worn five different credentials in a single day, and
`drivesoak.py` cycles it through four more; the row in `NEXT.md` has gone stale twice and both
times it sent experiments at the wrong specimen — once producing a "PAC reads 0/10 regression"
that was simply a HID tag.

⛔ **CONFIRM THE DRIVE CONTROL IS LIVE BEFORE BELIEVING ANY `--drive` NUMBER.** One capture at
`--drive 7` against one at `--drive 4`: live is roughly 2x apart, inert is identical. The
control silently stops taking effect mid-session (C148), a reboot restores it, and while it is
inert every drive produces the stock field — which invalidated a PAC result and cost four wrong
explanations in a row. ⚠ If it looks inert, run `hw lfdebug` **before** touching anything: a
reboot clears the state and destroys the evidence.

⭐ **A change and the note describing it belong in the same commit**, so the tree is never in
a state where the code and the notes disagree. `./checkdocs.sh` passes before every commit.

⚠ **`NEXT.md` is a plan, not a journal.** What happened and when goes in `LOG.md`; what is
believed now goes in `FINDINGS.md`; what to do next goes in `NEXT.md`, where a finished
section collapses to one line. Commentary accumulating there is what took it to 995 lines,
~600 of them duplicated, with every cross-reference still validating.

## Environment

| | |
|---|---|
| repo | `/Users/Shared/code/personal/rfid/ChameleonUltra`, branch `indala-psk-read` |
| harness | `/Users/Shared/code/personal/rfid/Momentum-Firmware`, branch `t5577-deep-read` |
| Proxmark | `/Users/Shared/code/personal/rfid/proxmark3`, device `/dev/tty.usbmodemiceman1` |
| python | `ChameleonUltra/software/script/.venv/bin/python` — numpy + pyserial |
| nrfutil | `/Users/Shared/code/personal/rfid/.tools/bin/nrfutil` (8.2.1) |

Build and flash (needs Docker running):

```bash
cd /Users/Shared/code/personal/rfid/ChameleonUltra/firmware && PATH=/Users/Shared/code/personal/rfid/.tools/bin:$PATH ./flash-dfu-app-macos.sh
```

⚠ If `nrfutil` is missing, install it Nordic's way — **not** Homebrew, whose cask is
disabled for a Gatekeeper failure. `curl` sets no quarantine attribute, so there is no
prompt:

```bash
mkdir -p /Users/Shared/code/personal/rfid/.tools/bin && curl -sL -o /Users/Shared/code/personal/rfid/.tools/bin/nrfutil https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/aarch64-apple-darwin/nrfutil && chmod 755 /Users/Shared/code/personal/rfid/.tools/bin/nrfutil && PATH=/Users/Shared/code/personal/rfid/.tools/bin:$PATH nrfutil install device nrf5sdk-tools
```

## `lf sniff` flags added by this work

`--bits 16` full 14-bit conversion instead of `>>5` · `--phase N` sample phase, 0–127 ticks
of 62.5 ns (**ticks 4–60 work, stock 0 fails**; the ranking inside that window moves
between sessions, so `lf indala read` rotates rather than picking one) · `--input {5,0}`
AIN5 or AIN0/`LF_RSSI` (measured dead) · `--rate N` free-running kHz · `--gain N` divisor ·
`--settle N` ms

## `lf indala read`

New command, `DATA_CMD_INDALA_SCAN` = 3033. Firmware in
`firmware/application/src/rfid/reader/lf/lf_indala_psk.{c,h}` (the demodulator, portable
integer C) and `lf_indala_data.{c,h}` (capture, phase rotation, agreement rule).
