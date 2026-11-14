"""The starter pistol pack's recipe, tools/weapon_pack/starter_pistol/recipe.json: the Digsite mod's 00_e3 pistol (the
official Halo CE Mod Tools' digsite/weapons/pistol/00_e3) as an Xbox weapon tuned as AE's starter sidearm: one precise
round a pull, a small 1.5x zoom, a tight bloom. Everywhere (CI): no game file is read; a run of the recipe is the box's
(make_pack.py --zip, docs/weapon-packs.md)."""
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "starter_pistol"
P = "digsite/weapons/pistol/00_e3/"
SHIELD, BODY = 75.0, 75.0


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
            found.update({key: value for key, value in edit[1]})
    return found


def test_recipe_rules_and_allowlist():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip" and data["source"]["root"] == ""
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("digsite/") for name in allowlist)
    # (nothing of the mod's suit interface or ammunition bar: the map's classic rounds panel draws the ammunition)
    assert not [name for name in allowlist if re.search("suit interface|hud_ammo", name)]
    assert P + "pistol_00_e3.weapon" in allowlist and not [n for n in allowlist if "reticle" in n]
    assert not [name for name in allowlist if "/maps/" in name or name.startswith("digsite/levels")]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert data["pack"]["id"] == "ae_starter_pistol" and data["pack"]["weapon_key"] == "starter_pistol"


def test_the_numbers_fit_the_sandbox():
    weapon = edits(P + "pistol_00_e3.weapon")
    bullet = edits(P + "h pistol bullet.damage_effect")
    assert weapon["model"] == P + "00_e3.model" and weapon["first_person_model"] == P + "_dig/fp/fp.model"
    assert weapon["zoom_levels"] == "1" and weapon["zoom_magnification_range"] == "1.5 1.5"
    assert [weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_total_initial"],
            weapon["magazines[0].rounds_reserved_maximum"]] == ["12", "48", "72"]
    rate = float(weapon["triggers[0].maximum_rate_of_fire"].split()[0])
    assert rate == 3.4
    damage = float(bullet["damage_upper_bound"].split()[0])
    assert bullet["damage_upper_bound"] == "24 24"
    shield_hit = damage * float(bullet["cyborg_energy_shield"])
    body_hit = damage * float(bullet["cyborg_armor"])
    # AE's 75 + 75 player (the cyborg collision model: maximum_shield_vitality 75, maximum_body_vitality 75; its head
    # material flagged head with a body multiplier 1.0). The game kills on a head hit whose shield-breaking round carries
    # any damage through (ae_perfect_required_hits, the game's own steps in single floats): three hits must leave a little
    # shield, so the third is never a head kill (25 or more a hit makes it one: the stock pistol's three)
    assert damage == 24.0 and shield_hit == 24.0 and body_hit == 42.0
    assert 3 * shield_hit < SHIELD < 4 * shield_hit
    # the fourth breaks it and carries the rest through: a head kill is four hits; the body: four plus one
    carried = (4 * shield_hit - SHIELD) * float(bullet["cyborg_armor"])
    assert carried < BODY <= carried + body_hit
    # the falloff (owner 11-09, "a slight nerf especially at distance"): the round slows from 2000 to 1000 units a second
    # between 15 and 35 units and its damage follows its speed down to the effect's minimum, 20: four hits (80) still
    # break the shield with some carried through, so a head kill stays four; the body takes six there
    round_ = edits(P + "h pistol bullet.projectile")
    assert round_["final_velocity"] == "1000" and round_["air_damage_range"] == "15 35"
    far = float(bullet["damage_lower_bound"])
    assert far == 20.0 and 3 * far < SHIELD < 4 * far
    far_carried = (4 * far - SHIELD) * float(bullet["cyborg_armor"])
    assert far_carried + far * float(bullet["cyborg_armor"]) < BODY <= far_carried + 2 * far * float(bullet["cyborg_armor"])
    pull = math.ceil(30 / rate) / 30
    assert abs(3 * pull - 0.9) < 0.01 and abs(4 * pull - 1.2) < 0.01
    error = [float(value) for value in weapon["triggers[0].error_angle"].split()]
    # a tight first shot, a wide cone for a held trigger (owner 11-09: "way more spread penalty for holding the trigger
    # down"): full in 0.35 s, back in 0.25 s, so shots paced half a second apart start on the dot
    assert error[0] <= 0.15 and error[1] == 2.5
    assert weapon["triggers[0].error_acceleration_time"] == "0.35" and weapon["triggers[0].error_deceleration_time"] == "0.25"
    assert weapon["intersection_reticle_range"] == edits(P + "h pistol bullet.projectile")["maximum_range"] == "60"


def test_the_round_is_real_and_the_hud_is_the_maps():
    weapon = edits(P + "pistol_00_e3.weapon")
    assert weapon["triggers[0].flags.projectile_is_client_side_only"] == "0"
    assert weapon["triggers[0].first_person_offset"] == "0 0 0"
    hud = step(P + "select pistol.weapon_hud_interface")["edits"]
    assert ["set", "child_hud", "ui\\hud\\master rounds.weapon_hud_interface"] in hud
    assert ["erase", "static_elements[*]"] in hud and ["erase", "meter_elements[*]"] in hud
    grafts = [e for e in hud if e[0] == "graft" and e[1] == "stock"]
    # the aiming reticle only: the stock pistol's zoomed overlays draw a "2x" label
    assert [e[3] for e in grafts] == ["crosshairs[0]"]
    # drawn smaller than the stock pistol's (owner 11-09: "a bit smaller precise crosshair")
    assert ["set", "crosshairs[0].crosshair_overlays[0].width_scale", "0.7"] in hud
    assert ["set", "crosshairs[0].crosshair_overlays[0].height_scale", "0.7"] in hud
    assert recipe()["pack"]["icon"] == "ae_pack\\hud\\starter_pistol_icon:0"


def test_texts_and_credits():
    texts = (FOLDER / "texts.txt").read_text()
    assert "menu_name=PROTOMAG" in texts and "%d" in texts
    credits = (FOLDER / "CREDITS.txt").read_text()
    assert "Digsite" in credits and "Mod Tools" in credits
