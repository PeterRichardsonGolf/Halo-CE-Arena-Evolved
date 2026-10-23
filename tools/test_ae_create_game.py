"""CREATE GAME, one screen with its lobby (0.3.9: the game on the left, the lobby panel on the right), in real games,
headless, through the ae_test harness (tools/ae_test), in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's
helpers).

- CREATE GAME (debug.ae_test_screen 32) at 1080p 100 % and 130 %: it hosts at once (LAN: internet play is off here),
  player 1 in, the map chosen, the copy applied, the lobby live;
- 130 %: the rows scroll with the focus, down past START GAME into the panel;
- LOBBY SETUP (X): the popover at 100 % and 130 %; PUBLIC on and NETWORK to HOST ONLINE: the lobby opens again online
  (internet play off: its INVITE row says so);
- a quick option stepped and applied to the server;
- START GAME: the game plays (no AE screen over it), the gametype's PC options in it;
- BOTS and BOT SKILL at the panel's foot: the lobby's bots join at once, PLAYERS counts the person alone.

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
    # (130 %: the rows scroll with the focus, the help kept: down past START GAME into the panel's LOBBY row)
    ("create-130-foot", 32, 130, ("pad:down",) * 11, 34),
    # (START GAME, the primary action, focused: down to the last row of the left column)
    ("create-start", 32, 100, ("pad:down",) * 10, 34),
    # (the panel's LOBBY row focused: down to its last row, BOT SKILL, then up past BOTS)
    ("panel-lobby", 32, 100, ("wait",) + ("pad:down",) * 14 + ("pad:up", "pad:up"), 40),
    ("panel-lobby-130", 32, 130, ("wait",) + ("pad:down",) * 14 + ("pad:up", "pad:up"), 40),
    # (LOBBY SETUP: X opens it over the panel)
    ("setup", 32, 100, ("wait", "pad:x"), 30),
    ("setup-130", 32, 130, ("wait", "pad:x"), 30),
    # (NETWORK to HOST ONLINE, PUBLIC on, B: the lobby opens again online)
    ("online", 32, 100, ("wait", "pad:x", "pad:right", "pad:down", "pad:right", "pad:b", "wait", "wait"), 36),
    # (the lobby: down to the panel's last row (BOT SKILL), up to BOTS, 3 bots, then BOT SKILL focused)
    ("lobby-bots", 32, 100, ("wait",) + ("pad:down",) * 14 + ("pad:up", "pad:right", "pad:right", "pad:right",
                                                                 "wait", "wait", "pad:down"), 44),
    # (a quick option: down past MAP, GAMETYPE, GAME OPTIONS to the first, stepped)
    ("lobby", 32, 100, ("wait", "pad:down", "pad:down", "pad:down", "pad:right"), 34),
    ("play", 32, 100, ("wait", "wait", "pad:start"), 50),
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


def test_create_game_opens_a_live_lobby(runs):
    for name in ("create", "create-130"):
        said = lines(runs[name][1], "ae create game: ")
        assert "open" in said, (name, said)
        assert any(line.startswith("hosting (LAN, ") for line in said), (name, said)
        assert said.index("hosted, player 1 in") < said.index("lobby: open"), (name, said)
        assert "lobby: players 1, bots 0" in said, (name, said)
        assert not [line for line in said if line.startswith("open failed")], (name, said)


def test_lobby_setup(runs):
    for name in ("setup", "setup-130"):
        assert "lobby: setup open" in lines(runs[name][1], "ae create game: "), name
    said = lines(runs["online"][1], "ae create game: ")
    assert "lobby: public on" in said, said
    reopen = said.index("lobby: network HOST ONLINE, reopening")
    hosted = [index for index, line in enumerate(said) if line == "hosted, player 1 in"]
    assert len(hosted) == 2 and hosted[0] < reopen < hosted[1], said
    assert any(line.startswith("hosting (HOST ONLINE, ") for line in said[reopen:]), said
    assert "ae lobby: hosting (ONLINE, invite only)" in runs["online"][1]


def test_quick_option_reaches_the_server(runs):
    said = lines(runs["lobby"][1], "ae create game: ")
    assert "hosted, player 1 in" in said and "lobby: open" in said, said
    assert any(line.startswith("lobby: quick ") for line in said), said
    assert any("to the server" in line for line in lines(runs["lobby"][1], "ae game options: applied ")), said


def test_bots_rows(runs):
    said = lines(runs["lobby-bots"][1], "ae create game: ")
    assert "lobby: bots 1" in said and "lobby: bots 3" in said, said
    # (the lobby's bots follow the setting at once; PLAYERS counts the person alone)
    assert lines(runs["lobby-bots"][1], "lan bots: bot 3 joined the lobby"), said
    assert "lobby: players 1, bots 3" in said, said


def test_the_lobby_starts_the_game(runs):
    text = runs["play"][1]
    assert "lobby: start" in lines(text, "ae create game: ")
    assert "starting" in lines(text, "ae lobby: ")
    # (player 1 plays with a profile, not as "Player": the lobby gives it the one used last before adding it)
    assert "player 1 has no active player profile assigned" not in text
    # (the gametype's PC options reach the game, not the defaults: AE TEAM SLY's motion sensor shows enemies only
    # and it has a time limit)
    rules = lines(text, "game rules: ")
    assert rules and "radar players enemies" in rules[-1] and "time limit none" not in rules[-1], rules[-1:]


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "create-game")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, (lines(text, "ae create game: ") + lines(text, "ae lobby: "))[-6:])
