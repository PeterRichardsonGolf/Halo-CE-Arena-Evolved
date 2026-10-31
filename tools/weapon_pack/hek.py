"""Halo Editing Kit tag files as the weapon pack recipe's own scripts read them (our own readers; no tool of the
kit or of anyone else is used or copied): the 64-byte file header, a bitmap tag's pixels and the DXT blocks in them.

A tag file is big-endian: a 0x40-byte header (the group's four characters at 0x24), the tag's root struct, then
every block's elements and every data's bytes in the order the fields name them.
"""
import struct

HEADER_SIZE = 0x40

# a bitmap tag's root: the sizes this module needs (bitmap_group, 108 bytes in the file)
BITMAP_ROOT_SIZE = 108
BITMAP_PLATE_SIZE = 0x1C        # compressed colour plate data: its size
BITMAP_PIXELS_SIZE = 0x30       # processed pixel data: its size
BITMAP_SEQUENCE_COUNT = 0x54
BITMAP_COUNT = 0x60
BITMAP_SEQUENCE_SIZE = 64
BITMAP_SPRITE_SIZE = 32
BITMAP_DATA_SIZE = 48

FORMAT_A8Y8 = 3
FORMAT_X8R8G8B8 = 10
FORMAT_A8R8G8B8 = 11
FORMAT_DXT1 = 14
FORMAT_DXT3 = 15
FORMAT_DXT5 = 16


class Refused(Exception):
    """a tag file this module will not work on, with the reason"""


def group(tag):
    """the four characters of the tag file's group"""
    if len(tag) < HEADER_SIZE:
        raise Refused("not a tag file: %d bytes" % len(tag))
    return bytes(tag[0x24:0x28])


class Bitmap:
    """one bitmap of a bitmap tag: where its 48-byte record is, and what it says"""

    def __init__(self, tag, entry):
        self.entry = entry
        if tag[entry:entry + 4] != b"bitm":
            raise Refused("not a bitmap record at %d" % entry)
        self.width, self.height, self.depth, self.type, self.format, self.flags = struct.unpack_from(
            ">HHHHHH", tag, entry + 4)
        self.mipmap_count = struct.unpack_from(">H", tag, entry + 0x14)[0]
        self.pixels_offset = struct.unpack_from(">I", tag, entry + 0x18)[0]


class BitmapTag:
    """a bitmap tag file's layout: the pixel bytes and the bitmap records (which are the file's last bytes)"""

    def __init__(self, tag):
        if group(tag) != b"bitm":
            raise Refused("not a bitmap tag: %r" % group(tag))
        if len(tag) < HEADER_SIZE + BITMAP_ROOT_SIZE:
            raise Refused("a bitmap tag of %d bytes" % len(tag))
        self.plate_size = struct.unpack_from(">i", tag, HEADER_SIZE + BITMAP_PLATE_SIZE)[0]
        self.pixels_size = struct.unpack_from(">i", tag, HEADER_SIZE + BITMAP_PIXELS_SIZE)[0]
        self.sequence_count = struct.unpack_from(">i", tag, HEADER_SIZE + BITMAP_SEQUENCE_COUNT)[0]
        self.bitmap_count = struct.unpack_from(">i", tag, HEADER_SIZE + BITMAP_COUNT)[0]
        if self.plate_size < 0 or self.pixels_size < 0 or self.sequence_count < 0 or self.bitmap_count < 1:
            raise Refused("a bitmap tag with no bitmaps or a negative size")
        self.pixels_start = HEADER_SIZE + BITMAP_ROOT_SIZE + self.plate_size
        records = len(tag) - self.bitmap_count * BITMAP_DATA_SIZE
        if self.pixels_start + self.pixels_size > records:
            raise Refused("a bitmap tag whose pixels do not fit the file")
        self.bitmaps = [Bitmap(tag, records + index * BITMAP_DATA_SIZE) for index in range(self.bitmap_count)]

    def pixels(self, tag):
        return bytes(tag[self.pixels_start:self.pixels_start + self.pixels_size])

    def first_sprite_offset(self):
        """where the first sprite's 32-byte record is: after the pixels and the sequences' records (the sprites of
        the sequences follow them in order, so this is sequence 0's first sprite when it has one)"""
        return self.pixels_start + self.pixels_size + self.sequence_count * BITMAP_SEQUENCE_SIZE


def block_count(width, height):
    return max(1, (width + 3) // 4) * max(1, (height + 3) // 4)


def for_each_level(tag, visit):
    """visit(bitmap, position, width, height) for every mipmap level of every 2D bitmap of the tag, position
    being the level's place in the pixel bytes; visit returns the level's size. Returns the bytes walked."""
    layout = BitmapTag(tag)
    position = 0
    for index, bitmap in enumerate(layout.bitmaps):
        if bitmap.depth != 1 or bitmap.type != 0:
            raise Refused("only 2D bitmaps are converted")
        if index:
            # (a sprite sheet's bitmaps each say where their pixels are)
            position = bitmap.pixels_offset
        width, height = bitmap.width, bitmap.height
        for _ in range(bitmap.mipmap_count + 1):
            size = visit(bitmap, layout.pixels_start + position, width, height)
            position += size
            if position > layout.pixels_size:
                raise Refused("a bitmap's pixels run past the tag's pixel data")
            width, height = max(1, width // 2), max(1, height // 2)
    return position


def colour_565(value):
    return (value >> 11) & 31, (value >> 5) & 63, value & 31


def pack_565(red, green, blue):
    return (red << 11) | (green << 5) | blue


def dxt1_block_recolour(c0, c1, indices, recolour):
    """a DXT1 block's two endpoint colours changed by recolour(565 value) -> 565 value with the block's meaning
    kept: the order of the endpoints says whether the block has four colours or three and a transparent one, so
    when the new colours change that order they are swapped and the indices with them. Returns (c0, c1, indices,
    whether the order was put right)."""
    four = c0 > c1
    n0, n1 = recolour(c0), recolour(c1)
    flipped = False
    if four and not n0 > n1:
        if n0 == n1:
            indices = 0          # all one colour
        else:
            n0, n1 = n1, n0      # endpoints swapped: indices 0 <-> 1, 2 <-> 3
            indices ^= 0x55555555
        flipped = True
    elif not four and n0 > n1:
        n0, n1 = n1, n0          # three colours: 0 <-> 1, 2 and 3 stay
        swapped = 0
        for texel in range(16):
            index = (indices >> (2 * texel)) & 3
            swapped |= (index ^ (1 if index < 2 else 0)) << (2 * texel)
        indices = swapped
        flipped = True
    return n0, n1, indices, flipped


def decode_rgba(pixels, width, height, pixel_format):
    """a DXT1 / DXT3 / DXT5 level as RGBA bytes, row by row (integer arithmetic only: the art scripts measure
    what this returns, so it must give the same numbers everywhere)"""
    if pixel_format not in (FORMAT_DXT1, FORMAT_DXT3, FORMAT_DXT5):
        raise Refused("format %d is not decoded" % pixel_format)
    out = bytearray(width * height * 4)
    position = 0

    def colour(value):
        red, green, blue = colour_565(value)
        return red * 255 // 31, green * 255 // 63, blue * 255 // 31

    for block_y in range(0, height, 4):
        for block_x in range(0, width, 4):
            alpha = None
            if pixel_format == FORMAT_DXT3:
                raw = pixels[position:position + 8]
                position += 8
                alpha = [((raw[index // 2] >> (4 * (index % 2))) & 15) * 17 for index in range(16)]
            elif pixel_format == FORMAT_DXT5:
                a0, a1 = pixels[position], pixels[position + 1]
                bits = int.from_bytes(pixels[position + 2:position + 8], "little")
                position += 8
                if a0 > a1:
                    table = [a0, a1] + [((6 - step) * a0 + step * a1) // 6 for step in range(1, 7)]
                else:
                    table = [a0, a1] + [((4 - step) * a0 + step * a1) // 4 for step in range(1, 5)] + [0, 255]
                alpha = [table[(bits >> (3 * index)) & 7] for index in range(16)]
            c0, c1, indices = struct.unpack_from("<HHI", pixels, position)
            position += 8
            first, second = colour(c0), colour(c1)
            if c0 > c1 or pixel_format != FORMAT_DXT1:
                colours = [first + (255,), second + (255,),
                           tuple((2 * x + y) // 3 for x, y in zip(first, second)) + (255,),
                           tuple((x + 2 * y) // 3 for x, y in zip(first, second)) + (255,)]
            else:
                colours = [first + (255,), second + (255,),
                           tuple((x + y) // 2 for x, y in zip(first, second)) + (255,), (0, 0, 0, 0)]
            for index in range(16):
                x, y = block_x + index % 4, block_y + index // 4
                if x < width and y < height:
                    texel = colours[(indices >> (2 * index)) & 3]
                    if alpha is not None:
                        texel = texel[:3] + (alpha[index],)
                    out[(y * width + x) * 4:(y * width + x) * 4 + 4] = bytes(texel)
    return bytes(out)


def first_bitmap_rgba(tag):
    """(width, height, RGBA bytes) of the first bitmap's largest level"""
    layout = BitmapTag(tag)
    bitmap = layout.bitmaps[0]
    return bitmap.width, bitmap.height, decode_rgba(layout.pixels(tag), bitmap.width, bitmap.height, bitmap.format)
