"""AE's zoom modes (port/linux/game/ae_zoom.c, ae_glue_zoom.c): the settings row, the config rows. The state machine
is ae_zoom_test.c's (tools/test_ae_units.py)."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import port_settings  # noqa: E402

CE = ROOT / "port" / "assets" / "menus" / "ce"
GAME_OPTIONS = "main_menu.settings_select.player_setup.player_profile_edit.mods_setup.xml"


def test_zoom_row_on_game_options():
    rows = {row[1]: row for row in port_settings.SCREENS["mods_setup"]["rows"]}
    row = rows["input.zoom_mode"]
    assert [v for _, v in row[2]] == ["toggle", "hold", "both"]
    assert len(row[3]) == 3 and all(len(text.split("\n")) <= 2 for text in row[3])
    text = (CE / GAME_OPTIONS).read_text()
    assert 'setting="input.zoom_mode"' in text and 'values="toggle|hold|both"' in text


def test_zoom_untouched_screens_are_byte_identical():
    generated = {**port_settings.settings_files(), **port_settings.multiplayer_files()}
    for name, lines in generated.items():
        assert "\n".join(lines) == (CE / name).read_text(), name


def test_config_rows():
    text = (ROOT / "port" / "linux" / "src" / "port_config.c").read_text()
    for key, kind, default, env in (("input.zoom_mode", "_config_string", r'"\\"toggle\\""', "HALO_ZOOM_MODE"),
                                    ("input.zoom_hold_time", "_config_real", '"0.25"', "HALO_ZOOM_HOLD_TIME")):
        assert re.search(rf'\{{ "{re.escape(key)}", {kind}, {default}, "{env}", _environment_value, /\* AE hook \*/',
                         text), key
