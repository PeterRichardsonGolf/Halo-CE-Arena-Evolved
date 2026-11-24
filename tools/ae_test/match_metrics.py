#!/usr/bin/env python3
"""match_metrics: a match film's numbers as JSON, for agents (bot tuning, spawn fixing, bot against human).

    python3 tools/ae_test/match_metrics.py FILM [FILM ...] [--cell 4] [--stuck-seconds 5] > out.json
    python3 tools/ae_test/match_metrics.py --compare A.json B.json [--threshold 0.15]   # exit 1 past the threshold

A film is the file debug.match_film writes (port/linux/game/ae_match_film.c): "ae obj:" lines, one schema
(ae_objective_log_rules.c), with every living player's pos line ten times a second. A debug.txt with "ae obj:" lines
is read the same way (without pos lines it has no paths). Times are seconds from the film's first line, 30 ticks a second.

Per film: {"film": {map, engine, gametype, complete, seconds}, "players": {slot: {...}}, "grids": {...}}.
A film with no game_end (the host quit, a crash) is complete: false. A player's lives: a spawn, a long gap between pos lines
or a jump with no spawn starts a new one, so a slot a second person takes over never joins two people's paths. Per player:
bot (any line of it with b=1: a bot is never a person), lives, distance (world units), seconds_alive, cells (the 20 cells
it spent most time in), stuck (spots with no one cell of progress for --stuck-seconds while it neither fired nor was
hurt: x, y, z, seconds), kills and deaths with positions, first_engagement_s (spawn to first damage dealt or taken, or
shots, median over lives; the shots are the fire lines', so it can be a second late), items (item pick-ups with their
times), shots, hits, accuracy (hits are a weapon's damage only: melee, blasts and turrets are not in them). A death with cause quit
is in the list but not in the grid or the deaths a minute.
Grids: cell size, the origin (x, y of cell 0,0), and presence (seconds), deaths and kills (counts), rows by y, columns by x.
"""
import argparse
import bisect
import json
import math
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

TICKS = 30.0
LINE = re.compile(r"ae obj: t=(-?\d+) host=(\d) mode=(\w+) ev=(\w+)(.*)$")
HEADER = re.compile(r"ae film: (.*)$")
WORD = re.compile(r"(\w+)=(\S*)")
# a gap between a player's pos lines longer than this (ticks) ends its life; a move of more than JUMP world units between
# two samples with no spawn is a person or a spawn we missed, not a walk
GAP_TICKS = 15
JUMP = 30.0


class Film:
    def __init__(self):
        self.meta = {}
        self.events = []
        self.complete = False
        self.start_tick = 0
        self.end_tick = 0

    @property
    def seconds(self):
        return max(0.0, (self.end_tick - self.start_tick) / TICKS)


def _value(text):
    try:
        return float(text) if "." in text else int(text)
    except ValueError:
        return text


def read_film(path):
    film = Film()
    for line in Path(path).read_text(errors="replace").splitlines():
        header = HEADER.search(line)
        if header:
            film.meta.update(dict(WORD.findall(header.group(1))))
            continue
        m = LINE.search(line)
        if not m:
            continue
        event = {"t": int(m.group(1)), "host": int(m.group(2)), "mode": m.group(3), "ev": m.group(4)}
        for key, text in WORD.findall(m.group(5)):
            event[key] = _value(text)
        film.events.append(event)
        if event["ev"] == "game_start":
            film.meta.setdefault("map", event.get("map", ""))
            film.meta.setdefault("engine", event["mode"])
        if event["ev"] == "game_end":
            film.complete = True
    if film.events:
        film.start_tick = film.events[0]["t"]
        film.end_tick = film.events[-1]["t"]
    return film


def _percent(value):
    return None if value is None else round(value, 3)


def _lives(film):
    """{slot: [life]}: a life is {bot, team, start, end, samples: [(t, x, y, z, fighting-free data)]}."""
    lives = defaultdict(list)
    open_life = {}

    def begin(slot, t, event):
        life = {"slot": slot, "bot": event.get("b", 0) == 1, "team": event.get("team", -1), "start": t, "end": t,
                "samples": [], "spawned": event["ev"] == "spawn"}
        lives[slot].append(life)
        open_life[slot] = life
        return life

    for event in film.events:
        ev, slot, t = event["ev"], event.get("p"), event["t"]
        if ev == "spawn":
            begin(slot, t, event)
        elif ev == "death":
            life = open_life.pop(slot, None)
            if life:
                life["end"] = t
        elif ev == "pos":
            life = open_life.get(slot)
            sample = (t, event["x"], event["y"], event["z"], event.get("spd", 0.0))
            if life and life["samples"]:
                last = life["samples"][-1]
                jump = math.dist((last[1], last[2], last[3]), sample[1:4])
                if t - last[0] > GAP_TICKS or jump > JUMP or (event.get("b", 0) == 1) != life["bot"]:
                    life = None
            if life is None:
                life = begin(slot, t, event)
            life["samples"].append(sample)
            life["end"] = t
            if event.get("b", 0) == 1:
                life["bot"] = True
    return lives


def _cell(x, y, size):
    return (math.floor(x / size), math.floor(y / size))


def _fight_ticks(film):
    """{slot: sorted ticks} where the player fired (its fire lines), dealt damage or took some."""
    ticks = defaultdict(list)
    for event in film.events:
        if event["ev"] == "fire" and event.get("shots", 0) > 0:
            ticks[event["p"]].append(event["t"])
        elif event["ev"] == "damage":
            ticks[event["p"]].append(event["t"])
            ticks[event["victim"]].append(event["t"])
    return ticks


def _stuck(life, fight, cell, stuck_ticks):
    """Spots of no one cell of progress (from a dwell's first sample) for stuck_ticks or more with no fight: a dwell is cut at
    its fighting samples (a sample is fighting when the player fired or was hurt in the second from it: the fire lines come
    up to a second late), and each run of the others that long is a spot."""
    spots = []
    fight = sorted(fight)

    def fighting(t):
        at = bisect.bisect_left(fight, t)
        return at < len(fight) and fight[at] <= t + TICKS

    def close(dwell):
        run = []
        for sample in dwell + [None]:
            if sample is not None and not fighting(sample[0]):
                run.append(sample)
                continue
            if run and run[-1][0] - run[0][0] >= stuck_ticks:
                spots.append({"x": round(run[0][1], 1), "y": round(run[0][2], 1), "z": round(run[0][3], 1),
                              "seconds": round((run[-1][0] - run[0][0]) / TICKS, 1)})
            run = []

    dwell = []
    for sample in life["samples"]:
        if dwell and math.hypot(sample[1] - dwell[0][1], sample[2] - dwell[0][2]) >= cell:
            close(dwell)
            dwell = []
        dwell.append(sample)
    if dwell:
        close(dwell)
    return spots


def metrics(film, cell=4.0, stuck_s=5.0):
    lives = _lives(film)
    fight = _fight_ticks(film)
    start = film.start_tick
    players = {}
    for slot, slot_lives in sorted(lives.items(), key=lambda item: (item[0] is None, item[0])):
        info = {"bot": any(life["bot"] for life in slot_lives), "lives": len(slot_lives), "distance": 0.0,
                "seconds_alive": 0.0, "stuck": [], "kills": [], "deaths": [], "items": [], "shots": 0, "hits": 0}
        presence = defaultdict(float)
        engagements = []
        for life in slot_lives:
            samples = life["samples"]
            for a, b in zip(samples, samples[1:]):
                info["distance"] += math.dist(a[1:4], b[1:4])
                presence[_cell(a[1], a[2], cell)] += (b[0] - a[0]) / TICKS
            info["seconds_alive"] += (life["end"] - life["start"]) / TICKS if samples else 0.0
            info["stuck"] += _stuck(life, fight.get(slot, []), cell, stuck_s * TICKS)
            first = [f for f in fight.get(slot, []) if life["start"] <= f <= life["end"]]
            if first and life["spawned"]:
                engagements.append((min(first) - life["start"]) / TICKS)
        info["distance"] = round(info["distance"], 1)
        info["seconds_alive"] = round(info["seconds_alive"], 1)
        top = sorted(presence.items(), key=lambda item: -item[1])[:20]
        info["cells"] = [{"cell": [c[0], c[1]], "x": c[0] * cell, "y": c[1] * cell, "seconds": round(s, 1)} for c, s in top]
        info["first_engagement_s"] = round(statistics.median(engagements), 2) if engagements else None
        players[slot] = info
    # the events keyed by player
    for event in film.events:
        ev = event["ev"]
        if ev == "kill" and event["p"] in players:
            players[event["p"]]["kills"].append({"t": round((event["t"] - start) / TICKS, 1), "x": event["x"], "y": event["y"],
                                                 "z": event["z"], "victim": event["victim"], "weapon": event["weapon"],
                                                 "head": event["head"], "dist": event["dist"]})
        elif ev == "death" and event["p"] in players:
            players[event["p"]]["deaths"].append({"t": round((event["t"] - start) / TICKS, 1), "x": event["x"], "y": event["y"],
                                                  "z": event["z"], "killer": event["killer"], "cause": event["cause"],
                                                  "weapon": event["weapon"]})
        elif ev == "item_take" and event["p"] in players:
            players[event["p"]]["items"].append({"t": round((event["t"] - start) / TICKS, 1), "item": event["item"]})
        elif ev == "fire" and event["p"] in players:
            players[event["p"]]["shots"] += event["shots"]
            players[event["p"]]["hits"] += event["hits"]
        if event.get("b") == 1 and event.get("p") in players:
            players[event["p"]]["bot"] = True
    for info in players.values():
        info["accuracy"] = round(info["hits"] / info["shots"], 3) if info["shots"] else None
    # grids over every position
    xs = [s[1] for slot_lives in lives.values() for life in slot_lives for s in life["samples"]]
    ys = [s[2] for slot_lives in lives.values() for life in slot_lives for s in life["samples"]]
    grids = {"cell": cell, "origin": [0, 0], "presence": [], "deaths": [], "kills": []}
    if xs:
        ox, oy = math.floor(min(xs) / cell), math.floor(min(ys) / cell)
        width, height = math.floor(max(xs) / cell) - ox + 1, math.floor(max(ys) / cell) - oy + 1
        grids["origin"] = [ox * cell, oy * cell]
        for name in ("presence", "deaths", "kills"):
            grids[name] = [[0] * width for _ in range(height)]
        for slot_lives in lives.values():
            for life in slot_lives:
                for a, b in zip(life["samples"], life["samples"][1:]):
                    cx, cy = _cell(a[1], a[2], cell)
                    grids["presence"][cy - oy][cx - ox] = round(grids["presence"][cy - oy][cx - ox] + (b[0] - a[0]) / TICKS, 1)
        for event in film.events:
            # (a quit is not a death of the game; a kill with dist -1 has no place for its killer)
            if event["ev"] in ("death", "kill") and event.get("cause") != "quit" and event.get("dist", 0) != -1:
                cx, cy = _cell(event["x"], event["y"], cell)
                if 0 <= cy - oy < height and 0 <= cx - ox < width:
                    grids["deaths" if event["ev"] == "death" else "kills"][cy - oy][cx - ox] += 1
    return {"film": {"map": film.meta.get("map", ""), "engine": film.meta.get("engine", ""),
                     "gametype": film.meta.get("gametype", ""), "complete": film.complete,
                     "seconds": round(film.seconds, 1)},
            "players": {str(slot): info for slot, info in players.items()}, "grids": grids}


def _films(data):
    return data if isinstance(data, list) else [data]


def key_metrics(data):
    """The numbers compare() watches, over every film of the data: per group (bots, humans) accuracy, kills and deaths a
    minute alive, speed (world units a second alive), stuck spots a 10 minutes and the median first engagement."""
    groups = {"bots": [], "humans": []}
    for film in _films(data):
        for info in film["players"].values():
            groups["bots" if info["bot"] else "humans"].append(info)
    out = {}
    for name, infos in groups.items():
        if not infos:
            continue
        shots, hits = sum(i["shots"] for i in infos), sum(i["hits"] for i in infos)
        alive = sum(i["seconds_alive"] for i in infos)
        if shots:
            out[f"{name}.accuracy"] = hits / shots
        if alive > 0:
            out[f"{name}.kills_per_min"] = sum(len(i["kills"]) for i in infos) * 60.0 / alive
            out[f"{name}.deaths_per_min"] = sum(1 for i in infos for d in i["deaths"] if d["cause"] != "quit") * 60.0 / alive
            out[f"{name}.speed"] = sum(i["distance"] for i in infos) / alive
            out[f"{name}.stuck_per_10min"] = sum(len(i["stuck"]) for i in infos) * 600.0 / alive
        engaged = [i["first_engagement_s"] for i in infos if i["first_engagement_s"] is not None]
        if engaged:
            out[f"{name}.first_engagement_s"] = statistics.median(engaged)
    return out


def compare(a, b, threshold=0.15):
    """{threshold, metrics: {name: {a, b, change, flagged}}, flagged: [names]}: a metric is flagged when it moved by more than
    threshold (a fraction of a's value; of 1 when a is 0)."""
    ka, kb = key_metrics(a), key_metrics(b)
    result = {}
    for name in sorted(set(ka) | set(kb)):
        va, vb = ka.get(name), kb.get(name)
        if va is None or vb is None:
            result[name] = {"a": va, "b": vb, "change": None, "flagged": va is not vb}
            continue
        change = (vb - va) / (abs(va) if abs(va) > 1e-9 else 1.0)
        result[name] = {"a": round(va, 3), "b": round(vb, 3), "change": round(change, 3), "flagged": abs(change) > threshold}
    return {"threshold": threshold, "metrics": result, "flagged": [n for n, m in result.items() if m["flagged"]]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("films", nargs="*")
    parser.add_argument("--cell", type=float, default=4.0)
    parser.add_argument("--stuck-seconds", type=float, default=5.0)
    parser.add_argument("--compare", nargs=2, metavar=("A.json", "B.json"))
    parser.add_argument("--threshold", type=float, default=0.15)
    args = parser.parse_args(argv)
    if args.compare:
        a, b = (json.loads(Path(p).read_text()) for p in args.compare)
        diff = compare(a, b, args.threshold)
        print(json.dumps(diff, indent=1))
        return 1 if diff["flagged"] else 0
    if not args.films:
        parser.error("a film, or --compare A.json B.json")
    results = [metrics(read_film(path), args.cell, args.stuck_seconds) for path in args.films]
    print(json.dumps(results[0] if len(results) == 1 else results))
    return 0


if __name__ == "__main__":
    sys.exit(main())
