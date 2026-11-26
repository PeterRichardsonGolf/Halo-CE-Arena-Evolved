"""Tests of the staging spot tool (tools/ae_test/spots/spots.py): the ground and flat-room reading on a made-up level,
the catalogs' shape, the `setup spot <name>` expansion and its lint."""
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "ae_test" / "spots"))
sys.path.insert(0, str(ROOT / "tools" / "ae_test"))

import spots  # noqa: E402
import test_run_lint  # noqa: E402


def floor(x0, y0, x1, y1, z):
    return {"v": [[x0, y0, z], [x1, y0, z], [x1, y1, z], [x0, y1, z]], "walk": True}


def level(cliff_at=None):
    """A flat 40 x 20 floor at z 0; with cliff_at the floor ends at that x (the ground is gone beyond it)."""
    end = 20 if cliff_at is None else cliff_at
    return {"surfaces": [floor(-20, -10, end, 10, 0.0)], "obstacles": [], "objects": []}


def scenario(enemy=None, ally=None):
    squads = []
    if enemy:
        squads.append({"encounter": "foe", "squad": "s", "team": "default", "actor": "elite", "ally": False,
                       "at_start": True, "bsp": 0, "p": enemy, "facing": 0})
    if ally:
        squads.append({"encounter": "mate", "squad": "s", "team": "player", "actor": "marine", "ally": True,
                       "at_start": True, "bsp": 0, "p": ally, "facing": 0})
    return {"players": [], "squads": squads, "vehicles": []}


def test_flat_room_runs_to_the_floors_end():
    ground = spots.Ground(level(cliff_at=6))
    g0, room = spots.room_ahead(ground, 0, 0, 0.0, 0)
    assert g0 == 0.0
    assert 5.0 <= room <= 6.0


def test_open_floor_gives_the_whole_look_distance():
    assert spots.room_ahead(spots.Ground(level()), -15, 0, 0.0, 0)[1] == spots.LOOK


def test_scenery_footprint_stops_the_room():
    dump = level()
    dump["obstacles"] = [{"name": "rock", "p": [3.0, 0.0, 0.5], "r": 0.5, "zlo": 0.0, "zhi": 1.0}]
    assert spots.room_ahead(spots.Ground(dump), 0, 0, 0.0, 0)[1] < 3.0


def test_rating_names_the_reason():
    ground = spots.Ground(level(cliff_at=4))
    poor = spots.rate(ground, scenario(), 0, 0, 0.0, 0)
    assert poor["rating"] == "POOR" and "only" in poor["why"][0]
    good = spots.rate(spots.Ground(level()), scenario(enemy=[19, 9, 0]), -15, 0, 0.0, 0, quiet=10)
    assert good["rating"] == "GOOD" and good["enemy_dist"] > 10
    near = spots.rate(spots.Ground(level()), scenario(enemy=[2, 0, 0]), -15, 0, 0.0, 0, quiet=40)
    assert near["rating"] == "POOR" and "enemy start" in near["why"][0]
    mate = spots.rate(spots.Ground(level()), scenario(ally=[-14, 0, 0]), -15, 0, 0.0, 0, quiet=40)
    assert mate["rating"] == "POOR" and "ally start" in mate["why"][0]


def test_spot_under_a_ledge_is_off_the_ground():
    assert spots.rate(spots.Ground(level()), scenario(), 0, 0, 5.0, 0)["rating"] == "POOR"


def test_other_bsp_is_not_assessed():
    assert spots.rate(spots.Ground(level()), scenario(), 0, 0, 0.0, 0, bsp=1)["rating"].startswith("NOT ASSESSED")


def test_catalogs_are_well_formed():
    names = []
    for map_name, entries in spots.catalogs().items():
        assert entries, map_name
        for spot in entries:
            assert spots.NAME.fullmatch(spot["name"]) and spot["name"].startswith(map_name.replace("_", "-") + "-")
            assert all(isinstance(spot[k], (int, float)) for k in ("x", "y", "z", "facing"))
            names.append(spot["name"])
    assert len(names) == len(set(names)) and names


def test_expand_writes_goto():
    name, (map_name, spot) = next(iter(spots.spot_names().items()))
    text = f"list t\ngroup g\nmap {map_name}\ntest a\nsetup spot {name}\nsetup actor commander 6\ncheck x\n"
    out = spots.expand_list(text)
    assert re.search(r"^setup goto \S+ \S+ \S+ facing \S+$", out, re.M) and "spot" not in out.split("setup actor")[0]
    assert out.endswith("setup actor commander 6\ncheck x\n")


def test_expand_refuses_unknown_and_foreign_names():
    with pytest.raises(ValueError):
        spots.expand_list("map b30\nsetup spot not-a-spot\n")
    name, (map_name, _spot) = next(iter(spots.spot_names().items()))
    with pytest.raises(ValueError):
        spots.expand_list("map other_map\nsetup spot %s\n" % name)


def test_lint_accepts_known_and_rejects_unknown(tmp_path):
    name, (map_name, _spot) = next(iter(spots.spot_names().items()))
    body = "list {id}\nrevision 1\ntitle t\ngroup g\nmap {map}\n\ntest a\nsetup spot {spot}\ncheck x\n"
    good = tmp_path / "good.txt"
    good.write_text(body.format(id="good", map=map_name, spot=name))
    bad = tmp_path / "bad.txt"
    bad.write_text(body.format(id="bad", map=map_name, spot="no-such-spot"))
    assert test_run_lint.lint(good) == []
    assert any("no spot of that name" in m for _l, m in test_run_lint.lint(bad))
