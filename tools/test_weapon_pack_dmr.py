"""The DMR and Evo rifle packs' recipes, tools/weapon_pack/dmr/recipe.json and tools/weapon_pack/evo_rifle/recipe.json:
the recipes' own rules (no prototype), the allowlist exact, nothing of the game's or CMT's committed, the packs'
arguments and texts, CMT credited, the zoom overlay's pictures (both our own drawing, evo_scope.py),
the sight's dot and the Evo's reticle drawn by our scripts and pinned by their pixels, the Evo's model made by a script
from CMT's own and CuiGames' H02 Holo (read from the kit's zip at build time, never kept here) and pinned by its bytes. Everywhere (CI): no game file is
read; a run of a recipe is the box's (make_pack.py, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import make_pack  # noqa: E402

KINDS = {"dmr": ("ae_dmr", "DMR", "dmr", 3), "evo_rifle": ("ae_evo_rifle", "EVO RIFLE", "evo_rifle", 2)}
# (both the EVO RIFLE's zoom overlay, our own drawing: the owner 10-30, "the same treatment" for the DMR)
SCOPE_ART = {"dmr": ("evo_scope_details.png", "evo_scope_mask.png"),
             "evo_rifle": ("evo_scope_details.png", "evo_scope_mask.png")}
SCOPE_TAGS = {"dmr": ("evo_scope_mask", "evo_scope_details"), "evo_rifle": ("evo_scope_mask", "evo_scope_details")}
EXTRA_SOURCES = {"dmr": set(), "evo_rifle": {"cuigames"}}


def recipe(kind):
    return json.loads((TOOLS / kind / "recipe.json").read_text())


@pytest.mark.parametrize("kind", sorted(KINDS))
def test_recipe_rules_allowlist_and_pins(kind):
    assert not re.search("/" + "home" + r"/|~/|\.\./", (TOOLS / kind / "recipe.json").read_text())
    data = recipe(kind)
    make_pack.check_recipe(data)
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    assert set(data["source"]["allowlist"]) == make_pack.recipe_inputs(data)["cmt"] | set(data["source"]["verbatim"])
    assert data["source"]["allowlist"] == sorted(set(data["source"]["allowlist"]))
    battle_rifle = json.loads((TOOLS / "battle_rifle" / "recipe.json").read_text())
    for key in ("name", "file", "sha256", "root"):
        assert data["source"][key] == battle_rifle["source"][key]
    assert data["stock_map"] == battle_rifle["stock_map"] and data["tools"]["invader"] == battle_rifle["tools"]["invader"]
    stock = {name for entry in data["steps"] for source, name in make_pack.step_inputs(entry) if source == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals"}
    assert data["pack"]["stock_derived"] == {} and data["pack"]["smooth"] == []
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert data["expected"]["external_tags"] == 54 and data["expected"]["own_tags"] >= 170
    assert make_pack.art_drawing_problems(data) == []
    # the pictures the Bonus Data release's own scope pictures are drawn from: a second zip, pinned
    assert set(data.get("extra_sources", {})) == EXTRA_SOURCES[kind]
    for source in data.get("extra_sources", {}).values():
        assert re.fullmatch(r"[0-9a-f]{64}", source["sha256"])
    for name in SCOPE_ART[kind]:
        assert name in data["art"] and data["art"][name] == recipe("evo_rifle")["art"][name]


@pytest.mark.parametrize("kind", sorted(KINDS))
def test_pack_arguments_and_texts(kind):
    pack = recipe(kind)["pack"]
    ident, display, key, behaviour = KINDS[kind]
    assert (pack["id"], pack["display"], pack["weapon_key"], pack["behaviour"]) == (ident, display, key, behaviour)
    assert pack["prefix"] == "ae\\packs\\%s\\" % kind and pack["weapon"] == "cmt\\weapons\\evolved\\dmr\\dmr"
    assert (pack["labels"], pack["replaces"]) == ("h_br,ar", 0)
    texts = (TOOLS / kind / pack["texts"]).read_text()
    assert re.search(r"^weapon_pickup=Picked up an? .+$", texts, re.M)
    assert re.search(r"^ammo_pickup=Picked up %d rounds for .+$", texts, re.M) and texts.count("%") == 1
    credits = (TOOLS / kind / pack["credits"]).read_text()
    assert "CMT" in credits and "Thank you, CMT." in credits and "didn't make" in credits
    arguments = make_pack.builder_arguments(recipe(kind), "donor.map", "names.txt", "out.aepack", TOOLS / kind)
    assert arguments[arguments.index("--behaviour") + 1] == str(behaviour)
    # (the message icon: CMT's own pickup icon sheet, its DMR's sequence 18; the pack's HUD names the sheet in an
    # off-screen meter element, so the sheet is a tag of the pack)
    data = recipe(kind)
    assert arguments[arguments.index("--icon") + 1] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons:18"
    assert "--root" not in arguments
    assert "cmt/ui/hudv4.1/bitmaps/weapons/pickup_icons.bitmap" in data["source"]["verbatim"]
    hud = [entry for entry in data["steps"] if entry["layer"] == "fix-v3" and entry["tag"] ==
           "cmt/ui/hudv4.1/weapons/evolved/human/dmr.weapon_hud_interface"][0]
    meter = [edit[1] for edit in hud["edits"] if edit[0] == "sets"][0]
    assert ["meter_elements[1].meter_bitmap", "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons.bitmap"] in meter
    assert ["meter_elements[1].sequence_index", "18"] in meter
    assert not any(entry["op"] == "ammo_hud" or entry["tag"].endswith("_meter.bitmap") for entry in data["steps"])
    assert not any(name.endswith("_icon.png") for name in data["art_drawings"])
    assert "pickup icon sheet" in credits


@pytest.mark.parametrize("kind", sorted(KINDS))
def test_scope_overlay_is_our_own(kind):
    data = recipe(kind)
    steps = {(step["layer"], step["tag"]): step for step in data["steps"]}
    hud = steps[("fix-v3", "cmt/ui/hudv4.1/weapons/evolved/human/dmr.weapon_hud_interface")]
    sets = {edit[1]: edit[2] for edit in hud["edits"] if edit[0] == "set"}
    mask, details = SCOPE_TAGS[kind]
    assert sets["crosshairs[0].crosshair_bitmap"] == "ae_pack\\scope\\%s.bitmap" % mask
    assert sets["crosshairs[1].crosshair_bitmap"] == "ae_pack\\scope\\%s.bitmap" % details
    # (CMT's labelled scope pictures are no part of the pack)
    assert not [name for name in data["source"]["allowlist"] if "/scopes/" in name]


def test_evo_has_its_own_reticle_and_its_own_scope():
    data = recipe("evo_rifle")
    steps = {(step["layer"], step["tag"]): step for step in data["steps"]}
    hud = steps[("fix-v3", "cmt/ui/hudv4.1/weapons/evolved/human/dmr.weapon_hud_interface")]
    sets = {edit[1]: edit[2] for edit in hud["edits"] if edit[0] == "set"}
    for crosshair in (2, 3):
        assert sets["crosshairs[%d].crosshair_bitmap" % crosshair] == "ae_pack\\scope\\evo_reticle.bitmap"
    assert data["art_drawings"]["evo_reticle.png"] == {"draw": "evo_reticle"}
    assert data["art_drawings"]["evo_scope_mask.png"] == {"draw": "evo_scope_mask"}
    assert data["art_drawings"]["evo_scope_details.png"] == {"draw": "evo_scope_details"}
    # (the details at two texels a HUD unit: drawn at half scale; the mask at one, its own size)
    assert sets["crosshairs[1].crosshair_overlays[0].width_scale"] == "0.5"
    assert sets["crosshairs[1].crosshair_overlays[0].height_scale"] == "0.5"


def test_dmr_keeps_its_scope_model_and_has_the_bold_reticle():
    data = recipe("dmr")
    assert "derived" not in data
    assert data["art_drawings"]["dmr_reticle.png"] == {"draw": "dmr_reticle", "picture": "reticle_kenney_033.png"}
    assert (TOOLS / "dmr" / "reticle_kenney_033.png").read_bytes() == (
        TOOLS / "battle_rifle" / "reticle_kenney_033.png").read_bytes()
    assert "Kenney" in (TOOLS / "dmr" / "CREDITS.txt").read_text()
    assert "cmt/weapons/_shared/dmr/shaders/dmr_scope.shader_model" in data["source"]["allowlist"]


def test_evo_model_is_made_from_cmts_by_a_script():
    data = recipe("evo_rifle")
    gbx = "cmt/weapons/_shared/dmr/dmr.gbxmodel"
    assert data["derived"]["dmr.gbxmodel"]["script"] == "evo_model.py" and (TOOLS / "evo_model.py").is_file()
    assert data["derived"]["dmr.gbxmodel"]["inputs"] == ["dmr_src.gbxmodel", "h02.glb"]
    # (the sight: CuiGames' H02 Holo, read from the kit's zip at build time; none of the kit's files is kept here)
    glb = data["art_inputs"]["h02.glb"]
    assert (glb["kind"], glb["zip"]) == ("zip", "cuigames") and (TOOLS / "h02_sight.py").is_file()
    assert not [path for path in TOOLS.rglob("*") if path.suffix.lower() in (".glb", ".fbx", ".blend", ".obj")]
    assert "CuiGames" in (TOOLS / "evo_rifle" / "CREDITS.txt").read_text()
    assert re.fullmatch(r"[0-9a-f]{64}", data["derived"]["dmr.gbxmodel"]["sha256"])
    steps = {(step["layer"], step["tag"]): step for step in data["steps"]}
    assert steps[("fix2", gbx)]["op"] == "derived"
    # (the MCC.6 model's display shaders swapped for their classic twins in a layer of their own, then the model)
    assert steps[("fix-mcc6", gbx)]["op"] == "edit" and steps[("fix-mcc6", gbx)]["from"] == ["fix2", gbx]
    assert steps[("fix2", "cmt/weapons/_shared/dmr/dmr.model")]["from"] == ["fix-mcc6", gbx]
    assert [step["tag"] for step in data["steps"]].index(gbx) < [step["tag"] for step in data["steps"]].index(
        "cmt/weapons/_shared/dmr/dmr.model")
    # (the scope is off the model, so its shader and cube map are no part of the pack)
    assert "cmt/weapons/_shared/dmr/shaders/dmr_scope.shader_model" not in data["source"]["allowlist"]
    assert data["art_drawings"]["evo_dot.png"] == {"draw": "evo_dot"}
    assert make_pack.provenance(data, "fix2", "cmt/weapons/_shared/dmr/dmr.model")[0] == "AE script evo_model.py"


@pytest.mark.parametrize("kind", sorted(KINDS))
def test_every_art_script_is_here(kind):
    assert make_pack.unknown_scripts(recipe(kind)) == []


def test_drawings_without_game_files_match_their_pins():
    pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import png_pixels
    import dmr_reticle
    import evo_dot
    import evo_reticle
    import reticle_art
    import tempfile
    with tempfile.TemporaryDirectory() as folder:
        dot = Path(folder) / "dot.png"
        evo_dot.draw().save(dot)
        assert png_pixels.pin(dot) == recipe("evo_rifle")["art"]["evo_dot.png"]
        evo = Path(folder) / "evo_reticle.png"
        evo_reticle.draw().save(evo)
        assert png_pixels.pin(evo) == recipe("evo_rifle")["art"]["evo_reticle.png"]
        import evo_scope
        for name, picture in (("evo_scope_mask.png", evo_scope.mask()), ("evo_scope_details.png", evo_scope.details())):
            picture.save(Path(folder) / name)
            assert png_pixels.pin(Path(folder) / name) == recipe("evo_rifle")["art"][name]
        reticle = Path(folder) / "reticle.png"
        dmr_reticle.draw(reticle_art.from_picture(TOOLS / "dmr" / "reticle_kenney_033.png")).save(reticle)
        assert png_pixels.pin(reticle) == recipe("dmr")["art"]["dmr_reticle.png"]


def test_aepack_reads_the_new_behaviours():
    import aepack
    source = (TOOLS / "aepack.py").read_text()
    assert "behaviour not in range(MAXIMUM_BEHAVIOUR + 1) or power not in (0, 1) or replaces not in REPLACEABLE" in source
    assert aepack.MAXIMUM_BEHAVIOUR == 5
    assert aepack.REPLACEABLE


@pytest.mark.parametrize("kind", sorted(KINDS))
def test_no_panel_of_the_packs_own_draws_under_the_classic_hud_style(kind):
    """CMT's panel (static elements, meter) is out of the DMR and Evo rifle HUD, the rounds are the map's own panel as
    a child HUD, and the icon's off-screen meter stays (the classic style drew CMT's panel top right over the shield)"""
    data = recipe(kind)
    hud = [entry for entry in data["steps"] if entry["layer"] == "fix-v3" and entry["tag"] ==
           "cmt/ui/hudv4.1/weapons/evolved/human/dmr.weapon_hud_interface"][0]
    edits = hud["edits"]
    assert ["erase", "static_elements[*]"] in edits and ["erase", "meter_elements[0]"] in edits
    assert ["set", "child_hud", "ui\\hud\\master rounds.weapon_hud_interface"] in edits
    # the order matters: the icon's meter is copied before CMT's own is erased
    assert [edit[0:2] for edit in edits].index(["copy", "meter_elements[0]"]) < edits.index(["erase", "meter_elements[0]"])
