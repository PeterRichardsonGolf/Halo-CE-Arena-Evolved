"""The HJ magnum's zoom overlay, from ZTeam's own circular scope picture (a bitmap tag of the source pack, read by
hek.py): its clear round window, drawn again where the HUD can show it round and black all round it.

ZTeam's picture is a 2048 x 1024 sheet with a round window (about 409 texels across, a little left of the sheet's
middle) in a surround of 0.69 black, made for a HUD drawn stretched to a wide screen: its overlay's scale (0.4125
wide, 0.55 high) squeezed the window to an upright oval here, and the sheet reached the screen's sides only on a
screen up to 21:9, with the surround not quite black. So the window is cut from the sheet, centred, and scaled to the
size the HUD showed it (0.55: 225 HUD units across), and put in a 2048 x 512 picture at one texel a HUD unit (the HUD
is 480 units high whatever the screen: the overlay's scale is 1.0 both ways), the surround solid black out to the
picture's edges, 512 units high and 2048 wide (a screen up to 4.2:1).

  mask     2048 x 512 RGBA: the colour black, the alpha the surround's, 0 in the window, its soft edge ZTeam's

Usage: hj_scope.py <ZTeam's scope bitmap tag> <out png>"""
import sys

import hek

SCALE = 0.55                # what ZTeam's overlay scaled the window by, up and down
WIDTH, HEIGHT = 2048, 512   # the picture, in HUD units
CLEAR = 88                  # an alpha below this is the window's, to find where it is
SURROUND = 175.0            # the alpha ZTeam's surround has
HALF = 500                  # the part of ZTeam's sheet taken round the window's middle (texels each side)


class Refused(Exception):
    """not a picture this takes, with the reason"""


def draw(tag):
    """the overlay as a Pillow image (RGBA, WIDTH x HEIGHT)"""
    import numpy as np
    from PIL import Image
    columns, rows, rgba = hek.first_bitmap_rgba(tag)
    alpha = np.frombuffer(rgba, np.uint8).reshape(rows, columns, 4)[..., 3].astype(np.float32)
    ys, xs = np.nonzero(alpha[:rows - 1] < CLEAR)       # (the sheet's last row holds stray texels: left out)
    if not len(xs) or xs.max() - xs.min() < 100:
        raise Refused("the picture has no clear window")
    centre_x = int(round((xs.min() + xs.max()) / 2.0))
    centre_y = int(round((ys.min() + ys.max()) / 2.0))
    if (centre_x - HALF < 0 or centre_x + HALF > columns or centre_y - HALF < 0 or centre_y + HALF > rows - 1):
        raise Refused("the window is too near the sheet's edge")
    part = alpha[centre_y - HALF:centre_y + HALF, centre_x - HALF:centre_x + HALF]
    # the surround solid; the window's soft edge between keeps its shape (the surround's 0.69 made 1)
    part = np.minimum(part * (255.0 / SURROUND), 255.0)
    size = int(round(2 * HALF * SCALE))
    small = Image.fromarray(part.astype(np.uint8), "L").resize((size, size), Image.LANCZOS)
    canvas = Image.new("L", (WIDTH, HEIGHT), 255)
    canvas.paste(small, (WIDTH // 2 - size // 2, HEIGHT // 2 - size // 2))
    out = np.zeros((HEIGHT, WIDTH, 4), np.uint8)
    out[..., 3] = np.asarray(canvas, np.uint8)
    return Image.fromarray(out, "RGBA")


def main(arguments):
    if len(arguments) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    with open(arguments[1], "rb") as file:
        draw(file.read()).save(arguments[2])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
