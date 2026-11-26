#!/usr/bin/env python3
"""A test player's route from a played game's log: the AI squad's position lines (written every 2 s with
debug.bot_events on) give where the human player walked; each becomes a point of `setup player route` lines for a test
list, so the test player walks the way a person did.

  route_from_log.py <debug.txt> [--from LINE] [--to LINE]

The first point is the first one on the ground; a point under 0.4 from the last one kept is left out; the point before
one where the player is in the air is marked for a crouch jump; at most the route's 32 points are written."""

import argparse
import re
import sys

POSITIONS = re.compile(r"companion: positions t\+\d+s human (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) \(([^,)]*)")
MOST_POINTS = 32
LEAST_STEP = 0.4
LINE_MOST = 119
HEAD = "setup player route"


def points_of(lines):
    """[(x, y, z, in_air)] of the human player's positions in the lines"""
    found = []
    for line in lines:
        match = POSITIONS.search(line)
        if match:
            x, y, z = (float(match.group(index)) for index in (1, 2, 3))
            found.append((x, y, z, match.group(4).startswith("air")))
    return found


def route_lines(lines):
    """the setup lines of the route the logged positions walk"""
    found = points_of(lines)
    while found and found[0][3]:
        found.pop(0)
    kept = []
    for index, (x, y, z, _) in enumerate(found):
        if kept and ((x - kept[-1][0]) ** 2 + (y - kept[-1][1]) ** 2) ** 0.5 < LEAST_STEP:
            continue
        jump = index + 1 < len(found) and found[index + 1][3]
        kept.append((x, y, z, jump))
        if len(kept) == MOST_POINTS:
            break
    words = []
    for x, y, z, jump in kept:
        words.append(f"{x:.1f} {y:.1f} {z + 0.1:.1f}" + (" crouch jump" if jump else ""))
    out, line = [], HEAD
    for word in words:
        if len(line) + 1 + len(word) > LINE_MOST:
            out.append(line)
            line = HEAD
        line += " " + word
    if line != HEAD:
        out.append(line)
    return [entry.replace(" -0.0 ", " 0.0 ") for entry in out]


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("--from", dest="first", type=int, default=1, help="the first line read (1 the first)")
    parser.add_argument("--to", dest="last", type=int, default=0, help="the last line read (0 the end)")
    args = parser.parse_args(argv)
    with open(args.log, errors="replace") as handle:
        lines = handle.read().splitlines()
    lines = lines[args.first - 1:args.last or None]
    for line in route_lines(lines):
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
