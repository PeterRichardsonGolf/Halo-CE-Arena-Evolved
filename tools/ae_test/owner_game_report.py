#!/usr/bin/env python3
"""owner_game_report: what happened in a person's own games, from the debug.txt the game wrote while they played.

    python3 tools/ae_test/owner_game_report.py debug.txt [--list] [--game N ...] [--all] [--cell 4] [--stuck-seconds 5]

One debug.txt holds every game of a session; a game starts at its "ae obj: ... ev=game_start" line (ae_objective_log.c)
and runs to the next one. Per game, as Markdown:
- the game: map, mode, the TEST RUN that set it up, length, score, the people and the bots' levels;
- where each person died (by --cell world-unit cell), who killed them and with what, from how far;
- where bots died, and how (a fall, the person, another bot);
- where bots stood still for --stuck-seconds or more while not fighting (from their once-a-second "bot event: ... ev=pos"
  lines), with the job they had, and the cells their stuck counter fired in ("ae obj: ... ev=stuck");
- how often bots' shots at a person hit, by range band, with the count and a 90 % interval;
- each player's own weapon hits ("ae obj: ... ev=fire": weapon damage only, no melee, blasts or turrets), people and bots
  alike, so both sides are counted the same way (these lines carry no range);
- strafe changes per second of fighting, by level (a description of the game, not a verdict);
- drivers' wedges and crashes by spot, and how often they steered round scenery.

Counting rules (30 ticks a second):
- A bot's "shot" is one trigger pull (a burst is one). It HIT when a hit on the same target lands within HIT_WINDOW
  ticks and before its next shot; hits are deduped by tick and target first (pellets are one), and a hit just after a
  blow (melee) with no shot between is the blow's. So a band's rate is the share of trigger pulls that hit, never above
  100 %.
- A band with fewer than 30 shots is shown, but marked "(few)": the interval says how little it tells.
- Bot events before WARMUP ticks (the countdown) are dropped.
By default only games with a person in them and at least 30 s long are shown; --all shows every game.
"""
import argparse
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

TICKS = 30.0
WARMUP = 120
HIT_WINDOW = 45         # ticks: a shot's projectile has landed by then (rockets and plasma at range included)
MELEE_WINDOW = 8        # ticks: a hit this soon after a blow, with no shot between, is the blow's
STUCK_RADIUS = 1.0      # world units
FEW = 30                # shots: below this a band's rate is noise
BANDS = [(0.0, 5.0), (5.0, 10.0), (10.0, 15.0), (15.0, 25.0), (25.0, 1.0e9)]
LOAD_LINES = 200        # a scenery line this many lines or fewer before a game's start line is that game's
SKILL_ORDER = ["recruit", "easy", "normal", "heroic", "elite", "legendary", "demon"]
Z90 = 1.6449

OBJ = re.compile(r"ae obj: t=(-?\d+) host=\d+ mode=(\w+) ev=(\w+)(.*)$")
BOT = re.compile(r"bot event: t=(-?\d+) bot=(\d+) skill=(\w+) ev=(\w+) target=(-?\d+) dist=(-?[\d.]+)(.*)$")
DRIVE = re.compile(r"bots: ev=(drive_\w+)(.*)$")
AT = re.compile(r" at \((-?[\d.]+) (-?[\d.]+) (-?[\d.]+)\)")
SETUP = re.compile(r"test run setup: (\S+) \(")
CLOCK = re.compile(r"^(\d\d\.\d\d\.\d\d \d\d:\d\d:\d\d)\s")
WORD = re.compile(r"(\w+)=(\S*)")


class Game:
    def __init__(self, number, start, setup, clock):
        self.number = number
        self.start = start      # the game_start line's words
        self.setup = setup      # the TEST RUN that set it up, or None
        self.clock = clock      # the last wall-clock time stamp before it, or None
        self.obj = []           # (ev, fields) of the "ae obj:" lines
        self.bot = []           # the "bot event:" lines as dicts
        self.drive = []         # (ev, bot, t, fields, at) of the drivers' lines
        self.end = None

    @property
    def map(self):
        return self.start.get("map", "?")


def _float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def parse(path):
    """The games of one debug.txt, in order."""
    games = []
    setup = clock = None
    game = None
    loaded = None   # (line number, the game it went to, the item) of the last scenery line
    with open(path, errors="replace") as f:
        for number, line in enumerate(f):
            m = CLOCK.match(line)
            if m:
                clock = m.group(1)
            m = SETUP.search(line)
            if m:
                setup = m.group(1)
                continue
            m = OBJ.search(line)
            if m:
                fields = dict(WORD.findall(m.group(4)))
                fields["t"] = int(m.group(1))
                fields["mode"] = m.group(2)
                ev = m.group(3)
                if ev == "game_start":
                    game = Game(len(games) + 1, fields, setup, clock)
                    # the scenery line is written when the bots get the map: just before its start line, or after it
                    # when bots join a running game; one just before belongs to this game, not the last
                    if loaded and number - loaded[0] <= LOAD_LINES:
                        if loaded[1] is not None and loaded[2] in loaded[1].drive:
                            loaded[1].drive.remove(loaded[2])
                        game.drive.insert(0, loaded[2])
                    loaded = None
                    games.append(game)
                    setup = None
                    continue
                if game is None:
                    continue
                if ev == "game_end":
                    game.end = fields
                game.obj.append((ev, fields))
                continue
            m = DRIVE.search(line)
            if m:
                fields = dict(WORD.findall(m.group(2)))
                at = AT.search(m.group(2))
                spot = tuple(float(v) for v in at.groups()) if at else None
                item = (m.group(1), int(_float(fields.get("bot"), -1)), int(_float(fields.get("t"), -1)), fields, spot)
                if m.group(1) == "drive_obstacles":
                    loaded = (number, game, item)
                if game is not None:
                    game.drive.append(item)
                continue
            if game is None:
                continue
            m = BOT.search(line)
            if m:
                event = dict(WORD.findall(m.group(7)))
                event.update({"t": int(m.group(1)), "bot": int(m.group(2)), "skill": m.group(3), "ev": m.group(4),
                              "target": int(m.group(5)), "dist": float(m.group(6))})
                game.bot.append(event)
                continue

    for game in games:
        game.bot.sort(key=lambda e: e["t"])
    return games


def wilson(hits, n, z=Z90):
    """The 90 % interval of a share, as (low, high) fractions; (None, None) with no tries."""
    if n <= 0:
        return None, None
    p = hits / n
    centre = (p + z * z / (2 * n)) / (1 + z * z / n)
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / (1 + z * z / n)
    return max(0.0, centre - half), min(1.0, centre + half)


def band_of(dist):
    for low, high in BANDS:
        if low <= dist < high:
            return (low, high)
    return BANDS[-1]


def band_name(band):
    low, high = band
    return f"{low:g}+ u" if high > 1.0e8 else f"{low:g}-{high:g} u"


def cell_of(x, y, size):
    return (math.floor(x / size), math.floor(y / size))


def cell_name(cell, size):
    return f"({(cell[0] + 0.5) * size:.0f}, {(cell[1] + 0.5) * size:.0f})"


def skill_key(skill):
    return SKILL_ORDER.index(skill) if skill in SKILL_ORDER else 99


def people(game):
    """The slots of people (b=0) in the game."""
    return sorted({int(f["p"]) for ev, f in game.obj if f.get("b") == "0" and "p" in f})


def bot_skills(game):
    """Each bot's level: the one most of its lines carry (a TEST RUN can set the level after the game starts)."""
    counts = defaultdict(Counter)
    for e in game.bot:
        counts[e["bot"]][e["skill"]] += 1
    return {bot: c.most_common(1)[0][0] for bot, c in counts.items()}


def length_s(game):
    last = max([f["t"] for _, f in game.obj] + [e["t"] for e in game.bot] + [0])
    return last / TICKS


def shots_that_hit(events, targets):
    """Per bot, its shots at the given targets as (dist, hit, skill, from a vehicle's gun) after WARMUP; see the module's
    counting rules."""
    by_bot = defaultdict(list)
    for e in events:
        if e["t"] >= WARMUP and e["ev"] in ("shot", "hit", "melee"):
            by_bot[e["bot"]].append(e)
    out = {}
    for bot, evs in by_bot.items():
        shots = [e for e in evs if e["ev"] == "shot"]
        melee = [e["t"] for e in evs if e["ev"] == "melee"]
        hits, seen = [], set()
        for e in evs:
            if e["ev"] != "hit" or (e["t"], e["target"]) in seen:
                continue
            seen.add((e["t"], e["target"]))
            blow = any(0 <= e["t"] - m <= MELEE_WINDOW for m in melee)
            shot_near = any(0 <= e["t"] - s["t"] <= MELEE_WINDOW for s in shots)
            if blow and not shot_near:
                continue
            hits.append(e)
        result = []
        claimed = set()
        for i, s in enumerate(shots):
            if s["target"] not in targets:
                continue
            until = s["t"] + HIT_WINDOW
            if i + 1 < len(shots):
                until = min(until, shots[i + 1]["t"])
            hit = False
            for j, h in enumerate(hits):
                if j not in claimed and h["target"] == s["target"] and s["t"] <= h["t"] <= until:
                    claimed.add(j)
                    hit = True
                    break
            result.append((s["dist"], hit, s["skill"], s.get("weapon", "").startswith("vehicles\\")))
        out[bot] = result
    return out


def stuck_stretches(pos, stuck_seconds):
    """Stretches a bot stayed within STUCK_RADIUS while not fighting, from its pos lines: dicts x, y, z, t, seconds, job."""
    spans = []
    anchor = None

    def close():
        if anchor and (anchor["last"] - anchor["t"]) / TICKS >= stuck_seconds:
            spans.append(dict(anchor, seconds=(anchor["last"] - anchor["t"]) / TICKS))

    for p in pos:
        x, y, z = _float(p.get("x")), _float(p.get("y")), _float(p.get("z"))
        fighting = p.get("fighting") == "1"
        if anchor and not fighting and p["t"] - anchor["last"] <= 2 * TICKS and \
                math.dist((x, y, z), (anchor["x"], anchor["y"], anchor["z"])) <= STUCK_RADIUS:
            anchor["last"] = p["t"]
            continue
        close()
        anchor = None if fighting else {"x": x, "y": y, "z": z, "t": p["t"], "last": p["t"], "job": p.get("job", "?"),
                                        "goal": p.get("route", "?")}
    close()
    return spans


def table(header, rows):
    out = ["| " + " | ".join(header) + " |", "|" + "|".join(["---"] * len(header)) + "|"]
    out += ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]
    return out


def pct(value):
    return "-" if value is None else f"{value * 100:.0f} %"


def share_cells(hits, n):
    low, high = wilson(hits, n)
    rate = hits / n if n else None
    note = " (few)" if 0 < n < FEW else ""
    return [str(n), str(hits), pct(rate) + note, "-" if low is None else f"{low * 100:.0f}-{high * 100:.0f} %"]


def top(counter, k=3):
    return ", ".join(f"{name} {n}" for name, n in counter.most_common(k)) or "-"


def report_game(game, cell, stuck_seconds):
    humans = people(game)
    skills = bot_skills(game)
    out = []
    title = f"## Game {game.number}: {game.map}, {game.start.get('mode', '?')}"
    out += [title, ""]
    level_count = Counter(skills.values())
    levels = ", ".join(f"{n} {s}" for s, n in sorted(level_count.items(), key=lambda kv: skill_key(kv[0]))) or "none"
    score = "-"
    if game.end:
        score = f"red {game.end.get('score_r', '?')}, blue {game.end.get('score_b', '?')} (winner {game.end.get('winner', '?')})"
    else:
        snaps = [f for ev, f in game.obj if ev == "snap"]
        if snaps:
            score = f"red {snaps[-1].get('score_r', '?')}, blue {snaps[-1].get('score_b', '?')} (no game end: quit or restart)"
    out += [f"- Started {game.clock or '?'}; TEST RUN {game.setup or 'none'}; {length_s(game):.0f} s; score {score}.",
            f"- People: {', '.join('p' + str(h) for h in humans) or 'none'}. Bots: {levels}.", ""]

    # where people died
    deaths = [f for ev, f in game.obj if ev == "death"]
    kills = {(f["t"], f.get("victim")): f for ev, f in game.obj if ev == "kill"}
    out += ["### Where you died", ""]
    rows = defaultdict(lambda: {"n": 0, "killers": Counter(), "weapons": Counter(), "dist": [], "z": []})
    for f in deaths:
        if f.get("b") != "0":
            continue
        c = cell_of(_float(f.get("x")), _float(f.get("y")), cell)
        r = rows[c]
        r["n"] += 1
        r["z"].append(_float(f.get("z")))
        killer = f.get("killer", "-1")
        if killer in ("-1", f.get("p")):
            r["killers"][f.get("cause", "self")] += 1
        else:
            k = int(killer)
            r["killers"][f"bot {k} {skills[k]}" if k in skills else f"p{k}"] += 1
        r["weapons"][f.get("weapon", "?")] += 1
        kill = kills.get((f["t"], f.get("p")))
        if kill and _float(kill.get("dist"), -1) >= 0:
            r["dist"].append(_float(kill.get("dist")))
    if rows:
        ordered = sorted(rows.items(), key=lambda kv: -kv[1]["n"])
        out += table(["cell (x, y)", "z", "deaths", "by", "weapon", "range med"],
                     [[cell_name(c, cell), f"{sum(r['z']) / len(r['z']):.1f}", r["n"], top(r["killers"]), top(r["weapons"]),
                       f"{sorted(r['dist'])[len(r['dist']) // 2]:.1f}" if r["dist"] else "-"] for c, r in ordered])
    else:
        out.append("No deaths.")
    out.append("")

    # where bots died
    out += ["### Where bots died (top 8 cells)", ""]
    rows = defaultdict(lambda: {"n": 0, "how": Counter()})
    for f in deaths:
        if f.get("b") != "1":
            continue
        c = cell_of(_float(f.get("x")), _float(f.get("y")), cell)
        killer = f.get("killer", "-1")
        if killer in ("-1", f.get("p")):
            how = f.get("cause", "self")
        elif int(killer) in humans:
            how = "a person"
        else:
            how = "a bot"
        rows[c]["n"] += 1
        rows[c]["how"][how] += 1
    if rows:
        ordered = sorted(rows.items(), key=lambda kv: -kv[1]["n"])[:8]
        out += table(["cell (x, y)", "deaths", "how"], [[cell_name(c, cell), r["n"], top(r["how"], 4)] for c, r in ordered])
    else:
        out.append("No bot deaths.")
    out.append("")

    # stuck
    out += [f"### Bots standing still {stuck_seconds:g} s or more, not fighting", ""]
    pos = defaultdict(list)
    for e in game.bot:
        if e["ev"] == "pos" and e["t"] >= WARMUP:
            pos[e["bot"]].append(e)
    rows = defaultdict(lambda: {"bots": set(), "seconds": 0.0, "longest": 0.0, "jobs": Counter(), "z": []})
    for bot, p in pos.items():
        for s in stuck_stretches(p, stuck_seconds):
            r = rows[cell_of(s["x"], s["y"], cell)]
            r["bots"].add(bot)
            r["seconds"] += s["seconds"]
            r["longest"] = max(r["longest"], s["seconds"])
            r["jobs"][s["job"]] += 1
            r["z"].append(s["z"])
    counter = defaultdict(lambda: {"n": 0, "goals": Counter()})
    for ev, f in game.obj:
        if ev == "stuck" and f.get("b") == "1":
            c = cell_of(_float(f.get("x")), _float(f.get("y")), cell)
            counter[c]["n"] += 1
            counter[c]["goals"][f"{f.get('job', '?')}/{f.get('goal_name', '?')}"] += 1
    if rows:
        ordered = sorted(rows.items(), key=lambda kv: -kv[1]["seconds"])[:10]
        out += table(["cell (x, y)", "z", "bots", "seconds in all", "longest s", "job", "stuck counter here (job/goal)"],
                     [[cell_name(c, cell), f"{sum(r['z']) / len(r['z']):.1f}", len(r["bots"]), f"{r['seconds']:.0f}",
                       f"{r['longest']:.0f}", top(r["jobs"]),
                       f"{counter[c]['n']}: {top(counter[c]['goals'], 2)}" if c in counter else "0"] for c, r in ordered])
    else:
        out.append("None.")
    loud = sorted(((c, v) for c, v in counter.items() if c not in rows), key=lambda kv: -kv[1]["n"])[:5]
    if loud:
        out += ["", "Stuck counter cells with no 5 s stand: " +
                "; ".join(f"{cell_name(c, cell)} {v['n']} ({top(v['goals'], 2)})" for c, v in loud)]
    out.append("")

    # bots' shots at people by range
    out += ["### Bots' shots at you, by range", ""]
    if humans:
        per_bot = shots_that_hit(game.bot, set(humans))
        per_skill = defaultdict(lambda: defaultdict(lambda: [0, 0]))
        turret = [0, 0]
        for bot, shots in per_bot.items():
            for dist, hit, skill, vehicle in shots:
                cellv = turret if vehicle else per_skill[skill][band_of(dist)]
                cellv[0] += 1
                cellv[1] += 1 if hit else 0
        rows = []
        for skill in sorted(per_skill, key=skill_key):
            for band in BANDS:
                n, h = per_skill[skill].get(band, [0, 0])
                if n:
                    rows.append([skill, band_name(band)] + share_cells(h, n))
        out += table(["level", "range", "shots", "hit", "rate", "90 % interval"], rows) if rows else ["No bot shots at people."]
        lead = []
        for bot, shots in per_bot.items():
            foot = [(d, h) for d, h, _, vehicle in shots if not vehicle]
            if foot:
                ds = sorted(d for d, _ in foot)
                lead.append((len(foot), sum(1 for _, h in foot if h), ds[len(ds) // 2], bot))
        lead.sort(reverse=True)
        if lead:
            out += ["", "Bots that fired most at you (many shots with few hits: a wall or rock between, or out of range):", ""]
            out += table(["bot", "level", "shots", "hit", "rate", "range med"],
                         [[f"bot {b}", skills.get(b, "?"), n, h, pct(h / n), f"{d:.1f}"] for n, h, d, b in lead[:4]])
        if turret[0]:
            out += ["", f"Not in the table: {turret[0]} shots from vehicle guns (their hits are the vehicle's, not logged as "
                    f"the gunner's: {turret[1]} matched)."]
    else:
        out.append("No person in this game.")
    out.append("")

    # weapon hits, both sides
    out += ["### Weapon hits, people and bots counted the same way (no range in these lines)", ""]
    fire = defaultdict(lambda: [0, 0])
    for ev, f in game.obj:
        if ev != "fire":
            continue
        side = "you" if f.get("b") == "0" else "bots"
        weapon = f.get("weapon", "?")
        fire[(side, weapon)][0] += int(_float(f.get("shots")))
        fire[(side, weapon)][1] += int(_float(f.get("hits")))
    if fire:
        rows = []
        for side in ("you", "bots"):
            items = sorted(((w, v) for (s, w), v in fire.items() if s == side), key=lambda kv: -kv[1][0])
            n = sum(v[0] for _, v in items)
            h = sum(v[1] for _, v in items)
            if n:
                rows.append([side, "all"] + share_cells(min(h, n), n))
            rows += [[side, w] + share_cells(min(v[1], v[0]), v[0]) for w, v in items[:4] if v[0]]
        out += table(["who", "weapon", "shots", "hit", "rate", "90 % interval"], rows)
    else:
        out.append("No fire lines.")
    out.append("")

    # strafes
    out += ["### Strafe changes per second of fighting, by level (a description, not a verdict)", ""]
    strafes, fight_s = Counter(), Counter()
    for e in game.bot:
        if e["t"] < WARMUP:
            continue
        if e["ev"] == "strafe":
            strafes[e["skill"]] += 1
        elif e["ev"] == "pos" and e.get("fighting") == "1":
            fight_s[e["skill"]] += 1
    rows = [[s, strafes[s], fight_s[s], f"{strafes[s] / fight_s[s]:.2f}" if fight_s[s] else "-"]
            for s in sorted(set(strafes) | set(fight_s), key=skill_key)]
    out += table(["level", "strafe changes", "fighting s", "per s"], rows) if rows else ["No fighting."]
    out.append("")

    # drivers
    out += ["### Drivers: wedges and crashes by spot", ""]
    rows = defaultdict(lambda: {"n": Counter(), "class": Counter()})
    dodges = 0
    obstacles = None
    for ev, bot, t, f, spot in game.drive:
        if ev == "drive_dodge":
            dodges += 1
        elif ev == "drive_obstacles":
            obstacles = f.get("count", "?")
        elif ev in ("drive_wedge", "drive_crash", "drive_wedge_learnt") and spot:
            r = rows[cell_of(spot[0], spot[1], cell)]
            r["n"][ev.replace("drive_", "")] += 1
            r["class"][f.get("class", "?")] += 1
    out.append(f"Scenery the drivers know: {obstacles if obstacles is not None else 'no line'}; steered round it {dodges} time(s).")
    if rows:
        ordered = sorted(rows.items(), key=lambda kv: -sum(kv[1]["n"].values()))
        out += [""] + table(["cell (x, y)", "what", "vehicle"],
                            [[cell_name(c, cell), top(r["n"]), top(r["class"])] for c, r in ordered])
    else:
        out.append("No wedges or crashes.")
    out.append("")
    return out


def game_list(games):
    rows = [[g.number, g.clock or "?", g.map, g.start.get("mode", "?"), g.setup or "-", f"{length_s(g):.0f}",
             len(people(g)), len(bot_skills(g))] for g in games]
    return "\n".join(table(["game", "started", "map", "mode", "TEST RUN", "seconds", "people", "bots"], rows)) + "\n"


def report(path, numbers=None, show_all=False, cell=4.0, stuck_seconds=5.0):
    games = parse(path)
    if numbers:
        chosen = [g for g in games if g.number in numbers]
    elif show_all:
        chosen = games
    else:
        chosen = [g for g in games if people(g) and length_s(g) >= 30]
    out = [f"# Your games: {Path(path).name}", "",
           f"{len(games)} game(s) in the file, {len(chosen)} shown. Cells are {cell:g} world units. "
           f"Rates come with their count and a 90 % interval; under {FEW} shots a rate is marked (few).", ""]
    for g in chosen:
        out += report_game(g, cell, stuck_seconds)
    return "\n".join(out)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("file", help="the debug.txt the game wrote")
    p.add_argument("--list", action="store_true", help="list the games in the file and stop")
    p.add_argument("--game", type=int, action="append", help="a game's number from --list (more than once for more)")
    p.add_argument("--all", action="store_true", help="every game, also those without a person or under 30 s")
    p.add_argument("--cell", type=float, default=4.0)
    p.add_argument("--stuck-seconds", type=float, default=5.0)
    a = p.parse_args(argv)
    if a.list:
        sys.stdout.write(game_list(parse(a.file)))
        return
    sys.stdout.write(report(a.file, a.game, a.all, a.cell, a.stuck_seconds))


if __name__ == "__main__":
    main()
