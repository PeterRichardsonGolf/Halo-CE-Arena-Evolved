"""Import Halo 1: NHE's voice timer clips as a local callout voice pack.

    python tools/import_nhe_voice.py <mods/NHE/maps> <data root>/voices/nhe

Reads the player's own NHE map files (every *.map in the folder, in name
order) and writes the timer clips as 16-bit PCM WAVs under our clip names,
plus manifest.json (each clip's source map, tag, sample rate, channels and
duration). The clips are NHE's: they come from the player's own files, are
never committed and are never shipped with the game, so the tool refuses to
write inside a git work tree (except under its build/ folder).

Clips (NHE tag under sound\\timer\\ -> file):
    cortana\\1 .. cortana\\10           -> one.wav .. ten.wav
    cortana\\20(twenny)_seconds         -> twenty_seconds.wav
    cortana\\30_seconds_left            -> thirty_seconds_left.wav
    cortana\\1_minute .. 30_minutes     -> one_minute.wav .. thirty_minutes.wav
    cortana\\rocket                     -> rockets.wav
    cortana\\red_rocket, blue_rocket    -> red_rockets.wav, blue_rockets.wav
                                           (Boarding Action only)
    cortana\\sniper, camo, overshield   -> sniper.wav, camo.wav, overshield.wav
    beeps\\timerbeep                    -> beep.wav
Each clip comes from the first map (by file name) that has its tag.

The format, as the engine reads it:
- A map file is a 2048-byte cache file header ('head' ... 'foot'; tag data
  offset at 0x10) followed by zlib data; the game plays the decompressed file
  (header included), and every file offset below is into that.
- The tag data is loaded at 0x803A6000: the tag index there lists each tag's
  group, name and definition address (as notes/tools/uidump.py walks it).
- A sound tag ('snd!', source/sound/sound_definitions.h): sample_rate at
  0x06 (0 = 22050 Hz, 1 = 44100 Hz), encoding at 0x6C (0 mono, 1 stereo),
  compression at 0x6E (0 none = 16-bit PCM, 1 Xbox ADPCM), pitch ranges
  (tag block, 0x48 bytes each) at 0x98. A pitch range's permutations (tag
  block at 0x3C, 0x7C bytes each) hold the samples as tag data at 0x40 (size,
  pad, file offset): the bytes are in the map file itself, outside the tag
  data (xbox_sound_cache.c reads them with cache_file_read). A long sound is
  split into permutations chained by next_permutation_index (0x2A).
- Xbox ADPCM (port/linux/src/dsound_sdl.c decode_adpcm): 36-byte blocks per
  channel, 64 samples each. A block starts with a 4-byte header per channel
  (first sample, int16; step index, byte; a pad byte), then 4-byte groups of
  eight IMA ADPCM nibbles, low nibble first, the groups alternating between
  channels. The header's sample is the block's first; the nibbles code the 63
  after it (the 64th nibble is padding).

Python 3 standard library only (no numpy needed): it takes about a second.
"""

import argparse
import array
import json
import os
import re
import struct
import sys
import wave
import zlib

HEADER_SIZE = 2048
TAG_DATA_BASE = 0x803A6000
SAMPLE_RATES = (22050, 44100)
COMPRESSION_NONE = 0
COMPRESSION_XBOX_ADPCM = 1
ADPCM_BLOCK_BYTES = 36
ADPCM_BLOCK_SAMPLES = 64

NUMBER_WORDS = (
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
    "sixteen", "seventeen", "eighteen", "nineteen", "twenty")


def number_word(number):
    if number <= 20:
        return NUMBER_WORDS[number]
    tens = {2: "twenty", 3: "thirty"}[number // 10]
    return tens if number % 10 == 0 else tens + "_" + NUMBER_WORDS[number % 10]


def expected_clips():
    """Our clip names, in the order the manifest lists them."""
    names = [number_word(n) for n in range(1, 11)]
    names += ["twenty_seconds", "thirty_seconds_left"]
    names += [number_word(n) + ("_minute" if n == 1 else "_minutes") for n in range(1, 31)]
    names += ["rockets", "red_rockets", "blue_rockets", "sniper", "camo", "overshield", "beep"]
    return names


def clip_name(tag_name):
    """Our clip name for an NHE timer sound tag, or None."""
    name = tag_name.lower()
    if name == "sound\\timer\\beeps\\timerbeep":
        return "beep"
    prefix = "sound\\timer\\cortana\\"
    if not name.startswith(prefix):
        return None
    name = name[len(prefix):]
    if re.fullmatch(r"([1-9]|10)", name):
        return number_word(int(name))
    if re.fullmatch(r"20\s*\(twenny\)\s*_?seconds", name):
        return "twenty_seconds"
    if name == "30_seconds_left":
        return "thirty_seconds_left"
    match = re.fullmatch(r"(\d+)_minutes?", name)
    if match and 1 <= int(match.group(1)) <= 30:
        minutes = int(match.group(1))
        return number_word(minutes) + ("_minute" if minutes == 1 else "_minutes")
    return {
        "rocket": "rockets",
        "red_rocket": "red_rockets",
        "blue_rocket": "blue_rockets",
        "sniper": "sniper",
        "camo": "camo",
        "overshield": "overshield",
    }.get(name)


# ---------- map files

class MapFile:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as file:
            raw = file.read()
        header = raw[:HEADER_SIZE]
        if len(header) < HEADER_SIZE or header[:4] != b"daeh" or header[2044:2048] != b"toof":
            raise ValueError("not a cache file")
        self.tag_data_offset, = struct.unpack_from("<i", header, 0x10)
        # (the header's file length is not always the decompressed length: some
        # NHE maps decompress to a little less, e.g. a10 and atlas)
        self.data = header + zlib.decompress(raw[HEADER_SIZE:])

    def offset(self, address):
        return self.tag_data_offset + (address - TAG_DATA_BASE)

    def string(self, address):
        start = self.offset(address)
        return self.data[start:self.data.index(b"\0", start)].decode("latin-1")

    def tags(self):
        """(group, name, definition address) of every tag."""
        instances, _, _, count = struct.unpack_from("<IiIi", self.data, self.tag_data_offset)
        for index in range(count):
            group, _, _, _, name, definition, _, _ = struct.unpack_from(
                "<iiiiIIII", self.data, self.offset(instances) + index * 32)
            yield group.to_bytes(4, "big").decode("latin-1"), self.string(name), definition

    def block(self, offset):
        count, address = struct.unpack_from("<iI", self.data, offset)
        return count, self.offset(address)

    def sound(self, definition):
        """(sample rate, channels, compression, sample bytes, permutation count)
        of a sound tag: pitch range 0's first permutation and its chain."""
        base = self.offset(definition)
        rate_index, = struct.unpack_from("<h", self.data, base + 0x06)
        encoding, compression = struct.unpack_from("<hh", self.data, base + 0x6C)
        ranges, range_base = self.block(base + 0x98)
        if ranges < 1 or rate_index not in (0, 1) or encoding not in (0, 1):
            raise ValueError("unexpected sound (rate %d, encoding %d, %d pitch ranges)" %
                (rate_index, encoding, ranges))
        count, permutations = self.block(range_base + 0x3C)
        samples = b""
        index, visited = 0, set()
        while 0 <= index < count and index not in visited:
            visited.add(index)
            permutation = permutations + index * 0x7C
            next_index, = struct.unpack_from("<h", self.data, permutation + 0x2A)
            size, _, file_offset = struct.unpack_from("<iIi", self.data, permutation + 0x40)
            if size < 0 or file_offset < 0 or file_offset + size > len(self.data):
                raise ValueError("samples outside the map file")
            samples += self.data[file_offset:file_offset + size]
            index = next_index
        return SAMPLE_RATES[rate_index], encoding + 1, compression, samples, count


# ---------- decoding

IMA_INDEX = (-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8)
IMA_STEP = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767)
# the difference each nibble codes at each step index
IMA_DIFFERENCE = [[0] * 16 for _ in IMA_STEP]
for _step_index, _step in enumerate(IMA_STEP):
    for _nibble in range(16):
        _difference = _step >> 3
        if _nibble & 1: _difference += _step >> 2
        if _nibble & 2: _difference += _step >> 1
        if _nibble & 4: _difference += _step
        IMA_DIFFERENCE[_step_index][_nibble] = -_difference if _nibble & 8 else _difference
IMA_NEXT_INDEX = [[min(88, max(0, i + IMA_INDEX[n])) for n in range(16)] for i in range(89)]


def decode_xbox_adpcm(data, channels):
    """Interleaved int16 samples (array 'h') of Xbox ADPCM data."""
    block_bytes = ADPCM_BLOCK_BYTES * channels
    blocks = len(data) // block_bytes
    output = array.array("h", bytes(2 * blocks * ADPCM_BLOCK_SAMPLES * channels))
    difference, next_index = IMA_DIFFERENCE, IMA_NEXT_INDEX
    for block in range(blocks):
        start = block * block_bytes
        first = block * ADPCM_BLOCK_SAMPLES * channels
        for channel in range(channels):
            predictor, index = struct.unpack_from("<hB", data, start + channel * 4)
            index = min(index, 88)
            out = first + channel
            output[out] = predictor
            nibbles = []
            for group in range(8):
                at = start + 4 * channels + (group * channels + channel) * 4
                for byte in data[at:at + 4]:
                    nibbles.append(byte & 0xF)
                    nibbles.append(byte >> 4)
            # nibble n codes sample n + 1; the 64th pads the block
            for nibble in nibbles[:ADPCM_BLOCK_SAMPLES - 1]:
                predictor += difference[index][nibble]
                if predictor > 32767: predictor = 32767
                elif predictor < -32768: predictor = -32768
                index = next_index[index][nibble]
                out += channels
                output[out] = predictor
    return output


def decode(compression, data, channels):
    if compression == COMPRESSION_XBOX_ADPCM:
        return decode_xbox_adpcm(data, channels)
    if compression == COMPRESSION_NONE:
        samples = array.array("h", data[:len(data) // (2 * channels) * 2 * channels])
        if sys.byteorder == "big":
            samples.byteswap()
        return samples
    raise ValueError("compression %d is not supported" % compression)


def write_wav(path, samples, rate, channels):
    if sys.byteorder == "big":
        samples = array.array("h", samples)
        samples.byteswap()
    with wave.open(path, "wb") as file:
        file.setnchannels(channels)
        file.setsampwidth(2)
        file.setframerate(rate)
        file.writeframes(samples.tobytes())


# ---------- output safety

def git_work_tree(path):
    """The git work tree containing path (which may not exist yet), or None."""
    path = os.path.realpath(path)
    while True:
        if os.path.exists(os.path.join(path, ".git")):
            return path
        parent = os.path.dirname(path)
        if parent == path:
            return None
        path = parent


def check_output_folder(path):
    tree = git_work_tree(path)
    if tree is None:
        return
    relative = os.path.relpath(os.path.realpath(path), tree)
    if relative.split(os.sep)[0] != "build":
        sys.exit("import_nhe_voice: %s is inside the git work tree %s: NHE's clips must never be "
            "committed. Write them to the game's data root (voices/nhe) or under build/." % (path, tree))


# ---------- main

def main():
    parser = argparse.ArgumentParser(
        description="Write Halo 1: NHE's voice timer clips (from your own NHE maps) as a voice pack.")
    parser.add_argument("maps", help="the folder with NHE's map files (mods/NHE/maps)")
    parser.add_argument("output", help="the voice pack folder to write (<data root>/voices/nhe)")
    arguments = parser.parse_args()

    check_output_folder(arguments.output)
    map_paths = sorted(
        os.path.join(arguments.maps, name) for name in os.listdir(arguments.maps)
        if name.lower().endswith(".map"))
    if not map_paths:
        sys.exit("import_nhe_voice: no .map files in %s" % arguments.maps)

    wanted = expected_clips()
    clips = {}
    unknown = set()
    for map_path in map_paths:
        if len(clips) == len(wanted):
            break
        try:
            map_file = MapFile(map_path)
        except (OSError, ValueError, zlib.error) as error:
            print("skipped %s: %s" % (os.path.basename(map_path), error), file=sys.stderr)
            continue
        for group, tag_name, definition in map_file.tags():
            if group != "snd!" or not tag_name.lower().startswith("sound\\timer\\"):
                continue
            name = clip_name(tag_name)
            if name is None:
                unknown.add(tag_name)
                continue
            if name in clips:
                continue
            rate, channels, compression, data, permutations = map_file.sound(definition)
            samples = decode(compression, data, channels)
            frames = len(samples) // channels
            clips[name] = {
                "samples": samples,
                "map": os.path.basename(map_path),
                "tag": tag_name,
                "sample_rate": rate,
                "channels": channels,
                "compression": {0: "none", 1: "xbox_adpcm"}.get(compression, compression),
                "permutations": permutations,
                "frames": frames,
                "duration": round(frames / rate, 3),
            }

    missing = [name for name in wanted if name not in clips]
    if not clips:
        sys.exit("import_nhe_voice: no NHE timer sounds found in %s" % arguments.maps)

    os.makedirs(arguments.output, exist_ok=True)
    manifest = {
        "pack": "nhe",
        "source": "Halo 1: Neutral Host Edition's voice timer, imported from the player's own map files "
            "by tools/import_nhe_voice.py; not distributed",
        "format": "16-bit PCM WAV at each tag's own sample rate and channel count",
        "clips": {},
        "missing": missing,
    }
    for name in wanted:
        if name not in clips:
            continue
        clip = clips[name]
        write_wav(os.path.join(arguments.output, name + ".wav"),
            clip.pop("samples"), clip["sample_rate"], clip["channels"])
        manifest["clips"][name] = clip
        print("%-28s %-18s %-40s %5d Hz %d ch %6.3f s" % (name + ".wav", clip["map"], clip["tag"],
            clip["sample_rate"], clip["channels"], clip["duration"]))
    with open(os.path.join(arguments.output, "manifest.json"), "w") as file:
        json.dump(manifest, file, indent=1)
        file.write("\n")

    print("%d of %d clips written to %s" % (len(manifest["clips"]), len(wanted), arguments.output))
    if unknown:
        print("timer sounds with no clip name (not written): %s" % ", ".join(sorted(unknown)))
    if missing:
        print("missing: %s" % ", ".join(missing), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
