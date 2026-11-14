"""tools/weapon_pack/apply_tune.py: a DEV MODE tune file (port/linux/game/ae_weapon_tune.h) becomes the recipe's "tune"
layer, which wins over the recipe's other layers (Invader takes a tag from the first folder that has it). The fixtures
are a copy of the silenced rifle's recipe in a temporary folder: no game file is read."""
import json
import shutil
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import apply_tune  # noqa: E402

WEAPON_TAG = "weapons/ma5s/ma5s.weapon"
PACK_WEAPON_TAG = "ae/packs/ar_silenced/weapons/ma5s/ma5s.weapon"


@pytest.fixture
def recipe(tmp_path):
    path = tmp_path / "recipe.json"
    shutil.copyfile(TOOLS / "ar_silenced" / "recipe.json", path)
    return path


def tune_file(tmp_path, edits, key="ar_silenced", name="tune.json"):
    path = tmp_path / name
    path.write_text(json.dumps({"format": 1, "weapon_key": key, "weapon_tag": PACK_WEAPON_TAG, "edits": edits}))
    return path


def edit(field, value, tag=PACK_WEAPON_TAG, kind="weapon"):
    return {"key": "x", "tag_kind": kind, "tag": tag, "set": [field, value]}


def tune_steps(path):
    return [step for step in json.loads(path.read_text())["steps"] if step["layer"] == "tune"]


def test_round_trip_keeps_the_recipes_text(recipe):
    """what the tool writes back is the recipes' own format: nothing but its own change differs"""
    data = json.loads(recipe.read_text())
    assert apply_tune.dump(data) == recipe.read_text()


def test_tune_layer_wins_over_the_others(recipe, tmp_path):
    """Invader's first folder wins, so the tune layer is the first in the recipe's list"""
    before = json.loads(recipe.read_text())["layers"]
    apply_tune.apply(recipe, tune_file(tmp_path, [edit("triggers[0].maximum_rate_of_fire", [9, 9])]))
    after = json.loads(recipe.read_text())["layers"]
    assert after == ["tune"] + before


def test_tune_step_shape(recipe, tmp_path):
    lines = apply_tune.apply(recipe, tune_file(tmp_path, [edit("triggers[0].maximum_rate_of_fire", [6, 9]),
                                                          edit("autoaim_angle", 4)]))
    steps = tune_steps(recipe)
    assert len(steps) == 1 and lines
    step = steps[0]
    assert step["tag"] == WEAPON_TAG and step["op"] == "edit"
    assert step["edits"] == [["set", "triggers[0].maximum_rate_of_fire", "6 9"], ["set", "autoaim_angle", "4"]]


def test_from_points_at_the_layer_that_wins_now(recipe, tmp_path):
    """the silenced rifle's weapon is edited in fix-ae, from the source: the tune step reads fix-ae's"""
    apply_tune.apply(recipe, tune_file(tmp_path, [edit("autoaim_angle", 4)]))
    assert tune_steps(recipe)[0]["from"] == ["fix-ae", WEAPON_TAG]


def test_from_the_source_when_no_layer_makes_the_tag(recipe, tmp_path):
    """a tag of the source's allowlist that no step edits is read from the source"""
    data = json.loads(recipe.read_text())
    data["steps"] = [step for step in data["steps"] if step["tag"] != WEAPON_TAG]
    recipe.write_text(apply_tune.dump(data))
    apply_tune.apply(recipe, tune_file(tmp_path, [edit("autoaim_angle", 4)]))
    assert tune_steps(recipe)[0]["from"] == ["source", WEAPON_TAG]


def test_tune_set_replaced_not_duplicated(recipe, tmp_path):
    """the same key saved twice: the second value replaces the first, one set line"""
    apply_tune.apply(recipe, tune_file(tmp_path, [edit("triggers[0].maximum_rate_of_fire", [6, 9])]))
    apply_tune.apply(recipe, tune_file(tmp_path, [edit("triggers[0].maximum_rate_of_fire", [10, 12]),
                                                  edit("autoaim_angle", 4)], name="second.json"))
    steps = tune_steps(recipe)
    assert len(steps) == 1
    assert steps[0]["edits"] == [["set", "triggers[0].maximum_rate_of_fire", "10 12"], ["set", "autoaim_angle", "4"]]
    assert json.loads(recipe.read_text())["layers"].count("tune") == 1


def test_unknown_weapon_key_refused(recipe, tmp_path):
    """a stock weapon has no recipe: the file is the DEV command's, and the tool names the key"""
    before = recipe.read_text()
    with pytest.raises(apply_tune.Refused, match="no recipe for assault rifle"):
        apply_tune.apply(recipe, tune_file(tmp_path, [edit("autoaim_angle", 4)], key="assault rifle"))
    assert recipe.read_text() == before


def test_a_tag_the_recipe_does_not_make_refused(recipe, tmp_path):
    """the bullet is the stock map's: an external of the pack, not a tag the recipe can edit"""
    before = recipe.read_text()
    with pytest.raises(apply_tune.Refused, match="not a tag the recipe makes"):
        apply_tune.apply(recipe, tune_file(tmp_path, [edit("initial_velocity", 600, tag="weapons/assault rifle/bullet.projectile",
                                                           kind="projectile")]))
    assert recipe.read_text() == before


def test_pins_are_cleared(recipe, tmp_path):
    """a tuned donor is another file: the recorded hashes go, the next build tells the new ones"""
    apply_tune.apply(recipe, tune_file(tmp_path, [edit("autoaim_angle", 4)]))
    expected = json.loads(recipe.read_text())["expected"]
    assert not expected["donor_sha256"] and not expected["pack_sha256"] and not expected["pack_own_sha256"]


def test_bad_file_refused(recipe, tmp_path):
    bad = tmp_path / "bad.json"
    bad.write_text("{\"format\": 2}")
    with pytest.raises(apply_tune.Refused, match="format"):
        apply_tune.apply(recipe, bad)
    empty = tune_file(tmp_path, [], name="empty.json")
    with pytest.raises(apply_tune.Refused, match="no edits"):
        apply_tune.apply(recipe, empty)


def test_recipe_is_written_whole_or_not_at_all(recipe, tmp_path, monkeypatch):
    """a failure while writing leaves the old recipe and no temporary file; a good run leaves no temporary file either"""
    before = recipe.read_text()

    def broken(fd):
        raise OSError("disk full")

    monkeypatch.setattr(apply_tune.os, "fsync", broken)
    with pytest.raises(OSError):
        apply_tune.apply(recipe, tune_file(tmp_path, [edit("autoaim_angle", 4)]))
    assert recipe.read_text() == before
    assert [p.name for p in recipe.parent.iterdir() if p.name.endswith(".new")] == []
    monkeypatch.undo()
    apply_tune.apply(recipe, tune_file(tmp_path, [edit("autoaim_angle", 4)], name="again.json"))
    assert "tune" in json.loads(recipe.read_text())["layers"]
    assert [p.name for p in recipe.parent.iterdir() if p.name.endswith(".new")] == []
