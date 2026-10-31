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
)
PRESSES = ("a", "wait", "wait", "wait", "a", "wait", "wait", "wait", "wait", "wait")


def play(cfg, out, name, players, gametype, scale, arena):
    env = {"HALO_ARENA_MENUS": "1" if arena else "0", "HALO_ARENA_MENUS_SCALE": str(scale),
           "SDL_AUDIO_DRIVER": "dummy", "HALO_BOTS": "1", "HALO_BOT_SKILL": "recruit",
           "HALO_NETWORK_TEST_KILL": "3", "HALO_NETWORK_TEST_SCORE": "1",
           "HALO_CAPTURE_ON_EVENT": ",".join(EVENTS + (("postgame",) if not arena else ()))}
    if players > 1:
        env["HALO_TEST_LOCAL_PLAYERS"] = str(players)
    spec = {"name": name, "build": menus.build(), "window": "1920x1080", "menus": "pc", "map": "bloodgulch",
            "saved_gametype": gametype, "local_players": players, "start": 5,
            "exit_after": LEAD_IN + len(PRESSES) + 3, "screenshots": 1000000,  # (only the captures: a frame every million)
            "test_input": "menu:" + " ".join(["wait"] * LEAD_IN + list(PRESSES)), "expect": {"ticks": 0},
            "env": env}
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
    return run_cases(cfg, menus.out_dir(cfg, "postgame-ae"))


def after(text, line):
    assert line in text, f"missing: {line!r}"
    return text.split(line, 1)[1]


def test_game_result_then_carnage_then_the_lobby(runs):
    """AE's menus on: GAME RESULT opens as the game's scores would, its frames taken; A goes to CARNAGE; the host's A
    there takes everyone back to AE's lobby"""
    for name, players, gametype, scale, arena in CASES:
        if not arena:
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
