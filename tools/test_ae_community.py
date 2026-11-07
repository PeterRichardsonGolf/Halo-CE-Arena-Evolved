"""AE's COMMUNITY SERVICES page (debug.ae_test_screen 37; the page the first start asks, ae_screen_community.c) in
real games, headless, through the ae_test harness, in tools/test_ae_game_options.py's pattern:

- the first look (DECLINE focused) at 1080p 100 % and 130 %, 4:3; ACCEPT focused; the row turned ON;
- DECLINE, ACCEPT, B and START answer once each, the answer written (the log's "community services:" line) and the
  page closed; the row's value goes with ACCEPT only; a press in the first half second takes nothing;
- the page keeps its words on the screen (no cut text) and has no focus of its own in the right column.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_community.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_community.py      (the frames alone)
"""
import importlib.util
import shutil
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_community_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 24
SCREEN = 37
# name, window, UI SCALE, the presses (after the lead-in: one a second)
CASES = (
    ("first", "1920x1080", 100, ("wait",)),
    ("first-130", "1920x1080", 130, ("wait",)),
    ("first-4x3", "1440x1080", 100, ("wait",)),
    ("accept-focused", "1920x1080", 100, ("pad:right",)),
    # (up to the row, A: ON)
    ("list-on", "1920x1080", 100, ("pad:up", "pad:a")),
    ("list-on-accept", "1920x1080", 100, ("pad:up", "pad:a", "pad:down", "pad:right")),
    ("decline", "1920x1080", 100, ("pad:a", "wait")),
    ("accept", "1920x1080", 100, ("pad:right", "pad:a", "wait")),
    # (the row ON, then DECLINE: nothing listed)
    ("decline-list-on", "1920x1080", 100, ("pad:up", "pad:a", "pad:down", "pad:a", "wait")),
    ("accept-list-on", "1920x1080", 100, ("pad:up", "pad:a", "pad:down", "pad:right", "pad:a", "wait")),
    ("back", "1920x1080", 100, ("pad:b", "wait")),
    ("start", "1920x1080", 100, ("pad:start", "wait")),
)


def play(cfg, out, name, window, scale, presses):
    return menus.play(cfg, out, name, views=SCREEN, window=window, env={"HALO_ARENA_MENUS_SCALE": str(scale)}, shots=8,
                      exit_after=EXIT_AFTER, test_input=menus.menu_input(*presses))


def run_cases(cfg, out, only=()):
    runs = {}
    for name, window, scale, presses in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, window, scale, presses)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        shutil.rmtree(out / name / "shots", ignore_errors=True)
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
    return run_cases(cfg, menus.out_dir(cfg, "community"))


def answers(text):
    return [line.split("community services: ", 1)[1] for line in text.splitlines()
            if "community services: USE COMMUNITY" in line or "community services: NOT NOW" in line]


@pytest.mark.parametrize("name", ["first", "first-130", "first-4x3", "accept-focused", "list-on", "list-on-accept"])
def test_the_page_opens_and_asks_nothing_by_itself(runs, name):
    result, text, frame = runs[name]
    assert "ae community: open" in text
    assert frame is not None
    assert not answers(text)


@pytest.mark.parametrize("name, line", [
    ("decline", "NOT NOW (list hosted games off"),
    ("accept", "USE COMMUNITY SERVICES (list hosted games off"),
    ("decline-list-on", "NOT NOW (list hosted games off"),
    ("accept-list-on", "USE COMMUNITY SERVICES (list hosted games on"),
    ("back", "NOT NOW (list hosted games off"),
    ("start", "NOT NOW (list hosted games off"),
])
def test_answers_are_written_once_and_the_page_closes(runs, name, line):
    result, text, frame = runs[name]
    found = answers(text)
    assert len(found) == 1, found
    assert found[0].startswith(line), found
    assert "ae community: closed" in text


def test_the_log_names_the_answer(runs):
    assert "ae community: answered decline (list games I host: on)" in runs["decline-list-on"][1]
    assert "ae community: answered accept (list games I host: on)" in runs["accept-list-on"][1]


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs $AE_MENUS_BUILD and the harness config")
    only = tuple(sys.argv[1:])
    folder = menus.out_dir(c, "community")
    for name, (result, text, frame) in run_cases(c, folder, only).items():
        print(name, frame, answers(text))
