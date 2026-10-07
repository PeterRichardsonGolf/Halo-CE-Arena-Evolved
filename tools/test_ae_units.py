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
    "ae_draw_layout_test.c": ([SRC / "ae_layout.c"], [], []),
    "ae_input_rules_test.c": ([UI / "ae_input_rules.c", UI / "ae_ui.c", SRC / "ae_back_presses.c"], [], []),
    "ae_list_test.c": ([UI / "ae_list.c"], [], []),
    # (the map reader with the system's zlib; under the sanitizers, as it reads corrupt files; it checks the stock
    # maps when $AE_MAPINFO_MAPS or $HALO_DATA_ROOT/maps has them, and exits 77 when it found none: a skip)
    "ae_mapinfo_test.c": ([UI / "ae_mapinfo.c"],
                          ["-DAE_MAPINFO_SYSTEM_ZLIB", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                          ["-lz"]),
    "ae_ui_test.c": ([UI / "ae_ui.c"], [], []),
}
# a test's exit code for "nothing to check here" (the real maps missing): pytest.skip
SKIPPED = 77


def compiler():
    """Finds a C compiler on the build machine: the path of gcc or clang (the first found), or None."""
    for name in ("gcc", "clang"):
        path = shutil.which(name)
        if path:
            return path
    return None


@pytest.mark.parametrize("test", sorted(UNITS))
def test_unit(test, tmp_path):
    """Builds one unit with its test, warnings as errors, and runs it."""
    cc = compiler()
    if not cc:
        pytest.skip("no C compiler")
    sources, flags, libraries = UNITS[test]
    binary = tmp_path / test.replace(".c", "")
    build = subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", *flags,
                            f"-I{UI}", str(TESTS / test), *map(str, sources), *libraries, "-lm", "-o", str(binary)],
                           capture_output=True, text=True)
    assert build.returncode == 0, build.stdout + build.stderr
    run = subprocess.run([str(binary)], capture_output=True, text=True, cwd=ROOT,
                         env=dict(os.environ, AE_TEST_SCRATCH=str(tmp_path)))
    if run.returncode == SKIPPED:
        pytest.skip(run.stdout.strip().splitlines()[-1] if run.stdout.strip() else "nothing to check")
    assert run.returncode == 0, run.stdout + run.stderr
