"""The focus beam pack's recipe, tools/weapon_pack/focus_beam/recipe.json: CMT's prototype focus beam from the
stock-compatible TSC:E 1.4 source release, made the Xbox's and set to AE's numbers: a heat beam that strips shields
fast and is weak on bare health, so that another weapon or a melee blow finishes. The recipe's own rules (no
prototype), the allowlist (CMT's tags only), only what an Xbox map needs done to CMT's tags, the numbers (shield and
body per round, heat, overheat, battery, reach, aim assist), the pad rumble, the pack's arguments, texts and credits.
Everywhere (CI): no game file is read; a run of the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
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

FOLDER = TOOLS / "focus_beam"
BASE = "cmt/weapons/evolved_prototypes/focus_beam/"
WEAPON = BASE + "focus_beam.weapon"
PROJECTILE = BASE + "projectiles/focus_beam_beam.projectile"
DAMAGE = BASE + "damage_effects/focus_beam_beam_impact.damage_effect"
TRIGGER = "ae_pack/focus_beam/beam_trigger.damage_effect"
# CE's player: 75 shield and 75 body; CE's 30 ticks a second
SHIELD = BODY = 75
TICKS = 30
# CMT's 4 a round, which the recipe leaves (its damage bounds are not among the edits)
PER_ROUND = 4.0


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(tag, layer="fix-ae"):
    return [entry for entry in recipe()["steps"] if entry["tag"] == tag and entry["layer"] == layer][0]


def sets(tag, layer="fix-ae"):
    return dict(step(tag, layer)["edits"][0][1])


def beam(seconds_cap=10.0):
    """the trigger held on a full player, tick by tick as CE fires and cools a weapon: the rate rising from its low
    bound to its high over the acceleration time, the heat each round makes and the heat lost each tick; returns the
    seconds the shield took, the seconds to overheat, the rounds fired by then and the body left"""
    weapon, damage = sets(WEAPON), sets(DAMAGE)
    low, high = (float(value) for value in weapon["triggers[0].maximum_rate_of_fire"].split())
    rise = float(weapon["triggers[0].acceleration_time"])
    heat_round = float(weapon["triggers[0].heat_generated_per_round"])
    heat_loss = float(weapon["heat_loss_rate"]) / TICKS
    per_round = PER_ROUND
    on_shield = per_round * float(damage["cyborg_energy_shield"])
    on_body = per_round * float(damage["cyborg_armor"])
    shield, body, heat, owed, rounds, strip, tick = SHIELD, BODY, 0.0, 0.0, 0, None, 0
    while tick < seconds_cap * TICKS:
        tick += 1
        owed += (low + (high - low) * min(1.0, tick / (rise * TICKS))) / TICKS
        while owed >= 1.0:
            owed -= 1.0
            rounds += 1
            heat += heat_round
            if shield > 0:
                spill = on_shield - shield
                shield = max(0.0, shield - on_shield)
                if shield == 0:
                    strip = strip or tick
                    body -= max(0.0, spill) * float(damage["cyborg_armor"])
            else:
                body -= on_body
        heat -= heat_loss
        if heat >= 1.0:
            return strip / TICKS, tick / TICKS, rounds, body
    raise AssertionError("the beam never overheats")


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
    assert not any(name.endswith((".model_collision_geometry", "melee.damage_effect")) for name in source["allowlist"])
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
    # (no Open Sauce in this release: nothing stripped; its Ogg Vorbis sounds re-encoded, nothing else of them)
    assert set(ops) == {"edit", "model", "multipurpose", "hud_hires", "sound"}
    assert sorted(ops["model"]) == ["cmt/weapons/_shared/focus_beam/focus_beam.model",
                                    "cmt/weapons/_shared/focus_beam/fp/fp.model"]
    assert ops["multipurpose"] == ["cmt/weapons/_shared/focus_beam/bitmaps/focus_beam_body_multi.bitmap"]
    assert len(ops["sound"]) == 7 and all(name.startswith("cmt/sounds/") for name in ops["sound"])
    assert "cmt/ui/hudv4.1/weapons/evolved_prototypes/focus_beam.weapon_hud_interface" in ops["hud_hires"]
    weapon = step(WEAPON, "fix-xbox")
    assert {edit[1] for edit in weapon["edits"]} == {"model", "first_person_model", "pickup_text_index",
                                                       "hud_text_message_index"}


def test_the_numbers():
    weapon = sets(WEAPON)
    assert (weapon["triggers[0].maximum_rate_of_fire"], weapon["triggers[0].acceleration_time"]) == ("20 30", "0.3")
    assert (weapon["triggers[0].error_angle"], weapon["triggers[0].error_acceleration_time"]) == ("1.5 0", "0.3")
    assert (weapon["heat_loss_rate"], weapon["heat_recovery_threshold"]) == ("0.5", "0.2")
    assert weapon["zoom_magnification_range"] == "2 2"
    assert (weapon["autoaim_angle"], weapon["autoaim_range"], weapon["magnetism_angle"],
            weapon["magnetism_range"]) == ("1.5", "30", "4", "30")
    # (aim assist at every zoom, not only zoomed as CMT's)
    assert weapon["weapon_flags.aim_assists_only_when_zoomed"] == "0"
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    assert weapon["collision_model"] == "weapons\\assault rifle\\assault rifle.model_collision_geometry"
    # a battery of 333 rounds
    assert math.floor(1 / float(weapon["triggers[0].age_generated_per_round"])) == 333
    # the overheat lock: from full heat down to the recovery threshold
    assert abs((1 - 0.2) / 0.5 - 1.6) < 1e-9
    damage = sets(DAMAGE)
    assert (damage["cyborg_energy_shield"], damage["cyborg_armor"]) == ("1.1", "0.2")
    assert (damage["damage_stun"], damage["damage_maximum_stun"], damage["damage_stun_time"]) == ("0.5", "0.5", "0.15")
    assert not {"damage_lower_bound", "damage_upper_bound"} & set(damage)
    assert step(PROJECTILE)["edits"] == [["set", "maximum_range", "30"]]


def test_the_shield_strip_then_finish_loop():
    """a full shield goes in under 0.7 s; a whole heat (about 2 s) leaves a bare player alive with more than half his
    body, which a melee blow (the stock 40 to 60 at x1.4) or a few rounds of another weapon finish"""
    strip, overheat, rounds, body = beam()
    assert 0.6 < strip < 0.7
    assert 1.9 < overheat < 2.2 and 55 <= rounds <= 65
    assert 0 < body < BODY and 35 < body < 45
    # the beam's body rate is a fifth of its shield rate: the stripper, not the killer
    damage = sets(DAMAGE)
    assert float(damage["cyborg_energy_shield"]) / float(damage["cyborg_armor"]) == 5.5


def test_the_narrow_beam():
    """the owner's pick (2026-11-04, from an A/B): the beam's two contrails narrowed (core 0.12, glow 0.08 to 0.14),
    so that thirty beams a second do not wash a shot player's view; nothing else of them changed"""
    base = "cmt/weapons/_shared/focus_beam/contrails/focus_beam_beam_"
    assert sets(base + "center.contrail") == {"point_states[0].width": "0.12", "point_states[1].width": "0.002"}
    assert sets(base + "edge.contrail") == {"point_states[0].width": "0.08", "point_states[1].width": "0.14"}
    verbatim = recipe()["source"]["verbatim"]
    assert base + "center.contrail" not in verbatim and base + "edge.contrail" not in verbatim


def test_the_pad_rumble():
    trigger = step(TRIGGER)
    assert trigger["from"] == ["cmt", "cmt/weapons/evolved/brute_shot/damage_effects/brute_shot_grenade_trigger.damage_effect"]
    fields = dict(trigger["edits"][0][1])
    assert float(fields["low_frequency_vibrate_frequency"]) > 0 and float(fields["high_frequency_vibrate_frequency"]) > 0
    # (a rumble only: no flash, kick or shake)
    for name in ("maximum_intensity", "temporary_camera_impulse_duration", "camera_shaking_duration"):
        assert fields[name] == "0"
    assert sets(WEAPON)["triggers[0].firing_effects[0].firing_damage"] == TRIGGER.replace("/", "\\")


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_focus_beam", "ae\\packs\\focus_beam\\", "focus_beam", "FOCUS BEAM")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("c_pbr,ar", 0, 0)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    # the prompts' and kill feed's picture: CMT's own pickup icon sheet, the focus beam's sequence 3
    assert pack["icon"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons:3"
    assert pack["roots"] == ["bitm:cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons"]
    assert "cmt/ui/hudv4.1/bitmaps/weapons/pickup_icons.bitmap" in data["source"]["verbatim"]
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a focus beam\n" in texts
    assert "ammo_pickup=Picked up %d charge for focus beam\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("CMT", "Teh Lag", "Ifafudafi", "Siliconmaster", "focus beam"):
        assert name in credits
