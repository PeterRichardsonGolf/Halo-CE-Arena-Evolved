"""The EVO rifle's zoom overlay, from CMT's own magnum (pistol) scope pictures (the Bonus Data release's original data,
used with credit) with the magnification labels left out: the zoom ladder ("2x" .. "1x") at the right and the two lines
of small print at the upper left. A label is taken out whole: each lit patch (lit pixels joined to their neighbours)
lying inside one of the label boxes goes, so the scope's rings and arcs next to them stay whole.

  details  the lines as a 512 x 512 RGBA (colour and alpha both the line's strength), CMT's 296 x 296 picture less a
           3 pixel margin, centred, as CMT's sprite was
  mask     the mask as a 2048 x 512 RGBA (white, alpha the picture's): CMT's 1024 x 512 picture in the middle and
           its edge columns carried out 512 each side, so that a wide view is covered

Usage: magnum_scope.py <details tif> <mask tif> <out details png> <out mask png>"""
import sys

MARGIN = 3
# the labels' boxes in the 290 x 290 picture (after the margin), (column0, row0, column1, row1), inclusive: the zoom
# ladder, the two lines of small print
LABELS = ((226, 133, 272, 196), (38, 55, 100, 84))
LIT = 6                            # a pixel lit for the patches


def patches(lit):
    """each lit pixel's patch number (8-neighbour patches), 0 unlit"""
    import numpy as np
    height, width = lit.shape
    number = np.zeros(lit.shape, np.int32)
    count = 0
    for row in range(height):
        for column in range(width):
            if not lit[row, column] or number[row, column]:
                continue
            count += 1
            number[row, column] = count
            stack = [(row, column)]
            while stack:
                r, c = stack.pop()
                for dr in (-1, 0, 1):
                    for dc in (-1, 0, 1):
                        rr, cc = r + dr, c + dc
                        if 0 <= rr < height and 0 <= cc < width and lit[rr, cc] and not number[rr, cc]:
                            number[rr, cc] = count
                            stack.append((rr, cc))
    return number


def details(path):
    import numpy as np
    from PIL import Image
    picture = np.array(Image.open(path).convert("L"))
    picture = picture[MARGIN:picture.shape[0] - MARGIN, MARGIN:picture.shape[1] - MARGIN].copy()
    number = patches(picture >= LIT)
    inside = np.zeros(picture.shape, bool)
    for column0, row0, column1, row1 in LABELS:
        inside[row0:row1 + 1, column0:column1 + 1] = True
    for patch in np.unique(number[inside]):
        if patch and inside[number == patch].all():
            picture[number == patch] = 0
    # the faint glow left round a label (under LIT) goes with it
    for column0, row0, column1, row1 in LABELS:
        box = picture[row0:row1 + 1, column0:column1 + 1]
        box[box < LIT] = 0
    height, width = picture.shape
    canvas = np.zeros((512, 512), np.uint8)
    canvas[(512 - height) // 2:(512 - height) // 2 + height, (512 - width) // 2:(512 - width) // 2 + width] = picture
    return Image.fromarray(np.dstack([canvas] * 4), "RGBA")


def mask(path):
    import numpy as np
    from PIL import Image
    picture = np.asarray(Image.open(path).convert("L"))
    wide = np.concatenate([np.repeat(picture[:, :1], 512, 1), picture, np.repeat(picture[:, -1:], 512, 1)], 1)
    return Image.fromarray(np.dstack([np.full(wide.shape, 255, np.uint8)] * 3 + [wide]), "RGBA")


if __name__ == "__main__":
    details(sys.argv[1]).save(sys.argv[3])
    mask(sys.argv[2]).save(sys.argv[4])
