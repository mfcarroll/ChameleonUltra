#!/usr/bin/env python3
"""Which read LENGTH decodes each emulate arm? Interleaved, a ladder of lengths per arm.

    ./shortread.py --interleave --repeat 8        # the whole ladder, all six arms
    ./shortread.py gproxii --lengths 6144,10000,12288 --repeat 10
    ./shortread.py --interleave --repeat 2 --null # the control: every count must be zero

⭐⭐ WHY. C490 measured that `lf indala reader` is silent on our emulation while `lf read -s 4096`
followed by `lf indala demod` returns the armed credential byte-exact, 4 of 4, in the same session
and the same field. The reader's own source says why: `cmdlfindala.c:633` is `lf_read(false,
30000)` — 240 ms, against the short read's 33 ms — and a span that long carries enough of C486's
beat to defeat the demodulator.

⛔⛔ **BUT "USE A SHORT READ" IS NOT THE RULE (C498).** At one length per arm, interleaved,
`gproxii` went 0/8 → 8/8 at 12288 while `keri` went the OTHER WAY — 3/8 on its own 10,000-sample
read against 0/8 at 4096. Each arm has a WINDOW, bounded below by needing whole frames and above by
the ~61 ms fading period. ⇒ **So the question is no longer "short or long" but "how many samples,
for this arm", and that is what this tool now sweeps.**

⛔ THE A/B IS THE POINT AND IT MUST BE INTERLEAVED. Every length for one arm runs inside ONE pm3
session and one arming, so field strength, coupling, temperature and the arm itself are identical
across the ladder — the ONLY variable is the sample count. Arms are then ROUND-ROBINED one round at
a time (`--interleave`, M59), so the bench's wander moves every arm together.

⛔⛔ AND THE RUNGS ARE SHUFFLED, because ascending order confounds the sample count with the
POSITION of the read after arming. The first run of this ladder scored `lf keri reader` 2/12 against
`lf read -s 10000` + `lf keri demod` 10/12 — and `cmdlfkeri.c:222` is `lf_read(false, 10000);
demodKeri()`, i.e. the SAME count through the SAME demodulator, so nothing but position could
differ. Shuffled (the default; `--no-shuffle` reproduces a pre-M60 run) the two agree. M60.

⭐ EACH ARM'S LADDER INCLUDES ITS OWN READER'S SAMPLE COUNT, and that is the control that makes the
rest readable. `lf gproxii reader` asks for 10,000 samples; so does one rung of gproxii's ladder. If
the two disagree AT THE SAME N, the difference is the reader command's code path and not the length
— which is C487's save/load artifact showing up somewhere it was thought bounded away from.

⛔ TWO SCORES, NOT ONE, because they answer different questions:
  - `marker` — the registry's own decode marker matched PER LINE (C488: a marker is matched
    against a LINE; `re.search` over a blob measures the regex, not the signal). It says a frame
    of this protocol was found.
  - `exact`  — the armed credential appears in the output. It says the frame was OURS, byte for
    byte. ⭐ A marker without an exact is a decoded frame carrying the WRONG payload, which is a
    result in its own right and is reported as such: C490 saw exactly that from a one-frame window
    (`a0000000e4000000` for `a0000000e6bd0e92`), confidently and with no warning. ⭐⭐ The ladder's
    SHORTEST rung is one frame precisely so that failure mode gets sampled rather than avoided.

⛔ LADDER CONSTRUCTION, so nobody has to reverse it from the numbers: multiples 1,2,3,4,6 of the
arm's OWN frame length in samples, plus its reader's own count, deduped, sorted, capped at 40,000
(the largest `lf read -s` this bench has used — C487/C488). Frame lengths are the arm's own:
64 bits x RF/32 for the PSK64 arms, 128 x RF/32 for NexWatch, 224 x RF/32 for Indala224,
96 x RF/64 for GProxII. At 125 kHz one sample is one carrier cycle, so samples/125000 = seconds.

⛔⛔⛔ **ITS RATES ARE SCHEDULE STATISTICS, NOT INDEPENDENT SAMPLES — ADDED AFTER C507/M61, AND IT
QUALIFIES EVERY NUMBER THIS TOOL HAS EVER PRINTED.** Every rung runs inside ONE pm3 session, back
to back, which is the whole point of the design (one arming, one field). ⛔ But six IDENTICAL reads
in one session return the same decode pattern in **16 of 16 sessions** — `gproxii` gave `.X.XX.`
every time, index 1/3/4 decoding and 0/2/5 never — because each read lands at its own phase of
C486's ~61-80 ms beat and the cadence is deterministic. A host-side `msleep`, touching no device
and raising no field, moves both the pattern and the rate (29.2% → 70.8%).

⇒ **A rung's n/24 is not 24 independent trials.** It is one schedule sampled 24 times, and the
shuffling that M60 added does not fix this — shuffling decorrelates LENGTH from position and leaves
the position effect itself intact, spread evenly, looking exactly like ordinary variance. That is
very likely a large part of "the hit rate wanders" (C497).
✅ **WHAT STILL STANDS**: zero-versus-non-zero. A rung that decoded at all decoded, and no
scheduling artifact manufactures a byte-exact credential — the `--null` control is 0 throughout.
⛔ **WHAT DOES NOT**: the LEVELS, and any ordering of two rungs by rate. ⚠ C499's best-length
argmax was already retracted on its own evidence (C504, four of five arms moved); this is the
mechanism for why.
⭐ If independent trials are ever wanted from this tool, the fix is `benchmatrix`' — a drawn pause
between repeats, not a constant one, since a constant pause is merely another fixed cadence.

⛔⛔ THIS GRADES NOTHING. No null sweep, no calibration row, no licence — a manual observation
like C487/C488/C490. It cannot move a cell in the matrix and must never be reported as if it had.

⚠ The arm table is TRANSCRIBED from `rfid-tools`' `benchmatrix/registry.py` (the same `pm3_read`,
`pm3_decode_marker`, `expect` and `cu_emulate` the graded matrix uses) rather than imported across
repos. If one drifts from the other that is a thing to report, not to paper over.
"""
import argparse
import json
import random
import re
import subprocess
import sys
import os
from typing import NamedTuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import seqdump

PM3 = "/Users/Shared/code/personal/rfid/proxmark3/pm3"
SAMPLE_HZ = 125000          # one LF sample per carrier cycle
MAX_SAMPLES = 40000         # the largest `lf read -s` this bench has used (C487/C488)


class Arm(NamedTuple):
    typ: str                # cu slot type
    econfig: str            # cu econfig command, `%d` is the slot
    reader: str             # the pm3 reader command the MATRIX grades on
    marker: str             # registry decode-marker regex, matched per LINE
    expect: str             # the armed credential, byte for byte
    frame: int              # this arm's frame length in samples
    reader_n: int           # how many samples its own reader command asks for
    raw_is_expect: bool     # ⛔ is the demod's `Raw:` field the SAME OBJECT as `expect`?
                            # Only then can --leading compare them bit 0 to bit 0. `lf keri
                            # demod` prints `Raw: %08X%08X` (cmdlfkeri.c:176) — 64 bits — against
                            # a 32-bit internal ID, and `lf nexwatch` reports a card number, not a
                            # raw. `expect` still works for the EXACT score, which is a substring
                            # search and does not care about framing.


ARMS = {
    "indala": Arm("Indala", "lf indala econfig -s %d --id a0000000e6bd0e92",
                  "lf indala reader", r"Indala \(len", "a0000000e6bd0e92", 2048, 30000, True),
    "keri": Arm("Keri", "lf keri econfig -s %d --id 80003039",
                "lf keri reader", r"KERI - Internal ID|Descrambled MS - FC:|probably KERI",
                "80003039", 2048, 10000, False),
    "idteck": Arm("IDTECK", "lf idteck econfig -s %d --id 4944544b55667788",
                  "lf idteck reader", r"IDTECK Tag Found: Card ID", "4944544B55667788", 2048, 5000, True),
    "nexwatch": Arm("NexWatch", "lf nexwatch econfig -s %d --cn 87654321 -m 2",
                    "lf nexwatch reader", r"NexWatch raw id|88bit id", "87654321", 4096, 20000, False),
    "indala224": Arm("Indala224",
                     "lf indala econfig -s %d --id "
                     "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5 --224",
                     "lf indala reader", r"Indala \(len",
                     "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5", 7168, 30000, True),
    "gproxii": Arm("GProxII", "lf gproxii econfig -s %d --raw fac2a38c2b081af0210b12c2",
                   "lf gproxii reader", r"G-Prox-II - (Unknown )?[Ll]en:",
                   "fac2a38c2b081af0210b12c2", 6144, 10000, True),
}
ORDER = ["indala", "keri", "idteck", "nexwatch", "indala224", "gproxii"]
MULTIPLES = (1, 2, 3, 4, 6)


def ladder(key):
    """The arm's own frame times MULTIPLES, plus its reader's count. See the module docstring."""
    a = ARMS[key]
    ns = {a.frame * m for m in MULTIPLES} | {a.reader_n}
    return sorted(n for n in ns if n <= MAX_SAMPLES)


def score(block, marker, expect):
    """⛔ PER LINE (C488). `^`-anchored markers never match inside a blob."""
    rx = re.compile(marker)
    hit = any(rx.search(ln) for ln in block.splitlines())
    ex = expect.lower() in block.lower()
    return hit, ex


RAW_RX = re.compile(r"Raw:\s*([0-9a-fA-F]+)")


def leading_bits(raw_hex, expect_hex):
    """How many LEADING bits of the demodulated payload are ours, and does a shift do better?

    ⛔ The second number is the control the criterion asked for. `lf indala demod` reports a
    nonsense length for indala224 (254-611 against 224), so the payload's alignment cannot be
    taken on trust: if some small bit-shift matched far more than offset 0, the demodulator
    locked somewhere other than our bit 0 and the offset-0 count would be meaningless. A correct
    lock has its best agreement AT offset 0."""
    def bits(h):
        return bin(int(h, 16))[2:].zfill(len(h) * 4)

    def prefix(a, b):
        n = 0
        for x, y in zip(a, b):
            if x != y:
                break
            n += 1
        return n

    a, b = bits(raw_hex), bits(expect_hex)
    at0 = prefix(a, b)
    best, shift = at0, 0
    for sh in range(1, 9):
        m = prefix(a[sh:], b)
        if m > best:
            best, shift = m, sh
    return at0, best, shift


def run(port, key, repeat, timeout, lengths=None, null=False, shuffle=True,
        rng=random):
    """⛔ `null=True` RUNS THE IDENTICAL COMMANDS WITH NOTHING ARMED. Rig B is tagless, so every
    count must be zero; a single hit there means the reads are picking up something that is not
    our emission, and no armed figure in the same run can be believed."""
    a = ARMS[key]
    ns = lengths or ladder(key)
    demod = a.reader.replace(" reader", " demod")
    if null:
        seqdump.disarm(port)
    else:
        ok, why = seqdump.arm(port, a.typ, a.econfig)
        if not ok:
            return {"arm": key, "error": "ARM FAILED: " + why}
    cmds = []
    for _ in range(repeat):
        # ⛔⛔ ORDER IS A CONFOUND AND IT NEARLY COST A FINDING. Ascending rungs put every sample
        # count at a FIXED POSITION after the arming, so position and length are perfectly
        # correlated and any warm-up effect reads as a length effect. The first ascending run
        # scored `lf keri reader` 2/12 against `lf read -s 10000` + `lf keri demod` 10/12 — the
        # SAME count through the SAME demodulator (`cmdlfkeri.c:222` is `lf_read(false, 10000);
        # demodKeri()`), so the gap could only be position. Shuffling decorrelates them. M60.
        items = [None] + list(ns)          # None = the arm's own reader command
        if shuffle:
            rng.shuffle(items)
        for it in items:
            if it is None:
                cmds.append(a.reader)
            else:
                cmds.append("lf read -s %d" % it)
                cmds.append(demod)
    r = subprocess.run([PM3, "-c", "; ".join(cmds)], capture_output=True, text=True,
                       timeout=timeout)
    out = r.stdout + r.stderr
    # Split the transcript on the client's own echo of each command.
    blocks, cur, label = [], [], None
    for ln in out.splitlines():
        m = re.search(r"pm3 --> (.+)$", ln)
        if m:
            if label is not None:
                blocks.append((label, "\n".join(cur)))
            label, cur = m.group(1).strip(), []
        elif label is not None:
            cur.append(ln)
    if label is not None:
        blocks.append((label, "\n".join(cur)))

    res = {"arm": key, "lengths": ns, "reader_n": a.reader_n, "frame": a.frame,
           "long": [], "by_len": {n: [] for n in ns},
           "raws": {n: [] for n in ns}, "raws_long": []}
    pending = None      # the `lf read -s N` whose samples the next demod will judge
    for label, body in blocks:
        m = re.match(r"lf read -s (\d+)$", label)
        if m:
            pending = int(m.group(1))
        elif label == a.reader:
            res["long"].append(score(body, a.marker, a.expect))
            res["raws_long"] += RAW_RX.findall(body)
        elif label == demod and pending is not None:
            res["by_len"][pending].append(score(body, a.marker, a.expect))
            res["raws"][pending] += RAW_RX.findall(body)
            pending = None
    return res


def fmt(res):
    """One line per arm: its own reader, then every rung of the ladder."""
    key = res["arm"]
    lx = sum(1 for _, e in res["long"] if e)
    lm = sum(1 for h, _ in res["long"] if h)
    rungs = []
    for n in res["lengths"]:
        hits = res["by_len"][n]
        ex = sum(1 for _, e in hits if e)
        mk = sum(1 for h, _ in hits if h)
        tag = "*" if n == res["reader_n"] else " "     # * = its own reader's sample count
        odd = "!" if mk > ex else ""                   # marker without exact = WRONG payload
        rungs.append("%d%s %d/%d%s" % (n, tag, ex, len(hits), odd))
    return ("%-10s frame %-5d  %-18s exact %d/%d marker %d/%d\n            %s"
            % (key, res["frame"], ARMS[key].reader, lx, len(res["long"]), lm, len(res["long"]),
               "  ".join(rungs)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("arms", nargs="*", default=ORDER)
    ap.add_argument("--port", default=seqdump.CU2_PORT)
    ap.add_argument("--repeat", type=int, default=3)
    ap.add_argument("--timeout", type=int, default=900)
    ap.add_argument("--lengths", help="comma-separated sample counts, overriding every arm's "
                                      "ladder. ⚠ Only meaningful for ONE arm at a time — the "
                                      "ladders differ because the FRAMES differ.")
    ap.add_argument("--interleave", action="store_true",
                    help="⭐ ROUND-ROBIN the arms one round at a time (M59). The bench's hit "
                         "rate wanders — the same arm gave 88%%, 60%%, 38%% and 75%% in one "
                         "evening — so arms measured one after another cannot be compared with "
                         "each other. Interleaved, whatever drifts moves all of them together and "
                         "the ORDERING becomes readable. ⚠ Lengths WITHIN one arm are already "
                         "safe without this: they share a session, a field and an arming.")
    ap.add_argument("--no-shuffle", dest="shuffle", action="store_false",
                    help="⛔ run the ladder in ASCENDING order. Off by default because it "
                         "confounds sample count with position-after-arming (M60). Use it only "
                         "to reproduce a pre-M60 run.")
    ap.add_argument("--seed", type=int, default=None,
                    help="seed for the shuffle, so a run can be repeated exactly")
    ap.add_argument("--leading", action="store_true",
                    help="⭐ report the LEADING-BIT agreement of every decoded payload instead of "
                         "pass/fail. A frame the beat cuts short is ours up to the cut and then "
                         "collapses, so the ceiling is the emission's null-free window measured in "
                         "bits — which is `indala224`'s whole story (C493).")
    ap.add_argument("--null", action="store_true",
                    help="run every read with NOTHING armed; every count must be zero")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    bad = [x for x in a.arms if x not in ARMS]
    if bad:
        print("unknown arm(s): %s\nknown: %s" % (", ".join(bad), ", ".join(ORDER)))
        return 2
    lengths = None
    if a.lengths:
        lengths = sorted({int(x) for x in a.lengths.split(",")})
        frames = {ARMS[k].frame for k in a.arms}
        if len(frames) > 1:
            print("⚠ --lengths across %d arms with DIFFERENT frame lengths %s: the same rung is a "
                  "different number of frames on each, so the rungs are NOT comparable across arms."
                  % (len(a.arms), sorted(frames)))
        elif len(a.arms) > 1:
            print("✅ --lengths across %d arms that all have a %d-sample frame, so each rung is the "
                  "same number of frames on every one of them and IS comparable across arms."
                  % (len(a.arms), frames.pop()))

    seed = a.seed if a.seed is not None else random.randrange(1 << 30)
    rng = random.Random(seed)
    print("order: %s (seed %d)" % ("SHUFFLED" if a.shuffle else "⛔ ASCENDING", seed))

    out = []
    try:
        if a.interleave:
            acc = {}
            for k in a.arms:
                ns = lengths or ladder(k)
                acc[k] = {"arm": k, "lengths": ns, "reader_n": ARMS[k].reader_n,
                          "frame": ARMS[k].frame, "long": [], "by_len": {n: [] for n in ns},
                          "raws": {n: [] for n in ns}, "raws_long": []}
            for _ in range(a.repeat):
                for key in a.arms:
                    r = run(a.port, key, 1, a.timeout, lengths=lengths, null=a.null,
                            shuffle=a.shuffle, rng=rng)
                    if "error" in r:
                        print("%-10s %s" % (key, r["error"]))
                        continue
                    acc[key]["long"] += r["long"]
                    acc[key]["raws_long"] += r.get("raws_long", [])
                    for n, v in r["by_len"].items():
                        acc[key]["by_len"][n] += v
                    for n, v in r.get("raws", {}).items():
                        acc[key]["raws"][n] += v
            out = [acc[k] for k in a.arms]
        else:
            # ⚠ valid per arm, NOT comparable across arms — M59.
            for key in a.arms:
                r = run(a.port, key, a.repeat, a.timeout, lengths=lengths, null=a.null,
                        shuffle=a.shuffle, rng=rng)
                out.append(r)
                if "error" in r:
                    print("%-10s %s" % (key, r["error"]))
        for r in out:
            if "error" not in r:
                print(fmt(r))
        if a.leading:
            print("\n⭐ LEADING-BIT AGREEMENT — how much of each decoded payload is ours before it "
                  "diverges.\n   bit period = frame_samples / bits, so a cut at T ms shows as "
                  "T/bit_period bits.")
            skipped = [r["arm"] for r in out
                       if "error" not in r and not ARMS[r["arm"]].raw_is_expect]
            if skipped:
                print("  ⛔ NOT MEASURABLE for %s: that reader's `Raw:` field is not the same "
                      "object as the expected credential, so a bit-0 comparison is meaningless "
                      "(`lf keri demod` prints 64 bits against a 32-bit internal ID, "
                      "cmdlfkeri.c:176; `lf nexwatch` reports a card number). ⭐ The alignment "
                      "control caught this rather than the numbers being read as low agreement."
                      % ", ".join(skipped))
            for r in out:
                if "error" in r or not ARMS[r["arm"]].raw_is_expect:
                    continue
                exp = ARMS[r["arm"]].expect
                nbits = len(exp) * 4
                rows = []
                for n in r["lengths"] + ["reader"]:
                    raws = r["raws_long"] if n == "reader" else r["raws"].get(n, [])
                    if not raws:
                        continue
                    got = [leading_bits(x, exp) for x in raws]
                    at0 = sorted(g[0] for g in got)
                    misaligned = sum(1 for g in got if g[1] > g[0])
                    rows.append("    %-8s n=%-3d leading %s of %d%s"
                                % (n, len(at0), "/".join(str(x) for x in at0), nbits,
                                   "   ⛔ %d payload(s) matched a SHIFT better than offset 0"
                                   % misaligned if misaligned else ""))
                if rows:
                    print("  %s (%d bits, bit period %.0f us):"
                          % (r["arm"], nbits, r["frame"] / nbits * 8.0))
                    print("\n".join(rows))
        print("\n* = the arm's OWN reader sample count, run through `lf read` + `demod` instead "
              "(H3's control)\n! = marker hit without an exact match: a decoded frame carrying "
              "the WRONG payload")
    finally:
        print("\ndisarming cu2 ...")
        seqdump.disarm(a.port)

    if a.null:
        tot = 0
        for r in out:
            tot += sum(1 for h, _ in r.get("long", []) if h)
            for v in r.get("by_len", {}).values():
                tot += sum(1 for h, _ in v if h)
        print("\n%s NULL: %d marker hits across every arm and length with NOTHING armed"
              % ("✅" if tot == 0 else "⛔⛔", tot))
    print("\n⛔ Ungraded: no null sweep, no calibration row, no licence. Moves no cell.")
    if a.json:
        print(json.dumps(out, indent=2, default=str))
    return 0


if __name__ == "__main__":
    sys.exit(main())
