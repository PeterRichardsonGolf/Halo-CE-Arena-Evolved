#!/usr/bin/env python3
"""Which of the high-res HUD's textures (port/assets/hud, laid out as the Xbox maps' HUD bitmaps) may also stand
for the Halo PC HUD bitmaps a Custom Edition map draws from its resource map (bitmaps.map), and the CRC-32 of
each one's first mip level there: writes port/linux/src/ae_ce_hud_table.c (numbers and tag names only), which
port/linux/src/ae_ce_hud.c reads.

    python tools/ae_ce_hud.py --map <an Xbox bloodgulch.map> --bitmaps <Custom Edition bitmaps.map>

Halo PC keeps the same HUD art as the Xbox, mostly at the Xbox's size and in its layout, but in other formats
(a8r8g8b8 or DXT where the Xbox has ay8 or a8y8), so its pixels are not the ones a texture's CRC names. A texture
stands for a PC bitmap when:
- it is not a meter's (a Custom Edition map's meters are sampled in Halo PC's channel order: ce_hud.c);
- the PC bitmap is the Xbox's size, or a whole multiple of it the same both ways, and the texture is a whole
  multiple of the PC bitmap's size larger than it (hud_hires_asset_fits);
- each sprite on the bitmap has the same place in it, as fractions of its size (the PC sheets may have more
  sequences: only their sprites on this bitmap count, in any order);
- the PC bitmap's texels, decoded as the game draws them and reduced to the Xbox's size, are the Xbox bitmap's
  (its colour premultiplied by its alpha, as the HUD blends: within TOLERANCE on average).
A PC bitmap whose pixels are the Xbox's (the same format) needs no row: the texture's own CRC names it. Anything
else keeps the map's own bitmap. The CRCs are of the bitmaps.map it reads (MCC's custom_edition one, which the
players' installs use): a copy whose pixels differ (another release's) keeps the map's own HUD, as a modified map
does. Run it again when layout.json changes. Needs NumPy and Pillow.
"""

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hud_assets  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
LAYOUT = ROOT / "port/assets/hud/layout.json"
OUTPUT = ROOT / "port/linux/src/ae_ce_hud_table.c"
# the mean difference (of 255) allowed between the premultiplied texels of the PC bitmap, reduced to the Xbox's size,
# and the Xbox's: the same art compressed otherwise differs by about one level (Halo PC's larger unit backgrounds,
# drawn again and placed otherwise in their sheet, by 3 and more)
TOLERANCE = 2.0
# how far (as a fraction of the bitmap) a sprite's edge may sit from the Xbox's
SPRITE_TOLERANCE = 0.002


class ResourceMap:
    """A Custom Edition bitmaps.map: its resources by path (a bitmap group's tag data, with its pointers relative to
    its start, and before it <path>__pixels); a bitmap's pixels offset is the file's (ce_resources.c)."""

    def __init__(self, path: Path):
        self.data = path.read_bytes()
        kind, paths, resources, count = struct.unpack_from("<4I", self.data, 0)
        if kind != 1:
            sys.exit(f"{path}: not a bitmaps resource map (type {kind})")
        self.resources = {}
        for index in range(count):
            name_offset, size, offset = struct.unpack_from("<3I", self.data, resources + index * 12)
            start = paths + name_offset
            name = self.data[start:self.data.index(b"\0", start)].decode("latin-1")
            self.resources[name] = (offset, size)

    def bitmap_group(self, name: str):
        if name not in self.resources:
            return None
        offset, size = self.resources[name]
        tag = self.data[offset:offset + size]
        sequence_count, sequences = struct.unpack_from("<II", tag, 0x54)
        bitmap_count, bitmaps = struct.unpack_from("<II", tag, 0x60)
        group = {"bitmaps": [], "sequences": []}
        for index in range(bitmap_count):
            bitmap = tag[bitmaps + index * 0x30:bitmaps + (index + 1) * 0x30]
            width, height, _, _, format, flags = struct.unpack_from("<hhhhhH", bitmap, 4)
            pixels, pixels_size = struct.unpack_from("<ii", bitmap, 0x18)
            group["bitmaps"].append({"width": width, "height": height, "format": format,
                                     # (Halo PC's textures are not swizzled)
                                     "flags": flags | hud_assets.BITMAP_LINEAR_FLAG,
                                     "pixels": self.data[pixels:pixels + pixels_size]})
        for index in range(sequence_count):
            sequence = tag[sequences + index * 0x40:sequences + (index + 1) * 0x40]
            sprite_count, sprites = struct.unpack_from("<II", sequence, 0x34)
            group["sequences"].append([hud_assets.sprite_from(tag[sprites + j * 0x20:sprites + (j + 1) * 0x20], "<")
                                       for j in range(sprite_count)])
        return group


def sprites_on(group: dict, bitmap: int) -> list:
    return sorted(tuple(sprite[1:]) for sequence in group["sequences"] for sprite in sequence if sprite[0] == bitmap)


def premultiplied(image: np.ndarray) -> np.ndarray:
    image = image.astype(np.float64)
    return np.concatenate([image[..., :3] * image[..., 3:] / 255.0, image[..., 3:]], axis=-1)


def reduced(image: np.ndarray, factor: int) -> np.ndarray:
    if factor == 1:
        return image
    height, width = image.shape[0] // factor, image.shape[1] // factor
    return image.reshape(height, factor, width, factor, -1).mean(axis=(1, 3))


def verdict(asset: dict, xbox: dict, pc: dict):
    """(None, (crc, difference)) when the texture stands for the PC bitmap, else (why not, None)"""
    # (a texture drawn for some of a bitmap's sprites, through a placeholder: hud_hires_sprites_drawable takes no
    # Custom Edition rows; layout.json has none, the buttons' sheet is port/assets/buttons')
    if asset.get("sprites"):
        return "drawn for some of its sprites", None
    if any(cell.get("kind") == "meter" or cell.get("thresholds") for cell in asset.get("cells", [])):
        return "a meter's (sampled in Halo PC's channels)", None
    index = asset["bitmap"]
    if index >= len(pc["bitmaps"]):
        return "no such bitmap there", None
    x, p = xbox["bitmaps"][index], pc["bitmaps"][index]
    if p["format"] not in hud_assets.FORMATS:
        return f"format {p['format']}", None
    factor = p["width"] // x["width"]
    if (factor < 1 or p["width"] != x["width"] * factor or p["height"] != x["height"] * factor):
        return f"{p['width']}x{p['height']}, not a multiple of the Xbox's {x['width']}x{x['height']}", None
    texture = (asset["width"] * asset["scale"], asset["height"] * asset["scale"])
    if texture[0] % p["width"] or texture[0] // p["width"] < 2 or texture[0] // p["width"] != texture[1] // p["height"]:
        return f"{p['width']}x{p['height']}, which its texture does not fit", None
    xs, ps = sprites_on(xbox, index), sprites_on(pc, index)
    if len(xs) != len(ps) or any(max(abs(a - b) for a, b in zip(s, t)) > SPRITE_TOLERANCE for s, t in zip(xs, ps)):
        return f"its sprites are placed otherwise ({len(ps)} there, {len(xs)} on the Xbox's)", None
    size = hud_assets.level0_size(p)
    if len(p["pixels"]) < size:
        return "its pixels are not all there", None
    difference = np.abs(premultiplied(reduced(hud_assets.decode_bitmap(p).astype(np.float64), factor)) -
                        premultiplied(hud_assets.decode_bitmap(x))).mean()
    if difference > TOLERANCE:
        return f"other art (differs by {difference:.1f} of 255)", None
    return None, (zlib.crc32(p["pixels"][:size]) & 0xFFFFFFFF, difference)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--map", type=Path, required=True, help="an Xbox map with the HUD (bloodgulch.map)")
    parser.add_argument("--bitmaps", type=Path, required=True, help="Custom Edition's bitmaps.map")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    arguments = parser.parse_args()
    xbox_map, resources = hud_assets.XboxMap(arguments.map), ResourceMap(arguments.bitmaps)
    rows = []
    for asset in json.loads(LAYOUT.read_text())["assets"]:
        pc = resources.bitmap_group(asset["tag"])
        if not pc:
            print(f"{asset['name']}: not in {arguments.bitmaps.name}")
            continue
        why, crc = verdict(asset, xbox_map.bitmap_group(asset["tag"]), pc)
        if crc is not None and crc[0] == asset["crc"]:
            why, crc = "the Xbox's own pixels (its texture's CRC names them)", None
        print(f"{asset['name']}: " + (why or f"stands for it (differs by {crc[1]:.1f}; crc 0x{crc[0]:08x})"))
        if crc is not None:
            crc = crc[0]
            tag = asset["tag"].replace("\\", "\\\\")
            rows.append(f'\t{{ "{tag}", {asset["bitmap"]}, 0x{crc:08x}u }},')
    lines = [
        "/* generated by tools/ae_ce_hud.py from port/assets/hud/layout.json and Custom Edition's bitmaps.map:",
        "do not edit. The Halo PC HUD bitmaps (a Custom Edition map's, from bitmaps.map) the high-res HUD's",
        "textures stand for: their bitmap group, their index there, and the CRC-32 of their first mip level */",
        "",
        '#include "ae_ce_hud.h"',
        "",
        "const struct ae_ce_hud_bitmap ae_ce_hud_bitmaps[] =",
        "{",
        *(rows or ["\t{ 0 },"]),
        "};",
        f"const unsigned int ae_ce_hud_bitmap_count = {len(rows)};",
        "",
    ]
    arguments.output.write_text("\n".join(lines))
    print(f"{len(rows)} bitmaps written to {arguments.output}")


if __name__ == "__main__":
    main()
