"""The game setup spike's CHANGE MAP screen (debug.ae_test_screen 31) in real games, headless, through the ae_test
harness (tools/ae_test), in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's helpers).

- the folders at 1080p 100 % and 130 %, the folder of the map used last open;
- the AE CTF filter (set at the start); LB / RB from filter to filter (LT / RT, the folders, checked live); left
  closing the open one;
- FAVOURITE (Y) making the FAVOURITES folder; the NHE disc's folder and the NHE filter;
- the facts read (the log's "facts N read" line) and no picture drawn where the data has none (the dashed place).

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_change_map.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_change_map.py      (the frames alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_change_map_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
SCREEN = 31
# name, window, UI SCALE, the presses (after the lead-in: one a second), more environment
CASES = (
    ("open", "1920x1080", 100, (), {}),
    ("open-130", "1920x1080", 130, (), {}),
    # (the filter set at the start: a hidden window has no focus, so AE takes no keys, and the test pad has no
    # triggers; LT / RT, the folders, are checked live)
    ("filter-ctf", "1920x1080", 100, ("pad:down",), {"HALO_AE_TEST_MAP_FILTER": "5"}),
    ("filters", "1920x1080", 100, ("pad:rb",) * 3, {}),
    ("closed", "1920x1080", 100, ("pad:left",), {}),
    ("favourite", "1920x1080", 100, ("pad:down", "pad:y", "pad:up", "pad:up"), {}),
    ("nhe", "1920x1080", 100, (), {"HALO_MOD": "NHE", "HALO_AE_TEST_MAP_FILTER": "6"}),
)


def play(cfg, out, name, window, scale, presses, env):
    e = {"HALO_ARENA_MENUS_SCALE": str(scale)}
    e.update(env)
    return menus.play(cfg, out, name, views=SCREEN, window=window, env=e, shots=8, exit_after=EXIT_AFTER,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, window, scale, presses, env in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, window, scale, presses, env)
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
    return run_cases(cfg, menus.out_dir(cfg, "change-map"))


def test_opens_with_folders_and_facts(runs):
    for name, *_ in CASES:
        _, text, frame = runs[name]
        assert lines(text, "ae change map: open "), name
        assert lines(text, "ae change map: maps "), name
        assert frame, name
    text = runs["open"][1]
    assert "HALO CE 13" in lines(text, "ae change map: maps ")[0] and "COMP 0" not in lines(text, "ae change map: maps ")[0]
    assert lines(text, "ae change map: facts "), "the facts are read within the run"


def test_the_filter_and_folders(runs):
    assert lines(runs["filter-ctf"][1], "ae change map: open ")[0] == "(filter AE CTF)"
    assert len(lines(runs["filters"][1], "ae change map: filter ")) >= 3
    assert lines(runs["closed"][1], "ae change map: folder ")[-1].endswith(" closed")
    assert lines(runs["favourite"][1], "ae change map: favourite ")[-1].endswith(" on")


def test_the_nhe_disc(runs):
    text = runs["nhe"][1]
    assert "NHE" in lines(text, "ae change map: maps ")[0]
    assert lines(text, "ae change map: open ")[0] == "(filter NHE)"


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "change-map")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae change map: ")[-4:])
