"""The game setup spike's GAME OPTIONS screen (debug.ae_test_screen 30) in real games, headless, through the ae_test
harness (tools/ae_test), in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's helpers).

- AE TEAM SLY at 1080p 100 % and 130 %, AE CTF and AE FFA BALL (their mode groups), 4:3;
- a value stepped (SHIELD TYPE): the changed row, the rail's dot, the crumb's count;
- the picker open (SENSOR RANGE) and the DEFAULTS dialog;
- the table agrees with the model (no "the model has" line) and the log's focus / group / set lines.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_game_options.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_game_options.py      (the frames and a sheet alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_options_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
SCREEN = 30
# name, gametype, window, UI SCALE, the presses (after the lead-in: one a second)
CASES = (
    ("slay-match", "AE TEAM SLY", "1920x1080", 100, ()),
    ("slay-match-130", "AE TEAM SLY", "1920x1080", 130, ()),
    # (PLAYERS, down to SHIELD TYPE, one step right: NONE, changed)
    ("slay-changed", "AE TEAM SLY", "1920x1080", 100, ("pad:rb", "pad:down", "pad:down", "pad:down", "pad:down",
                                                       "pad:right", "pad:down")),
    # (PLAYERS, down to SENSOR RANGE, A: its picker)
    ("slay-picker", "AE TEAM SLY", "1920x1080", 100, ("pad:rb",) + ("pad:down",) * 8 + ("pad:a",)),
    # (a change, then Y: the DEFAULTS dialog)
    ("slay-defaults", "AE TEAM SLY", "1920x1080", 100, ("pad:right", "pad:y")),
    # (four groups on: VEHICLES, then the mode's)
    ("ctf-mode", "AE CTF", "1920x1080", 100, ("pad:rb",) * 5),
    ("ball-mode", "AE FFA BALL", "1920x1080", 100, ("pad:rb",) * 5),
    ("slay-4x3", "AE TEAM SLY", "1440x1080", 100, ("pad:rb",)),
)


def play(cfg, out, name, gametype, presses, window, scale):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": gametype}
    return menus.play(cfg, out, name, views=SCREEN, window=window, env=env, shots=8, exit_after=EXIT_AFTER,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, gametype, window, scale, presses in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, gametype, presses, window, scale)
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
    return run_cases(cfg, menus.out_dir(cfg, "game-options"))


def test_opens_on_the_gametype(runs):
    for name, gametype, *_ in CASES:
        _, text, frame = runs[name]
        opened = lines(text, "ae game options: open ")
        assert opened and opened[0].startswith(f"'{gametype}'"), (name, opened)
        assert not lines(text, "the model has"), name
        assert frame, name


def test_a_step_changes_the_copy(runs):
    _, text, _ = runs["slay-changed"]
    assert "PLAYERS" in lines(text, "ae game options: group ")
    assert "SHIELD TYPE NONE" in lines(text, "ae game options: set ")
    assert lines(text, "ae game options: closed ")[-1:] != ["(0 changed)"]


def test_the_mode_groups(runs):
    assert "CAPTURE THE FLAG" in lines(runs["ctf-mode"][1], "ae game options: group ")
    assert "ODDBALL" in lines(runs["ball-mode"][1], "ae game options: group ")


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "game-options")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    frames = [done[n][2] for n, *_ in CASES if n in done and done[n][2]]
    if frames:
        print(menus.tile(frames, folder / "sheet.png", width=960, columns=2))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae game options: ")[-3:])
