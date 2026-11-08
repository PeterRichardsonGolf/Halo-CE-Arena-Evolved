#!/usr/bin/env python3
"""bot_report: what the bots did in a game, from the "bot event:" lines of debug.txt (debug.bot_events = true,
HALO_BOT_EVENTS=1; port/linux/game/ae_bots_events.inc), as Markdown tables.

    python3 tools/ae_test/bot_report.py debug.txt [more debug.txt ...] [--warmup-ticks 120] [--stuck-seconds 5]

One file is one game. Per skill, per team (team games: the team= of the bots' position lines) and per bot: shots, hits, hit rate, reaction (target seen -> first shot, median
and p90, seconds), time to kill (first shot at a target -> its death), engagement range (median distance of
shots, world units), strafe changes per second of fighting, crouches per minute alive, blows, kills, deaths; then
the spots where a bot stood still more than --stuck-seconds while not fighting.

Notes on the numbers: 30 ticks a second. Events before --warmup-ticks (the pre-game countdown, where bots stand) are dropped, and
so are first shots of a target seen before it. Hits are counted once for each bot, tick and victim (a shotgun's
pellets are one hit), and a hit that follows a blow without a shot is a blow's, not a shot's. A shot is seen one
tick after the trigger. A kill's time-to-kill starts at the bot's first shot at that target since its previous
kill or death (at most 60 s back).
"""
import argparse
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

TICKS = 30.0
LINE = re.compile(r"bot event: t=(-?\d+) bot=(\d+) skill=(\w+) ev=(\w+) target=(-?\d+) dist=(-?[\d.]+)(.*)$")
WORD = re.compile(r"(\w+)=(\S+)")
MELEE_WINDOW = 8        # ticks: a hit this soon after a blow, with no shot between, is the blow's
MAX_TTK_TICKS = 60 * 30
STUCK_RADIUS = 1.0      # world units
SKILL_ORDER = ["recruit", "easy", "normal", "heroic", "legendary", "demon"]


def parse(path):
    """The events of one debug.txt: dicts with t, bot, skill, ev, target, dist and the extra words."""
    events = []
    for line in Path(path).read_text(errors="replace").splitlines():
        m = LINE.search(line)
        if not m:
            continue
        event = {"t": int(m.group(1)), "bot": int(m.group(2)), "skill": m.group(3), "ev": m.group(4),
                 "target": int(m.group(5)), "dist": float(m.group(6))}
        event.update(WORD.findall(m.group(7)))
        events.append(event)
    events.sort(key=lambda e: e["t"])
    return events


def game_name(path):
    p = Path(path).resolve()
    for part in (p.parent, *p.parents):
        if part.name not in ("data", "") and part.name != p.name:
            return part.name
    return p.name


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(fraction * len(ordered)))]


def median(values):
    return statistics.median(values) if values else None


def analyse(events, warmup=120, stuck_seconds=5.0):
    """Per bot (by its number) the numbers of one game, and its stuck spots."""
    bots = {}
    by_bot = defaultdict(list)
    for e in events:
        by_bot[e["bot"]].append(e)
    for bot, evs in sorted(by_bot.items()):
        r = {"skill": evs[0]["skill"], "shots": 0, "hits": 0, "melee_hits": 0, "melee": 0, "kills": 0, "deaths": 0,
             "reaction": [], "ttk": [], "range": [], "strafes": 0, "crouches": 0, "alive_s": 0, "fight_s": 0,
             "stuck": [], "team": None}
        shot_ticks, melee_ticks, hit_keys = [], [], set()
        seen_at = {}
        boundary = None     # the bot's last kill or death
        first_shots = []    # (t, target)
        pos = []
        for e in evs:
            kind, t = e["ev"], e["t"]
            if kind == "pos":
                pos.append(e)
                if "team" in e:
                    r["team"] = int(e["team"])
                r["alive_s"] += 1
                if e.get("fighting") == "1":
                    r["fight_s"] += 1
            if t < warmup:
                continue
            if kind == "seen":
                seen_at[e["target"]] = t
            elif kind == "first_shot":
                since = int(e.get("since_seen", -1))
                if since >= 0 and t - since >= warmup:
                    r["reaction"].append(since / TICKS)
                first_shots.append((t, e["target"]))
            elif kind == "shot":
                r["shots"] += 1
                r["range"].append(e["dist"])
                shot_ticks.append(t)
            elif kind == "melee":
                r["melee"] += 1
                melee_ticks.append(t)
            elif kind == "hit":
                key = (t, e["target"])
                if key in hit_keys:
                    continue
                hit_keys.add(key)
                if (any(0 <= t - m <= MELEE_WINDOW for m in melee_ticks) and
                        not any(0 <= t - s <= MELEE_WINDOW for s in shot_ticks)):
                    r["melee_hits"] += 1
                else:
                    r["hits"] += 1
            elif kind == "strafe":
                r["strafes"] += 1
            elif kind == "crouch" and e.get("state") == "start":
                r["crouches"] += 1
            elif kind == "kill":
                r["kills"] += 1
                starts = [ft for ft, target in first_shots
                          if target == e["target"] and (boundary is None or ft > boundary) and 0 <= t - ft <= MAX_TTK_TICKS]
                if starts:
                    r["ttk"].append((t - min(starts)) / TICKS)
                boundary = t
            elif kind == "death":
                r["deaths"] += 1
                boundary = t
        r["stuck"] = stuck_spots([p for p in pos if p["t"] >= warmup], stuck_seconds)
        bots[bot] = r
    return bots


def stuck_spots(pos, stuck_seconds):
    """Spots a bot stayed within a unit of for the given seconds while it was not fighting (a gap of more than
    two seconds between its position lines is a death)."""
    spots = []
    anchor = None   # (x, y, z, start tick, last tick)

    def close():
        if anchor and (anchor[4] - anchor[3]) / TICKS >= stuck_seconds:
            spots.append({"x": anchor[0], "y": anchor[1], "z": anchor[2], "start": anchor[3],
                          "seconds": (anchor[4] - anchor[3]) / TICKS})

    for p in pos:
        x, y, z = float(p["x"]), float(p["y"]), float(p["z"])
        fighting = p.get("fighting") == "1"
        if anchor and not fighting and p["t"] - anchor[4] <= 2 * TICKS and \
                ((x - anchor[0]) ** 2 + (y - anchor[1]) ** 2 + (z - anchor[2]) ** 2) ** 0.5 <= STUCK_RADIUS:
            anchor = (anchor[0], anchor[1], anchor[2], anchor[3], p["t"])
            continue
        close()
        anchor = None if fighting else (x, y, z, p["t"], p["t"])
    close()
    return spots


def fmt(value, digits=2, suffix=""):
    return "-" if value is None else f"{value:.{digits}f}{suffix}"


def row(label, r):
    rate = r["hits"] / r["shots"] if r["shots"] else None
    return [label, str(r["shots"]), str(r["hits"]), fmt(None if rate is None else rate * 100, 0, "%"),
            fmt(median(r["reaction"]), 2), fmt(percentile(r["reaction"], 0.9), 2), fmt(median(r["ttk"]), 1),
            fmt(median(r["range"]), 1), fmt(r["strafes"] / r["fight_s"] if r["fight_s"] else None, 2),
            fmt(r["crouches"] / (r["alive_s"] / 60.0) if r["alive_s"] else None, 1), str(r["melee"]),
            str(r["kills"]), str(r["deaths"]), str(len(r["ttk"])), str(len(r["reaction"]))]


HEADER = ["", "shots", "hits", "hit rate", "reaction med s", "reaction p90 s", "time to kill med s", "range med",
          "strafes /s fighting", "crouches /min", "blows", "kills", "deaths", "(kills timed)", "(reactions)"]


def table(rows):
    out = ["| " + " | ".join(HEADER) + " |", "|" + "|".join(["---"] * len(HEADER)) + "|"]
    out += ["| " + " | ".join(r) + " |" for r in rows]
    return "\n".join(out)


TEAM_NAMES = {0: "red", 1: "blue"}


def merge(results):
    total = {"skill": results[0]["skill"], "stuck": [], "team": results[0].get("team")}
    for key in ("shots", "hits", "melee_hits", "melee", "kills", "deaths", "strafes", "crouches", "alive_s", "fight_s"):
        total[key] = sum(r[key] for r in results)
    for key in ("reaction", "ttk", "range"):
        total[key] = [v for r in results for v in r[key]]
    return total


def report(paths, warmup=120, stuck_seconds=5.0):
    games = []
    for path in paths:
        events = parse(path)
        games.append((game_name(path), analyse(events, warmup, stuck_seconds)))
    by_skill = defaultdict(list)
    for name, bots in games:
        for bot, r in bots.items():
            by_skill[r["skill"]].append(r)
    skills = sorted(by_skill, key=lambda s: SKILL_ORDER.index(s) if s in SKILL_ORDER else 99)
    out = ["# Bot report", "", f"{len(games)} game(s), {sum(len(b) for _, b in games)} bot(s); "
           f"warm-up {warmup} ticks dropped.", "", "## By skill", ""]
    out.append(table([row(s, merge(by_skill[s])) for s in skills]))
    by_team = defaultdict(list)
    for name, bots in games:
        for bot, r in bots.items():
            if r["team"] is not None:
                by_team[r["team"]].append(r)
    if by_team:
        out += ["", "## By team", ""]
        out.append(table([row(TEAM_NAMES.get(t, f"team {t}"), merge(by_team[t])) for t in sorted(by_team)]))
    out += ["", "## By bot", ""]
    rows = []
    for name, bots in games:
        for bot, r in bots.items():
            team = "" if r["team"] is None else ", " + TEAM_NAMES.get(r["team"], f"team {r['team']}")
            rows.append(row(f"{name} bot {bot} ({r['skill']}{team})", r))
    out.append(table(rows))
    stuck = [(name, bot, s) for name, bots in games for bot, r in bots.items() for s in r["stuck"]]
    out += ["", f"## Stuck spots (still more than {stuck_seconds:g} s, not fighting)", ""]
    if not stuck:
        out.append("None.")
    else:
        out += ["| game | bot | x | y | z | from tick | seconds |", "|---|---|---|---|---|---|---|"]
        out += [f"| {name} | {bot} | {s['x']:.1f} | {s['y']:.1f} | {s['z']:.1f} | {s['start']} | {s['seconds']:.0f} |"
                for name, bot, s in stuck]
    return "\n".join(out) + "\n"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("files", nargs="+", help="debug.txt files, one game each")
    p.add_argument("--warmup-ticks", type=int, default=120)
    p.add_argument("--stuck-seconds", type=float, default=5.0)
    a = p.parse_args(argv)
    sys.stdout.write(report(a.files, a.warmup_ticks, a.stuck_seconds))


if __name__ == "__main__":
    main()
