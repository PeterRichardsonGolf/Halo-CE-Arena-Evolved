"""The silenced assault rifle pack's recipe, tools/weapon_pack/ar_silenced/recipe.json: a recipe whose source is a
map's tags (the Halo 2 Mappack Collection v2's Gemini, h2d_gemini): the recipe's own rules (no prototype), the
allowlist exact (the ma5s's own tags, nothing of the map's stock ones), the pack's arguments, texts and credits, the
weapon taken as the collection made it, and the brass fix (the casing made at the ejection port, not the silencer's
tip). Also make_pack.py's map source: the source named "source" as CMT's "cmt", the map deflated again whole for
Invader, no icon when the recipe names none. Everywhere (CI): no game file is read; a run of the recipe is the box's
(make_pack.py --source-map, docs/weapon-packs.md)."""
import json
import re
import struct
import sys
import zlib
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import donor_map  # noqa: E402
import make_pack  # noqa: E402
from test_weapon_pack_tools import disc_map  # noqa: E402

FOLDER = TOOLS / "ar_silenced"
EFFECT = "weapons/ma5s/effects/fire bullet.effect"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "map"
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    source = data["source"]
    assert set(source["allowlist"]) == make_pack.recipe_inputs(data)["cmt"] | set(source["verbatim"])
    assert source["allowlist"] == sorted(set(source["allowlist"]))
    # (the ma5s's own tags and the one tag of the stock assault rifle's the map has in another group: everything
    # else it needs is the stock map's, left to the map)
    assert [name for name in source["allowlist"] if not name.startswith("weapons/ma5s/")] == [
        "weapons/assault rifle/shaders/bolt.shader_transparent_chicago"]
    assert source["file"] == "h2d_gemini.map" and source["tags"] == 1840
    stock = {name for entry in data["steps"] for root, name in make_pack.step_inputs(entry) if root == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals"}
    assert data["pack"]["stock_derived"] == {} and data["art"] == {}
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert (data["expected"]["own_tags"], data["expected"]["external_tags"]) == (9, 32)


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_ar_silenced", "ae\\packs\\ar_silenced\\", "ar_silenced", "SUPPRESSED AR")
    assert (pack["labels"], pack["behaviour"], pack["replaces"], pack["weapon"]) == ("ar", 0, 0, "weapons\\ma5s\\ma5s")
    # (its HUD is the stock assault rifle's, the map's: no icon of its own, and none passed to the builder)
    assert "icon" not in pack
    arguments = make_pack.builder_arguments(data, "donor.map", "names.txt", "out.aepack", FOLDER)
    assert "--icon" not in arguments and "--smooth" not in arguments
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a suppressed assault rifle\n" in texts
    assert "ammo_pickup=Picked up %d rounds for suppressed assault rifle\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("Dominater01", "S3anyboy", "GoodFellaPro", "Team NHE", "Gemini", "Halo 2 Mappack Collection v2"):
        assert name in credits


def test_the_weapon_is_the_collections_and_only_the_brass_is_fixed():
    data = recipe()
    # the weapon, its models, sound and silencer as the collection made them (its melee is the stock assault
    # rifle's already: weapons\assault rifle\melee, on every stock multiplayer map)
    assert "weapons/ma5s/ma5s.weapon" in data["source"]["verbatim"]
    assert [entry["tag"] for entry in data["steps"] if entry["tag"].startswith("weapons/")] == [EFFECT]
    fix = [entry for entry in data["steps"] if entry["tag"] == EFFECT][0]
    assert fix["from"] == ["source", EFFECT] and fix["layer"] == "fix-ae"
    # the casing: the effect's particle 2 of event 0, made at location 0 ('primary trigger', the silencer's tip),
    # now at location 1 ('primary ejection'), as the stock assault rifle's
    assert fix["edits"] == [["set", "events[0].particles[2].location", "1"]]
    assert "primary ejection" in fix["why"] and "primary trigger" in fix["why"]


def test_a_step_from_the_source_reads_the_allowlist_as_one_from_cmt():
    entry = {"layer": "fix-ae", "tag": "a.effect", "op": "edit", "from": ["source", "a.effect"],
             "edits": [["graft", "source", "b.effect", "x", "y"]]}
    assert make_pack.step_inputs(entry) == [("cmt", "a.effect"), ("cmt", "b.effect")]


def test_a_map_source_needs_its_tag_count_and_a_known_kind():
    data = recipe()
    del data["source"]["tags"]
    with pytest.raises(make_pack.Refused, match="no tags"):
        make_pack.check_recipe(data)
    data = recipe()
    data["source"]["kind"] = "folder"
    with pytest.raises(make_pack.Refused, match="no such source"):
        make_pack.check_recipe(data)


def test_the_map_source_is_written_whole_and_deflated_again(tmp_path):
    path, image = disc_map(tmp_path, 0x200, 0x1000 - 0x300)
    copy, added, count = make_pack.map_source_copy(path)
    assert (added, count) == (0x1000 - 0x300, 1)
    header = donor_map.MAP_HEADER_SIZE
    assert copy[:header] == path.read_bytes()[:header]
    rest = zlib.decompress(copy[header:])
    assert len(rest) == struct.unpack_from("<I", copy, 8)[0] - header
    assert rest[0x100:0x100 + len(image)] == image and not any(rest[0x300:])
