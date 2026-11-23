"""The per-player settings registry (port/linux/src/ae_config_players.c): it fits the profile file's cap, and every
crosshair / reticle row the menus write is in it (a row left out is shared by every split-screen player)."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def _names():
    text = (ROOT / "port/linux/src/ae_config_players.c").read_text()
    body = text.split("static const char *const names[] =", 1)[1].split("};", 1)[0]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return re.findall(r'"([a-z0-9_.]+)"', body)


def test_registry_fits_the_profile_cap():
    header = (ROOT / "port/linux/game/ae_player_settings.h").read_text()
    cap = int(re.search(r"AE_PLAYER_SETTINGS_MAXIMUM\s*=\s*(\d+)", header).group(1))
    names = _names()
    assert len(names) == len(set(names))
    assert len(names) <= cap, f"{len(names)} per-player settings, the profile file keeps {cap}"


def test_every_crosshair_row_is_per_player():
    names = set(_names())
    rows = (ROOT / "port/linux/game/ae_settings_rows.c").read_text()
    keys = set(re.findall(r'"(display\.(?:crosshair|reticle)[a-z0-9_]*)"', rows))
    assert keys, "no crosshair rows found"
    assert keys <= names, sorted(keys - names)
