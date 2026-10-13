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

Writes, for the bitmap compiler (2D textures, 32-bit, mipmapped), 4 texels a HUD unit (drawn at a scale of 0.25):
  br_meter_smooth.png   512 x 128: alpha = the rounds; red and blue = each round's fill threshold, carried out to
      the nearest round's over the empty texels, so that filtering and mip levels read a round's own threshold at
      its edge; green = the rounds' coverage (as the high-resolution HUD's meters: port/linux/src/hud_hires.h)
  br_alphas_smooth.png  512 x 128: the same shapes, colour and alpha both the shape (the low-ammo flash, which is
      drawn by its colour, as the HUD's own alpha sheets are)

Usage: counter_art.py <folder>
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


def rounds():
    """each round: (number, bottom-left x, top y), in HUD units"""
    for row in range(ROWS):
        for column in range(COLUMNS):
            yield (row * COLUMNS + column + 1,
                   LEFT + (2 - row) * ROW * LEAN + column * PITCH + (column // GROUP) * GROUP_GAP, TOP + row * ROW)


def sheets():
    """(the meter sheet, the alpha sheet): height x width x 4 arrays of bytes, red green blue alpha"""
    import numpy as np
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
    for n, left, top in rounds():
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
    if len(arguments) != 1:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    from PIL import Image
    os.makedirs(arguments[0], exist_ok=True)
    meter, alphas = sheets()
    Image.fromarray(meter, "RGBA").save(os.path.join(arguments[0], "br_meter_smooth.png"))
    Image.fromarray(alphas, "RGBA").save(os.path.join(arguments[0], "br_alphas_smooth.png"))
    right = max(left for _, left, _ in rounds()) + WIDTH + HEIGHT * LEAN
    print("rounds %d, sheet %dx%d (%g x %g HUD units), art to x %.1f, y %.1f..%.1f" % (
        COLUMNS * ROWS, WIDTH_TEXELS, HEIGHT_TEXELS, WIDTH_TEXELS / TEXELS, HEIGHT_TEXELS / TEXELS, right, TOP,
        TOP + (ROWS - 1) * ROW + HEIGHT))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
