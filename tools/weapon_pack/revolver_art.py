"""The heavy revolver's two pictures, from ZTeam's own h3 base map and multipurpose map (bitmap tags, read by hek.py)
re-dressed for the h3b model, whose texture coordinates are a flat projection of the gun's side
(revolver_texels.py says where each texel lands on the model). Nothing is painted by hand; every change is a
number below:

- the lights: ZTeam's lights are its multipurpose map's self-illumination; on the h3b model they land on the grip
  foot and the drum. Each is cleared (its texels take the median of the ring round it) and three are carried, with
  their own texels, to the place they have on the h3 model (MOVES: the box on ZTeam's sheet, the model-space point);
- the plates: the drum and the receiver's side panel take a dark red, the rest a dark metal, each a three-stop ramp
  of the texel's own brightness (its 5th, 50th and 95th percentiles), so ZTeam's panel lines and wear stay;
- the lights in the energy red (their self-illumination ZTeam's);
- the specular mask's contrast lifted (5th, 50th, 95th percentiles to 30, 110, 220).

The colours are numbers sampled once from a reference palette (dark red plates, dark metal, light grey, energy red);
no texel of any other source is read.

    revolver_art.py diffuse|multi <base map tag> <multipurpose tag> <texels.npy> <out.png>
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # (found when run with python -I, too)
import hek  # noqa: E402

# the palette, (r, g, b) 0-255
RED_DARK, RED, RED_LIGHT = (59, 22, 19), (88, 33, 31), (144, 61, 59)
METAL_DARK, METAL, METAL_LIGHT = (22, 23, 22), (53, 54, 52), (108, 108, 107)
ENERGY = (207, 62, 59)
# ZTeam's lights that sit on the gun's sides, carried: (what, the box on ZTeam's sheet x0, y0, x1, y1, the point on
# the model they sit at on the h3 model, x, y, z)
MOVES = (
    ("the two slide dots", 90, 375, 120, 389, (0.03805, 0.0114, 0.0457)),
    ("the rear II mark", 88, 198, 106, 219, (-0.0262, 0.0098, 0.0373)),
    ("the trigger dash", 138, 133, 158, 145, (0.0383, 0.0078, 0.0090)),
)
# the red plates: the drum (node 2) and the receiver's side panel (node 0, a box in x and z)
DRUM_NODE = 2
PANEL = ((0.012, 0.052), (-0.012, 0.030))
SPECULAR = ((30, 110, 220), (5, 50, 95))
LIGHT_THRESHOLD = 24          # a ZTeam texel is a light from this self-illumination up
SIZE = 512


class Refused(Exception):
    """not a picture this takes, with the reason"""


def load_rgba(tag):
    """a bitmap tag's first picture as a float32 array (rows x columns x 4)"""
    import numpy as np
    columns, rows, rgba = hek.first_bitmap_rgba(tag)
    if (columns, rows) != (SIZE, SIZE):
        raise Refused("a %d x %d picture, not %d x %d" % (columns, rows, SIZE, SIZE))
    return np.frombuffer(rgba, np.uint8).reshape(rows, columns, 4).astype(np.float32)


def load_texels(path):
    import numpy as np
    texels = np.load(path, allow_pickle=False)
    if texels.shape != (SIZE, SIZE, 4):
        raise Refused("texels of shape %s" % (texels.shape,))
    return texels


def dilate(mask, radius):
    import numpy as np
    out = mask.copy()
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            if dx * dx + dy * dy <= radius * radius:
                out |= np.roll(np.roll(mask, dy, 0), dx, 1)
    return out


def components(mask):
    """the mask's 4-connected components, numbered in reading order: (labels, count)"""
    import numpy as np
    labels = np.zeros(mask.shape, np.int32)
    count = 0
    for y, x in zip(*np.nonzero(mask)):
        if labels[y, x]:
            continue
        count += 1
        labels[y, x] = count
        stack = [(y, x)]
        while stack:
            cy, cx = stack.pop()
            for ny, nx in ((cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)):
                if 0 <= ny < mask.shape[0] and 0 <= nx < mask.shape[1] and mask[ny, nx] and not labels[ny, nx]:
                    labels[ny, nx] = count
                    stack.append((ny, nx))
    return labels, count


def nearest_side_texel(texels, point):
    """the texel whose surface point is nearest the given one"""
    import numpy as np
    distance = np.linalg.norm(np.nan_to_num(texels[..., 0:3], nan=9.0) - np.array(point, np.float32), axis=-1)
    y, x = np.unravel_index(np.argmin(distance), distance.shape)
    return int(x), int(y)


def moved_lights(base, multi, texels):
    """the lights cleared where they are and carried where they belong: (base, multi)"""
    import numpy as np
    lights = multi[..., 1] >= LIGHT_THRESHOLD
    labels, count = components(lights)
    out_base, out_multi = base.copy(), multi.copy()
    for index in range(1, count + 1):
        core = dilate(labels == index, 2)
        ring = dilate(core, 3) & ~core & ~lights
        out_base[core, :3] = np.median(base[ring, :3], axis=0)
        out_multi[core, 1] = 0
    for _, x0, y0, x1, y1, point in MOVES:
        patch_base, patch_multi = base[y0:y1, x0:x1], multi[y0:y1, x0:x1]
        x, y = nearest_side_texel(texels, point)
        height, width = patch_base.shape[:2]
        top, left = y - height // 2, x - width // 2
        alpha = dilate(patch_multi[..., 1] >= LIGHT_THRESHOLD, 1).astype(np.float32)
        alpha = (alpha + np.roll(alpha, 1, 0) + np.roll(alpha, -1, 0) + np.roll(alpha, 1, 1) + np.roll(alpha, -1, 1)) / 5
        area = (slice(top, top + height), slice(left, left + width))
        out_base[area][..., :3] = out_base[area][..., :3] * (1 - alpha[..., None]) + patch_base[..., :3] * alpha[..., None]
        out_multi[area][..., 1] = np.maximum(out_multi[area][..., 1], patch_multi[..., 1])
    return out_base, out_multi


def plates(texels):
    import numpy as np
    x, z, node = texels[..., 0], texels[..., 2], texels[..., 3]
    with np.errstate(invalid="ignore"):
        panel = (node == 0) & (x >= PANEL[0][0]) & (x <= PANEL[0][1]) & (z >= PANEL[1][0]) & (z <= PANEL[1][1])
    return (node == DRUM_NODE) | panel


def ramp(t, low, middle, high):
    import numpy as np
    low, middle, high = (np.array(c, np.float32) for c in (low, middle, high))
    return np.where(t < 0.5, low + (middle - low) * (t / 0.5), middle + (high - middle) * ((t - 0.5) / 0.5))


def pictures(base, multi, texels):
    """(the base map, the multipurpose map), each rows x columns x 4, 0-255 float32"""
    import numpy as np
    base, multi = moved_lights(base, multi, texels)
    light = base[..., :3] @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    p5, p50, p95 = np.percentile(light, [5, 50, 95])
    t = np.interp(light, [0, p5, p50, p95, 255], [0, 0, 0.5, 1, 1.15])[..., None]
    red = plates(texels)[..., None]
    out = base.copy()
    out[..., :3] = np.where(red, ramp(t, RED_DARK, RED, RED_LIGHT), ramp(t, METAL_DARK, METAL, METAL_LIGHT))
    glow = (multi[..., 1] / 255)[..., None]
    out[..., :3] = out[..., :3] * (1 - glow) + np.array(ENERGY, np.float32) * (0.6 + 0.4 * t) * glow
    out_multi = multi.copy()
    (low, middle, high), percentiles = SPECULAR
    s5, s50, s95 = np.percentile(multi[..., 2], percentiles)
    out_multi[..., 2] = np.interp(multi[..., 2], [0, s5, s50, s95, 255], [0, low, middle, high, 255])
    return np.clip(np.round(out), 0, 255), np.clip(np.round(out_multi), 0, 255)


def draw(kind, base_tag, multi_tag, texels_path):
    """the picture as a uint8 RGBA array: kind "diffuse" or "multi" (Halo PC's channel order)"""
    import numpy as np
    out = pictures(load_rgba(base_tag), load_rgba(multi_tag), load_texels(texels_path))
    return out[0 if kind == "diffuse" else 1].astype(np.uint8)


def main(arguments):
    if len(arguments) != 6 or arguments[1] not in ("diffuse", "multi"):
        sys.exit(__doc__)
    from PIL import Image
    with open(arguments[2], "rb") as a, open(arguments[3], "rb") as b:
        picture = draw(arguments[1], a.read(), b.read(), arguments[4])
    Image.fromarray(picture, "RGBA").save(arguments[5])


if __name__ == "__main__":
    main(sys.argv)
