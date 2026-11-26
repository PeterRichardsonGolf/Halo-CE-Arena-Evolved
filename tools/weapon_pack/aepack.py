"""Weapon pack format v1 (`.aepack`, docs/weapon-packs.md) in Python: the tests' writer, reader and hash.

The engine's reader is port/linux/game/ae_pack_format.c; this module mirrors its checks so that the tests can
compare verdicts (tools/test_weapon_pack_format.py: every pack either both accept, printing the same lines, or both
refuse). `build` writes what it is given WITHOUT checking it (the tests make hostile packs with it); `read` checks a
pack as the engine does; `describe` prints an accepted pack as tools/ae_pack_check.c does.

usage: python3 tools/weapon_pack/aepack.py check <pack>...
"""

import hashlib
import struct
import sys
import zlib
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

MAGIC = b"AEWPACK\0"
FORMAT_VERSION = 1
HEADER_SIZE = 0x100
SECTION_ENTRY_SIZE = 20
MAXIMUM_SECTIONS = 12
TAG_LAYOUT_XBOX = 1
ALIGNMENT = 16
NONE = 0xFFFFFFFF
SHA256_OFFSET = 0x5C
SHA256_SIZE = 32

# section kinds
STRINGS, TAGS, TAG_BYTES, FIXUPS, PARTS, GEOMETRY, RESOURCES, RESOURCE_DATA, WEAPONS, CREDITS = range(1, 11)
SECTION_NAMES = {STRINGS: "STRINGS", TAGS: "TAGS", TAG_BYTES: "TAG_BYTES", FIXUPS: "FIXUPS", PARTS: "PARTS",
                 GEOMETRY: "GEOMETRY", RESOURCES: "RESOURCES", RESOURCE_DATA: "RESOURCE_DATA", WEAPONS: "WEAPONS",
                 CREDITS: "CREDITS"}

# compressions
STORED, ZLIB = 0, 1

# header flags: a pack of tags only (no weapon entries: the vehicle damage pack, found by name)
FLAG_NO_WEAPONS = 1
# (0.7.5) a direct port of another game's asset: the game loads it only with the player's MCC install found
FLAG_DIRECT_PORT = 2
KNOWN_HEADER_FLAGS = FLAG_NO_WEAPONS | FLAG_DIRECT_PORT

# tag kinds, flags (R9)
OWN, EXTERNAL = 0, 1
FLAG_SMOOTH = 1
FLAG_METER_COVERAGE = 2
KNOWN_FLAGS = FLAG_SMOOTH | FLAG_METER_COVERAGE

# fixup kinds
FIXUP_POINTER, FIXUP_REFERENCE, FIXUP_TAG_INDEX = 1, 2, 3
TAG_REFERENCE_SIZE = 16

# record sizes
TAG_RECORD = struct.Struct("<4I4B9I")
FIXUP_RECORD = struct.Struct("<IHHI")
PART_RECORD = struct.Struct("<IHH6I")
RESOURCE_RECORD = struct.Struct("<6I")
WEAPON_RECORD = struct.Struct("<14I")
SECTION_ENTRY = struct.Struct("<5I")
HEADER = struct.Struct("<8s3I32sI32sI32sI128s")
assert TAG_RECORD.size == 56 and FIXUP_RECORD.size == 12 and PART_RECORD.size == 32
assert RESOURCE_RECORD.size == 24 and WEAPON_RECORD.size == 56 and HEADER.size == HEADER_SIZE

# limits (a pack past one is refused whole)
MiB = 1024 * 1024
MAXIMUM_FILE_SIZE = 64 * MiB
MAXIMUM_TAGS = 1024
MAXIMUM_OWN_TAGS = 512
MAXIMUM_TAG_SIZE = 4 * MiB
MAXIMUM_TAG_BYTES = 16 * MiB
MAXIMUM_RESOURCE_SIZE = 8 * MiB
MAXIMUM_RESOURCES_SIZE = 48 * MiB
MAXIMUM_STRINGS = 256 * 1024
MAXIMUM_FIXUPS = 262144
MAXIMUM_PARTS = 4096
MAXIMUM_GEOMETRY = 16 * MiB
MAXIMUM_RESOURCES = 4096
MAXIMUM_WEAPONS = 8
MAXIMUM_CREDITS = 64 * 1024
MAXIMUM_NAME = 255
MAXIMUM_KEY = 31
MAXIMUM_LABEL = 31
MAXIMUM_MENU_NAME = 63
MAXIMUM_PICKUP_TEXT = 127
MAXIMUM_ICON_SEQUENCE = 0xFFFF
# weapon behaviours: 0-3 the rifle profiles, 4 the PDW's burst, 5 the ravager's (the cache's AE_CACHE_BEHAVIOUR_LAST)
MAXIMUM_BEHAVIOUR = 5

# what each section may hold, inflated (and its record size)
SECTION_LIMITS = {
    STRINGS: (MAXIMUM_STRINGS, 1),
    TAGS: (MAXIMUM_TAGS * TAG_RECORD.size, TAG_RECORD.size),
    TAG_BYTES: (MAXIMUM_TAG_BYTES, 1),
    FIXUPS: (MAXIMUM_FIXUPS * FIXUP_RECORD.size, FIXUP_RECORD.size),
    PARTS: (MAXIMUM_PARTS * PART_RECORD.size, PART_RECORD.size),
    GEOMETRY: (MAXIMUM_GEOMETRY, 1),
    RESOURCES: (MAXIMUM_RESOURCES * RESOURCE_RECORD.size, RESOURCE_RECORD.size),
    RESOURCE_DATA: (MAXIMUM_FILE_SIZE, 1),
    WEAPONS: (MAXIMUM_WEAPONS * WEAPON_RECORD.size, WEAPON_RECORD.size),
    CREDITS: (MAXIMUM_CREDITS, 1),
}

# tag layout 1: a model_geometry_part's size, the vertex types' sizes (rasterizer_geometry.c), triangle buffer types
MODEL_PART_SIZE = 0x68
VERTEX_SIZES = (56, 32, 20, 8, 68, 32, 24, 36, 20, 16, 16, 8)
TRIANGLES, STRIP = 0, 1
# (a model part's vertices: model uncompressed or compressed)
MODEL_VERTEX_TYPES = (4, 5)
MAXIMUM_PART_COUNT = 0xFFFF

GROUP_WEAPON = 0x77656170  # 'weap'
GROUP_ITEM_COLLECTION = 0x69746D63  # 'itmc'
GROUP_BITMAP = 0x6269746D  # 'bitm'

# the stock loadout weapons a pack weapon may replace (tools/port_settings.py LOADOUT_WEAPONS: 0 none, 2..9)
REPLACEABLE = (0, 2, 3, 4, 5, 6, 7, 8, 9)


def group(text: str) -> int:
    """a group's tag from its four characters ('weap')"""
    return struct.unpack(">I", text.encode("ascii"))[0]


def align(value: int) -> int:
    return (value + ALIGNMENT - 1) // ALIGNMENT * ALIGNMENT


# ---------- what a pack holds (the writer's input)

@dataclass
class Fixup:
    offset: int
    kind: int
    value: int
    pad: int = 0


@dataclass
class Part:
    offset: int
    vertex_type: int
    triangle_type: int
    vertex_count: int
    triangle_count: int
    vertices: bytes
    indices: bytes


@dataclass
class Resource:
    map_offset: int
    data: bytes
    compression: int = STORED
    # (the tests' lies: what the record says, instead of the data's)
    size: Optional[int] = None
    tag: Optional[int] = None


@dataclass
class Tag:
    group: int
    name: Optional[str]
    kind: int = OWN
    required: int = 0
    flags: int = 0
    source_path: Optional[str] = ""
    bytes: bytes = b""
    fixups: List[Fixup] = field(default_factory=list)
    parts: List[Part] = field(default_factory=list)
    resources: List[Resource] = field(default_factory=list)
    parent_groups: Tuple[int, int] = (NONE, NONE)
    pad: int = 0


@dataclass
class Weapon:
    key: Optional[str]
    root_tag: int
    label: Optional[str]
    menu_name: Optional[str]
    item_collection_tag: int = NONE
    behaviour: int = 0
    fallback_labels: Tuple[Optional[str], Optional[str]] = (None, None)
    # the weapon picked up (no '%'), its ammo picked up (exactly one "%d", the engine's rounds)
    weapon_pickup_text: Optional[str] = None
    ammo_pickup_text: Optional[str] = None
    icon_bitmap_tag: int = NONE
    icon_sequence: int = 0
    replaces: int = 0
    power: int = 0


@dataclass
class Pack:
    pack_id: str
    pack_revision: int
    display_name: str
    tags: List[Tag]
    weapons: List[Weapon]
    credits: str = ""
    tag_layout: int = TAG_LAYOUT_XBOX
    flags: int = 0
    format_version: int = FORMAT_VERSION
    header_size: int = HEADER_SIZE
    # sections written zlib-compressed (the writer of format v1 writes none: R2; the tests do)
    compressed_sections: Tuple[int, ...] = ()


# ---------- the writer (no checks: the tests write hostile packs with it)

class _Strings:
    def __init__(self):
        self.data = bytearray()
        self.offsets = {}

    def add(self, text: Optional[str]) -> int:
        if text is None:
            return NONE
        if text not in self.offsets:
            self.offsets[text] = len(self.data)
            self.data += text.encode("utf-8") + b"\0"
        return self.offsets[text]


def _place(buffer: bytearray, data: bytes) -> int:
    """data appended at the buffer's next aligned offset: that offset"""
    buffer += b"\0" * (align(len(buffer)) - len(buffer))
    offset = len(buffer)
    buffer += data
    return offset


def _place_data(buffer: bytearray, data: bytes, seen: dict, share: bool) -> int:
    """_place, or (share: the cache) the offset of equal bytes already placed"""
    if share and data in seen:
        return seen[data]
    offset = _place(buffer, data)
    if share:
        seen[data] = offset
    return offset


def _encode_tags(tag_list, names, share=False):
    """the TAGS, TAG_BYTES, FIXUPS, PARTS, GEOMETRY, RESOURCES and RESOURCE_DATA sections of a tag list; names: each
    tag's (name, source path) STRINGS offsets. share: equal geometry and resource bytes are stored once (the cache)"""
    tags, tag_bytes, fixups, parts, geometry, resources, resource_data = (bytearray() for _ in range(7))
    shared_data = {}
    shared_geometry = {}
    fixup_count = part_count = resource_count = 0
    for index, tag in enumerate(tag_list):
        # (an external tag's bytes are written too, never dropped: the reader refuses them)
        bytes_offset = _place(tag_bytes, tag.bytes) if tag.bytes else 0
        # (each tag's fixups by offset, as ae_pack_write sorts them)
        for fixup in sorted(tag.fixups, key=lambda fixup: (fixup.offset, fixup.kind, fixup.value, fixup.pad)):
            fixups += FIXUP_RECORD.pack(fixup.offset, fixup.kind, fixup.pad, fixup.value)
        for part in tag.parts:
            vertices_offset = _place_data(geometry, part.vertices, shared_geometry, share)
            indices_offset = _place_data(geometry, part.indices, shared_geometry, share)
            parts += PART_RECORD.pack(part.offset, part.vertex_type, part.triangle_type, part.vertex_count,
                                      part.triangle_count, vertices_offset, len(part.vertices), indices_offset,
                                      len(part.indices))
        for resource in tag.resources:
            stored = zlib.compress(resource.data, 9) if resource.compression == ZLIB else resource.data
            data_offset = _place_data(resource_data, stored, shared_data, share)
            resources += RESOURCE_RECORD.pack(index if resource.tag is None else resource.tag, resource.map_offset,
                                              len(resource.data) if resource.size is None else resource.size,
                                              data_offset, len(stored), resource.compression)
        tags += TAG_RECORD.pack(tag.group, tag.parent_groups[0], tag.parent_groups[1], names[index][0], tag.kind,
                                tag.required, tag.flags, tag.pad, names[index][1], bytes_offset,
                                len(tag.bytes), fixup_count, len(tag.fixups), part_count,
                                len(tag.parts), resource_count, len(tag.resources))
        fixup_count += len(tag.fixups)
        part_count += len(tag.parts)
        resource_count += len(tag.resources)
    return tags, tag_bytes, fixups, parts, geometry, resources, resource_data


def build(pack: Pack) -> bytes:
    """the pack's file, laid out as ae_pack_write lays it out (docs/weapon-packs.md "Layout"), its SHA-256 set"""
    strings = _Strings()
    names = []
    for tag in pack.tags:
        names.append((strings.add(tag.name), strings.add(tag.source_path)))
    weapon_strings = []
    for weapon in pack.weapons:
        weapon_strings.append([strings.add(weapon.key), strings.add(weapon.label),
                               strings.add(weapon.fallback_labels[0]), strings.add(weapon.fallback_labels[1]),
                               strings.add(weapon.menu_name), strings.add(weapon.weapon_pickup_text),
                               strings.add(weapon.ammo_pickup_text)])

    tags, tag_bytes, fixups, parts, geometry, resources, resource_data = _encode_tags(pack.tags, names)
    weapons = bytearray()
    for weapon, text in zip(pack.weapons, weapon_strings):
        weapons += WEAPON_RECORD.pack(text[0], weapon.root_tag, weapon.item_collection_tag, weapon.behaviour, text[1],
                                      text[2], text[3], text[4], text[5], text[6], weapon.icon_bitmap_tag,
                                      weapon.icon_sequence, weapon.replaces, weapon.power)
    contents = {STRINGS: bytes(strings.data), TAGS: bytes(tags), TAG_BYTES: bytes(tag_bytes), FIXUPS: bytes(fixups),
                PARTS: bytes(parts), GEOMETRY: bytes(geometry), RESOURCES: bytes(resources),
                RESOURCE_DATA: bytes(resource_data), WEAPONS: bytes(weapons),
                CREDITS: pack.credits.encode("utf-8")}

    kinds = sorted(contents)
    image = bytearray(HEADER_SIZE + len(kinds) * SECTION_ENTRY_SIZE)
    entries = []
    for kind in kinds:
        inflated = contents[kind]
        compression = ZLIB if kind in pack.compressed_sections else STORED
        stored = zlib.compress(inflated, 9) if compression == ZLIB else inflated
        offset = _place(image, stored)
        entries.append((kind, offset, len(stored), len(inflated), compression))
    pack_id = pack.pack_id.encode("ascii")
    display_name = pack.display_name.encode("ascii")
    # (as ae_pack_write: a pack with no weapons is one of tags only, and says so)
    flags = pack.flags | (0 if pack.weapons else FLAG_NO_WEAPONS)
    image[:HEADER_SIZE] = HEADER.pack(MAGIC, pack.format_version, pack.header_size, flags, pack_id,
                                      pack.pack_revision, display_name, pack.tag_layout, b"\0" * 32, len(entries),
                                      b"\0" * 128)
    for index, entry in enumerate(entries):
        SECTION_ENTRY.pack_into(image, HEADER_SIZE + index * SECTION_ENTRY_SIZE, *entry)
    rehash(image)
    return bytes(image)


def digest(image) -> bytes:
    """the file's SHA-256 with its own 32 bytes zeroed"""
    hasher = hashlib.sha256()
    hasher.update(image[:SHA256_OFFSET])
    hasher.update(b"\0" * min(SHA256_SIZE, max(0, len(image) - SHA256_OFFSET)))
    hasher.update(image[SHA256_OFFSET + SHA256_SIZE:])
    return hasher.digest()


def rehash(image: bytearray) -> None:
    """sets the header's SHA-256 (after the tests change bytes)"""
    image[SHA256_OFFSET:SHA256_OFFSET + SHA256_SIZE] = digest(image)


def section_entries(image) -> List[Tuple[int, int, int, int, int]]:
    """the section table as written, unchecked: (kind, offset, stored size, inflated size, compression)"""
    count = struct.unpack_from("<I", image, 0x7C)[0]
    return [SECTION_ENTRY.unpack_from(image, HEADER_SIZE + index * SECTION_ENTRY_SIZE) for index in range(count)]


def section_entry_offset(image, kind: int) -> int:
    """where the kind's section entry is in the file"""
    for index, entry in enumerate(section_entries(image)):
        if entry[0] == kind:
            return HEADER_SIZE + index * SECTION_ENTRY_SIZE
    raise KeyError(kind)


def section_offset(image, kind: int) -> int:
    """where the kind's (stored) section starts in the file"""
    return next(entry[1] for entry in section_entries(image) if entry[0] == kind)


# ---------- the reader (the engine's checks, ae_pack_format.c)

class Refused(Exception):
    """a pack refused: the reason"""


@dataclass
class Parsed:
    header: tuple
    file_size: int
    sha256: bytes
    entries: list
    sections: dict
    tags: list
    fixups: list
    parts: list
    resources: list
    weapons: list
    strings: bytes
    credits: bytes
    own_tag_count: int


def inflate_exact(data: bytes, size: int) -> Optional[bytes]:
    """a zlib stream inflated: exactly size bytes, the stream ended, nothing after it; None if not"""
    stream = zlib.decompressobj()
    try:
        out = stream.decompress(data, size + 1)
    except zlib.error:
        return None
    if len(out) != size or not stream.eof or stream.unused_data or stream.unconsumed_tail:
        return None
    return out


def _printable(data: bytes) -> bool:
    return all(0x20 <= byte <= 0x7E for byte in data)


def _utf8(data: bytes) -> bool:
    try:
        data.decode("utf-8", errors="strict")
    except UnicodeDecodeError:
        return False
    return True


def _string(parsed_strings: bytes, offset: int, what: str) -> bytes:
    if offset >= len(parsed_strings):
        raise Refused(f"{what}: string {offset} past STRINGS")
    return parsed_strings[offset:parsed_strings.index(b"\0", offset)]


def _check_padded(field_bytes: bytes, maximum: int, allowed, what: str) -> None:
    end = field_bytes.find(b"\0")
    if end < 1 or end > maximum or any(field_bytes[end:]) or not all(allowed(byte) for byte in field_bytes[:end]):
        raise Refused(f"bad {what}")


def _pickup_format(text: bytes, ammo: bool) -> bool:
    """a pickup text the engine can fill safely: the weapon's has no "%"; the ammo's exactly one, a "%d" """
    percents = [index for index, byte in enumerate(text) if byte == 0x25]
    if any(text[index + 1:index + 2] != b"d" for index in percents):
        return False
    return len(percents) == (1 if ammo else 0)


def _is_key_character(byte: int) -> bool:
    return 0x61 <= byte <= 0x7A or 0x30 <= byte <= 0x39 or byte == 0x5F


PACK_LIMITS = {'tags': MAXIMUM_TAGS, 'own_tags': MAXIMUM_OWN_TAGS, 'resources_size': MAXIMUM_RESOURCES_SIZE}


def _check_tables(strings, tags, fixups, parts, resources, tag_bytes, geometry, resource_data, limits):
    """the TAGS, FIXUPS, PARTS and RESOURCES records checked against their sections (a pack and a cache share them);
    the own tag count"""
    own = 0
    bytes_cursor = 0
    cursors = [0, 0, 0]
    sums = [0, 0, 0]
    totals = [len(fixups), len(parts), len(resources)]
    for index, tag in enumerate(tags):
        (tag_group, _, _, name, kind, required, tag_flags, pad, source_path, bytes_offset, bytes_size,
         *ranges) = tag
        what = f"tag {index}"
        name_text = _string(strings, name, what)
        if not name_text or len(name_text) > MAXIMUM_NAME or not _printable(name_text):
            raise Refused(f"{what}: bad name")
        source = _string(strings, source_path, what)
        if len(source) > MAXIMUM_NAME or not _utf8(source):
            raise Refused(f"{what}: bad source path")
        if tag_group in (0, NONE):
            raise Refused(f"{what}: bad group")
        if kind not in (OWN, EXTERNAL) or pad:
            raise Refused(f"{what}: bad kind")
        if tag_flags & ~KNOWN_FLAGS:
            raise Refused(f"{what}: unknown flag bits")
        if tag_flags and (kind != OWN or tag_group != GROUP_BITMAP):
            raise Refused(f"{what}: flags on a tag not an own bitmap")
        if kind == OWN:
            own += 1
            if required:
                raise Refused(f"{what}: required set on an own tag")
            if bytes_size < 1 or bytes_size > MAXIMUM_TAG_SIZE:
                raise Refused(f"{what}: bad size")
            if bytes_offset > len(tag_bytes) or bytes_size > len(tag_bytes) - bytes_offset:
                raise Refused(f"{what}: bytes past TAG_BYTES")
            if bytes_offset < bytes_cursor:
                raise Refused(f"{what}: bytes overlap the previous tag's")
            bytes_cursor = bytes_offset + bytes_size
        else:
            if required not in (0, 1):
                raise Refused(f"{what}: bad required")
            if bytes_offset or bytes_size or ranges[1] or ranges[3] or ranges[5]:
                raise Refused(f"{what}: an external tag with contents")
        for which in range(3):
            first, count = ranges[which * 2], ranges[which * 2 + 1]
            if first > totals[which] or count > totals[which] - first:
                raise Refused(f"{what}: records past their section")
            if count:
                if first < cursors[which]:
                    raise Refused(f"{what}: records out of order")
                cursors[which] = first + count
            sums[which] += count
    if own > limits['own_tags']:
        raise Refused("too many own tags")
    if sums != totals:
        raise Refused("records no tag owns")
    keys = set()
    for index, tag in enumerate(tags):
        key = (tag[0], _string(strings, tag[3], ""))
        if key in keys:
            raise Refused(f"tag {index}: a second tag of that group and name")
        keys.add(key)

    resources_size = 0
    for index, tag in enumerate(tags):
        if tag[4] != OWN:
            continue
        size = tag[10]
        what = f"tag {index}"
        fixups_first, fixups_count, parts_first, parts_count, resources_first, resources_count = tag[11:17]
        end = None
        for fixup in fixups[fixups_first:fixups_first + fixups_count]:
            offset, fixup_kind, pad, value = fixup
            if pad:
                raise Refused(f"{what}: bad fixup")
            if fixup_kind == FIXUP_POINTER:
                if size < 4 or offset > size - 4 or value >= size:
                    raise Refused(f"{what}: pointer fixup past its tag")
            elif fixup_kind == FIXUP_REFERENCE:
                if size < TAG_REFERENCE_SIZE or offset > size - TAG_REFERENCE_SIZE:
                    raise Refused(f"{what}: reference fixup past its tag")
                if value >= len(tags) and value != NONE:
                    raise Refused(f"{what}: reference fixup past the tags")
            elif fixup_kind == FIXUP_TAG_INDEX:
                if size < 4 or offset > size - 4:
                    raise Refused(f"{what}: tag index fixup past its tag")
                if value >= len(tags):
                    raise Refused(f"{what}: tag index fixup past the tags")
            else:
                raise Refused(f"{what}: unknown fixup kind")
            if end is not None and offset < end:
                raise Refused(f"{what}: fixups overlap or out of order")
            end = offset + (TAG_REFERENCE_SIZE if fixup_kind == FIXUP_REFERENCE else 4)
        previous = None
        for part in parts[parts_first:parts_first + parts_count]:
            (offset, vertex_type, triangle_type, vertex_count, triangle_count, vertices_offset, vertices_size,
             indices_offset, indices_size) = part
            if size < MODEL_PART_SIZE or offset > size - MODEL_PART_SIZE:
                raise Refused(f"{what}: part past its tag")
            if previous is not None and offset < previous + MODEL_PART_SIZE:
                raise Refused(f"{what}: parts overlap or out of order")
            previous = offset
            if vertex_type not in MODEL_VERTEX_TYPES or triangle_type not in (TRIANGLES, STRIP):
                raise Refused(f"{what}: bad part type")
            if not 1 <= vertex_count <= MAXIMUM_PART_COUNT or not 1 <= triangle_count <= MAXIMUM_PART_COUNT:
                raise Refused(f"{what}: bad part count")
            index_count = 3 * triangle_count if triangle_type == TRIANGLES else triangle_count + 2
            if vertices_size != vertex_count * VERTEX_SIZES[vertex_type] or indices_size != index_count * 2:
                raise Refused(f"{what}: part sizes")
            if (vertices_offset > len(geometry) or vertices_size > len(geometry) - vertices_offset
                    or indices_offset > len(geometry) or indices_size > len(geometry) - indices_offset):
                raise Refused(f"{what}: part past GEOMETRY")
            indices = struct.unpack_from(f"<{index_count}H", geometry, indices_offset)
            if max(indices) >= vertex_count:
                raise Refused(f"{what}: part index past its vertices")
        for resource in resources[resources_first:resources_first + resources_count]:
            resource_tag, map_offset, resource_size, data_offset, stored_size, compression = resource
            if resource_tag != index:
                raise Refused(f"{what}: a resource of another tag")
            if resource_size < 1 or resource_size > MAXIMUM_RESOURCE_SIZE:
                raise Refused(f"{what}: resource too large")
            if map_offset > NONE - resource_size:
                raise Refused(f"{what}: bad resource offset")
            if compression == STORED:
                if stored_size != resource_size:
                    raise Refused(f"{what}: stored resource sizes differ")
            elif compression == ZLIB:
                if stored_size < 1:
                    raise Refused(f"{what}: empty zlib resource")
            else:
                raise Refused(f"{what}: bad resource compression")
            if data_offset > resource_data[1] or stored_size > resource_data[1] - data_offset:
                raise Refused(f"{what}: resource past RESOURCE_DATA")
            resources_size += resource_size
            if resources_size > limits['resources_size']:
                raise Refused("resources larger than the limit")
    return own


def read(image: bytes, verify_resources: bool = True) -> Parsed:
    """the pack checked as the engine checks it (ae_pack_open, then ae_pack_verify_resources): Refused if not"""
    if len(image) > MAXIMUM_FILE_SIZE:
        raise Refused("file larger than 64 MiB")
    if len(image) < HEADER_SIZE:
        raise Refused("truncated: no header")
    header = HEADER.unpack_from(image, 0)
    (magic, format_version, header_size, flags, pack_id, pack_revision, display_name, tag_layout, sha256,
     section_count, reserved) = header
    if magic != MAGIC:
        raise Refused("not a weapon pack")
    if format_version != FORMAT_VERSION:
        raise Refused(f"format version {format_version}")
    if header_size != HEADER_SIZE:
        raise Refused("bad header size")
    if flags & ~KNOWN_HEADER_FLAGS:
        raise Refused("unknown header flags")
    _check_padded(pack_id, MAXIMUM_KEY, _is_key_character, "pack id")
    if pack_revision < 1:
        raise Refused("bad revision")
    _check_padded(display_name, 31, lambda byte: 0x20 <= byte <= 0x7E, "display name")
    if tag_layout != TAG_LAYOUT_XBOX:
        raise Refused("unknown tag layout")
    if section_count < 1 or section_count > MAXIMUM_SECTIONS:
        raise Refused("bad section count")
    if any(reserved):
        raise Refused("reserved header bytes set")
    table_end = HEADER_SIZE + section_count * SECTION_ENTRY_SIZE
    if len(image) < table_end:
        raise Refused("truncated: no section table")
    file_size = len(image)
    entries = section_entries(image)
    seen = set()
    for kind, offset, stored, inflated, compression in entries:
        if kind not in SECTION_NAMES or kind in seen:
            raise Refused("unknown or repeated section")
        seen.add(kind)
        if compression not in (STORED, ZLIB) or (compression == ZLIB and kind == RESOURCE_DATA):
            raise Refused("bad compression")
        if compression == STORED and inflated != stored:
            raise Refused("stored section sizes differ")
        if compression == ZLIB and stored < 1:
            raise Refused("empty zlib section")
        if offset % ALIGNMENT or offset < table_end:
            raise Refused("misplaced section")
        if offset > file_size or stored > file_size - offset:
            raise Refused("section past the end of the file")
        maximum, record = SECTION_LIMITS[kind]
        if inflated > maximum:
            raise Refused(f"{SECTION_NAMES[kind]} too large")
        if inflated % record:
            raise Refused(f"{SECTION_NAMES[kind]} not whole records")
    for index, first in enumerate(entries):
        for second in entries[index + 1:]:
            if first[2] and second[2] and first[1] < second[1] + second[2] and second[1] < first[1] + first[2]:
                raise Refused("overlapping sections")
    if digest(image) != sha256:
        raise Refused("SHA-256 mismatch")

    sections = {kind: b"" for kind in SECTION_NAMES}
    resource_data = (0, 0)
    for kind, offset, stored, inflated, compression in entries:
        if kind == RESOURCE_DATA:
            resource_data = (offset, stored)
            continue
        data = bytes(image[offset:offset + stored])
        if compression == ZLIB:
            data = inflate_exact(data, inflated)
            if data is None:
                raise Refused(f"{SECTION_NAMES[kind]}: inflated size lie or bad zlib stream")
        sections[kind] = data

    strings = sections[STRINGS]
    if strings and strings[-1] != 0:
        raise Refused("STRINGS not terminated")
    tags = [TAG_RECORD.unpack_from(sections[TAGS], offset) for offset in range(0, len(sections[TAGS]),
                                                                                  TAG_RECORD.size)]
    fixups = [FIXUP_RECORD.unpack_from(sections[FIXUPS], offset)
              for offset in range(0, len(sections[FIXUPS]), FIXUP_RECORD.size)]
    parts = [PART_RECORD.unpack_from(sections[PARTS], offset) for offset in range(0, len(sections[PARTS]),
                                                                                    PART_RECORD.size)]
    resources = [RESOURCE_RECORD.unpack_from(sections[RESOURCES], offset)
                  for offset in range(0, len(sections[RESOURCES]), RESOURCE_RECORD.size)]
    weapons = [WEAPON_RECORD.unpack_from(sections[WEAPONS], offset)
               for offset in range(0, len(sections[WEAPONS]), WEAPON_RECORD.size)]
    tag_bytes = sections[TAG_BYTES]
    geometry = sections[GEOMETRY]
    if not tags:
        raise Refused("no tags")
    # (a pack of tags only says so in its header; every other pack has weapons)
    if flags & FLAG_NO_WEAPONS:
        if weapons:
            raise Refused("header flags say tags only, but it has weapons")
        # (the MCC gate is a weapon slot's: a pack of tags only is never a direct port)
        if flags & FLAG_DIRECT_PORT:
            raise Refused("header flags say tags only and direct port")
    elif not weapons:
        raise Refused("no weapons")
    if len(tags) > MAXIMUM_TAGS:
        raise Refused("too many tags")

    own = _check_tables(strings, tags, fixups, parts, resources, tag_bytes, geometry, resource_data, PACK_LIMITS)

    weapon_keys = set()
    for index, weapon in enumerate(weapons):
        (key, root_tag, items, behaviour, label, fallback0, fallback1, menu_name, pickup, ammo, icon, sequence,
         replaces, power) = weapon
        what = f"weapon {index}"
        key_text = _string(strings, key, what)
        if not 1 <= len(key_text) <= MAXIMUM_KEY or not all(_is_key_character(byte) for byte in key_text):
            raise Refused(f"{what}: bad key")
        if key_text in weapon_keys:
            raise Refused(f"{what}: a second weapon of that key")
        weapon_keys.add(key_text)
        if root_tag >= len(tags) or tags[root_tag][4] != OWN or tags[root_tag][0] != GROUP_WEAPON:
            raise Refused(f"{what}: bad root tag")
        if items != NONE and (items >= len(tags) or tags[items][0] != GROUP_ITEM_COLLECTION):
            raise Refused(f"{what}: bad item collection")
        if behaviour not in range(MAXIMUM_BEHAVIOUR + 1) or power not in (0, 1) or replaces not in REPLACEABLE:
            raise Refused(f"{what}: bad behaviour, power or replaces")
        for which, value in enumerate((label, fallback0, fallback1)):
            if value == NONE and which:
                continue
            text = _string(strings, value, what) if value != NONE else b""
            if not 1 <= len(text) <= MAXIMUM_LABEL or not _printable(text):
                raise Refused(f"{what}: bad label")
        text = _string(strings, menu_name, what) if menu_name != NONE else b""
        if not 1 <= len(text) <= MAXIMUM_MENU_NAME or not _utf8(text):
            raise Refused(f"{what}: bad menu name")
        for value, is_ammo in ((pickup, False), (ammo, True)):
            if value == NONE:
                continue
            text = _string(strings, value, what)
            if not 1 <= len(text) <= MAXIMUM_PICKUP_TEXT or not _utf8(text) or not _pickup_format(text, is_ammo):
                raise Refused(f"{what}: bad {'ammo' if is_ammo else 'weapon'} pickup text")
        if icon == NONE:
            if sequence:
                raise Refused(f"{what}: an icon sequence without an icon")
        elif icon >= len(tags) or tags[icon][0] != GROUP_BITMAP or sequence > MAXIMUM_ICON_SEQUENCE:
            raise Refused(f"{what}: bad icon")

    credits = sections[CREDITS]
    if b"\0" in credits or not _utf8(credits):
        raise Refused("bad credits")

    parsed = Parsed(header, file_size, sha256, entries, sections, tags, fixups, parts, resources, weapons, strings,
                    credits, own)
    if verify_resources:
        for resource in resources:
            if read_resource_data(image, parsed, resource) is None:
                raise Refused(f"resource of tag {resource[0]}: inflated size lie or bad zlib stream")
    return parsed


def read_resource_data(image: bytes, parsed: Parsed, resource) -> Optional[bytes]:
    """a resource's bytes (inflated), or None if its stream lies"""
    offset = next(entry[1] for entry in parsed.entries if entry[0] == RESOURCE_DATA)
    _, _, size, data_offset, stored_size, compression = resource
    data = bytes(image[offset + data_offset:offset + data_offset + stored_size])
    return data if compression == STORED else inflate_exact(data, size)


# ---------- what ae_pack_check prints

def _group_text(value: int) -> str:
    if value == NONE:
        return "-"
    raw = struct.pack(">I", value)
    return raw.decode("ascii") if _printable(raw) else f"0x{value:08x}"


def _flags_text(flags: int) -> str:
    names = [name for bit, name in ((FLAG_SMOOTH, "smooth"), (FLAG_METER_COVERAGE, "coverage")) if flags & bit]
    return ",".join(names) or "-"


def _index_text(value: int) -> str:
    return "NONE" if value == NONE else str(value)


def describe(parsed: Parsed) -> List[str]:
    """the lines ae_pack_check prints for an accepted pack (after its "pack <path>" line, before the verdict)"""
    strings = parsed.strings

    def text(offset: int) -> str:
        return "-" if offset == NONE else _string(strings, offset, "").decode("utf-8")

    header = parsed.header
    lines = [f"format {header[1]} header {header[2]} flags {header[3]} layout {header[7]} size {parsed.file_size}",
             f"id {header[4].rstrip(bytes(1)).decode('ascii')} revision {header[5]} "
             f"name \"{header[6].rstrip(bytes(1)).decode('ascii')}\"",
             f"sha256 {parsed.sha256.hex()}"]
    for kind, offset, stored, inflated, compression in parsed.entries:
        lines.append(f"section {SECTION_NAMES[kind]} offset {offset} stored {stored} inflated {inflated} "
                     f"compression {compression}")
    for index, tag in enumerate(parsed.tags):
        lines.append(f"tag {index} {_group_text(tag[0])} parents {_group_text(tag[1])},{_group_text(tag[2])} "
                     f"{'own' if tag[4] == OWN else 'external'} required {tag[5]} flags {_flags_text(tag[6])} "
                     f"bytes {tag[10]} at {tag[9]} fixups {tag[12]} from {tag[11]} parts {tag[14]} from {tag[13]} "
                     f"resources {tag[16]} from {tag[15]} name \"{text(tag[3])}\" source \"{text(tag[8])}\"")
    for index, weapon in enumerate(parsed.weapons):
        icon = "NONE" if weapon[10] == NONE else f"{weapon[10]}:{weapon[11]}"
        lines.append(f"weapon {index} key {text(weapon[0])} root {weapon[1]} items {_index_text(weapon[2])} "
                     f"behaviour {weapon[3]} label {text(weapon[4])} fallbacks {text(weapon[5])},{text(weapon[6])} "
                     f"menu \"{text(weapon[7])}\" pickup \"{text(weapon[8])}\" ammo \"{text(weapon[9])}\" "
                     f"icon {icon} replaces {weapon[12]} power {weapon[13]}")
    resources_size = sum(resource[2] for resource in parsed.resources)
    lines.append(f"credits {len(parsed.credits)} bytes")
    lines.append(f"resources {len(parsed.resources)} of {resources_size} bytes")
    return lines


def check(image: bytes, path: str) -> Tuple[bool, List[str]]:
    """(accepted, the lines ae_pack_check prints for the pack, its verdict last)"""
    try:
        parsed = read(image)
    except Refused as refusal:
        return False, [f"pack {path}", f"{path}: refused: {refusal}"]
    return True, [f"pack {path}", *describe(parsed), f"{path}: ok"]


def main(arguments: List[str]) -> int:
    if len(arguments) < 2 or arguments[0] != "check":
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    status = 0
    for path in arguments[1:]:
        with open(path, "rb") as file:
            accepted, lines = check(file.read(MAXIMUM_FILE_SIZE + 1), path)
        print("\n".join(lines))
        status |= 0 if accepted else 1
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))


# ======================================================================================================
# The weapon cache (`.aecache`, format v1): port/linux/game/ae_weapon_cache_format.h in Python. The writer is the
# linker's (tools/weapon_cache/link.py); the reader holds a file to the engine's rules, as `read` does for a pack.
# tools/test_weapon_cache_format.py holds this to the C reader (ae_pack_format.c, T2-1).
#
# Two points the freeze (T0) left open, settled here and to be mirrored by the C reader:
#   - DIRECTORY and EXTERNS start with a 16-byte prelude of four little-endian words: the record count, the length
#     of the tail list (root list / extern entry list, in words), then two zero words. The records follow, then the
#     tail list. (Without the count the records' end is not derivable from the section's size.)
#   - A section's 16-byte alignment and the section table (20-byte entries after the 0x200 header) are v1's.
# ======================================================================================================

CACHE_MAGIC = b"AEWCACHE"
CACHE_FORMAT_VERSION = 1
CACHE_HEADER_SIZE = 0x200
CACHE_SHA256_OFFSET = 0x38
CACHE_MAXIMUM_SECTIONS = 16
CACHE_MAXIMUM_PARTS = 4
FLAG_CACHE_PART = 1
PART_OPTIONAL = 1
DIRECTORY, CONFIGS, EXTERNS = 11, 12, 13
CACHE_SECTION_NAMES = {**SECTION_NAMES, DIRECTORY: "DIRECTORY", CONFIGS: "CONFIGS", EXTERNS: "EXTERNS"}

KIND_WEAPON, KIND_TAG_SET, KIND_GRENADE, KIND_BASE = 1, 2, 3, 4
ENTRY_POWER, ENTRY_NEEDS_MCC_INSTALL, ENTRY_RETIRED = 1, 2, 4
KNOWN_ENTRY_FLAGS = 7
BEHAVIOUR_LAST = 5
REPLACES_MAP_LAST = 2
OVERRIDE_KNOWN = 0x7FF
OVERRIDE_HUD, OVERRIDE_MENU_NAME, OVERRIDE_BEHAVIOUR = 1 << 9, 1 << 10, 1 << 8
KIND_OF_ID_BYTE = {0x00: KIND_WEAPON, 0x02: KIND_TAG_SET, 0x03: KIND_GRENADE, 0x04: KIND_BASE}
CONFIG_ID_BYTE = 0x01
GROUP_WPHI = 0x77706869

CACHE_PART = struct.Struct("<16sII2I32s")
CACHE_HEADER_HEAD = struct.Struct("<8s4I2I2I16s32s2I")  # up to and including part_count (0x60 bytes)
DIRECTORY_RECORD = struct.Struct("<IBBH14I5B3x5I2H")
CONFIG_RECORD = struct.Struct("<5I3I4fIf4f3I3I")
EXTERN_RECORD = struct.Struct("<5I")
PRELUDE = struct.Struct("<4I")
assert CACHE_PART.size == 0x40 and CACHE_HEADER_HEAD.size == 0x60
assert DIRECTORY_RECORD.size == 96 and CONFIG_RECORD.size == 96 and EXTERN_RECORD.size == 20

# (the freeze names no cache STRINGS limit; v1's 256 KiB holds 2,500 names, the cache has 3,000+ tag paths)
CACHE_MAXIMUM_STRINGS = 4 * MiB
CACHE_LIMITS = {"tags": 8192, "own_tags": 8192, "resources_size": 768 * MiB}
CACHE_SECTION_LIMITS = {
    STRINGS: (CACHE_MAXIMUM_STRINGS, 1), TAGS: (8192 * TAG_RECORD.size, TAG_RECORD.size),
    TAG_BYTES: (48 * MiB, 1), FIXUPS: (1048576 * FIXUP_RECORD.size, FIXUP_RECORD.size),
    PARTS: (16384 * PART_RECORD.size, PART_RECORD.size), GEOMETRY: (64 * MiB, 1),
    RESOURCES: (MAXIMUM_RESOURCES * 64 * RESOURCE_RECORD.size, RESOURCE_RECORD.size),
    RESOURCE_DATA: (1024 * MiB, 1), WEAPONS: (0, WEAPON_RECORD.size), CREDITS: (MAXIMUM_CREDITS, 1),
    DIRECTORY: (16 + 1024 * 96 + 4096 * 4, 4), CONFIGS: (4096 * 96, 96), EXTERNS: (16 + 8192 * 20 + 65536 * 4, 4),
}
CACHE_MAXIMUM_FILE_SIZE = 1024 * MiB
MAXIMUM_DIRECTORY = 1024
CACHE_MAXIMUM_WEAPONS = 128
CACHE_MAXIMUM_CONFIGS = 4096
MAXIMUM_ROOT_LIST = 4096


@dataclass
class DirectoryEntry:
    """one registry entry in the cache; tags are cache tag indices (NONE: none), strings plain text (None: none)"""
    id: int
    kind: int
    key: str
    display: str
    part: int = 0
    flags: int = 0
    first_tag: int = 0
    tag_count: int = 0
    root_tag: int = NONE
    item_collection_tag: int = NONE
    label: Optional[str] = None
    fallback_labels: Tuple[Optional[str], Optional[str]] = (None, None)
    menu_name: Optional[str] = None
    weapon_pickup_text: Optional[str] = None
    ammo_pickup_text: Optional[str] = None
    icon_bitmap_tag: int = NONE
    icon_sequence: int = 0
    behaviour: int = 0
    stock_fallback: int = 0
    replaces_map_own: int = 0
    legacy_slot: int = 0
    grenade_catalogue_id: int = 0
    projectile_tag: int = NONE
    equipment_tag: int = NONE
    collection_tag: int = NONE
    hud_tag: int = NONE
    field_effect_tag: int = NONE
    root_tags: List[int] = field(default_factory=list)  # tag sets: the root tags (DIRECTORY's tail list)


@dataclass
class ConfigEntry:
    config_id: int
    weapon_id: int
    key: str
    display: Optional[str] = None
    override_mask: int = 0
    rounds_loaded_maximum: int = 0
    rounds_total_initial: int = 0
    rounds_total_maximum: int = 0
    rate_of_fire: Tuple[float, float] = (0.0, 0.0)
    error_angle: Tuple[float, float] = (0.0, 0.0)
    zoom_level_count: int = 0
    magnification_maximum: float = 0.0
    autoaim: Tuple[float, float] = (0.0, 0.0)  # angle, range
    magnetism: Tuple[float, float] = (0.0, 0.0)
    behaviour: int = 0
    hud_interface_tag: int = NONE
    menu_name: Optional[str] = None


@dataclass
class ExternEntry:
    group: int
    name: str
    required: int
    entries: List[int] = field(default_factory=list)  # directory ids


@dataclass
class CachePart:
    name: str
    flags: int
    size: int
    sha256: bytes


@dataclass
class Cache:
    tags: List[Tag]
    directory: List[DirectoryEntry]
    configs: List[ConfigEntry] = field(default_factory=list)
    externs: List[ExternEntry] = field(default_factory=list)
    credits: str = ""
    serial: int = 1
    schema_hash: int = 0
    registry_revision: int = 1
    ae_version: str = "0.7.6"
    flags: int = 0
    parts: List[CachePart] = field(default_factory=list)
    tag_layout: int = TAG_LAYOUT_XBOX
    format_version: int = CACHE_FORMAT_VERSION
    header_size: int = CACHE_HEADER_SIZE


def cache_digest(image) -> bytes:
    """the file's SHA-256 with the 32 bytes at 0x38 zeroed"""
    hasher = hashlib.sha256()
    hasher.update(image[:CACHE_SHA256_OFFSET])
    hasher.update(b"\0" * SHA256_SIZE)
    hasher.update(image[CACHE_SHA256_OFFSET + SHA256_SIZE:])
    return hasher.digest()


def cache_rehash(image: bytearray) -> None:
    image[CACHE_SHA256_OFFSET:CACHE_SHA256_OFFSET + SHA256_SIZE] = cache_digest(image)


def cache_id(image) -> bytes:
    """the first 8 bytes of the header's SHA-256: what the network carries and the logs print"""
    return bytes(image[CACHE_SHA256_OFFSET:CACHE_SHA256_OFFSET + 8])


def build_cache(cache: Cache, check: bool = True, pad_strings_to: int = 0) -> bytes:
    """the cache's file, laid out like aepack v1 (first met strings, 16-aligned sections, all stored, equal geometry
    and resource bytes stored once), its SHA-256 set. check=False writes whatever it is given (the tests' hostile
    files); otherwise a cache without a directory is refused: a weaponless cache is never a valid artifact"""
    if check and not cache.directory:
        raise Refused("a cache needs a directory (at least one entry): refusing to write an empty one")
    strings = _Strings()
    names = [(strings.add(tag.name), strings.add(tag.source_path)) for tag in cache.tags]
    tags, tag_bytes, fixups, parts, geometry, resources, resource_data = _encode_tags(cache.tags, names, share=True)

    directory = bytearray()
    tail = []
    for entry in cache.directory:
        first = len(tail)
        tail += entry.root_tags
        directory += DIRECTORY_RECORD.pack(
            entry.id, entry.kind, entry.part, entry.flags, strings.add(entry.key), strings.add(entry.display),
            entry.first_tag, entry.tag_count, entry.root_tag, entry.item_collection_tag, strings.add(entry.label),
            strings.add(entry.fallback_labels[0]), strings.add(entry.fallback_labels[1]), strings.add(entry.menu_name),
            strings.add(entry.weapon_pickup_text), strings.add(entry.ammo_pickup_text), entry.icon_bitmap_tag,
            entry.icon_sequence, entry.behaviour, entry.stock_fallback, entry.replaces_map_own, entry.legacy_slot,
            entry.grenade_catalogue_id, entry.projectile_tag, entry.equipment_tag, entry.collection_tag,
            entry.hud_tag, entry.field_effect_tag, first if entry.root_tags else 0, len(entry.root_tags))
    directory = PRELUDE.pack(len(cache.directory), len(tail), 0, 0) + bytes(directory) + struct.pack(f"<{len(tail)}I", *tail)

    configs = bytearray()
    for c in cache.configs:
        configs += CONFIG_RECORD.pack(
            c.config_id, c.weapon_id, strings.add(c.key), strings.add(c.display), c.override_mask,
            c.rounds_loaded_maximum, c.rounds_total_initial, c.rounds_total_maximum, *c.rate_of_fire, *c.error_angle,
            c.zoom_level_count, c.magnification_maximum, *c.autoaim, *c.magnetism, c.behaviour, c.hud_interface_tag,
            strings.add(c.menu_name), 0, 0, 0)

    externs = bytearray()
    extern_tail = []
    for e in cache.externs:
        externs += EXTERN_RECORD.pack(e.group, strings.add(e.name), e.required, len(extern_tail), len(e.entries))
        extern_tail += e.entries
    externs = PRELUDE.pack(len(cache.externs), len(extern_tail), 0, 0) + bytes(externs) + struct.pack(
        f"<{len(extern_tail)}I", *extern_tail)

    if pad_strings_to > len(strings.data):  # (the boundary tests: STRINGS of an exact size, NUL padded)
        strings.data += b"\0" * (pad_strings_to - len(strings.data))
    contents = {STRINGS: bytes(strings.data), TAGS: bytes(tags), TAG_BYTES: bytes(tag_bytes), FIXUPS: bytes(fixups),
                PARTS: bytes(parts), GEOMETRY: bytes(geometry), RESOURCES: bytes(resources),
                RESOURCE_DATA: bytes(resource_data), WEAPONS: b"", CREDITS: cache.credits.encode("utf-8"),
                DIRECTORY: directory, CONFIGS: bytes(configs), EXTERNS: externs}
    kinds = sorted(contents)
    image = bytearray(CACHE_HEADER_SIZE + len(kinds) * SECTION_ENTRY_SIZE)
    entries = []
    for kind in kinds:
        data = contents[kind]
        entries.append((kind, _place(image, data), len(data), len(data), STORED))
    version = cache.ae_version.encode("ascii")
    head = CACHE_HEADER_HEAD.pack(CACHE_MAGIC, cache.format_version, cache.header_size, cache.flags, cache.serial,
                                  cache.schema_hash & 0xFFFFFFFF, cache.schema_hash >> 32, cache.tag_layout,
                                  cache.registry_revision, version, b"\0" * 32, len(entries), len(cache.parts))
    parts_blob = b"".join(CACHE_PART.pack(p.name.encode("ascii"), p.flags, 0, p.size & 0xFFFFFFFF, p.size >> 32, p.sha256)
                          for p in cache.parts)
    parts_blob += b"\0" * (CACHE_MAXIMUM_PARTS * 0x40 - len(parts_blob))
    image[:CACHE_HEADER_SIZE] = head + parts_blob + b"\0" * (CACHE_HEADER_SIZE - len(head) - len(parts_blob))
    for index, entry in enumerate(entries):
        SECTION_ENTRY.pack_into(image, CACHE_HEADER_SIZE + index * SECTION_ENTRY_SIZE, *entry)
    cache_rehash(image)
    return bytes(image)


@dataclass
class ParsedCache:
    header: dict
    file_size: int
    sha256: bytes
    entries: list
    sections: dict
    tags: list
    fixups: list
    parts: list
    resources: list
    strings: bytes
    credits: bytes
    own_tag_count: int
    directory: list
    root_list: list
    configs: list
    externs: list
    extern_list: list
    cache_parts: list


def _text_or_none(strings, offset, what, check):
    if offset == NONE:
        return None
    text = _string(strings, offset, what)
    if not check(text):
        raise Refused(f"{what}: bad string")
    return text.decode("utf-8")


def read_cache(image: bytes) -> ParsedCache:
    """the cache checked as the engine checks it: Refused if not. Every length, offset and count against the file and
    the format's limits; the tag tables through the same checks as a pack's (`_check_tables`) with the cache limits"""
    if len(image) > CACHE_MAXIMUM_FILE_SIZE:
        raise Refused("file larger than 1 GiB")
    if len(image) < CACHE_HEADER_SIZE:
        raise Refused("truncated: no header")
    (magic, format_version, header_size, flags, serial, hash_low, hash_high, tag_layout, registry_revision, version,
     sha256, section_count, part_count) = CACHE_HEADER_HEAD.unpack_from(image, 0)
    if magic != CACHE_MAGIC:
        raise Refused("not a weapon cache")
    if format_version != CACHE_FORMAT_VERSION:
        raise Refused(f"format version {format_version}")
    if header_size != CACHE_HEADER_SIZE:
        raise Refused("bad header size")
    if flags & ~FLAG_CACHE_PART:
        raise Refused("unknown header flags")
    if serial < 1:
        raise Refused("bad serial")
    if tag_layout != TAG_LAYOUT_XBOX:
        raise Refused("unknown tag layout")
    end = version.find(b"\0")
    if end < 1 or any(version[end:]) or not _printable(version[:end]):
        raise Refused("bad version text")
    if section_count < 1 or section_count > CACHE_MAXIMUM_SECTIONS or part_count > CACHE_MAXIMUM_PARTS:
        raise Refused("bad section or part count")
    cache_parts = []
    for index in range(CACHE_MAXIMUM_PARTS):
        name, part_flags, pad, size_low, size_high, part_sha = CACHE_PART.unpack_from(image, 0x60 + index * 0x40)
        if index >= part_count:
            if name.strip(b"\0") or part_flags or pad or size_low or size_high or any(part_sha):
                raise Refused("unused part record set")
            continue
        end = name.find(b"\0")
        text = name[:end] if end >= 0 else name
        if not 1 <= len(text) or any(name[len(text):]) or not all(0x61 <= c <= 0x7A or 0x30 <= c <= 0x39 or c == 0x5F
                                                               for c in text):
            raise Refused("bad part name")
        if part_flags & ~PART_OPTIONAL or pad:
            raise Refused("bad part flags")
        cache_parts.append(CachePart(text.decode(), part_flags, size_low | (size_high << 32), part_sha))
    if any(image[0x160:CACHE_HEADER_SIZE]):
        raise Refused("reserved header bytes set")
    table_end = CACHE_HEADER_SIZE + section_count * SECTION_ENTRY_SIZE
    if len(image) < table_end:
        raise Refused("truncated: no section table")
    file_size = len(image)
    entries = [SECTION_ENTRY.unpack_from(image, CACHE_HEADER_SIZE + i * SECTION_ENTRY_SIZE) for i in range(section_count)]
    seen = set()
    for kind, offset, stored, inflated, compression in entries:
        if kind not in CACHE_SECTION_NAMES or kind in seen:
            raise Refused("unknown or repeated section")
        seen.add(kind)
        if compression != STORED or stored != inflated:
            raise Refused("a cache's sections are stored")
        if offset % ALIGNMENT or offset < table_end:
            raise Refused("misplaced section")
        if offset > file_size or stored > file_size - offset:
            raise Refused("section past the end of the file")
        maximum, record = CACHE_SECTION_LIMITS[kind]
        if inflated > maximum:
            raise Refused(f"{CACHE_SECTION_NAMES[kind]} too large")
        if inflated % record:
            raise Refused(f"{CACHE_SECTION_NAMES[kind]} not whole records")
    for index, first in enumerate(entries):
        for second in entries[index + 1:]:
            if first[2] and second[2] and first[1] < second[1] + second[2] and second[1] < first[1] + first[2]:
                raise Refused("overlapping sections")
    if cache_digest(image) != sha256:
        raise Refused("SHA-256 mismatch")
    sections = {kind: b"" for kind in CACHE_SECTION_NAMES}
    for kind, offset, stored, inflated, compression in entries:
        sections[kind] = bytes(image[offset:offset + stored])

    strings = sections[STRINGS]
    if strings and strings[-1] != 0:
        raise Refused("STRINGS not terminated")
    tags = [TAG_RECORD.unpack_from(sections[TAGS], o) for o in range(0, len(sections[TAGS]), TAG_RECORD.size)]
    fixups = [FIXUP_RECORD.unpack_from(sections[FIXUPS], o) for o in range(0, len(sections[FIXUPS]), FIXUP_RECORD.size)]
    parts = [PART_RECORD.unpack_from(sections[PARTS], o) for o in range(0, len(sections[PARTS]), PART_RECORD.size)]
    resources = [RESOURCE_RECORD.unpack_from(sections[RESOURCES], o)
                 for o in range(0, len(sections[RESOURCES]), RESOURCE_RECORD.size)]
    if not tags:
        raise Refused("no tags")
    if len(tags) > CACHE_LIMITS["tags"]:
        raise Refused("too many tags")
    resource_data_size = len(sections[RESOURCE_DATA])
    own = _check_tables(strings, tags, fixups, parts, resources, sections[TAG_BYTES], sections[GEOMETRY],
                        (0, resource_data_size), CACHE_LIMITS)
    credits = sections[CREDITS]
    if b"\0" in credits or not _utf8(credits):
        raise Refused("bad credits")

    def table(kind, name, prelude_record):
        data = sections[kind]
        if not data:
            return [], []
        if len(data) < 16:
            raise Refused(f"{name}: no prelude")
        count, tail_count, z0, z1 = PRELUDE.unpack_from(data, 0)
        if z0 or z1 or len(data) != 16 + count * prelude_record.size + tail_count * 4:
            raise Refused(f"{name}: prelude disagrees with the section's size")
        records = [prelude_record.unpack_from(data, 16 + i * prelude_record.size) for i in range(count)]
        tail = list(struct.unpack_from(f"<{tail_count}I", data, 16 + count * prelude_record.size))
        return records, tail

    def string(offset, what, check=lambda t: 1 <= len(t) <= 255 and _utf8(t)):
        return _text_or_none(strings, offset, what, check)

    def key_ok(text):
        return 1 <= len(text) <= MAXIMUM_KEY and all(_is_key_character(b) for b in text)

    directory, root_list = table(DIRECTORY, "DIRECTORY", DIRECTORY_RECORD)
    if not directory:
        raise Refused("DIRECTORY empty or missing")
    if len(directory) > MAXIMUM_DIRECTORY or len(root_list) > MAXIMUM_ROOT_LIST:
        raise Refused("DIRECTORY too large")
    previous = -1
    weapons = 0
    keys = set()
    for index, rec in enumerate(directory):
        (rid, kind, part, rflags, key, display, first_tag, tag_count, root_tag, items, label, fb0, fb1, menu, pickup, ammo,
         icon, sequence, behaviour, stock, replaces, legacy, catalogue, p1, p2, p3, p4, p5, list_first, list_count) = (
            rec[0], rec[1], rec[2], rec[3], *rec[4:18], *rec[18:23], *rec[23:28], *rec[28:30])
        what = f"directory {index}"
        if rid <= previous:
            raise Refused(f"{what}: ids not ascending")
        previous = rid
        id_byte = rid >> 24
        if KIND_OF_ID_BYTE.get(id_byte) != kind:
            raise Refused(f"{what}: kind does not match the id's range")
        if part > len(cache_parts) or rflags & ~KNOWN_ENTRY_FLAGS:
            raise Refused(f"{what}: bad part or flags")
        text = string(key, what, key_ok)
        if text is None or (kind, text) in keys:
            raise Refused(f"{what}: bad or repeated key")
        keys.add((kind, text))
        if string(display, what) is None:
            raise Refused(f"{what}: no display")
        if first_tag > len(tags) or tag_count > len(tags) - first_tag:
            raise Refused(f"{what}: tags past the cache")
        for value in (root_tag, items, icon, p1, p2, p3, p4, p5):
            if value != NONE and value >= len(tags):
                raise Refused(f"{what}: tag index past the cache")
        if list_first > len(root_list) or list_count > len(root_list) - list_first:
            raise Refused(f"{what}: root list past its tail")
        for value in root_list[list_first:list_first + list_count]:
            if value >= len(tags):
                raise Refused(f"{what}: root tag past the cache")
        if kind == KIND_WEAPON:
            weapons += 1
            if root_tag >= len(tags) or tags[root_tag][4] != OWN or tags[root_tag][0] != GROUP_WEAPON:
                raise Refused(f"{what}: bad root tag")
            if items != NONE and tags[items][0] != GROUP_ITEM_COLLECTION:
                raise Refused(f"{what}: bad item collection")
            if behaviour > BEHAVIOUR_LAST or replaces > REPLACES_MAP_LAST or legacy > 24 or stock not in (0, *REPLACEABLE):
                raise Refused(f"{what}: bad behaviour, replaces, legacy slot or fallback")
            for which, value in enumerate((label, fb0, fb1)):
                if value == NONE and which:
                    continue
                if string(value, what, lambda t: 1 <= len(t) <= MAXIMUM_LABEL and _printable(t)) is None:
                    raise Refused(f"{what}: bad label")
            if string(menu, what, lambda t: 1 <= len(t) <= MAXIMUM_MENU_NAME and _utf8(t)) is None:
                raise Refused(f"{what}: bad menu name")
            for value, is_ammo in ((pickup, False), (ammo, True)):
                if value != NONE and string(value, what, lambda t, a=is_ammo: 1 <= len(t) <= MAXIMUM_PICKUP_TEXT
                                            and _utf8(t) and _pickup_format(t, a)) is None:
                    raise Refused(f"{what}: bad pickup text")
            if icon == NONE:
                if sequence:
                    raise Refused(f"{what}: an icon sequence without an icon")
            elif tags[icon][0] != GROUP_BITMAP or sequence > MAXIMUM_ICON_SEQUENCE:
                raise Refused(f"{what}: bad icon")
        if kind == KIND_BASE and rid != 0x04000001:
            raise Refused(f"{what}: the base layer has one id")
    if weapons > CACHE_MAXIMUM_WEAPONS:
        raise Refused("too many weapons")

    config_data = sections[CONFIGS]
    configs = [CONFIG_RECORD.unpack_from(config_data, o) for o in range(0, len(config_data), CONFIG_RECORD.size)]
    if len(configs) > CACHE_MAXIMUM_CONFIGS:
        raise Refused("too many configs")
    weapon_ids = {rec[0] for rec in directory if rec[1] == KIND_WEAPON}
    previous = -1
    for index, c in enumerate(configs):
        what = f"config {index}"
        config_id, weapon_id, key, display, mask = c[:5]
        if config_id <= previous or config_id >> 24 != CONFIG_ID_BYTE:
            raise Refused(f"{what}: ids not ascending or out of range")
        previous = config_id
        if weapon_id not in weapon_ids:
            raise Refused(f"{what}: its weapon is not in the cache")
        if string(key, what, key_ok) is None:
            raise Refused(f"{what}: bad key")
        if display != NONE and string(display, what) is None:
            raise Refused(f"{what}: bad display")
        if mask & ~OVERRIDE_KNOWN or any(c[21:24]):
            raise Refused(f"{what}: unknown override bits or reserved words set")
        if mask & OVERRIDE_BEHAVIOUR and c[18] > BEHAVIOUR_LAST:
            raise Refused(f"{what}: bad behaviour")
        if mask & OVERRIDE_HUD and (c[19] >= len(tags) or tags[c[19]][0] != GROUP_WPHI):
            raise Refused(f"{what}: bad HUD interface")
        if mask & OVERRIDE_MENU_NAME and string(c[20], what, lambda t: 1 <= len(t) <= MAXIMUM_MENU_NAME) is None:
            raise Refused(f"{what}: bad menu name")

    externs, extern_list = table(EXTERNS, "EXTERNS", EXTERN_RECORD)
    if len(externs) > 8192 or len(extern_list) > 65536:
        raise Refused("too many externs")
    ids = {rec[0] for rec in directory}
    for index, rec in enumerate(externs):
        group, name, required, first, count = rec
        what = f"extern {index}"
        if string(name, what) is None or required not in (0, 1) or first > len(extern_list) or count > len(extern_list) - first:
            raise Refused(f"{what}: bad record")
        if any(value not in ids for value in extern_list[first:first + count]):
            raise Refused(f"{what}: names an entry not in the cache")
    header = {"format_version": format_version, "flags": flags, "serial": serial, "schema_hash": hash_low | (hash_high << 32),
              "tag_layout": tag_layout, "registry_revision": registry_revision, "ae_version": version[:end].decode(),
              "section_count": section_count, "part_count": part_count}
    return ParsedCache(header, file_size, sha256, entries, sections, tags, fixups, parts, resources, strings, credits, own,
                       directory, root_list, configs, externs, extern_list, cache_parts)


def cache_read_resource_data(image: bytes, parsed: ParsedCache, resource) -> bytes:
    offset = next(entry[1] for entry in parsed.entries if entry[0] == RESOURCE_DATA)
    _, _, size, data_offset, stored_size, _ = resource
    return bytes(image[offset + data_offset:offset + data_offset + stored_size])
