"""The game setup spike end to end (CREATE GAME, its LOBBY, a game) in real games, headless, through the ae_test
harness (tools/ae_test), in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's helpers).

- CREATE GAME (debug.ae_test_screen 32) at 1080p 100 % and 130 %;
- START GAME: hosted (LAN), player 1 in, the map chosen, the copy applied, the LOBBY; a quick option stepped there
  and applied to the server;
- the LOBBY's START GAME: the game plays (no AE screen over it);
- BOTS and BOT SKILL: stepped in CREATE GAME (bots.count, bots.skill written), and in the LOBBY (down from START GAME
  to the rows under the roster): the lobby's bots join at once, PLAYERS counts the person alone.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is 30-50 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_create_game.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_create_game.py      (the frames alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_create_game_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

# name, debug.ae_test_screen, UI SCALE, the presses (after the lead-in: one a second), seconds in all
CASES = (
    ("create", 32, 100, (), 30),
    ("create-130", 32, 130, (), 30),
    # (130 %: the rows scroll with the focus, the help kept: down to the last row)
    ("create-130-foot", 32, 130, ("pad:down",) * 12, 34),
    # (HOST ONLINE: PRIVACY and MAX PLAYERS shown, past BOTS and BOT SKILL to JOIN WITH INVITE)
    ("online", 32, 100, ("pad:down",) * 5 + ("pad:right",) + ("pad:down",) * 6, 30),
    # (LAN: BOTS is the eighth row, BOT SKILL the ninth: OFF -> 2, NORMAL -> HEROIC)
    ("create-bots", 32, 100, ("pad:down",) * 7 + ("pad:right", "pad:right", "pad:down", "pad:right"), 34),
    # (the lobby: down from START GAME to BOTS under the roster, 3 bots, then BOT SKILL focused)
    ("lobby-bots", 32, 100, ("pad:start", "wait", "wait", "wait", "pad:down", "pad:right", "pad:right", "pad:right",
                             "wait", "wait", "pad:down"), 40),
    ("lobby", 32, 100, ("pad:start", "wait", "wait", "wait", "pad:up", "pad:right"), 34),
    ("play", 32, 100, ("pad:start", "wait", "wait", "wait", "pad:start"), 50),
)


def play(cfg, out, name, value, scale, presses, seconds):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", "HALO_NET_ONLINE": "false"}
    return menus.play(cfg, out, name, views=value, env=env, shots=8, exit_after=seconds,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, value, scale, presses, seconds in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, value, scale, presses, seconds)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        runs[name] = (result, text, frame)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "create-game"))


def test_create_game_opens(runs):
    for name in ("create", "create-130"):
        assert "open" in lines(runs[name][1], "ae create game: "), name


def test_start_hosts_and_opens_the_lobby(runs):
    said = lines(runs["lobby"][1], "ae create game: ")
    assert "hosted, player 1 in" in said and "lobby: open" in said, said
    assert any(line.startswith("lobby: quick ") for line in said), said
    assert any("to the server" in line for line in lines(runs["lobby"][1], "ae game options: applied ")), said


def test_bots_rows(runs):
    said = lines(runs["create-bots"][1], "ae create game: ")
    assert "bots 1" in said and "bots 2" in said and "bot skill HEROIC" in said, said
    said = lines(runs["lobby-bots"][1], "ae create game: ")
    assert "lobby: bots 3" in said, said
    # (the lobby's bots follow the setting at once)
    assert lines(runs["lobby-bots"][1], "lan bots: bot 3 joined the lobby"), said


def test_the_lobby_starts_the_game(runs):
    text = runs["play"][1]
    assert "lobby: start" in lines(text, "ae create game: ")
    assert "starting" in lines(text, "ae lobby: ")


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "create-game")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, (lines(text, "ae create game: ") + lines(text, "ae lobby: "))[-6:])
