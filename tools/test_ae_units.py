"""Builds and runs AE's C unit tests (port/linux/tests/ae_*_test.c) with the build machine's compiler.

The units under test have no SDL or game dependency (pattern of tools/test_touch_menu.py), so they compile
anywhere, with warnings as errors."""
import os
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "port" / "linux" / "tests"
# AE's game-side units sit flat in port/linux/game with the ae_ prefix
UI = ROOT / "port" / "linux" / "game"

SRC = ROOT / "port" / "linux" / "src"

# each test: its sources besides the test file, extra compile flags, and libraries
UNITS = {
    # (the catalog's readers take any text: under the sanitizers)
    "ae_catalog_test.c": ([UI / "ae_catalog.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_draw_layout_test.c": ([SRC / "ae_layout.c"], [], []),
    # (AE's faces with stb_truetype; under the sanitizers, as it is handed garbage and truncated fonts)
    "ae_font_test.c": ([SRC / "ae_font.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (ae_ui.c starts motions and asks for sounds: ae_motion.c and ae_sound.c with it, P5)
    "ae_input_rules_test.c": ([UI / "ae_input_rules.c", UI / "ae_ui.c", UI / "ae_motion.c", UI / "ae_sound.c",
                               SRC / "ae_back_presses.c"], [], []),
    "ae_list_test.c": ([UI / "ae_list.c"], [], []),
    # (the look's tokens, frame and densities, with the recording ae_draw stub that measures with the real fonts)
    "ae_style_test.c": ([UI / "ae_style.c", TESTS / "ae_draw_stub.c", SRC / "ae_font.c", SRC / "ae_layout.c"],
                        [f"-I{SRC}", f"-I{TESTS}"], []),
    # (the map reader with the system's zlib; under the sanitizers, as it reads corrupt files: the maps it writes
    # itself here, the stock maps in test_mapinfo_stock_maps)
    "ae_mapinfo_test.c": ([UI / "ae_mapinfo.c"],
                          ["-DAE_MAPINFO_SYSTEM_ZLIB", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                          ["-lz"]),
    "ae_motion_test.c": ([UI / "ae_motion.c"], [], []),
    # (the MCC mouse style's pure functions)
    "ae_mouse_test.c": ([UI / "ae_mouse.c"], [], []),
    "ae_sound_test.c": ([UI / "ae_sound.c"], [], []),
    # (editing a field's text: under the sanitizers, as it takes pasted text)
    "ae_text_edit_test.c": ([UI / "ae_text_edit.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_ui_test.c": ([UI / "ae_ui.c", UI / "ae_motion.c", UI / "ae_sound.c"], [], []),
    # (the texts, checked against Overpass 900)
    "ae_strings_test.c": ([UI / "ae_strings.c", SRC / "ae_font.c"], [f"-I{SRC}"], []),
    # (the widget core with the recording ae_draw stub and the real fonts; under the sanitizers)
    # (tabs, page dots, prompts, the in-view panel: the widget core with them, the stub, the real fonts)
    "ae_widgets_nav_test.c": ([UI / "ae_widgets_nav.c", UI / "ae_widgets.c", UI / "ae_style.c", UI / "ae_list.c",
                               UI / "ae_motion.c", UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c",
                               TESTS / "ae_draw_stub.c", SRC / "ae_font.c", SRC / "ae_layout.c"],
                              [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                              []),
    # (the value picker, chips, the help panel: the widget core and navigation with them)
    "ae_widgets_pick_test.c": ([UI / "ae_widgets_pick.c", UI / "ae_widgets_nav.c", UI / "ae_widgets.c",
                                UI / "ae_style.c", UI / "ae_list.c", UI / "ae_motion.c", UI / "ae_sound.c",
                                UI / "ae_strings.c", UI / "ae_ui.c", TESTS / "ae_draw_stub.c", SRC / "ae_font.c",
                                SRC / "ae_layout.c"],
                               [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               []),
    # (the text field, typing, AE's keyboard)
    "ae_widgets_text_test.c": ([UI / "ae_widgets_text.c", UI / "ae_text_edit.c", UI / "ae_widgets_nav.c",
                                UI / "ae_widgets.c", UI / "ae_style.c", UI / "ae_list.c", UI / "ae_motion.c",
                                UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c", TESTS / "ae_draw_stub.c",
                                SRC / "ae_font.c", SRC / "ae_layout.c"],
                               [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               []),
    # (the lobby's roster order and names, preflight P18)
    "ae_lobby_roster_test.c": ([UI / "ae_lobby_roster.c"], [f"-I{UI}", "-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (the lobby's host compatibility: the join's own test, halo_port_limits.h)
    "ae_lobby_compat_test.c": ([], [], []),
    # (the dialog, roster cards)
    "ae_widgets_dialog_test.c": ([UI / "ae_widgets_dialog.c", UI / "ae_widgets_text.c", UI / "ae_text_edit.c",
                                  UI / "ae_widgets_nav.c", UI / "ae_widgets.c", UI / "ae_style.c", UI / "ae_list.c",
                                  UI / "ae_motion.c", UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c",
                                  TESTS / "ae_draw_stub.c", SRC / "ae_font.c", SRC / "ae_layout.c"],
                                 [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                                 []),
    "ae_widgets_test.c": ([UI / "ae_widgets.c", UI / "ae_style.c", UI / "ae_list.c", UI / "ae_motion.c",
                           UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c", TESTS / "ae_draw_stub.c",
                           SRC / "ae_font.c", SRC / "ae_layout.c"],
                          [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
}
# a test's exit code for "nothing to check here" (the real maps missing): pytest.skip
SKIPPED = 77
# a build failing only for what this machine lacks (zlib's headers, the sanitizers' runtimes): pytest.skip
MISSING = ("zlib.h: No such file", "cannot find -lz", "cannot find -lasan", "cannot find -lubsan",
           "libasan", "libubsan", "libclang_rt.asan", "libclang_rt.ubsan")


def compiler():
    """Finds a C compiler on the build machine: the path of gcc or clang (the first found), or None."""
    for name in ("gcc", "clang"):
        path = shutil.which(name)
        if path:
            return path
    return None


def build_unit(test, tmp_path):
    """Builds one unit with its test, warnings as errors: the binary (skips without a compiler or what it needs)."""
    cc = compiler()
    if not cc:
        pytest.skip("no C compiler")
    sources, flags, libraries = UNITS[test]
    binary = tmp_path / test.replace(".c", "")
    build = subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", *flags,
                            f"-I{UI}", str(TESTS / test), *map(str, sources), *libraries, "-lm", "-o", str(binary)],
                           capture_output=True, text=True)
    if build.returncode != 0 and any(missing in build.stderr for missing in MISSING):
        pytest.skip("this machine lacks what the test builds with: " + build.stderr.strip().splitlines()[-1])
    assert build.returncode == 0, build.stdout + build.stderr
    return binary


def run_unit(binary, tmp_path, *args):
    run = subprocess.run([str(binary), *args], capture_output=True, text=True, cwd=ROOT,
                         env=dict(os.environ, AE_TEST_SCRATCH=str(tmp_path)))
    if run.returncode == SKIPPED:
        pytest.skip(run.stdout.strip().splitlines()[-1] if run.stdout.strip() else "nothing to check")
    assert run.returncode == 0, run.stdout + run.stderr


@pytest.mark.parametrize("test", sorted(UNITS))
def test_unit(test, tmp_path):
    """Builds one unit with its test, warnings as errors, and runs it."""
    run_unit(build_unit(test, tmp_path), tmp_path)


def test_mapinfo_stock_maps(tmp_path):
    """The map reader against the stock maps' known facts: skipped when $AE_MAPINFO_MAPS or $HALO_DATA_ROOT/maps
    doesn't have them (as on CI); the reader's own maps are test_unit's."""
    run_unit(build_unit("ae_mapinfo_test.c", tmp_path), tmp_path, "stock")
