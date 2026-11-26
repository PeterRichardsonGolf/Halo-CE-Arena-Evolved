"""TARGET FRAMERATE's one registry (port/linux/game/ae_target_framerate_rules.h): the classic menus' ARENA EVOLVED rows
(tools/port_settings_ae.py) read its texts, words and help, the written pages carry them, and the config words take the
same list."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import port_settings_ae  # noqa: E402

CE = ROOT / "port" / "assets" / "menus" / "ce"
PAGES = ("main_menu.settings_select.player_setup.player_profile_edit.ae_settings.video.video.xml",
         "main_menu.settings_select.player_setup.player_profile_edit.ae_settings.video.advanced.xml")


def test_values_are_the_registrys():
    assert port_settings_ae.TARGET_FRAMERATE == [("OFF", "off"), ("60", "60"), ("90", "90"), ("120", "120"),
                                                 ("144", "144"), ("DISPLAY", "display")]


def test_classic_help_fits_three_lines():
    for help_text in (port_settings_ae.TARGET_FRAMERATE_HELP, port_settings_ae.TARGET_FRAMERATE_SPLIT_HELP,
                      port_settings_ae.FRAME_RATE_LIMIT_HELP):
        lines = help_text.split("\n")
        assert len(lines) <= 3 and all(len(line) <= 50 for line in lines), help_text
    for help_text in (port_settings_ae.TARGET_FRAMERATE_HELP, port_settings_ae.TARGET_FRAMERATE_SPLIT_HELP):
        assert "resolution" in help_text and "never limits" in help_text and "FRAME RATE LIMIT rises" in help_text
    assert "TARGET FRAMERATE comes down" in port_settings_ae.FRAME_RATE_LIMIT_HELP


def test_classic_frame_rate_limit_takes_the_raises():
    """(TARGET FRAMERATE raises FRAME RATE LIMIT to 90 or 100 too: the desktop's spinner has them; the web's, which has
    no TARGET FRAMERATE, keeps its list and help)"""
    text = (CE / "main_menu.settings_select.player_setup.player_profile_edit.video_settings.xml").read_text()
    assert 'values="0|30|60|90|100|120|144|165|240|-1"' in text
    assert 'values="0|30|60|120|144|165|240|-1"' in text
    assert text.count("TARGET FRAMERATE comes down to fit under it") == 1


def test_rows_use_the_registry():
    keys = {row[1]: row for group in port_settings_ae.AE_GROUPS for page in group[3] for row in page[3]}
    assert keys["display.target_framerate"][2] is port_settings_ae.TARGET_FRAMERATE
    assert keys["display.split_target_framerate"][2] is port_settings_ae.TARGET_FRAMERATE
    assert "display.target_60_fps" not in keys and "display.split_target_60_fps" not in keys


def test_written_pages_carry_the_values():
    for page in PAGES:
        text = (CE / page).read_text()
        assert 'values="off|60|90|120|144|display"' in text, page
        assert "target_60_fps" not in text, page


def test_config_words_take_the_list():
    config = (ROOT / "port" / "linux" / "src" / "port_config.c").read_text()
    assert '{ "display.target_framerate", { AE_TARGET_FRAMERATE_WORDS, NULL }' in config
    assert '{ "display.split_target_framerate", { AE_TARGET_FRAMERATE_WORDS, NULL }' in config
