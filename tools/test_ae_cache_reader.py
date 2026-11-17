"""The weapon cache's reader (port/linux/game/ae_pack_format.c, ae_cache_open; ae_weapon_cache_format.h) through its check
tool (tools/ae_cache_check.c) on caches made here: no game files. The tool is built here with the build machine's
compiler under the address and undefined behaviour sanitizers (skipped where the machine lacks them or zlib's headers)
and, when `ninja linux` built it, build/linux/ae_cache_check too. A valid cache is accepted and described; every hostile
one (each header field, counts, sort order, kinds, a weapon's root, configs, externs) is refused with its reason; 1,000
byte-flipped caches neither crash nor hang. tools/test_weapon_cache_format.py (the Python writer's twin test) adds the
byte-for-byte agreement with tools/weapon_pack/aepack.py's cache functions."""
import hashlib
import random
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "weapon_pack"))

import aepack  # noqa: E402

NINJA_CHECKER = ROOT / "build" / "linux" / "ae_cache_check"
SOURCES = [ROOT / "tools" / "ae_cache_check.c", ROOT / "port" / "linux" / "game" / "ae_pack_format.c"]
MISSING = ("zlib.h: No such file", "cannot find -lz", "cannot find -lasan", "cannot find -lubsan", "libasan",
           "libubsan", "libclang_rt.asan", "libclang_rt.ubsan")
SANITIZER_ENVIRONMENT = {"ASAN_OPTIONS": "exitcode=99:detect_leaks=1",
                         "UBSAN_OPTIONS": "exitcode=98:print_stacktrace=1:halt_on_error=1"}
FUZZ_ITERATIONS = 1000

NONE = 0xFFFFFFFF
HEADER_SIZE = 0x200
SHA_OFFSET = 0x38
CACHE_HEADER = struct.Struct("<8sIIIIQII16s32sII")  # up to part_count
PART = struct.Struct("<16sIIQ32s")
assert CACHE_HEADER.size == 0x60 and PART.size == 0x40
DIRECTORY = struct.Struct("<IBBHIIIIIIIIIIIIIIBBBBB3xIIIIIHH")
CONFIG = struct.Struct("<5I3I4fIf4f3I3I")
EXTERN = struct.Struct("<5I")
assert DIRECTORY.size == 96 and CONFIG.size == 96 and EXTERN.size == 20
STRINGS, TAGS, TAG_BYTES, FIXUPS, PARTS, GEOMETRY, RESOURCES, RESOURCE_DATA, WEAPONS, CREDITS = range(1, 11)
DIRECTORY_KIND, CONFIGS_KIND, EXTERNS_KIND = 11, 12, 13
WEAPON, BITMAP, ITEMS, WPHI, EFFECT = (aepack.group(g) for g in ("weap", "bitm", "itmc", "wphi", "effe"))


def tag_bytes(size, seed):
    return bytes((seed * 31 + i) & 0xFF for i in range(size))


def base_pack():
    tags = [
        aepack.Tag(WEAPON, "ae\\base\\weapons\\rifle", bytes=tag_bytes(0x80, 1)),
        aepack.Tag(BITMAP, "ae\\base\\hud\\meter", bytes=tag_bytes(0x40, 2),
                   resources=[aepack.Resource(0x1000, tag_bytes(200, 3))]),
        aepack.Tag(WPHI, "ae\\base\\hud\\rifle", bytes=tag_bytes(0x40, 4)),
        aepack.Tag(EFFECT, "ae\\base\\fx\\flash", bytes=tag_bytes(0x40, 5)),
        aepack.Tag(ITEMS, "ae\\base\\items\\rifle", bytes=tag_bytes(0x20, 6)),
    ]
    weapons = [aepack.Weapon("x", 0, "h_x", "X")]
    return aepack.Pack("ae_x", 1, "X", tags, weapons)


class Strings:
    def __init__(self, data):
        self.data = bytearray(data)
        self.known = {}

    def add(self, text):
        if text is None:
            return NONE
        if text not in self.known:
            self.known[text] = len(self.data)
            self.data += text.encode("utf-8") + b"\0"
        return self.known[text]


def entry(strings, **fields):
    """a DIRECTORY record's field values by name, defaults for a weapon"""
    values = dict(id=1, kind=1, part=0, flags=0, key="battle_rifle", display="BATTLE RIFLE", first_tag=0, tag_count=5,
                  root_tag=0, item_collection_tag=4, label="h_br", fb0="ar", fb1=None, menu="BATTLE RIFLE",
                  pickup=None, ammo=None, icon_bitmap_tag=NONE, icon_sequence=0, behaviour=1, stock=3,
                  replaces_map=1, legacy=1, catalogue=0, projectile=NONE, equipment=NONE, collection=NONE, hud=NONE,
                  field=NONE, root_first=0, root_count=0)
    values.update(fields)
    return values


def pack_entry(strings, v):
    return DIRECTORY.pack(v["id"], v["kind"], v["part"], v["flags"], strings.add(v["key"]), strings.add(v["display"]),
                          v["first_tag"], v["tag_count"], v["root_tag"], v["item_collection_tag"],
                          strings.add(v["label"]), strings.add(v["fb0"]), strings.add(v["fb1"]),
                          strings.add(v["menu"]), strings.add(v["pickup"]), strings.add(v["ammo"]),
                          v["icon_bitmap_tag"], v["icon_sequence"], v["behaviour"], v["stock"], v["replaces_map"],
                          v["legacy"], v["catalogue"], v["projectile"], v["equipment"], v["collection"], v["hud"],
                          v["field"], v["root_first"], v["root_count"])


NON_WEAPON = dict(root_tag=NONE, item_collection_tag=NONE, label=None, fb0=None, fb1=None, menu=None, pickup=None,
                  ammo=None, behaviour=0, stock=0, replaces_map=0, legacy=0)


def sections_of(image):
    return {kind: image[offset:offset + stored] for kind, offset, stored, inflated, compression in
            aepack.section_entries(image)}


def build_cache(mutate=None, header=None, directory=None, configs=None, externs=None, roots=(3, 0),
                parts=(), serial=1, section_changes=None):
    """a valid cache (a weapon, a tag set, the base layer; one config; one extern), changed by the callers"""
    contents = sections_of(aepack.build(base_pack()))
    strings = Strings(contents[STRINGS])
    entries = directory if directory is not None else [
        entry(strings),
        entry(strings, id=0x02000001, kind=2, key="vehicle_damage", display="VEHICLE DAMAGE", tag_count=2,
              first_tag=1, root_first=0, root_count=2, **NON_WEAPON),
        entry(strings, id=0x04000001, kind=4, key="base", display="BASE LAYER", tag_count=5, **NON_WEAPON),
    ]
    root_list = struct.pack("<2I", 1, 3)
    records = b"".join(pack_entry(strings, e) for e in entries)
    directory_section = struct.pack("<4I", len(entries), 2, 0, 0) + records + root_list
    config_values = configs if configs is not None else [dict(config_id=0x01000001, weapon_id=1, key="br_mag24",
        display="24 ROUNDS", mask=1, rounds=(24, 0, 0), rate=(0, 0), error=(0, 0), zoom=(0, 0), autoaim=(0, 0),
        magnetism=(0, 0), behaviour=0, hud=NONE, menu=None)]
    config_section = b"".join(CONFIG.pack(c["config_id"], c["weapon_id"], strings.add(c["key"]), strings.add(c["display"]),
        c["mask"], *c["rounds"], *c["rate"], *c["error"], c["zoom"][0], c["zoom"][1], *c["autoaim"], *c["magnetism"],
        c["behaviour"], c["hud"], strings.add(c["menu"]), 0, 0, 0) for c in config_values)
    extern_values = externs if externs is not None else [dict(group=EFFECT, name="effects\\x", required=1, first=0, count=1)]
    extern_section = struct.pack("<4I", len(extern_values), 1, 0, 0) + b"".join(
        EXTERN.pack(x["group"], strings.add(x["name"]), x["required"], x["first"], x["count"]) for x in extern_values) + \
        struct.pack("<I", 1)
    if mutate:
        mutate(contents, strings)
    contents[STRINGS] = bytes(strings.data)
    contents[WEAPONS] = b""
    contents[DIRECTORY_KIND] = directory_section
    contents[CONFIGS_KIND] = config_section
    contents[EXTERNS_KIND] = extern_section
    for kind, data in (section_changes or {}).items():
        contents[kind] = data
    kinds = sorted(contents)
    image = bytearray(HEADER_SIZE + len(kinds) * 20)
    table = []
    for kind in kinds:
        image += b"\0" * (aepack.align(len(image)) - len(image))
        offset = len(image)
        image += contents[kind]
        table.append((kind, offset, len(contents[kind]), len(contents[kind]), 0))
    fields = dict(magic=b"AEWCACHE", version=1, header_size=HEADER_SIZE, flags=0, serial=serial, schema=0x1122334455667788,
                  layout=1, registry=1, text=b"0.7.6", sha=b"\0" * 32, sections=len(kinds), parts=len(parts))
    fields.update(header or {})
    image[:0x60] = CACHE_HEADER.pack(fields["magic"], fields["version"], fields["header_size"], fields["flags"],
                                     fields["serial"], fields["schema"], fields["layout"], fields["registry"],
                                     fields["text"], fields["sha"], fields["sections"], fields["parts"])
    for index, part in enumerate(parts):
        image[0x60 + index * 0x40:0x60 + index * 0x40 + 0x40] = PART.pack(*part)
    for index, row in enumerate(table):
        struct.pack_into("<5I", image, HEADER_SIZE + index * 20, *row)
    return rehash(bytes(image))


def cache_section_offset(image, kind):
    count = struct.unpack_from("<I", image, 0x58)[0]
    for index in range(count):
        entry = struct.unpack_from("<5I", image, HEADER_SIZE + index * 20)
        if entry[0] == kind:
            return entry[1]
    raise KeyError(kind)


def rehash(image):
    data = bytearray(image)
    data[SHA_OFFSET:SHA_OFFSET + 32] = b"\0" * 32
    data[SHA_OFFSET:SHA_OFFSET + 32] = hashlib.sha256(bytes(data)).digest()
    return bytes(data)


def patched(image, offset, fmt, value):
    data = bytearray(image)
    struct.pack_into(fmt, data, offset, value)
    return rehash(bytes(data))


# ---------- the check tool

def _compiler():
    for name in ("gcc", "clang"):
        if shutil.which(name):
            return shutil.which(name)
    return None


@pytest.fixture(scope="session")
def sanitized_checker(tmp_path_factory):
    cc = _compiler()
    if not cc:
        return None
    binary = tmp_path_factory.mktemp("ae_cache_check") / "ae_cache_check"
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
            pytest.skip("build/linux/ae_cache_check is not built (ninja linux)")
        return NINJA_CHECKER
    built = request.getfixturevalue("sanitized_checker")
    if built is None:
        pytest.skip("this machine lacks a compiler, the sanitizers or zlib's headers")
    return built


def run(checker, image, tmp_path, name="cache.aecache", extra=()):
    path = tmp_path / name
    path.write_bytes(image)
    result = subprocess.run([str(checker), *extra, str(path)], capture_output=True, text=True, timeout=60,
                            env=SANITIZER_ENVIRONMENT)
    return result.returncode, result.stdout


def refusal(stdout):
    for line in stdout.splitlines():
        if ": refused: " in line:
            return line.split(": refused: ", 1)[1]
    return None


# ---------- the valid cache

def test_a_valid_cache_is_accepted_and_described(checker, tmp_path):
    code, out = run(checker, build_cache(), tmp_path)
    assert code == 0, out
    assert out.rstrip().endswith("cache.aecache: ok")
    assert "serial 1 registry 1 version \"0.7.6\" schema 1122334455667788 parts 0" in out
    assert "entry 0 id 0x00000001 weapon" in out and "key battle_rifle" in out
    assert "entry 1 id 0x02000001 tag_set" in out and "roots 2 from 0" in out
    assert "entry 2 id 0x04000001 base" in out
    assert "config 0 id 0x01000001 weapon 0x00000001 key br_mag24 mask 0x1 rounds 24,0,0" in out
    assert "extern 0 effe required 1 entries 1 from 0" in out


def test_the_reader_reads_the_ids_it_wrote(checker, tmp_path):
    image = build_cache()
    digest = hashlib.sha256(image[:SHA_OFFSET] + b"\0" * 32 + image[SHA_OFFSET + 32:]).hexdigest()
    code, out = run(checker, image, tmp_path)
    assert code == 0 and f"sha256 {digest} id {digest[:16]}" in out


def test_a_part_is_recorded(checker, tmp_path):
    part = (b"mcc", 1, 0, 4096, b"\x11" * 32)
    code, out = run(checker, build_cache(parts=[part]), tmp_path)
    assert code == 0 and "part 0 name mcc flags 1 size 4096 sha256 " + "11" * 32 in out


# ---------- hostile caches

def header_cases():
    return [
        ("not a weapon cache", dict(magic=b"AEWPACK\0")),
        ("format version 2", dict(version=2)),
        ("bad header size", dict(header_size=0x100)),
        ("unknown header flags", dict(flags=2)),
        ("bad serial 0", dict(serial=0)),
        ("unknown tag layout", dict(layout=2)),
        ("bad registry revision 0", dict(registry=0)),
        ("bad version text", dict(text=b"")),
        ("bad version text", dict(text=b"0.7.\x01")),
        ("bad section count", dict(sections=0)),
        ("bad section count", dict(sections=17)),
        ("bad part count", dict(parts=5)),
    ]


@pytest.mark.parametrize("reason,header", header_cases(), ids=[case[0] + str(index) for index, case in
                                                               enumerate(header_cases())])
def test_a_bad_header_field_is_refused(checker, tmp_path, reason, header):
    code, out = run(checker, build_cache(header=header), tmp_path)
    assert code == 1 and (refusal(out) or "").startswith(reason), out


def test_a_pack_is_not_a_cache_and_a_cache_not_a_pack(checker, tmp_path):
    code, out = run(checker, aepack.build(base_pack()), tmp_path)
    assert code == 1 and refusal(out) == "not a weapon cache"


def test_a_damaged_cache_fails_its_hash(checker, tmp_path):
    image = bytearray(build_cache())
    image[-1] ^= 0xFF
    code, out = run(checker, bytes(image), tmp_path)
    assert code == 1 and refusal(out) == "SHA-256 mismatch"


def mutate_entry(index, **changes):
    def build():
        strings = Strings(sections_of(aepack.build(base_pack()))[STRINGS])
        entries = [entry(strings),
                   entry(strings, id=0x02000001, kind=2, key="vehicle_damage", display="VEHICLE DAMAGE", tag_count=2,
                         first_tag=1, root_first=0, root_count=2, **NON_WEAPON),
                   entry(strings, id=0x04000001, kind=4, key="base", display="BASE LAYER", tag_count=5, **NON_WEAPON)]
        entries[index].update(changes)
        return build_cache(directory=entries)
    return build


DIRECTORY_CASES = [
    ("ids not ascending", 1, dict(id=1)),
    ("kind does not match its id", 0, dict(kind=2)),
    ("kind does not match its id", 0, dict(id=0x01000009)),
    ("bad part", 0, dict(part=1)),
    ("unknown flags", 0, dict(flags=8)),
    ("bad key", 0, dict(key="Battle Rifle")),
    ("bad display name", 0, dict(display="")),
    ("tags past the cache's", 0, dict(first_tag=3, tag_count=4)),
    ("bad root tag", 0, dict(root_tag=1)),
    ("bad item collection", 0, dict(item_collection_tag=1)),
    ("bad behaviour, power or replaces", 0, dict(behaviour=6)),
    ("bad behaviour, power or replaces", 0, dict(stock=1)),
    ("bad replaces_map_own or legacy slot", 0, dict(replaces_map=3)),
    ("bad replaces_map_own or legacy slot", 0, dict(legacy=25)),
    ("bad label", 0, dict(label="x" * 40)),
    ("weapon fields on a non-weapon", 1, dict(behaviour=2)),
    ("weapon fields on a non-weapon", 2, dict(root_tag=0)),
    ("grenade fields on a non-grenade", 1, dict(projectile=0)),
    ("root list past the directory's", 1, dict(root_first=1, root_count=2)),
    ("a root past the cache's tags", 1, dict(root_first=0, root_count=2, tag_count=2, first_tag=1)),
    ("a root list on a non-tag-set", 2, dict(root_count=1)),
    ("a root list on a non-tag-set", 2, dict(root_first=3)),
]


@pytest.mark.parametrize("reason,index,changes", DIRECTORY_CASES, ids=[f"{case[0]}-{n}" for n, case in
                                                                       enumerate(DIRECTORY_CASES)])
def test_a_bad_directory_record_is_refused(checker, tmp_path, reason, index, changes):
    if reason == "a root past the cache's tags":
        # (the shared root list holds tags 1 and 3: make one past the cache's tags instead)
        image = build_cache()
        offset = cache_section_offset(image, DIRECTORY_KIND) + 16 + 3 * 96
        image = patched(image, offset, "<I", 99)
    else:
        image = mutate_entry(index, **changes)()
    code, out = run(checker, image, tmp_path)
    assert code == 1 and reason in (refusal(out) or ""), out


def test_two_weapons_of_one_key_are_refused(checker, tmp_path):
    def two(contents, strings):
        pass
    strings = Strings(sections_of(aepack.build(base_pack()))[STRINGS])
    entries = [entry(strings), entry(strings, id=2), entry(strings, id=0x04000001, kind=4, key="base", display="B",
                                                          tag_count=5, **NON_WEAPON)]
    code, out = run(checker, build_cache(directory=entries), tmp_path)
    assert code == 1 and "a second entry of that key" in (refusal(out) or ""), out


def test_the_directory_size_must_match_its_counts(checker, tmp_path):
    image = build_cache()
    code, out = run(checker, patched(image, cache_section_offset(image, DIRECTORY_KIND), "<I", 2), tmp_path)
    assert code == 1 and "DIRECTORY size does not match its counts" in (refusal(out) or ""), out
    code, out = run(checker, patched(image, cache_section_offset(image, DIRECTORY_KIND), "<I", 0), tmp_path)
    assert code == 1 and "DIRECTORY counts out of range" in (refusal(out) or ""), out


def test_a_cache_without_a_directory_is_refused(checker, tmp_path):
    code, out = run(checker, build_cache(section_changes={DIRECTORY_KIND: b""}), tmp_path)
    assert code == 1 and "no DIRECTORY" in (refusal(out) or ""), out


CONFIG_CASES = [
    ("not a config of a weapon", dict(weapon_id=0x04000001)),
    ("not a config of a weapon", dict(weapon_id=77)),
    ("id out of order or not a config id", dict(config_id=0x02000001)),
    ("bad key or display", dict(key="Bad Key")),
    ("unknown overrides", dict(mask=0x800)),
    ("a value without its override bit", dict(rounds=(24, 5, 0))),
    ("rounds out of range", dict(rounds=(70000, 0, 0))),
    ("bad rate of fire", dict(mask=1 | 8, rate=(5.0, 2.0))),
    ("bad error angle", dict(mask=1 | 16, error=(0.0, 4.0))),
    ("bad zoom", dict(mask=1 | 32, zoom=(9, 2.0))),
    ("bad autoaim or magnetism", dict(mask=1 | 64, autoaim=(0.1, -1.0))),
    ("bad behaviour", dict(mask=1 | 256, behaviour=6)),
    ("bad HUD interface", dict(mask=1 | 512, hud=0)),
]


@pytest.mark.parametrize("reason,changes", CONFIG_CASES, ids=[f"{case[0]}-{n}" for n, case in enumerate(CONFIG_CASES)])
def test_a_bad_config_is_refused(checker, tmp_path, reason, changes):
    config = dict(config_id=0x01000001, weapon_id=1, key="br_mag24", display="24 ROUNDS", mask=1, rounds=(24, 0, 0),
                  rate=(0, 0), error=(0, 0), zoom=(0, 0), autoaim=(0, 0), magnetism=(0, 0), behaviour=0, hud=NONE,
                  menu=None)
    config.update(changes)
    code, out = run(checker, build_cache(configs=[config]), tmp_path)
    assert code == 1 and reason in (refusal(out) or ""), out


def test_two_configs_of_a_weapon_may_sit_side_by_side_but_not_share_a_key(checker, tmp_path):
    base = dict(config_id=0x01000001, weapon_id=1, key="a", display=None, mask=1, rounds=(24, 0, 0), rate=(0, 0),
                error=(0, 0), zoom=(0, 0), autoaim=(0, 0), magnetism=(0, 0), behaviour=0, hud=NONE, menu=None)
    second = dict(base, config_id=0x01000002, key="b", rounds=(8, 0, 0))
    code, out = run(checker, build_cache(configs=[base, second]), tmp_path)
    assert code == 0, out
    code, out = run(checker, build_cache(configs=[base, dict(second, key="a")]), tmp_path)
    assert code == 1 and "a second config of that key" in (refusal(out) or ""), out


def test_a_cache_without_configs_or_externs_is_valid(checker, tmp_path):
    code, out = run(checker, build_cache(configs=[], externs=[], section_changes={EXTERNS_KIND: b""}), tmp_path)
    assert code == 0, out


EXTERN_CASES = [
    ("bad group, name or required", dict(group=0)),
    ("bad group, name or required", dict(required=2)),
    ("entries past the list", dict(first=1, count=1)),
]


@pytest.mark.parametrize("reason,changes", EXTERN_CASES, ids=[f"{case[0]}-{n}" for n, case in enumerate(EXTERN_CASES)])
def test_a_bad_extern_is_refused(checker, tmp_path, reason, changes):
    extern = dict(group=EFFECT, name="effects\\x", required=1, first=0, count=1)
    extern.update(changes)
    code, out = run(checker, build_cache(externs=[extern]), tmp_path)
    assert code == 1 and reason in (refusal(out) or ""), out


def test_an_extern_naming_an_entry_not_in_the_directory_is_refused(checker, tmp_path):
    image = build_cache()
    offset = cache_section_offset(image, EXTERNS_KIND) + 16 + 20
    code, out = run(checker, patched(image, offset, "<I", 99), tmp_path)
    assert code == 1 and "an entry that is not in the directory" in (refusal(out) or ""), out


def test_a_cache_with_weapon_records_is_refused(checker, tmp_path):
    code, out = run(checker, build_cache(section_changes={WEAPONS: b"\0" * 56}), tmp_path)
    assert code == 1 and "too large" in (refusal(out) or ""), out


def test_duplicate_tag_names_are_refused_in_a_big_cache_too(checker, tmp_path):
    def duplicate(contents, strings):
        pass
    pack = base_pack()
    pack.tags[3].name = pack.tags[2].name
    pack.tags[3].group = pack.tags[2].group
    contents = sections_of(aepack.build(pack))
    code, out = run(checker, build_cache(mutate=lambda c, s: c.update({TAGS: contents[TAGS], STRINGS: contents[STRINGS]})),
                    tmp_path)
    assert code == 1 and "a second tag of that group and name" in (refusal(out) or ""), out


# ---------- the STRINGS limit (pinned to ae_weapon_cache_format.h's AE_CACHE_MAXIMUM_STRINGS and aepack.py's)

def test_strings_up_to_four_mib_are_accepted_and_past_it_refused(checker, tmp_path):
    def padded(size):
        return lambda contents, strings: strings.data.extend(b"x" * (size - len(strings.data) - 1) + b"\0")
    code, out = run(checker, build_cache(mutate=padded(300 * 1024)), tmp_path, extra=("--no-resources",))
    assert code == 0, out
    code, out = run(checker, build_cache(mutate=padded(4 * 1024 * 1024)), tmp_path, extra=("--no-resources",))
    assert code == 0, out
    code, out = run(checker, build_cache(mutate=padded(4 * 1024 * 1024 + 16)), tmp_path, extra=("--no-resources",))
    assert code == 1 and "STRINGS too large" in (refusal(out) or ""), out


# ---------- fuzz

def test_byte_flipped_caches_neither_crash_nor_hang(checker, tmp_path):
    image = build_cache(parts=[(b"mcc", 1, 0, 4096, b"\x11" * 32)])
    generator = random.Random(2026)
    for _ in range(FUZZ_ITERATIONS):
        data = bytearray(image)
        for _ in range(generator.randint(1, 4)):
            data[generator.randrange(len(data))] = generator.randrange(256)
        code, out = run(checker, rehash(bytes(data)), tmp_path, name="fuzz.aecache", extra=("--no-resources",))
        assert code in (0, 1), out
