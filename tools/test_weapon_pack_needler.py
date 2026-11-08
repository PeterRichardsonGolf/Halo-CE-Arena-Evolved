"""The needler pack's recipe, tools/weapon_pack/needler/recipe.json: the recipe's own rules, the allowlist exact, no
path of a machine, the pack's arguments and texts, CMT credited, the numbers set as the report names them (a
latest-generation needler: 26 / 78 / 78, 12 a second from the first, aim assist 4.25 degrees to 10 wu), the needles
made pink, and the slot the engine gives it. Everywhere (CI): no game file is read; a run of a recipe is the box's
(make_pack.py, docs/weapon-packs.md)."""
import pytest
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import make_pack  # noqa: E402


def recipe():
    return json.loads((TOOLS / "needler" / "recipe.json").read_text())


def edits_of(data, tag):
    found = {}
    for step in data["steps"]:
        if step["tag"] == tag:
            for edit in step.get("edits", ()):
                if edit[0] == "sets":
                    found.update({field: value for field, value in edit[1]})
                elif edit[0] == "set":
                    found[edit[1]] = edit[2]
    return found


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (TOOLS / "needler" / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    assert set(data["source"]["allowlist"]) == make_pack.recipe_inputs(data)["cmt"] | set(data["source"]["verbatim"])
    assert data["source"]["allowlist"] == sorted(set(data["source"]["allowlist"]))
    battle_rifle = json.loads((TOOLS / "battle_rifle" / "recipe.json").read_text())
    # (the 1.4.1c release: the rifles read MCC.6, tools/weapon_pack/mcc6_source.py)
    for key, value in (("name", "CMT's The Silent Cartographer: Evolved 1.4.1c source release"),
                       ("file", "tsce_1-4-1c_source_release.zip"),
                       ("sha256", "311518659c4551342c867da2bb83c4f9c3b8c798604169a4301422970fede8a9"),
                       ("root", "tsce_1-4-1c_source_release/tags/")):
        assert data["source"][key] == value
    assert data["stock_map"] == battle_rifle["stock_map"] and data["tools"]["invader"] == battle_rifle["tools"]["invader"]
    stock = {name for entry in data["steps"] for source, name in make_pack.step_inputs(entry) if source == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals",
                     "weapons/assault rifle/assault rifle.weapon_hud_interface"}
    for key in ("pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert re.fullmatch(r"[0-9a-f]{64}", data["expected"]["donor_sha256"])
    assert make_pack.art_drawing_problems(data) == []


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["display"], pack["weapon_key"], pack["behaviour"]) == ("ae_needler", "NEEDLER", "needler", 0)
    assert pack["prefix"] == "ae\\packs\\needler\\" and pack["weapon"] == "cmt\\weapons\\evolved\\needler\\_needler_mp\\needler_mp"
    assert pack["labels"] == "c_ne,ne,ar" and pack["replaces"] == 9
    # (the message icon: CMT's own pickup icon sheet's needler, sequence 5; no icon of ours)
    assert pack["icon"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons:5"
    assert "cmt/ui/hudv4.1/bitmaps/weapons/pickup_icons.bitmap" in data["source"]["verbatim"]
    ammo = [entry for entry in data["steps"] if entry["op"] == "ammo_hud"][0]
    assert ammo["icon_sheet"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons.bitmap"
    counter = [entry for entry in data["steps"] if entry["op"] == "counter_hud"][0]
    assert counter["icon_sequence"] == 5
    assert "needler_icon.png" not in data["art_drawings"] and data["art_inputs"] == {}
    assert "pickup icon sheet" in (TOOLS / "needler" / pack["credits"]).read_text()
    texts = (TOOLS / "needler" / pack["texts"]).read_text()
    assert re.search(r"^weapon_pickup=Picked up an? .+$", texts, re.M)
    assert re.search(r"^ammo_pickup=Picked up %d needles for .+$", texts, re.M) and texts.count("%") == 1
    credits = (TOOLS / "needler" / pack["credits"]).read_text()
    assert "CMT" in credits and "Thank you, CMT." in credits and "didn't make" in credits
    arguments = make_pack.builder_arguments(data, "donor.map", "names.txt", "out.aepack", TOOLS / "needler")
    assert arguments[arguments.index("--behaviour") + 1] == "0" and "--icon" in arguments


def test_numbers_as_the_report_names_them():
    data = recipe()
    weapon = edits_of(data, "cmt/weapons/evolved/needler/_needler_mp/needler_mp.weapon")
    assert weapon["magazines[0].rounds_loaded_maximum"] == "26" and weapon["magazines[0].rounds_total_initial"] == "78"
    assert weapon["magazines[0].rounds_reserved_maximum"] == "78" and weapon["magazines[0].rounds_reloaded"] == "26"
    assert weapon["triggers[0].maximum_rate_of_fire"] == "12 12" and weapon["triggers[0].acceleration_time"] == "0"
    assert (weapon["autoaim_angle"], weapon["autoaim_range"]) == ("4.25", "10")
    assert (weapon["magnetism_angle"], weapon["magnetism_range"]) == ("12.7", "30")
    # (every pack weapon melees as the stock assault rifle, owner 10-22)
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    needle = edits_of(data, "cmt/weapons/evolved/needler/_needler_mp/projectiles/needler_mp_needle/needler_mp_needle.projectile")
    assert (needle["initial_velocity"], needle["maximum_range"], needle["guided_angular_velocity"]) == ("9.5", "25", "60")
    # (a needle landing 2.0 + stuck 1.57 = 3.57: 20 needles empty a 70 shield)
    impact = edits_of(data, "cmt/weapons/evolved/needler/damage_effects/needler_needle_impact.damage_effect")
    stuck = edits_of(data, "cmt/weapons/evolved/needler/damage_effects/needler_needle_detonation.damage_effect")
    assert abs(float(impact["damage_lower_bound"]) + float(stuck["damage_lower_bound"]) - 3.57) < 1e-9
    assert 20 * 3.57 >= 70 > 19 * 3.57


def test_the_needles_are_pink():
    data = recipe()
    glass = edits_of(data, "cmt/weapons/_shared/needler/shaders/needler_needles.shader_transparent_glass")
    plasma = edits_of(data, "cmt/weapons/_shared/needler/shaders/needler_needles_plasma.shader_transparent_plasma")
    for colour in list(glass.values()) + list(plasma.values()):
        red, green, blue = (float(part) for part in colour.split())
        # (pink: red the strongest, blue above green)
        assert red >= 1.0 and blue > green and green <= 0.75


def test_the_reticle_is_the_needler_form_and_drawn_by_us():
    import reticle_art
    data = recipe()
    assert data["art_drawings"]["needler_reticle.png"] == {"draw": "reticle", "form": "needler"}
    assert data["pack"]["revision"] == 30 and "needler" in reticle_art.FORMS
    # (the drawing itself needs numpy, which the laptop has; the box only assembles packs)
    pytest.importorskip("numpy")
    alpha = reticle_art.needler_reticle()
    size = alpha.shape[0]
    mid = size // 2
    # (two brackets and two triangles, mirrored; nothing in the middle)
    assert alpha[mid, 40:120].max() > 200 and alpha[mid, size - 120:size - 40].max() > 200
    assert alpha[mid, mid - 20:mid + 20].max() == 0 and alpha[10, mid] == 0
    assert abs(int(alpha[:, :mid].sum()) - int(alpha[:, :mid - 1:-1].sum())) < 0.02 * int(alpha.sum())
