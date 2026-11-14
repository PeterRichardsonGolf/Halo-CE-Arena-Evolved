"""(0.7.7) The CROSSHAIR screen (ae_screen_crosshair.c: HUD > RETICLE > EDIT CROSSHAIR..., layout C) in real games, headless,
through the ae_test harness (debug.ae_test_screen 35, group 9: HUD with the CROSSHAIR screen over it), in
tools/test_ae_settings.py's pattern:

- it opens on the main crosshair, its tabs switch (LB / RB) and a row steps;
- SHAPE: a shape focused and picked (A); COLOR: a colour picked, CUSTOM COLOR typed;
- the strip: NEW (a fresh crosshair in a free slot), DUPLICATE ("<NAME> 2"), DELETE (asks, CANCEL first), B then KEEP;
- Y: SHARE CODE from the panel, COPY; RENAME typed on the keyboard;
- the frames at 1080p, 130 %, 4:3, 21:9 and a split-screen player's (player 3).

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_crosshair_screen.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_crosshair_screen.py [case ...]   (the frames alone, kept)
"""
import importlib.util
import shutil
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_settings_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

CROSSHAIR = 10  # (0.9.0: SPLIT SCREEN after VIDEO)
# (a main crosshair and a second saved one, the BATTLE RIFLE using the second: the cases that only look)
GIVEN = {"HALO_CROSSHAIR": "1", "HALO_CROSSHAIR_1": "PRECISE|AE1-0426-QXFR-Z9J0-J1G9-CG7G-C6SW-CG36-80R2-A3NQ-C",
         "HALO_CROSSHAIR_2": "SNAP|AE1-04A2-TG78-Y1J2-J1G8-CG60-831W-CG36-8102-A2E3-P",
         "HALO_CROSSHAIR_WEAPONS": "battle_rifle=2;pistol=own"}
R = "pad:right"
# name, window, scale, presses, given crosshairs, players, the editing player
CASES = (
    ("xh-open", "1920x1080", 100, (), True, 1, 1),
    ("xh-lines", "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "pad:down", R), True, 1, 1),
    ("xh-shape", "1920x1080", 100, ("pad:down", "pad:down", R, R, "wait"), True, 1, 1),
    ("xh-pick-shape", "1920x1080", 100, ("pad:down", "pad:down", R, R, "pad:a", "wait"), False, 1, 1),
    ("xh-color", "1920x1080", 100, ("pad:rb", R, "pad:a", "wait"), False, 1, 1),
    ("xh-hex", "1920x1080", 100, ("pad:rb", "pad:down", "key:Return", "key:f", "key:f", "key:8", "key:0", "key:4",
                                  "key:0", "key:Return", "wait"), False, 1, 1),
    ("xh-top", "1920x1080", 100, ("pad:up", R), True, 1, 1),
    # (up to the strip: right to NEW, A (slot 4, the focus on its tile); right to DUPLICATE, A (slot 5, the focus on its tile);
    # right to DELETE, A, down to DELETE in the dialog, A; B asks KEEP / UNDO, A keeps)
    ("xh-slots", "1920x1080", 100, ("pad:up", R, R, R, "pad:a", R, R, R, "pad:a", R, R, R, R, R, "pad:a", "wait", "pad:down",
                                    "pad:a", "wait", "pad:b", "wait", "pad:a", "wait"), False, 1, 1),
    ("xh-share", "1920x1080", 100, ("pad:rb", "pad:y", "wait", "pad:a", "wait"), True, 1, 1),
    ("xh-rename", "1920x1080", 100, ("pad:up", R, R, R, R, "key:Return", "key:b", "key:o", "key:x", "key:Return", "wait"), False,
     1, 1),
    ("xh-130", "1920x1080", 130, ("pad:rb", "pad:rb"), True, 1, 1),
    ("xh-4x3", "1440x1080", 100, ("pad:rb", "pad:rb", "pad:down", "pad:down"), True, 1, 1),
    ("xh-4x3-shape", "1440x1080", 100, ("pad:down", "pad:down"), True, 1, 1),
    ("xh-21x9", "2560x1080", 100, ("pad:rb", "pad:rb", "pad:down", "pad:down"), True, 1, 1),
    ("xh-split", "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "pad:down"), False, 3, 3),
)


def play(cfg, out, name, window, scale, presses, given, players, editor):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_SETTINGS_GROUP": str(CROSSHAIR),
           "HALO_AE_TEST_SETTINGS_PLAYERS": str(players), "HALO_AE_TEST_CROSSHAIR_PLAYER": str(editor)}
    if given:
        env.update(GIVEN)
    exit_after = max(36, menus.LEAD_IN + len(presses) + 8)
    return menus.play(cfg, out, name, views=35, window=window, env=env, shots=8, exit_after=exit_after,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def said(runs, name):
    return lines(runs[name][1], "ae crosshair screen: ")


def run_cases(cfg, out, only=()):
    runs = {}
    for name, window, scale, presses, given, players, editor in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, window, scale, presses, given, players, editor)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        runs[name] = (result, text, frame)
    return runs


@pytest.fixture(scope="module")
def runs():
    cfg = menus.config()
    if not cfg:
        pytest.skip("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    out = menus.out_dir(cfg, "crosshair-screen")
    try:
        yield run_cases(cfg, out)
    finally:
        shutil.rmtree(out, ignore_errors=True)


def test_opens_on_the_main_crosshair(runs):
    for name, *_ in CASES:
        _, text, frame = runs[name]
        assert frame, name
        opened = lines(text, "ae crosshair screen: open player ")
        assert opened, name
    assert lines(runs["xh-open"][1], "ae crosshair screen: open player ") == ["1 slot 1"]
    assert lines(runs["xh-split"][1], "ae crosshair screen: open player ")[0].startswith("3 ")


def test_tabs_and_a_row(runs):
    told = said(runs, "xh-lines")
    assert "tab LINES" in told and "focus INNER THICKNESS" in told, told
    assert any(line.startswith("set xh.inner_thickness ") for line in told), told


def test_shape_focus_and_pick(runs):
    told = said(runs, "xh-shape")
    assert any(line.startswith("focus shape ") for line in told), told
    assert not [line for line in told if line.startswith("shape ")], told
    told = said(runs, "xh-pick-shape")
    picked = [line for line in told if line.startswith("shape ")]
    focused = [line for line in told if line.startswith("focus shape ")]
    assert picked and focused and picked[-1] == focused[-1].replace("focus ", ""), told


def test_color_pick_and_custom(runs):
    assert "color ICE" in said(runs, "xh-color"), said(runs, "xh-color")
    told = said(runs, "xh-hex")
    assert "focus CUSTOM COLOR" in told and "custom color ff8040" in told, told


def test_new_duplicate_delete(runs):
    told = said(runs, "xh-slots")
    assert "new slot 4" in told and "duplicate slot 5" in told and "deleted" in told and "keep" in told, told
    text = runs["xh-slots"][1]
    assert "ae crosshair: duplicate CROSSHAIR 5 in slot 5 (player 1)" in text
    assert "ae crosshair: slot 5 deleted (player 1)" in text
    assert lines(text, "ae crosshair screen: closed ") == ["(changed)"]


def test_share_code_and_copy(runs):
    told = said(runs, "xh-share")
    assert any(line.startswith("share code AE1-") for line in told), told
    assert lines(runs["xh-share"][1], "ae crosshair: code copied AE1-")


def test_rename_typed(runs):
    told = said(runs, "xh-rename")
    assert "focus RENAME" in told and "named 'box'" in told, told


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "crosshair-screen")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae crosshair screen: ")[-6:])
