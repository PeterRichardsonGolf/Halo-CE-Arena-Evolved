#!/usr/bin/env python3
"""bot_flow: how a bot game flowed, from the "bot event:" lines of debug.txt (debug.bot_events = true).

    python3 tools/ae_test/bot_flow.py path/to/debug.txt [path ...]

Time on each job, the crews and how their routes ended, how long a seat took and why bots gave one up or got out, why a target
was picked, how flanks ended, what was fired, and every flag or ball lost alive. Works on any log; a table is left out when its events are not in it.
"""
import argparse
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bot_report  # noqa: E402

THREATS = ("gun", "carrier", "aiming", "power", "driving")


def median(values):
    return round(statistics.median(values), 1) if values else None


def counted(counter, limit=8):
    return ", ".join(f"{name} {count}" for name, count in counter.most_common(limit)) or "none"


def flow(events):
    """The tables of one game, as label and text."""
    lines = []
    pos = [e for e in events if e["ev"] == "pos"]
    if pos and "job" in pos[0]:
        jobs = Counter(e["job"] for e in pos)
        lines.append(("jobs (bot-seconds)", counted(jobs, 12)))
        lines.append(("seats (bot-seconds)", counted(Counter(e["seat"] for e in pos))))
        lines.append(("with a human (bot-seconds)", str(sum(e["with"] != "-1" for e in pos))))
        lines.append(("held weapon (bot-seconds)", counted(Counter(e["weapon"] for e in pos))))
        crews = defaultdict(set)
        for e in pos:
            if e["crew"] != "0" and e.get("team") is not None:
                crews[e["team"], e["crew"]].add(e["bot"])
        lines.append(("crews", ", ".join(f"team {t} crew {c}: {len(b)} bots" for (t, c), b in sorted(crews.items())) or "none"))
    ends = [e for e in events if e["ev"] == "route_end"]
    routes = Counter(e["kind"] for e in events if e["ev"] == "route")
    if routes:
        lines.append(("routes picked", counted(routes)))
        by_kind = defaultdict(Counter)
        for e in ends:
            by_kind[e["kind"]][e["result"] + ("/" + e["why"] if "why" in e else "")] += 1
        lines.append(("routes ended", "; ".join(f"{k}: {counted(c)}" for k, c in sorted(by_kind.items())) or "none"))
    orders = [e for e in events if e["ev"] == "seat_order"]
    if orders:
        lines.append(("seat orders", counted(Counter(e["kind"] for e in orders))))
        waits = [float(e["wait"]) for e in events if e["ev"] == "seat" and float(e["wait"]) >= 0.0]
        lines.append(("seconds to a seat", f"median {median(waits)}, longest {max(waits) if waits else None}, {len(waits)} seated"))
        lost = Counter()
        for e in events:
            if e["ev"] == "seat_lost":
                why = [k for k, bad in (("holds", e["holds"] == "1"), ("carrying", e["carrying"] == "1"), ("taken", e["free"] == "0"),
                                        ("wrecked", e["alive"] == "0"), ("tipped", e["up"] == "0")) if bad]
                lost["+".join(why) or "too_long_or_too_fast"] += 1
        lines.append(("seats given up", counted(lost)))
    leaves = [e for e in events if e["ev"] == "seat_leave"]
    if leaves:
        lines.append(("got out", counted(Counter(f'{e["seat"]}, driver {e["driver"]}: {e["reason"]}' for e in leaves), 12)))
        lines.append(("seconds in a seat", f'median {median([float(e["seated"]) for e in leaves])}'))
    threats = [e for e in events if e["ev"] == "threat"]
    if threats:
        reasons = Counter(k for e in threats for k in THREATS if e[k] == "1")
        lines.append(("targets picked", f"{len(threats)}: {counted(reasons)}"))
        guns = [e for e in threats if e["self_gun"] == "1"]
        lines.append(("a gunner's targets", f'{len(guns)}, on an enemy gun {sum(e["gun"] == "1" for e in guns)}'))
    starts = sum(e["ev"] == "flank_start" for e in events)
    if starts:
        ends = [e for e in events if e["ev"] == "flank_end"]
        lines.append(("flanks", f'{starts} started, ended: {counted(Counter(e["why"] for e in ends))}'))
        lines.append(("flank seconds", f'median {median([float(e["seconds"]) for e in ends])}, still to go median {median([float(e["left"]) for e in ends if float(e["left"]) >= 0.0])}'))
    no_place = sum(e["ev"] == "flank_no_place" for e in events)
    if no_place:
        lines.append(("flanks with no place (bot, 5 s)", str(no_place)))
    shots = Counter(e.get("weapon", "?").rsplit("\\", 1)[-1] for e in events if e["ev"] == "shot")
    if shots:
        lines.append(("shots", counted(shots)))
    for e in events:
        if e["ev"] == "objective_lost":
            lines.append(("lost alive", f't={e["t"]} bot {e["bot"]} flags={e["flags"]} vitality={e["vitality"]} seat={e["seat"]} job={e["job"]} target={e["target"]}'))
    return lines


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+")
    args = parser.parse_args(argv)
    for path in args.logs:
        print(f"## {path}")
        for label, text in flow(bot_report.parse(path)):
            print(f"- {label}: {text}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
