"""Weapon pack format v1 (docs/weapon-packs.md): the engine's reader, port/linux/game/ae_pack_format.c, through its
check tool (tools/ae_pack_check.c) on packs made here by tools/weapon_pack/aepack.py: no game files.

Two builds of the check tool are run: the one `ninja linux` builds (build/linux/ae_pack_check, the game's 32-bit
objects; skipped where it is not built) and one built here with the build machine's compiler under the address and
undefined behaviour sanitizers (skipped where the machine lacks them or zlib's headers). Each must accept the valid
pack, printing exactly what aepack.py's reader prints, refuse every hostile one, and agree with aepack.py on 2,000
byte-flipped packs without crashing or hanging. The SHA-256 is checked against FIPS 180-4's vectors, hashlib and the
port's own p2p_sha256 (port/linux/src/p2p_crypto.c)."""
import hashlib
import os
import random
import shlex
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "weapon_pack"))

import aepack  # noqa: E402

NINJA_CHECKER = ROOT / "build" / "linux" / "ae_pack_check"
SOURCES = [ROOT / "tools" / "ae_pack_check.c", ROOT / "port" / "linux" / "game" / "ae_pack_format.c"]
# a build failing only for what this machine lacks: skipped
MISSING = ("zlib.h: No such file", "cannot find -lz", "cannot find -lasan", "cannot find -lubsan", "libasan",
           "libubsan", "libclang_rt.asan", "libclang_rt.ubsan")
# (the sanitizers' own exit codes, apart from the tool's 0 and 1)
SANITIZER_ENVIRONMENT = {"ASAN_OPTIONS": "exitcode=99:detect_leaks=1",
                         "UBSAN_OPTIONS": "exitcode=98:print_stacktrace=1:halt_on_error=1"}
FUZZ_ITERATIONS = 2000

WEAPON = aepack.group("weap")
BITMAP = aepack.group("bitm")
MODEL = aepack.group("mod2")
SOUND = aepack.group("snd!")
ITEMS = aepack.group("itmc")


# ---------- the packs

def tag_bytes(size, seed):
    return bytes((index * 7 + seed) & 0xFF for index in range(size))


def sample_pack(**changes):
    """a small pack with every kind of record: tags of each fixup kind and each flag, own and external (required
    and not), a model part, two resources (one stored, one zlib), an item collection and a weapon entry"""
    strip = struct.pack("<4H", 0, 1, 2, 3)
    tags = [
        aepack.Tag(WEAPON, "ae\\packs\\test\\weapons\\rifle\\rifle", source_path="donor\\weapons\\rifle\\rifle",
                   bytes=tag_bytes(256, 1),
                   fixups=[aepack.Fixup(0x10, aepack.FIXUP_POINTER, 0x80),
                           aepack.Fixup(0x20, aepack.FIXUP_REFERENCE, 1),
                           aepack.Fixup(0x40, aepack.FIXUP_REFERENCE, aepack.NONE),
                           aepack.Fixup(0x60, aepack.FIXUP_TAG_INDEX, 2)]),
        aepack.Tag(MODEL, "ae\\packs\\test\\weapons\\rifle\\fp\\fp", source_path="donor\\fp",
                   bytes=tag_bytes(0x100, 2),
                   parts=[aepack.Part(0x20, 5, aepack.STRIP, 4, 2, tag_bytes(4 * 32, 3), strip)]),
        aepack.Tag(BITMAP, "ae\\packs\\test\\hud\\meter", flags=aepack.FLAG_SMOOTH, bytes=tag_bytes(0x60, 4),
                   resources=[aepack.Resource(0x1000, tag_bytes(300, 5))]),
        aepack.Tag(BITMAP, "ae\\packs\\test\\hud\\counter", flags=aepack.FLAG_SMOOTH | aepack.FLAG_METER_COVERAGE,
                   bytes=tag_bytes(0x60, 6),
                   resources=[aepack.Resource(0x2000, tag_bytes(5000, 7), aepack.ZLIB)]),
        aepack.Tag(SOUND, "sound\\sfx\\weapons\\assault rifle\\fire", kind=aepack.EXTERNAL, source_path=""),
        aepack.Tag(BITMAP, "ui\\hud\\bitmaps\\combined\\hud_msg_icons", kind=aepack.EXTERNAL, required=1),
        aepack.Tag(ITEMS, "ae\\packs\\test\\items\\rifle", bytes=tag_bytes(32, 8),
                   fixups=[aepack.Fixup(0, aepack.FIXUP_REFERENCE, 0)]),
    ]
    weapons = [aepack.Weapon("battle_rifle", 0, "h_br", "BATTLE RIFLE", item_collection_tag=6, behaviour=1,
                             fallback_labels=("ar", None),
                             weapon_pickup_text="Picked up a battle rifle",
                             ammo_pickup_text="Picked up %d rounds for battle rifle",
                             icon_bitmap_tag=5, icon_sequence=0, replaces=2, power=0)]
    pack = aepack.Pack("ae_test_rifle", 1, "TEST RIFLE", tags, weapons, credits="Format test pack.\nNo art.\n")
    for name, value in changes.items():
        setattr(pack, name, value)
    return pack


def stored_variant(pack):
    """the pack with its zlib resources stored (what ae_pack_write writes, R2)"""
    for tag in pack.tags:
        for resource in tag.resources:
            resource.compression = aepack.STORED
    return pack


def patched(image, offset, fmt, value, rehash=True):
    data = bytearray(image)
    struct.pack_into(fmt, data, offset, value)
    if rehash:
        aepack.rehash(data)
    return bytes(data)


def record_offset(image, kind, index, record):
    return aepack.section_offset(image, kind) + index * record.size


# ---------- the check tools

def _compiler():
    for name in ("gcc", "clang"):
        if shutil.which(name):
            return shutil.which(name)
    return None


@pytest.fixture(scope="session")
def sanitized_checker(tmp_path_factory):
    """ae_pack_check built here with the sanitizers and the system's zlib (None: this machine can't)"""
    cc = _compiler()
    if not cc:
        return None
    binary = tmp_path_factory.mktemp("ae_pack_check") / "ae_pack_check"
    build = subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", "-g", "-DAE_PACK_SYSTEM_ZLIB",
                            "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
                            f"-I{ROOT / 'port' / 'linux' / 'game'}", *map(str, SOURCES), "-lz", "-o", str(binary)],
                           capture_output=True, text=True)
    if build.returncode != 0 and any(missing in build.stderr for missing in MISSING):
        return None
    assert build.returncode == 0, build.stdout + build.stderr
    return binary


@pytest.fixture(params=["ninja", "sanitized"])
def checker(request):
    if request.param == "ninja":
        if not NINJA_CHECKER.is_file():
            pytest.skip("build/linux/ae_pack_check is not built (ninja linux)")
        return NINJA_CHECKER
    sanitized_checker = request.getfixturevalue("sanitized_checker")
    if sanitized_checker is None:
        pytest.skip("this machine lacks a compiler, the sanitizers or zlib's headers")
    return sanitized_checker


def run(binary, *arguments, timeout=120):
    result = subprocess.run([str(binary), *map(str, arguments)], capture_output=True, text=True, timeout=timeout,
                            env=dict(os.environ, **SANITIZER_ENVIRONMENT))
    assert result.returncode in (0, 1), f"exit {result.returncode}\n{result.stdout[-3000:]}\n{result.stderr[-6000:]}"
    return result


def check_files(binary, paths):
    """{path: (accepted, lines)} as the tool printed them"""
    result = run(binary, *paths, timeout=300)
    verdicts = {}
    lines = []
    for line in result.stdout.splitlines():
        lines.append(line)
        for path in paths:
            if line == f"{path}: ok" or line.startswith(f"{path}: refused: "):
                verdicts[str(path)] = (line == f"{path}: ok", lines)
                lines = []
    assert len(verdicts) == len(paths), result.stdout[-3000:] + result.stderr[-3000:]
    return verdicts


def check_one(binary, tmp_path, image, name="pack.aepack"):
    path = tmp_path / name
    path.write_bytes(image)
    return check_files(binary, [path])[str(path)]


# ---------- the valid pack

def test_python_reader_takes_the_sample():
    image = aepack.build(sample_pack())
    parsed = aepack.read(image)
    assert len(parsed.tags) == 7 and parsed.own_tag_count == 5 and len(parsed.weapons) == 1
    assert image[:8] == b"AEWPACK\0"
    assert aepack.digest(image) == image[aepack.SHA256_OFFSET:aepack.SHA256_OFFSET + 32]


def test_a_pack_of_tags_only():
    """the vehicle damage pack's kind: no weapon entry, said in the header (AE_PACK_FLAG_NO_WEAPONS); a pack with
    weapons may not say it, and one with neither is refused"""
    image = aepack.build(sample_pack(weapons=[]))
    assert struct.unpack_from("<I", image, 16)[0] == aepack.FLAG_NO_WEAPONS
    parsed = aepack.read(image)
    assert parsed.weapons == [] and len(parsed.tags) == 7
    with pytest.raises(aepack.Refused, match="no weapons"):
        aepack.read(_header_field(16, "<I", 0)(image))
    with pytest.raises(aepack.Refused, match="flags"):
        aepack.read(_header_field(16, "<I", 2)(image))


def test_a_pack_of_tags_only_is_taken_by_the_checker(checker, tmp_path):
    path = tmp_path / "tags_only.aepack"
    path.write_bytes(aepack.build(sample_pack(weapons=[])))
    result = run(checker, path)
    assert result.returncode == 0, result.stdout
    assert "flags 1 " in result.stdout


def test_valid_pack_is_accepted_and_printed(checker, tmp_path):
    image = aepack.build(sample_pack())
    path = tmp_path / "valid.aepack"
    path.write_bytes(image)
    result = run(checker, path)
    assert result.returncode == 0, result.stdout
    accepted, expected = aepack.check(image, str(path))
    assert accepted
    assert result.stdout.splitlines() == expected
    text = result.stdout
    assert "id ae_test_rifle revision 1 name \"TEST RIFLE\"" in text
    assert "flags smooth bytes" in text and "flags smooth,coverage bytes" in text
    assert " external required 1 flags - " in text
    assert "weapon 0 key battle_rifle root 0 items 6 behaviour 1 label h_br fallbacks ar,- menu \"BATTLE RIFLE\"" in text
    assert "pickup \"Picked up a battle rifle\" ammo \"Picked up %d rounds for battle rifle\" icon 5:0" in text
    assert "icon 5:0 replaces 2 power 0" in text
    assert "resources 2 of 5300 bytes" in text
    assert f"sha256 {hashlib.sha256(image[:0x5C] + bytes(32) + image[0x7C:]).hexdigest()}" in text


@pytest.mark.parametrize("weapon_text, ammo_text", [
    ("Fusil de combat ramassé", "Ramassé %d balles pour le fusil"),
    ("ライフルを拾った", "弾%d発"),
    ("Picked up a battle rifle", "%dé"),
    (None, None),
])
def test_pickup_texts_accepted(checker, tmp_path, weapon_text, ammo_text):
    """pickup texts with multibyte UTF-8 right before or after the "%d", and none (the engine's own)"""
    pack = sample_pack()
    pack.weapons[0].weapon_pickup_text = weapon_text
    pack.weapons[0].ammo_pickup_text = ammo_text
    image = aepack.build(pack)
    accepted, lines = check_one(checker, tmp_path, image)
    assert accepted, lines[-1]
    assert lines == aepack.check(image, str(tmp_path / "pack.aepack"))[1]


def test_compressed_sections_are_read(checker, tmp_path):
    """format v1 readers take zlib sections (the v1 writer writes none, R2)"""
    image = aepack.build(sample_pack(compressed_sections=(aepack.STRINGS, aepack.TAGS, aepack.TAG_BYTES,
                                                          aepack.FIXUPS, aepack.PARTS, aepack.GEOMETRY,
                                                          aepack.RESOURCES, aepack.WEAPONS, aepack.CREDITS)))
    accepted, lines = check_one(checker, tmp_path, image)
    assert accepted, lines
    assert lines == aepack.check(image, str(tmp_path / "pack.aepack"))[1]


# ---------- hostile packs

def _flip_payload(image):
    data = bytearray(image)
    data[aepack.section_offset(image, aepack.TAG_BYTES) + 5] ^= 0x40
    return bytes(data)


def _overlap(image):
    entry = aepack.section_entry_offset(image, aepack.GEOMETRY)
    return patched(image, entry + 4, "<I", aepack.section_offset(image, aepack.TAGS))


def _section_lie(image):
    """CREDITS compressed, its inflated size one more than the stream gives"""
    image = aepack.build(sample_pack(compressed_sections=(aepack.CREDITS,)))
    entry = aepack.section_entry_offset(image, aepack.CREDITS)
    inflated = struct.unpack_from("<I", image, entry + 12)[0]
    return patched(image, entry + 12, "<I", inflated + 1)


def _resource_lie(image):
    """the zlib resource's record says one byte more than its stream gives"""
    offset = record_offset(image, aepack.RESOURCES, 1, aepack.RESOURCE_RECORD)
    return patched(image, offset + 8, "<I", 5001)


def _stored_resource_lie(image):
    """the zlib resource's record says one byte less than its stream gives"""
    offset = record_offset(image, aepack.RESOURCES, 1, aepack.RESOURCE_RECORD)
    return patched(image, offset + 8, "<I", 4999)


def _tag_field(index, field_offset, fmt, value):
    def change(image):
        return patched(image, record_offset(image, aepack.TAGS, index, aepack.TAG_RECORD) + field_offset, fmt, value)
    return change


def _fixup_field(index, field_offset, fmt, value):
    def change(image):
        return patched(image, record_offset(image, aepack.FIXUPS, index, aepack.FIXUP_RECORD) + field_offset, fmt,
                       value)
    return change


def _weapon_field(field_index, value):
    def change(image):
        return patched(image, record_offset(image, aepack.WEAPONS, 0, aepack.WEAPON_RECORD) + field_index * 4, "<I",
                       value)
    return change


def _header_field(offset, fmt, value, rehash=True):
    return lambda image: patched(image, offset, fmt, value, rehash)


def _cut(size, rehash):
    def change(image):
        data = bytearray(image[:size] if size >= 0 else image[:len(image) + size])
        if rehash and len(data) >= 0x7C:
            aepack.rehash(data)
        return bytes(data)
    return change


def _resource_past_data(image):
    offset = record_offset(image, aepack.RESOURCES, 0, aepack.RESOURCE_RECORD)
    data_size = next(entry[2] for entry in aepack.section_entries(image) if entry[0] == aepack.RESOURCE_DATA)
    return patched(image, offset + 12, "<I", data_size - 100)


def _part_index_past_vertices(image):
    data = bytearray(image)
    record = record_offset(image, aepack.PARTS, 0, aepack.PART_RECORD)
    indices_offset = struct.unpack_from("<I", image, record + 24)[0]
    struct.pack_into("<H", data, aepack.section_offset(image, aepack.GEOMETRY) + indices_offset + 2, 4)
    aepack.rehash(data)
    return bytes(data)


def _misaligned_section(image):
    entry = aepack.section_entry_offset(image, aepack.CREDITS)
    offset = struct.unpack_from("<I", image, entry + 4)[0]
    return patched(image, entry + 4, "<I", offset + 4)


def _duplicate_tag(image):
    pack = sample_pack()
    pack.tags[6].name = pack.tags[2].name
    pack.tags[6].group = BITMAP
    return aepack.build(pack)


def _swapped_fixups(image):
    """tag 0's first two fixups in the wrong order"""
    data = bytearray(image)
    first = record_offset(image, aepack.FIXUPS, 0, aepack.FIXUP_RECORD)
    second = first + aepack.FIXUP_RECORD.size
    data[first:second], data[second:second + aepack.FIXUP_RECORD.size] = (
        data[second:second + aepack.FIXUP_RECORD.size], data[first:second])
    aepack.rehash(data)
    return bytes(data)


def _overlapping_parts(image):
    """a second part 0x67 bytes after the first"""
    pack = sample_pack()
    strip = struct.pack("<4H", 0, 1, 2, 3)
    pack.tags[1].bytes = bytes(0x200)
    pack.tags[1].parts = [aepack.Part(0, 5, aepack.STRIP, 4, 2, bytes(128), strip),
                          aepack.Part(0x67, 5, aepack.STRIP, 4, 2, bytes(128), strip)]
    return aepack.build(pack)


def _part_vertex_type(vertex_type):
    def change(image):
        pack = sample_pack()
        part = pack.tags[1].parts[0]
        part.vertex_type = vertex_type
        part.vertices = bytes(4 * aepack.VERTEX_SIZES[vertex_type])
        return aepack.build(pack)
    return change


def _external_with_bytes(image):
    pack = sample_pack()
    pack.tags[4].bytes = b"\1\2\3\4"
    return aepack.build(pack)


def _texts(weapon_text, ammo_text):
    def change(image):
        pack = sample_pack()
        if weapon_text is not None:
            pack.weapons[0].weapon_pickup_text = weapon_text
        if ammo_text is not None:
            pack.weapons[0].ammo_pickup_text = ammo_text
        return aepack.build(pack)
    return change


REFUSALS = {
    "truncated header": (_cut(0x80, False), "truncated"),
    "truncated table": (_cut(0x110, True), "truncated"),
    "truncated half": (_cut(-2000, True), "past the end"),
    "truncated last byte": (_cut(-1, True), "past the end"),
    "truncated last byte, old hash": (_cut(-1, False), "past the end"),
    "bad magic": (_header_field(0, "<8s", b"AEWPACX\0"), "not a weapon pack"),
    "bad version": (_header_field(8, "<I", 2), "format version 2"),
    "bad header size": (_header_field(12, "<I", 0x200), "header size"),
    "header flags": (_header_field(16, "<I", 1), "flags"),
    "bad pack id": (_header_field(0x14, "<4s", b"AE_T"), "pack id"),
    "revision 0": (_header_field(0x34, "<I", 0), "revision"),
    "tag layout 2": (_header_field(0x58, "<I", 2), "tag layout"),
    "no sections": (_header_field(0x7C, "<I", 0), "section count"),
    "13 sections": (_header_field(0x7C, "<I", 13), "section count"),
    "reserved byte": (_header_field(0xF0, "<B", 1), "reserved"),
    "hash mismatch": (_flip_payload, "SHA-256"),
    "hash field changed": (_header_field(0x60, "<B", 0x55, rehash=False), "SHA-256"),
    "overlapping sections": (lambda image: _overlap(image), "overlap"),
    "misaligned section": (_misaligned_section, "section"),
    "fixup offset past its tag": (_fixup_field(0, 0, "<I", 253), "past its tag"),
    "pointer target past its tag": (_fixup_field(0, 8, "<I", 256), "past its tag"),
    "reference past the tag count": (_fixup_field(1, 8, "<I", 7), "past the tags"),
    "tag index past the tag count": (_fixup_field(3, 8, "<I", 7), "past the tags"),
    "unknown fixup kind": (_fixup_field(0, 4, "<H", 4), "fixup"),
    "fixup pad": (_fixup_field(0, 6, "<H", 1), "fixup"),
    "resource past RESOURCE_DATA": (_resource_past_data, "past RESOURCE_DATA"),
    "resource inflated size lie (more)": (_resource_lie, "inflated size"),
    "resource inflated size lie (less)": (_stored_resource_lie, "inflated size"),
    "section inflated size lie": (_section_lie, "inflated size"),
    "unknown flag bit": (_tag_field(2, 18, "<B", aepack.FLAG_SMOOTH | 4), "unknown flag"),
    "flag on a model": (_tag_field(1, 18, "<B", aepack.FLAG_SMOOTH), "flags on"),
    "flag on an external bitmap": (_tag_field(5, 18, "<B", aepack.FLAG_SMOOTH), "flags on"),
    "tag pad": (_tag_field(0, 19, "<B", 1), "kind"),
    "tag kind 2": (_tag_field(0, 16, "<B", 2), "kind"),
    "own tag required": (_tag_field(0, 17, "<B", 1), "required"),
    "tag bytes past TAG_BYTES": (_tag_field(6, 24, "<I", 0x10000), "past TAG_BYTES"),
    "tag name past STRINGS": (_tag_field(0, 12, "<I", 0x100000), "past STRINGS"),
    "fixups past their section": (_tag_field(6, 36, "<I", 2), "records"),
    "external with bytes": (_tag_field(4, 28, "<I", 4), "external"),
    "duplicate tag": (_duplicate_tag, "second tag"),
    "part index past its vertices": (_part_index_past_vertices, "index"),
    "weapon root not a weapon": (_weapon_field(1, 1), "root"),
    "weapon replaces random": (_weapon_field(12, 1), "replaces"),
    "weapon icon not a bitmap": (_weapon_field(10, 4), "icon"),
    "weapon item collection not one": (_weapon_field(2, 0), "item collection"),
    "own tags' bytes overlap": (_tag_field(1, 24, "<I", 0), "overlap"),
    "fixups overlap": (_fixup_field(1, 0, "<I", 0x12), "overlap"),
    "fixups out of order": (_swapped_fixups, "out of order"),
    "parts overlap": (_overlapping_parts, "overlap"),
    "part not of a model's vertex type": (_part_vertex_type(0), "part type"),
    "external tag with bytes (written)": (_external_with_bytes, "external"),
    "weapon pickup text with %": (_texts("Picked up 100% of a rifle", None), "weapon pickup"),
    "weapon pickup text with %%": (_texts("Picked up 100%% of a rifle", None), "weapon pickup"),
    "weapon pickup text with %d": (_texts("Picked up %d rifles", None), "weapon pickup"),
    "ammo pickup text with %s": (_texts(None, "Picked up %s rounds"), "ammo pickup"),
    "ammo pickup text with %n": (_texts(None, "Picked up %d rounds%n"), "ammo pickup"),
    "ammo pickup text with %%d": (_texts(None, "Picked up %%d rounds"), "ammo pickup"),
    "ammo pickup text with two %d": (_texts(None, "Picked up %d of %d rounds"), "ammo pickup"),
    "ammo pickup text with a width": (_texts(None, "Picked up %999d rounds"), "ammo pickup"),
    "ammo pickup text with a precision": (_texts(None, "Picked up %.5d rounds"), "ammo pickup"),
    "ammo pickup text with a lone %": (_texts(None, "Picked up %d rounds %"), "ammo pickup"),
    "ammo pickup text without %d": (_texts(None, "Picked up rounds"), "ammo pickup"),
    "ammo pickup text with %ld": (_texts(None, "Picked up %ld rounds"), "ammo pickup"),
    "ammo pickup text with %d%": (_texts(None, "Picked up %d%"), "ammo pickup"),
    "ammo pickup text with %5d": (_texts(None, "Picked up %5d rounds"), "ammo pickup"),
    "ammo pickup text with multibyte and %%d": (_texts(None, "Ramassé %%d balles"), "ammo pickup"),
}


@pytest.mark.parametrize("case", sorted(REFUSALS))
def test_hostile_pack_is_refused(checker, tmp_path, case):
    change, reason = REFUSALS[case]
    image = change(aepack.build(sample_pack()))
    accepted, lines = check_one(checker, tmp_path, image)
    assert not accepted, lines
    assert reason.lower() in lines[-1].lower(), lines[-1]
    python_accepted, python_lines = aepack.check(image, "x")
    assert not python_accepted, python_lines


# ---------- each limit, and one past it

def _many_tags(count, kind):
    pack = sample_pack()
    for index in range(count - len(pack.tags)):
        if kind == aepack.OWN:
            pack.tags.append(aepack.Tag(BITMAP, f"ae\\packs\\test\\filler\\{index}", bytes=b"\1"))
        else:
            pack.tags.append(aepack.Tag(BITMAP, f"filler\\{index}", kind=aepack.EXTERNAL))
    return pack


def _own_tags(count):
    """count own tags in all (the sample has 5) and 1024 tags in all"""
    pack = _many_tags(count + 2, aepack.OWN)
    for index in range(1024 - len(pack.tags)):
        pack.tags.append(aepack.Tag(BITMAP, f"external\\{index}", kind=aepack.EXTERNAL))
    return pack


def _tag_size(size):
    pack = sample_pack()
    pack.tags[6].bytes = bytes(size)
    return pack


def _fixups(count):
    pack = sample_pack()
    # (pointers 4 bytes apart: a tag's fixups may not overlap)
    pack.tags[6].bytes = bytes(4 * (count - 4))
    pack.tags[6].fixups = [aepack.Fixup(4 * index, aepack.FIXUP_POINTER, 0) for index in range(count - 4)]
    return pack


def _strings(size):
    """STRINGS of exactly size bytes: external tags with long names and source paths (each string 1..255 bytes and
    its terminator) added to the sample"""
    pack = sample_pack()
    used = next(entry[3] for entry in aepack.section_entries(aepack.build(pack)) if entry[0] == aepack.STRINGS)
    index = 0
    while used < size:
        remaining = size - used
        take = 512 if remaining >= 516 or remaining == 512 else 457 if remaining > 512 else remaining
        assert take >= 4
        name_length = min(255, take - 3)
        path_length = take - 2 - name_length
        pack.tags.append(aepack.Tag(BITMAP, (f"n{index}" + "z" * 255)[:name_length], kind=aepack.EXTERNAL,
                                    source_path=(f"p{index}" + "q" * 255)[:path_length]))
        used += take
        index += 1
    assert next(entry[3] for entry in aepack.section_entries(aepack.build(pack))
                if entry[0] == aepack.STRINGS) == size
    return pack


def _zlib_resources(sizes):
    pack = sample_pack()
    pack.tags[3].resources = [aepack.Resource(0x10000 + index * 0x1000000, bytes(size), aepack.ZLIB)
                              for index, size in enumerate(sizes)]
    return pack


def _weapons(count):
    pack = sample_pack()
    for index in range(count - 1):
        pack.weapons.append(aepack.Weapon(f"rifle_{index}", 0, "h_br", "RIFLE"))
    return pack


def _parts(count):
    pack = sample_pack()
    strip = struct.pack("<4H", 0, 1, 2, 3)
    # (parts 0x68 bytes apart: they may not overlap)
    pack.tags[1].bytes = bytes(0x68 * count)
    pack.tags[1].parts = [aepack.Part(index * 0x68, 5, aepack.STRIP, 4, 2, bytes(4 * 32), strip)
                          for index in range(count)]
    return pack


def _resource_records(count):
    pack = sample_pack()
    pack.tags[2].resources = [aepack.Resource(index * 4, b"\1\2\3\4") for index in range(count - 1)]
    return pack


def _claimed(kind, size):
    """a section compressed, its inflated size claimed past the limit (refused before inflating)"""
    def change(image):
        image = aepack.build(sample_pack(compressed_sections=(kind,)))
        return patched(image, aepack.section_entry_offset(image, kind) + 12, "<I", size)
    return change


LIMITS = {
    # (name: (the pack at the limit or None, the pack one past it, the reason's words))
    "tags": (lambda: _many_tags(1024, aepack.EXTERNAL), lambda: _many_tags(1025, aepack.EXTERNAL), "tags"),
    "own tags": (lambda: _own_tags(512), lambda: _own_tags(513), "own tags"),
    "tag size": (lambda: _tag_size(4 * aepack.MiB), lambda: _tag_size(4 * aepack.MiB + 1), "size"),
    "fixups": (lambda: _fixups(262144), lambda: _fixups(262145), "FIXUPS"),
    "strings": (lambda: _strings(256 * 1024), lambda: _strings(256 * 1024 + 1), "STRINGS"),
    "resource size": (lambda: _zlib_resources([8 * aepack.MiB]), lambda: _zlib_resources([8 * aepack.MiB + 1]),
                      "resource too large"),
    # (with the sample's 300-byte stored resource)
    "resources size": (lambda: _zlib_resources([8 * aepack.MiB] * 5 + [8 * aepack.MiB - 300]),
                       lambda: _zlib_resources([8 * aepack.MiB] * 5 + [8 * aepack.MiB - 299]), "48 MiB"),
    "weapons": (lambda: _weapons(8), lambda: _weapons(9), "WEAPONS"),
    "parts": (lambda: _parts(4096), lambda: _parts(4097), "PARTS"),
    "resource records": (lambda: _resource_records(4096), lambda: _resource_records(4097), "RESOURCES"),
}
CLAIMS = {
    "tag bytes claimed": (aepack.TAG_BYTES, 16 * aepack.MiB + 1, "TAG_BYTES"),
    "geometry claimed": (aepack.GEOMETRY, 16 * aepack.MiB + 1, "GEOMETRY"),
    "strings claimed": (aepack.STRINGS, 256 * 1024 + 1, "STRINGS"),
    "credits claimed": (aepack.CREDITS, 64 * 1024 + 1, "CREDITS"),
    "fixups claimed": (aepack.FIXUPS, 262145 * 12, "FIXUPS"),
}


@pytest.mark.parametrize("case", sorted(LIMITS))
def test_limit(checker, tmp_path, case):
    at_limit, past_limit, reason = LIMITS[case]
    image = aepack.build(at_limit())
    accepted, lines = check_one(checker, tmp_path, image, "at.aepack")
    assert accepted, lines[-1]
    image = aepack.build(past_limit())
    accepted, lines = check_one(checker, tmp_path, image, "past.aepack")
    assert not accepted
    assert reason.lower() in lines[-1].lower(), lines[-1]
    assert not aepack.check(image, "x")[0]


@pytest.mark.parametrize("case", sorted(CLAIMS))
def test_claimed_size_past_limit(checker, tmp_path, case):
    kind, size, reason = CLAIMS[case]
    image = _claimed(kind, size)(None)
    accepted, lines = check_one(checker, tmp_path, image)
    assert not accepted
    assert reason.lower() in lines[-1].lower(), lines[-1]
    assert not aepack.check(image, "x")[0]


def test_file_size_limit(checker, tmp_path):
    """a file past 64 MiB is refused for its size (a sparse file: nothing written past the pack)"""
    path = tmp_path / "large.aepack"
    path.write_bytes(aepack.build(sample_pack()))
    with open(path, "r+b") as file:
        file.truncate(64 * aepack.MiB + 1)
    accepted, lines = check_files(checker, [path])[str(path)]
    assert not accepted
    assert "64 mib" in lines[-1].lower(), lines[-1]


# ---------- byte flips

def _mutants(bases, count, seed):
    """count packs from the bases (the first three times as often as the others), each with a few bytes changed or
    (one in ten) its length cut or grown"""
    rng = random.Random(seed)
    regions = []
    for base in bases:
        structural = [range(0, aepack.HEADER_SIZE + 10 * aepack.SECTION_ENTRY_SIZE)]
        for kind, offset, stored, _, _ in aepack.section_entries(base):
            if kind not in (aepack.RESOURCE_DATA, aepack.TAG_BYTES, aepack.GEOMETRY) and stored:
                structural.append(range(offset, offset + stored))
        regions.append(structural)
    for _ in range(count):
        which = 0 if rng.random() < 0.75 else rng.randrange(1, len(bases))
        base, structural = bases[which], regions[which]
        interesting = [0, 1, 2, 3, 4, 0x10, 0x68, 0x7F, 0x80, 0xFF, 0x100, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000,
                       0xFFFFFFFE, 0xFFFFFFFF, len(base), len(base) - 16]
        data = bytearray(base)
        if rng.random() < 0.1:
            if rng.random() < 0.5:
                del data[rng.randrange(aepack.HEADER_SIZE // 2, len(data)):]
            else:
                data += bytes(rng.randrange(256) for _ in range(rng.randint(1, 64)))
            if rng.random() < 0.9 and len(data) >= 0x7C:
                aepack.rehash(data)
            yield bytes(data)
            continue
        for _ in range(rng.randint(1, 3)):
            region = rng.choice(structural) if rng.random() < 0.7 else range(len(data))
            position = rng.choice(region)
            choice = rng.random()
            if choice < 0.4:
                data[position] ^= 1 << rng.randrange(8)
            elif choice < 0.6:
                data[position] = rng.randrange(256)
            elif position + 4 <= len(data):
                position -= position % 2
                value = rng.choice(interesting)
                old = struct.unpack_from("<I", data, position)[0]
                value = rng.choice([value, (old + 1) & 0xFFFFFFFF, (old - 1) & 0xFFFFFFFF])
                struct.pack_into("<I", data, position, value)
        if rng.random() < 0.9:
            aepack.rehash(data)
        yield bytes(data)


def test_byte_flip_fuzz(checker, tmp_path):
    """2,000 packs with a few bytes changed (most hashed again, so the checks past the hash are reached): the tool
    never crashes or hangs, refuses what aepack.py refuses and prints what it prints for anything it accepts"""
    bases = [aepack.build(sample_pack()),
             aepack.build(sample_pack(compressed_sections=(aepack.STRINGS, aepack.TAGS, aepack.TAG_BYTES,
                                                           aepack.FIXUPS, aepack.PARTS, aepack.GEOMETRY,
                                                           aepack.RESOURCES, aepack.WEAPONS, aepack.CREDITS)))]
    accepted_count = 0
    batch = []
    images = {}
    for index, image in enumerate(_mutants(bases, FUZZ_ITERATIONS, 20261010)):
        path = tmp_path / f"m{index:04d}.aepack"
        path.write_bytes(image)
        images[str(path)] = image
        batch.append(path)
        if len(batch) == 100 or index == FUZZ_ITERATIONS - 1:
            verdicts = check_files(checker, batch)
            for path in batch:
                accepted, lines = verdicts[str(path)]
                python_accepted, python_lines = aepack.check(images[str(path)], str(path))
                assert accepted == python_accepted, (path, lines[-1], python_lines[-1])
                if accepted:
                    accepted_count += 1
                    assert lines == python_lines, path
                path.unlink()
            batch = []
            images = {}
    # (some flips leave a valid pack: padding, names, texts, unused values)
    assert 0 < accepted_count < FUZZ_ITERATIONS
    print(f"fuzz: {accepted_count} of {FUZZ_ITERATIONS} accepted")


# ---------- the writer

def test_c_writer_writes_the_python_writers_bytes(checker, tmp_path):
    """ae_pack_write (through `ae_pack_check --rewrite`) lays a pack out byte for byte as aepack.build does, its
    zlib resources stored (R2); twice the same"""
    stored = aepack.build(stored_variant(sample_pack()))
    source = tmp_path / "zlib.aepack"
    source.write_bytes(aepack.build(sample_pack()))
    for name in ("one", "two"):
        out = tmp_path / f"{name}.aepack"
        result = run(checker, "--rewrite", source, out)
        assert result.returncode == 0, result.stdout + result.stderr
        assert out.read_bytes() == stored, name
    stored_path = tmp_path / "stored.aepack"
    stored_path.write_bytes(stored)
    out = tmp_path / "three.aepack"
    assert run(checker, "--rewrite", stored_path, out).returncode == 0
    assert out.read_bytes() == stored


def test_c_writer_checks_what_it_writes(checker, tmp_path):
    """ae_pack_write refuses build data a reader would refuse (writing nothing), and its own pack reads back"""
    result = run(checker, "--writer-check", tmp_path / "written.aepack")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "PASS" in result.stdout


# ---------- SHA-256

FIPS_VECTORS = [
    (b"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
    (b"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
    (b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
    (b"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"),
    (b"a" * 1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"),
]


def test_sha256_vectors(checker, tmp_path):
    paths = []
    for index, (data, _) in enumerate(FIPS_VECTORS):
        path = tmp_path / f"v{index}"
        path.write_bytes(data)
        paths.append(path)
    result = run(checker, "--sha256", *paths)
    assert result.returncode == 0
    digests = [line.split()[0] for line in result.stdout.splitlines()]
    assert digests == [expected for _, expected in FIPS_VECTORS]


def test_sha256_random_buffer(checker, tmp_path):
    """5 MB of random bytes, the tool's streaming hash (fed in uneven pieces) against hashlib"""
    data = random.Random(5).randbytes(5 * 1000 * 1000)
    path = tmp_path / "random"
    path.write_bytes(data)
    result = run(checker, "--sha256", path)
    assert result.stdout.split()[0] == hashlib.sha256(data).hexdigest()


def test_sha256_equals_p2p_sha256(tmp_path):
    """ae_pack_sha256 and the port's p2p_sha256 (port/linux/src/p2p_crypto.c) on the same 5 MB buffer, built with
    the flags ninja gives the platform layer (tools/tests/ae_pack_sha256_check.c)"""
    if not shutil.which("clang") or not shutil.which("ninja") or not (ROOT / "build.ninja").is_file():
        pytest.skip("needs clang, ninja and a configured build")
    command = subprocess.run(["ninja", "-t", "commands", "build/linux/obj/port/linux/src/p2p_crypto.o"],
                             capture_output=True, text=True, check=True, cwd=ROOT).stdout.strip().splitlines()[-1]
    words = shlex.split(command)
    while words and not words[0].startswith("-"):
        words = words[1:]
    flags = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word in ("-MF", "-o", "-c"):
            skip = True
        elif word == "-MMD" or word.startswith("-flto") or word.startswith("-fprofile-use"):
            continue
        else:
            flags.append(word)
    program = tmp_path / "ae_pack_sha256_check"
    built = subprocess.run(["clang", *flags, "-O1", "-no-pie", "-pthread", "-Wl,--unresolved-symbols=ignore-all", "-o",
                            str(program), "tools/tests/ae_pack_sha256_check.c", "port/linux/src/p2p_crypto.c",
                            "port/linux/game/ae_pack_format.c", "port/third_party/monocypher/monocypher.c",
                            "port/third_party/monocypher/monocypher-ed25519.c"],
                           capture_output=True, text=True, cwd=ROOT)
    assert built.returncode == 0, built.stderr[-4000:]
    data = random.Random(6).randbytes(5 * 1000 * 1000)
    path = tmp_path / "random"
    path.write_bytes(data)
    result = subprocess.run([str(program), str(path)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    ours, theirs = result.stdout.split()[:2]
    assert ours == theirs == hashlib.sha256(data).hexdigest()


def test_zlib_streams_python_writes_are_inflated_exactly():
    """(aepack.inflate_exact, the reference the C reader is held to)"""
    data = tag_bytes(1000, 9)
    stream = zlib.compress(data, 9)
    assert aepack.inflate_exact(stream, 1000) == data
    assert aepack.inflate_exact(stream, 999) is None
    assert aepack.inflate_exact(stream, 1001) is None
    assert aepack.inflate_exact(stream + b"\0", 1000) is None
    assert aepack.inflate_exact(stream[:-1], 1000) is None
