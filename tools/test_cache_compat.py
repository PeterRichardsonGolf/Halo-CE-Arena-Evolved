#!/usr/bin/env python3
"""The weapon cache's join compatibility matrix, executable (tools/weapon_cache/COMPAT.md).

Each "machine" is one build's own join rules, compiled from that build's ae_silent_rules.[ch] and
ae_weapon_slots.[ch] by tools/weapon_cache/compat_driver.c:

  base    ae/release-076 (what 0.7.5 ships: the mark's four-bit level)
  bridge  ae/pack-slots-076 (the 24-slot widen: a level character past 15, refusal code 66's 0.7.6 texts)
  wave    ae/grenades-engine (the bridge and the grenade layer's feature mark, refusal code 68)

Two machines meet here the way a join request meets a host: the client's build writes the machine name, the host's
build reads it, and the host's rule (client level below the level the game needs: refused, code 65 for the battle
rifle alone, 66 above) decides. The texts are read from each build's own source. What a build with the cache identity
will do is in the TODO cases (skipped until a driver build defines AE_SILENT_JOIN_CACHE_MARK), written against the
reference model below, so the matrix is executable from day one.

    python3 -m pytest -q tools/test_cache_compat.py

Sources: refs (default) from the git checkout, or $AE_CACHE_COMPAT_TREES/<tag>/ holding those five files
(+ ae_silent.c, ae_joiner_notice.c) for a box without the refs: `python3 tools/test_cache_compat.py --export DIR`.
Override a tag's ref with $AE_CACHE_COMPAT_REF_<TAG>.
"""

import itertools
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
DRIVER = HERE / "weapon_cache" / "compat_driver.c"
LENGTH = 32
FILES = ["ae_silent_rules.c", "ae_silent_rules.h", "ae_weapon_slots.c", "ae_weapon_slots.h", "ae_pure_types.h",
         "ae_silent.c", "ae_joiner_notice.c"]
REFS = {"base": "ae/release-076", "bridge": "ae/pack-slots-076", "wave": "ae/grenades-engine"}
# (the build with the cache block: not a column of the shipped matrix, the TODO cases' machine)
CACHE_REFS = {"cache": "ae/wc-net"}
ALL_REFS = dict(REFS, **CACHE_REFS)

RIFLE_CODE, PACKS_CODE, GRENADES_CODE, CACHE_CODE = 65, 66, 68, 69
RIFLE_CAPABILITY = 2
NIBBLE_MAX = 15
BUILD_075_LAST = 14


def export_tree(tag, destination):
    tree = os.environ.get("AE_CACHE_COMPAT_TREES")
    destination.mkdir(parents=True, exist_ok=True)
    if tree and (Path(tree) / tag).is_dir():
        for name in FILES:
            source = Path(tree) / tag / name
            if source.exists():
                shutil.copy(source, destination / name)
        return
    ref = os.environ.get("AE_CACHE_COMPAT_REF_" + tag.upper(), ALL_REFS[tag])
    for name in FILES:
        shown = subprocess.run(["git", "-C", str(ROOT), "show", f"{ref}:port/linux/game/{name}"],
                               capture_output=True, text=True)
        if shown.returncode != 0:
            pytest.skip(f"no {ref}:{name} (and no $AE_CACHE_COMPAT_TREES/{tag})")
        (destination / name).write_text(shown.stdout)


class Machine:
    """one build's join rules, as a compiled driver"""

    def __init__(self, tag, folder):
        self.tag = tag
        self.folder = folder
        export_tree(tag, folder)
        self.exe = folder / "driver"
        built = subprocess.run(["gcc", "-Wall", f"-I{folder}", "-o", str(self.exe), str(DRIVER),
                                str(folder / "ae_silent_rules.c"), str(folder / "ae_weapon_slots.c")],
                               capture_output=True, text=True)
        assert built.returncode == 0, built.stderr
        self.info = dict(word.split("=") for word in self.run("info").split())
        self.source = (folder / "ae_silent.c").read_text()
        self.notice = (folder / "ae_joiner_notice.c").read_text()

    def run(self, *args):
        done = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True)
        assert done.returncode == 0, done.stderr
        return done.stdout.strip()

    @property
    def has_level_char(self):
        return self.info["level_char"] == "1"

    @property
    def has_features(self):
        return self.info["feature_mark"] == "1"

    @property
    def has_cache(self):
        return self.info["cache_mark"] == "1"

    def encode(self, level, grenades=0, name="Player", *cache):
        return self.run("encode", level, grenades, name, *cache).split()

    def decode(self, name):
        out = self.run("decode", *name)
        return dict(word.split("=") for word in out.split())

    def capability(self, name):
        return int(self.decode(name)["cap"])

    def features(self, name):
        told = self.decode(name)["features"]
        return None if told == "na" else int(told, 16)

    def with_features(self, name, grenades):
        return self.run("features", grenades, *name).split()

    # ---- the texts the build shows, from its source
    def client_text(self, code):
        """what the joining machine shows for a refusal code ('' when it knows none: 'could not join')"""
        key = {PACKS_CODE: "packs_text", RIFLE_CODE: "rifle_text", GRENADES_CODE: "grenades_text",
               CACHE_CODE: "cache_text"}[code]
        found = re.search(r"%s\[\]\s*=\s*((?:L\"(?:[^\"\\]|\\.)*\"\s*)+);" % key, self.source)
        if not found:
            return ""
        return "".join(re.findall(r'L"((?:[^"\\]|\\.)*)"', found.group(1)))

    def host_notice_texts(self):
        """the host's chat lines about a refused pack machine"""
        return re.findall(r'notice_say\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)', self.notice)


@pytest.fixture(scope="session")
def machines():
    root = Path(tempfile.mkdtemp(prefix="cache-compat-"))
    try:
        yield {tag: Machine(tag, root / tag) for tag in REFS}
    finally:
        shutil.rmtree(root, ignore_errors=True)


def host_decision(host, client_name, required):
    """what a host of this build does with a join whose gametype needs the pack level `required`: the refusal code
    (65 the battle rifle's alone, 66 above) or None when it takes the machine (ae_silent.c rifle_join_refused)"""
    if required <= 0 or host.capability(client_name) >= required:
        return None
    return PACKS_CODE if required > RIFLE_CAPABILITY else RIFLE_CODE


# ------------------------------------------------------------------------------------------ today: the facts

def test_builds_are_what_the_matrix_says(machines):
    assert machines["base"].info["level_char"] == "0" and machines["base"].info["feature_mark"] == "0"
    assert machines["bridge"].info["level_char"] == "1" and machines["bridge"].info["feature_mark"] == "0"
    assert machines["wave"].info["level_char"] == "1" and machines["wave"].info["feature_mark"] == "1"
    # (no build tells a slot past the 14th: the widen's 15-24 are levels the harness tells by hand)
    assert {m.info["slot_count"] for m in machines.values()} == {str(BUILD_075_LAST)}


def test_name_field_byte_map(machines):
    """the map COMPAT.md prints: index from the name's start (32 characters)"""
    wave = machines["wave"]
    name = wave.encode(24, 3, "Bob")
    assert name[:3] == ["0042", "006F", "0062"]
    assert int(name[31], 16) == 0xAE5F                        # the AE mark | the nibble (15)
    assert int(name[30], 16) == 0xAD00 | 24                   # the level character
    assert int(name[29], 16) == 0xAC00 | wave_features(3)     # the feature mark
    assert all(int(c, 16) == 0 for c in name[3:29])           # nothing else is written
    low = wave.encode(7, 0, "Bob")
    assert int(low[31], 16) == 0xAE57 and int(low[30], 16) == 0 and int(low[29], 16) == 0


def wave_features(version):
    return 0x01 | (version << 1)


# ------------------------------------------------------------------------------------------ today: the matrix

CLIENT_LEVELS = [0, 1, 2, 3, 14, 15, 16, 24, 32]


@pytest.mark.parametrize("host_tag,client_tag", list(itertools.product(REFS, REFS)))
def test_current_matrix(machines, host_tag, client_tag):
    """every host build x every client build x the levels a client tells x the levels a game needs"""
    host, client = machines[host_tag], machines[client_tag]
    for told in CLIENT_LEVELS:
        if client_tag == "base" and told > BUILD_075_LAST:
            continue                                   # (0.7.5 never tells past 14)
        name = client.encode(told) if told else client.encode(0)
        # (what the host reads: the nibble's 15 alone on a build without the level character)
        expected = told if (host.has_level_char or told <= NIBBLE_MAX) else NIBBLE_MAX
        if not told:
            expected = 1                               # (a marked machine without packs: the SILENT options')
        assert host.capability(name) == expected, (host_tag, client_tag, told)
        for required in (2, 3, 14, 16, 24):
            if host_tag == "base" and required > BUILD_075_LAST:
                continue                               # (0.7.5 cannot name a pack past the 14th)
            code = host_decision(host, name, required)
            assert (code is not None) == (expected < required), (host_tag, client_tag, told, required)
            if code:
                assert code == (PACKS_CODE if required > RIFLE_CAPABILITY else RIFLE_CODE)


def test_stock_machine_is_refused_everywhere(machines):
    """a machine that tells nothing (stock OpenCE / Chupa: zeros) reads as 0 and is refused by every pack game"""
    zeros = ["0000"] * LENGTH
    for host in machines.values():
        assert host.capability(zeros) == 0
        for required in (2, 14):
            assert host_decision(host, zeros, required) == (PACKS_CODE if required > 2 else RIFLE_CODE)


def test_widened_level_against_a_075_host(machines):
    """the bridge tells 24 to a 0.7.5 host: it reads 15, which every pack of its 14 is within"""
    for tag in ("bridge", "wave"):
        name = machines[tag].encode(24)
        assert machines["base"].capability(name) == NIBBLE_MAX
        assert host_decision(machines["base"], name, BUILD_075_LAST) is None
        assert machines[tag].capability(name) == 24


def test_075_machine_refused_by_a_game_past_its_slots(machines):
    """a bridge host's game naming slot 16-24: the 0.7.5 machine (14) is refused with code 66"""
    name = machines["base"].encode(14)
    for tag in ("bridge", "wave"):
        assert host_decision(machines[tag], name, 16) == PACKS_CODE
        assert host_decision(machines[tag], name, 24) == PACKS_CODE


# ------------------------------------------------------------------------------------------ today: the texts

def test_refusal_texts_by_cell(machines):
    base, bridge, wave = machines["base"], machines["bridge"], machines["wave"]
    # (a 0.7.5 joiner shows the packs text with no version; the bridge's names 0.7.6 for the newest packs)
    assert "Use the newest Arena Evolved with its packs folder" in base.client_text(PACKS_CODE)
    assert "0.7.6" not in base.client_text(PACKS_CODE)
    for tag in ("bridge", "wave"):
        assert "0.7.6 or later for the newest packs" in machines[tag].client_text(PACKS_CODE)
    for tag in REFS:
        assert "battle rifle" in machines[tag].client_text(RIFLE_CODE)
    # (code 68 only where the grenade layer is)
    assert base.client_text(GRENADES_CODE) == "" and bridge.client_text(GRENADES_CODE) == ""
    assert "Update Arena Evolved" in wave.client_text(GRENADES_CODE)
    # (the host's chat line: past the 14th the line names 0.7.6)
    assert not any("0.7.6" in t for t in base.host_notice_texts())
    for tag in ("bridge", "wave"):
        assert any("0.7.6" in t for t in machines[tag].host_notice_texts())


def test_unknown_code_is_only_could_not_join(machines):
    """a build that does not know code 69 / 68 shows nothing of its own (ae_silent_join_rejected returns)"""
    for tag in ("base", "bridge"):
        assert machines[tag].client_text(CACHE_CODE) == ""
    assert machines["base"].client_text(GRENADES_CODE) == ""


# ------------------------------------------------------------------------------------------ the proposed cache block
# Reference model of COMPAT.md's CACHE IDENTITY (characters 28..25 of the 32):
#   [28] CACHE_MARK 0xAB00 | format (1-255)   [27] generation (16 bits, 1.. per release that changes the cache)
#   [26] digest high 16                        [25] digest low 16
# written LAST (after the AE mark, the level and the feature mark), the name cut to end at 24 at the latest.

CACHE_MARK = 0xAB00
CACHE_INDEX = 28


def model_cache_write(name, format_, generation, digest):
    name = [int(c, 16) for c in name]
    end = next(i for i, c in enumerate(name) if c == 0)
    for i in range(min(end, 24), CACHE_INDEX + 1):
        name[i] = 0                                    # (the name ends at 24 at the latest; 24..28 start clean)
    name[CACHE_INDEX] = CACHE_MARK | format_
    name[27] = generation & 0xFFFF
    name[26] = (digest >> 16) & 0xFFFF
    name[25] = digest & 0xFFFF
    return ["%04X" % c for c in name]


def model_cache_read(name):
    name = [int(c, 16) for c in name]
    if (name[CACHE_INDEX] & 0xFF00) != CACHE_MARK:
        return None
    if not any(c == 0 for c in name[:25]):             # (the mark only after the name's end)
        return None
    if (name[31] & 0xFFF0) != 0xAE50:                  # (and only with the AE mark)
        return None
    return name[CACHE_INDEX] & 0xFF, name[27], (name[26] << 16) | name[25]


def test_cache_block_coexists_with_every_shipped_reader(machines):
    """the proposed block changes what NO build reads: capability and features come out as without it"""
    for tag, client in machines.items():
        for level in (2, 14):
            for grenades in (0, 3) if client.has_features else (0,):
                plain = client.encode(level, grenades, "Name")
                marked = model_cache_write(plain, 1, 7, 0xDEADBEEF)
                for reader in machines.values():
                    assert reader.capability(marked) == reader.capability(plain), (tag, reader.tag, level)
                    assert reader.features(marked) == reader.features(plain), (tag, reader.tag, level)
                assert model_cache_read(marked) == (1, 7, 0xDEADBEEF)
                assert model_cache_read(plain) is None
    wave = machines["wave"]
    for level in range(2, 33):
        for grenades in range(0, 8):
            plain = wave.encode(level, grenades, "x" * 40)         # (a long name: cut by the older writers)
            marked = model_cache_write(plain, 255, 0xFFFF, 0xFFFFFFFF)
            for reader in machines.values():
                assert reader.capability(marked) == reader.capability(plain)
                assert reader.features(marked) == reader.features(plain)
            assert model_cache_read(marked) == (255, 0xFFFF, 0xFFFFFFFF)


def test_cache_block_needs_a_name_end_before_it(machines):
    """a name that runs into the block is no block (the same rule as the level and the feature mark)"""
    name = ["0041"] * 25 + ["0000"] * 7
    name[28] = "%04X" % (CACHE_MARK | 1)
    name[31] = "AE52"
    assert model_cache_read(name) is None


def test_feature_writer_wipes_a_block_written_before_it(machines):
    """THE ORDER HAZARD: ae_silent_join_features zeroes from the name's end to character 28, so the cache block must be
    written after it. (COMPAT.md: the request writes mark, level, features, cache; a later feature writer must stop at 24)"""
    wave = machines["wave"]
    name = wave.encode(14, 0, "Bob")
    with_cache = model_cache_write(name, 1, 7, 0x12345678)
    after = wave.with_features(with_cache, 3)
    assert model_cache_read(after) is None, "the feature writer no longer wipes the block: COMPAT.md can drop its order rule"
    assert wave.features(after) == wave_features(3)
    # (the right order keeps both)
    right = model_cache_write(wave.encode(14, 3, "Bob"), 1, 7, 0x12345678)
    assert model_cache_read(right) == (1, 7, 0x12345678) and wave.features(right) == wave_features(3)


def test_level_writer_zeroes_the_feature_slot(machines):
    """the level writer clears character 29 (the feature mark's): the request writes the level before the features"""
    wave = machines["wave"]
    with_features = wave.encode(14, 3, "Bob")
    again = wave.run("encode", 24, 0, "Bob").split()
    assert int(again[29], 16) == 0
    assert int(with_features[29], 16) == 0xAC00 | wave_features(3)


# ------------------------------------------------------------------------------------------ TODO: the cache loader
# Active when a driver build defines AE_SILENT_JOIN_CACHE_MARK (ae_silent_join_cache / _told, COMPAT.md section 3).
# The reference decision of a host with the cache (COMPAT.md section 4):

CACHE_NEEDED = "needs the cache"


def cache_host_decision(client_kind, host_cache, client_cache, game_uses_cache, client_level=14):
    """client_kind: '075' / 'bridge' / 'cache'; caches are (format, generation, digest) or None.
    returns None (joins) or (code, text key)"""
    if not game_uses_cache:
        return None
    if client_cache is None:
        return (PACKS_CODE, "packs")              # 0.7.5, the bridge: the packs refusal, 'update'
    if client_cache[0] != host_cache[0]:
        return (CACHE_CODE, "format")
    if client_cache[1] < host_cache[1]:
        return (CACHE_CODE, "client older")
    if client_cache[1] > host_cache[1]:
        return (CACHE_CODE, "host older")
    if client_cache[2] != host_cache[2]:
        return (CACHE_CODE, "different build")
    return None


CACHE = (1, 3, 0xA1B2C3D4)


@pytest.mark.parametrize("client_cache,game_uses_cache,expected", [
    (None, False, None),                                   # 0.7.5 / bridge joins a game with no cache weapon
    (None, True, (PACKS_CODE, "packs")),                   # ... and is refused by one that has
    (CACHE, True, None),                                   # the same cache
    ((1, 3, 0x11111111), True, (CACHE_CODE, "different build")),
    ((1, 2, 0xA1B2C3D4), True, (CACHE_CODE, "client older")),
    ((1, 4, 0xA1B2C3D4), True, (CACHE_CODE, "host older")),
    ((2, 3, 0xA1B2C3D4), True, (CACHE_CODE, "format")),
    (CACHE, False, None),                                  # a cache client in a game without cache weapons
])
def test_reference_decision(client_cache, game_uses_cache, expected):
    assert cache_host_decision("x", CACHE, client_cache, game_uses_cache) == expected


@pytest.fixture
def cache_machine(machines, tmp_path):
    """the build with the cache block (ae/wc-net): a machine like the others; skipped where its ref is not there"""
    machine = Machine("cache", tmp_path / "cache")
    if not machine.has_cache:
        pytest.skip("no build defines AE_SILENT_JOIN_CACHE_MARK yet (the cache loader has not landed)")
    return machine


def test_cache_identity_round_trips(cache_machine, machines):
    """the build writes the block, its own host reads it, every older host reads the same capability: a cache build
    tells capability 1 (SILENT only), never a pack level (design 8.2)"""
    name = cache_machine.encode(0, 0, "Bob", 1, 3, 0xA1B2C3D4)
    assert cache_machine.decode(name)["cap"] == "1"
    assert cache_machine.decode(name)["cache"] == "1/3/0xA1B2C3D4"
    for tag, reader in machines.items():
        assert reader.capability(name) == 1, tag


def test_cache_matrix_cells(cache_machine, machines):
    """the driver's read against the told block for every cell of COMPAT.md's matrix, and the older readers' view: a
    cache machine in a pack game of an older host is refused by level (capability 1 < the game's), as design 8.6 says"""
    for told in (None, (1, 3, 0xA1B2C3D4), (1, 3, 0x11111111), (2, 3, 0xA1B2C3D4)):
        args = (*told,) if told else ()
        name = cache_machine.encode(0, 0, "Bob", *args)
        read = cache_machine.decode(name)["cache"]
        assert (read == "0/0/0x00000000") == (told is None)
        if told:
            assert read == "%d/%d/0x%08X" % told
    name = cache_machine.encode(0, 0, "Bob", 1, 3, 0xA1B2C3D4)
    for tag in ("base", "bridge", "wave"):
        assert host_decision(machines[tag], name, 2) == RIFLE_CODE
        assert host_decision(machines[tag], name, 14) == PACKS_CODE


def test_old_builds_join_only_games_without_ae_weapons(machines):
    """OWNER RULE (2026-11-05): a build without the cache joins only games that name no AE weapon. A cache host's
    decision for a machine without a block is code 66 (the packs refusal every build since 0.7.5 shows as 'use the newest
    Arena Evolved'); a game naming no weapon accepts it (the C rule, ae_silent_join_cache_decide; port/linux/tests/
    ae_silent_rules_test.c runs it; this restates it for the old machines' names)"""
    for tag in ("base", "bridge", "wave"):
        told = model_cache_read(machines[tag].encode(14, 0, "Bob"))
        assert told is None
        # (not told: no weapon named -> accept; any weapon named -> refuse with 66)
        assert cache_host_decision("old", CACHE, None, False) is None
        assert cache_host_decision("old", CACHE, None, True) == (PACKS_CODE, "packs")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--export":
        for tag in REFS:
            export_tree(tag, Path(sys.argv[2]) / tag)
        print("exported", ", ".join(REFS), "to", sys.argv[2])
    else:
        sys.exit(pytest.main([__file__, "-q"]))
