# Capability assessment — the brief

⛔⛔ **THIS REPLACES FEATURE WORK. Nothing new gets built until the grid below is real.**
Written 2026-09-15 after C473 was retracted by C474 — the third grid this project has published
and the third that measured its instruments instead of its emitter.

## 1. Why this exists

Four grids have now been published and all four were wrong in the same way.

| grid | judge | verdict | what was actually wrong |
|---|---|---|---|
| C378 | Flipper | FSK 0/6, "emulate column done" | one reader, no calibration |
| C431 | Flipper | 12/16, PAC a new gap | same, and PAC was never in C378 |
| C472 | pm3 | FSK decodes byte-exact | ✅ **correct — it carried a real-tag calibration** |
| C473 | pm3 | 16/16, all silences are decoder gaps | ⛔ **retracted (C474)** — no calibration rows |

⭐ **The single distinguishing feature of the one that held up**: before judging our HID Prox
emulation, a REAL pm3-written HID tag was put on the same pad and read by the same command. The
decoder was proven good on that protocol, in that geometry, first.

⛔ **THE RULE THIS YIELDS, AND IT IS STRUCTURAL, NOT A HABIT:**
> **A reader's silence on our emulation means NOTHING until that reader has decoded the SAME
> PROTOCOL from a known-good source on the same pad.** No calibration row ⇒ no verdict. Not a
> weaker verdict — no verdict.

## 2. What the operator's own bench already establishes

Supplied 2026-09-15, from manual testing. ⚠ Transcribed from a session, not script-captured — the
script must reproduce these, and a disagreement is a finding.

| protocol | pm3 reads real T5577 | pm3 reads FLIPPER emulation | our arm on pm3 (C473) |
|---|---|---|---|
| Indala | ✅ `a000000080089112` | ✅ same raw | ⚠ silent |
| KERI | ✅ (pm3-written) | ✅ `E000000092345678` | ⚠ silent |
| IDTECK | ✅ (pm3-written) | ✅ `4944544B55667788` | ⚠ silent |
| FDX-B | ✅ (Flipper-written) | ✅ | ⚠ silent |
| NexWatch | ✅ (pm3-written) | ⛔ no | ⚠ silent |
| GProxII | ✅ (pm3-written) | ⛔ no | ⚠ silent |
| Jablotron | ⛔ **no — not even Flipper-written** | ⛔ no | ⚠ silent |

⇒ **Four arms (Indala, KERI, IDTECK, FDX-B) now have suspicion pointing at OUR EMITTER**, because
the pm3 decodes that protocol from an emulation that is not ours. ⇒ Three (NexWatch, GProxII,
Jablotron) are genuinely ambiguous and need a third source before anything is claimed.

⭐ **A second, independent result is buried in that table and is worth its own unit**: the FLIPPER
fails to WRITE several protocols to a T5577 that the pm3 writes fine (keri, nexwatch, idteck,
gproxii), and the pm3 cannot read a Flipper-written Jablotron at all. That is a Flipper finding,
not a Chameleon one, but it bears on which sources are trustworthy as calibration.

## 3. The grid that has to be built

Per protocol, **SOURCE × READER**, every cell scored BYTE-EXACT against a value fixed in advance.

SOURCES: (a) real T5577 written by pm3 · (b) real T5577 written by Flipper · (c) Flipper
emulation · (d) **our** Chameleon emulation
READERS: (1) pm3 · (2) Flipper · (3) our Chameleon's own read arm

⛔ **(a)/(b)/(c) × (1)/(2) ARE THE CALIBRATION ROWS.** They are not optional and they are not a
nice-to-have: a (d) cell is uninterpretable without at least one passing non-(d) cell for the same
protocol and reader. **The script must refuse to print a (d) verdict when its calibration row is
missing or failing.** That refusal is the entire point.

⚠ M52 still applies to reader (3): our SAADC-family read arms must not be scored against ANY
emulation, ours or the Flipper's — only against (a)/(b).

## 4. Practical shape

- ⭐ Extend `pm3grade.sh` → `capgrid.sh`: same byte-exact scoring and A/B/A null sweeps, but a
  per-protocol calibration gate and an explicit `SOURCE=` so each pass records what was on the pad.
- ⛔ **It cannot be one unattended run.** Each source change is a tag move. Design it to run ONE
  protocol or ONE source at a time, resumable, appending to a machine-readable result file — so
  the operator's hands are needed for a swap, never for a decision.
- ⭐ Cross-check against the operator's table above first: if the script disagrees with a cell he
  measured by hand, the script is wrong until proven otherwise.

## 5. Notes debt — fix this at the same time or it will not get fixed

⛔ The documentation has become a burden and is now a *cause* of errors, not a guard against them.
`NEXT.md` is **1541 lines** and is doing five jobs: the queue, a historical narrative, the grid,
PR-blocking analysis, and superseded sections kept "for their reasoning". C473's fallacy went in
partly because nothing that long gets re-read before writing.

⭐ **Proposed split, to be confirmed by the operator:**
| file | job | rule |
|---|---|---|
| `STATE.md` (new) | what is true RIGHT NOW | **hard cap ~150 lines**, rewritten freely, no history, no superseded text |
| `NEXT.md` | the queue ONLY | ~50 lines. If it is not a thing to do next, it does not belong |
| `LOG.md` | history | unchanged — append-only, commit-pinned. **It is the one file doing its job** |
| `FINDINGS.md` | the reasoned record | **freeze it.** 711 lines of paragraph-sized table cells; stop adding, keep for reference |
| `METHOD.md` | the rules | add a 10-line preamble naming the rules that actually bite. Nobody reads 639 lines mid-unit |
| `AUTOPILOT.md` | 1734 lines | **audit for deletion.** Most of it is superseded process |
⭐ Everything deleted stays in git history. "Kept for its reasoning" is what produced 1541 lines.
