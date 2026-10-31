"""A bitmap tag's multipurpose map from Halo PC's channel order to the Xbox's, for a map built for the Xbox.

Halo PC: red auxiliary, green self-illumination, blue specular, alpha colour change. The Xbox: red specular, green
self-illumination, blue colour change, alpha auxiliary. A PC multipurpose map in an Xbox map gives a model no
specular at all (the engine's Custom Edition loader reorders them, ce_models.c; a map built for the Xbox is not
reordered by anyone). Nothing is decoded and encoded again:
- DXT1: each block's endpoint colours (r, g, b) become (b, g, 0), exact in 5:6:5 (no colour change; the one-bit
  alpha stays the auxiliary mask); the endpoints' order, which says what kind of block it is, is kept.
- DXT5: the colour block the same (always four colours, so the order is free); the alpha block becomes 255
  everywhere (the Xbox's auxiliary mask: full).
- 32-bit: the channels moved.
With "meter" (32-bit only): a Halo PC HUD meter (shape in the colour, fill order in the alpha) to the Xbox's (fill
order in the colour, shape in the alpha), as the engine's ce_hud.c samples them.

Usage: multipurpose_to_xbox.py <source tag> <destination tag> [meter]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek


def blue_to_red(colour):
    _, green, blue = hek.colour_565(colour)
    return hek.pack_565(blue, green, 0)


def convert(tag, meter=False):
    """(the converted tag, {"bitmaps", "blocks", "flipped", "bytes"})"""
    data = bytearray(tag)
    stats = {"bitmaps": hek.BitmapTag(tag).bitmap_count, "blocks": 0, "flipped": 0}

    def level(bitmap, position, width, height):
        if meter and bitmap.format not in (hek.FORMAT_X8R8G8B8, hek.FORMAT_A8R8G8B8):
            raise hek.Refused("the meter conversion takes 32-bit bitmaps only")
        if bitmap.format == hek.FORMAT_DXT1:
            count = hek.block_count(width, height)
            for index in range(count):
                offset = position + index * 8
                c0, c1, indices = struct.unpack_from("<HHI", data, offset)
                c0, c1, indices, flipped = hek.dxt1_block_recolour(c0, c1, indices, blue_to_red)
                stats["flipped"] += flipped
                struct.pack_into("<HHI", data, offset, c0, c1, indices)
            stats["blocks"] += count
            return count * 8
        if bitmap.format == hek.FORMAT_DXT5:
            count = hek.block_count(width, height)
            for index in range(count):
                offset = position + index * 16
                data[offset:offset + 8] = bytes((255, 255, 0, 0, 0, 0, 0, 0))
                c0, c1, indices = struct.unpack_from("<HHI", data, offset + 8)
                struct.pack_into("<HHI", data, offset + 8, blue_to_red(c0), blue_to_red(c1), indices)
            stats["blocks"] += count
            return count * 16
        if bitmap.format in (hek.FORMAT_X8R8G8B8, hek.FORMAT_A8R8G8B8):
            for index in range(width * height):
                offset = position + index * 4
                blue, green, red, alpha = data[offset:offset + 4]
                if meter:
                    data[offset:offset + 4] = bytes((alpha, alpha, alpha, red))    # colour = alpha, alpha = red
                else:
                    data[offset:offset + 4] = bytes((alpha, green, blue, red))     # blue = alpha, red = blue,
            return width * height * 4                                               # alpha = red
        raise hek.Refused("format %d is not converted" % bitmap.format)

    stats["bytes"] = hek.for_each_level(data, level)
    return bytes(data), stats


def main(arguments):
    if len(arguments) not in (2, 3) or (len(arguments) == 3 and arguments[2] != "meter"):
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    try:
        with open(arguments[0], "rb") as file:
            out, stats = convert(file.read(), meter=len(arguments) == 3)
    except hek.Refused as refusal:
        print("multipurpose_to_xbox: refused: %s" % refusal, file=sys.stderr)
        return 1
    with open(arguments[1], "wb") as file:
        file.write(out)
    print("converted %(bitmaps)d bitmaps, %(blocks)d DXT blocks (%(flipped)d endpoint orders put right), "
          "%(bytes)d pixel bytes" % stats)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
