"""The sharp scope's zoom label for another magnification. Takes the scope lines at 4x (scope_hd.py's
br_scope_details_hd.png) and writes them with the digit of the "[2x]" label replaced by another, drawn here: the
digit's box is found in the label (the inked columns after the left bracket, up to the x), cleared, and the new
digit drawn in the same box as lines of a round pen of the old digit's stroke weight (a flat top, a slope and a
bowl), at the label's own brightness. Everything else is copied as it is.

Usage: scope_label.py <in png> <out png> <digit>      (only the 3 is drawn)
"""
import sys


class Refused(Exception):
    """no label was drawn, with the reason"""


X0, X1, Y0, Y1 = 580, 690, 690, 780                 # the label's neighbourhood: "[2x]"
SAMPLES = 8
PEN = 10.0                                           # the old digit's stroke, in texels


def relabel(alpha, digit):
    """(the sheet's alpha with the new digit, what was found: the digit's box and ink); alpha is a 1024 x 1024
    array of bytes and is not changed"""
    import numpy as np
    from PIL import Image, ImageDraw, ImageFilter
    if digit != "3":
        raise Refused("only the 3 is drawn")
    a = np.array(alpha, dtype=np.uint8, copy=True)
    box = a[Y0:Y1, X0:X1]
    cols = np.nonzero(box.max(axis=0) > 60)[0]
    if len(cols) == 0:
        raise Refused("no label where the scope's is")
    runs = []
    start = prev = cols[0]
    for c in cols[1:]:
        if c != prev + 1:
            runs.append((start, prev))
            start = c
        prev = c
    runs.append((start, prev))
    if len(runs) not in (3, 4):                          # [ 2 x ], the digit and the x touching or not
        raise Refused("the label's parts were not found: %d runs of ink" % len(runs))
    g0, g1 = runs[1]
    if len(runs) == 3:                                   # (touching: parted where the least ink is, about the middle)
        ink = box[:, g0:g1 + 1].astype(float).sum(axis=0)
        n = len(ink)
        g1 = g0 + int(n * 0.4) + int(np.argmin(ink[int(n * 0.4):int(n * 0.7)])) - 1
    rows = np.nonzero(box[:, g0:g1 + 1].max(axis=1) > 60)[0]
    r0, r1 = rows[0], rows[-1]
    old = box[r0:r1 + 1, g0:g1 + 1].astype(float) / 255
    level = float(np.percentile(old[old > 0.5], 80))     # the label's brightness
    w, h = int(g1 - g0 + 1), int(r1 - r0 + 1)
    scale = SAMPLES

    def strokes(draw, pen):
        """the digit as lines of a round pen in a w x h box: a flat top, a slope, a bowl"""
        m = pen / 2
        top = [(m, m), (w - m - 1, m), (w * 0.42, h * 0.42)]
        cx, cy, rad = (w - 1) / 2 - 0.5, h - m - (h * 0.58 - 2 * m) / 2 - 0.5, (h * 0.58 - 2 * m) / 2
        rx = (w - 1) / 2 - m + 0.5
        bowl = [(cx + rx * np.sin(t), cy - rad * np.cos(t)) for t in np.linspace(-0.25, np.pi * 1.28, 40)]
        for line in (top, bowl):
            pts = [(x * scale, y * scale) for x, y in line]
            draw.line(pts, fill=255, width=int(pen * scale), joint="curve")
            for x, y in (pts[0], pts[-1]):
                draw.ellipse((x - pen * scale / 2, y - pen * scale / 2, x + pen * scale / 2, y + pen * scale / 2),
                             fill=255)

    big = Image.new("L", (w * scale, h * scale), 0)
    strokes(ImageDraw.Draw(big), PEN)
    g = np.asarray(big.filter(ImageFilter.GaussianBlur(0.35 * scale))).astype(float) / 255
    new = g.reshape(h, scale, w, scale).mean(axis=(1, 3)) * level
    box[r0 - 2:r1 + 3, g0 - 1:g1 + 2] = 0
    box[r0:r1 + 1, g0:g1 + 1] = np.round(new * 255).astype(np.uint8)
    a[Y0:Y1, X0:X1] = box
    found = {"x": int(X0 + g0), "y": int(Y0 + r0), "width": w, "height": h, "runs": len(runs),
             "old_ink": int(round(float(old.sum()))), "brightness": round(level, 2)}
    return a, found


def main(arguments):
    if len(arguments) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    import numpy as np
    from PIL import Image
    source, destination, digit = arguments
    alpha = np.asarray(Image.open(source).convert("RGBA"))[..., 3]
    try:
        out, found = relabel(alpha, digit)
    except Refused as refusal:
        print("scope_label: refused: %s" % refusal, file=sys.stderr)
        return 1
    Image.fromarray(np.dstack([out, out, out, out]), "RGBA").save(destination)
    print("digit box %(width)dx%(height)d at (%(x)d, %(y)d); the old digit's ink %(old_ink)d texels, "
          "brightness %(brightness).2f" % found)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
