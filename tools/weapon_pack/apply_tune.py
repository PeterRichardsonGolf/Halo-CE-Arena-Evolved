"""A DEV MODE tune file becomes a recipe's "tune" layer: the numbers set in the game (DEV MODE's tune save, written to
<config folder>/tune/<weapon key>.json: port/linux/game/ae_weapon_tune.h) are put into the weapon's recipe as `set` edits, so
the recipe stays the one source of truth and the next build of the pack has them.

    apply_tune.py --recipe tools/weapon_pack/<item>/recipe.json --tune <file.json>

What it writes in the recipe:
  - a layer "tune", the FIRST in the recipe's "layers" (Invader takes a tag from the first folder that has it, so the
    tune layer's tags win over every other layer's);
  - for each tag the tune file edits, one step of that layer: {"layer": "tune", "tag": <the tag as the recipe names it>,
    "op": "edit", "from": [<the layer whose tag wins now, else "source">, <the tag>], "edits": [["set", <field path>,
    <value>], ...]}. A bounds value ([a, b]) is written "a b", as Invader's editor takes it;
  - a field the tune step already sets is replaced, never added twice (a key saved twice keeps its last value);
  - the recorded hashes of the donor and the pack ("expected") are emptied: the tuned donor is another file, and the
    build refuses a donor that is not the recorded one. The next build logs the new ones, to be recorded.

It refuses (nothing written) a tune file of a weapon with no recipe here ("no recipe for <weapon key>": a stock
weapon's), and an edit of a tag the recipe does not make (a stock tag the pack leaves to the map).
"""
import argparse
import json
import os
import sys
import tempfile
from pathlib import Path

LAYER = "tune"
PINS = ("donor_sha256", "pack_sha256", "pack_own_sha256")


class Refused(Exception):
    """the tune file is not applied, with the reason"""


def dump(data):
    """the recipe's own format (one space of indent, a final newline)"""
    return json.dumps(data, indent=1, ensure_ascii=False) + "\n"


def number_text(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise Refused("a value that is no number: %r" % (value,))
    if float(value) == int(value) and abs(value) < 1e15:
        return str(int(value))
    return format(value, ".9g")


def value_text(value):
    if isinstance(value, list):
        if len(value) != 2:
            raise Refused("a bounds value has two numbers: %r" % (value,))
        return " ".join(number_text(item) for item in value)
    return number_text(value)


def read_tune(path):
    try:
        tune = json.loads(Path(path).read_text())
    except (OSError, ValueError) as failure:
        raise Refused("%s: %s" % (path, failure))
    if not isinstance(tune, dict) or tune.get("format") != 1:
        raise Refused("%s: not a tune file of format 1" % path)
    if not tune.get("weapon_key") or not isinstance(tune.get("edits"), list):
        raise Refused("%s: no weapon_key or edits" % path)
    if not tune["edits"]:
        raise Refused("%s: no edits (nothing was changed before tune save)" % path)
    return tune


def recipe_tag(recipe, tag):
    """the name the recipe gives a tag of the pack, or None for a tag the recipe does not make. The pack's own tags are
    named by the pack's prefix and the donor's name (an ae_pack\\ tag takes the prefix in its place)"""
    prefix = recipe["pack"]["prefix"].replace("\\", "/").lower()
    name = tag.replace("\\", "/")
    if not name.lower().startswith(prefix):
        return None
    rest = name[len(prefix):]
    known = {}
    for step in recipe["steps"]:
        known.setdefault(step["tag"].lower(), step["tag"])
    for entry in recipe["source"].get("allowlist", ()):
        known.setdefault(entry.lower(), entry)
    for candidate in (rest, "ae_pack/" + rest):
        if candidate.lower() in known:
            return known[candidate.lower()]
    return None


def winning_source(recipe, tag):
    """where the tag's present version comes from: the first layer (the recipe's own order) with a step on it, else
    the source"""
    for layer in recipe["layers"]:
        if layer == LAYER:
            continue
        if any(step["layer"] == layer and step["tag"] == tag for step in recipe["steps"]):
            return [layer, tag]
    return ["source", tag]


def write_atomic(path, text):
    """the file replaced whole or not at all: a temporary file in the same folder, written, synced and renamed over it
    (a crash or a full disk leaves the old recipe)"""
    path = Path(path)
    handle, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".new", dir=str(path.parent))
    try:
        with os.fdopen(handle, "w", encoding="utf-8") as file:
            file.write(text)
            file.flush()
            os.fsync(file.fileno())
        if path.exists():
            os.chmod(temporary, path.stat().st_mode & 0o7777)
        os.replace(temporary, path)
    except BaseException:
        if os.path.exists(temporary):
            os.unlink(temporary)
        raise


def apply(recipe_path, tune_path):
    """puts the tune file's numbers in the recipe's tune layer: the lines changed"""
    recipe_path = Path(recipe_path)
    recipe = json.loads(recipe_path.read_text())
    tune = read_tune(tune_path)
    key = recipe["pack"].get("weapon_key")
    if tune["weapon_key"] != key:
        raise Refused("no recipe for %s (this one is %s's)" % (tune["weapon_key"], key))
    by_tag = {}
    for edit in tune["edits"]:
        try:
            tag, (field, value) = edit["tag"], edit["set"]
        except (KeyError, TypeError, ValueError):
            raise Refused("an edit with no tag or set: %r" % (edit,))
        name = recipe_tag(recipe, tag)
        if name is None:
            raise Refused("%s is not a tag the recipe makes (a tag of the map's stays as it is)" % tag)
        by_tag.setdefault(name, []).append((field, value_text(value)))
    lines = []
    if LAYER not in recipe["layers"]:
        recipe["layers"].insert(0, LAYER)
    for name, sets in by_tag.items():
        steps = [step for step in recipe["steps"] if step["layer"] == LAYER and step["tag"] == name]
        if steps:
            step = steps[0]
        else:
            step = {"layer": LAYER, "tag": name, "op": "edit", "from": winning_source(recipe, name), "edits": [],
                    "why": "numbers set in the game (DEV MODE tune): apply_tune.py"}
            recipe["steps"].append(step)
        for field, text in sets:
            for existing in step["edits"]:
                if existing[0] == "set" and existing[1] == field:
                    lines.append("%s: %s %s -> %s" % (name, field, existing[2], text))
                    existing[2] = text
                    break
            else:
                step["edits"].append(["set", field, text])
                lines.append("%s: %s = %s" % (name, field, text))
    for pin in PINS:
        if pin in recipe.get("expected", {}):
            recipe["expected"][pin] = ""
    write_atomic(recipe_path, dump(recipe))
    return lines


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--recipe", required=True)
    parser.add_argument("--tune", required=True)
    options = parser.parse_args(arguments)
    try:
        for line in apply(options.recipe, options.tune):
            print(line)
    except Refused as refusal:
        print("apply_tune: refused: %s" % refusal, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
