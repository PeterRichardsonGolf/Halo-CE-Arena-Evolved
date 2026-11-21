"""The grenade launcher pack's recipe, tools/weapon_pack/grenade_launcher/recipe.json: ZTeam's grenade launcher (the
ZTeam tag pack's grenade_launcher/hr) as an Xbox weapon: the recipe's own rules (no prototype), the allowlist (ZTeam's
own tags of the launcher and nothing of its EMP grenade, its shot-down chain or its HUD panel chain), the models
converted, one trigger, a grenade that bursts on its fuse or on a player only, our own numbers, the stand-in message
icon (CMT's brute shot picture), the pack's arguments, texts and credits, and its TEST RUN list. Everywhere (CI): no
game file is read; a run of the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "grenade_launcher"
GL = "zteam/objects/weapons/single/grenade_launcher/hr/"
WEAPON = GL + "grenade_launcher.weapon"
GRENADE = GL + "grenade_default_initial.projectile"
BURST = GL + "grenade_default_explosion.damage_effect"
HUD = "zteam/hud/temp/weapon_interfaces/test/h3_brute_shot.weapon_hud_interface"
OWN_HUD = "grenade_launcher/hud/grenade_launcher.weapon_hud_interface"
ZTEAM_KICK = "zteam/shared/damage_effects/trigger/trigger_rifle_kickback_strong.damage_effect"
KICK = GL + "effects/trigger.damage_effect"
LIST = ROOT / "tools" / "ae_test" / "test_runs" / "pack-grenade-launcher.txt"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(tag):
    return [entry for entry in recipe()["steps"] if entry["tag"] == tag][0]


def edits(tag):
    return {edit[1]: edit[2] for edit in step(tag)["edits"] if edit[0] == "set"}


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip" and data["source"]["root"] == ""
    assert data["source"]["file"] == "zteam_Tag_Pack.zip"
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("zteam/") for name in allowlist)
    assert WEAPON in allowlist and HUD in allowlist
    # (nothing of the EMP grenade, the grenade's shot-down chain, the bounce's detonator or ZTeam's panel chain)
    assert not [name for name in allowlist if re.search("_emp[_.]|_final\\.|_harmless|_detonate|detonator|indirect|"
                                                        "hud/temp/master|melee_strike|weapon\\.material_effects", name)]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert data["expected"]["pack_size"] > 0 and data["expected"]["own_tags"] > 0


def test_only_what_an_xbox_map_needs_and_our_fixes():
    data = recipe()
    models = sorted(entry["tag"] for entry in data["steps"] if entry["op"] == "model")
    assert models == sorted(GL + "models/" + name + ".model" for name in
                            ("grenade_launcher_tp", "grenade_launcher_fp", "grenade"))
    assert [entry["tag"] for entry in data["steps"] if entry["op"] == "multipurpose"] == [
        GL + "bitmaps/grenade_launcher_multipurpose.bitmap"]
    assert data["pack"]["stock_derived"] == {}
    flash = step("zteam/effects/particle_systems/explosions/flash_explosion/flash_explosion_small.particle_system")
    assert flash["edits"] == [["set", "particle_types[0].complex_sprite_render_modes", "simple"]]
    # (the bounce makes no damage, and the grenade's one point of body cannot be lost: no shot-down detonation)
    assert step(GL + "effects/grenade_deflect.effect")["edits"] == [["erase", "events[1]"]]
    # (no collision model: nothing hits the grenade, it has no body to lose; a null reference, the class alone)
    assert edits(GRENADE)["collision_model"] == ".model_collision_geometry"
    assert GL + "collisions/grenade_default.model_collision_geometry" not in recipe()["source"]["allowlist"]


def test_one_trigger_and_the_weapons_numbers():
    weapon_step = step(WEAPON)
    assert ["erase", "triggers[1]"] in weapon_step["edits"]
    weapon = edits(WEAPON)
    assert (weapon["triggers[0].charging_time"], weapon["triggers[0].charged_time"]) == ("0", "0")
    assert weapon["triggers[0].maximum_rate_of_fire"] == "1.1 1.1"
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_reloaded"],
            weapon["magazines[0].rounds_total_initial"], weapon["magazines[0].rounds_reserved_maximum"]) == (
        "6", "6", "12", "24")
    # (hold to keep the grenade from bursting, let go to burst it: ae_tracked_grenade.c reads the flag)
    assert weapon["triggers[0].flags.tracks_fired_projectile"] == "1"
    assert weapon["model"] == GL + "models/grenade_launcher_tp.model"
    assert weapon["first_person_model"] == GL + "models/grenade_launcher_fp.model"
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    grenade = edits(GRENADE)
    assert (grenade["initial_velocity"], grenade["final_velocity"], grenade["air_gravity_scale"]) == ("20", "20", "1.3")
    assert grenade["timer"] == "0.8 0.8" and grenade["model"] == GL + "models/grenade.model"
    burst = edits(BURST)
    assert (burst["radius"], burst["damage_aoe_core_radius"]) == ("0.6 1.6", "0.3")
    full = float(burst["damage_lower_bound"])
    assert burst["damage_upper_bound"] == "%g %g" % (full, full)
    # (a 75 + 75 player: one close burst strips the shield and leaves the body alive, two kill)
    assert 75 <= full < 150 <= 2 * full


def test_haptics():
    # (ZTeam's kick is a file its weapons share: the pack edits its own copy, under its own name, and its weapon names it)
    assert step(KICK)["from"] == ["source", ZTEAM_KICK] and not [entry for entry in recipe()["steps"]
                                                                 if entry["tag"] == ZTEAM_KICK]
    assert edits(WEAPON)["triggers[0].firing_effects[0].firing_damage"] == KICK.replace("/", "\\")
    kick = step(KICK)
    pairs = dict(kick["edits"][0][1])
    assert float(pairs["low_frequency_vibrate_frequency"]) > 0 and float(pairs["high_frequency_vibrate_frequency"]) > 0


def test_no_picture_the_name_instead():
    """no licensed picture of a grenade launcher exists: the pack names no message icon and its own HUD no messaging
    icon (sequence -1), which ae_pack_names.c reads as "write my name" (prompts, kill feed, HUD weapon picture)"""
    data = recipe()
    # (ZTeam's leftover test path is taken by other packs with other content: the HUD is made under the pack's own)
    assert step(OWN_HUD)["from"] == ["source", HUD] and not [entry for entry in data["steps"] if entry["tag"] == HUD]
    assert edits(WEAPON)["hud_interface"] == OWN_HUD.replace("/", "\\")
    assert step(OWN_HUD)["edits"] == [["set", "child_hud", "ui\\hud\\master rounds.weapon_hud_interface"],
                                  ["set", "sequence_index", "-1"], ["erase", "static_elements[*]"],
                                  ["erase", "meter_elements[*]"], ["erase", "overlay_elements[*]"]]
    pack = data["pack"]
    assert "icon" not in pack and "roots" not in pack
    assert not [entry for entry in data["steps"] if "hud_globals" in entry["tag"] or "pickup_icons" in entry["tag"]]
    assert "extra_sources" not in data and "derived" not in data


def test_the_gun_screens_identifier_cleared():
    """the gun screen's icon sheet is ZTeam's with the identifier beside the arrows cleared: one rectangle, pinned by
    its pixels, compiled again as ZTeam's DXT3"""
    data = recipe()
    screen = GL + "bitmaps/grenade_launcher_display_icons.bitmap"
    assert data["art_drawings"]["gl_screen.png"] == {"draw": "screen_blank", "input": "screen_icons.bitmap",
                                                     "regions": [[40, 42, 127, 56]]}
    assert data["art_scripts"]["gl_screen.png"] == "screen_blank.py" and make_pack.DRAWINGS["screen_blank"] == \
        "screen_blank.py"
    assert data["art_inputs"]["screen_icons.bitmap"]["tag"] == screen
    assert re.fullmatch(r"[0-9a-f]{64}", data["art"]["gl_screen.png"]["pixels_sha256"])
    bitmap = step(screen)
    assert bitmap["op"] == "bitmap" and bitmap["art"] == ["gl_screen.png"] and "dxt3" in bitmap["arguments"]
    assert screen not in data["source"]["verbatim"]


def test_screen_blank_clears_only_its_rectangles():
    import numpy as np
    import screen_blank

    pixels = np.full((4, 6, 4), 200, np.uint8)
    original = screen_blank.hek.first_bitmap_rgba
    screen_blank.hek.first_bitmap_rgba = lambda tag: (6, 4, pixels.tobytes())
    try:
        out = screen_blank.picture(b"", [(1, 1, 2, 2)])
        assert out[1:3, 1:3].max() == 0 and out[0].min() == 200 and out[3].min() == 200 and out[:, 3:].min() == 200
        for bad in [(0, 0, 6, 1), (2, 2, 1, 1), (0, 0, 1)]:
            try:
                screen_blank.picture(b"", [bad])
                assert False, bad
            except screen_blank.Refused:
                pass
    finally:
        screen_blank.hek.first_bitmap_rgba = original


def test_pack_arguments_texts_and_credits():
    pack = recipe()["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_grenade_launcher", "ae\\packs\\grenade_launcher\\", "grenade_launcher", "GRENADE LAUNCHER")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("gl,ar", 0, 0)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a grenade launcher\n" in texts
    assert "ammo_pickup=Picked up %d grenades for grenade launcher\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("ZTeam", "halomaps.org", "names no individual authors", "written where a picture would be"):
        assert name in credits
    assert "NOBLE" not in credits


def test_the_test_run_list_gives_slot_22():
    text = LIST.read_text()
    assert "list pack-grenade-launcher\n" in text
    assert text.count("set debug.network_test_cache_weapon 22\n") == 4
