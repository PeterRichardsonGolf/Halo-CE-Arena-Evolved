"""The brute shot pack's recipe, tools/weapon_pack/brute_shot/recipe.json: CMT's multiplayer brute shot from the
stock-compatible TSC:E 1.4 source release (no Open Sauce tags), taken as CMT made it: the recipe's own rules (no
prototype), the allowlist (CMT's tags only), only what an Xbox map needs done to them (models converted, the
multipurpose map reordered, the high-resolution HUD made the Xbox's, the underwater flash whose bitmap no stock
multiplayer map has left out), the pack's arguments, texts and credits. Everywhere (CI): no game file is read; a run of
the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "brute_shot"
WEAPON = "cmt/weapons/evolved/brute_shot/_brute_shot_mp/brute_shot_mp.weapon"
EFFECT = "cmt/weapons/evolved/brute_shot/_brute_shot_mp/effects/brute_shot_mp_grenade_explosion.effect"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


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
    # (the underwater flash and its stock bitmap are not taken)
    assert not any("bubble flare" in name for name in source["allowlist"])
    assert data["pack"]["stock_derived"] == {} and data["art"] == {}
    for key in ("pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    # (the donor's hash is the box build's to record: empty until then)
    assert re.fullmatch(r"([0-9a-f]{64})?", data["expected"]["donor_sha256"])


def test_only_what_an_xbox_map_needs():
    data = recipe()
    ops = {}
    for entry in data["steps"]:
        ops.setdefault(entry["op"], []).append(entry["tag"])
    # (no Open Sauce in this release: nothing stripped; no Ogg sound: nothing re-encoded)
    assert set(ops) == {"edit", "model", "multipurpose", "hud_hires"}
    assert sorted(ops["model"]) == sorted([
        "cmt/weapons/_shared/brute_shot/brute_shot.model", "cmt/weapons/_shared/brute_shot/fp/fp.model",
        "cmt/weapons/_shared/brute_shot/projectiles/brute_shot_grenade/brute_shot_grenade.model"])
    assert ops["multipurpose"] == ["cmt/weapons/_shared/brute_shot/bitmaps/brute_shot_body_multi.bitmap"]
    weapon = [entry for entry in data["steps"] if entry["tag"] == WEAPON][0]
    # (the weapon's own numbers untouched: only its models and the message indices)
    assert {edit[1] for edit in weapon["edits"]} == {"model", "first_person_model", "pickup_text_index",
                                                       "hud_text_message_index"}
    effect = [entry for entry in data["steps"] if entry["tag"] == EFFECT][0]
    assert effect["edits"] == [["erase", "events[0].particles[8]"]] and "bigbubble" in effect["why"]


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_brute_shot", "ae\\packs\\brute_shot\\", "brute_shot", "BRUTE SHOT")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("c_bs,ar", 0, 0)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a brute shot\n" in texts
    assert "ammo_pickup=Picked up %d grenades for brute shot\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("CMT", "Teh Lag", "Ifafudafi", "Siliconmaster", "brute_shot_mp"):
        assert name in credits


def test_the_message_icon_is_cmts_own():
    """the pickup prompt's and kill feed's picture: CMT's own pickup icon sheet (hudv4.1), the brute shot's sequence
    2, a root of its own (only CMT's HUD globals name the sheet); the donor's HUD globals name it so the donor holds it"""
    data = recipe()
    pack = data["pack"]
    assert pack["icon"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons:2"
    assert pack["roots"] == ["bitm:cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons"]
    assert "cmt/ui/hudv4.1/bitmaps/weapons/pickup_icons.bitmap" in data["source"]["verbatim"]
    hud = [entry for entry in data["steps"] if entry["tag"] == "ae_pack/donor/hud.hud_globals"][0]
    assert hud["edits"] == [["set", "icon_bitmap", "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons.bitmap"]]
