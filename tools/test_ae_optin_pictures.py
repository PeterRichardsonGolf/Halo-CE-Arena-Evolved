"""The COMMUNITY SERVICES screen (the community services' first-run opt-in, 0.3.5): its file is the generator's, its
pictures (tools/ae_optin_pictures.py) are listed where the game reads them, and its answers are the screen's only:
B, BACK and Esc (START) are NOT NOW, the buttons take A (never START), the row holds its value (no setting)."""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port" / "assets" / "menus"
CE = MENUS / "ce"
sys.path.insert(0, str(ROOT / "tools"))

import port_settings  # noqa: E402
import ae_optin_pictures  # noqa: E402

NAME = "main_menu.community_services.xml"


def _widget(xml: str, name: str) -> str:
    match = re.search(r'<widget name="%s"[^>]*>(.*?)</widget>' % re.escape(name), xml, re.S)
    assert match, name
    return match.group(0)


def test_the_file_is_the_generators():
    files = port_settings.community_services_files()
    assert list(files) == [NAME]
    assert "\n".join(files[NAME]) == (CE / NAME).read_text()


def test_listed_in_bitmaps_xml_and_menus_json():
    xml = (CE / "bitmaps.xml").read_text()
    for line in ae_optin_pictures.bitmaps_xml():
        assert line.strip() in xml, line
    assert f'<bitmap name="{port_settings.COMMUNITY_HEADER}">' in xml
    listed = set(json.loads((MENUS / "menus.json").read_text())["files"])
    assert f"ce/{NAME}" in listed
    for png in [f"ce/port/{bitmap}__{index}.png" for bitmap in (ae_optin_pictures.BAND, ae_optin_pictures.BUTTON)
                for index in (0, 1)] + [f"ce/port/{port_settings.COMMUNITY_HEADER}.png"]:
        assert png in listed, png
        assert (MENUS / png).is_file(), png


def test_pictures_are_the_stock_ones_cut_to_size():
    Image = pytest.importorskip("PIL.Image")
    for bitmap, (width, height) in ((ae_optin_pictures.BAND, ae_optin_pictures.BAND_SIZE),
                                    (ae_optin_pictures.BUTTON, ae_optin_pictures.BUTTON_SIZE)):
        for index in (0, 1):
            image = Image.open(MENUS / f"ce/port/{bitmap}__{index}.png")
            assert image.size == (width * 4, height * 4), (bitmap, index)
    # (the button's middle is the stock button's face: no new colour)
    stock = Image.open(CE / "shell/bitmaps/text_button_background__0.png").convert("RGBA")
    wide = Image.open(MENUS / f"ce/port/{ae_optin_pictures.BUTTON}__0.png").convert("RGBA")
    assert wide.getpixel((wide.width // 2, 64)) == stock.getpixel((stock.width // 2, 64))


def test_answers_are_the_screens():
    xml = (CE / NAME).read_text()
    screen = _widget(xml, port_settings.COMMUNITY_SCREEN)
    for event in ("b", "back", "start"):
        assert f'<on event="{event}" run="ae community not now"/>' in screen, event
    assert '<data input="ae community update"/>' in screen
    for key, run in (("use", "ae community use"), ("not_now", "ae community not now")):
        button = _widget(xml, f"{port_settings.COMMUNITY}/button_{key}")
        assert f'<on event="a" run="{run}"/>' in button
        assert 'event="start"' not in button and 'back="true"' not in button
    # (NOT NOW in the stock CANCEL slot; USE ending where OK's button ends)
    bar = _widget(xml, f"{port_settings.COMMUNITY}/button_bar")
    assert f'button_not_now" x="510"' in bar
    assert f'button_use" x="{380 + 128 - port_settings.COMMUNITY_USE_WIDTH}"' in bar
    spinner = _widget(xml, f"{port_settings.COMMUNITY}/list_hosted_games_spinner")
    assert 'strings="OFF|ON"' in spinner and "setting=" not in spinner
    assert '<on event="created" run="ae community focus"/>' in _widget(xml, f"{port_settings.COMMUNITY}/options_menu")


def test_functions_are_the_games():
    source = (ROOT / "port" / "linux" / "game" / "menu_tags.c").read_text()
    for name in ("ae community focus", "ae community row", "ae community use", "ae community not now",
                 "ae community update"):
        assert f'"{name}"' in source, name


def test_network_setup_has_the_master_row_first():
    rows = port_settings.SCREENS["network_setup"]["rows"]
    label, setting, choices, *_ = rows[0]
    assert (label, setting) == ("COMMUNITY SERVICES:", "network.community_services")
    # ("ask", not answered yet, shows OFF: setting_value_index's first value)
    assert choices[0] == ("OFF", "no") and ("ON", "yes") in choices
