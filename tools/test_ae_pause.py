"""The AE pause menu (0.4.0; port/linux/game/ae_screen_pause.c) in real games, headless, through the ae_test harness
(tools/ae_test), in tools/test_ae_postgame_game.py's pattern (tools/test_ae_menus.py's helpers):

- one player (layout B: the list on the left, the game card on the right) at 100 % and 130 %, a team game (CHANGE
  TEAMS shown) and an FFA game (CHANGE TEAMS hidden); the host's rows under GAME (HOST);
- split screen, two and four players (layout A: the panel in that player's own view, no card), each pausing alone;
- RESUME (A) and B close it, and the player plays again (no input while it is open);
- SCOREBOARD, the BOTS page, END GAME's and CHANGE TEAMS' confirms;
- CHANGE MAP's RECENT (owner 2026-10-28: the pause menu's too; a seeded recent-games.txt): the maps played, A on one
  sets the next game's map only;
- split screen's SETTINGS in that player's own view, the other players' pauses still theirs;
- AE's menus off: the classic pause, as before.

Player 1 is the test input's plain presses (port 0: "start", "a", "down"); player 2 the virtual pad ("pad:start").
The frames: <case>.png in the run's folder (ae_menus/<stamp>-pause).

Runs only when asked ($AE_MENUS_BUILD and the harness's config); about 50 s a game:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_pause.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_pause.py [case ...]   (the frames alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_pause_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

# seconds of waits before the first press: CREATE GAME opens, hosts, starts the game (start 5), the map loads
LEAD_IN = 34
TEAM, FFA = "AE TEAM SLY", "AE FFA SLAY"
OPEN = "ae pause: open player "
# name, players, gametype, UI SCALE, AE's menus, the presses (after the lead-in: one a second)
CASES = (
    ("one", 1, TEAM, 100, True, ("start", "wait", "wait")),
    ("one-130", 1, TEAM, 130, True, ("start", "wait", "wait")),
    ("one-ffa", 1, FFA, 100, True, ("start", "wait", "wait")),
    # (two players: player 1 pauses, player 2 plays on; then player 2 pauses too)
    ("two", 2, TEAM, 100, True, ("start", "wait", "wait")),
    ("two-both", 2, TEAM, 100, True, ("start", "wait", "pad:start", "wait", "wait")),
    ("two-130", 2, TEAM, 130, True, ("start", "wait", "wait")),
    ("four", 4, TEAM, 100, True, ("start", "wait", "pad:start", "wait", "wait")),
    # (RESUME, the focused first row: A; and B)
    ("resume", 1, TEAM, 100, True, ("start", "wait", "a", "wait", "wait")),
    ("back", 1, TEAM, 100, True, ("start", "wait", "b", "wait", "wait")),
    # (SCOREBOARD: the fourth row in a team game; B back to the panel)
    ("scoreboard", 1, TEAM, 100, True, ("start", "wait", "down", "down", "down", "a", "wait", "wait")),
    ("scoreboard-back", 1, TEAM, 100, True, ("start", "wait", "down", "down", "down", "a", "wait", "b", "wait")),
    # (BOTS: down past the header, GAME OPTIONS, CHANGE MAP; A opens its page)
    ("bots", 1, TEAM, 100, True, ("start", "wait") + ("down",) * 7 + ("a", "wait", "wait")),
    # (END GAME asks; CANCEL is first; 0.7.0: the host's RESTART GAME comes before BOTS)
    ("end-asks", 1, TEAM, 100, True, ("start", "wait") + ("down",) * 8 + ("a", "wait", "wait")),
    # (the screens it opens: SETTINGS (player 1's), GAME OPTIONS and CHANGE MAP (the next game's), over the game)
    ("settings", 1, TEAM, 100, True, ("start", "wait", "down", "a", "wait", "wait")),
    ("options", 1, TEAM, 100, True, ("start", "wait") + ("down",) * 4 + ("a", "wait", "wait")),
    ("map", 1, TEAM, 100, True, ("start", "wait") + ("down",) * 5 + ("a", "wait", "wait")),
    # (CHANGE MAP's RECENT, from a seeded save root: up to the list's top, down to RECENT's first map; A takes it)
    ("map-recent", 1, TEAM, 100, True, ("start", "wait") + ("down",) * 5 + ("a", "wait") + ("up",) * 25 +
     ("down", "wait", "wait")),
    ("map-recent-pick", 1, TEAM, 100, True, ("start", "wait") + ("down",) * 5 + ("a", "wait") + ("up",) * 25 +
     ("down", "a", "wait", "wait")),
    # (split screen: SETTINGS in the player's own view, the other players' pauses theirs: player 2 opens one under it)
    ("two-settings", 2, TEAM, 100, True, ("start", "wait", "down", "a", "wait", "wait")),
    ("two-settings-other", 2, TEAM, 100, True, ("start", "wait", "down", "a", "wait", "pad:start", "wait", "wait")),
    # (player 2 cannot move player 1's SETTINGS to VIEW or open it; player 1's A still opens CONTROLS)
    ("two-settings-owned", 2, TEAM, 100, True, ("start", "wait", "down", "a", "wait", "pad:down", "pad:a",
                                                 "wait", "a", "wait", "wait")),
    # (four players: the keyboard's Enter is player 1's A here, the first port's A being another player's)
    ("four-settings", 4, TEAM, 100, True, ("start", "wait", "pad:start", "wait", "down", "wait", "key:Return", "wait",
                                           "wait")),
    # (CHANGE TEAMS asks: it kills the player; CANCEL first)
    ("teams-asks", 1, TEAM, 100, True, ("start", "wait", "down", "down", "a", "wait", "wait")),
    # (OFFLINE (CREATE GAME's NETWORK, issue #1): the pause stops the game, here 90 s, and it goes on with no network
    # error: the split screen session's client never goes stale)
    ("offline-long", 1, TEAM, 100, True, ("start",) + ("wait",) * 90 + ("b", "wait", "wait")),
    # (AE's menus off: the classic pause)
    ("classic", 1, TEAM, 100, False, ("start", "wait", "wait")),
    # (no campaign case: the harness's only level, a10, plays its opening scene for minutes, and no pause opens in a
    # cinematic, AE's or the classic; the campaign's rows are ae_pause_rows_test.c's)
)


def seeded_save(out):
    """a save root (a folder in out: never /tmp) whose private folder has RECENT's games: Prisoner twice, then Blood
    Gulch"""
    save = out / "seeded-save"
    (save / "private").mkdir(parents=True, exist_ok=True)
    (save / "private" / "recent-games.txt").write_text(
        "# AE recent games\ngame = prisoner | AE FFA SLAY\ngame = prisoner | AE TEAM SLY\n"
        "game = bloodgulch | AE TEAM SLY\n")
    return save


def play(cfg, out, name, players, gametype, scale, arena, presses):
    env = {"HALO_ARENA_MENUS": "1" if arena else "0", "HALO_ARENA_MENUS_SCALE": str(scale),
           "SDL_AUDIO_DRIVER": "dummy"}
    if players > 1:
        env["HALO_TEST_LOCAL_PLAYERS"] = str(players)
    # (a new lobby starts OFFLINE since 0.7.0: the other cases host a LAN game, as before)
    env["HALO_AE_NETWORK"] = "offline" if name.startswith("offline") else "lan"
    spec = {"name": name, "build": menus.build(), "window": "1920x1080", "menus": "pc", "map": "bloodgulch",
            "saved_gametype": gametype, "local_players": players, "start": 5,
            "exit_after": LEAD_IN + len(presses) + 3, "screenshots": 8,
            "test_input": "menu:" + " ".join(["wait"] * LEAD_IN + list(presses)), "expect": {"ticks": 0},
            "env": env}
    if name.startswith("map-recent"):
        spec["save_from"] = str(seeded_save(out))
    result = menus.harness.play(cfg, spec, out / name)
    debug = out / name / "debug.txt"
    text = debug.read_text(errors="replace") if debug.exists() else ""
    pngs = sorted((out / name).rglob("frame*.png")) or sorted((out / name).rglob("*.png"))
    frame = None
    if pngs:
        frame = out / f"{name}.png"
        frame.write_bytes(pngs[-1].read_bytes())
    return result, text, frame


def run_cases(cfg, out, only=()):
    runs = {}
    for name, players, gametype, scale, arena, presses in CASES:
        if only and name not in only:
            continue
        runs[name] = play(cfg, out, name, players, gametype, scale, arena, presses)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "pause"))


def after(text, line):
    assert line in text, f"missing: {line!r}"
    return text.split(line, 1)[1]


def test_opens_with_a_frame(runs):
    """START in game opens AE's pause for player 1, its frame drawn, in every case with AE's menus on"""
    for name, players, gametype, scale, arena, presses in CASES:
        result, text, frame = runs[name]
        assert result.get("status") == "PASS", (name, result.get("why"))
        assert frame, name
        if arena:
            assert OPEN + "1 (multiplayer" in text, name
        else:
            assert OPEN not in text, name


def test_host_rows(runs):
    """the host of a team game: CHANGE TEAMS and GAME (HOST)'s rows (the unit test checks which, these that they
    draw); an FFA game: no CHANGE TEAMS"""
    for name in ("one", "one-ffa"):
        assert runs[name][2], name


def test_each_split_screen_player_pauses_alone(runs):
    """two players: player 1's pause opens alone; player 2's START opens theirs too, in their own view"""
    _, text, _ = runs["two"]
    assert OPEN + "1 (multiplayer)" in text and OPEN + "2" not in text
    _, text, _ = runs["two-both"]
    rest = after(text, OPEN + "1 (multiplayer)")
    after(rest, OPEN + "2 (multiplayer)")
    _, text, _ = runs["four"]
    after(after(text, OPEN + "1 (multiplayer)"), OPEN + "2 (multiplayer)")


def test_resume_and_b_close_it(runs):
    for name in ("resume", "back"):
        _, text, _ = runs[name]
        after(after(text, OPEN + "1"), "ae pause: player 1 resumes")


def test_offline_pause_stops_the_game(runs):
    """OFFLINE: the pause stops the game's time, 90 s of it end with no network error, B runs it again; a LAN game's
    pause never stops it (its client would go stale)"""
    _, text, _ = runs["offline-long"]
    rest = after(after(text, OPEN + "1 (multiplayer, offline: stopped)"), "ae pause: the game's time runs again")
    assert "ae errors:" not in text
    assert "offline: stopped" not in runs["back"][1] and "the game's time stopped" not in runs["back"][1]


def test_scoreboard(runs):
    _, text, _ = runs["scoreboard"]
    after(after(text, OPEN + "1"), "ae pause: player 1 scoreboard")
    _, text, _ = runs["scoreboard-back"]
    rest = after(after(text, OPEN + "1"), "ae pause: player 1 scoreboard")
    assert "ae pause: player 1 resumes" not in rest


def test_screens_it_opens(runs):
    for name, line in (("settings", "ae settings: open player 1"), ("options", "ae game options: open"),
                       ("map", "ae change map: open")):
        _, text, _ = runs[name]
        after(after(text, OPEN + "1"), line)


def test_change_map_recent(runs):
    """the pause menu's CHANGE MAP shows RECENT (the maps played, each once), and A on one sets the next game's map
    only"""
    text = after(runs["map-recent"][1], OPEN + "1")
    assert "ae change map: recent 2: 'prisoner', 'bloodgulch'" in text
    # (the focus line names the map's list name: its path's last part is the id)
    assert [line for line in text.splitlines() if "ae change map: focus '" in line and "prisoner' (recent)" in line]
    pick = after(runs["map-recent-pick"][1], OPEN + "1")
    assert "ae change map: pick recent 'prisoner'" in pick
    assert [line for line in pick.splitlines() if "ae change map: pick '" in line and "prisoner' (for the caller)" in line]
    assert "ae game options: begin " not in pick, "a RECENT map sets no gametype"


def test_split_screen_settings(runs):
    """SETTINGS from a split-screen pause draws in that player's view (its inset logged by the screen's own layout: the
    frames show it), and another player's START still opens their own pause under it"""
    for name in ("two-settings", "four-settings"):
        _, text, _ = runs[name]
        after(after(text, OPEN + "1"), "ae settings: open player 1")
    _, text, _ = runs["two-settings-other"]
    rest = after(after(text, OPEN + "1"), "ae settings: open player 1")
    after(rest, OPEN + "2 (multiplayer)")
    _, text, _ = runs["two-settings-owned"]
    rest = after(after(text, OPEN + "1"), "ae settings: open player 1")
    assert "ae settings: VIEW" not in rest
    after(rest, "ae settings: CONTROLS")


def test_change_teams_asks(runs):
    _, text, _ = runs["teams-asks"]
    rest = after(after(text, OPEN + "1"), "ae pause: player 1 CHANGE TEAMS")
    assert "CHANGE TEAMS confirmed" not in rest and "player 1 resumes" not in rest


def test_bots_page_and_end_asks(runs):
    _, text, _ = runs["bots"]
    after(after(text, OPEN + "1"), "ae pause: player 1 bots page")
    _, text, _ = runs["end-asks"]
    rest = after(after(text, OPEN + "1"), "ae pause: player 1 END GAME")
    assert "ae pause: player 1 END GAME confirmed" not in rest
    assert "switching to postgame" not in rest


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and $AE_MENUS_BUILD")
    out = menus.out_dir(c, "pause")
    for case, (result, _, frame) in run_cases(c, out, sys.argv[1:]).items():
        print(case, result.get("status"), frame)
