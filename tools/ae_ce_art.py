#!/usr/bin/env python3
"""Content Evolved's skull and medal pictures, for a player without MCC (96 by 96, 8-bit RGBA).

port/assets/skulls holds one picture for each skull, named by its key in port/linux/include/halo_skulls.h's skull_descriptions;
port/assets/ce_medals one for each campaign medal, named by its key in campaign_medals.c's campaign_medal_descriptions. The skulls
screen and the end-of-mission card draw them where the MCC install read has no picture of its own.

tools/embed_assets.py embeds them through its AE hooks (inputs, configure_inputs, embedded_lines): the generated source gets
ae_ce_art[] (port/linux/src/ae_ce_art.h) with each picture as raw RGBA, whatever the folders have (none: an empty table); a
medal's entry is named "medal_" and its key. The pictures are Content Evolved's (port/assets/credits.txt).
"""

import struct
import sys
import zlib
from pathlib import Path
from typing import Callable, List

ROOT = Path(__file__).resolve().parent.parent
FOLDER = Path("port/assets/skulls")
MEDAL_FOLDER = Path("port/assets/ce_medals")
PICTURE = 96
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def files() -> List[Path]:
    """the pictures' files: the skulls by name, then the medals by name"""
    found: List[Path] = []
    for folder in (FOLDER, MEDAL_FOLDER):
        if (ROOT / folder).is_dir():
            found += sorted((ROOT / folder).glob("*.png"))
    return found


def picture_name(path: Path) -> str:
    """a skull's key, or "medal_" and a medal's key"""
    return ("medal_" if path.parent.name == MEDAL_FOLDER.name else "") + path.stem


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
    return [folder for folder in (FOLDER, MEDAL_FOLDER) if (ROOT / folder).is_dir()]


def embedded_lines(words: Callable, png_size: Callable) -> List[str]:
    """ae_ce_art[] and its count, as port/linux/src/ae_ce_art.h declares them."""
    lines = ['#include "ae_ce_art.h"', ""]
    table = []
    for index, path in enumerate(files()):
        pixels = rgba(path.read_bytes(), path.name)
        lines.append(f"static const unsigned int ae_ce_art{index}[] = {{")
        lines.extend(words(pixels))
        lines.append("};")
        lines.append("")
        table.append(f'\t{{ "{picture_name(path)}", ae_ce_art{index}, {PICTURE}, {PICTURE} }},')
    lines.append("const struct ae_ce_art ae_ce_art[] =")
    lines.append("{")
    lines.extend(table if table else ["\t{ 0 },"])
    lines.append("};")
    lines.append(f"const unsigned int ae_ce_art_count = {len(table)};")
    lines.append("")
    return lines
