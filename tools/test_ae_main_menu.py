"""AE's MAIN MENU and MULTIPLAYER (the 0.3.7 menus) in real games, headless, through the ae_test harness (tools/ae_test),
in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's helpers): no test screen, so AE's main menu takes the
game's main menu's place.

- the main menu at 1080p 100 % and 130 %, 21:9 (2560x1080), 3440x1440, 4K, 720p and 16:10 (1920x1200);
- MULTIPLAYER, CREATE GAME from it, and B back to MULTIPLAYER;
- CAMPAIGN, and its first level played;
- PROFILES: the list with no profile, a profile named on AE's keyboard, its colour changed in EDIT;
- SPLIT SCREEN: START on a second controller signs its player in, CREATE GAME's lobby takes them, B signs them out;
- JOIN WITH INVITE: empty, holding the clipboard's invite (HALO_AE_TEST_INVITE), a paste that is no invite (its
  error), AE's keyboard on it, and a join with internet play off (its reason on the field).

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s. Name cases to run some:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_main_menu.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_main_menu.py [case ...]
"""
import importlib.util
import sys
import shutil
import time
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
    # (0.4.2: a Nintendo pad (HALO_DEBUG_AE_MESSAGE_TEST=nintendo) reads its buttons by label in AE menus: its A, the
    # east button (pad:b), selects; its B, the south button (pad:a), goes back)
    ("nintendo-create", "1920x1080", 100, ("pad:b", "pad:b")),
    ("nintendo-back", "1920x1080", 100, ("pad:b", "pad:b", "wait", "pad:a")),
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
    # (0.3.9: PLAYERS read as information (a status's muted value), the co-op line under CO-OP; at 130 % too)
    ("campaign-130", "1920x1080", 130, ("pad:down", "pad:a")),
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
    # (JOIN WITH INVITE, MULTIPLAYER's second row: empty; the clipboard's invite in it; Y pasting a text that is no
    # invite; X opening AE's keyboard; A joining the clipboard's invite with internet play off)
    ("join", "1920x1080", 100, ("pad:a", "pad:down", "pad:a")),
    ("join-clipboard", "1920x1080", 100, ("pad:a", "pad:down", "pad:a")),
    ("join-paste", "1920x1080", 100, ("pad:a", "pad:down", "pad:a", "pad:y")),
    ("join-keyboard", "1920x1080", 100, ("pad:a", "pad:down", "pad:a", "pad:x")),
    ("join-offline", "1920x1080", 100, ("pad:a", "pad:down", "pad:a", "pad:a", "wait", "pad:a")),
    ("join-130", "1920x1080", 130, ("pad:a", "pad:down", "pad:a")),
    # (0.7.0 JOIN BY ADDRESS, from LAN GAMES' Y and SERVER BROWSER's Y; LB there is JOIN WITH INVITE: empty, a pad's X
    # keyboard (its ' key a colon), an address typed (Enter, 1 2 7 . 0 . 0 . 1), the empty LAN list's line, a bad text)
    ("address", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y")),
    ("address-130", "1920x1080", 130, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y")),
    ("address-keyboard", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                            "pad:x")),
    ("address-typed", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                         "key:Return", "key:1", "key:2", "key:7", "key:.", "key:0", "key:.", "key:0",
                                         "wait")),
    ("address-bad", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                       "key:Return", "key:a", "key:/", "key:b", "key:Return", "wait")),
    ("address-recent", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y")),
    ("address-recent-down", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                               "pad:down", "pad:down")),
    ("address-forget", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                          "pad:down", "pad:y")),
    # (nothing at the address: JOINING..., then within 10 s the field's Can't reach line, A tries again, X edits)
    ("address-joining", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                           "key:Return", "key:1", "key:2", "key:7", "key:.", "key:0", "key:.", "key:0",
                                           "key:.", "key:7", "key:7", "key:Return", "wait", "pad:a", "wait"), 8, 60),
    ("address-unreachable", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                               "key:Return", "key:1", "key:2", "key:7", "key:.", "key:0", "key:.",
                                               "key:0", "key:.", "key:7", "key:7", "key:Return", "wait", "pad:a",
                                               *(["wait"] * 16)), 8, 70),
    # (a name no resolver knows: looked up on a thread, "Looking up ...", then Can't reach; B during the lookup leaves it)
    ("address-name", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:y",
                                        "key:Return", "key:n", "key:o", "key:s", "key:u", "key:c", "key:h", "key:.", "key:i", "key:n", "key:v", "key:a", "key:l", "key:i", "key:d", "key:Return", "wait", "pad:a", *(["wait"] * 14)), 8, 60),
    ("address-browser", "1920x1080", 100, ("pad:a", "pad:down", "pad:down", "pad:a")),
    # (0.3.9 split screen: START on the second controller signs player 2 in (its card), CREATE GAME takes them into
    # the lobby after player 1; its B signs them out. The second START: the first may only attach the pad. The pad's
    # own A picks on its inline profile card: GUEST, focused)
    # ((0.4.3, owner) only while MY PARTY has the focus: X first; B back to the rows. Outside it START is refused)
    ("signin", "1920x1080", 100, ("pad:x", "2:start", "2:start", "wait", "2:a", "wait", "pad:b", "key:Return", "wait",
                                  "key:Return"), 8, 40),
    ("signout", "1920x1080", 100, ("pad:x", "2:start", "2:start", "wait", "2:a", "wait", "2:b")),
    ("signin-outside", "1920x1080", 100, ("2:start", "2:start", "wait", "2:a", "wait")),
    # (a lone pad sharing controller 1 with the keyboard: the keyboard used first, then the pad's START)
    ("lonepad", "1920x1080", 100, ("pad:x", "type:Down", "type:Up", "pad:start", "wait", "pad:a", "wait", "key:Escape",
                                   "key:Return", "wait", "key:Return"), 8, 40),
    ("lonepad-out", "1920x1080", 100, ("pad:x", "type:Down", "type:Up", "pad:start", "wait", "pad:a", "wait", "pad:b")),
)
INVITE = "halo://join/" + "0123456789abcdef" * 4
# the cases' own environment (the clipboard's text as JOIN WITH INVITE reads it: HALO_AE_TEST_INVITE)
CASE_ENV = {
    "nintendo-create": {"HALO_DEBUG_AE_MESSAGE_TEST": "nintendo"},
    "nintendo-back": {"HALO_DEBUG_AE_MESSAGE_TEST": "nintendo"},
    "join-clipboard": {"HALO_AE_TEST_INVITE": "join me: " + INVITE},
    "join-paste": {"HALO_AE_TEST_INVITE": "see you at 8"},
    "join-offline": {"HALO_AE_TEST_INVITE": INVITE, "HALO_NET_ONLINE": "false"},
    "join-130": {"HALO_AE_TEST_INVITE": INVITE},
}


def recent_seed_text():
    """four hosts: just now, yesterday, three days ago, and two weeks ago"""
    now = int(time.time())
    return (f"{now - 60}\t192.168.1.20\tHome Server\n{now - 90000}\t100.84.12.7:2302\tArena Night\n"
            f"{now - 3 * 86400}\tarena-host\tCreek 2v2\n{now - 20 * 86400}\t203.0.113.45:2310\tBTB Server\n")


def recent_seed(cfg, name):
    """a save root with RECENT's file (JOIN BY ADDRESS's list), for the cases that show it (in the harness's work
    folder: it refuses /tmp)"""
    root = menus.harness.expand(cfg["work_dir"]) / "seeds" / f"address-{name}"
    shutil.rmtree(root, ignore_errors=True)
    (root / "private").mkdir(parents=True)
    (root / "private" / "recent-addresses.txt").write_text(recent_seed_text())
    return root


def play(cfg, out, name, window, scale, presses, shots=8, exit_after=EXIT_AFTER):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", **CASE_ENV.get(name, {})}
    seed = recent_seed(cfg, name) if name in ("address-recent", "address-recent-down", "address-forget") else None
    return menus.play(cfg, out, name, views=0, window=window, env=env, shots=shots, exit_after=exit_after,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"), save_from=seed)


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
    assert any(line.startswith("made 'w'") for line in said), said
    assert "edit 'w'" in said, said
    glue = lines(runs["profiles-new"][1], "ae profiles: ")
    assert any(line.startswith("player 1 uses ") for line in glue), glue
    assert any(line.endswith(" colour 1 saved") for line in glue), glue


def test_join_with_invite(runs):
    assert "open (empty)" in lines(runs["join"][1], "ae join invite: ")
    assert "open (the clipboard's invite)" in lines(runs["join-clipboard"][1], "ae join invite: ")
    assert "pasted, not an invite" in lines(runs["join-paste"][1], "ae join invite: ")
    said = lines(runs["join-offline"][1], "ae join invite: ")
    assert "join failed: Internet play is off" in said, said
    # (the main menu's JOIN WITH INVITE no longer goes through CREATE GAME)
    assert not lines(runs["join"][1], "ae create game: ")


def test_join_by_address(runs):
    """(0.7.0) JOIN BY ADDRESS: LAN GAMES' Y opens it, empty or with RECENT (its file's four hosts, newest first);
    a pad's keyboard; an address typed; a bad text says why; Y forgets a recent host (the file written again)"""
    for name in ("address", "address-130", "address-keyboard", "address-typed", "address-bad"):
        assert "open (0 recent)" in lines(runs[name][1], "ae join address: "), name
    for name in ("address-recent", "address-recent-down", "address-forget"):
        assert "open (4 recent)" in lines(runs[name][1], "ae join address: "), name
    said = lines(runs["address-bad"][1], "ae join address: ")
    assert any(line.startswith("not an address: An address has letters") for line in said), said
    said = lines(runs["address-forget"][1], "ae join address: ")
    assert "forgot 100.84.12.7:2302" in said, said
    assert lines(runs["address-forget"][1], "ae join address: recent-addresses.txt could not") == []
    assert "ae browser: open (lan games)" in runs["address"][1]
    said = lines(runs["address-unreachable"][1], "ae join address: ")
    said = lines(runs["address-name"][1], "ae join address: ")
    assert "joining nosuch.invalid" in said and "join failed: The host did not answer" in said, said
    assert "looking up 'nosuch.invalid'" in lines(runs["address-name"][1], "ae lobby: "), runs["address-name"][1][-600:]
    said = lines(runs["address-unreachable"][1], "ae join address: ")
    assert "joining 127.0.0.77" in said and "join failed: The host did not answer" in said, said


def test_nintendo_pad_by_label(runs):
    said = lines(runs["nintendo-create"][1], "ae main menu: ")
    assert "CREATE GAME" in said, said
    assert "open" in lines(runs["nintendo-create"][1], "ae create game: ")
    said = lines(runs["nintendo-back"][1], "ae create game: ")
    assert said[-1:] == ["closed"], said


def test_split_screen_sign_in(runs):
    text = runs["signin"][1]
    assert "player 2 signed in (its own controller)" in lines(text, "ae main menu: "), lines(text, "ae main menu: ")
    # (CREATE GAME's lobby joins the signed-in pads now, each with the profile its card picked)
    assert "lobby: player 2 (signed in) joins: ok" in lines(text, "ae create game: "), lines(text, "ae create game: ")
    said = lines(text, "ae lobby: ")
    assert said.index("local player added (controller 1)") < said.index("local player added (controller 2)"), said
    said = lines(runs["signout"][1], "ae main menu: ")
    assert "player 2 signed out" in said and said.index("player 2 signed in (its own controller)") < said.index(
        "player 2 signed out"), said


def test_a_pad_signs_in_only_in_my_party(runs):
    """(0.4.3, owner 2026-10-28) the second pad's START with the menu's rows focused signs nobody in: refused"""
    said = lines(runs["signin-outside"][1], "ae main menu: ")
    assert "a pad's START outside MY PARTY: no sign in" in said, said
    assert "player 2 signed in (its own controller)" not in said, said


def test_split_screen_lone_pad(runs):
    """the keyboard plays player 1: a lone gamepad's START signs it in as player 2 (onto controller 2), B signs it out"""
    text = runs["lonepad"][1]
    said = lines(text, "ae main menu: ")
    assert "player 2 signed in (the keyboard's lone pad, now controller 2)" in said, said
    assert "lobby: player 2 (signed in) joins: ok" in lines(text, "ae create game: "), lines(text, "ae create game: ")
    said = lines(text, "ae lobby: ")
    assert said.index("local player added (controller 1)") < said.index("local player added (controller 2)"), said
    said = lines(runs["lonepad-out"][1], "ae main menu: ")
    assert "player 2 signed out" in said, said


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
