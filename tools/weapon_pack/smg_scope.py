"""The tactical SMG's zoom overlay, from CMT's own scope pictures (the SMG scope's details and mask, from CMT's
1.4.1c release, used with credit) with the magnification label and the lines of small print left out:

  details  the lines as a 512 x 512 RGBA (colour and alpha both the line's strength), CMT's 290 x 290 picture
           centred on its ring, so that the HUD draws it round the middle of the view
  mask     the mask as a 2048 x 512 RGBA (white, alpha the picture's): CMT's 1024 x 512 picture in the middle and
           its edge columns carried out 512 each side, so that a wide view is covered

Each takes the picture's alpha as scope_hd.load_alpha gives it (0..1, float32).
"""
import numpy as np

# where the art's ring is centred in CMT's 512 x 512 picture (measured: the ring is 280 across, its four lines
# run to the middle)
CENTRE = (145, 145)
# the labels' places (rows, columns): the lines of small print at the left, the zoom label and its ruler at the
# right; the horizontal line of the reticle (rows 143-147) is left whole
ERASED = (((149, 186), (14, 87)), ((149, 186), (205, 278)))


def details(alpha):
    """the lines on a 512 x 512 canvas, ring centred, labels erased: an RGBA Pillow image"""
    from PIL import Image
    picture = np.array(np.clip(alpha * 255.0 + 0.5, 0, 255), dtype=np.uint8, copy=True)
    for (row0, row1), (column0, column1) in ERASED:
        picture[row0:row1, column0:column1] = 0
    canvas = np.zeros((512, 512), np.uint8)
    top, left = 256 - CENTRE[0], 256 - CENTRE[1]
    canvas[top:top + 290, left:left + 290] = picture[:290, :290]
    return Image.fromarray(np.dstack([canvas] * 4), "RGBA")


def mask(alpha):
    """the mask carried out to 2048 wide: an RGBA Pillow image"""
    from PIL import Image
    picture = np.array(np.clip(alpha * 255.0 + 0.5, 0, 255), dtype=np.uint8)
    wide = np.concatenate([np.repeat(picture[:, :1], 512, 1), picture, np.repeat(picture[:, -1:], 512, 1)], 1)
    return Image.fromarray(np.dstack([np.full(wide.shape, 255, np.uint8)] * 3 + [wide]), "RGBA")
