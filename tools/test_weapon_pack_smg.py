"""The SMG packs' recipes, tools/weapon_pack/smg/recipe.json and tools/weapon_pack/smg_silenced/recipe.json: the
recipe's own rules (make_pack.check_recipe; no prototype), the allowlist exact, nothing taken from outside, the
pack's arguments and texts, the numbers set (each edit with its reason), the silenced one's quiet, and
the art pinned by its pixels and named by its drawings. Everywhere (CI): no game file is read; a run of the recipes
needs the game's maps and CMT's release and is the box's (make_pack.py, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import make_pack  # noqa: E402

KINDS = ("smg", "smg_silenced")
WEAPONS = {"smg": "cmt/weapons/evolved/smg/smg.weapon",
           "smg_silenced": "cmt/weapons/evolved/smg/_silenced/smg_silenced.weapon"}
PROJECTILE = "cmt/weapons/evolved/assault_rifle/projectiles/assault_rifle_bullet.projectile"
DAMAGE = "cmt/weapons/evolved/assault_rifle/damage_effects/assault_rifle_bullet.damage_effect"


def recipe(kind):
    return json.loads((TOOLS / kind / "recipe.json").read_text())


def step(data, layer, tag):
    found = [entry for entry in data["steps"] if entry["layer"] == layer and entry["tag"] == tag]
    assert len(found) == 1, (layer, tag)
    return found[0]


def sets(entry):
    """a step's field edits as {field: value} (its "set" and "sets" edits)"""
    found = {}
    for edit in entry["edits"]:
        if edit[0] == "set":
            found[edit[1]] = edit[2]
        elif edit[0] == "sets":
            found.update(dict(edit[1]))
    return found


@pytest.mark.parametrize("kind", KINDS)
def test_recipe_rules_and_pins(kind):
    text = (TOOLS / kind / "recipe.json").read_text()
    assert not re.search("/" + "home" + r"/|~/|[A-Za-z]:\\\\Users|\.\./", text)
    data = recipe(kind)
    make_pack.check_recipe(data)
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)          # made against no prototype
    battle_rifle = json.loads((TOOLS / "battle_rifle" / "recipe.json").read_text())
    # the same zip, stock map and programs as the rifle's
    for key in ("sha256", "root", "file"):
        assert data["source"][key] == battle_rifle["source"][key]
    assert data["stock_map"] == battle_rifle["stock_map"] and data["tools"] == battle_rifle["tools"]
    expected = data["expected"]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", expected[key]), key
    assert expected["own_tags"] > 100 and expected["external_tags"] > 40
    assert all(set(entry) == {"mode", "size", "pixels_sha256"} for entry in data["art"].values())
    assert all(re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) for entry in data["art_inputs"].values())


@pytest.mark.parametrize("kind", KINDS)
def test_allowlist_is_exact_and_steps_take_nothing_from_outside(kind):
    data = recipe(kind)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist))
    assert set(allowlist) == make_pack.recipe_inputs(data)["cmt"] | set(data["source"]["verbatim"])
    assert not set(data["source"]["verbatim"]) & {entry["from"][1] for entry in data["steps"]
                                                    if entry.get("from", [""])[0] == "cmt"}
    assert not any(name.startswith(("util/", "scripts/")) or ".." in name for name in allowlist)
    assert all(name.startswith("cmt/") for name in allowlist)
    stock = set()
    made = set()
    for entry in data["steps"]:
        for source, name in make_pack.step_inputs(entry):
            if source == "stock":
                stock.add(name)
            elif source == "layer":
                assert name in made
        made.add((entry["layer"], entry["tag"]))
        assert entry["tag"] == entry["tag"].lower() and "\\" not in entry["tag"]
    # the stock map gives the donor its scaffolding and the ammunition HUD its structure, nothing else
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals",
                     "weapons/assault rifle/assault rifle.weapon_hud_interface"}
    assert list(data["pack"]["stock_derived"]) == ["ae\\packs\\%s\\hud\\smg_ammo" % kind]
    assert not any(entry["op"] == "copy" for entry in data["steps"])


@pytest.mark.parametrize("kind", KINDS)
def test_pack_arguments_and_texts(kind):
    data = recipe(kind)
    pack = data["pack"]
    silenced = kind == "smg_silenced"
    assert (pack["id"], pack["prefix"], pack["weapon_key"]) == ("ae_" + kind, "ae\\packs\\%s\\" % kind, kind)
    assert pack["display"] == ("SUPPRESSED SMG" if silenced else "SMG")
    assert pack["weapon"] == WEAPONS[kind][:-len(".weapon")].replace("/", "\\")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("h_smg,ar", 0, 0 if silenced else 2)
    texts = (TOOLS / kind / pack["texts"]).read_text()
    name = "suppressed SMG" if silenced else "SMG"
    assert "weapon_pickup=Picked up %s %s\n" % ("a" if silenced else "an", name) in texts
    assert "ammo_pickup=Picked up %%d rounds for %s\n" % name in texts and texts.count("%") == 1
    credits = (TOOLS / kind / pack["credits"]).read_text()
    assert "CMT" in credits and "Beaucephal" in credits and "Xalener" in credits
    arguments = make_pack.builder_arguments(data, "donor.map", "names.txt", "out.aepack", TOOLS / kind)
    assert arguments[arguments.index("--icon") + 1] == "ae_pack\\hud\\smg_meter:0"
    assert arguments[arguments.index("--replaces") + 1] == str(pack["replaces"])


@pytest.mark.parametrize("kind", KINDS)
def test_numbers_are_halo_2s_and_the_owners_tuning(kind):
    """the brief of 2026-10-21 (the numbers, written down): each set in one fix-ae edit"""
    data = recipe(kind)
    silenced = kind == "smg_silenced"
    weapon = sets(step(data, "fix-ae", WEAPONS[kind]))
    assert weapon["triggers[0].maximum_rate_of_fire"] == "15 15"
    # (the silenced one: the silenced SMG's numbers, 2026-10-21: a magazine of 48, 144 and 144)
    magazine, initial, reserve = ("48", "144", "144") if silenced else ("60", "180", "180")
    assert weapon["magazines[0].rounds_loaded_maximum"] == magazine and weapon["magazines[0].rounds_reloaded"] == magazine
    assert weapon["magazines[0].rounds_total_initial"] == initial
    assert weapon["magazines[0].rounds_reserved_maximum"] == reserve
    # (the SMG widened in play, owner 2026-10-21: "a little less tight at range"; the silenced one tight at first,
    # wide after 2 s of fire)
    assert weapon["triggers[0].error_angle"] == ("0.25 5.5" if silenced else "1.5 5.5")
    assert weapon["triggers[0].minimum_error"] == ("0.05" if silenced else "1")
    assert (weapon["triggers[0].error_acceleration_time"], weapon["triggers[0].error_deceleration_time"]) == (
        ("2", "0.25") if silenced else ("0.8", "0.2"))
    # (the silenced one zooms: one level of 2x; the SMG keeps CMT's none)
    assert ((weapon.get("zoom_levels"), weapon.get("zoom_magnification_range")) ==
            (("1", "2 2") if silenced else (None, None)))
    # the melee the stock assault rifle's (CMT's took about five blows from the front)
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    assert weapon["player_melee_response"] == "weapons\\assault rifle\\melee_response.damage_effect"
    assert (weapon["autoaim_angle"], weapon["autoaim_range"], weapon["magnetism_angle"],
            weapon["magnetism_range"]) == ("6", "12", "12", "18")
    assert weapon["triggers[0].projectiles_between_contrails"] == "3"
    assert weapon["triggers[0].firing_noise"] == ("silent" if silenced else "loud")
    assert weapon["collision_model"] == "weapons\\assault rifle\\assault rifle.model_collision_geometry"
    assert weapon["weapon_flags.enables_integrated_night_vision"] == "0"
    projectile = step(data, "fix-ae", PROJECTILE)
    bullet = sets(projectile)
    # the close-range falloff (owner 2026-10-21): the round slows to 160 and its damage with it, over 2 units
    # (the SMG) or 3.5 (the silenced one: 4.4 by about 9.5 units); Spartans' armour and skin
    # take the stock assault rifle's smaller impacts
    assert {key: bullet[key] for key in ("initial_velocity", "final_velocity", "maximum_range",
                                          "air_damage_range")} == {
        "initial_velocity": "400", "final_velocity": "160", "maximum_range": "40",
        "air_damage_range": "25.4 28.9" if silenced else "25.4 27.4"}
    assert bullet["projectile_material_response[24].default_effect"] == \
        "weapons\\assault rifle\\effects\\impact h skin.effect"
    # the silenced one's bullet carries no tracer: the contrail attachment is taken off
    assert (["erase", "attachments[*]"] in projectile["edits"]) == silenced
    # 12 (the SMG) or 11 (the silenced one) close, 4.4 once the round has slowed
    lower, upper = ("4.4", "11") if silenced else ("4.4", "12")
    assert sets(step(data, "fix-ae", DAMAGE)) == {"damage_lower_bound": lower, "damage_upper_bound": upper + " " + upper}
    for entry in data["steps"]:
        if entry["layer"] == "fix-ae" and entry["op"] == "edit":
            assert len(entry.get("why", "")) > 30, entry["tag"]


@pytest.mark.parametrize("kind", KINDS)
def test_art_is_ours_and_named_by_its_drawings(kind):
    data = recipe(kind)
    assert make_pack.art_drawing_problems(data) == []
    drawings = data["art_drawings"]
    assert drawings["smg_reticle.png"] == {"draw": "reticle", "form": "smg"}
    assert drawings["smg_meter_smooth.png"]["layout"] == drawings["smg_alphas_smooth.png"]["layout"]
    layout = drawings["smg_meter_smooth.png"]["layout"]
    assert layout["columns"] * layout["rows"] == 60
    assert drawings["smg_icon.png"]["width"] == (86 if kind == "smg_silenced" else 60)
    assert all(entry["root"] == "cmt" and entry["kind"] == "geometry" for entry in data["art_inputs"].values())
    # the crosshair is our reticle in CE's colour, red on target, as the rifle's; never a hit marker
    hud = sets(step(data, "fix-ae", "cmt/ui/hudv4.1/weapons/evolved/human/%s.weapon_hud_interface" % kind))
    assert hud["crosshairs[0].crosshair_bitmap"] == "ae_pack\\hud\\smg_reticle.bitmap"
    assert hud["crosshairs[0].crosshair_overlays[0].default_color"] == "0 40 150 255"
    assert hud["crosshairs[0].crosshair_overlays[0].flashing_color"] == "0 255 0 0"


def test_the_two_packs_share_their_art_but_the_icon():
    smg, silenced = recipe("smg"), recipe("smg_silenced")
    for name in ("smg_meter_smooth.png", "smg_alphas_smooth.png", "smg_reticle.png"):
        assert smg["art"][name] == silenced["art"][name]
    assert smg["art"]["smg_icon.png"] != silenced["art"]["smg_icon.png"]
