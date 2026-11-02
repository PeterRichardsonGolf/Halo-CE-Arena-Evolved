#!/usr/bin/env python3
"""Arena Evolved: the keys' pictures of the HUD's prompts on mouse + keyboard (0.3.5 item 1, the owner's approved
design of 2026-10-18, look A). While the keyboard and mouse are the last used, a prompt
of the keyboard's player shows the key bound to its action ("Hold [E] to swap for") in place of the pad's button
(port/linux/game/ae_key_prompts.c draws them). Kenney's Input Prompts glyphs (CC0, port/linux/ui/fonts/
input_keyboard.ttf), drawn here into two sheets:

- HUD STYLE AE (look A): Kenney's OUTLINE glyph in ice (#E8F4FF) on a navy keycap (#081226 at 0.88: the glyph's
  silhouette, its holes filled), a black outline round it (1.8 px at 0.85, the HUD text's);
- the CE lines (HUD STYLE CE, and AE's "ce" comparison): Kenney's FILLED glyph in the old chip blue (#2896F0), its
  letter knocked out over #060E1E, a soft dark shadow (0.55) and no hard outline.

Each sheet also has a plain keycap (a rounded box, radius 0.2 of its height) the game stretches for a key Kenney has
no glyph for, the key's name written on it in the HUD's font ("Num 7"). No amber.

    python3 tools/ae_key_pictures.py --write

writes port/assets/buttons/keys/ae_keys_ae.png, ae_keys_ce.png and keys.json (tools/ae_button_glyphs.py embeds them
as AE's own textures), and port/linux/game/ae_key_sheet.inc (the cells and the key names, which
ae_key_prompts_rules.c reads). tools/test_ae_key_pictures.py checks the files are what this draws. Needs Pillow,
NumPy, SciPy and fontTools.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "port" / "linux" / "ui" / "fonts" / "input_keyboard.ttf"
FOLDER = ROOT / "port" / "assets" / "buttons" / "keys"
INCLUDE = ROOT / "port" / "linux" / "game" / "ae_key_sheet.inc"

# texels a pixel at 1080p (the HUD at scale 1): 4, so a 4K screen (scale 2) still draws them from more texels than
# pixels; drawn SUPER times larger again and reduced (smooth edges)
TEXELS = 4
SUPER = 4
# room round each cell (texels) so that the reduced mip levels never take a neighbour's colour
PAD = 8
SHEET_WIDTH = 2048

# (the spec's sizes at 1080p, px) the keycap's height, a mouse's, and the room round them for the outline or shadow
STYLES = {
    "ae": {"key": 26.0, "mouse": 30.0, "margin": 2.0},
    "ce": {"key": 30.0, "mouse": 34.0, "margin": 3.0},
}
ICE = (0xE8, 0xF4, 0xFF)
NAVY = (0x08, 0x12, 0x26)
RIM = (0x96, 0xC8, 0xFF)
CE_BLUE = (0x28, 0x96, 0xF0)
CE_DARK = (0x06, 0x0E, 0x1E)
BLACK = (0, 0, 0)
AE_FILL_ALPHA = 0.88
AE_OUTLINE = 1.8
AE_OUTLINE_ALPHA = 0.85
FRAME_FILL_ALPHA = 0.94
FRAME_RIM = 1.6
FRAME_RADIUS = 0.2
FRAME_WIDE = 2.0          # the plain keycap's cell, of its height (the game stretches its middle)
CE_SHADOW_ALPHA = 0.55
CE_SHADOW_DOWN = 1.5
CE_SHADOW_BLUR = 1.2


def _keys():
    """(the names config.toml and the keyboard's layout give a key: halo_input_shown_name, SDL's scancode names;
    matched without case) -> Kenney's glyph (its filled name; the outline is name + "_outline")"""
    keys = [((chr(c),), f"keyboard_{chr(c).lower()}") for c in range(ord("A"), ord("Z") + 1)]
    keys += [((str(d),), f"keyboard_{d}") for d in range(10)]
    keys += [((f"F{n}",), f"keyboard_f{n}") for n in range(1, 13)]
    keys += [
        (("Space",), "keyboard_space"),
        (("Left Shift", "Right Shift", "Shift"), "keyboard_shift"),
        (("Left Ctrl", "Right Ctrl", "Ctrl"), "keyboard_ctrl"),
        (("Left Alt", "Right Alt", "Alt"), "keyboard_alt"),
        (("Tab",), "keyboard_tab"),
        (("Return", "Enter"), "keyboard_enter"),
        (("Keypad Enter",), "keyboard_numpad_enter"),
        (("Escape", "Esc"), "keyboard_escape"),
        (("Backspace",), "keyboard_backspace"),
        (("CapsLock",), "keyboard_capslock"),
        (("Insert",), "keyboard_insert"),
        (("Delete",), "keyboard_delete"),
        (("Home",), "keyboard_home"),
        (("End",), "keyboard_end"),
        (("PageUp",), "keyboard_page_up"),
        (("PageDown",), "keyboard_page_down"),
        (("Numlock", "NumLock"), "keyboard_numlock"),
        (("PrintScreen",), "keyboard_printscreen"),
        (("ScrollLock",), "keyboard_scroll_lock"),
        (("Pause",), "keyboard_pause"),
        (("Left GUI", "Right GUI"), "keyboard_win"),
        (("Up",), "keyboard_arrow_up"),
        (("Down",), "keyboard_arrow_down"),
        (("Left",), "keyboard_arrow_left"),
        (("Right",), "keyboard_arrow_right"),
        (("-",), "keyboard_minus"),
        (("=",), "keyboard_equals"),
        (("[",), "keyboard_bracket_open"),
        (("]",), "keyboard_bracket_close"),
        ((";",), "keyboard_semicolon"),
        (("'",), "keyboard_apostrophe"),
        (("Comma", ","), "keyboard_comma"),
        ((".",), "keyboard_period"),
        (("/",), "keyboard_slash_forward"),
        (("\\",), "keyboard_slash_back"),
        (("`", "~"), "keyboard_tilde"),
        (("Keypad +",), "keyboard_numpad_plus"),
        (("Keypad *",), "keyboard_asterisk"),
        # (the mouse: the pressed button lit; the middle button and the wheel both the wheel lit)
        (("Mouse Left",), "mouse_left"),
        (("Mouse Right",), "mouse_right"),
        (("Mouse Middle", "Wheel"), "mouse_scroll"),
        (("Wheel Up",), "mouse_scroll_up"),
        (("Wheel Down",), "mouse_scroll_down"),
        (("Mouse 4",), "mouse_side_back"),
        (("Mouse 5",), "mouse_side_forward"),
    ]
    return keys


KEYS = _keys()
FRAME = "frame"


def glyphs():
    """the cells' glyphs in order (each once), the plain keycap last"""
    seen = []
    for _, glyph in KEYS:
        if glyph not in seen:
            seen.append(glyph)
    return seen + [FRAME]


def _codes():
    from fontTools.ttLib import TTFont
    table = TTFont(str(FONT)).getBestCmap()
    return {name: point for point, name in table.items()}


def _ink(name: str, codes: dict):
    """the glyph's coverage drawn large, cut to its ink"""
    from PIL import Image, ImageDraw, ImageFont
    face = ImageFont.truetype(str(FONT), 640)
    big = Image.new("L", (1024, 1024), 0)
    ImageDraw.Draw(big).text((192, 128), chr(codes[name]), font=face, fill=255)
    return big.crop(big.getbbox())


def _filled_holes(mask):
    """a mask (bool array) with its holes filled: all but what the border reaches"""
    import numpy as np
    from scipy import ndimage
    return ndimage.binary_fill_holes(mask)


def _ring(mask, radius: float):
    """coverage of the ring up to radius (px of the array) outside the mask, antialiased"""
    import numpy as np
    from scipy import ndimage
    distance = ndimage.distance_transform_edt(~mask)
    return np.clip(radius + 0.5 - distance, 0.0, 1.0) * (~mask)


def _layer(rgb, alpha):
    import numpy as np
    out = np.zeros(alpha.shape + (4,), np.float64)
    out[..., 0], out[..., 1], out[..., 2] = (c / 255.0 for c in rgb)
    out[..., 3] = np.clip(alpha, 0.0, 1.0)
    return out


def _over(top, bottom):
    """straight-alpha 'over'"""
    import numpy as np
    alpha = top[..., 3:] + bottom[..., 3:] * (1.0 - top[..., 3:])
    colour = top[..., :3] * top[..., 3:] + bottom[..., :3] * bottom[..., 3:] * (1.0 - top[..., 3:])
    safe = np.where(alpha > 0.0, alpha, 1.0)
    return np.concatenate([colour / safe, alpha], axis=-1)


def _reduce(array):
    """the supersampled picture down to texels (premultiplied while reduced)"""
    import numpy as np
    from PIL import Image
    premultiplied = array.copy()
    premultiplied[..., :3] *= premultiplied[..., 3:]
    h, w = array.shape[:2]
    size = (w // SUPER, h // SUPER)
    channels = [np.asarray(Image.fromarray(premultiplied[..., i].astype(np.float32), "F").resize(size, Image.BOX))
                for i in range(4)]
    out = np.stack(channels, axis=-1)
    alpha = np.clip(out[..., 3:], 0.0, 1.0)
    colour = np.where(alpha > 0.0, out[..., :3] / np.where(alpha > 0.0, alpha, 1.0), 0.0)
    return np.clip(np.concatenate([colour, alpha], axis=-1), 0.0, 1.0)


def _scaled(ink, height: int):
    """the ink scaled to height (supersampled px), its aspect kept: coverage 0..1"""
    import numpy as np
    from PIL import Image
    width = max(1, round(ink.width * height / ink.height))
    return np.asarray(ink.resize((width, height), Image.LANCZOS), np.float64) / 255.0


def _cell_ae(name: str, codes: dict):
    """look A: the outline glyph in ice on the navy keycap, the black outline round it"""
    import numpy as np
    style = STYLES["ae"]
    body = (style["mouse"] if name.startswith("mouse") else style["key"]) * TEXELS * SUPER
    margin = round(style["margin"] * TEXELS * SUPER)
    glyph = _scaled(_ink(name + "_outline", codes), round(body))
    glyph = np.pad(glyph, margin)
    silhouette = _filled_holes(glyph > 0.5)
    outline = _ring(silhouette, AE_OUTLINE * TEXELS * SUPER)
    picture = _layer(BLACK, outline * AE_OUTLINE_ALPHA)
    picture = _over(_layer(NAVY, silhouette * AE_FILL_ALPHA), picture)
    picture = _over(_layer(ICE, glyph), picture)
    return picture


def _cell_ce(name: str, codes: dict):
    """CE's lines: the filled glyph in the chip blue, its letter knocked out over the dark, a soft shadow"""
    import numpy as np
    from scipy import ndimage
    style = STYLES["ce"]
    body = (style["mouse"] if name.startswith("mouse") else style["key"]) * TEXELS * SUPER
    margin = round(style["margin"] * TEXELS * SUPER)
    glyph = _scaled(_ink(name, codes), round(body))
    glyph = np.pad(glyph, margin)
    silhouette = _filled_holes(glyph > 0.5)
    shadow = ndimage.gaussian_filter(silhouette.astype(np.float64), CE_SHADOW_BLUR * TEXELS * SUPER)
    shadow = ndimage.shift(shadow, (CE_SHADOW_DOWN * TEXELS * SUPER, 0), order=1)
    picture = _layer(CE_DARK, shadow * CE_SHADOW_ALPHA)
    picture = _over(_layer(CE_DARK, silhouette.astype(np.float64)), picture)
    picture = _over(_layer(CE_BLUE, glyph), picture)
    return picture


def _rounded(width: int, height: int, inset: float, radius: float):
    """a rounded box's coverage (supersampled px), inset from the array's edge"""
    import numpy as np
    y, x = np.mgrid[0:height, 0:width] + 0.5
    x0, y0, x1, y1 = inset, inset, width - inset, height - inset
    cx = np.clip(x, x0 + radius, x1 - radius)
    cy = np.clip(y, y0 + radius, y1 - radius)
    distance = np.hypot(x - cx, y - cy) - radius
    return np.clip(0.5 - distance, 0.0, 1.0)


def _cell_frame(style_name: str):
    """the plain keycap (look C's construction): AE a navy box with the ice-blue rim and the black outline; CE the chip
    blue with the soft shadow"""
    import numpy as np
    from scipy import ndimage
    style = STYLES[style_name]
    unit = TEXELS * SUPER
    body = style["key"] * unit
    margin = style["margin"] * unit
    width = round(body * FRAME_WIDE + 2 * margin)
    height = round(body + 2 * margin)
    radius = FRAME_RADIUS * body
    box = _rounded(width, height, margin, radius)
    if style_name == "ae":
        outer = _rounded(width, height, margin - AE_OUTLINE * unit, radius + AE_OUTLINE * unit)
        inner = _rounded(width, height, margin + FRAME_RIM * unit, max(radius - FRAME_RIM * unit, 0.0))
        picture = _layer(BLACK, np.clip(outer - box, 0.0, 1.0) * AE_OUTLINE_ALPHA)
        picture = _over(_layer(NAVY, inner * FRAME_FILL_ALPHA), picture)
        picture = _over(_layer(RIM, np.clip(box - inner, 0.0, 1.0)), picture)
        return picture
    shadow = ndimage.gaussian_filter(box, CE_SHADOW_BLUR * unit)
    shadow = ndimage.shift(shadow, (CE_SHADOW_DOWN * unit, 0), order=1)
    picture = _layer(CE_DARK, shadow * CE_SHADOW_ALPHA)
    picture = _over(_layer(CE_BLUE, box), picture)
    return picture


def draw():
    """the two sheets (arrays of texels, 0..1 RGBA) and each cell's place: {style: (sheet, [(x, y, w, h)])}"""
    import numpy as np
    codes = _codes()
    result = {}
    for style_name in STYLES:
        pictures = []
        for name in glyphs():
            if name == FRAME:
                big = _cell_frame(style_name)
            elif style_name == "ae":
                big = _cell_ae(name, codes)
            else:
                big = _cell_ce(name, codes)
            # (whole texels: the supersampled picture cut to a multiple of SUPER)
            h, w = big.shape[:2]
            big = np.pad(big, ((0, -h % SUPER), (0, -w % SUPER), (0, 0)))
            pictures.append(_reduce(big))
        # (shelves, in order)
        places, x, y, shelf = [], PAD, PAD, 0
        for picture in pictures:
            h, w = picture.shape[:2]
            if x + w + PAD > SHEET_WIDTH:
                x, y, shelf = PAD, y + shelf + PAD, 0
            places.append((x, y, w, h))
            x += w + PAD
            shelf = max(shelf, h)
        height = y + shelf + PAD
        height += -height % 4
        sheet = np.zeros((height, SHEET_WIDTH, 4), np.float64)
        for picture, (x, y, w, h) in zip(pictures, places):
            sheet[y:y + h, x:x + w] = picture
        result[style_name] = (sheet, places)
    return result


def _png(sheet):
    import io
    import numpy as np
    from PIL import Image
    data = io.BytesIO()
    Image.fromarray(np.round(sheet * 255.0).astype(np.uint8), "RGBA").save(data, "PNG", optimize=True)
    return data.getvalue()


def _c_string(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def include_text(drawn) -> str:
    """port/linux/game/ae_key_sheet.inc"""
    codes = _codes()
    names = glyphs()
    lines = ["/* generated by tools/ae_key_pictures.py: do not edit (ae_key_prompts_rules.c includes it) */", "",
             f"#define AE_KEY_TEXELS {TEXELS} /* a cell's texels a pixel at 1080p */"]
    for style_name, style in STYLES.items():
        sheet = drawn[style_name][0]
        upper = style_name.upper()
        lines.append(f"#define AE_KEY_SHEET_{upper}_WIDTH {sheet.shape[1]}")
        lines.append(f"#define AE_KEY_SHEET_{upper}_HEIGHT {sheet.shape[0]}")
        lines.append(f"#define AE_KEY_{upper}_KEY {style['key']:.1f}f /* a keycap's height at 1080p, px */")
        lines.append(f"#define AE_KEY_{upper}_MOUSE {style['mouse']:.1f}f /* a mouse's */")
        lines.append(f"#define AE_KEY_{upper}_MARGIN {style['margin']:.1f}f /* the outline's or shadow's room */")
    lines.append(f"#define AE_KEY_FRAME_CELL {names.index(FRAME)}")
    lines.append(f"#define AE_KEY_FRAME_WIDE {FRAME_WIDE:.1f}f /* the plain keycap's cell, of its height */")
    lines.append("")
    lines.append("/* each cell: Kenney's glyph (its filled name), its outline's code point, whether a mouse's, its place in")
    lines.append("the AE and the CE sheet (texels: x, y, w, h, the outline's or shadow's room included) */")
    lines.append("static const struct ae_key_cell ae_key_cells[] =")
    lines.append("{")
    for index, name in enumerate(names):
        code = codes.get(name + "_outline", 0) if name != FRAME else 0
        mouse = int(name.startswith("mouse"))
        ae = drawn["ae"][1][index]
        ce = drawn["ce"][1][index]
        lines.append(f"\t{{ {_c_string(name)}, 0x{code:04X}u, {mouse}, {{ {{ {ae[0]}, {ae[1]}, {ae[2]}, {ae[3]} }}, "
                     f"{{ {ce[0]}, {ce[1]}, {ce[2]}, {ce[3]} }} }} }},")
    lines.append("};")
    lines.append("")
    lines.append("/* the keys' names (matched without case) and their cells */")
    lines.append("static const struct ae_key_name ae_key_names[] =")
    lines.append("{")
    for key_names, glyph in KEYS:
        for key_name in key_names:
            lines.append(f"\t{{ {_c_string(key_name)}, {names.index(glyph)} }},")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def listing(drawn) -> str:
    """keys.json: the sheets as tools/ae_button_glyphs.py embeds them (AE's own textures, drawn whatever the pad)"""
    assets = []
    for style_name in STYLES:
        sheet = drawn[style_name][0]
        assets.append({"name": f"ae_keys_{style_name}", "tag": f"ae\\key_prompts\\{style_name}", "bitmap": 0,
                       "width": int(sheet.shape[1]), "height": int(sheet.shape[0]), "scale": 1, "crc": 0,
                       "own": True})
    return json.dumps({"assets": assets}, indent=1) + "\n"


def outputs():
    """{path: bytes} of everything this writes"""
    drawn = draw()
    files = {FOLDER / f"ae_keys_{style}.png": _png(drawn[style][0]) for style in STYLES}
    files[FOLDER / "keys.json"] = listing(drawn).encode()
    files[INCLUDE] = include_text(drawn).encode()
    return files


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--write", action="store_true", help="write the sheets, keys.json and ae_key_sheet.inc")
    arguments = parser.parse_args()
    files = outputs()
    for path, data in files.items():
        if arguments.write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        print(f"{path.relative_to(ROOT)}: {len(data)} bytes")


if __name__ == "__main__":
    main()
