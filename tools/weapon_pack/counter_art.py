"""The battle rifle's ammunition counter (our own art, drawn from nothing): its 36 rounds as 2 rows of 18 in groups
of 3 (a burst is one group), in the style of the high-resolution HUD's assault rifle rounds (slanted, pointed,
flat fill), a rifle round: a little wider, with a longer point.

Measures taken from the high-resolution assault rifle meter (in HUD units, 640 x 480): rounds 7.63 high, leaning
0.479 to the right a unit up, rows 8.5 apart and 4.07 to the left a row down, the top row's top at 1.4; its rounds
are 2.75 wide, 5.15 apart, their point the top 21 %. Ours: its top two rows' places, rounds 2.9 wide, 4.7 apart
with 2.4 more after every third, the point the top 36 %.

The order the rounds go out in is the fill threshold each round's texels hold, nothing else (the meter shows a
texel while its threshold is under 4 x the rounds loaded + 2): round n holds 4 n, and round 1, the last one left,
is the top row's left end, round 36, the first to go, the bottom row's right end, as the assault rifle's sheet
numbers its rounds (its top left 4, row by row to its bottom right 240): the bottom row empties first, from the
right.

Another weapon's counter is the same drawing with other numbers (a layout: rounds, rows, groups, a round's size and
spacing; the battle rifle's are the defaults below). A layout may also move the threshold of the n-th round: always
4 n, so a counter holds at most 63 rounds.

Writes, for the bitmap compiler (2D textures, 32-bit, mipmapped), 4 texels a HUD unit (drawn at a scale of 0.25):
  br_meter_smooth.png   512 x 128: alpha = the rounds; red and blue = each round's fill threshold, carried out to
      the nearest round's over the empty texels, so that filtering and mip levels read a round's own threshold at
      its edge; green = the rounds' coverage (as the high-resolution HUD's meters: port/linux/src/hud_hires.h)
  br_alphas_smooth.png  512 x 128: the same shapes, colour and alpha both the shape (the low-ammo flash, which is
      drawn by its colour, as the HUD's own alpha sheets are)

Usage: counter_art.py <folder> [<layout json>]
"""
import os
import sys

TEXELS = 4                 # texels a HUD unit
WIDTH_TEXELS, HEIGHT_TEXELS = 512, 128
SAMPLES = 6                # supersampling
HEIGHT, LEAN, ROW, TOP = 7.63, 0.479, 8.5, 1.4
WIDTH, PITCH, GROUP_GAP, POINT = 2.9, 4.7, 2.4, 0.36
LEFT = 0.9                 # where a third row's first round's bottom left would be (the assault rifle's)
COLUMNS, ROWS, GROUP = 18, 2, 3
# the names a layout may give, and the battle rifle's values of them
DEFAULTS = {"height": HEIGHT, "lean": LEAN, "row": ROW, "top": TOP, "width": WIDTH, "pitch": PITCH,
            "group_gap": GROUP_GAP, "point": POINT, "left": LEFT, "columns": COLUMNS, "rows": ROWS, "group": GROUP}
MAXIMUM_ROUNDS = 63          # 4 n must fit a byte


class Refused(Exception):
    """no counter was drawn, with the reason"""


def layout_of(layout=None):
    """the battle rifle's numbers with the layout's in their place; a name not in DEFAULTS, a count that is not a
    positive whole number, more rounds than a byte's thresholds hold, or art past the sheet is refused"""
    found = dict(DEFAULTS)
    for name, value in (layout or {}).items():
        if name not in DEFAULTS:
            raise Refused("no such counter number: %s" % name)
        found[name] = value
    for name in ("columns", "rows", "group"):
        if not isinstance(found[name], int) or found[name] < 1:
            raise Refused("%s must be a positive whole number" % name)
    if found["columns"] * found["rows"] > MAXIMUM_ROUNDS:
        raise Refused("%d rounds: a counter holds at most %d" % (found["columns"] * found["rows"], MAXIMUM_ROUNDS))
    places = list(_rounds(found))
    right = max(left for _, left, _ in places) + found["width"] + found["height"] * found["lean"]
    bottom = max(top for _, _, top in places) + found["height"]
    if min(left for _, left, _ in places) < 0 or right > WIDTH_TEXELS / TEXELS or bottom > HEIGHT_TEXELS / TEXELS:
        raise Refused("the rounds run past the %g x %g unit sheet" % (WIDTH_TEXELS / TEXELS, HEIGHT_TEXELS / TEXELS))
    return found


def _rounds(numbers):
    rows, columns, group = numbers["rows"], numbers["columns"], numbers["group"]
    for row in range(rows):
        for column in range(columns):
            # (rows in the assault rifle's places: LEFT is where its third row starts, and each row above starts
            # one row's lean further right)
            yield (row * columns + column + 1,
                   numbers["left"] + (2 - row) * numbers["row"] * numbers["lean"] + column * numbers["pitch"] +
                   (column // group) * numbers["group_gap"], numbers["top"] + row * numbers["row"])


def rounds(layout=None):
    """each round: (number, bottom-left x, top y), in HUD units"""
    return _rounds(layout_of(layout))


def sheets(layout=None):
    """(the meter sheet, the alpha sheet): height x width x 4 arrays of bytes, red green blue alpha"""
    import numpy as np
    numbers = layout_of(layout)
    HEIGHT, LEAN, WIDTH, POINT = numbers["height"], numbers["lean"], numbers["width"], numbers["point"]
    width, height, samples = WIDTH_TEXELS, HEIGHT_TEXELS, SAMPLES
    ys, xs = np.mgrid[0:height * samples, 0:width * samples]
    px = (xs + 0.5) / (TEXELS * samples)
    py = (ys + 0.5) / (TEXELS * samples)                       # HUD units
    alpha = np.zeros((height, width))
    number = np.zeros((height, width), np.int32)
    best = np.full((height, width), 1e9)
    cy, cx = np.mgrid[0:height, 0:width]
    cxu = (cx + 0.5) / TEXELS
    cyu = (cy + 0.5) / TEXELS
    for n, left, top in _rounds(numbers):
        up = (top + HEIGHT) - py                               # units above the round's bottom
        u = px - (left + up * LEAN)                            # across it, the lean taken out
        body = HEIGHT * (1 - POINT)
        half = np.where(up <= body, WIDTH / 2, WIDTH / 2 * (HEIGHT - up) / (HEIGHT * POINT))
        inside = (up >= 0) & (up <= HEIGHT) & (np.abs(u - WIDTH / 2) <= half)
        cover = inside.reshape(height, samples, width, samples).mean(axis=(1, 3))
        alpha = np.maximum(alpha, cover)
        # (the nearest round: by the distance to its middle line)
        upc = np.clip((top + HEIGHT) - cyu, 0, HEIGHT)
        mid = left + upc * LEAN + WIDTH / 2
        d = np.hypot(cxu - mid, np.clip(cyu - (top + HEIGHT), 0, None) + np.clip(top - cyu, 0, None))
        near = d < best
        best[near] = d[near]
        number[near] = n
    a8 = np.round(alpha * 255).astype(np.uint8)
    t8 = (number * 4).astype(np.uint8)
    return np.dstack([t8, a8, t8, a8]), np.dstack([a8, a8, a8, a8])


def main(arguments):
    if len(arguments) not in (1, 2):
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    import json
    from PIL import Image
    layout = json.loads(arguments[1]) if len(arguments) == 2 else None
    try:
        numbers = layout_of(layout)
    except Refused as refusal:
        print("counter_art: refused: %s" % refusal, file=sys.stderr)
        return 1
    os.makedirs(arguments[0], exist_ok=True)
    meter, alphas = sheets(layout)
    Image.fromarray(meter, "RGBA").save(os.path.join(arguments[0], "br_meter_smooth.png"))
    Image.fromarray(alphas, "RGBA").save(os.path.join(arguments[0], "br_alphas_smooth.png"))
    right = max(left for _, left, _ in _rounds(numbers)) + numbers["width"] + numbers["height"] * numbers["lean"]
    print("rounds %d, sheet %dx%d (%g x %g HUD units), art to x %.1f, y %.1f..%.1f" % (
        numbers["columns"] * numbers["rows"], WIDTH_TEXELS, HEIGHT_TEXELS, WIDTH_TEXELS / TEXELS,
        HEIGHT_TEXELS / TEXELS, right, numbers["top"], numbers["top"] + (numbers["rows"] - 1) * numbers["row"] +
        numbers["height"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
