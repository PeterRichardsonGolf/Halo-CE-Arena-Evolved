"""SETTINGS > CONTROLS > CUSTOM LAYOUT (ae_screen_keys.c) in real games, headless, through the ae_test harness
(tools/ae_test), with a virtual gamepad (debug.test_gamepads) pressed through SDL as a real one would be.

- A on a row waits; the press swaps, its log line says what moved, and BUTTON LAYOUT becomes CUSTOM;
- a press of a trigger binds; the button the action has already changes nothing;
- START cancels, as Esc and a click do; the d-pad is refused and the wait goes on; 8 s with no press and an unplugged
  pad change nothing;
- Y and the RESET row reset; B with changes asks KEEP / UNDO, and UNDO puts it back.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_custom_layout.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_custom_layout.py [case ...]   (the frames alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_custom_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
CONTROLS = 0
OPEN = ("pad1:down", "pad1:a", "wait")     # (CONTROLS: CUSTOM LAYOUT is the second row)
MELEE = OPEN + ("pad1:down",) * 3          # (JUMP, FIRE, THROW GRENADE, MELEE)
WAIT = MELEE + ("pad1:a",)
# name, the presses (after the lead-in: one a second)
CASES = (
    ("open", OPEN),
    ("waiting", WAIT + ("wait",)),
    ("swap", WAIT + ("wait", "pad1:rb", "wait")),
    ("trigger", WAIT + ("wait", "pad1:rt", "wait")),
    ("same", WAIT + ("wait", "pad1:b", "wait")),
    ("cancel-start", WAIT + ("wait", "pad1:start", "wait")),
    ("cancel-esc", WAIT + ("wait", "type:Escape", "wait")),
    ("cancel-click", WAIT + ("wait", "click:320:240", "wait")),
    ("dpad", WAIT + ("wait", "pad1:up", "wait", "pad1:rb", "wait")),
    ("timeout", WAIT + ("wait",) * 10),
    ("unplug", WAIT + ("wait", "pad1:unplug", "wait")),
    ("reset-dialog", WAIT + ("wait", "pad1:rb", "wait", "pad1:y", "wait")),
    ("reset-action", WAIT + ("wait", "pad1:rb", "wait", "pad1:y", "wait", "pad1:down", "pad1:down", "pad1:a", "wait")),
    ("reset-all", WAIT + ("wait", "pad1:rb", "wait", "pad1:y", "wait", "pad1:down", "pad1:a", "wait")),
    ("keep", WAIT + ("wait", "pad1:rb", "wait", "pad1:b", "wait", "pad1:a", "wait")),
    # (no pad: the page opens, and A waits for nothing)
    ("no-pad", ("down", "a", "wait", "down", "a", "wait")),
    ("undo", WAIT + ("wait", "pad1:rb", "wait", "pad1:b", "wait", "pad1:down", "pad1:a", "wait")),
)


def play(cfg, out, name, presses):
    env = {"HALO_ARENA_MENUS_SCALE": "100", "HALO_AE_TEST_SETTINGS_GROUP": str(CONTROLS),
           "HALO_AE_TEST_SETTINGS_PLAYERS": "1", "HALO_TEST_GAMEPADS": "0" if name == "no-pad" else "1"}
    exit_after = max(EXIT_AFTER, menus.LEAD_IN + len(presses) + 6)
    # (its last frame in the 8 s wait)
    if name == "waiting":
        exit_after = menus.LEAD_IN + len(presses) + 4
    return menus.play(cfg, out, name, views=35, env=env, shots=8, exit_after=exit_after,
                      test_input=menus.menu_input(*presses))


def lines(text):
    start = "ae custom layout: "
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    runs = {}
    for name, presses in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, presses)
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
    return run_cases(cfg, menus.out_dir(cfg, "custom-layout"))


def said(runs, name):
    return lines(runs[name][1])


def set_lines(runs, name):
    return [line for line in said(runs, name) if line.startswith("set ")]


def test_opens_and_waits(runs):
    assert any(line.startswith("open player 1") for line in said(runs, "open"))
    assert "press a button for melee" in [line.lower() for line in said(runs, "waiting")]


def test_a_press_swaps_and_makes_the_layout_custom(runs):
    lines_said = said(runs, "swap")
    assert any(line.startswith("set MELEE ") and "moved SWITCH GRENADE" in line for line in lines_said), lines_said
    assert "player 1 BUTTON LAYOUT now CUSTOM" in lines_said


def test_a_trigger_binds(runs):
    lines_said = said(runs, "trigger")
    assert any(line.startswith("set MELEE ") and "moved FIRE" in line for line in lines_said), lines_said


def test_the_same_button_changes_nothing(runs):
    assert not set_lines(runs, "same") and "same button" in said(runs, "same")


def test_cancels(runs):
    for name in ("cancel-start", "cancel-esc", "cancel-click"):
        assert not set_lines(runs, name) and "cancelled" in said(runs, name), name


def test_the_dpad_is_refused_and_the_wait_goes_on(runs):
    found = set_lines(runs, "dpad")
    assert len(found) == 1 and found[0].startswith("set MELEE "), found


def test_no_press_changes_nothing(runs):
    assert not set_lines(runs, "timeout") and "timed out" in said(runs, "timeout")


def test_unplugged_changes_nothing(runs):
    assert not set_lines(runs, "unplug") and "unplugged" in said(runs, "unplug")


def test_no_pad_waits_for_nothing(runs):
    lines_said = said(runs, "no-pad")
    assert any(line.startswith("open player 1") for line in lines_said), lines_said
    assert not any(line.startswith("press a button") for line in lines_said), lines_said


def test_reset(runs):
    assert "reset 3" in said(runs, "reset-action")
    assert "reset -1" in said(runs, "reset-all")


def test_keep_and_undo(runs):
    assert "keep" in said(runs, "keep")
    assert "undo" in said(runs, "undo")


if __name__ == "__main__":
    config = menus.config()
    if not config:
        sys.exit("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(config, "custom-layout")
    for case, (_, game_text, picture) in run_cases(config, folder, sys.argv[1:]).items():
        print(case, picture)
        for line in lines(game_text):
            print("   ", line)
    print(folder)
