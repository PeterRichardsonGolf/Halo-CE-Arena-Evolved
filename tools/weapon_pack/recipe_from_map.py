"""A weapon pack's recipe from a map: one item of a map's sandbox (a weapon, equipment or a grenade) becomes a
recipe of the map-source kind that make_pack.py builds, as ar_silenced/, gravity_hammer/ and sniper_bolt/ are.

    recipe_from_map.py --map <the map> --tag <tag path, as weapons/ma5s/ma5s.weapon> --kind weapon --id <ae_xxx>
                       --display "<NAME>" --invader <Invader's build folder> [--library-path <folder>]
                       --stock-names <file> --out <new folder>
    recipe_from_map.py --batch ...   (a map's whole sandbox, see batch())

What it does (Invader 0.44.0 as the recipes pin it, run as separate programs):
  - the map is written as make_pack.py's prepare writes a source map (whole, deflated again for an Xbox map), and
    `invader-extract -r` takes the item's tag and every tag it leads to into an empty folder;
  - the recipe's allowlist is what that wrote, minus every tag whose group and path the stock maps have
    (--stock-names: `ae_pack_build --list-names --out <file> <the stock maps>`): those stay externals, never the
    pack's own tags;
  - steps: the donor scenario (Blood Gulch's, no vehicles, the weapon or equipment in its palette) and globals (a grenade:
    the grenade list's first entry set to the grenade's equipment and projectile), as the recipes' first two; for a Custom Edition map (version 609), the models converted to Xbox models (and the weapon's model
    references pointed at them) and each multipurpose bitmap a shader_model names converted too;
  - equipment and grenades are packs of tags only, as vehicle_damage/ is (no weapon; roots eqip:<tag>, a grenade's
    also proj:<its projectile>), "loadable" meaning aepack.py check accepts the pack;
  - the recipe's folder gets recipe.json, texts.txt (a weapon's) and CREDITS.txt, whose author and licence lines say TO FILL.

It refuses, writing nothing: a tag the map does not have, a map with stripped or protected names (a tag named
"<protected>", or one name given twice), an item that is the stock maps' own tag, and an output folder that is not empty.
What a built pack needs beyond this (a Custom Edition map's sounds and HUD tags: steps "sound" and "hud_hires", which
need the source's decoded sounds) is named in the recipe's "about"; the recipe is a start, make_pack.py says what is
missing.
"""
import argparse
import copy
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import make_pack  # noqa: E402

HERE = Path(__file__).resolve().parent
KINDS = ("weapon", "equipment", "grenade")
# the extension each tag group's files have, to the group's four letters (stock-names.txt names tags by group)
GROUPS = {
    "actor": "actr", "actor_variant": "actv", "antenna": "ant!", "biped": "bipd", "bitmap": "bitm", "camera_track": "trak",
    "color_table": "colo", "continuous_damage_effect": "cdmg", "contrail": "cont", "damage_effect": "jpt!",
    "decal": "deca", "detail_object_collection": "dobc", "device_control": "ctrl", "device_light_fixture": "lifi",
    "device_machine": "mach", "dialogue": "udlg", "effect": "effe", "equipment": "eqip", "flag": "flag", "fog": "fog ",
    "font": "font", "garbage": "garb", "gbxmodel": "mod2", "globals": "matg", "glow": "glw!",
    "grenade_hud_interface": "grhi", "hud_globals": "hudg", "hud_message_text": "hmt ", "hud_number": "hud#",
    "item_collection": "itmc", "lens_flare": "lens", "light": "ligh", "light_volume": "mgs2", "lightning": "elec",
    "material_effects": "foot", "meter": "metr", "model": "mode", "model_animations": "antr",
    "model_collision_geometry": "coll", "multiplayer_scenario_description": "mply", "particle": "part",
    "particle_system": "pctl", "physics": "phys", "placeholder": "plac", "point_physics": "pphy",
    "projectile": "proj", "scenario": "scnr", "scenario_structure_bsp": "sbsp", "scenery": "scen",
    "shader_environment": "senv", "shader_model": "soso", "shader_transparent_chicago": "schi",
    "shader_transparent_chicago_extended": "scex", "shader_transparent_generic": "sotr",
    "shader_transparent_glass": "sgla", "shader_transparent_meter": "smet", "shader_transparent_plasma": "spla",
    "shader_transparent_water": "swat", "sky": "sky ", "sound": "snd!", "sound_environment": "snde",
    "sound_looping": "lsnd", "sound_scenery": "ssce", "string_list": "str#", "tag_collection": "tagc",
    "ui_widget_collection": "Soul", "ui_widget_definition": "DeLa", "unicode_string_list": "ustr",
    "unit_hud_interface": "unhi", "vehicle": "vehi", "virtual_keyboard": "vcky", "weapon": "weap",
    "weapon_hud_interface": "wphi", "wind": "wind",
}
# groups an Xbox pack gets by a step, from a Custom Edition map's tag of another group
HAND_STEP_GROUPS = ("sound", "weapon_hud_interface")
TEMPLATE = HERE / "ar_silenced" / "recipe.json"
CE_VERSION = make_pack.CE_MAP_VERSION


class Refused(Exception):
    """no recipe is written, with the reason"""


def extension(tag):
    return tag.rsplit(".", 1)[-1] if "." in tag.rsplit("/", 1)[-1] else ""


# groups the PC and the Xbox name differently for one kind of tag: the PC's gbxmodel is the Xbox's model
GROUP_EQUIVALENTS = {"gbxmodel": "model"}


def stock_key(tag):
    """(group, name) as stock-names.txt has a tag, made canonical: the four letters of the group (a PC group taken as
    its Xbox equivalent: a gbxmodel is a stock model's twin), the path in lower case with backslashes and no extension.
    Case and separators never tell two tags apart (the game finds them without case)"""
    name, _, ext = tag.rpartition(".")
    ext = GROUP_EQUIVALENTS.get(ext.lower(), ext.lower())
    group = GROUPS.get(ext)
    return (group, name.replace("/", "\\").lower()) if group else None


def read_stock_names(path):
    """the set of (group, name) the stock maps hold (ae_pack_build --list-names: group, a tab, the name, a line each)"""
    names = set()
    for line in Path(path).read_text(errors="replace").splitlines():
        group, _, name = line.partition("\t")
        if name:
            names.add((group, name.strip().replace("/", "\\").lower()))
    return names


def name_problem(tag):
    """why a tag name is not a canonical relative path, or "": the names come from a map nobody vouches for, and become
    files in folders of ours. Refused: empty, absolute (a slash first, a drive), a "." or ".." part, an empty part, a
    backslash (the separator is /), a control character or a DEL, a part with a space at either end"""
    if not isinstance(tag, str) or not tag:
        return "an empty tag name"
    if any(ord(c) < 0x20 or ord(c) == 0x7F for c in tag):
        return "a tag name with a control character: %r" % tag
    if "\\" in tag:
        return "a tag name with a backslash: %r" % tag
    if tag.startswith("/") or re.match(r"^[A-Za-z]:", tag):
        return "an absolute tag name: %r" % tag
    for part in tag.split("/"):
        if part in ("", ".", ".."):
            return "a tag name with an empty, . or .. part: %r" % tag
        if part != part.strip(" "):
            return "a tag name with a space at the end of a part: %r" % tag
    return ""


def nameless_problem(listing):
    """why the extracted names cannot be trusted, or ""; the listing is every `Extracted <tag>` line, in order"""
    seen = set()
    for tag in listing:
        problem = name_problem(tag)
        if problem:
            return problem
        low = tag.lower()
        if "<" in tag or "protected" in low or not tag.rsplit("/", 1)[-1].split(".")[0]:
            return "a tag with a stripped or protected name: %r" % tag
        if low in seen:
            return "two tags named %r" % tag
        seen.add(low)
    return ""


def id_parts(item_id):
    if not re.fullmatch(r"ae_[a-z0-9_]+", item_id):
        raise Refused("an id is ae_ and lower-case letters, digits and _: %r" % item_id)
    return item_id[3:]


def backslashes(tag):
    return tag.replace("/", "\\")


def without_ext(tag):
    return tag.rsplit(".", 1)[0]


def allowlist_of(listing, stock):
    """the extracted tags that are not the stock maps' (their group and path), sorted"""
    kept = []
    for tag in listing:
        key = stock_key(tag)
        if key is not None and key in stock:
            continue
        kept.append(tag)
    return sorted(set(kept))


def compose(tag, kind, item_id, display, map_name, map_file, sha256, tag_count, version, listing, stock,
            reader, template=None, projectile=None):
    """the recipe (a dict) for one item. listing: the tags `invader-extract -r` wrote for it; stock: the stock maps'
    (group, name); reader(tag, field): a field of an extracted tag, as Invader's editor prints it; projectile: a
    grenade's projectile tag (the equipment's is the item's tag)"""
    if kind not in KINDS:
        raise Refused("no such kind: %s" % kind)
    if kind == "grenade" and not projectile:
        raise Refused("a grenade recipe needs the grenade's projectile tag")
    short = id_parts(item_id)
    for named in [tag] + ([projectile] if projectile else []):
        if name_problem(named):
            raise Refused(name_problem(named))
    problem = nameless_problem(listing)
    if problem:
        raise Refused(problem)
    items = [tag] + ([projectile] if kind == "grenade" else [])
    for item in items:
        if item not in listing:
            raise Refused("the map has no tag %s" % item)
        if stock_key(item) in stock:
            raise Refused("%s is a tag of the stock maps: nothing to extract, the map's own is used" % item)
    recipe = copy.deepcopy(template or json.loads(TEMPLATE.read_text()))
    allowlist = allowlist_of(listing, stock)
    custom_edition = version == CE_VERSION
    scenario_edits = [["erase", "vehicles[*]"], ["erase", "vehicle_palette[*]"]]
    globals_edits = [["erase", "multiplayer_information[0].vehicles[*]"]]
    if kind == "weapon":
        scenario_edits += [["insert", "weapon_palette", 1, "end"], ["set", "weapon_palette[0].name", backslashes(tag)]]
    elif kind == "equipment":
        # (Blood Gulch's scenario has no equipment palette: the item is its first entry)
        scenario_edits += [["insert", "equipment_palette", 1, "end"], ["set", "equipment_palette[0].name", backslashes(tag)]]
    else:
        # (a grenade is named by the globals' grenade list: the first of its two)
        globals_edits += [["set", "grenades[0].equipment", backslashes(tag)],
                          ["set", "grenades[0].projectile", backslashes(projectile)]]
    steps = [
        {"layer": "fix-xbox", "tag": "ae_pack/donor/donor.scenario", "op": "edit",
         "from": ["stock", "levels/test/bloodgulch/bloodgulch.scenario"], "edits": scenario_edits,
         "why": "the donor map: Blood Gulch's scenario without vehicles, the item named in it"},
        {"layer": "fix-xbox", "tag": "globals/globals.globals", "op": "edit", "from": ["stock", "globals/globals.globals"],
         "edits": globals_edits, "why": "the donor's globals without vehicles"},
    ]
    read_by_steps = set()
    new_names = {}
    if custom_edition:
        for entry in allowlist:
            if entry.endswith(".gbxmodel"):
                new_names[entry] = entry[:-len(".gbxmodel")] + ".model"
                steps.append({"layer": "fix-xbox", "tag": new_names[entry], "op": "model", "from": ["source", entry]})
                read_by_steps.add(entry)
        for entry in allowlist:
            if entry.endswith(".shader_model"):
                multipurpose = reader(entry, "multipurpose_map")
                if multipurpose and multipurpose in allowlist and multipurpose not in read_by_steps:
                    steps.append({"layer": "fix-xbox", "tag": multipurpose, "op": "multipurpose",
                                  "from": ["source", multipurpose]})
                    read_by_steps.add(multipurpose)
        # (every tag that names a gbxmodel: the item, its projectile, a child object, a scenery: the donor build takes
        # no gbxmodel, so each is read from the source with its model references pointed at the Xbox models)
        for entry in allowlist:
            if entry.endswith(".gbxmodel") or entry in read_by_steps:
                continue
            edits = []
            for field in ("model", "first_person_model"):
                value = reader(entry, field)
                if value in new_names:
                    edits.append(["set", field, new_names[value]])
            if edits:
                steps.append({"layer": "fix-xbox", "tag": entry, "op": "edit", "from": ["source", entry], "edits": edits,
                              "why": "the tag names the Xbox models the map's models were converted to"})
                read_by_steps.add(entry)
    recipe["steps"] = steps
    recipe["source"] = {
        "kind": "map", "name": "%s (TO FILL: its author and year)" % map_name, "file": map_file, "sha256": sha256,
        "tags": tag_count, "verbatim": sorted(set(allowlist) - read_by_steps), "allowlist": allowlist}
    pack = {"id": item_id, "revision": 1, "display": display, "prefix": "ae\\packs\\%s\\" % short}
    if kind == "weapon":
        pack.update({"weapon": backslashes(without_ext(tag)), "weapon_key": short, "labels": reader(tag, "label") or "",
                     "behaviour": 0, "replaces": 0, "texts": "texts.txt"})
    pack.update({"credits": "CREDITS.txt", "smooth": [], "stock_derived": {}})
    if kind != "weapon":
        # (a pack of tags only, as vehicle_damage/ is: roots "<group>:<name>", which the game finds by name)
        pack["roots"] = ["%s:%s" % (GROUPS[extension(item)], backslashes(without_ext(item))) for item in items]
        pack["roots"].sort(key=lambda root: root.startswith("eqip:"))
    recipe["pack"] = pack
    recipe["layers"] = ["fix-ae", "fix-xbox"]
    recipe["art"], recipe["art_scripts"], recipe["art_inputs"], recipe["art_settings"] = {}, {}, {}, {}
    recipe["art_drawings"] = {}
    for key in ("extra_sources", "derived"):
        recipe.pop(key, None)
    recipe["expected"] = {"about": "no build recorded yet: make_pack.py logs the donor's and the pack's hashes to record",
                          "donor_sha256": "", "pack_sha256": ""}
    needs = sorted({extension(e) for e in allowlist if extension(e) in HAND_STEP_GROUPS})
    recipe["about"] = ("The %s of %s (%s), taken from the map's tags by recipe_from_map.py. Nothing is tuned: numbers are "
                       "the map's. %s" % (display, map_name, tag,
                                          ("A Custom Edition map: its %s tags need steps made by hand "
                                           "(sound, hud_hires) before a build is the Xbox's." % ", ".join(needs))
                                          if custom_edition and needs else ""))
    return recipe


def texts_text(display):
    return ("# The pack's texts in its weapon entry (ae_pack_build --texts; docs/weapon-packs.md).\n"
            "weapon_pickup=Picked up a %s\nammo_pickup=Picked up %%d rounds for %s\n" % (display.lower(), display.lower()))


def credits_text(item_id, display, map_name, tag):
    return ("%s (weapon pack %s) - credits\n\nTaken from the tags of %s (%s).\n\nAuthor: TO FILL\nYear: TO FILL\n"
            "Licence or permission: TO FILL\nKind: TO FILL (released asset pack, community map, ...)\n\n"
            "Nothing of the map's content ships until the author's name and the kind are filled in and its reuse is "
            "allowed.\n" % (display, item_id, map_name, tag))


# ---------- the programs

def run_invader(invader, library_path, program, *arguments):
    environment = dict(os.environ)
    if library_path:
        environment["LD_LIBRARY_PATH"] = str(library_path)
    result = subprocess.run([str(Path(invader) / program), *[str(a) for a in arguments]], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, env=environment, check=False)
    return result.returncode, result.stdout.decode("utf-8", "replace"), result.stderr.decode("utf-8", "replace")


def map_version(path):
    import struct
    with open(path, "rb") as file:
        head = file.read(8)
    return struct.unpack_from("<I", head, 4)[0] if len(head) == 8 and head[:4] == b"daeh" else 0


class Extractor:
    """invader-extract and invader-edit on one map, in a scratch folder next to the output"""

    def __init__(self, map_path, invader, library_path, scratch):
        self.invader, self.library_path = invader, library_path
        self.scratch = Path(scratch)
        self.scratch.mkdir(parents=True)
        image, _, self.tag_count = make_pack.map_source_copy(map_path)
        self.copy = self.scratch / "map" / Path(map_path).name
        self.copy.parent.mkdir()
        self.copy.write_bytes(image)
        self.tags = self.scratch / "tags"

    def extract(self, *tags):
        """the tags the items lead to: every `Extracted` line's tag"""
        if self.tags.exists():
            shutil.rmtree(self.tags)
        self.tags.mkdir()
        searches = []
        for tag in tags:
            searches += ["-s", backslashes(tag)]
        code, out, err = run_invader(self.invader, self.library_path, "invader-extract", "-r", *searches,
                                     "-t", self.tags, self.copy)
        listing = [line[len("Extracted "):].strip() for line in out.splitlines() if line.startswith("Extracted ") and
                   " matched tags" not in line]
        if code != 0 or not listing:
            raise Refused("the map has no tag %s%s" % (" or ".join(tags), (": " + err.strip()[-200:]) if err.strip() else ""))
        return listing

    def read(self, tag, field):
        code, out, _ = run_invader(self.invader, self.library_path, "invader-edit", "-t", self.tags, "-G", field, tag)
        return out.strip() if code == 0 else ""

    def count(self, tag, block):
        code, out, _ = run_invader(self.invader, self.library_path, "invader-edit", "-t", self.tags, "-C", block, tag)
        return int(out.strip()) if code == 0 and out.strip().isdigit() else 0

    def extract_many(self, patterns):
        """the tags matching any of the patterns (not their dependencies; the multiplayer globals too): the listing"""
        if self.tags.exists():
            shutil.rmtree(self.tags)
        self.tags.mkdir()
        arguments = ["-n", "-t", self.tags]
        for pattern in patterns:
            arguments += ["-s", pattern]
        code, out, err = run_invader(self.invader, self.library_path, "invader-extract", *arguments, self.copy)
        if code != 0:
            raise Refused("the map's tags could not be listed: %s" % err.strip()[-200:])
        return [line[len("Extracted "):].strip() for line in out.splitlines() if line.startswith("Extracted ") and
                " matched tags" not in line]


def grenade_projectile(extractor, equipment):
    """the projectile the map's globals pair with the grenade's equipment"""
    for globals_tag in (tag for tag in extractor.extract_many(("*.globals",)) if tag.endswith(".globals")):
        for index in range(extractor.count(globals_tag, "grenades")):
            if extractor.read(globals_tag, "grenades[%d].equipment" % index).lower() == equipment.lower():
                projectile = extractor.read(globals_tag, "grenades[%d].projectile" % index)
                if projectile:
                    return projectile
    raise Refused("no grenade of the map's globals has the equipment %s" % equipment)


def make_recipe(map_path, tag, kind, item_id, display, invader, stock_names, out_folder, library_path=None):
    """writes the item's recipe folder (recipe.json, texts.txt, CREDITS.txt) and returns its recipe.json"""
    map_path, out_folder = Path(map_path), Path(out_folder)
    if out_folder.exists() and any(out_folder.iterdir()):
        raise Refused("the output folder is not empty: %s" % out_folder)
    scratch = out_folder.parent / (out_folder.name + ".scratch")
    if scratch.exists():
        shutil.rmtree(scratch)
    try:
        extractor = Extractor(map_path, invader, library_path, scratch)
        projectile = None
        if kind == "grenade":
            projectile = grenade_projectile(extractor, tag)
        listing = extractor.extract(tag, *([projectile] if projectile else []))
        recipe = compose(tag, kind, item_id, display, map_path.name, map_path.name,
                         make_pack.sha256_file(map_path), extractor.tag_count, map_version(map_path), listing,
                         read_stock_names(stock_names), extractor.read, projectile=projectile)
    finally:
        if scratch.exists():
            shutil.rmtree(scratch)
    out_folder.mkdir(parents=True, exist_ok=True)
    (out_folder / "recipe.json").write_text(json.dumps(recipe, indent=1, ensure_ascii=False) + "\n")
    if kind == "weapon":
        (out_folder / "texts.txt").write_text(texts_text(display))
    (out_folder / "CREDITS.txt").write_text(credits_text(item_id, display, map_path.name, tag))
    return out_folder / "recipe.json"


# ---------- a map's whole sandbox

SANDBOX_PATTERNS = ("*.scenario", "*.item_collection", "*.globals")


def sandbox_items(listing, get, count):
    """(kind, tag) for every weapon, equipment and grenade a map's sandbox has, sorted: the scenario's weapon and
    equipment palettes and its netgame equipment's item collections, every item collection's permutations, and the
    globals' grenades (their equipment, whose kind is grenade, not equipment). listing: the extracted tags; get(tag,
    key) and count(tag, block): Invader's editor on them"""
    problem = nameless_problem(listing)
    if problem:
        raise Refused(problem)
    found = {}

    def add(kind, tag):
        tag = tag.strip()
        if tag and extension(tag) == kind:
            if name_problem(tag):
                raise Refused(name_problem(tag))
            found.setdefault(tag.lower(), (kind, tag))

    collections = [tag for tag in listing if tag.endswith(".item_collection")]
    for scenario in (tag for tag in listing if tag.endswith(".scenario")):
        for block, field, kind in (("weapon_palette", "name", "weapon"), ("equipment_palette", "name", "equipment")):
            for index in range(count(scenario, block)):
                add(kind, get(scenario, "%s[%d].%s" % (block, index, field)))
        for index in range(count(scenario, "netgame_equipment")):
            collection = get(scenario, "netgame_equipment[%d].item_collection" % index)
            if collection and collection not in collections and collection in listing:
                collections.append(collection)
    for collection in collections:
        for index in range(count(collection, "permutations")):
            item = get(collection, "permutations[%d].item" % index)
            for kind in ("weapon", "equipment"):
                add(kind, item)
    for globals_tag in (tag for tag in listing if tag.endswith(".globals")):
        for index in range(count(globals_tag, "grenades")):
            equipment = get(globals_tag, "grenades[%d].equipment" % index).strip()
            if equipment:
                if name_problem(equipment):
                    raise Refused(name_problem(equipment))
                found[equipment.lower()] = ("grenade", equipment)
    return sorted(found.values(), key=lambda item: (item[1].lower(), item[0]))


def list_sandbox(map_path, invader, scratch, library_path=None):
    """a map's sandbox: [(kind, tag)] (see sandbox_items), the map's tags as Invader extracts them into scratch (a folder
    that does not exist yet, and is left for the caller to delete)"""
    extractor = Extractor(map_path, invader, library_path, scratch)
    listing = extractor.extract_many(SANDBOX_PATTERNS)
    return sandbox_items(listing, extractor.read, extractor.count)


def item_ids(prefix, tags):
    """one id for each tag: ae_<prefix>_<the tag's last name, lower case, anything but letters and digits as _>, with
    _2, _3 ... on a clash"""
    prefix = re.sub(r"[^a-z0-9]+", "_", prefix.lower()).strip("_")
    ids, used = [], set()
    for tag in tags:
        leaf = re.sub(r"[^a-z0-9]+", "_", without_ext(tag.rsplit("/", 1)[-1]).lower()).strip("_") or "item"
        base = "ae_%s_%s" % (prefix, leaf)
        item_id, number = base, 1
        while item_id in used:
            number += 1
            item_id = "%s_%d" % (base, number)
        used.add(item_id)
        ids.append(item_id)
    return ids


def display_name(tag):
    return re.sub(r"[^A-Za-z0-9]+", " ", without_ext(tag.rsplit("/", 1)[-1])).strip().upper() or "ITEM"


def one_line(text):
    return " ".join(str(text).split())


def write_manifest(out_folder, rows):
    manifest = Path(out_folder) / "manifest.tsv"
    manifest.write_text("\n".join(rows) + "\n")
    return manifest


def made_count(manifest):
    return sum(1 for line in Path(manifest).read_text().splitlines()[1:] if line.split("\t")[3:4] == ["made"])


def batch(map_path, invader, stock_names, out_folder, prefix, library_path=None, items=None):
    """a recipe for each item of the map's sandbox in <out_folder>/<id>/, and manifest.tsv (kind, tag, id, status, reason:
    "made" or "refused" and why, one line an item); an item that fails is recorded and the rest go on. Returns the
    manifest's path. items: the (kind, tag) list when it is already known (tests)"""
    map_path, out_folder = Path(map_path), Path(out_folder)
    if out_folder.exists() and any(out_folder.iterdir()):
        raise Refused("the output folder is not empty: %s" % out_folder)
    if items is None:
        scratch = out_folder.parent / (out_folder.name + ".list")
        if scratch.exists():
            shutil.rmtree(scratch)
        try:
            items = list_sandbox(map_path, invader, scratch, library_path)
        finally:
            if scratch.exists():
                shutil.rmtree(scratch)
    out_folder.mkdir(parents=True, exist_ok=True)
    rows = ["kind\ttag\tid\tstatus\treason"]
    for (kind, tag), item_id in zip(items, item_ids(prefix, [tag for _, tag in items])):
        try:
            make_recipe(map_path, tag, kind, item_id, display_name(tag), invader, stock_names, out_folder / item_id,
                        library_path)
            rows.append("\t".join((one_line(kind), one_line(tag), item_id, "made", "")))
        except Refused as failure:     # (one item refused: the batch goes on; any other error stops it)
            rows.append("\t".join((one_line(kind), one_line(tag), item_id, "refused", one_line(failure))))
        except BaseException:
            write_manifest(out_folder, rows)
            raise
    return write_manifest(out_folder, rows)


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--map", required=True)
    parser.add_argument("--tag")
    parser.add_argument("--kind", default="weapon")
    parser.add_argument("--id")
    parser.add_argument("--display")
    parser.add_argument("--batch", action="store_true")
    parser.add_argument("--prefix")
    parser.add_argument("--invader", required=True)
    parser.add_argument("--library-path")
    parser.add_argument("--stock-names", required=True)
    parser.add_argument("--out", required=True)
    options = parser.parse_args(arguments)
    if options.batch:
        if not options.prefix:
            parser.error("--batch needs --prefix")
    elif not (options.tag and options.id and options.display):
        parser.error("--tag, --id and --display are needed (or --batch)")
    try:
        if options.batch:
            manifest = batch(options.map, options.invader, options.stock_names, options.out, options.prefix,
                             options.library_path)
            print(manifest)
            if not made_count(manifest):
                print("recipe_from_map: no item was made: see %s" % manifest, file=sys.stderr)
                return 1
            return 0
        path = make_recipe(options.map, options.tag, options.kind, options.id, options.display, options.invader,
                           options.stock_names, options.out, options.library_path)
    except (Refused, make_pack.Refused) as refusal:
        print("recipe_from_map: refused: %s" % refusal, file=sys.stderr)
        return 1
    print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
