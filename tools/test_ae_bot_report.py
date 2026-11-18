"""tools/ae_test/bot_report.py on a small recorded sample of "bot event:" lines (ae_bots_events.inc)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import bot_report  # noqa: E402

P = "arena-evolved: bot event: "
SAMPLE = "\n".join([
    "noise line",
    P + "t=2 bot=1 skill=easy ev=seen target=0 dist=9.0",          # warm-up: dropped
    P + "t=10 bot=1 skill=easy ev=pos target=0 dist=9.0 x=1.0 y=2.0 z=3.0 fighting=0",
    P + "t=200 bot=1 skill=easy ev=seen target=3 dist=10.0",
    P + "t=230 bot=1 skill=easy ev=first_shot target=3 dist=10.0 since_seen=30",   # reaction 1.0 s
    P + "t=230 bot=1 skill=easy ev=shot target=3 dist=10.0 weapon=weapons\\pistol\\pistol",
    P + "t=231 bot=1 skill=easy ev=hit target=3 dist=10.0 dmg=0.333",
    P + "t=231 bot=1 skill=easy ev=hit target=3 dist=10.0 dmg=0.100",    # same tick, same victim: one hit
    P + "t=250 bot=1 skill=easy ev=shot target=3 dist=14.0 weapon=weapons\\pistol\\pistol",
    P + "t=260 bot=1 skill=easy ev=strafe target=3 dist=14.0 dir=1",
    P + "t=270 bot=1 skill=easy ev=crouch target=3 dist=14.0 state=start",
    P + "t=280 bot=1 skill=easy ev=melee target=3 dist=1.0",
    P + "t=282 bot=1 skill=easy ev=hit target=3 dist=1.0 dmg=0.2",       # the blow's
    P + "t=330 bot=1 skill=easy ev=kill target=3 dist=14.0",             # 100 ticks after the first shot
    P + "t=400 bot=1 skill=easy ev=death target=3 dist=14.0",
    P + "t=150 bot=2 skill=recruit ev=seen target=0 dist=5.0",
    P + "t=170 bot=2 skill=recruit ev=first_shot target=0 dist=5.0 since_seen=20",
    P + "t=180 bot=2 skill=recruit ev=first_shot target=0 dist=5.0 since_seen=170",    # seen in the warm-up: no reaction
])
STILL = "\n".join(
    [P + f"t={t} bot=3 skill=normal ev=pos target=-1 dist=-1 x=50.0 y=60.0 z=1.0 fighting=0" for t in range(300, 600, 30)] +
    [P + f"t={t} bot=3 skill=normal ev=pos target=-1 dist=-1 x={80 + (t - 600) / 30:.1f} y=60.0 z=1.0 fighting=0"
     for t in range(630, 760, 30)])


def test_parse_and_tables(tmp_path):
    f = tmp_path / "bloodgulch-easy" / "data" / "debug.txt"
    f.parent.mkdir(parents=True)
    f.write_text(SAMPLE)
    events = bot_report.parse(f)
    assert len(events) == 17 and events[0]["ev"] == "seen" and events[0]["t"] == 2
    assert bot_report.game_name(f) == "bloodgulch-easy"
    bots = bot_report.analyse(events)
    r = bots[1]
    assert r["shots"] == 2 and r["hits"] == 1 and r["melee_hits"] == 1 and r["melee"] == 1
    assert r["reaction"] == [1.0]               # the warm-up sighting is not counted
    assert r["strafes"] == 1 and r["crouches"] == 1 and r["kills"] == 1 and r["deaths"] == 1
    assert r["ttk"] == [100 / 30.0]
    assert r["range"] == [10.0, 14.0]
    assert bots[2]["reaction"] == [20 / 30.0]
    text = bot_report.report([f])
    assert "| easy | 2 | 1 | 50% | 1.00 |" in text and "## Stuck spots" in text


def test_stuck_spot(tmp_path):
    f = tmp_path / "g" / "debug.txt"
    f.parent.mkdir()
    f.write_text(STILL)
    bots = bot_report.analyse(bot_report.parse(f), warmup=0)
    spots = bots[3]["stuck"]
    assert len(spots) == 1 and spots[0]["x"] == 50.0 and spots[0]["y"] == 60.0 and spots[0]["seconds"] >= 9
    assert bot_report.analyse(bot_report.parse(f), warmup=0, stuck_seconds=60)[3]["stuck"] == []


def test_by_team(tmp_path):
    """(team games) the bots' team= on their position lines gives a table by team; an FFA game has none"""
    f = tmp_path / "g" / "debug.txt"
    f.parent.mkdir()
    f.write_text("\n".join([
        P + "t=200 bot=1 skill=normal ev=pos target=-1 dist=-1 x=1 y=1 z=1 fighting=0 team=0",
        P + "t=210 bot=1 skill=normal ev=shot target=4 dist=8.0 weapon=w",
        P + "t=211 bot=1 skill=normal ev=hit target=4 dist=8.0 dmg=0.3",
        P + "t=200 bot=2 skill=normal ev=pos target=-1 dist=-1 x=5 y=1 z=1 fighting=0 team=1",
        P + "t=220 bot=2 skill=normal ev=shot target=3 dist=9.0 weapon=w",
        P + "t=230 bot=2 skill=normal ev=shot target=3 dist=9.0 weapon=w",
    ]))
    bots = bot_report.analyse(bot_report.parse(f))
    assert bots[1]["team"] == 0 and bots[2]["team"] == 1
    text = bot_report.report([f])
    assert "## By team" in text and "| red | 1 | 1 | 100% |" in text and "| blue | 2 | 0 | 0% |" in text
    assert "bot 2 (normal, blue)" in text
    g = tmp_path / "ffa" / "debug.txt"
    g.parent.mkdir()
    g.write_text(SAMPLE)
    assert "## By team" not in bot_report.report([g])


def test_fighting_is_not_stuck():
    pos = [{"t": 30 * i, "x": "1", "y": "1", "z": "1", "fighting": "1"} for i in range(20)]
    assert bot_report.stuck_spots(pos, 5) == []
