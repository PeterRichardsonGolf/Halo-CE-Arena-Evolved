#!/usr/bin/env python3
"""Arena Evolved's own waypoint glyphs: the overshield's, the active camouflage's and the power weapons' mark, for the item waypoints'
ICONS and BOTH styles (display.waypoint_style; source/interface/hud_item_timers.c draws them), and DEAD ALLY MARKERS' cross
and its outline. The stock
maps have no picture of either (their hud_msg_icons sheet has weapons and buttons only).

Each glyph is drawn here from plain geometry (no picture of any game is read or traced): drawn on a 32 by 32 grid and sampled at 64 by 64, flat, one
colour, its coverage as alpha. The game tints it as it tints its label.

    python3 tools/ae_waypoint_glyphs.py            writes port/linux/game/ae_waypoint_glyphs.c and the PNGs in
                                                   port/assets/waypoints/ (the pictures of the same tables)
    python3 tools/ae_waypoint_glyphs.py --check    fails when those files are not what this draws
"""
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "port" / "linux" / "game" / "ae_waypoint_glyphs.c"
PICTURES = ROOT / "port" / "assets" / "waypoints"
SIZE = 64
UNITS = 32  # (the shapes are drawn on a 32 by 32 grid, sampled at SIZE)
SAMPLES = 4  # by 4, in each texel


def _segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    length = dx * dx + dy * dy
    t = 0.0 if length == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def _shield_outline():
    """a heater shield: a low peak on top, straight sides, curved in to a point at the foot"""
    right = [(16.0, 2.5), (27.5, 5.5), (27.5, 15.0)]
    for step in range(1, 13):  # (a quadratic curve from the side to the foot)
        t = step / 12
        right.append(((1 - t) ** 2 * 27.5 + 2 * (1 - t) * t * 27.0 + t * t * 16.0,
                      (1 - t) ** 2 * 15.0 + 2 * (1 - t) * t * 24.5 + t * t * 30.0))
    return right + [(32.0 - x, y) for x, y in reversed(right[:-1])][:-1]


SHIELD = _shield_outline()


def _inside(px, py, polygon):
    inside = False
    for (ax, ay), (bx, by) in zip(polygon, polygon[1:] + polygon[:1]):
        if (ay > py) != (by > py) and px < ax + (py - ay) * (bx - ax) / (by - ay):
            inside = not inside
    return inside


def overshield(px, py):
    """a shield's outline around a smaller solid shield: a shield over a shield"""
    if not _inside(px, py, SHIELD):
        return False
    edge = min(_segment_distance(px, py, ax, ay, bx, by) for (ax, ay), (bx, by) in zip(SHIELD, SHIELD[1:] + SHIELD[:1]))
    return edge < 2.6 or edge > 5.4


def camo(px, py):
    """an eye struck through: unseen. The eye is the lens two circles share, with a pupil; the stroke runs corner
    to corner, with a gap cut either side of it"""
    stroke = _segment_distance(px, py, 6.0, 27.5, 26.0, 4.5)
    if stroke < 1.4:
        return True
    if stroke < 2.9:
        return False
    radius, offset = 16.25, 8.25  # (a lens 28 wide and 16 high)
    upper = radius - math.hypot(px - 16.0, py - (16.0 + offset))
    lower = radius - math.hypot(px - 16.0, py - (16.0 - offset))
    edge = min(upper, lower)
    if edge < 0:
        return False
    return edge < 2.6 or math.hypot(px - 16.0, py - 16.0) < 4.8


def power_weapon(px, py):
    """The power weapon mark, our own: an eight-point burst, four strokes through the middle (the straight
    ones longer than the diagonal ones) round a small solid centre"""
    centre = math.hypot(px - 16.0, py - 16.0)
    if centre < 3.4:
        return True
    for angle, length in ((0, 13.5), (90, 13.5), (45, 10.5), (135, 10.5)):
        dx, dy = math.cos(math.radians(angle)) * length, math.sin(math.radians(angle)) * length
        # (a stroke thinning to its tips)
        reach = _segment_distance(px, py, 16.0 - dx, 16.0 - dy, 16.0 + dx, 16.0 + dy)
        if reach < 1.7 * max(0.25, 1.0 - centre / (length * 1.15)) + 0.35:
            return True
    return False


def _cross(px, py, half_width):
    """two strokes corner to corner, square ended"""
    return min(_segment_distance(px, py, 7.0, 7.0, 25.0, 25.0),
               _segment_distance(px, py, 7.0, 25.0, 25.0, 7.0)) < half_width


def dead_ally(px, py):
    """DEAD ALLY MARKERS' mark: a cross where a teammate fell"""
    return _cross(px, py, 2.9)


def dead_ally_outline(px, py):
    """the same cross, wider: drawn dark under it in HUD STYLE AE (the names' dark outline)"""
    return _cross(px, py, 4.4)


GLYPHS = [("overshield", overshield), ("camo", camo), ("power_weapon", power_weapon), ("dead_ally", dead_ally),
          ("dead_ally_outline", dead_ally_outline)]


def coverage(shape):
    rows = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            hits = sum(shape((x + (i + 0.5) / SAMPLES) * UNITS / SIZE, (y + (j + 0.5) / SAMPLES) * UNITS / SIZE)
                       for j in range(SAMPLES) for i in range(SAMPLES))
            row.append(round(255 * hits / (SAMPLES * SAMPLES)))
        rows.append(row)
    return rows


def png(rows):
    """white, the coverage as alpha (grey + alpha, 8 bits each)"""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    raw = b"".join(b"\0" + b"".join(bytes((255, alpha)) for alpha in row) for row in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 4, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def source(tables):
    lines = ["/*", "AE_WAYPOINT_GLYPHS.C", "",
             "Arena Evolved's own waypoint glyphs (ae_waypoint_icons.h): each 64 by 64, its coverage",
             "as alpha, row by row from the top. Written by tools/ae_waypoint_glyphs.py, which draws",
             "them from plain geometry: change that, not this (tools/test_ae_hud_settings.py checks).",
             "*/", "", '#include "ae_waypoint_icons.h"', "",
             "const unsigned char ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_COUNT][AE_WAYPOINT_GLYPH_SIZE * AE_WAYPOINT_GLYPH_SIZE] =",
             "{"]
    for name, rows in tables:
        lines += [f"\t/* {name} */", "\t{"]
        lines += ["\t\t" + ",".join(f"{alpha:3d}" for alpha in row) + "," for row in rows]
        lines += ["\t},"]
    lines += ["};", ""]
    return "\n".join(lines)


def files():
    tables = [(name, coverage(shape)) for name, shape in GLYPHS]
    out = {SOURCE: source(tables).encode()}
    for name, rows in tables:
        out[PICTURES / f"{name}.png"] = png(rows)
    return out


def _same(path, data):
    """the file is what was drawn: a PNG by its header and its decompressed pixels (another zlib may compress the same
    pixels to other bytes), anything else byte for byte"""
    if not path.is_file():
        return False
    have = path.read_bytes()
    if path.suffix != ".png":
        return have == data

    def pixels(png):
        chunks, at = [], 8
        while at < len(png):
            size = struct.unpack(">I", png[at:at + 4])[0]
            chunks.append((png[at + 4:at + 8], png[at + 8:at + 8 + size]))
            at += 12 + size
        return ([body for kind, body in chunks if kind == b"IHDR"],
                zlib.decompress(b"".join(body for kind, body in chunks if kind == b"IDAT")))
    try:
        return have[:8] == data[:8] and pixels(have) == pixels(data)
    except (struct.error, zlib.error):
        return False


def main(argv):
    stale = []
    for path, data in files().items():
        if "--check" in argv:
            if not _same(path, data):
                stale.append(str(path.relative_to(ROOT)))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    if stale:
        print("not what tools/ae_waypoint_glyphs.py draws: " + ", ".join(stale))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
