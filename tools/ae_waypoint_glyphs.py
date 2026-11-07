#!/usr/bin/env python3
"""Arena Evolved's own waypoint glyphs: the overshield's, the active camouflage's and the power weapons' mark, for the item waypoints'
ICONS and BOTH styles (display.waypoint_style; source/interface/hud_item_timers.c draws them), and DEAD ALLY MARKERS' cross
and its outline. The stock
maps have no picture of either (their hud_msg_icons sheet has weapons and buttons only).

Each glyph is drawn here from plain geometry (no picture of any game is read or traced): drawn on a 32 by 32 grid and sampled at 64 by 64, flat, one
colour, its coverage as alpha. The game tints it as it tints its label. The one exception is the callout wheel's
NEED A RIDE steering wheel, a human-drawn icon (Delapouite's, CC BY 3.0: AUTHORS.md) whose coverage is read from
port/assets/hud/steering-wheel-coverage.png; its outline is drawn from that coverage as the others' are.

    python3 tools/ae_waypoint_glyphs.py            writes port/linux/game/ae_waypoint_glyphs.c and the PNGs in
                                                   port/assets/waypoints/ (the pictures of the same tables)
    python3 tools/ae_waypoint_glyphs.py --check    fails when those files are not what this draws

The wheel's coverage picture is the only thing here that needs a rasterizer, so it is made once and committed
(--check reads it, and needs none). To make it again, after a change to port/assets/hud/steering-wheel.svg:

    rsvg-convert -w 64 -h 64 port/assets/hud/steering-wheel.svg -o port/assets/hud/steering-wheel-coverage.png
"""
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "port" / "linux" / "game" / "ae_waypoint_glyphs.c"
PICTURES = ROOT / "port" / "assets" / "waypoints"
WHEEL = ROOT / "port" / "assets" / "hud" / "steering-wheel-coverage.png"
HELMET = ROOT / "port" / "assets" / "hud" / "helmet-downed-coverage.png"
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


def _lens_depth(px, py):
    """how far inside the eye's lens (the part two circles share, 28 wide and 16 high) a point is; negative outside"""
    radius, offset = 16.25, 8.25
    upper = radius - math.hypot(px - 16.0, py - (16.0 + offset))
    lower = radius - math.hypot(px - 16.0, py - (16.0 - offset))
    return min(upper, lower)


def camo(px, py):
    """an eye struck through: unseen. The eye is the lens two circles share, with a pupil; the stroke runs corner
    to corner, with a gap cut either side of it"""
    stroke = _segment_distance(px, py, 6.0, 27.5, 26.0, 4.5)
    if stroke < 1.4:
        return True
    if stroke < 2.9:
        return False
    edge = _lens_depth(px, py)
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


def _in_triangle(px, py, a, b, c):
    def side(p, q):
        return (q[0] - p[0]) * (py - p[1]) - (q[1] - p[1]) * (px - p[0])
    sides = (side(a, b), side(b, c), side(c, a))
    return all(s >= 0 for s in sides) or all(s <= 0 for s in sides)


def _mark_enemy_solid(px, py):
    return _in_triangle(px, py, (16.0, 1.5), (31.0, 29.0), (1.0, 29.0))


def mark_enemy(px, py):
    """MARKS' enemy: a triangle with a bar and a dot cut out"""
    if _segment_distance(px, py, 16.0, 11.5, 16.0, 19.0) < 2.1 or math.hypot(px - 16.0, py - 24.3) < 2.3:
        return False
    return _mark_enemy_solid(px, py)


def _mark_item_solid(px, py):
    return 4.0 <= px <= 28.0 and 4.0 <= py <= 28.0


def mark_item(px, py):
    """MARKS' item: a box in a box"""
    if 13.0 <= px <= 19.0 and 13.0 <= py <= 19.0:
        return True
    return _mark_item_solid(px, py) and not (9.0 <= px <= 23.0 and 9.0 <= py <= 23.0)


def _mark_place_solid(px, py):
    if math.hypot(px - 16.0, py - 11.5) < 10.0:
        return True
    # (the tangents from the tip, (16, 31), to the head)
    length = 31.0 - 11.5
    tangent = math.sqrt(length * length - 100.0)
    half = 10.0 * tangent / length
    at = 11.5 + 100.0 / length
    return _in_triangle(px, py, (16.0, 31.0), (16.0 - half, at), (16.0 + half, at))


def mark_place(px, py):
    """MARKS' place: a pin, its tip on the spot"""
    return _mark_place_solid(px, py) and math.hypot(px - 16.0, py - 11.5) >= 4.0


def _stroke(px, py, points, half_width):
    """a line through points, round ended"""
    return min(_segment_distance(px, py, ax, ay, bx, by)
               for (ax, ay), (bx, by) in zip(points, points[1:])) < half_width


def _call_attack(px, py):
    """ATTACKING HERE: two stacked chevrons"""
    return any(_stroke(px, py, [(5.0, top + 11.0), (16.0, top), (27.0, top + 11.0)], 2.4) for top in (5.5, 15.5))


def _call_defend(px, py):
    """DEFENDING HERE: the heater shield, solid"""
    return _inside(px, py, SHIELD)


def _call_watch_solid(px, py):
    return _lens_depth(px, py) >= 0


def _call_watch(px, py):
    """WATCHING HERE: the eye, the camouflage's lens and pupil"""
    depth = _lens_depth(px, py)
    return depth >= 0 and (depth < 2.6 or math.hypot(px - 16.0, py - 16.0) < 4.8)


def _call_help(px, py):
    """HELP ME: a plus, square ended"""
    across, along = abs(px - 16.0), abs(py - 16.0)
    return (across <= 3.2 and along <= 11.0) or (along <= 3.2 and across <= 11.0)


def _call_smart(px, py):
    """SMART CALL: corner brackets and a dot"""
    if math.hypot(px - 16.0, py - 16.0) < 3.6:
        return True
    if not (4.0 <= px <= 28.0 and 4.0 <= py <= 28.0):
        return False
    x, y = min(px - 4.0, 28.0 - px), min(py - 4.0, 28.0 - py)
    return (x < 3.2 and y < 8.0) or (y < 3.2 and x < 8.0)


def _call_flag_solid(px, py):
    return (6.5 <= px <= 9.5 and 3.0 <= py <= 29.0) or _in_triangle(px, py, (9.5, 4.0), (27.0, 10.5), (9.5, 17.0))


def _read_png(path):
    """a PNG's 8 bit RGBA pixels, as rows of (r, g, b, a): the rasterized wheel (no PNG library is needed)"""
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", f"{path} is not a PNG"
    at, header, idat = 8, None, []
    while at < len(data):
        size, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + size]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat.append(body)
        at += 12 + size
    width, height, depth, colour, _, _, interlace = header
    assert (depth, colour, interlace) == (8, 6, 0), f"{path} is not an 8 bit RGBA PNG without interlacing"
    raw, stride, rows, above = zlib.decompress(b"".join(idat)), width * 4, [], bytearray(width * 4)
    for y in range(height):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            left = line[i - 4] if i >= 4 else 0
            corner = above[i - 4] if i >= 4 else 0
            if kind == 1:
                line[i] = (line[i] + left) & 255
            elif kind == 2:
                line[i] = (line[i] + above[i]) & 255
            elif kind == 3:
                line[i] = (line[i] + (left + above[i]) // 2) & 255
            elif kind == 4:
                guess = left + above[i] - corner
                near = min((abs(guess - left), left), (abs(guess - above[i]), above[i]), (abs(guess - corner), corner))[1]
                line[i] = (line[i] + near) & 255
        rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(width)])
        above = line
    return rows


def _wheel_rows():
    """NEED A RIDE: the wheel's coverage, as rasterized (its alpha), unchanged"""
    rows = _read_png(WHEEL)
    assert len(rows) == SIZE and all(len(row) == SIZE for row in rows), f"{WHEEL} is not {SIZE} by {SIZE}"
    return [[pixel[3] for pixel in row] for row in rows]


def _helmet_rows():
    """the DOWNED helmet (the owner's art, port/assets/hud/helmet-downed.svg without its ring), as rasterized: its alpha.
    The game draws the six ring segments itself (ae_revive_hud.c); the helmet's own square is 430 of the art's pixels"""
    rows = _read_png(HELMET)
    assert len(rows) == SIZE and all(len(row) == SIZE for row in rows), f"{HELMET} is not {SIZE} by {SIZE}"
    return [[pixel[3] for pixel in row] for row in rows]


def _of_rows(rows):
    """a shape from a picture's coverage (its alpha across the texels, blended, half or more is in): what an outline
    is grown from, to the same range as the others'"""
    def at(x, y):
        return rows[max(0, min(SIZE - 1, y))][max(0, min(SIZE - 1, x))]

    def shape(px, py):
        u, v = px * SIZE / UNITS - 0.5, py * SIZE / UNITS - 0.5
        x, y = math.floor(u), math.floor(v)
        fx, fy = u - x, v - y
        top = at(x, y) * (1 - fx) + at(x + 1, y) * fx
        bottom = at(x, y + 1) * (1 - fx) + at(x + 1, y + 1) * fx
        return top * (1 - fy) + bottom * fy >= 127.5
    return shape


def _grown(shape, reach=1.5):
    """the shape, its edge moved out by reach: a mark's dark outline"""
    steps = [(reach * math.cos(i * math.pi / 8.0), reach * math.sin(i * math.pi / 8.0)) for i in range(16)]
    return lambda px, py: shape(px, py) or any(shape(px + x, py + y) for x, y in steps)


def _wheel_outline_rows():
    """NEED A RIDE's dark outline: its coverage grown, as the other outlines are"""
    return coverage(_grown(_of_rows(_wheel_rows())))


GLYPHS = [("overshield", overshield), ("camo", camo), ("power_weapon", power_weapon), ("dead_ally", dead_ally),
          ("dead_ally_outline", dead_ally_outline),
          ("mark_enemy", mark_enemy), ("mark_enemy_outline", _grown(_mark_enemy_solid)),
          ("mark_item", mark_item), ("mark_item_outline", _grown(_mark_item_solid)),
          ("mark_place", mark_place), ("mark_place_outline", _grown(_mark_place_solid)),
          # (THE CALLOUT WHEEL'S and the markers' own: each followed by its dark outline; ENEMY and GOING are the marks')
          ("call_attack", _call_attack), ("call_attack_outline", _grown(_call_attack)),
          ("call_defend", _call_defend), ("call_defend_outline", _grown(_call_defend)),
          ("call_watch", _call_watch), ("call_watch_outline", _grown(_call_watch_solid)),
          ("call_help", _call_help), ("call_help_outline", _grown(_call_help)),
          ("call_smart", _call_smart), ("call_smart_outline", _grown(_call_smart)),
          ("call_flag", _call_flag_solid), ("call_flag_outline", _grown(_call_flag_solid)),
          ("call_ride", _wheel_rows), ("call_ride_outline", _wheel_outline_rows),
          # (the owner's DOWNED helmet: a picture, its dark outline drawn by the game as offsets)
          ("helmet_downed", _helmet_rows)]


def coverage(shape):
    if shape in (_wheel_rows, _wheel_outline_rows, _helmet_rows):  # (a whole picture, not a shape to sample)
        return shape()
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
             "them from plain geometry (CALL_RIDE's wheel from its coverage picture): change that, not this",
             "(tools/test_ae_hud_settings.py checks).",
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
