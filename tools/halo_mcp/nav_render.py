"""Top-down pictures of a bot-navigation map: ground, scenery footprints, routes, keepouts, spawns, flags and
vehicle-driving events read from a game log. Parsing and counting need only the standard library; drawing needs
matplotlib (imported when a picture is made)."""
import json
import math
import os
import re
from collections import Counter
from pathlib import Path

from . import core, nav

REPO = Path(__file__).resolve().parents[2]
# (the geometry dumps and pictures stay out of the repo: HALO_NAV_GEO and HALO_NAV_OUT name their folders, else these)
GEO_DEFAULT = 'bot_nav_geo'
PICTURE_DEFAULT = 'bot_nav_pictures'
CELL = 4.0

KINDS = ('crash', 'wedge', 'reverse', 'stuck')
_EVENT_RE = re.compile(r'ev=drive_(crash|wedge|reverse|stuck)(?=\s)')
_AT_RE = re.compile(r'at \((-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+)\)')
_HIT_RE = re.compile(r'hit=(\S+)')
_NAV_RE = re.compile(r'bot nav: (\S+) \(built in\)')
_START_RE = re.compile(r'ev=game_start map=(\S+)')


def geo_folder(path=None):
    return Path(path or os.environ.get('HALO_NAV_GEO') or GEO_DEFAULT)


def known_maps(folder=None):
    """Names of the maps that have a geometry dump in the folder."""
    d = geo_folder(folder)
    return {p.stem for p in d.glob('*.json') if p.stem != 'footprints'} if d.is_dir() else set()


def plain_map(name, known):
    """A log map name without a trailing '@...' or '_dig' style suffix, when the plain name has a dump."""
    if name in known:
        return name
    base = name.split('@', 1)[0]
    if base not in known and base.endswith('_dig'):
        base = base[:-4]
    return base if base in known else name


def parse_events(debug_path, known=None):
    """Driving events of a game log as {map: [{'kind','x','y','z','line','hit'}]}. The map of an event is the most
    recent earlier line that names a map (a built-in nav line or a game start). Events before any such line are
    filed under ''."""
    known = known_maps() if known is None else set(known)
    out = {}
    cur = ''
    with open(debug_path, 'r', encoding='utf-8', errors='replace') as f:
        for n, line in enumerate(f, 1):
            if 'bot nav:' in line:
                m = _NAV_RE.search(line)
                if m:
                    cur = plain_map(m.group(1), known)
                    continue
            if 'ev=game_start' in line:
                m = _START_RE.search(line)
                if m:
                    cur = plain_map(m.group(1), known)
                    continue
            if 'ev=drive_' not in line:
                continue
            m = _EVENT_RE.search(line)
            a = _AT_RE.search(line)
            if not m or not a:
                continue
            h = _HIT_RE.search(line)
            out.setdefault(cur, []).append({'kind': m.group(1), 'x': float(a.group(1)), 'y': float(a.group(2)),
                                            'z': float(a.group(3)), 'line': n, 'hit': h.group(1) if h else None})
    return out


def event_counts(events):
    return Counter(e['kind'] for e in events)


def hit_counts(events):
    return Counter(e['hit'] for e in events if e.get('hit'))


def dense_cells(events, top=5, cell=CELL):
    """The densest cell x cell squares: [(count, x0, y0, {kind: count})], most events first."""
    cells = {}
    for e in events:
        k = (math.floor(e['x'] / cell), math.floor(e['y'] / cell))
        cells.setdefault(k, []).append(e['kind'])
    rows = [(len(v), k[0] * cell, k[1] * cell, dict(Counter(v))) for k, v in cells.items()]
    rows.sort(key=lambda r: (-r[0], r[1], r[2]))
    return rows[:top]


def near_circles(circles, x, y, reach):
    """Obstacle circles whose edge is within reach of a point: [(distance to edge, name, x, y, r, source)]."""
    out = []
    for name, cx, cy, _zlo, _zhi, r, src in circles:
        d = math.hypot(cx - x, cy - y) - r
        if d <= reach:
            out.append((d, name.rsplit('/', 1)[-1], cx, cy, r, src))
    out.sort()
    return out


def load_dump(map_name, folder=None):
    path = geo_folder(folder) / (map_name + '.json')
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def _own_circles(data, folder):
    """Obstacle circles from the local footprint table when nav.py has no obstacle_circles()."""
    table = {}
    try:
        with open(geo_folder(folder) / 'footprints.json', 'r', encoding='utf-8') as f:
            table = (json.load(f) or {}).get(str(data.get('file')), {})
    except (OSError, ValueError):
        pass
    out = []
    for o in data.get('objects') or []:
        try:
            name = str(o.get('name', ''))
            x, y, z, r = float(o['p'][0]), float(o['p'][1]), float(o['p'][2]), float(o['r'])
        except (KeyError, TypeError, ValueError, IndexError):
            continue
        ent = table.get(name)
        if not isinstance(ent, dict):
            solid = nav.solid_radius(name, r) if hasattr(nav, 'solid_radius') else r
            out.append((name, x, y, z - solid, z + solid, solid, 'guess'))
            continue
        if ent.get('collision') is False:
            continue
        bo = ent.get('bounding_offset') or [0.0, 0.0, 0.0]
        org = (x - bo[0], y - bo[1], z - bo[2])
        spheres = ent.get('spheres') or []
        if not spheres:
            br = float(ent.get('bounding_radius', r))
            out.append((name, x, y, z - br, z + br, br, 'table'))
        for sx, sy, sz, sr in (s[:4] for s in spheres):
            out.append((name, org[0] + sx, org[1] + sy, org[2] + sz - sr, org[2] + sz + sr, sr, 'table'))
    return out


def circles_for(data, folder=None):
    """Obstacle circles (name, x, y, zlo, zhi, r, source) for a dump: nav.obstacle_circles() when it exists,
    otherwise computed here from the footprint table."""
    fn = getattr(nav, 'obstacle_circles', None)
    if fn is None:
        return _own_circles(data, folder)
    # nav.py reads the footprint table through the allowed-roots check: allow the dump folder while it runs
    old_env, old_roots = os.environ.get('HALO_NAV_GEO'), core.READ_ROOTS
    os.environ['HALO_NAV_GEO'] = str(geo_folder(folder))
    core.READ_ROOTS = list(old_roots or []) + [geo_folder(folder).resolve()]
    try:
        return list(fn(nav.Geo(data)))
    finally:
        core.READ_ROOTS = old_roots
        if old_env is None:
            del os.environ['HALO_NAV_GEO']
        else:
            os.environ['HALO_NAV_GEO'] = old_env


def _in_box(x0, y0, x1, y1, box):
    return box is None or not (x1 < box[0] or x0 > box[2] or y1 < box[1] or y0 > box[3])


def _data_bounds(data):
    xs, ys = [], []
    for s in data.get('surfaces') or []:
        if s.get('walk'):
            for v in s['v']:
                xs.append(v[0])
                ys.append(v[1])
    if not xs:
        b = data.get('bounds') or [[0, 0, 0], [100, 100, 0]]
        return b[0][0], b[0][1], b[1][0], b[1][1]
    return min(xs), min(ys), max(xs), max(ys)


def render(map_name, data, out_path, route=None, events=None, circles=None, box=None, zband=None,
           size=2400, nav_label=''):
    """Draw the picture to a PNG and return the path."""
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection, LineCollection
    from matplotlib.colors import LinearSegmentedColormap
    from matplotlib.lines import Line2D
    from matplotlib.patches import Circle, Patch

    events = events or []
    circles = circles or []
    bx0, by0, bx1, by1 = box if box else _data_bounds(data)
    if box is None:
        pad = 0.02 * max(bx1 - bx0, by1 - by0)
        bx0, by0, bx1, by1 = bx0 - pad, by0 - pad, bx1 + pad, by1 + pad
    view = (bx0, by0, bx1, by1)

    def z_ok(z):
        return zband is None or zband[0] <= z <= zband[1]

    ground, zs, steep = [], [], []
    for s in data.get('surfaces') or []:
        v = s.get('v') or []
        if len(v) < 3:
            continue
        xs = [p[0] for p in v]
        ys = [p[1] for p in v]
        if not _in_box(min(xs), min(ys), max(xs), max(ys), view):
            continue
        zmean = sum(p[2] for p in v) / len(v)
        if zband is not None and not any(z_ok(p[2]) for p in v):
            continue
        if s.get('walk'):
            ground.append([(p[0], p[1]) for p in v])
            zs.append(zmean)
        else:
            steep.append([(p[0], p[1]) for p in v])

    w_in = size / 100.0
    span_x, span_y = bx1 - bx0, by1 - by0
    h_in = min(max(w_in * span_y / span_x, 2.0), w_in * 2.5)
    fig, ax = plt.subplots(figsize=(w_in, h_in), dpi=100)
    ax.set_xlim(bx0, bx1)
    ax.set_ylim(by0, by1)
    ax.set_aspect('equal')
    ax.set_facecolor('#15191c')
    cmap = LinearSegmentedColormap.from_list('height', ['#3b4b52', '#9aa79a', '#f1ecd2'])
    if ground:
        pc = PolyCollection(ground, array=zs, cmap=cmap, edgecolors='face', linewidths=0.2)
        if zband is not None:
            pc.set_clim(zband[0], zband[1])
        ax.add_collection(pc)
    if steep:
        ax.add_collection(LineCollection([p + [p[0]] for p in steep], colors='#0a0c0d', linewidths=0.35, alpha=0.8))

    for name, cx, cy, zlo, zhi, r, src in circles:
        if not _in_box(cx - r, cy - r, cx + r, cy + r, view) or not (zband is None or zhi >= zband[0] and zlo <= zband[1]):
            continue
        if src == 'guess':
            ax.add_patch(Circle((cx, cy), r, fill=False, edgecolor='#e0302a', hatch='////', linewidth=0.6, zorder=3))
        else:
            ax.add_patch(Circle((cx, cy), r, facecolor='#e0302a', edgecolor='#7a0f0c', linewidth=0.5, alpha=0.8,
                                zorder=3))

    if route is not None:
        for k, r in enumerate(route.keepouts):
            cx, cy = r['p'][0], r['p'][1]
            ax.add_patch(Circle((cx, cy), r['r'], fill=False, edgecolor='#2f7bff', linestyle='--', linewidth=1.0,
                                zorder=4))
        palette = plt.get_cmap('tab10')
        for k, rt in enumerate(route.routes):
            col = palette(k % 10)
            pts = [p['p'] for p in rt.points]
            seq = pts + ([pts[0]] if rt.loop else [])
            ax.plot([p[0] for p in seq], [p[1] for p in seq], color=col, linewidth=1.3, zorder=5)
            for a, b in zip(seq, seq[1:]):
                if abs(b[0] - a[0]) + abs(b[1] - a[1]) < 1e-6:
                    continue
                ax.annotate('', xy=((a[0] + b[0]) / 2 + (b[0] - a[0]) * 0.05, (a[1] + b[1]) / 2 + (b[1] - a[1]) * 0.05),
                            xytext=((a[0] + b[0]) / 2, (a[1] + b[1]) / 2),
                            arrowprops=dict(arrowstyle='-|>', color=col, lw=1.2, mutation_scale=11), zorder=6)
            for i, p in enumerate(pts):
                if _in_box(p[0], p[1], p[0], p[1], view):
                    ax.text(p[0], p[1], str(i), fontsize=6, color='white', zorder=7,
                            bbox=dict(boxstyle='round,pad=0.1', fc=col, ec='none', alpha=0.85))
            if pts and _in_box(pts[0][0], pts[0][1], pts[0][0], pts[0][1], view):
                ax.text(pts[0][0], pts[0][1] + 1.2, rt.name, fontsize=8, color=col, fontweight='bold', zorder=7,
                        bbox=dict(boxstyle='round,pad=0.15', fc='#101315', ec='none', alpha=0.7))

    for v in data.get('vehicles') or []:
        p = v.get('p') or [0, 0, 0]
        if not _in_box(p[0], p[1], p[0], p[1], view) or not z_ok(p[2]):
            continue
        ax.scatter([p[0]], [p[1]], marker='s', s=40, c='#ffd23f', edgecolors='black', linewidths=0.6, zorder=8)
        ax.text(p[0] + 0.8, p[1] + 0.8, str(v.get('class', '?')), fontsize=7, color='#ffd23f', zorder=8)
    for f in data.get('flags') or []:
        p = f.get('p') or [0, 0, 0]
        if _in_box(p[0], p[1], p[0], p[1], view) and z_ok(p[2]):
            ax.scatter([p[0]], [p[1]], marker='*', s=160, c='#ffffff', edgecolors='black', linewidths=0.7, zorder=8)

    style = {'crash': dict(marker='X', c='#ff2020', s=70, edgecolors='black'),
             'wedge': dict(marker='o', c='#ff9a1f', s=40, edgecolors='black'),
             'reverse': dict(marker='^', c='#ffe600', s=45, edgecolors='black'),
             'stuck': dict(marker='s', c='#a24dff', s=40, edgecolors='black')}
    shown = [e for e in events if _in_box(e['x'], e['y'], e['x'], e['y'], view) and z_ok(e['z'])]
    for kind in KINDS:
        pts = [(e['x'], e['y']) for e in shown if e['kind'] == kind]
        if pts:
            ax.scatter([p[0] for p in pts], [p[1] for p in pts], linewidths=0.6, zorder=9, **style[kind])

    counts = event_counts(shown)
    hits = hit_counts(shown)
    handles = [Patch(facecolor='#e0302a', edgecolor='#7a0f0c', label='scenery footprint'),
               Patch(facecolor='none', edgecolor='#e0302a', hatch='////', label='scenery guessed from bounding sphere'),
               Line2D([0], [0], color='#2f7bff', linestyle='--', label='keepout'),
               Line2D([0], [0], color='#cccccc', marker='>', markersize=5, label='route (arrows, point numbers)'),
               Line2D([0], [0], marker='s', color='none', markerfacecolor='#ffd23f', markeredgecolor='black',
                      label='vehicle spawn'),
               Line2D([0], [0], marker='*', color='none', markerfacecolor='white', markeredgecolor='black',
                      markersize=11, label='flag')]
    for kind in KINDS:
        st = style[kind]
        handles.append(Line2D([0], [0], marker=st['marker'], color='none', markerfacecolor=st['c'],
                              markeredgecolor='black', markersize=7, label='%s (%d)' % (kind, counts.get(kind, 0))))
    for text, n in hits.most_common(6):
        handles.append(Line2D([0], [0], color='none', label='hit=%s: %d' % (text, n)))
    ax.legend(handles=handles, loc='upper center', bbox_to_anchor=(0.5, -0.03), ncol=5, fontsize=8, frameon=False,
              labelcolor='#222222')
    band = '' if zband is None else '  z %g..%g' % tuple(zband)
    ax.set_title('%s   nav: %s   events: %s%s' % (
        map_name, nav_label or 'none',
        ', '.join('%s %d' % (k, counts.get(k, 0)) for k in KINDS), band), fontsize=11)
    ax.grid(color='#ffffff', alpha=0.12, linewidth=0.4)
    ax.tick_params(labelsize=7)
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=100, bbox_inches='tight')
    plt.close(fig)
    return str(out_path)
