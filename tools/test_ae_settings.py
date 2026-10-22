"""The 0.3.7 menus' SETTINGS (debug.ae_test_screen 34: the top level; 35: a group) in real games, headless, through
the ae_test harness (tools/ae_test), in tools/test_ae_game_options.py's pattern (tools/test_ae_menus.py's helpers).

- the top level and CONTROLS / VIEW / HUD at 1080p 100 % and 130 %;
- a value stepped (BUTTON LAYOUT, LOOK SENSITIVITY): changed, in use at once, saved to the player's ae.toml;
- B with changes: KEEP / UNDO (KEEP keeps, UNDO puts the opened values back), B without changes just goes back;
- Y RESTORE DEFAULTS: CANCEL first, then RESTORE DEFAULTS changes the shown rows back;
- the picker (A), LB / RB sections;
- two players on this PC: X switches player, a change is the second player's own;
- every profile gets its ae.toml seeded on first use.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_settings.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_settings.py [case ...]   (the frames and a sheet alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_settings_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
CONTROLS, VIEW, HUD = 0, 2, 4
# name, screen (34 top, 35 a group), group, players, window, UI SCALE, the presses (after the lead-in: one a second)
CASES = (
    ("top", 34, 0, 1, "1920x1080", 100, ()),
    ("top-130", 34, 0, 1, "1920x1080", 130, ()),
    ("controls", 35, CONTROLS, 1, "1920x1080", 100, ()),
    ("controls-130", 35, CONTROLS, 1, "1920x1080", 130, ()),
    ("view", 35, VIEW, 1, "1920x1080", 100, ()),
    ("hud", 35, HUD, 1, "1920x1080", 100, ()),
    ("hud-130", 35, HUD, 1, "1920x1080", 130, ()),
    # (BUTTON LAYOUT one right: SWAP TRIGGERS; down twice to LOOK SENSITIVITY, one right: 4)
    ("changed", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:down", "pad:down", "pad:right")),
    # (a change, then B: KEEP / UNDO, shown)
    ("keep-dialog", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:b")),
    # (KEEP: back to nothing; UNDO: down once, A)
    ("keep", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:b", "pad:a")),
    ("undo", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:b", "pad:down", "pad:a")),
    # (no change: B goes back with no dialog)
    ("back", 35, VIEW, 1, "1920x1080", 100, ("pad:b",)),
    # (two changes, then Y: RESTORE DEFAULTS asked (CANCEL first): shown, then down, A restores)
    ("defaults-dialog", 35, HUD, 1, "1920x1080", 100, ("pad:right", "pad:down", "pad:right", "pad:y")),
    ("defaults", 35, HUD, 1, "1920x1080", 100, ("pad:right", "pad:down", "pad:right", "pad:y", "pad:down", "pad:a")),
    # (A: the values' picker; RB twice: the third section)
    ("picker", 35, VIEW, 1, "1920x1080", 100, ("pad:a",)),
    ("sections", 35, HUD, 1, "1920x1080", 100, ("pad:rb", "pad:rb")),
    # (two players: X switches to player 2, a change there is theirs: LEFT STICK DEADZONE, 5 %)
    ("seat", 35, CONTROLS, 2, "1920x1080", 100, ("pad:x",) + ("pad:down",) * 7 + ("pad:right",)),
    ("top-seat", 34, 0, 2, "1920x1080", 100, ("pad:x",)),
    # (a change for player 1, then X: KEEP / UNDO first; KEEP, then player 2)
    ("seat-keep", 35, CONTROLS, 2, "1920x1080", 100, ("pad:right", "pad:x", "pad:a")),
)


def play(cfg, out, name, screen, group, players, presses, window, scale):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_SETTINGS_GROUP": str(group),
           "HALO_AE_TEST_SETTINGS_PLAYERS": str(players)}
    return menus.play(cfg, out, name, views=screen, window=window, env=env, shots=8, exit_after=EXIT_AFTER,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, screen, group, players, window, scale, presses in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, screen, group, players, presses, window, scale)
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
    return run_cases(cfg, menus.out_dir(cfg, "settings"))


def test_screens_open(runs):
    for name, screen, group, *_ in CASES:
        _, text, frame = runs[name]
        assert frame, name
        if screen == 34:
            assert lines(text, "ae settings: open player "), name
        else:
            opened = lines(text, "ae settings: group ")
            assert opened and opened[0].startswith(("CONTROLS", "VIDEO", "VIEW", "AUDIO", "HUD")[group]), (name, opened)


def test_profiles_seeded(runs):
    """the test's profile gets its ae.toml on first use"""
    _, text, _ = runs["controls"]
    assert lines(text, "ae player settings: player 1 ") and any("seeded" in line for line in
                                                                lines(text, "ae player settings: player 1 "))


def test_a_step_is_in_use_and_saved(runs):
    _, text, _ = runs["changed"]
    said = lines(text, "ae settings: set ")
    assert "BUTTON LAYOUT SWAP TRIGGERS" in said and "LOOK SENSITIVITY 4" in said, said
    assert "ae profiles: controls 1 " in text
    assert lines(text, "ae settings: closed CONTROLS ")[-1:] != ["(0 changed)"]


def test_keep_and_undo(runs):
    assert lines(runs["keep"][1], "ae settings: keep") and not lines(runs["keep"][1], "ae settings: undo")
    undo = lines(runs["undo"][1], "ae settings: undo ")
    assert undo == ["(1 rows)"], undo
    # (no change: B goes back, no dialog)
    back = runs["back"][1]
    assert lines(back, "ae settings: closed VIEW ") == ["(0 changed)"] and "Keep your changes?" not in back


def test_restore_defaults(runs):
    said = lines(runs["defaults"][1], "ae settings: defaults ")
    assert said == ["(2 rows)"], said
    assert not lines(runs["defaults-dialog"][1], "ae settings: defaults ")


def test_sections_and_picker(runs):
    assert lines(runs["sections"][1], "ae settings: section ")[-1:] == ["SCOREBOARD"]
    assert lines(runs["picker"][1], "ae settings: focus ")[:1] == ["FOV"]


def test_the_second_players_own(runs):
    text = runs["seat"][1]
    assert "player 2" in lines(text, "ae settings: ")
    own = lines(text, "ae player settings: player 2 set ")
    assert own == ["input.left_stick_deadzone 5"], own
    assert not lines(text, "ae player settings: player 1 set ")
    assert "player 2" in lines(runs["top-seat"][1], "ae settings: ")
    # (X with a change asks first: KEEP, then the next player)
    said = lines(runs["seat-keep"][1], "ae settings: ")
    assert "keep" in said and "player 2" in said and said.index("keep") < said.index("player 2"), said


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "settings")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    frames = [done[n][2] for n, *_ in CASES if n in done and done[n][2]]
    if frames:
        print(menus.tile(frames, folder / "sheet.png", width=960, columns=2))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae settings: ")[-4:])
