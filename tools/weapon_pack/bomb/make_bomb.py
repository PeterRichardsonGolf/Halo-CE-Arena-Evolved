"""Builds the Search and Destroy bomb pack (ae_bomb.aepack) from its recipe: the bomb's model and art (our own
geometry; its textures baked from the game's own stock textures at build time), our own pickup chirp, CMT's blast
effects (TSC:E 1.4.1c source release) and the stock maps of the player's disc.

    make_bomb.py --recipe tools/weapon_pack/bomb/recipe.json --work <folder> --stage art
                 --hek-tags <the official HEK's tags> (--zip <CMT's zip> | --tsce-tags <folder of its tags>)
    make_bomb.py --recipe ... --work <folder> --stage build --zip <CMT's zip> --stock-map <bloodgulch.map>
                 [--stock-maps <folder of the 13 stock multiplayer maps>] --invader <Invader's build folder>
                 [--library-path <folder>] --builder <ae_pack_build> [--data <the art stage's data folder>]

Two stages, because the art needs numpy, SciPy and Pillow and the build needs Invader (the machine that has
the one often lacks the other; the art's folder is carried across):
  art    the mockup's geometry (mockup/export_jms.py) baked into data/: the model's JMS, shaders.json and one
         PNG a material, the three flat multipurpose maps and the pickup chirp (gen_pickup.py). Every input
         file (the HEK's and CMT's textures and models the mockup reads) is checked against the recipe's
         SHA-256, and every output against the recipe's pins (PNGs by their decoded pixels, the rest by bytes).
  build  Invader makes the bomb's tags (bitmaps, sound, model, the shaders from the stock rocket ammo shader),
         CMT's two blast effects are edited, stripped of Open Sauce's blocks and put in a layer with the effects'
         closure, a donor map is built, and ae_pack_build makes the pack from it, which is audited (every own
         tag from the allowlist, our scripts or a tag the recipe names stock-derived), compared with its donor and
         checked against the recipe's recorded pack.

Nothing of CMT's, of the game's or built from them is written anywhere but --work. The work folder is one this
made (marked), on a disk (not /tmp).
"""
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import types
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))    # (tools/weapon_pack: found when run with python -I, too)
import aepack
import donor_map
import extract_allowlist
import make_pack
import os_strip
import pack_compare
import png_pixels
import tag_fields

Refused = make_pack.Refused
log = make_pack.log
sha256_file = make_pack.sha256_file
# the tag groups Open Sauce's extension blocks are stripped from (as vehicle_damage's recipe does)
STRIPPED_GROUPS = ("particle", "particle_system", "shader_model")
DATA = "ae_pack/bomb/"       # the bomb's tags and data below the tags folder
SHADER_SOURCE = "powerups/rocket launcher ammo/shaders/rl ammo body.shader_model"
HOLDER_SOURCE = "weapons/frag grenade/effects/explosion.effect"
SCENERY_SOURCE = "scenery/small beacon/small red beacon.scenery"
SCENARIO_SOURCE = "levels/test/bloodgulch/bloodgulch.scenario"
GLOBALS_SOURCE = "globals/globals.globals"
# the multipurpose maps (red channel, green = self-illumination, as the stock rocket ammo shader reads them):
# flat pictures of our own, written by the art stage
FLAT_MULTIPURPOSE = {"mp_plain": (8, [(70, 0, 0, 255)] * 8), "mp_glow": (8, [(40, 255, 0, 255)] * 8),
                     "mp_bar": (16, [(70, 196, 0, 255)] * 4 + [(70, 255, 0, 255)] * 8 + [(70, 196, 0, 255)] * 4)}


def load_recipe(path):
    with open(path) as file:
        recipe = json.load(file)
    for key in ("pack", "source", "stock_map", "tools", "inputs", "art", "blast", "donor", "expected"):
        if key not in recipe:
            raise Refused("the recipe has no %s" % key)
    return recipe


# ---------- the art stage

def check_inputs(recipe, hek, tsce):
    """each file the mockup reads: present, and the recipe's (SHA-256). Returns nothing, or refuses"""
    for kind, root in (("hek", hek), ("tsce", tsce)):
        for name, digest in recipe["inputs"][kind].items():
            path = Path(root) / name
            if not path.is_file():
                raise Refused("no %s input: %s" % (kind, name))
            if sha256_file(path) != digest:
                raise Refused("%s input %s is not the recipe's (SHA-256)" % (kind, name))
    log("art: %d inputs checked" % (len(recipe["inputs"]["hek"]) + len(recipe["inputs"]["tsce"])))


def write_flat_png(path, size, row):
    """one of the flat multipurpose maps: `size` wide and tall (rows alike), `row` its pixels' RGBA, repeated"""
    import zlib
    pixels = row if len(row) == size else row * (size // len(row))
    raw = b"".join(b"\0" + bytes(value for pixel in pixels for value in pixel) for _ in range(size))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png_pixels.SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def check_outputs(recipe, data):
    """every art file the stage makes is the recipe's and no other is there"""
    root = Path(data) / "ae_pack" / "bomb"
    pins = recipe["art"]
    have = {str(path.relative_to(root)).replace(os.sep, "/") for path in root.rglob("*") if path.is_file()}
    if have != set(pins):
        raise Refused("the art files are not the recipe's: %s" % ", ".join(sorted(have ^ set(pins))[:6]))
    for name, pin in sorted(pins.items()):
        path = root / name
        if "sha256" in pin:
            if sha256_file(path) != pin["sha256"]:
                raise Refused("art %s is not the recipe's (SHA-256)" % name)
            continue
        try:
            seen = png_pixels.pin(path)
        except (OSError, png_pixels.Refused) as failure:
            raise Refused("art %s cannot be read: %s" % (name, failure))
        if (seen["mode"], seen["size"], seen["pixels_sha256"]) != (pin["mode"], list(pin["size"]),
                                                                      pin["pixels_sha256"]):
            raise Refused("art %s has other pixels than the recipe's (mode, size, decoded pixels)" % name)
    log("art: %d files, each the recipe's" % len(pins))


def art_stage(recipe, options, work):
    try:
        import numpy      # noqa: F401
        import scipy      # noqa: F401
        import PIL        # noqa: F401
    except ImportError:
        raise Refused("the art needs numpy, SciPy and Pillow: run --stage art on a machine that has them and "
                      "bring its data/ folder to the build with --data")
    if not options.hek_tags:
        raise Refused("--hek-tags is needed (the official HEK's tags folder)")
    tsce = Path(options.tsce_tags).expanduser() if options.tsce_tags else None
    if options.zip:
        names = sorted(recipe["inputs"]["tsce"])
        tsce = work / "tsce-tags"
        if tsce.exists():
            shutil.rmtree(tsce)
        extract_allowlist.extract(Path(options.zip).expanduser(), [recipe["source"]["root"] + name for name in names],
                                  tsce, strip=recipe["source"]["root"], sha256=recipe["source"]["sha256"])
    if tsce is None:
        raise Refused("--zip or --tsce-tags is needed")
    hek = Path(options.hek_tags).expanduser()
    check_inputs(recipe, hek, tsce)
    data = work / "data"
    if data.exists():
        shutil.rmtree(data)
    data.mkdir(parents=True)
    environment = dict(os.environ, AE_BOMB_HEK_TAGS=str(hek), AE_BOMB_TSCE_TAGS=str(tsce), AE_BOMB_DATA=str(data),
                       AE_BOMB_SCRATCH=str(work / "scratch"))
    (work / "scratch").mkdir(exist_ok=True)
    result = subprocess.run([sys.executable, str(HERE / "mockup" / "export_jms.py")], env=environment,
                            capture_output=True, text=True)
    if result.returncode:
        raise Refused("the mockup's export failed: %s" % result.stderr.strip().splitlines()[-1:])
    log("art: " + result.stdout.strip())
    root = data / "ae_pack" / "bomb"
    for name, (size, row) in FLAT_MULTIPURPOSE.items():
        write_flat_png(root / "bitmaps" / (name + ".png"), size, row)
    result = subprocess.run([sys.executable, str(HERE / "gen_pickup.py"), str(root / "sounds/pickup/pickup.wav")],
                            capture_output=True, text=True)
    if result.returncode:
        raise Refused("the pickup chirp failed: %s" % result.stderr.strip().splitlines()[-1:])
    check_outputs(recipe, data)


# ---------- the build stage

class Build:
    def __init__(self, recipe, options, work):
        self.recipe, self.options, self.work = recipe, options, work
        self.data = Path(options.data).expanduser().resolve() if options.data else work / "data"
        self.cmt, self.stock = work / "cmt-tags", work / "stock-tags"
        self.tags, self.layer, self.verbatim = work / "tags", work / "layer", work / "cmt"
        self.programs = make_pack.Programs(types.SimpleNamespace(
            invader=options.invader, library_path=options.library_path, ffmpeg="ffmpeg", builder=options.builder),
            recipe)
        self.donor_map = work / "donor" / "donor.map"

    def run(self, program, *arguments):
        return self.programs.run(program, *arguments)

    def edit(self, tags, tag, *arguments):
        self.run("invader-edit", "-t", tags, *arguments, tag)

    def prepare(self):
        recipe, options = self.recipe, self.options
        self.programs.check()
        model = Path(options.invader).expanduser() / "invader-model"
        if not model.is_file() or sha256_file(model) != recipe["tools"]["invader"]["model_sha256"]:
            raise Refused("invader-model is not the recipe's build of Invader (SHA-256)")
        for folder in (self.cmt, self.stock, self.tags, self.layer, self.verbatim, self.work / "donor",
                       self.work / "pack", self.work / "edit"):
            if folder.exists():
                shutil.rmtree(folder)
        stock_map = Path(options.stock_map).expanduser()
        if sha256_file(stock_map) != recipe["stock_map"]["sha256"]:
            raise Refused("the stock map is not the recipe's (SHA-256): %s" % stock_map)
        if not options.zip:
            raise Refused("--zip is needed")
        source = recipe["source"]
        written = extract_allowlist.extract(Path(options.zip).expanduser(),
                                            [source["root"] + name for name in source["allowlist"]], self.cmt,
                                            strip=source["root"], sha256=source["sha256"])
        log("prepare: %d of CMT's files unpacked (the allowlist, nothing else)" % len(written))
        self.stock.mkdir(parents=True)
        self.run("invader-extract", "-m", stock_map.parent, "-t", self.stock, stock_map)
        count = sum(1 for path in self.stock.rglob("*") if path.is_file())
        if count != recipe["stock_map"]["tags"]:
            raise Refused("%d tags extracted from the stock map, not %d" % (count, recipe["stock_map"]["tags"]))
        log("prepare: %d stock tags extracted" % count)
        check_outputs(recipe, self.data)

    def blast_tags(self):
        """CMT's blast tags: the edited effects, the stripped groups and the rest verbatim (the donor takes them
        from the layer, then the verbatim folder, then the stock tags)"""
        recipe = self.recipe
        edits = recipe["blast"]["edits"]
        counts = {"edited": 0, "os_strip": 0, "verbatim": 0}
        for tag in recipe["source"]["allowlist"]:
            source = self.cmt / tag
            group = tag.rsplit(".", 1)[1]
            if tag in edits:
                out = self.layer / tag
                out.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, out)
                try:
                    editor = self.programs.editor(self.layer, tag)
                    tag_fields.apply(editor, edits[tag])
                except tag_fields.Failed as failure:
                    raise Refused("%s: %s" % (tag, failure))
                counts["edited"] += 1
            elif group in STRIPPED_GROUPS:
                out = self.layer / tag
                out.parent.mkdir(parents=True, exist_ok=True)
                stripped, _, _ = os_strip.strip(group, source.read_bytes())
                out.write_bytes(stripped)
                counts["os_strip"] += 1
            else:
                out = self.verbatim / tag
                out.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, out)
                counts["verbatim"] += 1
        log("build: blast tags: %s" % counts)

    def bomb_tags(self):
        recipe = self.recipe
        tags, data = self.tags, self.data
        for sub in ("bomb/shaders", "bomb/bitmaps", "donor"):
            (tags / "ae_pack" / sub).mkdir(parents=True, exist_ok=True)
        pictures = sorted((data / "ae_pack/bomb/bitmaps").glob("*.png"))
        for picture in pictures:
            name = picture.stem
            fmt = "dxt1" if name.startswith("detail_") else "32-bit" if name.startswith("mp_") else "dxt5"
            self.run("invader-bitmap", "-F", fmt, "-d", data, "-t", tags, DATA + "bitmaps/" + name)
        # the pickup sound and an effect holding it for the donor's scenery (an attachment cannot be a sound; the
        # game plays the sound itself)
        self.run("invader-sound", "-d", data, "-t", tags, "-F", "xbox_adpcm", "-c", "game_event",
                 DATA + "sounds/pickup")
        holder = DATA + "sounds/pickup_holder.effect"
        shutil.copyfile(self.stock / HOLDER_SOURCE, tags / holder)
        self.edit(tags, holder, "-E", "events[*]", "-I", "events", "1", "end", "-I", "events[0].parts", "1", "end",
                  "-S", "events[0].parts[0].type", DATA + "sounds/pickup.sound")
        # the shaders: the stock rocket ammo shader's tag with the bomb's maps, one a material
        shaders = json.loads((data / "ae_pack/bomb/shaders.json").read_text())["shaders"]
        for shader in shaders:
            tag = DATA + "shaders/%s.shader_model" % shader["name"]
            shutil.copyfile(self.stock / SHADER_SOURCE, tags / tag)
            role = shader.get("role") or ""
            settings = [("base_map", shader["bitmap"].replace("\\", "/") + ".bitmap"),
                        ("multipurpose_map", DATA + "bitmaps/" + (
                            "mp_bar" if role.startswith("bar_") else "mp_glow" if shader["emissive"] else "mp_plain") +
                         ".bitmap"),
                        ("reflection_cube_map", ".bitmap"),
                        ("material_type", "metal_thin" if shader["tag"] in ("body", "timer", "beacon", None)
                         else "plastic")]
            if shader["detail"]:
                settings += [("detail_map", shader["detail"].replace("\\", "/") + ".bitmap"),
                             ("detail_map_scale", str(shader["detail_scale"]))]
            else:
                settings.append(("detail_map", ".bitmap"))
            settings += [("shader_model_flags.two_sided", "1" if shader["alpha_test"] else "0"),
                         ("shader_model_flags.not_alpha_tested", "0" if shader["alpha_test"] else "1")]
            if shader["emissive"]:
                # (the timer bars and the beacon: the game sets these each tick, ae_bomb_pack.c; here their carried
                # look)
                colour = "0.1 1 0.2" if role.startswith("bar_") else "0 0 0" if role == "beacon" else "1 0.1 0.04"
                settings += [("animation_color_lower_bound", colour), ("animation_color_upper_bound", colour)]
            arguments = []
            for key, value in settings:
                arguments += ["-S", key, value]
            self.edit(tags, tag, *arguments)
        # the model from the JMS
        (self.work / "model-data/ae_pack/bomb/bomb/models").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(data / "ae_pack/bomb/models/bomb.jms", self.work / "model-data/ae_pack/bomb/bomb/models/bomb.jms")
        self.run("invader-model", "-T", "model", "-d", self.work / "model-data", "-t", tags, "-t", self.stock,
                 "ae_pack/bomb/bomb")
        # the donor: a scenery holding the model and the blast effects and the holder as attachments, in Blood Gulch's
        # scenario without vehicles
        donor = tags / "ae_pack/donor"
        shutil.copyfile(self.stock / SCENERY_SOURCE, donor / "roots_1.scenery")
        attachments = recipe["donor"]["attachments"] + [DATA.rstrip("/") + "/sounds/pickup_holder.effect"]
        self.edit(tags, "ae_pack/donor/roots_1.scenery", "-E", "attachments[*]", "-S", "model",
                  "ae_pack/bomb/bomb.model")
        arguments = ["-I", "attachments", str(len(attachments)), "end"]
        for index, attachment in enumerate(attachments):
            arguments += ["-S", "attachments[%d].type" % index, attachment]
        self.edit(tags, "ae_pack/donor/roots_1.scenery", *arguments)
        shutil.copyfile(self.stock / SCENARIO_SOURCE, donor / "donor.scenario")
        count = self.run("invader-edit", "-t", tags, "-C", "scenery_palette", "ae_pack/donor/donor.scenario").strip()
        self.edit(tags, "ae_pack/donor/donor.scenario", "-E", "vehicles[*]", "-E", "vehicle_palette[*]")
        self.edit(tags, "ae_pack/donor/donor.scenario", "-I", "scenery_palette", "1", "end", "-S",
                  "scenery_palette[%s].name" % count, "ae_pack/donor/roots_1.scenery")
        (tags / "globals").mkdir(exist_ok=True)
        shutil.copyfile(self.stock / GLOBALS_SOURCE, tags / "globals/globals.globals")
        self.edit(tags, "globals/globals.globals", "-E", "multiplayer_information[0].vehicles[*]")

    def build_donor(self):
        self.donor_map.parent.mkdir(parents=True, exist_ok=True)
        arguments = list(self.recipe["tools"]["invader"]["build_arguments"])
        for folder in (self.tags, self.layer, self.verbatim, self.stock):
            arguments += ["-t", folder]
        output = self.run("invader-build", *arguments, "-o", self.donor_map, "ae_pack\\donor\\donor")
        (self.work / "donor" / "build.log").write_text("invader: %s\n%s" % (
            self.recipe["tools"]["invader"]["version"], output))
        digest = sha256_file(self.donor_map)
        log("build: donor %d bytes, sha256 %s" % (self.donor_map.stat().st_size, digest))
        expected = self.recipe["expected"].get("donor_sha256")
        if not expected:
            log("build: the recipe records no donor yet: record its sha256 in \"expected\"")
        elif digest != expected:
            self.donor_map.rename(self.donor_map.with_suffix(".refused"))
            raise Refused("the donor is not the recipe's (SHA-256 %s, not %s)" % (digest, expected))

    def build_pack(self):
        recipe, options = self.recipe, self.options
        if not self.programs.builder or not self.programs.builder.is_file():
            raise Refused("--builder: no ae_pack_build")
        maps = Path(options.stock_maps or Path(options.stock_map).expanduser().parent).expanduser()
        stock_maps = [maps / (name + ".map") for name in make_pack.STOCK_MULTIPLAYER_MAPS]
        for path in stock_maps:
            if not path.is_file():
                raise Refused("no stock multiplayer map %s" % path)
        names = self.work / "pack" / "stock-names.txt"
        names.parent.mkdir(parents=True, exist_ok=True)
        self.run(self.programs.builder, "--list-names", "--out", names, *stock_maps)
        pack = self.work / "pack" / (recipe["pack"]["id"] + ".aepack")
        output = self.run(self.programs.builder, *make_pack.builder_arguments(recipe, self.donor_map, names, pack, HERE))
        (self.work / "pack" / "build.log").write_text(output)
        try:
            self.check_pack(pack, output, stock_maps)
        except (Refused, aepack.Refused, donor_map.Refused):
            if pack.exists():
                pack.rename(pack.with_suffix(".refused"))
            raise
        return pack

    def check_pack(self, pack, output, stock_maps):
        recipe = self.recipe
        make_pack.require_no_corrections(output)
        image = pack.read_bytes()
        parsed = aepack.read(image)
        strings = parsed.strings

        def text(offset):
            return strings[offset:strings.index(b"\0", offset)].decode("utf-8")

        order = [("bomb", self.tags), ("blast", self.layer), ("cmt", self.verbatim), ("stock", self.stock)]
        build = make_pack.resolve_sources(order)
        stock = make_pack.index_tags(self.stock)
        allowlist, derived = set(recipe["source"]["allowlist"]), set(recipe["pack"]["stock_derived"])
        audit, externals, problems = [], [], []
        for record in parsed.tags:
            group = struct.pack(">I", record[0]).decode("latin-1")
            if record[4] != aepack.OWN:
                externals.append((group, text(record[3]), record[5]))
                continue
            donor_name = text(record[8])
            key = (group, donor_name.lower())
            if key not in build:
                raise Refused("an own tag no folder of the build has: %s %s" % key)
            root, path = build[key]
            relative = str(path.relative_to(dict(order)[root])).replace(os.sep, "/")
            pack_name = text(record[3])
            audit.append((pack_name, root, relative, group, record[10]))
            if root == "stock":
                problems.append("%s: a stock tag made the pack's own" % pack_name)
            elif root == "bomb":
                if relative.startswith("ae_pack/donor/"):
                    problems.append("%s: a donor-only tag in the pack" % pack_name)
            elif relative not in allowlist:
                problems.append("%s: not from the allowlist: %s" % (pack_name, relative))
        own_names = {row[0] for row in audit}
        problems += ["stock_derived names a tag the pack lacks: %s" % name for name in sorted(derived - own_names)]
        problems += ["%s: stock-derived and not named in the recipe" % row[0] for row in audit
                     if row[1] == "bomb" and row[3] in ("bitm", "soso", "effe") and row[0] not in derived
                     and row[0] not in recipe["pack"]["own_art"]]
        make_pack.write_table(self.work / "audit.tsv", ("pack_name", "layer", "source", "group", "size"), audit)
        wanted = {(group, name.lower()) for group, name, _ in externals}
        fingerprints = {key: sha256_file(path) for key, (_, path) in build.items() if key in wanted}
        stock_fingerprints = {key: sha256_file(path) for key, path in stock.items() if key in fingerprints}
        problems += make_pack.external_problems([(group, name.lower()) for group, name, _ in externals],
                                                fingerprints, stock_fingerprints)
        on_maps = {}
        for path in stock_maps:
            have = set(donor_map.Tags(donor_map.read_map(path)).names())
            for group, name, _ in externals:
                if (group, name.lower()) in have:
                    on_maps[(group, name)] = on_maps.get((group, name), 0) + 1
        rows = []
        for group, name, required in externals:
            count = on_maps.get((group, name), 0)
            rows.append((group, name, required, count))
            if count != len(stock_maps):
                problems.append("%s %s: on %d of the %d stock multiplayer maps" % (group, name, count,
                                                                                    len(stock_maps)))
        make_pack.write_table(self.work / "externals.tsv", ("group", "name", "required", "maps"), rows)
        report = pack_compare.compare_pack(image, donor_map.MapFile(self.donor_map))
        problems += report["problems"]
        digest = hashlib.sha256(image).hexdigest()
        log("build: pack %d bytes, sha256 %s (the pack's own: %s): %d own tags, %d external" % (
            len(image), digest, parsed.sha256.hex(), len(audit), len(externals)))
        log("build: against its donor: " + pack_compare.summary(report))
        if problems:
            for problem in problems[:40]:
                log("  " + problem)
            raise Refused("%d problems with the pack (above): it is set aside as .refused" % len(problems))
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


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--recipe", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--stage", choices=("art", "build", "all"), default="all")
    parser.add_argument("--zip")
    parser.add_argument("--tsce-tags", help="CMT's tags unpacked (art stage; instead of --zip)")
    parser.add_argument("--hek-tags", help="the official HEK's tags folder (art stage)")
    parser.add_argument("--stock-map")
    parser.add_argument("--stock-maps")
    parser.add_argument("--invader", default="invader")
    parser.add_argument("--library-path")
    parser.add_argument("--builder")
    parser.add_argument("--data", help="the art stage's data folder, when it was made elsewhere")
    options = parser.parse_args(arguments)
    try:
        recipe = load_recipe(options.recipe)
        work = Path(options.work).expanduser()
        make_pack.check_work_folder(work)
        make_pack.claim_work_folder(work, "all")
        if options.stage in ("art", "all"):
            art_stage(recipe, options, work)
        if options.stage in ("build", "all"):
            if not options.stock_map:
                raise Refused("--stock-map is needed")
            build = Build(recipe, options, work)
            build.prepare()
            build.blast_tags()
            build.bomb_tags()
            build.build_donor()
            build.build_pack()
    except (Refused, extract_allowlist.Refused, donor_map.Refused, tag_fields.Failed, aepack.Refused,
            png_pixels.Refused) as refusal:
        print("make_bomb: refused: %s" % refusal, file=sys.stderr)
        return 1
    except KeyError as missing:
        print("make_bomb: refused: the recipe has no %s where one is needed" % missing, file=sys.stderr)
        return 1
    except (OSError, ValueError) as failure:
        print("make_bomb: refused: %s: %s" % (type(failure).__name__, failure), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
