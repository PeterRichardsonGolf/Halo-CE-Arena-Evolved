"""The gravity hammer pack's recipe, tools/weapon_pack/gravity_hammer/recipe.json: a recipe whose source is a Custom
Edition map's tags (grifball.map, Grifball Beta by SlappyThePirate): the recipe's own rules (no prototype), the
allowlist exact (the hammer's own tags and what they name that no stock map has), every tag renamed to the pack's own
path (hammer/...), the pack's arguments, texts and credits, the Xbox conversions (models, the multipurpose bitmap, the
Ogg sound, the HUDs, the message indices), AE's numbers on the swing (the blast, the reach) and the melee blow, its rumble, and the
prompts' picture (the map's own picture of the hammer, a sequence of its HUD's icon sheet). Also make_pack.py's Custom
Edition map source, its renames and hek.py's 16-bit sheets. Everywhere (CI): no game file is read; a run of the recipe
is the box's (make_pack.py --source-map, docs/weapon-packs.md)."""
import json
import re
import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import hek  # noqa: E402
import make_pack  # noqa: E402

FOLDER = TOOLS / "gravity_hammer"
WEAPON = "hammer/gravity_hammer.weapon"
BLAST = "hammer/effects/push.damage_effect"
MELEE = "hammer/effects/melee.damage_effect"
SHIELD, BODY = 75.0, 75.0


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(data, tag):
    return [entry for entry in data["steps"] if entry["tag"] == tag][0]


def edits(data, tag):
    return {edit[1]: edit[2] for edit in step(data, tag)["edits"] if edit[0] == "set"}


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "map"
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    source = data["source"]
    assert set(source["allowlist"]) == make_pack.recipe_inputs(data)["cmt"] | set(source["verbatim"])
    assert source["allowlist"] == sorted(set(source["allowlist"]))
    assert source["file"] == "grifball.map" and source["tags"] == 2369
    assert re.fullmatch(r"[0-9a-f]{64}", source["sha256"])
    stock = {name for entry in data["steps"] for root, name in make_pack.step_inputs(entry) if root == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals",
                     "ui/hud/default.hud_globals"}
    assert data["pack"]["stock_derived"] == {}
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert (data["expected"]["own_tags"], data["expected"]["external_tags"]) == (28, 8)


def test_every_tag_has_the_packs_own_path():
    """the map's 26 tags, each renamed from the map's folders to hammer/ (Invader's refactor moves the tag and every
    reference to it; nothing else of a tag changes): the allowlist, the steps and the pack name only the pack's paths"""
    data = recipe()
    source = data["source"]
    renames = source["renames"]
    assert sorted(renames) == source["allowlist"] and len(renames) == 26
    assert all(name.startswith("hammer/") for name in source["allowlist"])
    assert len(set(renames.values())) == len(renames)
    assert all(new.rsplit(".", 1)[1] == old.rsplit(".", 1)[1] for new, old in renames.items())
    assert make_pack.RENAME_PROGRAM in data["tools"]["invader"]["sha256"]
    # (the map's own folder names are only the renames' sources and the about's mapping table)
    names = json.dumps([data["pack"], data["steps"], source["allowlist"], source["verbatim"], data["art_inputs"]])
    assert not re.search(r"halo3|h3mt|\bh3 ", names)
    assert all(old in data["about"] for old in renames.values())
    assert "shipped unchanged, to everyone" in data["about"]


def test_renames_are_checked():
    data = recipe()
    broken = json.loads(json.dumps(data))
    broken["source"]["renames"]["hammer/not_listed.bitmap"] = "x/y.bitmap"
    with pytest.raises(make_pack.Refused, match="allowlist does not have"):
        make_pack.check_recipe(broken)
    broken = json.loads(json.dumps(data))
    first = sorted(broken["source"]["renames"])[0]
    broken["source"]["renames"][first] = broken["source"]["renames"][first].rsplit(".", 1)[0] + ".sound"
    with pytest.raises(make_pack.Refused, match="group"):
        make_pack.check_recipe(broken)
    broken = json.loads(json.dumps(data))
    del broken["tools"]["invader"]["sha256"][make_pack.RENAME_PROGRAM]
    with pytest.raises(make_pack.Refused, match=make_pack.RENAME_PROGRAM):
        make_pack.check_recipe(broken)
    assert make_pack.source_names(data) == [data["source"]["renames"][name] for name in data["source"]["allowlist"]]


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_gravity_hammer", "ae\\packs\\gravity_hammer\\", "gravity_hammer", "GRAVITY HAMMER")
    assert (pack["labels"], pack["behaviour"], pack["replaces"], pack["melee"]) == ("gh,rl,ar", 0, 0, True)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    # (a community model, shipped to every player: no MCC gate)
    assert pack.get("kind") != "direct_port"
    arguments = make_pack.builder_arguments(data, "donor.map", "names.txt", "out.aepack", FOLDER)
    assert "--direct-port" not in arguments and "--smooth" not in arguments
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a gravity hammer\n" in texts and "%" not in texts
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("Gravity hammer from Grifball Beta (grifball.map) by SlappyThePirate, 2011",
                 "the hammer art's original maker is not", "Kind: community model"):
        assert name in " ".join(credits.split())


def test_the_weapon_is_the_maps_and_only_the_xbox_conversions_are_made():
    data = recipe()
    for model in ("3p", "fp/fp"):
        entry = step(data, "hammer/%s.model" % model)
        assert entry["op"] == "model" and entry["from"] == ["source", "hammer/%s.gbxmodel" % model]
    assert step(data, "hammer/bitmaps/hammer_multi.bitmap")["op"] == "multipurpose"
    assert step(data, "hammer/sounds/melee/melee.sound")["op"] == "sound"
    assert {e["tag"] for e in data["steps"] if e["op"] == "hud_hires"} == {
        "hammer/hud/master plasma.weapon_hud_interface", "hammer/hud/hammer.weapon_hud_interface"}
    # (everything else of the map's is taken as it is: the textures, the HUD's bitmaps, the effect, the particles)
    edited = {e["tag"] for e in data["steps"]}
    assert not set(data["source"]["verbatim"]) & edited
    assert {"hammer/effects/explosion old.effect", "hammer/hud/bitmaps/weapon icons.bitmap",
            "hammer/fp/fp.model_animations"} <= set(data["source"]["verbatim"])
    weapon = edits(data, WEAPON)
    assert set(weapon) == {"model", "first_person_model", "pickup_text_index", "hud_text_message_index",
                           "autoaim_range", "magnetism_range", "triggers[0].maximum_rate_of_fire", "ready_time",
                           "player_melee_damage", "scale"}
    assert weapon["scale"] == "1", "the map's 0.001 left the hammer all but invisible on the ground"
    assert (weapon["model"], weapon["first_person_model"]) == ("hammer/3p.model", "hammer/fp/fp.model")
    assert (weapon["pickup_text_index"], weapon["hud_text_message_index"]) == ("4", "0")


def test_the_swing_numbers():
    """the classic-era hammer's swing in a game with no sprint: the reach 2.8 units (0.70 of the sword pack's 3.5, the classic
    hammer's reach over the sword's, lengthened by an eighth), one swing every 1.43 s (the map's fire animation is 43 frames; the
    classic swing is 1.27 s), the reticle red inside half the autoaim range; the blast: a core of 1.3 units and an edge of 2.6 (the
    classic 0.75 and 1.5 times 1.73), damage 255 so that a full shield and health (150) die out to the classic 41 percent of the way
    from the core to the edge, the shield stripped out to about 2.2, nothing past 2.6; the swinger never hurt"""
    data = recipe()
    weapon = edits(data, WEAPON)
    reach = 2.45 * 8 / 7  # 0.70 of the sword pack's 3.5, lengthened by an eighth
    assert abs(float(weapon["magnetism_range"]) - reach) < 0.05
    assert float(weapon["autoaim_range"]) == 2 * float(weapon["magnetism_range"])
    assert weapon["triggers[0].maximum_rate_of_fire"] == "0.7 0.7"
    assert abs(1 / 0.7 - 43 / 30) < 0.01  # (the map's 43-frame swing at 30 frames a second, to a hundredth)
    assert weapon["ready_time"] == "1"
    assert weapon["player_melee_damage"] == MELEE
    blast = edits(data, BLAST)
    core, edge = (float(value) for value in blast["radius"].split())
    assert (core, edge) == (1.3, 2.6)
    damage = float(blast["damage_lower_bound"])
    assert damage == 255.0 and blast["damage_upper_bound"] == "255 255"
    assert damage > SHIELD + BODY

    def strike(distance):
        """the damage at a distance: full inside the core, falling to nothing at the edge"""
        return damage if distance <= core else damage * max(0.0, (edge - distance) / (edge - core))

    # (the classic blast kills out to 41 percent of the way from the core to the edge: here 1.83 units, the old kill radius)
    assert strike(1.8) >= SHIELD + BODY > strike(1.9)
    assert abs((1.83 - core) / (edge - core) - 0.41) < 0.01
    # (the shield is stripped, the body spared, a little past that; nothing at the edge)
    assert strike(2.1) >= SHIELD and strike(2.3) < SHIELD and strike(edge) == 0
    # (the classic blast's shape scaled: the core to the edge keeps the classic ratio of one to two)
    assert abs(edge / core - 2.0) < 1e-9
    # (the map's "does not hurt owner" is kept: no edit touches the flags)
    assert not any(key.startswith("damage_flags") for key in blast)
    # (the push is the classic's 3.5 of the latest's 7.5 times the map's 8: 4)
    assert float(blast["damage_instantaneous_acceleration"]) == 4.0


def test_the_melee_buttons_blow():
    """the melee button is a blow of the hammer: its own damage tag (the blast's tag the map has, edited a second time under
    its own name), 175 to the one it lands on, a kill against a full shield and health, the same push and camera shake"""
    data = recipe()
    entry = step(data, MELEE)
    assert entry["op"] == "edit" and entry["from"] == ["source", "hammer/effects/push.damage_effect"]
    melee = edits(data, MELEE)
    assert float(melee["damage_lower_bound"]) == 175.0 and melee["damage_upper_bound"] == "175 175"
    assert float(melee["damage_lower_bound"]) > SHIELD + BODY
    blast = edits(data, BLAST)
    for key in ("damage_instantaneous_acceleration", "temporary_camera_impulse_duration", "camera_shaking_duration"):
        assert melee[key] == blast[key]
    assert not any(key.startswith(("radius", "damage_flags")) for key in melee)
    # (the weapon's player melee damage names it, and the blast's step is made before the melee blow's)
    assert edits(data, WEAPON)["player_melee_damage"] == MELEE
    order = [item["tag"] for item in data["steps"]]
    assert order.index(BLAST) < order.index(MELEE)


def test_haptics_and_the_prompts_picture():
    data = recipe()
    blast = edits(data, BLAST)
    assert float(blast["low_frequency_vibrate_frequency"]) > 0 and float(blast["high_frequency_vibrate_frequency"]) > 0
    pack = data["pack"]
    assert pack["icon"] == "ae_pack\\hud\\hammer_icon:0" and pack["roots"] == ["bitm:ae_pack\\hud\\hammer_icon"]
    drawing = data["art_drawings"]["hammer_icon.png"]
    assert drawing == {"draw": "source_icon", "input": "hammer_icons.bitmap", "width": 100, "sequence": 8}
    entry = data["art_inputs"]["hammer_icons.bitmap"]
    assert (entry["kind"], entry["root"], entry["tag"]) == ("tag", "source", "hammer/hud/bitmaps/weapon icons.bitmap")
    assert re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])
    assert re.fullmatch(r"[0-9a-f]{64}", data["art"]["hammer_icon.png"]["pixels_sha256"])
    hud = step(data, "ae_pack/donor/hud.hud_globals")
    assert hud["edits"] == [["set", "icon_bitmap", "ae_pack\\hud\\hammer_icon.bitmap"]]


def test_a_custom_edition_map_is_taken_as_it_is(tmp_path):
    tags = 0x100
    image = bytearray(0x800 + 0x100)
    image[0:4] = b"daeh"
    struct.pack_into("<II", image, 4, make_pack.CE_MAP_VERSION, len(image))
    struct.pack_into("<II", image, 0x10, 0x800, 0x100)
    struct.pack_into("<I", image, 0x800 + 0xC, tags)
    path = tmp_path / "ce.map"
    path.write_bytes(bytes(image))
    copy, added, count = make_pack.map_source_copy(path)
    assert (copy, added, count) == (bytes(image), 0, tags)
    path.write_bytes(bytes(image) + b"x")
    with pytest.raises(make_pack.Refused, match="Custom Edition"):
        make_pack.map_source_copy(path)


def bitmap_tag(pictures, sequences):
    """a bitmap tag of 16-bit A4R4G4B4 pictures (each (width, height, [texel values])), with sequences of one bitmap"""
    pixels, records, offset = b"", b"", 0
    for width, height, texels in pictures:
        data = b"".join(struct.pack("<H", value) for value in texels)
        records += b"bitm" + struct.pack(">HHHHHH", width, height, 1, 0, hek.FORMAT_A4R4G4B4, 0) + bytes(8) + \
            struct.pack(">I", offset) + bytes(48 - 28)
        pixels += data
        offset += len(data)
    root = bytearray(hek.BITMAP_ROOT_SIZE)
    struct.pack_into(">i", root, hek.BITMAP_PIXELS_SIZE, len(pixels))
    struct.pack_into(">i", root, hek.BITMAP_SEQUENCE_COUNT, len(sequences))
    struct.pack_into(">i", root, hek.BITMAP_COUNT, len(pictures))
    header = bytearray(hek.HEADER_SIZE)
    header[0x24:0x28] = b"bitm"
    sequence_records = b"".join(bytes(32) + struct.pack(">hh", first, 1) + bytes(28) for first in sequences)
    return bytes(header) + bytes(root) + pixels + sequence_records + records


def test_a_sheets_sequence_picture_is_decoded():
    """hek.py reads a 16-bit A4R4G4B4 picture, and a sheet's sequence by its first bitmap (the icon sheet holds a
    picture per weapon, one a sequence)"""
    tag = bitmap_tag([(1, 1, [0xF000]), (2, 1, [0x8F00, 0xF0A5])], [1, 0])
    assert hek.first_bitmap_rgba(tag) == (1, 1, bytes((0, 0, 0, 255)))
    width, height, rgba = hek.first_bitmap_rgba(tag, 0)
    assert (width, height) == (2, 1)
    assert rgba == bytes((255, 0, 0, 136, 0, 170, 85, 255))
    assert hek.first_bitmap_rgba(tag, 1)[:2] == (1, 1)
    with pytest.raises(hek.Refused, match="no sequence"):
        hek.first_bitmap_rgba(tag, 2)
