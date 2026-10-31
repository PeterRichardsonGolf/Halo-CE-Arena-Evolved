"""The carnage report in real games, headless, through the ae_test harness (owner, 2026-10-15):

- B asks first: B on the carnage report puts "LEAVE THE GAME?" over it (port/linux/game/ae_postgame.c) instead of
  leaving to the main menu at once (where the next B opened "exit Halo?"); B on the confirm stays on the report, A on
  it leaves as B did. On a two-player split-screen host whose game a kill ends (score 1);
- only player 1 controls the carnage report (owner: "p1/host only"): player 1 is the test input's own presses ("b",
  "a": port 0, the keyboard's), player 2 the virtual pad ("pad:b": a lone pad is port 1's while two play); player 2's
  B and A do nothing there;
- Custom Edition maps' button icons: the report's "%b-button =quit    %a-button =pick game" drew no icon on homobox@ce
  (Halo PC's hud_msg_icons_sm has none of the Xbox's sequences); now from the message icons sheet upstream's ce_hud.c
  gives the Xbox's icons (ae_buttons_tags.c), PlayStation's with a PlayStation pad ($AE_TEST_PAD_PLAYSTATION), here
  player 1's (debug.lone_pad_first).

The screenshots of each game are in the harness's out folder (ae_menus/<stamp>-postgame/<case>/shots).

Runs only when asked ($AE_MENUS_BUILD and the harness's config, as tools/test_ae_menus.py); about 60 s a game:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_postgame_game.py
"""
import importlib.util
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_postgame_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

ASKED = "postgame: leave the game? asked over the carnage report"
STAYED = "postgame: stayed on the carnage report (B on the confirm)"
LEFT = "postgame: leaving the game (A on the confirm)"
DISPOSED = "network server disposed"
POSTGAME = "switching to postgame"
CE_ICONS = "Custom Edition map: text icons (menus, carnage report) from the message icons' Xbox copy"
PLAYER_1 = "postgame: the carnage report's buttons are player 1's (gamepad 0)"
CE_PLAYSTATION = "PlayStation buttons: 1 sheets of the Xbox's icons ui.map gave this Custom Edition map"


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


def play(cfg, name, map_name, presses, waits=30, env=None):
    """a two-player host on map_name whose game the host's kill ends (score 1); presses one a second after waits
    seconds (the carnage report is up by then), then 15 s more"""
    out = menus.out_dir(cfg, "postgame")
    spec = {"name": name, "build": menus.build(), "window": "1280x720", "menus": "pc", "map": map_name,
            "local_players": 2, "start": 5, "exit_after": waits + len(presses) + 20, "screenshots": 30,
            "test_input": "menu:" + " ".join(["wait"] * waits + list(presses) + ["wait"] * 15),
            "expect": {"ticks": 0},
            "env": dict({"HALO_NETWORK_TEST_KILL": "3", "HALO_NETWORK_TEST_SCORE": "1",
                         "HALO_TEST_LOCAL_PLAYERS": "2"}, **(env or {}))}
    result = menus.harness.play(cfg, spec, out / name)
    debug = out / name / "debug.txt"
    return result, debug.read_text(errors="replace") if debug.exists() else ""


def after(text, line):
    """the text after the line (which must be there)"""
    assert line in text, f"missing: {line!r}"
    return text.split(line, 1)[1]


def test_b_asks_and_b_stays(cfg):
    """player 1's B on the report asks; their B on the confirm stays: the game is not left (its server stays up)"""
    result, text = play(cfg, "b-then-b", "wizard", ["b", "wait", "wait", "b", "wait", "wait"])
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, POSTGAME)
    rest = after(rest, PLAYER_1)
    rest = after(rest, ASKED)
    after(rest, STAYED)
    assert DISPOSED not in text.split(POSTGAME, 1)[1], "the game was left"


def test_b_asks_and_a_leaves(cfg):
    """player 1's B on the report asks; their A on the confirm leaves the game, as B alone did before"""
    result, text = play(cfg, "b-then-a", "wizard", ["b", "wait", "wait", "a", "wait", "wait"])
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, POSTGAME)
    rest = after(rest, ASKED)
    rest = after(rest, LEFT)
    after(rest, DISPOSED)
    assert STAYED not in text


def test_player_2_does_nothing(cfg):
    """player 2's (the pad's) B and A do nothing on the report or the confirm; player 1's B and A still do"""
    result, text = play(cfg, "player-2", "wizard", ["pad:b", "wait", "wait", "pad:a", "wait", "wait", "b", "wait",
                                                    "wait", "pad:a", "wait", "wait", "pad:b", "wait", "wait", "a",
                                                    "wait", "wait"])
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, POSTGAME)
    asked = rest.index(ASKED)
    # (nothing from player 2 before player 1's B: no confirm, no leaving; and no pick game, or player 1's B would
    # find no carnage report to ask over)
    assert DISPOSED not in rest[:asked]
    rest = rest[asked + len(ASKED):]
    # (player 2's A and B on the confirm: it stays up; player 1's A leaves)
    left = rest.index(LEFT)
    assert STAYED not in rest[:left] and DISPOSED not in rest[:left]
    after(rest[left:], DISPOSED)
    assert text.count(ASKED) == 1


def test_custom_edition_icons(cfg):
    """homobox@ce with a PlayStation pad as player 1: the text icons come from the message icons' Xbox copy, whose
    PlayStation sprites AE draws (the screenshots show the report's icons and the confirm's)"""
    result, text = play(cfg, "ce-icons", "homobox@ce", ["pad:b", "wait", "wait", "wait"], waits=40,
                        env={"AE_TEST_PAD_PLAYSTATION": "1", "HALO_DEBUG_LONE_PAD_FIRST": "true"})
    assert result.get("status") == "PASS", result.get("why")
    assert CE_PLAYSTATION in text
    # (said once, at the first text icon drawn: the in-game scoreboard prompt's, or the report's)
    assert CE_ICONS in text
    rest = after(text, POSTGAME)
    after(rest, ASKED)


AE_OPEN = "ae main menu: open"
AE_REPLACED = "ae menus: replaced the game's menus"
SHELL_MUSIC = "starting main menu music"
AE_UNWAITED = "ae menus: the game's main menu did not load: AE's opens without it"
AE_LEAVE_ASKED = "ae postgame: leave asked"
AE_LEFT = "ae postgame: leaving the game"


def test_ae_menus_take_the_main_menu_back(cfg):
    """with AE's menus on (display.arena_menus): back from a game AE's main menu opens only once the game has loaded
    its own main menu (main_screen_shell_load starts the music), so the replacing closes it; before 0.3.7.1 AE's
    opened first, on the ui scenario alone, and the game's main menu loaded after it drew under AE's. (0.4.0: the
    post-game is AE's own there, tools/test_ae_postgame.py: A to its CARNAGE, B asks, down to LEAVE, A leaves)"""
    result, text = play(cfg, "ae-back", "wizard", ["a", "wait", "wait", "b", "wait", "down", "wait", "a", "wait",
                                                   "wait"], waits=40, env={"HALO_ARENA_MENUS": "1"})
    assert result.get("status") == "PASS", result.get("why")
    rest = after(text, POSTGAME)
    rest = after(rest, AE_LEAVE_ASKED)
    rest = after(rest, AE_LEFT)
    rest = after(rest, DISPOSED)
    assert SHELL_MUSIC in rest and AE_REPLACED in rest, rest[-2000:]
    assert rest.index(SHELL_MUSIC) < rest.index(AE_OPEN) < rest.index(AE_REPLACED), rest[-2000:]
    # (by the game's main menu loading, not by the fallback's wait running out)
    assert AE_UNWAITED not in text
