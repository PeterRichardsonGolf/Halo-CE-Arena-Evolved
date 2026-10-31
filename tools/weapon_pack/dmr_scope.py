"""The zoom overlay of the DMR and the DMR Evo rifle, from CMT's own scope pictures (the Bonus Data release's original
data of its DMR scope, used with credit) with the magnification labels left out (the ladder of the zoom steps at the
top and the two lines of small print at the left; the range ruler stays):

  details  the lines as a 512 x 512 RGBA (colour and alpha both the line's strength), CMT's 320 x 290 picture
           centred, as CMT's sprite was
  mask     the mask as a 2048 x 512 RGBA (white, alpha the picture's): CMT's 1024 x 512 picture in the middle and
           its edge columns carried out 512 each side, so that a wide view is covered

Usage: dmr_scope.py <details tif> <mask tif> <out details png> <out mask png>"""
import sys

# the labels' places in the 320 x 290 picture (rows, columns): the zoom ladder, the two lines of small print
ERASED = (((25, 98), (185, 288)), ((117, 146), (20, 103)))
CROP = (0, 0, 326, 296)            # the part of the 326 x 591 picture that holds the scope
MARGIN = 3


def details(path):
    import numpy as np
    from PIL import Image
    picture = np.array(Image.open(path).convert("L").crop(CROP))
    picture = picture[MARGIN:picture.shape[0] - MARGIN, MARGIN:picture.shape[1] - MARGIN].copy()
    for (row0, row1), (column0, column1) in ERASED:
        picture[row0:row1, column0:column1] = 0
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
