"""The weapon pack recipe's own scripts, tools/weapon_pack/ (make_pack.py and what it runs), and the battle rifle's
recipe, tools/weapon_pack/battle_rifle/recipe.json.

Everywhere (CI): every script on bytes made here (no game file, no file of anyone's art): Open Sauce blocks cut
from tag files, DXT1 / DXT5 / 32-bit channel moves, the brightening of DXT endpoints, high-resolution HUD scales
halved, Ogg permutations cut out of a sound tag, the field edits' command lines and the copy of one part of a tag
onto another, the allowlist extractor's refusals, the recipe's own rules (every fix tag one step, every input in
the allowlist or made by a script here, no path of a machine), the audit's rules, the comparison of a pack's tags
with a map's. The art scripts (the counter's rounds and their thresholds 4 n, the gun screen's digits and
compass, the scope's label, the icon sheet) need numpy and Pillow and are skipped without them.

On the build box (game files; skipped elsewhere): a finished run of make_pack.py is checked: fixes.tsv has no
differing tag, the donor's SHA-256 is the recipe's (the approved v6e's), audit.tsv and externals.tsv hold their
rules, every external name is on all 13 stock multiplayer maps, and the pack's own tags are the approved donor's
bytes. Paths (each may be set in the environment):
    AE_PACK_RECIPE_WORK  a finished make_pack.py work folder  (default ~/halo-test/weapon-pack/br)
    AE_PACK_STOCK_MAPS   the stock maps' folder                (default ~/halo-data/maps)
    AE_PACK_V6E_DONOR    the approved v6e donor     (default ~/halo-test/weapon-pack/donors/ae_cmt_br_v6e.map)
Nothing made there is ever committed."""
import hashlib
import json
import os
import re
import struct
import sys
import zipfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import aepack  # noqa: E402
import donor_map  # noqa: E402
import dxt_lift  # noqa: E402
import extract_allowlist  # noqa: E402
import hek  # noqa: E402
import hud_hires_to_xbox  # noqa: E402
import hud_tag  # noqa: E402
import make_pack  # noqa: E402
import multipurpose_to_xbox  # noqa: E402
import os_strip  # noqa: E402
import pack_compare  # noqa: E402
import sound_oggs  # noqa: E402
import tag_fields  # noqa: E402

RECIPE = TOOLS / "battle_rifle" / "recipe.json"
V6E_SHA256 = "0aa3ffcbb704e59407945cd5eece3aaf53740992a582e23fe61aec51dca36884"
MULTIPLAYER_MAPS = ("beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation", "hangemhigh",
                    "longest", "prisoner", "putput", "ratrace", "sidewinder", "wizard")


def environment_path(name, default):
    return Path(os.environ.get(name, str(default))).expanduser()


WORK = environment_path("AE_PACK_RECIPE_WORK", Path.home() / "halo-test" / "weapon-pack" / "br")
STOCK_MAPS = environment_path("AE_PACK_STOCK_MAPS", Path.home() / "halo-data" / "maps")
V6E_DONOR = environment_path("AE_PACK_V6E_DONOR", Path.home() / "halo-test" / "weapon-pack" / "donors" /
                             "ae_cmt_br_v6e.map")


# ---------- tag files made here

def tag_file(group, body):
    header = bytearray(hek.HEADER_SIZE)
    header[0x24:0x28] = group
    return bytes(header) + bytes(body)


def bitmap_tag(bitmaps, sequences=0, sprites=0):
    """bitmaps: (width, height, format, mipmap count, pixel bytes) each; the file as the kit lays it out"""
    pixels = b"".join(entry[4] for entry in bitmaps)
    root = bytearray(hek.BITMAP_ROOT_SIZE)
    struct.pack_into(">i", root, hek.BITMAP_PIXELS_SIZE, len(pixels))
    struct.pack_into(">i", root, hek.BITMAP_SEQUENCE_COUNT, sequences)
    struct.pack_into(">i", root, hek.BITMAP_COUNT, len(bitmaps))
    records = bytearray()
    offset = 0
    for width, height, pixel_format, mips, data in bitmaps:
        record = bytearray(hek.BITMAP_DATA_SIZE)
        record[0:4] = b"bitm"
        struct.pack_into(">HHHHHH", record, 4, width, height, 1, 0, pixel_format, 0)
        struct.pack_into(">H", record, 0x14, mips)
        struct.pack_into(">I", record, 0x18, offset)
        offset += len(data)
        records += record
    middle = bytes(sequences * hek.BITMAP_SEQUENCE_SIZE + sprites * hek.BITMAP_SPRITE_SIZE)
    return tag_file(b"bitm", bytes(root) + pixels + middle + bytes(records))


def dxt1_block(c0, c1, indices):
    return struct.pack("<HHI", c0, c1, indices)


def rgb565(red, green, blue):
    return (red << 11) | (green << 5) | blue


# ---------- hek.py

def test_bitmap_tag_layout_and_refusals():
    tag = bitmap_tag([(4, 4, hek.FORMAT_DXT1, 0, dxt1_block(5, 3, 0))], sequences=2, sprites=1)
    layout = hek.BitmapTag(tag)
    assert (layout.bitmap_count, layout.pixels_size, layout.sequence_count) == (1, 8, 2)
    assert layout.pixels(tag) == dxt1_block(5, 3, 0)
    assert layout.first_sprite_offset() == hek.HEADER_SIZE + hek.BITMAP_ROOT_SIZE + 8 + 2 * hek.BITMAP_SEQUENCE_SIZE
    with pytest.raises(hek.Refused):
        hek.BitmapTag(tag_file(b"snd!", bytes(200)))
    with pytest.raises(hek.Refused):
        hek.BitmapTag(tag[:120])
    with pytest.raises(hek.Refused):
        hek.BitmapTag(bytes(10))


def test_dxt_decode_is_exact_integer_arithmetic():
    # four colours: both endpoints and the two thirds between them
    block = dxt1_block(rgb565(31, 63, 31), rgb565(0, 0, 0), 0b11100100)
    rgba = hek.decode_rgba(block, 4, 4, hek.FORMAT_DXT1)
    assert [tuple(rgba[index * 4:index * 4 + 4]) for index in range(4)] == [
        (255, 255, 255, 255), (0, 0, 0, 255), (170, 170, 170, 255), (85, 85, 85, 255)]
    # three colours and a transparent one when the first endpoint is not the larger
    block = dxt1_block(rgb565(0, 0, 0), rgb565(31, 63, 31), 0b11100100)
    rgba = hek.decode_rgba(block, 4, 4, hek.FORMAT_DXT1)
    assert tuple(rgba[8:12]) == (127, 127, 127, 255) and tuple(rgba[12:16]) == (0, 0, 0, 0)
    # DXT5: eight alphas from the two given, a0 > a1
    alpha = bytes((255, 0)) + ((1 << 3) | (2 << 6) | (7 << 9)).to_bytes(6, "little")
    rgba = hek.decode_rgba(alpha + dxt1_block(rgb565(31, 0, 0), 0, 0), 4, 4, hek.FORMAT_DXT5)
    assert [rgba[index * 4 + 3] for index in range(4)] == [255, 0, (5 * 255) // 6, 0]
    assert tuple(rgba[0:3]) == (255, 0, 0)
    with pytest.raises(hek.Refused):
        hek.decode_rgba(bytes(16), 4, 4, hek.FORMAT_A8R8G8B8)


# ---------- os_strip.py

def shader_model_with_extension(paths=(b"a\\b", b""), references_before=(b"base\\map", b"", b"")):
    """a shader_model file: the root with reference lengths, the strings before the block, then one 192-byte Open
    Sauce element holding two bitmap references and their path strings, then what follows it"""
    body_size, block, element, before = os_strip.LAYOUTS["shader_model"]
    root = bytearray(body_size)
    for offset, path in zip(before, references_before):
        root[offset:offset + 4] = b"bitm"
        struct.pack_into(">iI", root, offset + 8, len(path), 0xFFFFFFFF)
    struct.pack_into(">iII", root, block, 1, 0x1234, 0x5678)
    strings_before = b"".join(path + b"\0" for path in references_before if path)
    extension = bytearray(element)
    extension_strings = b""
    for index, path in enumerate(paths):
        extension[index * 16:index * 16 + 4] = b"bitm"
        struct.pack_into(">iI", extension, index * 16 + 8, len(path), 0xFFFFFFFF)
        if path:
            extension_strings += path + b"\0"
    after = b"reflection\\cube\0"
    return tag_file(b"soso", bytes(root) + strings_before + bytes(extension) + extension_strings + after), after, \
        len(extension) + len(extension_strings)


def test_os_strip_cuts_the_block_and_its_strings():
    tag, after, cut = shader_model_with_extension()
    out, removed, elements = os_strip.strip("shader_model", tag)
    assert (removed, elements) == (cut, 1)
    assert len(out) == len(tag) - cut and out.endswith(b"base\\map\0" + after)
    body_size, block, _, _ = os_strip.LAYOUTS["shader_model"]
    assert out[hek.HEADER_SIZE + block:hek.HEADER_SIZE + block + 12] == bytes(12)
    assert out[:hek.HEADER_SIZE + block] == tag[:hek.HEADER_SIZE + block]


def test_os_strip_leaves_a_tag_without_the_block():
    body_size = os_strip.LAYOUTS["particle"][0]
    tag = tag_file(b"part", bytes(body_size) + b"tail")
    assert os_strip.strip("particle", tag) == (tag, 0, 0)


def test_os_strip_refusals():
    body_size, block, _, _ = os_strip.LAYOUTS["particle"]
    root = bytearray(body_size)
    struct.pack_into(">i", root, block, 16)
    with pytest.raises(hek.Refused):
        os_strip.strip("particle", tag_file(b"part", root))
    struct.pack_into(">i", root, block, 1)
    with pytest.raises(hek.Refused):                      # the element is not in the file
        os_strip.strip("particle", tag_file(b"part", root))
    with pytest.raises(hek.Refused):
        os_strip.strip("weapon", tag_file(b"weap", bytes(2000)))


# ---------- multipurpose_to_xbox.py

def test_multipurpose_dxt1_moves_blue_to_red_exactly():
    c0, c1 = rgb565(2, 40, 20), rgb565(1, 10, 5)             # four colours (c0 > c1) before and after
    tag = bitmap_tag([(4, 4, hek.FORMAT_DXT1, 0, dxt1_block(c0, c1, 0x1B))])
    out, stats = multipurpose_to_xbox.convert(tag)
    block = hek.BitmapTag(out).pixels(out)
    assert struct.unpack("<HHI", block) == (rgb565(20, 40, 0), rgb565(5, 10, 0), 0x1B)
    assert stats == {"bitmaps": 1, "blocks": 1, "flipped": 0, "bytes": 8}
    assert len(out) == len(tag) and out[:hek.HEADER_SIZE + hek.BITMAP_ROOT_SIZE] == tag[:hek.HEADER_SIZE +
                                                                                         hek.BITMAP_ROOT_SIZE]


def test_multipurpose_dxt1_keeps_the_block_kind_when_the_order_turns():
    # four colours, c0 > c1 by red only; after the move the blues decide and the order turns: swapped, indices too
    c0, c1 = rgb565(9, 0, 1), rgb565(3, 0, 7)
    out, stats = multipurpose_to_xbox.convert(bitmap_tag([(4, 4, hek.FORMAT_DXT1, 0, dxt1_block(c0, c1, 0x1B))]))
    n0, n1, indices = struct.unpack("<HHI", hek.BitmapTag(out).pixels(out))
    assert (n0, n1) == (rgb565(7, 0, 0), rgb565(1, 0, 0)) and n0 > n1
    assert indices == 0x1B ^ 0x55555555 and stats["flipped"] == 1
    # the same colours after the move: one colour, every texel the first
    c0, c1 = rgb565(9, 5, 4), rgb565(3, 5, 4)
    out, _ = multipurpose_to_xbox.convert(bitmap_tag([(4, 4, hek.FORMAT_DXT1, 0, dxt1_block(c0, c1, 0xFFFFFFFF))]))
    assert struct.unpack("<HHI", hek.BitmapTag(out).pixels(out)) == (rgb565(4, 5, 0), rgb565(4, 5, 0), 0)
    # three colours (c0 <= c1) that would become four: swapped, indices 0 and 1 exchanged, 2 and 3 kept
    c0, c1 = rgb565(1, 0, 9), rgb565(8, 0, 2)
    out, _ = multipurpose_to_xbox.convert(bitmap_tag([(4, 4, hek.FORMAT_DXT1, 0, dxt1_block(c0, c1, 0b11100100))]))
    n0, n1, indices = struct.unpack("<HHI", hek.BitmapTag(out).pixels(out))
    assert n0 <= n1 and (n0, n1) == (rgb565(2, 0, 0), rgb565(9, 0, 0)) and indices == 0x555555E1


def test_multipurpose_dxt5_and_32_bit_and_meter():
    alpha = bytes(range(8))
    tag = bitmap_tag([(4, 4, hek.FORMAT_DXT5, 0, alpha + dxt1_block(rgb565(1, 2, 3), rgb565(4, 5, 6), 7))])
    out, _ = multipurpose_to_xbox.convert(tag)
    block = hek.BitmapTag(out).pixels(out)
    assert block[:8] == bytes((255, 255, 0, 0, 0, 0, 0, 0))
    assert struct.unpack("<HHI", block[8:]) == (rgb565(3, 2, 0), rgb565(6, 5, 0), 7)
    # 32-bit, stored blue green red alpha: new blue = old alpha, new red = old blue, new alpha = old red
    tag = bitmap_tag([(1, 1, hek.FORMAT_A8R8G8B8, 0, bytes((10, 20, 30, 40)))])
    out, _ = multipurpose_to_xbox.convert(tag)
    assert hek.BitmapTag(out).pixels(out) == bytes((40, 20, 10, 30))
    # a HUD meter: the colour becomes the old alpha, the alpha the old red
    out, _ = multipurpose_to_xbox.convert(tag, meter=True)
    assert hek.BitmapTag(out).pixels(out) == bytes((40, 40, 40, 30))
    with pytest.raises(hek.Refused):
        multipurpose_to_xbox.convert(bitmap_tag([(4, 4, hek.FORMAT_DXT1, 0, bytes(8))]), meter=True)
    with pytest.raises(hek.Refused):
        multipurpose_to_xbox.convert(bitmap_tag([(2, 2, hek.FORMAT_A8Y8, 0, bytes(8))]))


def test_multipurpose_walks_mipmaps_and_each_bitmaps_own_offset():
    levels = dxt1_block(rgb565(0, 0, 9), 0, 0) * 4 + dxt1_block(rgb565(0, 0, 8), 0, 0) + dxt1_block(
        rgb565(0, 0, 7), 0, 0) + dxt1_block(rgb565(0, 0, 6), 0, 0)
    second = dxt1_block(rgb565(0, 0, 5), 0, 0)
    out, stats = multipurpose_to_xbox.convert(bitmap_tag([(8, 8, hek.FORMAT_DXT1, 3, levels),
                                                          (4, 4, hek.FORMAT_DXT1, 0, second)]))
    pixels = hek.BitmapTag(out).pixels(out)
    reds = [struct.unpack_from("<H", pixels, offset)[0] >> 11 for offset in range(0, len(pixels), 8)]
    assert reds == [9, 9, 9, 9, 8, 7, 6, 5] and stats["blocks"] == 8 and stats["bitmaps"] == 2


# ---------- dxt_lift.py

def test_lift_curve():
    assert dxt_lift.lifted(0, 31, 1.8) == 0 and dxt_lift.lifted(31, 31, 1.8) == 31
    assert dxt_lift.lifted(16, 31, 1.8) == round((1 - (1 - 16 / 31) ** 1.8) * 31) == 23
    assert dxt_lift.lifted(10, 63, 1.0) == 10
    assert [dxt_lift.lifted(value, 63, 1.8) for value in (1, 2, 32)] == [2, 4, 45]


def test_lift_changes_only_the_endpoints():
    c0, c1 = rgb565(16, 32, 16), rgb565(4, 8, 4)
    tag = bitmap_tag([(4, 4, hek.FORMAT_DXT1, 0, dxt1_block(c0, c1, 0xDEADBEEF))])
    out, stats = dxt_lift.lift(tag, 1.8)
    n0, n1, indices = struct.unpack("<HHI", hek.BitmapTag(out).pixels(out))
    assert n0 == rgb565(dxt_lift.lifted(16, 31, 1.8), dxt_lift.lifted(32, 63, 1.8), dxt_lift.lifted(16, 31, 1.8))
    assert n1 == rgb565(dxt_lift.lifted(4, 31, 1.8), dxt_lift.lifted(8, 63, 1.8), dxt_lift.lifted(4, 31, 1.8))
    assert indices == 0xDEADBEEF and stats["flipped"] == 0 and stats["blocks"] == 1
    with pytest.raises(hek.Refused):
        dxt_lift.lift(bitmap_tag([(1, 1, hek.FORMAT_A8R8G8B8, 0, bytes(4))]), 1.8)


# ---------- hud_hires_to_xbox.py

class FakeEditor:
    """what the HUD conversion asks of a tag editor: counts, values, and sets it records"""

    def __init__(self, counts, values):
        self.counts, self.values, self.sets = counts, dict(values), []

    def count(self, key):
        if key not in self.counts:
            raise tag_fields.Failed("no such block: " + key)
        return self.counts[key]

    def get(self, key):
        return self.values.get(key, "0")

    def set(self, key, value):
        self.sets.append((key, value))
        self.values[key] = value


def test_hud_scale_halving_text():
    assert hud_hires_to_xbox.halved("1.000000") == "0.5"
    assert hud_hires_to_xbox.halved("0.000000") == "0.5"          # an unset scale is 1
    assert hud_hires_to_xbox.halved("0.750000") == "0.375"
    assert hud_hires_to_xbox.halved("1.300000") == "0.65"


def test_hud_hires_elements_halved_and_flag_cleared():
    flag = ".scaling_flags.use_high_res_scale"
    editor = FakeEditor(
        {"static_elements": 2, "meter_elements": 0, "number_elements": 1, "crosshairs": 2,
         "crosshairs[0].crosshair_overlays": 1, "crosshairs[1].crosshair_overlays": 2, "overlay_elements": 1,
         "overlay_elements[0].overlays": 1},
        {"static_elements[1]" + flag: "1", "static_elements[1].width_scale": "0.000000",
         "static_elements[1].height_scale": "2.000000",
         "crosshairs[1].crosshair_overlays[1]" + flag: "1",
         "crosshairs[1].crosshair_overlays[1].width_scale": "0.500000",
         "crosshairs[1].crosshair_overlays[1].height_scale": "0.500000",
         "overlay_elements[0].overlays[0]" + flag: "0"})
    done = hud_hires_to_xbox.convert(editor)
    assert done == ["static_elements[1]", "crosshairs[1].crosshair_overlays[1]"]
    assert editor.sets == [
        ("static_elements[1].width_scale", "0.5"), ("static_elements[1].height_scale", "1.0"),
        ("static_elements[1]" + flag, "0"),
        ("crosshairs[1].crosshair_overlays[1].width_scale", "0.25"),
        ("crosshairs[1].crosshair_overlays[1].height_scale", "0.25"),
        ("crosshairs[1].crosshair_overlays[1]" + flag, "0")]


def test_hud_hires_a_tag_without_a_block_is_left():
    editor = FakeEditor({"static_elements": 0, "meter_elements": 0, "number_elements": 0}, {})
    assert hud_hires_to_xbox.convert(editor) == [] and editor.sets == []


# ---------- sound_oggs.py

def sound_tag(ranges, promotion=b""):
    """ranges: (name, [(permutation name, compression, samples, mouth bytes, subtitle bytes)])"""
    root = bytearray(sound_oggs.ROOT_SIZE)
    struct.pack_into(">i", root, sound_oggs.PROMOTION_REFERENCE + 8, len(promotion))
    struct.pack_into(">i", root, sound_oggs.RANGE_COUNT, len(ranges))
    body = bytes(root) + (promotion + b"\0" if promotion else b"")
    for name, permutations in ranges:
        element = bytearray(sound_oggs.RANGE_SIZE)
        element[:len(name)] = name
        struct.pack_into(">i", element, sound_oggs.RANGE_PERMUTATION_COUNT, len(permutations))
        body += bytes(element)
    for _, permutations in ranges:
        for name, compression, samples, mouth, subtitle in permutations:
            element = bytearray(sound_oggs.PERMUTATION_SIZE)
            element[:len(name)] = name
            struct.pack_into(">h", element, sound_oggs.PERMUTATION_COMPRESSION, compression)
            for index, data in enumerate((samples, mouth, subtitle)):
                struct.pack_into(">i", element, sound_oggs.PERMUTATION_DATA + index * 20, len(data))
            body += bytes(element)
        for _, _, samples, mouth, subtitle in permutations:
            body += samples + mouth + subtitle
    return tag_file(b"snd!", body)


def test_sound_permutations_found_with_their_samples():
    ogg = b"OggS" + bytes(range(40))
    tag = sound_tag([(b"default", [(b"BR1", 3, ogg, b"mm", b""), (b"BR3", 3, b"OggS-second", b"", b"sub")]),
                     (b"far", [(b"", 1, b"\x01\x02\x03\x04", b"", b"")])], promotion=b"sound\\next")
    found = sound_oggs.permutations(tag)
    assert [(entry["range"], entry["name"], entry["compression"], entry["ogg"]) for entry in found] == [
        ("default", "BR1", 3, True), ("default", "BR3", 3, True), ("far", "", 1, False)]
    assert found[0]["samples"] == ogg and found[1]["samples"] == b"OggS-second"
    assert sound_oggs.ogg_files(tag) == [("BR1.ogg", ogg), ("BR3.ogg", b"OggS-second")]


def test_sound_refusals():
    with pytest.raises(hek.Refused):
        sound_oggs.permutations(tag_file(b"bitm", bytes(400)))
    tag = sound_tag([(b"default", [(b"one", 3, b"OggS1234", b"", b"")])])
    with pytest.raises(hek.Refused):
        sound_oggs.permutations(tag[:-4])                           # the samples run past the file
    with pytest.raises(hek.Refused):
        sound_oggs.permutations(tag + b"x")                         # bytes the layout does not account for
    names = sound_tag([(b"default", [(b"../up", 3, b"OggS", b"", b"")])])
    with pytest.raises(hek.Refused):
        sound_oggs.ogg_files(names)


# ---------- tag_fields.py

LISTING = """\
child_hud (ui\\hud\\master rounds.weapon_hud_interface)   tag reference
flash_cutoff (0.000000)   float
crosshairs[2]   array
├──crosshairs[0]   struct
│   ├──crosshair_type   enum
│   │   ├──aim   enum value
│   │   ├──zoom_overlay (*)   enum value
│   │   └──charge   enum value
│   ├──crosshair_bitmap (ui\\hud\\bitmaps\\scope.bitmap)   tag reference
│   └──crosshair_overlays[1]   array
│       └──crosshair_overlays[0]   struct
│           ├──anchor_offset (3 -4)   point2d
│           ├──width_scale (0.750000)   float
│           └──scaling_flags   bitmask
│               ├──dont_scale_offset (0)   bitfield
│               └──use_high_res_scale (1)   bitfield
└──crosshairs[1]   struct
    └──crosshair_bitmap (ui\\hud\\bitmaps\\other.bitmap)   tag reference
"""


def test_tree_to_sets_copies_one_part_under_another_key():
    sets = tag_fields.tree_to_sets(LISTING, "crosshairs[0]", "crosshairs[3]")
    assert sets == [
        ("S", "crosshairs[3].crosshair_type", "zoom_overlay"),
        ("S", "crosshairs[3].crosshair_bitmap", "ui\\hud\\bitmaps\\scope.bitmap"),
        ("C", "crosshairs[3].crosshair_overlays", "1"),
        ("S", "crosshairs[3].crosshair_overlays[0].anchor_offset", "3 -4"),
        ("S", "crosshairs[3].crosshair_overlays[0].width_scale", "0.750000"),
        ("S", "crosshairs[3].crosshair_overlays[0].scaling_flags.dont_scale_offset", "0"),
        ("S", "crosshairs[3].crosshair_overlays[0].scaling_flags.use_high_res_scale", "1")]
    assert tag_fields.tree_to_sets(LISTING, "crosshairs[1]", "x") == [
        ("S", "x.crosshair_bitmap", "ui\\hud\\bitmaps\\other.bitmap")]
    assert tag_fields.tree_to_sets(LISTING, "crosshairs[7]", "x") == []


class RecordingRunner:
    """stands in for the editor program: records each command line, answers counts and listings"""

    def __init__(self, answers=None):
        self.commands, self.answers = [], answers or {}

    def __call__(self, command):
        self.commands.append(command)
        for key, answer in self.answers.items():
            if key in command:
                return answer
        return ""


def test_editor_command_lines():
    runner = RecordingRunner({"-C": "2\n"})
    editor = tag_fields.Editor("/inv", "/tags", "a/b.weapon", runner=runner)
    editor.set("zoom_magnification_range", "3 3")
    assert editor.count("triggers") == 2
    tag_fields.apply(editor, [["sets", [["a", "1"], ["b.c", "x y"]]], ["erase", "extra_layers[0-2]"],
                              ["insert", "screen_effect", 1, "end"], ["copy", "meter_elements[0]", "end"]])
    program = str(Path("/inv") / "invader-edit")
    assert runner.commands == [
        [program, "-t", "/tags", "-S", "zoom_magnification_range", "3 3", "a/b.weapon"],
        [program, "-t", "/tags", "-C", "triggers", "a/b.weapon"],
        [program, "-t", "/tags", "-S", "a", "1", "-S", "b.c", "x y", "a/b.weapon"],
        [program, "-t", "/tags", "-E", "extra_layers[0-2]", "a/b.weapon"],
        [program, "-t", "/tags", "-I", "screen_effect", "1", "end", "a/b.weapon"],
        [program, "-t", "/tags", "-c", "meter_elements[0]", "end", "a/b.weapon"]]
    with pytest.raises(tag_fields.Failed):
        tag_fields.apply(editor, [["rename", "a", "b"]])


def test_graft_grows_arrays_then_sets(tmp_path):
    runner = RecordingRunner({"-L": LISTING, "-C": "0\n"})
    editor = tag_fields.Editor("/inv", "/tags", "hud.weapon_hud_interface", runner=runner)
    source = tag_fields.Editor("/inv", "/stock", "pistol.weapon_hud_interface", runner=runner)
    tag_fields.graft(editor, source, "crosshairs[0]", "crosshairs[4]")
    edits = [command[3:-1] for command in runner.commands if command[3] in ("-S", "-I")]
    assert edits[0] == ["-S", "crosshairs[4].crosshair_type", "zoom_overlay"]
    assert ["-I", "crosshairs[4].crosshair_overlays", "1", "end"] in edits
    assert edits[-1] == ["-S", "crosshairs[4].crosshair_overlays[0].scaling_flags.use_high_res_scale", "1"]
    assert runner.commands[0] == [str(Path("/inv") / "invader-edit"), "-t", "/stock", "-L",
                                  "pistol.weapon_hud_interface"]


# ---------- hud_tag.py

def test_hud_tag_edits_put_the_icon_sheet_off_screen_and_the_counter_second():
    edits = hud_tag.counter_edits(alphas="ae_pack\\hud\\br_alphas_smooth.bitmap",
                                  meter="ae_pack\\hud\\br_meter_smooth.bitmap", scale=0.25, anchor=(-1, 20),
                                  off_screen=(-3000, -3000))
    assert edits[0] == ["copy", "meter_elements[0]", "end"]
    sets = dict(edits[1][1])
    assert edits[1][0] == "sets"
    assert sets["meter_elements[1].meter_bitmap"] == "ae_pack\\hud\\br_meter_smooth.bitmap"
    assert sets["meter_elements[1].width_scale"] == "0.25" and sets["static_elements[0].height_scale"] == "0.25"
    assert sets["static_elements[0].anchor_offset"] == "-1 20" and sets["meter_elements[1].sequence_index"] == "0"
    assert not any(key.startswith("meter_elements[0].") for key in sets)
    assert edits[-1] == ["set", "meter_elements[0].anchor_offset", "-3000 -3000"]


# ---------- extract_allowlist.py

def make_zip(path, files):
    with zipfile.ZipFile(path, "w") as archive:
        for name, data in files.items():
            archive.writestr(name, data)


def test_allowlist_extracts_exactly_the_names(tmp_path):
    archive = tmp_path / "release.zip"
    make_zip(archive, {"rel/tags/a/one.bitmap": b"1", "rel/tags/a/two.bitmap": b"22", "rel/util/tool.exe": b"x",
                       "rel/tags/b/three.sound": b"333"})
    out = tmp_path / "out"
    written = extract_allowlist.extract(archive, ["rel/tags/a/one.bitmap", "rel/tags/b/three.sound"], out,
                                        strip="rel/tags/")
    assert sorted(written) == ["a/one.bitmap", "b/three.sound"]
    assert sorted(str(path.relative_to(out)) for path in out.rglob("*") if path.is_file()) == [
        "a/one.bitmap", "b/three.sound"]
    assert (out / "b" / "three.sound").read_bytes() == b"333"


@pytest.mark.parametrize("names, reason", [
    ([], "empty"),
    (["rel/tags/a/missing.bitmap"], "not in the archive"),
    (["rel/tags/a/one.bitmap", "rel/tags/a/one.bitmap"], "twice"),
    (["rel/util/tool.exe"], "util"),
    (["rel/tags/../util/tool.exe"], "not a plain path"),
    (["/rel/tags/a/one.bitmap"], "not a plain path"),
    (["rel/tags/a/*.bitmap"], "not a plain path"),
    (["rel/tags/a/"], "not a plain path"),
    (["other/a/one.bitmap"], "outside"),
])
def test_allowlist_refusals_write_nothing(tmp_path, names, reason):
    archive = tmp_path / "release.zip"
    make_zip(archive, {"rel/tags/a/one.bitmap": b"1", "rel/util/tool.exe": b"x", "other/a/one.bitmap": b"1"})
    out = tmp_path / "out"
    with pytest.raises(extract_allowlist.Refused) as refusal:
        extract_allowlist.extract(archive, names, out, strip="rel/tags/")
    assert reason in str(refusal.value)
    assert not out.exists() or not any(out.rglob("*"))


def test_allowlist_refuses_a_folder_with_files_in_it(tmp_path):
    archive = tmp_path / "release.zip"
    make_zip(archive, {"rel/tags/a/one.bitmap": b"1"})
    out = tmp_path / "out"
    out.mkdir()
    (out / "left.bitmap").write_bytes(b"x")
    with pytest.raises(extract_allowlist.Refused):
        extract_allowlist.extract(archive, ["rel/tags/a/one.bitmap"], out, strip="rel/tags/")


def test_allowlist_checks_the_archive_hash(tmp_path):
    archive = tmp_path / "release.zip"
    make_zip(archive, {"rel/tags/a/one.bitmap": b"1"})
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    assert extract_allowlist.sha256_file(archive) == digest
    out = tmp_path / "out"
    with pytest.raises(extract_allowlist.Refused):
        extract_allowlist.extract(archive, ["rel/tags/a/one.bitmap"], out, strip="rel/tags/", sha256="0" * 64)
    extract_allowlist.extract(archive, ["rel/tags/a/one.bitmap"], out, strip="rel/tags/", sha256=digest)


# ---------- the art scripts (numpy and Pillow)

def test_counter_rounds_thresholds_and_drain_order():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import counter_art
    rounds = list(counter_art.rounds())
    assert [number for number, _, _ in rounds] == list(range(1, 37))
    # round 1, the last one left, is the top row's left end; round 36, the first to go, the bottom row's right end
    assert min(rounds, key=lambda entry: (entry[2], entry[1]))[0] == 1
    assert max(rounds, key=lambda entry: (entry[2], entry[1]))[0] == 36
    top = [entry for entry in rounds if entry[0] <= 18]
    bottom = [entry for entry in rounds if entry[0] > 18]
    assert len({entry[2] for entry in top}) == 1 and len({entry[2] for entry in bottom}) == 1
    assert bottom[0][2] - top[0][2] == counter_art.ROW
    # groups of three: the gap after every third round is wider by GROUP_GAP
    gaps = [round(top[index + 1][1] - top[index][1], 6) for index in range(17)]
    assert gaps == [counter_art.PITCH if index % 3 != 2 else round(counter_art.PITCH + counter_art.GROUP_GAP, 6)
                    for index in range(17)]
    meter, alphas = counter_art.sheets()
    assert meter.shape == (128, 512, 4) and alphas.shape == (128, 512, 4)
    covered = meter[..., 3] == 255
    thresholds = sorted(int(value) for value in numpy.unique(meter[..., 0][covered]))
    assert thresholds == [4 * number for number in range(1, 37)]              # round n holds 4 n
    assert (meter[..., 0] == meter[..., 2]).all() and (meter[..., 1] == meter[..., 3]).all()
    assert set(numpy.unique(meter[..., 0])) <= {4 * number for number in range(1, 37)}   # carried out, no zero
    # the meter shows a texel while its threshold is under 4 x the rounds loaded + 2: with 33 loaded the three
    # rounds that went are the bottom row's right group
    gone = covered & (meter[..., 0] >= 4 * 33 + 2)
    rows, columns = numpy.nonzero(gone)
    lit_rows, lit_columns = numpy.nonzero(covered & ~gone)
    assert rows.min() > 128 // 4 and columns.min() > lit_columns[lit_rows > rows.min()].max() - 60
    assert columns.max() == numpy.nonzero(covered[rows.min():])[1].max()      # the bottom row's right end
    # with 18 loaded the whole bottom row is out and the whole top row lit
    gone = covered & (meter[..., 0] >= 4 * 18 + 2)
    assert numpy.nonzero(gone)[0].min() > numpy.nonzero(covered & ~gone)[0].min()
    assert abs(int((covered & ~gone).sum()) * 2 - int(covered.sum())) < 0.02 * int(covered.sum())
    assert (alphas[..., 0] == meter[..., 3]).all() and (alphas[..., 3] == meter[..., 3]).all()


def test_screen_digits_and_compass_frames():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import screen_art
    digits = [numpy.asarray(screen_art.digit(value)) for value in range(10)]
    assert all(frame.shape == (128, 128, 3) for frame in digits)
    lit = [frame.max(axis=2) > 128 for frame in digits]
    # a glyph keeps to 44 px about x 64 and to y 40..122 (where the model's digit quads sample)
    for frame in lit:
        rows, columns = numpy.nonzero(frame)
        assert rows.min() >= 39 and rows.max() <= 122 and columns.min() >= 41 and columns.max() <= 86
    assert len({frame.tobytes() for frame in lit}) == 10
    assert lit[8].sum() > max(lit[value].sum() for value in (0, 6, 9)) and lit[1].sum() == min(
        frame.sum() for frame in lit)
    assert (lit[8] >= lit[0]).all() and (lit[8] >= lit[3]).all()                 # the segments of an eight
    frames = [numpy.asarray(screen_art.compass(index)) for index in range(8)]
    assert len({frame.tobytes() for frame in frames}) == 8
    bright = [frame.max(axis=2) > 200 for frame in frames]
    # frame 0's needle points up, frame 4's down, about the dial's centre (64, 50)
    assert numpy.nonzero(bright[0])[0].mean() < 50 < numpy.nonzero(bright[4])[0].mean()
    assert numpy.nonzero(bright[2])[1].mean() < 64 < numpy.nonzero(bright[6])[1].mean()
    plate = screen_art.plate([screen_art.digit(value) for value in range(10)])
    assert plate.size == (1 + 10 * 129, 130) and plate.getpixel((0, 0)) == (0, 0, 255)
    assert plate.getpixel((129, 5)) == (0, 0, 255) and plate.getpixel((1, 1)) != (0, 0, 255)


def label_sheet(numpy):
    """a 1024 x 1024 alpha sheet with a "[2x]"-like label where the scope's is: a bracket, a digit, an x, a bracket"""
    alpha = numpy.zeros((1024, 1024), numpy.uint8)
    alpha[707:757, 587:601] = 255        # [
    alpha[707:757, 607:634] = 255        # the digit's box, 27 x 50
    alpha[720:757, 640:660] = 255        # x
    alpha[707:757, 663:677] = 255        # ]
    alpha[100:110, 100:900] = 200        # a line elsewhere
    return alpha


def test_scope_label_box_found_and_only_it_changed():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import scope_label
    alpha = label_sheet(numpy)
    out, box = scope_label.relabel(alpha, "3")
    assert box == {"x": 607, "y": 707, "width": 27, "height": 50, "runs": 4, "old_ink": 27 * 50,
                   "brightness": 1.0}
    changed = numpy.nonzero(out != alpha)
    assert changed[0].min() >= 707 - 2 and changed[0].max() <= 756 + 2
    assert changed[1].min() >= 607 - 1 and changed[1].max() <= 633 + 1
    assert out[100:110].tolist() == alpha[100:110].tolist() and (out[707:757, 587:601] == 255).all()
    new = out[707:757, 607:634]
    assert 0 < int((new > 128).sum()) < 27 * 50 and new[:8].mean() > new[20:28, :8].mean()   # a flat top
    with pytest.raises(scope_label.Refused):
        scope_label.relabel(alpha, "5")                              # only the 3 is drawn
    with pytest.raises(scope_label.Refused):
        scope_label.relabel(numpy.zeros((1024, 1024), numpy.uint8), "3")


def test_scope_label_parts_a_digit_that_touches_the_x():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import scope_label
    alpha = label_sheet(numpy)
    alpha[720:757, 634:640] = 255                                    # the digit runs into the x
    alpha[720:757, 634:637] = 70                                     # with a faint neck between them
    _, box = scope_label.relabel(alpha, "3")
    assert box["runs"] == 3 and box["x"] == 607 and 20 <= box["width"] <= 34


def test_icon_sheet_ticks_and_icon():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import hud_sheet
    stock = numpy.zeros((256, 256, 2), numpy.uint8)
    stock[0:28, 0:116] = 7                                           # the stock meter's place: cleared
    stock[200:210, 10:20] = (9, 200)                                 # something else on the sheet: kept
    geometry = "part 0\n0 0 0\n1 0 0\n1 0 0.3\n0 0 0.3\ntris\n0 1 2\n0 2 3\n"
    raw, rect = hud_sheet.icon_sheet(stock.tobytes(), geometry)
    sheet = numpy.frombuffer(raw, numpy.uint8).reshape(256, 256, 2)
    thresholds, alpha = sheet[..., 0], sheet[..., 1]
    ticks = alpha[0:28, 0:116] > 40
    assert sorted(int(value) for value in numpy.unique(thresholds[0:28, 0:116][ticks])) == [
        4 * number for number in range(1, 37)]
    assert thresholds[:, :][0:10][alpha[0:10] > 40].max() <= 72 < thresholds[11:18][alpha[11:18] > 40].min()
    assert (sheet[200:210, 10:20] == (9, 200)).all()
    left, top, right, bottom = rect
    assert (left, top) == (2, 100) and right - left == 86 and bottom > top
    assert alpha[top:bottom, left:right].max() > 100 and alpha[top - 2:bottom + 2, right + 6:].max() == 0
    alphas = hud_sheet.alpha_sheet(bytes(22528), raw)
    plate = numpy.frombuffer(alphas, numpy.uint8)[2048:2048 + 4096].reshape(32, 128)
    assert (plate[0:27, 0:115] == alpha[1:28, 1:116]).all() and plate[27:].max() == 0
    with pytest.raises(hud_sheet.Refused):
        hud_sheet.icon_sheet(bytes(100), geometry)


def test_icon_sheet_tag_keeps_the_stock_tag_but_for_pixels_and_sprite():
    pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import hud_sheet
    stock = bitmap_tag([(2, 2, hek.FORMAT_A8Y8, 0, bytes(8))], sequences=2, sprites=2)
    out = hud_sheet.sheet_tag(stock, bytes(range(8)), (2, 100, 88, 129))
    layout = hek.BitmapTag(out)
    assert layout.pixels(out) == bytes(range(8)) and len(out) == len(stock)
    sprite = layout.first_sprite_offset()
    assert struct.unpack_from(">4f", out, sprite + 8) == (2 / 256, 88 / 256, 100 / 256, 129 / 256)
    assert struct.unpack_from(">2f", out, sprite + 24) == (0.0, 0.0)
    assert out[:layout.pixels_start] == stock[:layout.pixels_start] and out[sprite + 32:] == stock[sprite + 32:]
    with pytest.raises(hek.Refused):
        hud_sheet.sheet_tag(stock, bytes(9), (0, 0, 1, 1))


def test_scope_redraw_keeps_the_ink_and_clears_the_ring_band():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import scope_hd
    details = numpy.zeros((256, 256), numpy.float32)
    details[60:63, 30:200] = 0.6                                     # a line
    details[100:110, 10:60] = 1.0                                    # the small print's place: removed
    alpha, level, ink, wanted = scope_hd.details(details, "t2")
    assert alpha.shape == (1024, 1024) and alpha.dtype == numpy.uint8
    assert abs(ink - wanted) / wanted < 0.01 and 0.02 < level < 0.9
    assert alpha[400:480, 20:300].max() == 0                         # where the small print was
    rows = numpy.nonzero(alpha.max(axis=1) > 128)[0]
    assert 4 * (60 + 7) - 6 <= rows.min() and rows.max() <= 4 * (63 + 7) + 6     # moved by 7, drawn at 4x
    yy, xx = numpy.mgrid[0:512, 0:1024]
    radius = numpy.hypot(xx - 511.5, yy - 255.5)
    mask = numpy.where(radius > 140, 1.0, 0.0).astype(numpy.float32)
    mask[(radius >= 126.5) & (radius < 127.6)] = 1.0                 # the ring's inner circle
    ring, wide, measured = scope_hd.ring_and_mask(mask)
    assert abs(measured["centre"][0] - 511.5) < 0.6 and abs(measured["centre"][1] - 255.5) < 0.6
    assert abs(measured["inner"] - 126.5) < 0.8
    assert ring.shape == (1024, 1024) and wide.shape == (512, 2048)
    assert wide[256, 1024 + 127] == 0 and wide[256, 1024 + 160] == 255 and wide[256, 5] == 255
    with pytest.raises(scope_hd.Refused):
        scope_hd.details(details, "t9")


# ---------- the recipe

def recipe():
    return json.loads(RECIPE.read_text())


def test_recipe_names_no_machine_and_pins_its_inputs():
    text = RECIPE.read_text()
    # (no user folder of any machine, no path that climbs out: locations are make_pack.py's arguments)
    assert not re.search("/" + "home" + r"/|~/|[A-Za-z]:\\\\Users|\.\./", text)
    data = recipe()
    assert re.fullmatch(r"[0-9a-f]{64}", data["source"]["sha256"])
    assert re.fullmatch(r"[0-9a-f]{64}", data["stock_map"]["sha256"])
    assert data["expected"]["donor_sha256"] == V6E_SHA256
    assert data["tools"]["invader"]["version"] == "0.44.0"
    for name in ("invader-build", "invader-extract", "invader-edit", "invader-bitmap", "invader-sound",
                 "invader-convert", "invader-bludgeon"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["tools"]["invader"]["sha256"][name])
    assert all(re.fullmatch(r"[0-9a-f]{64}", digest) for digest in data["art"].values())
    make_pack.check_recipe(data)


def test_recipe_layers_are_v6es_folders_in_its_order():
    data = recipe()
    assert data["layers"] == ["fix-3x", "fix-counter-e", "fix-light", "fix-col", "fix-hud-hd", "fix-scope-hd",
                              "fix-v6", "fix-v3", "fix2"]
    counts = {}
    for step in data["steps"]:
        counts[step["layer"]] = counts.get(step["layer"], 0) + 1
    # one step a tag of each of the spike's fix folders
    assert counts == {"fix2": 68, "fix-v3": 11, "fix-light": 2, "fix-col": 1, "fix-hud-hd": 1, "fix-scope-hd": 3,
                      "fix-v6": 5, "fix-3x": 2, "fix-counter-e": 3}
    outputs = [(step["layer"], step["tag"]) for step in data["steps"]]
    assert len(set(outputs)) == len(outputs) == 96


def test_recipe_allowlist_is_exact():
    data = recipe()
    allowlist = data["source"]["allowlist"]
    assert allowlist == sorted(set(allowlist)) and len(allowlist) > 100
    root = data["source"]["root"]
    assert all(not name.startswith(("/", root)) and ".." not in name.split("/") for name in allowlist)
    assert not any("/util/" in "/" + name or name.startswith(("util/", "scripts/")) for name in allowlist)
    used = make_pack.recipe_inputs(data)["cmt"] | set(data["source"]["verbatim"])
    assert set(allowlist) == used                                    # nothing unpacked that nothing uses
    # the sounds: CMT's own (decision 2), each listed: the ten the approved donor holds and no other (CMT's own
    # melee and posing sounds for the rifle, its spec-ops and "super" shots are not in it: the rifle's animations
    # name CMT's assault rifle melee and shared grenade sounds)
    sounds = "cmt/sounds/sfx/"
    assert sorted(name for name in allowlist if name.endswith((".sound", ".sound_looping"))) == sorted(
        sounds + name + ".sound" for name in (
            "weapons/human/battle_rifle/fx/battle_rifle_bullet_fire_1",
            "weapons/human/battle_rifle/fx/battle_rifle_bullet_fire_2",
            "weapons/human/battle_rifle/fx/battle_rifle_bullet_fire_3",
            "weapons/human/battle_rifle/fx/battle_rifle_ammo_pickup",
            "weapons/human/battle_rifle/animations/first-person reload",
            "weapons/human/battle_rifle/animations/first-person ready",
            "weapons/human/assault_rifle/animations/first-person melee",
            "weapons/_shared/animations/weapon_fp_generic_grenade",
            "effects/impacts/flesh_impact", "effects/impacts/bullet_impact_terrain"))
    assert not any(part in name for name in allowlist for part in ("specops", "super_fire"))


def test_recipe_steps_take_nothing_from_outside():
    data = recipe()
    made = set()
    for step in data["steps"]:
        for kind, name in make_pack.step_inputs(step):
            if kind == "cmt":
                assert name in data["source"]["allowlist"], (step["tag"], name)
            elif kind == "layer":
                assert name in made, "%s uses %s before it is made" % (step["tag"], name)   # no tag from nowhere
            elif kind == "art":
                assert name in data["art"], (step["tag"], name)
            else:
                assert kind == "stock", kind
        made.add((step["layer"], step["tag"]))
        assert step["op"] in make_pack.OPERATIONS, step["op"]
        assert step["tag"] == step["tag"].lower() and "\\" not in step["tag"]
    assert make_pack.unknown_scripts(data) == []


def test_recipe_pack_arguments():
    data = recipe()["pack"]
    assert data["id"] == "ae_battle_rifle" and data["prefix"] == "ae\\packs\\battle_rifle\\"
    assert data["weapon"] == "cmt\\weapons\\evolved\\battle_rifle\\battle_rifle"
    assert data["icon"] == "ae_pack\\hud\\br_meter:0"                 # the named sequence (R30)
    assert data["smooth"] == ["ae_pack\\hud\\br_meter_smooth:coverage", "ae_pack\\hud\\br_alphas_smooth"]
    assert (data["weapon_key"], data["labels"], data["behaviour"], data["replaces"]) == (
        "battle_rifle", "h_br,ar", 1, 2)
    texts = (TOOLS / "battle_rifle" / data["texts"]).read_text()
    assert "weapon_pickup=Picked up a battle rifle\n" in texts
    assert "ammo_pickup=Picked up %d rounds for battle rifle\n" in texts and texts.count("%") == 1
    credits = (TOOLS / "battle_rifle" / data["credits"]).read_text()
    assert "CMT" in credits and "Beaucephal" in credits and "Xalener" in credits
    arguments = make_pack.builder_arguments(recipe(), "donor.map", "names.txt", "out.aepack", TOOLS / "battle_rifle")
    assert arguments[arguments.index("--icon") + 1] == "ae_pack\\hud\\br_meter:0"
    assert [arguments[index + 1] for index, word in enumerate(arguments) if word == "--smooth"] == data["smooth"]
    assert "--power" not in arguments and arguments[arguments.index("--revision") + 1] == str(data["revision"])


# ---------- make_pack.py's own rules

@pytest.mark.parametrize("path", ["/tmp/br", "/tmp", "/var/tmp/x", "/dev/shm/x", "~/halo-test/wp2/new",
                                  "~/halo-test/r5", "~/halo-test/r6/x/y", "relative/work"])
def test_work_folder_refused(path):
    with pytest.raises(make_pack.Refused):
        make_pack.check_work_folder(Path(path).expanduser() if path.startswith("~") else Path(path))


def test_work_folder_accepted(tmp_path, monkeypatch):
    monkeypatch.setattr(make_pack, "TEMPORARY_ROOTS", ())
    make_pack.check_work_folder(tmp_path / "br")


def audit_row(name, source, group="bitm", size=10):
    return {"pack_name": name, "source": source, "group": group, "size": size}


def test_audit_rules():
    allowlist = {"cmt/a/b.bitmap", "cmt/sounds/fire.sound"}
    scripts = {"counter_art.py"}
    good = [audit_row("ae\\packs\\x\\a\\b", "cmt/a/b.bitmap"),
            audit_row("ae\\packs\\x\\hud\\meter", "AE script counter_art.py"),
            audit_row("ae\\packs\\x\\sounds\\fire", "cmt/sounds/fire.sound", group="snd!")]
    assert make_pack.audit_problems(good, allowlist, scripts) == []
    bad = [audit_row("ae\\packs\\x\\c", "cmt/c.bitmap"), audit_row("ae\\packs\\x\\d", "AE script other.py"),
           audit_row("ae\\packs\\x\\snd", "AE script counter_art.py", group="snd!"),
           audit_row("ae\\packs\\x\\e", ""), audit_row("ae\\packs\\x\\f", "stock weapons/x.bitmap")]
    problems = make_pack.audit_problems(bad, allowlist, scripts)
    assert len(problems) == 5 and "sound" in problems[2]


def test_externals_rules():
    stock = {("bitm", "ui\\a"): b"same", ("effe", "fx\\b"): b"stock"}
    donor = {("bitm", "ui\\a"): b"same", ("effe", "fx\\b"): b"changed"}
    problems = make_pack.external_problems([("bitm", "ui\\a"), ("effe", "fx\\b"), ("snd!", "s\\c")], donor, stock)
    assert len(problems) == 2 and "fx\\b" in problems[0] and "s\\c" in problems[1]
    assert make_pack.external_problems([("bitm", "ui\\a")], donor, stock) == []


def test_a_stock_named_tag_from_a_fix_folder_is_caught(tmp_path):
    """the builder leaves a tag out of the pack by its group and name alone: a fix folder's tag under a stock
    name must be found as not the stock map's own"""
    def write(root, name, group, body):
        path = tmp_path / root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(tag_file(group, body))

    write("stock", "ui/hud/meter.bitmap", b"bitm", b"stock pixels")
    write("stock", "sound/fire.sound", b"snd!", b"stock samples")
    write("stock", "weapons/ar/ar.weapon", b"weap", b"stock")
    write("fix", "ui/hud/meter.bitmap", b"bitm", b"CHANGED pixels")      # a changed tag under a stock name
    write("fix", "ae_pack/hud/own.bitmap", b"bitm", b"ours")
    write("cmt", "sound/fire.shader_model", b"soso", b"another group, the same path")
    order = [("fix", tmp_path / "fix"), ("cmt", tmp_path / "cmt"), ("stock", tmp_path / "stock")]
    build = make_pack.resolve_sources(order)
    stock = make_pack.index_tags(tmp_path / "stock")
    assert build[("bitm", "ui\\hud\\meter")][0] == "fix" and build[("snd!", "sound\\fire")][0] == "stock"
    assert ("soso", "sound\\fire") in build and ("bitm", "ae_pack\\hud\\own") not in stock

    def fingerprints(table, keys):
        return {key: hashlib.sha256(Path(table[key][1] if isinstance(table[key], tuple) else table[key])
                                    .read_bytes()).hexdigest() for key in keys if key in table}

    externals = [("bitm", "ui\\hud\\meter"), ("snd!", "sound\\fire"), ("weap", "weapons\\ar\\ar")]
    problems = make_pack.external_problems(externals, fingerprints(build, externals), fingerprints(stock, externals))
    assert len(problems) == 1 and "ui\\hud\\meter" in problems[0] and "not the stock map's own" in problems[0]


def test_corrected_donor_is_refused():
    assert make_pack.corrections("ae_pack_build: 238 tags (186 own, 52 external), 0 corrections\n") == 0
    assert make_pack.corrections("x\nthe donor: 3 corrections\n") == 3
    with pytest.raises(make_pack.Refused):
        make_pack.require_no_corrections("the donor: 3 corrections\n")
    with pytest.raises(make_pack.Refused):
        make_pack.require_no_corrections("no count printed\n")
    make_pack.require_no_corrections("... 0 corrections\n")


def test_fix_comparison_rows(tmp_path):
    made, reference = tmp_path / "made", tmp_path / "reference"
    for folder in (made, reference):
        (folder / "a").mkdir(parents=True)
    (made / "a" / "x.bitmap").write_bytes(b"same")
    (reference / "a" / "x.bitmap").write_bytes(b"same")
    (made / "a" / "y.weapon").write_bytes(b"one")
    (reference / "a" / "y.weapon").write_bytes(b"two")
    (made / "a" / "z.sound").write_bytes(b"only here")
    rows = make_pack.compare_layer("fix2", made, reference, lambda *_: 3)
    assert rows == [("fix2", "a/x.bitmap", "byte-equal", 4, 4, ""),
                    ("fix2", "a/y.weapon", "differs", 3, 3, "3 fields"),
                    ("fix2", "a/z.sound", "no reference", 9, 0, "")]
    rows = make_pack.compare_layer("fix2", made, reference, lambda *_: 0)
    assert rows[1][2:] == ("0 field differences", 3, 3, "header bytes")      # (the bytes are in the header's 64)
    # the fields listed alike but bytes after the header not (pixels, samples): not "0 field differences"
    (made / "a" / "y.weapon").write_bytes(bytes(64) + b"pixels one")
    (reference / "a" / "y.weapon").write_bytes(bytes(64) + b"pixels two")
    rows = make_pack.compare_layer("fix2", made, reference, lambda *_: 0)
    assert rows[1][2:] == ("differs", 74, 74, "data bytes")
    (reference / "a" / "w.bitmap").write_bytes(b"x")
    assert ("fix2", "a/w.bitmap", "not made", 0, 1, "") in make_pack.compare_layer("fix2", made, reference,
                                                                                   lambda *_: 0)


# ---------- a pack's tags against a map's

BASE = donor_map.TAG_BASE


def tiny_map(tags, base=BASE):
    """an uncompressed-tags Xbox map image: (group, name, root bytes with pointers already absolute, extra blobs)"""
    header_size = 0x24
    table = base + header_size
    data_start = header_size + 32 * len(tags)
    blob = bytearray()
    records = bytearray()
    for index, (group, name, root) in enumerate(tags):
        name_address = base + data_start + len(blob)
        blob += name.encode() + b"\0"
        while len(blob) % 4:
            blob += b"\0"
        root_address = base + data_start + len(blob)
        blob += root
        records += struct.pack("<4sIIIIIII", group[::-1], 0xFFFFFFFF, 0xFFFFFFFF, 0xE1740000 + index, name_address,
                               root_address, 0, 0)
    head = struct.pack("<IIIIIIIII", table, 0xE1740000, 0, len(tags), 0, 0, 0, 0, 0x74616773)
    return head + bytes(records) + bytes(blob), data_start


def test_tag_comparison_masks_only_what_the_builder_blanks():
    # a root of 32 bytes: a block {count, address, definition} at 4, a reference at 16; one element of 8 bytes
    element = b"ELEMENT!"
    root = bytearray(32)
    struct.pack_into("<I", root, 0, 0x11223344)
    struct.pack_into("<III", root, 4, 1, 0, 0)
    root[16:20] = b"mtib"
    struct.pack_into("<III", root, 20, 0x80001234, 0, 0xE1740001)
    image, data_start = tiny_map([(b"weap", "w\\rifle", bytes(root) + element), (b"bitm", "b\\skin", bytes(8))])
    tags = donor_map.Tags(image)
    rifle = tags.find("weap", "w\\rifle")
    address = rifle.address
    patched = bytearray(image)
    struct.pack_into("<I", patched, address - BASE + 8, address + 32)             # the block's elements
    tags = donor_map.Tags(bytes(patched))
    # the pack's copy: addresses 0, the reference's name 0 and index NONE, fixups for both
    copy = bytearray(root) + element
    struct.pack_into("<II", copy, 8, 0, 0)
    struct.pack_into("<I", copy, 20, 0)
    struct.pack_into("<I", copy, 28, 0xFFFFFFFF)
    fixups = [(8, aepack.FIXUP_POINTER, 0, 32), (16, aepack.FIXUP_REFERENCE, 0, 1)]
    assert pack_compare.compare_tag(bytes(copy), fixups, [], tags, tags.find("weap", "w\\rifle")) == []
    # one byte of the tag's own data changed: found, with its offset
    wrong = bytearray(copy)
    wrong[1] ^= 0xFF
    assert pack_compare.compare_tag(bytes(wrong), fixups, [], tags, tags.find("weap", "w\\rifle")) == [
        "byte 1 of the root: pack 0xcc, map 0x33"]
    wrong = bytearray(copy)
    wrong[35] ^= 1
    problems = pack_compare.compare_tag(bytes(wrong), fixups, [], tags, tags.find("weap", "w\\rifle"))
    assert len(problems) == 1 and "byte 3 of the bytes at 32" in problems[0]
    # a reference's group is NOT masked
    wrong = bytearray(copy)
    wrong[16:20] = b"!dns"
    assert len(pack_compare.compare_tag(bytes(wrong), fixups, [], tags, tags.find("weap", "w\\rifle"))) == 4


def test_tag_comparison_of_a_sound_takes_the_validators_reset_and_nothing_else_there():
    # a sound's root (0xA4 bytes): one pitch range of 72 bytes whose played permutation mask the map has as -1
    root = bytearray(0xA4)
    struct.pack_into("<III", root, 0x98, 1, 0, 0)
    pitch_range = bytearray(72)
    pitch_range[0:7] = b"default"
    struct.pack_into("<I", pitch_range, 52, 0xFFFFFFFF)
    image, _ = tiny_map([(b"snd!", "s\\fire", bytes(root) + bytes(pitch_range))])
    tags = donor_map.Tags(image)
    address = tags.find("snd!", "s\\fire").address
    patched = bytearray(image)
    struct.pack_into("<I", patched, address - BASE + 0x9C, address + 0xA4)
    tags = donor_map.Tags(bytes(patched))
    copy = bytearray(root) + pitch_range
    struct.pack_into("<I", copy, 0xA4 + 52, 0)
    fixups = [(0x9C, aepack.FIXUP_POINTER, 0, 0xA4)]
    sound = tags.find("snd!", "s\\fire")
    counts = {}
    assert pack_compare.compare_tag(bytes(copy), fixups, [], tags, sound, counts=counts) == []
    assert counts == {"compared": 0xA4 + 72 - 8 - 4, "masked": 8 + 4, "pointers": 8, "sound_played_masks": 4}
    wrong = bytearray(copy)
    struct.pack_into("<I", wrong, 0xA4 + 52, 0xFFFFFFFF)                 # the map's value kept: not what a pack holds
    assert pack_compare.compare_tag(bytes(wrong), fixups, [], tags, sound) == [
        "pitch range 0's played permutation mask is not 0 in the pack"]
    wrong = bytearray(copy)
    wrong[0xA4 + 48] ^= 1                                                # the field before it is compared
    assert len(pack_compare.compare_tag(bytes(wrong), fixups, [], tags, sound)) == 1


def test_tags_of_a_map_found_by_group_and_name_case_aside():
    image, _ = tiny_map([(b"weap", "W\\Rifle", bytes(8)), (b"bitm", "b\\skin", bytes(8))])
    tags = donor_map.Tags(image)
    assert tags.find("weap", "w\\rifle").name == "W\\Rifle" and tags.find("bitm", "w\\rifle") is None
    assert sorted(tags.names()) == [("bitm", "b\\skin"), ("weap", "w\\rifle")]
    with pytest.raises(donor_map.Refused):
        donor_map.Tags(image[:20])


# ---------- on the build box: a finished run

def finished_run():
    return (WORK / "fixes.tsv").is_file() and (WORK / "pack" / "ae_battle_rifle.aepack").is_file()


box = pytest.mark.skipif(not finished_run(), reason="no finished make_pack.py run in " + str(WORK))


def rows_of(path):
    lines = path.read_text().splitlines()
    names = lines[0].split("\t")
    return [dict(zip(names, line.split("\t"))) for line in lines[1:]]


@box
def test_run_every_fix_tag_reproduced():
    rows = rows_of(WORK / "fixes.tsv")
    assert len(rows) == 96
    assert {row["result"] for row in rows} <= {"byte-equal", "0 field differences"}, [
        row for row in rows if row["result"] not in ("byte-equal", "0 field differences")]


@box
def test_run_donor_is_the_approved_one():
    donor = WORK / "donor" / "donor.map"
    digest = hashlib.sha256(donor.read_bytes()).hexdigest()
    assert digest == recipe()["expected"]["donor_sha256"] == V6E_SHA256
    if V6E_DONOR.is_file():
        assert donor.read_bytes() == V6E_DONOR.read_bytes()


@box
def test_run_pack_is_the_recorded_one_and_reads():
    pack = (WORK / "pack" / "ae_battle_rifle.aepack").read_bytes()
    expected = recipe()["expected"]
    assert hashlib.sha256(pack).hexdigest() == expected["pack_sha256"] and len(pack) == expected["pack_size"]
    parsed = aepack.read(pack)
    assert parsed.own_tag_count == expected["own_tags"]
    assert len(parsed.tags) - parsed.own_tag_count == expected["external_tags"]


@box
def test_run_audit_and_externals():
    data = recipe()
    audit = rows_of(WORK / "audit.tsv")
    assert len(audit) == data["expected"]["own_tags"]
    rows = [{"pack_name": row["pack_name"], "source": row["source"], "group": row["group"], "size": row["size"]}
            for row in audit]
    assert make_pack.audit_problems(rows, set(data["source"]["allowlist"]), make_pack.scripts()) == []
    sounds = [row for row in rows if row["group"] == "snd!"]
    assert sounds and all(row["source"].endswith(".sound") and row["source"] in data["source"]["allowlist"]
                          for row in sounds)
    externals = rows_of(WORK / "externals.tsv")
    assert len(externals) == data["expected"]["external_tags"]
    assert all(row["same_as_stock"] == "yes" and row["maps"] == "13" for row in externals)


@box
def test_run_every_external_is_on_all_thirteen_stock_maps():
    if not all((STOCK_MAPS / (name + ".map")).is_file() for name in MULTIPLAYER_MAPS):
        pytest.skip("the stock multiplayer maps are not here")
    externals = [(row["group"], row["name"].lower()) for row in rows_of(WORK / "externals.tsv")]
    for name in MULTIPLAYER_MAPS:
        names = set(donor_map.Tags(donor_map.read_map(STOCK_MAPS / (name + ".map"))).names())
        missing = [external for external in externals if external not in names]
        assert not missing, (name, missing)


@box
def test_run_pack_tags_are_the_approved_donors_bytes():
    if not V6E_DONOR.is_file():
        pytest.skip("the approved donor is not here")
    pack = (WORK / "pack" / "ae_battle_rifle.aepack").read_bytes()
    report = pack_compare.compare_pack(pack, donor_map.read_map(V6E_DONOR))
    assert report["own"] == recipe()["expected"]["own_tags"]
    assert report["problems"] == []
    assert report["compared_bytes"] > 100000 and report["masked_bytes"] < report["compared_bytes"]
