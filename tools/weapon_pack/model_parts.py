"""Takes the last parts of a gbxmodel tag file out (and their shaders), byte for byte: the rest of the tag is left exactly
as it was (its nodes, their bind poses, its vertices and skinning, its other parts), which a JMS round trip through
Invader's model compiler does not keep (it makes the nodes' inverse transforms again, and a model built for another
animation graph's rest pose then draws wrong).

CMT's tactical SMG is one model with a part for each of its shaders: the gun, the silencer, the scope, the arms and the
two flashlight cones. The tactical SMG pack's third-person model is it without the last three parts, its first-person
model without the last two.

It also sets a marker's place, byte for byte (set_marker): every marker of a name, in every permutation, gets the
rotation and translation given, in its node's frame; nothing else changes. Invader's field editor holds a marker's
rotation and translation read-only. The gravity hammer's "ground point" pointed its up along the shaft, so a hammer on
the ground stood on end in the floor.

It also folds a piece of a kept part away, byte for byte (collapse_vertices): every vertex of the part held by one node
alone is put on one point, held by another node alone, so the piece's faces have no size and draw nothing; the other
vertices, the faces and every other byte stay. CMT's gun part carries a spare magazine on the first-person left upper
arm, which a third-person model draws in the gun's resting pose far behind it (a weapon in its thigh holster trailed
it: the owner 2026-11-15, "it looks like a tac mag trailing the player").

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
# a permutation's marker: its name (32 bytes, NUL-padded), node index (a short), 2 bytes, rotation (i, j, k, w floats),
# translation (x, y, z floats), 16 bytes
MARKER_NAME_LENGTH, MARKER_NODE, MARKER_ROTATION, MARKER_TRANSLATION = 32, 32, 36, 52
NODE_NAME_LENGTH, NODE_ROTATION = 32, 52   # a node's name and its default rotation (i, j, k, w), in its element
GEOMETRY_PARTS = 36               # a geometry's parts block, in its element
PART_BLOCKS = ((32, 68), (44, 32), (56, 6))      # a part's blocks (in its element) and their elements' sizes
PART_SHADER = 4                   # a part's shader index (a short)
SHADER_PATH_LENGTH = 8            # a shader dependency's path length (without its NUL), in its element
CHECKSUM = 40                     # the tag header's CRC-32 (inverted) of the rest of the file
PART_LOCAL_NODE_COUNT = 107       # a part's local node count (a byte) and its 22 local node indices (bytes), in its element
PART_LOCAL_NODES = 108
PART_LOCAL_NODE_MAXIMUM = 22
# a vertex: its position (3 floats) first in both; an uncompressed one's node indices (2 shorts, the part's local nodes;
# -1 none) and weights (2 floats) at 56 and 60; a compressed one's node indices (2 signed bytes, the model's node times 3;
# -3 none) and its first weight (a short, 32767 for 1) at 28 and 30
UNCOMPRESSED, COMPRESSED = 68, 32
UNCOMPRESSED_NODES, UNCOMPRESSED_WEIGHTS = 56, 60
COMPRESSED_NODES, COMPRESSED_WEIGHT = 28, 30
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
    markers = []
    for index in range(regions):
        permutations = count(tag, first + index * REGION + REGION_PERMUTATIONS)
        at = position
        position += permutations * PERMUTATION
        for permutation in range(permutations):
            held = count(tag, at + permutation * PERMUTATION + PERMUTATION_MARKERS)
            markers += [position + marker * MARKER for marker in range(held)]
            position += held * MARKER
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
            "strings": strings, "markers": markers}


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


def collapse_vertices(tag, part, node, onto, point):
    """(the tag with every vertex of part `part` held by model node `node` alone put on `point` (x, y, z) and held by
    model node `onto` alone, its uncompressed and compressed copies both, the count of them): the faces made only of
    them have no size. Both nodes must be among the part's local nodes, each vertex's two copies must agree, and at
    least one is moved, or it is refused; nothing else changes but those vertices and the checksum"""
    where = layout(tag)
    parts_at, parts = where["parts"]
    if not 0 <= part < parts:
        raise Refused("no part %d of %d" % (part, parts))
    element = parts_at + part * PART
    local_count = tag[element + PART_LOCAL_NODE_COUNT]
    if not 0 < local_count <= PART_LOCAL_NODE_MAXIMUM:
        raise Refused("part %d has %d local nodes" % (part, local_count))
    local = list(tag[element + PART_LOCAL_NODES:element + PART_LOCAL_NODES + local_count])
    if node not in local or onto not in local:
        raise Refused("part %d's local nodes %s do not hold nodes %d and %d" % (part, local, node, onto))
    vertices = count(tag, element + PART_BLOCKS[0][0])
    if count(tag, element + PART_BLOCKS[1][0]) != vertices:
        raise Refused("part %d's compressed vertices are not its vertices" % part)
    uncompressed = where["children"][part][0]
    compressed = uncompressed + vertices * UNCOMPRESSED
    out = bytearray(tag)
    moved = 0
    for index in range(vertices):
        at = uncompressed + index * UNCOMPRESSED
        nodes = struct.unpack_from(">hh", tag, at + UNCOMPRESSED_NODES)
        squeezed = struct.unpack_from(">bb", tag, compressed + index * COMPRESSED + COMPRESSED_NODES)
        held = 0 <= nodes[0] < local_count and local[nodes[0]] == node
        if held != (squeezed[0] == node * 3):
            raise Refused("part %d's vertex %d: its two copies are held by different nodes" % (part, index))
        if not held:
            continue
        if nodes[1] != -1 or squeezed[1] != -3:
            raise Refused("part %d's vertex %d is held by a second node" % (part, index))
        struct.pack_into(">3f", out, at, *point)
        struct.pack_into(">hh", out, at + UNCOMPRESSED_NODES, local.index(onto), -1)
        struct.pack_into(">ff", out, at + UNCOMPRESSED_WEIGHTS, 1.0, 0.0)
        at = compressed + index * COMPRESSED
        struct.pack_into(">3f", out, at, *point)
        struct.pack_into(">bb", out, at + COMPRESSED_NODES, onto * 3, -3)
        struct.pack_into(">H", out, at + COMPRESSED_WEIGHT, 32767)
        moved += 1
    if not moved:
        raise Refused("part %d has no vertex held by node %d" % (part, node))
    struct.pack_into(">I", out, CHECKSUM, zlib.crc32(bytes(out[HEADER:])) ^ 0xFFFFFFFF)
    layout(bytes(out))
    return bytes(out), moved


def set_marker(tag, name, translation, rotation):
    """(the tag with every permutation's marker called `name` given `translation` (x, y, z) and `rotation` (i, j, k, w,
    a unit quaternion), in its node's frame, the count of them): no marker of the name, or a rotation not of unit length,
    is refused; nothing else changes but those floats and the checksum"""
    if len(translation) != 3 or len(rotation) != 4 or abs(sum(value * value for value in rotation) - 1) > 1e-4:
        raise Refused("a marker's place is 3 numbers and a unit quaternion of 4")
    out = bytearray(tag)
    found = 0
    for at in layout(tag)["markers"]:
        if bytes(tag[at:at + MARKER_NAME_LENGTH]).split(b"\0", 1)[0] != name.encode("ascii"):
            continue
        struct.pack_into(">4f", out, at + MARKER_ROTATION, *rotation)
        struct.pack_into(">3f", out, at + MARKER_TRANSLATION, *translation)
        found += 1
    if not found:
        raise Refused("no marker called %s" % name)
    struct.pack_into(">I", out, CHECKSUM, zlib.crc32(bytes(out[HEADER:])) ^ 0xFFFFFFFF)
    layout(bytes(out))
    return bytes(out), found


def set_node_rotation(tag, name, rotation):
    """(the tag with the node called `name` given the default rotation `rotation` (i, j, k, w, a unit quaternion), its
    old rotation): no node of the name, or a rotation not of unit length, is refused; nothing else changes but those
    floats and the checksum (Invader's field editor holds a node's default rotation read-only)"""
    if len(rotation) != 4 or abs(sum(value * value for value in rotation) - 1) > 1e-4:
        raise Refused("a node's rotation is a unit quaternion of 4")
    if tag[36:40] != MODEL_GROUP or len(tag) < HEADER + ROOT:
        raise Refused("not a gbxmodel")
    if count(tag, HEADER + MARKERS):
        raise Refused("a model with markers of its own")
    out = bytearray(tag)
    nodes = count(tag, HEADER + NODES)
    if HEADER + ROOT + nodes * NODE > len(tag):
        raise Refused("%d nodes run past the file" % nodes)
    for index in range(nodes):
        at = HEADER + ROOT + index * NODE
        if bytes(tag[at:at + NODE_NAME_LENGTH]).split(b"\0", 1)[0] == name.encode("ascii"):
            old = struct.unpack_from(">4f", tag, at + NODE_ROTATION)
            struct.pack_into(">4f", out, at + NODE_ROTATION, *rotation)
            struct.pack_into(">I", out, CHECKSUM, zlib.crc32(bytes(out[HEADER:])) ^ 0xFFFFFFFF)
            return bytes(out), old
    raise Refused("no node called %s" % name)


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
