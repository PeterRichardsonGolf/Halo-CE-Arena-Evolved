"""model_parts.py: the last parts of a gbxmodel tag and their shaders taken out byte for byte, the rest of the tag as it
was, its checksum made again; a tag it cannot walk to its end is refused."""
import struct
import sys
import zlib
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent / "weapon_pack"))

import model_parts  # noqa: E402

PATHS = [b"cmt\\a\\body", b"cmt\\a\\scope", b"cmt\\a\\arms"]


def part(shader, vertices, triangles):
    record = bytearray(model_parts.PART)
    struct.pack_into(">H", record, model_parts.PART_SHADER, shader)
    struct.pack_into(">i", record, 32, vertices)
    struct.pack_into(">i", record, 44, vertices)
    struct.pack_into(">i", record, 56, triangles)
    return bytes(record)


def model(paths=PATHS, counts=((4, 2), (3, 1), (5, 3))):
    """a gbxmodel: 2 nodes, a region with a permutation with 2 markers, one geometry with a part for each shader"""
    root = bytearray(model_parts.ROOT)
    struct.pack_into(">i", root, model_parts.NODES, 2)
    struct.pack_into(">i", root, model_parts.REGIONS, 1)
    struct.pack_into(">i", root, model_parts.GEOMETRIES, 1)
    struct.pack_into(">i", root, model_parts.SHADERS, len(paths))
    region = bytearray(model_parts.REGION)
    struct.pack_into(">i", region, model_parts.REGION_PERMUTATIONS, 1)
    permutation = bytearray(model_parts.PERMUTATION)
    struct.pack_into(">i", permutation, model_parts.PERMUTATION_MARKERS, 2)
    geometry = bytearray(model_parts.GEOMETRY)
    struct.pack_into(">i", geometry, model_parts.GEOMETRY_PARTS, len(paths))
    body = bytes(root) + bytes(2 * model_parts.NODE) + bytes(region) + bytes(permutation) + bytes(2 * model_parts.MARKER)
    body += bytes(geometry)
    body += b"".join(part(index, *counts[index]) for index in range(len(paths)))
    for index, (vertices, triangles) in enumerate(counts[:len(paths)]):
        body += bytes([index + 1]) * (vertices * 68 + vertices * 32 + triangles * 6)
    shaders = bytearray()
    for index, name in enumerate(paths):
        record = bytearray(model_parts.SHADER)
        struct.pack_into(">i", record, model_parts.SHADER_PATH_LENGTH, len(name))
        shaders += record
    body += bytes(shaders) + b"".join(name + b"\0" for name in paths)
    header = bytearray(model_parts.HEADER)
    header[36:40] = b"mod2"
    struct.pack_into(">I", header, model_parts.CHECKSUM, zlib.crc32(body) ^ 0xFFFFFFFF)
    return bytes(header) + body


def test_the_last_parts_and_their_shaders_go_and_the_rest_stays():
    tag = model()
    result, names = model_parts.drop_last_parts(tag, 1)
    assert names == ["cmt\\a\\arms"]
    shorter = model(PATHS[:2], ((4, 2), (3, 1)))
    # (what is left is the model with two parts, byte for byte: only the counts and the checksum were made again)
    assert result == shorter
    assert model_parts.layout(result)["shaders"][1] == 2


def test_two_parts_go_and_the_kept_parts_keep_their_blocks():
    result, names = model_parts.drop_last_parts(model(), 2)
    assert names == ["cmt\\a\\scope", "cmt\\a\\arms"]
    assert result == model(PATHS[:1], ((4, 2),))


def test_the_checksum_is_the_files_again():
    result, _ = model_parts.drop_last_parts(model(), 1)
    assert struct.unpack_from(">I", result, model_parts.CHECKSUM)[0] == zlib.crc32(result[model_parts.HEADER:]) ^ 0xFFFFFFFF


def test_what_cannot_be_walked_or_dropped_is_refused():
    tag = model()
    with pytest.raises(model_parts.Refused):
        model_parts.drop_last_parts(tag + b"\0", 1)             # ends past the walk
    with pytest.raises(model_parts.Refused):
        model_parts.drop_last_parts(tag, 3)                      # every part
    with pytest.raises(model_parts.Refused):
        model_parts.drop_last_parts(tag, 0)
    with pytest.raises(model_parts.Refused):
        model_parts.drop_last_parts(b"\0" * 400, 1)              # no gbxmodel
    shuffled = bytearray(tag)
    parts_at = model_parts.layout(tag)["parts"][0]
    struct.pack_into(">H", shuffled, parts_at + model_parts.PART_SHADER, 2)      # part 0 draws shader 2
    with pytest.raises(model_parts.Refused):
        model_parts.drop_last_parts(bytes(shuffled), 1)


def skinned(nodes):
    """the model with part 0's 4 vertices held by these model nodes alone (its local nodes 2 and 1: model nodes 2 and 1),
    each vertex's uncompressed and compressed copies at (index, index, index)"""
    tag = bytearray(model())
    where = model_parts.layout(bytes(tag))
    element = where["parts"][0]
    tag[element + model_parts.PART_LOCAL_NODE_COUNT] = 2
    tag[element + model_parts.PART_LOCAL_NODES:element + model_parts.PART_LOCAL_NODES + 2] = bytes([2, 1])
    start = where["children"][0][0]
    for index, node in enumerate(nodes):
        at = start + index * model_parts.UNCOMPRESSED
        struct.pack_into(">3f", tag, at, index, index, index)
        struct.pack_into(">hh", tag, at + model_parts.UNCOMPRESSED_NODES, [2, 1].index(node), -1)
        struct.pack_into(">ff", tag, at + model_parts.UNCOMPRESSED_WEIGHTS, 1.0, 0.0)
        at = start + len(nodes) * model_parts.UNCOMPRESSED + index * model_parts.COMPRESSED
        struct.pack_into(">3f", tag, at, index, index, index)
        struct.pack_into(">bbH", tag, at + model_parts.COMPRESSED_NODES, node * 3, -3, 32767)
    struct.pack_into(">I", tag, model_parts.CHECKSUM, zlib.crc32(bytes(tag[model_parts.HEADER:])) ^ 0xFFFFFFFF)
    return bytes(tag), start


def test_a_nodes_vertices_fold_onto_a_point_and_the_rest_stays():
    tag, start = skinned([2, 1, 2, 1])
    result, moved = model_parts.collapse_vertices(tag, 0, 2, 1, (0.5, 0.25, 0.125))
    assert moved == 2 and len(result) == len(tag)
    compressed = start + 4 * model_parts.UNCOMPRESSED
    for index in (0, 2):
        at = start + index * model_parts.UNCOMPRESSED
        assert struct.unpack_from(">3f", result, at) == (0.5, 0.25, 0.125)
        assert struct.unpack_from(">hh", result, at + model_parts.UNCOMPRESSED_NODES) == (1, -1)
        at = compressed + index * model_parts.COMPRESSED
        assert struct.unpack_from(">3f", result, at) == (0.5, 0.25, 0.125)
        assert struct.unpack_from(">bb", result, at + model_parts.COMPRESSED_NODES) == (3, -3)
    # (the other vertices, and every byte but the folded ones and the checksum, as they were)
    changed = [at for at in range(len(tag)) if tag[at] != result[at]]
    folded = [range(start + i * model_parts.UNCOMPRESSED, start + (i + 1) * model_parts.UNCOMPRESSED) for i in (0, 2)]
    folded += [range(compressed + i * model_parts.COMPRESSED, compressed + (i + 1) * model_parts.COMPRESSED) for i in (0, 2)]
    checksum = range(model_parts.CHECKSUM, model_parts.CHECKSUM + 4)
    assert all(any(at in span for span in folded + [checksum]) for at in changed)
    assert struct.unpack_from(">I", result, model_parts.CHECKSUM)[0] == zlib.crc32(result[model_parts.HEADER:]) ^ 0xFFFFFFFF


def test_a_fold_that_cannot_be_made_is_refused():
    tag, start = skinned([2, 1, 2, 1])
    with pytest.raises(model_parts.Refused):
        model_parts.collapse_vertices(tag, 0, 5, 1, (0, 0, 0))          # no such local node
    with pytest.raises(model_parts.Refused):
        model_parts.collapse_vertices(tag, 3, 2, 1, (0, 0, 0))          # no such part
    folded, _ = model_parts.collapse_vertices(tag, 0, 2, 1, (0, 0, 0))
    with pytest.raises(model_parts.Refused):
        model_parts.collapse_vertices(folded, 0, 2, 1, (0, 0, 0))       # nothing left to fold
    split = bytearray(tag)
    struct.pack_into(">b", split, start + 4 * model_parts.UNCOMPRESSED + model_parts.COMPRESSED_NODES, 3)
    with pytest.raises(model_parts.Refused):
        model_parts.collapse_vertices(bytes(split), 0, 2, 1, (0, 0, 0))  # the two copies disagree
    shared = bytearray(tag)
    struct.pack_into(">h", shared, start + model_parts.UNCOMPRESSED_NODES + 2, 1)
    with pytest.raises(model_parts.Refused):
        model_parts.collapse_vertices(bytes(shared), 0, 2, 1, (0, 0, 0))  # held by a second node


def named_markers(names):
    """the test model with its permutation's two markers given names"""
    tag = bytearray(model())
    for at, name in zip(model_parts.layout(bytes(tag))["markers"], names):
        tag[at:at + len(name)] = name
    struct.pack_into(">I", tag, model_parts.CHECKSUM, zlib.crc32(bytes(tag[model_parts.HEADER:])) ^ 0xFFFFFFFF)
    return bytes(tag)


def test_a_marker_is_placed_and_nothing_else_changes():
    tag = named_markers([b"ground point", b"primary trigger"])
    result, placed = model_parts.set_marker(tag, "ground point", (0.5, -0.25, 1.0), (0.0, 0.6, 0.0, 0.8))
    assert placed == 1 and len(result) == len(tag)
    ground, trigger = model_parts.layout(result)["markers"]
    assert struct.unpack_from(">4f", result, ground + model_parts.MARKER_ROTATION) == pytest.approx((0.0, 0.6, 0.0, 0.8))
    assert struct.unpack_from(">3f", result, ground + model_parts.MARKER_TRANSLATION) == (0.5, -0.25, 1.0)
    # (only those 28 bytes and the checksum: the other marker, its node index and the rest are the tag's)
    changed = [index for index in range(len(tag)) if tag[index] != result[index]]
    assert all(ground + model_parts.MARKER_ROTATION <= index < ground + model_parts.MARKER_TRANSLATION + 12 or
               model_parts.CHECKSUM <= index < model_parts.CHECKSUM + 4 for index in changed)
    assert result[trigger:trigger + model_parts.MARKER] == tag[trigger:trigger + model_parts.MARKER]
    assert struct.unpack_from(">I", result, model_parts.CHECKSUM)[0] == zlib.crc32(result[model_parts.HEADER:]) ^ 0xFFFFFFFF


def test_a_marker_not_there_or_a_rotation_not_unit_is_refused():
    tag = named_markers([b"ground point", b"primary trigger"])
    with pytest.raises(model_parts.Refused):
        model_parts.set_marker(tag, "ground", (0, 0, 0), (0, 0, 0, 1))      # (a name's start is not the name)
    with pytest.raises(model_parts.Refused):
        model_parts.set_marker(tag, "ground point", (0, 0, 0), (0, 0, 0, 2))
    with pytest.raises(model_parts.Refused):
        model_parts.set_marker(tag, "ground point", (0, 0), (0, 0, 0, 1))
