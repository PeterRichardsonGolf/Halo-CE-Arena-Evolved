"""The picture of the rifle that the "hold to swap for" and pickup prompts show: a side view drawn from the model's
geometry in the stock HUD's style (a grey fill, a light outline and inner lines), 86 texels wide.

It is drawn from nothing but the model's vertices and triangles (CMT's battle rifle, read by make_pack.py through
Invader's field editor as text: "part N", a position a line, "tris", three vertex indices a line): no pixel of the
game's HUD is read or kept. The picture goes out as a colour plate (the sprite on the plate's blue, a pixel of it
around) for the bitmap compiler, which makes a sprite sheet holding only it; the pack's weapon entry names that
sheet's sequence 0.

Usage: hud_sheet.py <geometry text> <out png>      prints the picture's size
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek


class Refused(Exception):
    """no picture was made, with the reason"""


ICON_WIDTH, ICON_SAMPLES = 86, 6
FILL, LINE = 100.0, 225.0              # the fill's and the lines' brightness
DEPTH_STEP = 0.003                     # a step in depth this large between neighbours is an inner line
BLUE = (0, 0, 255, 255)                # a colour plate's background


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


def icon(geometry, width=ICON_WIDTH):
    """the picture: rows x width bytes (86 the battle rifle's), 0 outside the weapon (the bitmap's colour and its
    alpha are both this)"""
    import numpy as np
    if not isinstance(width, int) or not 16 <= width <= 256:
        raise Refused("an icon %r texels wide" % (width,))
    parts = _parts(geometry)
    every = np.array([v for p in parts for v in p["v"]])
    xmin, xmax, zmin, zmax = every[:, 0].min(), every[:, 0].max(), every[:, 2].min(), every[:, 2].max()
    samples = ICON_SAMPLES
    scale = (width - 4) * samples / (xmax - xmin)
    rows = int((zmax - zmin) * scale / samples) + 5
    depth = np.full((rows * samples, width * samples), 1e9)
    ident = np.full((rows * samples, width * samples), -1)
    for part_index, part in enumerate(parts):
        v = np.array(part["v"])
        indices = [int(i) for triangle in part["t"] for i in triangle]
        for k in range(len(indices) - 2):
            a, b, c = indices[k], indices[k + 1], indices[k + 2]
            if max(a, b, c) >= len(v) or min(a, b, c) < 0 or a == b or b == c or a == c:
                continue
            corners = np.array([[(v[i][0] - xmin) * scale + 2 * samples, (zmax - v[i][2]) * scale + 2 * samples,
                                 v[i][1]]
                          for i in (a, b, c)])
            x0 = int(max(0, np.floor(corners[:, 0].min())))
            x1 = int(min(width * samples - 1, np.ceil(corners[:, 0].max())))
            y0 = int(max(0, np.floor(corners[:, 1].min())))
            y1 = int(min(rows * samples - 1, np.ceil(corners[:, 1].max())))
            if x1 < x0 or y1 < y0:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            area = (corners[1, 1] - corners[2, 1]) * (corners[0, 0] - corners[2, 0]) + \
                (corners[2, 0] - corners[1, 0]) * (corners[0, 1] - corners[2, 1])
            if abs(area) < 1e-9:
                continue
            w0 = ((corners[1, 1] - corners[2, 1]) * (xs - corners[2, 0]) +
                  (corners[2, 0] - corners[1, 0]) * (ys - corners[2, 1])) / area
            w1 = ((corners[2, 1] - corners[0, 1]) * (xs - corners[2, 0]) +
                  (corners[0, 0] - corners[2, 0]) * (ys - corners[2, 1])) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
            z = w0 * corners[0, 2] + w1 * corners[1, 2] + w2 * corners[2, 2]
            sub = depth[y0:y1 + 1, x0:x1 + 1]
            near = inside & (z < sub)
            sub[near] = z[near]
            ident[y0:y1 + 1, x0:x1 + 1][near] = part_index
    solid = ident >= 0
    depths = np.where(solid, depth, 0.0)
    edge = np.zeros_like(solid)
    for axis in (0, 1):
        for shift in (1, -1):
            beside_solid = np.roll(solid, shift, axis)
            beside_depth = np.roll(depths, shift, axis)
            edge |= solid & (~beside_solid)                                   # the outline
            edge |= solid & beside_solid & (np.abs(depths - beside_depth) > DEPTH_STEP)   # the inner lines
    thick = edge.copy()
    for axis in (0, 1):
        for shift in (1, -1, 2, -2):
            thick |= np.roll(edge, shift, axis)
    thick &= solid
    value = np.where(thick, LINE, np.where(solid, FILL, 0.0))
    picture = value.reshape(rows, samples, width, samples).mean(axis=(1, 3))
    cover = solid.reshape(rows, samples, width, samples).mean(axis=(1, 3))
    picture = np.where(cover > 0.05, picture / np.maximum(cover, 0.05) * np.minimum(1, cover * 1.3), 0)
    picture = picture.clip(0, 255).astype(np.uint8)
    return picture


def plate(picture):
    """the picture as a sprite on a colour plate: red, green, blue and alpha all the picture"""
    import numpy as np
    from PIL import Image
    rows, columns = picture.shape
    image = Image.new("RGBA", (columns + 2, rows + 2), BLUE)
    image.paste(Image.fromarray(np.dstack([picture] * 4), "RGBA"), (1, 1))
    return image


def place_by_corner(tag):
    """a sprite sheet tag (the bitmap compiler's, of one sequence with one sprite) with that sprite's registration
    point set to its top left corner, as the prompts place their pictures; the compiler sets the sprite's middle
    and Invader's field editor will not change it"""
    layout = hek.BitmapTag(tag)
    sprite = layout.first_sprite_offset()
    if layout.sequence_count != 1 or sprite + hek.BITMAP_SPRITE_SIZE != len(tag) - layout.bitmap_count * \
            hek.BITMAP_DATA_SIZE:
        raise hek.Refused("not a sheet of one sequence with one sprite")
    data = bytearray(tag)
    struct.pack_into(">2f", data, sprite + 24, 0.0, 0.0)
    return bytes(data)


def main(arguments):
    if len(arguments) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    with open(arguments[0]) as file:
        geometry = file.read()
    try:
        picture = icon(geometry)
    except Refused as refusal:
        print("hud_sheet: refused: %s" % refusal, file=sys.stderr)
        return 1
    plate(picture).save(arguments[1])
    print("icon %d x %d" % (picture.shape[1], picture.shape[0]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
