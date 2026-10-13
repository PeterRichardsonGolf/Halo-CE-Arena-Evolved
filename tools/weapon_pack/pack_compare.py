"""A pack's own tags against the map they were taken from: every byte of each tag, and every byte of its pixels,
samples, vertices and indices, but for what the pack builder blanks because the engine writes it again when it
adds the pack to a map.

A pack tag's bytes are the tag's root followed by each block's elements and each data's bytes as the builder met
them, with a POINTER fixup wherever one was pointed at. This walks the same pointers in the map (the root at the
tag's address, each piece where the map's own pointer says) and compares each piece. NOT compared, and nothing
else (each class counted):
  - pointers: a POINTER fixup's 8 bytes, the address and the definition pointer after it (a block's or a data's:
    the builder writes both 0, the fixup says where the address points);
  - references: a REFERENCE fixup's name pointer (bytes 4..8 of the 16) and tag index (bytes 12..16): the builder
    writes 0 and NONE, the fixup names the tag. The reference's group and its third word ARE compared;
  - tag_indices: a TAG_INDEX fixup's 4 bytes (NONE in the pack; this covers a sound permutation's two own tag
    indices and a bitmap's own);
  - part_buffers: a model part's two buffer structs, bytes 68..104 of its 0x68 (the PARTS record carries their
    types and counts; the pack's bytes are 0);
  - bitmap_pixel_addresses: a bitmap's pixel data address and definition, bytes 0x3C..0x44 of its root (0 in the
    pack: pixels are a resource). The pixel data's size and file offset before them ARE compared when the pack is
    held to its own donor (see "offsets" below);
  - sound_played_masks: a sound's pitch ranges' "played permutation mask", bytes 52..56 of each 72-byte range: a
    value the game sets as it plays, which the tag validator resets to 0 on every map it loads
    (tag_schema_effects.c, TAG_SCHEMA_RESET) and so did on the donor before the builder copied it; Invader writes
    -1 there. The pack's must be 0;
  - padding: the up to 3 bytes that pad a piece to the next one's 4-byte boundary, which are the pack's own (0)
    and no part of the tag: taken as padding only when the piece is a data whose size, the word 12 bytes before
    its pointer, says so.
The references' targets are compared by name: the map's tag at a reference must be the tag the fixup names (own
tags by their source path, external ones by their name).

Resources: each of a tag's resources (a bitmap's pixels, a sound's samples) is compared with the map FILE's bytes
where the map's own tag says they are, and each model part's vertices and indices with the map's buffers.

Offsets: a tag's bytes hold file offsets (a bitmap's pixel data, a sound's samples). A pack is held to the donor
it was built from with them compared (same_file). Held to ANOTHER build of the donor (the approved one, from
which a newer recipe departs in named tags), every file offset differs although the bytes there do not: those
words are then counted as "file_offsets" and not compared, and the bytes they lead to are.

Usage: pack_compare.py <pack> <map> [--other-build]      prints each difference; exit 1 if there is one
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import aepack
import donor_map

MODEL_PART_SIZE = 0x68
MODEL_PART_BUFFERS = (68, 104)
BITMAP_PIXEL_DATA = 0x30                # a bitmap's root: its pixel data (size, pad, file offset, address,
                                        # definition)
BITMAP_PIXEL_ADDRESS = (0x3C, 0x44)
BITMAP_BITMAPS = (0x60, 0x64)           # its bitmaps' count, and their address
BITMAP_DATA_SIZE = 48
BITMAP_DATA_PIXELS_OFFSET = 0x18
TAG_DATA_SIZE = 20                      # size, pad, file offset, address, definition
PART_TRIANGLE_BUFFER = 80               # a model part: where its triangle buffer's and vertex buffer's
PART_VERTEX_BUFFER = 100                # addresses in the map's buffer tables are
BITMAP_GROUP = "bitm"
SOUND_GROUP = "snd!"
SOUND_RANGES = (0x98, 0x9C)             # a sound's root: its pitch ranges' count, and their address
SOUND_RANGE_SIZE = 72
SOUND_RANGE_PLAYED = (52, 56)
MAXIMUM_PROBLEMS = 40
# what is not compared, by class
(MASK_POINTER, MASK_REFERENCE, MASK_TAG_INDEX, MASK_PART_BUFFERS, MASK_BITMAP_PIXELS, MASK_SOUND_PLAYED,
 MASK_PADDING, MASK_FILE_OFFSET) = range(1, 9)
MASK_NAMES = {MASK_POINTER: "pointers", MASK_REFERENCE: "references", MASK_TAG_INDEX: "tag_indices",
              MASK_PART_BUFFERS: "part_buffers", MASK_BITMAP_PIXELS: "bitmap_pixel_addresses",
              MASK_SOUND_PLAYED: "sound_played_masks", MASK_PADDING: "padding", MASK_FILE_OFFSET: "file_offsets"}


def _text(strings, offset):
    return strings[offset:strings.index(b"\0", offset)].decode("utf-8")


def compare_tag(tag_bytes, fixups, parts, tags, map_tag, targets=None, counts=None, file_offsets=(), walk=None):
    """the differences between a pack tag's bytes and the map's tag, as lines (none: the same). fixups: (offset,
    kind, pad, value) each; parts: the offsets of the tag's model parts; targets: for each pack tag index, the
    (group, lower-case name) the map's tag at a reference to it must have (None: references are not checked);
    counts: a dictionary that takes "compared" and "masked" byte counts and one count a class; file_offsets: the
    offsets of 4-byte file offsets not to compare (another build's); walk: a dictionary that takes, for each
    piece's start in the pack's bytes, the map's address of it ("addresses")"""
    problems = []
    masked = bytearray(len(tag_bytes))          # 0 compared, else the class of what is not (MASK_NAMES)

    def mask(start, end, kind):
        masked[start:end] = bytes((kind,)) * (end - start)

    pointers = {}
    for offset, kind, _, value in fixups:
        if kind == aepack.FIXUP_POINTER:
            mask(offset, offset + 8, MASK_POINTER)
            pointers[offset] = value
        elif kind == aepack.FIXUP_REFERENCE:
            mask(offset + 4, offset + 8, MASK_REFERENCE)
            mask(offset + 12, offset + 16, MASK_REFERENCE)
        elif kind == aepack.FIXUP_TAG_INDEX:
            mask(offset, offset + 4, MASK_TAG_INDEX)
    for offset in parts:
        mask(offset + MODEL_PART_BUFFERS[0], offset + MODEL_PART_BUFFERS[1], MASK_PART_BUFFERS)
    if map_tag.group == BITMAP_GROUP:
        mask(BITMAP_PIXEL_ADDRESS[0], BITMAP_PIXEL_ADDRESS[1], MASK_BITMAP_PIXELS)
    if map_tag.group == SOUND_GROUP and SOUND_RANGES[1] in pointers and len(tag_bytes) >= SOUND_RANGES[1]:
        ranges = pointers[SOUND_RANGES[1]]
        for index in range(struct.unpack_from("<I", tag_bytes, SOUND_RANGES[0])[0]):
            field = ranges + index * SOUND_RANGE_SIZE + SOUND_RANGE_PLAYED[0]
            if field + 4 > len(tag_bytes):
                problems.append("a pitch range past the tag's bytes")
                break
            if any(tag_bytes[field:field + 4]):
                problems.append("pitch range %d's played permutation mask is not 0 in the pack" % index)
            mask(field, field + 4, MASK_SOUND_PLAYED)
    # the pieces: the root, and what each pointer points at, each up to the next one's start
    starts = sorted({0} | set(pointers.values()))
    ends = {start: (starts[index + 1] if index + 1 < len(starts) else len(tag_bytes))
            for index, start in enumerate(starts)}
    # (a data's exact size, where a pointer's own tag_data says so: the bytes after it are padding)
    for offset, value in pointers.items():
        if offset >= 12:
            size = struct.unpack_from("<I", tag_bytes, offset - 12)[0]
            if size % 4 and value + size < ends[value] and (value + size + 3) // 4 * 4 == ends[value] and not any(
                    tag_bytes[value + size:ends[value]]):
                if counts is not None:
                    counts["padding"] = counts.get("padding", 0) + ends[value] - (value + size)
                    counts["masked"] = counts.get("masked", 0) + ends[value] - (value + size)
                ends[value] = value + size
    for offset in file_offsets:
        mask(offset, offset + 4, MASK_FILE_OFFSET)
    addresses = {0: map_tag.address}
    done = 0
    pending = [0]
    compared = hidden = 0
    while pending:
        start = pending.pop()
        end = ends[start]
        address = addresses[start]
        what = "the root" if start == 0 else "the bytes at %d" % start
        try:
            theirs = tags.read(address, end - start)
        except donor_map.Refused:
            problems.append("%s: not in the map's tags (%08x + %d)" % (what, address, end - start))
            continue
        for index in range(end - start):
            if masked[start + index]:
                hidden += 1
                if counts is not None:
                    name = MASK_NAMES[masked[start + index]]
                    counts[name] = counts.get(name, 0) + 1
            elif tag_bytes[start + index] != theirs[index]:
                if len(problems) < MAXIMUM_PROBLEMS:
                    problems.append("byte %d of %s: pack 0x%02x, map 0x%02x" % (
                        index, what, tag_bytes[start + index], theirs[index]))
            else:
                compared += 1
        for offset, value in pointers.items():
            if start <= offset < end:
                target = struct.unpack_from("<I", theirs, offset - start)[0]
                if value in addresses:
                    if addresses[value] != target:
                        problems.append("the pointer at %d: the map's leads elsewhere" % offset)
                else:
                    addresses[value] = target
                    pending.append(value)
        if targets is not None:
            for offset, kind, _, value in fixups:
                if kind in (aepack.FIXUP_REFERENCE, aepack.FIXUP_TAG_INDEX) and start <= offset < end:
                    field = offset + (12 if kind == aepack.FIXUP_REFERENCE else 0) - start
                    index = struct.unpack_from("<I", theirs, field)[0]
                    wanted = None if value == aepack.NONE else targets[value]
                    have = None
                    if index != donor_map.NONE and (index & 0xFFFF) < len(tags.tags):
                        named = tags.tags[index & 0xFFFF]
                        have = (named.group, named.name.lower())
                    if have != wanted:
                        problems.append("the reference at %d: pack %r, map %r" % (offset, wanted, have))
        done += 1
    if done != len(starts):
        problems.append("%d pieces of the tag are reached by no pointer" % (len(starts) - done))
    if counts is not None:
        counts["compared"] = counts.get("compared", 0) + compared
        counts["masked"] = counts.get("masked", 0) + hidden
    if walk is not None:
        walk["addresses"] = addresses
        walk["ends"] = ends
    return problems


def _map_address(walk, offset):
    """the map's address of a byte of the pack tag's bytes, or None"""
    for start, address in walk["addresses"].items():
        if start <= offset < walk["ends"][start]:
            return address + offset - start
    return None


def resource_fields(group, tag_bytes, fixups, resources):
    """where a tag's bytes hold each resource's file offset: {resource index: (offsets of the 4-byte words that
    make it, summed)}. A bitmap's pixels are at its pixel data's file offset plus the bitmap's own pixel offset
    (in a built map the second is the file offset itself and the first 0); a sound's samples at the file offset
    of a data field of that size"""
    pointers = {offset: value for offset, kind, _, value in fixups if kind == aepack.FIXUP_POINTER}
    found = {}
    if group == BITMAP_GROUP:
        if len(tag_bytes) < BITMAP_BITMAPS[1] + 4 or BITMAP_BITMAPS[1] not in pointers:
            return found
        base = struct.unpack_from("<I", tag_bytes, BITMAP_PIXEL_DATA + 8)[0]
        first = pointers[BITMAP_BITMAPS[1]]
        for index in range(struct.unpack_from("<I", tag_bytes, BITMAP_BITMAPS[0])[0]):
            entry = first + index * BITMAP_DATA_SIZE + BITMAP_DATA_PIXELS_OFFSET
            if entry + 4 > len(tag_bytes):
                break
            offset = base + struct.unpack_from("<I", tag_bytes, entry)[0]
            for number, (_, map_offset, _, _, _, _) in enumerate(resources):
                if map_offset == offset and number not in found:
                    found[number] = (BITMAP_PIXEL_DATA + 8, entry)
    else:
        for number, (_, map_offset, size, _, _, _) in enumerate(resources):
            for offset in range(0, len(tag_bytes) - TAG_DATA_SIZE + 1, 4):
                if struct.unpack_from("<I", tag_bytes, offset)[0] == size and struct.unpack_from(
                        "<I", tag_bytes, offset + 8)[0] == map_offset:
                    found[number] = (offset + 8,)
                    break
    return found


def compare_pack(image, map_file, same_file=True):
    """every own tag of the pack against the map's tag of its source path, its resources and model parts with it.
    map_file: donor_map.MapFile. same_file: the map is the very donor the pack was built from (file offsets
    compared). Returns {"own", "problems", "by_tag", "compared_bytes", "masked_bytes", "masked",
    "resources", "resource_bytes", "parts", "geometry_bytes"}; problems are "<tag>: <difference>" lines and by_tag
    the same grouped by the tag's source path"""
    parsed = aepack.read(image)
    tags = map_file.tags
    strings = parsed.strings
    tag_bytes = parsed.sections[aepack.TAG_BYTES]
    geometry = parsed.sections[aepack.GEOMETRY]
    targets = []
    for record in parsed.tags:
        group = struct.pack(">I", record[0]).decode("latin-1")
        name = _text(strings, record[8]) if record[4] == aepack.OWN else _text(strings, record[3])
        targets.append((group, name.lower()))
    by_tag = {}
    counts = {}
    report = {"own": 0, "resources": 0, "resource_bytes": 0, "parts": 0, "geometry_bytes": 0}

    def problem(source, text):
        by_tag.setdefault(source, []).append(text)

    for index, record in enumerate(parsed.tags):
        if record[4] != aepack.OWN:
            continue
        report["own"] += 1
        group, source = targets[index]
        map_tag = tags.find(group, source)
        if map_tag is None:
            problem(source, "the map has no such %s tag" % group)
            continue
        own_bytes = tag_bytes[record[9]:record[9] + record[10]]
        fixups = parsed.fixups[record[11]:record[11] + record[12]]
        parts = parsed.parts[record[13]:record[13] + record[14]]
        resources = parsed.resources[record[15]:record[15] + record[16]]
        fields = resource_fields(group, own_bytes, fixups, resources)
        other = [] if same_file else sorted({field for found in fields.values() for field in found})
        walk = {}
        for text in compare_tag(own_bytes, fixups, [part[0] for part in parts], tags, map_tag, targets, counts,
                                other, walk):
            problem(source, text)
        # the resources: the map file's bytes where the map's own tag says they are
        for number, resource in enumerate(resources):
            data = aepack.read_resource_data(image, parsed, resource)
            if number not in fields or data is None:
                problem(source, "resource %d: its place in the tag's bytes was not found" % number)
                continue
            offset = 0
            for field in fields[number]:
                address = _map_address(walk, field)
                if address is None or not tags.contains(address, 4):
                    offset = None
                    break
                offset += tags.u32(address)
            theirs = None if offset is None else map_file.file_bytes(offset, len(data))
            if theirs is None:
                problem(source, "resource %d: not in the map's file" % number)
            elif theirs != data:
                different = sum(1 for a, b in zip(data, theirs) if a != b)
                problem(source, "resource %d: %d of its %d bytes differ" % (number, different, len(data)))
            else:
                report["resources"] += 1
                report["resource_bytes"] += len(data)
        # the model parts: vertices and indices in the map's buffers
        for part in parts:
            offset, _, _, _, _, vertices_offset, vertices_size, indices_offset, indices_size = part
            address = _map_address(walk, offset)
            same = address is not None
            for field, start, size, what in ((PART_VERTEX_BUFFER, vertices_offset, vertices_size, "vertices"),
                                             (PART_TRIANGLE_BUFFER, indices_offset, indices_size, "indices")):
                try:
                    buffer = tags.u32(address + field)
                    theirs = tags.read(tags.u32(buffer + 4), size)
                except (donor_map.Refused, TypeError, struct.error):
                    problem(source, "the part at %d: its %s are not in the map's buffers" % (offset, what))
                    same = False
                    continue
                if bytes(theirs) != bytes(geometry[start:start + size]):
                    problem(source, "the part at %d: its %s differ" % (offset, what))
                    same = False
                else:
                    report["geometry_bytes"] += size
            report["parts"] += 1 if same else 0
    report.update({"problems": ["%s: %s" % (source, text) for source, texts in by_tag.items() for text in texts],
                   "by_tag": by_tag, "compared_bytes": counts.get("compared", 0),
                   "masked_bytes": counts.get("masked", 0),
                   "masked": {name: counts.get(name, 0) for name in MASK_NAMES.values()}})
    return report


def summary(report):
    return ("%(own)d own tags: %(compared_bytes)d tag bytes the same, %(masked_bytes)d not compared; "
            "%(resources)d resources of %(resource_bytes)d bytes and %(parts)d model parts of %(geometry_bytes)d "
            "bytes the same" % report)


def main(arguments):
    other = "--other-build" in arguments
    arguments = [argument for argument in arguments if argument != "--other-build"]
    if len(arguments) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    with open(arguments[0], "rb") as file:
        image = file.read()
    try:
        report = compare_pack(image, donor_map.MapFile(arguments[1]), same_file=not other)
    except (aepack.Refused, donor_map.Refused) as refusal:
        print("pack_compare: refused: %s" % refusal, file=sys.stderr)
        return 1
    for line in report["problems"]:
        print(line)
    print(summary(report))
    print("not compared: " + ", ".join("%s %d" % item for item in report["masked"].items()))
    return 1 if report["problems"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
