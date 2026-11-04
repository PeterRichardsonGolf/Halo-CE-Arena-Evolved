"""The JOIN screen's pictures (tools/ae_seat_pictures.py): listed where the game reads them, round button chips that
keep a clear rim (never cut at the frame's edge), and no amber anywhere (the OG theme's standard, 2026-10-18)."""
import colorsys
import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port" / "assets" / "menus"
sys.path.insert(0, str(ROOT / "tools"))

Image = pytest.importorskip("PIL.Image")
import ae_seat_pictures  # noqa: E402


def _pngs():
    return sorted((MENUS / "ce" / "port" / "bitmaps").glob("ae_seat_*.png"))


def test_listed_in_bitmaps_xml_and_menus_json():
    xml = (MENUS / "ce" / "bitmaps.xml").read_text()
    listed = json.loads((MENUS / "menus.json").read_text())
    listed = set(listed if isinstance(listed, list) else [p for v in listed.values() for p in v])
    for line in ae_seat_pictures.bitmaps_xml():
        assert line.strip() in xml, line
    for png in _pngs():
        assert f"ce/port/bitmaps/{png.name}" in listed, png.name


def test_chips_keep_a_clear_rim():
    rim = 2
    for png in _pngs():
        if not (png.name.startswith("ae_seat_buttons__") or png.name.startswith("ae_seat_key_esc__")):
            continue
        alpha = Image.open(png).convert("RGBA").getchannel("A")
        w, h = alpha.size
        for x in range(w):
            for y in list(range(rim)) + list(range(h - rim, h)):
                assert alpha.getpixel((x, y)) == 0, (png.name, x, y)
        for y in range(h):
            for x in list(range(rim)) + list(range(w - rim, w)):
                assert alpha.getpixel((x, y)) == 0, (png.name, x, y)


def test_no_amber():
    for png in _pngs():
        image = Image.open(png).convert("RGBA")
        image = image.resize((max(1, image.width // 4), max(1, image.height // 4)))
        for r, g, b, a in image.getdata():
            if a < 128:
                continue
            hue, saturation, value = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            # (amber and yellow: hue 25-65 degrees, saturated and bright)
            assert not (25 / 360 <= hue <= 65 / 360 and saturation > 0.5 and value > 0.5), (png.name, (r, g, b))


def test_options_is_a_pill_not_start():
    """PlayStation's OPTIONS (frame 1) is an upright pill, so it is never read as the Xbox's round START (frame 0)"""
    start = Image.open(MENUS / "ce" / "port" / "bitmaps" / "ae_seat_buttons__0.png").convert("RGBA").getchannel("A")
    options = Image.open(MENUS / "ce" / "port" / "bitmaps" / "ae_seat_buttons__1.png").convert("RGBA").getchannel("A")
    x0, y0, x1, y1 = start.getbbox()
    assert abs((x1 - x0) - (y1 - y0)) <= 1
    x0, y0, x1, y1 = options.getbbox()
    assert (x1 - x0) < 0.65 * (y1 - y0)
