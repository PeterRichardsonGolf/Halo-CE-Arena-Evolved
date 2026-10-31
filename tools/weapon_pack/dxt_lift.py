"""Brightens a bitmap tag's DXT1 colour without decoding it: each block's two endpoint colours go through
x -> 1 - (1 - x)^lift a channel (dark values rise most, white stays white); the texels' indices stay, so the art's
detail is as it was. The endpoints' order, which says what kind of block it is, is kept.

Usage: dxt_lift.py <source tag> <destination tag> <lift>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek


def lifted(value, top, lift):
    """a channel value 0..top through the curve"""
    return int(round((1 - (1 - value / top) ** lift) * top))


def lift(tag, amount):
    """(the brightened tag, {"bitmaps", "blocks", "flipped", "bytes"})"""
    data = bytearray(tag)
    stats = {"bitmaps": hek.BitmapTag(tag).bitmap_count, "blocks": 0, "flipped": 0}

    def recolour(colour):
        red, green, blue = hek.colour_565(colour)
        return hek.pack_565(lifted(red, 31, amount), lifted(green, 63, amount), lifted(blue, 31, amount))

    def level(bitmap, position, width, height):
        if bitmap.format != hek.FORMAT_DXT1:
            raise hek.Refused("format %d is not brightened: DXT1 only" % bitmap.format)
        count = hek.block_count(width, height)
        for index in range(count):
            offset = position + index * 8
            c0, c1, indices = struct.unpack_from("<HHI", data, offset)
            c0, c1, indices, flipped = hek.dxt1_block_recolour(c0, c1, indices, recolour)
            stats["flipped"] += flipped
            struct.pack_into("<HHI", data, offset, c0, c1, indices)
        stats["blocks"] += count
        return count * 8

    stats["bytes"] = hek.for_each_level(data, level)
    return bytes(data), stats


def main(arguments):
    if len(arguments) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    try:
        with open(arguments[0], "rb") as file:
            out, stats = lift(file.read(), float(arguments[2]))
    except hek.Refused as refusal:
        print("dxt_lift: refused: %s" % refusal, file=sys.stderr)
        return 1
    with open(arguments[1], "wb") as file:
        file.write(out)
    print("brightened %(bitmaps)d bitmaps, %(blocks)d DXT1 blocks (%(flipped)d endpoint orders put right), "
          "%(bytes)d pixel bytes" % stats)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
