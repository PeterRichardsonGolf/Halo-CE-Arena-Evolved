"""Item 73 (LAN party 2026-10-11) in real games, headless through the ae_test harness, PC menus: leaving a LAN lobby
with split-screen players on one gamepad and the keyboard ("two times it froze when I backed out to the main menu"),
and Join Game > LAN's firewall hint on Linux.

Leaving. One gamepad and the keyboard: the pad shares port 0 with the keyboard, and is port 1's while there are two
players (xinput_sdl.c port_gamepad, menu_functions.c pc_menu_split_players). The test input's "pad:<button>" presses
are a virtual SDL gamepad's (ae_platform.c), so the pad goes through that real mapping. Each game: a profile made,
Create Game > LAN, Battle Creek, the first gametype, START GAME; in the lobby ADD PLAYER, the pad's START and A add
player 2 (controller 2); then they leave in turn, and B goes back to Multiplayer (where the host's game is let go:
"network server disposed").

- player 1 (the keyboard) leaves first: player 2's pad must stay port 1's. Before the fix it went back to port 0, port
  1 was unplugged, and "Please reconnect #2 controller." stood over the lobby for ever: no B or anything else left it;
- player 2 (the pad) leaves first, then player 1: as before.

The hint. Join Game > LAN with no game to find: after 10 s, with a firewall on (a fake system tree with ufw's
ENABLED=yes, $AE_TEST_FIREWALL_ROOT), debug.txt says so once; with none (this system, the box has no firewall),
nothing. The two lines themselves are in the screenshots (the hint's own cases are ae_lan_hint_test.c's).

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


def test_b_from_a_controller_with_no_player(cfg):
    """item 108: player 1 (the keyboard) leaves, then the keyboard's B again, its controller with no player now: the
    whole machine leaves the lobby (player 2 too) and B goes back out. Before the fix that B did nothing ("port lobby
    leave" failed) and the lobby stayed until every player pressed their own B"""
    out = menus.out_dir(cfg, "lobby-leave")
    result, text = play(cfg, out, "no-player-b", ["b", "wait", "wait", "wait", "b", "wait", "wait", "wait",
                                                  *BACK_TO_MULTIPLAYER])
    assert result.get("status") == "PASS", result.get("why")
    rest = joined_two(text)
    rest = after(rest, "requesting a player removal (controller index #0)")
    rest = after(rest, "requesting a player removal (controller index #1)")
    rest = after(rest, "ae lobby: B of controller #0 (no player of its own): this machine left the lobby (1 players)")
    after(rest, "network server disposed")


# up to the lobby only (no ADD PLAYER): the keyboard's player 1 alone with the host's LAN bots
TO_LOBBY_ALONE = ("down a a wait wait down down down down a wait wait a wait wait a wait wait up wait a wait wait wait "
                  "wait wait wait").split()


def test_last_person_leaves_with_lan_bots(cfg):
    """the host's only person, with 3 LAN bots (bots.count 3), presses B: they and the bots leave, the lobby goes back
    to Server Setup, and B goes back out. Before the fix the game counted the bots as this machine's players, so only
    the person left, the bots followed, and the host sat in an empty lobby ("port lobby leave" failed) it could not
    leave (owner, 2026-10-16)"""
    out = menus.out_dir(cfg, "lobby-leave")
    result, text = play(cfg, out, "lan-bots-b", ["b", "wait", "wait", "wait", *BACK_TO_MULTIPLAYER],
                        lead=TO_LOBBY_ALONE, env={"HALO_BOTS": "3"})
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, "lan bots: bot 3 joined the lobby")
    rest = after(rest, "ae lobby: B of controller #0")
    assert "this machine left the lobby (4 players)" in rest.split("\n", 1)[0], rest.split("\n", 1)[0]
    assert "'port lobby leave' failed" not in rest
    after(rest, "network server disposed")


def test_back_out_with_lan_bots_and_start(cfg):
    """the owner's flow: the host alone with 3 LAN bots backs out of the lobby to Server Setup, comes back with START
    GAME (focused there: player 1 and the bots join again), and START NOW starts the game"""
    out = menus.out_dir(cfg, "lobby-leave")
    result, text = play(cfg, out, "lan-bots-back-in", ["b", "wait", "wait", "wait", "a", "wait", "wait",
                                                       "wait", "wait", "wait", "wait", "down", "wait", "down", "wait", "down", "wait",
                                                       "down", "wait", "a", "wait",
                                                       "wait", "wait", "wait", "wait", "wait"],
                        lead=TO_LOBBY_ALONE, env={"HALO_BOTS": "3"}, exit_after=120)
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, "this machine left the lobby (4 players)")
    rest = after(rest, "server added player from machine #0 at controller index #0 to the game")
    rest = after(rest, "lan bots: bot 3 joined the lobby")
    after(rest, "game rules:")


# the main menu's MULTIPLAYER (a new profile's name screen, its Done), then Join Game > LAN
TO_LAN_LIST = "down a a wait wait down a".split()
HINT_LOG = "lan: no games found in 10 s and a firewall is on (ufw): No games found. On Linux a firewall can hide LAN games:"


def test_lan_hint_with_a_firewall(cfg):
    """ufw on (a fake /etc/ufw/ufw.conf): the hint after 10 s with nothing found, said once in debug.txt"""
    out = menus.out_dir(cfg, "lan-hint")
    # (beside the results, not under /tmp: each game has a /tmp of its own)
    root = out / "system"
    (root / "etc" / "ufw").mkdir(parents=True)
    (root / "etc" / "ufw" / "ufw.conf").write_text("# Set to yes to start on boot.\nENABLED=yes\nLOGLEVEL=low\n")
    result, text = play(cfg, out, "firewall", ["wait"] * 20, lead=TO_LAN_LIST, exit_after=45,
                        env={"AE_TEST_FIREWALL_ROOT": str(root)})
    assert result.get("status") == "PASS", result.get("why")
    assert text.count(HINT_LOG) == 1, [line for line in text.splitlines() if "lan:" in line]


def test_lan_hint_without_a_firewall(cfg):
    """no firewall (an empty fake tree): the list only looks, nothing in debug.txt"""
    out = menus.out_dir(cfg, "lan-hint")
    root = out / "system"
    root.mkdir()
    result, text = play(cfg, out, "none", ["wait"] * 20, lead=TO_LAN_LIST, exit_after=45,
                        env={"AE_TEST_FIREWALL_ROOT": str(root)})
    assert result.get("status") == "PASS", result.get("why")
    assert "lan: no games found" not in text
