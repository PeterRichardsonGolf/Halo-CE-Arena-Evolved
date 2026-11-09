"""The weapon cache format (.aecache v1): aepack.py's writer and reader, and, when the engine's checker is built
(AE_CACHE_CHECK=<path to ae_cache_check>), the same verdict from the C reader on every file written here."""
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "weapon_pack"))
import aepack as ap  # noqa: E402

WEAP, BITM, WPHI, ITMC = ap.group("weap"), ap.group("bitm"), ap.group("wphi"), ap.group("itmc")


def bitmap(name, pixels):
    tag = ap.Tag(BITM, name, bytes=b"B" * 64, resources=[ap.Resource(0x1000, pixels)])
    return tag


def small(configs=(), extra=None):
    tags = [ap.Tag(BITM, "ae\\base\\a", bytes=b"b" * 64, resources=[ap.Resource(0x2000, b"P" * 512)]),
            ap.Tag(WEAP, "ae\\packs\\gun\\gun", bytes=b"W" * 128),
            ap.Tag(WPHI, "ae\\packs\\gun\\hud", bytes=b"H" * 64),
            ap.Tag(BITM, "ae\\packs\\gun\\b", bytes=b"b" * 64, resources=[ap.Resource(0x9000, b"P" * 512)])]
    directory = [ap.DirectoryEntry(1, ap.KIND_WEAPON, "gun", "GUN", first_tag=1, tag_count=3, root_tag=1, label="ar",
                                   menu_name="GUN", behaviour=0, stock_fallback=2, legacy_slot=1),
                 ap.DirectoryEntry(0x02000001, ap.KIND_TAG_SET, "vehicle_damage", "VEHICLE DAMAGE", first_tag=3,
                                   tag_count=1, root_tags=[3]),
                 ap.DirectoryEntry(0x04000001, ap.KIND_BASE, "base", "BASE", first_tag=0, tag_count=1)]
    cache = ap.Cache(tags, directory, list(configs), credits="credits", schema_hash=0x1122334455667788)
    if extra:
        extra(cache)
    return cache


def configs(n):
    return [ap.ConfigEntry(0x01000001 + i, 1, f"mag{i}", f"MAG {i}", override_mask=1, rounds_loaded_maximum=12 + i)
            for i in range(n)]


def refused(image):
    try:
        ap.read_cache(image)
    except ap.Refused as why:
        return str(why)
    return None


class CacheFormatTest(unittest.TestCase):
    def test_round_trip(self):
        image = ap.build_cache(small())
        parsed = ap.read_cache(image)
        self.assertEqual(parsed.header["serial"], 1)
        self.assertEqual(parsed.header["schema_hash"], 0x1122334455667788)
        self.assertEqual([d[0] for d in parsed.directory], [1, 0x02000001, 0x04000001])
        self.assertEqual(parsed.root_list, [3])
        self.assertEqual(ap.build_cache(small()), image, "the writer is deterministic")
        self.assertEqual(len(ap.cache_id(image)), 8)

    def test_equal_resource_bytes_are_stored_once(self):
        image = ap.build_cache(small())
        parsed = ap.read_cache(image)
        self.assertEqual(len(parsed.sections[ap.RESOURCE_DATA]), 512)
        self.assertEqual(parsed.resources[0][3], parsed.resources[1][3])

    def test_several_configs_per_weapon(self):
        for n in (1, 3, 7):
            parsed = ap.read_cache(ap.build_cache(small(configs(n))))
            self.assertEqual(len(parsed.configs), n)
            self.assertEqual({c[1] for c in parsed.configs}, {1})
            self.assertEqual([c[5] for c in parsed.configs], [12 + i for i in range(n)])

    def test_parts_round_trip(self):
        def add(cache):
            cache.parts = [ap.CachePart("mcc", ap.PART_OPTIONAL, 5 << 32 | 7, b"\x11" * 32)]
        parsed = ap.read_cache(ap.build_cache(small(extra=add)))
        self.assertEqual(parsed.cache_parts[0].size, 5 << 32 | 7)

    def test_externs_round_trip(self):
        def add(cache):
            cache.externs = [ap.ExternEntry(BITM, "weapons\\x", 1, [1, 0x02000001])]
        parsed = ap.read_cache(ap.build_cache(small(extra=add)))
        self.assertEqual(parsed.extern_list, [1, 0x02000001])

    def test_refusals(self):
        good = bytearray(ap.build_cache(small(configs(2))))
        cases = {
            "not a weapon cache": lambda b: b.__setitem__(slice(0, 8), b"AEWPACK\0"),
            "format version": lambda b: b.__setitem__(8, 2),
            "unknown header flags": lambda b: b.__setitem__(0x10, 2),
            "SHA-256 mismatch": lambda b: b.__setitem__(len(b) - 1, b[-1] ^ 1),
        }
        for why, mutate in cases.items():
            bad = bytearray(good)
            mutate(bad)
            if why != "SHA-256 mismatch":
                ap.cache_rehash(bad)
            self.assertIn(why, refused(bytes(bad)), why)
        self.assertIsNone(refused(bytes(good)))

    def test_semantic_refusals(self):
        def with_(mutate):
            cache = small(configs(2))
            mutate(cache)
            return refused(ap.build_cache(cache))
        self.assertIn("ids not ascending", with_(lambda c: c.directory.reverse()))
        self.assertIn("kind does not match", with_(lambda c: setattr(c.directory[0], "kind", ap.KIND_TAG_SET)))
        self.assertIn("bad root tag", with_(lambda c: setattr(c.directory[0], "root_tag", 2)))
        self.assertIn("past the cache", with_(lambda c: setattr(c.directory[0], "tag_count", 99)))
        self.assertIn("not in the cache", with_(lambda c: setattr(c.configs[0], "weapon_id", 9)))
        self.assertIn("ids not ascending", with_(lambda c: c.configs.reverse()))
        self.assertIn("bad behaviour", with_(lambda c: setattr(c.directory[0], "behaviour", 6)))
        self.assertIn("bad string", with_(lambda c: setattr(c.directory[0], "key", "Bad Key")))

    def test_empty_or_missing_directory_is_refused_by_reader_and_writer(self):
        with self.assertRaises(ap.Refused):
            ap.build_cache(small(extra=lambda c: c.directory.clear()))
        # a file written without the check is refused by the reader
        self.assertIn("DIRECTORY empty", refused(ap.build_cache(small(extra=lambda c: c.directory.clear()), check=False)))
        # empty CONFIGS and EXTERNS stay legal
        self.assertIsNone(refused(ap.build_cache(small())))

    def test_a_pack_and_a_cache_refuse_each_other(self):
        cache = ap.build_cache(small())
        self.assertIsNotNone(self._pack_refusal(cache))
        pack = ap.build(ap.Pack("ae_x", 1, "X", [ap.Tag(WEAP, "w", bytes=b"W" * 64)], [ap.Weapon("x", 0, "ar", "X")]))
        self.assertIn("not a weapon cache", refused(pack))

    def test_strings_boundary(self):
        four_mib = 4 * 1024 * 1024
        at = ap.build_cache(small(), pad_strings_to=four_mib)
        over = ap.build_cache(small(), pad_strings_to=four_mib + 16)
        self.assertIsNone(refused(at), "exactly 4 MiB of STRINGS is accepted")
        self.assertIn("STRINGS too large", refused(over))
        self.c_verdicts(at, over)

    def c_verdicts(self, accepted, refused_image):
        """the C reader (AE_CACHE_CHECK) gives the same verdicts when it is set"""
        checker = os.environ.get("AE_CACHE_CHECK")
        if not checker:
            return
        with tempfile.TemporaryDirectory() as d:
            for name, image, want in (("at", accepted, 0), ("over", refused_image, 1)):
                path = os.path.join(d, name + ".aecache")
                with open(path, "wb") as handle:
                    handle.write(image)
                result = subprocess.run([checker, path], capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, want == 0, name + ": " + result.stdout + result.stderr)

    @staticmethod
    def _pack_refusal(image):
        try:
            ap.read(image)
        except ap.Refused as why:
            return str(why)
        return None

    @unittest.skipUnless(os.environ.get("AE_CACHE_CHECK"), "AE_CACHE_CHECK (the C reader's ae_cache_check) not set")
    def test_c_reader_agrees(self):
        good = ap.build_cache(small(configs(3)))
        bad = bytearray(good)
        bad[0x10] = 2
        ap.cache_rehash(bad)
        with tempfile.TemporaryDirectory() as d:
            for name, image in (("good", good), ("bad", bytes(bad))):
                path = os.path.join(d, name + ".aecache")
                open(path, "wb").write(image)
                result = subprocess.run([os.environ["AE_CACHE_CHECK"], path], capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, refused(image) is None, name + ": " + result.stdout)


if __name__ == "__main__":
    unittest.main()
