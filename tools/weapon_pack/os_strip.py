"""Drops Open Sauce's extension block from a tag file, so that tools made for the stock kit can read it.

Open Sauce (a Custom Edition extension CMT's tags are saved with) keeps a tag block of its own where the stock
kit's tag has padding: 192-byte elements in a shader_model (at 0xF0 of the root), 48-byte elements in a particle
(at 0xE0). A stock tool stops at them. This cuts the elements and the path strings of the references they hold out
of the file and clears the block's count; nothing else changes. The engine has no use for what they held (a
specular colour map and a normal map for the shader, extra particle settings).

Our own code: it reads the file's layout, no tool of Open Sauce or of CMT is used.
Usage: os_strip.py <shader_model|particle> <source tag> <destination tag>
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


def strip(group, tag):
    """(the tag without the block, bytes cut, elements cut); the tag itself when it has no such block"""
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
