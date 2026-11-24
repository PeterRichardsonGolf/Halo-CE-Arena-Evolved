#!/usr/bin/env python3
"""Draw a top-down picture of a bot-navigation map with its footprints, routes and driving events.

  python3 tools/bot_nav_picture.py --map dangercanyon --debug debug.txt --box -40 -20 0 20 --out yard.png
"""
import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.halo_mcp import nav, nav_render  # noqa: E402


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--map', required=True)
    ap.add_argument('--debug', help='game log to read driving events from')
    ap.add_argument('--nav', help='route file (default: the repo bot_nav file of the map)')
    ap.add_argument('--geo', help='folder of geometry dumps (default: HALO_NAV_GEO or bot_nav_geo)')
    ap.add_argument('--box', nargs=4, type=float, metavar=('X0', 'Y0', 'X1', 'Y1'), help='zoom to this world box')
    ap.add_argument('--zband', nargs=2, type=float, metavar=('ZLO', 'ZHI'), help='draw only this height band')
    ap.add_argument('--out', help='PNG path (default: HALO_NAV_OUT or bot_nav_pictures)')
    ap.add_argument('--size', type=int, default=2400, help='picture width in pixels')
    a = ap.parse_args(argv)

    folder = nav_render.geo_folder(a.geo)
    data = nav_render.load_dump(a.map, folder)
    circles = nav_render.circles_for(data, folder)

    nav_path = Path(a.nav) if a.nav else nav_render.REPO / 'port' / 'assets' / 'bot_nav' / (a.map + '.txt')
    route = None
    if nav_path.is_file():
        route = nav.parse_route(nav_path.read_text(encoding='utf-8'))
        for n, msg in route.errors:
            print('nav file line %d: %s' % (n, msg), file=sys.stderr)

    events = []
    if a.debug:
        allev = nav_render.parse_events(a.debug, nav_render.known_maps(folder))
        events = allev.get(a.map, [])
        print('events by map in the log: %s' % {m or '(none)': len(v) for m, v in allev.items()})
    counts = nav_render.event_counts(events)
    print('%s: events %s' % (a.map, {k: counts.get(k, 0) for k in nav_render.KINDS}))
    for text, n in nav_render.hit_counts(events).most_common():
        print('  hit=%s: %d' % (text, n))
    for n, x0, y0, kinds in nav_render.dense_cells(events):
        x1, y1 = x0 + nav_render.CELL, y0 + nav_render.CELL
        near = nav_render.near_circles(circles, (x0 + x1) / 2, (y0 + y1) / 2, 4.0)[:3]
        lay = ', '.join('%s (%.1f,%.1f r%.1f %s, %.1f off)' % (nm, cx, cy, r, src, d) for d, nm, cx, cy, r, src in near)
        print('  cell (%g %g)..(%g %g): %d %s; scenery: %s' % (x0, y0, x1, y1, n, kinds, lay or 'none within 4'))

    out = a.out or str(Path(os.environ.get('HALO_NAV_OUT') or nav_render.PICTURE_DEFAULT) / ('%s%s.png' % (a.map, '_zoom' if a.box else '')))
    path = nav_render.render(a.map, data, out, route=route, events=events, circles=circles, box=a.box,
                             zband=a.zband, size=a.size, nav_label=nav_path.name if route else '')
    print('wrote %s (%d bytes)' % (path, os.path.getsize(path)))


if __name__ == '__main__':
    main()
