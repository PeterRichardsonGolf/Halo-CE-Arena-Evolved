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
