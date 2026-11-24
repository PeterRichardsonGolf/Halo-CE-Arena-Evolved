"""Tints a bitmap tag's DXT1 colour without decoding it: each block's two endpoint colours become the tint at their own
brightness (0.299 R + 0.587 G + 0.114 B, times a gain, at most 1); the texels' indices stay, so the art's detail is as
it was. The endpoints' order, which says what kind of block it is, is kept. A display's digits made amber, say.

Usage: dxt_tint.py <source tag> <destination tag> <red> <green> <blue> <gain>   (the tint's channels 0-1)
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek


def tinted(colour, tint, gain):
    """a 565 colour as the tint at its brightness"""
    red, green, blue = hek.colour_565(colour)
    light = min(1.0, (0.299 * red / 31 + 0.587 * green / 63 + 0.114 * blue / 31) * gain)
    return hek.pack_565(int(round(tint[0] * light * 31)), int(round(tint[1] * light * 63)),
                        int(round(tint[2] * light * 31)))


def tint(tag, colour, gain):
    """(the tinted tag, {"bitmaps", "blocks", "flipped"})"""
    if len(colour) != 3 or not all(0.0 <= value <= 1.0 for value in colour) or not 0.0 < gain <= 4.0:
        raise hek.Refused("a tint is three channels 0-1 and a gain above 0, at most 4")
    data = bytearray(tag)
    stats = {"bitmaps": hek.BitmapTag(tag).bitmap_count, "blocks": 0, "flipped": 0}

    def level(bitmap, position, width, height):
        if bitmap.format != hek.FORMAT_DXT1:
            raise hek.Refused("format %d is not tinted: DXT1 only" % bitmap.format)
        count = hek.block_count(width, height)
        for index in range(count):
            offset = position + index * 8
            c0, c1, indices = struct.unpack_from("<HHI", data, offset)
            c0, c1, indices, flipped = hek.dxt1_block_recolour(c0, c1, indices, lambda c: tinted(c, colour, gain))
            stats["flipped"] += flipped
            struct.pack_into("<HHI", data, offset, c0, c1, indices)
        stats["blocks"] += count
        return count * 8

    hek.for_each_level(data, level)
    return bytes(data), stats


def main(arguments):
    if len(arguments) != 6:
        print(__doc__.strip().split("\n")[-1].strip())
        return 2
    with open(arguments[0], "rb") as file:
        tag = file.read()
    out, stats = tint(tag, [float(value) for value in arguments[2:5]], float(arguments[5]))
    with open(arguments[1], "wb") as file:
        file.write(out)
    print("%d bitmaps, %d blocks, %d flipped" % (stats["bitmaps"], stats["blocks"], stats["flipped"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
