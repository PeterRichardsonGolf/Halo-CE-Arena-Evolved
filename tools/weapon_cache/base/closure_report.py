#!/usr/bin/env python3
"""BASE-CLOSURE.md from a closure.tsv (group, name, then one 0/1 column per map: is that tag in the map).

usage: closure_report.py closure.tsv > BASE-CLOSURE.md
Maps whose name starts with a letter and a digit pair (a10, b30, c10, d20, ...) are campaign levels; the rest are
multiplayer maps. The base layer carries a copy of every row, so a name missing from a map is supplied by the base
copy there; the report says how much the base layer is doing on each family.
"""
import re
import sys
from collections import Counter


def main(path):
    with open(path) as handle:
        header = handle.readline().rstrip("\n").split("\t")
        rows = [line.rstrip("\n").split("\t") for line in handle if line.strip()]
    maps = header[2:]
    campaign = [m for m in maps if re.fullmatch(r"[a-d]\d\d", m)]
    out = ["# Base layer closure", "",
           f"{len(rows)} base names checked against {len(maps)} maps (tag present in the map: 1).", "",
           "| map | kind | present | missing |", "|---|---|---|---|"]
    for i, m in enumerate(maps):
        present = sum(1 for r in rows if r[2 + i] == "1")
        out.append(f"| {m} | {'campaign' if m in campaign else 'multiplayer'} | {present} | {len(rows) - present} |")
    mp = [i for i, m in enumerate(maps) if m not in campaign]
    cp = [i for i, m in enumerate(maps) if m in campaign]
    missing_mp = [r for r in rows if any(r[2 + i] == "0" for i in mp)]
    missing_cp = [r for r in rows if any(r[2 + i] == "0" for i in cp)]
    always = [r for r in rows if cp and all(r[2 + i] == "0" for i in cp)]
    out += ["", f"Missing from at least one multiplayer map checked: {len(missing_mp)}.",
            f"Missing from at least one campaign level checked: {len(missing_cp)}; from every campaign level checked: "
            f"{len(always)}.", "",
            "By group, names missing from at least one campaign level:", ""]
    by_group = Counter(r[0] for r in missing_cp)
    for g, n in sorted(by_group.items(), key=lambda x: -x[1]):
        out.append(f"- {g}: {n}")
    out += ["", "Names missing from every campaign level checked (the base copy is the only source there):", ""]
    out += [f"- {r[0]}:{r[1]}" for r in always]
    print("\n".join(out))


if __name__ == "__main__":
    main(sys.argv[1])
