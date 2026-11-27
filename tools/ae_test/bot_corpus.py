#!/usr/bin/env python3
"""bot_corpus: one table row per bot game, for a balance corpus (overnight bot runs, 2026-10-28).

    python3 tools/ae_test/bot_corpus.py label=path/to/debug.txt [label=debug.txt ...] [--cell 4]

Per game, from debug.txt (HALO_BOT_EVENTS=1): bot-seconds alive, kills and deaths of team 0 and team 1 (the bots'
only; a person who stands idle is not counted), the deaths by cause, the spots a bot stood still for 5 s while not
fighting (count and seconds), the share of the bot-seconds spent stuck, the ground cells a bot stood in (cells of --cell
world units) and, in SEARCH AND DESTROY, the rounds, plants and defuses of the host's bomb log, the carrier drops and the
last "bots: snd tick" zone line. Markdown out. The library half of bot_report.py does the stuck spots.
"""
import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bot_report  # noqa: E402

CAUSE = re.compile(r"ev=death .*? cause=(\w+)")


def one(label, path, cell):
    events = bot_report.parse(path)
    bots = bot_report.analyse(events)
    text = Path(path).read_text(errors="replace")
    teams = defaultdict(lambda: [0, 0])
    stuck_n = stuck_s = alive = 0
    for r in bots.values():
        t = r["team"] if r["team"] is not None else 0
        teams[t][0] += r["kills"]
        teams[t][1] += r["deaths"]
        stuck_n += len(r["stuck"])
        stuck_s += sum(s["seconds"] for s in r["stuck"])
        alive += r["alive_s"]
    causes = Counter(CAUSE.findall(text))
    cells = {(round(float(e["x"]) / cell), round(float(e["y"]) / cell), round(float(e["z"]) / 3.0))
             for e in events if e["ev"] == "pos"}
    skills = sorted({r["skill"] for r in bots.values()})
    snd = {"rounds": len(re.findall(r"round \d+ ends", text)), "plants": len(re.findall(r"bomb: host tick \d+ plant ", text)),
           "saves": len(re.findall(r"bomb: host tick \d+ save ", text)),
           "drops": len(re.findall(r"ev=objective_drop", text)), "pickups": len(re.findall(r"ev=objective_pickup", text))}
    zones = ""
    for line in text.splitlines()[::-1]:
        if "bots: snd tick" in line:
            zones = re.sub(r"^.*bots: snd tick \d+: ", "", line)
            break
    return {"label": label, "skills": "/".join(skills), "alive": alive, "teams": dict(teams), "causes": causes,
            "stuck_n": stuck_n, "stuck_s": stuck_s, "cells": len(cells), "snd": snd, "zones": zones}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("games", nargs="+", help="label=debug.txt")
    p.add_argument("--cell", type=float, default=4.0)
    a = p.parse_args(argv)
    rows = [one(*g.split("=", 1), a.cell) for g in a.games]
    print("| game | skill | bot-s alive | red K/D | blue K/D | deaths by cause | stuck spots (s) | stuck share | cells | rounds | plants | saves | drops/pickups |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        t = r["teams"]
        kd = lambda k: "%d/%d" % tuple(t.get(k, [0, 0]))
        causes = ", ".join("%s %d" % (c, n) for c, n in r["causes"].most_common(4))
        share = "%.1f%%" % (100.0 * r["stuck_s"] / r["alive"]) if r["alive"] else "-"
        s = r["snd"]
        print("| %s | %s | %d | %s | %s | %s | %d (%.0f) | %s | %d | %d | %d | %d | %d/%d |" % (
            r["label"], r["skills"], r["alive"], kd(0), kd(1), causes, r["stuck_n"], r["stuck_s"], share, r["cells"],
            s["rounds"], s["plants"], s["saves"], s["drops"], s["pickups"]))
    for r in rows:
        if r["zones"]:
            print("\n%s: %s" % (r["label"], r["zones"]))


if __name__ == "__main__":
    main()
