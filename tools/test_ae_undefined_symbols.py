"""Every symbol the game's objects reference is defined by an object or library of the link (tools/ae_undefined_symbols.py).
Needs a release build: set AE_MENUS_BUILD to its build dir (build/linux64)."""
import os
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent


def test_no_undefined_symbols():
    build = os.environ.get("AE_MENUS_BUILD")
    if not build or not Path(build).is_dir():
        pytest.skip("AE_MENUS_BUILD names no build dir (a built build/linux64)")
    run = subprocess.run([sys.executable, str(ROOT / "tools" / "ae_undefined_symbols.py"), build],
                         capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
