"""The concussion rifle pack's recipe, tools/weapon_pack/concussion_rifle/recipe.json: ZTeam's arc launcher (its
weapon tag, models, shaders, textures, first-person animations, collision, sounds, explosive bolt with its effects,
and its launcher HUD) as an Xbox weapon tuned for displacement and splash: a softer, wider blast than the brute shot
pack's that throws players and vehicles. Everywhere (CI): no game file is read; a run of the recipe is the box's
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

FOLDER = TOOLS / "concussion_rifle"
HR = "zteam/objects/weapons/single/concussion_rifle/hr/"
ZTEAM_HUD = "zteam/hud/temp/weapon_interfaces/test/h3_brute_shot.weapon_hud_interface"
# (ZTeam's HUD and kick are files its weapons share: the pack makes its copies under its own names, so no other
# pack's copy of the same file with other edits meets them in a merged cache)
HUD = HR + "hud/concussion_rifle.weapon_hud_interface"
BLAST = HR + "bolt_explosion.damage_effect"
CLOUD = "zteam/effects/particle_systems/explosions/concussion_bolt_explosion/concussion_bolt_explosion.particle_system"
ZTEAM_KICK = "zteam/shared/damage_effects/trigger/trigger_rifle_kickback_strong.damage_effect"
KICK = HR + "effects/trigger.damage_effect"
SHIELD, BODY = 75.0, 75.0
# the engine's knockback (objects/damage.c object_damage_aftermath, units/bipeds.c biped_accelerate): the damage
# effect's instantaneous acceleration x the object's acceleration scale, in world units a second, halved for a living
# biped and doubled for a vehicle by "detonates explosives"; the stock cyborg's scale 0.7, the warthog's 0.3, the
# stock frag's blast 4 and the stock rocket's 6 with the flag
SPARTAN_SCALE, WARTHOG_SCALE, FRAG, ROCKET = 0.7, 0.3, 4.0, 6.0


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


def brute_shot_hit():
    data = json.loads((TOOLS / "brute_shot" / "recipe.json").read_text())
    blast = [entry for entry in data["steps"] if entry["tag"].endswith("brute_shot_grenade_explosion.damage_effect")][0]
    found = {edit[1]: edit[2] for edit in blast["edits"] if edit[0] == "set"}
    return float(found["damage_upper_bound"].split()[0]), float(found["radius"].split()[1])


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip" and data["source"]["root"] == ""
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("zteam/") for name in allowlist)
    # (ZTeam's HUD chain with its full-screen visor, its melee, materials and the brute shot picture are not taken)
    assert not [name for name in allowlist if re.search(
        "hud/temp/master|visors/|panels/|weapons_temp|damage_effects/melee|weapon\\.material_effects", name)]
    assert {HR + "concussion_rifle.weapon", ZTEAM_HUD, ZTEAM_KICK, BLAST, HR + "bolt.projectile", CLOUD} <= set(allowlist)
    # (the shared files are read by the steps that make the pack's own copies, never taken as they are)
    assert ZTEAM_HUD not in data["source"]["verbatim"] and ZTEAM_KICK not in data["source"]["verbatim"]
    # (the first-person animations are the hr_cad folder's: the hr folder has none)
    assert "zteam/objects/weapons/single/concussion_rifle/hr_cad/animations/concussion_rifle_fp_spartan" \
        ".model_animations" in data["source"]["verbatim"]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key]) and set(data["expected"][key]) != {"0"}
    for entry in data["art"].values():
        assert re.fullmatch(r"[0-9a-f]{64}", entry["pixels_sha256"]) and set(entry["pixels_sha256"]) != {"0"}
    for entry in data["art_inputs"].values():
        assert re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) and set(entry["sha256"]) != {"0"}


def test_the_models_converted():
    data = recipe()
    models = sorted(entry["tag"] for entry in data["steps"] if entry["op"] == "model")
    assert models == [HR + "models/bolt.model", HR + "models/concussion_rifle_fp.model",
                      HR + "models/concussion_rifle_tp.model"]
    weapon = edits(HR + "concussion_rifle.weapon")
    assert weapon["model"] == HR + "models/concussion_rifle_tp.model"
    assert weapon["first_person_model"] == HR + "models/concussion_rifle_fp.model"
    assert edits(HR + "bolt.projectile")["model"] == HR + "models/bolt.model"
    multipurpose = sorted(entry["tag"] for entry in data["steps"] if entry["op"] == "multipurpose")
    assert multipurpose == sorted([
        HR + "bitmaps/concussion_rifle_multipurpose.bitmap",
        "zteam/objects/weapons/grenade/plasma_grenade/hr/bitmaps/plasma_grenade_multipurpose.bitmap"])
    # (the blast's fire cloud drawn as plain sprites: its bitmaps have no rotational sequence)
    assert edits(CLOUD) == {"particle_types[0].complex_sprite_render_modes": "simple"}


def test_hits_to_kill_and_not_a_better_brute_shot():
    blast = edits(BLAST)
    direct = float(blast["damage_upper_bound"].split()[0])
    edge = float(blast["damage_lower_bound"])
    inner, outer = (float(value) for value in blast["radius"].split())
    assert blast["damage_upper_bound"] == "%g %g" % (direct, direct)
    # three direct hits strip a full shield (two do not), five kill (four do not)
    assert 3 * direct >= SHIELD > 2 * direct
    assert 5 * direct >= SHIELD + BODY > 4 * direct
    assert 0 < edge < direct and float(blast["damage_aoe_core_radius"]) < inner < outer
    # softer and wider than the brute shot pack: less a hit, a larger blast
    brute_hit, brute_radius = brute_shot_hit()
    assert direct < brute_hit and outer > brute_radius
    weapon = edits(HR + "concussion_rifle.weapon")
    assert weapon["triggers[0].maximum_rate_of_fire"] == "2 2"
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_reloaded"],
            weapon["magazines[0].rounds_total_initial"], weapon["magazines[0].rounds_reserved_maximum"]) == (
        "6", "6", "18", "24")
    # a five-bolt kill takes 4 intervals at 2 a second: 2 s (the brute shot's 4 at its 2.6: about 1.2 s)
    assert 4 / float(weapon["triggers[0].maximum_rate_of_fire"].split()[0]) == 2.0
    assert (weapon["autoaim_angle"], weapon["autoaim_range"], weapon["magnetism_angle"],
            weapon["magnetism_range"]) == ("4", "20", "6.5", "25")
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"


def test_knockback_pushes_players_and_vehicles():
    blast = edits(BLAST)
    push = float(blast["damage_instantaneous_acceleration"])
    spartan = push * SPARTAN_SCALE * 0.5
    # a Spartan is thrown harder than by a frag (1.4 units a second), not harder than by a rocket (2.1)
    assert FRAG * SPARTAN_SCALE * 0.5 < spartan <= ROCKET * SPARTAN_SCALE * 0.5
    # the flag that doubles a vehicle's push is cleared: a warthog moves under half a rocket's a bolt (tried in the
    # game: at 8 one bolt flipped an empty warthog, at 5 it rears up and lands on its wheels, a second flips it)
    assert blast["damage_flags.detonates_explosives"] == "0"
    assert 0 < push * WARTHOG_SCALE < ROCKET * WARTHOG_SCALE * 2 / 2 and push < 8
    projectile = edits(HR + "bolt.projectile")
    assert (projectile["initial_velocity"], projectile["final_velocity"], projectile["air_gravity_scale"]) == (
        "20", "16", "0.25")
    assert float(projectile["danger_radius"]) == float(blast["radius"].split()[1])


def test_haptics():
    assert step(KICK)["from"] == ["source", ZTEAM_KICK]
    assert edits(HR + "concussion_rifle.weapon")["triggers[0].firing_effects[0].firing_damage"] == KICK.replace("/", "\\")
    kick = edits(KICK)
    assert float(kick["low_frequency_vibrate_frequency"]) > 0 and float(kick["high_frequency_vibrate_frequency"]) > 0
    blast = edits(BLAST)
    assert float(blast["low_frequency_vibrate_duration"]) <= 0.5 and float(blast["camera_shaking_duration"]) <= 0.6


def test_zteams_hud_and_the_picture():
    data = recipe()
    # ZTeam's launcher HUD: its six-shell meter kept, the stock rounds panel, its launcher reticle (sequence 17,
    # the drop ladder) at an even scale, its picture of a brute shot left out
    assert edits(HUD) == {"child_hud": "ui\\hud\\master rounds.weapon_hud_interface",
                          "sequence_index": "-1",
                          "crosshairs[0].crosshair_overlays[0].sequence_index": "17",
                          "crosshairs[0].crosshair_overlays[0].height_scale": "0.4125"}
    assert ["erase", "overlay_elements[*]"] in step(HUD)["edits"]
    assert step(HUD)["from"] == ["source", ZTEAM_HUD]
    assert edits(HR + "concussion_rifle.weapon")["hud_interface"] == HUD.replace("/", "\\")
    # no picture: no licensed picture of this weapon exists, so it takes the grenade launcher's opt-in (no pack icon,
    # its own HUD's messaging icon -1): its name is written where a picture would be (ae_pack_names.c)
    assert "icon" not in data["pack"] and "roots" not in data["pack"]
    assert not data["art"] and not data["art_drawings"] and not data["art_inputs"]
    assert not [entry for entry in data["steps"] if entry["tag"].startswith("ae_pack/hud/")
                or entry["tag"].endswith(".hud_globals")]
    assert not [edit for edit in step("globals/globals.globals")["edits"] if "hud_globals" in edit[1]]


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_concussion_rifle", "ae\\packs\\concussion_rifle\\", "concussion_rifle", "CONCUSSION RIFLE")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("cr,ar", 0, 0)
    assert pack["stock_derived"] == {}
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a concussion rifle\n" in texts
    assert "ammo_pickup=Picked up %d bolts for concussion rifle\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    assert "ZTeam" in credits and "halomaps.org" in credits and "names no individual authors" in credits
    assert "concussion_rifle" in (TOOLS / "SHIPPED.txt").read_text().split()
