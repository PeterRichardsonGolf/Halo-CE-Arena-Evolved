"""No two weapons stand in the same area of a shipped layout.

Every section of port/assets/ae/layouts.txt is applied, as the game does (ae_layout.c: `clear items`, `remove item <n>`,
the item lines), to the map's own netgame items, which tools/ae_layout_map_items.json lists (index, kind, position,
game types: numbers only). The weapons that are left, in game types that overlap, must stand at least
MINIMUM_DISTANCE world units apart. Two of the same weapon side by side are one placement: the layout removes one.
A section with a comp section (`comp` ... `end comp`, AE's comp positions) is checked as the COMP weapon set plays it
(its removes and items applied too) and, when it changes the map outside its comp section, as every other game plays
it; a section of a comp section alone leaves every other game the map's own placement, which is the map's, not AE's."""
import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAYOUTS = ROOT / "port" / "assets" / "ae" / "layouts.txt"
MAP_ITEMS = Path(__file__).resolve().parent / "ae_layout_map_items.json"

MINIMUM_DISTANCE = 4.0
# what a layout's alias or pack weapon is: everything else an `item` line names is a weapon
POWERUPS = {"overshield", "camo"}


def sections(text=None):
    """(map file name, hash, [lines]) of each section of the bundle."""
    text = LAYOUTS.read_text() if text is None else text
    found = []
    for line in text.splitlines():
        words = line.split("#")[0].split()
        if line.startswith("layout ") and len(words) >= 4:
            found.append((words[2].lower(), words[3], []))
        elif found:
            found[-1][2].append(line)
    return found


def map_items():
    return json.loads(MAP_ITEMS.read_text())


def applied_items(lines, own, comp=False):
    """The items a section leaves on a map whose own items are `own` (dicts: index, kind, x, y, z, games); comp: as the
    COMP weapon set plays it (the comp section's lines applied), else as every other game does (left out)."""
    items = {entry["index"]: dict(entry, source="map item %d" % entry["index"]) for entry in own}
    by_index = dict(items)
    added = []
    in_comp = False
    for number, line in enumerate(lines, 2):
        words = line.split("#")[0].split()
        if not words:
            continue
        if words == ["comp"]:
            in_comp = True
            continue
        if words == ["end", "comp"]:
            in_comp = False
            continue
        if in_comp and not comp:
            continue
        if words[:2] == ["clear", "items"]:
            items = {}
        elif words[:2] == ["remove", "item"]:
            items.pop(int(words[2]), None)
        elif words[0] == "item":
            at = words.index("at")
            x, y, z = (float(value) for value in words[at + 1:at + 4])
            games = "all"
            kind = "powerup" if words[1] in POWERUPS else "weapon"
            if "like" in words:
                source = by_index.get(int(words[words.index("like") + 1]))
                if source:
                    games = source["games"]
                    if words[1] == "like":
                        kind = source["kind"]
            if "games" in words:
                games = "all"
            added.append({"kind": kind, "x": x, "y": y, "z": z, "games": games,
                          "source": "line %d (item %s)" % (number, words[1])})
    return list(items.values()) + added


def games_overlap(first, second):
    return first == "all" or second == "all" or bool(set(first) & set(second))


def has_comp(lines):
    return any(line.split("#")[0].split() == ["comp"] for line in lines)


def changes_outside_comp(lines):
    """Whether a section has a line outside its comp section (it changes the map in every game)."""
    in_comp = False
    for line in lines:
        words = line.split("#")[0].split()
        if words == ["comp"]:
            in_comp = True
        elif words == ["end", "comp"]:
            in_comp = False
        elif words and not in_comp:
            return True
    return False


def stacked(lines, own):
    """Pairs of weapons closer than MINIMUM_DISTANCE: (first source, second source, distance), in every game and, for a
    section with a comp section, under the COMP set (each pair once; every other game's only when the section changes
    the map outside its comp section)."""
    found = []
    views = (False,) if not has_comp(lines) else (False, True) if changes_outside_comp(lines) else (True,)
    for comp in views:
        for pair in stacked_view(lines, own, comp):
            if pair not in found:
                found.append(pair)
    return found


def stacked_view(lines, own, comp):
    weapons = [item for item in applied_items(lines, own, comp) if item["kind"] == "weapon"]
    found = []
    for index, first in enumerate(weapons):
        for second in weapons[index + 1:]:
            if not games_overlap(first["games"], second["games"]):
                continue
            distance = math.dist((first["x"], first["y"], first["z"]), (second["x"], second["y"], second["z"]))
            if distance < MINIMUM_DISTANCE:
                found.append((first["source"], second["source"], round(distance, 2)))
    return found
