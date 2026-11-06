"""The auto-shotgun pack's recipe, tools/weapon_pack/auto_shotgun/recipe.json: ZTeam's modern combat shotgun (the ZTeam
tag pack's shotgun/h3_hunter) as an Xbox weapon with a semi-automatic action: the recipe's own rules (no prototype), the
allowlist (ZTeam's own tags of the shotgun and nothing else: no stroke sound, no third-person animation graph), the
model under a third-person and a first-person tag, the first-person animations made by auto_shotgun_anims.py (a derived
file pinned by SHA-256), the pellet, firing effect and kick built from the stock shotgun's (named as stock-derived), the
weapon's own numbers, the pack's arguments, texts and credits. Everywhere (CI): no game file is read; a run of the recipe
is the box's (make_pack.py --zip --derive-python, docs/weapon-packs.md)."""
import ast
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "auto_shotgun"
H = "zteam/objects/weapons/single/shotgun/h3_hunter/"
ANTR = H + "animations/shotgun_fp_spartan.model_animations"
SCRIPT = TOOLS / "auto_shotgun_anims.py"


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
            found.update(dict(edit[1]))
    return found


def test_recipe_rules_and_allowlist():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip" and data["source"]["root"] == ""
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("zteam/") for name in allowlist)
    # (nothing of ZTeam's flashlight, impacts, particles or pellet: the stock shotgun's are the map's; ZTeam's HUD is the
    # shotgun's own, its rounds panel the map's; no stroke sound but the draw's rack and the reload exit's (the shot's,
    # the posings' and the chamber's are left out) and no third-person animation graph)
    assert not [name for name in allowlist if re.search("flashlight|effects/|pellet\\.|hud/temp/master", name)]
    assert not [name for name in allowlist if re.search("fire_anim|posing|chamber|shotgun_tp\\.model_anim", name)]
    assert "zteam/objects/weapons/single/shotgun/h3/sounds/ready.sound" in allowlist
    assert "zteam/hud/temp/weapon_interfaces/test/h3_shotgun.weapon_hud_interface" in allowlist
    assert H + "shotgun.weapon" in allowlist and ANTR in allowlist and ANTR not in data["source"]["verbatim"]
    for key in ("pack_sha256", "pack_own_sha256", "donor_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])


def test_models_and_stock_derived_pellet():
    data = recipe()
    models = sorted(entry["tag"] for entry in data["steps"] if entry["op"] == "model")
    assert models == [H + "models/shotgun_fp.model"]
    # (ZTeam's weapon wears its one model in both views: the third-person tag is a copy of the converted one)
    assert step(H + "models/shotgun_tp.model")["from"] == ["fix-ae", H + "models/shotgun_fp.model"]
    weapon = edits(H + "shotgun.weapon")
    assert (weapon["model"], weapon["first_person_model"]) == (H + "models/shotgun_tp.model", H + "models/shotgun_fp.model")
    stock = {entry["tag"]: entry["from"][1] for entry in data["steps"] if entry.get("from", [""])[0] == "stock"}
    assert stock[H + "pellet.projectile"] == "weapons/shotgun/pellet.projectile"
    assert stock[H + "pellet.damage_effect"] == "weapons/shotgun/pellet.damage_effect"
    assert stock[H + "effects/fire_shell.effect"] == "weapons/shotgun/effects/shotgun firing.effect"
    assert stock[H + "effects/trigger.damage_effect"] == "weapons/shotgun/trigger.damage_effect"
    prefix = data["pack"]["prefix"] + H.replace("/", "\\")
    assert set(data["pack"]["stock_derived"]) == {prefix + "pellet", prefix + "effects\\fire_shell", prefix + "effects\\trigger"}
    assert weapon["triggers[0].firing_effects[0].firing_damage"] == H.replace("/", "\\") + "effects\\trigger.damage_effect"
    # (the shot is ZTeam's in both of the stock firing effect's sound events)
    shot = edits(H + "effects/fire_shell.effect")
    assert set(shot.values()) == {"zteam/objects/weapons/single/shotgun/h2/sounds/fire.sound"} and len(shot) == 2


def test_semi_automatic_animations_are_derived_and_pinned():
    data = recipe()
    entry = data["derived"]["shotgun_fp_spartan.model_animations"]
    assert entry["script"] == "auto_shotgun_anims.py" and re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])
    assert data["art_inputs"][entry["input"]]["tag"] == ANTR
    assert (step(ANTR)["op"], step(ANTR)["derived"]) == ("derived", "shotgun_fp_spartan.model_animations")
    # (no third-person graph: ZTeam's names shotgun/h3's, a forend stroke)
    assert edits(H + "shotgun.weapon")["animation_graph"] == ".model_animations"


def test_the_animation_script_works_the_action_once():
    """the script's own constants: the fire's three permutations lose the stroke and its sound, the recoil is 18 source
    frames played in 12, the port and shell are moved into it, the draw (its one rack) is not touched, the reload's
    enter and loop play about a fifth faster, the posing slot is the second posing, and the exit (the one stroke) is
    left as ZTeam made it, named by both exit slots"""
    tree = ast.parse(SCRIPT.read_text())
    constants = {node.targets[0].id: ast.literal_eval(node.value) for node in tree.body
                 if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name)
                 and isinstance(node.value, (ast.Constant, ast.Tuple))
                 and all(isinstance(e, ast.Constant) for e in getattr(node.value, "elts", [node.value]))}
    assert constants["FIRE"] == ("first-person fire-1%0", "first-person fire-1%1", "first-person fire-1%2")
    assert (constants["RECOIL_LAST"], constants["RECOIL_FRAMES"]) == (17, 12)
    assert all(frame > constants["RECOIL_LAST"] for frame in constants["PORT"] + constants["SHELL"])
    assert "READY" not in constants and "first-person ready" not in SCRIPT.read_text()   # (ZTeam's draw, one rack)
    # (the shell timing is the animations': ZTeam's 19 and 16 frames about a fifth faster, 0.5 s and 0.43 s)
    assert (constants["ENTER_FRAMES"], constants["LOOP_FRAMES"]) == (15, 13)
    assert 0.75 <= 13 / 16 <= 0.85 and 0.75 <= 15 / 19 <= 0.85
    assert constants["POSING"] == "first-person posing%1" and constants["EXIT"] == "first-person reload-exit-empty"
    assert (constants["SLOT_POSING"], constants["SLOT_EXIT_EMPTY"], constants["SLOT_EXIT_FULL"]) == (1, 24, 25)
    text = SCRIPT.read_text()
    assert 'animations[index[name]].sound = -1' in text
    assert 'raise Refused("the exit lost its sound")' in text
    # (14 frames of fire at 30 a second fit inside the 1.75-a-second cycle)
    assert (constants["RECOIL_FRAMES"] + 2) / 30 < 1 / 1.75


def test_the_weapons_numbers():
    weapon = edits(H + "shotgun.weapon")
    assert weapon["magazines[0].rounds_loaded_maximum"] == "6" and weapon["magazines[0].rounds_reloaded"] == "1"
    # (the tag's reload time agrees with the shell loop's 13 frames, which the engine times a shell by)
    assert abs(float(weapon["magazines[0].reload_time"]) - 13 / 30) < 0.001
    assert (weapon["magazines[0].rounds_total_initial"], weapon["magazines[0].rounds_reserved_maximum"]) == ("12", "24")
    assert weapon["triggers[0].maximum_rate_of_fire"] == "1.75 1.75"
    assert weapon["triggers[0].projectiles_per_shot"] == "12"
    assert weapon["triggers[0].error"] == "0 0" and weapon["triggers[0].error_angle"] == "5 5"
    damage = edits(H + "pellet.damage_effect")
    near, far = float(damage["damage_upper_bound"].split()[0]), float(damage["damage_lower_bound"])
    assert (damage["cyborg_armor"], damage["cyborg_energy_shield"]) == ("1", "1")
    # (12 pellets: up close 2 shots kill a full 150 and 1 never does; slowed, 3 strip the 75 shield and 5 kill, 4 do not)
    assert 2 * 12 * near >= 150 > 12 * near
    assert 3 * 12 * far >= 75 > 2 * 12 * far and 5 * 12 * far >= 150 > 4 * 12 * far
    projectile = edits(H + "pellet.projectile")
    assert (projectile["initial_velocity"], projectile["final_velocity"], projectile["maximum_range"]) == ("750", "90", "15")
    low, high = (float(value) for value in projectile["air_damage_range"].split())
    # (the slowing starts in the first tick: the lower bound at least the first tick's travel)
    assert low >= 750 / 30 and high > low


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_auto_shotgun", "ae\\packs\\auto_shotgun\\", "auto_shotgun", "AUTO SHOTGUN")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("sg,ar", 0, 0)
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up an auto-shotgun\n" in texts
    assert "ammo_pickup=Picked up %d shells for auto-shotgun\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    assert "ZTeam" in credits and "halomaps.org" in credits and "names no individual authors" in credits


def test_zteams_own_hud_and_picture():
    """the shotgun's HUD is ZTeam's own (its rounds panel the map's stock one); the prompts' picture is ZTeam's own
    picture of the shotgun resampled onto a one-sprite sheet (source_icon.py), a root of the pack"""
    data = recipe()
    hud = [entry for entry in data["steps"] if entry["tag"].endswith("h3_shotgun.weapon_hud_interface")][0]
    assert hud["edits"] == [["set", "child_hud", "ui\\hud\\master rounds.weapon_hud_interface"],
                            ["erase", "static_elements[*]"], ["erase", "meter_elements[*]"]]
    assert edits(H + "shotgun.weapon")["hud_interface"] == hud["tag"].replace("/", "\\")
    assert data["art_drawings"]["auto_shotgun_icon.png"]["draw"] == "source_icon"
    assert data["art_inputs"]["shotgun_picture.bitmap"]["tag"] == "zteam/hud/h3/bitmaps/icons/weapons/shotgun.bitmap"
    assert data["pack"]["icon"] == "ae_pack\\hud\\auto_shotgun_icon:0"
    assert data["pack"]["roots"] == ["bitm:ae_pack\\hud\\auto_shotgun_icon"]
