"""tools/ae_test/obj_report.py on a small recorded sample of "ae obj:" lines (ae_objective_log.c), and the format check."""
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import obj_report  # noqa: E402

P = "arena-evolved: ae obj: "
W = "p={p} b={b} team={team} x=1.0 y=2.0 z=3.0"


def who(p, team, b=1):
    return W.format(p=p, b=b, team=team)


CTF = "\n".join([
    "noise line",
    P + "t=0 host=1 mode=ctf ev=game_start map=bloodgulch teams=1 players=3 bots=2",
    P + "t=30 host=1 mode=ctf ev=snap score_r=0 score_b=0 time_s=1 time_left_s=899 o1=home o2=home",
    P + "t=300 host=1 mode=ctf ev=flag_take flag=1 " + who(2, 0) + " from=stand prev=-1",
    P + "t=330 host=1 mode=ctf ev=snap score_r=0 score_b=0 time_s=11 time_left_s=889 o1=home o2=carried:2",
    P + "t=600 host=1 mode=ctf ev=flag_drop flag=1 " + who(2, 0) + " why=killed held_s=10.0",
    P + "t=700 host=1 mode=ctf ev=flag_take flag=1 " + who(4, 0) + " from=dropped prev=2",
    P + "t=1300 host=1 mode=ctf ev=flag_capture flag=1 " + who(4, 0) + " carry_s=20.0 dist=95.5",
    P + "t=1301 host=1 mode=ctf ev=flag_drop flag=1 " + who(4, 0) + " why=capture held_s=20.0",
    P + "t=1500 host=1 mode=ctf ev=flag_return flag=0 how=touch by=1 b=1 x=1.0 y=2.0 z=3.0",
    P + "t=1600 host=1 mode=ctf ev=flag_return flag=0 how=timeout by=-1 b=0 x=1.0 y=2.0 z=3.0",
    P + "t=30 host=1 mode=ctf ev=ps " + who(2, 0) + " hp=100 shield=100 job=take role=taker goal=roam objective=flag1 d_obj=40.0 state=alive seat=none ft=0 order=none",
    P + "t=60 host=1 mode=ctf ev=ps " + who(2, 0) + " hp=100 shield=100 job=take role=taker goal=roam objective=flag1 d_obj=30.0 state=alive seat=none ft=0 order=none",
    P + "t=60 host=1 mode=ctf ev=ps " + who(0, 0, 0) + " hp=100 shield=100 job=human role=none goal=none objective=flag1 d_obj=80.0 state=alive seat=none ft=-1 order=none",
    P + "t=150 host=1 mode=ctf ev=obj_blocked p=2 b=1 team=0 job=take objective=flag1 d_obj=30.0 reason=tethered since_s=5.0",
    P + "t=1 host=1 mode=ctf ev=job_change p=2 b=1 team=0 from=fight to=take util_from=10 util_to=63 forced=none d_obj=95.0",
    P + "t=1800 host=1 mode=ctf ev=game_end winner=0 score_r=1 score_b=0",
])
BOMB = "\n".join([
    P + "t=0 host=1 mode=snd ev=round_start round=1 attackers=0 score_r=0 score_b=0",
    P + "t=300 host=1 mode=snd ev=bomb_take " + who(2, 0),
    P + "t=600 host=1 mode=snd ev=plant_done " + who(2, 0),
    P + "t=700 host=1 mode=snd ev=defuse_start " + who(3, 1),
    P + "t=710 host=1 mode=snd ev=defuse_cancel p=3 b=1 team=1 held_s=0.3",
    P + "t=800 host=1 mode=snd ev=defuse_start " + who(3, 1),
    P + "t=950 host=1 mode=snd ev=defuse_done " + who(3, 1),
    P + "t=951 host=1 mode=snd ev=round_end round=1 winner=1 why=defuse score_r=0 score_b=1",
])


def write(tmp_path, text, name="debug.txt"):
    path = tmp_path / name
    path.write_text(text + "\n")
    return path


def test_the_table_is_read_from_the_c_file():
    table = obj_report.event_table()
    assert table["flag_take"] == ["flag", "p", "b", "team", "x", "y", "z", "from", "prev"]
    assert table["hill_new"] == ["x", "y", "z"]
    assert len(table) >= 30


def test_sample_conforms(tmp_path):
    table = obj_report.event_table()
    assert obj_report.check(write(tmp_path, CTF), table) == []
    assert obj_report.check(write(tmp_path, BOMB, "b.txt"), table) == []


def test_check_finds_a_moved_key(tmp_path):
    bad = P + "t=1 host=1 mode=koth ev=hill_new y=1.0 x=1.0 z=1.0"
    found = obj_report.check(write(tmp_path, bad), obj_report.event_table())
    assert len(found) == 1 and "keys" in found[0][2]
    unknown = obj_report.check(write(tmp_path, P + "t=1 host=1 mode=koth ev=nonsense", "u.txt"), obj_report.event_table())
    assert unknown[0][2] == "unknown event"


def test_ctf_numbers(tmp_path):
    text = obj_report.report(write(tmp_path, CTF))
    assert "| takes from the stand | 0 | 1 |" in text
    assert "| takes from the ground | 0 | 1 |" in text
    assert "| captures | 0 | 1 |" in text
    assert "| returns by touch | 1 | 0 |" in text
    assert "| returns by time | 1 | 0 |" in text
    assert "take -> capture seconds (median / p90): 20.0 / 20.0" in text
    assert "drops by cause: capture 1, killed 1" in text
    assert "winner 0, score 1-0" in text
    assert "carried 1" in text


def test_jobs_and_why(tmp_path):
    text = obj_report.report(write(tmp_path, CTF))
    assert "| 0 | take | taker | 2 | 35.0 / 40.0 |" in text or "| 0 | take | taker | 2 | 35.0 / 40.0 " in text
    assert "| 0 | human | none | 1 | 80.0 / 80.0 |" in text
    assert "| tethered | 1 | take |" in text
    assert "job changes 1; forced: none 1" in text


def test_bomb_times(tmp_path):
    text = obj_report.report(write(tmp_path, BOMB))
    assert "| 1 | 1 | defuse | 0-1 |" in text
    assert "plants 1; seconds from the round's start (median / p90): 20.0 / 20.0" in text
    assert "defuses started 2, done 1, let go 1; seconds start -> done (median / p90): 5.0 / 5.0" in text


def test_empty_file(tmp_path):
    assert "no ae obj: lines" in obj_report.report(write(tmp_path, "nothing here"))


def test_main_check_status(tmp_path):
    assert obj_report.main(["--check", str(write(tmp_path, CTF))]) == 0
    assert obj_report.main(["--check", str(write(tmp_path, P + "t=1 host=1 mode=ctf ev=nonsense", "x.txt"))]) == 1


def test_check_bots_flags_bot_job_with_b0(tmp_path):
    sample = Path(__file__).resolve().parent / "ae_test" / "sample_logs" / "server_bots_flag.txt"
    failures = obj_report.check_bots(sample.read_text().splitlines())
    assert len(failures) > 0
    # one failure per ps line with a bot's job and b=0 (the sample's slot 0 lines)
    assert sum("job" in f for f in failures) == sum(1 for l in sample.read_text().splitlines() if " b=0 " in l)
    assert obj_report.check_bots(CTF.splitlines()) == []
    mixed = "\n".join([
        P + "t=0 host=1 mode=ctf ev=game_start map=x teams=1 players=2 bots=1",
        P + "t=1 host=1 mode=ctf ev=ps " + who(2, 0, 1) + " hp=100 shield=100 job=take role=taker goal=roam objective=flag1 d_obj=1.0 state=alive seat=none ft=0 order=none",
        P + "t=2 host=1 mode=ctf ev=ps " + who(2, 0, 0) + " hp=100 shield=100 job=human role=none goal=none objective=flag1 d_obj=1.0 state=alive seat=none ft=0 order=none",
    ])
    assert len(obj_report.check_bots(mixed.splitlines())) == 1
    path = write(tmp_path, mixed)
    assert obj_report.main(["--check-bots", str(path)]) == 1


LISTEN = Path(__file__).resolve().parent / "ae_test" / "sample_logs" / "listen_events.txt"


def test_listen_sample_conforms_and_has_no_bot_failures():
    assert obj_report.check(LISTEN, obj_report.event_table()) == []
    assert obj_report.check_bots(LISTEN.read_text().splitlines()) == []


def test_every_death_has_a_spawn_before_it():
    spawns, deaths = Counter(), Counter()
    for event in obj_report.parse(LISTEN):
        if event["ev"] == "spawn":
            spawns[event["p"]] += 1
        elif event["ev"] == "death":
            deaths[event["p"]] += 1
            assert deaths[event["p"]] <= spawns[event["p"]]
    assert sum(spawns.values()) > 0 and sum(deaths.values()) > 0


def test_fire_lines_sum_matches_bot_report():
    """a 70 s host game with 4 bots (box, HALO_OBJECTIVE_LOG=full, HALO_BOT_EVENTS=1): the bots' fire shots against
    bot_report's shot count"""
    import bot_report
    sample = Path(__file__).resolve().parent / "ae_test" / "sample_logs" / "fire_sample.txt"
    fired = sum(e["shots"] for e in obj_report.parse(sample) if e["ev"] == "fire" and e["b"] == 1)
    analysed = bot_report.analyse(bot_report.parse(sample), warmup=0)
    reported = sum(r["shots"] for r in analysed[0].values()) if isinstance(analysed, tuple) else sum(r["shots"] for r in analysed.values())
    assert reported > 20
    assert abs(fired - reported) <= max(1, 0.02 * reported)
    assert obj_report.check(sample, obj_report.event_table()) == []


def test_check_bots_reads_every_line_with_a_flag():
    game = P + "t=0 host=1 mode=ctf ev=game_start map=x teams=1 players=2 bots=1"
    spawn_bot = P + "t=1 host=1 mode=ctf ev=spawn p=2 b=1 team=0 x=1.0 y=2.0 z=3.0 weapon=none"
    pos_person = P + "t=2 host=1 mode=ctf ev=pos p=2 b=0 x=1.0 y=2.0 z=3.0 yaw=0 pitch=0 spd=0.0 crouch=0 air=0 seat=- weapon=none hp=100 shield=100"
    assert len(obj_report.check_bots([game, spawn_bot, pos_person])) == 1
    fire = P + "t=3 host=1 mode=ctf ev=fire p=2 b=0 weapon=none shots=1 hits=0"
    assert len(obj_report.check_bots([game, spawn_bot, fire])) == 1
    death = P + "t=4 host=1 mode=ctf ev=death p=0 b=0 team=1 x=1.0 y=2.0 z=3.0 killer=2 kb=0 cause=kill weapon=none"
    assert len(obj_report.check_bots([game, spawn_bot, death])) == 1
    death_ok = death.replace("kb=0", "kb=1")
    assert obj_report.check_bots([game, spawn_bot, death_ok]) == []
