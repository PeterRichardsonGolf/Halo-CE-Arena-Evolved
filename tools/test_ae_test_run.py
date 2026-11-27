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


def test_expect_stat_rule_judges_the_squads_counter_lines(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype coop\n"
                              "test one\ncheck Warps\ncheck Too many\ncheck Pair\ncheck Pair close\ncheck None\n"
                              "expect 1 stat warps stuck max 0\nexpect 2 stat path failed max 1\n"
                              "expect 3 stat slots pair min 0.5\nexpect 4 stat slots pair min 0.7\n"
                              "expect 5 stat cost rays max 10\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "PASS"}]) + "\n")
    log = tmp_path / "game.log"
    log.write_text("arena-evolved: test run setup: one (map m coop, 1 players here)\n"
                   "arena-evolved: squad warps: stuck 2 catchup 0\n"
                   "arena-evolved: squad path: reached 5 partial 0 failed 3\n"
                   "arena-evolved: squad slots: replans 1 pair 0.90\n"
                   "arena-evolved: squad warps: stuck 2 catchup 1\n"
                   "arena-evolved: squad path: reached 9 partial 1 failed 6\n"
                   "arena-evolved: squad slots: replans 3 pair 0.60\n"
                   "arena-evolved: test run: case one PASS\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    # (a counter: its rise over the test, the first line to the last; a distance: its least or most in the test)
    assert rows[("one", 1)]["verdict"] == "PASS"
    assert rows[("one", 2)]["verdict"] == "FAIL" and "3" in rows[("one", 2)]["reason"]
    assert rows[("one", 3)]["verdict"] == "PASS" and rows[("one", 4)]["verdict"] == "FAIL"
    assert rows[("one", 5)]["verdict"] == "FAIL" and "no squad cost line" in rows[("one", 5)]["reason"]
    assert expect.validate("stat warps stuck max 0") is None
    assert expect.validate("stat warps stuck most 0") and expect.validate("stat warps stuck max x")


def test_expect_member_and_crossed_rules_judge_each_member(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype coop\n"
                              "test one\ncheck Stuck\ncheck Lip\ncheck Gap\ncheck Across\ncheck Across fast\ncheck Back\n"
                              "expect 1 member stuck max 2\nexpect 2 member lip max 1\nexpect 3 member gap90 max 4\n"
                              "expect 4 crossed x gt -47.6 2 within 20\nexpect 5 crossed x gt -47.6 3 within 20\n"
                              "expect 6 crossed y gt 5 1 within 10\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "PASS"}]) + "\n")
    log = tmp_path / "game.log"
    log.write_text("arena-evolved: test run setup: one (map m coop, 1 players here)\n"
                   "arena-evolved: squad member 1: stuck 0.5 lip 0.0 gap90 2.50\n"
                   "arena-evolved: squad member 2: stuck 1.0 lip 0.2 gap90 3.00\n"
                   "arena-evolved: companion: positions t+10s human -50.0 0.0 0.0 (bsp 1), 1 -51.0 0.0 0.0 (bsp 2), "
                   "2 -52.0 0.0 0.0 (bsp 3), 3 -53.0 0.0 0.0 (bsp 4)\n"
                   "arena-evolved: companion: positions t+12s human -47.0 0.0 0.0 (air, ray bsp 0.2), 1 -48.0 0.0 0.0 (bsp 2), "
                   "2 -50.0 0.0 0.0 (bsp 3), 3 -51.0 0.0 0.0 (bsp 4)\n"
                   "arena-evolved: companion: positions t+18s human -45.0 0.0 0.0 (bsp 1), 1 -46.0 0.0 0.0 (bsp 2), "
                   "2 -47.9 0.0 0.0 (bsp 3), 3 dead\n"
                   "arena-evolved: companion: positions t+40s human -44.0 0.0 0.0 (bsp 1), 1 -45.0 0.0 0.0 (bsp 2), "
                   "2 -47.0 0.0 0.0 (bsp 3), 3 not in\n"
                   "arena-evolved: squad member 1: stuck 1.5 lip 0.4 gap90 3.50\n"
                   "arena-evolved: squad member 2: stuck 4.0 lip 1.6 gap90 5.50\n"
                   "arena-evolved: test run: case one PASS\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    # (a member's seconds: their rise over the test, the worst member's; its gap: the worst member's last)
    assert rows[("one", 1)]["verdict"] == "FAIL" and "member 2" in rows[("one", 1)]["reason"]
    assert rows[("one", 2)]["verdict"] == "FAIL" and "1.4" in rows[("one", 2)]["reason"]
    assert rows[("one", 3)]["verdict"] == "FAIL" and "5.5" in rows[("one", 3)]["reason"]
    # (across: the player's first line past the plane, then each member past it within the seconds; 1 at once, 2 after 28 s)
    assert rows[("one", 4)]["verdict"] == "FAIL" and "member 2" in rows[("one", 4)]["reason"]
    assert rows[("one", 5)]["verdict"] == "FAIL"
    assert rows[("one", 6)]["verdict"] == "FAIL" and "never" in rows[("one", 6)]["reason"]
    assert expect.validate("member stuck max 2") is None and expect.validate("member stuck most 2")
    assert expect.validate("member speed max 2") and expect.validate("crossed x gt -47.6 3 within 20") is None
    assert expect.validate("crossed z lt 1 3 within 20") and expect.validate("crossed x lt -47.6 3 in 20")


def test_expect_crossed_rule_passes_members_across_in_time(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype coop\n"
                              "test one\ncheck Across\nexpect 1 crossed x gt -47.6 2 within 20\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "PASS"}]) + "\n")
    log = tmp_path / "game.log"
    log.write_text("arena-evolved: test run setup: one (map m coop, 1 players here)\n"
                   "arena-evolved: companion: positions t+12s human -47.0 0.0 0.0 (bsp 1), 1 -48.0 0.0 0.0 (bsp 2), "
                   "2 -50.0 0.0 0.0 (bsp 3)\n"
                   "arena-evolved: companion: positions t+30s human -45.0 0.0 0.0 (bsp 1), 1 -46.0 0.0 0.0 (bsp 2), "
                   "2 -47.0 0.0 0.0 (bsp 3)\n"
                   "arena-evolved: test run: case one PASS\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    assert rows[("one", 1)]["verdict"] == "PASS" and "18" in rows[("one", 1)]["reason"]


def test_expect_member_rule_adds_rises_across_a_reset(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype coop\n"
                              "test one\ncheck Stuck\nexpect 1 member stuck max 2\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "PASS"}]) + "\n")
    log = tmp_path / "game.log"
    # (the counts carried in from an earlier test (5.0), then started again from 0 when the squad came back: 1.0 + 2.5)
    log.write_text("arena-evolved: test run setup: one (map m coop, 1 players here)\n"
                   "arena-evolved: squad member 1: stuck 5.0 lip 0.0 gap90 2.00\n"
                   "arena-evolved: squad member 1: stuck 6.0 lip 0.0 gap90 2.00\n"
                   "arena-evolved: squad member 1: stuck 0.5 lip 0.0 gap90 2.00\n"
                   "arena-evolved: squad member 1: stuck 3.0 lip 0.0 gap90 2.00\n"
                   "arena-evolved: test run: case one PASS\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    assert rows[("one", 1)]["verdict"] == "FAIL" and "4" in rows[("one", 1)]["reason"]


def test_expect_ahead_rule_and_comma_counts(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype coop\n"
                              "test one\ncheck Stacked\ncheck None first\ncheck None first here\n"
                              "expect 1 stat doors stacks min 1\nexpect 2 ahead x lt -8.5 max 0\nexpect 3 ahead x lt -20 max 0\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "PASS"}]) + "\n")
    log = tmp_path / "game.log"
    # (a count followed by a comma is read; member 2 past the door 2 s before the player)
    log.write_text("arena-evolved: test run setup: one (map m coop, 1 players here)\n"
                   "arena-evolved: squad doors: track ticks 0, stacks 0, entries 0\n"
                   "arena-evolved: companion: positions t+10s human -6.1 -2.8 0.0 (bsp 1), 1 -6.5 -3.8 0.0 (bsp 2), "
                   "2 -11.8 -2.8 0.0 (bsp 3)\n"
                   "arena-evolved: companion: positions t+12s human -9.0 -2.8 0.0 (bsp 1), 1 -7.0 -3.8 0.0 (bsp 2), "
                   "2 -12.0 -2.8 0.0 (bsp 3)\n"
                   "arena-evolved: squad doors: track ticks 9, stacks 1, entries 0\n"
                   "arena-evolved: test run: case one PASS\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    assert rows[("one", 1)]["verdict"] == "PASS"
    assert rows[("one", 2)]["verdict"] == "FAIL" and "member 2" in rows[("one", 2)]["reason"]
    assert rows[("one", 3)]["verdict"] == "FAIL" and "never" in rows[("one", 3)]["reason"]
    assert expect.validate("ahead x lt -8.5 max 0") is None and expect.validate("ahead x lt -8.5 min 0")


def test_expect_member_near_is_the_least_and_old_lines_still_read(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype coop\n"
                              "test one\ncheck Apart\ncheck Stuck\nexpect 1 member near min 1.0\nexpect 2 member stuck max 2\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "PASS"}]) + "\n")
    log = tmp_path / "game.log"
    # (near: the least gap to the player while he stood; 99 before he stood; the worst member is the nearest; a line without
    # near (an older build) still reads)
    log.write_text("arena-evolved: test run setup: one (map m coop, 1 players here)\n"
                   "arena-evolved: squad member 1: stuck 0.0 lip 0.0 gap90 2.00\n"
                   "arena-evolved: squad member 1: stuck 0.5 lip 0.0 gap90 2.00 near 99.00\n"
                   "arena-evolved: squad member 2: stuck 0.0 lip 0.0 gap90 2.00 near 1.60\n"
                   "arena-evolved: squad member 1: stuck 1.0 lip 0.0 gap90 2.00 near 0.70\n"
                   "arena-evolved: test run: case one PASS\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    assert rows[("one", 1)]["verdict"] == "FAIL" and "member 1 near" in rows[("one", 1)]["reason"] and "0.7" in rows[("one", 1)]["reason"]
    assert rows[("one", 2)]["verdict"] == "PASS"
    assert expect.validate("member near min 1.0") is None


def test_route_from_log_walks_the_players_logged_positions(tmp_path):
    import route_from_log

    log = tmp_path / "debug.txt"
    log.write_text("arena-evolved: companion: positions t+0s human -55.4 0.4 0.0 (air, ray bsp -0.00), 1 not in\n"
                   "arena-evolved: companion: positions t+2s human -55.3 0.4 -0.0 (bsp 7371, ray bsp -0.00), 1 not in\n"
                   "arena-evolved: squad walk: follow ticks 3\n"
                   "arena-evolved: companion: positions t+4s human -53.5 1.4 -0.0 (bsp 935, ray bsp -0.00), 1 -55.0 0.1 0.2 (bsp 1)\n"
                   "arena-evolved: companion: positions t+6s human -50.0 -0.2 -0.0 (bsp 15, ray bsp -0.00), 1 -53.0 0.1 0.2 (bsp 1)\n"
                   "arena-evolved: companion: positions t+8s human -48.6 -0.1 0.2 (bsp 4546, ray bsp -0.00), 1 -51.0 0.1 0.2 (bsp 1)\n"
                   "arena-evolved: companion: positions t+10s human -47.1 -0.4 0.6 (air, ray bsp 0.25), 1 -50.0 0.1 0.2 (bsp 1)\n"
                   "arena-evolved: companion: positions t+12s human -46.8 0.8 -0.0 (bsp 12, ray bsp -0.00), 1 -49.0 0.1 0.2 (bsp 1)\n")
    lines = route_from_log.route_lines(log.read_text().splitlines())
    text = " ".join(lines)
    # (each line a setup line of at most 119 characters; the first point the first one on the ground, a point under 0.4 from
    # the last left out, the point before he is in the air marked for a crouch jump)
    assert all(line.startswith("setup player route ") and len(line) <= 119 for line in lines)
    assert text.split("route ")[1].startswith("-55.3 0.4 0.1")
    assert "-48.6 -0.1 0.3 crouch jump -47.1 -0.4 0.7" in text and "-46.8 0.8 0.1" in text
    assert text.count("-55.") == 1
    many = ["arena-evolved: companion: positions t+%ds human %.1f 0.0 0.0 (bsp 1), 1 not in" % (i, -80.0 + i) for i in range(50)]
    lines = route_from_log.route_lines(many)
    assert sum(line.count(" 0.0 0.1") for line in lines) == 32


def test_member_and_crossed_expects_lint(tmp_path):
    good = write(tmp_path, "list case\nrevision 1\ntitle Outcome\ngroup g\nmap a10\ngametype coop\n"
                           "test one\ntitle One\nsetup goto 0 0 0\ncheck They cross\n"
                           "expect 1 crossed x gt -47.6 3 within 20\nexpect 1 member stuck max 2\n")
    assert messages(good) == []
    bad = write(tmp_path, "list case\nrevision 1\ntitle Outcome\ngroup g\nmap a10\ngametype coop\n"
                          "test one\ntitle One\nsetup goto 0 0 0\ncheck They cross\nexpect 1 crossed z gt 0 3 within 20\n", "bad")
    assert any("crossed needs" in line for line in messages(bad))


def test_route_from_log_writes_his_stops_as_waits():
    import route_from_log

    # (half-second positions: he walks, stands 6 lines (3 s) at -6.1 -2.8, then walks on)
    lines = ["arena-evolved: companion: positions t+%ds human %.1f -2.8 0.0 (bsp 1), 1 not in" % (i // 2, -1.0 - i) for i in range(6)]
    lines += ["arena-evolved: companion: positions t+3s human -6.1 -2.8 0.0 (bsp 1), 1 not in"] * 6
    lines += ["arena-evolved: companion: positions t+6s human %.1f -2.8 0.0 (bsp 1), 1 not in" % (-7.1 - i) for i in range(3)]
    text = " ".join(route_from_log.route_lines(lines))
    assert "-6.0 -2.8 0.1 wait 3.0" in text or "-6.1 -2.8 0.1 wait 3.0" in text
    assert text.count("wait") == 1
    # (his aim, when the line has it, faces the wait)
    aimed = [line.replace("(bsp 1)", "(bsp 1) aim 180") if " -6.1 " in line else line for line in lines]
    assert "wait 3.0 face 180" in " ".join(route_from_log.route_lines(aimed))
    # (a stand before the first step (the test's start) or after the last (its end) is no wait)
    ends = [lines[0]] * 8 + lines + [lines[-1]] * 8
    assert " ".join(route_from_log.route_lines(ends)).count("wait") == 1


def test_stat_expects_lint(tmp_path):
    good = write(tmp_path, "list case\nrevision 1\ntitle Stats\ngroup g\nmap a10\ngametype coop\n"
                           "test one\ntitle One\nsetup goto 0 0 0\ncheck They keep up\nexpect 1 stat warps stuck max 0\n")
    assert messages(good) == []
    bad = write(tmp_path, "list case\nrevision 1\ntitle Stats\ngroup g\nmap a10\ngametype coop\n"
                          "test one\ntitle One\nsetup goto 0 0 0\ncheck They keep up\nexpect 1 stat warps stuck most 0\n", "bad")
    assert any("stat needs" in line for line in messages(bad))


def test_expect_hurt_rule_counts_hits_on_the_player():
    import expect

    assert expect.validate("hurt blocked min 1 taken max 0") is None
    assert expect.validate("hurt player 0 taken max 0") is None
    assert expect.validate("hurt") and expect.validate("hurt taken most 0") and expect.validate("hurt player x taken max 0")
    window = [{"ev": "test", "t": 0, "_line": 1},
              {"ev": "hurt", "player": 0, "blocked": 1, "t": 1000, "_line": 2},
              {"ev": "hurt", "player": 0, "blocked": 1, "t": 2000, "_line": 3},
              {"ev": "hurt", "player": 1, "blocked": 0, "t": 3000, "_line": 4},
              {"ev": "test", "t": 20000, "_line": 5}]
    passed, reason, evidence = expect.judge("hurt player 0 blocked min 1 taken max 0", window)
    assert passed and reason == "taken 0, blocked 2 in 20 s" and evidence == [2, 3]
    passed, reason, _ = expect.judge("hurt taken max 0", window)
    assert not passed and reason.startswith("taken 1 (wanted max 0)")
    # (no hit at all: god is not shown working, unseen passes on its window's seconds)
    quiet = [window[0], window[-1]]
    assert not expect.judge("hurt blocked min 1 taken max 0", quiet)[0]
    assert expect.judge("hurt taken max 0", quiet)[0]


def test_hurt_expects_lint(tmp_path):
    text = GOOD.format(name="hu") + "expect 1 hurt blocked min 1 taken max 0\n"
    assert messages(write(tmp_path, text, "hu")) == []
    assert messages(write(tmp_path, GOOD.format(name="hu") + "expect 1 hurt\n", "hu"))


def test_expect_setup_error_is_not_run(tmp_path):
    import json

    import expect

    listing = write(tmp_path, "list case\nrevision 1\ngroup g\nmap m\ngametype coop\n"
                              "test one\ncheck Never\nexpect 1 no log died at\n")
    stream = tmp_path / "events.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in [
        {"t": 0, "ev": "test", "list": "case", "test": "one", "state": "start"},
        {"t": 9, "ev": "test", "list": "case", "test": "one", "state": "verdict", "verdict": "SKIP"}]) + "\n")
    log = tmp_path / "game.log"
    log.write_text("arena-evolved: test run setup: one (map m coop, 1 players here)\n"
                   "arena-evolved: test run: case one SETUP ERROR: god me: no game running\n"
                   "arena-evolved: test run: case one SKIP\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream, log)}
    # (a test whose setup failed judged nothing: a "no" rule would pass on an empty window)
    assert rows[("one", 1)]["verdict"] == "NOT RUN" and "setup" in rows[("one", 1)]["reason"]


# ---------- the test environment: the gate, deploy, run by hand, preflight and remote copy

import json  # noqa: E402

import testenv  # noqa: E402
import deploy_latest  # noqa: E402
import preflight  # noqa: E402
import run_list  # noqa: E402
import scenario_dump  # noqa: E402
import spots  # noqa: E402

LEFT_LIST = """list left
revision 2
test a
check A
map bloodgulch
test b
check B
map bloodgulch
retest 2
test c
check C
map bloodgulch
"""


def record(test, verdict, revision, list_id="left"):
    return json.dumps({"list": list_id, "test": test, "verdict": verdict, "revision": revision})


def test_tests_left_follows_the_runner():
    # (a: passed at revision 1, kept; b: passed before its retest 2, played again; c: an old FAIL is stale, a new
    # revision is the fix, so it is played; a NOTE and another list's records count for nothing)
    results = "\n".join([record("a", "PASS", 1), record("b", "PASS", 1), record("c", "FAIL", 1),
                         record("a", "NOTE", 2), record("c", "PASS", 2, "other")])
    assert testenv.tests_left(LEFT_LIST, results) == ["b", "c"]
    assert testenv.tests_left(LEFT_LIST, record("c", "PASS", 1)) == ["a", "b"]
    # (the last record that counts wins: a FAIL after a PASS at this revision plays it again)
    assert testenv.tests_left(LEFT_LIST, "\n".join([record("a", "PASS", 2), record("a", "FAIL", 2),
                                                    record("b", "PASS", 2), record("c", "PASS", 2)])) == ["a"]
    assert testenv.tests_left(LEFT_LIST, "\n".join(record(t, "PASS", 2) for t in "abc")) == []
    # (a torn last line is skipped)
    assert testenv.tests_left(LEFT_LIST, record("a", "PASS", 2) + "\n{\"list\": \"le") == ["b", "c"]


def fake_install(tmp_path, list_text=LEFT_LIST, results=""):
    install = tmp_path / "install"
    (install / "test-runs").mkdir(parents=True)
    (install / testenv.PROGRAM).write_bytes(b"program 1234abcd")
    (install / "social.txt").write_text("playlists\n")
    (install / "test-runs" / "left.txt").write_text(list_text)
    if results:
        (install / "test-runs" / "left.results.jsonl").write_text(results + "\n")
    facts = {name: testenv.file_facts(install / name) for name in (testenv.PROGRAM, "social.txt")}
    (install / testenv.RECORD).write_text(json.dumps({"commit": "1234abcd", "source": "host:tree", "time": "t",
                                                      "files": facts}))
    return install


def gate(install, *words, lists=("left",)):
    rows = testenv.Rows()
    commit = testenv.check_build(install, rows)
    testenv.check_files(install, rows)
    for list_id in lists:
        testenv.check_list(install, list_id, commit, rows, *words)
    return {(row[0], row[1]) for row in rows.rows}, rows


def test_gate_build_and_files(tmp_path):
    install = fake_install(tmp_path)
    found, _ = gate(install, "", "waived")
    assert ("OK", "build") in found and ("OK", "files") in found
    # (a binary swapped by hand, or a playlist file changed, fails the gate)
    (install / "social.txt").write_text("older playlists\n")
    assert ("FAIL", "files") in gate(install, "", "waived")[0]
    (install / testenv.PROGRAM).write_bytes(b"another program")
    assert ("FAIL", "build") in gate(install, "", "waived")[0]
    # (no record: the build is not known)
    (install / testenv.RECORD).unlink()
    assert ("FAIL", "build") in gate(install, "", "waived")[0]


def test_gate_unknown_commit_fails(tmp_path):
    install = fake_install(tmp_path)
    data = json.loads((install / testenv.RECORD).read_text())
    data["commit"] = "unknown"
    (install / testenv.RECORD).write_text(json.dumps(data))
    assert ("FAIL", "build") in gate(install, "", "waived")[0]


def test_gate_nothing_left_fails(tmp_path):
    install = fake_install(tmp_path, results="\n".join(record(t, "PASS", 2) for t in "abc"))
    assert ("FAIL", "left") in gate(install, "", "waived")[0]
    install = fake_install(tmp_path / "two")
    assert ("OK", "left") in gate(install, "", "waived")[0]


def preflight_record(folder, text, **change):
    data = {"list": "left", "list_sha256": __import__("hashlib").sha256(text.encode()).hexdigest(), "mode": "owner",
            "result": "PASS", "passed": True, "build": "1234abcd", "time": "t"}
    data.update(change)
    folder.mkdir(exist_ok=True)
    (folder / "left.preflight.json").write_text(json.dumps(data))


@pytest.mark.parametrize("change, state", [
    ({}, "OK"), ({"mode": "automatic"}, "FAIL"), ({"passed": False, "result": "FAIL"}, "FAIL"),
    ({"list_sha256": "0" * 64}, "FAIL"), ({"build": "99999999"}, "FAIL"),
])
def test_gate_wants_the_owner_way_preflight(tmp_path, change, state):
    install = fake_install(tmp_path)
    preflight_record(tmp_path / "pf", LEFT_LIST, **change)
    assert (state, "preflight") in gate(install, str(tmp_path / "pf"), "")[0]


def test_gate_preflight_missing_or_waived(tmp_path):
    install = fake_install(tmp_path)
    assert ("FAIL", "preflight") in gate(install, "", "")[0]
    assert ("FAIL", "preflight") in gate(install, str(tmp_path / "none"), "")[0]
    assert ("WAIVED", "preflight") in gate(install, "", "a menus list checked an hour ago")[0]


def test_gate_deployed_copy_is_the_repo_list(tmp_path):
    install = fake_install(tmp_path)
    repo = LISTS / "theater.txt"
    (install / "test-runs" / "theater.txt").write_text(testenv.deployed_text(repo.read_text()))
    found, _ = gate(install, "", "waived", lists=("theater",))
    assert ("OK", "deployed") in found
    (install / "test-runs" / "theater.txt").write_text(repo.read_text() + "\n# an edit by hand\n")
    assert ("FAIL", "deployed") in gate(install, "", "waived", lists=("theater",))[0]


def test_owner_way_auto_text_and_problems():
    class Args:
        auto, owner, after = "", True, 6
    assert run_list.auto_text(Args) == "OWNER,PASS:6"
    Args.owner = False
    assert run_list.auto_text(Args) == "PASS:6"
    log = "arena-evolved: test run: verdicts pressed as in a run by hand, on the first pad, after 6 s\n"
    results = __import__("pathlib").Path
    assert run_list.owner_problems(LEFT_LIST, results("/nonexistent"), log) == [
        f"BY HAND {t}: no verdict: the pressed BACK + A never reached TEST RUN (a screen or dialog held the pad, the run "
        f"stopped, or the test never came up)" for t in "abc"]
    assert run_list.owner_problems(LEFT_LIST, results("/nonexistent"), "")[0].startswith("BY HAND the game did not take")


def test_owner_way_problems_read_the_records(tmp_path):
    path = tmp_path / "left.results.jsonl"
    path.write_text("\n".join([record("a", "PASS", 2), record("b", "SKIP", 2), record("c", "NOTE", 2)]) + "\n")
    log = "test run: verdicts pressed as in a run by hand, on the first pad, after 6 s"
    assert run_list.owner_problems(LEFT_LIST, path, log) == ["BY HAND c: recorded NOTE, not the pressed PASS"]


def test_preflight_owner_word_and_record(tmp_path):
    assert preflight.summary_word(3) == "STAGED, EXPECT FAILED"
    assert preflight.summary_word(3, owner=True) == "FAIL, EXPECT NOT MET BY THE SETUP ALONE"
    assert preflight.summary_word(0, owner=True) == "PASS"
    assert preflight.game_build("arena-evolved: test run: left started, run x, 3 of 3 tests, build 1234abcd-dirty\n") \
        == "1234abcd-dirty"
    listed = tmp_path / "left.txt"
    listed.write_text(LEFT_LIST)
    (tmp_path / "root" / "left").mkdir(parents=True)
    (tmp_path / "root" / "left" / "game.log").write_text("test run: left started, run x, 3 of 3 tests, build 1234abcd\n")
    target = preflight.write_record(tmp_path / "root", listed, "PASS", True, [])
    data = json.loads(target.read_text())
    assert data["mode"] == "owner" and data["passed"] and data["build"] == "1234abcd"
    assert data["list_sha256"] == __import__("hashlib").sha256(testenv.deployed_text(LEFT_LIST).encode()).hexdigest()


def test_deploy_refuses_a_build_with_no_commit(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(deploy_latest, "STAGING", tmp_path / "staging")
    tree = tmp_path / "tree"
    (tree / "build" / "linux64").mkdir(parents=True)
    (tree / "build" / "linux64" / "halo").write_bytes(b"program unknown")
    assert deploy_latest.main(["--from", str(tree), "--to", str(tmp_path / "install")]) == 1
    assert "no BUILD_COMMIT" in capsys.readouterr().out
    # (the staging folder goes when the run ends)
    assert not any((tmp_path / "staging").iterdir())


def test_deploy_uses_the_install_environment_when_to_is_omitted(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(deploy_latest, "STAGING", tmp_path / "staging")
    monkeypatch.setenv("AE_TEST_INSTALL", str(tmp_path / "install"))
    tree = tmp_path / "tree"
    tree.mkdir()
    assert deploy_latest.main(["--from", str(tree)]) == 1
    assert f"-> {tmp_path / 'install'}" in capsys.readouterr().out


def test_deploy_requires_to_without_the_install_environment(tmp_path, monkeypatch):
    monkeypatch.delenv("AE_TEST_INSTALL", raising=False)
    with pytest.raises(SystemExit):
        deploy_latest.main(["--from", str(tmp_path)])


def test_deploy_puts_a_build_in_place_with_its_record(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(deploy_latest, "STAGING", tmp_path / "staging")
    tree = tmp_path / "tree"
    build = tree / "build" / "linux64"
    build.mkdir(parents=True)
    (tree / "BUILD_COMMIT").write_text("1234abcd\n")
    (tree / "CHANGELOG.md").write_text("# changes\n")
    (build / "halo").write_bytes(b"program 1234abcd")
    for name in ("social.txt", "matchmaking.txt", "credits.txt", "forge_markers.bin", "tag_props.bin"):
        (build / name).write_text(name)
    install = tmp_path / "install"
    install.mkdir()
    (install / "social.txt").write_text("old playlists")
    # (a dry run changes nothing)
    assert deploy_latest.main(["--from", str(tree), "--to", str(install), "--label", "t"]) == 0
    assert (install / "social.txt").read_text() == "old playlists" and not (install / testenv.PROGRAM).exists()
    assert deploy_latest.main(["--from", str(tree), "--to", str(install), "--label", "t", "--go"]) == 0
    assert (install / "social.txt").read_text() == "social.txt"
    assert [p.name for p in install.glob("social.txt.prev-t-*")]
    assert json.loads((install / testenv.RECORD).read_text())["commit"] == "1234abcd"
    rows = testenv.Rows()
    assert testenv.check_build(install, rows) == "1234abcd"
    testenv.check_files(install, rows)
    assert all(row[0] == "OK" for row in rows.rows)
    # (a program that does not hold its BUILD_COMMIT's commit is refused)
    (build / "halo").write_bytes(b"program unknown")
    assert deploy_latest.main(["--from", str(tree), "--to", str(install), "--go"]) == 1
    assert "does not hold the commit" in capsys.readouterr().out


def test_box_sync_commit_stamp_and_command(tmp_path):
    import box_sync
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    (tmp_path / "a.c").write_text("int a;\n")
    (tmp_path / ".gitignore").write_text("build/\n")
    (tmp_path / "build").mkdir()
    (tmp_path / "build" / "x.o").write_text("object")
    env = dict(GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
    subprocess.run(["git", "-C", str(tmp_path), "add", "."], check=True)
    subprocess.run(["git", "-C", str(tmp_path), "commit", "-qm", "x"], check=True, env={**__import__("os").environ, **env})
    stamp = box_sync.commit_stamp(tmp_path)
    assert len(stamp) == 8 and not stamp.endswith("-dirty")
    (tmp_path / "a.c").write_text("int b;\n")
    (tmp_path / "new.c").write_text("int c;\n")
    assert box_sync.commit_stamp(tmp_path).endswith("-dirty")
    assert box_sync.file_list(tmp_path) == [".gitignore", "a.c", "new.c"]
    command = box_sync.rsync_command(tmp_path, "host:tree", tmp_path / "list", False)
    assert "--no-times" in command and "--checksum" in command and "--delete" not in command


CAMPAIGN_GROUP = """list camp
revision 1
group b30
map b30
gametype coop
test first
check A
setup goto 1 2 3
test second
check B
setup goto 4 5 6
{fresh}
"""


def test_lint_warns_a_campaign_goto_after_the_tester_walked_on(tmp_path):
    path = write(tmp_path, CAMPAIGN_GROUP.format(fresh=""), "camp")
    found = [message for _, message in test_run_lint.warnings(path)]
    assert any("test second: a goto in b30" in message for message in found)
    assert not any("test first:" in message for message in found)
    path = write(tmp_path, CAMPAIGN_GROUP.format(fresh="fresh"), "camp")
    assert not any("a goto in b30" in message for _, message in test_run_lint.warnings(path))


def test_lint_warns_a_kill_with_none_placed_in_the_test(tmp_path):
    text = """list kills
revision 1
group b30
map b30
gametype coop
test place
check A
setup actor commander 5
test kill
check B
setup actor kill
test both
check C
setup actor commander 5
setup actor kill
setup companion 2
setup companion kill
"""
    found = [message for _, message in test_run_lint.warnings(write(tmp_path, text, "kills"))]
    assert [m for m in found if "kill with none placed" in m] == [
        "test kill: actor kill with none placed in this test: those an earlier test placed may be dead when the tester "
        "gets here. Place and kill in one test"]


def test_runner_paths_ask_automatic_path():
    """ae_test_run.c: a place where an automatic run goes another way than a run by hand asks automatic_path(), so an
    OWNER run (preflight.py --owner) takes the owner's path. A bare `automatics` is allowed only in the verdict script's
    own functions and scripted_run() (what every scripted run does: the scene's picture, a list's presses). An `if (automatics && test->menus)` once gave
    automatic runs a screen reset that runs by hand did not get, and the preflight passed what failed in front of the
    owner."""
    import re
    source = (HERE.parent / "port" / "linux" / "game" / "ae_test_run.c").read_text(errors="replace")
    # (the comments out, their lines kept)
    source = re.sub(r"/\*.*?\*/", lambda found: "\n" * found.group(0).count("\n"), source, flags=re.S)
    allowed = {"parse_automatic", "automatic_key", "automatic_path", "scripted_run"}
    function, bare = "", []
    for number, line in enumerate(source.splitlines(), 1):
        match = re.match(r"^(?:static\s+)?[A-Za-z_][\w\s\*]*?\b(\w+)\s*\(", line)
        if match and not line.startswith((" ", "\t")) and not line.rstrip().endswith(";"):
            function = match.group(1)
        if re.search(r"\bautomatics\b", line) and function not in allowed and not line.startswith("static int"):
            bare.append((number, function, line.strip()))
    assert bare == [], bare


def test_lint_retired_lines(tmp_path):
    good = GOOD.format(name="ret") + "retired 1 measured the command, not the effect | cmd unseen ok\n"
    assert messages(write(tmp_path, good, "ret")) == []
    for line, words in (("retired 2 why | log x", "retired names no check"), ("retired 1 no bar here", "retired is"),
                        ("retired 1 | log x", "retired is"), ("retired 1 why |", "retired is")):
        found = messages(write(tmp_path, GOOD.format(name="ret") + line + "\n", "ret"))
        assert len(found) == 1 and words in found[0], (line, found)


def test_join_scores_the_testers_verdicts_on_the_same_build():
    text = LEFT_LIST + "retired 1 measured the command | cmd x ok\n"
    report = [{"test": "a", "check": 1, "verdict": "PASS", "rule": "log a"},
              {"test": "b", "check": 1, "verdict": "FAIL", "rule": "log b"},
              {"test": "c", "check": 1, "verdict": "LOOK", "rule": ""}]
    rows = [json.dumps({"list": "left", "test": "a", "verdict": "FAIL", "build": "1234abcd"}),
            json.dumps({"list": "left", "test": "b", "verdict": "FAIL", "build": "1234abcd-dirty"}),
            json.dumps({"list": "left", "test": "c", "verdict": "PASS", "build": "1234abcd"}),
            json.dumps({"list": "left", "test": "a", "verdict": "PASS", "build": "unknown"})]
    joined = testenv.join(text, "\n".join(rows), report, "1234abcd")
    by = {row["test"]: row for row in joined["tests"]}
    assert (by["a"]["predicted"], by["a"]["own" "er"]) == ("PASS", "FAIL")
    assert (by["b"]["predicted"], by["b"]["own" "er"]) == ("FAIL", "FAIL")
    assert (by["c"]["predicted"], by["c"]["own" "er"]) == ("not measured", "PASS")
    assert joined["other_builds"] == 1
    lines = testenv.join_lines("left", joined, testenv.retired_lines(text))
    assert "automated PASS -> your PASS: 0/1; automated FAIL -> your FAIL: 1/1; " \
           "your FAILs the automation passed: a" in lines[-2]
    assert lines[-1].startswith("  retired expect lines: 1") and "c #1: measured the command (cmd x ok)" in lines[-1]


def test_testenv_uses_the_install_environment_when_install_is_omitted(tmp_path, monkeypatch, capsys):
    install = fake_install(tmp_path)
    monkeypatch.setenv("AE_TEST_INSTALL", str(install))
    assert testenv.main(["left", "left"]) == 0
    assert "left: 3 of 3" in capsys.readouterr().out


def test_testenv_requires_install_without_the_environment(monkeypatch):
    monkeypatch.delenv("AE_TEST_INSTALL", raising=False)
    with pytest.raises(SystemExit):
        testenv.main(["left", "left"])


def test_spots_output_has_no_private_default(tmp_path, monkeypatch):
    monkeypatch.setenv("HALO_SPOTS_OUT", str(tmp_path))
    assert spots.out_dir() == tmp_path
    monkeypatch.delenv("HALO_SPOTS_OUT")
    with pytest.raises(SystemExit, match="HALO_SPOTS_OUT"):
        spots.out_dir()


def test_scenario_reader_is_found_by_argument_or_path(tmp_path, monkeypatch):
    tool = tmp_path / "xreader.py"
    tool.write_text("# reader\n")
    assert scenario_dump.reader_folder(tmp_path) == tmp_path
    monkeypatch.setattr(scenario_dump.shutil, "which", lambda name: str(tool) if name == "xreader.py" else None)
    assert scenario_dump.reader_folder(None) == tmp_path


def test_bot_gate_score_only_does_not_need_a_maps_folder(tmp_path, monkeypatch, capsys):
    import bot_gate

    monkeypatch.delenv("AE_TEST_MAPS", raising=False)
    monkeypatch.setattr(sys, "argv", ["bot_gate.py", "--game", "unused", "--out", str(tmp_path),
                                      "--score-only", "--scenes", "missing"])
    assert bot_gate.main() == 1
    assert "no logs: the scene did not run" in capsys.readouterr().out


import report_words  # noqa: E402


@pytest.mark.parametrize("rule, check, proxy", [
    ("cmd unseen me ok", "Enemies do not see you", True),
    ("cmd give dmr ok", "You hold a DMR", True),
    ("log ae randomizer: delete dialog opened", "X deletes the seed after a yes", True),
    ("log ae skulls: open", "The skull grid shows", True),
    ("log ae theater: open", "One row: NO RECORDINGS YET", True),
    ("log ae theater: open", "A on THEATER opens the Theater screen", False),
    ("log ae randomizer: deleted seed", "X deletes the seed after a yes", False),
    ("weapon sniper rifle loaded 4", "You pick up the sniper", False),
])
def test_proxy_reason(rule, check, proxy):
    assert bool(report_words.proxy_reason(rule, check)) == proxy


def test_report_words_never_say_a_bare_pass():
    rows = [{"test": "a", "check": 1, "rule": "log x", "verdict": "PASS", "text": "x shows"},
            {"test": "a", "check": 2, "rule": "cmd god ok", "verdict": "PASS", "text": "You take no damage"},
            {"test": "b", "check": 1, "rule": "log y", "verdict": "FAIL", "text": "y"},
            {"test": "b", "check": 2, "rule": "", "verdict": "LOOK", "text": "looks right"}]
    counts = report_words.count(rows, ["a", "b", "c"], ["c"], retired=1)
    assert counts == {"staged": 2, "tests": 3, "measured_passed": 1, "measured": 2, "proxy": 1, "look": 1, "retired": 1}
    assert report_words.words(counts) == "STAGED 2/3 · MEASURED 1/2 · PROXY 1 · LOOK 1 · RETIRED 1"
    assert "PASS" not in report_words.words(counts)


def test_lint_warns_a_proxy_expect(tmp_path):
    text = GOOD.format(name="px").replace("check Look at it\n", "check Enemies do not see you\n") + \
        "expect 1 cmd unseen me ok\n"
    found = [message for _, message in test_run_lint.warnings(write(tmp_path, text, "px"))]
    assert any("expect 1 is a proxy" in message for message in found)


def test_snapshot_keeps_his_settings_not_the_machine():
    text = """# comment
[display]
hud_style = "classic"
window_size = "1920x1080"
max_fps = 240
fov = 90
[paths]
data = "/x"
[input]
layout = "reclaimer"
[network]
online = true
"""
    snap = testenv.snapshot(text)
    assert snap.splitlines() == ["[display]", 'hud_style = "classic"', "fov = 90", "", "[input]", 'layout = "reclaimer"']


def test_gate_settings_row(tmp_path):
    install = fake_install(tmp_path)
    (install / "config.toml").write_text('[display]\nhud_style = "classic"\n')
    mine = testenv.snapshot_digest(install)
    preflight_record(tmp_path / "pf", LEFT_LIST, config_sha256=mine)
    assert ("OK", "settings") in gate(install, str(tmp_path / "pf"), "")[0]
    preflight_record(tmp_path / "pf", LEFT_LIST, config_sha256="0" * 64)
    assert ("WARN", "settings") in gate(install, str(tmp_path / "pf"), "")[0]
    preflight_record(tmp_path / "pf", LEFT_LIST)
    assert ("WARN", "settings") in gate(install, str(tmp_path / "pf"), "")[0]


def test_preflight_fails_placed_ai_out_of_view():
    import preflight

    log = PREFLIGHT_LOG.replace(
        "arena-evolved: test run setup ready first\n",
        "arena-evolved: test run setup ready first\n"
        "arena-evolved: test run: pre first scene picture test-runs/preflight/pre-first.png, actors placed 3, in view 0\n")
    rows = preflight.staging(PREFLIGHT_LIST, log, preflight_events())
    assert rows[0]["actors"] == (3, 0)
    assert any("none of the 3 placed AI" in words for test, words in preflight.problems(rows) if test == "first")
    rows = preflight.staging(PREFLIGHT_LIST, log.replace("in view 0", "in view 2"), preflight_events())
    assert not any("placed AI" in words for _, words in preflight.problems(rows))


def test_lint_press_lines(tmp_path):
    good = GOOD.format(name="pr") + "press 1 x a\n"
    assert messages(write(tmp_path, good, "pr")) == []
    for line, words in (("press 2 a", "press names no check"), ("press 1", "press has no button"),
                        ("press 1 jump", "press: 'jump' is not a button"), ("press 1 " + "a " * 30, "press is")):
        found = messages(write(tmp_path, GOOD.format(name="pr") + line + "\n", "pr"))
        assert len(found) == 1 and words in found[0], (line, found)


import frame_lint  # noqa: E402


def frame_box(kind, i, x, y, w, h, **more):
    box = {"k": kind, "i": i, "x": x, "y": y, "w": w, "h": h, "clip": [0, 0, 1000, 1000]}
    box.update(more)
    return box


def test_frame_lint_findings():
    panel = frame_box("shape", 0, 100, 100, 200, 50, thickness=0, top=0x202830FF, bottom=0x202830FF)
    fits = frame_box("text", 1, 110, 110, 150, 20, color=0xCDD6DDFF, text="FITS")
    past = frame_box("text", 2, 110, 130, 220, 15, color=0xCDD6DDFF, text="RUNS PAST")
    dim = frame_box("text", 3, 110, 100, 50, 10, color=0x30383FFF, text="DIM")
    over = frame_box("text", 4, 120, 112, 100, 18, color=0xCDD6DDFF, text="ON TOP")
    shadow = frame_box("text", 5, 111, 111, 150, 20, color=0xCDD6DDFF, text="FITS")
    cut = frame_box("text", 6, 500, 500, 300, 20, color=0xFFFFFFFF, text="CUT", clip=[500, 500, 600, 600])
    found = {(kind, text) for kind, text, _ in frame_lint.findings([panel, fits, past, dim, over, shadow, cut])}
    assert ("PAST", "RUNS PAST") in found and ("CONTRAST", "DIM") in found and ("CUT", "CUT") in found
    assert ("OVERLAP", "FITS") in found
    # (a shadow of the same text, the panel's own fitting text, white on the dark panel: none)
    assert not any(text == "FITS" and kind != "OVERLAP" for kind, text in found)
    assert frame_lint.findings([panel, fits]) == []


def test_frame_lint_contrast_numbers():
    assert round(frame_lint.contrast((1, 1, 1), (0, 0, 0)), 1) == 21.0
    assert round(frame_lint.contrast((0.5, 0.5, 0.5), (0.5, 0.5, 0.5)), 1) == 1.0


def test_test_page_renders_first_and_counts(tmp_path):
    import test_page

    info = {"id": "left", "revision": 2, "title": "LEFT", "tests": [{"id": "a", "title": "A"}, {"id": "b", "title": "B"}]}
    report = [{"test": "a", "check": 1, "text": "x deletes it", "rule": "cmd x ok", "verdict": "PASS", "reason": "ok"},
              {"test": "b", "check": 1, "text": "shows", "rule": "", "verdict": "LOOK", "reason": ""}]
    his = {"a": {"verdict": "FAIL", "note": "did <not> work", "build": "1234abcd"}}
    joined = testenv.join(LEFT_LIST, "", report, "1234abcd")
    joined["retired"] = []
    page = test_page.render(info, {"build": "1234abcd", "mode": "owner", "time": "2026-11-25T20:15:00"}, report,
                            {"proxies": [{"test": "a", "check": 1, "why": "a dev command's answer"}]}, his, {}, {},
                            joined, "left-1234abcd")
    assert "Look here first" in page and 'href="#a"' in page and "did &lt;not&gt; work" in page
    assert "only a proxy passed: #1" in page and "the automation passed it" not in page
    assert '<span class="chip proxy">PROXY</span>' in page and "your eyes only" in page
    assert "MEASURED<b>0/1</b>" not in page and "2026-11-25 20:15" in page


def test_test_page_uses_install_and_pages_environments(tmp_path, monkeypatch, capsys):
    import test_page

    install = fake_install(tmp_path)
    preflight = tmp_path / "preflight"
    preflight_record(preflight, LEFT_LIST)
    (preflight / "left").mkdir()
    (preflight / "left" / "report.json").write_text("[]")
    (preflight / "left" / "summary.json").write_text("{}")
    pages = tmp_path / "pages"
    monkeypatch.setenv("AE_TEST_INSTALL", str(install))
    monkeypatch.setenv("AE_TEST_PAGES", str(pages))
    assert test_page.main(["left", "--preflight", str(preflight)]) == 0
    assert (pages / "left-1234abcd.html").is_file()
    assert str(pages / "left-1234abcd.html") in capsys.readouterr().out


def test_test_page_explicit_pages_folder_overrides_the_environment(tmp_path, monkeypatch):
    import test_page

    install = fake_install(tmp_path)
    preflight = tmp_path / "preflight"
    preflight_record(preflight, LEFT_LIST)
    (preflight / "left").mkdir()
    (preflight / "left" / "report.json").write_text("[]")
    (preflight / "left" / "summary.json").write_text("{}")
    monkeypatch.setenv("AE_TEST_INSTALL", str(install))
    monkeypatch.setenv("AE_TEST_PAGES", str(tmp_path / "environment-pages"))
    explicit = tmp_path / "explicit-pages"
    assert test_page.main(["left", "--preflight", str(preflight), "--out", str(explicit)]) == 0
    assert (explicit / "left-1234abcd.html").is_file()
    assert not (tmp_path / "environment-pages").exists()


def test_test_page_requires_missing_install_and_pages_paths(tmp_path, monkeypatch):
    import test_page

    monkeypatch.delenv("AE_TEST_INSTALL", raising=False)
    monkeypatch.delenv("AE_TEST_PAGES", raising=False)
    with pytest.raises(SystemExit):
        test_page.main(["left", "--preflight", str(tmp_path)])
    install = fake_install(tmp_path)
    with pytest.raises(SystemExit):
        test_page.main([str(install), "left", "--preflight", str(tmp_path)])


def test_frame_lint_leaves_the_card_and_key_caps_alone():
    panel = frame_box("shape", 0, 0, 0, 1000, 600, thickness=0, top=0x202830FF, bottom=0x202830FF)
    cap = frame_box("shape", 1, 100, 500, 24, 22, thickness=0, top=0xC9D3DAFF, bottom=0xC9D3DAFF)
    hint = frame_box("text", 2, 130, 502, 300, 18, color=0xCDD6DDFF, text="Wheel scroll   Tab move")
    under = frame_box("text", 3, 700, 100, 200, 20, color=0xCDD6DDFF, text="SERGEANT MAJOR")
    card_text = frame_box("text", 4, 690, 95, 250, 20, color=0xCDD6DDFF, text="TEST 1 / 4")
    glyph = frame_box("text", 5, 106, 503, 10, 16, color=0x10161CFF, text="P")
    found = frame_lint.findings([panel, cap, hint, under, card_text, glyph], overlay=[680, 80, 1000, 300])
    assert found == [], found


def test_frame_lint_reads_a_dialog_over_a_screen():
    row = frame_box("shape", 0, 60, 300, 600, 40, thickness=0, top=0x202830BD, bottom=0x202830BD)
    behind = frame_box("text", 1, 600, 305, 30, 20, color=0xCDD6DDFF, text="38")
    dialog = frame_box("shape", 2, 420, 250, 440, 220, thickness=0, top=0x10161CF2, bottom=0x10161CF2)
    body = frame_box("text", 3, 436, 300, 400, 18, color=0xCDD6DDFF, text="This PC's 2 found skulls are forgotten")
    assert frame_lint.findings([row, behind, dialog, body]) == []
    # (the same body text wider than its dialog is past it)
    wide = dict(body, w=520)
    assert [kind for kind, _, _ in frame_lint.findings([row, behind, dialog, wide])] == ["PAST"]


def test_lint_warns_hurt_without_player_in_coop_and_with_nothing_placed(tmp_path):
    text = """list hu2
revision 1
group b30
map b30
gametype coop
set game.companions 3
test a
check Not hurt
setup actor commander 4
expect 1 hurt taken max 0
test b
check Not hurt
expect 1 hurt player 0 taken max 0
"""
    found = [message for _, message in test_run_lint.warnings(write(tmp_path, text, "hu2"))]
    assert any("test a: hurt without `player N`" in message for message in found)
    assert any("test b: hurt ... taken max 0 with no AI placed" in message for message in found)
    assert not any("test b: hurt without" in message or "test a: hurt ... taken max 0" in message for message in found)


def test_gate_host_unreachable_warns_and_brief_line(tmp_path, monkeypatch):
    install = fake_install(tmp_path)
    monkeypatch.setattr(testenv, "read_preflight", lambda source, list_id: testenv.UNREACHABLE)
    found, rows = gate(install, "host:/preflight", "")
    assert ("WARN", "preflight") in found and rows.failed() == 0
    assert testenv.brief_line(rows, ["left"]).startswith("TEST RUN left ready (2 warning(s): left: host did not answer")
    monkeypatch.setattr(testenv, "read_preflight", lambda source, list_id: None)
    _, rows = gate(install, "host:/preflight", "")
    line = testenv.brief_line(rows, ["left"])
    assert line.startswith("TEST RUN left not started: left: no record in host:/preflight. To fix: ")


def test_deploy_copies_a_standalone_test_tree(tmp_path):
    target = tmp_path / "testenv-tree"
    assert deploy_latest.fetch_tree(str(HERE.parent), target)
    for name in deploy_latest.TREE_FILES + ["tools/ae_test/testenv.py", "tools/ae_test/test_runs/pve-menus.txt"]:
        assert (target / name).exists(), name
    # (the copy runs alone: its lint finds its tables beside it)
    done = subprocess.run([sys.executable, str(target / "tools" / "ae_test" / "test_run_lint.py"),
                           str(target / "tools" / "ae_test" / "test_runs" / "rc8-smalls.txt")],
                          capture_output=True, text=True, cwd=tmp_path)
    assert done.returncode == 0, done.stdout + done.stderr


def test_requires_lines(tmp_path):
    good = GOOD.format(name="rq") + "requires mcc\nrequires workshop 3007701381\nrequires map bloodgulch\n"
    assert messages(write(tmp_path, good, "rq")) == []
    assert any("requires is" in m for m in messages(write(tmp_path, GOOD.format(name="rq") + "requires a steam pc\n", "rq")))
    needs = run_list.requirements(good)
    assert needs == [("mcc", ""), ("workshop", "3007701381"), ("map", "bloodgulch")]
    maps = tmp_path / "maps"
    maps.mkdir()
    assert run_list.unmet(needs, maps, "", []) == ["MCC (--mcc <its folder>)", "the Workshop item 3007701381 (in MCC's Steam library)",
                                                  "the map bloodgulch"]
    library = tmp_path / "steamapps"
    (library / "workshop" / "content" / "976730" / "3007701381").mkdir(parents=True)
    (library / "common" / "MCC").mkdir(parents=True)
    (maps / "bloodgulch.map").write_bytes(b"")
    assert run_list.unmet(needs, maps, str(library / "common" / "MCC"), []) == []
    assert preflight.summary_word(4, owner=True) == "NOT RUN, REQUIRES NOT MET"


def test_preflight_fails_a_fall_staged_too_low():
    import preflight

    text = PREFLIGHT_LIST.replace("test first\ncheck Look", "test first\ncheck Walk off and land: a hard landing lock")
    log = PREFLIGHT_LOG.replace(
        "arena-evolved: test run setup ready first\n",
        "arena-evolved: test run setup ready first\n"
        "arena-evolved: test run: pre first scene picture test-runs/preflight/pre-first.png, actors placed 0, in view 0, "
        "drop below 0.0, ahead 6.0\n")
    rows = preflight.staging(text, log, preflight_events())
    assert rows[0]["drops"] == (0.0, 6.0) and rows[0]["fall_check"]
    assert any("deepest drop here is 6.0 wu" in words for test, words in preflight.problems(rows) if test == "first")
    rows = preflight.staging(text, log.replace("ahead 6.0", "ahead 13.2"), preflight_events())
    assert not any("deepest drop" in words for _, words in preflight.problems(rows))


def test_join_does_not_count_a_proxy_as_a_prediction():
    report = [{"test": "a", "check": 1, "verdict": "PASS", "rule": "cmd god me ok", "text": "You take no damage"},
              {"test": "b", "check": 1, "verdict": "PASS", "rule": "log b", "text": "b shows"}]
    rows = "\n".join([json.dumps({"list": "left", "test": "a", "verdict": "FAIL", "build": "1234abcd"}),
                      json.dumps({"list": "left", "test": "b", "verdict": "PASS", "build": "1234abcd"})])
    by = {row["test"]: row for row in testenv.join(LEFT_LIST, rows, report, "1234abcd")["tests"]}
    assert by["a"]["predicted"] == "not measured" and by["a"]["proxy"] == {1: True}
    assert by["b"]["predicted"] == "PASS"


def test_gate_frame_switch(tmp_path):
    install = fake_install(tmp_path)
    preflight_record(tmp_path / "pf", LEFT_LIST, counts={"staged": 3, "tests": 3, "measured_passed": 0, "measured": 0,
                                                         "proxy": 0, "look": 3, "frame": 2})
    assert ("WARN", "frame") in gate(install, str(tmp_path / "pf"), "")[0]
    rows = testenv.Rows()
    commit = testenv.check_build(install, rows)
    testenv.check_list(install, "left", commit, rows, str(tmp_path / "pf"), "", frame_fails=True)
    assert ("FAIL", "frame") in {(row[0], row[1]) for row in rows.rows}
