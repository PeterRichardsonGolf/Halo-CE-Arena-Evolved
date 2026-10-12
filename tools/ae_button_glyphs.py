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

# each symbol's colour, as PlayStation controllers have them (DamnationCE's)
COLOURS = {"cross": (110, 156, 226), "circle": (232, 86, 86), "square": (216, 132, 198), "triangle": (66, 212, 170)}
# the message icons' sequences 0 to 3 are A, B, X and Y
SEQUENCE_SYMBOLS = {0: "cross", 1: "circle", 2: "square", 3: "triangle"}
# drawn at this many times the texture's size, then reduced (its edges)
SUPERSAMPLE = 4


def assets() -> List[dict]:
    if not (ROOT / LIST).is_file():
        return []
    return json.loads((ROOT / LIST).read_text())["assets"]


# ---------- tools/embed_assets.py's AE hooks


def inputs() -> List[Path]:
    """The files the embedded table is made from."""
    return ([LIST] if (ROOT / LIST).is_file() else []) + [FOLDER / f"{asset['name']}.png" for asset in assets()]


def configure_inputs() -> List[Path]:
    """What configure.py is run again for (the list, and the folder for files added or removed)."""
    return [FOLDER, LIST] if (ROOT / LIST).is_file() else []


def embedded_lines(words: Callable, png_size: Callable) -> List[str]:
    """ae_button_glyphs[] and its count, as port/linux/src/ae_buttons.h declares them."""
    lines = ['#include "ae_buttons.h"', ""]
    table = []
    for index, asset in enumerate(assets()):
        name = f"{asset['name']}.png"
        data = (ROOT / FOLDER / name).read_bytes()
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
                     f'0x{asset["crc"]:08x}u, 0x{sprites:x}u, ae_button_glyph{index}, {len(data)} }},')
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


def draw(arguments) -> None:
    sys.path.insert(0, str(ROOT / "tools"))
    import button_assets as tool  # noqa: E402 (upstream's map reader and helpers)
    from PIL import Image

    maps = {"ui": tool.XboxMap(Path(arguments.map)), "game": tool.XboxMap(Path(arguments.game_map))}
    for asset in assets():
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
            area, x, y = tool.shape(alpha)
            image = tool.bleed(draw_icon(asset["scale"], asset["width"], asset["height"],
                                         (x, y, (area / 3.141592653589793) ** 0.5), asset["symbol"]))
        Image.fromarray(image, "RGBA").save(ROOT / FOLDER / f"{asset['name']}.png", optimize=True)
        print(f"{asset['name']}.png: {image.shape[1]}x{image.shape[0]}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    command = commands.add_parser("draw", help="draw the textures again from the Xbox maps")
    command.add_argument("--map", required=True, help="the Xbox ui.map")
    command.add_argument("--game-map", required=True, help="an Xbox multiplayer map (the in-game message icons)")
    arguments = parser.parse_args()
    if arguments.command == "draw":
        draw(arguments)


if __name__ == "__main__":
    main()
