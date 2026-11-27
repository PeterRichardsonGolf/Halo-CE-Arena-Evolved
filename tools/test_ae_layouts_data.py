"""AE's shipped layouts: the committed port/linux/game/ae_layouts_data.c is what tools/ae_layouts_data.py writes from
port/assets/ae/layouts.txt (the layouts' own reader and its tests: port/linux/tests/ae_layout_rules_test.c)."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def test_layouts_data_is_current():
    """tools/ae_layouts_data.py --check: the generated file matches the layouts."""
    run = subprocess.run([sys.executable, str(ROOT / "tools" / "ae_layouts_data.py"), "--check"], capture_output=True,
                         text=True, cwd=ROOT)
    assert run.returncode == 0, run.stdout + run.stderr
