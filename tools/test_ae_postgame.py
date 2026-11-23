"""AE's post-game (0.4.0; port/linux/game/ae_screen_postgame.c) in real games, headless, through the ae_test harness
(tools/ae_test), in tools/test_ae_pause.py's pattern (tools/test_ae_menus.py's helpers):

- a game the host's kill ends (score 1, one bot to kill: debug.network_test_kill) shows AE's GAME RESULT, its rows
  counting in, then (player 1's A) CARNAGE, the AE scoreboard under the result banner; the host's A there goes back
  to the lobby (network_game_server_reset_to_pregame: AE's lobby takes the pregame UI);
- one player at 100 % and 130 % (the FINAL SCORE card on the right), FFA (the player's place, the leaders' card), two
  players in split screen (each player's own result in their own view);
- AE's menus off: the game's own carnage report, as before (no AE post-game).

The frames, taken by debug.capture_on_event at AE's own events (pg_counting: two rows in, the second counting;
pg_result: every row in; pg_carnage): <case>-<event>.png in the run's folder (ae_menus/<stamp>-postgame-ae).

Runs only when asked ($AE_MENUS_BUILD and the harness's config); about 70 s a game:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_postgame.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_postgame.py [case ...]   (the frames alone)
"""
import importlib.util
import os
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_postgame_ae_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

# seconds of waits before the presses: CREATE GAME opens, hosts, starts the game (start 5), the map loads, the host's
# kill ends it (3 s in), the game's 7 s before its scores, GAME RESULT's rows (about 5 s)
LEAD_IN = 36
TEAM, FFA = "AE TEAM SLY", "AE FFA SLAY"
EVENTS = ("pg_counting", "pg_result", "pg_carnage")
# (the report case's frames of the lobby and of the reopened report)
REPORT_EVENTS = ("pgcr_lobby", "pgcr_last")
# (the rivals case's: a player's card, the board with the rival marked, the service record page)
RIVAL_EVENTS = ("pgcr_card", "pgcr_rival", "record_page")
OPEN = "ae postgame: open ("
CARNAGE = "ae postgame: carnage"
CONTINUE = "ae postgame: continue: the server back to the pregame"
LOBBY = "ae lobby: took the pregame UI"
CLOSED = "ae postgame: closed"
LOBBY_OFF = "ae create game: lobby: off the stack"
# name, local players, gametype, UI SCALE, AE's menus, the presses after the lead-in (one a second): A to CARNAGE,
# a frame of it, A back to the lobby
CASES = (
    ("team", 1, TEAM, 100, True),
    ("team-130", 1, TEAM, 130, True),
    ("ffa", 1, FFA, 100, True),
    ("two", 2, TEAM, 100, True),
    ("four", 4, TEAM, 100, True),
    ("classic", 1, TEAM, 100, False),
    ("report", 1, TEAM, 100, True),
    ("rivals", 1, TEAM, 100, True),
)
PRESSES = ("a", "wait", "wait", "wait", "a", "wait", "wait", "wait", "wait", "wait")
# (A to the carnage report (the counting is long done), down to the other team's row, RB back to the first team's; A back
# to the lobby, RT reopens the last game's report there, B closes it)
REPORT_PRESSES = ("a", "wait", "wait", "wait", "down", "wait", "rb", "wait", "x", "wait", "wait", "a", "wait", "wait", "b",
                  "wait", "a", "wait", "wait", "wait", "wait", "rt", "wait", "wait", "wait", "b", "wait", "wait")
# (the rivals case, HALO_AE_PGCR_DEMO=4v4: this PC's player is the 4th red row. A to CARNAGE; down twice to a blue player,
# X opens their card, A takes SET RIVAL; up twice to this PC's player, X, A opens the service record, B back; down twice, X
# on the rival: REMOVE RIVAL on the card, B closes it)
RIVAL_PRESSES = ("a", "wait", "wait", "wait", "down", "down", "x", "wait", "wait", "a", "wait", "wait", "wait", "up", "up",
                 "x", "wait", "a", "wait", "wait", "wait", "b", "wait", "down", "down", "x", "wait", "wait", "b", "wait")


def seed_record(out):
    """a save root with a service record (this PC has played before: Kite, the made-up report's player of this PC, 14 games
    and a rival, Pike): what the record page and the rival marks read from the file"""
    root = out / "record-seed"
    (root / "private").mkdir(parents=True, exist_ok=True)
    modes = "9 5 120 90 22 3 2 60 40 11 0 0 0 0 0 2 1 32 30 8 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"
    (root / "private" / "service-record.txt").write_text(
        "# AE service record\nversion = 1\nclock = 14\n"
        f"player = kite | Kite | 14 | 14 8 212 160 41 | {modes} | 3:6,10:2,17:4,25:1,31:3\n"
        f"player = pike | Pike | 13 | 9 3 70 84 12 | 9 3 70 84 12 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | 3:2\n"
        "rival = kite | pike | Pike\n"
        "versus = kite | pike | 9 51 38\n")
    return root


def play(cfg, out, name, players, gametype, scale, arena):
    env = {"HALO_ARENA_MENUS": "1" if arena else "0", "HALO_ARENA_MENUS_SCALE": str(scale),
           "SDL_AUDIO_DRIVER": "dummy", "HALO_BOTS": "1", "HALO_BOT_SKILL": "recruit",
           "HALO_NETWORK_TEST_KILL": "3", "HALO_NETWORK_TEST_SCORE": "1",
           "HALO_CAPTURE_ON_EVENT": ",".join(EVENTS + (REPORT_EVENTS if name == "report" else ()) +
                                             (RIVAL_EVENTS if name == "rivals" else ()) +
                                             (("postgame",) if not arena else ()))}
    if name == "rivals":
        env["HALO_AE_PGCR_DEMO"] = "4v4"
    if players > 1:
        env["HALO_TEST_LOCAL_PLAYERS"] = str(players)
    presses = REPORT_PRESSES if name == "report" else RIVAL_PRESSES if name == "rivals" else PRESSES
    spec = {"name": name, "build": menus.build(), "window": "1920x1080", "menus": "pc", "map": "bloodgulch",
            "saved_gametype": gametype, "local_players": players, "start": 5,
            "exit_after": LEAD_IN + len(presses) + 3, "screenshots": 1000000,  # (only the captures: a frame every million)
            "test_input": "menu:" + " ".join(["wait"] * LEAD_IN + list(presses)), "expect": {"ticks": 0},
            "env": env}
    if name == "rivals":
        spec["save_from"] = str(seed_record(out))
    result = menus.harness.play(cfg, spec, out / name)
    debug = out / name / "debug.txt"
    text = debug.read_text(errors="replace") if debug.exists() else ""
    # (the captures in the order the log took them; the PNGs' names sort by time)
    taken = re.findall(r"test capture: (\w+)", text)
    pngs = sorted((out / name / "captures").glob("*.png"))
    frames = {}
    for event, png in zip(taken, pngs):
        frame = out / f"{name}-{event}.png"
        frame.write_bytes(png.read_bytes())
        frames[event] = frame
    return result, text, frames


def run_cases(cfg, out, only=()):
    runs = {}
    for name, players, gametype, scale, arena in CASES:
        if only and name not in only:
            continue
        runs[name] = play(cfg, out, name, players, gametype, scale, arena)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    # (AE_POSTGAME_CASES="report rivals": only those games, for the tests that read them)
    return run_cases(cfg, menus.out_dir(cfg, "postgame-ae"), tuple(os.environ.get("AE_POSTGAME_CASES", "").split()))


def after(text, line):
    assert line in text, f"missing: {line!r}"
    return text.split(line, 1)[1]


def test_game_result_then_carnage_then_the_lobby(runs):
    """AE's menus on: GAME RESULT opens as the game's scores would, its frames taken; A goes to CARNAGE; the host's A
    there takes everyone back to AE's lobby"""
    for name, players, gametype, scale, arena in CASES:
        if not arena or name == "rivals":
            continue
        result, text, frames = runs[name]
        assert result.get("status") == "PASS", (name, result.get("why"))
        rest = after(text, "switching to postgame")
        rest = after(rest, OPEN)
        rest = after(rest, CARNAGE)
        rest = after(rest, CONTINUE)
        rest = after(rest, LOBBY)
        # (the post-game closes under the lobby, which stays: it takes only its own screen off the stack)
        assert CLOSED in rest, name
        assert LOBBY_OFF not in rest.split(CLOSED, 1)[0], name
        assert set(frames) >= set(EVENTS), (name, sorted(frames))


def opened(text):
    """the open line's player count and kind"""
    found = re.search(r"ae postgame: open \((\d+) players, (\w+)\)", text)
    assert found, "no open line"
    return int(found.group(1)), found.group(2)


def test_teams_and_ffa(runs):
    assert opened(runs["team"][1]) == (2, "teams")
    assert opened(runs["ffa"][1]) == (2, "ffa")


def test_carnage_report_cursor_and_the_lobby_reopen(runs):
    """the carnage report (ae_screen_pgcr.c): shown once with the host's summaries, down and RB move the cursor between the
    teams' rows; once back in the lobby the last report reopens (RT), shows again as it was, and B closes it"""
    result, text, frames = runs["report"]
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, CARNAGE)
    assert re.search(r"ae pgcr: shown \(\d+ players, teams, summaries yes\)", rest)
    cursor = re.findall(r"ae pgcr: cursor (\d+) ", rest)
    assert cursor[:2] == ["1", "0"], cursor
    rest = after(rest, LOBBY)
    # (the report shown again from the lobby, then closed with B)
    again = after(rest, "ae pgcr: shown (")
    assert "ae pgcr: closed" in again


def test_carnage_card_service_record_page_and_the_record_file(runs):
    """the report case: X on this PC's player opens their card (only SERVICE RECORD: no rival of oneself), A opens the page,
    B closes the page; the finished game was added to the record once (the bot is not a player of the record) and the
    file was written"""
    result, text, frames = runs["report"]
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, CARNAGE)
    rest = after(rest, "ae pgcr: card ")
    rest = after(rest, "ae record: page ")
    assert "ae record: page closed" in rest
    assert text.count("ae record: game added (SLAYER, 1 players)") == 1, text.count("ae record: game added")
    assert re.search(r"ae record: service-record.txt written \(1 players, 0 rivals\)", rest) or \
        re.search(r"ae record: service-record.txt written \(1 players, 0 rivals\)", text)


def test_rival_set_on_a_card_marks_the_row_and_the_page_lists_it(runs):
    """the rivals case (the made-up report): A on a card's SET RIVAL keeps the rival for this profile, the board marks
    it, the player's own record page lists it; the next card of the rival says REMOVE RIVAL"""
    result, text, frames = runs["rivals"]
    assert result.get("status") == "PASS", result.get("why")
    # (the file this PC had: 2 players and a rival, read at the start)
    assert "ae record: service-record.txt: 2 players, 1 rivals" in text
    rest = after(text, CARNAGE)
    rest = after(rest, "ae pgcr: card ")
    rest = after(rest, "ae record: rival Kite -> ")
    assert " on" in rest.split("\n", 1)[0]
    # (both rivals kept in the file now)
    rest = after(rest, "ae record: service-record.txt written (2 players, 2 rivals)")
    rest = after(rest, "ae record: page Kite (14 games)")
    assert "ae record: page closed" in rest
    assert set(frames) >= set(RIVAL_EVENTS), sorted(frames)


def test_split_screen(runs):
    """each local player's own result in their own view: two and four players (bots even the teams up)"""
    for name, players in (("two", 2), ("four", 4)):
        count, kind = opened(runs[name][1])
        assert count > players and kind == "teams", (name, count, kind)


def test_classic_menus_keep_the_carnage_report(runs):
    """AE's menus off: no AE post-game; the game's carnage report and its buttons (ae_postgame.c's player 1 line)"""
    result, text, frames = runs["classic"]
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, "switching to postgame")
    assert OPEN not in rest
    assert "postgame: the carnage report's buttons are player 1's" in rest


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and $AE_MENUS_BUILD")
    out = menus.out_dir(c, "postgame-ae")
    for name, (result, text, frames) in run_cases(c, out, tuple(sys.argv[1:])).items():
        print(name, result.get("status"), sorted(str(f) for f in frames.values()))
    print(out)
