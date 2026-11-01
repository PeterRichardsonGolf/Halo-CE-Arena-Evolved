#!/usr/bin/env python3
"""Arena Evolved: the pictures of split screen's JOIN screen (main_menu/multiplayer_type_select/split_screen/join_*,
tools/port_settings.py's _join_screen; spec notes/specs/2026-10-16-split-screen-pc.md 4.2 and 6). In the PC menus'
own look: the cards are the game's panel (bitmaps/spinner_list_3_wide_item_background, its outlined frame) cut to a
card's size, the profile bar the option row's band; the devices are Kenney's Input Prompts glyphs (CC0,
port/linux/ui/fonts) in the menus' blue, but for "another pad", drawn here; the buttons are round chips drawn here
(a blue disc, the symbol in the panel's navy: the OG theme's standard, .superpowers design review 2026-10-18). No
amber: a held seat is dimmed. Nothing is traced.

    python3 tools/ae_seat_pictures.py --write

writes the PNGs (4 pixels a unit, as ce/port's others) under port/assets/menus/ce/port/bitmaps; bitmaps.xml and
menus.json list them (tools/test_ae_seat_pictures.py checks both).

The frames, as ae_split_screen.c sets them:
- bitmaps/ae_seat_card: 0 an empty seat (dim edge, a plus in a dashed ring), 1 a seated player (the menus' blue
  edge), 2 an unplugged pad's held seat (dim edge);
- bitmaps/ae_seat_devices: 0 keyboard + mouse, 1 Xbox pad, 2 PlayStation pad, 3 Nintendo pad, 4 another pad,
  5 unplugged (dim, struck through in grey);
- bitmaps/ae_seat_buttons (round chips): 0 Xbox START, 1 PlayStation OPTIONS (not round: the DualSense's own
  button, an upright pill, Kenney's shape, so it never reads as START), 2 ENTER, 3 Xbox B, 4 PlayStation circle,
  5 ESC (small; the capsule is better), 6 Nintendo B, 7 Nintendo +;
- bitmaps/ae_seat_key_esc: the keyboard's ESC as a capsule;
- bitmaps/ae_seat_button_assign, bitmaps/ae_seat_button_leave: the stock button (bitmaps/text_button_background,
  both its frames) narrowed for a card's ASSIGN / CANCEL and its LEAVE (also the lobby's ASSIGN);
- bitmaps/ae_seat_profile_bar: a pill's bar (the option row's band, outlined): 0 at rest, 1 focused (the menus'
  ice-blue glow; ASSIGN's wait pulses 0 and 1), 2 filled blue (a seat's device pressed, a swap: its name in navy),
  3 inert (its edge dim: an empty seat's OR PICK ONE with no free pad);
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
GREY = (150, 150, 150)
NAVY = (0, 14, 35)
OVERPASS = ROOT / "port" / "assets" / "fonts" / "Overpass-900.ttf"

# (design units: tools/port_settings.py's _join_screen places them)
CARD = (146, 300)
DEVICE = (30, 16)
BUTTON = (16, 16)
KEY_ESC = (22, 14)
# (a card's ASSIGN / CANCEL and its LEAVE, side by side in its 134 units)
ASSIGN_BUTTON = (58, 32)
LEAVE_BUTTON = (54, 32)
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
    None,
    None,
]
# the chips' symbols, by frame
BUTTON_SYMBOLS = ["bars", "options", "return", "B", "ring", "ESC", "B", "plus"]
# (the OPTIONS pill: Kenney's playstation5_button_options_alternative stood upright (it leans 14.5 degrees), as wide as
# 0.55 of its height, its ends fully round)
OPTIONS_ASPECT = 0.55
# (supersampled, then scaled down: smooth edges)
SUPER = 4


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
    cx, cy, r, w = size[0] / 2, 58 * UNIT, 24 * UNIT, 2 * UNIT
    for k in range(24):
        draw.arc((cx - r, cy - r, cx + r, cy + r), k * 15, k * 15 + 9, fill=DIM + (255,), width=w)
    draw.line((cx - 10 * UNIT, cy, cx + 10 * UNIT, cy), fill=DIM + (255,), width=3 * UNIT)
    draw.line((cx, cy - 10 * UNIT, cx, cy + 10 * UNIT), fill=DIM + (255,), width=3 * UNIT)
    return [empty, seated, _recolour_edge(panel, DIM)]


def _neutral_pad(size, colour):
    """a pad of no family: a plain body with two grips and two sticks (drawn here: Kenney's set has none)"""
    from PIL import Image, ImageDraw
    w, h = size[0] * SUPER, size[1] * SUPER
    big = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    ink = colour + (255,)
    body = (w * 0.14, h * 0.1, w * 0.86, h * 0.64)
    d.rounded_rectangle(body, radius=h * 0.22, fill=ink)
    d.ellipse((w * 0.1, h * 0.28, w * 0.38, h * 0.96), fill=ink)
    d.ellipse((w * 0.62, h * 0.28, w * 0.9, h * 0.96), fill=ink)
    for cx in (0.36, 0.64):
        r = h * 0.11
        d.ellipse((w * cx - r, h * 0.42 - r, w * cx + r, h * 0.42 + r), fill=NAVY + (255,))
        r = h * 0.06
        d.ellipse((w * cx - r, h * 0.42 - r, w * cx + r, h * 0.42 + r), fill=ink)
    return big.resize(size, Image.LANCZOS)


def devices() -> list:
    from PIL import Image, ImageDraw
    size = (DEVICE[0] * UNIT, DEVICE[1] * UNIT)
    frames = []
    for index, parts in enumerate(DEVICE_GLYPHS):
        image = Image.new("RGBA", size, (0, 0, 0, 0))
        colour = DIM if index == 5 else BLUE
        if parts is None:
            pad = _neutral_pad((round(size[0] * 0.8), round(size[1] * 0.8)), colour)
            image.alpha_composite(pad, ((size[0] - pad.width) // 2, (size[1] - pad.height) // 2))
            if index == 5:
                ImageDraw.Draw(image).line((size[0] * 0.18, size[1] * 0.9, size[0] * 0.82, size[1] * 0.1),
                                           fill=GREY + (255,), width=3 * UNIT)
            frames.append(image)
            continue
        # (a pad about as tall as the PLAYER title's capitals are wide apart: the mockup's proportion)
        top, bottom = size[1] * 0.12, size[1] * 0.88
        if len(parts) == 2:
            # (keyboard and mouse side by side, the mouse a little smaller)
            _glyph(image, *parts[0], (size[0] * 0.08, top, size[0] * 0.6, bottom), colour)
            _glyph(image, *parts[1], (size[0] * 0.66, top + size[1] * 0.08, size[0] * 0.92, bottom), colour)
        else:
            _glyph(image, *parts[0], (0, top, size[0], bottom), colour)
        frames.append(image)
    return frames


def _text(draw, centre, text: str, height: float, colour):
    """text in Overpass 900, its capitals height pixels tall, centred on centre"""
    from PIL import ImageFont
    font = ImageFont.truetype(str(OVERPASS), max(1, round(height / 0.7)))
    x0, y0, x1, y1 = draw.textbbox((0, 0), text, font=font)
    draw.text((centre[0] - (x0 + x1) / 2, centre[1] - (y0 + y1) / 2), text, font=font, fill=colour)


def chip(symbol: str):
    """a round button chip: a blue disc 15 units across in the 16-unit frame (a clear rim all round), its symbol in
    the panel's navy; PlayStation's OPTIONS is the one chip that is not round: an upright pill as tall as the disc
    with the three bars in it (the DualSense's button: a round chip would be START's)"""
    from PIL import Image, ImageDraw
    side = BUTTON[0] * UNIT * SUPER
    big = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    # (about 15 units: a rim of 2 pixels of 64 stays clear after the scale-down's soft edge)
    D = side * (1 - 2 * 2.5 / 64)
    c = side / 2
    navy = NAVY + (255,)
    if symbol == "options":
        # (edges on whole supersampled pixels, so the scale-down leaves one soft pixel at most: crisp)
        half = round(D * OPTIONS_ASPECT / 2)
        d.rounded_rectangle((c - half, c - D / 2, c + half, c + D / 2), radius=half, fill=BLUE + (255,))
        width, thick, pitch = 1.12 * half, 0.085 * D, 0.2 * D
        for k in (-1, 0, 1):
            y = c + k * pitch
            d.rounded_rectangle((c - width / 2, y - thick / 2, c + width / 2, y + thick / 2), radius=thick / 2,
                                fill=navy)
        return big.resize((BUTTON[0] * UNIT, BUTTON[1] * UNIT), Image.BOX)
    d.ellipse((c - D / 2, c - D / 2, c + D / 2, c + D / 2), fill=BLUE + (255,))
    if symbol == "bars":
        for k in (-1, 0, 1):
            y = c + k * 0.17 * D
            d.rounded_rectangle((c - 0.24 * D, y - 0.045 * D, c + 0.24 * D, y + 0.045 * D), radius=0.045 * D, fill=navy)
    elif symbol == "plus":
        d.rectangle((c - 0.26 * D, c - 0.06 * D, c + 0.26 * D, c + 0.06 * D), fill=navy)
        d.rectangle((c - 0.06 * D, c - 0.26 * D, c + 0.06 * D, c + 0.26 * D), fill=navy)
    elif symbol == "ring":
        r, t = 0.25 * D, 0.1 * D
        d.ellipse((c - r, c - r, c + r, c + r), outline=navy, width=round(t))
    elif symbol == "return":
        t = 0.1 * D
        x_right, y_top, y_mid, x_left = c + 0.22 * D, c - 0.24 * D, c + 0.08 * D, c - 0.16 * D
        d.rectangle((x_right - t, y_top, x_right, y_mid + t / 2), fill=navy)
        d.rectangle((x_left, y_mid - t / 2, x_right, y_mid + t / 2), fill=navy)
        a = 0.15 * D
        d.polygon([(x_left - a * 0.7, y_mid), (x_left + a * 0.5, y_mid - a), (x_left + a * 0.5, y_mid + a)], fill=navy)
    else:
        _text(d, (c, c), symbol, (0.62 if len(symbol) == 1 else 0.28) * D, navy)
    return big.resize((BUTTON[0] * UNIT, BUTTON[1] * UNIT), Image.BOX)  # (an area average: no ringing into the rim)


def buttons() -> list:
    return [chip(symbol) for symbol in BUTTON_SYMBOLS]


def key_esc():
    """the keyboard's ESC: a capsule 26x16 units, its ends fully round, with a clear rim, ESC in navy"""
    from PIL import Image, ImageDraw
    w, h = KEY_ESC[0] * UNIT * SUPER, KEY_ESC[1] * UNIT * SUPER
    big = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    rim = h * 2.5 / 64
    d.rounded_rectangle((rim, rim, w - rim, h - rim), radius=(h - 2 * rim) / 2, fill=BLUE + (255,))
    _text(d, (w / 2, h / 2), "ESC", 0.42 * h, NAVY + (255,))
    return big.resize((KEY_ESC[0] * UNIT, KEY_ESC[1] * UNIT), Image.BOX)  # (an area average: no ringing into the rim)


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


def small_buttons(menus: Path, size) -> list:
    """the stock button's frames narrowed to size (units), their rounded ends kept"""
    from PIL import Image
    frames = []
    for frame in (0, 1):
        source = Image.open(menus / "ce" / "shell" / "bitmaps" / f"text_button_background__{frame}.png").convert("RGBA")
        frames.append(_nine_slice(source, (size[0] * UNIT, size[1] * UNIT), 40))
    return frames


ICE = (184, 214, 255)
ICE_OUTER = (120, 175, 235)


def pills(menus: Path) -> list:
    """the pill's four looks: the band, the band in the focus's glow (an inner 3-pixel ice-blue edge, an outer
    1-pixel lighter blue), the band filled with the menus' blue, and the band with its edge dim (nothing to pick)"""
    from PIL import Image, ImageDraw
    band = _band(menus, 1, PROFILE_BAR[0])
    w, h = band.size
    focused = band.copy()
    d = ImageDraw.Draw(focused)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=h // 2, outline=ICE_OUTER + (255,), width=1 * UNIT // 4 + 1)
    d.rounded_rectangle((1, 1, w - 2, h - 2), radius=h // 2 - 1, outline=ICE + (255,), width=3)
    filled = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(filled).rounded_rectangle((0, 0, w - 1, h - 1), radius=h // 2, fill=BLUE + (255,))
    # (the band's own fill kept: only its edge goes dim, as an empty card's)
    inert = _recolour_edge(band, DIM, fill=band.getpixel((w // 2, h // 2))[:3])
    return [band, focused, filled, inert]


# each bitmap's name, its frames' size (units) and their pictures
def bitmaps(menus: Path) -> dict:
    return {"bitmaps/ae_seat_card": (CARD, cards(menus)),
            "bitmaps/ae_seat_devices": (DEVICE, devices()),
            "bitmaps/ae_seat_buttons": (BUTTON, buttons()),
            "bitmaps/ae_seat_key_esc": (KEY_ESC, [key_esc()]),
            "bitmaps/ae_seat_button_assign": (ASSIGN_BUTTON, small_buttons(menus, ASSIGN_BUTTON)),
            "bitmaps/ae_seat_button_leave": (LEAVE_BUTTON, small_buttons(menus, LEAVE_BUTTON)),
            "bitmaps/ae_seat_profile_bar": (PROFILE_BAR, pills(menus)),
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
