"""An Xbox map's tags as a reader of our own sees them: enough for the recipe's checks (which tags a map has, a
tag's bytes where its addresses say), nothing the game needs.

A map file: a 0x800-byte header ('head' ... 'foot', version 5, the whole map's length at 8, the tags' place at
0x10 and size at 0x14), then the rest, deflated on the disc. The tags are made to be read at the address
0x803A6000: a 0x24-byte header (the tag table's address, the tag count at 0xC, 'tags' at 0x20), the table (32
bytes a tag: group, two parent groups, index, the name's address, the root's address), then every tag's bytes.
"""
import struct
import zlib

TAG_BASE = 0x803A6000
MAP_HEADER_SIZE = 0x800
TAG_HEADER_SIZE = 0x24
TAG_RECORD_SIZE = 32
MAXIMUM_MAP_SIZE = 256 * 1024 * 1024
MAXIMUM_TAGS = 0x10000
NONE = 0xFFFFFFFF


class Refused(Exception):
    """not a map this reads, with the reason"""


def read_whole_map(path):
    """(the whole map as the game reads it: inflated when the file is as the disc has it; its tag bytes)"""
    with open(path, "rb") as file:
        disk = file.read(MAXIMUM_MAP_SIZE + 1)
    if len(disk) < MAP_HEADER_SIZE or len(disk) > MAXIMUM_MAP_SIZE:
        raise Refused("not an Xbox map: %s" % path)
    head, version, length = struct.unpack_from("<4sII", disk, 0)
    if head != b"daeh" or disk[MAP_HEADER_SIZE - 4:MAP_HEADER_SIZE] != b"toof" or version != 5 or not (
            MAP_HEADER_SIZE <= length <= MAXIMUM_MAP_SIZE):
        raise Refused("not an Xbox map: %s" % path)
    if len(disk) == length:
        whole = disk
    else:
        try:
            inflater = zlib.decompressobj()
            rest = inflater.decompress(disk[MAP_HEADER_SIZE:], length - MAP_HEADER_SIZE)
        except zlib.error:
            raise Refused("the map could not be inflated: %s" % path)
        # (a stream that ends whole but short of the header's length is taken when all it lacks lies past the
        # tags: the game loads such a map (the H2 Collection's h2d_gemini inflates 888 bytes short, its tags
        # ending where the stream does); a stream cut off before its end, or short inside the tags, is not)
        if len(rest) > length - MAP_HEADER_SIZE or not inflater.eof:
            raise Refused("the map could not be inflated: %s" % path)
        whole = disk[:MAP_HEADER_SIZE] + rest
        if len(whole) < length:
            offset, size = struct.unpack_from("<II", whole, 0x10)
            if offset > len(whole) or size > len(whole) - offset:
                raise Refused("the map inflates %d bytes short, inside its tags: %s" % (length - len(whole), path))
    offset, size = struct.unpack_from("<II", whole, 0x10)
    if size < TAG_HEADER_SIZE or offset > length or size > length - offset:
        raise Refused("the map's tags are not in its file: %s" % path)
    return whole, whole[offset:offset + size]


def whole_length_map(path):
    """the map inflated to every byte its header counts, what the stream lacks (past the tags: read_whole_map
    refuses any other shortfall) written as zeros, so that a reader that wants the whole length (Invader) takes
    it. Returns (the bytes, how many were added)"""
    whole, _ = read_whole_map(path)
    length = struct.unpack_from("<I", whole, 8)[0]
    return whole + bytes(length - len(whole)), length - len(whole)


def read_map(path):
    """the map's tag bytes"""
    return read_whole_map(path)[1]


class MapFile:
    """a map: its tags, and the file's bytes by file offset (pixels and samples are there)"""

    def __init__(self, path=None, whole=None, tag_data=None):
        if path is not None:
            whole, tag_data = read_whole_map(path)
        self.whole = whole
        self.tags = Tags(tag_data)

    def file_bytes(self, offset, size):
        if offset < 0 or size < 0 or offset + size > len(self.whole):
            return None
        return self.whole[offset:offset + size]


class Tag:
    def __init__(self, index, group, name, address):
        self.index, self.group, self.name, self.address = index, group, name, address


class Tags:
    """a map's tags: tag_data are the bytes the game reads at base"""

    def __init__(self, tag_data, base=TAG_BASE):
        self.data, self.base = tag_data, base
        if len(tag_data) < TAG_HEADER_SIZE:
            raise Refused("no tag header")
        table, _, _, count = struct.unpack_from("<IIII", tag_data, 0)
        if tag_data[0x20:0x24] != b"sgat" or not 0 < count <= MAXIMUM_TAGS or not self.contains(
                table, count * TAG_RECORD_SIZE):
            raise Refused("not a map's tags")
        self.tags = []
        self.by_name = {}
        for index in range(count):
            group, _, _, _, name, address = struct.unpack_from("<4sIIIII", tag_data,
                                                                table - base + index * TAG_RECORD_SIZE)
            text = self.string(name)
            tag = Tag(index, group[::-1].decode("latin-1"), text, address)
            self.tags.append(tag)
            self.by_name.setdefault((tag.group, text.lower()), tag)

    def contains(self, address, size):
        return address >= self.base and size >= 0 and address - self.base + size <= len(self.data)

    def read(self, address, size):
        if not self.contains(address, size):
            raise Refused("bytes outside the map's tags: %08x + %d" % (address, size))
        return self.data[address - self.base:address - self.base + size]

    def u32(self, address):
        return struct.unpack("<I", self.read(address, 4))[0]

    def string(self, address):
        if not self.contains(address, 1):
            raise Refused("a tag name outside the map's tags")
        end = self.data.index(b"\0", address - self.base)
        return self.data[address - self.base:end].decode("latin-1")

    def find(self, group, name):
        """the tag of that group and name (ASCII case aside), or None"""
        return self.by_name.get((group, name.lower()))

    def names(self):
        return list(self.by_name)
