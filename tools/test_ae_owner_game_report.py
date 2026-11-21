"""tools/ae_test/owner_game_report.py on a small made-up debug.txt of two games (ae_objective_log.c, ae_bots_events.inc)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import owner_game_report as r  # noqa: E402

O = "arena-evolved: ae obj: "
B = "arena-evolved: bot event: "
BR = "weapon=ae\\packs\\battle_rifle\\weapons\\evolved\\battle_rifle\\battle_rifle"
LOG = "\n".join([
    "11.14.26 09:00:00  noise before any game",
    B + "t=500 bot=1 skill=elite ev=shot target=0 dist=9.0 " + BR,      # before the first game: dropped
    "arena-evolved: test run setup: rocks-1 (map bloodgulch AE BR TS, 1 players here)",
    "11.14.26 09:01:00  game start text: TEAM AE BR SLAYER",
    "arena-evolved: bots: ev=drive_obstacles count=40 scenery=40 machines=0 skipped_no_collision=3 dropped=0",
    O + "t=0 host=1 mode=slayer ev=game_start map=bloodgulch teams=1 players=2 bots=1",
    O + "t=150 host=1 mode=slayer ev=ps p=0 b=0 team=0 x=10.0 y=10.0 z=0.5 hp=100 shield=100 job=human",
    O + "t=150 host=1 mode=slayer ev=ps p=1 b=1 team=1 x=20.0 y=10.0 z=0.5 hp=100 shield=100 job=fight",
    B + "t=100 bot=1 skill=heroic ev=pos target=-1 dist=-1 x=1.0 y=1.0 z=0.0 fighting=0",     # warm-up level
    # shot 1 hits (pellet dup on the same tick is one hit); shot 2 misses; shot 3 hits at 12 u
    B + "t=200 bot=1 skill=elite ev=shot target=0 dist=8.0 " + BR,
    B + "t=203 bot=1 skill=elite ev=hit target=0 dist=8.0 dmg=0.2",
    B + "t=203 bot=1 skill=elite ev=hit target=0 dist=8.0 dmg=0.2",
    B + "t=220 bot=1 skill=elite ev=shot target=0 dist=8.5 " + BR,
    B + "t=240 bot=1 skill=elite ev=shot target=0 dist=12.0 " + BR,
    B + "t=250 bot=1 skill=elite ev=hit target=0 dist=12.0 dmg=0.2",
    # a blow's hit is not a shot's; a vehicle gun's shot is kept out of the bands
    B + "t=300 bot=1 skill=elite ev=melee target=0 dist=1.0",
    B + "t=302 bot=1 skill=elite ev=hit target=0 dist=1.0 dmg=0.5",
    B + "t=310 bot=1 skill=elite ev=shot target=0 dist=20.0 weapon=vehicles\\warthog\\warthog",
    # the bot stands still 8 s, not fighting
    *[B + f"t={t} bot=1 skill=elite ev=pos target=-1 dist=-1 x=50.0 y=60.0 z=1.0 fighting=0 job=item"
      for t in range(400, 670, 30)],
    B + "t=700 bot=1 skill=elite ev=pos target=-1 dist=-1 x=70.0 y=60.0 z=1.0 fighting=0 job=item",
    O + "t=420 host=1 mode=slayer ev=stuck p=1 b=1 team=1 x=50.5 y=60.5 z=1.0 count=1 job=item goal_name=item",
    O + "t=600 host=1 mode=slayer ev=fire p=0 b=0 weapon=battle_rifle shots=10 hits=4",
    O + "t=600 host=1 mode=slayer ev=fire p=1 b=1 weapon=battle_rifle shots=20 hits=5",
    O + "t=800 host=1 mode=slayer ev=death p=0 b=0 team=0 x=10.0 y=10.0 z=0.5 killer=1 kb=1 cause=kill weapon=bullet",
    O + "t=800 host=1 mode=slayer ev=kill p=1 b=1 team=1 x=20.0 y=10.0 z=0.5 victim=0 weapon=bullet head=0 dist=10.0",
    O + "t=900 host=1 mode=slayer ev=death p=1 b=1 team=1 x=21.0 y=11.0 z=0.5 killer=1 kb=1 cause=fall weapon=none",
    "arena-evolved: bots: ev=drive_dodge bot=1 t=950 obstacle=4 side=1",
    "arena-evolved: bots: ev=drive_wedge bot=1 t=960 class=warthog kind=early ahead 0.7 at (85.0 -127.0 1.4) new_goal 0",
    O + "t=1200 host=1 mode=slayer ev=game_end winner=0 score_r=5 score_b=3",
    # a second game with nobody in it, short: not shown by default
    O + "t=0 host=1 mode=ctf ev=game_start map=temple teams=1 players=1 bots=1",
    "arena-evolved: bots: ev=drive_obstacles count=7 skipped_no_collision=0 skipped_door_or_gate=0 dropped=0",
    B + "t=200 bot=1 skill=normal ev=shot target=2 dist=8.0 " + BR,
])


def write(tmp_path):
    f = tmp_path / "debug.txt"
    f.write_text(LOG + "\n")
    return f


def test_parse_splits_games(tmp_path):
    games = r.parse(write(tmp_path))
    assert [g.map for g in games] == ["bloodgulch", "temple"]
    first = games[0]
    assert first.setup == "rocks-1" and first.clock == "11.14.26 09:01:00"
    assert games[1].setup is None
    assert [d[3]["count"] for d in games[1].drive] == ["7"]          # written after its start line: its own
    assert r.people(first) == [0]
    assert r.bot_skills(first) == {1: "elite"}           # the level most lines carry
    assert all(e["t"] != 500 for e in first.bot)          # the line before the first game is in no game
    assert first.end["score_r"] == "5"
    assert [d[0] for d in first.drive] == ["drive_obstacles", "drive_dodge", "drive_wedge"]   # map load line first
    assert first.drive[2][4] == (85.0, -127.0, 1.4)


def test_shots_that_hit(tmp_path):
    first = r.parse(write(tmp_path))[0]
    shots = r.shots_that_hit(first.bot, {0})[1]
    assert [(d, h) for d, h, _, v in shots if not v] == [(8.0, True), (8.5, False), (12.0, True)]
    assert [v for _, _, _, v in shots] == [False, False, False, True]


def test_wilson():
    low, high = r.wilson(5, 10)
    assert round(low, 3) == 0.269 and round(high, 3) == 0.731
    assert r.wilson(0, 0) == (None, None)
    low, high = r.wilson(0, 20)
    assert low == 0.0 and 0.0 < high < 0.15


def test_report(tmp_path):
    text = r.report(write(tmp_path))
    assert "2 game(s) in the file, 1 shown" in text
    assert "## Game 1: bloodgulch, slayer" in text and "Game 2" not in text
    assert "TEST RUN rocks-1" in text and "score red 5, blue 3" in text
    # your death, by the bot, with the kill's range
    assert "| (10, 10) | 0.5 | 1 | bot 1 elite 1 | bullet 1 | 10.0 |" in text
    # the bot's fall
    assert "| (22, 10) | 1 | fall 1 |" in text
    # 8 s still at (50, 60): t=400..610, with the stuck counter there
    assert "| (50, 62) | 1.0 | 1 | 8 | 8 | item 1 | 1: item/item 1 |" in text
    # 2 shots at 5-10 u, 1 hit; 1 at 10-15 u, hit; few
    assert "| elite | 5-10 u | 2 | 1 | 50 % (few) |" in text
    assert "| elite | 10-15 u | 1 | 1 | 100 % (few) |" in text
    assert "Not in the table: 1 shots from vehicle guns" in text
    assert "| you | all | 10 | 4 | 40 % (few) |" in text and "| bots | all | 20 | 5 | 25 % (few) |" in text
    assert "Scenery the drivers know: 40; steered round it 1 time(s)." in text
    assert "| (86, -126) | wedge 1 | warthog 1 |" in text
    assert "Game 2: temple" in r.report(write(tmp_path), show_all=True)


def test_list(tmp_path, capsys):
    r.main([str(write(tmp_path)), "--list"])
    out = capsys.readouterr().out
    assert "| 1 | 11.14.26 09:01:00 | bloodgulch | slayer | rocks-1 |" in out
    assert "| 2 | 11.14.26 09:01:00 | temple | ctf | - |" in out
