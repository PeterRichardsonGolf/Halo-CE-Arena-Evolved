"""Builds a weapon pack from a recipe: from CMT's source release (its zip, untouched) and the player's own stock map
to the donor map and the pack, every step scripted and checked.

    make_pack.py --recipe tools/weapon_pack/battle_rifle/recipe.json --work <folder> --zip <CMT's zip>
                 --stock-map <the stock map the donor's scenario starts from>
                 [--stock-maps <folder of the 13 stock multiplayer maps>]   (default: --stock-map's folder)
                 [--invader <Invader's build folder>] [--library-path <folder of the libraries Invader needs>]
                 [--ffmpeg <program>] [--builder <ae_pack_build>]
                 [--reference <folder of the prototype's fix folders>] [--approved-donor <the approved donor map>]
                 [--stage prepare|art|build|all]

What it does, all of it under --work (never /tmp, never the prototype's folders; a folder it made itself, which
it marks, since it deletes and remakes the folders in it):
  prepare  Invader's programs, the zip and the stock map checked against the recipe's SHA-256s; exactly the
           recipe's allowlist unpacked from the zip (cmt-tags/); the stock map's tags extracted (stock-tags/);
           what the art scripts read written to art-in/
  art      our own art drawn (art/): the gun screen's digits and compass, the sharp scope and its label, the
           ammunition counter, the prompts' picture of the rifle. Each picture must be the recipe's: it is pinned
           by its decoded pixels, mode and size, so the same picture from another library version or compression
           is taken, and one differing pixel is refused. The art scripts need numpy and Pillow; on a machine
           without them, art-in/ is taken to one that has them, "--stage art" run there on a folder holding it,
           and art/ brought back. (The pixels depend on the libraries' arithmetic: another numpy or Pillow may
           draw a pixel differently, and is then refused, not used.)
  build    every fix tag made by its step into its layer (layers/<name>/), each compared with the prototype's tag
           of that name when --reference is given (fixes.tsv): all the same but the recipe's named departures;
           the donor built by Invader from the layers in the recipe's order (donor/donor.map), its SHA-256 the
           recipe's or nothing goes on; the stock maps' tag names listed; the pack built by ae_pack_build
           (pack/<id>.aepack), a donor the validator corrected refused; audit.tsv (every own tag: pack name,
           where it came from, group, size, everything its steps read) and externals.tsv (every tag the pack
           leaves to the map) written and their rules checked; the pack compared with its donor (tags, pixels,
           samples, vertices) and, with --approved-donor, with the approved donor, from which it may differ only
           in the tags the recipe names (departures.tsv). A pack refused at any point is set aside as .refused

Pinned by SHA-256: the zip, the stock map, Invader's programs, the art inputs, the donor, the pack; by pixels:
the art. Not pinned, only recorded or logged: ffmpeg (its version is written to donor/build.log; a decoder that
gave other samples changes the donor's hash), Python, numpy and Pillow (the art's pins catch them),
ae_pack_build (this repository's own, built from the same commit).

Invader (GPL-3), ffmpeg and ae_pack_build are separate programs run from here; nothing of CMT's, of the game's or
built from them is written anywhere but --work.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import aepack
import donor_map
import dxt_lift
import extract_allowlist
import hek
import hud_hires_to_xbox
import hud_tag
import multipurpose_to_xbox
import os_strip
import pack_compare
import png_pixels
import sound_oggs
import tag_fields

HERE = Path(__file__).resolve().parent
TEMPORARY_ROOTS = ("/tmp", "/var/tmp", "/dev/shm", "/run")
# the prototype's folders (read-only references): <anything>/halo-test/<one of these>
SPIKE_FOLDERS = ("wp2", "r5", "r6")
OPERATIONS = ("copy", "os_strip", "multipurpose", "dxt_lift", "model", "sound", "edit", "hud_hires", "bitmap",
              "ammo_hud", "counter_hud")
# a file make_pack.py puts in a work folder it made: it deletes folders only where this is
MARKER = ".make_pack-work"
ART_STAGE_FOLDERS = ("art-in", "art")
INVADER_PROGRAMS = ("invader-build", "invader-extract", "invader-edit", "invader-bitmap", "invader-sound",
                    "invader-convert", "invader-bludgeon")
STOCK_MULTIPLAYER_MAPS = ("beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation",
                          "hangemhigh", "longest", "prisoner", "putput", "ratrace", "sidewinder", "wizard")
SCRIPT_PREFIX = "AE script "
STOCK_PREFIX = "stock "


class Refused(Exception):
    """the run stops, with the reason"""


def sha256_file(path):
    return extract_allowlist.sha256_file(path)


def log(text):
    print(text, flush=True)


# ---------- the recipe

def scripts():
    """the names of this folder's scripts"""
    return {path.name for path in HERE.glob("*.py")}


def edit_inputs(edits):
    found = []
    for edit in edits or ():
        # (a graft from "self" copies a part of the tag being edited: it reads nothing else)
        if edit and edit[0] == "graft" and edit[1] != "self":
            found.append((edit[1], edit[2]))
    return found


def step_inputs(step):
    """what a step reads: ("cmt", allowlist name) / ("stock", tag) / ("layer", (layer, tag)) / ("art", file)"""
    found = []

    def add(root, tag):
        if root == "cmt":
            found.append(("cmt", tag))
        elif root == "stock":
            found.append(("stock", tag))
        else:
            found.append(("layer", (root, tag)))

    if "from" in step:
        add(*step["from"])
    if step["op"] == "sound":
        add("cmt", step["oggs"])
    for name in step.get("art", ()):
        found.append(("art", name))
    for root, tag in edit_inputs(step.get("edits")):
        add(root, tag)
    return found


def recipe_inputs(recipe):
    """every file the recipe reads: {"cmt": allowlist names, "stock": tags, "art": files}"""
    found = {"cmt": set(), "stock": set(), "art": set()}
    for step in recipe["steps"]:
        for kind, name in step_inputs(step):
            if kind in found:
                found[kind].add(name)
    for entry in recipe["art_inputs"].values():
        if entry["root"] in found:
            found[entry["root"]].add(entry["tag"])
    return found


def unknown_scripts(recipe):
    """the art files whose script is not one of this folder's"""
    have = scripts()
    return sorted(name for name, script in recipe["art_scripts"].items() if script not in have) + sorted(
        name for name in recipe["art"] if name not in recipe["art_scripts"])


def check_recipe(recipe):
    """the recipe's own rules, before anything is run"""
    for key in ("pack", "source", "stock_map", "tools", "donor", "layers", "steps", "art", "art_scripts",
                "art_inputs", "art_settings", "expected", "reference", "departures", "approved_reference"):
        if key not in recipe:
            raise Refused("the recipe has no %s" % key)
    made = set()
    for step in recipe["steps"]:
        if step["op"] not in OPERATIONS:
            raise Refused("no such step: %s" % step["op"])
        if step["layer"] not in recipe["layers"]:
            raise Refused("a step in no layer: %s" % step["tag"])
        if (step["layer"], step["tag"]) in made:
            raise Refused("made twice: %s/%s" % (step["layer"], step["tag"]))
        for kind, name in step_inputs(step):
            if kind == "cmt" and name not in recipe["source"]["allowlist"]:
                raise Refused("%s reads %s, which the allowlist does not have" % (step["tag"], name))
            if kind == "layer" and tuple(name) not in made:
                raise Refused("%s reads %s before it is made" % (step["tag"], "/".join(name)))
            if kind == "art" and name not in recipe["art"]:
                raise Refused("%s reads art the recipe does not list: %s" % (step["tag"], name))
        made.add((step["layer"], step["tag"]))
    if unknown_scripts(recipe):
        raise Refused("art with no script here: %s" % ", ".join(unknown_scripts(recipe)))
    used = recipe_inputs(recipe)["cmt"] | set(recipe["source"]["verbatim"])
    listed = set(recipe["source"]["allowlist"])
    if used != listed:
        raise Refused("the allowlist and what the recipe reads differ: %s" % ", ".join(sorted(used ^ listed)[:5]))
    if set(recipe["reference"]) != set(recipe["layers"]):
        raise Refused("the reference folders are not the layers'")
    for name in recipe["departures"]:
        if name.split("/", 1)[0] not in recipe["layers"]:
            raise Refused("a departure in no layer: %s" % name)
    for name, entry in recipe["art"].items():
        if set(entry) != {"mode", "size", "pixels_sha256"}:
            raise Refused("art pinned by something other than its pixels: %s" % name)


def builder_arguments(recipe, donor, names, out, recipe_folder):
    """ae_pack_build's arguments for the recipe's pack"""
    pack = recipe["pack"]
    arguments = ["--donor", str(donor), "--weapon", pack["weapon"], "--external-names", str(names),
                 "--prefix", pack["prefix"], "--id", pack["id"], "--revision", str(pack["revision"]),
                 "--display", pack["display"], "--weapon-key", pack["weapon_key"], "--labels", pack["labels"],
                 "--behaviour", str(pack["behaviour"]), "--replaces", str(pack["replaces"]),
                 "--texts", str(Path(recipe_folder) / pack["texts"]), "--icon", pack["icon"]]
    for bitmap in pack["smooth"]:
        arguments += ["--smooth", bitmap]
    arguments += ["--credits", str(Path(recipe_folder) / pack["credits"]), "--verbose", "--out", str(out)]
    return arguments


# ---------- the run's own rules

def check_work_folder(path):
    """the work folder: an absolute path, on a disk (not a temporary folder: they are memory on some machines),
    and not inside the prototype's folders"""
    path = Path(path)
    if not path.is_absolute():
        raise Refused("the work folder must be an absolute path: %s" % path)
    resolved = Path(os.path.abspath(str(path)))
    for candidate in {str(path), str(resolved), os.path.realpath(str(path))}:
        for root in TEMPORARY_ROOTS:
            if candidate == root or candidate.startswith(root + "/"):
                raise Refused("the work folder is a temporary folder: %s" % path)
        parts = Path(candidate).parts
        for index in range(len(parts) - 1):
            if parts[index] == "halo-test" and parts[index + 1] in SPIKE_FOLDERS:
                raise Refused("the work folder is inside the prototype's folders: %s" % path)


def corrections(text):
    """the validator's corrections of the donor as ae_pack_build prints them, or None"""
    found = re.findall(r"(\d+) corrections", text)
    return int(found[-1]) if found else None


def require_no_corrections(text):
    count = corrections(text)
    if count is None:
        raise Refused("ae_pack_build did not say how many corrections the donor needed")
    if count:
        raise Refused("the donor needed %d corrections by the tag validator: a corrected donor makes no pack" % count)


def provenance(recipe, layer, tag):
    """where a tag a step made came from: (source, inputs). The source is the allowlisted file of CMT's at the
    start of the chain of steps, "stock <tag>" when the chain starts from a stock tag, or "AE script <name>" for
    a bitmap compiled from our art. The inputs are EVERYTHING the chain read, each "cmt <file>", "stock <tag>" or
    "art <file>": every step's own inputs (grafts too) and, for art, what the script that drew it read"""
    steps = {(step["layer"], step["tag"]): step for step in recipe["steps"]}
    if (layer, tag) not in steps:
        raise Refused("no step makes %s/%s" % (layer, tag))
    step = steps[(layer, tag)]
    inputs = []
    seen = set()

    def note(kind, name):
        text = "%s %s" % (kind, name)
        if text not in inputs:
            inputs.append(text)

    def art_reads(name):
        for entry in recipe["art_inputs"].values():
            if recipe["art_scripts"].get(name) in entry["scripts"]:
                note(entry["root"], entry["tag"])

    while True:
        if (step["layer"], step["tag"]) in seen:
            raise Refused("a step reads itself: %s" % step["tag"])
        seen.add((step["layer"], step["tag"]))
        for kind, name in step_inputs(step):
            if kind == "art":
                note("art", name)
                art_reads(name)
            elif kind != "layer":
                note(kind, name)
        for kind, name in step_inputs(step):
            # (a graft from another layer's tag brings that tag's own inputs with it)
            if kind == "layer" and list(name) != list(step.get("from", ())):
                for text in provenance(recipe, name[0], name[1])[1]:
                    if text not in inputs:
                        inputs.append(text)
        if step["op"] == "bitmap":
            return SCRIPT_PREFIX + recipe["art_scripts"][step["art"][0]], inputs
        root, name = step["from"]
        if root == "cmt":
            return name, inputs
        if root == "stock":
            return STOCK_PREFIX + name, inputs
        step = steps[(root, name)]


def audit_problems(rows, allowlist, script_names, allowed_stock=()):
    """the audit's rules over its rows ({"pack_name", "source", "group", "size", "inputs"}): every own tag comes
    from an allowlisted file of CMT's or from one of our scripts; every own sound from an allowlisted sound of
    CMT's. A tag is judged by its INPUTS, not by the name of what made it: one whose source or any input is a
    stock tag (a script fed stock pixels, fields grafted from a stock tag) passes only when the recipe names it
    (allowed_stock: pack names), and a sound never"""
    problems = []
    for row in rows:
        source = row["source"]
        stock = [text for text in [source] + list(row.get("inputs", ())) if text.startswith(STOCK_PREFIX)]
        if stock and (row["pack_name"] not in allowed_stock or row["group"] == "snd!"):
            problems.append("%s: made from a stock tag the recipe does not allow: %s" % (
                row["pack_name"], "; ".join(sorted(set(stock)))))
        elif source.startswith(SCRIPT_PREFIX):
            script = source[len(SCRIPT_PREFIX):].split(" ")[0]
            if row["group"] == "snd!":
                problems.append("%s: a sound made by a script, not one of CMT's: %s" % (row["pack_name"], source))
            elif script not in script_names:
                problems.append("%s: no such script: %s" % (row["pack_name"], source))
        elif source.startswith(STOCK_PREFIX):
            pass
        elif source not in allowlist:
            problems.append("%s: not from the allowlist: %r" % (row["pack_name"], source))
        elif row["group"] == "snd!" and not source.endswith(".sound"):
            problems.append("%s: a sound not from a sound file: %s" % (row["pack_name"], source))
    return problems


def departure_problems(rows, departures):
    """fixes.tsv's rows against the recipe's named departures ("layer/tag": why): a tag that is not the
    prototype's must be named, and a named one must really differ (a stale name is a problem too)"""
    problems = []
    named = set(departures)
    for row in rows:
        name = "%s/%s" % (row[0], row[1])
        same = row[2] in ("byte-equal", "0 field differences")
        if name in named:
            named.discard(name)
            if same:
                problems.append("%s: named as a departure but it is the prototype's" % name)
        elif not same:
            problems.append("%s: %s, and the recipe names no departure there" % (name, row[2]))
    problems += ["%s: named as a departure but neither made nor in the prototype" % name for name in sorted(named)]
    return problems


def pack_departure_problems(by_tag, departures):
    """the differences of a pack from the approved donor (pack_compare's by_tag) against the recipe's named ones
    (the donor's tag name, lower case: why): differences anywhere else are problems, and so is a named tag that
    does not differ"""
    problems = []
    for name, texts in by_tag.items():
        if name not in departures:
            problems += ["%s: %s" % (name, text) for text in texts]
    problems += ["%s: named as a departure from the approved donor but it is the same" % name
                 for name in sorted(departures) if name not in by_tag]
    return problems


def claim_work_folder(path, stage):
    """the work folder made ours: a new or empty one gets the marker; one that holds files without it is refused
    (folders in it are deleted between runs, so it must be one this made), but for the art stage's folder that
    holds only art-in/ brought from another machine"""
    path = Path(path)
    path.mkdir(parents=True, exist_ok=True)
    names = {entry.name for entry in path.iterdir()}
    if MARKER not in names:
        if names and not (stage == "art" and names <= set(ART_STAGE_FOLDERS)):
            raise Refused("the work folder holds files and is not one make_pack.py made (no %s in it): %s" % (
                MARKER, path))
        (path / MARKER).write_text("a work folder of tools/weapon_pack/make_pack.py: its folders are deleted "
                                   "and made again by each run\n")


def art_pin_problem(path, entry):
    """why an art file is not the recipe's, or None: pinned by its pixels, mode and size, so that the same
    picture written by another library or compression is taken and a different pixel is not"""
    try:
        have = png_pixels.pin(path)
    except (OSError, png_pixels.Refused) as failure:
        return "cannot be read: %s" % failure
    if have["mode"] != entry["mode"] or have["size"] != list(entry["size"]):
        return "is %s %dx%d, not %s %dx%d" % (have["mode"], have["size"][0], have["size"][1], entry["mode"],
                                               entry["size"][0], entry["size"][1])
    if have["pixels_sha256"] != entry["pixels_sha256"]:
        return "has other pixels than the recipe's (SHA-256 of the decoded pixels)"
    return None


def external_problems(externals, donor, stock):
    """externals: (group, name) the pack leaves to the map; donor, stock: what the donor's build and the stock
    map's tags have for each (a fingerprint). A tag that is not the stock map's own, unchanged, would be left out
    of the pack and the map's used in its place: each is a problem, the tag named"""
    problems = []
    for key in externals:
        if key not in stock or key not in donor:
            problems.append("%s %s: not a tag of the stock map's" % key)
        elif donor[key] != stock[key]:
            problems.append("%s %s: the donor's is not the stock map's own (a changed tag under a stock name "
                            "would be left out of the pack)" % key)
    return problems


def compare_layer(layer, made, reference, field_differences):
    """rows (layer, tag, result, made size, reference size, note) for every tag of either folder: "byte-equal";
    "0 field differences" (the field editor lists the two alike and the bytes that differ are all in the file's
    64-byte header); "differs" (the note: how many listed fields, or "data bytes" when the fields are alike but
    bytes after the header are not: pixels, samples, vertices are no listed fields); "no reference" (made, not in
    the reference); "not made" (in the reference only)"""
    made, reference = Path(made), Path(reference)

    def files(root):
        return {str(path.relative_to(root)).replace(os.sep, "/") for path in root.rglob("*") if path.is_file()}

    ours, theirs = files(made) if made.is_dir() else set(), files(reference) if reference.is_dir() else set()
    rows = []
    for tag in sorted(ours | theirs):
        if tag not in theirs:
            rows.append((layer, tag, "no reference", (made / tag).stat().st_size, 0, ""))
        elif tag not in ours:
            rows.append((layer, tag, "not made", 0, (reference / tag).stat().st_size, ""))
        else:
            a, b = (made / tag).read_bytes(), (reference / tag).read_bytes()
            if a == b:
                rows.append((layer, tag, "byte-equal", len(a), len(b), ""))
            else:
                count = field_differences(made, reference, tag)
                if count:
                    rows.append((layer, tag, "differs", len(a), len(b), "%d fields" % count))
                elif a[hek.HEADER_SIZE:] != b[hek.HEADER_SIZE:]:
                    rows.append((layer, tag, "differs", len(a), len(b), "data bytes"))
                else:
                    rows.append((layer, tag, "0 field differences", len(a), len(b), "header bytes"))
    return rows


def backslashes(tag):
    return tag.replace("/", "\\")


def without_extension(tag):
    return tag.rsplit(".", 1)[0]


def index_tags(root):
    """a tags folder's files by (group, lower-case name with backslashes): the group is the one the file's own
    header names, the name the path without its extension"""
    found = {}
    root = Path(root)
    for path in sorted(root.rglob("*")):
        if path.is_file():
            with open(path, "rb") as file:
                header = file.read(hek.HEADER_SIZE)
            if len(header) == hek.HEADER_SIZE:
                name = without_extension(str(path.relative_to(root)).replace(os.sep, "/"))
                found[(header[0x24:0x28].decode("latin-1"), backslashes(name).lower())] = path
    return found


def resolve_sources(order):
    """order: (root name, folder) in the build's order of tags folders. Each tag by (group, name): the (root
    name, file) of the FIRST folder that has it, which is the one the map is built from"""
    build = {}
    for name, root in order:
        for key, path in index_tags(root).items():
            build.setdefault(key, (name, path))
    return build


def write_table(path, header, rows):
    with open(path, "w") as file:
        file.write("\t".join(header) + "\n")
        for row in rows:
            file.write("\t".join(str(value) for value in row) + "\n")


# ---------- the programs

class Programs:
    def __init__(self, options, recipe):
        self.invader = Path(options.invader).expanduser()
        self.library_path = str(Path(options.library_path).expanduser()) if options.library_path else None
        self.ffmpeg = options.ffmpeg
        self.builder = Path(options.builder).expanduser() if options.builder else None
        self.recipe = recipe

    def check(self):
        tools = self.recipe["tools"]["invader"]
        for name in INVADER_PROGRAMS:
            path = self.invader / name
            if not path.is_file():
                raise Refused("no %s in %s" % (name, self.invader))
            if sha256_file(path) != tools["sha256"][name]:
                raise Refused("%s is not the recipe's build of Invader %s (SHA-256)" % (name, tools["version"]))
        library = self.invader / "libinvader.so"
        if "libinvader.so" in tools["sha256"] and (not library.is_file() or sha256_file(library) !=
                                                    tools["sha256"]["libinvader.so"]):
            raise Refused("libinvader.so is not the recipe's build of Invader %s (SHA-256)" % tools["version"])

    def run(self, program, *arguments):
        """a program run to its end (Invader's by name, any other by its path): what it printed"""
        program = str(program)
        command = [str(self.invader / program) if program.startswith("invader-") else program,
                   *[str(argument) for argument in arguments]]
        try:
            return tag_fields.run_program(command, self.library_path)
        except tag_fields.Failed as failure:
            raise Refused(str(failure))

    def editor(self, tags, tag):
        return tag_fields.Editor(self.invader, tags, tag, library_path=self.library_path)


# ---------- prepare

class Run:
    def __init__(self, options, recipe):
        self.options, self.recipe = options, recipe
        self.work = Path(options.work).expanduser()
        self.recipe_folder = Path(options.recipe).resolve().parent
        self.programs = Programs(options, recipe)
        self.cmt = self.work / "cmt-tags"
        self.stock = self.work / "stock-tags"
        self.layers = self.work / "layers"
        self.data = self.work / "data"
        self.art_in = self.work / "art-in"
        self.art = self.work / "art"

    def root(self, name):
        if name == "cmt":
            return self.cmt
        if name == "stock":
            return self.stock
        if name not in self.recipe["layers"]:
            raise Refused("no such layer: %s" % name)
        return self.layers / name

    def source(self, pair):
        path = self.root(pair[0]) / pair[1]
        if not path.is_file():
            raise Refused("no such tag: %s/%s" % tuple(pair))
        return path

    # ---- prepare

    def prepare(self):
        recipe, options = self.recipe, self.options
        self.programs.check()
        for name in ("zip", "stock_map"):
            if not getattr(options, name):
                raise Refused("--%s is needed" % name.replace("_", "-"))
        stock_map = Path(options.stock_map).expanduser()
        if sha256_file(stock_map) != recipe["stock_map"]["sha256"]:
            raise Refused("the stock map is not the recipe's (SHA-256): %s" % stock_map)
        for folder in (self.cmt, self.stock, self.art_in):
            if folder.exists():
                shutil.rmtree(folder)
        source = recipe["source"]
        written = extract_allowlist.extract(Path(options.zip).expanduser(),
                                            [source["root"] + name for name in source["allowlist"]], self.cmt,
                                            strip=source["root"], sha256=source["sha256"])
        log("prepare: %d of CMT's files unpacked (the allowlist, nothing else)" % len(written))
        self.stock.mkdir(parents=True)
        self.programs.run("invader-extract", "-m", stock_map.parent, "-t", self.stock, stock_map)
        count = sum(1 for path in self.stock.rglob("*") if path.is_file())
        if count != recipe["stock_map"]["tags"]:
            raise Refused("%d tags extracted from the stock map, not %d" % (count, recipe["stock_map"]["tags"]))
        log("prepare: %d stock tags extracted" % count)
        self.art_in.mkdir(parents=True)
        for name, entry in recipe["art_inputs"].items():
            path = self.art_in / name
            if entry["kind"] == "tag":
                shutil.copyfile(self.source((entry["root"], entry["tag"])), path)
            elif entry["kind"] == "geometry":
                path.write_text(self.geometry(entry))
            else:
                raise Refused("no such art input: %s" % entry["kind"])
            if sha256_file(path) != entry["sha256"]:
                raise Refused("art-in/%s is not the recipe's (SHA-256)" % name)
        log("prepare: %d files for the art scripts in %s" % (len(recipe["art_inputs"]), self.art_in))

    def geometry(self, entry):
        """a model's geometry as text for hud_sheet.py: for each part, its vertices' positions, then its
        triangles' three vertex indices a line"""
        editor = self.programs.editor(self.root(entry["root"]), entry["tag"])
        lines = []
        for part in range(entry["parts"]):
            key = "geometries[%d].parts[%d]" % (entry["geometry"], part)
            lines.append("part %d" % part)
            lines += editor.run("-G", key + ".uncompressed_vertices[*].position").splitlines()
            lines.append("tris")
            columns = [editor.run("-G", key + ".triangles[*].vertex%d_index" % vertex).splitlines()
                       for vertex in range(3)]
            lines += [" ".join(row) for row in zip(*columns)]
        return "\n".join(lines) + "\n"

    # ---- art

    def make_art(self):
        import scope_hd
        import scope_label
        import hud_sheet
        try:
            self.draw_art()
        except (scope_hd.Refused, scope_label.Refused, hud_sheet.Refused) as refusal:
            raise Refused("the art: %s" % refusal)

    def draw_art(self):
        try:
            import numpy as np
            from PIL import Image
        except ImportError:
            raise Refused("the art scripts need numpy and Pillow: take %s to a machine that has them, run "
                          "--stage art there on a work folder holding it, and bring its art/ back here" %
                          self.art_in)
        import counter_art
        import hud_sheet
        import scope_hd
        import scope_label
        import screen_art
        recipe = self.recipe
        for name, entry in recipe["art_inputs"].items():
            if not (self.art_in / name).is_file() or sha256_file(self.art_in / name) != entry["sha256"]:
                raise Refused("art-in/%s is missing or not the recipe's (SHA-256)" % name)
        if self.art.exists():
            shutil.rmtree(self.art)
        self.art.mkdir(parents=True)
        settings = recipe["art_settings"]
        screen_art.plate([screen_art.digit(value) for value in range(10)]).save(self.art / "digits.png")
        screen_art.plate([screen_art.compass(frame) for frame in range(8)]).save(self.art / "compass.png")
        meter, alphas = counter_art.sheets()
        Image.fromarray(meter, "RGBA").save(self.art / "br_meter_smooth.png")
        Image.fromarray(alphas, "RGBA").save(self.art / "br_alphas_smooth.png")
        lines, _, _, _ = scope_hd.details(scope_hd.load_alpha(self.art_in / "scope_details.bitmap"),
                                          settings["scope_ink"])
        ring, mask, _ = scope_hd.ring_and_mask(scope_hd.load_alpha(self.art_in / "scope_mask.bitmap"),
                                               settings["scope_ink"] == "bold")

        def grey(alpha, white=False):
            image = np.zeros(alpha.shape + (4,), np.uint8)
            image[..., 0] = image[..., 1] = image[..., 2] = 255 if white else alpha
            image[..., 3] = alpha
            return Image.fromarray(image, "RGBA")

        grey(lines).save(self.art / "br_scope_details_hd.png")
        grey(ring, white=True).save(self.art / "br_scope_ring_hd.png")
        grey(mask).save(self.art / "br_scope_mask_noring.png")
        import reticle_art
        # (grey, not white: an aiming reticle is drawn by its colour as well as its alpha (the crosshair's colour has
        # no alpha of its own), so a white sheet shows as a filled square)
        picture = settings.get("reticle_picture")
        grey(reticle_art.from_picture(self.recipe_folder / picture) if picture else
             reticle_art.zoom_reticle()).save(self.art / "br_zoom_reticle.png")
        # (the bold scope draws its own label: scope_hd.bold_label)
        if settings["scope_ink"] == "bold":
            if settings["scope_label_digit"] != scope_hd.BOLD_LABEL:
                raise Refused("the bold scope's label is drawn for %sx only" % scope_hd.BOLD_LABEL)
            relabelled = lines
        else:
            relabelled, _ = scope_label.relabel(lines, settings["scope_label_digit"])
        Image.fromarray(np.dstack([relabelled] * 4), "RGBA").save(self.art / "br_scope_details_3x.png")
        picture = hud_sheet.icon((self.art_in / "br_geo.txt").read_text())
        if [picture.shape[1], picture.shape[0]] != settings["icon_size"]:
            raise Refused("the icon is %d x %d, not the recipe's size" % (picture.shape[1], picture.shape[0]))
        hud_sheet.plate(picture).save(self.art / "br_icon.png")
        self.check_art()
        log("art: %d files drawn in %s, each the recipe's" % (len(recipe["art"]), self.art))

    def check_art(self):
        for name, entry in self.recipe["art"].items():
            path = self.art / name
            if not path.is_file():
                raise Refused("art/%s is missing (the art stage makes it)" % name)
            problem = art_pin_problem(path, entry)
            if problem:
                raise Refused("art/%s %s" % (name, problem))

    # ---- the steps

    def step(self, step):
        operation = step["op"]
        layer = self.root(step["layer"])
        out = layer / step["tag"]
        out.parent.mkdir(parents=True, exist_ok=True)
        if operation in ("copy", "edit", "hud_hires", "ammo_hud", "counter_hud"):
            shutil.copyfile(self.source(step["from"]), out)
            if operation == "hud_hires":
                hud_hires_to_xbox.convert(self.programs.editor(layer, step["tag"]))
        elif operation == "os_strip":
            stripped, _, _ = os_strip.strip(step["group"], self.source(step["from"]).read_bytes())
            out.write_bytes(stripped)
        elif operation == "multipurpose":
            converted, _ = multipurpose_to_xbox.convert(self.source(step["from"]).read_bytes(),
                                                        meter=step.get("meter", False))
            out.write_bytes(converted)
        elif operation == "dxt_lift":
            lifted, _ = dxt_lift.lift(self.source(step["from"]).read_bytes(), step["lift"])
            out.write_bytes(lifted)
        elif operation == "model":
            self.source(step["from"])
            self.programs.run("invader-convert", "-T", "g2m", "-t", self.root(step["from"][0]), "-o", layer, "-s",
                              backslashes(step["from"][1]))
            for kind in step.get("bludgeon", ()):
                self.programs.run("invader-bludgeon", "-t", layer, "-T", kind, step["tag"])
        elif operation == "sound":
            self.sound(step, layer, out)
        elif operation == "bitmap":
            data = self.data / step["layer"]
            picture = data / (without_extension(step["tag"]) + ".png")
            picture.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(self.art / step["art"][0], picture)
            self.programs.run("invader-bitmap", *step["arguments"], "-d", data, "-t", layer,
                              without_extension(step["tag"]))
            if step.get("sprite") == "corner":
                import hud_sheet
                out.write_bytes(hud_sheet.place_by_corner(out.read_bytes()))
        else:
            raise Refused("no such step: %s" % operation)
        edits = list(step.get("edits", ()))
        if operation == "ammo_hud":
            edits = hud_tag.base_edits(step["icon_sheet"]) + edits
        elif operation == "counter_hud":
            edits = hud_tag.counter_edits(step["alphas"], step["meter"], step["scale"], step["anchor"],
                                          step["off_screen"], step["icon_sequence"]) + edits
        if edits:
            try:
                editor = self.programs.editor(layer, step["tag"])
                tag_fields.apply(editor, edits, lambda root, tag: editor if root == "self" else
                                 self.programs.editor(self.root(root), tag))
            except tag_fields.Failed as failure:
                raise Refused("%s/%s: %s" % (step["layer"], step["tag"], failure))
        if not out.is_file():
            raise Refused("%s/%s was not made" % (step["layer"], step["tag"]))

    def sound(self, step, layer, out):
        """a sound of CMT's with its Ogg Vorbis permutations decoded (ffmpeg) and encoded again as Xbox ADPCM
        (Invader), which an Xbox map can hold; nothing about the sound is changed"""
        data = self.data / step["layer"]
        folder = data / without_extension(step["tag"])
        if folder.exists():
            shutil.rmtree(folder)
        folder.mkdir(parents=True)
        oggs = self.work / "ogg" / without_extension(step["oggs"])
        oggs.mkdir(parents=True, exist_ok=True)
        files = sound_oggs.ogg_files(self.source(("cmt", step["oggs"])).read_bytes())
        if not files:
            raise Refused("no Ogg Vorbis permutation in %s" % step["oggs"])
        for name, samples in files:
            (oggs / name).write_bytes(samples)
            self.programs.run(self.programs.ffmpeg, "-v", "error", "-y", "-i", oggs / name, "-c:a", "pcm_s16le",
                              folder / (name[:-4] + ".wav"))
        shutil.copyfile(self.source(step["from"]), out)
        self.programs.run("invader-sound", "-F", "xbox_adpcm", "-d", data, "-t", layer,
                          without_extension(step["tag"]))

    def field_differences(self, made, reference, tag):
        """how many lines of the field editor's listings of the two tags differ (None if a listing fails)"""
        try:
            ours = self.programs.editor(made, tag).listing().splitlines()
            theirs = self.programs.editor(reference, tag).listing().splitlines()
        except tag_fields.Failed:
            return -1
        return sum(1 for a, b in zip(ours, theirs) if a != b) + abs(len(ours) - len(theirs))

    def make_layers(self):
        recipe = self.recipe
        self.check_art()
        try:
            self.ffmpeg_version = self.programs.run(self.programs.ffmpeg, "-version").splitlines()[0]
        except (Refused, IndexError):
            raise Refused("no ffmpeg: %s" % self.programs.ffmpeg)
        # (nothing of an earlier build stays: a refused run leaves no donor, pack or table behind)
        for folder in (self.layers, self.data, self.work / "ogg", self.work / "donor", self.work / "pack"):
            if folder.exists():
                shutil.rmtree(folder)
        for name in ("fixes.tsv", "audit.tsv", "externals.tsv", "departures.tsv"):
            if (self.work / name).exists():
                (self.work / name).unlink()
        for index, step in enumerate(recipe["steps"]):
            self.step(step)
            if (index + 1) % 16 == 0:
                log("build: %d of %d fix tags made" % (index + 1, len(recipe["steps"])))
        log("build: %d fix tags made in %d layers" % (len(recipe["steps"]), len(recipe["layers"])))
        if not self.options.reference:
            log("build: no --reference: the fix tags are not compared with the prototype's")
            return
        reference = Path(self.options.reference).expanduser()
        rows = []
        for layer in recipe["layers"]:
            rows += compare_layer(layer, self.layers / layer, reference / recipe["reference"][layer],
                                  self.field_differences)
        write_table(self.work / "fixes.tsv", ("layer", "tag", "result", "size", "reference_size", "note"), rows)
        counts = {}
        for row in rows:
            counts[row[2]] = counts.get(row[2], 0) + 1
        log("build: fixes.tsv: %s" % ", ".join("%d %s" % (count, result) for result, count in sorted(counts.items())))
        problems = departure_problems(rows, recipe["departures"])
        if problems:
            for problem in problems[:20]:
                log("  " + problem)
            raise Refused("%d fix tags are not the prototype's where the recipe says they are, or the other way "
                          "(fixes.tsv)" % len(problems))
        log("build: every fix tag is the prototype's but the recipe's %d named departures" % len(
            recipe["departures"]))

    # ---- the donor and the pack

    def build_donor(self):
        recipe = self.recipe
        donor = self.work / "donor" / "donor.map"
        donor.parent.mkdir(parents=True, exist_ok=True)
        if donor.exists():
            donor.unlink()
        arguments = list(recipe["tools"]["invader"]["build_arguments"])
        for layer in recipe["layers"]:
            arguments += ["-t", self.layers / layer]
        arguments += ["-t", self.cmt, "-t", self.stock, "-o", donor, recipe["donor"]["scenario"]]
        output = self.programs.run("invader-build", *arguments)
        (self.work / "donor" / "build.log").write_text("ffmpeg: %s\ninvader: %s\n%s" % (
            self.ffmpeg_version, recipe["tools"]["invader"]["version"], output))
        digest = sha256_file(donor)
        log("build: donor %s, %d bytes, sha256 %s" % (donor, donor.stat().st_size, digest))
        if not recipe["expected"].get("donor_sha256"):
            log("build: the recipe records no donor yet: record its sha256 and size in \"expected\"")
        elif digest != recipe["expected"]["donor_sha256"]:
            donor.rename(donor.with_suffix(".refused"))
            raise Refused("the donor is not the recipe's (SHA-256 %s, not %s): no pack is built from it" % (
                digest, recipe["expected"]["donor_sha256"]))
        return donor

    def sources(self):
        """(every tag file the donor's build can take, the stock tags' own): see resolve_sources"""
        order = [(name, self.layers / name) for name in self.recipe["layers"]] + [("cmt", self.cmt),
                                                                                  ("stock", self.stock)]
        return resolve_sources(order), index_tags(self.stock)

    def build_pack(self, donor):
        recipe, options = self.recipe, self.options
        if not self.programs.builder or not self.programs.builder.is_file():
            raise Refused("--builder: no ae_pack_build (ninja linux builds it)")
        maps = Path(options.stock_maps).expanduser()
        stock_maps = [maps / (name + ".map") for name in STOCK_MULTIPLAYER_MAPS]
        for path in stock_maps:
            if not path.is_file():
                raise Refused("no stock multiplayer map %s" % path)
        names = self.work / "pack" / "stock-names.txt"
        names.parent.mkdir(parents=True, exist_ok=True)
        self.programs.run(self.programs.builder, "--list-names", "--out", names, *stock_maps)
        pack = self.work / "pack" / (recipe["pack"]["id"] + ".aepack")
        if pack.exists():
            pack.unlink()
        output = self.programs.run(self.programs.builder,
                                   *builder_arguments(recipe, donor, names, pack, self.recipe_folder))
        (self.work / "pack" / "build.log").write_text(output)
        try:
            self.check_pack(pack, donor, output, stock_maps)
        except (Refused, aepack.Refused, donor_map.Refused):
            # (whatever refuses it once the builder has written it: it does not stay under a pack's name)
            if pack.exists():
                pack.rename(pack.with_suffix(".refused"))
            raise
        return pack

    def check_pack(self, pack, donor, output, stock_maps):
        recipe = self.recipe
        require_no_corrections(output)
        image = pack.read_bytes()
        parsed = aepack.read(image)
        strings = parsed.strings

        def text(offset):
            return strings[offset:strings.index(b"\0", offset)].decode("utf-8")

        build, stock = self.sources()
        audit, externals = [], []
        for record in parsed.tags:
            group = struct.pack(">I", record[0]).decode("latin-1")
            if record[4] == aepack.OWN:
                donor_name = text(record[8])
                key = (group, donor_name.lower())
                if key not in build:
                    raise Refused("an own tag no folder of the build has: %s %s" % key)
                root, path = build[key]
                tag = str(path.relative_to(self.root(root))).replace(os.sep, "/")
                if root == "cmt":
                    source, inputs = tag, []
                elif root == "stock":
                    source, inputs = STOCK_PREFIX + tag, []
                else:
                    source, inputs = provenance(recipe, root, tag)
                audit.append((text(record[3]), source, group, record[10], donor_name, root, "; ".join(inputs)))
            else:
                externals.append((group, text(record[3]), record[5]))
        write_table(self.work / "audit.tsv", ("pack_name", "source", "group", "size", "donor_name", "layer",
                                              "inputs"), audit)
        problems = audit_problems([{"pack_name": row[0], "source": row[1], "group": row[2], "size": row[3],
                                    "inputs": row[6].split("; ") if row[6] else []} for row in audit],
                                  set(recipe["source"]["allowlist"]), scripts(),
                                  set(recipe["pack"]["stock_derived"]))
        # externals: each the stock map's own tag, unchanged, and on every stock multiplayer map
        fingerprints = {key: sha256_file(path) for key, (_, path) in build.items()
                        if key in {(group, name.lower()) for group, name, _ in externals}}
        stock_fingerprints = {key: sha256_file(path) for key, path in stock.items() if key in fingerprints}
        problems += external_problems([(group, name.lower()) for group, name, _ in externals], fingerprints,
                                      stock_fingerprints)
        on_maps = {}
        for path in stock_maps:
            have = set(donor_map.Tags(donor_map.read_map(path)).names())
            for group, name, _ in externals:
                if (group, name.lower()) in have:
                    on_maps[(group, name)] = on_maps.get((group, name), 0) + 1
        rows = []
        for group, name, required in externals:
            key = (group, name.lower())
            same = key in fingerprints and key in stock_fingerprints and fingerprints[key] == stock_fingerprints[key]
            count = on_maps.get((group, name), 0)
            rows.append((group, name, required, "yes" if same else "no", count))
            if count != len(stock_maps):
                problems.append("%s %s: on %d of the %d stock multiplayer maps" % (group, name, count,
                                                                                    len(stock_maps)))
        write_table(self.work / "externals.tsv", ("group", "name", "required", "same_as_stock", "maps"), rows)
        report = pack_compare.compare_pack(image, donor_map.MapFile(donor))
        problems += report["problems"]
        digest = hashlib.sha256(image).hexdigest()
        log("build: pack %s, %d bytes, sha256 %s (the pack's own: %s): %d own tags, %d external" % (
            pack, len(image), digest, parsed.sha256.hex(), len(audit), len(externals)))
        log("build: against its donor: " + pack_compare.summary(report))
        log("build: not compared: " + ", ".join("%s %d" % item for item in report["masked"].items()))
        if (report["resources"], report["parts"]) != (len(parsed.resources), len(parsed.parts)):
            problems.append("not every resource and model part was compared")
        problems += self.check_departures(image)
        if problems:
            for problem in problems[:40]:
                log("  " + problem)
            raise Refused("%d problems with the pack (above): it is set aside as %s" % (
                len(problems), pack.with_suffix(".refused").name))
        expected = recipe["expected"]
        if expected.get("pack_sha256"):
            if (digest, parsed.sha256.hex(), len(image), len(audit), len(externals)) != (
                    expected["pack_sha256"], expected["pack_own_sha256"], expected["pack_size"],
                    expected["own_tags"], expected["external_tags"]):
                raise Refused("the pack is not the recipe's recorded one (SHA-256 %s, not %s)" % (
                    digest, expected["pack_sha256"]))
            log("build: the pack is the recipe's recorded one")
        else:
            log("build: the recipe records no pack yet: record sha256, size and tag counts in its \"expected\"")

    def check_departures(self, image):
        """the pack against the APPROVED donor (another build of it): it may differ only in the tags the recipe
        names; departures.tsv lists every difference. Returns the problems"""
        approved = self.recipe["approved_reference"]
        if not self.options.approved_donor:
            log("build: no --approved-donor: the pack is not compared with the approved donor")
            return []
        path = Path(self.options.approved_donor).expanduser()
        if sha256_file(path) != approved["donor_sha256"]:
            raise Refused("--approved-donor is not the approved donor (SHA-256): %s" % path)
        report = pack_compare.compare_pack(image, donor_map.MapFile(path), same_file=False)
        rows = [(name, "departure" if name in approved["pack_departures"] else "NOT NAMED", len(texts), text)
                for name, texts in sorted(report["by_tag"].items()) for text in texts]
        write_table(self.work / "departures.tsv", ("donor_tag", "named", "differences", "difference"), rows)
        log("build: against the approved donor: " + pack_compare.summary(report))
        log("build: %d tags differ from the approved donor's: %s" % (
            len(report["by_tag"]), ", ".join(sorted(report["by_tag"]))))
        return pack_departure_problems(report["by_tag"], approved["pack_departures"])


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--recipe", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--zip")
    parser.add_argument("--stock-map")
    parser.add_argument("--stock-maps", help="the stock multiplayer maps' folder (default: --stock-map's)")
    parser.add_argument("--invader", default="invader")
    parser.add_argument("--library-path")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--builder")
    parser.add_argument("--reference", help="the folder holding the prototype's fix folders (read only)")
    parser.add_argument("--approved-donor", help="the approved donor map the recipe's departures are measured from")
    parser.add_argument("--stage", choices=("prepare", "art", "build", "all"), default="all")
    options = parser.parse_args(arguments)
    try:
        with open(options.recipe) as file:
            recipe = json.load(file)
        check_recipe(recipe)
        check_work_folder(Path(options.work).expanduser())
        if options.stock_map and not options.stock_maps:
            options.stock_maps = str(Path(options.stock_map).expanduser().parent)
        run = Run(options, recipe)
        claim_work_folder(run.work, options.stage)
        if options.stage in ("prepare", "all"):
            run.prepare()
        if options.stage == "art" or (options.stage == "all" and not _art_ready(run)):
            run.make_art()
        if options.stage in ("build", "all"):
            run.programs.check()
            if not options.stock_maps:
                raise Refused("--stock-maps (or --stock-map) is needed")
            run.make_layers()
            run.build_pack(run.build_donor())
    except (Refused, hek.Refused, extract_allowlist.Refused, donor_map.Refused, tag_fields.Failed, aepack.Refused,
            png_pixels.Refused) as refusal:
        print("make_pack: refused: %s" % refusal, file=sys.stderr)
        return 1
    except KeyError as missing:
        print("make_pack: refused: the recipe has no %s where one is needed" % missing, file=sys.stderr)
        return 1
    except (OSError, zipfile.BadZipFile, ValueError) as failure:
        print("make_pack: refused: %s: %s" % (type(failure).__name__, failure), file=sys.stderr)
        return 1
    return 0


def _art_ready(run):
    """the art is already there and the recipe's (drawn on another machine and brought back)"""
    try:
        run.check_art()
    except Refused:
        return False
    return True


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
