"""The scoped magnum pack's recipe, tools/weapon_pack/magnum_scoped/recipe.json: ZTeam's black slide magnum (the ZTeam
tag pack's magnum/h3o) as an Xbox weapon: the recipe's own rules (no prototype), the allowlist (ZTeam's own tags of the
magnum and nothing else), the models converted, the flashlight left out, the round and firing effect built from the
stock pistol's (named as stock-derived), the weapon's own numbers, the pack's arguments, texts and credits. Everywhere
(CI): no game file is read; a run of the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "magnum_scoped"
H3O = "zteam/objects/weapons/single/magnum/h3o/"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(tag):
    return [entry for entry in recipe()["steps"] if entry["tag"] == tag][0]


def edits(tag):
    return {edit[1]: edit[2] for edit in step(tag)["edits"] if edit[0] == "set"}


def test_recipe_rules_and_allowlist():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip" and data["source"]["root"] == ""
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("zteam/") for name in allowlist)
    # (nothing of ZTeam's flashlight, impacts or particles: the stock pistol's are the map's; ZTeam's HUD is the
    # magnum's own, its rounds panel the map's)
    assert not [name for name in allowlist if re.search("flashlight|effects/|bullet\\.|hud/temp/master", name)]
    assert "zteam/hud/temp/weapon_interfaces/test/h3_odst_magnum.weapon_hud_interface" in allowlist
    assert H3O + "magnum.weapon" in allowlist
    for key in ("pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert re.fullmatch(r"[0-9a-f]{64}", data["expected"]["donor_sha256"])


def test_models_converted_and_round_stock_derived():
    data = recipe()
    models = sorted(entry["tag"] for entry in data["steps"] if entry["op"] == "model")
    assert models == [H3O + "models/magnum_fp.model", H3O + "models/magnum_tp.model"]
    stock = {entry["tag"]: entry["from"][1] for entry in data["steps"] if entry.get("from", [""])[0] == "stock"}
    assert stock[H3O + "bullet.projectile"] == "weapons/pistol/bullet.projectile"
    assert stock[H3O + "bullet.damage_effect"] == "weapons/pistol/bullet.damage_effect"
    assert stock[H3O + "effects/fire_bullet.effect"] == "weapons/pistol/effects/fire bullet.effect"
    assert stock[H3O + "effects/trigger.damage_effect"] == "weapons/pistol/trigger.damage_effect"
    prefix = data["pack"]["prefix"] + H3O.replace("/", "\\")
    assert set(data["pack"]["stock_derived"]) == {prefix + "bullet", prefix + "effects\\fire_bullet",
                                                   prefix + "effects\\trigger"}
    # (haptics: the firing kick with a rumble on both motors, the weapon's own)
    assert edits(H3O + "magnum.weapon")["triggers[0].firing_effects[0].firing_damage"] == \
        H3O.replace("/", "\\") + "effects\\trigger.damage_effect"


def test_the_weapons_numbers():
    weapon = edits(H3O + "magnum.weapon")
    assert (weapon["zoom_levels"], weapon["zoom_magnification_range"]) == ("1", "2 2")
    assert weapon["magazines[0].rounds_loaded_maximum"] == "12"
    assert weapon["triggers[0].maximum_rate_of_fire"] == "2.4 2.4"
    assert weapon["triggers[0].error"] == "0 1" and weapon["triggers[0].error_angle"] == "0 1.25"
    damage = edits(H3O + "bullet.damage_effect")
    assert (damage["damage_lower_bound"], damage["cyborg_armor"], damage["cyborg_energy_shield"]) == ("24", "1.6", "1.6")
    # (38.4 a hit on a player: two strip a full 75 shield, the third to the head kills, four to the body)
    hit = 24 * 1.6
    assert 2 * hit >= 75 > hit and 4 * hit >= 150 > 3 * hit


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_magnum_scoped", "ae\\packs\\magnum_scoped\\", "magnum_scoped", "HJ MAGNUM")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("hp,ar", 0, 0)
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up an HJ magnum\n" in texts
    assert "ammo_pickup=Picked up %d rounds for HJ magnum\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    assert "ZTeam" in credits and "halomaps.org" in credits and "names no individual authors" in credits


def test_zteams_own_hud_and_picture():
    """the magnum's HUD is ZTeam's own (its rounds panel the map's stock one); the prompts' picture is ZTeam's own
    picture of the magnum resampled onto a one-sprite sheet (source_icon.py), a root of the pack"""
    data = recipe()
    hud = [entry for entry in data["steps"] if entry["tag"].endswith("h3_odst_magnum.weapon_hud_interface")][0]
    assert hud["edits"][0] == ["set", "child_hud", "ui\\hud\\master rounds.weapon_hud_interface"]
    # (the unzoomed reticles are CMT's pistol ones, as the sidearm's; the scope, crosshairs[1], is ZTeam's round window
    # drawn round: its picture hj_scope_mask 2048 x 512 at one texel a HUD unit, scale 1 both ways, so a round window
    # with a solid black surround at any aspect up to 4.2:1)
    assert [e[3:] for e in hud["edits"][1:3]] == [["crosshairs[2]", "crosshairs[0]"], ["crosshairs[3]", "crosshairs[2]"]]
    assert hud["edits"][3] == ["set", "crosshairs[1].crosshair_bitmap", "ae_pack\\hud\\hj_scope_mask.bitmap"]
    scale = dict(hud["edits"][4][1])
    over = "crosshairs[1].crosshair_overlays[0]."
    assert scale[over + "width_scale"] == scale[over + "height_scale"] == "1" and scale[over + "anchor_offset"] == "0 0"
    assert data["art"]["hj_scope_mask.png"]["size"] == [2048, 512]
    # (and no panel of the pack's own draws under the classic HUD style: ZTeam's static picture and meter are out)
    assert ["erase", "static_elements[*]"] in hud["edits"] and ["erase", "meter_elements[*]"] in hud["edits"]
    assert edits(H3O + "magnum.weapon")["hud_interface"] == hud["tag"].replace("/", "\\")
    assert data["art_drawings"]["magnum_icon.png"]["draw"] == "source_icon"
    assert data["art_inputs"]["magnum_picture.bitmap"]["tag"] == "zteam/hud/h3/bitmaps/icons/weapons/magnum.bitmap"
    assert data["pack"]["icon"] == "ae_pack\\hud\\magnum_icon:0" and data["pack"]["roots"] == ["bitm:ae_pack\\hud\\magnum_icon"]
