"""The combat assault rifle pack's recipe, tools/weapon_pack/ar_modern/recipe.json: CMT's individual combat weapon
system rifle from the stock-compatible TSC:E 1.4 source release, made the Xbox's and set to AE's assault rifle numbers:
the recipe's own rules (no prototype), the allowlist (CMT's tags only), the numbers (a magazine of 36, 12 rounds a
second, error 0.25 to 2.1 degrees, one light zoom, 7.6 a round on shield and body alike), the HUD built as the SMG
pack's (our counter, reticle and prompts' picture), the pack's arguments, texts and credits. Everywhere (CI): no game
file is read; a run of the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "ar_modern"
BASE = "cmt/weapons/evolved/_egg/ma5c_individual_combat_wep_sys/"
WEAPON = BASE + "ma5c_individual_combat_wep_sys.weapon"
DAMAGE = BASE + "damage_effects/7.62x51mm_fmj_ap_impact.damage_effect"
PROJECTILE = BASE + "projectiles/7.62x51mm_fmj_ap.projectile"
# CE's player: 75 shield and 75 body
SHIELD = BODY = 75


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def sets(tag, layer="fix-ae"):
    entry = [entry for entry in recipe()["steps"] if entry["tag"] == tag and entry["layer"] == layer][0]
    return dict(entry["edits"][0][1])


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip"
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    source = data["source"]
    assert source["file"] == "tsce_1-4_source_release_stock.zip"
    assert source["allowlist"] == sorted(set(source["allowlist"]))
    assert all(name.startswith("cmt/") for name in source["allowlist"]) and WEAPON in source["allowlist"]
    assert data["layers"] == ["fix-ae", "fix-xbox"]
    for key in ("pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    # (the donor's hash is the box build's to record: empty until then)
    assert re.fullmatch(r"([0-9a-f]{64})?", data["expected"]["donor_sha256"])
    for name, entry in data["art"].items():
        assert set(entry) == {"mode", "size", "pixels_sha256"}


def test_the_numbers():
    weapon = sets(WEAPON)
    assert (weapon["magazines[0].rounds_loaded_maximum"], weapon["magazines[0].rounds_reloaded"]) == ("36", "36")
    assert (weapon["magazines[0].rounds_total_initial"], weapon["magazines[0].rounds_reserved_maximum"]) == (
        "108", "216")
    assert weapon["triggers[0].maximum_rate_of_fire"] == "12 12"
    assert (weapon["triggers[0].error_angle"], weapon["triggers[0].minimum_error"]) == ("0.25 2.1", "0")
    assert (weapon["triggers[0].error_acceleration_time"], weapon["triggers[0].error_deceleration_time"]) == (
        "1", "0.17")
    # (one light zoom: CE's zoom field is a magnification)
    assert (weapon["zoom_levels"], weapon["zoom_magnification_range"]) == ("1", "1.4 1.4")
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    damage = sets(DAMAGE)
    assert (damage["cyborg_armor"], damage["cyborg_energy_shield"]) == ("1", "1")
    per_round = float(damage["damage_lower_bound"])
    assert damage["damage_upper_bound"] == " ".join([damage["damage_lower_bound"]] * 2)
    # 10 rounds take the shield, 20 the player: about 1.6 s at 12 a second
    assert (math.ceil(SHIELD / per_round), math.ceil((SHIELD + BODY) / per_round)) == (10, 20)
    assert abs(19 / 12 - 1.58) < 0.01
    projectile = sets(PROJECTILE)
    assert (projectile["initial_velocity"], projectile["final_velocity"], projectile["maximum_range"]) == (
        "360", "360", "100")


def test_the_hud_is_built_as_the_smg_packs():
    data = recipe()
    # (no icon of ours: the prompts' picture is CMT's own pickup icon sheet, its assault rifle's sequence 0)
    assert {entry["draw"] for entry in data["art_drawings"].values()} == {"counter_meter", "counter_alphas", "reticle"}
    ammo = [entry for entry in data["steps"] if entry["op"] == "ammo_hud"][0]
    assert ammo["icon_sheet"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons.bitmap"
    assert "cmt/ui/hudv4.1/bitmaps/weapons/pickup_icons.bitmap" in data["source"]["verbatim"]
    layout = data["art_drawings"]["ar_modern_meter_smooth.png"]["layout"]
    assert layout["columns"] * layout["rows"] == 36 and layout["group"] == 6
    assert data["art_drawings"]["ar_modern_reticle.png"]["form"] == "smg"
    counter = [entry for entry in data["steps"] if entry["op"] == "counter_hud"][0]
    assert counter["tag"] == "ae_pack/hud/ar_modern_ammo.weapon_hud_interface"


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_ar_modern", "ae\\packs\\ar_modern\\", "ar_modern", "COMBAT AR")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("ar", 0, 0)
    assert pack["icon"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons:0"
    texts = (FOLDER / pack["texts"]).read_text()
    assert "menu_name=COMBAT ASSAULT RIFLE\n" in texts
    assert "ammo_pickup=Picked up %d rounds for combat assault rifle\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("CMT", "Teh Lag", "Ifafudafi", "Siliconmaster"):
        assert name in credits
