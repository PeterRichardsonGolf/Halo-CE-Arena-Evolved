#!/usr/bin/env python3
"""Arena Evolved's CROSSHAIR SHAPE LIBRARY (the code-drawn crosshairs, port/linux/game/ae_crosshair_rules.c): 37 shapes,
each authored once as centre-line primitives on a 64 unit canvas (x right, y down, from the centre), curated from
Kenney's Crosshair Pack (CC0: https://kenney.nl, credited in CREDITS.md) by reading its outlines' coordinates; no
picture or SVG is read at run time and none is shipped, only this table.

A shape is parametric lines (inner and outer sets: a layout, offset, length, thickness) + a centre (dot or plus) + a
frame of primitives (rings, arcs, segments, polylines, wedges, squares, corner brackets). The lines answer the
editor's INNER and OUTER rows; the frame answers FRAME.

    python3 tools/ae_crosshair_shapes.py            writes port/linux/game/ae_crosshair_shapes.inc
    python3 tools/ae_crosshair_shapes.py --check    fails when the file is not what this table makes
"""
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "port" / "linux" / "game" / "ae_crosshair_shapes.inc"

LAYOUTS = ["CROSS", "TEE", "X", "SIDES", "UPDOWN", "TRIPOD"]
FAMILIES = ["LINES", "CHEVRONS", "RINGS", "SHAPES"]
KINDS = ["RING", "ARC", "SEG", "POLY", "WEDGE", "SQUARE", "BRACKETS"]
CENTRES = ["NONE", "DOT", "PLUS"]

P = []


def L(layout, off, length, th=4.0):
    return (LAYOUTS.index(layout), off, length, th)


def add(name, src, fam, centre=None, inner=None, outer=None, frame=(), fth=4.0):
    P.append(dict(name=name, src=src, fam=FAMILIES.index(fam), centre=centre, inner=inner, outer=outer,
                  frame=list(frame), fth=fth))


def chev(tip, back, half, ang):
    c, s = math.cos(math.radians(ang)), math.sin(math.radians(ang))

    def pt(along, side):
        return (along * c - side * s, along * s + side * c)
    return ("POLY", [pt(back, -half), pt(tip, 0), pt(back, half)], False)


# LINES
add("DOT", "000", "LINES", centre=("DOT", 2.2))
add("PLUS", "001 015 192", "LINES", centre=("PLUS", 5, 3))
add("CROSS", "020 022", "LINES", inner=L("CROSS", 10, 16))
add("TWIN CROSS", "016 023", "LINES", centre=("DOT", 2), inner=L("CROSS", 6, 6), outer=L("CROSS", 18, 10))
add("TEE", "132 133 134", "LINES", inner=L("TEE", 10, 16))
add("X", "035 036 037 038 143-148", "LINES", inner=L("X", 9, 18))
add("VEE", "039 190", "LINES", centre=("DOT", 2), frame=[("SEG", 10, 5, 22, -7), ("SEG", -10, 5, -22, -7)])
add("TRIPOD", "056 059", "LINES", inner=L("TRIPOD", 3, 20))
add("LEVEL", "193 194", "LINES", centre=("DOT", 2), inner=L("SIDES", 8, 22, 2.5))
add("PILLARS", "196 197 198 199", "LINES", centre=("DOT", 2.2), frame=[("SEG", 22, -17, 22, 17), ("SEG", -22, -17, -22, 17)])
# CHEVRONS
add("ARROWS IN", "041 061 062 136", "CHEVRONS", centre=("DOT", 2), frame=[chev(8, 14, 6, 0), chev(8, 14, 6, 180)], fth=3)
add("ARROWS OUT", "040 042 060", "CHEVRONS", centre=("DOT", 2), frame=[chev(16, 10, 6, 0), chev(16, 10, 6, 180)], fth=3)
add("FOUR ARROWS", "044 141 179", "CHEVRONS", centre=("DOT", 2), frame=[chev(24, 19, 5, a) for a in (0, 90, 180, 270)], fth=3)
add("CARET", "063 064", "CHEVRONS", frame=[("POLY", [(-7, 3), (0, -4), (7, 3)], False)], fth=3)
add("POINTERS", "135 191", "CHEVRONS",
    frame=[("WEDGE", [(7, 0), (26, -6), (22, 0), (26, 6)]), ("WEDGE", [(-7, 0), (-26, -6), (-22, 0), (-26, 6)])])
# RINGS
add("RING", "086 087 088 092 172 173", "RINGS", centre=("DOT", 2), frame=[("RING", 22)])
add("BROKEN RING", "089 090 091", "RINGS", centre=("DOT", 2), frame=[("ARC", 26, a - 32, a + 32) for a in (0, 90, 180, 270)])
add("ARCS", "011 139 165 189", "RINGS", centre=("DOT", 2), frame=[("ARC", 27, -48, 48), ("ARC", 27, 132, 228)])
add("SCOPE", "006 046 073 113", "RINGS", frame=[("RING", 27)], inner=L("CROSS", 18, 14))
add("SIGHT", "026 028 107 108 111 112", "RINGS", frame=[("RING", 27)], inner=L("CROSS", 5, 27))
add("TARGET", "007 009", "RINGS", frame=[("RING", 28), ("RING", 15)], inner=L("CROSS", 8, 24))
add("BULLSEYE", "068 174", "RINGS", centre=("PLUS", 4, 3), frame=[("RING", 27), ("RING", 15)])
add("ORBIT", "027 033 072", "RINGS", centre=("DOT", 2), frame=[("RING", 27)] + [("ARC", 12, a - 30, a + 30) for a in (45, 135, 225, 315)])
add("HORIZON", "030 053", "RINGS", centre=("DOT", 2), inner=L("SIDES", 10, 22), frame=[("ARC", 27, 196, 344), ("ARC", 27, 16, 164)])
add("WHEEL", "051 070 071", "RINGS", centre=("DOT", 2), inner=L("X", 17, 13), frame=[("ARC", 27, a - 30, a + 30) for a in (0, 90, 180, 270)])
add("COMPASS", "010 012 032 052 069", "RINGS", frame=[("ARC", 27, a - 34, a + 34) for a in (45, 135, 225, 315)] + [("RING", 10)],
    inner=L("CROSS", 16, 12))
add("PIP", "175 176 177", "RINGS", centre=("DOT", 1.8), frame=[("RING", 8)], inner=L("CROSS", 9, 6, 3))
add("LOCK ON", "160 161", "RINGS", centre=("DOT", 2),
    frame=[("RING", 25), ("BRACKETS", 30, 6)] + [("ARC", 12, a - 30, a + 30) for a in (0, 90, 180, 270)], fth=3)
# SHAPES
add("CORNERS", "080 081 085 125-128", "SHAPES", frame=[("BRACKETS", 24, 12)])
add("BOX", "083 084 104 129-131", "SHAPES", centre=("DOT", 2), frame=[("SQUARE", 24)], inner=L("CROSS", 14, 10))
add("OCTAGON", "100 120-124", "SHAPES", centre=("DOT", 1.8),
    frame=[("POLY", [(12 * math.cos(math.radians(a + 22.5)), 12 * math.sin(math.radians(a + 22.5))),
                     (12 * math.cos(math.radians(a + 67.5)), 12 * math.sin(math.radians(a + 67.5)))], False)
           for a in (0, 90, 180, 270)], fth=3.5)
add("DIAMOND", "018 140 142", "SHAPES", centre=("DOT", 2), frame=[("POLY", [(0, -24), (24, 0), (0, 24), (-24, 0)], True)])
add("HEXAGON", "017", "SHAPES", centre=("DOT", 2), frame=[("POLY", [(-8, -22), (8, -22), (28, 0), (8, 22), (-8, 22), (-28, 0)], True)])
add("TRIANGLE", "055 057 170", "SHAPES", centre=("DOT", 2), frame=[("POLY", [(0, -26), (25, 15), (-25, 15)], True)])
add("SIDE BRACKETS", "019 185 187", "SHAPES", centre=("DOT", 2.2),
    frame=[("POLY", [(14, -15), (21, -8), (21, 8), (14, 15)], False), ("POLY", [(-14, -15), (-21, -8), (-21, 8), (-14, 15)], False)])
add("WINGS", "181 182", "SHAPES", centre=("DOT", 2),
    frame=[("POLY", [(-32, -7), (-12, -7), (0, -18), (12, -7), (32, -7)], False),
           ("POLY", [(-32, 7), (-12, 7), (0, 18), (12, 7), (32, 7)], False)], fth=3.5)
add("RANGE", "097 098 099 150", "SHAPES",
    frame=[("SEG", 0, -20, 0, 22), ("SEG", -12, -10, 12, -10), ("SEG", -8, -1, 8, -1), ("SEG", -6, 8, 6, 8),
           ("SEG", -4, 18, 4, 18), ("POLY", [(-5, -21), (0, -26), (5, -21)], False)], fth=3)


def num(value):
    text = ("%.4f" % value).rstrip("0").rstrip(".")
    text = text if text not in ("-0", "") else "0"
    return (text if "." in text else text + ".0") + "f"


def make():
    points, prims, rows = [], [], []
    for shape in P:
        first = len(prims)
        for prim in shape["frame"]:
            kind = prim[0]
            row = dict(kind=kind, closed=0, count=0, first=0, a=0.0, b=0.0, c=0.0)
            if kind == "RING":
                row["a"] = prim[1]
            elif kind == "ARC":
                row["a"], row["b"], row["c"] = prim[1], prim[2], prim[3]
            elif kind == "SEG":
                row["first"], row["count"] = len(points) // 2, 2
                points += [prim[1], prim[2], prim[3], prim[4]]
            elif kind in ("POLY", "WEDGE"):
                pts = prim[1]
                row["first"], row["count"], row["closed"] = len(points) // 2, len(pts), 1 if kind == "POLY" and prim[2] else 0
                for x, y in pts:
                    points += [x, y]
            elif kind == "SQUARE":
                row["a"] = prim[1]
            elif kind == "BRACKETS":
                row["a"], row["b"] = prim[1], prim[2]
            prims.append(row)
        rows.append((shape, first, len(prims) - first))
    out = ["/* GENERATED by tools/ae_crosshair_shapes.py: do not edit. The crosshair shape library (Kenney's Crosshair Pack, CC0, curated). */",
           "", "static const float shape_points[] =", "{"]
    for index in range(0, len(points), 8):
        out.append("\t" + ", ".join(num(v) for v in points[index:index + 8]) + ",")
    out += ["\t0.0f", "};", "", "static const struct ae_xh_prim shape_prims[] =", "{"]
    for p in prims:
        out.append("\t{ AE_XH_PRIM_%s, %d, %d, %d, %s, %s, %s }," % (p["kind"], p["closed"], p["count"], p["first"],
                                                                      num(p["a"]), num(p["b"]), num(p["c"])))
    out += ["\t{ 0, 0, 0, 0, 0.0f, 0.0f, 0.0f }", "};", "", "static const struct ae_xh_shape shape_table[] =", "{"]
    for shape, first, count in rows:
        centre = shape["centre"]
        ckind = CENTRES.index(centre[0]) if centre else 0
        cone = centre[1] if centre else 0.0
        ctwo = centre[2] if centre and len(centre) > 2 else 0.0
        inner = shape["inner"] or (-1, 0, 0, 0)
        outer = shape["outer"] or (-1, 0, 0, 0)
        out.append("\t{ \"%s\", %d, %d, %s, %s, { %d, %s, %s, %s }, { %d, %s, %s, %s }, %d, %d, %s }," % (
            shape["name"], shape["fam"], ckind, num(cone), num(ctwo), inner[0], num(inner[1]), num(inner[2]), num(inner[3]),
            outer[0], num(outer[1]), num(outer[2]), num(outer[3]), first, count, num(shape["fth"])))
    out += ["};", "", "#define AE_XH_SHAPE_COUNT ((int)(sizeof(shape_table) / sizeof(shape_table[0])))", ""]
    return "\n".join(out)


def main():
    text = make()
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text() != text:
            print("ae_crosshair_shapes.inc is not what tools/ae_crosshair_shapes.py makes")
            return 1
        print("ae_crosshair_shapes.inc ok (%d shapes)" % len(P))
        return 0
    OUT.write_text(text)
    print("wrote %s (%d shapes)" % (OUT, len(P)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
