"""SETTINGS > CONTROLS > KEY BINDINGS and BACK PADDLES (0.3.9; ae_screen_keys.c) in real games, headless, through the
ae_test harness (tools/ae_test), in tools/test_ae_settings.py's pattern (debug.ae_test_screen 35: CONTROLS).

- KEY BINDINGS opened from CONTROLS' KEYBOARD section (RB three times, A) at 100 % and 130 %; PRESS A KEY shown;
- a key typed (debug.test_input "type:G", a real key event) on JUMP: in config.toml at once, and THROW GRENADE, which
  had G, loses it (the line says so); the wheel's up: SWITCH WEAPON's WHEEL moves;
- Esc cancels, Backspace and X leave no key there;
- B with changes: KEEP / UNDO; Y RESTORE DEFAULTS for the section and for all;
- AUDIO's PUSH TO TALK (A) opens KEY BINDINGS on it;
- BACK PADDLES (CONTROLLER's last row): LEFT PADDLE stepped to A, the player's own.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_key_bindings.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_key_bindings.py [case ...]   (the frames and a sheet alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_keys_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
CONTROLS, AUDIO = 0, 3
OPEN = ("pad:rb", "pad:rb", "pad:rb", "pad:a")   # CONTROLS: RB to KEYBOARD (after GYRO, MOUSE), A on KEY BINDINGS
JUMP = OPEN + ("pad:down",) * 4        # (MOVE FORWARD first: JUMP is the fifth)
# name, group, UI SCALE, the presses (after the lead-in: one a second)
CASES = (
    ("keys", CONTROLS, 100, OPEN),
    ("keys-130", CONTROLS, 130, OPEN),
    ("keys-press", CONTROLS, 100, JUMP + ("pad:a",)),
    ("keys-jump", CONTROLS, 100, JUMP + ("pad:a", "type:G", "wait")),
    ("keys-jump-130", CONTROLS, 130, JUMP + ("pad:a", "type:G", "wait")),
    ("keys-wheel", CONTROLS, 100, JUMP + ("pad:a", "wheel:1", "wait")),
    ("keys-cancel", CONTROLS, 100, JUMP + ("pad:a", "type:Escape", "wait")),
    ("keys-backspace", CONTROLS, 100, JUMP + ("pad:a", "type:Backspace", "wait")),
    # (a pad's B while a row waits: the capture ends, the screen stays, the old key stays; A waits again, Esc ends it)
    ("keys-cancel-pad", CONTROLS, 100, JUMP + ("pad:a", "pad:b", "pad:a", "type:Escape", "wait")),
    ("keys-clear", CONTROLS, 100, JUMP + ("pad:x", "wait")),
    # (the mouse moved over the list, nothing clicked: JUMP's PRIMARY cap, its SECONDARY, STRAFE LEFT's and CROUCH's
    # labels, WEAPONS' header, MOVE FORWARD's SECONDARY. Each hover focuses the row under the pointer and nothing
    # scrolls; the header keeps the focus where it was. 0.3.9's first build read a cap's hit (item x 2 + slot) as a
    # list item: JUMP's cap focused MELEE, and the list scrolled under the pointer)
    ("keys-hover", CONTROLS, 100, OPEN + ("move:560:218", "move:645:218", "move:300:168", "move:300:243",
                                          "move:300:272", "move:645:119", "wait")),
    # (the mouse: a click on JUMP's PRIMARY cap (1500, 491 at 1080p: the menus' 640 x 480 in the middle) waits for a
    # key, Delete leaves none; a click on its SECONDARY, F5 typed)
    ("keys-mouse", CONTROLS, 100, OPEN + ("click:560:218", "type:Delete", "click:645:218", "type:F5", "wait")),
    # (G on JUMP, then B: KEEP / UNDO; UNDO (down, A))
    ("keys-undo", CONTROLS, 100, JUMP + ("pad:a", "type:G", "pad:b", "pad:down", "pad:a", "wait")),
    ("keys-keep-dialog", CONTROLS, 100, JUMP + ("pad:a", "type:G", "pad:b")),
    # (Y: CANCEL, RESTORE MOVEMENT, RESTORE ALL)
    ("keys-defaults-dialog", CONTROLS, 100, JUMP + ("pad:a", "type:G", "pad:y")),
    ("keys-defaults-section", CONTROLS, 100, JUMP + ("pad:a", "type:G", "pad:y", "pad:down", "pad:a", "wait")),
    ("keys-defaults-all", CONTROLS, 100, JUMP + ("pad:a", "type:G", "pad:y", "pad:down", "pad:down", "pad:a",
                                                 "wait")),
    ("keys-sections", CONTROLS, 100, OPEN + ("pad:rb", "pad:rb", "pad:rb")),
    # (AUDIO: VOICE is RB once away; PUSH TO TALK its last row with push to talk on (the default))
    ("keys-ptt", AUDIO, 100, ("pad:rb", "pad:down", "pad:down", "pad:down", "pad:a", "wait")),
    # (BACK PADDLES: CONTROLLER's twelfth row; right: LEFT PADDLE A; B: KEEP)
    ("paddles", CONTROLS, 100, ("pad:down",) * 11 + ("pad:a", "pad:right")),
    ("paddles-keep", CONTROLS, 100, ("pad:down",) * 11 + ("pad:a", "pad:right", "pad:b", "pad:a", "wait")),
)


def play(cfg, out, name, group, presses, scale):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_SETTINGS_GROUP": str(group),
           "HALO_AE_TEST_SETTINGS_PLAYERS": "1"}
    exit_after = max(EXIT_AFTER, menus.LEAD_IN + len(presses) + 6)
    return menus.play(cfg, out, name, views=35, env=env, shots=8, exit_after=exit_after,
                      test_input=menus.menu_input(*presses))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    runs = {}
    for name, group, scale, presses in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, group, presses, scale)
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
    return run_cases(cfg, menus.out_dir(cfg, "key-bindings"))


def test_opens(runs):
    for name in ("keys", "keys-130", "keys-press"):
        _, text, frame = runs[name]
        assert frame, name
        assert lines(text, "ae keys: open ") == ["(focus MOVE FORWARD)"], name
    assert "press a key for JUMP 1" in lines(runs["keys-press"][1], "ae keys: ")


def test_a_key_moves_and_is_written(runs):
    for name in ("keys-jump", "keys-jump-130"):
        said = lines(runs[name][1], "ae keys: ")
        assert "set JUMP 1 G (moved from THROW GRENADE)" in said, (name, said)
        assert 'controls.jump = "G"' in said and 'controls.throw_grenade = "Mouse Right"' in said, (name, said)
        assert "closed (2 changed)" in said or not any(line.startswith("closed") for line in said), (name, said)


def test_the_wheel(runs):
    said = lines(runs["keys-wheel"][1], "ae keys: ")
    assert "set JUMP 1 Wheel Up (moved from SWITCH WEAPON)" in said, said
    assert 'controls.switch_weapon = "1"' in said, said


def test_cancel_and_clear(runs):
    said = lines(runs["keys-cancel"][1], "ae keys: ")
    assert "cancelled" in said and not any(line.startswith("set ") for line in said), said
    said = lines(runs["keys-cancel-pad"][1], "ae keys: ")
    assert said.count("press a key for JUMP 1") == 2 and said.count("cancelled") == 2, said
    assert not any(line.startswith(("set ", "cleared", "keep asked", "closed")) for line in said), said
    for name in ("keys-backspace", "keys-clear"):
        said = lines(runs[name][1], "ae keys: ")
        assert "cleared JUMP 1" in said and 'controls.jump = ""' in said, (name, said)


def test_the_mouse(runs):
    said = lines(runs["keys-mouse"][1], "ae keys: ")
    assert "cleared JUMP 1" in said and "set JUMP 2 F5" in said and 'controls.jump = "F5"' in said, said


def test_hover_never_scrolls(runs):
    said = lines(runs["keys-hover"][1], "ae keys: focus ")
    assert said == ["MOVE FORWARD", "JUMP", "STRAFE LEFT", "CROUCH", "MOVE FORWARD"], said


def test_keep_and_undo(runs):
    assert "keep asked (2 changed)" in lines(runs["keys-keep-dialog"][1], "ae keys: ")
    said = lines(runs["keys-undo"][1], "ae keys: ")
    assert "undo (2 actions)" in said and 'controls.jump = "Space"' in said, said
    assert 'controls.throw_grenade = "Mouse Right, G"' in said, said


def test_restore_defaults(runs):
    assert "defaults asked (MOVEMENT 1, all 2)" in lines(runs["keys-defaults-dialog"][1], "ae keys: ")
    said = lines(runs["keys-defaults-section"][1], "ae keys: ")
    assert "defaults MOVEMENT (1 actions)" in said and 'controls.jump = "Space"' in said, said
    said = lines(runs["keys-defaults-all"][1], "ae keys: ")
    assert "defaults ALL (2 actions)" in said and 'controls.throw_grenade = "Mouse Right, G"' in said, said


def test_sections(runs):
    assert lines(runs["keys-sections"][1], "ae keys: section ") == ["WEAPONS", "ACTIONS", "CHAT"]


def test_push_to_talk_opens_key_bindings(runs):
    assert lines(runs["keys-ptt"][1], "ae keys: open ") == ["(focus PUSH TO TALK)"]


def test_back_paddles(runs):
    text = runs["paddles"][1]
    assert lines(text, "ae paddles: open player ") and "set LEFT PADDLE A" in lines(text, "ae paddles: ")
    said = lines(runs["paddles-keep"][1], "ae paddles: ")
    assert "keep" in said and "closed (1 changed)" in said, said
    assert "input.back_left a" in lines(runs["paddles-keep"][1], "ae player settings: player 1 set ")


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "key-bindings")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    frames = [done[n][2] for n, *_ in CASES if n in done and done[n][2]]
    if frames:
        print(menus.tile(frames, folder / "sheet.png", width=960, columns=2))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae keys: ")[-5:], lines(text, "ae paddles: ")[-3:])
