#!/usr/bin/env python3
"""Arena Evolved: the pictures of the COMMUNITY SERVICES screen (the first-run opt-in, 0.3.5:
port/linux/game/ae_community_screen.c, tools/port_settings.py's _community_services). All are the PC menus' own,
cut to the screen's sizes; nothing new is drawn:

- bitmaps/ae_optin_button_use: the stock button (bitmaps/text_button_background, both its frames: at rest and the
  focus's ice glow) widened to USE COMMUNITY SERVICES's 216 units (its words and 40 px of face either side at
  1080p), its rounded ends kept;
- bitmaps/ae_option_bkds_optin: the option row's band (bitmaps/option_bkds, both frames) at the screen's row width,
  328 units (its right end 40 px short of the card), its rounded ends kept, as the lobby's band is cut
  (tools/ae_menu_pictures.py's lobby_band);
- the title, COMMUNITY SERVICES, as tools/ce_menus.py sets every title of port_settings.TITLES (its Art.title: the
  same function, so a regeneration agrees).

    python3 tools/ae_optin_pictures.py --write

writes the PNGs (4 pixels a unit, as ce/port's others) under port/assets/menus/ce/port; tools/ce_menus.py writes
them too, and lists them in bitmaps.xml and menus.json (tools/test_ae_optin_pictures.py checks both).
"""
import argparse
import sys
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port" / "assets" / "menus"
sys.path.insert(0, str(ROOT / "tools"))
UNIT = 4

BUTTON = "bitmaps/ae_optin_button_use"
BUTTON_SIZE = (216, 32)
BAND = "bitmaps/ae_option_bkds_optin"
BAND_SIZE = (328, 32)
# (the stock button's rounded end, in its 4x pixels: ae_seat_pictures.small_buttons')
BUTTON_CORNER = 40
# (the band's ends kept whole: ae_menu_pictures.lobby_band's)
BAND_END = 96


def _png(bitmap: str, frame: int) -> str:
    return f"ce/port/{bitmap}__{frame}.png"


def _button_frames(menus: Path) -> list:
    from PIL import Image
    import ae_seat_pictures
    frames = []
    for frame in (0, 1):
        source = Image.open(menus / "ce" / "shell" / "bitmaps" / f"text_button_background__{frame}.png").convert("RGBA")
        frames.append(ae_seat_pictures._nine_slice(source, (BUTTON_SIZE[0] * UNIT, BUTTON_SIZE[1] * UNIT),
                                                   BUTTON_CORNER))
    return frames


def _band_frames(menus: Path) -> list:
    from PIL import Image
    frames = []
    for frame in (0, 1):
        source = Image.open(menus / "ce" / "shell" / "bitmaps" / f"option_bkds__{frame}.png").convert("RGBA")
        width, height, end = BAND_SIZE[0] * UNIT, source.height, BAND_END
        band = Image.new("RGBA", (width, height))
        band.paste(source.crop((0, 0, end, height)), (0, 0))
        band.paste(source.crop((end, 0, end + width - 2 * end, height)), (end, 0))
        band.paste(source.crop((source.width - end, 0, source.width, height)), (width - end, 0))
        frames.append(band)
    return frames


def title_name() -> str:
    import port_settings
    return port_settings.COMMUNITY_HEADER


def write(menus: Path = MENUS) -> list:
    """the PNGs; their paths, relative to the menus' folder"""
    written = []
    for bitmap, frames in ((BUTTON, _button_frames(menus)), (BAND, _band_frames(menus))):
        for index, image in enumerate(frames):
            png = _png(bitmap, index)
            (menus / png).parent.mkdir(parents=True, exist_ok=True)
            image.save(menus / png, optimize=True)
            written.append(png)
    return written


def write_title(menus: Path = MENUS) -> list:
    """the title's picture, drawn by ce_menus.Art.title itself (its SVG backdrop goes to port_svg/ as it does)"""
    import ce_menus
    import port_settings
    art = SimpleNamespace(pngs=[])
    ce_menus.Art.title(art, port_settings.COMMUNITY_HEADER, port_settings.TITLES[port_settings.COMMUNITY_HEADER])
    return art.pngs


def bitmaps_xml() -> list:
    """the two <bitmap>s for bitmaps.xml (tools/ce_menus.py writes them; the title's is TITLES')"""
    lines = []
    for bitmap, (width, height) in ((BAND, BAND_SIZE), (BUTTON, BUTTON_SIZE)):
        lines.append(f'\t<bitmap name="{bitmap}">')
        lines += [f'\t\t<frame png="{_png(bitmap, index)}" width="{width}" height="{height}"/>' for index in range(2)]
        lines.append("\t</bitmap>")
    return lines


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--write", action="store_true", help="write the PNGs into port/assets/menus")
    arguments = parser.parse_args()
    if arguments.write:
        for path in write() + write_title():
            print(path)
