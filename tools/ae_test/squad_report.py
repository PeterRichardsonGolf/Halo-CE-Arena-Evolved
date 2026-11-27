#!/usr/bin/env python3
"""squad_report: what the campaign AI squad did with each order, from a game's debug.txt (the "squad order:" lines of
port/linux/game/ae_bots_squad.inc, debug.bot_events on).

    python3 tools/ae_test/squad_report.py debug.txt [--session N] [--slow 8]

One table row an order, of the last game session in the file (--session 2 is the one before it; a session ends at
"window closed"): the game second, the order, the point man, how far each member was from its place, the seconds until
each was in position (- for never before the next order), and what each member did on the way. Then the orders to look
at: a member not in position before the next order (when that came --slow seconds or more later), one slower than
--slow seconds, one with no way to its place.
Seconds come from the squad's own "t+Ns" lines, so they are whole seconds of the test in play."""
import argparse
import re
import sys
from pathlib import Path

ORDER = re.compile(r"squad order: (move|attack|defend|watch|mount|fall in)\b(.*)")
PLACE = re.compile(r"squad order: \w+: (\d)'s place .* \(([\d.]+) away\)")
POINT = re.compile(r"squad order: \w+: (\d) is the point man")
ARRIVE = re.compile(r"squad order: \w+: (\d) in position")
DOES = re.compile(r"squad order: (\d) ((?:walks|stands|fights|has no way|waits|goes|is at|takes)[^(]*)")
OLD = re.compile(r"squad order: (\d) finds no (way|path)")
STAMP = re.compile(r"companion: t\+(\d+)s")
TEST = re.compile(r"test run setup ready (\S+)")


def sessions(lines):
    out, start = [], 0
    for i, line in enumerate(lines):
        if "window closed" in line:
            out.append(lines[start:i])
            start = i + 1
    out.append(lines[start:])
    return [s for s in out if any("squad order:" in line for line in s)]


def orders(lines):
    rows, row, second, test = [], None, 0, ""
    for line in lines:
        m = STAMP.search(line)
        if m:
            second = int(m.group(1))
        m = TEST.search(line)
        if m:
            test, second = m.group(1), 0
        m = PLACE.search(line)
        if m and (row is None or m.group(1) in row["away"]):
            row = None
        m = ORDER.search(line)
        if not m:
            m2 = DOES.search(line) or OLD.search(line)
            if m2 and row:
                word = m2.group(2).strip() if m2.re is DOES else "has no " + m2.group(2)
                row["did"].setdefault(m2.group(1), [])
                if word not in row["did"][m2.group(1)]:
                    row["did"][m2.group(1)].append(word)
            continue
        kind, rest = m.group(1), m.group(2)
        if row is None or row["kind"] != kind or "is the point man" in rest or kind in ("fall in",) or \
                (kind == "mount" and second != row["second"]):
            row = {"test": test, "second": second, "kind": kind, "point": "", "away": {}, "took": {}, "did": {}, "note": []}
            rows.append(row)
        m = POINT.search(line)
        if m:
            row["point"] = m.group(1)
        m = PLACE.search(line)
        if m:
            row["away"][m.group(1)] = float(m.group(2))
        m = ARRIVE.search(line)
        if m:
            row["took"].setdefault(m.group(1), second - row["second"])
        if "no vehicle" in rest or "gets out" in rest or "sent to the vehicle" in rest:
            row["note"].append(rest.strip(": ")[:40])
    return rows


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("file")
    p.add_argument("--session", type=int, default=1, help="1 = the last session with squad orders")
    p.add_argument("--slow", type=float, default=8.0)
    a = p.parse_args(argv)
    found = sessions(Path(a.file).read_text(errors="replace").splitlines())
    if len(found) < a.session:
        print("squad_report: no such session (%d with squad orders)" % len(found))
        return 1
    rows = orders(found[-a.session])
    flagged = []
    print("| test | t+s | order | point | away | s to position | on the way |")
    print("|---|---|---|---|---|---|---|")
    for i, row in enumerate(rows):
        nxt = rows[i + 1] if i + 1 < len(rows) and rows[i + 1]["test"] == row["test"] else None
        had = nxt["second"] - row["second"] if nxt else None
        away = " ".join("%s:%.1f" % kv for kv in sorted(row["away"].items()))
        took = " ".join("%s:%s" % (k, row["took"].get(k, "-")) for k in sorted(row["away"]))
        did = "; ".join("%s %s" % (k, ", ".join(v)) for k, v in sorted(row["did"].items())) or " ".join(row["note"])
        print("| %s | %d | %s | %s | %s | %s | %s |" % (row["test"], row["second"], row["kind"], row["point"], away, took, did))
        if row["kind"] in ("mount", "fall in", "watch"):
            continue
        for k in sorted(row["away"]):
            if k not in row["took"] and had is not None and had < a.slow:
                continue  # (a new order came before it could have got there)
            if k not in row["took"]:
                flagged.append("%s t+%d %s: %s (%.1f away) NOT in position before the next order" % (row["test"], row["second"], row["kind"], k, row["away"][k]))
            elif row["took"][k] > a.slow:
                flagged.append("%s t+%d %s: %s took %d s for %.1f" % (row["test"], row["second"], row["kind"], k, row["took"][k], row["away"][k]))
        for k, words in sorted(row["did"].items()):
            if any("no way" in w for w in words):
                flagged.append("%s t+%d %s: %s had no way to its place" % (row["test"], row["second"], row["kind"], k))
    print("\n%d orders; to look at: %d" % (len(rows), len(flagged)))
    for line in flagged:
        print("- " + line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
