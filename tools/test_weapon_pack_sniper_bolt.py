"""The bolt-action sniper pack's recipe, tools/weapon_pack/sniper_bolt/recipe.json: a recipe whose source is a map's
tags (the CE+ X modded disc's Blood Gulch): the recipe's own rules (no prototype), the allowlist exact (the rifle's own
tags, nothing of the map's stock ones), the weapon taken as the disc has it (no step changes any of its tags), the
pack's arguments, texts and credits. Everywhere (CI): no game file is read; a run of the recipe is the box's
(make_pack.py --source-map, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "sniper_bolt"
WEAPON = "dw2/weapons/sniper_rifle/e3_2000/sniper rifle.weapon"
TRIGGER = "dw2/weapons/sniper_rifle/e3_2000/trigger.damage_effect"
ICONS = "dw2/ui/standard/hud/bitmaps/pickups/ce_plus_pickup_icons.bitmap"
HUD = "dw2/ui/standard/hud/weapon_interfaces/e3000.weapon_hud_interface"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "map"
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    source = data["source"]
    assert source["allowlist"] == sorted(set(source["allowlist"]))
    # (every tag taken as the disc has it, nothing of the stock map's; the trigger alone gets its rumble)
    assert source["verbatim"] == [name for name in source["allowlist"] if name not in (TRIGGER, HUD)]
    assert len(source["allowlist"]) == 24 and ICONS in source["verbatim"]
    assert all(name.split("/")[0] in ("dw2", "digsite") for name in source["allowlist"])
    assert WEAPON in source["allowlist"]
    assert source["file"] == "bloodgulch.map" and source["tags"] == 1761
    assert data["pack"]["stock_derived"] == {} and data["art"] == {}
    for key in ("pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert re.fullmatch(r"[0-9a-f]{64}", data["expected"]["donor_sha256"])
    assert (data["expected"]["own_tags"], data["expected"]["external_tags"]) == (24, 23)


def test_only_the_donor_and_the_rumble_are_made():
    data = recipe()
    # (the donor's HUD globals name CE+'s pickup icon sheet, so the donor holds it: donor only, never in the pack)
    assert [entry["tag"] for entry in data["steps"]] == ["ae_pack/donor/donor.scenario", "ae_pack/donor/hud.hud_globals",
                                                         "globals/globals.globals", TRIGGER, HUD]
    # (haptics, the done checklist's point 14: the firing kick's rumble on both motors, nothing else of the trigger)
    rumble = data["steps"][3]
    assert rumble["from"] == ["source", TRIGGER] and [edit[0] for edit in rumble["edits"]] == ["sets"]
    assert {field for field, _ in rumble["edits"][0][1]} == {"low_frequency_vibrate_frequency",
                                                              "low_frequency_vibrate_duration",
                                                              "high_frequency_vibrate_frequency",
                                                              "high_frequency_vibrate_duration"}
    stock = {name for entry in data["steps"] for root, name in make_pack.step_inputs(entry) if root == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals",
                     "ui/hud/default.hud_globals"}
    palette = [edit for edit in data["steps"][0]["edits"] if edit[1] == "weapon_palette[0].name"]
    assert palette == [["set", "weapon_palette[0].name", WEAPON.replace("/", "\\")]]


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_sniper_bolt", "ae\\packs\\sniper_bolt\\", "sniper_bolt", "BOLT SNIPER")
    # (held as the disc holds it, by the shotgun's label, which every stock player has)
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("sg", 0, 0)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    arguments = make_pack.builder_arguments(data, "donor.map", "names.txt", "out.aepack", FOLDER)
    # (the message icon is CE+'s own pickup icon sheet, its sniper's sequence 29, a root of its own: only CE+'s HUD
    # globals name it)
    assert pack["icon"] == "dw2\\ui\\standard\\hud\\bitmaps\\pickups\\ce_plus_pickup_icons:29"
    assert pack["roots"] == ["bitm:dw2\\ui\\standard\\hud\\bitmaps\\pickups\\ce_plus_pickup_icons"]
    assert arguments[arguments.index("--icon") + 1] == pack["icon"] and "--smooth" not in arguments
    assert arguments[arguments.index("--root") + 1] == pack["roots"][0]
    texts = (FOLDER / pack["texts"]).read_text()
    assert "menu_name=BOLT-ACTION SNIPER\n" in texts
    assert "weapon_pickup=Picked up a bolt-action sniper rifle\n" in texts
    assert "ammo_pickup=Picked up %d rounds for bolt-action sniper rifle\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("VKMT", "CE+ X", "Digsite Crew", "Sean T", "Ludus", "Holy Crust", "Roushyy"):
        assert name in credits


def test_the_huds_empty_elements_are_dropped():
    """CE+'s HUD's three static elements and its meter never draw (the game's tag check: "has no bitmap: not drawn",
    on CE+ X's maps too): they are dropped, the scope crosshairs are set to the stock sniper's size"""
    hud = [entry for entry in recipe()["steps"] if entry["tag"] == HUD][0]
    assert hud["from"] == ["source", HUD]
    assert hud["edits"][:2] == [["erase", "static_elements[*]"], ["erase", "meter_elements[*]"]]
    # (the scope crosshairs' offsets and scale are the stock sniper rifle's: no larger than the stock one on screen)
    sets = dict((e[1], e[2]) for e in hud["edits"][2:])
    assert sets["crosshairs[2].crosshair_overlays[1].anchor_offset"] == "189 0"
    assert sets["crosshairs[3].crosshair_overlays[0].width_scale"] == "1" and len(sets) == 10
    assert "four empty elements" in (FOLDER / "CREDITS.txt").read_text()
