#!/usr/bin/env python3
"""A test player's route from a played game's log: the AI squad's position lines (written every half second with
debug.bot_events on) give where the human player walked; each becomes a point of `setup player route` lines for a test
list, so the test player walks the way a person did.

  route_from_log.py <debug.txt> [--from LINE] [--to LINE] [--step UNITS]

The first point is the first one on the ground; a point under 0.4 (--step) from the last one kept is left out; the
point before one where the player is in the air is marked for a crouch jump, and a point he stood at 1.5 s or more
for a wait that long, facing his aim there when the log has it; at most the route's 32 points are written."""

import argparse
import re
import sys

POSITIONS = re.compile(r"companion: positions t\+\d+s human (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) \(([^,)]*)")
MOST_POINTS = 32
LEAST_STEP = 0.4
# (his stops: the position lines' period with bot events on; positions within STILL of a kept point for WAIT_LEAST seconds or
# more make it a wait, at most WAIT_MOST)
SAMPLE_SECONDS = 0.5
STILL = 0.3
WAIT_LEAST = 1.5
WAIT_MOST = 60.0
LINE_MOST = 119
HEAD = "setup player route"


AIM = re.compile(r"human [^(]*\([^)]*\) aim (-?\d+)")


def points_of(lines):
    """[(x, y, z, in_air, aim or None)] of the human player's positions in the lines"""
    found = []
    for line in lines:
        match = POSITIONS.search(line)
        if match:
            x, y, z = (float(match.group(index)) for index in (1, 2, 3))
            aim = AIM.search(line)
            found.append((x, y, z, match.group(4).startswith("air"), int(aim.group(1)) if aim else None))
    return found


def route_lines(lines, step=LEAST_STEP):
    """the setup lines of the route the logged positions walk; points under step apart left out"""
    found = points_of(lines)
    while found and found[0][3]:
        found.pop(0)
    kept, still, faced = [], [], []
    for index, (x, y, z, _, aim) in enumerate(found):
        away = ((x - kept[-1][0]) ** 2 + (y - kept[-1][1]) ** 2) ** 0.5 if kept else None
        if away is not None and away < STILL and len(still) == len(kept):
            still[-1] += 1
            # (the way he faced as he stood there, last seen)
            if aim is not None:
                faced[-1] = aim
        if away is not None and away < step:
            continue
        if len(kept) == MOST_POINTS:
            break
        jump = index + 1 < len(found) and found[index + 1][3]
        kept.append((x, y, z, jump))
        still.append(0)
        faced.append(None)
    words = []
    for index, ((x, y, z, jump), samples, face) in enumerate(zip(kept, still, faced)):
        # (a stand at the first point (the test's start) or the last (its end) is no wait)
        wait = 0.0 if index in (0, len(kept) - 1) else min(samples * SAMPLE_SECONDS, WAIT_MOST)
        stand = f" wait {wait:.1f}" + (f" face {face}" if face is not None else "") if wait >= WAIT_LEAST else ""
        words.append(f"{x:.1f} {y:.1f} {z + 0.1:.1f}" + stand + (" crouch jump" if jump else ""))
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
    parser.add_argument("--step", type=float, default=LEAST_STEP, help="the least distance between two points kept")
    args = parser.parse_args(argv)
    with open(args.log, errors="replace") as handle:
        lines = handle.read().splitlines()
    lines = lines[args.first - 1:args.last or None]
    for line in route_lines(lines, args.step):
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
