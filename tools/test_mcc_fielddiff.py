"""Unit tests for the converted-map field-diff gate (tools/mcc_convert)."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'mcc_convert'))
import getsrc  # noqa: E402
import halve  # noqa: E402
import unswizzle  # noqa: E402
import mcc_fielddiff as fd  # noqa: E402
import struct  # noqa: E402
import tempfile  # noqa: E402

FIX = os.path.join(os.path.dirname(__file__), 'mcc_convert', 'fixtures')


def fixture_dump(root, tag):
    """Reads the synthetic dump stored next to a fixture tag file."""
    with open(os.path.join(root, tag), encoding='utf-8') as f:
        return f.read().splitlines()


def diff2(a, b, **kw):
    field, ref, _ = fd.diff_dumps(a, b, **kw)
    return field, ref


def entries(*lines):
    return fd.parse_dump(list(lines))


class ParseTests(unittest.TestCase):
    def test_tree_art_and_alias(self):
        self.assertEqual(
            entries(' \u251c\u2500\u2500model (a/b.gbxmodel)      dependency'),
            [('', 'model (a/b.model)')])

    def test_only_the_selected_enum_option_is_a_value(self):
        a = entries('a_in      enum', ' \u251c\u2500\u2500none (*)      enum value',
                    ' \u2514\u2500\u2500alive      enum value')
        self.assertEqual(a, [('a_in', 'none (*) enum value')])

    def test_array_elements_are_part_of_the_path(self):
        a = entries('items[2]   array', ' \u251c\u2500\u2500items[0]   struct',
                    ' \u2502   \u2514\u2500\u2500rounds (5)   int16',
                    ' \u2514\u2500\u2500items[1]   struct',
                    '     \u2514\u2500\u2500rounds (5)   int16')
        self.assertEqual([p for p, _ in a], ['items[2]/items[0]', 'items[2]/items[1]'])


class DiffTests(unittest.TestCase):
    ENUM = ['a_in      enum', ' \u251c\u2500\u2500none%s      enum value',
            ' \u2514\u2500\u2500alive%s      enum value']

    def enum(self, none, alive):
        return [self.ENUM[0], self.ENUM[1] % (' (*)' if none else ''),
                self.ENUM[2] % (' (*)' if alive else '')]

    def test_enum_selection_change_is_a_difference(self):
        field, ref = diff2(self.enum(True, False), self.enum(False, True))
        self.assertEqual(ref, [])
        self.assertEqual(len(field), 2)

    def test_same_enum_selection_is_equal(self):
        self.assertEqual(diff2(self.enum(True, False), self.enum(True, False)), ([], []))

    def test_source_only_fields_ignored(self):
        a = ['rounds (5)  int16', 'use_3d_instantaneous_acceleration (1)  bitfield']
        b = ['rounds (5)  int16']
        self.assertEqual(diff2(a, b), ([], []))

    def test_ignored_enum_header_skips_its_options(self):
        a = ['prediction_type      enum', ' \u251c\u2500\u2500none (*)      enum value',
             ' \u2514\u2500\u2500other      enum value', 'rounds (5)  int16']
        b = ['prediction_type      enum', ' \u251c\u2500\u2500none      enum value',
             ' \u2514\u2500\u2500other (*)      enum value', 'rounds (5)  int16']
        self.assertEqual(diff2(a, b), ([], []))
        self.assertEqual(entries(*a), [('', 'rounds (5) int16')])

    def test_script_node_handles_are_ignored(self):
        a = ['root_expression_index (3816751116)  uint32']
        b = ['root_expression_index (7)  uint32']
        self.assertEqual(diff2(a, b), ([], []))

    def test_acceleration_compares_first_component_only(self):
        a = ['damage_instantaneous_acceleration (4.0,1.0,2.0)  vector3d (float)']
        b = ['damage_instantaneous_acceleration (4.0,0.0,0.0)  vector3d (float)']
        c = ['damage_instantaneous_acceleration (6.0,0.0,0.0)  vector3d (float)']
        self.assertEqual(diff2(a, b), ([], []))
        self.assertEqual(len(diff2(a, c)[0]), 2)

    def test_index_fields_are_compared(self):
        for name in ('magazine', 'location', 'pickup_text_index', 'loop_start_event'):
            field, _ = diff2([name + ' (1)  index'], [name + ' (3)  index'])
            self.assertEqual(len(field), 2, name)

    def test_same_value_elsewhere_does_not_hide_a_change(self):
        a = ['items[2]   array', ' \u251c\u2500\u2500items[0]   struct', ' \u2502   \u2514\u2500\u2500rounds (5)   int16',
             ' \u2514\u2500\u2500items[1]   struct', '     \u2514\u2500\u2500rounds (7)   int16']
        b = ['items[2]   array', ' \u251c\u2500\u2500items[0]   struct', ' \u2502   \u2514\u2500\u2500rounds (7)   int16',
             ' \u2514\u2500\u2500items[1]   struct', '     \u2514\u2500\u2500rounds (5)   int16']
        self.assertEqual(len(diff2(a, b)[0]), 4)

    def test_repeated_line_counts(self):
        a = ['rounds (5)  int16', 'rounds (5)  int16']
        b = ['rounds (5)  int16']
        self.assertEqual(len(diff2(a, b)[0]), 1)

    def test_field_change(self):
        field, ref = diff2(['rounds (5) int16'], ['rounds (7) int16'])
        self.assertEqual(ref, [])
        self.assertEqual(sorted(field), [' | rounds (5) int16', ' | rounds (7) int16'])

    def test_reference_change_is_a_ref_not_a_field(self):
        a = ['item (weapons/shotgun/shotgun.weapon) dependency']
        b = ['item (weapons/pistol/pistol.weapon) dependency']
        field, ref = diff2(a, b)
        self.assertEqual(field, [])
        self.assertEqual(len(ref), 1)
        self.assertIn('shotgun', ref[0][0])
        self.assertIn('pistol', ref[0][1])

    def test_model_group_alias_is_not_a_change(self):
        self.assertEqual(diff2(['model (m.gbxmodel) dependency'], ['model (m.model) dependency']), ([], []))


class RepairedIndexTests(unittest.TestCase):
    def run_pair(self, a, b):
        return fd.diff_dumps(a, b, repaired=True)

    def test_one_to_none_is_exempted(self):
        field, ref, ex = self.run_pair(['location (1)  index'], ['location (65535)  index'])
        self.assertEqual((field, ref), ([], []))
        self.assertEqual(len(ex), 1)

    def test_not_exempted_without_the_repaired_flag(self):
        field, _, ex = fd.diff_dumps(['location (1)  index'], ['location (65535)  index'])
        self.assertEqual(len(field), 2)
        self.assertEqual(ex, [])

    def test_a_change_to_another_index_is_counted(self):
        field, _, ex = self.run_pair(['magazine (2)  index'], ['magazine (5)  index'])
        self.assertEqual(len(field), 2)
        self.assertEqual(ex, [])

    def test_two_source_entries_for_one_field_are_counted(self):
        a = ['location (1)  index', 'location (2)  index']
        field, _, ex = self.run_pair(a, ['location (65535)  index'])
        self.assertEqual(ex, [])
        self.assertEqual(len(field), 3)

    def test_other_differences_in_the_tag_still_count(self):
        field, _, ex = self.run_pair(['location (1)  index', 'rounds (3)  int16'],
                                     ['location (65535)  index', 'rounds (4)  int16'])
        self.assertEqual(len(ex), 1)
        self.assertEqual(len(field), 2)


class ReferenceFormatTests(unittest.TestCase):
    def test_source_line_without_type_column(self):
        a = ['model (scenery/trees/tree_pine_snowsmall/tree_pine_snowsmall.gbxmodel)']
        b = ['model (scenery/trees/tree_pine_snowsmall/tree_pine_snowsmall.model)   dependency']
        self.assertEqual(diff2(a, b), ([], []))

    def test_changed_target_without_type_column_is_a_ref(self):
        a = ['item (weapons/shotgun/shotgun.weapon)']
        b = ['item (weapons/pistol/pistol.weapon)   dependency']
        field, ref = diff2(a, b)
        self.assertEqual(field, [])
        self.assertEqual(len(ref), 1)

    def test_numbers_in_parentheses_are_not_references(self):
        self.assertFalse(fd.is_reference('rounds (0.5)'))
        self.assertFalse(fd.is_reference('rounds (5) int'))

    def test_ref_diff_does_not_hide_field_diff(self):
        a = ['item (a/b.weapon) dependency', 'rounds (3) int']
        b = ['item (a/c.weapon) dependency', 'rounds (4) int']
        field, ref = diff2(a, b)
        self.assertEqual(len(ref), 1)
        self.assertEqual(sorted(field), [' | rounds (3) int', ' | rounds (4) int'])


class SourceTextTests(unittest.TestCase):
    def tag(self, entries, count=1, padding=b'\x00' * 16):
        b = bytearray(64 + 1216) + struct.pack('>I', count)
        b += b';*' + b'\x00' * 50 + b';*' + b'x' * 30   # node data holding ';*' pairs
        for name, text in entries:
            b += name.ljust(32, b'\x00') + struct.pack('>I', len(text)) + padding + text
        return bytes(b)

    def test_skips_node_data_pairs(self):
        text = b';* script\n(sleep 1)\n'
        self.assertEqual(getsrc.find_source(self.tag([(b'extracted', text)])), text.rstrip(b'\x00'))

    def test_no_sources_gives_empty(self):
        self.assertEqual(getsrc.find_source(self.tag([], count=0)), b'')

    def test_entry_with_nonzero_padding_is_not_an_entry(self):
        data = self.tag([(b'extracted', b';*ab')], padding=b'\x00' * 15 + b'\x01')
        self.assertEqual(getsrc.find_source(data), b'')

    def test_size_past_end_is_not_an_entry(self):
        b = bytearray(self.tag([(b'extracted', b';*ab')]))
        struct.pack_into('>I', b, len(b) - 4 - 16 - 4, 9999)
        self.assertEqual(getsrc.find_source(bytes(b)), b'')


class HalveTests(unittest.TestCase):
    def bitmap(self, entries):
        """A bitmap tag of entries (w, h, depth, type, format, mips), pixel bytes filled by entry number."""
        px = b''
        data = b''
        offset = 0
        for n, (w, h, d, t, f, m) in enumerate(entries):
            size = halve.entry_bytes(w, h, d, t, f, m)
            px += bytes([n + 1]) * size
            data += b'bitm' + struct.pack('>HHHHHH', w, h, d, t, f, 0) + b'\x00' * 4 + struct.pack('>HH', m, 0)
            data += struct.pack('>I', offset) + b'\x00' * 20
            offset += size
        head = bytearray(64 + 108)
        struct.pack_into('>I', head, 64 + 48, len(px))
        struct.pack_into('>I', head, 64 + 96, len(entries))
        return bytes(head) + px + data

    def run_on(self, entries, minw=4):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'a.bitmap')
            with open(p, 'wb') as f:
                f.write(self.bitmap(entries))
            r = halve.run(p, minw)
            with open(p, 'rb') as f:
                return r, f.read()

    def test_drops_mip_zero_and_rewrites_offsets(self):
        saved, out = self.run_on([(8, 8, 1, 0, 10, 3), (8, 8, 1, 0, 10, 3)])
        self.assertEqual(saved, 2 * 8 * 8 * 4)
        each = (4 * 4 + 2 * 2 + 1) * 4
        self.assertEqual(struct.unpack_from('>I', out, 64 + 48)[0], 2 * each)
        first = len(out) - 2 * 48
        self.assertEqual(struct.unpack_from('>HH', out, first + 4), (4, 4))
        self.assertEqual(struct.unpack_from('>H', out, first + 20)[0], 2)
        self.assertEqual(struct.unpack_from('>I', out, first + 24)[0], 0)
        self.assertEqual(struct.unpack_from('>I', out, first + 48 + 24)[0], each)

    def test_depth_is_kept(self):
        _, out = self.run_on([(8, 8, 1, 0, 10, 3)])
        self.assertEqual(struct.unpack_from('>H', out, len(out) - 48 + 8)[0], 1)

    def test_interface_bitmaps_are_left_alone(self):
        data = bytearray(self.bitmap([(8, 8, 1, 0, 10, 3)]))
        struct.pack_into('>H', data, 64, 4)
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'a.bitmap')
            with open(p, 'wb') as f:
                f.write(bytes(data))
            self.assertEqual(halve.run(p, 4), 0)

    def test_other_types_and_formats_are_left_alone(self):
        self.assertEqual(self.run_on([(8, 8, 1, 2, 10, 3)])[0], 0)
        self.assertEqual(self.run_on([(8, 8, 1, 0, 16, 3)])[0], 0)

    def test_offsets_that_do_not_tile_are_not_understood(self):
        data = bytearray(self.bitmap([(8, 8, 1, 0, 10, 3), (8, 8, 1, 0, 10, 3)]))
        struct.pack_into('>I', data, len(data) - 48 + 24, 12345)
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'a.bitmap')
            with open(p, 'wb') as f:
                f.write(bytes(data))
            self.assertIsNone(halve.run(p, 4))


class UnswizzleTests(unittest.TestCase):
    def swizzled_tag(self, entries):
        """A bitmap tag whose entries are flagged swizzled; the pixel bytes of an entry run 0, 1, 2, ..."""
        tag = bytearray(HalveTests().bitmap(entries))
        n = len(entries)
        for i in range(n):
            o = len(tag) - (n - i) * 48 + 14
            struct.pack_into('>H', tag, o, 0x01 | 0x08)
        npx = struct.unpack_from('>I', tag, 64 + 48)[0]
        start = 64 + 108
        tag[start:start + npx] = bytes(k % 256 for k in range(npx))
        return bytes(tag)

    def test_known_4x4_order(self):
        self.assertEqual(unswizzle.swizzle_2d(list(range(16)), 4, 4, True),
                         [0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15])

    def test_swizzle_and_deswizzle_are_inverse(self):
        for w, h in ((8, 8), (16, 4), (4, 16), (2, 8), (8, 1)):
            pixels = list(range(w * h))
            self.assertEqual(unswizzle.swizzle_2d(unswizzle.swizzle_2d(pixels, w, h, False), w, h, True), pixels)

    def test_pixels_are_reordered_and_the_flag_cleared(self):
        tag = self.swizzled_tag([(4, 4, 1, 0, 8, 0)])      # a1r5g5b5, 2 bytes a pixel
        out, n = unswizzle.unswizzle(tag)
        self.assertEqual(n, 1)
        flags = struct.unpack_from('>H', out, len(out) - 48 + 14)[0]
        self.assertEqual(flags, 0x01)
        first = [out[64 + 108 + 2 * k] for k in range(16)]
        self.assertEqual(first, [2 * p for p in [0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15]])

    def test_every_mip_is_handled(self):
        tag = self.swizzled_tag([(4, 4, 1, 0, 10, 1)])      # 4x4 then 2x2, 4 bytes a pixel
        out, _ = unswizzle.unswizzle(tag)
        px = 64 + 108
        # level 0: pixel 2 of the output is pixel 4 of the input
        self.assertEqual(out[px + 8:px + 12], tag[px + 16:px + 20])
        # level 1 is 2 pixels wide, so it is a straight copy
        self.assertEqual(out[px + 64:px + 80], tag[px + 64:px + 80])

    def test_compressed_and_cube_entries_are_refused(self):
        with self.assertRaises(ValueError):
            unswizzle.unswizzle(self.swizzled_tag([(8, 8, 1, 0, 16, 0)]))
        with self.assertRaises(ValueError):
            unswizzle.unswizzle(self.swizzled_tag([(8, 8, 1, 2, 10, 0)]))

    def test_unknown_layout_is_an_error(self):
        with self.assertRaises(ValueError):
            unswizzle.unswizzle(bytes(300))


class TreeTests(unittest.TestCase):
    def run_gate(self, allowed=()):
        return fd.compare_trees(os.path.join(FIX, 'src'), os.path.join(FIX, 'conv'),
                                fixture_dump, allowed=allowed)

    def test_fixture_counts(self):
        res = self.run_gate()
        self.assertEqual(res['checked'], 4)
        self.assertEqual(res['same'], 1)
        self.assertEqual(list(res['missing']), ['weapons/gone/gone.weapon'])
        self.assertEqual(list(res['ref']), ['item collections/shotgun_mp.item_collection'])
        self.assertEqual(list(res['field']), ['weapons/shotgun/shotgun.weapon'])
        self.assertEqual(res['failed'], 3)

    def test_non_gameplay_groups_skipped(self):
        res = self.run_gate()
        self.assertNotIn('sound/a.sound', res['field'])

    def test_allowed_tags_do_not_fail(self):
        res = self.run_gate(allowed=['weapons/gone/gone.weapon',
                                     'item collections/shotgun_mp.item_collection',
                                     'weapons/shotgun/shotgun.weapon'])
        self.assertEqual(res['failed'], 0)
        self.assertEqual(res['allowed'], 3)

    def test_repaired_index_fields_are_not_counted_for_listed_tags(self):
        def dump(root, tag):
            return ['location (%d)  index' % (1 if root.endswith('src') else 65535), 'rounds (3)  int16']
        res = fd.compare_trees(FIX + '/src', FIX + '/conv', dump, fixed=['weapons/ok/ok.weapon'])
        self.assertNotIn('weapons/ok/ok.weapon', res['field'])
        self.assertIn('weapons/shotgun/shotgun.weapon', res['field'])
        self.assertEqual(sorted(res['exempted']), ['weapons/ok/ok.weapon'])
        self.assertEqual(len(res['exempted']['weapons/ok/ok.weapon']), 1)

    def test_scenario_renamed_by_the_build_is_paired(self):
        with tempfile.TemporaryDirectory() as d:
            for side, name in (('src', 'levels/x/map mcc pm.scenario'), ('conv', 'levels/x/map.scenario')):
                os.makedirs(os.path.join(d, side, 'levels/x'))
                with open(os.path.join(d, side, name), 'w', encoding='utf-8') as f:
                    f.write('type (1)  enum\n')
            res = fd.compare_trees(d + '/src', d + '/conv', fixture_dump)
            self.assertEqual((res['same'], res['failed']), (1, 0))

    def test_listed_tag_with_a_real_index_change_fails(self):
        def dump(root, tag):
            return ['magazine (%d)  index' % (2 if root.endswith('src') else 5)]
        res = fd.compare_trees(FIX + '/src', FIX + '/conv', dump, fixed=['weapons/ok/ok.weapon'])
        self.assertIn('weapons/ok/ok.weapon', res['field'])
        self.assertNotIn('weapons/ok/ok.weapon', res['exempted'])

    def test_scenario_is_not_paired_when_the_source_folder_holds_two(self):
        with tempfile.TemporaryDirectory() as d:
            for side, names in (('src', ('levels/x/a.scenario', 'levels/x/b.scenario')), ('conv', ('levels/x/map.scenario',))):
                os.makedirs(os.path.join(d, side, 'levels/x'))
                for name in names:
                    with open(os.path.join(d, side, name), 'w', encoding='utf-8') as f:
                        f.write('type (1)  enum\n')
            res = fd.compare_trees(d + '/src', d + '/conv', fixture_dump)
            self.assertEqual(len(res['missing']), 2)

    def test_failed_dump_is_an_error(self):
        def dump(root, tag):
            if tag.endswith('shotgun.weapon') and root.endswith('conv'):
                raise fd.DumpError('invader-edit exit 1')
            return fixture_dump(root, tag)
        res = fd.compare_trees(FIX + '/src', FIX + '/conv', dump)
        self.assertIn('weapons/shotgun/shotgun.weapon', res['error'])
        self.assertNotIn('weapons/shotgun/shotgun.weapon', res['field'])
        self.assertEqual(res['failed'], 3)

    def test_empty_dump_is_an_error_even_when_both_sides_are_empty(self):
        res = fd.compare_trees(FIX + '/src', FIX + '/src', lambda root, tag: [])
        self.assertEqual(len(res['error']), 4)
        self.assertEqual(res['same'], 0)
        self.assertEqual(res['failed'], 4)

    def test_tree_against_itself_passes(self):
        res = fd.compare_trees(FIX + '/src', FIX + '/src', fixture_dump)
        self.assertEqual(res['failed'], 0)
        self.assertEqual(res['same'], 4)


if __name__ == '__main__':
    unittest.main()
