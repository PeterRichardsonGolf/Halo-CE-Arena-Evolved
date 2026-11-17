#!/usr/bin/env python3
"""Arena Evolved's own medal pictures: the 31 medals MCC has no picture for (port/assets/medals, 80 by 80, 8-bit RGBA).

port/linux/game/ae_medal_pack.c draws them, in the MCC medal style and the UNIVERSAL one, for a player whose MCC
install is read and has no picture of its own for the medal (the game draws the 56 pixel size from these by bilinear
filtering). A file's name is the medal's key in port/linux/game/ae_medal_ids.h without its "medal_" (three_sixty is
"medal_360").

tools/embed_assets.py embeds them through its AE hooks (inputs, configure_inputs, embedded_lines): the generated
source gets ae_medal_art[] (port/linux/src/ae_medal_art.h) with each picture as raw RGBA, whatever the folder has
(none: an empty table). The credits of the parts are in port/assets/medals/NOTICE.md and AUTHORS.md.
"""

import struct
import sys
import zlib
from pathlib import Path
from typing import Callable, List

ROOT = Path(__file__).resolve().parent.parent
FOLDER = Path("port/assets/medals")
PICTURE = 80
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def files() -> List[Path]:
    """the pictures' files, by name"""
    return sorted((ROOT / FOLDER).glob("*.png")) if (ROOT / FOLDER).is_dir() else []


def rgba(data: bytes, name: str) -> bytes:
    """The pixels of an 8-bit RGBA, non-interlaced PNG, rows top first, PICTURE across and down."""
    if data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        sys.exit(f"{name}: not a PNG")
    width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", data[16:29])
    if depth != 8 or colour != 6 or interlace != 0:
        sys.exit(f"{name}: must be 8-bit RGBA and not interlaced")
    if (width, height) != (PICTURE, PICTURE):
        sys.exit(f"{name}: {width}x{height}, not {PICTURE}x{PICTURE}")
    joined = b""
    position = 8
    while position + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        if kind == b"IDAT":
            joined += data[position + 8:position + 8 + length]
        position += 12 + length
    raw = zlib.decompress(joined)
    stride = width * 4
    if len(raw) != height * (stride + 1):
        sys.exit(f"{name}: its data is not {height} rows")
    out = bytearray()
    previous = bytearray(stride)
    for row in range(height):
        start = row * (stride + 1)
        filter_type = raw[start]
        line = bytearray(raw[start + 1:start + 1 + stride])
        for column in range(stride):
            left = line[column - 4] if column >= 4 else 0
            up = previous[column]
            up_left = previous[column - 4] if column >= 4 else 0
            if filter_type == 1:
                line[column] = (line[column] + left) & 255
            elif filter_type == 2:
                line[column] = (line[column] + up) & 255
            elif filter_type == 3:
                line[column] = (line[column] + (left + up) // 2) & 255
            elif filter_type == 4:
                estimate = left + up - up_left
                near_left, near_up, near_corner = abs(estimate - left), abs(estimate - up), abs(estimate - up_left)
                if near_left <= near_up and near_left <= near_corner:
                    predictor = left
                elif near_up <= near_corner:
                    predictor = up
                else:
                    predictor = up_left
                line[column] = (line[column] + predictor) & 255
            elif filter_type != 0:
                sys.exit(f"{name}: unknown filter {filter_type}")
        out += line
        previous = line
    return bytes(out)


# ---------- tools/embed_assets.py's AE hooks


def inputs() -> List[Path]:
    """The files the embedded table is made from."""
    return [path.relative_to(ROOT) for path in files()]


def configure_inputs() -> List[Path]:
    """What configure.py is run again for (the folder, for files added or removed)."""
    return [FOLDER] if (ROOT / FOLDER).is_dir() else []


def embedded_lines(words: Callable, png_size: Callable) -> List[str]:
    """ae_medal_art[] and its count, as port/linux/src/ae_medal_art.h declares them."""
    lines = ['#include "ae_medal_art.h"', ""]
    table = []
    for index, path in enumerate(files()):
        pixels = rgba(path.read_bytes(), path.name)
        lines.append(f"static const unsigned int ae_medal_art{index}[] = {{")
        lines.extend(words(pixels))
        lines.append("};")
        lines.append("")
        table.append(f'\t{{ "{path.stem}", ae_medal_art{index}, {PICTURE}, {PICTURE} }},')
    lines.append("const struct ae_medal_art ae_medal_art[] =")
    lines.append("{")
    lines.extend(table if table else ["\t{ 0 },"])
    lines.append("};")
    lines.append(f"const unsigned int ae_medal_art_count = {len(table)};")
    lines.append("")
    return lines
