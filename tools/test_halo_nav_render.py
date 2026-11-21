"""Tests for the nav picture tool (tools/halo_mcp/nav_render.py) on a small synthetic dump and log."""
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.halo_mcp import nav, nav_render

try:
    import matplotlib  # noqa: F401
    HAVE_MPL = True
except ImportError:
    HAVE_MPL = False


def quad(i, x0, y0, x1, y1, z=0.0, walk=True):
    return {'i': i, 'v': [[x0, y0, z], [x1, y0, z], [x1, y1, z], [x0, y1, z]], 'n': [0, 0, 1], 'walk': walk,
            'piece': 0, 'mat': 0}


def dump():
    s = [quad(k, x, y, x + 25, y + 25, z=x / 25.0) for k, (x, y) in enumerate(
        (x, y) for x in (0, 25, 50, 75) for y in (0, 25, 50, 75))]
    s.append({'i': 99, 'v': [[50, 0, 0], [50, 80, 0], [50, 80, 6], [50, 0, 6]], 'n': [1, 0, 0], 'walk': False,
              'piece': -1, 'mat': 1})
    return {'version': 1, 'file': 'toy', 'level': 'toy@src', 'hash': '1a2b3c4d', 'bounds': [[0, 0, -1], [100, 100, 20]],
            'surfaces': s,
            'objects': [{'kind': 'scenery', 'name': 'rocks/known_rock', 'p': [20.0, 20.0, 0.3], 'r': 2.0},
                        {'kind': 'scenery', 'name': 'trees/pine_tall', 'p': [40.0, 40.0, 5.0], 'r': 7.7},
                        {'kind': 'scenery', 'name': 'rocks/unlisted', 'p': [60.0, 60.0, 1.0], 'r': 1.5}],
            'vehicles': [{'name': 'v/mp_warthog', 'class': 'warthog', 'p': [10, 10, 0], 'yaw': 0.0}],
            'flags': [{'type': 'ctf_flag', 'team': 0, 'p': [5, 5, 0]}], 'items': [], 'spawns': []}


FOOT = {'toy': {'rocks/known_rock': {'bounding_offset': [0, 0, 0.3], 'bounding_radius': 2.0,
                                     'spheres': [[0, 0, 0.3, 1.2]], 'collision': True},
                'trees/pine_tall': {'bounding_offset': [0, 0, 5.0], 'bounding_radius': 7.7,
                                    'spheres': [[0, 0, 0.0, 0.5]], 'collision': True}}}

ROUTE = """nav 1 toy
keepout 20 20 0 radius 2
route lane warthog loop
point 10 10 0
point 40 10 0
point 40 40 0
end
"""

LOG = """x: bot nav: toy (built in): 1 routes
x: bots: ev=drive_crash bot=8 t=1 class=a speed 6 -> 0 ahead 1 at (20.5 20.5 0.0) hit=rock
x: bots: ev=drive_wedge bot=8 t=2 class=a kind=early at (21.0 21.0 0.0) new_goal 0
x: bots: ev=drive_wedge_other at (1 1 1)
x: bots: ev=drive_reverse bot=1 at (60.0 60.0 1.0)
x: bots: ev=drive_dodge bot=1 at (60.0 60.0 1.0)
x: ae obj: t=0 ev=game_start map=other_dig teams=1
x: bots: ev=drive_stuck bot=3 at (5.0 5.0 0.0)
x: ae obj: t=0 ev=game_start map=toy@mcc teams=1
x: bots: ev=drive_crash bot=2 at (21.5 21.5 0.0)
"""


class ParseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        (self.dir / 'toy.json').write_text(json.dumps(dump()))
        (self.dir / 'footprints.json').write_text(json.dumps(FOOT))
        self.log = self.dir / 'debug.txt'
        self.log.write_text(LOG)

    def tearDown(self):
        self.tmp.cleanup()

    def test_events_by_map(self):
        ev = nav_render.parse_events(self.log, {'toy'})
        self.assertEqual(sorted(ev), ['other_dig', 'toy'])
        self.assertEqual([e['kind'] for e in ev['toy']], ['crash', 'wedge', 'reverse', 'crash'])
        self.assertEqual([e['kind'] for e in ev['other_dig']], ['stuck'])
        first = ev['toy'][0]
        self.assertEqual((first['x'], first['y'], first['z'], first['hit']), (20.5, 20.5, 0.0, 'rock'))
        self.assertIsNone(ev['toy'][1]['hit'])

    def test_suffix_stripped_only_with_dump(self):
        self.assertEqual(nav_render.plain_map('toy@mcc', {'toy'}), 'toy')
        self.assertEqual(nav_render.plain_map('toy_dig', {'toy'}), 'toy')
        self.assertEqual(nav_render.plain_map('birdcage_dig', {'toy'}), 'birdcage_dig')

    def test_dense_cells(self):
        ev = nav_render.parse_events(self.log, {'toy'})['toy']
        rows = nav_render.dense_cells(ev, top=2)
        self.assertEqual(rows[0][0], 3)
        self.assertEqual((rows[0][1], rows[0][2]), (20.0, 20.0))
        self.assertEqual(rows[0][3], {'crash': 2, 'wedge': 1})

    def test_circles_from_table(self):
        with mock.patch.dict(os.environ, {'HALO_NAV_GEO': str(self.dir)}):
            circles = nav_render.circles_for(dump(), self.dir)
        by = {c[0]: c for c in circles}
        rock = by['rocks/known_rock']
        self.assertEqual((rock[1], rock[2], rock[5], rock[6]), (20.0, 20.0, 1.2, 'table'))
        tree = by['trees/pine_tall']
        self.assertAlmostEqual(tree[5], 0.5)
        self.assertEqual(tree[6], 'table')
        self.assertEqual(by['rocks/unlisted'][6], 'guess')

    def test_own_circles_match_origin_shift(self):
        circles = nav_render._own_circles(dump(), self.dir)
        tree = [c for c in circles if c[0] == 'trees/pine_tall'][0]
        # origin is the object position minus the bounding offset: the sphere sits at ground height
        self.assertAlmostEqual(tree[3], 0.0 - 0.5)
        self.assertAlmostEqual(tree[4], 0.0 + 0.5)

    @unittest.skipUnless(HAVE_MPL, 'matplotlib is not installed')
    def test_render(self):
        rf = nav.parse_route(ROUTE)
        self.assertEqual(rf.errors, [])
        ev = nav_render.parse_events(self.log, {'toy'})['toy']
        with mock.patch.dict(os.environ, {'HALO_NAV_GEO': str(self.dir)}):
            circles = nav_render.circles_for(dump(), self.dir)
        out = self.dir / 'p' / 'toy.png'
        path = nav_render.render('toy', dump(), out, route=rf, events=ev, circles=circles, size=600,
                                 nav_label='toy.txt')
        data = Path(path).read_bytes()
        self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
        self.assertGreater(len(data), 2000)
        zoom = nav_render.render('toy', dump(), self.dir / 'z.png', route=rf, events=ev, circles=circles,
                                 box=(10, 10, 30, 30), zband=(-1, 2), size=400)
        self.assertTrue(Path(zoom).is_file())


if __name__ == '__main__':
    unittest.main()
