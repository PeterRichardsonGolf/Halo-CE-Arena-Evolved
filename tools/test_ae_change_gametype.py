"""AE's CHANGE GAMETYPE screen (debug.ae_test_screen 33, and from CREATE GAME's GAMETYPE row, 32) in real games,
headless, through the ae_test harness (tools/ae_test), in tools/test_ae_change_map.py's pattern (tools/test_ae_menus.py's
helpers).

- opens on the edit's gametype (TEAM AE BR 2V2 CTF): AE 2V2 open, the pair's row focused on BR START; 1080p at 100 %
  and 130 %;
- LT / RT from folder to folder (the test pad's triggers), LB / RB from filter to filter, the CTF filter set at the
  start;
- left / right on the pair: PISTOL START and BR START;
- a search with results ("ctf") and one with none ("qzx", the empty state), B clearing a search; X on a pad opens
  AE's keyboard;
- the open folder's header pinned on the list's top once it scrolls off (deep in AE ACTION SACK);
- Y FAVOURITE; A picks: alone, the GAME OPTIONS edit begins on the gametype picked (its file); from CREATE GAME, the
  pick comes back to it (the pair's PISTOL START one).

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_change_gametype.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_change_gametype.py      (the frames alone)
"""
import importlib.util
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_change_gametype_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
SCREEN = 33
CREATE_GAME = 32
GAMETYPE = "AE BR 2V2 C"
# name, debug.ae_test_screen, UI SCALE, the presses (after the lead-in: one a second), more environment
CASES = (
    ("open", SCREEN, 100, (), {}),
    ("open-130", SCREEN, 130, (), {}),
    ("folders", SCREEN, 100, ("pad:rt", "pad:rt", "pad:lt"), {}),
    ("filters", SCREEN, 100, ("pad:rb", "pad:rb", "pad:lb"), {}),
    ("filter-ctf", SCREEN, 100, ("pad:down",), {"HALO_AE_TEST_GAMETYPE_FILTER": "2"}),
    ("start", SCREEN, 100, ("pad:left", "pad:right", "pad:left"), {}),
    ("search", SCREEN, 100, ("pad:down",), {"HALO_AE_TEST_GAMETYPE_SEARCH": "ctf"}),
    ("search-130", SCREEN, 130, ("pad:down",), {"HALO_AE_TEST_GAMETYPE_SEARCH": "ctf"}),
    ("search-empty", SCREEN, 100, (), {"HALO_AE_TEST_GAMETYPE_SEARCH": "qzx"}),
    ("search-clear", SCREEN, 100, ("pad:b",), {"HALO_AE_TEST_GAMETYPE_SEARCH": "ctf"}),
    ("keyboard", SCREEN, 100, ("pad:x",), {}),
    # (Y on the pair, then up to the list's top: FAVOURITES above the folders)
    ("top", SCREEN, 100, ("pad:y",) + ("pad:up",) * 8, {}),
    ("pick", SCREEN, 100, ("pad:a",), {}),
    # (deep in AE ACTION SACK: its header scrolled off, pinned on the list's top)
    ("pinned", SCREEN, 100, ("pad:down",), {"HALO_AE_TEST_GAMETYPE": "AE SHOTGUNS"}),
    # (CREATE GAME: down to GAMETYPE, A opens CHANGE GAMETYPE, left to the pair's PISTOL START, A picks)
    ("create-pick", CREATE_GAME, 100, ("pad:down", "pad:a", "wait", "pad:left", "pad:a"), {}),
)


def play(cfg, out, name, screen, scale, presses, env):
    e = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": GAMETYPE, "HALO_NET_ONLINE": "false"}
    e.update(env)
    return menus.play(cfg, out, name, views=screen, window="1920x1080", env=e, shots=8, exit_after=EXIT_AFTER,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, screen, scale, presses, env in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, screen, scale, presses, env)
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
    return run_cases(cfg, menus.out_dir(cfg, "change-gametype"))


def opened(text):
    found = lines(text, "ae change gametype: open ")
    assert found, "the screen opened"
    return found[0]


def test_opens_on_the_edits_gametype(runs):
    for name, *_ in CASES:
        _, text, frame = runs[name]
        assert frame, name
        assert lines(text, "ae change gametype: gametypes "), name
    for name in ("open", "open-130"):
        text = runs[name][1]
        gametypes = lines(text, "ae change gametype: gametypes ")[0]
        assert f"current '{GAMETYPE}'" in gametypes and int(re.match(r"(\d+) \(pairs (\d+)\)", gametypes)[2]) >= 20
        assert opened(text).startswith("(filter ALL): ")
        assert int(re.match(r"\(filter ALL\): (\d+) gametypes", opened(text))[1]) >= 80
        assert lines(text, "ae change gametype: folder ")[0] == "AE 2V2 open"
        assert f"'{GAMETYPE}' (BR START)" in lines(text, "ae change gametype: focus ")[0]


def test_the_pinned_header(runs):
    text = runs["pinned"][1]
    assert lines(text, "ae change gametype: folder ")[0] == "AE ACTION SACK open"
    assert "'AE HEAVIES'" in lines(text, "ae change gametype: focus ")[-1]


def test_folders_and_filters(runs):
    folders = lines(runs["folders"][1], "ae change gametype: folder ")
    assert "AE 4V4 open" in folders and "AE BTB open" in folders and folders[-1] == "AE 4V4 open"
    filters = lines(runs["filters"][1], "ae change gametype: filter ")
    assert [f.split(":")[0] for f in filters] == ["SLAYER", "CTF", "SLAYER"]
    assert opened(runs["filter-ctf"][1]).startswith("(filter CTF): ")


def test_the_pairs_start(runs):
    starts = lines(runs["start"][1], "ae change gametype: start ")
    assert starts == ["'AE 2V2 CTF' PISTOL START", f"'{GAMETYPE}' BR START", "'AE 2V2 CTF' PISTOL START"]


def test_search(runs):
    assert re.search(r"\(filter ALL\): (\d+) gametypes, search 'ctf'", opened(runs["search"][1]))
    found = int(re.search(r": (\d+) gametypes", opened(runs["search"][1]))[1])
    assert 5 <= found < 60
    assert ": 0 gametypes, search 'qzx'" in opened(runs["search-empty"][1])
    cleared = lines(runs["search-clear"][1], "ae change gametype: search ")
    assert cleared and cleared[-1].startswith("'': ") and int(cleared[-1].split(": ")[1]) > found
    assert lines(runs["keyboard"][1], "ae change gametype: keyboard open")


def test_favourite_and_pick(runs):
    text = runs["top"][1]
    assert lines(text, "ae change gametype: favourite ")[0] == "'AE 2V2 CTF' on"
    assert "FAVOURITES" in " ".join(lines(text, "ae change gametype: focus folder "))
    pick = runs["pick"][1]
    assert lines(pick, "ae change gametype: pick ")[0] == f"'{GAMETYPE}' BR START"
    begins = lines(pick, "ae game options: begin ")
    assert begins[-1].startswith(f"'{GAMETYPE}' (file ") and lines(pick, "ae change gametype: closed")
    create = runs["create-pick"][1]
    assert lines(create, "ae change gametype: pick ")[0] == "'AE 2V2 CTF' PISTOL START (for the caller)"
    assert lines(create, "ae create game: gametype ")[-1] == "'AE 2V2 CTF'"


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "change-gametype")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae change gametype: ")[-4:])
