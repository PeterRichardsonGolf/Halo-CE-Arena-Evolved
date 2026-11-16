"""Takes the last parts of a gbxmodel tag file out (and their shaders), byte for byte: the rest of the tag is left exactly
as it was (its nodes, their bind poses, its vertices and skinning, its other parts), which a JMS round trip through
Invader's model compiler does not keep (it makes the nodes' inverse transforms again, and a model built for another
animation graph's rest pose then draws wrong).

CMT's tactical SMG is one model with a part for each of its shaders: the gun, the silencer, the scope, the arms and the
two flashlight cones. The tactical SMG pack's third-person model is it without the last three parts, its first-person
model without the last two.

The tag file is walked as the stock kit lays a gbxmodel out (every block's elements, then, for each element, its
dependencies' path strings and child blocks in field order; sizes 156 a node, 76 a region, 88 a permutation, 80 a
marker, 48 a geometry, 132 a part holding blocks of 68-byte, 32-byte and 6-byte elements, 32 a shader) and must end
where the walk does, or it is refused.

Usage: model_parts.py <in gbxmodel> <out gbxmodel> <how many parts to drop from the end>
"""
import struct
import sys
import zlib

HEADER = 64
ROOT = 232
# the root's blocks (at these offsets: count, then 8 bytes the stock kit leaves for the address and the definition)
MARKERS, NODES, REGIONS, GEOMETRIES, SHADERS = 0xAC, 0xB8, 0xC4, 0xD0, 0xDC
NODE, REGION, PERMUTATION, MARKER, GEOMETRY, PART, SHADER = 156, 76, 88, 80, 48, 132, 32
REGION_PERMUTATIONS = 64          # a region's permutations block, in its element
PERMUTATION_MARKERS = 76          # a permutation's markers block, in its element
GEOMETRY_PARTS = 36               # a geometry's parts block, in its element
PART_BLOCKS = ((32, 68), (44, 32), (56, 6))      # a part's blocks (in its element) and their elements' sizes
PART_SHADER = 4                   # a part's shader index (a short)
SHADER_PATH_LENGTH = 8            # a shader dependency's path length (without its NUL), in its element
CHECKSUM = 40                     # the tag header's CRC-32 (inverted) of the rest of the file
MODEL_GROUP = b"mod2"


class Refused(Exception):
    """not a gbxmodel this walks, with the reason"""


def count(tag, offset):
    value = struct.unpack_from(">i", tag, offset)[0]
    if not 0 <= value <= 1 << 20:
        raise Refused("a block of %d elements" % value)
    return value


def layout(tag):
    """where the parts of the one geometry and the shaders are: {"parts": (offset of the first part, count),
    "children": [(offset, size) per part], "shaders": (offset, count), "strings": [(offset, size) per shader]}"""
    if tag[36:40] != MODEL_GROUP or len(tag) < HEADER + ROOT:
        raise Refused("not a gbxmodel")
    root = HEADER
    if count(tag, root + MARKERS):
        raise Refused("a model with markers of its own")
    position = root + ROOT + count(tag, root + NODES) * NODE
    regions = count(tag, root + REGIONS)
    first = position
    position += regions * REGION
    for index in range(regions):
        permutations = count(tag, first + index * REGION + REGION_PERMUTATIONS)
        at = position
        position += permutations * PERMUTATION
        for permutation in range(permutations):
            position += count(tag, at + permutation * PERMUTATION + PERMUTATION_MARKERS) * MARKER
    if count(tag, root + GEOMETRIES) != 1:
        raise Refused("a model with other than one geometry")
    geometry = position
    parts = count(tag, geometry + GEOMETRY_PARTS)
    position += GEOMETRY
    parts_at = position
    position += parts * PART
    children = []
    for index in range(parts):
        start = position
        for field, size in PART_BLOCKS:
            position += count(tag, parts_at + index * PART + field) * size
        children.append((start, position - start))
    shaders = count(tag, root + SHADERS)
    shaders_at = position
    position += shaders * SHADER
    strings = []
    for index in range(shaders):
        size = count(tag, shaders_at + index * SHADER + SHADER_PATH_LENGTH) + 1
        strings.append((position, size))
        position += size
    if position != len(tag):
        raise Refused("the walk ends at %d, the file at %d" % (position, len(tag)))
    return {"geometry": geometry, "parts": (parts_at, parts), "children": children, "shaders": (shaders_at, shaders),
            "strings": strings}


def drop_last_parts(tag, drop):
    """(the tag without its last `drop` parts and their shaders: part n has shader n, the kept parts' shaders are the
    kept shaders, nothing else changes but the counts and the checksum, the names of what went)"""
    where = layout(tag)
    parts_at, parts = where["parts"]
    shaders_at, shaders = where["shaders"]
    if not 0 < drop < parts or parts != shaders:
        raise Refused("%d parts, %d shaders: cannot drop %d" % (parts, shaders, drop))
    for index in range(parts):
        if struct.unpack_from(">H", tag, parts_at + index * PART + PART_SHADER)[0] != index:
            raise Refused("part %d has not shader %d" % (index, index))
    keep = parts - drop
    names = [bytes(tag[start:start + size - 1]).decode("ascii") for start, size in where["strings"][keep:]]
    cuts = [(parts_at + keep * PART, parts_at + parts * PART),
            (where["children"][keep][0], where["children"][-1][0] + where["children"][-1][1]),
            (shaders_at + keep * SHADER, shaders_at + shaders * SHADER),
            (where["strings"][keep][0], len(tag))]
    out = bytearray()
    cursor = 0
    for start, end in cuts:
        out += tag[cursor:start]
        cursor = end
    out += tag[cursor:]
    struct.pack_into(">i", out, where["geometry"] + GEOMETRY_PARTS, keep)
    struct.pack_into(">i", out, HEADER + SHADERS, keep)
    struct.pack_into(">I", out, CHECKSUM, zlib.crc32(bytes(out[HEADER:])) ^ 0xFFFFFFFF)
    layout(bytes(out))                  # (the result must walk as a model again)
    return bytes(out), names


def main(arguments):
    if len(arguments) != 3:
        print(__doc__)
        return 2
    with open(arguments[0], "rb") as file:
        result, names = drop_last_parts(file.read(), int(arguments[2]))
    with open(arguments[1], "wb") as file:
        file.write(result)
    print("%s: dropped %s" % (arguments[1], ", ".join(names)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
