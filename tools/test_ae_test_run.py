"""Tests of tools/ae_test/test_run_lint.py: the sample passes, one case per rule fails."""
import subprocess
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "ae_test"))
import test_run_lint  # noqa: E402

LISTS = HERE / "ae_test" / "test_runs"
LINT = HERE / "ae_test" / "test_run_lint.py"

GOOD = """list {name}
revision 1
test a
check Look at it
map bloodgulch
"""


def write(tmp_path, text, name="case"):
    path = tmp_path / f"{name}.txt"
    path.write_text(text)
    return path


def messages(path):
    return [f"{line}: {message}" for line, message in test_run_lint.lint(path)]


@pytest.mark.parametrize("path", sorted(LISTS.glob("*.txt")), ids=lambda p: p.name)
def test_shipped_lists_pass(path):
    assert test_run_lint.lint(path) == []


def test_sample_passes_from_the_command_line():
    run = subprocess.run([sys.executable, str(LINT), str(LISTS / "sample.txt")], capture_output=True, text=True)
    assert run.returncode == 0 and run.stdout == ""


def test_good_case_passes(tmp_path):
    assert messages(write(tmp_path, GOOD.format(name="case"))) == []


@pytest.mark.parametrize("extra, expect", [
    ("frobnicate yes\n", "6: unknown line"),
    ("check two\ncheck three\ncheck four\n", "more than 3 checks"),
    ("screen 2\n", "both map and screen"),
    ("set no.such_key 1\n", "not in the config table"),
    ("set debug.ae_test_screen\n", "set needs a key and a value"),
    ("check " + "x" * 61 + "\n", "at most 60"),
    ("players 5\n", "players must be 1 to 4"),
    ("card left\n", "card is top or bottom"),
    ("retest soon\n", "bad retest"),
    ("test a\ncheck again\nmap m\n", "duplicate test id 'a'"),
    ("test Bad_Id\ncheck c\nmap m\n", "bad test id"),
    ("".join("set debug.ae_test_screen 1\n" for _ in range(17)), "more than 16 sets"),
    ("".join(f"test t{i}\ncheck c\nmap m\n" for i in range(64)), "more than 64 tests"),
    ("screen 0\n", "bad screen number"),
    ("screen 40000\n", "bad screen number"),
    ("screen \u00b2\n", "bad screen number"),
    ("players \u00b2\n", "players must be 1 to 4"),
    ("players 0\n", "players must be 1 to 4"),
    ("check\n", "empty check"),
    ("map\n", "map has no name"),
    ("set display.fov 90\n", "per-player setting"),
    ("set input.invert_mouse true\n", "per-player setting"),
    ("setup frobnicate 3\n", "is not a dev command"),
    ("setup " + "mark " + "x" * 120 + "\n", "at most 119"),
    ("".join("setup mark x\n" for _ in range(11)), "more than 10 setups"),
    ("expect 2 game_over\n", "names no check"),
    ("expect 1 frobnicate\n", "not one expect.py judges"),
    ("expect 1\n", "expect has no rule"),
    ("expect 1 spawn banana\n", "expect rule: spawn: unknown words"),
    ("expect 1 no frobnicate\n", "expect rule: unknown rule"),
    ("group Bad_Id\n", "bad group id"),
    ("check NEEDLER MCC (pause, GAME OPTIONS): faster needles\n", "setup by hand"),
    ("check Turn TIMERS off in GAME OPTIONS; start the game\n", "setup by hand"),
    ("check Set the bots to 4 in settings by hand\n", "setup by hand"),
])
def test_bad_case_fails(tmp_path, extra, expect):
    path = write(tmp_path, GOOD.format(name="case") + extra)
    found = messages(path)
    assert any(expect in message for message in found), found
    run = subprocess.run([sys.executable, str(LINT), str(path)], capture_output=True, text=True)
    assert run.returncode == 1 and f"{path}:" in run.stdout


def test_group_with_setup_and_expect_passes(tmp_path):
    text = ("list case\ngroup g\nmap bloodgulch\nsetup goto armoury\nsetup dev give dmr\n"
            "test a\ncheck It is there\nexpect 1 weapon dmr\nsetup spawn ghost\n"
            "test b\ncheck Still there\n")
    assert messages(write(tmp_path, text)) == []


def test_looking_at_a_pause_page_is_not_hand_setup(tmp_path):
    text = GOOD.format(name="case") + "check Pause > PLAYERS lists everyone by team\n"
    assert messages(write(tmp_path, text)) == []


def test_group_setups_count_with_the_tests(tmp_path):
    text = "list case\ngroup g\nmap m\n" + "setup mark g\n" * 6 + "test a\ncheck c\n" + "setup mark a\n" * 5
    assert any("more than 10 setups" in m for m in messages(write(tmp_path, text)))


def test_game_setup_on_a_screen_or_menus_test_fails(tmp_path):
    """No game runs on a screen or menus test: a setup line that changes one (give, spawn, goto, option...) fails, the
    group's too; the ones that need no game (mark, shot, config, bots, agent) pass."""
    text = ("list case\ntest s\ncheck c\nscreen 2\nsetup give dmr\nsetup mark here\nsetup config bots.skill easy\n"
            "test m\ncheck c\nmenus\nsetup dev option fall_damage halo_2\nsetup shot\n"
            "test ok\ncheck c\nmenus\nsetup bots 2\nsetup agent say hi\n"
            "group g\nmenus\nsetup goto armoury\ntest g1\ncheck c\n")
    found = messages(write(tmp_path, text))
    assert any(m.startswith("5: test s: setup 'give' changes a game, and a screen test") for m in found), found
    assert any(m.startswith("11: test m: setup 'option' changes a game, and a menus test") for m in found), found
    assert any(m.startswith("20: test g1: setup 'goto'") for m in found), found
    assert len(found) == 3, found


def test_known_set_key_passes(tmp_path):
    path = write(tmp_path, GOOD.format(name="case") + "set debug.network_test_loadout assault_rifle\n")
    assert messages(path) == []


def test_neither_map_nor_screen_and_no_check(tmp_path):
    assert any("neither map nor screen" in m for m in messages(write(tmp_path, "list case\ntest a\ncheck c\n")))
    assert any("no check" in m for m in messages(write(tmp_path, "list case\ntest a\nmap m\n")))


def test_list_id_must_be_the_file_name(tmp_path):
    assert any("not the file name" in m for m in messages(write(tmp_path, GOOD.format(name="other"))))


def test_no_list_line(tmp_path):
    assert any("no list line" in m for m in messages(write(tmp_path, "test a\ncheck c\nmap m\n")))


@pytest.mark.parametrize("name", ["Bad_Id", "bad.id", "bad id", "a" * 32])
def test_bad_list_id_characters(tmp_path, name):
    path = write(tmp_path, GOOD.format(name=name), name="case")
    assert any("bad list id" in m for m in messages(path))


def test_leading_zeros_are_numbers(tmp_path):
    """The parser reads `players 04` as 4, so the linter accepts it too."""
    assert not any("players" in m for m in messages(write(tmp_path, GOOD.format(name="case") + "players 04\n")))


def test_byte_order_mark_is_skipped(tmp_path):
    path = tmp_path / "case.txt"
    path.write_bytes(b"\xef\xbb\xbf" + GOOD.format(name="case").encode())
    assert messages(path) == []


def test_missing_file_is_a_lint_error(tmp_path):
    path = tmp_path / "nope.txt"
    run = subprocess.run([sys.executable, str(LINT), str(path)], capture_output=True, text=True)
    assert run.returncode == 1 and f"{path}:1: cannot read" in run.stdout and "Traceback" not in run.stderr


def test_per_player_keys_are_read_from_the_table():
    keys = test_run_lint.player_keys()
    assert {"display.fov", "display.spawn_heat", "input.keyboard_prompts"} <= keys
    assert keys <= test_run_lint.config_keys()


def test_no_per_player_keys_fails_loudly(tmp_path, monkeypatch):
    table = tmp_path / "ae_config_players.c"
    table.write_text("nothing here\n")
    monkeypatch.setattr(test_run_lint, "PLAYER_TABLE", table)
    with pytest.raises(SystemExit) as stopped:
        test_run_lint.player_keys()
    assert "cannot read per-player keys" in str(stopped.value)


def test_no_config_keys_fails_loudly(tmp_path, monkeypatch):
    table = tmp_path / "port_config.c"
    table.write_text("nothing here\n")
    monkeypatch.setattr(test_run_lint, "CONFIG_TABLE", table)
    with pytest.raises(SystemExit) as stopped:
        test_run_lint.config_keys()
    assert "cannot read config keys" in str(stopped.value)


@pytest.mark.parametrize("value", ["x", "40000", "\u0663", "-1", ""])
def test_bad_revision(tmp_path, value):
    path = write(tmp_path, GOOD.format(name="case").replace("revision 1", f"revision {value}"))
    assert any("bad revision" in m for m in messages(path))


def test_a_powerup_no_stock_item_has_warns(tmp_path):
    # (rc1 round: "powerup overshield" matched no item ("over shield"), so the test gave no overshield)
    text = ("list case\ntest os\ncheck The overshield is on your arms\nmap m\nsetup powerup overshield now\n"
            "test fine\ncheck You are invisible\nmap m\nsetup powerup active_camouflage\n"
            "test fine2\ncheck Over the top\nmap m\nsetup powerup over_shield now\n")
    path = write(tmp_path, text)
    assert messages(path) == []
    found = test_run_lint.warnings(path)
    assert len(found) == 1 and "powerup overshield" in found[0][1] and "over_shield" in found[0][1], found


def test_a_place_without_goto_warns(tmp_path):
    text = ("list case\ntest cap\ncheck Capture the blue flag: red 2\nmap m\n"
            "test there\ncheck Grab the flag and run\nmap m\nsetup goto flag_blue facing red_base\n"
            "group g\nmap m\nsetup goto armoury\ntest racks\ncheck The tank is on its plate\n"
            "test plain\ncheck The sniper is in your hands\nmap m\n")
    path = write(tmp_path, text)
    assert messages(path) == []
    found = test_run_lint.warnings(path)
    assert len(found) == 1 and "test cap" in found[0][1] and "'flag'" in found[0][1], found
    run = subprocess.run([sys.executable, str(LINT), str(path)], capture_output=True, text=True)
    assert run.returncode == 0 and "warning:" in run.stdout


# ---------- tools/ae_test/preflight.py: what a headless run's log and event stream say about each test's scene

PREFLIGHT_LIST = """list pre
revision 1
group ctf
map beavercreek
gametype AE CTF
test first
check Look
setup goto start 4
setup flag ours carry
test second
check Look
setup goto 1 2 3
test third
check Look
setup shot
test fourth
check Look
"""
PREFLIGHT_LOG = """arena-evolved: test run: pre started, run 2026-01-01T00:00:00, 4 of 4 tests, build abc
arena-evolved: test run setup: first (map beavercreek AE CTF, 1 players here)
arena-evolved: network test: gametype 'AE CTF' built in
arena-evolved: ae presets: a gametype that is no preset chosen (rounds 0, round time 0 s, mode 0, territories 0)
arena-evolved: ae seed rules: shootable grenades on, flag return h2, flag pickup hold
arena-evolved: test run setup ready first
arena-evolved: test run rules: pre first: gametype AE CTF, shootable grenades on, flag pickup hold, bots.count 0
11.18.26 08:40:16  bots: bot 1 joined the game in progress (team 0)
arena-evolved: test run setup kept second (no load)
11.18.26 08:43:12  arena gametype 'AE CTF' kept as it is
arena-evolved: test run setup kept third (no load)
arena-evolved: test run: pre ended (1 passed, 0 failed, 2 skipped, 0 noted)
arena-evolved: ae seed rules: a later game's line
"""


def preflight_events():
    def test(t, name, state, **more):
        return dict({"t": t, "ev": "test", "list": "pre", "test": name, "state": state}, **more)

    def cmd(t, line, ok, text):
        return {"t": t, "ev": "cmd", "source": "test run", "line": line, "ok": ok, "text": text}

    return [
        test(0, "first", "load"), test(4200, "first", "ready"), test(4200, "first", "start"),
        cmd(4233, "goto start 4", True, "at 11.3 8.6 -0.2"),
        # (refused twice while its dummy joined, then accepted: one line, three tries, its last answer)
        cmd(4266, "flag ours carry", False, "no living enemy"), cmd(4366, "flag ours carry", False, "no living enemy"),
        cmd(4466, "flag ours carry", True, "the nearest enemy carries your flag"),
        test(4500, "first", "setup"), test(9000, "first", "verdict", verdict="PASS"),
        test(9000, "second", "load"), test(9000, "second", "ready"),
        cmd(9500, "reset", True, "reset: everyone respawns"), test(9600, "second", "start"),
        cmd(9633, "goto 1 2 3", False, "no room for a player at 1.0 2.0 3.0"),
        cmd(12700, "goto 1 2 3", False, "no room for a player at 1.0 2.0 3.0"),
        test(12700, "second", "setup_failed", error="goto 1 2 3: no room for a player at 1.0 2.0 3.0"),
        test(15000, "second", "verdict", verdict="SKIP"),
        test(15000, "third", "load"), test(15000, "third", "ready"), cmd(15500, "reset", True, "reset"),
        # (an older game: a refused line and the test goes on as if it were set up)
        test(15600, "third", "start"), cmd(15633, "shot", False, "no game running"), test(15700, "third", "setup"),
        {"t": 15800, "ev": "cmd", "source": "console", "line": "god me", "ok": False, "text": "not a test's line"},
    ]


def test_preflight_reads_each_tests_staging():
    import preflight

    rows = preflight.staging(PREFLIGHT_LIST, PREFLIGHT_LOG, preflight_events())
    assert [row["id"] for row in rows] == ["first", "second", "third", "fourth"]
    first, second, third, fourth = rows
    assert first["state"] == "set up" and first["load_s"] == 4.2 and first["setup_s"] == 4.5 and first["verdict"] == "PASS"
    assert first["loaded"] == "map beavercreek AE CTF, 1 players here"
    assert first["rules"] == ["network test: gametype 'AE CTF' built in",
                              "ae presets: a gametype that is no preset chosen (rounds 0, round time 0 s, mode 0, territories 0)",
                              "ae seed rules: shootable grenades on, flag return h2, flag pickup hold"]
    assert [(line["line"], line["ok"], line["tries"]) for line in first["lines"]] == [
        ("goto start 4", True, 1), ("flag ours carry", True, 3)]
    assert first["lines"][1]["text"] == "the nearest enemy carries your flag"
    # (the rules the game said it played at the scene; a bot that joined after the game was ready is a setup line's)
    assert first["live"] == "gametype AE CTF, shootable grenades on, flag pickup hold, bots.count 0"
    assert first["bots"] == 0 and first["bots_set"] is None and second["live"] == ""
    # (a test that goes on in the same game: no load, that game's rule lines, and its own as they come; the reset is not
    # one of its lines)
    assert second["loaded"] == "no load" and second["rules"][:3] == first["rules"]
    assert second["rules"][3] == "arena gametype 'AE CTF' kept as it is"
    assert second["state"] == "setup failed" and second["error"] == "goto 1 2 3: no room for a player at 1.0 2.0 3.0"
    assert [(line["line"], line["ok"], line["tries"]) for line in second["lines"]] == [("goto 1 2 3", False, 2)]
    assert third["state"] == "set up" and third["lines"] == [{"line": "shot", "ok": False, "text": "no game running",
                                                              "tries": 1}]
    # (the console's lines are nobody's setup; a line after the run ended is no test's rule line)
    assert all("a later game" not in rule for row in rows for rule in row["rules"])
    assert fourth["state"] == "not set up" and fourth["lines"] == [] and fourth["rules"] == []


def test_preflight_fails_a_list_on_refused_lines_missing_tests_and_missing_pictures():
    import preflight

    rows = preflight.staging(PREFLIGHT_LIST, PREFLIGHT_LOG, preflight_events())
    found = preflight.problems(rows)
    assert found == [("second", "SETUP ERROR goto 1 2 3: no room for a player at 1.0 2.0 3.0"),
                     ("third", "SETUP ERROR shot: no game running"),
                     ("fourth", "the test never came up (no load, a list error, or the run ended before it)")]
    # (a picture is asked of every test that was set up)
    found = preflight.problems(rows, {"first": "frames/pre-first.png"})
    assert ("third", "no picture of the scene") in found and not any(test == "first" for test, _ in found)
    assert not any(words == "no picture of the scene" for test, words in found if test in ("second", "fourth"))
    # (every line accepted, at last, and a picture: nothing fails)
    assert preflight.problems(rows[:1], {"first": "x"}) == []
    text = "\n".join(preflight.lines(rows, {"first": "frames/pre-first.png"}))
    assert "TEST first: SET UP (map beavercreek AE CTF, 1 players here)" in text
    assert "    setup 2: flag ours carry -> ok the nearest enemy carries your flag (sent 3 times)" in text
    assert "    game: ae seed rules: shootable grenades on, flag return h2, flag pickup hold" in text
    assert "    time: load to ready 4.2 s, load to scene 4.5 s" in text and "    picture: frames/pre-first.png" in text
    assert "TEST second: SETUP FAILED (no load)" in text and "    setup 1: goto 1 2 3 -> REFUSED no room" in text
    assert "    SETUP ERROR: goto 1 2 3: no room for a player at 1.0 2.0 3.0" in text
    assert "TEST fourth: NOT SET UP" in text and text.count("    picture: NONE") == 3
    assert "    live: gametype AE CTF, shootable grenades on, flag pickup hold, bots.count 0" in text
    assert text.count("    live: NOT SAID") == 3


def test_preflight_flags_bots_in_a_game_the_list_set_to_none():
    import preflight

    text = ("list pre\nrevision 1\ntest menu\nmenus\ncheck Look\n"
            "group g\nmap beavercreek\ngametype AE TEAM SLY\nset bots.count 0\nset debug.x 1\n"
            "test quiet\ncheck Look\ntest loud\nset bots.count 2\ncheck Look\n")
    assert preflight.list_sets(text) == {"menu": {}, "quiet": {"bots.count": "0", "debug.x": "1"},
                                         "loud": {"bots.count": "2", "debug.x": "1"}}
    log = """arena-evolved: test run setup: quiet (map beavercreek AE TEAM SLY, 1 players here)
11.18.26 08:40:06  bots: bot 1 joined the game in progress (team 1)
11.18.26 08:40:06  bots: bot 2 joined the game in progress (team 0)
arena-evolved: test run setup ready quiet
11.18.26 08:40:16  bots: bot 3 joined the game in progress (team 0)
arena-evolved: test run setup: loud (map beavercreek AE TEAM SLY, 1 players here)
11.18.26 08:41:06  bots: bot 1 joined the game in progress (team 1)
arena-evolved: test run setup ready loud
"""
    events = [{"t": 0, "ev": "test", "list": "pre", "test": name, "state": state}
              for name in ("quiet", "loud") for state in ("load", "ready", "start", "setup")]
    rows = preflight.staging(text, log, events)
    quiet, loud = rows[1], rows[2]
    assert quiet["bots"] == 2 and quiet["bots_set"] == 0 and loud["bots"] == 1 and loud["bots_set"] == 2
    found = preflight.problems(rows)
    assert ("quiet", "2 bots joined the game, the list set bots.count 0") in found
    assert not any(test == "loud" for test, _ in found)
    assert "    bots: 2 joined as the game came up (the list set bots.count 0)" in preflight.lines(rows)
    # (an event stream that stops at a test's start: the log's scene picture line says it was set up)
    rows = preflight.staging(text, log + "arena-evolved: test run: pre loud scene picture test-runs/preflight/pre-loud.png\n",
                             [event for event in events if not (event["test"] == "loud" and event["state"] == "setup")])
    assert rows[2]["state"] == "set up"
    rows = preflight.staging(text, log, [event for event in events if not (event["test"] == "loud" and event["state"] == "setup")])
    assert rows[2]["state"] == "not set up"


def test_preflight_summary_tells_staging_from_expect_lines():
    import preflight

    assert preflight.summary_word(0) == "PASS" and preflight.summary_word(3) == "STAGED, EXPECT FAILED"
    assert preflight.summary_word(1) == "FAIL" and preflight.summary_word(-15) == "FAIL"


def test_preflight_collects_the_scene_pictures(tmp_path):
    import preflight

    folder = tmp_path / "run" / "bin" / "test-runs" / "preflight"
    folder.mkdir(parents=True)
    (folder / "pre-first.png").write_bytes(b"png")
    (folder / "pre-with-dash.png").write_bytes(b"png")
    (folder / "other-first.png").write_bytes(b"png")
    found = preflight.collect_pictures(tmp_path / "run", "pre", tmp_path / "frames")
    assert sorted(found) == ["first", "with-dash"]
    assert (tmp_path / "frames" / "pre-first.png").read_bytes() == b"png"
    assert not (tmp_path / "frames" / "other-first.png").exists()


def test_bot_settings_after_a_tournament_test_warn(tmp_path):
    text = ("list case\ntest tour\nmenus\nsetup social start digsite-big-team now\ncheck Look at it\n"
            "group g\nmap m\nset bots.count 0\ntest quiet\ncheck Look at it\n"
            "test own\nset bots.skill easy\ncheck Look at it\n")
    found = test_run_lint.warnings(write(tmp_path, text))
    assert len(found) == 2, found
    assert found[0][0] == 8 and "test quiet" in found[0][1] and "Move test tour to the end" in found[0][1]
    assert found[1][0] == 12 and "test own: sets bots.skill" in found[1][1]
    # (the tournament test last: its game ends after every other test's settings went back)
    text = ("list case\ngroup g\nmap m\nset bots.count 0\ntest quiet\ncheck Look at it\n"
            "test tour\nmenus\nsetup social start digsite-big-team now\ncheck Look at it\n")
    assert test_run_lint.warnings(write(tmp_path, text, "last")) == []


def test_an_item_at_the_feet_with_a_check_that_goes_to_it_warns(tmp_path):
    text = ("list case\ngroup g\nmap m\nsetup goto start 1\n"
            "test feet\nsetup powerup over_shield now\ncheck An overshield lies in front of you\n"
            "test flag\nsetup flag theirs here\ncheck Walk over their flag: nothing happens\n"
            "test ahead\nsetup powerup over_shield ahead\nsetup flag theirs ahead\ncheck It lies in front of you\n"
            "test taken\nsetup powerup over_shield now\nsetup flag theirs here\ncheck Your shield is charged twice\n"
            "test given\nsetup powerup over_shield now dummy\ncheck Shoot it: the dummy's shield holds\n")
    found = test_run_lint.warnings(write(tmp_path, text))
    assert len(found) == 2, found
    assert found[0][0] == 6 and "test feet" in found[0][1] and "use powerup <item> ahead" in found[0][1]
    assert found[1][0] == 9 and "test flag" in found[1][1] and "use flag theirs ahead" in found[1][1]


def test_expect_log_rules_read_the_games_own_log_of_each_test(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype AE CTF\n"
                              "test one\ncheck Rules\ncheck Timers\ncheck Stream\ncheck Never\n"
                              "expect 1 log flag return h2, flag pickup hold\nexpect 2 log item timers: 1 taken\n"
                              "expect 3 log flag spotted\nexpect 4 no log item timers: 2 taken\n"
                              "test two\ncheck Its own\ncheck Not the other's\n"
                              "expect 1 log item timers: 2 taken\nexpect 2 log item timers: 1 taken\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 5, "ev": "log", "text": "flag spotted: team 0's flag carrier seen"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "PASS"},
        {"t": 10, "ev": "test", "list": "case", "test": "two", "state": "start"},
        {"t": 19, "ev": "test", "list": "case", "test": "two", "state": "verdict", "verdict": "PASS"}]) + "\n")
    log = tmp_path / "game.log"
    log.write_text("arena-evolved: test run setup: one (map m AE CTF, 1 players here)\n"
                   "arena-evolved: ae seed rules: shootable grenades on, flag return h2, flag pickup hold\n"
                   "arena-evolved: test run setup ready one\n"
                   "arena-evolved: item timers: 1 taken\n"
                   "arena-evolved: test run: case one PASS\n"
                   "arena-evolved: item timers: 9 taken\n"
                   "arena-evolved: test run setup kept two (no load)\n"
                   "arena-evolved: item timers: 2 taken\n"
                   "arena-evolved: test run: case two PASS\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    # (the lines of the test's load are its own: its game's start)
    assert rows[("one", 1)]["verdict"] == "PASS" and rows[("one", 1)]["evidence"] == ["game log 2"]
    assert rows[("one", 2)]["verdict"] == "PASS" and rows[("one", 2)]["evidence"] == ["game log 4"]
    assert rows[("one", 3)]["verdict"] == "PASS" and rows[("one", 4)]["verdict"] == "PASS"
    assert rows[("two", 1)]["verdict"] == "PASS" and rows[("two", 2)]["verdict"] == "FAIL"
    # (without the game's log: the event stream alone, as before)
    rows = {(row["test"], row["check"]): row["verdict"] for row in expect.report(listing, stream)}
    assert rows[("one", 2)] == "FAIL" and rows[("one", 3)] == "PASS"


def test_an_expect_on_the_chosen_rules_line_warns(tmp_path):
    text = ("list case\ngroup g\nmap m\ngametype AE CTF\ntest hold\ncheck Hold the button\n"
            "expect 1 log ae seed rules: shootable grenades on, flag return h2, flag pickup hold\n"
            "test live\ncheck Hold the button\nexpect 1 log live rule: flag pickup hold\n")
    found = test_run_lint.warnings(write(tmp_path, text))
    assert len(found) == 1 and found[0][0] == 7 and "live rule:" in found[0][1], found


def test_a_setup_line_longer_than_the_game_reads_fails(tmp_path):
    line = "setup social start team-objective vote seed 7 party 2 votes 1,1,0 left 6"
    path = write(tmp_path, f"list case\ntest a\nmenus\n{line}\ncheck Look at it\n")
    found = messages(path)
    assert len(found) == 1 and "11 words after its name" in found[0] and found[0].startswith("4:"), found
    path = write(tmp_path, f"list short\ntest a\nmenus\n{line.rsplit(' left', 1)[0]}\ncheck Look at it\n", "short")
    assert messages(path) == []
    # (the game's own count)
    rules = (HERE.parent / "port" / "linux" / "game" / "ae_dev_cmd_rules.h").read_text()
    assert f"#define AE_DEV_CMD_WORDS {test_run_lint.COMMAND_WORDS}\n" in rules
