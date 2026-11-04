"""Drops Open Sauce's extension block from a tag file, so that tools made for the stock kit can read it.

Open Sauce (a Custom Edition extension CMT's tags are saved with) keeps a tag block of its own where the stock
kit's tag has padding: 192-byte elements in a shader_model (at 0xF0 of the root), 48-byte elements in a particle
(at 0xE0) and in each particle state of a particle_system (at 0xE8 of the state). A stock tool stops at them. This cuts the elements and the path strings of the references they hold out
of the file and clears the block's count; nothing else changes. The engine has no use for what they held (a
specular colour map and a normal map for the shader, extra particle settings).

Our own code: it reads the file's layout, no tool of Open Sauce or of CMT is used.
Usage: os_strip.py <shader_model|particle|particle_system> <source tag> <destination tag>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek

# group: (the root's size, the block's place in it, an element's size, the tag references of the root whose path
# strings lie before the block's elements in the file)
LAYOUTS = {
    "shader_model": (0x1B8, 0xF0, 192, (0xA4, 0xBC, 0xDC)),
    "particle": (356, 0xE0, 48, (0x04, 0x14, 0x24, 0x48, 0x58)),
}
# the groups a reference inside an element may name
REFERENCE_GROUPS = (b"bitm", b"shdr", b"snd!", b"effe")
MAXIMUM_ELEMENTS = 15


# a particle_system (the stock kit's layout): the root, its particle types, their states and particle states, each
# (size, the dependencies' offsets, the blocks' offsets in field order: (offset, the element's layout name)); a
# particle state's Open Sauce block is at 0xE8, between its point physics and its secondary map
SYSTEM_LAYOUTS = {
    "system": (104, ((56, "dep"), (80, 4), (92, "type"))),
    "type": (128, ((92, 4), (104, "state"), (116, "particle_state"))),
    "state": (192, ((180, 4),)),
    "particle_state": (376, ((48, "dep"), (132, "dep"), (0xE8, "os"), (244, "dep"), (364, 4))),
}
SYSTEM_EXTENSION_SIZE = 48


def strip_particle_system(tag):
    """a particle_system without its particle states' Open Sauce blocks: (the tag, bytes cut, elements cut). The
    file is walked as the stock kit lays it out (each block's elements, then each element's dependencies' strings
    and blocks in field order) and must end where the walk does, or it is refused"""
    data = bytearray(tag)
    cuts = []           # (where, how many bytes, the block's count field)
    total = [0]

    def count_at(offset):
        value = struct.unpack_from(">i", data, offset)[0]
        if not 0 <= value <= 4096:
            raise hek.Refused("a block of %d elements" % value)
        return value

    def element(layout, base, position):
        size, fields = SYSTEM_LAYOUTS[layout]
        for offset, kind in fields:
            if kind == "dep":
                length = struct.unpack_from(">i", data, base + offset + 8)[0]
                position += length + 1 if length > 0 else 0
            elif kind == "os":
                count = count_at(base + offset)
                if count:
                    if count > MAXIMUM_ELEMENTS:
                        raise hek.Refused("%d elements in an extension block" % count)
                    start = position
                    for _ in range(count):
                        strings = 0
                        for word in range(0, SYSTEM_EXTENSION_SIZE, 4):
                            if bytes(data[position + word:position + word + 4]) in REFERENCE_GROUPS:
                                length, index = struct.unpack_from(">iI", data, position + word + 8)
                                if index == 0xFFFFFFFF and 0 <= length < 256:
                                    strings += length + 1 if length > 0 else 0
                        position += SYSTEM_EXTENSION_SIZE + strings
                    cuts.append((start, position - start, base + offset))
                    total[0] += count
            elif isinstance(kind, int):
                position += count_at(base + offset) * kind
            else:
                count = count_at(base + offset)
                child_size = SYSTEM_LAYOUTS[kind][0]
                first = position
                position += count * child_size
                for index in range(count):
                    position = element(kind, first + index * child_size, position)
            if position > len(data):
                raise hek.Refused("the particle_system's blocks are not in the file")
        return position

    start = hek.HEADER_SIZE
    if len(data) < start + SYSTEM_LAYOUTS["system"][0]:
        raise hek.Refused("a particle_system tag of %d bytes" % len(data))
    end = element("system", start, start + SYSTEM_LAYOUTS["system"][0])
    if end != len(data):
        raise hek.Refused("the particle_system's layout ends at %d of its %d bytes" % (end, len(data)))
    cut = 0
    for where, size, count_offset in reversed(cuts):
        del data[where:where + size]
        struct.pack_into(">iII", data, count_offset, 0, 0, 0)
        cut += size
    return bytes(data), cut, total[0]


def strip(group, tag):
    """(the tag without the block, bytes cut, elements cut); the tag itself when it has no such block"""
    if group == "particle_system":
        return strip_particle_system(tag)
    if group not in LAYOUTS:
        raise hek.Refused("no layout for %s" % group)
    body_size, block, element_size, before = LAYOUTS[group]
    data = bytearray(tag)
    start = hek.HEADER_SIZE
    if len(data) < start + body_size:
        raise hek.Refused("a %s tag of %d bytes" % (group, len(data)))
    count = struct.unpack_from(">i", data, start + block)[0]
    if count == 0:
        return bytes(tag), 0, 0
    if not 0 < count <= MAXIMUM_ELEMENTS:
        raise hek.Refused("%d elements in the extension block" % count)
    position = start + body_size
    for offset in before:
        length = struct.unpack_from(">i", data, start + offset + 8)[0]
        position += length + 1 if length > 0 else 0
    if position > len(data):
        raise hek.Refused("reference lengths do not fit the file")
    cut = 0
    for _ in range(count):
        element = position + cut
        if element + element_size > len(data):
            raise hek.Refused("the extension block's elements are not in the file")
        strings = 0
        for offset in range(0, element_size, 4):
            if bytes(data[element + offset:element + offset + 4]) in REFERENCE_GROUPS:
                length, index = struct.unpack_from(">iI", data, element + offset + 8)
                if index == 0xFFFFFFFF and 0 <= length < 256:
                    strings += length + 1 if length > 0 else 0
        cut += element_size + strings
    if position + cut > len(data):
        raise hek.Refused("the extension block's strings are not in the file")
    del data[position:position + cut]
    struct.pack_into(">iII", data, start + block, 0, 0, 0)
    return bytes(data), cut, count


def main(arguments):
    if len(arguments) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    group, source, destination = arguments
    try:
        with open(source, "rb") as file:
            out, cut, count = strip(group, file.read())
    except hek.Refused as refusal:
        print("os_strip: refused: %s" % refusal, file=sys.stderr)
        return 1
    with open(destination, "wb") as file:
        file.write(out)
    print("stripped %d bytes (%d elements)" % (cut, count) if count else "no extension")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
