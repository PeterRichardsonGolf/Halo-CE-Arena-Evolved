"""The spiker pack's recipe, tools/weapon_pack/spiker/recipe.json: CMT's multiplayer spiker (spiker_mp) from the
stock-compatible TSC:E 1.4 source release, made the Xbox's and set to AE's numbers: a close-to-mid automatic. The
recipe's own rules (no prototype), the allowlist (CMT's tags only), only what an Xbox map needs done to CMT's tags,
the numbers (spikes to strip and to kill, rate, magazine, spike flight, aim assist), the pad pulse, the pack's
arguments, texts and credits. Everywhere (CI): no game file is read; a run of the recipe is the box's (make_pack.py
--zip, docs/weapon-packs.md)."""
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

FOLDER = TOOLS / "spiker"
SHARED = "cmt/weapons/_shared/spiker/"
WEAPON = "cmt/weapons/evolved/spiker/_spiker_mp/spiker_mp.weapon"
PROJECTILE = "cmt/weapons/evolved/spiker/_spiker_mp/projectiles/spiker_mp_spike/spiker_mp_spike.projectile"
DAMAGE = "cmt/weapons/evolved/spiker/damage_effects/spiker_spike_impact.damage_effect"
TRIGGER = "cmt/weapons/evolved/spiker/damage_effects/spiker_spike_trigger.damage_effect"
# CE's player: 75 shield and 75 body
SHIELD = BODY = 75


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(tag, layer="fix-ae"):
    return [entry for entry in recipe()["steps"] if entry["tag"] == tag and entry["layer"] == layer][0]


def sets(tag, layer="fix-ae"):
    return dict(step(tag, layer)["edits"][0][1])


def spikes_to_kill():
    """(spikes to strip a full shield, spikes to kill from full), without and with the strip's spill on the body"""
    damage = sets(DAMAGE)
    per = float(damage["damage_upper_bound"].split()[0])
    on_shield, on_body = per * float(damage["cyborg_energy_shield"]), per * float(damage["cyborg_armor"])
    strip = math.ceil(SHIELD / on_shield)
    spill = (strip * on_shield - SHIELD) / float(damage["cyborg_energy_shield"]) * float(damage["cyborg_armor"])
    return strip, strip + math.ceil(BODY / on_body), strip + math.ceil((BODY - spill) / on_body)


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip"
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    source = data["source"]
    assert source["file"] == "tsce_1-4_source_release_stock.zip"
    assert source["root"] == "tsce_1-4_source_release_stock/tags/"
    assert source["allowlist"] == sorted(set(source["allowlist"]))
    assert all(name.startswith("cmt/") for name in source["allowlist"]) and WEAPON in source["allowlist"]
    # (the held weapon's collision and melee are the stock assault rifle's: CMT's are not taken)
    assert not any(name.endswith((".model_collision_geometry", "melee_blade.damage_effect",
                                  "melee_response.damage_effect")) for name in source["allowlist"])
    assert data["layers"] == ["fix-nopanel", "fix-ae", "fix-xbox"]
    assert data["pack"]["stock_derived"] == {} and data["art"] == {}
    for key in ("pack_sha256", "pack_own_sha256", "donor_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])


def test_only_what_an_xbox_map_needs():
    data = recipe()
    ops = {}
    for entry in data["steps"]:
        if entry["layer"] == "fix-xbox":
            ops.setdefault(entry["op"], []).append(entry["tag"])
    assert set(ops) == {"edit", "model", "multipurpose", "hud_hires", "sound"}
    assert sorted(ops["model"]) == sorted([SHARED + "spiker.model", SHARED + "fp/fp.model",
                                           SHARED + "projectiles/spiker_spike/spiker_spike.model"])
    assert sorted(ops["multipurpose"]) == [SHARED + "bitmaps/spiker_body_multi.bitmap",
                                           SHARED + "bitmaps/spiker_clip_multi.bitmap"]
    # (the one Ogg Vorbis sound of the spiker's tags, re-encoded)
    assert ops["sound"] == ["cmt/sounds/sfx/effects/impacts/flesh_impact.sound"]
    assert "cmt/ui/hudv4.1/weapons/evolved/covenant/spiker.weapon_hud_interface" in ops["hud_hires"]
    weapon = step(WEAPON, "fix-xbox")
    assert {edit[1] for edit in weapon["edits"]} == {"model", "first_person_model", "pickup_text_index",
                                                       "hud_text_message_index"}
    # the dull body shader's detail map names a campaign bitmap no multiplayer map has: left empty, nothing else
    dull = step(SHARED + "shaders/spiker_body_dull.shader_model", "fix-xbox")
    assert dull["edits"] == [["set", "detail_map", ".bitmap"]] and "lifepod" in dull["why"]


def test_the_numbers():
    weapon = sets(WEAPON)
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_reloaded"]) == ("40", "40")
    assert (weapon["magazines[0].rounds_total_initial"], weapon["magazines[0].rounds_reserved_maximum"]) == (
        "80", "160")
    assert weapon["triggers[0].maximum_rate_of_fire"] == "10 10"
    assert (weapon["triggers[0].error_angle"], weapon["triggers[0].minimum_error"]) == ("0.6 2.4", "0.4")
    assert (weapon["autoaim_angle"], weapon["autoaim_range"], weapon["magnetism_angle"],
            weapon["magnetism_range"]) == ("3.5", "15", "8", "20")
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    assert weapon["collision_model"] == "weapons\\assault rifle\\assault rifle.model_collision_geometry"
    assert sets(PROJECTILE) == {"initial_velocity": "50", "final_velocity": "30", "maximum_range": "60"}
    damage = sets(DAMAGE)
    assert damage["damage_lower_bound"] == "10" and damage["damage_upper_bound"] == "10 10"
    assert (damage["cyborg_energy_shield"], damage["cyborg_armor"]) == ("0.85", "1.3")


def test_spikes_to_kill_a_close_to_mid_automatic():
    """9 spikes strip a full shield and 15 kill from full (with or without the strip's spill), 1.4 s at 10 a
    second: between the SMG up close (0.8 s) and the assault rifle (20 rounds, 1.6 s)"""
    strip, kill, kill_spill = spikes_to_kill()
    assert (strip, kill, kill_spill) == (9, 15, 15)
    rate = float(sets(WEAPON)["triggers[0].maximum_rate_of_fire"].split()[0])
    assert abs((kill - 1) / rate - 1.4) < 1e-9
    # a magazine kills two from full, with spikes to spare
    assert 2 * kill <= int(sets(WEAPON)["magazines[0].rounds_loaded_maximum"])


def test_the_pad_pulse():
    fields = sets(TRIGGER)
    assert float(fields["low_frequency_vibrate_frequency"]) > 0 and float(fields["high_frequency_vibrate_frequency"]) > 0
    # (a pulse shorter than the 0.1 s between spikes, so the pulses do not run into each other)
    assert float(fields["low_frequency_vibrate_duration"]) < 0.1 and float(fields["high_frequency_vibrate_duration"]) < 0.1


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_spiker", "ae\\packs\\spiker\\", "spiker", "SPIKER")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("c_spk,ar", 0, 0)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    # the prompts' and kill feed's picture: CMT's own pickup icon sheet, the spiker's sequence 11 (its HUD's)
    assert pack["icon"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons:11"
    assert pack["roots"] == ["bitm:cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons"]
    assert "cmt/ui/hudv4.1/bitmaps/weapons/pickup_icons.bitmap" in data["source"]["verbatim"]
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a spiker\n" in texts
    assert "ammo_pickup=Picked up %d spikes for spiker\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("CMT", "Teh Lag", "Ifafudafi", "Siliconmaster", "spiker_mp"):
        assert name in credits
