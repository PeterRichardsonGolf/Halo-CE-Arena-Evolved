"""The burst PDW pack's recipe, tools/weapon_pack/pdw/recipe.json: ZTeam's compact submachine gun (the ZTeam tag pack's
smg/h3 models in its smg/h3b texture) as an Xbox weapon: the recipe's own rules (no prototype), the allowlist (ZTeam's
own tags of the weapon and nothing else), the models converted and bound to the h3b shaders, the round and firing effect
built from the stock assault rifle's (named as stock-derived), the engine's burst (behaviour 4) and the weapon's numbers
the same as its profile's (port/linux/game/ae_rifle_profiles.c), the HUD,
the pack's arguments, texts and credits. Everywhere (CI): no game file is read; a run of the recipe is the box's
(make_pack.py --zip, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "pdw"
H3 = "zteam/objects/weapons/single/smg/h3/"
H3B = "zteam/objects/weapons/single/smg/h3b/"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(tag):
    return [entry for entry in recipe()["steps"] if entry["tag"] == tag][0]


def edits(tag):
    found = {}
    for edit in step(tag)["edits"]:
        if edit[0] == "set":
            found[edit[1]] = edit[2]
        elif edit[0] == "sets":
            found.update({field: value for field, value in edit[1]})
    return found


def test_recipe_rules_and_allowlist():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip" and data["source"]["root"] == ""
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("zteam/") for name in allowlist)
    # (nothing of ZTeam's impacts, particles, round or reload effect: the stock assault rifle's are the map's; ZTeam's
    # HUD is the weapon's own, its rounds panel the map's; the h3 texture and shaders are not taken, h3b's are)
    assert not [name for name in allowlist if re.search("effects/|bullet\\.|hud/temp/master|reload\\.effect", name)]
    assert not [name for name in allowlist if name.startswith(H3 + "shaders/") or name == H3 + "bitmaps/smg.bitmap"]
    assert {H3B + "bitmaps/smg.bitmap", H3B + "shaders/smg_metal.shader_model",
            H3B + "shaders/smg_rubber.shader_model", H3 + "smg.weapon"} <= set(allowlist)
    for key in ("pack_sha256", "pack_own_sha256", "donor_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])


def test_models_wear_the_h3b_texture():
    data = recipe()
    models = sorted(entry["tag"] for entry in data["steps"] if entry["op"] == "model")
    assert models == [H3 + "models/smg_fp.model", H3 + "models/smg_tp.model"]
    for tag in models:
        assert edits(tag) == {"shaders[0].shader": H3B + "shaders/smg_metal.shader_model",
                              "shaders[1].shader": H3B + "shaders/smg_rubber.shader_model"}


def test_round_and_effects_stock_derived():
    data = recipe()
    stock = {entry["tag"]: entry["from"][1] for entry in data["steps"] if entry.get("from", [""])[0] == "stock"}
    assert stock[H3 + "bullet.projectile"] == "weapons/assault rifle/bullet.projectile"
    assert stock[H3 + "bullet.damage_effect"] == "weapons/assault rifle/bullet.damage_effect"
    assert stock[H3 + "effects/fire_bullet.effect"] == "weapons/assault rifle/effects/fire bullet.effect"
    assert stock[H3 + "effects/trigger.damage_effect"] == "weapons/assault rifle/trigger.damage_effect"
    prefix = data["pack"]["prefix"] + H3.replace("/", "\\")
    assert set(data["pack"]["stock_derived"]) == {prefix + "bullet", prefix + "effects\\fire_bullet",
                                                   prefix + "effects\\trigger"}
    # (haptics: the firing kick with a rumble on both motors, the weapon's own)
    assert edits(H3 + "smg.weapon")["triggers[0].firing_effects[0].firing_damage"] == \
        H3.replace("/", "\\") + "effects\\trigger.damage_effect"
    kick = edits(H3 + "effects/trigger.damage_effect")
    assert float(kick["low_frequency_vibrate_frequency"]) > 0 and float(kick["high_frequency_vibrate_frequency"]) > 0


PROFILES = ROOT / "port" / "linux" / "game" / "ae_rifle_profiles.c"


def pdw_profile():
    """the BURST PDW row of ae_rifle_profiles.c's profiles, its values as written"""
    text = PROFILES.read_text(encoding="latin-1")
    row = re.search(r'\{ "BURST PDW",(.*?)\}', text, re.S).group(1)
    return [value.strip() for value in row.split(",")]


def test_the_burst_is_the_engines():
    """the engine's burst (behaviour 4, ae_rifle_profiles.c's BURST PDW, played by ae_burst_rifle.c): the tags' trigger
    is plain (no charge, no spew, no second trigger, not latched); the recipe's numbers are the profile's"""
    weapon = edits(H3 + "smg.weapon")
    data = recipe()
    assert data["pack"]["behaviour"] == 4
    assert not [edit for edit in step(H3 + "smg.weapon")["edits"] if edit[0] == "copy"]
    for field in ("charging_time", "charged_time", "overcharged_action", "spew_time", "flags.does_not_repeat_automatically"):
        assert "triggers[0]." + field not in weapon
    assert not [field for field in weapon if field.startswith("triggers[1]")]
    # (name, a rifle, rounds a pull, ticks between its rounds, keeps a pull, recovery, damage / slowed, shield / body,
    # loaded, at first, reserve, between tracers, autoaim, magnetism, spread, zoom, bullet speeds and range, ...)
    row = pdw_profile()
    assert row[0:5] == ["FALSE", "3", "1", "FALSE", "12"]
    # (pulls 16 ticks apart: the last round 2 ticks after the first, the tick after, the recovery, the next pull's tick;
    # the killing round at mid range, the 4th burst's 2nd, 49 ticks after the first, just slower than the battle
    # rifle's 4 bursts' 46; close, the 3rd burst's 2nd, 33)
    spacing = (3 - 1) * 1 + 12 + 2
    assert spacing == 16 and 3 * spacing + 1 == 49 > 46 and 2 * spacing + 1 == 33
    damage = edits(H3 + "bullet.damage_effect")
    assert [float(v.rstrip("f")) for v in row[5:9]] == [float(damage["damage_upper_bound"].split()[0]),
                                                        float(damage["damage_lower_bound"]), 1.0, 1.0]
    assert row[9:13] == ["36", "144", "144", "2"]
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_total_initial"],
            weapon["magazines[0].rounds_reserved_maximum"], weapon["triggers[0].projectiles_between_contrails"]) == (
        "36", "144", "144", "2")
    assert [float(v.rstrip("f")) for v in row[13:17]] == [float(weapon["autoaim_angle"]), float(weapon["autoaim_range"]),
                                                          float(weapon["magnetism_angle"]), float(weapon["magnetism_range"])]
    assert [float(v.rstrip("f")) for v in row[17:19]] == [float(v) for v in weapon["triggers[0].error_angle"].split()]
    projectile = edits(H3 + "bullet.projectile")
    assert [float(v.rstrip("f")) for v in row[19:23]] == [0.0, float(projectile["initial_velocity"]),
                                                          float(projectile["final_velocity"]),
                                                          float(projectile["maximum_range"])]
    assert weapon["triggers[0].maximum_rate_of_fire"] == "30 30"


def test_three_bursts_close_four_at_mid():
    """a full-shield Spartan, 75 shield and 75 body, the shield's excess carried into the body: 20 a round up close,
    14 once slowed, x1.0 on shield and body"""
    damage = edits(H3 + "bullet.damage_effect")
    close = float(damage["damage_upper_bound"].split()[0])
    mid = float(damage["damage_lower_bound"])
    assert (damage["cyborg_armor"], damage["cyborg_energy_shield"]) == ("1", "1")
    vitality = 150
    # close: 3 bursts, the 8th round kills (one to spare), 2 bursts do not
    assert 8 * close >= vitality > 7 * close and 6 * close < vitality
    # mid: 4 bursts, the 11th round kills (one to spare), 3 bursts do not
    assert 11 * mid >= vitality > 10 * mid and 9 * mid < vitality
    projectile = edits(H3 + "bullet.projectile")
    assert (projectile["initial_velocity"], projectile["final_velocity"]) == ("400", "240")
    assert projectile["air_damage_range"] == "25.4 27.4" and projectile["maximum_range"] == "40"


def test_the_weapons_numbers():
    weapon = edits(H3 + "smg.weapon")
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_reloaded"]) == ("36", "36")
    assert (weapon["magazines[0].rounds_total_initial"], weapon["magazines[0].rounds_reserved_maximum"]) == ("144", "144")
    assert weapon["triggers[0].error_angle"] == "0.4 1.2"
    assert (weapon["autoaim_angle"], weapon["magnetism_angle"]) == ("4", "8")
    assert "zoom_levels" not in weapon
    # (ZTeam's "primary firing on" input is past the Xbox's values: the validator would correct the donor)
    assert weapon["weapon_b_in"] == "primary_firing"


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_pdw", "ae\\packs\\pdw\\", "pdw", "BURST PDW")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("h_smg,ar", 4, 0)
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a burst PDW\n" in texts
    assert "ammo_pickup=Picked up %d rounds for burst PDW\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    assert "ZTeam" in credits and "halomaps.org" in credits and "names no" in credits and "CMT" in credits


def test_zteams_own_hud_cmts_reticle_and_picture():
    """the HUD is ZTeam's own (its rounds panel the map's stock one, its meter ZTeam's 36-round art); the reticle CMT's
    SMG one; the prompts' picture ZTeam's own picture of the weapon resampled onto a one-sprite sheet (source_icon.py)"""
    data = recipe()
    tag = "zteam/hud/temp/weapon_interfaces/test/h3_smg.weapon_hud_interface"
    hud = step(tag)
    assert hud["edits"][0] == ["set", "child_hud", "ui\\hud\\master rounds.weapon_hud_interface"]
    assert (edits(tag)["static_elements[0].sequence_index"], edits(tag)["meter_elements[0].sequence_index"]) == ("2", "2")
    grafts = [edit for edit in hud["edits"] if edit[0] == "graft"]
    assert grafts == [["graft", "fix-ae", "ae_pack/hud/cmt_smg.weapon_hud_interface", "crosshairs[0]", "crosshairs[0]"]]
    assert step("ae_pack/hud/cmt_smg.weapon_hud_interface")["op"] == "hud_hires"
    assert edits(H3 + "smg.weapon")["hud_interface"] == tag.replace("/", "\\")
    assert data["art_drawings"]["pdw_icon.png"]["draw"] == "source_icon"
    assert data["art_inputs"]["pdw_picture.bitmap"]["tag"] == "zteam/hud/h3/bitmaps/icons/weapons/smg.bitmap"
    assert data["pack"]["icon"] == "ae_pack\\hud\\pdw_icon:0" and data["pack"]["roots"] == ["bitm:ae_pack\\hud\\pdw_icon"]
