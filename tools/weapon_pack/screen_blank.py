"""A source picture with rectangles of it cleared (transparent black): for text on a source's texture that may not
ship, such as a name or an identifier painted on a gun's screen. Every other pixel is the source's, as hek.py
decodes its first bitmap's largest level; nothing is drawn.

    screen_blank.py <bitmap tag> <x0,y0,x1,y1>... <out.png>     (each rectangle's corners included)
"""
import sys

import hek


class Refused(Exception):
    """no picture was made, with the reason"""


def picture(tag, regions):
    """the source bitmap's first picture (RGBA rows x columns) with each region (x0, y0, x1, y1; inclusive) cleared"""
    import numpy as np
    columns, rows, rgba = hek.first_bitmap_rgba(tag)
    pixels = np.frombuffer(rgba, np.uint8).reshape(rows, columns, 4).copy()
    for region in regions:
        if len(region) != 4:
            raise Refused("a region is four numbers: %r" % (region,))
        x0, y0, x1, y1 = (int(value) for value in region)
        if not (0 <= x0 <= x1 < columns and 0 <= y0 <= y1 < rows):
            raise Refused("the region %r is not inside the %d x %d picture" % (region, columns, rows))
        pixels[y0:y1 + 1, x0:x1 + 1] = 0
    return pixels


def main(arguments):
    from PIL import Image
    if len(arguments) < 4:
        raise SystemExit(__doc__)
    regions = [tuple(part.split(",")) for part in arguments[2:-1]]
    try:
        pixels = picture(open(arguments[1], "rb").read(), regions)
    except (Refused, hek.Refused) as refusal:
        raise SystemExit("screen_blank.py: %s" % refusal)
    Image.fromarray(pixels, "RGBA").save(arguments[-1])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
