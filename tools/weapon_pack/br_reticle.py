"""The battle rifle's aiming reticle (our own drawing, made from geometry, after the owner's pick, 10-30), in two parts so
the HUD can spread the ticks while the ring keeps its size (ae_reticle_growth.c): a grey ring broken at the four axes
(the plate's first bitmap) and four white ticks along the axes, from near the middle out through the ring's gaps (the
second). A colour plate for invader-bitmap: the two 64 x 64 pictures one above the other on a blue (0 0 255)
background, a texel of it round each, so each is a sequence of its own (an overlay's sequence index names it). Each picture: colour and alpha both the line's strength (the HUD adds the colour: the grey ring shows dimmer),
drawn 8 x 8 supersampled. At the recipe's crosshair scale (0.30) it is about 40 pixels across at 1080p.
Usage: br_reticle.py <out png>"""
import sys

N = 64
SUPER = 8
RING = (23.0, 2.8, 12.0)           # the ring: its middle's radius, its width (texels), each axis gap's half (degrees)
TICK = (8.0, 30.0, 1.4)            # the ticks: from this radius out to this one, half their width
GREY = 0.55                        # the ring's strength (the ticks full)


def shape(x, y):
    """the ring's and the ticks' coverage, 0 or 1, at x, y (texels from the middle)"""
    import numpy as np
    r = np.hypot(x, y)
    off_axis = np.degrees(np.arctan2(np.minimum(np.abs(x), np.abs(y)), np.maximum(np.abs(x), np.abs(y))))
    ring = (np.abs(r - RING[0]) <= RING[1] / 2.0) & (off_axis >= RING[2])
    along, across = np.maximum(np.abs(x), np.abs(y)), np.minimum(np.abs(x), np.abs(y))
    ticks = (across <= TICK[2]) & (along >= TICK[0]) & (along <= TICK[1])
    return ring, ticks


def part(cover, strength):
    import numpy as np
    value = cover.astype(float).reshape(N, SUPER, N, SUPER).mean(axis=(1, 3))
    return np.dstack([value * strength * 255] * 3 + [value * 255]).round().astype(np.uint8)


def draw():
    """the plate (a Pillow image)"""
    import numpy as np
    from PIL import Image
    size = N * SUPER
    c = (size - 1) / 2.0
    y, x = np.mgrid[0:size, 0:size].astype(float)
    ring, ticks = shape((x - c) / SUPER, (c - y) / SUPER)
    plate = np.zeros((2 * N + 3, N + 2, 4), np.uint8)
    plate[...] = (0, 0, 255, 255)
    plate[1:N + 1, 1:N + 1] = part(ring, GREY)
    plate[N + 2:2 * N + 2, 1:N + 1] = part(ticks, 1.0)
    return Image.fromarray(plate, "RGBA")


if __name__ == "__main__":
    draw().save(sys.argv[1])
