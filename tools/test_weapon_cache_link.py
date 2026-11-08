"""tools/weapon_cache/link.py: the linker's rules on small built fragments (no maps, no box), and on a real pack set
when AE_PACK_SET names one."""
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "weapon_pack"))
sys.path.insert(0, os.path.join(HERE, "weapon_cache"))
import aepack as ap  # noqa: E402
import link as lk  # noqa: E402
import registry as rg  # noqa: E402

BITM, WEAP, SNDS, PROJ = ap.group("bitm"), ap.group("weap"), ap.group("snd!"), ap.group("proj")


def ref(offset, target):
    return ap.Fixup(offset, ap.FIXUP_REFERENCE, target)


def fragment(pack_id, key, prefix, pixels=b"P" * 256, shared="effects\\spark", ext=(), extra=()):
    """a weapon fragment: weapon -> projectile (per weapon) and a shared-path bitmap; externals by name"""
    tags = [ap.Tag(WEAP, prefix + "w", bytes=b"W" * 64 + bytes([len(key)]) * 16, fixups=[ref(0, 1), ref(16, 2)]),
            ap.Tag(PROJ, prefix + "p", bytes=b"J" * 64 + pixels[:4], fixups=[ref(0, 2)]),
            ap.Tag(BITM, prefix + shared, bytes=b"B" * 8 + (0x5000 + 7).to_bytes(4, "little") + b"B" * 52,
                   resources=[ap.Resource(0x5000 + 7, pixels)])]
    for n, name in enumerate(ext):
        tags.append(ap.Tag(ap.group("jpt!"), name, kind=ap.EXTERNAL, required=1, bytes=b""))
    tags.extend(extra)
    return ap.build(ap.Pack(pack_id, 1, key.upper(), tags, [ap.Weapon(key, 0, "ar", key.upper())]))


def write(directory, name, image):
    with open(os.path.join(directory, name + ".aepack"), "wb") as handle:
        handle.write(image)


def registry(*keys):
    data = rg.load()
    data["rows"] = [r for r in data["rows"] if r["kind"] != "config" and
                    (r["kind"] != "weapon" or r["key"] in keys) and r["kind"] not in ("tag_set", "grenade")]
    return data


class LinkTest(unittest.TestCase):
    def link(self, fragments, base=None, release=False, keys=None):
        with tempfile.TemporaryDirectory() as d:
            for name, image in fragments.items():
                write(d, name, image)
            reg = registry(*(keys or [n[3:] for n in fragments if n != "ae_base"]))
            b, entries = lk.find_fragments(d, reg, include_held=True, allow_missing=True)  # (fixtures name pulled rows too)
            cache, report = lk.link(reg, b, entries, release)
            image = ap.build_cache(cache)
            return cache, report, ap.read_cache(image), image

    def test_equal_content_merges_across_weapons_names_stay_per_weapon(self):
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\")
        b = fragment("ae_smg", "smg", "ae\\packs\\smg\\")
        cache, report, parsed, _ = self.link({"ae_battle_rifle": a, "ae_smg": b})
        names = [t.name for t in cache.tags]
        # the bitmap is equal in both (different offsets masked): one copy, under the first weapon's name
        self.assertEqual([n for n in names if n.endswith("effects\\spark")], ["ae\\packs\\battle_rifle\\effects\\spark"])
        self.assertEqual(len(report["merged"]), 1)
        self.assertEqual(parsed.own_tag_count, 5)  # 2 weapons, 2 projectiles, 1 bitmap
        self.assertEqual(len(parsed.sections[ap.RESOURCE_DATA]), 256)

    def test_runtime_written_groups_never_merge(self):
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\", pixels=b"P" * 256)
        b = fragment("ae_smg", "smg", "ae\\packs\\smg\\", pixels=b"P" * 256)
        cache, _, _, _ = self.link({"ae_battle_rifle": a, "ae_smg": b})
        self.assertEqual(sum(1 for t in cache.tags if t.group == PROJ), 2)
        self.assertEqual(sum(1 for t in cache.tags if t.group == WEAP), 2)

    def test_different_content_same_path_stays_apart_then_a_flat_name_clash_refuses(self):
        # (inventory's collisions: same path under different pack prefixes, different content)
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\", pixels=b"P" * 256)
        b = fragment("ae_smg", "smg", "ae\\packs\\smg\\", pixels=b"Q" * 256)
        cache, report, _, _ = self.link({"ae_battle_rifle": a, "ae_smg": b})
        self.assertEqual(report["merged"], [])
        self.assertEqual(sum(1 for t in cache.tags if t.group == BITM), 2)
        # the same full name with different content is a refusal naming the tag
        c = fragment("ae_smg", "smg", "ae\\packs\\battle_rifle\\", pixels=b"Q" * 256)
        with self.assertRaises(lk.LinkError) as why:
            self.link({"ae_battle_rifle": a, "ae_smg": c})
        self.assertIn("different content", str(why.exception))

    def test_resource_offsets_are_the_caches_own_and_unique(self):
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\", pixels=b"P" * 256)
        b = fragment("ae_smg", "smg", "ae\\packs\\smg\\", pixels=b"Q" * 256)
        cache, _, parsed, _ = self.link({"ae_battle_rifle": a, "ae_smg": b})
        offsets = [r[1] for r in parsed.resources]
        self.assertEqual(len(set(offsets)), len(offsets))
        self.assertTrue(all(o & 0xFFF == 7 for o in offsets), "low 12 bits kept")

    def test_a_bitmap_groups_file_offset_is_folded_into_its_pixels_offset(self):
        # (a cube map's resource is keyed by file_offset + pixels_offset, here 0x5707 + 0x61000, found in no word of
        # the tag. The game's texture cache reads a placed bitmap at its pixels_offset ALONE (texture_cache_bitmap_new
        # never adds the group's file_offset there: 22 tags of the first cache read back as zeros), so the linked
        # tag has file_offset 0 and pixels_offset = the resource's map offset)
        def cube(pack_id, key, file_offset, pixels_offset):
            prefix = "ae\\packs\\" + key + "\\"
            body = bytearray(220)
            body[56:60] = file_offset.to_bytes(4, "little")
            body[96:100] = (1).to_bytes(4, "little")
            body[172:176] = b"mtib"
            body[172 + 0x18:172 + 0x1c] = pixels_offset.to_bytes(4, "little")
            body[172 + 0x1c:172 + 0x20] = (256).to_bytes(4, "little")
            tag = ap.Tag(BITM, prefix + "cubemap", bytes=body, fixups=[ap.Fixup(100, ap.FIXUP_POINTER, 172)],
                         resources=[ap.Resource(file_offset + pixels_offset, b"C" * 256)])
            return fragment(pack_id, key, prefix, extra=[tag])

        a = cube("ae_battle_rifle", "battle_rifle", 0x5707, 0x61000)
        b = cube("ae_smg", "smg", 0x1207, 0x61000)
        cache, report, parsed, _ = self.link({"ae_battle_rifle": a, "ae_smg": b}, keys=["battle_rifle", "smg"])
        cubes = [t for t in cache.tags if t.name.endswith("cubemap")]
        self.assertEqual(len(cubes), 1, "equal cube maps merge although their file offsets differ")
        tag = cubes[0]
        self.assertEqual(int.from_bytes(tag.bytes[56:60], "little", signed=True), 0, "the group's file_offset is 0")
        pixels_offset = int.from_bytes(tag.bytes[172 + 0x18:172 + 0x1c], "little", signed=True)
        self.assertEqual(tag.resources[0].map_offset, pixels_offset, "the game's read offset is the resource's")
        self.assertEqual(pixels_offset & 0xFFF, 0x707)
        self.assertEqual(lk.pixel_reads(tag), [(pixels_offset, 256)])
        lk.check_pixel_reads(tag, "test")
        # (and a group whose file_offset is not 0 is refused: its reads would land below the resource)
        bad = ap.Tag(tag.group, tag.name, bytes=bytearray(tag.bytes), fixups=tag.fixups, resources=tag.resources)
        bad.bytes[56:60] = (0x1000).to_bytes(4, "little")
        with self.assertRaises(lk.LinkError):
            lk.check_pixel_reads(bad, "test")

    def test_sound_resources_move_together_and_each_must_be_pointed_at(self):
        # (a sound's samples are resources named by words of the tag. They move all at once: one resource's NEW offset
        # may equal another's OLD one, and a word already rewritten must not be taken for the other's old offset. A
        # resource no word of the tag points at is refused: the game would read it from nowhere)
        def sound(pack_id, key, offsets, words):
            prefix = "ae\\packs\\" + key + "\\"
            body = bytearray(64)
            for at, value in zip((8, 24, 40), words):
                body[at:at + 4] = value.to_bytes(4, "little")
            tag = ap.Tag(SNDS, prefix + "fire", bytes=body,
                         resources=[ap.Resource(o, bytes([n + 1]) * 64) for n, o in enumerate(offsets)])
            return fragment(pack_id, key, prefix, extra=[tag])

        # (the weapon's bitmap takes cursor 0 first; the sound's first resource lands at 0x1000 + 0x007: choose the
        # second resource's old offset to be exactly that)
        first, second = 0x3007, 0x1007
        a = sound("ae_battle_rifle", "battle_rifle", [first, second], [first, second, 0])
        cache, _, parsed, _ = self.link({"ae_battle_rifle": a}, keys=["battle_rifle"])
        tag = [t for t in cache.tags if t.name.endswith("fire")][0]
        words = [int.from_bytes(tag.bytes[at:at + 4], "little") for at in (8, 24)]
        self.assertEqual(words, [r.map_offset for r in tag.resources], "each word names its own resource's new offset")
        self.assertNotEqual(words[0], words[1])
        b = sound("ae_smg", "smg", [0x3007, 0x4007], [0x3007, 0, 0])
        with self.assertRaises(lk.LinkError):
            self.link({"ae_smg": b}, keys=["smg"])

    def test_externals_resolve_to_the_base_layer_or_refuse_in_release(self):
        base_tag = ap.Tag(ap.group("jpt!"), "ae\\base\\weapons\\melee", bytes=b"M" * 32)
        base = ap.build(ap.Pack("ae_base", 1, "BASE", [base_tag], []))
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\", ext=("weapons\\melee",))
        # (the base layer's group allowlist: jpt! is runtime-written but the base is a single copy by name)
        cache, report, parsed, _ = self.link({"ae_battle_rifle": a, "ae_base": base})
        self.assertEqual(report["externs_left"], 0)
        self.assertEqual(parsed.externs, [])
        miss = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\", ext=("weapons\\nope",))
        with self.assertRaises(lk.LinkError):
            self.link({"ae_battle_rifle": miss, "ae_base": base}, release=True)
        cache, report, parsed, _ = self.link({"ae_battle_rifle": miss, "ae_base": base})
        self.assertEqual(report["externs_left"], 1)
        self.assertEqual(len(parsed.externs), 1)

    def test_base_in_several_files_is_joined(self):
        one = ap.build(ap.Pack("ae_base", 1, "BASE", [ap.Tag(ap.group("jpt!"), "ae\\base\\a", bytes=b"A" * 32),
                                                      ap.Tag(ap.group("jpt!"), "ae\\base\\shared", bytes=b"S" * 32)], []))
        two = ap.build(ap.Pack("ae_base", 1, "BASE", [ap.Tag(ap.group("jpt!"), "ae\\base\\b", bytes=b"B" * 32,
                                                             fixups=[ref(0, 1)]),
                                                      ap.Tag(ap.group("jpt!"), "ae\\base\\shared", bytes=b"S" * 32)], []))
        w = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\", ext=("a", "b"))
        cache, report, parsed, _ = self.link({"ae_battle_rifle": w, "ae_base_1": one, "ae_base_2": two})
        self.assertEqual(report["externs_left"], 0)
        self.assertEqual(sum(1 for t in cache.tags if t.name == "ae\\base\\shared"), 1)
        bad = ap.build(ap.Pack("ae_base", 1, "BASE", [ap.Tag(ap.group("jpt!"), "ae\\base\\shared", bytes=b"X" * 32)], []))
        with self.assertRaises(lk.LinkError):
            self.link({"ae_battle_rifle": w, "ae_base_1": one, "ae_base_3": bad})

    def test_fragment_must_carry_the_registry_weapon(self):
        a = fragment("ae_smg", "smg", "ae\\packs\\smg\\")
        reg = registry("battle_rifle")
        with tempfile.TemporaryDirectory() as d:
            write(d, "ae_battle_rifle", a)
            _, entries = lk.find_fragments(d, reg, allow_missing=True)
            with self.assertRaises(lk.LinkError):
                lk.link(reg, None, entries)

    def test_missing_fragment_for_an_active_row_refuses(self):
        with tempfile.TemporaryDirectory() as d:
            write(d, "ae_base", ap.build(ap.Pack("ae_base", 1, "BASE", [ap.Tag(BITM, "ae\\base\\x", bytes=b"x" * 8)], [])))
            with self.assertRaises(lk.LinkError):
                lk.find_fragments(d, registry("battle_rifle"))

    def test_several_configs_per_weapon_and_no_merging_of_them(self):
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\")
        cache, _, _, _ = self.link({"ae_battle_rifle": a})
        reg = registry("battle_rifle")
        for i in range(3):
            reg["rows"].append({"id": 0x01000001 + i, "kind": "config", "weapon": 1, "key": f"mag{i}", "display": f"MAG {i}",
                                "overrides": {"rounds_loaded": 12 + i}, "status": "active", "part": "main", "since": "0.7.6"})
        lk.add_configs(cache, reg)
        parsed = ap.read_cache(ap.build_cache(cache))
        self.assertEqual([c[5] for c in parsed.configs], [12, 13, 14])
        self.assertEqual({c[1] for c in parsed.configs}, {1})

    def test_unknown_override_and_dead_weapon_config_refuse(self):
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\")
        cache, _, _, _ = self.link({"ae_battle_rifle": a})
        reg = registry("battle_rifle")
        row = {"id": 0x01000001, "kind": "config", "weapon": 1, "key": "x", "overrides": {"nonsense": 1},
               "status": "active", "part": "main", "since": "0.7.6"}
        reg["rows"].append(row)
        with self.assertRaises(lk.LinkError):
            lk.add_configs(cache, reg)
        row["overrides"] = {}
        row["weapon"] = 9
        with self.assertRaises(lk.LinkError):
            lk.add_configs(cache, reg)

    def test_held_rows_are_left_out_of_a_release_link(self):
        registry_data = rg.load()
        held = {r["key"] for r in registry_data["rows"] if r["status"] == "held" and r["kind"] in ("weapon", "grenade")}
        # 0.7.6 cut: ids 15-24 and the EMP grenade are held (reserved, back in 0.7.7)
        self.assertTrue({"smg_tactical", "pdw", "revolver", "focus_beam", "spiker", "ravager", "auto_shotgun",
                         "grenade_launcher", "concussion_rifle", "gravity_hammer", "emp"} <= held)
        with tempfile.TemporaryDirectory() as d:
            for key in ("battle_rifle", "pdw", "emp"):
                write(d, "ae_grenades" if key == "emp" else "ae_" + key,
                      fragment("ae_" + key, key, "ae\\packs\\" + key + "\\"))
            _, entries = lk.find_fragments(d, registry_data, allow_missing=True)
            self.assertEqual([row["key"] for row, _ in entries], ["battle_rifle"])
            _, entries = lk.find_fragments(d, registry_data, include_held=True, allow_missing=True)
            self.assertIn("pdw", [row["key"] for row, _ in entries])

    def test_behaviours_up_to_five_are_carried_to_the_directory(self):
        for behaviour in (0, 3, 4, 5):
            tags = [ap.Tag(WEAP, "ae\\packs\\pdw\\w", bytes=b"W" * 64)]
            image = ap.build(ap.Pack("ae_pdw", 1, "PDW", tags, [ap.Weapon("pdw", 0, "ar", "PDW", behaviour=behaviour)]))
            cache, _, parsed, _ = self.link({"ae_pdw": image})
            entry = next(e for e in cache.directory if e.id == 16)
            self.assertEqual(entry.behaviour, behaviour)
            self.assertEqual(parsed.directory[0][18], behaviour)
        bad = ap.build(ap.Pack("ae_pdw", 1, "PDW", [ap.Tag(WEAP, "w", bytes=b"W" * 64)],
                               [ap.Weapon("pdw", 0, "ar", "PDW", behaviour=6)]))
        with self.assertRaises(ap.Refused):
            ap.read(bad)

    def test_damage_effects_never_merge_so_equal_kicks_stay_two_tags(self):
        jpt = ap.group("jpt!")
        kick = lambda prefix: ap.Tag(jpt, prefix + "kick", bytes=b"K" * 64)
        a = ap.build(ap.Pack("ae_grenade_launcher", 1, "GL", [ap.Tag(WEAP, "ae\\packs\\gl\\w", bytes=b"W" * 64,
                             fixups=[ref(0, 1)]), kick("ae\\packs\\gl\\")], [ap.Weapon("grenade_launcher", 0, "ar", "GL")]))
        b = ap.build(ap.Pack("ae_concussion_rifle", 1, "CR", [ap.Tag(WEAP, "ae\\packs\\cr\\w", bytes=b"V" * 64,
                             fixups=[ref(0, 1)]), kick("ae\\packs\\cr\\")], [ap.Weapon("concussion_rifle", 0, "ar", "CR")]))
        cache, report, _, _ = self.link({"ae_grenade_launcher": a, "ae_concussion_rifle": b})
        self.assertEqual(sum(1 for t in cache.tags if t.group == jpt), 2, "expected duplicates: jpt! is runtime-written")
        self.assertEqual(report["merged"], [])

    def test_schema_hash_comes_from_the_checker(self):
        with tempfile.TemporaryDirectory() as d:
            fake = os.path.join(d, "fake_check")
            with open(fake, "w") as handle:
                handle.write("#!/bin/sh\necho 'schema hash 773c4d0e1bbb1efa'\n")
            os.chmod(fake, 0o755)
            self.assertEqual(lk.schema_hash_from(fake), 0x773c4d0e1bbb1efa)
            with open(fake, "w") as handle:
                handle.write("#!/bin/sh\necho nothing\nexit 1\n")
            with self.assertRaises(lk.LinkError):
                lk.schema_hash_from(fake)

    def test_link_is_deterministic(self):
        a = fragment("ae_battle_rifle", "battle_rifle", "ae\\packs\\battle_rifle\\")
        b = fragment("ae_smg", "smg", "ae\\packs\\smg\\")
        first = self.link({"ae_battle_rifle": a, "ae_smg": b})[3]
        second = self.link({"ae_smg": b, "ae_battle_rifle": a})[3]
        self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
