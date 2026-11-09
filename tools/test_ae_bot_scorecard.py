"""tools/ae_test/bot_scorecard.py on synthetic Arena Evolved bot logs."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import bot_scorecard  # noqa: E402

P = "arena-evolved: "


def write(tmp_path, name, lines):
    path = tmp_path / name
    path.write_text("\n".join(P + line for line in lines) + "\n")
    return path


def test_counts_every_line_family_and_noise(tmp_path):
    log = write(tmp_path, "all.log", [
        "bot event: t=0 bot=1 skill=normal ev=pos target=-1 dist=-1",
        "bot event: t=30 bot=1 skill=normal ev=item_take target=-1 dist=2 item=sniper_rifle",
        "bots: ev=horn_get_in bot=1 t=60 caller=2",
        "ae obj: t=90 host=1 mode=ctf ev=game_start map=bloodgulch",
        "bots: bot 1 took the driver seat of a heavy scorpion",
        "bots: ride: t=120 bot 1 gets kind 2, vehicle 7, caller 3",
        "bots: fireteams: team 0: chief 4, bots 7",
        "ae obj: t=1800 host=1 mode=ctf ev=game_end",
    ])
    card = bot_scorecard.score(bot_scorecard.parse(log))
    assert card["metrics"]["ev.item_take"] == 1
    assert card["metrics"]["ev.horn_get_in"] == 1 and card["metrics"]["ev.game_start"] == 1
    assert card["metrics"]["item.sniper_rifle"] == 1
    assert card["metrics"]["seat.driver.heavy_scorpion"] == 1
    assert card["metrics"]["seat.driver"] == 1 and card["metrics"]["seat.any"] == 1
    assert card["metrics"]["ride.orders"] == 1 and card["metrics"]["fireteam.largest"] == 7
    assert "ev.pos" not in card["metrics"]
    assert bot_scorecard.score(bot_scorecard.parse(log), keep_noise=True)["metrics"]["ev.pos"] == 1


def test_ride_answered_inside_outside_and_inferred_ticks(tmp_path):
    log = write(tmp_path, "rides.log", [
        "bot event: t=100 bot=1 skill=normal ev=seen target=2 dist=10",
        "bots: ride: bot 1 gets kind 1, vehicle 4",
        "bot event: t=999 bot=1 skill=normal ev=seen target=2 dist=10",
        "bots: bot 1 took the gunner seat of a warthog",
        "bots: ride: t=2000 bot 2 gets kind 1, vehicle 4, caller 1",
        "bots: ev=seat bot=2 t=2900 vehicle=4",
        "bots: ride: t=3000 bot 3 gets kind 1, vehicle 4",
        "bots: ev=seat bot=3 t=3901 vehicle=4",
    ])
    metrics = bot_scorecard.score(bot_scorecard.parse(log))["metrics"]
    assert metrics["ride.orders"] == 3 and metrics["ride.answered"] == 2


def run_main(capsys, *args):
    code = bot_scorecard.main([str(arg) for arg in args])
    return code, capsys.readouterr()


def test_save_compare_and_stopped_failure(tmp_path, capsys):
    good = write(tmp_path, "good.log", [
        f"bot event: t={tick} bot=1 skill=normal ev=kill target=2 dist=4" for tick in (0, 600, 1800)
    ])
    baseline = tmp_path / "base.json"
    assert bot_scorecard.main([str(good), "--save", str(baseline)]) == 0
    capsys.readouterr()
    code, output = run_main(capsys, good, "--baseline", baseline)
    assert code == 0 and "FAIL" not in output.out
    saved = json.loads(baseline.read_text())
    assert saved["version"] == 1 and saved["source"] == str(good) and saved["metrics"]["ev.kill"] == 3
    empty = write(tmp_path, "empty.log", ["ae obj: t=0 host=1 mode=slayer ev=game_start map=beavercreek",
                                          "ae obj: t=1800 host=1 mode=slayer ev=game_end"])
    code, output = run_main(capsys, empty, "--baseline", baseline)
    assert code == 1 and "| ev.kill | 3 | 0 |" in output.out and "FAIL stopped" in output.out


def test_rate_halving_warns_without_failure(tmp_path, capsys):
    base_log = write(tmp_path, "base.log", [
        *[f"bot event: t={tick} bot=1 skill=normal ev=kill target=2 dist=4" for tick in (0, 300, 600, 900)],
        "ae obj: t=1800 host=1 mode=slayer ev=game_end",
    ])
    current = write(tmp_path, "current.log", [
        *[f"bot event: t={tick} bot=1 skill=normal ev=kill target=2 dist=4" for tick in (0, 1800, 3600, 5400)],
        "ae obj: t=7200 host=1 mode=slayer ev=game_end",
    ])
    baseline = tmp_path / "base.json"
    assert bot_scorecard.main([str(base_log), "--save", str(baseline)]) == 0
    capsys.readouterr()
    code, output = run_main(capsys, current, "--baseline", baseline)
    assert code == 0 and "WARN fell" in output.out and "FAIL" not in output.out


def test_fireteam_largest_fails(tmp_path, capsys):
    baseline = tmp_path / "base.json"
    baseline.write_text(json.dumps({"version": 1, "source": "old", "minutes": 1,
                                    "metrics": {"fireteam.largest": 4}, "rates": {"fireteam.largest": 4}}))
    log = write(tmp_path, "teams.log", ["bots: fireteams: team 0: alpha 7"])
    code, output = run_main(capsys, log, "--baseline", baseline)
    assert code == 1 and "FAIL exceed" in output.out


def test_two_logs_add_and_per_game_splits(tmp_path, capsys):
    one = write(tmp_path, "one.log", ["bot event: t=0 bot=1 skill=normal ev=kill target=2 dist=4"])
    two = write(tmp_path, "two.log", ["bot event: t=0 bot=2 skill=normal ev=kill target=1 dist=4"])
    combined = bot_scorecard.combine([bot_scorecard.score(bot_scorecard.parse(one)),
                                      bot_scorecard.score(bot_scorecard.parse(two))])
    assert combined["metrics"]["ev.kill"] == 2 and combined["minutes"] == 2
    games = write(tmp_path, "games.log", [
        "ae obj: t=0 host=1 mode=slayer ev=game_start map=a",
        "bot event: t=30 bot=1 skill=normal ev=kill target=2 dist=4",
        "ae obj: t=1800 host=1 mode=slayer ev=game_end",
        "ae obj: t=2000 host=1 mode=slayer ev=game_start map=b",
        "bot event: t=2030 bot=1 skill=normal ev=kill target=2 dist=4",
        "ae obj: t=3800 host=1 mode=slayer ev=game_end",
    ])
    code, output = run_main(capsys, games, "--per-game")
    assert code == 0 and output.out.count("## Game") == 2 and output.out.count("| ev.kill | 0 | 1 |") == 2
