"""The SINGLESHOT BR (a Digsite map's own battle rifle becomes it, ae_map_skins_rules.h; CE+ X's battle rifle look, one
round a pull), the EVO rifle's Digsite look (worn on the CE Digsite Multiplayer maps only), the recipe source's extra
maps they need (make_pack.py), the display tint (dxt_tint.py), and the CMT bullets' metal_hollow impact. Everywhere
(CI): no game file is read; the packs are checked by the build-machine tests."""
import copy
import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import dxt_tint  # noqa: E402
import hek  # noqa: E402
import make_pack  # noqa: E402


def recipe(kind):
    return json.loads((TOOLS / kind / "recipe.json").read_text())


def dig_steps(data):
    return {step["tag"]: step for step in data["steps"] if step["layer"] == "dig"}


def sets(step):
    found = {}
    for edit in step["edits"]:
        if edit[0] == "set":
            found[edit[1]] = edit[2]
        elif edit[0] == "sets":
            found.update({key: value for key, value in edit[1]})
    return found


# ---------- the source's extra maps

def small_recipe():
    return {"source": {"kind": "zip", "allowlist": ["a/x.bitmap", "m/y.model", "m/z.shader_model"],
                       "extra_maps": {"one": {"name": "a map", "file": "one.map", "sha256": "0" * 64, "tags": 9,
                                              "allowlist": ["m/y.model", "m/z.shader_model"]}}}}


def test_extra_maps_rules():
    data = small_recipe()
    assert make_pack.extra_map_problems(data) == []
    assert make_pack.extra_map_names(data) == {"m/y.model", "m/z.shader_model"}
    assert make_pack.extra_map_problems({"source": {"allowlist": []}}) == []
    # (only a zip source has them)
    mapped = copy.deepcopy(data)
    mapped["source"]["kind"] = "map"
    assert make_pack.extra_map_problems(mapped) == ["only a zip source has extra maps"]
    # (each with its keys, a list, every name the allowlist's, no name in two lists)
    for change, word in ((lambda d: d["source"]["extra_maps"]["one"].pop("tags"), "keys"),
                         (lambda d: d["source"]["extra_maps"]["one"].update(allowlist=[]), "no tags"),
                         (lambda d: d["source"]["extra_maps"]["one"]["allowlist"].append("q/r.bitmap"), "does not have"),
                         (lambda d: d["source"]["extra_maps"].update(two=dict(d["source"]["extra_maps"]["one"])),
                          "two lists")):
        bad = copy.deepcopy(data)
        change(bad)
        problems = make_pack.extra_map_problems(bad)
        assert problems and word in problems[0], problems


# ---------- the SINGLESHOT BR: CE+ X's battle rifle models, one round a pull

def test_rifle_look_comes_from_the_extra_map():
    data = recipe("singleshot_br")
    make_pack.check_recipe(data)
    cex = data["source"]["extra_maps"]["cex"]
    assert cex["file"] == "bloodgulch.map" and cex["tags"] == 1761 and len(cex["sha256"]) == 64
    assert cex["allowlist"] == sorted(cex["allowlist"]) and all(name.startswith("dw2/") for name in cex["allowlist"])
    # (no sound of the source map: the animations play the pack's own; nothing the stock maps have)
    assert not [name for name in cex["allowlist"] if name.endswith(".sound")]
    animations = "dw2/weapons/battle_rifle/fp/fp.model_animations"
    assert set(cex["allowlist"]) - {animations} <= set(data["source"]["verbatim"])
    assert animations not in data["source"]["verbatim"]
    assert data["layers"][0] == "dig"


def test_rifle_look_plays_the_packs_sounds():
    data = recipe("singleshot_br")
    edits = sets(dig_steps(data)["dw2/weapons/battle_rifle/fp/fp.model_animations"])
    assert sorted(edits) == ["sound_references[%d].sound" % index for index in range(4)]
    assert all(value in data["source"]["allowlist"] and value.startswith("cmt/sounds/") for value in edits.values())


def test_singleshot_weapon_is_its_own():
    data = recipe("singleshot_br")
    pack = data["pack"]
    assert (pack["id"], pack["display"], pack["weapon_key"], pack["behaviour"], pack["replaces"]) == \
        ("ae_singleshot_br", "SINGLESHOT BR", "singleshot_br", 0, 0)
    assert pack["prefix"] == "ae\\packs\\singleshot_br\\" and "roots" not in pack
    steps = dig_steps(data)
    weapon = steps["cmt/weapons/evolved/battle_rifle/battle_rifle.weapon"]
    assert weapon["from"] == ["fix-3x", "cmt/weapons/evolved/battle_rifle/battle_rifle.weapon"]
    # (its look, and one round a pull in its tags: no charge, no spew; the game paces it by its key)
    assert sets(weapon) == {"model": "dw2/weapons/battle_rifle/tp/tp.model",
                            "first_person_model": "dw2/weapons/battle_rifle/fp/fp.model",
                            "first_person_animations": "dw2/weapons/battle_rifle/fp/fp.model_animations",
                            "triggers[0].charging_time": "0", "triggers[0].charged_time": "0",
                            "triggers[0].overcharged_action": "none", "triggers[0].spew_time": "0",
                            "triggers[0].maximum_rate_of_fire": "2.5 2.5"}
    assert "ae_pack/donor/donor.scenario" not in steps
    credits = (TOOLS / "singleshot_br" / "CREDITS.txt").read_text()
    assert "CE+ X team" in credits and "CE Digsite Multiplayer team" in credits
    # (its registry row: weapon 26, a Digsite map's own battle rifle becomes it)
    rows = {row["key"]: row for row in json.loads((ROOT / "tools" / "weapon_cache" / "registry.json").read_text())["rows"]}
    row = rows["singleshot_br"]
    assert (row["id"], row["display"], row["status"], row["recipe"]) == (26, "SINGLESHOT BR", "active", "tools/weapon_pack/singleshot_br")
    header = (ROOT / "port" / "linux" / "game" / "ae_map_skins_rules.h").read_text()
    assert '#define AE_MAP_SKINS_DIGSITE_RIFLE_KEY "singleshot_br"' in header


def test_battle_rifle_has_no_digsite_look():
    """rc5: the Digsite rifle is a weapon of its own, AE's battle rifle keeps its own look everywhere"""
    data = recipe("battle_rifle")
    assert "dig" not in data["layers"] and "roots" not in data["pack"]
    assert "extra_maps" not in data["source"]
    assert not [step for step in data["steps"] if "digsite" in step["tag"] or step["tag"].startswith("dw2/")]


# ---------- the EVO's Digsite look: its own model in a repainted finish, its sounds the rifle's

def test_evo_look_is_a_look_only():
    data = recipe("evo_rifle")
    make_pack.check_recipe(data)
    steps = dig_steps(data)
    # (the EVO's sounds stay; nothing of the look is a sound)
    assert not [step for step in data["steps"] if step["layer"] in ("dig", "dig-art") and
                (step["op"] == "sound" or step["tag"].endswith(".sound"))]
    look = steps["cmt/weapons/evolved/dmr/dmr_digsite.weapon"]
    assert sets(look) == {"model": "cmt/weapons/_shared/dmr/dmr_digsite.model",
                          "first_person_model": "cmt/weapons/_shared/dmr/dmr_digsite.model"}
    shaders = sets(steps["cmt/weapons/_shared/dmr/dmr_digsite.model"])
    assert sorted(shaders) == ["shaders[%d].shader" % index for index in (0, 1, 3, 4, 5)]
    assert all("_digsite." in value for value in shaders.values())
    assert data["pack"]["roots"] == ["weap:cmt\\weapons\\evolved\\dmr\\dmr_digsite"]


def test_evo_look_art_and_tints():
    data = recipe("evo_rifle")
    drawing = data["art_drawings"]["evo_digsite_body.png"]
    assert drawing == {"draw": "evo_digsite_diffuse", "input": "dmr_body_diff.bitmap", "mask": "digsite_paint_mask.png"}
    assert data["art"]["evo_digsite_body.png"]["size"] == [1024, 1024]
    mask = TOOLS / "evo_rifle" / drawing["mask"]
    assert mask.is_file() and mask.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
    tints = [step for step in dig_steps(data).values() if step["op"] == "dxt_tint"]
    assert len(tints) == 3 and all(step["colour"] == [1.0, 0.62, 0.12] and step["gain"] == 1.25 for step in tints)
    body = sets(dig_steps(data)["cmt/weapons/_shared/dmr/shaders/dmr_body_digsite.shader_model"])
    assert (body["perpendicular_brightness"], body["parallel_brightness"]) == ("0.35", "0.25")


# ---------- the tint

def test_tint_keeps_brightness_order():
    amber = (1.0, 0.62, 0.12)
    assert dxt_tint.tinted(0x0000, amber, 1.25) == 0x0000
    red, green, blue = hek.colour_565(dxt_tint.tinted(0xFFFF, amber, 1.25))
    assert (red, green, blue) == (31, 39, 4)
    dim = hek.colour_565(dxt_tint.tinted(hek.pack_565(8, 16, 8), amber, 1.25))
    assert dim[0] < red and dim[1] < green
    for colour, gain in (((1.5, 0, 0), 1.0), ((1, 1), 1.0), ((1, 1, 1), 0.0), ((1, 1, 1), 5.0)):
        with pytest.raises(hek.Refused):
            dxt_tint.tint(b"", colour, gain)


def test_evo_art_draws_a_square_picture():
    np = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import evo_digsite_art
    rgba = np.full((64, 64, 4), 0.5, np.float32)
    rgba[0:4, 0:4, :3] = (0.9, 0.8, 0.1)    # a yellow stripe
    mask = np.zeros((64, 64, 1), np.float32)
    mask[32:, :] = 1.0
    out = evo_digsite_art.diffuse(rgba, mask)
    assert out.shape == (64, 64, 3) and 0.0 <= out.min() and out.max() <= 1.0
    assert out[1, 1, 0] > out[1, 1, 1] * 3    # the stripe red
    assert out[48:, 8:, 1].mean() > out[48:, 8:, 2].mean()    # the paint olive
    with pytest.raises(evo_digsite_art.Refused):
        evo_digsite_art.diffuse(np.zeros((8, 4, 4), np.float32), mask)


# ---------- the CMT bullets' metal_hollow impact

CMT_BULLETS = {
    "battle_rifle": "cmt/weapons/evolved/battle_rifle/projectiles/battle_rifle_bullet.projectile",
    "singleshot_br": "cmt/weapons/evolved/battle_rifle/projectiles/battle_rifle_bullet.projectile",
    "dmr": "cmt/weapons/evolved/dmr/projectiles/dmr_bullet.projectile",
    "evo_rifle": "cmt/weapons/evolved/dmr/projectiles/dmr_bullet.projectile",
    "carbine": "cmt/weapons/evolved/carbine/projectiles/carbine_bolt.projectile",
    "smg": "cmt/weapons/evolved/assault_rifle/projectiles/assault_rifle_bullet.projectile",
    "smg_silenced": "cmt/weapons/evolved/assault_rifle/projectiles/assault_rifle_bullet.projectile",
    "smg_tactical": "cmt/weapons/evolved/assault_rifle/projectiles/assault_rifle_bullet.projectile",
    "needler": "cmt/weapons/evolved/needler/_needler_mp/projectiles/needler_mp_needle/needler_mp_needle.projectile",
    "ar_modern": "cmt/weapons/evolved/_egg/ma5c_individual_combat_wep_sys/projectiles/7.62x51mm_fmj_ap.projectile",
}


@pytest.mark.parametrize("kind", sorted(CMT_BULLETS))
def test_cmt_bullet_metal_hollow_plays_metal(kind):
    """CMT's tags give metal_hollow (5) their grunt armor's impact (grunt blood, a flesh hit); stock and Xbox maps use
    metal_hollow for floors and grates, so every pack's CMT bullet plays a metal impact there"""
    data = recipe(kind)
    bullet = CMT_BULLETS[kind]
    assert bullet not in data["source"].get("verbatim", [])
    edits = [sets(step) for step in data["steps"] if step["tag"] == bullet]
    assert len(edits) == 1
    effect = edits[0].get("projectile_material_response[5].default_effect", "")
    assert "metal" in effect and "grunt" not in effect and effect in data["source"]["allowlist"]


def test_every_cmt_bullet_of_a_shipped_pack_is_checked():
    """a pack recipe that carries a CMT bullet with impact effects is in CMT_BULLETS (the brute shot's grenade plays none)"""
    registry = json.loads((ROOT / "tools" / "weapon_cache" / "registry.json").read_text())
    active = {row["recipe"].rsplit("/", 1)[1] for row in registry["rows"]
              if row.get("status") == "active" and (row.get("recipe") or "").startswith("tools/weapon_pack/")}
    for kind in sorted(active):
        text = (TOOLS / kind / "recipe.json").read_text()
        carried = {name for name in json.loads(text)["source"].get("allowlist", []) if name.startswith("cmt/") and
                   name.endswith(".projectile")}
        expected = {CMT_BULLETS[kind]} if kind in CMT_BULLETS else set()
        if kind == "brute_shot":
            expected = carried
        assert carried == expected, kind
