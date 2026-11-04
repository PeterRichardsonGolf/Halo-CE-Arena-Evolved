"""The energy sword pack's recipe, tools/weapon_pack/energy_sword/recipe.json: CMT's energy sword from the
stock-compatible TSC:E 1.4 source release (no Open Sauce tags) for the third person, CMT's mesh on the node frames of
ZTeam's first-person sword for the first person, with ZTeam's first-person animations, sounds, HUD, reticle and picture
(an extra source, each file taken as it is). The recipe's own rules (no prototype), the allowlist (CMT's tags only), what
an Xbox map needs done (models converted, glow widgets and the detonation's boom left out, the blade shaders' draw-before-
water flag cleared, the pickup index the stock one's), the flag that deleted the sword on a weapon switch cleared, a
rumble on the melee blow, the pack's arguments, texts and credits. Everywhere (CI): no game file is read; a run of the
recipe is the box's (make_pack.py --zip --extra-zip, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "energy_sword"
WEAPON = "cmt/weapons/evolved/energy_sword/energy_sword.weapon"
MELEE = "cmt/weapons/evolved/energy_sword/damage_effects/energy_sword_melee.damage_effect"
DETONATION = "cmt/weapons/_shared/energy_sword/effects/energy_sword_detonation.effect"
FP = "cmt/weapons/_shared/energy_sword/fp/fp.model"
GRAPH = "zteam/objects/weapons/single/energy_sword/h3_lunge_parry/animations/energy_sword_fp_spartan.model_animations"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(data, tag):
    return [entry for entry in data["steps"] if entry["tag"] == tag][0]


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
    assert all(name.startswith("cmt/") for name in source["allowlist"])
    assert WEAPON in source["allowlist"]
    # (the glow widgets, whose sprite no Xbox map has, are not taken)
    assert not any("/widgets/" in name for name in source["allowlist"])
    assert data["pack"]["stock_derived"] == {}
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])


def test_only_what_an_xbox_map_needs():
    data = recipe()
    ops = {}
    for entry in data["steps"]:
        ops.setdefault(entry["op"], []).append(entry["tag"])
    # (no Open Sauce in this release: nothing stripped; no multipurpose map or Ogg sound: nothing converted; ZTeam's tags
    # are taken as they are: derived copies; the first-person model is CMT's mesh re-rigged and converted)
    assert set(ops) == {"edit", "model", "model_rig", "derived", "bitmap"}
    assert ops["model"] == ["cmt/weapons/_shared/energy_sword/energy_sword.model"]
    assert ops["model_rig"] == [FP]
    assert len(ops["derived"]) == 6
    weapon = step(data, WEAPON)
    # (the weapon's own numbers untouched: its model, the glow widgets, the message index, the first-person tags, and
    # the flag that deleted the sword when it was put away)
    assert {edit[1] for edit in weapon["edits"]} == {"weapon_flags.detonates_when_dropped", "widgets[*]", "model",
                                                       "pickup_text_index", "first_person_model",
                                                       "first_person_animations", "hud_interface"}
    assert ["set", "weapon_flags.detonates_when_dropped", "0"] in weapon["edits"]
    detonation = step(data, DETONATION)
    assert detonation["edits"] == [["erase", "events[0].parts[2]"]] and "elite_sword_boom" in detonation["why"]
    # (the blade shaders are drawn in first person too, which the transparent pass refuses for "draw before water")
    for name in ("energy_sword_blade_top", "energy_sword_blade_bottom"):
        shader = step(data, "cmt/weapons/_shared/energy_sword/shaders/%s.shader_transparent_chicago" % name)
        assert shader["edits"] == [["set", "shader_transparent_chicago_flags.draw_before_water", "0"]]


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_energy_sword", "ae\\packs\\energy_sword\\", "energy_sword", "ENERGY SWORD")
    # (behaviour 0: the lunge finds the sword by its slot, never by this byte; 2 is the EVO RIFLE's)
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("c_swd,ar", 0, 0)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up an energy sword\n" in texts and "%" not in texts
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("CMT", "Teh Lag", "Ifafudafi", "Siliconmaster", "energy_sword", "ZTeam", "different authors",
                 "Kind: community model"):
        assert name in credits
    assert "Banshee" not in credits and "ba2" not in json.dumps(data)


def test_first_person_is_cmts_mesh_on_zteams_rig():
    """the first-person model is CMT's mesh moved onto the node frames of ZTeam's first-person sword ("frame handle",
    "frame blades" 8 units out along the blade), its grip centre and blade tip where ZTeam's are; ZTeam's animations
    (melee, lunge, lunge in the air), taken as they are, with the table's secondary-fire slot the air lunge"""
    data = recipe()
    rig = step(data, FP)
    assert rig["op"] == "model_rig" and rig["from"][1] == "cmt/weapons/_shared/energy_sword/energy_sword.gbxmodel"
    assert rig["rig"]["nodes"] == {"frame blades": [8, 0, 0]}
    assert 1.0 < rig["rig"]["scale"] < 1.1 and len(rig["shaders"]) == 3
    weapon = step(data, WEAPON)
    assert ["set", "first_person_model", FP] in weapon["edits"]
    assert ["set", "first_person_animations", GRAPH] in weapon["edits"]
    assert data["extra_sources"]["zteam"]["file"] == "zteam_Tag_Pack.zip"
    assert re.fullmatch(r"[0-9a-f]{64}", data["extra_sources"]["zteam"]["sha256"])
    assert GRAPH in {entry["member"] for entry in data["art_inputs"].values() if entry["kind"] == "zip"}
    for name, entry in data["derived"].items():
        assert entry["script"] == "copy_input.py" and entry["sha256"] == data["art_inputs"][name]["sha256"]
    air = [entry for entry in data["steps"] if entry["tag"] == GRAPH and entry["op"] == "edit"][0]
    assert air["edits"] == [["set", "first_person_weapons[0].animations[14].animation", "3"]]
    assert not any(name.startswith("zteam/") for name in data["source"]["allowlist"])


def test_the_message_icon_and_reticle_are_zteams():
    """the prompts' picture: ZTeam's picture of the sword (its HUD's weapon picture) resampled by source_icon.py, a root
    of its own; the reticle is ZTeam's, in its HUD (taken as it is)"""
    data = recipe()
    pack = data["pack"]
    assert pack["icon"] == "ae_pack\\hud\\sword_icon:0" and pack["roots"] == ["bitm:ae_pack\\hud\\sword_icon"]
    assert data["art_drawings"]["sword_icon.png"]["draw"] == "source_icon"
    hud = step(data, "ae_pack/donor/hud.hud_globals")
    assert hud["edits"] == [["set", "icon_bitmap", "ae_pack\\hud\\sword_icon.bitmap"]]
    members = {entry["member"] for entry in data["art_inputs"].values()}
    assert "zteam/hud/temp/weapon_interfaces/h3_energy_sword.weapon_hud_interface" in members
    assert "zteam/hud/shared/bitmaps/reticles/weapons/energy_sword.bitmap" in members


def test_haptics_on_the_melee_blow():
    data = recipe()
    melee = step(data, MELEE)
    fields = dict(melee["edits"][0][1])
    assert float(fields["low_frequency_vibrate_frequency"]) > 0 and float(fields["high_frequency_vibrate_frequency"]) > 0
