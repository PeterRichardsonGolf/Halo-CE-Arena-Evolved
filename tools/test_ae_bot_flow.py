"""tools/ae_test/bot_flow.py on a small recorded sample of "bot event:" lines, old logs too."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import bot_flow  # noqa: E402
import bot_report  # noqa: E402

P = "arena-evolved: bot event: "
NEW = "\n".join([
    P + "t=30 bot=1 skill=heroic ev=pos target=-1 dist=-1.0 x=1.0 y=2.0 z=3.0 fighting=0 team=0 job=take crew=1 route=ledge with=-1 seat=none weapon=dmr",
    P + "t=30 bot=2 skill=heroic ev=pos target=-1 dist=-1.0 x=1.0 y=2.0 z=3.0 fighting=0 team=0 job=escort crew=0 route=none with=0 seat=gunner weapon=assault_rifle",
    P + "t=31 bot=1 skill=heroic ev=route target=-1 dist=-1.0 kind=ledge crew=1 x=4.0 y=5.0 z=6.0 far=30.0",
    P + "t=90 bot=1 skill=heroic ev=route_end target=-1 dist=-1.0 kind=ledge crew=1 result=dropped why=no_progress walked=7.0",
    P + "t=91 bot=2 skill=heroic ev=seat_order target=-1 dist=-1.0 kind=honk seat=gunner far=6.0",
    P + "t=151 bot=2 skill=heroic ev=seat target=-1 dist=-1.0 seat=gunner driver=human wait=2.0",
    P + "t=160 bot=3 skill=heroic ev=seat_lost target=-1 dist=-1.0 walked=12.0 near=-1.0 speed=0.0 alive=1 up=1 holds=1 carrying=0 free=1",
    P + "t=170 bot=2 skill=heroic ev=seat_leave target=-1 dist=-1.0 reason=parked seat=gunner driver=human seated=0.6 goal=-1.0 parked=0.0 no_driver=0.0 going_away=0.0",
    P + "t=180 bot=2 skill=heroic ev=threat target=4 dist=30.0 gun=1 self_gun=1 carrier=0 aiming=1 power=0 driving=0 score=32.0",
    P + "t=181 bot=2 skill=heroic ev=shot target=4 dist=30.0 weapon=vehicles\\warthog\\warthog",
    P + "t=182 bot=6 skill=heroic ev=flank_start target=4 dist=-1.0 seconds=12.0 x=1.0 y=2.0 z=3.0",
    P + "t=183 bot=6 skill=heroic ev=flank_end target=4 dist=-1.0 why=unseen seconds=3.0 left=9.0",
    P + "t=184 bot=7 skill=heroic ev=flank_no_place target=4 dist=-1.0 squad=5 enemy_x=1.0 enemy_y=2.0 far=20.0",
    P + "t=190 bot=5 skill=heroic ev=objective_lost target=4 dist=9.0 flags=0x4 vitality=1.20 seat=0 job=carry",
])
OLD = P + "t=30 bot=1 skill=heroic ev=pos target=-1 dist=-1.0 x=1.0 y=2.0 z=3.0 fighting=0 team=0\n"


def tables(tmp_path, text):
    path = tmp_path / "debug.txt"
    path.write_text(text)
    return dict(bot_flow.flow(bot_report.parse(path)))


def test_new_log(tmp_path):
    t = tables(tmp_path, NEW)
    assert t["jobs (bot-seconds)"] == "take 1, escort 1"
    assert t["with a human (bot-seconds)"] == "1"
    assert t["routes ended"] == "ledge: dropped/no_progress 1"
    assert t["seat orders"] == "honk 1"
    assert t["seconds to a seat"].startswith("median 2.0")
    assert t["seats given up"] == "holds 1"
    assert t["got out"] == "gunner, driver human: parked 1"
    assert t["a gunner's targets"] == "1, on an enemy gun 1"
    assert "bot 5 flags=0x4" in t["lost alive"]
    assert t["flanks"] == "1 started, ended: unseen 1"
    assert t["flank seconds"] == "median 3.0, still to go median 9.0"
    assert t["flanks with no place (bot, 5 s)"] == "1"


def test_old_log_has_no_new_tables(tmp_path):
    assert tables(tmp_path, OLD) == {}
