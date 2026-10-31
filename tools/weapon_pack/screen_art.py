"""Our own art for the rifle's rear screen (nothing traced or copied): bold segment digits 0-9 and an 8-frame
compass, written as colour plates (blue background, one row of 128 x 128 frames) for the bitmap compiler.

Geometry (from the model): a digit quad shows texture x 24..104 of its cell and the two digit quads overlap by a
third, so a glyph keeps to 44 px wide (centred on x 64); its top 30 % lies under the compass quad, so the glyph
uses y 40..122. The compass quad shows the whole cell over the panel's full width, the panel's label and logo sit
in its top corners and the digits under its lower part: the dial keeps to a 70 px circle centred at (64, 50).

Usage: screen_art.py <folder>   writes <folder>/digits.png and <folder>/compass.png
"""
import math
import os
import sys

SIZE = 128
SAMPLES = 4                  # supersampling
DIGIT = (150, 230, 255)      # bright cyan-white, the stock HUD's family
RING = (40, 120, 170)
NEEDLE = (150, 230, 255)
TAIL = (40, 110, 150)
BLUE = (0, 0, 255)           # a colour plate's background
SEGMENTS = {0: "abcdef", 1: "bc", 2: "abged", 3: "abgcd", 4: "fgbc", 5: "afgcd", 6: "afgedc", 7: "abc",
            8: "abcdefg", 9: "abcdfg"}


def digit(value):
    """a blocky segment glyph: the segments' union with no gaps, square corners, one stroke weight"""
    from PIL import Image, ImageDraw
    samples = SAMPLES
    image = Image.new("RGB", (SIZE * samples, SIZE * samples), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    w, t = 44 * samples, 12 * samples          # glyph width, stroke
    x0 = 64 * samples - w // 2
    x1 = x0 + w
    y0, y1 = 40 * samples, 122 * samples
    ym0 = (y0 + y1) // 2 - t // 2
    ym1 = ym0 + t
    segments = {"a": (x0, y0, x1, y0 + t), "g": (x0, ym0, x1, ym1), "d": (x0, y1 - t, x1, y1),
                "f": (x0, y0, x0 + t, ym1), "b": (x1 - t, y0, x1, ym1), "e": (x0, ym0, x0 + t, y1),
                "c": (x1 - t, ym0, x1, y1)}
    for segment in SEGMENTS[value]:
        draw.rectangle(segments[segment], fill=DIGIT)
    if value == 1:   # a one stands in the middle, as a bar
        image = Image.new("RGB", (SIZE * samples, SIZE * samples), (0, 0, 0))
        draw = ImageDraw.Draw(image)
        draw.rectangle((64 * samples - t // 2 + 6 * samples, y0, 64 * samples + t // 2 + 6 * samples, y1),
                       fill=DIGIT)
        draw.rectangle((64 * samples - t // 2 - 4 * samples, y0, 64 * samples + 6 * samples, y0 + t), fill=DIGIT)
    return image.resize((SIZE, SIZE), Image.LANCZOS)


def compass(frame):
    """frame k: north at k x 45 degrees anticlockwise from up"""
    from PIL import Image, ImageDraw
    samples = SAMPLES
    image = Image.new("RGB", (SIZE * samples, SIZE * samples), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    cx, cy = 64 * samples, 50 * samples
    r = 33 * samples
    rt = 5 * samples
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=RING, width=rt)
    a = math.radians(90 + 45 * frame)
    ux, uy = math.cos(a), -math.sin(a)
    px, py = -uy, ux
    tip = (cx + ux * 30 * samples, cy + uy * 30 * samples)
    back = (cx - ux * 19 * samples, cy - uy * 19 * samples)
    wdt = 12 * samples
    left = (cx + px * wdt, cy + py * wdt)
    right = (cx - px * wdt, cy - py * wdt)
    draw.polygon([tip, left, (cx, cy), right], fill=NEEDLE)
    draw.polygon([back, left, (cx, cy), right], fill=TAIL)
    # a notch on the ring where north is
    nt = (cx + ux * (r + 2 * samples), cy + uy * (r + 2 * samples))
    nb = (cx + ux * (r - 8 * samples), cy + uy * (r - 8 * samples))
    draw.line([nb, nt], fill=NEEDLE, width=7 * samples)
    return image.resize((SIZE, SIZE), Image.LANCZOS)


def plate(frames):
    """the frames in a row on the plate's blue, a pixel of it around each (a frame's own pure blue made 0 0 254)"""
    from PIL import Image
    width = 1 + len(frames) * (SIZE + 1)
    image = Image.new("RGB", (width, SIZE + 2), BLUE)
    for index, frame in enumerate(frames):
        pixels = frame.load()
        for y in range(SIZE):
            for x in range(SIZE):
                if pixels[x, y] == BLUE:
                    pixels[x, y] = (0, 0, 254)
        image.paste(frame, (1 + index * (SIZE + 1), 1))
    return image


def main(arguments):
    if len(arguments) != 1:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    os.makedirs(arguments[0], exist_ok=True)
    plate([digit(value) for value in range(10)]).save(os.path.join(arguments[0], "digits.png"))
    plate([compass(frame) for frame in range(8)]).save(os.path.join(arguments[0], "compass.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
