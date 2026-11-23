"""The EVO rifle's Digsite look: CMT's DMR body picture repainted by numbers (our own recolour, no new art) in the
Digsite weapons' finish, which the rifle wears on the CE Digsite Multiplayer maps only:

- the furniture (stock, grip and handguard: where the paint mask beside the recipe is white, a mask drawn from the
  model's geometry) takes an olive-drab paint at the picture's own brightness, mottled and grained, worn to bare metal
  on its brightest edges;
- the rest (receiver, barrel) takes a warm gunmetal at the picture's own brightness;
- the yellow stripe and the green light become red; the brightest glints (the lenses) stay as they were.

The grain and mottling are seeded smooth noise, so the picture is the same each run. Every number is here and nowhere else.

    evo_digsite_art.py <diffuse bitmap tag> <paint mask png> <out.png>
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek  # noqa: E402

OLIVE = (0.35, 0.34, 0.22)
GUNMETAL = (0.15, 0.145, 0.14)
RED = (0.72, 0.09, 0.06)
LIGHT = (0.95, 0.12, 0.08)
SEED = 7
# the grain: smooth noise (32 x 32 values enlarged, blurred 4 px) with this spread
GRAIN = 0.115


class Refused(Exception):
    """not a picture this takes, with the reason"""


def load_rgba(tag):
    """a bitmap tag's first picture as a float32 array 0-1 (rows x columns x 4)"""
    import numpy as np
    columns, rows, rgba = hek.first_bitmap_rgba(tag)
    return np.frombuffer(rgba, np.uint8).reshape(rows, columns, 4).astype(np.float32) / 255


def load_mask(path, size):
    """the paint mask (greyscale, white the paint) grown 1 px and softened, as float32 0-1 (rows x columns x 1)"""
    import numpy as np
    from PIL import Image, ImageFilter
    picture = Image.open(path).convert("L")
    if picture.size != (size, size):
        raise Refused("%s is %d x %d, not %d x %d" % (path, picture.size[0], picture.size[1], size, size))
    picture = picture.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(1.2))
    return np.asarray(picture, np.float32)[..., None] / 255


def noise(rng, cells, size, radius):
    """value noise: cells x cells random values, enlarged to size and blurred, float32 0-1 (rows x columns x 1)"""
    import numpy as np
    from PIL import Image, ImageFilter
    small = Image.fromarray((rng.random((cells, cells)) * 255).astype(np.uint8), "L")
    picture = small.resize((size, size), Image.BICUBIC)
    if radius:
        picture = picture.filter(ImageFilter.GaussianBlur(radius))
    return np.asarray(picture, np.float32)[..., None] / 255


def diffuse(rgba, mask):
    """the repainted picture, RGB float32 0-1"""
    import numpy as np
    size = rgba.shape[0]
    if rgba.shape[1] != size:
        raise Refused("the picture is not square")
    rgb = rgba[..., :3]
    light = rgb @ np.array([0.299, 0.587, 0.114], np.float32)
    low, high = np.percentile(light, 2), np.percentile(light, 98)
    shade = np.clip((light - low) / max(high - low, 1e-3), 0, 1)[..., None]
    rng = np.random.default_rng(SEED)
    grain = noise(rng, 32, size, 4)
    grain = (grain - grain.mean()) / max(float(grain.std()), 1e-6) * GRAIN
    blot = noise(rng, 64, size, 6)
    paint = np.array(OLIVE, np.float32) * (0.55 + 0.75 * shade) * (0.9 + 0.25 * blot) + grain * 0.12
    wear = np.clip((shade - 0.78) * 4.0, 0, 1)
    paint = paint * (1 - wear) + (0.32 + 0.4 * shade) * wear
    metal = np.array(GUNMETAL, np.float32) * (0.6 + 1.4 * shade) + grain * 0.05
    out = metal * (1 - mask) + paint * mask
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    yellow = ((r > 0.45) & (g > 0.40) & (b < 0.30))[..., None]
    green = ((g > 0.35) & (g > r * 1.6) & (g > b * 1.4))[..., None]
    out = np.where(yellow, np.array(RED, np.float32) * (0.6 + 0.5 * shade), out)
    out = np.where(green, np.array(LIGHT, np.float32), out)
    out = np.where((light > 0.85)[..., None], rgb, out)
    return np.clip(out, 0, 1)


def picture(tag, mask_path):
    """the repainted picture as an RGB uint8 array"""
    import numpy as np
    rgba = load_rgba(tag)
    return (diffuse(rgba, load_mask(mask_path, rgba.shape[0])) * 255).astype(np.uint8)


def main(arguments):
    from PIL import Image
    if len(arguments) != 3:
        print(__doc__.strip().split("\n")[-1].strip())
        return 2
    with open(arguments[0], "rb") as file:
        tag = file.read()
    Image.fromarray(picture(tag, arguments[1]), "RGB").save(arguments[2])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
