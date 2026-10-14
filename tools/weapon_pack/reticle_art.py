"""The battle rifle's zoomed aiming reticle, drawn here from geometry (no art of the game's or of anyone's is read):
an open ring in four arcs, a small inner circle, and four hairlines that run in through the ring's gaps and stop at
the inner circle, so the centre stays clear. White on nothing, alpha the shape: the HUD tints it (the crosshair's
colour, red on a target).

  br_zoom_reticle.png  512 x 512. Drawn by the HUD at RETICLE_SCALE of its size: the ring is 48 px across on a
      1080-line screen, its lines a little over 3 px (the scope ring's weight).

Usage: reticle_art.py <out png>
"""
import sys

SIZE = 512
SUPER = 4                    # drawn this many times as large, then brought down: smooth edges
RING_OUTER, RING_INNER = 200.0, 170.0
GAP_DEGREES = 9.0            # each side of up, down, left and right, cut out of the ring
EYE_OUTER, EYE_INNER = 88.0, 62.0
HAIR_WIDTH, HAIR_END = 26.0, 250.0
RETICLE_SCALE = 0.0533       # 400 texels of ring x this x 2.25 screen px a HUD unit (1080 lines) = 48 px


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


def main(arguments):
    if len(arguments) != 1:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    import numpy as np
    from PIL import Image
    alpha = zoom_reticle()
    image = np.zeros(alpha.shape + (4,), np.uint8)
    image[..., :3] = 255
    image[..., 3] = alpha
    Image.fromarray(image, "RGBA").save(arguments[0])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
