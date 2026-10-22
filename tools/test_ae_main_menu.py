"""AE's MAIN MENU and MULTIPLAYER (the 0.3.7 menus) in real games, headless, through the ae_test harness (tools/ae_test),
in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's helpers): no test screen, so AE's main menu takes the
game's main menu's place.

- the main menu at 1080p 100 % and 130 %, 21:9 (2560x1080), 3440x1440, 4K, 720p and 16:10 (1920x1200);
- MULTIPLAYER, CREATE GAME from it, and B back to MULTIPLAYER;
- CAMPAIGN, and its first level played;
- PROFILES: the list with no profile, a profile named on AE's keyboard, its colour changed in EDIT.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s. Name cases to run some:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_main_menu.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_main_menu.py [case ...]
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_main_menu_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
# name, window, UI SCALE, the presses (after the lead-in: one a second)[, screenshot every (8 frames), seconds in all
# (EXIT_AFTER)]
CASES = (
    ("menu", "1920x1080", 100, ()),
    ("menu-130", "1920x1080", 130, ()),
    ("menu-21x9", "2560x1080", 100, ()),
    ("menu-3440", "3440x1440", 100, ()),
    ("menu-4k", "3840x2160", 100, ()),
    ("menu-720", "1280x720", 100, ()),
    ("menu-16x10", "1920x1200", 100, ()),
    ("multiplayer", "1920x1080", 100, ("pad:a",)),
    ("create", "1920x1080", 100, ("pad:a", "pad:a")),
    ("create-back", "1920x1080", 100, ("pad:a", "pad:a", "wait", "pad:b")),
    # (CAMPAIGN: the levels; A on the first plays it at the difficulty shown)
    ("campaign", "1920x1080", 100, ("pad:down", "pad:a", "pad:right")),
    ("campaign-play", "1920x1080", 100, ("pad:down", "pad:a", "wait", "pad:a")),
    # (PROFILES: none on this PC; NEW PROFILE named on AE's keyboard with the second pad (the first counts as the
    # keyboard's, and the second's first press only attaches it): A opens it on Q, right, A types W, START keeps it
    # and makes 'w'; A edits it, right steps its COLOUR. Its presses outlast EXIT_AFTER: 40 s. Typing on a keyboard
    # (key:A) reaches the gallery's field headless but not this one's, nor do pad: presses once AE's keyboard is
    # open: not found yet, checked by hand)
    ("profiles", "1920x1080", 100, ("pad:down", "pad:down", "pad:down", "pad:a")),
    ("profiles-new", "1920x1080", 100, ("down", "down", "down", "a", "wait", "2:a", "2:a", "2:right", "2:a",
                                        "2:start", "wait", "a", "right"), 8, 40),
    # (SERVER BROWSER: the next filter (RB), a refresh (X); LAN GAMES: searching, then B ends the search)
    ("browser", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:a", "pad:rb", "pad:x")),
    ("lan", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a")),
    ("lan-back", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:b")),
)


def play(cfg, out, name, window, scale, presses, shots=8, exit_after=EXIT_AFTER):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": "AE TEAM SLY"}
    return menus.play(cfg, out, name, views=0, window=window, env=env, shots=shots, exit_after=exit_after,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, window, scale, presses, *more in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, window, scale, presses, *more)
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
    return run_cases(cfg, menus.out_dir(cfg, "main-menu"))


def test_the_main_menu_takes_the_game_menus_place(runs):
    for name, *_ in CASES:
        _, text, frame = runs[name]
        assert "open" in lines(text, "ae main menu: "), name
        assert frame, name


def test_multiplayer_and_create_game(runs):
    assert "MULTIPLAYER" in lines(runs["multiplayer"][1], "ae main menu: ")
    assert "CREATE GAME" in lines(runs["create"][1], "ae main menu: ")
    assert "open" in lines(runs["create"][1], "ae create game: ")
    said = lines(runs["create-back"][1], "ae create game: ")
    assert said[-1:] == ["closed"], said


def test_campaign(runs):
    assert lines(runs["campaign"][1], "ae campaign: open ")
    assert any(line.startswith("start level 1 on ") for line in lines(runs["campaign-play"][1], "ae campaign: "))
    # (the level loads after CAMPAIGN closes: AE's main menu never stays up over it)
    said = lines(runs["campaign-play"][1], "ae main menu: ")
    assert said[-1] in ("CAMPAIGN", "closed (the game left its main menu)"), said


def test_profiles(runs):
    assert lines(runs["profiles"][1], "ae profiles screen: open ")
    said = lines(runs["profiles-new"][1], "ae profiles screen: ")
    assert "made 'w'" in said, said
    assert "edit 'w'" in said, said
    glue = lines(runs["profiles-new"][1], "ae profiles: ")
    assert any(line.startswith("player 1 uses ") for line in glue), glue
    assert any(line.endswith(" colour 1 saved") for line in glue), glue


def test_browsers(runs):
    said = lines(runs["browser"][1], "ae browser: ")
    assert "open (server browser)" in said, said
    assert any(line.startswith("filter NOT FULL: ") for line in said), said
    assert "refresh" in said, said
    assert "open (lan games)" in lines(runs["lan"][1], "ae browser: ")
    assert "listing the LAN's games" in lines(runs["lan"][1], "ae lobby: ")
    said = lines(runs["lan-back"][1], "ae browser: ")
    assert said[-1:] == ["closed"], said
    assert "left" in lines(runs["lan-back"][1], "ae lobby: ")


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "main-menu")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, (lines(text, "ae main menu: ") + lines(text, "ae create game: "))[-4:])
