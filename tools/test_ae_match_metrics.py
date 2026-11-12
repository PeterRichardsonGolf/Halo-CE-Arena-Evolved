"""tools/ae_test/match_metrics.py on synthetic films (the format of port/linux/game/ae_match_film.c) and a recorded sample."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import match_metrics  # noqa: E402

HEADER = "ae film: v=1 map=bloodgulch engine=ctf gametype=CTF build=0.7.9-dev"


def line(t, ev, rest, mode="ctf"):
    return f"ae obj: t={t} host=1 mode={mode} ev={ev} {rest}"


def pos(t, p, x, y=0.0, z=0.0, b=0):
    return line(t, "pos", f"p={p} b={b} x={x:.1f} y={y:.1f} z={z:.1f} yaw=0 pitch=0 spd=3.0 crouch=0 air=0 seat=- weapon=assault_rifle hp=100 shield=100")


def spawn(t, p, x=0.0, b=0, team=0):
    return line(t, "spawn", f"p={p} b={b} team={team} x={x:.1f} y=0.0 z=0.0 weapon=assault_rifle")


def death(t, p, x=0.0, killer=-1, b=0):
    return line(t, "death", f"p={p} b={b} team=0 x={x:.1f} y=0.0 z=0.0 killer={killer} kb=0 cause=other weapon=none")


def film(tmp_path, lines, name="f.txt"):
    path = tmp_path / name
    path.write_text("\n".join(lines) + "\n")
    return match_metrics.read_film(path)


def test_distance_of_straight_line(tmp_path):
    lines = [HEADER, spawn(0, 0, 0.0)] + [pos(3 * i, 0, float(i)) for i in range(11)] + [line(30, "game_end", "winner=-1 score_r=0 score_b=0")]
    result = match_metrics.metrics(film(tmp_path, lines))
    assert result["players"]["0"]["distance"] == 10.0
    assert result["film"]["complete"] is True and result["film"]["map"] == "bloodgulch" and result["film"]["seconds"] == 1.0
    assert result["players"]["0"]["bot"] is False


def test_stuck_spot(tmp_path):
    lines = [HEADER, spawn(0, 1, 0.0, b=1)] + [pos(3 * i, 1, 5.0, b=1) for i in range(0, 71)]
    result = match_metrics.metrics(film(tmp_path, lines), stuck_s=5.0)
    stuck = result["players"]["1"]["stuck"]
    assert len(stuck) == 1 and stuck[0]["seconds"] >= 6 and stuck[0]["x"] == 5.0
    assert result["players"]["1"]["bot"] is True
    # a player that fires meanwhile is not stuck
    fighting = lines + [line(60, "fire", "p=1 b=1 weapon=assault_rifle shots=10 hits=2")]
    assert match_metrics.metrics(film(tmp_path, fighting, "g.txt"), stuck_s=5.0)["players"]["1"]["stuck"] == []


def test_slot_reuse_splits_paths(tmp_path):
    lines = [HEADER, spawn(0, 2, 0.0, b=1)] + [pos(3 * i, 2, float(i), b=1) for i in range(5)] + [death(12, 2, 4.0)]
    # the slot's player leaves (no lines), a person takes it over far away: a new spawn
    lines += [spawn(300, 2, 100.0, b=0)] + [pos(300 + 3 * i, 2, 100.0 + i) for i in range(5)]
    result = match_metrics.metrics(film(tmp_path, lines))
    player = result["players"]["2"]
    assert player["lives"] == 2
    # 4 + 4 world units of walking, no 100-unit jump between the two
    assert player["distance"] == 8.0
    # a bot was in the slot: never classed as a person
    assert player["bot"] is True
    # without the spawn and the death, the gap and the jump split them too
    lines = [HEADER] + [pos(3 * i, 3, float(i)) for i in range(5)] + [pos(300 + 3 * i, 3, 100.0 + i) for i in range(5)]
    assert match_metrics.metrics(film(tmp_path, lines, "h.txt"))["players"]["3"]["distance"] == 8.0


def test_incomplete_film(tmp_path):
    result = match_metrics.metrics(film(tmp_path, [HEADER, spawn(0, 0), pos(3, 0, 1.0)]))
    assert result["film"]["complete"] is False


def test_kills_deaths_and_first_engagement(tmp_path):
    lines = [HEADER, spawn(0, 0, 0.0), spawn(0, 1, 10.0, b=1, team=1)] + [pos(3 * i, 0, 0.0) for i in range(40)]
    lines += [line(60, "damage", "p=1 b=1 victim=0 amount=10.0 shield_left=80 body_left=100 weapon=bolt head=0"),
              line(90, "kill", "p=1 b=1 team=1 x=10.0 y=0.0 z=0.0 victim=0 vx=0.0 vy=0.0 vz=0.0 weapon=bolt head=0 dist=10.0"),
              death(90, 0, 0.0, killer=1)]
    result = match_metrics.metrics(film(tmp_path, lines))
    assert result["players"]["0"]["first_engagement_s"] == 2.0
    assert len(result["players"]["1"]["kills"]) == 1 and len(result["players"]["0"]["deaths"]) == 1
    assert result["grids"]["deaths"] and sum(map(sum, result["grids"]["deaths"])) == 1


def test_compare_flags_change():
    def data(hits):
        return {"film": {}, "players": {"0": {"bot": False, "shots": 100, "hits": hits, "seconds_alive": 60.0, "distance": 100.0,
                                              "kills": [], "deaths": [], "stuck": [], "first_engagement_s": 2.0}}, "grids": {}}
    flagged = match_metrics.compare(data(30), data(20), 0.15)
    assert "humans.accuracy" in flagged["flagged"]
    assert match_metrics.compare(data(30), data(29), 0.15)["flagged"] == []


def test_main_compare_exit_status(tmp_path, capsys):
    a, b = tmp_path / "a.json", tmp_path / "b.json"
    for path, hits in ((a, 30), (b, 20)):
        path.write_text(json.dumps({"film": {}, "players": {"0": {"bot": True, "shots": 100, "hits": hits, "seconds_alive": 60.0,
                                                                 "distance": 1.0, "kills": [], "deaths": [], "stuck": [],
                                                                 "first_engagement_s": None}}, "grids": {}}))
    assert match_metrics.main(["--compare", str(a), str(b)]) == 1
    assert match_metrics.main(["--compare", str(a), str(a)]) == 0


def test_recorded_sample(tmp_path, capsys):
    sample = Path(__file__).resolve().parent / "ae_test" / "sample_logs" / "film_sample.txt"
    if not sample.exists():
        return
    assert match_metrics.main([str(sample)]) == 0
    result = json.loads(capsys.readouterr().out)
    assert result["film"]["map"] == "bloodgulch"
    assert result["players"] and all(p["bot"] for p in result["players"].values())
    assert sum(p["distance"] for p in result["players"].values()) > 100


def test_stuck_dwell_is_split_at_a_fight(tmp_path):
    # 20 s still; a shot lands at tick 300 (10 s): the parts before and after it that are 5 s or more are spots, the fight is not
    lines = [HEADER, spawn(0, 1, 5.0, b=1)] + [pos(3 * i, 1, 5.0, b=1) for i in range(0, 201)]
    lines += [line(300, "fire", "p=1 b=1 weapon=assault_rifle shots=5 hits=0")]
    stuck = match_metrics.metrics(film(tmp_path, lines), stuck_s=5.0)["players"]["1"]["stuck"]
    assert len(stuck) == 2
    assert stuck[0]["seconds"] >= 5 and stuck[1]["seconds"] >= 5


def test_quit_is_not_a_death_a_minute(tmp_path):
    quit_line = line(60, "death", "p=0 b=0 team=0 x=0.0 y=0.0 z=0.0 killer=-1 kb=0 cause=quit weapon=none")
    lines = [HEADER, spawn(0, 0, 0.0)] + [pos(3 * i, 0, 0.0) for i in range(21)] + [quit_line]
    result = match_metrics.metrics(film(tmp_path, lines))
    assert len(result["players"]["0"]["deaths"]) == 1
    assert sum(map(sum, result["grids"]["deaths"])) == 0
    assert "humans.deaths_per_min" in match_metrics.key_metrics(result)
    assert match_metrics.key_metrics(result)["humans.deaths_per_min"] == 0.0
