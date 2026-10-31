"""The weapon pack recipe's own scripts, tools/weapon_pack/ (make_pack.py and what it runs), and the battle rifle's
recipe, tools/weapon_pack/battle_rifle/recipe.json.

Everywhere (CI): every script on bytes made here (no game file, no file of anyone's art): Open Sauce blocks cut
from tag files, DXT1 / DXT5 / 32-bit channel moves, the brightening of DXT endpoints, high-resolution HUD scales
halved, Ogg permutations cut out of a sound tag, the field edits' command lines and the copy of one part of a tag
onto another, the allowlist extractor's refusals, the recipe's own rules (every fix tag one step, every input in
the allowlist or made by a script here, no path of a machine, no pixel of the game's, the departures from the
approved donor named), the audit's rules (a tag judged by what its steps read), art pinned by its pixels, the
comparison of a pack's tags with a map's (each mask exactly the field the builder blanks). The art scripts (the
counter's rounds and their thresholds 4 n, the gun screen's digits and compass, the scope's label, the prompts'
icon) need numpy and Pillow and are skipped without them.

On the build box (needs the maps; skipped elsewhere): a finished run of make_pack.py is checked: fixes.tsv differs
from the prototype's tags only in the recipe's named departures, the donor's and the pack's SHA-256s are the
recipe's, audit.tsv and externals.tsv hold their rules, every external name is on all 13 stock multiplayer maps,
the pack is its donor's bytes (tags, resources, geometry), and it differs from the approved donor (v6e) only in
the tags the recipe names. Paths (each may be set in the environment):
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
import zlib
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
import png_pixels  # noqa: E402
import sound_oggs  # noqa: E402
import tag_fields  # noqa: E402

RECIPE = TOOLS / "battle_rifle" / "recipe.json"
# the donor the owner approved (v6e): the recipe departs from it in named tags only (R33)
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


def particle_system_with_extension(extension_path=b"a\\b"):
    """a particle_system file in the stock kit's layout: the root (a point physics path), one particle type with one
    state and one particle state; the particle state's Open Sauce block (at 0xE8) holds one 48-byte element with a
    bitmap reference and its path, between the state's point physics path and its secondary map's"""
    root = bytearray(104)
    root[56:60] = b"pphy"
    struct.pack_into(">iI", root, 56 + 8, 4, 0xFFFFFFFF)
    struct.pack_into(">i", root, 92, 1)                     # one particle type
    kind = bytearray(128)
    struct.pack_into(">i", kind, 92, 1)                     # one physics constant
    struct.pack_into(">i", kind, 104, 1)                    # one state
    struct.pack_into(">i", kind, 116, 1)                    # one particle state
    state = bytearray(192)
    struct.pack_into(">i", state, 180, 2)                   # two physics constants
    particle = bytearray(376)
    for offset, path in ((48, b"bits"), (132, b""), (244, b"second")):
        particle[offset:offset + 4] = b"bitm"
        struct.pack_into(">iI", particle, offset + 8, len(path), 0xFFFFFFFF)
    struct.pack_into(">iII", particle, 0xE8, 1, 0x1234, 0x5678)
    extension = bytearray(48)
    extension[0:4] = b"bitm"
    struct.pack_into(">iI", extension, 8, len(extension_path), 0xFFFFFFFF)
    cut = bytes(extension) + (extension_path + b"\0" if extension_path else b"")
    body = (bytes(root) + b"phys\0" + bytes(kind) + bytes(4) + bytes(state) + bytes(8) + bytes(particle) +
            b"bits\0" + cut + b"second\0")
    return tag_file(b"pctl", body), cut


def test_os_strip_cuts_a_particle_systems_blocks():
    tag, cut = particle_system_with_extension()
    out, removed, elements = os_strip.strip("particle_system", tag)
    assert (removed, elements) == (len(cut), 1)
    assert len(out) == len(tag) - len(cut) and out.endswith(b"bits\0second\0")
    assert cut not in out
    # (the block's count, address and definition cleared; a second strip finds nothing)
    assert os_strip.strip("particle_system", out) == (out, 0, 0)


def test_os_strip_refuses_a_particle_system_that_does_not_end_where_its_layout_does():
    tag, _ = particle_system_with_extension()
    with pytest.raises(hek.Refused, match="ends at"):
        os_strip.strip("particle_system", tag + b"x")
    with pytest.raises(hek.Refused):
        os_strip.strip("particle_system", tag[:-3])


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
            raise tag_fields.Failed("the editor failed: " + key)
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


def test_hud_hires_a_tag_with_empty_blocks_is_left_and_a_failing_editor_is_not_nothing_to_do():
    empty = {"static_elements": 0, "meter_elements": 0, "number_elements": 0, "crosshairs": 0, "overlay_elements": 0}
    editor = FakeEditor(empty, {})
    assert hud_hires_to_xbox.convert(editor) == [] and editor.sets == []
    del empty["crosshairs"]
    with pytest.raises(tag_fields.Failed):
        hud_hires_to_xbox.convert(FakeEditor(empty, {}))


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
                                  off_screen=(-3000, -3000), icon_sequence=0)
    assert edits[0] == ["copy", "meter_elements[0]", "end"]
    sets = dict(edits[1][1])
    assert edits[1][0] == "sets"
    assert sets["meter_elements[1].meter_bitmap"] == "ae_pack\\hud\\br_meter_smooth.bitmap"
    assert sets["meter_elements[1].width_scale"] == "0.25" and sets["static_elements[0].height_scale"] == "0.25"
    assert sets["static_elements[0].anchor_offset"] == "-1 20" and sets["meter_elements[1].sequence_index"] == "0"
    assert not any(key.startswith("meter_elements[0].") for key in sets)
    # the icon sheet's element: off the screen, showing the sheet's one sequence
    assert edits[2:] == [["set", "meter_elements[0].anchor_offset", "-3000 -3000"],
                         ["set", "meter_elements[0].sequence_index", "0"]]
    # the first HUD names our icon sheet and no other bitmap of ours
    assert hud_tag.base_edits("ae_pack\\hud\\br_meter.bitmap") == [
        ["erase", "crosshairs[*]"], ["set", "meter_elements[0].meter_bitmap", "ae_pack\\hud\\br_meter.bitmap"]]


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


def test_icon_is_drawn_from_the_geometry_alone():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import hud_sheet
    geometry = "part 0\n0 0 0\n1 0 0\n1 0 0.3\n0 0 0.3\ntris\n0 1 2\n0 2 3\n"
    picture = hud_sheet.icon(geometry)
    assert picture.shape == (29, 86) and picture.dtype == numpy.uint8
    # a rectangle seen from the side: a grey fill inside a lighter outline, nothing outside it
    assert picture[:2].max() == 0 and picture[:, :1].max() == 0 and picture[-2:].max() == 0
    assert picture[14, 43] == 100 and 150 < picture.max() <= 225 and picture[14, 2] > picture[14, 43]
    plate = hud_sheet.plate(picture)
    assert plate.size == (88, 31) and plate.mode == "RGBA"
    assert plate.getpixel((0, 0)) == (0, 0, 255, 255) and plate.getpixel((87, 30)) == (0, 0, 255, 255)
    assert plate.getpixel((44, 15)) == (100, 100, 100, 100)             # colour and alpha both the picture
    inside = numpy.asarray(plate)[1:-1, 1:-1]
    assert (inside[..., 0] == picture).all() and (inside[..., 3] == picture).all()
    narrow = hud_sheet.icon(geometry, 60)                               # another width, the same drawing
    assert narrow.shape[1] == 60 and narrow.shape[0] < picture.shape[0]
    for width in (8, 300, 60.5):
        with pytest.raises(hud_sheet.Refused):
            hud_sheet.icon(geometry, width)
    with pytest.raises(hud_sheet.Refused):
        hud_sheet.icon("tris\n0 1 2\n")
    with pytest.raises(hud_sheet.Refused):
        hud_sheet.icon("")


def test_icon_sprite_is_placed_by_its_corner():
    import hud_sheet
    sheet = bitmap_tag([(2, 2, hek.FORMAT_A8R8G8B8, 0, bytes(16))], sequences=1, sprites=1)
    layout = hek.BitmapTag(sheet)
    sprite = layout.first_sprite_offset()
    made = bytearray(sheet)
    struct.pack_into(">4f", made, sprite + 8, 0.25, 0.75, 0.25, 0.5)
    struct.pack_into(">2f", made, sprite + 24, 0.5, 0.375)              # the compiler's: the sprite's middle
    out = hud_sheet.place_by_corner(bytes(made))
    assert struct.unpack_from(">2f", out, sprite + 24) == (0.0, 0.0)
    assert out[:sprite + 24] == bytes(made[:sprite + 24]) and out[sprite + 32:] == bytes(made[sprite + 32:])
    with pytest.raises(hek.Refused):                                    # a sheet with more than the one sprite
        hud_sheet.place_by_corner(bitmap_tag([(2, 2, hek.FORMAT_A8R8G8B8, 0, bytes(16))], sequences=2, sprites=2))
    with pytest.raises(hek.Refused):
        hud_sheet.place_by_corner(bitmap_tag([(2, 2, hek.FORMAT_A8R8G8B8, 0, bytes(16))], sequences=1, sprites=3))


def test_counter_layout_another_weapons_rounds():
    """the SMG's counter: the same drawing with its own numbers (60 rounds, 3 rows of 20 in groups of 5)"""
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import counter_art
    layout = {"columns": 20, "rows": 3, "group": 5, "height": 6.2, "width": 2.3, "pitch": 4.5, "group_gap": 1.8}
    rounds = list(counter_art.rounds(layout))
    assert [number for number, _, _ in rounds] == list(range(1, 61))
    assert sorted({top for _, _, top in rounds}) == [counter_art.TOP + row * counter_art.ROW for row in range(3)]
    first = [entry for entry in rounds if entry[0] <= 20]
    gaps = [round(first[index + 1][1] - first[index][1], 6) for index in range(19)]
    assert gaps == [4.5 if index % 5 != 4 else 6.3 for index in range(19)]
    # the rows keep the assault rifle's places: each row starts one row's lean left of the one above
    starts = [entry[1] for entry in rounds if entry[0] in (1, 21, 41)]
    assert [round(starts[0] - starts[1], 6), round(starts[1] - starts[2], 6)] == [
        round(counter_art.ROW * counter_art.LEAN, 6)] * 2
    meter, _ = counter_art.sheets(layout)
    covered = meter[..., 3] == 255
    assert sorted(int(value) for value in numpy.unique(meter[..., 0][covered])) == [4 * n for n in range(1, 61)]
    # the defaults are the battle rifle's, given or not
    assert list(counter_art.rounds({})) == list(counter_art.rounds()) == list(counter_art.rounds(None))
    for bad, reason in (({"colour": 1}, "no such"), ({"columns": 32, "rows": 2}, "at most 63"),
                        ({"rows": 0}, "positive"), ({"columns": 30, "rows": 2, "pitch": 4.5}, "past the")):
        with pytest.raises(counter_art.Refused) as refusal:
            counter_art.layout_of(bad)
        assert reason in str(refusal.value)


def test_smg_reticle_is_four_arcs_with_ticks_and_a_clear_middle():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import reticle_art
    alpha = reticle_art.smg_reticle().astype(int)
    assert alpha.shape == (512, 512)
    middle = 256
    ring = int(reticle_art.SMG_RING)
    # the arcs up, down, left and right; the diagonals open
    for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        assert alpha[middle + dy * ring, middle + dx * ring] > 200
    diagonal = int(ring / 2 ** 0.5)
    assert alpha[middle - diagonal, middle - diagonal] == 0 and alpha[middle + diagonal, middle + diagonal] == 0
    # the ticks run out past the ring; the middle is clear but for the faint small ring
    assert alpha[middle, middle + int(reticle_art.SMG_TICK_END) - 4] > 200
    assert alpha[middle, middle + int(reticle_art.SMG_TICK_END) + 4] == 0
    assert alpha[middle, middle] == 0 and alpha[middle - 30:middle + 30, middle - 30:middle + 30].max() == 0
    assert 80 < alpha[middle, middle + int(reticle_art.SMG_EYE)] <= reticle_art.SMG_EYE_ALPHA
    # symmetric four ways (but for the drawing's edge texels, a supersample apart)
    assert numpy.abs(alpha - alpha[::-1, :]).mean() < 1 and numpy.abs(alpha - alpha.T).mean() < 1


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


def test_bold_scope_label_drawn_or_left_out():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import scope_hd
    sheet = numpy.zeros((1024, 1024), numpy.float32)
    sheet[700:770, 590:700] = 1.0                                    # CMT's "[2x]"
    sheet[920:950, 600:640] = 1.0                                    # its "1x"
    sheet[100:104, 100:900] = 1.0                                    # a scope line
    labelled = scope_hd.bold_label(sheet, "3")
    bare = scope_hd.bold_label(sheet, "")                            # no zoom indication (the owner, 10-30)
    assert labelled[680:790, 570:724].max() > 0.9 and labelled[905:962, 592:652].max() == 0
    assert bare[680:790, 570:724].max() == 0 and bare[905:962, 592:652].max() == 0
    assert bare[100:104, 100:900].min() == 1.0 and numpy.array_equal(bare, scope_hd.bold_label(sheet, None))
    with pytest.raises(scope_hd.Refused):
        scope_hd.bold_label(sheet, "4")


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
    # the approved donor stays named; the recipe's own donor departs from it (R33) and is pinned too
    assert data["approved_reference"]["donor_sha256"] == V6E_SHA256
    assert re.fullmatch(r"[0-9a-f]{64}", data["expected"]["donor_sha256"]) and data["expected"][
        "donor_sha256"] != V6E_SHA256
    assert re.fullmatch(r"[0-9a-f]{64}", data["expected"]["pack_sha256"])
    assert re.fullmatch(r"[0-9a-f]{64}", data["expected"]["pack_own_sha256"])
    assert data["tools"]["invader"]["version"] == "0.44.0"
    for name in ("invader-build", "invader-extract", "invader-edit", "invader-bitmap", "invader-sound",
                 "invader-convert", "invader-bludgeon"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["tools"]["invader"]["sha256"][name])
    # art is pinned by what is drawn (decoded pixels, mode, size), never by a file's bytes
    assert all(set(entry) == {"mode", "size", "pixels_sha256"} and re.fullmatch(r"[0-9a-f]{64}",
               entry["pixels_sha256"]) for entry in data["art"].values())
    assert all(name.endswith(".png") for name in data["art"])
    make_pack.check_recipe(data)


def test_recipe_layers_are_v6es_folders_in_its_order():
    data = recipe()
    assert data["layers"] == ["fix-3x", "fix-counter-e", "fix-light", "fix-col", "fix-hud-hd", "fix-scope-hd",
                              "fix-v6", "fix-v3", "fix2"]
    counts = {}
    for step in data["steps"]:
        counts[step["layer"]] = counts.get(step["layer"], 0) + 1
    # one step a tag of each of the spike's fix folders, less the two R33 does not make (the cube map's copy in
    # fix2, the stock alpha sheet's in fix-v3); and the owner's changes of 2026-10-13: the three firing effects'
    # smaller flash (fix-light) and the aiming reticle's bitmap (fix-scope-hd)
    assert counts == {"fix2": 67, "fix-v3": 10, "fix-light": 5, "fix-col": 1, "fix-hud-hd": 1, "fix-scope-hd": 4,
                      "fix-v6": 5, "fix-3x": 2, "fix-counter-e": 3}
    outputs = [(step["layer"], step["tag"]) for step in data["steps"]]
    assert len(set(outputs)) == len(outputs) == 98


def test_recipe_departures_from_the_approved_donor_are_named():
    data = recipe()
    made = {"%s/%s" % (step["layer"], step["tag"]) for step in data["steps"]}
    departures = data["departures"]
    assert len(departures) == 6 and all(len(reason) > 20 for reason in departures.values())
    assert sorted(name for name in departures if name not in made) == [
        "fix-v3/ae_pack/hud/br_alphas.bitmap", "fix2/levels/a10/bitmaps/cube map glass.bitmap"]     # not made
    assert sorted(data["approved_reference"]["pack_departures"]) == [
        "ae_pack\\hud\\br_ammo", "ae_pack\\hud\\br_meter",
        "cmt\\weapons\\_shared\\smg\\shaders\\smg_attachments_silencer"]
    assert list(data["approved_reference"]["absent"]) == ["levels\\a10\\bitmaps\\cube map glass"]


def test_recipe_takes_no_pixel_of_the_games():
    """R33: the stock map gives the donor its scaffolding (scenario, globals) and the ammunition HUD its structure;
    no step copies a stock tag, no art script reads one, and the cube map is named, not carried"""
    data = recipe()
    stock = {}
    for step in data["steps"]:
        for kind, name in make_pack.step_inputs(step):
            if kind == "stock":
                stock.setdefault(name, []).append((step["op"], step["tag"]))
    assert stock == {
        "levels/test/bloodgulch/bloodgulch.scenario": [("edit", "ae_pack/donor/donor.scenario")],
        "globals/globals.globals": [("edit", "globals/globals.globals")],
        "weapons/assault rifle/assault rifle.weapon_hud_interface": [
            ("ammo_hud", "ae_pack/hud/br_ammo.weapon_hud_interface")]}
    assert all(entry["root"] == "cmt" for entry in data["art_inputs"].values())
    assert not any(step["op"] == "copy" and step["from"][0] == "stock" for step in data["steps"])
    silencer = [step for step in data["steps"] if step["tag"].endswith("smg_attachments_silencer.shader_model")]
    assert len(silencer) == 1 and silencer[0]["edits"] == [
        ["set", "reflection_cube_map", "characters\\cyborg\\bitmaps\\cubemap mirror surface.bitmap"]]
    assert "rifle's own model" in silencer[0]["why"] and "glass impact" not in RECIPE.read_text()
    sheet = [step for step in data["steps"] if step["tag"] == "ae_pack/hud/br_meter.bitmap"]
    assert len(sheet) == 1 and sheet[0]["op"] == "bitmap" and sheet[0]["art"] == ["br_icon.png"]
    assert "from" not in sheet[0] and data["art_scripts"]["br_icon.png"] == "hud_sheet.py"


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


def test_recipe_names_the_own_tags_made_from_stock_tags():
    data = recipe()["pack"]
    # one is left (R33): the ammunition HUD, which is layout numbers in the stock HUD's structure, no art
    assert list(data["stock_derived"]) == ["ae\\packs\\battle_rifle\\hud\\br_ammo"]
    assert "layout numbers set by AE" in data["stock_derived"]["ae\\packs\\battle_rifle\\hud\\br_ammo"]


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


@pytest.mark.parametrize("kind", ["battle_rifle", "carbine", "smg", "smg_silenced", "dmr", "evo_rifle"])
def test_every_pack_weapon_melees_as_the_stock_assault_rifle(kind):
    """the owner (2026-10-22): a pack weapon's melee took about five blows from the front. CMT's melee is 30 flat
    at 1.0 against a Spartan; every pack weapon names the stock assault rifle's melee damage and response instead
    (left to the map, on every stock multiplayer map), so it strikes as every stock weapon does. The weapon tag the
    donor takes is the first layer's that makes it, so the edit is followed through the steps it is made from."""
    data = json.loads((TOOLS / kind / "recipe.json").read_text())
    tag = data["pack"]["weapon"].replace("\\", "/") + ".weapon"
    made = {(step["layer"], step["tag"]): step for step in data["steps"]}
    layer = next(layer for layer in data["layers"] if (layer, tag) in made)
    found = {}
    while layer != "cmt":
        step = made[(layer, tag)]
        for edit in step["edits"]:
            pairs = [edit[1:]] if edit[0] == "set" else edit[1] if edit[0] == "sets" else []
            for field, value in pairs:
                found.setdefault(field, value)
        layer, tag = step["from"]
    assert found["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    assert found["player_melee_response"] == "weapons\\assault rifle\\melee_response.damage_effect"
    assert not [name for name in data["source"]["allowlist"]
                if name.startswith("cmt/globals/evolved/damage_effects/melee")]


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


def disc_map(tmp_path, tags_size, inflated_short, stream_cut=0):
    """an Xbox map file as the disc has it (header, then the rest deflated) whose header counts 0x1000 bytes past
    the header, its tags at 0x900; the stream inflates inflated_short bytes less than that, and stream_cut bytes
    are cut off the end of the deflated stream"""
    length = donor_map.MAP_HEADER_SIZE + 0x1000
    header = bytearray(donor_map.MAP_HEADER_SIZE)
    struct.pack_into("<4sIIIII", header, 0, b"daeh", 5, length, 0, 0x900, tags_size)
    header[-4:] = b"toof"
    image, _ = tiny_map([(b"weap", "w\\rifle", bytes(8))])
    rest = bytearray(0x1000 - inflated_short)
    rest[0x100:0x100 + len(image)] = image[:len(rest) - 0x100]
    stream = zlib.compress(bytes(rest))
    path = tmp_path / "disc.map"
    path.write_bytes(bytes(header) + stream[:len(stream) - stream_cut])
    return path, image


def test_a_map_whose_stream_ends_short_past_its_tags_is_read(tmp_path):
    """h2d_gemini (H2 Collection v2) inflates 888 bytes short of its header's length, its tags ending where the
    stream does, and the game loads it: what is missing lies past the tags, so the reader takes it"""
    path, image = disc_map(tmp_path, 0x200, 0x1000 - 0x300)
    whole, tags = donor_map.read_whole_map(path)
    assert len(whole) == donor_map.MAP_HEADER_SIZE + 0x300 and tags[:len(image)] == image
    assert donor_map.Tags(tags).find("weap", "w\\rifle") is not None
    padded, added = donor_map.whole_length_map(path)
    assert added == 0x1000 - 0x300 and len(padded) == donor_map.MAP_HEADER_SIZE + 0x1000
    assert padded[:len(whole)] == whole and not any(padded[len(whole):])
    # the whole length: nothing added
    path, _ = disc_map(tmp_path, 0x200, 0)
    assert donor_map.whole_length_map(path)[1] == 0


def test_a_map_short_inside_its_tags_or_cut_off_is_refused(tmp_path):
    # short by one byte inside the tags
    path, _ = disc_map(tmp_path, 0x200, 0x1000 - 0x2FF)
    with pytest.raises(donor_map.Refused, match="inside its tags"):
        donor_map.read_whole_map(path)
    # a stream cut off before its end (no end of stream): refused however much it holds
    path, _ = disc_map(tmp_path, 0x200, 0, stream_cut=4)
    with pytest.raises(donor_map.Refused, match="could not be inflated"):
        donor_map.read_whole_map(path)


def one_piece_tag(group, size):
    """a map holding one tag of size bytes (each byte its offset, so that any byte can be told), and the tag"""
    root = bytes(index & 0xFF for index in range(size))
    image, _ = tiny_map([(group, "t\\one", root)])
    tags = donor_map.Tags(image)
    return root, tags, tags.find(group.decode(), "t\\one")


def changed_bytes(root, fixups, parts, tags, tag, offsets):
    """which of the offsets, each changed alone in the pack's copy, the comparison reports"""
    reported = []
    for offset in offsets:
        copy = bytearray(root)
        copy[offset] ^= 0xFF
        if pack_compare.compare_tag(bytes(copy), fixups, parts, tags, tag):
            reported.append(offset)
    return reported


def test_bitmap_mask_is_the_pixel_datas_address_and_definition_only():
    # pixel data at 0x30: size, pad, file offset (0x38), address (0x3C), definition (0x40); the builder blanks
    # the last two (ae_pack_build.c) and READS the file offset
    root, tags, tag = one_piece_tag(b"bitm", 0x6C)
    assert changed_bytes(root, [], [], tags, tag, range(0x30, 0x48)) == list(range(0x30, 0x3C)) + list(
        range(0x44, 0x48))
    counts = {}
    assert pack_compare.compare_tag(root, [], [], tags, tag, counts=counts) == []
    assert counts["bitmap_pixel_addresses"] == 8 and counts["masked"] == 8
    # a tag of another group has no such mask
    root, tags, tag = one_piece_tag(b"weap", 0x6C)
    assert changed_bytes(root, [], [], tags, tag, range(0x38, 0x48)) == list(range(0x38, 0x48))


def test_model_part_mask_is_its_two_buffer_structs_only():
    root, tags, tag = one_piece_tag(b"mode", 0x20 + 0x68 + 8)
    part = 0x20
    reported = changed_bytes(root, [], [part], tags, tag, range(part, part + 0x68 + 8))
    assert reported == list(range(part, part + 68)) + list(range(part + 104, part + 0x68 + 8))
    counts = {}
    pack_compare.compare_tag(root, [], [part], tags, tag, counts=counts)
    assert counts["part_buffers"] == 36


def test_tag_index_mask_is_its_four_bytes_only():
    root, tags, tag = one_piece_tag(b"weap", 64)
    fixups = [(20, aepack.FIXUP_TAG_INDEX, 0, 0)]
    assert changed_bytes(root, fixups, [], tags, tag, range(16, 28)) == [16, 17, 18, 19, 24, 25, 26, 27]


def test_pointer_and_reference_masks_cover_exactly_their_fields():
    # a pointer's 8 bytes (address, definition): the count before it and the bytes after are compared
    root = bytes(range(64))
    image, _ = tiny_map([(b"weap", "t\\one", root + bytes(range(100, 108)))])
    tags = donor_map.Tags(image)
    tag = tags.find("weap", "t\\one")
    patched = bytearray(image)
    struct.pack_into("<I", patched, tag.address - BASE + 8, tag.address + 64)
    tags = donor_map.Tags(bytes(patched))
    tag = tags.find("weap", "t\\one")
    copy = root + bytes(range(100, 108))
    fixups = [(8, aepack.FIXUP_POINTER, 0, 64), (32, aepack.FIXUP_REFERENCE, 0, aepack.NONE)]
    reported = changed_bytes(copy, fixups, [], tags, tag, range(0, 56))
    assert reported == [index for index in range(56) if not (8 <= index < 16 or 36 <= index < 40 or
                                                             44 <= index < 48)]


def test_file_offsets_are_set_aside_only_against_another_build():
    root, tags, tag = one_piece_tag(b"snd!", 64)
    copy = bytearray(root)
    copy[40:44] = b"\x01\x02\x03\x04"                        # a file offset that is another in this build
    assert len(pack_compare.compare_tag(bytes(copy), [], [], tags, tag)) == 4
    counts = {}
    assert pack_compare.compare_tag(bytes(copy), [], [], tags, tag, counts=counts, file_offsets=[40]) == []
    assert counts["file_offsets"] == 4


def test_padding_is_counted():
    # a data of 5 bytes at 32, padded to 40 by the builder: the word 12 bytes before its pointer is its size
    root = bytearray(32)
    struct.pack_into("<I", root, 0, 5)
    image, _ = tiny_map([(b"weap", "t\\one", bytes(root) + b"DATA!xyz"), (b"bitm", "after", bytes(4))])
    tags = donor_map.Tags(image)
    tag = tags.find("weap", "t\\one")
    patched = bytearray(image)
    struct.pack_into("<I", patched, tag.address - BASE + 12, tag.address + 32)
    tags = donor_map.Tags(bytes(patched))
    tag = tags.find("weap", "t\\one")
    copy = bytes(root) + b"DATA!\0\0\0"
    counts = {}
    assert pack_compare.compare_tag(copy, [(12, aepack.FIXUP_POINTER, 0, 32)], [], tags, tag, counts=counts) == []
    assert counts["padding"] == 3 and counts["compared"] == 32 - 8 + 5


def test_where_a_tags_bytes_hold_its_resources_offsets():
    # a bitmap: pixel data's file offset at 0x38 (0 in a built map), each bitmap's own at 0x18 of its 48 bytes
    root = bytearray(0x6C + 96)
    struct.pack_into("<I", root, 0x38, 0)
    struct.pack_into("<I", root, 0x60, 2)
    struct.pack_into("<I", root, 0x6C + 0x18, 5000)
    struct.pack_into("<I", root, 0x6C + 48 + 0x18, 9000)
    fixups = [(0x64, aepack.FIXUP_POINTER, 0, 0x6C)]
    resources = [(0, 9000, 16, 0, 16, 0), (0, 5000, 64, 16, 64, 0)]
    assert pack_compare.resource_fields("bitm", bytes(root), fixups, resources) == {
        1: (0x38, 0x6C + 0x18), 0: (0x38, 0x6C + 48 + 0x18)}
    # a sound: the data field whose size and file offset are the resource's
    sound = bytearray(200)
    struct.pack_into("<III", sound, 120, 777, 0, 123456)
    assert pack_compare.resource_fields("snd!", bytes(sound), [], [(0, 123456, 777, 0, 777, 0)]) == {0: (128,)}
    assert pack_compare.resource_fields("snd!", bytes(sound), [], [(0, 123456, 778, 0, 778, 0)]) == {}


def test_map_file_bytes_by_file_offset():
    image, _ = tiny_map([(b"weap", "t\\one", bytes(8))])
    whole = bytes(range(256)) * 9 + image
    map_file = donor_map.MapFile(whole=whole, tag_data=image)
    assert map_file.file_bytes(256, 4) == bytes((0, 1, 2, 3)) and map_file.file_bytes(len(whole) - 2, 4) is None
    assert map_file.tags.find("weap", "t\\one") is not None


# ---------- the audit judges a tag by what its steps read

def synthetic_recipe(steps, art_inputs=None, art_scripts=None):
    return {"steps": steps, "art_inputs": art_inputs or {}, "art_scripts": art_scripts or {},
            "layers": ["a", "b"]}


def test_provenance_follows_the_steps_and_collects_every_input():
    data = synthetic_recipe([
        {"layer": "b", "tag": "x.bitmap", "op": "multipurpose", "from": ["cmt", "cmt/x.bitmap"]},
        {"layer": "a", "tag": "x.bitmap", "op": "edit", "from": ["b", "x.bitmap"], "edits": [["set", "k", "v"]]}])
    assert make_pack.provenance(data, "a", "x.bitmap") == ("cmt/x.bitmap", ["cmt cmt/x.bitmap"])
    with pytest.raises(make_pack.Refused):
        make_pack.provenance(data, "a", "missing.bitmap")


def test_an_art_script_fed_stock_pixels_is_stock_derived_whatever_made_the_tag():
    data = synthetic_recipe(
        [{"layer": "a", "tag": "hud/sheet.bitmap", "op": "bitmap", "art": ["sheet.png"], "arguments": []}],
        art_inputs={"meters.raw": {"kind": "tag", "root": "stock", "tag": "ui/hud/meters.bitmap", "sha256": "0",
                                   "scripts": ["sheet.py"]},
                    "model.txt": {"kind": "geometry", "root": "cmt", "tag": "cmt/rifle.gbxmodel", "sha256": "0",
                                  "scripts": ["sheet.py", "other.py"]}},
        art_scripts={"sheet.png": "sheet.py"})
    source, inputs = make_pack.provenance(data, "a", "hud/sheet.bitmap")
    assert source == "AE script sheet.py"
    assert inputs == ["art sheet.png", "stock ui/hud/meters.bitmap", "cmt cmt/rifle.gbxmodel"]
    row = {"pack_name": "ae\\packs\\x\\hud\\sheet", "source": source, "group": "bitm", "size": 1, "inputs": inputs}
    problems = make_pack.audit_problems([row], set(), {"sheet.py"})
    assert len(problems) == 1 and "stock ui/hud/meters.bitmap" in problems[0]
    assert make_pack.audit_problems([row], set(), {"sheet.py"}, {"ae\\packs\\x\\hud\\sheet"}) == []
    # the same script fed CMT's model alone: ours
    del data["art_inputs"]["meters.raw"]
    source, inputs = make_pack.provenance(data, "a", "hud/sheet.bitmap")
    row = {"pack_name": "ae\\packs\\x\\hud\\sheet", "source": source, "group": "bitm", "size": 1, "inputs": inputs}
    assert make_pack.audit_problems([row], set(), {"sheet.py"}) == []


def test_fields_grafted_from_a_stock_tag_make_a_tag_stock_derived():
    hud = "cmt/hud.weapon_hud_interface"
    data = synthetic_recipe([
        {"layer": "a", "tag": hud, "op": "edit", "from": ["cmt", hud],
         "edits": [["graft", "stock", "weapons/pistol/pistol.weapon_hud_interface", "crosshairs[1]",
                    "crosshairs[0]"]]}])
    source, inputs = make_pack.provenance(data, "a", hud)
    assert source == hud
    assert inputs == ["cmt " + hud, "stock weapons/pistol/pistol.weapon_hud_interface"]
    row = {"pack_name": "ae\\packs\\x\\hud", "source": source, "group": "wphi", "size": 1, "inputs": inputs}
    assert len(make_pack.audit_problems([row], {hud}, set())) == 1
    assert make_pack.audit_problems([row], {hud}, set(), {"ae\\packs\\x\\hud"}) == []
    # a graft from the tag itself reads nothing else
    data["steps"][0]["edits"] = [["graft", "self", "", "crosshairs[0]", "crosshairs[4]"]]
    assert make_pack.provenance(data, "a", hud)[1] == ["cmt " + hud]


def test_a_graft_from_another_layers_tag_brings_that_tags_inputs():
    hud = "cmt/hud.weapon_hud_interface"
    data = synthetic_recipe([
        {"layer": "b", "tag": "stockish.weapon_hud_interface", "op": "edit",
         "from": ["stock", "weapons/ar/ar.weapon_hud_interface"], "edits": []},
        {"layer": "a", "tag": hud, "op": "edit", "from": ["cmt", hud],
         "edits": [["graft", "b", "stockish.weapon_hud_interface", "x", "y"]]}])
    assert "stock weapons/ar/ar.weapon_hud_interface" in make_pack.provenance(data, "a", hud)[1]


def test_a_sound_with_a_stock_input_is_refused_even_when_named():
    row = {"pack_name": "ae\\packs\\x\\fire", "source": "stock sound/fire.sound", "group": "snd!", "size": 1,
           "inputs": []}
    assert len(make_pack.audit_problems([row], set(), set(), {"ae\\packs\\x\\fire"})) == 1
    row = {"pack_name": "ae\\packs\\x\\fire", "source": "cmt/fire.sound", "group": "snd!", "size": 1,
           "inputs": ["cmt cmt/fire.sound", "stock sound/fire.sound"]}
    assert len(make_pack.audit_problems([row], {"cmt/fire.sound"}, set(), {"ae\\packs\\x\\fire"})) == 1


# ---------- departures from the approved donor

def test_fix_departures_must_be_named_and_real():
    rows = [("fix2", "a.bitmap", "byte-equal", 1, 1, ""), ("fix2", "b.bitmap", "differs", 1, 1, "2 fields"),
            ("fix-v3", "c.bitmap", "not made", 0, 1, ""), ("fix-v3", "d.bitmap", "0 field differences", 1, 1, "")]
    named = {"fix2/b.bitmap": "why", "fix-v3/c.bitmap": "why"}
    assert make_pack.departure_problems(rows, named) == []
    assert len(make_pack.departure_problems(rows, {"fix2/b.bitmap": "why"})) == 1            # c is not named
    problems = make_pack.departure_problems(rows, dict(named, **{"fix2/a.bitmap": "stale", "fix2/z.bitmap": "gone"}))
    assert len(problems) == 2 and "it is the prototype's" in problems[0] and "neither made" in problems[1]


def test_pack_departures_must_be_named_and_real():
    by_tag = {"ae_pack\\hud\\br_meter": ["resource 0: 5 of its 9 bytes differ"], "cmt\\x": ["byte 1 of the root"]}
    assert make_pack.pack_departure_problems(by_tag, {"ae_pack\\hud\\br_meter": "why", "cmt\\x": "why"}) == []
    problems = make_pack.pack_departure_problems(by_tag, {"ae_pack\\hud\\br_meter": "why", "cmt\\y": "why"})
    assert problems == ["cmt\\x: byte 1 of the root",
                        "cmt\\y: named as a departure from the approved donor but it is the same"]


# ---------- art pinned by its pixels

def png_file(path, mode, size, pixels, level=6, filter_type=0):
    """a PNG written here: 8 bits a channel, each row with the given filter (0 none, 1 sub, 2 up)"""
    import zlib
    channels = {"L": 1, "RGB": 3, "RGBA": 4}[mode]
    width, height = size
    stride = width * channels
    raw = bytearray()
    previous = bytes(stride)
    for row in range(height):
        line = pixels[row * stride:(row + 1) * stride]
        raw.append(filter_type)
        if filter_type == 1:
            raw += bytes((line[index] - (line[index - channels] if index >= channels else 0)) & 255
                         for index in range(stride))
        elif filter_type == 2:
            raw += bytes((line[index] - previous[index]) & 255 for index in range(stride))
        else:
            raw += line
        previous = line

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    colour = {"L": 0, "RGB": 2, "RGBA": 6}[mode]
    header = chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, colour, 0, 0, 0))
    path.write_bytes(png_pixels.SIGNATURE + header + chunk(b"IDAT", zlib.compress(bytes(raw), level)) +
                     chunk(b"IEND", b""))


def test_art_pin_takes_the_same_pixels_from_any_file_and_refuses_other_pixels(tmp_path):
    pixels = bytes((index * 7) & 255 for index in range(8 * 4 * 4))
    one, two, three = tmp_path / "one.png", tmp_path / "two.png", tmp_path / "three.png"
    png_file(one, "RGBA", (8, 4), pixels, level=9, filter_type=0)
    png_file(two, "RGBA", (8, 4), pixels, level=1, filter_type=1)
    png_file(three, "RGBA", (8, 4), pixels, level=0, filter_type=2)
    assert len({one.read_bytes(), two.read_bytes(), three.read_bytes()}) == 3       # three different files
    pin = png_pixels.pin(one)
    assert pin == {"mode": "RGBA", "size": [8, 4], "pixels_sha256": hashlib.sha256(pixels).hexdigest()}
    assert png_pixels.pin(two) == pin and png_pixels.pin(three) == pin
    assert make_pack.art_pin_problem(two, pin) is None and make_pack.art_pin_problem(three, pin) is None
    changed = bytearray(pixels)
    changed[50] ^= 1                                               # one channel of one pixel
    png_file(two, "RGBA", (8, 4), bytes(changed))
    assert "other pixels" in make_pack.art_pin_problem(two, pin)
    png_file(two, "RGB", (8, 4), pixels[:96])
    assert "RGB 8x4, not RGBA 8x4" in make_pack.art_pin_problem(two, pin)
    png_file(two, "RGBA", (4, 8), pixels)                          # the same bytes as another shape
    assert "4x8" in make_pack.art_pin_problem(two, pin)
    two.write_bytes(b"not a picture")
    assert "cannot be read" in make_pack.art_pin_problem(two, pin)
    assert "cannot be read" in make_pack.art_pin_problem(tmp_path / "missing.png", pin)


def test_png_reader_agrees_with_pillow_on_every_row_filter(tmp_path):
    image_module = pytest.importorskip("PIL.Image")
    pixels = bytes((index * 37 + index // 5) & 255 for index in range(33 * 9 * 4))
    for mode, channels in (("RGBA", 4), ("RGB", 3), ("L", 1)):
        image = image_module.frombytes(mode, (33, 9), pixels[:33 * 9 * channels])
        image.save(tmp_path / "pillow.png")                        # Pillow chooses row filters of all kinds
        assert png_pixels.read((tmp_path / "pillow.png").read_bytes()) == (mode, 33, 9, pixels[:33 * 9 * channels])
    with pytest.raises(png_pixels.Refused):
        png_pixels.read(b"\x89PNG\r\n\x1a\n" + bytes(20))


# ---------- the recipe's own checks, and the work folder

def test_check_recipe_takes_a_recipe_made_against_no_prototype():
    """the SMG packs' recipes have no prototype: no reference folders, departures or approved donor"""
    data = recipe()
    for key in make_pack.PROTOTYPE_KEYS:
        data.pop(key)
    make_pack.check_recipe(data)
    half = recipe()
    half.pop("departures")
    with pytest.raises(make_pack.Refused) as refusal:
        make_pack.check_recipe(half)
    assert "names all of" in str(refusal.value)


def test_every_art_file_names_a_drawing_of_its_script():
    data = recipe()
    assert make_pack.art_drawing_problems(data) == []
    assert set(make_pack.DRAWINGS.values()) <= make_pack.scripts()
    for change, reason in (
            (lambda data: data["art_drawings"].pop("br_icon.png"), "no drawing"),
            (lambda data: data["art_drawings"]["br_icon.png"].update(draw="paint"), "no drawing"),
            (lambda data: data["art_drawings"]["br_icon.png"].update(draw="reticle"), "not the recipe's script"),
            (lambda data: data["art_drawings"]["br_icon.png"].update(input="other.txt"), "no art input"),
            (lambda data: data["art_drawings"].update({"extra.png": {"draw": "icon"}}), "of no art file")):
        changed = recipe()
        change(changed)
        assert any(reason in problem for problem in make_pack.art_drawing_problems(changed)), reason
        with pytest.raises(make_pack.Refused):
            make_pack.check_recipe(changed)


def test_the_art_stage_draws_each_file_as_its_drawing_says(tmp_path):
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import argparse
    import reticle_art
    data = recipe()
    options = argparse.Namespace(work=str(tmp_path), recipe=str(RECIPE), invader="invader", library_path=None,
                                 ffmpeg="ffmpeg", builder=None)
    run = make_pack.Run(options, data)
    smg = numpy.asarray(run.draw("smg_reticle.png", {"draw": "reticle", "form": "smg"}))
    assert (smg[..., 3] == reticle_art.smg_reticle()).all() and (smg[..., 0] == smg[..., 3]).all()
    layout = {"columns": 20, "rows": 3, "group": 5, "height": 6.2, "width": 2.3, "pitch": 4.5, "group_gap": 1.8}
    meter = numpy.asarray(run.draw("m.png", {"draw": "counter_meter", "layout": layout}))
    assert int(meter[..., 0][meter[..., 3] == 255].max()) == 240                     # 60 rounds
    with pytest.raises(make_pack.Refused):
        run.draw("x.png", {"draw": "reticle", "form": "star"})
    with pytest.raises(make_pack.Refused):
        run.draw("x.png", {"draw": "counter_meter", "layout": {"rows": 9}})
    with pytest.raises(make_pack.Refused):
        run.draw("x.png", {"draw": "paint"})


def test_check_recipe_refusals():
    good = recipe()
    make_pack.check_recipe(good)

    def refused(change):
        data = json.loads(json.dumps(good))
        change(data)
        with pytest.raises(make_pack.Refused) as refusal:
            make_pack.check_recipe(data)
        return str(refusal.value)

    assert "no donor" in refused(lambda data: data.pop("donor"))
    assert "no art_settings" in refused(lambda data: data.pop("art_settings"))
    assert "made twice" in refused(lambda data: data["steps"].append(dict(data["steps"][5])))
    assert "before it is made" in refused(lambda data: data["steps"].insert(0, data["steps"].pop()))
    assert "allowlist" in refused(lambda data: data["source"]["allowlist"].pop())
    assert "allowlist" in refused(lambda data: data["source"]["allowlist"].append("cmt/extra/unused.bitmap"))
    assert "no such step" in refused(lambda data: data["steps"][3].update(op="copy_spike_tag"))
    assert "in no layer" in refused(lambda data: data["steps"][3].update(layer="fix-other"))
    assert "art the recipe does not list" in refused(lambda data: data["art"].pop("br_icon.png"))
    assert "no script here" in refused(lambda data: data["art_scripts"].update({"digits.png": "elsewhere.py"}))
    assert "departure in no layer" in refused(lambda data: data["departures"].update({"fix-x/a.bitmap": "why"}))
    assert "other than its pixels" in refused(lambda data: data["art"].update({"digits.png": {"sha256": "0" * 64}}))
    assert "no art_drawings" in refused(lambda data: data.pop("art_drawings"))
    assert "drawings" in refused(lambda data: data["art_drawings"]["digits.png"].update(draw="screen_compass_x"))


def test_work_folder_must_be_one_make_pack_made(tmp_path):
    new = tmp_path / "new"
    make_pack.claim_work_folder(new, "all")
    assert (new / make_pack.MARKER).is_file()
    (new / "layers").mkdir()
    make_pack.claim_work_folder(new, "build")                      # ours: taken again
    other = tmp_path / "other"
    (other / "layers").mkdir(parents=True)
    (other / "layers" / "another's.bitmap").write_bytes(b"x")
    with pytest.raises(make_pack.Refused):
        make_pack.claim_work_folder(other, "build")
    assert not (other / make_pack.MARKER).exists()
    # the art stage on another machine: a folder that holds only what was brought to it
    hop = tmp_path / "hop"
    (hop / "art-in").mkdir(parents=True)
    with pytest.raises(make_pack.Refused):
        make_pack.claim_work_folder(hop, "build")
    make_pack.claim_work_folder(hop, "art")
    assert (hop / make_pack.MARKER).is_file()


# ---------- on the build box: a finished run

def finished_run():
    return (WORK / "fixes.tsv").is_file() and (WORK / "pack" / "ae_battle_rifle.aepack").is_file()


box = pytest.mark.skipif(not finished_run(), reason="no finished make_pack.py run in " + str(WORK))


def rows_of(path):
    lines = path.read_text().splitlines()
    names = lines[0].split("\t")
    return [dict(zip(names, line.split("\t"))) for line in lines[1:]]


@box
def test_run_every_fix_tag_is_the_prototypes_but_the_named_departures():
    rows = rows_of(WORK / "fixes.tsv")
    assert len(rows) == 96                                          # the prototype's 96 fix tags, made or not
    data = recipe()
    plain = [(row["layer"], row["tag"], row["result"]) for row in rows]
    assert make_pack.departure_problems(plain, data["departures"]) == []
    departed = sorted("%s/%s" % (row["layer"], row["tag"]) for row in rows
                      if row["result"] not in ("byte-equal", "0 field differences"))
    assert departed == sorted(data["departures"]) and len(departed) == 6
    assert sum(1 for row in rows if row["result"] == "byte-equal") == 90


@box
def test_run_donor_is_the_recipes_and_not_the_approved_one():
    donor = WORK / "donor" / "donor.map"
    digest = hashlib.sha256(donor.read_bytes()).hexdigest()
    assert digest == recipe()["expected"]["donor_sha256"] != V6E_SHA256
    assert donor.stat().st_size == recipe()["expected"]["donor_size"]


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
    rows = [{"pack_name": row["pack_name"], "source": row["source"], "group": row["group"], "size": row["size"],
             "inputs": row["inputs"].split("; ") if row["inputs"] else []} for row in audit]
    stock_derived = set(data["pack"]["stock_derived"])
    assert make_pack.audit_problems(rows, set(data["source"]["allowlist"]), make_pack.scripts(), stock_derived) == []
    # the own tags made from a stock tag are exactly the ones the recipe names, each with its reason
    assert {row["pack_name"] for row in rows if any(text.startswith(make_pack.STOCK_PREFIX)
                                                    for text in [row["source"]] + row["inputs"])} == stock_derived
    assert len(make_pack.audit_problems(rows, set(data["source"]["allowlist"]), make_pack.scripts())) == len(
        stock_derived)
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
def test_run_pack_is_its_donors_bytes_tags_resources_and_geometry():
    pack = (WORK / "pack" / "ae_battle_rifle.aepack").read_bytes()
    parsed = aepack.read(pack)
    report = pack_compare.compare_pack(pack, donor_map.MapFile(WORK / "donor" / "donor.map"))
    assert report["problems"] == [] and report["own"] == recipe()["expected"]["own_tags"]
    # every byte of every own tag is either compared or of a counted class
    assert report["compared_bytes"] + report["masked_bytes"] == sum(
        tag[10] for tag in parsed.tags if tag[4] == aepack.OWN)
    assert report["resources"] == len(parsed.resources) and report["parts"] == len(parsed.parts)
    assert report["resource_bytes"] == sum(resource[2] for resource in parsed.resources) > 16000000
    assert report["geometry_bytes"] == sum(part[6] + part[8] for part in parsed.parts)   # vertices and indices
    assert report["masked"]["file_offsets"] == 0 and report["masked"]["padding"] == 0


@box
def test_run_pack_differs_from_the_approved_donor_only_where_the_recipe_says():
    if not V6E_DONOR.is_file():
        pytest.skip("the approved donor is not here")
    assert hashlib.sha256(V6E_DONOR.read_bytes()).hexdigest() == V6E_SHA256
    pack = (WORK / "pack" / "ae_battle_rifle.aepack").read_bytes()
    report = pack_compare.compare_pack(pack, donor_map.MapFile(V6E_DONOR), same_file=False)
    named = recipe()["approved_reference"]["pack_departures"]
    assert sorted(report["by_tag"]) == sorted(named)
    assert make_pack.pack_departure_problems(report["by_tag"], named) == []
    # the silencer shader: one reference and nothing else; the HUD: one byte; the sheet: its pixels and records
    silencer = report["by_tag"]["cmt\\weapons\\_shared\\smg\\shaders\\smg_attachments_silencer"]
    assert len(silencer) == 1 and "cubemap mirror surface" in silencer[0] and "cube map glass" in silencer[0]
    assert report["by_tag"]["ae_pack\\hud\\br_ammo"] == ["byte 106 of the bytes at 560: pack 0x00, map 0x01"]
    assert any(text.startswith("resource 0:") for text in report["by_tag"]["ae_pack\\hud\\br_meter"])
    externals = {row["name"]: row for row in rows_of(WORK / "externals.tsv")}
    cube = externals["characters\\cyborg\\bitmaps\\cubemap mirror surface"]
    assert (cube["group"], cube["required"], cube["same_as_stock"], cube["maps"]) == ("bitm", "1", "yes", "13")
    audit = {row["donor_name"]: row for row in rows_of(WORK / "audit.tsv")}
    assert "levels\\a10\\bitmaps\\cube map glass" not in audit
    assert audit["ae_pack\\hud\\br_meter"]["source"] == "AE script hud_sheet.py"
    assert "stock" not in audit["ae_pack\\hud\\br_meter"]["inputs"]
