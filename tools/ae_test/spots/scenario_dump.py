#!/usr/bin/env python3
"""Writes the placements of a level's scenario as JSON: player starts, every AI encounter squad start (with its team,
the actor variant and whether the encounter exists at level start), placed vehicles and the BSP count.

Run it with a Python environment for a compatible tag extractor such as Invader. The scenario tag is read through a
compatible Python reader:

  python3 tools/ae_test/spots/scenario_dump.py <map file> <out.json> [--reader-dir <tag reader folder>]

The reader folder is given with --reader-dir, or xreader.py is found on PATH. Positions are world units; facing is
degrees (east 0, north 90), as `setup goto` reads them."""
import argparse
import json
import math
import re
import shutil
import sys
from pathlib import Path

ALLY_WORDS = ("marine", "johnson", "captain", "keyes", "pilot", "crewman", "engineer", "cortana", "sergeant")
FRIENDLY_TEAMS = {"player", "human"}


def reader_folder(folder):
    if folder:
        return Path(folder)
    found = shutil.which("xreader.py")
    if found:
        return Path(found).parent
    raise SystemExit("a compatible tag reader is required: use --reader-dir or put xreader.py on PATH")


def reader(folder=None):
    sys.path.insert(0, str(reader_folder(folder)))
    import xreader  # noqa: E402
    xreader.MAX_NODES = 5_000_000
    return xreader


def blocks(flat, name):
    """The element indexes of a block (name.count)."""
    return range(int(flat.get(name + ".count", 0)))


def point(flat, prefix):
    return [round(float(flat[prefix + "." + a]), 3) for a in ("x", "y", "z")]


def degrees(radians):
    return round(math.degrees(float(radians)), 1)


def is_ally(team, variant):
    return team in FRIENDLY_TEAMS or any(word in variant.lower() for word in ALLY_WORDS)


def dump(map_path, reader_dir=None):
    x = reader(reader_dir)
    m = x.load(map_path)
    scen = m.tag_index.scenario_tag_id & 0xFFFF
    row = x.tag_rows(m)[scen]
    flat, _refs, truncated = x.flatten(x.meta_of(m, row))
    if truncated:
        raise SystemExit("scenario too large for the reader")
    palette = [re.sub(r"\.actor_variant$", "", flat.get("actors_palette[%d].name" % i, "")) for i in blocks(flat, "actors_palette")]
    vehicle_palette = [re.sub(r"\.vehicle$", "", flat.get("vehicles_palette[%d].name" % i, "")) for i in blocks(flat, "vehicles_palette")]
    out = {"map": str(m.map_header.map_name), "scenario": row[1].replace("\\", "/"), "players": [], "squads": [], "vehicles": []}
    for i in blocks(flat, "player_starting_locations"):
        p = "player_starting_locations[%d]" % i
        out["players"].append({"p": point(flat, p + ".position"), "facing": degrees(flat[p + ".facing"]),
                               "team": flat[p + ".team_index"], "bsp": flat[p + ".bsp_index"]})
    for e in blocks(flat, "encounters"):
        ep = "encounters[%d]" % e
        team = str(flat[ep + ".team_index"])
        created = "not_initially_created" not in flat[ep + ".flags"]
        for s in blocks(flat, ep + ".squads"):
            sp = "%s.squads[%d]" % (ep, s)
            actor = int(flat[sp + ".actor_type"])
            for k in blocks(flat, sp + ".starting_locations"):
                lp = "%s.starting_locations[%d]" % (sp, k)
                kind = int(flat[lp + ".actor_type"])
                if kind < 0:
                    kind = actor
                variant = palette[kind] if 0 <= kind < len(palette) else "?"
                out["squads"].append({"encounter": flat[ep + ".name"], "squad": flat[sp + ".name"], "team": team,
                                      "actor": variant, "ally": is_ally(team, variant), "at_start": created,
                                      "bsp": flat[ep + ".bsp_index"], "p": point(flat, lp + ".position"),
                                      "facing": degrees(flat[lp + ".facing"])})
    for i in blocks(flat, "vehicles"):
        vp = "vehicles[%d]" % i
        t = int(flat[vp + ".type"])
        out["vehicles"].append({"type": vehicle_palette[t] if 0 <= t < len(vehicle_palette) else "?",
                                "p": point(flat, vp + ".position"), "facing": degrees(flat[vp + ".rotation.y"]),
                                "not_placed": flat[vp + ".not_placed"]})
    out["bsp_count"] = int(flat.get("structure_bsps.count", 0)) or None
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map")
    parser.add_argument("out", type=Path)
    parser.add_argument("--reader-dir", type=Path, help="folder containing a compatible xreader.py tag reader")
    args = parser.parse_args(argv)
    result = dump(args.map, args.reader_dir)
    args.out.write_text(json.dumps(result, indent=1))
    print("%s: %d players, %d squad starts (%d enemy at start), %d vehicles" % (
        result["map"], len(result["players"]), len(result["squads"]),
        sum(1 for s in result["squads"] if s["at_start"] and not s["ally"]), len(result["vehicles"])))


if __name__ == "__main__":
    main()
