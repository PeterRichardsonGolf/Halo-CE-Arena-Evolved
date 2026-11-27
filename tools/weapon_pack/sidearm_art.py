"""The sidearm's three pictures from ZTeam's own h2 magnum bitmaps (bitmap tags, read by hek.py) and two masks of
ours (sidearm/glow_mask.png, sidearm/face_mask.png: the front post and the two rear dots, and the rear sight's face):

- the body picture: desaturated, its brightness remapped so the 5th, 50th and 95th percentiles land on the given
  levels, everything above the 95th squeezed so the brightest texel lands on "top"; the face mask filled with the
  median level; the glow colour blended in by the glow mask;
- the multipurpose picture (Halo PC channel order: red auxiliary, green self-illumination, blue specular, alpha
  colour change): self-illumination = the glow mask, specular scaled and cleared inside the face mask;
- the small body picture the far models use: the same remap, no masks.

    sidearm_art.py diffuse|multi|lod <bitmap tag> <glow mask> <face mask> <out.png>
"""
import sys

import hek

# the glow colour: #66FF14
GLOW = (0x66, 0xFF, 0x14)


class Refused(Exception):
    """not a picture this takes, with the reason"""


def load_rgba(tag):
    """a bitmap tag's first picture as a float32 array (rows x columns x 4)"""
    import numpy as np
    columns, rows, rgba = hek.first_bitmap_rgba(tag)
    return np.frombuffer(rgba, np.uint8).reshape(rows, columns, 4).astype(np.float32)


def load_mask(path, size):
    """a mask picture (greyscale) as a float32 array 0-255 of the given (columns, rows)"""
    import numpy as np
    from PIL import Image
    picture = Image.open(path).convert("L")
    if picture.size != tuple(size):
        raise Refused("%s is %d x %d, not %d x %d" % (path, picture.size[0], picture.size[1], size[0], size[1]))
    return np.asarray(picture).astype(np.float32)


def remap(rgba, t5, t50, t95, top):
    """the brightness (0.2126 R + 0.7152 G + 0.0722 B) with its 5th, 50th and 95th percentiles moved to t5, t50 and
    t95 (piecewise linear and monotone: the panel lines keep their shape), what is above t95 squeezed to end at
    top: a float32 array 0-255"""
    import numpy as np
    light = 0.2126 * rgba[..., 0] + 0.7152 * rgba[..., 1] + 0.0722 * rgba[..., 2]
    p5, p50, p95 = np.percentile(light, [5, 50, 95])
    low = (t50 - t5) / max(p50 - p5, 1e-6)
    high = (t95 - t50) / max(p95 - p50, 1e-6)
    xs = [0.0, p5, p50, p95, 255.0]
    ys = [max(0.0, t5 - low * p5) if p5 > 0 else 0.0, t5, t50, t95, min(255.0, t95 + high * (255.0 - p95))]
    keep_x, keep_y = [xs[0]], [ys[0]]
    for x, y in zip(xs[1:], ys[1:]):
        if x > keep_x[-1] + 1e-6:
            keep_x.append(x)
            keep_y.append(y)
    light = np.interp(light, keep_x, keep_y)
    brightest = light.max()
    if brightest > t95:
        light = np.where(light > t95, t95 + (light - t95) * (top - t95) / (brightest - t95), light)
    return np.clip(light, 0, 255)


def grey(light, alpha):
    import numpy as np
    level = np.clip(light + 0.5, 0, 255).astype(np.uint8)
    return np.dstack([level, level, level, alpha.astype(np.uint8)])


def diffuse(rgba, glow, face, t5, t50, t95, top):
    """the body picture (uint8 RGBA); glow and face are masks, 0-255"""
    import numpy as np
    light = remap(rgba, t5, t50, t95, top)
    light = np.where(face >= 128, float(t50), light)
    picture = grey(light, rgba[..., 3]).astype(np.float32)
    weight = (glow / 255.0)[..., None]
    picture[..., :3] = picture[..., :3] * (1 - weight) + np.array(GLOW, np.float32) * weight
    return np.clip(picture + 0.5, 0, 255).astype(np.uint8)


def multipurpose(rgba, glow, face, specular):
    """the multipurpose picture (uint8 RGBA, Halo PC channel order)"""
    import numpy as np
    picture = rgba.copy()
    picture[..., 2] *= specular
    picture[..., 2] = np.where(face >= 128, 0, picture[..., 2])
    picture[..., 1] = np.clip(glow, 0, 255)
    return np.clip(picture + 0.5, 0, 255).astype(np.uint8)


def lod(rgba, t5, t50, t95, top):
    """the small body picture (uint8 RGBA)"""
    return grey(remap(rgba, t5, t50, t95, top), rgba[..., 3])


def main(arguments):
    from PIL import Image
    if len(arguments) != 5 or arguments[0] not in ("diffuse", "multi", "lod"):
        print(__doc__)
        return 2
    kind, tag, glow, face, out = arguments
    rgba = load_rgba(open(tag, "rb").read())
    size = (rgba.shape[1], rgba.shape[0])
    if kind == "lod":
        picture = lod(rgba, 0, 7, 22, 24)
    elif kind == "diffuse":
        picture = diffuse(rgba, load_mask(glow, size), load_mask(face, size), 0, 7, 22, 24)
    else:
        picture = multipurpose(rgba, load_mask(glow, size), load_mask(face, size), 0.5)
    Image.fromarray(picture, "RGBA").save(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
