"""Bot navigation helpers: read a map's geometry dump and place vehicle routes and keepouts.

All tool logic lives here as plain functions (no MCP needed). nav_server.py wraps them.
Folders come from two environment variables:
  HALO_NAV_GEO   folder of <file>.json geometry dumps (read)
  HALO_NAV_OUT   folder the route files <file>.txt are written to (default: <first write root>/bot_nav)
Paths go through core.safe_path, so the HALO_MCP_ROOTS / HALO_MCP_WRITE_ROOTS rules apply.
Standard library only.
"""
import heapq
import json
import math
import os
import re
from pathlib import Path

from . import core

ToolError = core.ToolError

# vehicle: (half-width, max slope in degrees, max step)
VEHICLES = {
    'warthog': (1.3, 32.0, 0.45),
    'ghost': (1.0, 35.0, 0.45),
    'scorpion': (2.2, 25.0, 0.6),
}
VEHICLES['any'] = VEHICLES['warthog']

# route file limits
MAX_ROUTES = 32
MAX_ROUTE_POINTS = 128
MAX_TOTAL_POINTS = 1024
MAX_KEEPOUTS = 64
MAX_LINKS = 64
MAX_AVOIDS = 32
JUNCTION_DIST = 4.0
JUNCTION_MAX = 4
GRID_CELL = 8.0
GROUND_TOL = 1.5

_NAME_RE = re.compile(r'^[a-z0-9_-]{1,31}$')
_FILE_RE = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_.-]{0,63}$')
_HEX8_RE = re.compile(r'^[0-9a-fA-F]{8}$')


def _vehicle(v):
    if v not in VEHICLES:
        raise ToolError('unknown vehicle %r (warthog ghost scorpion any)' % (v,))
    return VEHICLES[v]


def _file(name):
    if not isinstance(name, str) or not _FILE_RE.match(name) or '..' in name:
        raise ToolError('bad map file name %r' % (name,))
    return name


def _vec(v, what):
    try:
        if len(v) != 3:
            raise ValueError
        out = tuple(float(c) for c in v)
    except (TypeError, ValueError):
        raise ToolError('%s must be three numbers [x, y, z]' % what)
    if not all(math.isfinite(c) for c in out):
        raise ToolError('%s must be finite' % what)
    return out


# ---------------------------------------------------------------- geometry

class Poly:
    __slots__ = ('i', 'pts', 'n', 'd', 'x0', 'y0', 'x1', 'y1', 'z0', 'z1', 'walk', 'piece', 'mat', 'axis')

    def __init__(self, i, pts, n, walk, piece, mat):
        self.i = i
        self.pts = pts
        ln = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
        if ln < 1e-9:
            n = _newell(pts)
            ln = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) or 1.0
        self.n = (n[0] / ln, n[1] / ln, n[2] / ln)
        self.d = self.n[0] * pts[0][0] + self.n[1] * pts[0][1] + self.n[2] * pts[0][2]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        zs = [p[2] for p in pts]
        self.x0, self.x1, self.y0, self.y1, self.z0, self.z1 = min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)
        self.walk = walk
        self.piece = piece
        self.mat = mat
        a = [abs(c) for c in self.n]
        self.axis = a.index(max(a))


TREE_WORDS = ('tree', 'pine', 'palm', 'trunk')


def solid_radius(name, r):
    """The part of an object a vehicle hits. The dump's r is the object's bounding sphere: for a tree that is the
    canopy (a tall pine's is 7.7), and only the trunk is solid: 0.08 of it, 0.4 at least (a tall pine's 0.62)."""
    leaf = name.replace('\\', '/').rsplit('/', 1)[-1].lower()
    if any(w in leaf for w in TREE_WORDS):
        return min(r, max(0.4, r * 0.08))
    return r


def solid_span(name, z, r, solid):
    """The heights an object blocks between. A tree's origin sits up in its canopy (a tall pine's about 5 units above the
    ground), so its trunk reaches from the bottom of its bounding sphere to the top; any other object blocks within twice
    its radius of its origin."""
    if solid < r:
        return z - r, z + r
    return z - 2 * solid, z + 2 * solid


def _newell(pts):
    nx = ny = nz = 0.0
    for k in range(len(pts)):
        a, b = pts[k], pts[(k + 1) % len(pts)]
        nx += (a[1] - b[1]) * (a[2] + b[2])
        ny += (a[2] - b[2]) * (a[0] + b[0])
        nz += (a[0] - b[0]) * (a[1] + b[1])
    return (nx, ny, nz)


def _pip(px, py, pts2):
    """Point in polygon (even-odd) with a small edge tolerance. pts2 = [(u, v), ...]."""
    inside = False
    n = len(pts2)
    j = n - 1
    for k in range(n):
        xi, yi = pts2[k]
        xj, yj = pts2[j]
        if (yi > py) != (yj > py) and px < (xj - xi) * (py - yi) / (yj - yi) + xi:
            inside = not inside
        j = k
    if inside:
        return True
    j = n - 1
    for k in range(n):
        if _seg_dist2(px, py, pts2[j][0], pts2[j][1], pts2[k][0], pts2[k][1]) < 1e-8:
            return True
        j = k
    return False


def _seg_dist2(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    l2 = dx * dx + dy * dy
    t = 0.0 if l2 < 1e-12 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / l2))
    qx, qy = ax + t * dx - px, ay + t * dy - py
    return qx * qx + qy * qy


def _project(poly, drop):
    return [tuple(c for k, c in enumerate(p) if k != drop) for p in poly.pts]


class Geo:
    """A loaded dump with a 2D grid index of its polygons."""

    def __init__(self, data):
        if not isinstance(data, dict) or data.get('version') != 1:
            raise ToolError('geometry dump: version 1 expected')
        self.file = data.get('file')
        self.level = data.get('level')
        self.hash = data.get('hash')
        self.bounds = data.get('bounds')
        self.vehicles = data.get('vehicles') or []
        self.flags = data.get('flags') or []
        self.items = data.get('items') or []
        self.spawns = data.get('spawns') or []
        self.objects = []
        for o in data.get('objects') or []:
            try:
                name = str(o.get('name', ''))
                z, r = float(o['p'][2]), float(o['r'])
                solid = solid_radius(name, r)
                zlo, zhi = solid_span(name, z, r, solid)
                self.objects.append((name, float(o['p'][0]), float(o['p'][1]), z, solid, o.get('kind'), zlo, zhi))
            except (KeyError, TypeError, ValueError, IndexError):
                continue
        self.polys = []
        for s in data.get('surfaces') or []:
            try:
                pts = [(float(v[0]), float(v[1]), float(v[2])) for v in s['v']]
                if len(pts) < 3:
                    continue
                n = s.get('n') or [0.0, 0.0, 0.0]
                self.polys.append(Poly(int(s.get('i', len(self.polys))), pts, (float(n[0]), float(n[1]), float(n[2])),
                                       bool(s.get('walk')), int(s.get('piece', -1)), int(s.get('mat', 0))))
            except (KeyError, TypeError, ValueError, IndexError):
                continue
        self.grid = {}
        for k, p in enumerate(self.polys):
            self._register(self.grid, GRID_CELL, k, p.x0 - 1.0, p.y0 - 1.0, p.x1 + 1.0, p.y1 + 1.0)
        self._steep = {}
        self._circles = None
        self._circles_key = None
        self._raw_obstacles = data.get('obstacles')
        self._sweep_idx = {}

    @staticmethod
    def _register(grid, cell, key, x0, y0, x1, y1):
        for i in range(int(math.floor(x0 / cell)), int(math.floor(x1 / cell)) + 1):
            for j in range(int(math.floor(y0 / cell)), int(math.floor(y1 / cell)) + 1):
                grid.setdefault((i, j), []).append(key)

    def near_xy(self, x, y):
        return self.grid.get((int(math.floor(x / GRID_CELL)), int(math.floor(y / GRID_CELL))), ())

    def near_segment(self, a, b):
        """Polygon indices whose cells the xy projection of a..b passes through."""
        seen = set()
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        steps = max(1, int(length / (GRID_CELL * 0.5)) + 1)
        for s in range(steps + 1):
            t = s / steps
            seen.update(self.near_xy(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
        return seen

    def floors(self, x, y):
        out = []
        for k in self.near_xy(x, y):
            p = self.polys[k]
            if p.n[2] < 0.05 or x < p.x0 or x > p.x1 or y < p.y0 or y > p.y1:
                continue
            if _pip(x, y, [(q[0], q[1]) for q in p.pts]):
                z = (p.d - p.n[0] * x - p.n[1] * y) / p.n[2]
                out.append((z, p))
        return out

    def steep(self, vehicle):
        """Non-walkable polygons steeper than the vehicle's slope limit (walls, cliffs)."""
        if vehicle not in self._steep:
            c = math.cos(math.radians(VEHICLES[vehicle][1]))
            self._steep[vehicle] = [p for p in self.polys if not p.walk and abs(p.n[2]) < c]
        return self._steep[vehicle]

    def segment_hit(self, a, b):
        """First solid on a..b: (t, kind, ref) or None. kind is 'surface' or 'object'."""
        best = None
        d = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        for k in self.near_segment(a, b):
            p = self.polys[k]
            den = p.n[0] * d[0] + p.n[1] * d[1] + p.n[2] * d[2]
            if abs(den) < 1e-9:
                continue
            t = (p.d - p.n[0] * a[0] - p.n[1] * a[1] - p.n[2] * a[2]) / den
            if t < 0.0 or t > 1.0 or (best and t >= best[0]):
                continue
            hx, hy, hz = a[0] + d[0] * t, a[1] + d[1] * t, a[2] + d[2] * t
            if hx < p.x0 - 1e-4 or hx > p.x1 + 1e-4 or hy < p.y0 - 1e-4 or hy > p.y1 + 1e-4 \
                    or hz < p.z0 - 1e-4 or hz > p.z1 + 1e-4:
                continue
            h = (hx, hy, hz)
            drop = p.axis
            if _pip(*[c for q, c in enumerate(h) if q != drop], _project(p, drop)):
                best = (t, 'surface', p.i)
        for name, ox, oy, zlo, zhi, r, _src in obstacle_circles(self):
            t = _seg_cylinder(a, b, ox, oy, zlo, zhi, r)
            if t is not None and (not best or t < best[0]):
                best = (t, 'object', name)
        return best


def _seg_cylinder(a, b, cx, cy, zlo, zhi, r):
    """First parameter t in 0..1 where a..b is inside the vertical cylinder, or None."""
    t0, t1 = 0.0, 1.0
    dz = b[2] - a[2]
    if abs(dz) < 1e-9:
        if a[2] < zlo or a[2] > zhi:
            return None
    else:
        ta, tb = (zlo - a[2]) / dz, (zhi - a[2]) / dz
        if ta > tb:
            ta, tb = tb, ta
        t0, t1 = max(t0, ta), min(t1, tb)
        if t0 > t1:
            return None
    ax, ay = a[0] - cx, a[1] - cy
    dx, dy = b[0] - a[0], b[1] - a[1]
    qa = dx * dx + dy * dy
    qb = 2 * (ax * dx + ay * dy)
    qc = ax * ax + ay * ay - r * r
    if qa < 1e-12:
        return t0 if qc <= 0 else None
    disc = qb * qb - 4 * qa * qc
    if disc < 0:
        return None
    sq = math.sqrt(disc)
    lo = max((-qb - sq) / (2 * qa), t0)
    hi = min((-qb + sq) / (2 * qa), t1)
    return lo if lo <= hi else None


# ---------------------------------------------------------------- dump loading

_CACHE = {}


def geo_dir():
    d = os.environ.get('HALO_NAV_GEO', '')
    if not d:
        raise ToolError('HALO_NAV_GEO is not set (folder of <file>.json geometry dumps)')
    return core.safe_path(d)


def load(file):
    """Load (cached by path and mtime) the dump for a map file name."""
    _file(file)
    path = core.safe_path(str(geo_dir() / (file + '.json')))
    st = path.stat()
    key = str(path)
    hit = _CACHE.get(key)
    if hit and hit[0] == (st.st_mtime_ns, st.st_size):
        return hit[1]
    if st.st_size > core.MAX_MAP_BYTES:
        raise ToolError('dump too large: %s' % path.name)
    try:
        with open(path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except (OSError, ValueError) as e:
        raise ToolError('cannot read dump %s: %s' % (path.name, e))
    g = Geo(data)
    _CACHE[key] = ((st.st_mtime_ns, st.st_size), g)
    return g


def _slope(p):
    return math.degrees(math.acos(max(-1.0, min(1.0, p.n[2]))))


# ---------------------------------------------------------------- tools

def nav_maps():
    """The dumps found: file, level, hash, surface count."""
    d = geo_dir()
    out = []
    for f in sorted(d.glob('*.json')):
        name = f.stem
        try:
            g = load(name)
            out.append({'file': name, 'level': g.level, 'hash': g.hash, 'surfaces': len(g.polys)})
        except ToolError as e:
            out.append({'file': name, 'error': str(e)})
    return out


def nav_points(file):
    g = load(file)
    return {'file': file, 'bounds': g.bounds, 'vehicles': g.vehicles, 'flags': g.flags, 'items': g.items,
            'spawns': g.spawns}


def nav_ground(file, x, y, from_z=None):
    """Every floor at x y, top first (or nearest to from_z when given)."""
    g = load(file)
    rows = []
    for z, p in g.floors(float(x), float(y)):
        rows.append({'z': round(z, 3), 'surface': p.i, 'walk': p.walk, 'piece': p.piece,
                     'slope': round(_slope(p), 2), 'mat': p.mat})
    if from_z is None:
        rows.sort(key=lambda r: -r['z'])
    else:
        rows.sort(key=lambda r: abs(r['z'] - float(from_z)))
    return rows


def clear_check(g, a, b, height=1.0, radius=0.0):
    """Core of nav_clear on a loaded Geo. Returns None when clear, else a hit dict."""
    a = (a[0], a[1], a[2] + height)
    b = (b[0], b[1], b[2] + height)
    dx, dy = b[0] - a[0], b[1] - a[1]
    ln = math.hypot(dx, dy)
    px, py = (-dy / ln, dx / ln) if ln > 1e-9 else (1.0, 0.0)
    best = None
    offsets = (0.0,) if radius <= 0 else (0.0, -radius, radius)
    for off in offsets:
        sa = (a[0] + px * off, a[1] + py * off, a[2])
        sb = (b[0] + px * off, b[1] + py * off, b[2])
        h = g.segment_hit(sa, sb)
        if h and (not best or h[0] < best[0][0]):
            best = (h, sa, sb, off)
    if not best:
        return None
    (t, kind, ref), sa, sb, off = best
    pt = [round(sa[k] + (sb[k] - sa[k]) * t, 3) for k in range(3)]
    return {'clear': False, 'point': pt, 'kind': kind, 'surface' if kind == 'surface' else 'object': ref,
            't': round(t, 4), 'offset': off}


def nav_clear(file, a, b, height=1.0, radius=0.0):
    g = load(file)
    hit = clear_check(g, _vec(a, 'a'), _vec(b, 'b'), float(height), max(0.0, float(radius)))
    return hit or {'clear': True}


class _Blocker:
    """Cells near steep surfaces or objects, for one search area and one vehicle."""

    CELL = 4.0

    def __init__(self, g, vehicle, box):
        hw, maxslope, _ = VEHICLES[vehicle]
        self.hw = hw
        x0, y0, x1, y1 = box
        self.objs = [(o[1], o[2], o[3], o[4], o[5]) for o in obstacle_circles(g)
                     if x0 - o[5] - hw <= o[1] <= x1 + o[5] + hw and y0 - o[5] - hw <= o[2] <= y1 + o[5] + hw]
        self.segs = []
        self.grid = {}
        for p in g.steep(vehicle):
            if p.x1 < x0 - hw or p.x0 > x1 + hw or p.y1 < y0 - hw or p.y0 > y1 + hw:
                continue
            n = len(p.pts)
            for k in range(n):
                u, v = p.pts[k], p.pts[(k + 1) % n]
                idx = len(self.segs)
                self.segs.append((u[0], u[1], v[0], v[1], p.z0, p.z1))
                Geo._register(self.grid, self.CELL, idx, min(u[0], v[0]) - hw, min(u[1], v[1]) - hw,
                              max(u[0], v[0]) + hw, max(u[1], v[1]) + hw)

    def blocked(self, x, y, z):
        hw = self.hw
        for ox, oy, zlo, zhi, r in self.objs:
            if zlo - 2.0 <= z <= zhi + 2.0 and (x - ox) ** 2 + (y - oy) ** 2 < (r + hw) ** 2:
                return True
        for k in self.grid.get((int(math.floor(x / self.CELL)), int(math.floor(y / self.CELL))), ()):
            sx, sy, ex, ey, z0, z1 = self.segs[k]
            if z1 < z + 0.3 or z0 > z + 2.0:
                continue
            if _seg_dist2(x, y, sx, sy, ex, ey) < hw * hw:
                return True
        return False


def _dp(points, eps):
    """Douglas-Peucker on 3D points; keeps the ends."""
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        lo, hi = stack.pop()
        a, b = points[lo], points[hi]
        ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        l2 = ab[0] ** 2 + ab[1] ** 2 + ab[2] ** 2
        worst, wi = -1.0, -1
        for k in range(lo + 1, hi):
            p = points[k]
            ap = (p[0] - a[0], p[1] - a[1], p[2] - a[2])
            t = 0.0 if l2 < 1e-12 else max(0.0, min(1.0, (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / l2))
            q = (ap[0] - ab[0] * t, ap[1] - ab[1] * t, ap[2] - ab[2] * t)
            dist = math.sqrt(q[0] ** 2 + q[1] ** 2 + q[2] ** 2)
            if dist > worst:
                worst, wi = dist, k
        if worst > eps:
            keep[wi] = True
            stack.append((lo, wi))
            stack.append((wi, hi))
    return [p for p, k in zip(points, keep) if k]


def _turn_degrees(a, b, c):
    v1 = (b[0] - a[0], b[1] - a[1])
    v2 = (c[0] - b[0], c[1] - b[1])
    l1, l2 = math.hypot(*v1), math.hypot(*v2)
    if l1 < 1e-9 or l2 < 1e-9:
        return 0.0
    cs = (v1[0] * v2[0] + v1[1] * v2[1]) / (l1 * l2)
    return math.degrees(math.acos(max(-1.0, min(1.0, cs))))


def nav_corridor(file, a, b, vehicle='warthog', max_cells=400000):
    """A* over a 1-unit grid of drivable ground from a to b. Returns route points with speed and turn."""
    g = load(file)
    hw, maxslope, maxstep = _vehicle(vehicle)
    a, b = _vec(a, 'a'), _vec(b, 'b')
    cosmax = math.cos(math.radians(maxslope))
    tanmax = math.tan(math.radians(maxslope))
    margin = max(60.0, math.dist(a, b))  # how far the search may leave the straight line's box
    box = (min(a[0], b[0]) - margin, min(a[1], b[1]) - margin, max(a[0], b[0]) + margin, max(a[1], b[1]) + margin)
    blocker = _Blocker(g, vehicle, box)

    def floor_near(x, y, zref):
        best = None
        for z, p in g.floors(x, y):
            if p.n[2] < cosmax or abs(z - zref) > 2.0:
                continue
            if best is None or abs(z - zref) < abs(best - zref):
                best = z
        return best

    za = floor_near(a[0], a[1], a[2])
    zb = floor_near(b[0], b[1], b[2])
    if za is None:
        raise ToolError('no drivable ground near the start point')
    if zb is None:
        raise ToolError('no drivable ground near the end point')
    gi = int(round(b[0] - a[0])), int(round(b[1] - a[1]))
    start = (0, 0)
    best_g = {start: 0.0}
    zs = {start: za}
    parent = {}
    heap = [(0.0, 0.0, start)]
    closed = set()
    nbrs = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]
    found = False
    expanded = 0
    while heap:
        _, gc, cur = heapq.heappop(heap)
        if cur in closed:
            continue
        closed.add(cur)
        if cur == gi:
            found = True
            break
        expanded += 1
        if expanded > max_cells:
            break
        zc = zs[cur]
        for di, dj in nbrs:
            nb = (cur[0] + di, cur[1] + dj)
            if nb in closed:
                continue
            x, y = a[0] + nb[0], a[1] + nb[1]
            if not (box[0] <= x <= box[2] and box[1] <= y <= box[3]):
                continue
            z = floor_near(x, y, zc)
            if z is None:
                continue
            dist = math.hypot(di, dj)
            if abs(z - zc) > dist * tanmax + maxstep:
                continue
            if blocker.blocked(x, y, z):
                continue
            ng = gc + dist + 0.05 * abs(z - zc)
            if ng < best_g.get(nb, 1e18):
                best_g[nb] = ng
                zs[nb] = z
                parent[nb] = cur
                h = math.hypot(gi[0] - nb[0], gi[1] - nb[1])
                heapq.heappush(heap, (ng + h, ng, nb))
    if not found:
        raise ToolError('no route found (%d cells searched)' % expanded)
    cells = [gi]
    while cells[-1] != start:
        cells.append(parent[cells[-1]])
    cells.reverse()
    pts = [(a[0] + c[0], a[1] + c[1], zs[c]) for c in cells]
    pts[0] = (a[0], a[1], za)
    pts[-1] = (b[0], b[1], zb)

    def drivable_line(p, q):
        """True when the straight line p..q passes the cell rule at 0.5-unit samples."""
        length = math.hypot(q[0] - p[0], q[1] - p[1])
        steps = max(1, int(math.ceil(length / 0.5)))
        step = length / steps
        zprev = p[2]
        for s in range(1, steps + 1):
            t = s / steps
            x, y = p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t
            z = floor_near(x, y, zprev)
            if z is None or abs(z - zprev) > step * tanmax + maxstep or blocker.blocked(x, y, z):
                return False
            zprev = z
        return True

    # string-pull: from each kept point go as far along the path as a straight line stays drivable
    pulled = [pts[0]]
    i = 0
    while i < len(pts) - 1:
        j = i + 1
        while j + 1 < len(pts) and drivable_line(pts[i], pts[j + 1]):
            j += 1
        pulled.append(pts[j])
        i = j
    pts = _dp(pulled, 0.75)
    out = []
    for k, p in enumerate(pts):
        speed, turn = 1.0, 0.0
        if 0 < k < len(pts) - 1:
            deg = _turn_degrees(pts[k - 1], p, pts[k + 1])
            if deg >= 15.0:
                speed = max(0.35, 1.0 - deg / 120.0)
                turn = round(min(30.0, hw * 2 + deg / 6.0), 1)
        out.append({'p': [round(c, 2) for c in p], 'speed': round(speed, 2), 'turn': turn})
    lines = []
    for r in out:
        s = 'point %g %g %g' % tuple(r['p'])
        if r['speed'] < 1.0:
            s += ' speed %g' % r['speed']
        if r['turn'] > 0:
            s += ' turn %g' % r['turn']
        lines.append(s)
    return {'found': True, 'cells_searched': expanded, 'points': out, 'text': '\n'.join(lines)}


def nav_keepouts(file, vehicle='warthog', near=3.0):
    """Object spheres and steep-surface clusters next to drivable ground, as keepout lines."""
    g = load(file)
    hw, maxslope, _ = _vehicle(vehicle)
    cosmax = math.cos(math.radians(maxslope))
    near = max(0.0, float(near))

    def drivable_near(cx, cy, cz, rad, zrange):
        for k in range(16):
            ang = k * math.pi / 8.0
            x, y = cx + math.cos(ang) * rad, cy + math.sin(ang) * rad
            for z, p in g.floors(x, y):
                if p.n[2] >= cosmax and abs(z - cz) <= zrange:
                    return True
        return False

    circles = []  # [x, y, z, r]
    for _name, ox, oy, oz, r, _kind, zlo, zhi in g.objects:
        mid, half = (zlo + zhi) / 2.0, (zhi - zlo) / 2.0 + 2.0
        if drivable_near(ox, oy, mid, r + near, half) or drivable_near(ox, oy, mid, r + 0.5, half):
            circles.append([ox, oy, oz, r])
    # steep surfaces: sample points along every polygon edge (4 units apart), one circle per 8x8 cell of points
    cells = {}
    for p in g.steep(vehicle):
        for k in range(len(p.pts)):
            u, v = p.pts[k], p.pts[(k + 1) % len(p.pts)]
            steps = max(1, int(math.dist(u, v) / 4.0))
            for s in range(steps):
                q = tuple(u[c] + (v[c] - u[c]) * s / steps for c in range(3))
                cells.setdefault((int(math.floor(q[0] / 8.0)), int(math.floor(q[1] / 8.0))), []).append(q)
    for verts in cells.values():
        mx = sum(q[0] for q in verts) / len(verts)
        my = sum(q[1] for q in verts) / len(verts)
        mz = sum(q[2] for q in verts) / len(verts)
        r = max(math.hypot(q[0] - mx, q[1] - my) for q in verts)
        zlo = min(q[2] for q in verts)
        r = max(r, 0.5)
        if drivable_near(mx, my, zlo, r + near, 3.0) or drivable_near(mx, my, zlo, r, 3.0):
            circles.append([mx, my, mz, r])
    circles = _merge_circles(circles)
    lines = ['keepout %g %g %g radius %g' % (round(c[0], 1), round(c[1], 1), round(c[2], 1), round(c[3], 1))
             for c in circles]
    return {'count': len(circles), 'keepouts': [[round(c[0], 1), round(c[1], 1), round(c[2], 1), round(c[3], 1)]
                                                for c in circles], 'text': '\n'.join(lines)}


def _merge_circles(circles, max_radius=12.0):
    """Merge overlapping circles while the merged circle stays at most max_radius."""
    circles = [list(c) for c in circles]
    changed = True
    while changed:
        changed = False
        for i in range(len(circles)):
            for j in range(i + 1, len(circles)):
                a, b = circles[i], circles[j]
                d = math.hypot(a[0] - b[0], a[1] - b[1])
                if d >= a[3] + b[3]:
                    continue
                if d + b[3] <= a[3]:
                    new = a
                elif d + a[3] <= b[3]:
                    new = b
                else:
                    r = (d + a[3] + b[3]) / 2.0
                    t = (r - a[3]) / d
                    new = [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, (a[2] + b[2]) / 2.0, r]
                if new[3] > max_radius:
                    continue
                circles[i] = list(new)
                del circles[j]
                changed = True
                break
            if changed:
                break
    return circles


# ---------------------------------------------------------------- route file parser

class Route:
    def __init__(self, name, kinds, loop, oneway, line):
        self.name, self.kinds, self.loop, self.oneway, self.line = name, kinds, loop, oneway, line
        self.points = []  # dicts: p, speed, turn, wait, line


class RouteFile:
    def __init__(self):
        self.file = None
        self.made = []
        self.links = []     # dicts: kind, a, b, oneway, line
        self.avoids = []    # dicts: p, r, line
        self.keepouts = []  # dicts: p, r, line
        self.routes = []
        self.errors = []    # (line, message)


def _num(tok, what):
    try:
        v = float(tok)
    except ValueError:
        raise ValueError('%s: %r is not a number' % (what, tok))
    if not math.isfinite(v):
        raise ValueError('%s: %r is not finite' % (what, tok))
    return v


def parse_route(text):
    """Parse a route file text. Bad or over-limit lines are refused with their line number; the rest loads."""
    rf = RouteFile()
    cur = None          # the route being read
    cur_bad = False
    total_points = 0
    seen_header = False
    names = set()

    def err(n, msg):
        rf.errors.append((n, msg))

    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        tok = line.split()
        word = tok[0]
        try:
            if not seen_header:
                if word != 'nav' or len(tok) != 3 or tok[1] != '1':
                    raise ValueError('first line must be: nav 1 <file>')
                if not _FILE_RE.match(tok[2]):
                    raise ValueError('bad file name %r' % tok[2])
                rf.file = tok[2]
                seen_header = True
                continue
            if cur is not None:
                if word == 'point':
                    if cur_bad:
                        continue
                    if len(cur.points) >= MAX_ROUTE_POINTS:
                        raise ValueError('more than %d points in route %s' % (MAX_ROUTE_POINTS, cur.name))
                    if total_points >= MAX_TOTAL_POINTS:
                        raise ValueError('more than %d points in the file' % MAX_TOTAL_POINTS)
                    if len(tok) < 4:
                        raise ValueError('point needs X Y Z')
                    pt = {'p': tuple(_num(t, 'point') for t in tok[1:4]), 'speed': 1.0, 'turn': 0.0, 'wait': 0.0,
                          'line': n}
                    rest = tok[4:]
                    if len(rest) % 2:
                        raise ValueError('point option without a value')
                    ranges = {'speed': (0.1, 1.0), 'turn': (0.0, 30.0), 'wait': (0.0, 10.0)}
                    for k in range(0, len(rest), 2):
                        key = rest[k]
                        if key not in ranges:
                            raise ValueError('unknown point option %r' % key)
                        v = _num(rest[k + 1], key)
                        if not ranges[key][0] <= v <= ranges[key][1]:
                            raise ValueError('%s %g outside %g..%g' % ((key, v) + ranges[key]))
                        pt[key] = v
                    cur.points.append(pt)
                    total_points += 1
                    continue
                if word == 'end':
                    if len(tok) != 1:
                        raise ValueError('end takes no arguments')
                    r, bad = cur, cur_bad
                    cur, cur_bad = None, False
                    if not bad:
                        if len(r.points) < 2:
                            raise ValueError('route %s has fewer than 2 points' % r.name)
                        rf.routes.append(r)
                    continue
                raise ValueError('%r is not allowed inside a route (point or end)' % word)
            if word == 'made':
                if len(tok) < 2:
                    raise ValueError('made needs at least one hash prefix')
                for t in tok[1:]:
                    if not _HEX8_RE.match(t):
                        raise ValueError('made: %r is not 8 hex digits' % t)
                rf.made.extend(t.lower() for t in tok[1:])
            elif word == 'link':
                if len(rf.links) >= MAX_LINKS:
                    raise ValueError('more than %d links' % MAX_LINKS)
                if len(tok) not in (9, 10) or tok[1] not in ('walk', 'jump', 'drop') or tok[5] != 'to':
                    raise ValueError('link walk|jump|drop X Y Z to X Y Z [oneway]')
                if len(tok) == 10 and tok[9] != 'oneway':
                    raise ValueError('link: unknown option %r' % tok[9])
                rf.links.append({'kind': tok[1], 'a': tuple(_num(t, 'link') for t in tok[2:5]),
                                 'b': tuple(_num(t, 'link') for t in tok[6:9]), 'oneway': len(tok) == 10, 'line': n})
            elif word in ('avoid', 'keepout'):
                lst, cap = (rf.avoids, MAX_AVOIDS) if word == 'avoid' else (rf.keepouts, MAX_KEEPOUTS)
                if len(lst) >= cap:
                    raise ValueError('more than %d %s lines' % (cap, word))
                if len(tok) != 6 or tok[4] != 'radius':
                    raise ValueError('%s X Y Z radius R' % word)
                r = _num(tok[5], 'radius')
                if r <= 0:
                    raise ValueError('radius must be above 0')
                lst.append({'p': tuple(_num(t, word) for t in tok[1:4]), 'r': r, 'line': n})
            elif word == 'route':
                if len(tok) < 3:
                    raise ValueError('route NAME KIND[,KIND...] [loop] [oneway]')
                if not _NAME_RE.match(tok[1]):
                    raise ValueError('bad route name %r ([a-z0-9_-]{1,31})' % tok[1])
                kinds = tok[2].split(',')
                if any(k not in VEHICLES for k in kinds):
                    raise ValueError('route kinds must be from: warthog ghost scorpion any')
                opts = tok[3:]
                if any(o not in ('loop', 'oneway') for o in opts):
                    raise ValueError('route options are loop and oneway')
                # the block is read in either case, so its lines do not raise more errors
                cur = Route(tok[1], kinds, 'loop' in opts, 'oneway' in opts, n)
                if tok[1] in names:
                    cur_bad = True
                    raise ValueError('route name %s is used twice' % tok[1])
                if len(rf.routes) >= MAX_ROUTES:
                    cur_bad = True
                    raise ValueError('more than %d routes' % MAX_ROUTES)
                names.add(tok[1])
            elif word in ('point', 'end'):
                raise ValueError('%s outside a route' % word)
            else:
                raise ValueError('unknown statement %r' % word)
        except ValueError as e:
            err(n, str(e))
            if word == 'route' and seen_header and cur is None:
                cur, cur_bad = Route('?', [], False, False, n), True  # skip the block's lines
    if not seen_header and not rf.errors:
        err(1, 'empty file: first line must be: nav 1 <file>')
    if cur is not None:
        err(cur.line, 'route %s has no end' % cur.name)
    return rf


def junctions(rf):
    """Points of different routes within 4.0 units (3D) are joined both ways, at most 4 other routes' points per
    point (the nearest). Returns {(route_index, point_index): [(route_index, point_index), ...]}."""
    allp = [(ri, pi, pt['p']) for ri, r in enumerate(rf.routes) for pi, pt in enumerate(r.points)]
    out = {}
    for ri, pi, p in allp:
        near = []
        for rj, pj, q in allp:
            if rj == ri:
                continue
            d = math.dist(p, q)
            if d <= JUNCTION_DIST:
                near.append((d, rj, pj))
        if near:
            near.sort()
            out[(ri, pi)] = [(rj, pj) for _d, rj, pj in near[:JUNCTION_MAX]]
    return out


# ---------------------------------------------------------------- obstacles and the vehicle sweep

def _is_tree(name):
    leaf = name.replace('\\', '/').rsplit('/', 1)[-1].lower()
    return any(w in leaf for w in TREE_WORDS)


def _footprints(g):
    """The local footprint table for this map: {tag name: entry} and the table file's stamp, or ({}, None)."""
    try:
        path = geo_dir() / 'footprints.json'
        st = path.stat()
    except (ToolError, OSError):
        return {}, None
    key = str(path)
    hit = _FOOT_CACHE.get(key)
    if not hit or hit[0] != (st.st_mtime_ns, st.st_size):
        try:
            with open(path, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except (OSError, ValueError):
            data = {}
        hit = ((st.st_mtime_ns, st.st_size), data if isinstance(data, dict) else {})
        _FOOT_CACHE[key] = hit
    maps = hit[1]
    entry = maps.get(g.file) or maps.get(str(g.file) + '.map') or {}
    return (entry if isinstance(entry, dict) else {}), hit[0]


_FOOT_CACHE = {}


def obstacle_circles(g):
    """Vertical circles a vehicle can hit, as (name, x, y, zlo, zhi, r, source); cached on the Geo.
    The dump's own 'obstacles' list is used when it has one. Otherwise each dump object is looked up in the local
    footprint table (its collision spheres, placed at the object's origin; no circle when the tag has no collision),
    and an object the table lacks falls back to the bounding-sphere guess (solid_radius and solid_span)."""
    table, stamp = _footprints(g)
    if g._circles is not None and g._circles_key == stamp:
        return g._circles
    out = []
    raw = g._raw_obstacles
    if isinstance(raw, list):
        for o in raw:
            try:
                name = str(o.get('name', ''))
                source = str(o.get('source', 'measured'))
                x, y, z, r = float(o['p'][0]), float(o['p'][1]), float(o['p'][2]), float(o['r'])
            except (AttributeError, KeyError, TypeError, ValueError, IndexError):
                continue
            # (the nav file's own keepouts and what one player's games learnt are not the map's scenery)
            if source in ('keepout', 'learnt'):
                continue
            if 'zlo' in o and 'zhi' in o:
                try:
                    out.append((name, x, y, float(o['zlo']), float(o['zhi']), r, 'dump'))
                    continue
                except (TypeError, ValueError):
                    pass
            out.append((name, x, y, z - r, z + r + (2.0 if _is_tree(name) else 0.0), r, 'dump'))
    else:
        for name, ox, oy, oz, solid, _kind, zlo, zhi in g.objects:
            ent = table.get(name)
            if not isinstance(ent, dict):
                out.append((name, ox, oy, zlo, zhi, solid, 'guess'))
                continue
            if ent.get('collision') is False:
                continue
            try:
                bo = ent.get('bounding_offset') or [0.0, 0.0, 0.0]
                org = (ox - float(bo[0]), oy - float(bo[1]), oz - float(bo[2]))
                spheres = ent.get('spheres') or []
                if spheres:
                    for sp in spheres:
                        sx, sy, sz, sr = (float(c) for c in sp[:4])
                        out.append((name, org[0] + sx, org[1] + sy, org[2] + sz - sr, org[2] + sz + sr, sr, 'table'))
                else:
                    br = float(ent['bounding_radius'])
                    out.append((name, ox, oy, oz - br, oz + br, br, 'table'))
            except (KeyError, TypeError, ValueError, IndexError):
                out.append((name, ox, oy, zlo, zhi, solid, 'guess'))
    g._circles = out
    g._circles_key = stamp
    return out


SWEEP_STEP = 0.5
BODY_LO = 0.3
BODY_HI = 1.6


def _sweep_index(g, vehicle):
    """Grid of obstacle circles and of steep-surface edges for the sweep, cached per Geo and vehicle."""
    key = (vehicle, g._circles_key, len(obstacle_circles(g)))
    hit = g._sweep_idx.get(vehicle)
    if hit and hit[0] == key:
        return hit[1]
    cell = 4.0
    hw = VEHICLES[vehicle][0]
    circ, cgrid = obstacle_circles(g), {}
    for k, c in enumerate(circ):
        Geo._register(cgrid, cell, k, c[1] - c[5] - hw, c[2] - c[5] - hw, c[1] + c[5] + hw, c[2] + c[5] + hw)
    edges, egrid = [], {}
    for p in g.steep(vehicle):
        n = len(p.pts)
        for k in range(n):
            u, v = p.pts[k], p.pts[(k + 1) % n]
            idx = len(edges)
            edges.append((u[0], u[1], v[0], v[1], p.z0, p.z1, p.i))
            Geo._register(egrid, cell, idx, min(u[0], v[0]) - hw, min(u[1], v[1]) - hw,
                          max(u[0], v[0]) + hw, max(u[1], v[1]) + hw)
    idx = (circ, cgrid, edges, egrid, cell)
    g._sweep_idx[vehicle] = (key, idx)
    return idx


def sweep_segment(g, a, b, vehicle='warthog'):
    """Drive a..b the way a vehicle does. None when clear, else the first hit along it:
    {kind: 'object'|'wall'|'step'|'ground', what, at: [x, y, z], s: distance along, gap: how far into the clearance}.
    Every 0.5 units the ground is the floor nearest the straight-line height (within twice GROUND_TOL); the body spans
    ground+0.3 .. ground+1.6 and the vehicle's half-width either side. An object or steep wall edge in reach is a hit,
    and so is ground that rises more than the vehicle's step between samples (a drop is fine)."""
    hw, _slope_lim, maxstep = _vehicle(vehicle)
    circ, cgrid, edges, egrid, cell = _sweep_index(g, vehicle)
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.hypot(dx, dy)
    n = max(1, int(math.ceil(length / SWEEP_STEP)))
    prev = None
    for i in range(n + 1):
        t = i / n
        x, y = a[0] + dx * t, a[1] + dy * t
        zl = a[2] + dz * t
        best = None
        for z, p in g.floors(x, y):
            if abs(z - zl) <= GROUND_TOL * 2 and (best is None or abs(z - zl) < abs(best - zl)):
                best = z
        s = round(t * length, 3)
        if best is None:
            return {'kind': 'ground', 'what': None, 'at': [round(x, 3), round(y, 3), round(zl, 3)], 's': s,
                    'gap': 0.0}
        gz = best
        if prev is not None and gz - prev > maxstep + 1e-6:
            return {'kind': 'step', 'what': round(gz - prev, 3), 'at': [round(x, 3), round(y, 3), round(gz, 3)],
                    's': s, 'gap': round(gz - prev - maxstep, 3)}
        prev = gz
        lo, hi = gz + BODY_LO, gz + BODY_HI
        cellkey = (int(math.floor(x / cell)), int(math.floor(y / cell)))
        worst = None
        for k in cgrid.get(cellkey, ()):
            name, cx, cy, zlo, zhi, r, _src = circ[k]
            if zhi < lo or zlo > hi:
                continue
            gap = r + hw - math.hypot(x - cx, y - cy)
            if gap > 0 and (worst is None or gap > worst['gap']):
                worst = {'kind': 'object', 'what': name, 'at': [round(x, 3), round(y, 3), round(gz, 3)], 's': s,
                         'gap': round(gap, 3)}
        for k in egrid.get(cellkey, ()):
            sx, sy, ex, ey, z0, z1, pi = edges[k]
            if z1 < lo or z0 > hi:
                continue
            gap = hw - math.sqrt(_seg_dist2(x, y, sx, sy, ex, ey))
            if gap > 0 and (worst is None or gap > worst['gap']):
                worst = {'kind': 'wall', 'what': pi, 'at': [round(x, 3), round(y, 3), round(gz, 3)], 's': s,
                         'gap': round(gap, 3)}
        if worst:
            return worst
    return None


def _seg_point_dist(px, py, a, b):
    return math.sqrt(_seg_dist2(px, py, a[0], a[1], b[0], b[1]))


# ---------------------------------------------------------------- validate and write

def _fmt(p):
    return '(%s)' % ' '.join('%g' % round(c, 2) for c in p)


def _describe_hit(h):
    at = _fmt(h['at'])
    if h['kind'] == 'object':
        return 'hits object %s at %s' % (str(h['what']).replace('\\', '/').rsplit('/', 1)[-1], at)
    if h['kind'] == 'wall':
        return 'hits wall surface %s at %s' % (h['what'], at)
    if h['kind'] == 'step':
        return 'hits a step of %g at %s' % (h['what'], at)
    return 'has no ground at %s' % at


def nav_validate(file, text):
    g = load(file)
    rf = parse_route(text)
    errors = ['line %d: %s' % e for e in rf.errors]
    warnings = []
    sweep = {'segments': 0, 'failed': 0}
    if rf.file is not None and rf.file != file:
        errors.append('line 1: header names file %s but the target is %s' % (rf.file, file))
    if rf.made and g.hash and not any(str(g.hash).lower().startswith(m) for m in rf.made):
        warnings.append('line 1: made hashes do not include the dump hash %s' % g.hash)

    def ground_ok(p, need_walk=False):
        for z, s in g.floors(p[0], p[1]):
            if abs(z - p[2]) <= GROUND_TOL and (s.walk or not need_walk):
                return True
        return False

    for r in rf.routes:
        for pt in r.points:
            if not ground_ok(pt['p']):
                errors.append('line %d: route %s: no ground within %g units of the point' % (pt['line'], r.name,
                                                                                         GROUND_TOL))
            for k in rf.keepouts:
                if math.hypot(pt['p'][0] - k['p'][0], pt['p'][1] - k['p'][1]) < k['r']:
                    errors.append('line %d: route %s: point is inside the keepout of line %d' % (pt['line'], r.name,
                                                                                               k['line']))
        pairs = [(i, i + 1) for i in range(len(r.points) - 1)]
        if r.loop and len(r.points) > 2:
            pairs.append((len(r.points) - 1, 0))
        kinds = [k for k in r.kinds]
        for ia, ib in pairs:
            a, b = r.points[ia], r.points[ib]
            for k in rf.keepouts:
                if _seg_point_dist(k['p'][0], k['p'][1], a['p'], b['p']) < k['r'] and \
                        math.hypot(a['p'][0] - k['p'][0], a['p'][1] - k['p'][1]) >= k['r'] and \
                        math.hypot(b['p'][0] - k['p'][0], b['p'][1] - k['p'][1]) >= k['r']:
                    warnings.append('line %d: route %s seg %d-%d passes within the keepout of line %d' %
                                    (b['line'], r.name, ia, ib, k['line']))
            for kind in kinds:
                hw, maxslope, _ = VEHICLES[kind]
                horiz = math.hypot(b['p'][0] - a['p'][0], b['p'][1] - a['p'][1])
                dz = abs(b['p'][2] - a['p'][2])
                deg = math.degrees(math.atan2(dz, horiz)) if (horiz > 1e-9 or dz > 1e-9) else 0.0
                if deg > maxslope + 1e-6:
                    errors.append('line %d: route %s: segment slope %.1f degrees is over the %s limit %g' %
                                  (b['line'], r.name, deg, kind, maxslope))
                    continue
                sweep['segments'] += 1
                hit = sweep_segment(g, a['p'], b['p'], kind)
                if hit:
                    sweep['failed'] += 1
                    errors.append('line %d: route %s seg %d-%d %s->%s: %s %s' % (
                        b['line'], r.name, ia, ib, _fmt(a['p']), _fmt(b['p']), kind, _describe_hit(hit)))
    for l in rf.links:
        for end in (l['a'], l['b']):
            if not ground_ok(end, need_walk=True):
                errors.append('line %d: link end %s is not on a walk surface' % (l['line'], list(end)))
    jn = junctions(rf)
    return {'ok': not errors, 'errors': errors, 'warnings': warnings,
            'routes': len(rf.routes), 'points': sum(len(r.points) for r in rf.routes),
            'keepouts': len(rf.keepouts), 'links': len(rf.links), 'avoids': len(rf.avoids),
            'junction_points': len(jn), 'sweep': sweep}


def out_dir():
    d = os.environ.get('HALO_NAV_OUT', '')
    base = Path(d) if d else (core.WRITE_ROOTS[0] / 'bot_nav' if core.WRITE_ROOTS else None)
    if base is None:
        raise ToolError('no write root is configured and HALO_NAV_OUT is not set')
    return core.safe_path(str(base), write=True, must_exist=False)


def nav_write(file, text):
    res = nav_validate(file, text)
    if not res['ok']:
        raise ToolError('route file refused, %d error(s): %s' % (len(res['errors']), '; '.join(res['errors'][:20])))
    d = out_dir()
    d.mkdir(parents=True, exist_ok=True)
    path = core.safe_path(str(d / (file + '.txt')), write=True, must_exist=False)
    tmp = path.with_name(path.name + '.tmp')
    body = text if text.endswith('\n') else text + '\n'
    tmp.write_text(body, encoding='utf-8')
    os.replace(tmp, path)
    res['written'] = str(path)
    return res
