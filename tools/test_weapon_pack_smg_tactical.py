"""The tactical SMG pack's recipe, tools/weapon_pack/smg_tactical/recipe.json: the recipe's own rules
(make_pack.check_recipe), the allowlist exact, the two models made of the source's parts (the ODST arms and the flashlight
cones left out of both: the first-person arms are the stock cyborg's), the zoom overlay drawn from CMT's scope without its
labels, the numbers set and marked provisional, the art pinned. Everywhere (CI): no game file is read; a run of the recipe
needs the game's maps and CMT's release and is the box's (make_pack.py, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import make_pack  # noqa: E402
import smg_scope  # noqa: E402

WEAPON = "cmt/weapons/evolved/_egg/smg_tactical/smg_tactical.weapon"
MODEL_TP = "cmt/weapons/evolved/_egg/smg_tactical/smg_tactical_tp.model"
MODEL_FP = "cmt/weapons/evolved/_egg/smg_tactical/smg_tactical_fp.model"
HUD = "cmt/ui/hudv4.1/weapons/evolved/human/smg_super.weapon_hud_interface"


def recipe():
    return json.loads((TOOLS / "smg_tactical" / "recipe.json").read_text())


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


def test_recipe_rules_and_pins():
    text = (TOOLS / "smg_tactical" / "recipe.json").read_text()
    assert not re.search("/" + "home" + r"/|~/|[A-Za-z]:\\\\Users|\.\./", text)
    data = recipe()
    make_pack.check_recipe(data)
    silenced = json.loads((TOOLS / "smg_silenced" / "recipe.json").read_text())
    # the same zip, stock map and programs as the SMG packs
    for key in ("sha256", "root", "file"):
        assert data["source"][key] == silenced["source"][key]
    assert data["stock_map"] == silenced["stock_map"] and data["tools"] == silenced["tools"]
    expected = data["expected"]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", expected[key]), key
    assert all(set(entry) == {"mode", "size", "pixels_sha256"} for entry in data["art"].values())
    assert all(re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) for entry in data["art_inputs"].values())


def test_allowlist_is_exact():
    data = recipe()
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist))
    assert set(allowlist) == make_pack.recipe_inputs(data)["cmt"] | set(data["source"]["verbatim"])
    assert all(name.startswith("cmt/") for name in allowlist)
    stock = {name for entry in data["steps"] for kind, name in make_pack.step_inputs(entry) if kind == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals",
                     "weapons/assault rifle/assault rifle.weapon_hud_interface"}


def test_pack_arguments_and_texts():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_smg_tactical", "ae\\packs\\smg_tactical\\", "smg_tactical", "TACTICAL SMG")
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("h_smg,ar", 0, 0)
    texts = (TOOLS / "smg_tactical" / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a tactical SMG\n" in texts
    assert "ammo_pickup=Picked up %d rounds for tactical SMG\n" in texts and texts.count("%") == 1
    assert "CMT" in (TOOLS / "smg_tactical" / pack["credits"]).read_text()
    assert "PROVISIONAL" in data["about"]


def test_the_models_are_the_source_without_their_last_parts():
    data = recipe()
    world = step(data, "fix-ae", MODEL_TP.replace(".model", ".gbxmodel"))
    first = step(data, "fix-ae", MODEL_FP.replace(".model", ".gbxmodel"))
    assert world["op"] == first["op"] == "model_parts" and world["from"] == first["from"]
    assert world["from"][1].endswith("smg_tactical.gbxmodel")
    cones = ["cmt/weapons/evolved/_egg/smg_tactical/shaders/smg_tactical_light_front",
             "cmt/weapons/evolved/_egg/smg_tactical/shaders/smg_tactical_light_side"]
    # (the cones and the arms, the ODST rubber material, go from both: the first-person arms are the stock cyborg's)
    arms = ["cmt/characters/_shared/marine/_odst/shaders/marine_odst_base_rubber"]
    assert first["drop_last"] == world["drop_last"] == 3 and first["drops"] == world["drops"] == arms + cones
    assert not any("marine" in name for name in data["source"]["allowlist"])
    # (each is converted to the Xbox model under its own name; the weapon names them)
    for name in (MODEL_TP, MODEL_FP):
        converted = step(data, "fix-xbox", name)
        assert converted["op"] == "model" and converted["from"] == ["fix-ae", name.replace(".model", ".gbxmodel")]
    weapon = sets(step(data, "fix-ae", WEAPON))
    assert weapon["model"] == MODEL_TP and weapon["first_person_model"] == MODEL_FP
    assert ["erase", "attachments[1]"] in step(data, "fix-ae", WEAPON)["edits"]       # CMT's flashlight


def test_numbers_are_the_first_set_and_say_so():
    data = recipe()
    entry = step(data, "fix-ae", WEAPON)
    weapon = sets(entry)
    assert "PROVISIONAL" in entry["why"]
    assert weapon["triggers[0].maximum_rate_of_fire"] == "13 13"
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_reloaded"]) == ("48", "48")
    assert (weapon["magazines[0].rounds_total_initial"], weapon["magazines[0].rounds_reserved_maximum"]) == ("144", "144")
    assert weapon["zoom_levels"] == "1" and weapon["zoom_magnification_range"] == "2 2"
    assert weapon["collision_model"] == "weapons\\assault rifle\\assault rifle.model_collision_geometry"
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    assert weapon["weapon_flags.enables_integrated_night_vision"] == "0"
    for tag in ("cmt/weapons/evolved/assault_rifle/projectiles/assault_rifle_bullet.projectile",
                "cmt/weapons/evolved/assault_rifle/damage_effects/assault_rifle_bullet.damage_effect"):
        assert "PROVISIONAL" in step(data, "fix-ae", tag)["why"]
    for entry in data["steps"]:
        if entry["layer"] == "fix-ae" and entry["op"] in ("edit", "model_parts"):
            assert len(entry.get("why", "")) > 30, entry["tag"]


def test_the_zoom_overlay_is_cmts_lines_without_labels():
    data = recipe()
    assert make_pack.art_drawing_problems(data) == []
    drawings = data["art_drawings"]
    assert drawings["smg_scope_details.png"]["draw"] == "smg_scope_details"
    assert drawings["smg_scope_mask.png"]["draw"] == "smg_scope_mask"
    assert data["art"]["smg_scope_details.png"]["size"] == [512, 512]
    assert data["art"]["smg_scope_mask.png"]["size"] == [2048, 512]
    hud = sets(step(data, "fix-ae", HUD))
    assert hud["crosshairs[0].crosshair_bitmap"] == "ae_pack\\scope\\smg_scope_mask.bitmap"
    assert hud["crosshairs[1].crosshair_bitmap"] == "ae_pack\\scope\\smg_scope_details.bitmap"
    assert hud["crosshairs[2].crosshair_bitmap"] == "ae_pack\\hud\\smg_reticle.bitmap"
    assert ["erase", "crosshairs[3]"] in step(data, "fix-ae", HUD)["edits"]


def test_the_scope_art_erases_the_labels_and_keeps_the_ring():
    alpha = np.ones((512, 512), np.float32)
    details = np.asarray(smg_scope.details(alpha))
    assert details.shape == (512, 512, 4) and (details[..., 0] == details[..., 3]).all()
    # (the label's and the small print's places, centred: erased; the ring's own pixels kept)
    top, left = 256 - smg_scope.CENTRE[0], 256 - smg_scope.CENTRE[1]
    for (row0, row1), (column0, column1) in smg_scope.ERASED:
        assert details[top + row0:top + row1, left + column0:left + column1, 3].max() == 0
    assert details[top + 140, left + 140, 3] == 255
    mask = np.asarray(smg_scope.mask(np.full((512, 1024), 0.5, np.float32)))
    assert mask.shape == (512, 2048, 4) and (mask[..., :3] == 255).all()
