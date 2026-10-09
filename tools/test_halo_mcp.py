"""Offline tests of tools/halo_mcp: the path policy, the argument checks (Invader replaced by a recorder) and the
map reader on a small synthetic cache file. No Invader, no game files and no `mcp` package needed.

    python3 -m unittest tools.test_halo_mcp
"""
import os
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from halo_mcp import core, mapread  # noqa: E402

SOURCE = HERE.parent / 'source'


class PolicyBase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name).resolve()
        self.read = base / 'read'
        self.write = base / 'write'
        self.outside = base / 'outside'
        for d in (self.read, self.write, self.outside):
            d.mkdir()
        self.saved = (core.READ_ROOTS, core.WRITE_ROOTS, core.run)
        core.READ_ROOTS = [self.read, self.write]
        core.WRITE_ROOTS = [self.write]
        self.calls = []

        def fake_run(prog, args, cwd=None):
            self.calls.append((prog, [str(a) for a in args]))
            return 0, '', ''
        core.run = fake_run

    def tearDown(self):
        core.READ_ROOTS, core.WRITE_ROOTS, core.run = self.saved
        self.tmp.cleanup()


class SafePath(PolicyBase):
    def test_inside_read_root(self):
        f = self.read / 'a.map'
        f.write_bytes(b'x')
        self.assertEqual(core.safe_path(str(f)), f)

    def test_outside_refused(self):
        f = self.outside / 'a.map'
        f.write_bytes(b'x')
        with self.assertRaises(core.ToolError):
            core.safe_path(str(f))

    def test_dotdot_escape_refused(self):
        (self.outside / 'a.map').write_bytes(b'x')
        with self.assertRaises(core.ToolError):
            core.safe_path(str(self.read / '..' / 'outside' / 'a.map'))

    def test_symlink_escape_refused(self):
        (self.outside / 'a.map').write_bytes(b'x')
        link = self.read / 'link'
        link.symlink_to(self.outside)
        with self.assertRaises(core.ToolError):
            core.safe_path(str(link / 'a.map'))

    def test_nul_and_empty_refused(self):
        for bad in ('', 'a\0b', None, 5):
            with self.assertRaises(core.ToolError):
                core.safe_path(bad)

    def test_write_root_is_narrower_than_read_root(self):
        self.assertEqual(core.safe_path(str(self.write / 'x'), write=True, must_exist=False), self.write / 'x')
        with self.assertRaises(core.ToolError):
            core.safe_path(str(self.read / 'x'), write=True, must_exist=False)

    def test_missing_path(self):
        with self.assertRaises(core.ToolError):
            core.safe_path(str(self.read / 'nope'))
        self.assertEqual(core.safe_path(str(self.read / 'nope'), must_exist=False), self.read / 'nope')

    def test_no_roots_configured(self):
        core.WRITE_ROOTS = []
        with self.assertRaises(core.ToolError):
            core.safe_path(str(self.write / 'x'), write=True, must_exist=False)


class Words(unittest.TestCase):
    def test_safe_tag(self):
        self.assertEqual(core.safe_tag('weapons\\pistol\\pistol.weapon'), 'weapons/pistol/pistol.weapon')
        for bad in ('', '-x', '/etc/passwd', '../a', 'a/../b', 'a\\..\\b', 'a\0', None):
            with self.assertRaises(core.ToolError, msg=repr(bad)):
                core.safe_tag(bad)

    def test_safe_word(self):
        self.assertEqual(core.safe_word('weapon', 'group'), 'weapon')
        for bad in ('', '-s', 'a\0', None):
            with self.assertRaises(core.ToolError, msg=repr(bad)):
                core.safe_word(bad, 'group')


class Arguments(PolicyBase):
    def test_extract_checks_and_arguments(self):
        m = self.read / 'a.map'
        m.write_bytes(b'x')
        with self.assertRaises(core.ToolError):  # output folder outside the write root
            core.extract_tags(str(m), ['*'], str(self.read / 'out'))
        with self.assertRaises(core.ToolError):  # a search pattern that looks like an option
            core.extract_tags(str(m), ['-O'], str(self.write / 'out'))
        with self.assertRaises(core.ToolError):
            core.extract_tags(str(m), [], str(self.write / 'out'))
        self.assertEqual(self.calls, [])  # nothing reached Invader
        core.extract_tags(str(m), 'weapons/*', str(self.write / 'out'), recursive=True)
        prog, args = self.calls[-1]
        self.assertEqual(prog, 'invader-extract')
        self.assertEqual(args, ['-t', str(self.write / 'out'), '-s', 'weapons/*', '-r', str(m)])

    def test_build_map_checks(self):
        td = self.read / 'tags'
        td.mkdir()
        with self.assertRaises(core.ToolError):
            core.build_map('a/b', [str(td)], str(self.write / 'o.map'), engine='evil')
        with self.assertRaises(core.ToolError):
            core.build_map('a/b', [str(td)], str(self.read / 'o.map'))  # not a write root
        with self.assertRaises(core.ToolError):
            core.build_map('../b', [str(td)], str(self.write / 'o.map'))
        with self.assertRaises(core.ToolError):
            core.build_map('a/b', [], str(self.write / 'o.map'))
        self.assertEqual(self.calls, [])
        core.build_map('a/b.scenario', str(td), str(self.write / 'o.map'))
        prog, args = self.calls[-1]
        self.assertEqual(prog, 'invader-build')
        self.assertEqual(args[-1], 'a/b')
        self.assertIn('xbox-2276', args)

    def test_tag_info_checks(self):
        td = self.read / 'tags'
        td.mkdir()
        (td / 'a.weapon').write_bytes(b'x')
        (self.outside / 'secret.weapon').write_bytes(b'x')
        (td / 'link.weapon').symlink_to(self.outside / 'secret.weapon')
        for bad in ('../x', '-l', 'link.weapon', 'missing.weapon'):
            with self.assertRaises(core.ToolError, msg=bad):
                core.tag_info(str(td), bad)
        with self.assertRaises(core.ToolError):
            core.tag_info(str(td), 'a.weapon', key='-S')
        self.assertEqual(self.calls, [])
        core.tag_info(str(td), 'a.weapon', key='triggers[0].x')
        self.assertEqual(self.calls[-1][1][-3:], ['-G', 'triggers[0].x', 'a.weapon'])

    def test_compare_checks(self):
        a = self.read / 'a'
        a.mkdir()
        with self.assertRaises(core.ToolError):
            core.compare_tags(str(a), str(self.outside))
        with self.assertRaises(core.ToolError):
            core.compare_tags(str(a), str(a), show='bogus')
        self.assertEqual(self.calls, [])

    def test_list_tags_arguments_and_limit(self):
        td = self.read / 'tags'
        td.mkdir()
        for i in range(8):
            (td / ('t%d.weapon' % i)).write_bytes(b'x')
        with self.assertRaises(core.ToolError):
            core.list_tags()
        with self.assertRaises(core.ToolError):
            core.list_tags(map='a', tags_dir='b')
        with self.assertRaises(core.ToolError):
            core.list_tags(tags_dir=str(self.outside))
        r = core.list_tags(tags_dir=str(td), limit=3)
        self.assertEqual((r['total'], r['shown']), (8, 3))
        self.assertEqual(core.list_tags(tags_dir=str(td), limit=-5)['shown'], 1)  # a negative limit must not slice from the end
        self.assertEqual(core.list_tags(tags_dir=str(td), group='weapon', contains='T7')['tags'], ['t7.weapon'])

    def test_missing_invader(self):
        core.run = self.saved[2]
        saved = core.INVADER_DIR
        core.INVADER_DIR = self.outside
        try:
            with self.assertRaises(core.ToolError):
                core.run('invader-info', [])
        finally:
            core.INVADER_DIR = saved


def synthetic_cache():
    """A tiny Xbox-format cache file: 2048-byte header, then a zlib body holding the tag data (one weapon tag)."""
    base = mapread.CacheMap.BASE
    toff = 0                       # the tag data starts at the body's start
    # layout of the body: tag header (16 bytes), tag instance table (1 x 32), name, weapon bytes
    header_size = 16
    inst_addr = base + header_size
    name = b'weapons\\pistol\\pistol\0'
    name_addr = base + header_size + 32
    weap_addr = name_addr + len(name)
    body = bytearray(header_size)
    struct.pack_into('<IiIi', body, 0, inst_addr, 0, 0, 1)
    body += struct.pack('<iiiiIIII', int.from_bytes(b'weap', 'big'), 0, 0, 0, name_addr, weap_addr, 0, 0)
    body += name
    body += bytes(64)
    hdr = bytearray(2048)
    flen = 2048 + len(body)
    struct.pack_into('<iiiii', hdr, 4, 5, flen, 0, toff + 2048, len(body))
    return bytes(hdr) + zlib.compress(bytes(body)), flen


class MapReader(unittest.TestCase):
    def write(self, data):
        f = tempfile.NamedTemporaryFile(delete=False)
        f.write(data)
        f.close()
        self.addCleanup(os.unlink, f.name)
        return f.name

    def test_synthetic_header(self):
        data, _ = synthetic_cache()
        cm = mapread.CacheMap(self.write(data))
        self.assertEqual(list(cm.tags.values())[0][:2], ('weap', 'weapons\\pistol\\pistol'))
        self.assertIn(('weap', 'weapons\\pistol\\pistol'), cm.by)

    def test_wrong_length_refused(self):
        data, flen = synthetic_cache()
        bad = bytearray(data)
        struct.pack_into('<i', bad, 8, flen + 7)
        with self.assertRaises(ValueError):
            mapread.CacheMap(self.write(bytes(bad)))

    def test_not_a_cache_file(self):
        with self.assertRaises(Exception):
            mapread.CacheMap(self.write(b'\0' * 3000))

    def test_inflate_cap(self):
        data, _ = synthetic_cache()
        with self.assertRaises(ValueError):
            mapread.CacheMap(self.write(data), max_bytes=10)

    def test_weapon_missing_from_map(self):
        data, _ = synthetic_cache()
        cm = mapread.CacheMap(self.write(data))
        if not (SOURCE / 'items/weapon_definitions.h').is_file():
            self.skipTest('engine headers not found')
        with self.assertRaises(KeyError):
            mapread.weapon_numbers(cm, mapread.Layouts(str(SOURCE)), 'weapons\\nope\\nope')

    def test_layouts_from_engine_headers(self):
        if not (SOURCE / 'items/weapon_definitions.h').is_file():
            self.skipTest('engine headers not found')
        lay = mapread.Layouts(str(SOURCE))
        self.assertEqual(lay.layout('weapon_magazine_definition')[1], 0x70)
        self.assertEqual(lay.layout('weapon_trigger_definition')[1], 0x114)

    def test_unparsable_field_fails_loudly(self):
        with tempfile.TemporaryDirectory() as d:
            for h in mapread.HEADERS:
                p = Path(d) / h
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text('struct ok_t {\n\treal a;\n};\n')
            (Path(d) / 'game').mkdir(exist_ok=True)
            (Path(d) / 'game/game_globals.h').write_text('_material_dirt = 0, _material_x, NUMBER_OF_MATERIAL_TYPES')
            mapread.Layouts(d)  # parses
            (Path(d) / 'items/weapon_definitions.h').write_text('struct bad_t {\n\treal a[2][3];\n};\n')
            with self.assertRaises(ValueError):
                mapread.Layouts(d)


if __name__ == '__main__':
    unittest.main()
