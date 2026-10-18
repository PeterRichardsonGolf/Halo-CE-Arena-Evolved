#!/usr/bin/env python3
"""Arena Evolved: the pictures of split screen's JOIN screen (main_menu/multiplayer_type_select/split_screen/join_*,
tools/port_settings.py's _join_screen; spec notes/specs/2026-10-16-split-screen-pc.md 4.2 and 6). In the PC menus'
own look: the cards are the game's panel (bitmaps/spinner_list_3_wide_item_background, its outlined frame) cut to a
card's size, the profile bar the option row's band; the devices and buttons are Kenney's Input Prompts glyphs (CC0,
port/linux/ui/fonts) in the menus' blue. Nothing is traced.

    python3 tools/ae_seat_pictures.py --write

writes the PNGs (4 pixels a unit, as ce/port's others) under port/assets/menus/ce/port/bitmaps; bitmaps.xml and
menus.json list them (tools/test_ae_seat_pictures.py checks both).

The frames, as ae_split_screen.c sets them:
- bitmaps/ae_seat_card: 0 an empty seat (dim edge, a plus in a dashed ring), 1 a seated player (the menus' blue
  edge), 2 an unplugged pad's held seat (amber edge);
- bitmaps/ae_seat_devices: 0 keyboard + mouse, 1 Xbox pad, 2 PlayStation pad, 3 Nintendo pad, 4 another pad,
  5 unplugged (amber, struck through);
- bitmaps/ae_seat_buttons: 0 Xbox START, 1 PlayStation OPTIONS, 2 ENTER, 3 Xbox B, 4 PlayStation circle, 5 ESC,
  6 Nintendo B, 7 Nintendo +;
- bitmaps/ae_seat_profile_bar: the profile's bar (the option row's band, outlined);
- bitmaps/ae_seat_strip: the line across the top (the option row's band, outlined).
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port" / "assets" / "menus"
GLYPHS = ROOT / "port" / "linux" / "ui" / "fonts"
UNIT = 4

BLUE = (41, 149, 255)
DIM = (27, 90, 150)
AMBER = (255, 186, 64)

# (design units: tools/port_settings.py's _join_screen places them)
CARD = (146, 300)
DEVICE = (72, 40)
BUTTON = (16, 16)
PROFILE_BAR = (130, 28)
STRIP = (608, 28)

FONTS = {"key": "input_keyboard.ttf", "xbox": "input_xbox.ttf", "ps": "input_playstation.ttf",
         "nintendo": "input_nintendo.ttf"}
# Kenney's glyphs (font, code point or None: by its name, name); the Xbox's START is its menu button
DEVICE_GLYPHS = [
    [("key", None, "keyboard"), ("key", None, "mouse")],
    [("xbox", None, "controller_xboxseries")],
    [("ps", None, "controller_playstation5")],
    [("nintendo", None, "controller_wiiu_pro")],
    [("xbox", None, "controller_xbox360")],
    [("xbox", None, "controller_xboxseries")],
]
BUTTON_GLYPHS = [("xbox", None, "xbox_button_menu"), ("ps", None, "playstation5_button_options"),
                 ("key", None, "keyboard_enter"), ("xbox", None, "xbox_button_b"),
                 ("ps", None, "playstation_button_circle"), ("key", None, "keyboard_escape"),
                 ("nintendo", None, "wiiu_button_b"), ("nintendo", None, "wiiu_button_plus")]


def _code(font: str, code, name: str) -> int:
    """a glyph's code point: given, or found by its name in the font's own table"""
    from fontTools.ttLib import TTFont
    table = TTFont(str(GLYPHS / FONTS[font])).getBestCmap()
    if code is not None:
        if table.get(code) != name:
            raise SystemExit(f"{FONTS[font]}: U+{code:04X} is {table.get(code)}, not {name}")
        return code
    for point, glyph in table.items():
        if glyph == name:
            return point
    raise SystemExit(f"{FONTS[font]}: no {name}")


def _glyph(image, font: str, code, name: str, box, colour, alpha=255):
    """the glyph in the box (pixels), as large as fits, centred: drawn large, cut to its ink (Kenney's glyphs keep a
    wide margin round it), then scaled down into the box"""
    from PIL import Image, ImageDraw, ImageFont
    x0, y0, x1, y1 = box
    character = chr(_code(font, code, name))
    face = ImageFont.truetype(str(GLYPHS / FONTS[font]), 512)
    big = Image.new("RGBA", (768, 768), (0, 0, 0, 0))
    ImageDraw.Draw(big).text((128, 128), character, font=face, fill=colour + (alpha,))
    ink = big.crop(big.getchannel("A").getbbox())
    scale = min((x1 - x0) / ink.width, (y1 - y0) / ink.height)
    ink = ink.resize((max(1, round(ink.width * scale)), max(1, round(ink.height * scale))), Image.LANCZOS)
    image.alpha_composite(ink, (int(x0 + (x1 - x0 - ink.width) / 2), int(y0 + (y1 - y0 - ink.height) / 2)))


def _nine_slice(source, size, corner: int):
    """the source stretched to size with its corners (corner pixels) kept"""
    from PIL import Image
    width, height = size
    out = Image.new("RGBA", size)
    sw, sh = source.size
    c = corner
    pieces = [((0, 0, c, c), (0, 0, c, c)), ((sw - c, 0, sw, c), (width - c, 0, width, c)),
              ((0, sh - c, c, sh), (0, height - c, c, height)), ((sw - c, sh - c, sw, sh), (width - c, height - c, width, height)),
              ((c, 0, sw - c, c), (c, 0, width - c, c)), ((c, sh - c, sw - c, sh), (c, height - c, width - c, height)),
              ((0, c, c, sh - c), (0, c, c, height - c)), ((sw - c, c, sw, sh - c), (width - c, c, width, height - c)),
              ((c, c, sw - c, sh - c), (c, c, width - c, height - c))]
    for (a, b, c2, d), (e, f, g, h) in pieces:
        out.paste(source.crop((a, b, c2, d)).resize((g - e, h - f)), (e, f))
    return out


# (the stock panel as the game draws it, measured on screen: a 2-unit edge over this navy; the source's own fill is
# lighter and its edge 4 units, the game drawing it smaller)
PANEL_FILL = (0, 14, 35)


def _recolour_edge(card, colour, fill=PANEL_FILL):
    """the panel in an edge colour over the stock fill: each pixel's share of edge (how far it is from the source's
    own fill toward its blue) kept"""
    import numpy as np
    from PIL import Image
    a = np.array(card).astype(float)
    navy = a[a.shape[0] // 2, a.shape[1] // 2, :3].copy()
    blue = np.array(BLUE, float)
    t = np.clip(((a[..., :3] - navy) * (blue - navy)).sum(2) / ((blue - navy) ** 2).sum(), 0, 1)[..., None]
    t[t < 0.04] = 0
    a[..., :3] = np.array(fill, float) + t * (np.array(colour, float) - np.array(fill, float))
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), "RGBA")


def _panel(menus: Path):
    """the game's outlined panel at half its size: its edge then 2 units, as the game draws it"""
    from PIL import Image
    source = Image.open(menus / "ce" / "shell" / "bitmaps" / "spinner_list_3_wide_item_background__1.png").convert("RGBA")
    source = source.crop(source.getbbox())
    return source.resize((source.width // 2, source.height // 2), Image.LANCZOS)


def cards(menus: Path) -> list:
    from PIL import Image, ImageDraw
    size = (CARD[0] * UNIT, CARD[1] * UNIT)
    panel = _nine_slice(_panel(menus), size, 9 * UNIT // 4 * 4)
    seated = _recolour_edge(panel, BLUE)
    empty = _recolour_edge(panel, DIM)
    # (the empty seat's plus in a dashed ring, under PLAYER n: tools/port_settings.py's _join_screen leaves its room)
    draw = ImageDraw.Draw(empty)
    cx, cy, r, w = size[0] / 2, 86 * UNIT, 30 * UNIT, 2 * UNIT
    for k in range(24):
        draw.arc((cx - r, cy - r, cx + r, cy + r), k * 15, k * 15 + 9, fill=DIM + (255,), width=w)
    draw.line((cx - 12 * UNIT, cy, cx + 12 * UNIT, cy), fill=DIM + (255,), width=3 * UNIT)
    draw.line((cx, cy - 12 * UNIT, cx, cy + 12 * UNIT), fill=DIM + (255,), width=3 * UNIT)
    return [empty, seated, _recolour_edge(panel, AMBER)]


def devices() -> list:
    from PIL import Image, ImageDraw
    size = (DEVICE[0] * UNIT, DEVICE[1] * UNIT)
    frames = []
    for index, parts in enumerate(DEVICE_GLYPHS):
        image = Image.new("RGBA", size, (0, 0, 0, 0))
        colour = AMBER if index == 5 else BLUE
        # (a pad about as tall as the PLAYER title's capitals are wide apart: the mockup's proportion)
        top, bottom = size[1] * 0.12, size[1] * 0.88
        if len(parts) == 2:
            # (keyboard and mouse side by side, the mouse a little smaller)
            _glyph(image, *parts[0], (size[0] * 0.08, top, size[0] * 0.6, bottom), colour)
            _glyph(image, *parts[1], (size[0] * 0.66, top + size[1] * 0.08, size[0] * 0.92, bottom), colour)
        else:
            _glyph(image, *parts[0], (0, top, size[0], bottom), colour)
        if index == 5:
            ImageDraw.Draw(image).line((size[0] * 0.18, size[1] * 0.9, size[0] * 0.82, size[1] * 0.1),
                                       fill=AMBER + (255,), width=3 * UNIT)
        frames.append(image)
    return frames


def buttons() -> list:
    from PIL import Image
    size = (BUTTON[0] * UNIT, BUTTON[1] * UNIT)
    frames = []
    for glyph in BUTTON_GLYPHS:
        image = Image.new("RGBA", size, (0, 0, 0, 0))
        _glyph(image, *glyph, (0, 0, size[0], size[1]), BLUE)
        frames.append(image)
    return frames


def _band(menus: Path, frame: int, width: int):
    """the option row's band (bitmaps/option_bkds) at width units, its rounded ends kept (ae_menu_pictures.py's
    lobby_band)"""
    from PIL import Image
    source = Image.open(menus / "ce" / "shell" / "bitmaps" / f"option_bkds__{frame}.png").convert("RGBA")
    pixels, end = width * UNIT, 96
    band = Image.new("RGBA", (pixels, source.height))
    band.paste(source.crop((0, 0, end, source.height)), (0, 0))
    # (the middle cut from the source's, stretched when the band is longer than the source)
    middle = source.crop((end, 0, min(source.width - end, end + pixels - 2 * end), source.height))
    band.paste(middle.resize((pixels - 2 * end, source.height)), (end, 0))
    band.paste(source.crop((source.width - end, 0, source.width, source.height)), (pixels - end, 0))
    # (cut to its ink: the widget then shows all of it, its rounded bottom too)
    return band.crop((0, 0, pixels, band.getbbox()[3]))


# each bitmap's name, its frames' size (units) and their pictures
def bitmaps(menus: Path) -> dict:
    return {"bitmaps/ae_seat_card": (CARD, cards(menus)),
            "bitmaps/ae_seat_devices": (DEVICE, devices()),
            "bitmaps/ae_seat_buttons": (BUTTON, buttons()),
            "bitmaps/ae_seat_profile_bar": (PROFILE_BAR, [_band(menus, 1, PROFILE_BAR[0])]),
            "bitmaps/ae_seat_strip": (STRIP, [_band(menus, 1, STRIP[0])])}


def png_path(bitmap: str, frame: int) -> str:
    return f"ce/port/{bitmap}__{frame}.png"


def write(menus: Path = MENUS) -> list:
    written = []
    for bitmap, (_, frames) in bitmaps(menus).items():
        for frame, image in enumerate(frames):
            path = menus / png_path(bitmap, frame)
            path.parent.mkdir(parents=True, exist_ok=True)
            image.save(path)
            written.append(png_path(bitmap, frame))
    return written


def bitmaps_xml(menus: Path = MENUS) -> list:
    """bitmaps.xml's lines for them"""
    lines = []
    for bitmap, (size, frames) in bitmaps(menus).items():
        lines.append(f'\t<bitmap name="{bitmap}">')
        lines += [f'\t\t<frame png="{png_path(bitmap, frame)}" width="{size[0]}" height="{size[1]}"/>'
                  for frame in range(len(frames))]
        lines.append("\t</bitmap>")
    return lines


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--write", action="store_true", help="write the PNGs into port/assets/menus")
    parser.add_argument("--xml", action="store_true", help="print bitmaps.xml's lines")
    arguments = parser.parse_args()
    if arguments.write:
        for path in write():
            print(path)
    if arguments.xml:
        print("\n".join(bitmaps_xml()))
