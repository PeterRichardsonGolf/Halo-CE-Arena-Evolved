"""The EVO rifle's aiming reticle (our own drawing, made from geometry, after the owner's pick, 10-30): a grey ring broken
at the four axes and inside it a white ring with a small nub out at each axis. 64 x 64, colour and alpha both the line's
strength (the HUD adds the colour: the grey ring shows dimmer than the white one), drawn 8 x 8 supersampled so the edges
are soft by a texel and no more. At the recipe's crosshair scale (0.30) it is about 40 pixels across at 1080p.
shape() gives the two parts' coverage on any grid in this picture's texels, so the holo sight's glass (evo_dot.py)
draws the same reticle.
Usage: evo_reticle.py <out png>"""
import sys

N = 64
SUPER = 8
RING = (27.5, 3.0, 9.0)            # the outer ring: its middle's radius, its width (texels), each axis gap's half (degrees)
INNER = (12.7, 4.0)                # the inner ring: its middle's radius, its width
NUB = (17.0, 2.0)                  # the nubs: out to this radius from the inner ring, half their width
GREY = 0.55                        # the outer ring's strength (the inner ring and nubs full)


def shape(x, y):
    """the outer ring's and the inner ring's (with its nubs) coverage, 0 or 1, at x, y (texels from the middle, up +y)"""
    import numpy as np
    r = np.hypot(x, y)
    off_axis = np.degrees(np.arctan2(np.minimum(np.abs(x), np.abs(y)), np.maximum(np.abs(x), np.abs(y))))
    ring = (np.abs(r - RING[0]) <= RING[1] / 2.0) & (off_axis >= RING[2])
    inner = np.abs(r - INNER[0]) <= INNER[1] / 2.0
    across = np.minimum(np.abs(x), np.abs(y))
    nubs = (across <= NUB[1]) & (r >= INNER[0]) & (r <= NUB[0])
    return ring, inner | nubs


def draw():
    """the picture (a Pillow image)"""
    import numpy as np
    from PIL import Image
    size = N * SUPER
    c = (size - 1) / 2.0
    y, x = np.mgrid[0:size, 0:size].astype(float)
    ring, white = shape((x - c) / SUPER, (c - y) / SUPER)
    lit = np.where(white, 1.0, np.where(ring, GREY, 0.0))
    alpha = (ring | white).astype(float)
    value = lit.reshape(N, SUPER, N, SUPER).mean(axis=(1, 3))
    a = alpha.reshape(N, SUPER, N, SUPER).mean(axis=(1, 3))
    # (colour the strength, alpha the coverage: the grey ring keeps its full edge, dimmer)
    out = np.dstack([np.clip(value, 0, 1) * 255] * 3 + [np.clip(a, 0, 1) * 255]).round().astype(np.uint8)
    return Image.fromarray(out, "RGBA")


if __name__ == "__main__":
    draw().save(sys.argv[1])
