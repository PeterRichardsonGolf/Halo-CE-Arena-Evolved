"""AE's MATCHMAKING lobby (menus phase 1, R2-04 / R2-05) in real games, headless, through the ae_test harness, in
tools/test_ae_main_menu.py's pattern: MULTIPLAYER's second row opens it; LB / RB change the playlist and it is kept;
every CASUAL server full (HALO_AE_TEST_MM_SEEN stands in for the listings) ends the search on its line; B cancels a
search.

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_matchmaking.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_matchmaking.py [case ...]
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_mm_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

OPEN = ("pad:a", "pad:down", "pad:a", "wait")
FULL = "play.arenaevolved.com:10660=16/16"
# name, the presses, the case's own environment
CASES = (
    ("mm-open", OPEN, {}),
    ("mm-playlist", (*OPEN, "pad:rb", "pad:rb", "wait", "pad:b", "wait", "pad:a"), {}),
    ("mm-all-full", (*OPEN, "pad:start", "wait", "wait"), {"HALO_AE_TEST_MM_SEEN": FULL}),
    # (owner 11-11: the playlists are rows; down to BTB, A joins it)
    ("mm-rows", (*OPEN, "pad:down", "pad:a", "wait"), {"HALO_AE_TEST_MM_SEEN": FULL}),
    # (a server that is heard of but not answering: tried, the join refused or not answered, B cancels it)
    ("mm-cancel", (*OPEN, "pad:start", "pad:b", "wait"), {"HALO_AE_TEST_MM_SEEN": "play.arenaevolved.com:10660=2/16"}),
)


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    runs = {}
    for name, presses, env in CASES:
        if only and name not in only:
            continue
        env = {"HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", "HALO_NET_ONLINE": "false", **env}
        result, text, pngs = menus.play(cfg, out, name, views=0, window="1920x1080", env=env, shots=8, exit_after=30,
                                        test_input=menus.menu_input(*presses))
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
    return run_cases(cfg, menus.out_dir(cfg, "matchmaking"))


def test_matchmaking_opens_from_multiplayer(runs):
    said = lines(runs["mm-open"][1], "ae matchmaking: ")
    assert "open (5 playlists, CASUAL)" in said, said


def test_matchmaking_playlist_lb_rb(runs):
    """RB twice: BTB, then COMP; reopened, COMP again (menus.last_playlist)"""
    said = lines(runs["mm-playlist"][1], "ae matchmaking: ")
    assert said[:3] == ["open (5 playlists, CASUAL)", "playlist BTB", "playlist COMP"], said
    assert "open (5 playlists, COMP)" in said, said


def test_matchmaking_all_full(runs):
    said = lines(runs["mm-all-full"][1], "ae matchmaking: ")
    assert "searching CASUAL (a party of 1)" in said, said
    assert "none with room (CASUAL)" in said, said
    assert not any(line.startswith("joining ") for line in said), said


def test_matchmaking_cancel(runs):
    said = lines(runs["mm-cancel"][1], "ae matchmaking: ")
    assert "searching CASUAL (a party of 1)" in said, said
    assert "cancelled" in said or "none with room (CASUAL)" in said or "turned down (CASUAL)" in " ".join(said), said


def test_matchmaking_rows(runs):
    said = lines(runs["mm-rows"][1], "ae matchmaking: ")
    assert "playlist BTB" in said and "searching BTB (a party of 1)" in said, said


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    done = run_cases(c, menus.out_dir(c, "matchmaking"), tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae matchmaking: ")[-6:])
