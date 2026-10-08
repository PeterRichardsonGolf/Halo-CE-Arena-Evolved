"""AE's fonts: the committed port/linux/src/ae_font_data.c is what tools/ae_font_data.py writes from
port/assets/fonts/OpenCE-Regular.ttf (the faces' own tests are port/linux/tests/ae_font_test.c, tools/test_ae_units.py)."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def test_font_data_is_current():
    """tools/ae_font_data.py --check: the generated file matches the font."""
    run = subprocess.run([sys.executable, str(ROOT / "tools" / "ae_font_data.py"), "--check"], capture_output=True,
                         text=True, cwd=ROOT)
    assert run.returncode == 0, run.stdout + run.stderr
