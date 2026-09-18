#!/usr/bin/env python3
"""⭐ Seed-to-seed REPRODUCIBILITY of every banked cap pair, in date order. Offline, no bench.

⛔⛔ WHY IT EXISTS. K34a's burst-500 control hit 0 of 10 regions where C538's burst-500 run hit
8, one day apart on an unmoved bench (C549). Two readings explain that and they point opposite
ways: **today's pair was an outlier**, in which case K34a is simply worth re-running; or **the
structure has been weakening across the round**, in which case that reaches back over every band
on this line and must be reported before anything new is run. ⭐ Every cap needed to tell them
apart is already banked, so this costs no bench time and no capacity on captures.

WHAT IT COMPUTES. For every pair of caps sharing a name prefix (so: the same band, the same
ladder, differing only in seed), for every arm they share, the **Spearman rank agreement between
the two seeds' per-cell decode profiles**. That is the same statistic the K-series gates use for
split-half drift, applied BETWEEN seeds instead of within one.

⚠⚠ READ IT BY LADDER, NEVER POOLED. A 15-cell dec-2 ladder and a 38-cell dec-1 ladder do not
produce comparable correlations — fewer cells and a lower level both depress it for reasons that
have nothing to do with the bench. ⇒ the `cells` and `reps` columns are part of every reading
(M73), and only rows matching in both may be compared.

⭐ THE ONE COMPARISON THAT IS SOUND AS OF C549: the **38-cell dec-1** rows. They run ~+0.82 in the
morning (k26/k27/k29) against **+0.573 to +0.751** for K34a in the afternoon — ⚠ and K34a used
**reps 16 against their 8**, which should have RAISED agreement by cutting per-cell sampling
noise. ⇒ the decline is in the wrong direction for a sampling artefact.

⛔ AN AUDIT, NOT A BAND. It fires nothing, has no threshold and moves no cell. ⛔ And it is
DESCRIPTIVE of caps taken for other purposes: nothing here was randomised against time of day, so
it can show that agreement moved but never why.
"""
import collections
import datetime
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def _rank(v):
    s = sorted(range(len(v)), key=lambda i: v[i])
    r = [0.0] * len(v)
    i = 0
    while i < len(s):
        j = i
        while j + 1 < len(s) and v[s[j + 1]] == v[s[i]]:
            j += 1
        for k in range(i, j + 1):
            r[s[k]] = (i + j) / 2.0
        i = j + 1
    return r


def spearman(a, b):
    ra, rb = _rank(a), _rank(b)
    n = len(a)
    ma, mb = sum(ra) / n, sum(rb) / n
    num = sum((x - ma) * (y - mb) for x, y in zip(ra, rb))
    den = (sum((x - ma) ** 2 for x in ra) * sum((y - mb) ** 2 for y in rb)) ** 0.5
    return num / den if den else float("nan")


def _profile(cells, keys):
    return [100.0 * sum(1 for _, e in cells[k]["scores"] if e) / len(cells[k]["scores"])
            for k in keys]


def main():
    capdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "caps")
    groups = collections.defaultdict(list)
    for f in sorted(os.listdir(capdir)):
        if not f.endswith(".json"):
            continue
        m = re.match(r"^(.*)_s(\d+)$", f[:-5])
        if not m:
            continue
        path = os.path.join(capdir, f)
        try:
            groups[m.group(1)].append((f[:-5], json.load(open(path)), os.path.getmtime(path)))
        except Exception as exc:                       # noqa: BLE001 - reported, never swallowed
            print("   ⛔ %s unreadable: %s" % (f, exc))
    rows = []
    for _, items in groups.items():
        if len(items) < 2:
            continue
        items.sort(key=lambda x: x[2])
        for i in range(len(items) - 1):
            for j in range(i + 1, len(items)):
                n1, d1, t1 = items[i]
                n2, d2, t2 = items[j]
                for arm in sorted(set(d1) & set(d2)):
                    c1, c2 = d1[arm], d2[arm]
                    ks = [k for k in c1
                          if k != "none" and not k.startswith("_") and k in c2]
                    if len(ks) < 10:
                        continue
                    ks.sort(key=float)
                    rows.append((max(t1, t2), n1, n2, arm, len(ks),
                                 len(c1[ks[0]]["scores"]),
                                 spearman(_profile(c1, ks), _profile(c2, ks))))
    rows.sort()
    print("## Seed-to-seed reproducibility of every banked cap pair (C549). ⛔ An audit, not a band.")
    print("   ⚠ COMPARE ONLY ROWS MATCHING IN `cells` AND `reps` — a shorter ladder and a lower")
    print("     level both depress r for reasons that are not the bench (M73).\n")
    print("   %-26s %-9s %5s %5s %7s  %s"
          % ("pair", "arm", "cells", "reps", "r", "when"))
    for t, n1, n2, arm, nc, nr, r in rows:
        print("   %-26s %-9s %5d %5d  %+.3f  %s"
              % (os.path.commonprefix([n1, n2]).rstrip("_s") or n1, arm, nc, nr, r,
                 datetime.datetime.fromtimestamp(t).strftime("%m-%d %H:%M")))
    print("\n⛔ Ungraded, and not a band — it fires nothing and moves no cell.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
