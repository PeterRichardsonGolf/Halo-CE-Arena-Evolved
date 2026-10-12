"""Item 73 (LAN party 2026-10-11) in real games, headless through the ae_test harness, PC menus: leaving a LAN lobby
with split-screen players on one gamepad and the keyboard ("two times it froze when I backed out to the main menu").

Leaving. One gamepad and the keyboard: the pad shares port 0 with the keyboard, and is port 1's while there are two
players (xinput_sdl.c port_gamepad, menu_functions.c pc_menu_split_players). The test input's "pad:<button>" presses
are a virtual SDL gamepad's (ae_platform.c), so the pad goes through that real mapping. Each game: a profile made,
Create Game > LAN, Battle Creek, the first gametype, START GAME; in the lobby ADD PLAYER, the pad's START and A add
player 2 (controller 2); then they leave in turn, and B goes back to Multiplayer (where the host's game is let go:
"network server disposed").

- player 1 (the keyboard) leaves first: player 2's pad must stay port 1's. Before the fix it went back to port 0, port
  1 was unplugged, and "Please reconnect #2 controller." stood over the lobby for ever: no B or anything else left it;
- player 2 (the pad) leaves first, then player 1: as before.

Runs only when asked ($AE_MENUS_BUILD and the harness's config, as tools/test_ae_menus.py); 30 to 95 s a game:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_lan_lobby.py
"""
import importlib.util
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_lan_lobby_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

# the main menu's MULTIPLAYER (a fresh save root: a new profile's name screen, its Done), Create Game > LAN, the map,
# the gametype, Server Setup's START GAME (up from its name row), and in the lobby: down to its buttons, right to ADD
# PLAYER, A; the pad's START joins, its A takes the profile offered
TO_LOBBY = ("down a a wait wait down down down down a wait wait a wait wait a wait wait up wait a wait wait wait wait "
            "down wait wait right wait wait a wait wait pad:start wait wait pad:a wait wait wait").split()
# B back from Server Setup, Select Gametype and Select Map to Multiplayer
BACK_TO_MULTIPLAYER = "b wait wait b wait wait b wait wait wait".split()


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


def play(cfg, out, name, presses, lead=TO_LOBBY, exit_after=95, env=None):
    spec = {"name": name, "build": menus.build(), "window": "1280x720", "menus": "pc", "exit_after": exit_after,
            "screenshots": 120, "test_input": menus.menu_input(*lead, *presses), "expect": {"ticks": 0},
            "env": env or {}}
    result = menus.harness.play(cfg, spec, out / name)
    debug = out / name / "debug.txt"
    return result, debug.read_text(errors="replace") if debug.exists() else ""


def after(text, line):
    """the text after the line (which must be there)"""
    assert line in text, f"missing: {line!r}"
    return text.split(line, 1)[1]


def joined_two(text):
    rest = after(text, "test input: virtual gamepad attached")
    rest = after(rest, "server added player from machine #0 at controller index #0 to the game")
    return after(rest, "server added player from machine #0 at controller index #1 to the game")


def test_player_1_leaves_first(cfg):
    """the keyboard's player 1 leaves; player 2's pad keeps port 1, so their B leaves too, and B goes back out"""
    out = menus.out_dir(cfg, "lobby-leave")
    result, text = play(cfg, out, "p1-first", ["b", "wait", "wait", "wait", "pad:b", "wait", "wait", "wait",
                                               *BACK_TO_MULTIPLAYER])
    assert result.get("status") == "PASS", result.get("why")
    rest = joined_two(text)
    rest = after(rest, "requesting a player removal (controller index #0)")
    rest = after(rest, "requesting a player removal (controller index #1)")
    after(rest, "network server disposed")


def test_player_2_leaves_first(cfg):
    """the pad's player 2 leaves, then the keyboard's player 1, and B goes back out (as before the fix)"""
    out = menus.out_dir(cfg, "lobby-leave")
    result, text = play(cfg, out, "p2-first", ["pad:b", "wait", "wait", "wait", "b", "wait", "wait", "wait",
                                               *BACK_TO_MULTIPLAYER])
    assert result.get("status") == "PASS", result.get("why")
    rest = joined_two(text)
    rest = after(rest, "requesting a player removal (controller index #1)")
    rest = after(rest, "requesting a player removal (controller index #0)")
    after(rest, "network server disposed")

