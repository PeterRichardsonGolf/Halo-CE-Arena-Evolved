"""A pack's own tags against the tags of the map they were taken from: byte for byte, but for what the pack
builder blanks because the engine writes it again when it adds the pack to a map.

A pack tag's bytes are the tag's root followed by each block's elements and each data's bytes as the builder met
them, with a POINTER fixup wherever one was pointed at. This walks the same pointers in the map (the root at the
tag's address, each piece where the map's own pointer says) and compares each piece. NOT compared, and nothing
else:
  - a POINTER fixup's 8 bytes: the address and the definition pointer after it (a block's or a data's: the
    builder writes both 0, the fixup says where the address points);
  - a REFERENCE fixup's name pointer (bytes 4..8 of the 16) and tag index (bytes 12..16): the builder writes 0 and
    NONE, the fixup names the tag. The reference's group and its third word ARE compared;
  - a TAG_INDEX fixup's 4 bytes (NONE in the pack; this covers a sound permutation's two own tag indices);
  - a model part's two buffer structs, bytes 68..104 of its 0x68 (the PARTS record carries their types and
    counts; the pack's bytes are 0);
  - a bitmap's pixel data address and definition, bytes 0x38..0x40 of its root (0 in the pack: pixels are a
    resource);
  - a sound's pitch ranges' "played permutation mask", bytes 52..56 of each 72-byte range: a value the game sets
    as it plays, which the tag validator resets to 0 on every map it loads (tag_schema_effects.c, TAG_SCHEMA_RESET)
    and so did on the donor before the builder copied it; Invader writes -1 there. The pack's must be 0;
  - the up to 3 bytes that pad a piece to the next one's 4-byte boundary, which are the pack's own (0) and no
    part of the tag: taken as padding only when the piece is a data whose size, the word 12 bytes before its
    pointer, says so.
The references' targets are compared by name: the map's tag at a reference must be the tag the fixup names (own
tags by their source path, external ones by their name).

Usage: pack_compare.py <pack> <map>      prints each difference; exit 1 if there is one
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import aepack
import donor_map

MODEL_PART_SIZE = 0x68
MODEL_PART_BUFFERS = (68, 104)
BITMAP_PIXEL_ADDRESS = (0x38, 0x40)
BITMAP_GROUP = "bitm"
SOUND_GROUP = "snd!"
SOUND_RANGES = (0x98, 0x9C)             # a sound's root: its pitch ranges' count, and their address
SOUND_RANGE_SIZE = 72
SOUND_RANGE_PLAYED = (52, 56)
MAXIMUM_PROBLEMS = 40
# what is not compared, by class
MASK_POINTER, MASK_REFERENCE, MASK_TAG_INDEX, MASK_PART_BUFFERS, MASK_BITMAP_PIXELS, MASK_SOUND_PLAYED = range(1, 7)
MASK_NAMES = {MASK_POINTER: "pointers", MASK_REFERENCE: "references", MASK_TAG_INDEX: "tag_indices",
              MASK_PART_BUFFERS: "part_buffers", MASK_BITMAP_PIXELS: "bitmap_pixel_addresses",
              MASK_SOUND_PLAYED: "sound_played_masks"}


def _text(strings, offset):
    return strings[offset:strings.index(b"\0", offset)].decode("utf-8")


def compare_tag(tag_bytes, fixups, parts, tags, map_tag, targets=None, counts=None):
    """the differences between a pack tag's bytes and the map's tag, as lines (none: the same). fixups: (offset,
    kind, pad, value) each; parts: the offsets of the tag's model parts; targets: for each pack tag index, the
    (group, lower-case name) the map's tag at a reference to it must have (None: references are not checked);
    counts: a dictionary that takes "compared" and "masked" byte counts"""
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
                ends[value] = value + size
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
    return problems


def compare_pack(image, tag_data):
    """every own tag of the pack against the map's tag of its source path: {"own", "problems", "compared_bytes",
    "masked_bytes"}; problems are "<tag>: <difference>" lines"""
    parsed = aepack.read(image)
    tags = donor_map.Tags(tag_data)
    strings = parsed.strings
    tag_bytes = parsed.sections[aepack.TAG_BYTES]
    targets = []
    for record in parsed.tags:
        group = struct.pack(">I", record[0]).decode("latin-1")
        name = _text(strings, record[8]) if record[4] == aepack.OWN else _text(strings, record[3])
        targets.append((group, name.lower()))
    problems = []
    counts = {}
    own = 0
    for index, record in enumerate(parsed.tags):
        if record[4] != aepack.OWN:
            continue
        own += 1
        group, source = targets[index]
        map_tag = tags.find(group, source)
        if map_tag is None:
            problems.append("%s: the map has no such %s tag" % (source, group))
            continue
        fixups = parsed.fixups[record[11]:record[11] + record[12]]
        parts = [part[0] for part in parsed.parts[record[13]:record[13] + record[14]]]
        for problem in compare_tag(tag_bytes[record[9]:record[9] + record[10]], fixups, parts, tags, map_tag,
                                   targets, counts):
            problems.append("%s: %s" % (source, problem))
    return {"own": own, "problems": problems, "compared_bytes": counts.get("compared", 0),
            "masked_bytes": counts.get("masked", 0),
            "masked": {name: counts.get(name, 0) for name in MASK_NAMES.values()}}


def main(arguments):
    if len(arguments) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    with open(arguments[0], "rb") as file:
        image = file.read()
    try:
        report = compare_pack(image, donor_map.read_map(arguments[1]))
    except (aepack.Refused, donor_map.Refused) as refusal:
        print("pack_compare: refused: %s" % refusal, file=sys.stderr)
        return 1
    for problem in report["problems"]:
        print(problem)
    print("%(own)d own tags, %(compared_bytes)d bytes the same, %(masked_bytes)d not compared" % report)
    print("not compared: " + ", ".join("%s %d" % item for item in report["masked"].items()))
    return 1 if report["problems"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
