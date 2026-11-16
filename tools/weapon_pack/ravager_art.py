"""The ravager's own look: CMT's spiker textures recoloured by numbers (our own recolour, no new art), so the ravager,
which shares the spiker's models, reads as another weapon. Three pictures, from CMT's own bitmap tags (read by
hek.py):

- the body and clip pictures: a gradient map over the diffuse picture's brightness. The multipurpose picture's
  specular (blue in Halo PC's order), blurred and thresholded into whole shapes, splits the texture into "paint" and
  "metal" (blade, drum, clamp plates); the paint takes a three-stop ramp (dark, mid, light), the darkest recesses go to
  a trim colour, the metal takes a two-stop ramp; the small self-lit dots (CMT's orange) take the look's light colour
  at their own brightness; a look may tint the blade's steel like tempered steel (a colour run along the blade) and
  light the body's panel lines (its glow, drawn in the glow colour);
- the body's multipurpose picture (Halo PC's channel order: red auxiliary, green self-illumination, blue specular,
  alpha colour change): CMT's own, with the look's glow (if it has one) added to the self-illumination channel. A
  look without a glow gives CMT's picture back unchanged.

Every number of a look is in LOOKS below and nowhere else: the recipe names the look, and a new pick is a change of
these numbers and a re-pin of the pictures and the pack.

    ravager_art.py body|clip|multi <look> <diffuse bitmap tag> <multipurpose bitmap tag> <out.png>
"""
import sys

import hek

# A look: paint (dark, mid, light) over the brightness; metal (dark, light); the steel mask: blur radius (px), its
# edge (lo, hi) on the blurred specular, small shiny islands dropped (shrink, px), holes closed (close, px), grown
# (grow, px); trim: (brightness below which the paint goes to the trim colour, the colour) or None; lights: the colour
# of the self-lit dots; glow: None, or a self-illumination glow along the panel lines (the grooves: the brightness
# below its own 4 px blur by edge (lo, hi), widened by "widen" px) inside the body texture's UV rectangles "rects"
# (x0, y0, x1, y1 at 1024 px, feathered 6 px), on paint only, at "amount", drawn in "colour" on the body picture and
# put in the multipurpose picture's self-illumination channel (the shaders' self-illumination colour is the recipe's
# shader edit), and, with "seam" (box, width px), a glowing seam that wide just outside the steel where it meets the
# paint inside the box (the blade's edge against the body); heat: None, or the blade tinted like tempered steel: four colours run left to right across the blade's
# UV box "box" (x0, y0, x1, y1 at 1024 px, feathered 8 px), mixed into bright steel by "amount", on metal only. The
# clip has no glow and no heat.
# the body texture's UV boxes (1024 px, found with a UV test render): the barrel shroud (top and sides), the side
# panel under the barrel, the blade
SHROUD = (27, 80, 493, 240)
LOWER = (27, 253, 493, 467)
BLADE = (200, 670, 690, 880)
LOOKS = {
    # owner's base pick 2026-11-04: crimson panels, near-black steel trim in the recesses, bright steel, cyan lights
    "B3-trim": {
        "paint": ((0.18, 0.015, 0.015), (0.60, 0.055, 0.045), (0.90, 0.24, 0.18)),
        "metal": ((0.25, 0.25, 0.27), (0.95, 0.95, 0.98)),
        "blur": 10, "lo": 0.50, "hi": 0.58, "shrink": 25, "close": 21, "grow": 5,
        "trim": (0.30, (0.05, 0.05, 0.055)),
        "lights": (0.30, 0.95, 1.00),
        "glow": None,
        "heat": None,
    },
    # owner's pick 2026-11-04: B3 with B3b's glow (white-cyan panel lines on the shroud and the lower panel: the seam
    # where the blade meets the body, the X marks, the deep panel lines) and B3d's tempered blade (straw at the tip,
    # then bronze, violet and blue toward the body); B3's crimson and near-black trim
    "B3bd": {
        "paint": ((0.18, 0.015, 0.015), (0.60, 0.055, 0.045), (0.90, 0.24, 0.18)),
        "metal": ((0.25, 0.25, 0.27), (0.95, 0.95, 0.98)),
        "blur": 10, "lo": 0.50, "hi": 0.58, "shrink": 25, "close": 21, "grow": 5,
        "trim": (0.30, (0.05, 0.05, 0.055)),
        "lights": (0.30, 0.95, 1.00),
        "glow": {"rects": (SHROUD, LOWER), "edge": (0.05, 0.12), "widen": 3, "colour": (0.35, 0.95, 1.00),
                 "amount": 1.0, "seam": (BLADE, 7)},
        "heat": {"box": BLADE, "colours": ((0.95, 0.80, 0.42), (0.80, 0.45, 0.25), (0.45, 0.25, 0.62),
                                           (0.28, 0.45, 0.85)), "amount": 0.6},
    },
}


class Refused(Exception):
    """not a picture this takes, with the reason"""


def load_rgba(tag):
    """a bitmap tag's first picture as a float32 array 0-1 (rows x columns x 4)"""
    import numpy as np
    columns, rows, rgba = hek.first_bitmap_rgba(tag)
    return np.frombuffer(rgba, np.uint8).reshape(rows, columns, 4).astype(np.float32) / 255


def look(name):
    if name not in LOOKS:
        raise Refused("no look %r (the looks: %s)" % (name, ", ".join(sorted(LOOKS))))
    return LOOKS[name]


def smooth(e0, e1, x):
    import numpy as np
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def blur(x, radius):
    import numpy as np
    from PIL import Image, ImageFilter
    picture = Image.fromarray((np.clip(x, 0, 1) * 255).astype(np.uint8), "L").filter(ImageFilter.GaussianBlur(radius))
    return np.asarray(picture, np.float32) / 255


def ramp3(stops, t):
    import numpy as np
    a, b, c = (np.array(stop, np.float32) for stop in stops)
    t = t[..., None]
    return np.where(t < 0.5, a + (b - a) * (t / 0.5), b + (c - b) * ((t - 0.5) / 0.5))


def matched(multi, shape):
    """the multipurpose picture at the diffuse picture's size"""
    import numpy as np
    from PIL import Image
    if multi.shape[:2] == shape[:2]:
        return multi
    picture = Image.fromarray((multi * 255 + 0.5).astype(np.uint8), "RGBA").resize((shape[1], shape[0]),
                                                                                   Image.BILINEAR)
    return np.asarray(picture).astype(np.float32) / 255


def steel_mask(specular, o):
    """the metal parts (0-1): the specular blurred and thresholded, small islands dropped, holes closed, grown and
    softened, so whole parts turn steel"""
    import numpy as np
    from PIL import Image, ImageFilter
    mask = smooth(o["lo"], o["hi"], blur(specular, o["blur"]))
    grey = Image.fromarray((mask * 255).astype(np.uint8), "L")
    grey = grey.filter(ImageFilter.MinFilter(o["shrink"])).filter(ImageFilter.MaxFilter(o["shrink"]))
    grey = grey.filter(ImageFilter.MaxFilter(o["close"])).filter(ImageFilter.MinFilter(o["close"]))
    grey = grey.filter(ImageFilter.MaxFilter(o["grow"]))
    return blur(np.asarray(grey, np.float32) / 255, 3)


def rectangle(shape, box, feather=6):
    import numpy as np
    mask = np.zeros(shape, np.float32)
    x0, y0, x1, y1 = box
    mask[y0:y1, x0:x1] = 1
    return blur(mask, feather)


def glow_mask(rgba, multi, o):
    """the look's glow (0-1, rows x columns), or None for a look without one"""
    import numpy as np
    from PIL import Image, ImageFilter
    if not o["glow"]:
        return None
    g = o["glow"]
    light = rgba[..., :3] @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    grooves = smooth(g["edge"][0], g["edge"][1], -(light - blur(light, 4)))
    grooves = np.asarray(Image.fromarray((grooves * 255).astype(np.uint8), "L").filter(
        ImageFilter.MaxFilter(g["widen"])), np.float32) / 255
    area = sum(rectangle(light.shape, box) for box in g["rects"])
    metal = steel_mask(matched(multi, rgba.shape)[..., 2], o)
    glow = grooves * (1 - metal) * area * g["amount"]
    if g.get("seam"):
        box, width = g["seam"]
        inside = Image.fromarray(((metal > 0.5) * 255).astype(np.uint8), "L")
        seam = (np.asarray(inside.filter(ImageFilter.MaxFilter(width)), np.float32) -
                np.asarray(inside, np.float32)) / 255
        glow = glow + blur(seam * rectangle(light.shape, box, 2), 1) * g["amount"]
    return np.clip(glow, 0, 1)


def tempered(out, level, metal, heat):
    """the blade's steel tinted like tempered steel: the four colours run across the blade's box (three equal spans),
    mixed into bright steel by the amount and shaded by the brightness, on the metal inside the box"""
    import numpy as np
    x0, y0, x1, y1 = heat["box"]
    rows, columns = level.shape
    t = np.clip((np.arange(columns, dtype=np.float32) - x0) / (x1 - x0), 0, 1)[None, :].repeat(rows, 0)
    stops = [np.array(colour, np.float32) for colour in heat["colours"]]
    span = np.minimum((t * 3).astype(int), 2)
    u = (t * 3 - span)[..., None]
    a, b = np.stack(stops[:3])[span], np.stack(stops[1:])[span]
    tint = a + (b - a) * u
    weight = (metal * rectangle(level.shape, heat["box"], 8))[..., None]
    grey = np.array((0.92, 0.92, 0.95), np.float32)
    steel = (grey + (tint - grey) * heat["amount"]) * (0.30 + 0.75 * level[..., None])
    return out * (1 - weight) + np.clip(steel, 0, 1) * weight


def diffuse(rgba, multi, o, body=True):
    """the recoloured diffuse picture (uint8 RGBA); rgba and multi are float32 0-1 arrays; the glow on the body only"""
    import numpy as np
    multi = matched(multi, rgba.shape)
    rgb = rgba[..., :3]
    light = rgb @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    top = np.percentile(light, 99.5) or 1.0
    level = np.clip(light / top, 0, 1) ** 0.85
    metal = steel_mask(multi[..., 2], o)[..., None]
    paint = ramp3(o["paint"], level)
    if o["trim"]:
        cut, colour = o["trim"]
        trim = (1 - smooth(cut * 0.7, cut, blur(level, 2)))[..., None]
        paint = paint * (1 - trim) + np.array(colour, np.float32) * trim
    dark, bright = (np.array(stop, np.float32) for stop in o["metal"])
    steel = dark + (bright - dark) * level[..., None]
    out = paint * (1 - metal) + steel * metal
    most, least = rgb.max(-1), rgb.min(-1)
    saturation = (most - least) / np.maximum(most, 1e-4)
    lights = smooth(0.35, 0.6, saturation) * smooth(0.15, 0.35, most) * (rgb[..., 0] > rgb[..., 2])
    out = out * (1 - lights[..., None]) + np.array(o["lights"]) * most[..., None] * lights[..., None]
    if body and o["heat"]:
        out = tempered(out, level, metal[..., 0], o["heat"])
    glow = glow_mask(rgba, multi, o) if body else None
    if glow is not None:
        out = out * (1 - glow[..., None]) + np.array(o["glow"]["colour"], np.float32) * glow[..., None]
    picture = np.dstack([np.clip(out, 0, 1), rgba[..., 3:]])
    return (picture * 255 + 0.5).astype(np.uint8)


def multipurpose(rgba, multi, o):
    """the body's multipurpose picture (uint8 RGBA, Halo PC's order): CMT's, the look's glow (if any) added to the
    self-illumination channel (green)"""
    import numpy as np
    picture = multi.copy()
    glow = glow_mask(rgba, multi, o)
    if glow is not None:
        picture[..., 1] = np.maximum(picture[..., 1], glow)
    return (np.clip(picture, 0, 1) * 255 + 0.5).astype(np.uint8)


def main(arguments):
    from PIL import Image
    if len(arguments) != 5 or arguments[0] not in ("body", "clip", "multi"):
        print(__doc__)
        return 2
    kind, name, diffuse_tag, multi_tag, out = arguments
    o = look(name)
    rgba = load_rgba(open(diffuse_tag, "rb").read())
    multi = load_rgba(open(multi_tag, "rb").read())
    if kind == "multi":
        picture = multipurpose(rgba, multi, o)
    else:
        picture = diffuse(rgba, multi, o, body=kind == "body")
    Image.fromarray(picture, "RGBA").save(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
