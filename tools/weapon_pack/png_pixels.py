"""A PNG file's pixels, read by our own small reader (8 bits a channel, grey, RGB or RGBA, with or without alpha,
not interlaced: what the art scripts write), so that the recipe can pin art by WHAT IS DRAWN: the SHA-256 of the
decoded pixel bytes with the picture's mode and size. Two files of the same pixels pin alike whatever library,
version or compression level wrote them; one differing pixel pins differently.

Usage: png_pixels.py <png>...      prints each file's mode, size and pixel SHA-256
"""
import hashlib
import struct
import sys
import zlib


class Refused(Exception):
    """not a PNG this reads, with the reason"""


SIGNATURE = b"\x89PNG\r\n\x1a\n"
MODES = {0: ("L", 1), 2: ("RGB", 3), 4: ("LA", 2), 6: ("RGBA", 4)}
MAXIMUM_PIXELS = 64 * 1024 * 1024


def read(data):
    """(mode, width, height, the pixel bytes row by row)"""
    if data[:8] != SIGNATURE:
        raise Refused("not a PNG")
    position = 8
    header = None
    packed = []
    while position + 8 <= len(data):
        length, kind = struct.unpack_from(">I4s", data, position)
        body = data[position + 8:position + 8 + length]
        if len(body) != length:
            raise Refused("a chunk past the file's end")
        position += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            packed.append(body)
        elif kind == b"IEND":
            break
    if header is None:
        raise Refused("no header")
    width, height, depth, colour, compression, filtering, interlace = header
    if depth != 8 or colour not in MODES or compression or filtering or interlace:
        raise Refused("not an 8-bit grey, RGB or RGBA picture without interlace")
    if not 0 < width * height <= MAXIMUM_PIXELS:
        raise Refused("a picture of %d x %d" % (width, height))
    mode, channels = MODES[colour]
    stride = width * channels
    try:
        raw = zlib.decompress(b"".join(packed))
    except zlib.error:
        raise Refused("its pixel data does not inflate")
    if len(raw) != height * (stride + 1):
        raise Refused("its pixel data is not its size")
    out = bytearray(height * stride)
    previous = bytes(stride)
    for row in range(height):
        start = row * (stride + 1)
        kind = raw[start]
        line = bytearray(raw[start + 1:start + 1 + stride])
        if kind == 1:
            for index in range(channels, stride):
                line[index] = (line[index] + line[index - channels]) & 255
        elif kind == 2:
            for index in range(stride):
                line[index] = (line[index] + previous[index]) & 255
        elif kind == 3:
            for index in range(stride):
                left = line[index - channels] if index >= channels else 0
                line[index] = (line[index] + ((left + previous[index]) >> 1)) & 255
        elif kind == 4:
            for index in range(stride):
                left = line[index - channels] if index >= channels else 0
                above = previous[index]
                corner = previous[index - channels] if index >= channels else 0
                estimate = left + above - corner
                a, b, c = abs(estimate - left), abs(estimate - above), abs(estimate - corner)
                line[index] = (line[index] + (left if a <= b and a <= c else above if b <= c else corner)) & 255
        elif kind != 0:
            raise Refused("a row with filter %d" % kind)
        out[row * stride:(row + 1) * stride] = line
        previous = line
    return mode, width, height, bytes(out)


def pin(path):
    """what the recipe records of an art file: {"mode", "size": [width, height], "pixels_sha256"}"""
    with open(path, "rb") as file:
        mode, width, height, pixels = read(file.read())
    return {"mode": mode, "size": [width, height], "pixels_sha256": hashlib.sha256(pixels).hexdigest()}


def main(arguments):
    if not arguments:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    for path in arguments:
        try:
            found = pin(path)
        except Refused as refusal:
            print("png_pixels: refused: %s: %s" % (path, refusal), file=sys.stderr)
            return 1
        print("%s %dx%d %s  %s" % (found["mode"], found["size"][0], found["size"][1], found["pixels_sha256"], path))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
