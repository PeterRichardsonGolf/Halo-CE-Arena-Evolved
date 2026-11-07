"""The sidearm pack's recipe, tools/weapon_pack/sidearm/recipe.json: ZTeam's h2 magnum (the ZTeam tag pack's
magnum/h2) with its trigger guard cut off (the model_cut step), made jet black with a green front post and two green
rear dots (sidearm_art.py and our two masks), as an Xbox weapon tuned as a quick sidearm with a wider bloom, one
light zoom and no scope picture; the round and firing effect built from the stock pistol's. Everywhere (CI): no game
file is read; a run of the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
import json
import math
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "sidearm"
H2 = "zteam/objects/weapons/single/magnum/h2/"
H1 = "zteam/objects/weapons/single/magnum/h1/"
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
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("zteam/") for name in allowlist)
    # (nothing of ZTeam's flashlight, impacts, particles or scope: the stock pistol's are the map's)
    assert not [name for name in allowlist if re.search("flashlight|effects/|hud/temp/master|scope", name)]
    assert "zteam/hud/temp/weapon_interfaces/test/h2_magnum.weapon_hud_interface" in allowlist
    assert H2 + "magnum.weapon" in allowlist
    # (the h2 folder's shaders only: invader-model binds a shader by its name, so no h1 shader of a name may be there)
    assert not [name for name in allowlist if "/shaders/" in name and not name.startswith(H2)
                and name != H1 + "shaders/magnum_bullet.shader_model"]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])


def test_the_guard_is_cut_and_the_models_bind_the_h2_shaders():
    data = recipe()
    cuts = [entry for entry in data["steps"] if entry["op"] == "model_cut"]
    assert sorted(entry["tag"] for entry in cuts) == [H2 + "models/magnum_fp.model", H2 + "models/magnum_tp.model"]
    for entry in cuts:
        assert entry["cuts"] == [[0, 0.5, 3.9, -0.25, 0.25, -1.15, 2.6]]
        assert H2 + "shaders/magnum_metal.shader_model" in entry["shaders"]
        assert H2 + "shaders/magnum_rubber.shader_model" in entry["shaders"]
        assert not [name for name in entry["shaders"] if name.startswith(H1 + "shaders/magnum_m")]
    assert {"invader-recover", "invader-model"} <= set(data["tools"]["invader"]["sha256"])
    weapon = edits(H2 + "magnum.weapon")
    assert weapon["model"] == H2 + "models/magnum_tp.model" and weapon["first_person_model"] == H2 + "models/magnum_fp.model"


def test_the_art_and_the_shaders():
    data = recipe()
    assert {entry["draw"] for entry in data["art_drawings"].values()} == {
        "source_icon", "sidearm_diffuse", "sidearm_multi", "sidearm_lod"}
    body = data["art_drawings"]["sidearm_body.png"]
    assert body["levels"] == [0, 7, 22, 24] and data["art_drawings"]["sidearm_lod.png"]["levels"] == [0, 7, 22, 24]
    assert data["art_drawings"]["sidearm_multi.png"]["specular"] == 0.5
    for name in (body["glow"], body["face"]):
        assert (FOLDER / name).is_file()
    metal = edits(H2 + "shaders/magnum_metal.shader_model")
    assert (metal["perpendicular_brightness"], metal["parallel_brightness"]) == ("0.5", "0.25")
    assert edits(H2 + "shaders/magnum_lod.shader_model") == {"parallel_brightness": "0.25"}
    # (the multipurpose map is compiled in Halo PC's channel order, then reordered for the Xbox)
    assert step(H2 + "bitmaps/magnum_multipurpose.bitmap")["from"] == ["fix-ae", H2 + "bitmaps/magnum_multipurpose_pc.bitmap"]


def test_the_weapons_numbers_and_hit_counts():
    weapon = edits(H2 + "magnum.weapon")
    assert (weapon["zoom_levels"], weapon["zoom_magnification_range"]) == ("1", "1.4 1.4")
    assert weapon["magazines[0].rounds_loaded_maximum"] == "12" and weapon["magazines[0].rounds_reloaded"] == "12"
    assert weapon["triggers[0].maximum_rate_of_fire"] == "5.6 5.6"
    # (a wider bloom than the stock pistol's, which grows 0.2 to 2 degrees over 0.5 s: faster and higher)
    low, high = (float(value) for value in weapon["triggers[0].error_angle"].split())
    assert high > 2 and float(weapon["triggers[0].error_acceleration_time"]) < 0.5
    damage = edits(H2 + "bullet.damage_effect")
    base = float(damage["damage_lower_bound"])
    assert damage["damage_upper_bound"] == "%s %s" % (damage["damage_lower_bound"], damage["damage_lower_bound"])
    on_shield, on_body = base * float(damage["cyborg_energy_shield"]), base * float(damage["cyborg_armor"])
    # six hits strip a full shield; eleven kill from full whether or not a hit's overflow passes through the shield
    assert 5 * on_shield < SHIELD <= 6 * on_shield
    assert 6 + math.ceil(BODY / on_body) == 11
    assert 6 + math.ceil((BODY - (6 * on_shield - SHIELD)) / on_body) == 11
    assert weapon["triggers[0].projectile_object"] == H2.replace("/", "\\") + "bullet.projectile"
    assert edits(H2 + "bullet.projectile")["impact_damage"] == H2.replace("/", "\\") + "bullet.damage_effect"


def test_haptics_and_the_round_are_stock_derived():
    data = recipe()
    stock = {entry["tag"]: entry["from"][1] for entry in data["steps"] if entry.get("from", [""])[0] == "stock"}
    assert stock[H2 + "bullet.projectile"] == "weapons/pistol/bullet.projectile"
    assert stock[H2 + "bullet.damage_effect"] == "weapons/pistol/bullet.damage_effect"
    assert stock[H2 + "effects/trigger.damage_effect"] == "weapons/pistol/trigger.damage_effect"
    prefix = data["pack"]["prefix"] + H2.replace("/", "\\")
    assert set(data["pack"]["stock_derived"]) == {prefix + "bullet", prefix + "effects\\fire_bullet",
                                                   prefix + "effects\\trigger"}
    kick = edits(H2 + "effects/trigger.damage_effect")
    assert all(float(kick[key]) > 0 for key in kick) and len(kick) == 4
    assert edits(H2 + "magnum.weapon")["triggers[0].firing_effects[0].firing_damage"] == \
        H2.replace("/", "\\") + "effects\\trigger.damage_effect"
    import pack_done
    assert pack_done.recipe_vibrates(data)


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_sidearm", "ae\\packs\\sidearm\\", "sidearm", "SIDEARM")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("hp,ar", 0, 0)
    texts = (FOLDER / pack["texts"]).read_text()
    assert "menu_name=SIDEARM PISTOL\n" in texts and "weapon_pickup=Picked up a sidearm pistol\n" in texts
    assert "ammo_pickup=Picked up %d rounds for sidearm pistol\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    assert "ZTeam" in credits and "halomaps.org" in credits and "names no individual authors" in credits


def test_zteams_own_hud_without_a_scope_and_picture():
    data = recipe()
    hud = [entry for entry in data["steps"] if entry["tag"].endswith("h2_magnum.weapon_hud_interface")][0]
    # (ZTeam's ring with ticks is out: the two aiming reticles are CMT's pistol HUD's, grafted from it)
    assert hud["edits"][0] == ["set", "child_hud", "ui\\hud\\master rounds.weapon_hud_interface"]
    assert [edit[3:] for edit in hud["edits"][1:] if edit[0] == "graft"] == [["crosshairs[2]", "crosshairs[0]"], ["crosshairs[3]", "crosshairs[1]"]]
    # (and no panel of the pack's own draws under the classic HUD style: ZTeam's static picture and meter are out)
    assert ["erase", "static_elements[*]"] in hud["edits"] and ["erase", "meter_elements[*]"] in hud["edits"]
    assert "zteam/hud/shared/bitmaps/reticles/reticles.bitmap" not in data["source"]["allowlist"]
    assert data["extra_sources"]["tsce"]["file"] == "tsce_1-4_source_release_stock.zip"
    assert "weapon_reticules" in data["art_inputs"]["cmt_weapon_reticules.bitmap"]["member"]
    assert "CMT" in (FOLDER / data["pack"]["credits"]).read_text() and "reticle" in (FOLDER / data["pack"]["credits"]).read_text()
    assert edits(H2 + "magnum.weapon")["hud_interface"] == hud["tag"].replace("/", "\\")
    assert not [name for name in data["source"]["allowlist"] if "scope" in name]
    assert data["art_drawings"]["sidearm_icon.png"]["draw"] == "source_icon"
    assert data["art_inputs"]["magnum_picture.bitmap"]["tag"] == "zteam/hud/h3/bitmaps/icons/weapons/magnum.bitmap"
    assert data["pack"]["icon"] == "ae_pack\\hud\\sidearm_icon:0"
    assert data["pack"]["roots"] == ["bitm:ae_pack\\hud\\sidearm_icon"]


def test_the_art_script_remaps_and_masks():
    np = pytest.importorskip("numpy")
    import sidearm_art
    rng = np.random.default_rng(1)
    rgba = np.concatenate([rng.integers(0, 256, (16, 16, 3)), np.full((16, 16, 1), 255)], axis=2).astype(np.float32)
    light = sidearm_art.remap(rgba, 0, 7, 22, 24)
    assert abs(float(np.percentile(light, 50)) - 7) < 1 and abs(float(light.max()) - 24) < 1e-3
    glow = np.zeros((16, 16), np.float32)
    face = np.zeros((16, 16), np.float32)
    glow[2, 3], face[5, 6] = 255, 255
    body = sidearm_art.diffuse(rgba, glow, face, 0, 7, 22, 24)
    assert tuple(body[2, 3, :3]) == sidearm_art.GLOW and tuple(body[5, 6, :3]) == (7, 7, 7) and body[..., 3].min() == 255
    multi = sidearm_art.multipurpose(rgba, glow, face, 0.5)
    assert multi[2, 3, 1] == 255 and multi[0, 0, 1] == 0 and multi[5, 6, 2] == 0
    assert multi[0, 0, 2] == int(rgba[0, 0, 2] * 0.5 + 0.5) and multi[0, 0, 0] == rgba[0, 0, 0]
