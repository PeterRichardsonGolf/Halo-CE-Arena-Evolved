#!/usr/bin/env python3
"""Staging spots for TEST RUN lists: where a test may put the player, rated from the level's own data.

  spots.py render <map> [--box X0 Y0 X1 Y1]     top-down picture + JSON of the level's points
  spots.py check <map> <x> <y> <z> <facing>     rate one candidate spot
  spots.py list [<map>]                         the catalog's spots with a fresh rating
  spots.py fill <map>                           rewrite the measured numbers in the catalog
  spots.py find <map> [--box ...] [--min-enemy N] [--room N]   candidate spots, best first
  spots.py expand <list.txt>                    the list with every `setup spot <name>` as its `setup goto` line

Input files (written by scenario_dump.py and the game's `nav dump`) sit in HALO_SPOTS_OUT: <map>.scenario.json
(starts of the players, AI squads, vehicles) and <map>.json (the
level's first collision BSP, with scenery footprints). Pictures and JSON go there too, never into the repo. The
catalog is <map>.toml beside this file. Units are world units; facing is degrees, east 0, north 90 (as `setup goto`).
"""
import argparse
import json
import math
import os
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
NAME = re.compile(r"[a-z0-9-]+")
SETUP_SPOT = re.compile(r"^(?P<lead>\s*setup\s+)spot\s+(?P<name>\S+)\s*$")
CELL = 4.0
# the largest triangle edge kept: a few huge planes (the level's outer floor and ceiling) are not ground to stand on
MAX_EDGE = 150.0
# a spot rates GOOD when the ground ahead is flat (within FLAT of the ground at the spot) this far, with no enemy
# squad start within QUIET and no ally within ALLY, TIGHT when the room is between NEED_MIN and NEED, else POOR
FLAT, RAMP, SIDE, STEP, NEED, NEED_MIN, QUIET, ALLY = 0.6, 0.35, 0.7, 0.5, 9.0, 5.0, 40.0, 12.0
LOOK = 24.0


def out_dir():
    folder = os.environ.get("HALO_SPOTS_OUT")
    if not folder:
        raise SystemExit("HALO_SPOTS_OUT is required")
    return Path(folder)


def load_json(map_name, suffix):
    path = out_dir() / f"{map_name}{suffix}.json"
    if not path.is_file():
        raise SystemExit(f"missing {path}: see the README (scenario_dump.py and `dev nav dump` in a game)")
    return json.loads(path.read_text())


# ---------------------------------------------------------------- ground

class Ground:
    """The walkable collision triangles of a geometry dump and the scenery footprints, with ground-height lookups."""

    def __init__(self, dump):
        self.cells = {}
        self.circles = [(o["p"][0], o["p"][1], o["r"], o.get("zlo", o["p"][2] - 1), o.get("zhi", o["p"][2] + 1))
                        for o in dump.get("obstacles", [])]
        self.circles += [(o["p"][0], o["p"][1], o["r"], o["p"][2] - 1.0, o["p"][2] + 2.5)
                         for o in dump.get("objects", []) if o.get("kind") == "machine"]
        self.tris = []
        for s in dump["surfaces"]:
            if not s["walk"]:
                continue
            v = s["v"]
            for i in range(1, len(v) - 1):
                tri = (v[0], v[i], v[i + 1])
                xs, ys = [p[0] for p in tri], [p[1] for p in tri]
                if max(xs) - min(xs) > MAX_EDGE or max(ys) - min(ys) > MAX_EDGE:
                    continue
                n = len(self.tris)
                self.tris.append(tri)
                for cx in range(int(math.floor(min(xs) / CELL)), int(math.floor(max(xs) / CELL)) + 1):
                    for cy in range(int(math.floor(min(ys) / CELL)), int(math.floor(max(ys) / CELL)) + 1):
                        self.cells.setdefault((cx, cy), []).append(n)
        self.cache = {}

    def heights(self, x, y):
        key = (round(x * 8), round(y * 8))
        found = self.cache.get(key)
        if found is not None:
            return found
        found = []
        for n in self.cells.get((int(math.floor(x / CELL)), int(math.floor(y / CELL))), ()):
            (ax, ay, az), (bx, by, bz), (cx, cy, cz) = self.tris[n]
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if den == 0:
                continue
            u = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / den
            v = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / den
            if u >= -1e-6 and v >= -1e-6 and u + v <= 1 + 1e-6:
                found.append(u * az + v * bz + (1 - u - v) * cz)
        self.cache[key] = found
        return found

    def at(self, x, y, zref, up=1.5, down=3.0):
        """The highest ground height at (x, y) from zref - down to zref + up, None when there is none."""
        best = None
        for h in self.heights(x, y):
            if zref - down <= h <= zref + up and (best is None or h > best):
                best = h
        return best

    def blocked(self, x, y, z):
        """The scenery footprint that stands on the point (a body 0.35 wide, from the ground to 1.5 above it)."""
        for cx, cy, r, zlo, zhi in self.circles:
            if (x - cx) ** 2 + (y - cy) ** 2 < (r + 0.35) ** 2 and zhi > z and zlo < z + 1.5:
                return True
        return False


def room_ahead(ground, x, y, z, facing, limit=LOOK):
    """(ground z at the spot or None, flat room ahead in units): how far the ground stays within FLAT of the spot's
    ground (steps of at most RAMP between samples, both sides SIDE away too) with no scenery footprint on it."""
    g0 = ground.at(x, y, z)
    if g0 is None:
        return None, 0.0
    a = math.radians(facing)
    dx, dy = math.cos(a), math.sin(a)
    prev, room = g0, 0.0
    for i in range(1, int(limit / STEP) + 1):
        d = i * STEP
        cx, cy = x + dx * d, y + dy * d
        hs = [ground.at(cx + sx * -dy * SIDE, cy + sx * dx * SIDE, prev, up=RAMP, down=RAMP) for sx in (0, 1, -1)]
        if any(h is None for h in hs) or abs(hs[0] - g0) > FLAT or any(abs(h - hs[0]) > RAMP for h in hs):
            break
        if ground.blocked(cx, cy, hs[0]):
            break
        prev, room = hs[0], d
    return g0, room


# ---------------------------------------------------------------- scenario

def starts(scn, bsp, ally):
    return [s for s in scn["squads"] if s["ally"] == ally and s["bsp"] == bsp]


def nearest(points, p):
    best = None
    for s in points:
        d = math.dist(s["p"], p)
        if best is None or d < best[0]:
            best = (d, s)
    return best


def rate(ground, scn, x, y, z, facing, bsp=0, geo_bsp=0, quiet=QUIET, need=NEED, ally=ALLY):
    """The measured numbers and the rating of a candidate spot."""
    out = {"bsp": bsp}
    enemy = nearest(starts(scn, bsp, False), (x, y, z))
    friend = nearest(starts(scn, bsp, True), (x, y, z))
    out["enemy_dist"] = round(enemy[0], 1) if enemy else None
    out["enemy_encounter"] = enemy[1]["encounter"] if enemy else None
    out["ally_dist"] = round(friend[0], 1) if friend else None
    if bsp != geo_bsp:
        out.update(ground_dz=None, flat_room=None, rating="NOT ASSESSED (geometry is of bsp %d)" % geo_bsp)
        return out
    g0, room = room_ahead(ground, x, y, z, facing)
    out["ground_dz"] = None if g0 is None else round(z - g0, 2)
    out["flat_room"] = round(room, 1)
    why = []
    if g0 is None:
        why.append("no ground under the spot")
    elif abs(z - g0) > 1.5:
        why.append("spot floats %.1f above the ground" % (z - g0))
    if room < NEED_MIN:
        why.append("only %.1f flat ahead" % room)
    elif room < need:
        why.append("tight: %.1f flat ahead" % room)
    if enemy and enemy[0] < quiet:
        why.append("enemy start %s %.0f away" % (enemy[1]["encounter"], enemy[0]))
    if friend and friend[0] < ally:
        why.append("ally start %.0f away" % friend[0])
    poor = g0 is None or room < NEED_MIN or abs(z - g0) > 1.5 or (enemy and enemy[0] < quiet) or (friend and friend[0] < ally)
    out["rating"] = "POOR" if poor else "TIGHT" if room < need else "GOOD"
    out["why"] = why
    return out


# ---------------------------------------------------------------- catalog

def catalog_path(map_name):
    return HERE / f"{map_name}.toml"


def load_toml(path):
    import tomllib
    return tomllib.loads(Path(path).read_text())


def catalogs():
    """{map: [spot dict]} of every catalog beside this file."""
    found = {}
    for path in sorted(HERE.glob("*.toml")):
        data = load_toml(path)
        found[data["map"]] = data.get("spot", [])
    return found


def spot_names():
    """{spot name: (map, spot)}."""
    return {s["name"]: (m, s) for m, spots in catalogs().items() for s in spots}


def write_catalog(map_name, geo_bsp, quiet, need, ally, header, spots):
    lines = [h if h.startswith("#") else "# " + h for h in header]
    lines += [f'map = "{map_name}"', f"geometry_bsp = {geo_bsp}", f"quiet = {quiet:g}", f"need = {need:g}", f"ally = {ally:g}", ""]
    for s in spots:
        lines.append("[[spot]]")
        for key in ("name", "x", "y", "z", "facing", "bsp", "flat_room", "enemy_dist", "ally_dist", "ground_dz", "rating", "notes"):
            if s.get(key) is None:
                continue
            v = s[key]
            lines.append(f"{key} = {json.dumps(v) if isinstance(v, str) else v}")
        lines.append("")
    catalog_path(map_name).write_text("\n".join(lines))


def catalog_value(map_name, key, default):
    """A number a map's catalog sets for the rating."""
    return load_toml(catalog_path(map_name)).get(key, default) if catalog_path(map_name).is_file() else default


def catalog_quiet(map_name):
    """The quiet distance a map's catalog sets (a map with scripted waves has enemy starts everywhere)."""
    return load_toml(catalog_path(map_name)).get("quiet", QUIET) if catalog_path(map_name).is_file() else QUIET


def rated_catalog(map_name):
    data = load_toml(catalog_path(map_name))
    ground, scn = Ground(load_json(map_name, "")), load_json(map_name, ".scenario")
    out = []
    for s in data.get("spot", []):
        r = rate(ground, scn, s["x"], s["y"], s["z"], s["facing"], s.get("bsp", 0), data.get("geometry_bsp", 0), data.get("quiet", QUIET), data.get("need", NEED), data.get("ally", ALLY))
        out.append((s, r))
    return data, out


# ---------------------------------------------------------------- list expansion

def list_map(text):
    """The first `map` line of a list: the map a `setup spot` before any map line is looked up on."""
    for raw in text.splitlines():
        words = raw.split()
        if len(words) == 2 and words[0] == "map":
            return words[1].split("@")[0]
    return None


def expand_list(text):
    """The list text with each `setup spot <name>` line replaced by `setup goto <x> <y> <z> facing <degrees>`.
    Raises ValueError for a name the catalog does not hold or one made for another map than the group's."""
    names = spot_names()
    current = list_map(text)
    out = []
    for raw in text.split("\n"):
        words = raw.split()
        if len(words) == 2 and words[0] == "map":
            current = words[1].split("@")[0]
        match = SETUP_SPOT.match(raw)
        if not match:
            out.append(raw)
            continue
        name = match.group("name")
        if name not in names:
            raise ValueError(f"setup spot {name}: no spot of that name in tools/ae_test/spots/*.toml")
        map_name, spot = names[name]
        if current and current != map_name:
            raise ValueError(f"setup spot {name}: the spot is on {map_name}, the list's map is {current}")
        out.append("{}goto {:g} {:g} {:g} facing {:g}".format(match.group("lead"), spot["x"], spot["y"], spot["z"], spot["facing"]))
    return "\n".join(out)


# ---------------------------------------------------------------- picture

def render(map_name, box=None, size=14):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    from matplotlib.lines import Line2D

    dump, scn = load_json(map_name, ""), load_json(map_name, ".scenario")
    ground = Ground(dump)
    geo_bsp = load_toml(catalog_path(map_name)).get("geometry_bsp", 0) if catalog_path(map_name).is_file() else 0
    polys = [[(p[0], p[1]) for p in t] for t in ground.tris]
    zs = [sum(p[2] for p in t) / 3 for t in ground.tris]
    xs = [p[0] for t in polys for p in t]
    ys = [p[1] for t in polys for p in t]
    x0, y0, x1, y1 = box or (min(xs) - 5, min(ys) - 5, max(xs) + 5, max(ys) + 5)
    fig, ax = plt.subplots(figsize=(size, size * (y1 - y0) / max(x1 - x0, 1)))
    ax.set_facecolor("#e4e4e4")
    # (the colour range is of the ground in the picture's box: a deep pit would wash the rest out)
    shown = sorted(z for t, z in zip(polys, zs) if x0 <= t[0][0] <= x1 and y0 <= t[0][1] <= y1) or zs
    coll = PolyCollection(polys, array=zs, cmap="viridis", linewidths=0, edgecolors="none")
    coll.set_clim(shown[int(len(shown) * 0.02)], shown[int(len(shown) * 0.98)])
    ax.add_collection(coll)
    fig.colorbar(coll, ax=ax, shrink=0.5, label="ground height")
    points = {"players": [], "enemy_squads": [], "ally_squads": [], "vehicles": [], "spots": []}

    def inside(p):
        return x0 <= p[0] <= x1 and y0 <= p[1] <= y1

    for s in scn["squads"]:
        if s["bsp"] != geo_bsp or not inside(s["p"]):
            continue
        ax.plot(s["p"][0], s["p"][1], "o", color="#2fb344" if s["ally"] else "#d63939", ms=4, mec="black", mew=0.3)
        points["ally_squads" if s["ally"] else "enemy_squads"].append(s)
    for v in scn["vehicles"]:
        # (a vehicle placed 100 or more units above the level's ground waits to be spawned in)
        if inside(v["p"]) and v["p"][2] < max(zs) + 100:
            ax.plot(v["p"][0], v["p"][1], "s", color="#f59f00", ms=6, mec="black", mew=0.4)
            points["vehicles"].append(v)
    for pl in scn["players"]:
        if pl["bsp"] == geo_bsp and inside(pl["p"]):
            a = math.radians(pl["facing"])
            ax.annotate("", xy=(pl["p"][0] + 4 * math.cos(a), pl["p"][1] + 4 * math.sin(a)), xytext=pl["p"][:2],
                        arrowprops={"arrowstyle": "-|>", "color": "#1c7ed6", "lw": 2})
            ax.plot(pl["p"][0], pl["p"][1], "^", color="#1c7ed6", ms=9, mec="black")
            points["players"].append(pl)
    if catalog_path(map_name).is_file():
        _data, rated = rated_catalog(map_name)
        for s, r in rated:
            if s.get("bsp", 0) != geo_bsp or not inside((s["x"], s["y"])):
                continue
            a = math.radians(s["facing"])
            ax.annotate("", xy=(s["x"] + 6 * math.cos(a), s["y"] + 6 * math.sin(a)), xytext=(s["x"], s["y"]),
                        arrowprops={"arrowstyle": "->", "color": "black", "lw": 1.5})
            ax.plot(s["x"], s["y"], "*", color="#ffd43b", ms=15, mec="black")
            ax.annotate("%s %s" % (s["name"], r["rating"]), (s["x"], s["y"]), textcoords="offset points", xytext=(6, 6),
                        fontsize=8, bbox={"fc": "white", "alpha": 0.8, "pad": 1})
            points["spots"].append(dict(s, **r))
    ax.set_xlim(x0, x1)
    ax.set_ylim(y0, y1)
    ax.set_aspect("equal")
    step = 5 if (x1 - x0) < 120 else 10 if (x1 - x0) < 350 else 25
    ax.set_xticks(range(int(math.floor(x0 / step) * step), int(x1) + 1, step))
    ax.set_yticks(range(int(math.floor(y0 / step) * step), int(y1) + 1, step))
    ax.tick_params(labelsize=7)
    ax.grid(True, color="black", alpha=0.15, lw=0.5)
    ax.set_xlabel("x (world units)")
    ax.set_ylabel("y (world units, north up)")
    ax.set_title("%s, collision bsp %d: walkable ground; enemy squad starts red, ally green, players blue, vehicles orange, "
                 "named spots stars" % (map_name, geo_bsp), fontsize=9)
    ax.legend(handles=[Line2D([], [], marker=m, ls="", color=c, mec="black", label=t)
                       for m, c, t in (("^", "#1c7ed6", "player start"), ("o", "#d63939", "enemy squad start"),
                                       ("o", "#2fb344", "ally squad start"), ("s", "#f59f00", "vehicle"),
                                       ("*", "#ffd43b", "named spot"))], loc="upper left", fontsize=7)
    suffix = "_zoom" if box else ""
    png = out_dir() / f"{map_name}_spots{suffix}.png"
    fig.savefig(png, dpi=110, bbox_inches="tight")
    (out_dir() / f"{map_name}_spots{suffix}.points.json").write_text(json.dumps(points, indent=1))
    return png


# ---------------------------------------------------------------- find

def find(map_name, box, quiet, room, limit=12, step=2.0):
    need = room
    dump, scn = load_json(map_name, ""), load_json(map_name, ".scenario")
    ground = Ground(dump)
    xs = [p[0] for t in ground.tris for p in t]
    ys = [p[1] for t in ground.tris for p in t]
    x0, y0, x1, y1 = box or (min(xs), min(ys), max(xs), max(ys))
    found, seen = [], set()
    x = x0
    while x <= x1:
        y = y0
        while y <= y1:
            for z in sorted(set(round(h, 1) for h in ground.heights(x, y) if h > -20)):
                best = None
                for facing in range(0, 360, 45):
                    g0, r = room_ahead(ground, x, y, z, facing, limit=room + 4)
                    if best is None or r > best[0]:
                        best = (r, facing, g0)
                if best[0] >= room:
                    rt = rate(ground, scn, x, y, best[2] + 0.05, best[1], quiet=quiet, need=need)
                    if rt["rating"] == "GOOD":
                        found.append((rt["enemy_dist"] or 999, x, y, round(best[2] + 0.05, 2), best[1], rt))
            y += step
        x += step
    found.sort(key=lambda f: -f[0])
    picked = []
    for f in found:
        if all(math.dist(f[1:3], p[1:3]) > min(20, step * 6) for p in picked):
            picked.append(f)
        if len(picked) >= limit:
            break
    return picked


# ---------------------------------------------------------------- main

def show(name, spot, r):
    print("%-26s %-8s (%g %g %g facing %g) bsp %s room %s enemy %s (%s) ally %s dz %s%s" % (
        name, r["rating"], spot["x"], spot["y"], spot["z"], spot["facing"], r["bsp"], r.get("flat_room"),
        r["enemy_dist"], r["enemy_encounter"], r["ally_dist"], r.get("ground_dz"),
        ("  -- " + "; ".join(r["why"])) if r.get("why") else ""))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("render")
    p.add_argument("map")
    p.add_argument("--box", nargs=4, type=float)
    p = sub.add_parser("check")
    p.add_argument("map")
    p.add_argument("x", type=float)
    p.add_argument("y", type=float)
    p.add_argument("z", type=float)
    p.add_argument("facing", type=float)
    p.add_argument("--bsp", type=int, default=0)
    p.add_argument("--quiet", type=float, default=None)
    for cmd in ("list", "fill"):
        p = sub.add_parser(cmd)
        p.add_argument("map", nargs="?" if cmd == "list" else None)
    p = sub.add_parser("find")
    p.add_argument("map")
    p.add_argument("--box", nargs=4, type=float)
    p.add_argument("--quiet", type=float, default=None, help="enemy start distance that counts as quiet (default: the catalog's, else %g)" % QUIET)
    p.add_argument("--room", type=float, default=None, help="flat room ahead that counts as enough (default: the catalog's, else %g)" % NEED)
    p = sub.add_parser("expand")
    p.add_argument("list")
    a = ap.parse_args(argv)

    if a.cmd == "render":
        print("wrote", render(a.map, a.box))
    elif a.cmd == "check":
        r = rate(Ground(load_json(a.map, "")), load_json(a.map, ".scenario"), a.x, a.y, a.z, a.facing, a.bsp, quiet=a.quiet or catalog_quiet(a.map), need=catalog_value(a.map, "need", NEED))
        show("candidate", {"x": a.x, "y": a.y, "z": a.z, "facing": a.facing}, r)
        return 0 if r["rating"] == "GOOD" else 1
    elif a.cmd == "list":
        for m in ([a.map] if a.map else sorted(catalogs())):
            for s, r in rated_catalog(m)[1]:
                show(s["name"], s, r)
    elif a.cmd == "fill":
        data, rated = rated_catalog(a.map)
        spots = []
        for s, r in rated:
            s = dict(s)
            for key in ("flat_room", "enemy_dist", "ally_dist", "ground_dz"):
                s[key] = r.get(key)
            s["rating"] = r["rating"]
            spots.append(s)
        text = catalog_path(a.map).read_text()
        header = [line for line in text.splitlines() if line.startswith("#")]
        write_catalog(a.map, data.get("geometry_bsp", 0), data.get("quiet", QUIET), data.get("need", NEED), data.get("ally", ALLY), header, spots)
        print("rewrote", catalog_path(a.map))
    elif a.cmd == "find":
        for _d, x, y, z, facing, r in find(a.map, a.box, a.quiet or catalog_quiet(a.map), a.room or catalog_value(a.map, "need", NEED)):
            print("%g %g %g facing %g  room %s enemy %s (%s) ally %s" % (x, y, z, facing, r["flat_room"], r["enemy_dist"],
                                                                      r["enemy_encounter"], r["ally_dist"]))
    elif a.cmd == "expand":
        try:
            print(expand_list(Path(a.list).read_text()), end="")
        except ValueError as e:
            raise SystemExit(str(e))
    return 0


if __name__ == "__main__":
    sys.exit(main())
