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
        f"bot event: t={tick} bot=1 skill=normal ev=kill target=2 dist=4" for tick in range(0, 1801, 200)
    ])
    baseline = tmp_path / "base.json"
    assert bot_scorecard.main([str(good), "--save", str(baseline)]) == 0
    capsys.readouterr()
    code, output = run_main(capsys, good, "--baseline", baseline)
    assert code == 0 and "FAIL" not in output.out
    saved = json.loads(baseline.read_text())
    assert saved["version"] == 1 and saved["source"] == str(good) and saved["metrics"]["ev.kill"] == 10
    empty = write(tmp_path, "empty.log", ["ae obj: t=0 host=1 mode=slayer ev=game_start map=beavercreek",
                                          "ae obj: t=1800 host=1 mode=slayer ev=game_end"])
    code, output = run_main(capsys, empty, "--baseline", baseline)
    assert code == 1 and "| ev.kill | 10 | 0 |" in output.out and "FAIL stopped" in output.out


def test_few_stopped_is_a_warning():
    """A counter with fewer than STOPPED_MINIMUM in the baseline that goes to 0 is a warning, not a stopped behaviour."""
    assert bot_scorecard.verdict("item.pistol", 4, 0, 0.26, 0.0, 3, 0.5, True) == "WARN fell"
    assert bot_scorecard.verdict("item.pistol", 8, 0, 0.5, 0.0, 3, 0.5, True) == "WARN fell"
    assert bot_scorecard.verdict("ev.item_take", 8, 0, 0.5, 0.0, 3, 0.5, True) == "FAIL stopped"


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


TICK = ("bots: tick {t}, 7 bots (heroic): searches 10 (0 failed, 0 learnt, 0 short, 0 full, 0 unavoided, 0 unsmoothed), "
        "stuck {s} (goals given up {g}), kills/deaths {kd}")
FEAR = ("bots: fear tick {t}: squads formed 4 (with a player 1), flanks {f} (reached {r}, no place {n}), holds {h}, "
        "suppressing {x} s (firing blind 2.0 s)")
FALLS = ("bots: falls tick {t}: steps turned 3, held 2, goals given up at an edge {g}, jumps held 1, own grenades run from 0, "
         "throws held 0, explosive shots held 0 (lethal drop 5.0)")
TRAVEL = ("bots: travel tick {t}: teleports {tp} (2 planned, {pm} pads missed), jumps {j} ({jm} missed), walks {w} ({wm} missed), "
          "links tried 1 taken 1 declined 0 (routed 0); stuck spots this minute: none")
DRIVE = "bots: ev=drive_stats mode={m} map=cmt_snow_grove drive_s={d} dist=900 bumps={b} reverses={r} wedges={w} stuck_s={s} detour_s=0 fallback_s=0 (minute)"


def test_summary_lines_use_the_last_value(tmp_path):
    """The counters grow from 0 at the map's start: what came before the first minute line counts too."""
    log = write(tmp_path, "sum.log", [
        TICK.format(t=1800, s=2, g=1, kd="1:1/0 2:0/2"),
        FEAR.format(t=1800, f=10, r=1, n=2, h=3, x=4.0),
        FALLS.format(t=1800, g=1),
        TRAVEL.format(t=1800, tp=5, pm=1, j=10, jm=2, w=20, wm=3),
        TICK.format(t=5400, s=12, g=4, kd="1:5/1 2:3/4"),
        FEAR.format(t=5400, f=30, r=4, n=5, h=9, x=10.5),
        FALLS.format(t=5400, g=6),
        TRAVEL.format(t=5400, tp=9, pm=1, j=30, jm=12, w=50, wm=6),
    ])
    m = bot_scorecard.score(bot_scorecard.parse(log))["metrics"]
    assert (m["sum.stuck"], m["sum.giveups"], m["sum.kills"], m["sum.deaths"]) == (12, 4, 8, 5)
    assert (m["fear.flanks"], m["fear.flanks_reached"], m["fear.flanks_no_place"], m["fear.holds"]) == (30, 4, 5, 9)
    assert m["fear.suppress_s"] == 10.5 and m["falls.goals_given_up"] == 6
    assert (m["travel.jumps_missed"], m["travel.teleports"], m["travel.jumps"]) == (12, 9, 30)


def test_summary_counter_reset_and_single_line(tmp_path):
    log = write(tmp_path, "reset.log", [FALLS.format(t=1800, g=7), FALLS.format(t=3600, g=2),
                                        TICK.format(t=1800, s=9, g=9, kd="1:1/1")])
    m = bot_scorecard.score(bot_scorecard.parse(log))["metrics"]
    # (7, then a new map from 0 to 2: 9 in all; one line is its own figure)
    assert m["falls.goals_given_up"] == 9 and m["sum.stuck"] == 9


def test_fights_line(tmp_path):
    line = ("bots: fights since the map's start: grenades {g} (1 at a last-seen spot), blows 4 (0 held back), head aims 5, "
            "retreats {r}, charges 2, jumps 3, halts 1, shots paced 0, zoomed again 0, high ground picks 0, "
            "enemies out of vertical view 0")
    log = write(tmp_path, "fights.log", [line.format(g=2, r=1), line.format(g=9, r=6)])
    m = bot_scorecard.score(bot_scorecard.parse(log))["metrics"]
    assert m["fight.grenades"] == 9 and m["fight.retreats"] == 6 and m["fight.head_aims"] == 5


def test_drive_stats_picks_the_longest_mode_and_single_line(tmp_path):
    log = write(tmp_path, "drive.log", [
        DRIVE.format(m="classic", d=30, b=1, r=1, w=0, s=1.0),
        DRIVE.format(m="planned", d=60, b=3, r=2, w=1, s=2.0),
        DRIVE.format(m="planned", d=120, b=9, r=6, w=2, s=8.5),
    ])
    m = bot_scorecard.score(bot_scorecard.parse(log))["metrics"]
    assert (m["drive.reverses"], m["drive.wedges"], m["drive.stuck_s"], m["drive.bumps"]) == (6, 2, 8.5, 9)
    one = write(tmp_path, "one.log", [DRIVE.format(m="planned", d=60, b=3, r=2, w=1, s=2.0)])
    assert bot_scorecard.score(bot_scorecard.parse(one))["metrics"]["drive.wedges"] == 1


def test_worse_when_a_bad_rate_more_than_doubles(tmp_path):
    base = {"version": 1, "source": "b", "minutes": 4,
            "metrics": {"sum.stuck": 8, "sum.giveups": 8, "ev.kill": 10, "drive.wedges": 4},
            "rates": {"sum.stuck": 2.0, "sum.giveups": 2.0, "ev.kill": 2.5, "drive.wedges": 1.0}}
    cur = {"minutes": 4, "metrics": {"sum.stuck": 17, "sum.giveups": 16, "ev.kill": 10, "drive.wedges": 0},
           "rates": {"sum.stuck": 4.25, "sum.giveups": 4.0, "ev.kill": 2.5, "drive.wedges": 0.0}}
    text, failed = bot_scorecard.table(cur, base)
    assert "| sum.stuck | 8 | 17 | 2.00 | 4.25 | WORSE doubled |" in text
    assert "| sum.giveups | 8 | 16 | 2.00 | 4.00 | OK |" not in text  # exactly double is not more than double
    assert "| drive.wedges | 4 | 0 |" not in text and not failed  # fewer is never a failure


def test_worse_new_and_noise_floor():
    assert bot_scorecard.verdict("sum.stuck", 0, 5, 0, 1.0, 3, 0.5, True) == "WORSE new"
    # (not in the baseline at all, as drive_stats in an older build: information only)
    assert bot_scorecard.verdict("drive.wedges", 0, 5, 0, 1.0, 3, 0.5, False) == "OK"
    assert bot_scorecard.verdict("sum.stuck", 1, 2, 0.1, 0.5, 3, 0.5, True) == "OK"
    assert bot_scorecard.verdict("sum.stuck", 5, 0, 1.0, 0.0, 3, 0.5, True) == "OK"


def test_bad_counters_falling_are_not_failures():
    """A crash or a flank with no place that stops is better, not a stopped behaviour."""
    for name in ("ev.drive_crash", "fear.flanks_no_place", "ev.climb_drop"):
        assert bot_scorecard.verdict(name, 4, 0, 0.3, 0.0, 3, 0.5, True) == "OK"


def test_drive_counters_are_per_drive_minute():
    rates = bot_scorecard.rates_of({"drive.drive_s": 120.0, "drive.wedges": 6.0, "ev.kill": 20.0, "game.minutes": 10.0},
                                   10.0)
    assert rates["drive.wedges"] == 3.0  # 6 in 2 driving minutes
    assert rates["drive.drive_s"] == 12.0  # the drive time itself: a game minute
    assert rates["ev.kill"] == 2.0
    # (no driving logged: a game minute, no division by zero)
    assert bot_scorecard.rates_of({"drive.wedges": 4.0}, 2.0)["drive.wedges"] == 2.0


def test_baseline_rates_come_again_from_counts(tmp_path):
    path = tmp_path / "b.json"
    path.write_text(json.dumps({"version": 1, "rates": {"drive.wedges": 0.6},
                                "metrics": {"drive.wedges": 6.0, "drive.drive_s": 120.0, "game.minutes": 10.0}}))
    assert bot_scorecard.load_baseline(path)["rates"]["drive.wedges"] == 3.0


def test_gate_flags_one_game_played_twice(tmp_path):
    import bot_gate
    same = ["bot event: t=1 bot=1 skill=normal ev=pos target=-1 dist=-1", "bots: something else"]
    a = write(tmp_path, "a.log", same)
    b = write(tmp_path, "b.log", same + ["bots: a different summary line"])
    c = write(tmp_path, "c.log", ["bot event: t=2 bot=1 skill=normal ev=pos target=-1 dist=-1"])
    assert bot_gate.same_games([a, b, c]) == [[0, 1]]
    assert bot_gate.same_games([a, c]) == []


def test_gate_flatten_keeps_each_tests_settings():
    import bot_gate
    source = "list x\nrevision 1\ngroup g\nmap bloodgulch\nset debug.bot_events true\nsetup a\n\ntest t1\ncheck c\nexpect e\n"
    flat = bot_gate.flatten(source)
    assert "setup" not in flat and "expect" not in flat and "group" not in flat
    assert flat.index("test t1") < flat.index("map bloodgulch") and "check c" in flat


def test_map_scenes_play_four_games():
    """Owner 11-13: the map scenes (canyon, stuck) play four games a round, the others two."""
    import bot_gate
    assert bot_gate.scene_runs("canyon") == 4 and bot_gate.scene_runs("stuck") == 4
    assert bot_gate.scene_runs("btb") == 2 and bot_gate.scene_runs("drive") == 2
