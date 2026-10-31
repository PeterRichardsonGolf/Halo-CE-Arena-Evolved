"""Cuts the Ogg Vorbis permutations out of a sound tag file (our own reader of the file's layout).

An Xbox map cannot hold Ogg Vorbis samples; the recipe decodes each permutation (ffmpeg, a separate program) and
has it encoded again as Xbox ADPCM. A sound tag file: the root (164 bytes), the promotion sound's path, the pitch
ranges (72 bytes each), then for each range its permutations (124 bytes each) followed by their bytes: samples,
mouth data, subtitle data.

Usage: sound_oggs.py <sound tag> <folder>    writes <folder>/<permutation name>.ogg for each Ogg permutation
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek

ROOT_SIZE = 0xA4
PROMOTION_REFERENCE = 0x70
RANGE_COUNT = 0x98
RANGE_SIZE = 0x48
RANGE_PERMUTATION_COUNT = 0x3C
PERMUTATION_SIZE = 0x7C
PERMUTATION_COMPRESSION = 0x28
PERMUTATION_DATA = 0x40             # three data fields of 20 bytes: samples, mouth data, subtitle data
MAXIMUM_RANGES = 64
MAXIMUM_PERMUTATIONS = 256


def _name(element):
    return bytes(element[:32]).split(b"\0")[0].decode("ascii", "replace")


def permutations(tag):
    """every permutation in file order: {"range", "name", "compression", "samples", "ogg"}"""
    if hek.group(tag) != b"snd!":
        raise hek.Refused("not a sound tag: %r" % hek.group(tag))
    start = hek.HEADER_SIZE
    if len(tag) < start + ROOT_SIZE:
        raise hek.Refused("a sound tag of %d bytes" % len(tag))
    promotion = struct.unpack_from(">i", tag, start + PROMOTION_REFERENCE + 8)[0]
    range_count = struct.unpack_from(">i", tag, start + RANGE_COUNT)[0]
    if not 0 <= range_count <= MAXIMUM_RANGES or not 0 <= promotion < 256:
        raise hek.Refused("a sound tag with %d pitch ranges" % range_count)
    position = start + ROOT_SIZE + (promotion + 1 if promotion > 0 else 0)
    ranges = []
    for index in range(range_count):
        element = tag[position + index * RANGE_SIZE:position + (index + 1) * RANGE_SIZE]
        if len(element) != RANGE_SIZE:
            raise hek.Refused("the pitch ranges are not in the file")
        count = struct.unpack_from(">i", element, RANGE_PERMUTATION_COUNT)[0]
        if not 0 <= count <= MAXIMUM_PERMUTATIONS:
            raise hek.Refused("a pitch range with %d permutations" % count)
        ranges.append((_name(element), count))
    position += range_count * RANGE_SIZE
    found = []
    for range_name, count in ranges:
        elements = [tag[position + index * PERMUTATION_SIZE:position + (index + 1) * PERMUTATION_SIZE]
                    for index in range(count)]
        position += count * PERMUTATION_SIZE
        for element in elements:
            if len(element) != PERMUTATION_SIZE:
                raise hek.Refused("the permutations are not in the file")
            sizes = [struct.unpack_from(">i", element, PERMUTATION_DATA + index * 20)[0] for index in range(3)]
            if min(sizes) < 0 or position + sum(sizes) > len(tag):
                raise hek.Refused("a permutation's bytes are not in the file")
            samples = bytes(tag[position:position + sizes[0]])
            position += sum(sizes)
            found.append({"range": range_name, "name": _name(element),
                          "compression": struct.unpack_from(">h", element, PERMUTATION_COMPRESSION)[0],
                          "samples": samples, "ogg": samples[:4] == b"OggS"})
    if position != len(tag):
        raise hek.Refused("%d bytes of the file are not the layout's" % (len(tag) - position))
    return found


def ogg_files(tag):
    """(file name, bytes) of each Ogg Vorbis permutation; a permutation without a name is "default" """
    files = []
    for entry in permutations(tag):
        if entry["ogg"]:
            name = entry["name"] or "default"
            if not re.fullmatch(r"[A-Za-z0-9_ \-]+", name):
                raise hek.Refused("a permutation name that is not a file name: %r" % name)
            files.append((name + ".ogg", entry["samples"]))
    if len({name.lower() for name, _ in files}) != len(files):
        raise hek.Refused("two permutations of one name")
    return files


def main(arguments):
    if len(arguments) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    try:
        with open(arguments[0], "rb") as file:
            tag = file.read()
        for entry in permutations(tag):
            print("range %r permutation %r compression %d bytes %d %s" % (
                entry["range"], entry["name"], entry["compression"], len(entry["samples"]),
                "Ogg" if entry["ogg"] else "not Ogg"))
        files = ogg_files(tag)
    except hek.Refused as refusal:
        print("sound_oggs: refused: %s" % refusal, file=sys.stderr)
        return 1
    os.makedirs(arguments[1], exist_ok=True)
    for name, data in files:
        with open(os.path.join(arguments[1], name), "wb") as file:
            file.write(data)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
