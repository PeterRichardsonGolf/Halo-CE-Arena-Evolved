"""The battle rifle's zoomed aiming reticle, drawn here from geometry (no art of the game's or of anyone's is read):
an open ring in four arcs, a small inner circle, and four hairlines that run in through the ring's gaps and stop at
the inner circle, so the centre stays clear. White on nothing, alpha the shape: the HUD tints it (the crosshair's
colour, red on a target).

  br_zoom_reticle.png  512 x 512. Drawn by the HUD at RETICLE_SCALE of its size: the ring is 48 px across on a
      1080-line screen, its lines a little over 3 px (the scope ring's weight).

  smg_reticle.png  512 x 512: the SMG's, in the form Halo 2's SMG reticle has (drawn here, nothing of it read):
      four arcs of a ring centred up, down, left and right with the diagonals open, a short tick out from each
      arc's middle, a faint small ring in the centre. Drawn by the HUD at SMG_SCALE: the ring 58 px across on a
      1080-line screen (about the SMG's first-shot error cone, 1 degree each side at the default field of view),
      the ticks' ends 72 px, its lines 2.2 px (the battle rifle reticle's weight).

Usage: reticle_art.py <out png> [smg]
"""
import sys

SIZE = 512
SUPER = 4                    # drawn this many times as large, then brought down: smooth edges
RING_OUTER, RING_INNER = 200.0, 170.0
GAP_DEGREES = 9.0            # each side of up, down, left and right, cut out of the ring
EYE_OUTER, EYE_INNER = 88.0, 62.0
HAIR_WIDTH, HAIR_END = 26.0, 250.0
RETICLE_SCALE = 0.0453       # the recipe's scale for both aim crosshairs: 500 texels x this x 2.25 = 51 px on 1080 lines


def zoom_reticle():
    """the reticle's alpha, SIZE x SIZE bytes (a numpy array)"""
    import numpy as np
    from PIL import Image, ImageDraw
    n = SIZE * SUPER
    c = n / 2.0
    image = Image.new("L", (n, n), 0)
    draw = ImageDraw.Draw(image)

    def disc(radius, fill):
        r = radius * SUPER
        draw.ellipse((c - r, c - r, c + r, c + r), fill=fill)

    disc(RING_OUTER, 255)
    disc(RING_INNER, 0)
    r = (RING_OUTER + 4) * SUPER
    for centre in (0, 90, 180, 270):             # the gaps
        draw.pieslice((c - r, c - r, c + r, c + r), centre - GAP_DEGREES, centre + GAP_DEGREES, fill=0)
    half = HAIR_WIDTH * SUPER / 2
    near, far = EYE_OUTER * SUPER - 2, HAIR_END * SUPER
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        x0, y0, x1, y1 = c + dx * near, c + dy * near, c + dx * far, c + dy * far
        draw.rectangle((min(x0, x1) - abs(dy) * half, min(y0, y1) - abs(dx) * half,
                        max(x0, x1) + abs(dy) * half, max(y0, y1) + abs(dx) * half), fill=255)
    disc(EYE_OUTER, 255)
    disc(EYE_INNER, 0)
    return np.array(image.resize((SIZE, SIZE), Image.LANCZOS))


SMG_RING, SMG_LINE = 200.0, 15.0          # the arcs' middle radius and their width, texels
SMG_ARC_DEGREES = 30.0                     # each side of an arc's middle
SMG_TICK_END = 250.0                       # the ticks run out from the ring to here
SMG_EYE, SMG_EYE_LINE, SMG_EYE_ALPHA = 64.0, 9.0, 110
SMG_SCALE = 0.0644                         # 400 texels x this x 2.25 = 58 px on 1080 lines


def smg_reticle():
    """the SMG reticle's alpha, SIZE x SIZE bytes (a numpy array)"""
    import numpy as np
    from PIL import Image, ImageDraw
    n = SIZE * SUPER
    c = n / 2.0
    arcs = Image.new("L", (n, n), 0)
    draw = ImageDraw.Draw(arcs)
    r = (SMG_RING + SMG_LINE / 2) * SUPER
    for middle in (0, 90, 180, 270):
        draw.pieslice((c - r, c - r, c + r, c + r), middle - SMG_ARC_DEGREES, middle + SMG_ARC_DEGREES, fill=255)
    r = (SMG_RING - SMG_LINE / 2) * SUPER
    draw.ellipse((c - r, c - r, c + r, c + r), fill=0)
    half = SMG_LINE * SUPER / 2
    near, far = (SMG_RING - SMG_LINE / 2) * SUPER, SMG_TICK_END * SUPER
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        x0, y0, x1, y1 = c + dx * near, c + dy * near, c + dx * far, c + dy * far
        draw.rectangle((min(x0, x1) - abs(dy) * half, min(y0, y1) - abs(dx) * half,
                        max(x0, x1) + abs(dy) * half, max(y0, y1) + abs(dx) * half), fill=255)
    eye = Image.new("L", (n, n), 0)
    draw = ImageDraw.Draw(eye)
    r = (SMG_EYE + SMG_EYE_LINE / 2) * SUPER
    draw.ellipse((c - r, c - r, c + r, c + r), fill=SMG_EYE_ALPHA)
    r = (SMG_EYE - SMG_EYE_LINE / 2) * SUPER
    draw.ellipse((c - r, c - r, c + r, c + r), fill=0)
    both = np.maximum(np.array(arcs), np.array(eye))
    return np.array(Image.fromarray(both).resize((SIZE, SIZE), Image.LANCZOS))


def from_picture(path):
    """a reticle kept as a picture beside the recipe (512 x 512, its alpha the shape), as bytes (a numpy array):
    battle_rifle/reticle_kenney_033.png is crosshair 033 of Kenney's Crosshair Pack 1.1 (www.kenney.nl, CC0): its
    vector file drawn large, fitted to 500 texels, and its lines brought to 22 texels wide by a round pen (2.24 px
    on a 1080-line screen; 23 is the pistol reticle's line as measured in the game)"""
    import numpy as np
    from PIL import Image
    image = Image.open(path).convert("RGBA")
    if image.size != (SIZE, SIZE):
        raise ValueError("the reticle's picture is %d x %d, not %d x %d" % (image.size + (SIZE, SIZE)))
    return np.array(image.getchannel("A"))


def main(arguments):
    if len(arguments) not in (1, 2) or arguments[1:] not in ([], ["smg"]):
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    import numpy as np
    from PIL import Image
    alpha = smg_reticle() if arguments[1:] == ["smg"] else zoom_reticle()
    image = np.zeros(alpha.shape + (4,), np.uint8)
    image[..., :3] = 255
    image[..., 3] = alpha
    Image.fromarray(image, "RGBA").save(arguments[0])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
