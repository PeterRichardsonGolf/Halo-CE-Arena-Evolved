"""tools/weapon_pack/pack_done.py, the weapon pack "done" check's static half (notes/plans/2026-11-02-weapon-pack-done-
checklist.md): a pack with everything passes, each missing piece fails its point, the recipe's recorded pack and the
game's load line are read, and a pack built from the repo's recipes (the build machine's, absent in CI: skipped) is held
to it. No game file is read."""
import hashlib
import json
import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "weapon_pack"))

import aepack  # noqa: E402
import pack_done  # noqa: E402


def tags(icon=True, models=2, sound=True, hud=True, projectile=True):
    def tag(group, name, kind=aepack.OWN):
        return aepack.Tag(aepack.group(group), name, kind=kind, bytes=b"\0" * 16 if kind == aepack.OWN else b"")
    found = [tag("weap", "ae\\packs\\t\\w")]
    found += [tag("mode", "ae\\packs\\t\\m%d" % index) for index in range(models)]
    found += [tag("bitm", "ae\\packs\\t\\hud\\icon"), tag("effe", "ae\\packs\\t\\fire")]
    if projectile:
        found.append(tag("proj", "ae\\packs\\t\\bullet"))
    found.append(tag("jpt!", "ae\\packs\\t\\hit"))
    if sound:
        found.append(tag("snd!", "sounds\\fire", aepack.EXTERNAL))
    if hud:
        found.append(tag("wphi", "ae\\packs\\t\\hud\\ammo"))
    return found


def pack(icon=True, behaviour=0, pickup=True, ammo=True, credits="Thank you, somebody: the art is theirs, used with credit, " * 3,
         **kinds):
    found = tags(**kinds)
    icon_tag = 1 + kinds.get("models", 2)
    weapon = aepack.Weapon("t", 0, "ar", "T", behaviour=behaviour,
                           weapon_pickup_text="Picked up a t" if pickup else None,
                           ammo_pickup_text="Picked up %d rounds for t" if pickup and ammo else None,
                           icon_bitmap_tag=icon_tag if icon else aepack.NONE)
    return aepack.build(aepack.Pack("ae_t", 1, "T", found, [weapon], credits=credits))


def run(tmp_path, image, **options):
    path = tmp_path / "t.aepack"
    path.write_bytes(image)
    return {(point, name.split(": ", 1)[-1]): verdict for point, name, verdict, _ in pack_done.check_pack(str(path), **options)}


def failed(results):
    return sorted(name for (point, name), verdict in results.items() if verdict == "fail")


def test_a_complete_pack_passes(tmp_path):
    assert failed(run(tmp_path, pack())) == []


@pytest.mark.parametrize("changes, point_name", [
    ({"icon": False}, "the message icon (pickup prompt, kill feed)"),
    ({"pickup": False}, "pickup texts"),
    ({"models": 1}, "a weapon root, 3P and FP models"),
    ({"sound": False}, "sounds"),
    ({"hud": False}, "a HUD interface"),
    ({"credits": "x"}, "credits"),
])
def test_each_missing_piece_fails(tmp_path, changes, point_name):
    assert point_name in failed(run(tmp_path, pack(**changes)))


def test_recipe_pin_and_haptics(tmp_path):
    image = pack()
    ok = {"expected": {"pack_sha256": hashlib.sha256(image).hexdigest()},
          "steps": [{"edits": [["sets", [["high_frequency_vibrate_frequency", "0.35"]]]]}]}
    assert failed(run(tmp_path, image, recipe=ok)) == []
    stale = {"expected": {"pack_sha256": "0" * 64}, "steps": []}
    assert failed(run(tmp_path, image, recipe=stale)) == ["haptics (a rumble on the weapon's damage effect)",
                                                          "the recipe's recorded pack"] or \
        "the recipe's recorded pack" in failed(run(tmp_path, image, recipe=stale))
    # (an AE rifle has the engine's kick: no rumble edit needed)
    assert "haptics (the engine's rifle kick)" not in failed(run(tmp_path, pack(behaviour=2)))


def test_the_game_load_line(tmp_path):
    good = ("weapon pack: t's look: pickup texts \"a\" / \"b\", message icon ae\\packs\\t\\hud\\icon sequence 0 (1 sprites); "
            "its HUD's icon sequence 5\n")
    bad = good.replace("message icon ae\\packs\\t\\hud\\icon sequence 0 (1 sprites)", "message icon none")
    assert "the game's load line shows the icon" not in failed(run(tmp_path, pack(), log=good))
    assert "the game's load line shows the icon" in failed(run(tmp_path, pack(), log=bad))


def test_corrections_count_per_pack(tmp_path):
    """a log of several packs: each pack's corrections are those of the added line before its own look line, so
    another pack's corrections in the same log do not fail this one"""
    look = ("weapon pack: %s's look: pickup texts \"a\" / \"b\", message icon ae\\packs\\%s\\hud\\icon sequence 0 "
            "(1 sprites); its HUD's icon sequence 5\n")
    added = "weapon pack: added to bloodgulch: 23 tags new (1 bytes), 29 the map's own, 0 optional ones the map lacks, %d corrections; its tag table 1 to 2\n"
    log = (added % 6) + (look % ("other", "other")) + (added % 0) + (look % ("t", "t"))
    assert pack_done.pack_corrections(log, "t") == ["0"] and pack_done.pack_corrections(log, "other") == ["6"]
    assert "no tag corrections in the game" not in " ".join(failed(run(tmp_path, pack(), log=log)))
    worse = (added % 0) + (look % ("other", "other")) + (added % 2) + (look % ("t", "t"))
    assert any("no tag corrections in the game" in name for name in failed(run(tmp_path, pack(), log=worse)))
    # (no added line before the pack's look line: not passed)
    assert pack_done.pack_corrections(look % ("t", "t"), "t") == []


def test_the_needler_pack_when_built():
    built = Path(os.environ.get("AE_NEEDLER_PACK", Path.home() / "src/halo-test/impl-075/packs/ae_needler.aepack"))
    if not built.is_file():
        pytest.skip("no built needler pack here")
    recipe = json.loads((ROOT / "tools/weapon_pack/needler/recipe.json").read_text())
    got = pack_done.check_pack(str(built), recipe)
    if hashlib.sha256(built.read_bytes()).hexdigest() != recipe["expected"]["pack_sha256"]:
        pytest.skip("the built pack is another revision")
    assert [name for _, name, verdict, _ in got if verdict == "fail"] == []


SWORD_RECIPE = ROOT / "tools/weapon_pack/energy_sword/recipe.json"


def melee_pack():
    """a melee weapon's pack: no projectile, no ammo pickup text (the energy sword's shape)"""
    return pack(projectile=False, ammo=False)


def sword_recipe(image):
    """the energy sword's recipe (it says "melee": true), its recorded pack this test's"""
    recipe = json.loads(SWORD_RECIPE.read_text())
    recipe["expected"] = dict(recipe.get("expected", {}), pack_sha256=hashlib.sha256(image).hexdigest())
    return recipe


def test_the_sword_recipe_says_melee():
    assert json.loads(SWORD_RECIPE.read_text())["pack"]["melee"] is True


def test_a_melee_weapon_is_asked_for_no_projectile_and_no_ammo_text(tmp_path):
    image = melee_pack()
    results = run(tmp_path, image, recipe=sword_recipe(image))
    assert failed(results) == []
    assert results[(4, "no ammo pickup text (a melee weapon)")] == "pass"
    assert results[(6, "no projectile (a melee weapon)")] == "pass"
    assert results[(4, "the weapon pickup text")] == "pass" and results[(6, "a damage effect")] == "pass"
    # (its weapon pickup text is still asked for)
    no_text = pack(projectile=False, pickup=False)
    assert "the weapon pickup text" in failed(run(tmp_path, no_text, recipe=sword_recipe(no_text)))


def test_a_melee_looking_pack_without_a_recipe_is_skipped_not_passed(tmp_path):
    results = run(tmp_path, melee_pack())
    assert failed(results) == []
    assert results[(4, "no ammo pickup text (a melee weapon)")] == "skip"
    assert results[(6, "no projectile (a melee weapon)")] == "skip"


def test_a_recipe_that_does_not_say_melee_is_held_to_the_usual_checks(tmp_path):
    image = melee_pack()
    recipe = sword_recipe(image)
    del recipe["pack"]["melee"]
    assert {"pickup texts", "projectile and damage tags"} <= set(failed(run(tmp_path, image, recipe=recipe)))


def test_a_recipe_that_says_melee_for_a_weapon_that_fires_fails(tmp_path):
    for image in (pack(), pack(projectile=False), pack(ammo=False)):
        results = failed(run(tmp_path, image, recipe=sword_recipe(image)))
        assert "no ammo pickup text (a melee weapon)" in results and "no projectile (a melee weapon)" in results


def test_the_sword_pack_when_built():
    """the built energy sword (the build machine's, absent in CI: skipped) passes every check of the pack itself (ZTeam's
    picture is its message icon)"""
    built = Path(os.environ.get("AE_SWORD_PACK", Path.home() / "ae-pack-cex-sniper/packs-sword/ae_energy_sword.aepack"))
    if not built.is_file():
        pytest.skip("no built energy sword pack here")
    recipe = json.loads(SWORD_RECIPE.read_text())
    results = pack_done.check_pack(str(built), recipe)
    assert sorted(name.split(": ", 1)[-1] for point, name, verdict, _ in results if verdict == "fail") == []
