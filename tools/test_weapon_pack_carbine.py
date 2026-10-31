"""The carbine pack's recipe, tools/weapon_pack/carbine/recipe.json: the recipe's own rules (no prototype), the
allowlist exact, nothing taken from outside, the pack's arguments and texts, the numbers set (each edit
with its reason; single shots kept where the barrel names 2 shots per fire), its scope left CMT's and its
aiming reticle ours, the art pinned by its pixels. Everywhere (CI): no game file is read; a run of the recipe is the
box's (make_pack.py, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import make_pack  # noqa: E402

FOLDER = TOOLS / "carbine"
WEAPON = "cmt/weapons/evolved/carbine/carbine.weapon"
HUD = "cmt/ui/hudv4.1/weapons/evolved/covenant/carbine.weapon_hud_interface"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(data, layer, tag):
    found = [entry for entry in data["steps"] if entry["layer"] == layer and entry["tag"] == tag]
    assert len(found) == 1, (layer, tag)
    return found[0]


def sets(entry):
    found = {}
    for edit in entry["edits"]:
        if edit[0] == "set":
            found[edit[1]] = edit[2]
        elif edit[0] == "sets":
            found.update(dict(edit[1]))
    return found


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    assert set(data["source"]["allowlist"]) == make_pack.recipe_inputs(data)["cmt"] | set(data["source"]["verbatim"])
    assert data["source"]["allowlist"] == sorted(set(data["source"]["allowlist"]))
    stock = {name for entry in data["steps"] for source, name in make_pack.step_inputs(entry) if source == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals",
                     "weapons/assault rifle/assault rifle.weapon_hud_interface"}
    assert list(data["pack"]["stock_derived"]) == ["ae\\packs\\carbine\\hud\\carbine_ammo"]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert make_pack.art_drawing_problems(data) == []
    assert data["art_drawings"]["carbine_reticle.png"] == {"draw": "reticle", "form": "carbine"}
    layout = data["art_drawings"]["carbine_meter_smooth.png"]["layout"]
    assert layout["columns"] * layout["rows"] == 18


def test_pack_arguments_and_texts():
    pack = recipe()["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_carbine", "ae\\packs\\carbine\\", "carbine", "CARBINE")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("c_cb,ar", 0, 0)
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a carbine\n" in texts and "ammo_pickup=Picked up %d rounds for carbine\n" in texts
    assert texts.count("%") == 1
    assert "CMT" in (FOLDER / pack["credits"]).read_text() and "carbine" in (FOLDER / pack["credits"]).read_text()


def test_numbers_are_halo_2s_and_the_owners_tuning():
    data = recipe()
    weapon = step(data, "fix-ae", WEAPON)
    numbers = sets(weapon)
    assert (numbers["magazines[0].rounds_loaded_maximum"], numbers["magazines[0].rounds_total_initial"],
            numbers["magazines[0].rounds_reserved_maximum"]) == ("18", "36", "72")
    # (the owner's tuning in play, 2026-10-21: the rate, no bloom, the BR's round and aim assist reach)
    assert numbers["triggers[0].maximum_rate_of_fire"] == "5 5"                  # one round each 0.2 s
    assert (numbers["triggers[0].error_angle"], numbers["triggers[0].minimum_error"]) == ("0.3 0.3", "0")
    assert (numbers["zoom_levels"], numbers["zoom_magnification_range"]) == ("1", "2 2")
    assert (numbers["autoaim_angle"], numbers["autoaim_range"], numbers["magnetism_angle"],
            numbers["magnetism_range"]) == ("3", "20", "6", "30")
    assert "2 shots per fire" in weapon["why"] and "single shots" in weapon["why"]
    projectile = sets(step(data, "fix-ae", "cmt/weapons/evolved/carbine/projectiles/carbine_bolt.projectile"))
    assert projectile == {"initial_velocity": "1000", "final_velocity": "1000", "maximum_range": "200"}
    damage = sets(step(data, "fix-ae", "cmt/weapons/evolved/carbine/damage_effects/carbine_bolt_impact.damage_effect"))
    assert damage == {"damage_lower_bound": "15.2", "damage_upper_bound": "15.2 15.2"}   # 5 pop a shield, the sixth to the head kills


def test_scope_is_cmts_and_the_reticle_ours():
    hud = step(recipe(), "fix-ae", HUD)
    numbers = sets(hud)
    for index in (2, 3):
        assert numbers["crosshairs[%d].crosshair_bitmap" % index] == "ae_pack\\hud\\carbine_reticle.bitmap"
        assert numbers["crosshairs[%d].crosshair_overlays[0].flashing_color" % index] == "0 255 0 0"
    assert not any(key.startswith(("crosshairs[0]", "crosshairs[1]")) for key in numbers)


def test_carbine_reticle_is_arcs_wedges_and_a_clear_middle():
    pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import reticle_art
    alpha = reticle_art.carbine_reticle().astype(int)
    middle, ring = 256, int(reticle_art.CARBINE_RING)
    for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        assert alpha[middle + dy * ring, middle + dx * ring] > 200                      # the arcs
    outer = int((reticle_art.CARBINE_WEDGE[1] - 8) / 2 ** 0.5)
    assert alpha[middle - outer, middle - outer] > 200 and alpha[middle + outer, middle + outer] > 200   # wedges
    assert alpha[middle - 30:middle + 30, middle - 30:middle + 30].max() == 0
    assert set(reticle_art.FORMS) == {"zoom", "smg", "carbine"}
