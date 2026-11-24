"""The bots' built-in nav files (tools/bot_nav_assets.py): the generated C data is current with port/assets/bot_nav."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def test_bot_nav_data_current():
    result = subprocess.run([sys.executable, str(ROOT / "tools" / "bot_nav_assets.py"), "--check"], capture_output=True,
                            text=True)
    assert result.returncode == 0, result.stdout + result.stderr
