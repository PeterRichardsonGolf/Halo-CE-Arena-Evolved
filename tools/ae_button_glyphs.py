#!/usr/bin/env python3
"""Arena Evolved: the PlayStation textures of the button icons (from DamnationCE v0.3.41, CC0).

While a PlayStation controller is the one last used, the game's button icons show its buttons (A, B, X and Y as
cross, circle, square and triangle): port/linux/src/ae_buttons.c draws these textures in the icons' place. They live
in port/assets/buttons/playstation (playstation.json lists them: the bitmap each stands for, its tag, index, size and
the CRC-32 of its first mip level's pixels, and for a sheet the sprites it is drawn for).

    python tools/ae_button_glyphs.py draw --map <Xbox ui.map> --game-map <an Xbox multiplayer map>

draws them again (DamnationCE's tools/button_assets.py playstation, CC0): each icon a round dark button with its
symbol in its colour, a message icons' sheet white discs with their symbols cut out (the game tints them). Plain
shapes, no logos. Needs Pillow and NumPy, and tools/button_assets.py's map reader.

tools/embed_assets.py embeds them through its AE hooks (inputs, configure_inputs, embedded_lines): the generated
source gets ae_button_glyphs[] (port/linux/src/ae_buttons.h), whatever the folder has (none: an empty table).
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Callable, List

ROOT = Path(__file__).resolve().parent.parent
FOLDER = Path("port/assets/buttons/playstation")
LIST = FOLDER / "playstation.json"
# the Nintendo textures (the same bitmaps, drawn for a Nintendo pad), a family of the same table (nintendo.json)
NINTENDO_FOLDER = Path("port/assets/buttons/nintendo")
NINTENDO_LIST = NINTENDO_FOLDER / "nintendo.json"
NINTENDO_FONT = ROOT / "port" / "linux" / "ui" / "fonts" / "input_nintendo.ttf"
# the families of ae_button_glyph.family (port/linux/src/ae_buttons.h): AE's own pictures 0, PlayStation 1, Nintendo 2
FAMILY_PLAYSTATION = 1
FAMILY_NINTENDO = 2
# Kenney's Nintendo face glyphs (input_nintendo.ttf, U+E005 A, U+E007 B, U+E019 Y, U+E017 X: its discs with the
# letter knocked out), by the LABEL on the button, as the menus name them (ui_overlay.c: the Xbox's A, the south
# button, is Nintendo's B): the message icons' sequences 0 to 3 are the Xbox's A, B, X, Y
SEQUENCE_LETTERS = {0: 0xE007, 1: 0xE005, 2: 0xE019, 3: 0xE017}
SYMBOL_LETTERS = {"cross": 0xE007, "circle": 0xE005, "square": 0xE019, "triangle": 0xE017}
LETTER_INK = (234, 236, 242)
# AE's own pictures drawn whatever the pad: the HUD's key prompts' sheets (tools/ae_key_pictures.py,
# port/linux/game/ae_key_prompts.c), in the same table with own set
OWN_FOLDER = Path("port/assets/buttons/keys")
OWN_LIST = OWN_FOLDER / "keys.json"

# each symbol's colour, as PlayStation controllers have them (DamnationCE's)
COLOURS = {"cross": (110, 156, 226), "circle": (232, 86, 86), "square": (216, 132, 198), "triangle": (66, 212, 170)}
# the message icons' sequences 0 to 3 are A, B, X and Y
SEQUENCE_SYMBOLS = {0: "cross", 1: "circle", 2: "square", 3: "triangle"}
# drawn at this many times the texture's size, then reduced (its edges)
SUPERSAMPLE = 4


LISTS = ((FOLDER, LIST, FAMILY_PLAYSTATION), (NINTENDO_FOLDER, NINTENDO_LIST, FAMILY_NINTENDO), (OWN_FOLDER, OWN_LIST, 0))


def assets() -> List[dict]:
    """the PlayStation textures (their folder FOLDER), then AE's own pictures (OWN_FOLDER, own set)"""
    found = []
    for folder, listing, family in LISTS:
        if (ROOT / listing).is_file():
            found += [dict(asset, folder=folder, family=family)
                      for asset in json.loads((ROOT / listing).read_text())["assets"]]
    return found


# ---------- tools/embed_assets.py's AE hooks


def inputs() -> List[Path]:
    """The files the embedded table is made from."""
    return ([listing for _, listing, _ in LISTS if (ROOT / listing).is_file()] +
            [asset["folder"] / f"{asset['name']}.png" for asset in assets()])


def configure_inputs() -> List[Path]:
    """What configure.py is run again for (the list, and the folder for files added or removed)."""
    return [path for folder, listing, _ in LISTS if (ROOT / listing).is_file() for path in (folder, listing)]


def embedded_lines(words: Callable, png_size: Callable) -> List[str]:
    """ae_button_glyphs[] and its count, as port/linux/src/ae_buttons.h declares them."""
    lines = ['#include "ae_buttons.h"', ""]
    table = []
    for index, asset in enumerate(assets()):
        name = f"{asset['name']}.png"
        data = (ROOT / asset["folder"] / name).read_bytes()
        width, height = png_size(data, name)
        scale = asset["scale"]
        if (width, height) != (asset["width"] * scale, asset["height"] * scale):
            sys.exit(f"{name}: {width}x{height}, not {scale}x its bitmap's {asset['width']}x{asset['height']}")
        sequences = [sprite["sequence"] for sprite in asset.get("sprites", [])]
        if any(not 0 <= sequence < 32 for sequence in sequences):
            sys.exit(f"{name}: sprite sequences must be 0 to 31 (ae_buttons.h: sprites)")
        sprites = sum(1 << sequence for sequence in sequences)
        lines.append(f"static const unsigned int ae_button_glyph{index}[] = {{")
        lines.extend(words(data))
        lines.append("};")
        lines.append("")
        tag = asset["tag"].replace("\\", "\\\\")
        table.append(f'\t{{ "{tag}", {asset["bitmap"]}, {asset["width"]}, {asset["height"]}, '
                     f'0x{asset["crc"]:08x}u, 0x{sprites:x}u, ae_button_glyph{index}, {len(data)}, '
                     f'{int(bool(asset.get("own")))}, {asset["family"]} }},')
    lines.append("const struct ae_button_glyph ae_button_glyphs[] =")
    lines.append("{")
    lines.extend(table if table else ["\t{ 0 },"])
    lines.append("};")
    lines.append(f"const unsigned int ae_button_glyph_count = {len(table)};")
    lines.append("")
    return lines


# ---------- drawing (DamnationCE v0.3.41's tools/button_assets.py playstation, CC0)


def symbol_shapes(draw, symbol: str, x: float, y: float, radius: float, width: float, fill) -> None:
    """A PlayStation symbol about x, y on a button of radius, its lines width wide."""
    if symbol == "cross":
        arm = 0.36 * radius
        for sign in (1, -1):
            draw.line([(x - arm, y - sign * arm), (x + arm, y + sign * arm)], fill=fill, width=round(width))
    elif symbol == "circle":
        ring = 0.40 * radius
        draw.ellipse([x - ring, y - ring, x + ring, y + ring], outline=fill, width=round(width))
    elif symbol == "square":
        half = 0.34 * radius
        draw.rectangle([x - half, y - half, x + half, y + half], outline=fill, width=round(width))
    else:
        # (its centroid a little low, as the symbol sits on the button)
        size = 0.46 * radius
        centre = y + 0.06 * radius
        corners = [(x, centre - size), (x + size * 0.866, centre + size / 2), (x - size * 0.866, centre + size / 2)]
        draw.line(corners + [corners[0], corners[1]], fill=fill, width=round(width), joint="curve")


def draw_icon(scale: int, width: int, height: int, circle: tuple, symbol: str):
    """An icon's texture: a round dark button, rimmed, its symbol in its colour (circle: texels' centre and radius)."""
    import numpy as np
    from PIL import Image, ImageDraw

    factor = scale * SUPERSAMPLE
    image = Image.new("RGBA", (width * factor, height * factor), (0, 0, 0, 0))
    x, y, radius = (value * factor for value in circle)
    # (the body darker downward, as the Xbox icons' ovals are lit from above)
    shade = np.linspace(64, 16, image.size[1]).astype(np.uint8)
    body_pixels = np.zeros((image.size[1], image.size[0], 4), np.uint8)
    body_pixels[..., 0] = body_pixels[..., 1] = shade[:, None]
    body_pixels[..., 2] = (shade[:, None] * 1.08).clip(0, 255).astype(np.uint8)
    body_pixels[..., 3] = 255
    body = Image.fromarray(body_pixels, "RGBA")
    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).ellipse([x - radius, y - radius, x + radius, y + radius], fill=255)
    image.paste(body, (0, 0), mask)
    draw = ImageDraw.Draw(image)
    draw.ellipse([x - radius, y - radius, x + radius, y + radius], outline=(150, 152, 158, 255),
                 width=max(1, round(0.08 * radius)))
    if symbol != "blank":
        symbol_shapes(draw, symbol, x, y, radius, 0.14 * radius, COLOURS[symbol] + (255,))
    return np.asarray(image.resize((width * scale, height * scale), Image.LANCZOS))


def draw_stencils(scale: int, width: int, height: int, circles: list):
    """A message icons' sheet's texture: each button a white disc with its symbol cut out (circles: texels' centre,
    radius and symbol), grey the same as alpha, as the sheet's."""
    import numpy as np
    from PIL import Image, ImageDraw

    factor = scale * SUPERSAMPLE
    mask = Image.new("L", (width * factor, height * factor), 0)
    draw = ImageDraw.Draw(mask)
    for x, y, radius, symbol in circles:
        x, y, radius = x * factor, y * factor, radius * factor
        draw.ellipse([x - radius, y - radius, x + radius, y + radius], fill=255)
        symbol_shapes(draw, symbol, x, y, radius, 0.17 * radius, 0)
    alpha = np.asarray(mask.resize((width * scale, height * scale), Image.LANCZOS))
    return np.dstack([alpha, alpha, alpha, alpha])


def letter_mask(codepoint: int):
    """Kenney's Nintendo glyph for a face button as a float image of its disc (0 to 1): (disc, letter), both square,
    the disc touching the edges, the letter the glyph's knocked-out part."""
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
    from scipy import ndimage

    size = 1024
    font = ImageFont.truetype(str(NINTENDO_FONT), size)
    canvas = Image.new("L", (2 * size, 2 * size), 0)
    ImageDraw.Draw(canvas).text((size // 2, size // 2), chr(codepoint), font=font, fill=255)
    pixels = np.asarray(canvas)
    rows, columns = np.nonzero(pixels > 8)
    top, bottom, left, right = rows.min(), rows.max() + 1, columns.min(), columns.max() + 1
    side = max(bottom - top, right - left)
    # (the glyph's disc is a circle: its box centred in a square one)
    middle_y, middle_x = (top + bottom) // 2, (left + right) // 2
    square = pixels[middle_y - side // 2:middle_y - side // 2 + side, middle_x - side // 2:middle_x - side // 2 + side]
    glyph = square.astype(np.float32) / 255.0
    disc = ndimage.binary_fill_holes(glyph > 0.5).astype(np.float32)
    # (the disc's own edge keeps the glyph's antialiasing: its letter is what the glyph lacks inside it)
    letter = np.clip(disc - glyph, 0.0, 1.0) * (glyph < 0.99)
    return disc, letter


def resized(array, side: int):
    import numpy as np
    from PIL import Image

    image = Image.fromarray((np.clip(array, 0.0, 1.0) * 255).astype(np.uint8), "L")
    return np.asarray(image.resize((side, side), Image.LANCZOS)).astype(np.float32) / 255.0


def place(canvas, x: float, y: float, diameter: float, layer) -> None:
    """layer (a square float image) added onto the canvas' 2D array, its middle at x, y, diameter wide"""
    side = max(2, round(diameter))
    layer = resized(layer, side)
    left, top = round(x - side / 2.0), round(y - side / 2.0)
    height, width = canvas.shape
    x0, y0, x1, y1 = max(left, 0), max(top, 0), min(left + side, width), min(top + side, height)
    if x1 > x0 and y1 > y0:
        canvas[y0:y1, x0:x1] = canvas[y0:y1, x0:x1] + layer[y0 - top:y1 - top, x0 - left:x1 - left]


def nintendo_icon(scale: int, width: int, height: int, circle: tuple, codepoint: int):
    """An icon's texture: the PlayStation icon's round dark button, rimmed, Kenney's letter on it in light grey."""
    import numpy as np

    base = draw_icon(scale, width, height, circle, "blank")
    _, letter = letter_mask(codepoint)
    factor = scale
    x, y, radius = (value * factor for value in circle)
    ink = np.zeros(base.shape[:2], np.float32)
    place(ink, x, y, 2.0 * radius * 0.98, letter)
    ink = np.clip(ink, 0.0, 1.0)
    out = base.astype(np.float32)
    for channel, value in enumerate(LETTER_INK):
        out[..., channel] = out[..., channel] * (1.0 - ink) + value * ink
    return out.clip(0, 255).astype(np.uint8)


def nintendo_stencils(scale: int, width: int, height: int, circles: list):
    """A message icons' sheet's texture: each button Kenney's disc with its letter cut out (circles: texels' centre,
    radius and letter's code point), grey the same as alpha, as the sheet's."""
    import numpy as np

    alpha = np.zeros((height * scale, width * scale), np.float32)
    for x, y, radius, codepoint in circles:
        disc, letter = letter_mask(codepoint)
        place(alpha, x * scale, y * scale, 2.0 * radius * scale, disc - letter)
    alpha = (np.clip(alpha, 0.0, 1.0) * 255).astype(np.uint8)
    return np.dstack([alpha, alpha, alpha, alpha])


def circle_of(mask):
    """centre and radius (pixels) of the one disc in a boolean image, its holes filled"""
    import numpy as np
    from scipy import ndimage

    filled = ndimage.binary_fill_holes(mask)
    rows, columns = np.nonzero(filled)
    return (columns.min() + columns.max() + 1) / 2.0, (rows.min() + rows.max() + 1) / 2.0, \
        ((columns.max() + 1 - columns.min()) + (rows.max() + 1 - rows.min())) / 4.0


def draw_nintendo() -> None:
    """The Nintendo textures, from the PlayStation ones' discs (the same bitmaps, so the same places) and Kenney's
    letters; nintendo.json is the PlayStation list with each symbol named by its letter."""
    import numpy as np
    from PIL import Image

    sys.path.insert(0, str(ROOT / "tools"))
    import button_assets as tool  # noqa: E402

    listing = json.loads((ROOT / LIST).read_text())
    out = {"comment": "Arena Evolved: the Nintendo textures of the button icons, drawn by tools/ae_button_glyphs.py "
                      "nintendo from the PlayStation ones' places and Kenney's Input Prompts glyphs (CC0, "
                      "port/linux/ui/fonts/input_nintendo.ttf), labelled as the Nintendo pad's buttons are (its east "
                      "and south are A and B where the Xbox's are B and A); embedded by it through tools/embed_assets.py's "
                      "AE hook, drawn by port/linux/src/ae_buttons.c while a Nintendo controller is the last used.",
           "assets": []}
    (ROOT / NINTENDO_FOLDER).mkdir(parents=True, exist_ok=True)
    for asset in listing["assets"]:
        scale = asset["scale"]
        source = np.asarray(Image.open(ROOT / FOLDER / f"{asset['name']}.png").convert("RGBA"))
        alpha = source[..., 3] > 128
        name = asset["name"].replace("_playstation", "_nintendo")
        if "sprites" in asset:
            circles = []
            for sprite in asset["sprites"]:
                left, top, right, bottom = sprite["cell"]
                cx, cy, radius = circle_of(alpha[top * scale:bottom * scale, left * scale:right * scale])
                circles.append((left + cx / scale, top + cy / scale, radius / scale, SEQUENCE_LETTERS[sprite["sequence"]]))
            image = nintendo_stencils(scale, asset["width"], asset["height"], circles)
        else:
            cx, cy, outer = circle_of(alpha)
            # (the rim is drawn 0.04 of the radius beyond it: draw_icon's)
            image = tool.bleed(nintendo_icon(scale, asset["width"], asset["height"],
                                             (cx / scale, cy / scale, outer / scale / 1.04), SYMBOL_LETTERS[asset["symbol"]]))
        Image.fromarray(image, "RGBA").save(ROOT / NINTENDO_FOLDER / f"{name}.png", optimize=True)
        entry = {key: value for key, value in asset.items() if key != "symbol"}
        entry["name"] = name
        if "symbol" in asset:
            entry["letter"] = {"cross": "B", "circle": "A", "square": "Y", "triangle": "X"}[asset["symbol"]]
        out["assets"].append(entry)
        print(f"{name}.png: {image.shape[1]}x{image.shape[0]}")
    (ROOT / NINTENDO_LIST).write_text(json.dumps(out, indent=1) + "\n")


def draw(arguments) -> None:
    sys.path.insert(0, str(ROOT / "tools"))
    import button_assets as tool  # noqa: E402 (upstream's map reader and helpers)
    from PIL import Image

    maps = {"ui": tool.XboxMap(Path(arguments.map)), "game": tool.XboxMap(Path(arguments.game_map))}
    for asset in assets():
        if asset.get("own") or asset["family"] == FAMILY_NINTENDO:
            continue  # (AE's own pictures: tools/ae_key_pictures.py draws them)
        xbox_map = maps["ui"] if ("bitm", asset["tag"]) in maps["ui"].tags else maps["game"]
        if ("bitm", asset["tag"]) not in xbox_map.tags:
            sys.exit(f"{asset['tag']}: in neither map")
        group = xbox_map.bitmap_group(asset["tag"])
        bitmap = group["bitmaps"][asset["bitmap"]]
        alpha = tool.decode_bitmap(bitmap)[..., 3]
        if "sprites" in asset:
            circles = []
            for sprite in asset["sprites"]:
                left, top, right, bottom = sprite["cell"]
                area, x, y = tool.shape(alpha[top:bottom, left:right])
                circles.append((left + x, top + y, (area / 3.141592653589793) ** 0.5,
                                SEQUENCE_SYMBOLS[sprite["sequence"]]))
            image = draw_stencils(asset["scale"], asset["width"], asset["height"], circles)
        else:
            import numpy as np
            area, x, y = tool.shape(alpha)
            radius = (area / 3.141592653589793) ** 0.5
            # (the disc whole inside the Xbox art's own box with a texel to spare, rim included: the game draws a little less than
            # bitmap, and a disc of the art's area ran past it and was drawn cut flat, owner 10-15)
            columns = np.nonzero(alpha.max(axis=0) > 8)[0]
            rows = np.nonzero(alpha.max(axis=1) > 8)[0]
            x0, x1, y0, y1 = columns[0], columns[-1] + 1, rows[0], rows[-1] + 1
            x, y = (x0 + x1) / 2.0, (y0 + y1) / 2.0
            radius = min(radius, ((x1 - x0) / 2.0 - 1.5) / 1.04, ((y1 - y0) / 2.0 - 1.5) / 1.04)
            image = tool.bleed(draw_icon(asset["scale"], asset["width"], asset["height"], (x, y, radius),
                                         asset["symbol"]))
        Image.fromarray(image, "RGBA").save(ROOT / FOLDER / f"{asset['name']}.png", optimize=True)
        print(f"{asset['name']}.png: {image.shape[1]}x{image.shape[0]}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    command = commands.add_parser("draw", help="draw the textures again from the Xbox maps")
    command.add_argument("--map", required=True, help="the Xbox ui.map")
    command.add_argument("--game-map", required=True, help="an Xbox multiplayer map (the in-game message icons)")
    commands.add_parser("nintendo", help="draw the Nintendo textures from the PlayStation ones (no maps needed)")
    arguments = parser.parse_args()
    if arguments.command == "draw":
        draw(arguments)
    elif arguments.command == "nintendo":
        draw_nintendo()


if __name__ == "__main__":
    main()
