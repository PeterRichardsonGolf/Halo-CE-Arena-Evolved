"""AE's lobby glue in real games, headless, through the ae_test harness (tools/test_ae_menus.py's pattern):

- the lobby drive (debug.ae_test_screen 90) hosts a LAN game with no upstream widget, 3 system-link bots join, it sets
  Blood Gulch and slayer, starts, ends the game, goes back to the lobby, and AE takes the pregame UI (the game's own
  SELECT MAP screen is never loaded);
- with display.arena_menus on but a session AE didn't make (network_test's host), the game's own pregame UI runs
  as before and the hook says it left it to the game.

Runs only when asked ($AE_MENUS_BUILD and the harness's config, as test_ae_menus.py); about 90 s a game:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_lobby.py
"""
import importlib.util
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_lobby_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

SELECT_MAP_FAILED = "failed to load map select postgame screen"


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


def play(cfg, out, name, spec):
    base = {"name": name, "build": menus.build(), "window": "1280x720", "screenshots": 120}
    base.update(spec)
    result = menus.harness.play(cfg, base, out / name)
    debug = out / name / "debug.txt"
    text = debug.read_text(errors="replace") if debug.exists() else ""
    pngs = sorted((out / name).rglob("frame*.png"))
    return result, text, pngs


def in_order(text, lines):
    """the index in text of each line, in order; a missing (or out of order) one fails with what came before"""
    position = 0
    for line in lines:
        found = text.find(line, position)
        assert found >= 0, f"missing or out of order: {line!r} (after {text[max(0, position - 400):position]!r})"
        position = found + len(line)


def test_host_bots_round_trip(cfg):
    """host + 3 bots: join, start, end, back to the lobby, with no upstream widget; the take-over shows AE's screen"""
    out = menus.out_dir(cfg, "lobby")
    result, text, pngs = play(cfg, out, "round-trip", {
        "exit_after": 90, "bots": 3,
        "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "90"},
        "expect": {"ticks": 0}})
    assert result.get("status") == "PASS", result.get("why")
    in_order(text, ["ae lobby: hosting (LAN)", "ae lobby: local player added (controller 1)", "ae lobby: roster 4",
                    "ae lobby: map bloodgulch", "ae lobby: gametype slayer", "ae lobby: starting",
                    "ae lobby: game ended", "ae lobby: back to pregame", "ae lobby: took the pregame UI",
                    "ae lobby: back in the lobby, roster 4"])
    assert SELECT_MAP_FAILED not in text
    assert "pregame UI left to the game" not in text
    assert pngs, "no screenshots"
    (out / "back-in-the-lobby.png").write_bytes(pngs[-1].read_bytes())


def test_not_ae_session_left_to_the_game(cfg):
    """preflight P20: display.arena_menus on, a session AE didn't make (network_test hosts, 1-minute games): back
    from the game the hook leaves the pregame UI to the game, whose screen loads"""
    out = menus.out_dir(cfg, "lobby-not-ae")
    result, text, pngs = play(cfg, out, "not-ae", {
        "map": "bloodgulch", "gametype": "slayer,slayer", "start": 10, "exit_after": 110,
        "env": {"HALO_ARENA_MENUS": "1", "HALO_NETWORK_TEST_TIME_LIMIT": "1"}})
    assert result.get("status") == "PASS", result.get("why")
    assert "ae lobby: pregame UI left to the game (not AE's session)" in text
    assert "ae lobby: took the pregame UI" not in text
    assert SELECT_MAP_FAILED not in text
