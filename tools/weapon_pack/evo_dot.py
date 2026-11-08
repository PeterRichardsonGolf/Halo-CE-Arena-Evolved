"""The holo sight's reticle of the DMR Evo rifle (our own art): the HUD's EVO reticle (evo_reticle.py) drawn in the HUD
reticle's colour on a clear glass, so the sight on the model and the aiming reticle match, and no bigger than the HUD's
on screen (the owner 10-30): its outer ring SCALE of the HUD picture's texels a glass texel. 128 x 128 RGBA: the colour
where drawn (the outer ring dimmer, as on the HUD), alpha its coverage (not premultiplied: the glass's shader blends by
alpha, so the colour shows as the HUD's whatever is behind it), and a faint sheen so the clear glass reads as glass.
Usage: evo_dot.py <out png>"""
import sys

import evo_reticle

N = 128
SUPER = 8
COLOUR = (82, 151, 220)            # the EVO HUD reticle's default colour (its HUD tag's crosshair overlays)
SCALE = 0.70                       # glass texels a HUD picture texel: the outer ring about 40 % of the glass's width


def draw():
    """the picture (a Pillow image)"""
    import numpy as np
    from PIL import Image
    size = N * SUPER
    c = (size - 1) / 2.0
    y, x = np.mgrid[0:size, 0:size].astype(float)
    ring, white = evo_reticle.shape((x - c) / SUPER / SCALE, (c - y) / SUPER / SCALE)
    strength = np.where(white, 1.0, np.where(ring, evo_reticle.GREY, 0.0))
    a = (ring | white).astype(float).reshape(N, SUPER, N, SUPER).mean(axis=(1, 3))
    strength = strength.reshape(N, SUPER, N, SUPER).mean(axis=(1, 3)) / np.maximum(a, 1e-6)
    r = np.hypot(*np.mgrid[0:N, 0:N] - (N - 1) / 2.0)
    sheen = np.clip(1.0 - (r / 90.0) ** 2, 0, 1) * 0.10                       # so the clear glass reads as glass
    alpha = np.maximum(a, sheen)
    rgb = np.zeros((N, N, 3))
    for k in range(3):
        rgb[..., k] = (COLOUR[k] * strength * a + (40, 120, 170)[k] * sheen * (1.0 - a)) / np.maximum(
            a + sheen * (1.0 - a), 1e-6)
    out = np.dstack([np.clip(rgb, 0, 255), alpha * 255]).round().astype(np.uint8)
    return Image.fromarray(out, "RGBA")


if __name__ == "__main__":
    draw().save(sys.argv[1])
