"""Tests for the halo-nav tools (tools/halo_mcp/nav.py) on small synthetic dumps."""
import json
import math
import os
import tempfile
import unittest
from pathlib import Path

from tools.halo_mcp import core, nav


def quad(i, x0, y0, z00, x1, y1, z11, n=(0, 0, 1), walk=True, piece=0, mat=0, corners=None):
    v = corners or [[x0, y0, z00], [x1, y0, z00], [x1, y1, z11], [x0, y1, z11]]
    return {'i': i, 'v': v, 'n': list(n), 'walk': walk, 'piece': piece, 'mat': mat}


def plane(size=100.0, step=25.0):
    out = []
    k = 0
    x = 0.0
    while x < size:
        y = 0.0
        while y < size:
            out.append(quad(k, x, y, 0.0, x + step, y + step, 0.0))
            k += 1
            y += step
        x += step
    return out


def dump(name, surfaces, objects=None, vehicles=None, h='1a2b3c4d'):
    return {'version': 1, 'file': name, 'level': name + '@src', 'hash': h,
            'bounds': [[0, 0, -1], [100, 100, 20]], 'surfaces': surfaces, 'pieces': {'0': len(surfaces)},
            'objects': objects or [], 'vehicles': vehicles or [], 'flags': [{'type': 'ctf_flag', 'team': 0,
                                                                              'p': [5, 5, 0]}],
            'items': [{'name': 'weapons/pistol', 'p': [10, 10, 0]}], 'spawns': [{'team': 0, 'p': [20, 20, 0],
                                                                                'yaw': 0.0}]}


def ramp(deg):
    """Flat at z=0 for x<40, a ramp from x=40..60 up to a plateau, flat plateau for x>60."""
    rise = 20.0 * math.tan(math.radians(deg))
    nz, nx = math.cos(math.radians(deg)), -math.sin(math.radians(deg))
    s = []
    k = 0
    for y in (0.0, 50.0):
        s.append(quad(k, 0, y, 0, 40, y + 50, 0)); k += 1
        s.append(quad(k, 0, 0, 0, 0, 0, 0, n=(nx, 0, nz),
                      corners=[[40, y, 0], [60, y, rise], [60, y + 50, rise], [40, y + 50, 0]])); k += 1
        s.append(quad(k, 60, y, rise, 100, y + 50, rise)); k += 1
    return s, rise


def wall():
    """Flat plane with a wall across x=50 from y=0 to y=80 (gap at y 80..100)."""
    s = plane()
    s.append({'i': 900, 'v': [[50, 0, 0], [50, 80, 0], [50, 80, 6], [50, 0, 6]], 'n': [1, 0, 0], 'walk': False,
              'piece': -1, 'mat': 1})
    return s


def dip_surfaces():
    """z=0 for x<40, a pit floor at z=-2.5 for 40<x<60 (vertical sides left out), z=0 beyond."""
    return [quad(0, 0, 0, 0, 40, 100, 0), quad(1, 40, 0, -2.5, 60, 100, -2.5), quad(2, 60, 0, 0, 100, 100, 0)]


def curb_surfaces():
    """z=0 for x<50, a plateau 0.6 higher for x>=50 (a curb)."""
    return [quad(0, 0, 0, 0, 50, 100, 0), quad(1, 50, 0, 0.6, 100, 100, 0.6)]


GOOD = """nav 1 flat
made 1a2b3c4d
route loop1 warthog loop
point 10 10 0
point 90 10 0 speed 0.8
point 90 90 0 turn 5
point 10 90 0 wait 2
end
route cross ghost,warthog oneway
point 12 10 0
point 50 90 0
end
keepout 50 50 0 radius 4
link walk 10 10 0 to 20 20 0
avoid 70 70 0 radius 3
"""


class NavTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        root = Path(cls.tmp.name).resolve()
        cls.geo = root / 'geo'
        cls.out = root / 'out'
        cls.geo.mkdir()
        ramp20, cls.rise20 = ramp(20)
        ramp40, _ = ramp(40)
        files = {
            'flat': dump('flat', plane(), vehicles=[{'name': 'vehicles/warthog', 'class': 'warthog',
                                                     'p': [30, 30, 0], 'yaw': 90.0}]),
            'ramp': dump('ramp', ramp20),
            'steep': dump('steep', ramp40),
            'wall': dump('wall', wall()),
            'rock': dump('rock', plane(), objects=[{'kind': 'scenery', 'name': 'scenery/rock', 'p': [50, 50, 4],
                                                    'r': 4.0}]),
            'trunk': dump('trunk', plane(), objects=[{'kind': 'scenery', 'name': 'scenery/mp_tree_pine_small',
                                                      'p': [50, 50, 3], 'r': 2.9}]),
            'dip': dump('dip', dip_surfaces(), objects=[{'kind': 'scenery', 'name': 'scenery/rock',
                                                         'p': [50, 50, -3.0], 'r': 1.0}]),
            'curb': dump('curb', curb_surfaces()),
            'pine': dump('pine', plane(), objects=[{'kind': 'scenery', 'name': 'scenery/mp_tree_pine_tall',
                                                    'p': [50, 50, 5], 'r': 7.7}]),
        }
        for name, d in files.items():
            (cls.geo / (name + '.json')).write_text(json.dumps(d))
        cls.saved = (core.READ_ROOTS, core.WRITE_ROOTS, os.environ.get('HALO_NAV_GEO'), os.environ.get('HALO_NAV_OUT'))
        core.READ_ROOTS = [root]
        core.WRITE_ROOTS = [root]
        os.environ['HALO_NAV_GEO'] = str(cls.geo)
        os.environ['HALO_NAV_OUT'] = str(cls.out)

    @classmethod
    def tearDownClass(cls):
        core.READ_ROOTS, core.WRITE_ROOTS = cls.saved[0], cls.saved[1]
        for k, v in (('HALO_NAV_GEO', cls.saved[2]), ('HALO_NAV_OUT', cls.saved[3])):
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        cls.tmp.cleanup()

    # ---- tools

    def test_maps(self):
        rows = {r['file']: r for r in nav.nav_maps()}
        self.assertEqual(set(rows), {'flat', 'ramp', 'steep', 'wall', 'rock', 'pine', 'trunk', 'dip', 'curb'})
        self.assertEqual(rows['flat']['surfaces'], 16)
        self.assertEqual(rows['flat']['level'], 'flat@src')
        self.assertEqual(rows['flat']['hash'], '1a2b3c4d')

    def test_points(self):
        r = nav.nav_points('flat')
        self.assertEqual(r['vehicles'][0]['class'], 'warthog')
        self.assertEqual(len(r['flags']), 1)
        self.assertEqual(len(r['spawns']), 1)
        self.assertEqual(r['bounds'][1][0], 100)

    def test_cache_reloads_on_change(self):
        a = nav.load('flat')
        self.assertIs(a, nav.load('flat'))
        p = self.geo / 'flat.json'
        st = p.stat()
        os.utime(p, ns=(st.st_atime_ns, st.st_mtime_ns + 5_000_000_000))
        self.assertIsNot(a, nav.load('flat'))

    def test_bad_file_names(self):
        for bad in ('../x', 'a/b', '', '.hidden'):
            with self.assertRaises(core.ToolError):
                nav.nav_points(bad)
        with self.assertRaises(core.ToolError):
            nav.nav_points('missing')

    def test_ground_flat_and_ramp(self):
        g = nav.nav_ground('flat', 33, 41)
        self.assertEqual(len(g), 1)
        self.assertAlmostEqual(g[0]['z'], 0.0)
        self.assertTrue(g[0]['walk'])
        self.assertAlmostEqual(g[0]['slope'], 0.0)
        r = nav.nav_ground('ramp', 50, 25)
        self.assertAlmostEqual(r[0]['z'], self.rise20 / 2, places=2)
        self.assertAlmostEqual(r[0]['slope'], 20.0, places=1)
        self.assertEqual(nav.nav_ground('flat', 500, 500), [])
        # on a shared edge a floor is still found
        self.assertEqual(len(nav.nav_ground('flat', 25, 25)) >= 1, True)

    def test_clear_flat_and_wall(self):
        self.assertEqual(nav.nav_clear('flat', [10, 10, 0], [90, 90, 0]), {'clear': True})
        hit = nav.nav_clear('wall', [10, 40, 0], [90, 40, 0])
        self.assertFalse(hit['clear'])
        self.assertEqual(hit['kind'], 'surface')
        self.assertEqual(hit['surface'], 900)
        self.assertAlmostEqual(hit['point'][0], 50.0, places=2)
        self.assertEqual(nav.nav_clear('wall', [10, 90, 0], [90, 90, 0]), {'clear': True})
        # width: a body passing 1 unit from the wall end is clear thin, blocked when 2 units wide
        self.assertTrue(nav.nav_clear('wall', [10, 81, 0], [90, 81, 0])['clear'])
        self.assertFalse(nav.nav_clear('wall', [10, 81, 0], [90, 81, 0], 1.0, 2.0)['clear'])
        # above the wall top (6) it is clear
        self.assertTrue(nav.nav_clear('wall', [10, 40, 7], [90, 40, 7], 1.0)['clear'])

    def test_clear_object(self):
        hit = nav.nav_clear('rock', [10, 50, 0], [90, 50, 0])
        self.assertFalse(hit['clear'])
        self.assertEqual(hit['kind'], 'object')
        self.assertEqual(hit['object'], 'scenery/rock')
        self.assertAlmostEqual(hit['point'][0], 46.0, places=1)
        self.assertTrue(nav.nav_clear('rock', [10, 60, 0], [90, 60, 0])['clear'])

    def test_clear_tree_trunk_only(self):
        # (a tree's dump r is its canopy: only the trunk, 0.08 of it, blocks)
        self.assertAlmostEqual(nav.solid_radius('a/mp_tree_pine_tall', 7.7), 0.616, places=3)
        self.assertEqual(nav.solid_radius('a/mp_tree_pine_small', 2.9), 0.4)
        self.assertEqual(nav.solid_radius('a/boulder_granite', 1.3), 1.3)
        self.assertFalse(nav.nav_clear('pine', [10, 50, 0], [90, 50, 0])['clear'])
        self.assertTrue(nav.nav_clear('pine', [10, 51, 0], [90, 51, 0])['clear'])
        # (its origin is up in the canopy: the trunk still blocks at the ground, 5 below it)
        self.assertEqual(nav.solid_span('a/mp_tree_pine_tall', 5.0, 7.7, 0.616), (5.0 - 7.7, 5.0 + 7.7))
        self.assertEqual(nav.solid_span('a/rock', 4.0, 4.0, 4.0), (-4.0, 12.0))

    def test_clear_bad_args(self):
        with self.assertRaises(core.ToolError):
            nav.nav_clear('flat', [1, 2], [3, 4, 5])
        with self.assertRaises(core.ToolError):
            nav.nav_clear('flat', [float('nan'), 0, 0], [1, 1, 1])

    def test_corridor_flat_straight(self):
        r = nav.nav_corridor('flat', [10, 10, 0], [90, 10, 0], 'warthog')
        self.assertTrue(r['found'])
        self.assertEqual(len(r['points']), 2)
        self.assertEqual(r['points'][0]['p'], [10, 10, 0])
        self.assertEqual(r['points'][-1]['p'], [90, 10, 0])
        self.assertEqual(r['text'].count('point'), 2)

    def test_corridor_diagonal_and_corners(self):
        r = nav.nav_corridor('flat', [10, 10, 0], [90, 70, 0], 'warthog')
        self.assertEqual(len(r['points']), 2)
        self.assertTrue(all(p['speed'] == 1.0 and p['turn'] == 0.0 for p in r['points']))
        # a path with one real corner: around the rock, the result is few points and only corners slow down
        r = nav.nav_corridor('rock', [10, 50, 0], [90, 50, 0], 'warthog')
        pts = r['points']
        self.assertLessEqual(len(pts), 5)
        self.assertEqual((pts[0]['speed'], pts[-1]['speed']), (1.0, 1.0))
        for p in pts:
            self.assertTrue(p['speed'] == 1.0 or p['turn'] > 0)

    def test_corridor_around_wall(self):
        r = nav.nav_corridor('wall', [10, 40, 0], [90, 40, 0], 'warthog')
        pts = r['points']
        self.assertGreater(len(pts), 2)
        # it has to pass the gap at y > 80 (+ half-width)
        self.assertGreater(max(p['p'][1] for p in pts), 81.0)
        for p in pts[1:-1]:
            self.assertLess(p['speed'], 1.0 + 1e-9)
        self.assertTrue(any(p['speed'] < 1.0 for p in pts))
        # every segment of the result is clear for the vehicle
        g = nav.load('wall')
        for a, b in zip(pts, pts[1:]):
            self.assertIsNone(nav.clear_check(g, a['p'], b['p'], 0.8, 1.0))

    def test_corridor_around_rock(self):
        r = nav.nav_corridor('rock', [10, 50, 0], [90, 50, 0], 'scorpion')
        pts = r['points']
        self.assertGreater(len(pts), 2)
        for p in pts:
            self.assertGreaterEqual(math.hypot(p['p'][0] - 50, p['p'][1] - 50), 4.0 + 2.2 - 1.0)

    def test_corridor_ramp_and_steep(self):
        r = nav.nav_corridor('ramp', [10, 25, 0], [80, 25, self.rise20], 'warthog')
        self.assertAlmostEqual(r['points'][-1]['p'][2], self.rise20, places=1)
        with self.assertRaises(core.ToolError):
            nav.nav_corridor('steep', [10, 25, 0], [80, 25, 16.8], 'warthog')

    def test_corridor_errors(self):
        with self.assertRaises(core.ToolError):
            nav.nav_corridor('flat', [10, 10, 0], [500, 500, 0])
        with self.assertRaises(core.ToolError):
            nav.nav_corridor('flat', [10, 10, 0], [20, 20, 0], 'tank')
        with self.assertRaises(core.ToolError):
            nav.nav_corridor('wall', [10, 10, 30], [20, 20, 30])  # no ground near the start

    def test_keepouts_rock_and_wall(self):
        r = nav.nav_keepouts('rock')
        self.assertEqual(r['count'], 1)
        k = r['keepouts'][0]
        self.assertAlmostEqual(k[0], 50.0)
        self.assertAlmostEqual(k[3], 4.0)
        self.assertTrue(r['text'].startswith('keepout 50 50 4 radius 4'))
        w = nav.nav_keepouts('wall', 'warthog', 3.0)
        self.assertGreaterEqual(w['count'], 1)
        for k in w['keepouts']:
            self.assertAlmostEqual(k[0], 50.0, delta=1.0)
            self.assertLessEqual(k[3], 12.0)
        self.assertEqual(nav.nav_keepouts('flat')['count'], 0)

    def test_keepouts_merge(self):
        c = nav._merge_circles([[0, 0, 0, 3], [4, 0, 0, 3], [100, 0, 0, 2]])
        self.assertEqual(len(c), 2)
        self.assertAlmostEqual(c[0][3], 5.0)
        self.assertEqual(len(nav._merge_circles([[0, 0, 0, 8], [10, 0, 0, 8]])), 2)  # merged radius over 12

    # ---- parser

    def test_parse_good(self):
        rf = nav.parse_route(GOOD)
        self.assertEqual(rf.errors, [])
        self.assertEqual(rf.file, 'flat')
        self.assertEqual(rf.made, ['1a2b3c4d'])
        self.assertEqual([r.name for r in rf.routes], ['loop1', 'cross'])
        self.assertTrue(rf.routes[0].loop)
        self.assertTrue(rf.routes[1].oneway)
        self.assertEqual(rf.routes[1].kinds, ['ghost', 'warthog'])
        p = rf.routes[0].points
        self.assertEqual((p[1]['speed'], p[2]['turn'], p[3]['wait']), (0.8, 5.0, 2.0))
        self.assertEqual(len(rf.links), 1)
        self.assertEqual(len(rf.avoids), 1)
        self.assertEqual(rf.keepouts[0]['r'], 4.0)

    def errs(self, text):
        return nav.parse_route(text).errors

    def test_parse_bad_lines(self):
        e = self.errs('nav 1 flat\nbogus 1 2 3\n')
        self.assertEqual(e[0][0], 2)
        e = self.errs('nav 2 flat\n')
        self.assertEqual(e[0][0], 1)
        e = self.errs('# c\n\nroute a warthog\n')
        self.assertEqual(e[0][0], 3)
        e = self.errs('nav 1 flat\npoint 1 2 3\n')
        self.assertEqual(e, [(2, 'point outside a route')])
        e = self.errs('nav 1 flat\nroute a warthog\npoint 1 2\nend\n')
        self.assertEqual(e[0][0], 3)
        self.assertEqual(e[1][0], 4)  # the route then has too few points
        e = self.errs('nav 1 flat\nroute a warthog\npoint 1 2 x\npoint 1 2 3\npoint 4 5 6\nend\n')
        self.assertEqual([x[0] for x in e], [3])
        e = self.errs('nav 1 flat\nroute A warthog\nend\n')
        self.assertEqual(e[0][0], 2)
        e = self.errs('nav 1 flat\nroute a boat\n')
        self.assertEqual(e[0][0], 2)
        e = self.errs('nav 1 flat\nkeepout 1 2 3 radius 0\nkeepout 1 2 3 radius nan\nkeepout 1 2 3 r 4\n')
        self.assertEqual([x[0] for x in e], [2, 3, 4])
        e = self.errs('nav 1 flat\nlink fly 0 0 0 to 1 1 1\nlink walk 0 0 0 to 1 1 1 twoway\nmade xyz\n')
        self.assertEqual([x[0] for x in e], [2, 3, 4])
        e = self.errs('nav 1 flat\nroute a warthog\npoint 1 2 3 speed 2\npoint 1 2 3 turn 31\n'
                      'point 1 2 3 wait 11\npoint 1 2 3 mood 1\npoint 1 2 3 speed\nend\n')
        self.assertEqual([x[0] for x in e[:5]], [3, 4, 5, 6, 7])
        e = self.errs('nav 1 flat\nroute a warthog\npoint 1 2 3\npoint 1 2 4\n')
        self.assertEqual(e, [(2, 'route a has no end')])
        e = self.errs('')
        self.assertEqual(e[0][0], 1)

    def test_parse_tabs_comments(self):
        rf = nav.parse_route('nav\t1\tflat # x\nroute\ta\twarthog\n point 1\t2 3 # c\npoint 4 5 6\nend\n')
        self.assertEqual(rf.errors, [])
        self.assertEqual(len(rf.routes[0].points), 2)

    def test_limit_route_points(self):
        pts = ''.join('point %d 0 0\n' % i for i in range(130))
        rf = nav.parse_route('nav 1 flat\nroute a warthog\n' + pts + 'end\n')
        self.assertEqual(len(rf.routes[0].points), 128)
        self.assertEqual([e[0] for e in rf.errors], [131, 132])

    def test_limit_total_points(self):
        # 9 routes of 128 points = 1152; the file holds at most 1024 points
        text = 'nav 1 flat\n'
        for r in range(9):
            text += 'route r%d warthog\n' % r + ''.join('point %d %d 0\n' % (i, r) for i in range(128)) + 'end\n'
        rf = nav.parse_route(text)
        self.assertEqual(sum(len(r.points) for r in rf.routes), 1024)
        self.assertEqual(len(rf.errors), 129)  # 128 refused points, then the empty route
        self.assertIn('1024', rf.errors[0][1])

    def test_limit_routes(self):
        text = 'nav 1 flat\n' + ''.join('route r%d warthog\npoint 0 0 0\npoint 1 0 0\nend\n' % i for i in range(33))
        rf = nav.parse_route(text)
        self.assertEqual(len(rf.routes), 32)
        self.assertEqual(len(rf.errors), 1)
        self.assertEqual(rf.errors[0][0], 2 + 32 * 4)  # only the route line is reported

    def test_limits_keepouts_links_avoids(self):
        rf = nav.parse_route('nav 1 flat\n' + 'keepout 0 0 0 radius 1\n' * 65)
        self.assertEqual((len(rf.keepouts), len(rf.errors), rf.errors[0][0]), (64, 1, 66))
        rf = nav.parse_route('nav 1 flat\n' + 'link walk 0 0 0 to 1 1 1\n' * 65)
        self.assertEqual((len(rf.links), len(rf.errors)), (64, 1))
        rf = nav.parse_route('nav 1 flat\n' + 'avoid 0 0 0 radius 1\n' * 33)
        self.assertEqual((len(rf.avoids), len(rf.errors)), (32, 1))

    def test_duplicate_route_name(self):
        rf = nav.parse_route('nav 1 flat\nroute a warthog\npoint 0 0 0\npoint 1 0 0\nend\n'
                             'route a ghost\npoint 0 0 0\npoint 1 0 0\nend\n')
        self.assertEqual(len(rf.routes), 1)
        self.assertEqual([e[0] for e in rf.errors], [6])

    def test_junctions(self):
        # five routes pass within 4 units of one point of route a: only its 4 nearest are joined
        text = 'nav 1 flat\nroute a warthog\npoint 0 0 0\npoint 50 0 0\nend\n'
        for k in range(5):
            text += 'route b%d warthog\npoint 0 %g 0\npoint 0 %g 30\nend\n' % (k, 0.5 * (k + 1), 0.5 * (k + 1))
        rf = nav.parse_route(text)
        self.assertEqual(rf.errors, [])
        j = nav.junctions(rf)
        self.assertEqual(len(j[(0, 0)]), 4)
        self.assertEqual([r for r, _ in j[(0, 0)]], [1, 2, 3, 4])  # nearest four
        self.assertIn((0, 0), j[(1, 0)])  # joined both ways
        self.assertNotIn((0, 1), j)
        # beyond 4.0 units: not joined
        rf = nav.parse_route('nav 1 flat\nroute a warthog\npoint 0 0 0\npoint 9 0 0\nend\n'
                             'route b warthog\npoint 0 4.5 0\npoint 9 9 0\nend\n')
        self.assertEqual(nav.junctions(rf), {})
        # same route points are never joined
        rf = nav.parse_route('nav 1 flat\nroute a warthog\npoint 0 0 0\npoint 1 0 0\nend\n')
        self.assertEqual(nav.junctions(rf), {})

    # ---- sweep

    def test_sweep_trunk_between_old_rays(self):
        # trunk radius 0.4 at (50,50); a warthog (1.3) passing 0.65 off it clips it, rays 1.3 apart missed it
        self.assertEqual(nav.sweep_segment(nav.load('flat'), (10, 50.65, 0), (90, 50.65, 0), 'warthog'), None)
        g = nav.load('trunk')
        hit = nav.sweep_segment(g, (10, 50.65, 0), (90, 50.65, 0), 'warthog')
        self.assertEqual(hit['kind'], 'object')
        self.assertEqual(hit['what'], 'scenery/mp_tree_pine_small')
        self.assertAlmostEqual(hit['at'][0], 50.0, delta=2.0)
        self.assertGreater(hit['gap'], 0)
        r = nav.nav_validate('trunk', 'nav 1 trunk\nroute a warthog\npoint 10 50.65 0\npoint 90 50.65 0\nend\n')
        self.assertFalse(r['ok'])
        self.assertTrue(any('seg 0-1' in e and 'hits object mp_tree_pine_small' in e for e in r['errors']))
        self.assertEqual(r['sweep'], {'segments': 1, 'failed': 1})
        # a ghost (1.0) passing the same line: 0.65 < 0.4 + 1.0, also hits; 1.6 off is clear
        self.assertIsNone(nav.sweep_segment(g, (10, 52, 0), (90, 52, 0), 'warthog'))

    def test_sweep_rock_in_dip(self):
        g = nav.load('dip')
        # both ends at z=0 (ground ends at the pit edges); the pit rock spans z -4..-2, standing on the pit floor at -2.5
        hit = nav.sweep_segment(g, (35, 50, 0), (65, 50, 0), 'warthog')
        self.assertIsNotNone(hit)
        self.assertEqual(hit['kind'], 'object')
        # the old straight line 0.8 above the ends floats over the rock
        self.assertIsNone(nav.clear_check(g, (35, 50, 0), (65, 50, 0), 0.8, 1.3))

    def test_sweep_curb_and_drop(self):
        g = nav.load('curb')
        hit = nav.sweep_segment(g, (30, 50, 0), (70, 50, 0.6), 'warthog')
        self.assertEqual(hit['kind'], 'step')
        self.assertAlmostEqual(hit['what'], 0.6, places=2)
        self.assertAlmostEqual(hit['at'][0], 50.0, delta=0.6)
        # driving off the curb is fine
        self.assertIsNone(nav.sweep_segment(g, (70, 50, 0.6), (30, 50, 0), 'warthog'))
        r = nav.nav_validate('curb', 'nav 1 curb\nroute a warthog\npoint 30 50 0\npoint 70 50 0.6\nend\n')
        self.assertTrue(any('a step of' in e for e in r['errors']))

    def test_sweep_flat_and_wall_and_ground(self):
        self.assertIsNone(nav.sweep_segment(nav.load('flat'), (10, 10, 0), (90, 90, 0), 'scorpion'))
        hit = nav.sweep_segment(nav.load('wall'), (10, 40, 0), (90, 40, 0), 'warthog')
        self.assertEqual((hit['kind'], hit['what']), ('wall', 900))
        self.assertAlmostEqual(hit['at'][0], 50.0 - 1.3, delta=0.6)
        hit = nav.sweep_segment(nav.load('flat'), (50, 50, 0), (150, 50, 0), 'warthog')
        self.assertEqual(hit['kind'], 'ground')

    def test_validate_keepout_warning(self):
        r = nav.nav_validate('flat', 'nav 1 flat\nkeepout 50 12 0 radius 3\nroute a warthog\npoint 10 10 0\n'
                                     'point 90 10 0\nend\n')
        self.assertTrue(r['ok'])
        self.assertTrue(any('keepout of line 2' in w for w in r['warnings']))

    def test_obstacle_sources(self):
        # none: the guess
        g = nav.Geo(dump('s', plane(), objects=[{'kind': 'scenery', 'name': 'scenery/mp_tree_pine_small',
                                                 'p': [50, 50, 3], 'r': 2.9}]))
        c = nav.obstacle_circles(g)
        self.assertEqual([(x[0], x[5], x[6]) for x in c], [('scenery/mp_tree_pine_small', 0.4, 'guess')])
        # a table in the geo folder: spheres placed at the origin; collision false drops the object
        with tempfile.TemporaryDirectory() as td:
            old = os.environ['HALO_NAV_GEO']
            root = Path(td).resolve()
            saved_roots = core.READ_ROOTS
            core.READ_ROOTS = list(saved_roots) + [root]
            os.environ['HALO_NAV_GEO'] = str(root)
            try:
                (root / 'footprints.json').write_text(json.dumps({'s': {
                    'scenery/mp_tree_pine_small': {'bounding_offset': [0, 0, 2], 'bounding_radius': 2.9,
                                                   'spheres': [[0.1, 0, 0.5, 0.3]], 'collision': True},
                    'scenery/bush': {'bounding_offset': [0, 0, 0], 'bounding_radius': 1, 'spheres': [],
                                     'collision': False}}}))
                d = dump('s', plane(), objects=[
                    {'kind': 'scenery', 'name': 'scenery/mp_tree_pine_small', 'p': [50, 50, 3], 'r': 2.9},
                    {'kind': 'scenery', 'name': 'scenery/bush', 'p': [60, 60, 0], 'r': 1.0},
                    {'kind': 'scenery', 'name': 'scenery/rock', 'p': [70, 70, 1], 'r': 1.0}])
                c = nav.obstacle_circles(nav.Geo(d))
                self.assertEqual([(x[0], x[6]) for x in c], [('scenery/mp_tree_pine_small', 'table'),
                                                             ('scenery/rock', 'guess')])
                self.assertAlmostEqual(c[0][1], 50.1)
                self.assertAlmostEqual(c[0][3], 1.0 + 0.5 - 0.3)
                self.assertAlmostEqual(c[0][5], 0.3)
                # the dump's own obstacles win over the table
                d['obstacles'] = [{'name': 'scenery/mp_tree_pine_small', 'p': [51, 50, 1], 'r': 0.5}]
                c = nav.obstacle_circles(nav.Geo(d))
                self.assertEqual(len(c), 1)
                self.assertEqual((c[0][6], c[0][1], c[0][3], c[0][4]), ('dump', 51.0, 0.5, 3.5))
            finally:
                os.environ['HALO_NAV_GEO'] = old
                core.READ_ROOTS = saved_roots

    # ---- validate / write

    def test_validate_good(self):
        r = nav.nav_validate('flat', GOOD)
        self.assertEqual(r['errors'], [])
        self.assertTrue(r['ok'])
        self.assertEqual(r['routes'], 2)
        self.assertGreaterEqual(r['junction_points'], 1)

    def test_validate_parse_errors_have_lines(self):
        r = nav.nav_validate('flat', 'nav 1 flat\nzzz\n')
        self.assertFalse(r['ok'])
        self.assertTrue(r['errors'][0].startswith('line 2:'))

    def test_validate_wrong_file_and_hash_warning(self):
        r = nav.nav_validate('flat', 'nav 1 other\nmade deadbeef\n')
        self.assertFalse(r['ok'])
        self.assertTrue(any('header names file other' in e for e in r['errors']))
        self.assertTrue(any('made hashes' in w for w in r['warnings']))

    def test_validate_no_ground(self):
        r = nav.nav_validate('flat', 'nav 1 flat\nroute a warthog\npoint 10 10 5\npoint 20 10 0\nend\n')
        self.assertFalse(r['ok'])
        self.assertTrue(any(e.startswith('line 3:') and 'no ground' in e for e in r['errors']))
        r = nav.nav_validate('flat', 'nav 1 flat\nroute a warthog\npoint 10 10 0\npoint 200 10 0\nend\n')
        self.assertTrue(any(e.startswith('line 4:') and 'no ground' in e for e in r['errors']))

    def test_validate_blocked_segment(self):
        r = nav.nav_validate('wall', 'nav 1 wall\nroute a warthog\npoint 10 40 0\npoint 90 40 0\nend\n')
        self.assertFalse(r['ok'])
        self.assertTrue(any(e.startswith('line 4:') and 'hits wall surface 900' in e for e in r['errors']))
        r = nav.nav_validate('rock', 'nav 1 rock\nroute a ghost\npoint 10 50 0\npoint 90 50 0\nend\n')
        self.assertTrue(any('hits object rock at' in e for e in r['errors']))

    def test_validate_wide_vehicle_blocked_narrow_ok(self):
        # passes 6 units from the rock centre: ghost (1.0) fits, scorpion (2.2 + 4.0) does not
        body = 'route a %s\npoint 10 56 0\npoint 90 56 0\nend\n'
        self.assertTrue(nav.nav_validate('rock', 'nav 1 rock\n' + body % 'ghost')['ok'])
        self.assertFalse(nav.nav_validate('rock', 'nav 1 rock\n' + body % 'scorpion')['ok'])

    def test_validate_slope(self):
        r = nav.nav_validate('steep', 'nav 1 steep\nroute a warthog\npoint 41 25 0\npoint 59 25 15\nend\n')
        self.assertFalse(r['ok'])
        self.assertTrue(any('slope' in e for e in r['errors']))
        _, rise = ramp(20)
        r = nav.nav_validate('ramp', 'nav 1 ramp\nroute a warthog\npoint 10 25 0\npoint 70 25 %g\nend\n' % rise)
        self.assertFalse(any('slope' in e for e in r['errors']))

    def test_validate_keepout_and_links(self):
        r = nav.nav_validate('flat', 'nav 1 flat\nkeepout 50 10 0 radius 3\nroute a warthog\npoint 10 10 0\n'
                                     'point 50 11 0\nend\n')
        self.assertTrue(any(e.startswith('line 5:') and 'keepout of line 2' in e for e in r['errors']))
        r = nav.nav_validate('flat', 'nav 1 flat\nlink jump 10 10 0 to 500 500 0\n')
        self.assertTrue(any(e.startswith('line 2:') and 'walk surface' in e for e in r['errors']))

    def test_validate_link_needs_walk(self):
        s = plane()
        s[0]['walk'] = False
        d = dump('nowalk', s)
        (self.geo / 'nowalk.json').write_text(json.dumps(d))
        r = nav.nav_validate('nowalk', 'nav 1 nowalk\nlink walk 5 5 0 to 60 60 0\n')
        self.assertFalse(r['ok'])
        self.assertEqual(len(r['errors']), 1)

    def test_write_ok_and_refused(self):
        res = nav.nav_write('flat', GOOD)
        path = Path(res['written'])
        self.assertEqual(path, (self.out / 'flat.txt').resolve())
        self.assertEqual(path.read_text(), GOOD)
        before = path.read_text()
        bad = 'nav 1 flat\nroute a warthog\npoint 10 10 0\npoint 10 10 9\nend\n'
        with self.assertRaises(core.ToolError) as cm:
            nav.nav_write('flat', bad)
        self.assertIn('refused', str(cm.exception))
        self.assertEqual(path.read_text(), before)  # the earlier file is untouched
        with self.assertRaises(core.ToolError):
            nav.nav_write('wall', 'nav 1 wall\nroute a warthog\npoint 10 40 0\npoint 90 40 0\nend\n')
        self.assertFalse((self.out / 'wall.txt').exists())

    def test_write_outside_root_refused(self):
        old = os.environ['HALO_NAV_OUT']
        os.environ['HALO_NAV_OUT'] = '/definitely/not/allowed'
        try:
            with self.assertRaises(core.ToolError):
                nav.nav_write('flat', GOOD)
        finally:
            os.environ['HALO_NAV_OUT'] = old

    def test_geo_env_required(self):
        old = os.environ.pop('HALO_NAV_GEO')
        try:
            with self.assertRaises(core.ToolError):
                nav.nav_maps()
        finally:
            os.environ['HALO_NAV_GEO'] = old


if __name__ == '__main__':
    unittest.main()
