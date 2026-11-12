"""The heavy revolver pack's recipe, tools/weapon_pack/revolver/recipe.json: ZTeam's heavy revolver as its h3b model
(untextured as released) dressed with ZTeam's h3 textures re-made for it (revolver_art.py over revolver_texels.py's
texel map: lights carried, dark red plates on the drum and side panel), on ZTeam's h3 weapon tag,
animations, collision, sounds and HUD, as an Xbox weapon tuned as a heavy close-to-mid sidearm firing one slow slug;
the slug, firing effect, kick and melee built from the stock pistol's. Everywhere (CI): no game file is read; a run of
the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "revolver"
H3 = "zteam/objects/weapons/single/mauler/h3/"
H3B = "zteam/objects/weapons/single/mauler/h3b/"
HUD = "zteam/hud/temp/weapon_interfaces/test/h3_mauler.weapon_hud_interface"
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


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip" and data["source"]["root"] == ""
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and all(name.startswith("zteam/") for name in allowlist)
    # (nothing of ZTeam's light, pellet, impacts, particles or HUD chain: the stock pistol's are the map's)
    assert not [name for name in allowlist if re.search("flashlight|muzzle|pellet|effects/|hud/temp/master", name)]
    assert HUD in allowlist and H3 + "mauler.weapon" in allowlist
    assert {H3B + "models/mauler_tp.gbxmodel", H3B + "models/mauler_fp.gbxmodel"} <= set(allowlist)
    # (ZTeam's h3 models are not in it: the h3b models are the revolver's)
    assert not [name for name in allowlist if name.startswith(H3 + "models/")]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key]) and set(data["expected"][key]) != {"0"}
    assert re.fullmatch(r"[0-9a-f]{64}", data["derived"]["h3b_texels.npy"]["sha256"])
    for entry in data["art"].values():
        assert re.fullmatch(r"[0-9a-f]{64}", entry["pixels_sha256"]) and set(entry["pixels_sha256"]) != {"0"}


def test_the_h3b_models_bind_the_revolvers_shader():
    data = recipe()
    models = sorted(entry["tag"] for entry in data["steps"] if entry["op"] == "model")
    assert models == [H3B + "models/mauler_fp.model", H3B + "models/mauler_tp.model"]
    shader = H3B + "shaders/revolver_metal.shader_model"
    for view in ("tp", "fp"):
        gbx = H3B + "models/mauler_%s.gbxmodel" % view
        assert step(gbx)["from"] == ["source", gbx]
        assert edits(gbx) == {"shaders[0].shader": shader.replace("/", "\\")}
        assert step(gbx[:-len(".gbxmodel")] + ".model")["from"] == ["fix-ae", gbx]
    weapon = edits(H3 + "mauler.weapon")
    assert weapon["model"] == H3B + "models/mauler_tp.model"
    assert weapon["first_person_model"] == H3B + "models/mauler_fp.model"


def test_the_art_and_the_shader():
    data = recipe()
    assert data["art_scripts"] == {"revolver_icon.png": "source_icon.py", "revolver_body.png": "revolver_art.py",
                                   "revolver_multi.png": "revolver_art.py"}
    assert data["derived"]["h3b_texels.npy"]["script"] == "revolver_texels.py"
    for name, kind in (("revolver_body.png", "revolver_diffuse"), ("revolver_multi.png", "revolver_multi")):
        drawing = data["art_drawings"][name]
        assert (drawing["draw"], drawing["derived_input"]) == (kind, "h3b_texels.npy")
        assert data["art_inputs"][drawing["input"]]["tag"] == H3 + "bitmaps/mauler.bitmap"
        assert data["art_inputs"][drawing["multi"]]["tag"] == H3 + "bitmaps/mauler_multipurpose.bitmap"
    # (the multipurpose picture is drawn in Halo PC's order and converted to the Xbox's)
    multi = H3B + "bitmaps/revolver_multipurpose.bitmap"
    assert step(multi)["op"] == "multipurpose"
    assert step(multi)["from"] == ["fix-ae", H3B + "bitmaps/revolver_multipurpose_pc.bitmap"]
    shader = edits(H3B + "shaders/revolver_metal.shader_model")
    assert step(H3B + "shaders/revolver_metal.shader_model")["from"] == [
        "source", H3 + "shaders/mauler_metal_black.shader_model"]
    assert shader["base_map"] == (H3B + "bitmaps/revolver.bitmap").replace("/", "\\")
    assert shader["multipurpose_map"] == multi.replace("/", "\\")
    assert shader["parallel_brightness"] == "1"


def test_the_weapons_numbers_and_hit_counts():
    weapon = edits(H3 + "mauler.weapon")
    assert weapon["triggers[0].projectiles_per_shot"] == "1"
    assert weapon["triggers[0].maximum_rate_of_fire"] == "1.67 1.67"
    assert weapon["triggers[0].error_angle"] == "0.15 2"
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_total_initial"],
            weapon["magazines[0].rounds_reserved_maximum"]) == ("8", "24", "32")
    # (the role's own: 8 a drum, 24 to start, 40 at most; magnetism 10 degrees)
    # (the field editor takes degrees, as the other recipes give them)
    assert (weapon["autoaim_angle"], weapon["autoaim_range"]) == ("3", "15")
    assert (weapon["magnetism_angle"], weapon["magnetism_range"]) == ("10", "25")
    damage = edits(H3B + "slug.damage_effect")
    hit = float(damage["damage_lower_bound"]) * float(damage["cyborg_armor"])
    assert damage["damage_upper_bound"] == "%s %s" % (damage["damage_lower_bound"], damage["damage_lower_bound"])
    assert damage["cyborg_armor"] == damage["cyborg_energy_shield"]
    # two strip a full shield, four kill to the body (three do not), the third to the head kills
    assert 2 * hit >= SHIELD > hit and 4 * hit >= SHIELD + BODY > 3 * hit
    # a slug and a blow kill from full, a blow alone does not
    melee = edits(H3B + "effects/melee.damage_effect")
    blow = float(melee["damage_lower_bound"]) * 1.4
    assert blow + hit >= SHIELD + BODY > blow
    # SWAT rules (no shields, headshots kill): two to the body kill, one does not
    assert 2 * hit >= BODY > hit
    projectile = edits(H3B + "slug.projectile")
    assert (projectile["initial_velocity"], projectile["final_velocity"], projectile["maximum_range"]) == (
        "75", "45", "150")
    assert projectile["impact_damage"] == (H3B + "slug.damage_effect").replace("/", "\\")


def test_the_view_model_is_set_back():
    """the first-person animations are ZTeam's with the root node moved (revolver_fp.py, a derived file): forward,
    right and down, so the gun sits off and lower right; the step puts them at ZTeam's tag name"""
    data = recipe()
    anim = H3 + "animations/mauler_fp_spartan.model_animations"
    assert anim not in data["source"]["verbatim"] and anim in data["source"]["allowlist"]
    assert step(anim) ["op"] == "derived" and step(anim)["derived"] == "fp_spartan.model_animations"
    entry = data["derived"]["fp_spartan.model_animations"]
    assert entry["script"] == "revolver_fp.py" and data["art_inputs"][entry["input"]]["tag"] == anim
    assert re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])
    text = (TOOLS / "revolver_fp.py").read_text()
    shift = [float(v) for v in re.search(r"^SHIFT = \(([^)]*)\)", text, re.M).group(1).split(",")]
    assert shift[0] > 0 and shift[1] < 0 and shift[2] < 0 and '"frame bone24"' in text


def test_stock_derived_and_haptics():
    data = recipe()
    stock = {entry["tag"]: entry["from"][1] for entry in data["steps"] if entry.get("from", [""])[0] == "stock"}
    assert stock[H3B + "slug.projectile"] == "weapons/pistol/bullet.projectile"
    assert stock[H3B + "slug.damage_effect"] == "weapons/pistol/bullet.damage_effect"
    assert stock[H3B + "effects/fire_slug.effect"] == "weapons/pistol/effects/fire bullet.effect"
    assert stock[H3B + "effects/trigger.damage_effect"] == "weapons/pistol/trigger.damage_effect"
    assert stock[H3B + "effects/melee.damage_effect"] == "weapons/pistol/melee.damage_effect"
    prefix = data["pack"]["prefix"] + H3B.replace("/", "\\")
    assert set(data["pack"]["stock_derived"]) == {prefix + "slug", prefix + "effects\\fire_slug",
                                                   prefix + "effects\\trigger", prefix + "effects\\melee"}
    weapon = edits(H3 + "mauler.weapon")
    assert weapon["triggers[0].firing_effects[0].firing_damage"] == (H3B + "effects/trigger.damage_effect").replace(
        "/", "\\")
    assert weapon["player_melee_damage"] == (H3B + "effects/melee.damage_effect").replace("/", "\\")
    assert float(edits(H3B + "effects/trigger.damage_effect")["low_frequency_vibrate_frequency"]) > 0


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_revolver", "ae\\packs\\revolver\\", "revolver", "HEAVY REVOLVER")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("hp,ar", 0, 0)
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a heavy revolver\n" in texts
    assert "ammo_pickup=Picked up %d rounds for heavy revolver\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    assert "ZTeam" in credits and "halomaps.org" in credits and "names no individual authors" in credits
    assert "revolver" in (TOOLS / "SHIPPED.txt").read_text().split()


def test_zteams_own_hud_and_picture():
    data = recipe()
    # (the ring reticle round: its height scale its width's, ZTeam's 0.55 drew it as a tall oval)
    # (the eight-shell meter, sequence 7 of ZTeam's sheets, as ZTeam's h3 magnum HUD has it, for the eight-round drum)
    assert edits(HUD) == {"child_hud": "ui\\hud\\master rounds.weapon_hud_interface",
                          "crosshairs[0].crosshair_overlays[0].height_scale": "0.4125",
                          "meter_elements[0].sequence_index": "7", "meter_elements[0].alpha_multiplier": "20",
                          "meter_elements[0].alpha_bias": "0", "static_elements[0].sequence_index": "7",
                          "total_ammo_cutoff": "13", "loaded_ammo_cutoff": "4"}
    assert edits(H3 + "mauler.weapon")["hud_interface"] == HUD.replace("/", "\\")
    icon = data["art_drawings"]["revolver_icon.png"]
    assert (icon["draw"], icon["bitmap"]) == ("source_icon", 5)
    assert data["art_inputs"][icon["input"]]["tag"] == "zteam/hud/h3/bitmaps/icons/weapons_temp.bitmap"
    assert data["pack"]["icon"] == "ae_pack\\hud\\revolver_icon:0"
    assert data["pack"]["roots"] == ["bitm:ae_pack\\hud\\revolver_icon"]


def test_the_art_script_carries_lights_and_plates():
    np = pytest.importorskip("numpy")
    import revolver_art as art
    size = art.SIZE
    base = np.full((size, size, 4), 255, np.float32)
    base[..., :3] = np.linspace(20, 200, size, dtype=np.float32)[None, :, None]
    multi = np.zeros((size, size, 4), np.float32)
    multi[..., 2], multi[..., 3] = 100, 255
    # one of ZTeam's lights: the two slide dots' box on the sheet
    _, x0, y0, x1, y1, point = art.MOVES[0]
    multi[y0 + 4:y0 + 8, x0 + 6:x0 + 10, 1] = 255
    base[y0 + 4:y0 + 8, x0 + 6:x0 + 10, :3] = (200, 160, 60)
    # a side projection: x along the columns, z down the rows; the drum node on the right quarter
    columns = np.linspace(-0.06, 0.12, size, dtype=np.float32)
    rows = np.linspace(0.07, -0.05, size, dtype=np.float32)
    texels = np.zeros((size, size, 4), np.float32)
    texels[..., 0] = columns[None, :]
    texels[..., 1] = 0.01
    texels[..., 2] = rows[:, None]
    texels[..., 3] = np.where(columns[None, :] > 0.055, art.DRUM_NODE, 0)
    body, out_multi = art.pictures(base, multi, texels)
    # the light left its place on the sheet and sits at its point on the model
    assert out_multi[y0 + 4:y0 + 8, x0 + 6:x0 + 10, 1].max() == 0
    x, y = art.nearest_side_texel(texels, point)
    assert out_multi[y - 8:y + 8, x - 16:x + 16, 1].max() == 255
    # the drum is red (red over green), the frame a grey metal
    drum = body[200, int(np.searchsorted(columns, 0.08))]
    frame = body[200, int(np.searchsorted(columns, -0.04))]
    assert drum[0] > drum[1] + 10 and abs(float(frame[0]) - float(frame[1])) < 6
    # no glow but ZTeam's lights: no self-illumination outside the carried light's patch
    glow = out_multi[..., 1].copy()
    glow[y - 8:y + 8, x - 16:x + 16] = 0
    assert glow.max() == 0
    assert body[..., 3].min() == 255 and out_multi[..., 3].min() == 255
