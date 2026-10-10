"""The rifle's icon sheet: the picture the "hold to swap for" and pickup prompts show, a side view of the rifle
drawn from its model's geometry in the stock HUD's grey fill and light outline, on a copy of the stock ammunition
meter sheet (A8Y8, 256 x 256: low byte the meter's fill threshold, high byte the shape).

  - The stock sheet's sequence 1 area (0, 0)-(114, 27) is redrawn as 36 ticks, 12 groups of 3, in two rows (one
    tick a round, tick n holding threshold 4 n). The approved rifle draws its rounds from counter_art.py's sharp
    sheet instead; these ticks stay in the sheet as they were made.
  - The icon goes in the free area below, and the sheet's first sprite (sequence 0's) is set to its rectangle.

The stock sheet and the model's geometry are read from the player's own game files and CMT's model by
make_pack.py (which asks Invader's field editor for the geometry as text: "part N", a position a line, "tris",
three vertex indices a line); nothing of them is kept in this file.

Usage: hud_sheet.py <stock sheet raw> <geometry text> <out raw>      prints the icon's rectangle
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek


class Refused(Exception):
    """no sheet was made, with the reason"""


SHEET = 256
METER_AREA = (28, 116)                 # rows, columns of the stock meter's place
TICK_PITCH, TICK_GAP = 5.0, 3.0
TICK_LEFTS, TICK_TOPS = (8.0, 3.5), (2, 11)
ICON_WIDTH, ICON_SAMPLES = 86, 6
ICON_TOP, ICON_LEFT = 100, 2
ALPHA_SHEET_SIZE = 22528               # the stock alpha sheet's pixel bytes
ALPHA_PLATE = (2048, 32, 128)          # where its meter plate is: byte offset, rows, columns


def _tick(thresholds, alpha, x_bottom_left, y_top, threshold):
    """a slanted bar like the stock one: 7 rows high, 2.6 wide, leaning right 0.5 a row going up"""
    import numpy as np
    samples = 8
    h, w, lean = 7, 2.6, 0.5
    x0 = int(x_bottom_left) - 1
    cover = np.zeros((h, 10))
    for sy in range(h * samples):
        yy = (sy + 0.5) / samples                     # 0 top .. 7 bottom
        left = x_bottom_left + (h - yy) * lean
        for sx in range(10 * samples):
            xx = x0 + (sx + 0.5) / samples
            if left <= xx < left + w:
                cover[sy // samples, sx // samples] += 1
    cover /= samples * samples
    for r in range(h):
        for c in range(10):
            a = int(round(cover[r, c] * 255))
            if a > 40:
                y, x = y_top + r, x0 + c
                if a > alpha[y, x]:
                    alpha[y, x] = a
                    thresholds[y, x] = threshold


def _parts(geometry):
    parts = []
    current = None
    mode = None
    for line in geometry.splitlines():
        line = line.strip()
        if line.startswith("part"):
            current = {"v": [], "t": []}
            parts.append(current)
            mode = "v"
        elif line == "tris":
            mode = "t"
        elif line:
            if current is None:
                raise Refused("geometry before its part line")
            (current["v"] if mode == "v" else current["t"]).append([float(x) for x in line.split()])
    if not parts or not any(part["v"] for part in parts):
        raise Refused("no geometry")
    return parts


def icon_sheet(stock, geometry):
    """(the sheet's raw bytes, the icon's rectangle (left, top, right, bottom) in texels)"""
    import numpy as np
    if len(stock) != SHEET * SHEET * 2:
        raise Refused("the stock sheet is not 256 x 256 A8Y8: %d bytes" % len(stock))
    d = np.frombuffer(stock, dtype=np.uint8).reshape(SHEET, SHEET, 2).copy()
    T, A = d[:, :, 0], d[:, :, 1]
    # ---- the meter
    T[0:METER_AREA[0], 0:METER_AREA[1]] = 0
    A[0:METER_AREA[0], 0:METER_AREA[1]] = 0
    for n in range(1, 37):
        row, col = (n - 1) // 18, (n - 1) % 18
        x = TICK_LEFTS[row] + col * TICK_PITCH + (col // 3) * TICK_GAP
        _tick(T, A, x, TICK_TOPS[row], 4 * n)
    # ---- the icon
    parts = _parts(geometry)
    allv = np.array([v for p in parts for v in p["v"]])
    xmin, xmax, zmin, zmax = allv[:, 0].min(), allv[:, 0].max(), allv[:, 2].min(), allv[:, 2].max()
    W = ICON_WIDTH
    S = ICON_SAMPLES
    scale = (W - 4) * S / (xmax - xmin)
    Hpx = int((zmax - zmin) * scale / S) + 5
    depth = np.full((Hpx * S, W * S), 1e9)
    ident = np.full((Hpx * S, W * S), -1)
    for pi, p in enumerate(parts):
        v = np.array(p["v"])
        idx = [int(i) for t in p["t"] for i in t]
        for k in range(len(idx) - 2):
            a, b, c = idx[k], idx[k + 1], idx[k + 2]
            if max(a, b, c) >= len(v) or min(a, b, c) < 0 or a == b or b == c or a == c:
                continue
            P = np.array([[(v[i][0] - xmin) * scale + 2 * S, (zmax - v[i][2]) * scale + 2 * S, v[i][1]]
                          for i in (a, b, c)])
            x0, x1 = int(max(0, np.floor(P[:, 0].min()))), int(min(W * S - 1, np.ceil(P[:, 0].max())))
            y0, y1 = int(max(0, np.floor(P[:, 1].min()))), int(min(Hpx * S - 1, np.ceil(P[:, 1].max())))
            if x1 < x0 or y1 < y0:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            den = (P[1, 1] - P[2, 1]) * (P[0, 0] - P[2, 0]) + (P[2, 0] - P[1, 0]) * (P[0, 1] - P[2, 1])
            if abs(den) < 1e-9:
                continue
            w0 = ((P[1, 1] - P[2, 1]) * (xs - P[2, 0]) + (P[2, 0] - P[1, 0]) * (ys - P[2, 1])) / den
            w1 = ((P[2, 1] - P[0, 1]) * (xs - P[2, 0]) + (P[0, 0] - P[2, 0]) * (ys - P[2, 1])) / den
            w2 = 1 - w0 - w1
            inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
            z = w0 * P[0, 2] + w1 * P[1, 2] + w2 * P[2, 2]
            sub = depth[y0:y1 + 1, x0:x1 + 1]
            near = inside & (z < sub)
            sub[near] = z[near]
            ident[y0:y1 + 1, x0:x1 + 1][near] = pi
    solid = ident >= 0
    dd = np.where(solid, depth, 0.0)
    edge = np.zeros_like(solid)
    for ax in (0, 1):
        for sh in (1, -1):
            n_solid = np.roll(solid, sh, ax)
            n_d = np.roll(dd, sh, ax)
            edge |= solid & (~n_solid)                                   # the outline
            edge |= solid & n_solid & (np.abs(dd - n_d) > 0.003)         # steps in depth: the inner lines
    thick = edge.copy()
    for ax in (0, 1):
        for sh in (1, -1, 2, -2):
            thick |= np.roll(edge, sh, ax)
    thick &= solid
    val = np.where(thick, 225.0, np.where(solid, 100.0, 0.0))
    icon = val.reshape(Hpx, S, W, S).mean(axis=(1, 3))
    cov = solid.reshape(Hpx, S, W, S).mean(axis=(1, 3))
    icon = np.where(cov > 0.05, icon / np.maximum(cov, 0.05) * np.minimum(1, cov * 1.3), 0).clip(0, 255).astype(
        np.uint8)
    IY, IX = ICON_TOP, ICON_LEFT
    T[IY - 2:IY + Hpx + 2, 0:W + 6] = 0
    A[IY - 2:IY + Hpx + 2, 0:W + 6] = 0
    T[IY:IY + Hpx, IX:IX + W] = icon
    A[IY:IY + Hpx, IX:IX + W] = icon
    return d.tobytes(), (IX, IY, IX + W, IY + Hpx)


def alpha_sheet(stock_alphas, sheet):
    """the stock alpha sheet's pixel bytes with its meter plate redrawn as the sheet's ticks (the low-ammo flash's
    shapes for the first HUD; the approved rifle flashes counter_art.py's instead)"""
    import numpy as np
    if len(stock_alphas) != ALPHA_SHEET_SIZE or len(sheet) != SHEET * SHEET * 2:
        raise Refused("not the stock alpha sheet's or the icon sheet's size")
    al = np.frombuffer(stock_alphas, dtype=np.uint8).copy()
    mine = np.frombuffer(sheet, dtype=np.uint8).reshape(SHEET, SHEET, 2)[:, :, 1]
    offset, rows, columns = ALPHA_PLATE
    n = np.zeros((rows, columns), np.uint8)
    n[0:27, 0:115] = mine[1:28, 1:116]
    al[offset:offset + rows * columns] = n.reshape(-1)
    return al.tobytes()


def sheet_tag(stock_tag, raw, rectangle=None):
    """the stock bitmap tag with our pixels; with a rectangle (left, top, right, bottom in texels of a 256 sheet),
    its first sprite set to it, its registration point 0 0"""
    data = bytearray(stock_tag)
    layout = hek.BitmapTag(stock_tag)
    if layout.pixels_size != len(raw):
        raise hek.Refused("the pixels are not the tag's size: %d, not %d" % (len(raw), layout.pixels_size))
    data[layout.pixels_start:layout.pixels_start + layout.pixels_size] = raw
    if rectangle is not None:
        left, top, right, bottom = [float(value) / SHEET for value in rectangle]
        sprite = layout.first_sprite_offset()
        struct.pack_into(">4f", data, sprite + 8, left, right, top, bottom)
        struct.pack_into(">2f", data, sprite + 24, 0.0, 0.0)
    return bytes(data)


def main(arguments):
    if len(arguments) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    with open(arguments[0], "rb") as file:
        stock = file.read()
    with open(arguments[1]) as file:
        geometry = file.read()
    try:
        raw, rectangle = icon_sheet(stock, geometry)
    except Refused as refusal:
        print("hud_sheet: refused: %s" % refusal, file=sys.stderr)
        return 1
    with open(arguments[2], "wb") as file:
        file.write(raw)
    print("icon rect %d %d %d %d" % rectangle)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
