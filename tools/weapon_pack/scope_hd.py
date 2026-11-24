"""The battle rifle's sharp scope: CMT's scope art (used with credit) redrawn four times as large with crisp
edges, for the bitmap compiler (2D textures, mipmapped). The HUD draws a bitmap at its pixel size times the
overlay's scale, so a 4x bitmap at a quarter of the scale is the same size on screen with four times the texels.

  br_scope_details_hd.png  1024 x 1024: CMT's scope lines (battle_rifle_scope_details_2x). The 256 px art
      (alpha^0.5, the two lines of small print removed, centred) is enlarged 4x and cut at a level chosen so that
      the lines carry the same ink as the chosen thickness step: sharp edges a texel wide at 4x instead of a soft
      halo, so the lines read by brightness, not width. Grey lines on black, alpha the same (the HUD's colour
      tints them). Steps: t1 = CMT's line width, t2 = the middle (the approved one), t3 = a pixel wider each side.
      "bold" = the solid scope: main lines a pixel wider each side at full ink, the faint arcs left out, and the
      ring one solid band (drawn black by the HUD: the recipe's colour).
  br_scope_ring_hd.png     1024 x 1024: the ring of CMT's scope mask (battle_rifle_scope_mask: two thin circles
      joined by four ticks) redrawn as geometry at the measured centre and radii, 3.7 texels to the mask's one.
  br_scope_mask_noring.png 2048 x 512: CMT's mask with the ring's band cleared (its soft vignette stays as it
      was), its edges carried out to twice the width so that a wide split-screen view is covered.

Usage: scope_hd.py <CMT details tag> <CMT mask tag> <folder> [t1|t2|t3]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import hek


class Refused(Exception):
    """no art was made, with the reason"""


SCALE = 4
RING_SPAN = 280.0            # mask texels the ring's texture covers
RING_SIZE = 1024
# the bold ring's width in mask texels (one is 2.25 px on a 1080-line screen): the pistol scope's circle, measured
# in the game at 2.7 px on 864 lines = 3.4 px on 1080
BOLD_RING = 1.5
# how far the bold lines are widened toward a pixel each side (1: all of it; the owner asked for a bit thinner
# than that for the markings inside the ring)
BOLD_WIDEN = 0.45
BOLD_KEEP = 0.5             # the "bold" step keeps art at least this strong (lifted, x1.4): the main lines


def load_alpha(path):
    """the first bitmap of a tag file: its alpha, 0..1, as float32"""
    import numpy as np
    with open(path, "rb") as file:
        width, height, rgba = hek.first_bitmap_rgba(file.read())
    return (np.frombuffer(rgba, np.uint8).reshape(height, width, 4).astype(np.float32) / 255.0)[..., 3]


def details(alpha, ink, label=None):
    """(the lines at 4x as bytes, the cut level, the ink drawn, the ink wanted) from the 256 x 256 alpha; the bold
    scope's zoom label reads "[<label>x]", or with no label (None or "") none at all"""
    import numpy as np
    from PIL import Image, ImageFilter
    a = np.array(alpha, dtype=np.float32, copy=True)
    a[95:118, 5:70] = 0.0                       # the small print (rifle and round names): unreadable at any size
    # CMT's sprite is centred at 121 px (registration 0.4727 of 256); ours is centred at 128: the art moves by 7
    a = np.roll(np.roll(a, 7, axis=0), 7, axis=1)
    a[:7, :] = 0
    a[:, :7] = 0
    lift = np.power(a, 0.5)
    thick = np.array(Image.fromarray((lift * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))).astype(
        np.float32) / 255.0
    steps = {"t1": lift, "t2": np.maximum(lift, thick * 0.45), "t3": np.maximum(lift, thick * 0.85)}
    # "bold" (the solid black scope): the main lines a pixel wider each side and at full ink, the faint arcs and
    # inner circles left out, so the lines are either there, crisp and whole, or not there
    main_lines = (np.array(Image.fromarray((lift * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))).astype(
        np.float32) / 255.0 * 1.4) > BOLD_KEEP
    steps["bold"] = np.maximum(lift, thick * BOLD_WIDEN) * main_lines
    if ink not in steps:
        raise Refused("no such thickness step: %s" % ink)
    want = steps[ink].sum() * 16.0                    # the step's ink, in 4x texels

    def up(x):
        return np.array(Image.fromarray((np.clip(x, 0, 1) * 255).astype(np.uint8)).resize(
            (256 * SCALE, 256 * SCALE), Image.LANCZOS)).astype(np.float32) / 255.0

    # (shape from a strongly lifted copy, so that the faint rings keep their shape; their strength from the art, a
    # little brighter, so faint stays fainter than the main lines but crisp)
    big = up(np.power(a, 0.25))
    strength = np.clip(up(np.array(Image.fromarray((lift * 255).astype(np.uint8)).filter(
        ImageFilter.MaxFilter(5))).astype(np.float32) / 255.0) * 1.4, 0, 1)

    if ink == "bold":
        strength = (strength > BOLD_KEEP).astype(np.float32)

    def cut(level, soft=0.06):
        x = np.clip((big - (level - soft)) / (2 * soft), 0, 1)
        return x * x * (3 - 2 * x) * strength

    lo, hi = 0.02, 0.9
    for _ in range(40):                                # the level whose ink matches the step's
        mid = (lo + hi) / 2
        if cut(mid).sum() > want:
            lo = mid
        else:
            hi = mid
    det = cut((lo + hi) / 2)
    if ink == "bold":
        det = bold_label(det, label)
    return (det * 255).astype(np.uint8), (lo + hi) / 2, float(det.sum()), float(want)


BOLD_LABEL = "3"            # the one zoom the bold scope's label can read: "[3x]"


def bold_label(det, digit):
    """the bold sheet (1024 x 1024, 0..1) with CMT's zoom label and its "1x" small print cleared and our own
    "[3x]" drawn in the label's place: thickened, CMT's small lettering runs together, so the bold scope's is drawn
    here as strokes of a round pen (brackets, a 3 with a flat top, a slope and a bowl, an x). No digit (None or
    ""): the labels cleared and none drawn (the owner, 10-30: no zoom indication, the zoom may be retuned)"""
    import numpy as np
    from PIL import Image, ImageDraw
    if digit and digit != BOLD_LABEL:
        raise Refused("only the 3 is drawn")
    det = np.array(det, dtype=np.float32, copy=True)
    det[680:790, 570:724] = 0.0                  # "[2x]"
    det[905:962, 592:652] = 0.0                  # "1x"
    if not digit:
        return det
    X0, Y0, W, H, SS, PEN = 570, 680, 154, 110, 4, 7.5
    image = Image.new("L", (W * SS, H * SS), 0)
    draw = ImageDraw.Draw(image)

    def stroke(points):
        scaled = [((x - X0) * SS, (y - Y0) * SS) for x, y in points]
        draw.line(scaled, fill=255, width=int(PEN * SS), joint="curve")
        r = PEN * SS / 2
        for x, y in scaled:                      # round ends and joints
            draw.ellipse((x - r, y - r, x + r, y + r), fill=255)

    stroke([(600, 700), (588, 700), (588, 770), (600, 770)])                                  # [
    stroke([(612, 704), (646, 704), (627, 730), (640, 733), (647, 748), (640, 762), (625, 767), (611, 761)])  # 3
    stroke([(658, 732), (686, 766)])                                                          # x
    stroke([(686, 732), (658, 766)])
    stroke([(698, 700), (710, 700), (710, 770), (698, 770)])                                  # ]
    label = np.array(image.resize((W, H), Image.LANCZOS)).astype(np.float32) / 255.0
    det[Y0:Y0 + H, X0:X0 + W] = np.maximum(det[Y0:Y0 + H, X0:X0 + W], label)
    return np.clip(det, 0.0, 1.0)


def ring_and_mask(alpha, bold=False):
    """(the ring's texture as bytes, the mask without its ring at twice the width, what was measured) from the
    mask's alpha"""
    import numpy as np
    al = alpha
    H0, W0 = al.shape[:2]
    # centre and radii from the inner circle (first alpha > 0.5 going out from the middle on 16 rays)
    cy, cx = (H0 - 1) / 2.0, (W0 - 1) / 2.0
    pts = []
    for k in range(16):
        t = k * np.pi / 8
        c, s = np.cos(t), np.sin(t)
        for r in np.arange(100, 140, 0.25):
            x, y = cx + r * c, cy + r * s
            if al[int(round(y)), int(round(x))] > 0.5:
                pts.append((x, y))
                break
    if len(pts) < 3:
        raise Refused("no ring found in the mask")
    P = np.array(pts)
    A = np.c_[2 * P, np.ones(len(P))]
    bb = (P ** 2).sum(1)
    sol = np.linalg.lstsq(A, bb, rcond=None)[0]
    mx, my = sol[0], sol[1]
    r_in = np.sqrt(sol[2] + mx * mx + my * my)
    # the ring: its own texture covering RING_SPAN mask texels around the ring's centre (drawn as its own
    # crosshair item, black as the mask is); the mask keeps its vignette with the ring's band cleared
    RS = RING_SIZE
    k = RS / RING_SPAN
    yy, xx = np.mgrid[0:RS, 0:RS]
    r = np.hypot((xx + 0.5 - RS / 2) / k, (yy + 0.5 - RS / 2) / k)           # in mask texels
    th = np.arctan2(yy + 0.5 - RS / 2, xx + 0.5 - RS / 2)
    aa = 1.2 / k

    def band(r0, r1):
        return np.clip((r - r0) / aa + 0.5, 0, 1) * np.clip((r1 - r) / aa + 0.5, 0, 1)

    ri = r_in
    # (bold: one solid circle of the pistol scope's weight, in place of two thin circles and their ticks)
    ring = band(ri, ri + BOLD_RING) if bold else np.maximum(band(ri, ri + 1.1), band(ri + 3.0, ri + 4.1))
    for t0 in (() if bold else (0, np.pi / 2, np.pi, -np.pi / 2)):
        d = np.abs(np.angle(np.exp(1j * (th - t0)))) * r
        ring = np.maximum(ring, np.clip((0.55 - d) / aa + 0.5, 0, 1) * band(ri, ri + 4.1))
    g = (ring * 255).astype(np.uint8)
    yy, xx = np.mgrid[0:H0, 0:W0]
    rm = np.hypot(xx - mx, yy - my)
    keep = np.clip(rm - (ri + 4.6), 0, 1)                                     # the ring's band and its gap cleared
    v = (al * keep * 255).astype(np.uint8)
    # (twice as wide, its edges carried out: CMT's 2:1 mask left the sides of a wide split-screen view uncovered)
    v = np.pad(v, ((0, 0), (W0 // 2, W0 // 2)), mode="edge")
    return g, v, {"centre": (float(mx), float(my)), "inner": float(r_in), "size": (W0, H0)}


def main(arguments):
    if len(arguments) not in (3, 4):
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    import numpy as np
    from PIL import Image
    details_tag, mask_tag, out = arguments[:3]
    ink = arguments[3] if len(arguments) > 3 else "t2"
    try:
        v, level, drawn, wanted = details(load_alpha(details_tag), ink, BOLD_LABEL)
        g, mask, measured = ring_and_mask(load_alpha(mask_tag), ink == "bold")
    except (Refused, hek.Refused) as refusal:
        print("scope_hd: refused: %s" % refusal, file=sys.stderr)
        return 1
    os.makedirs(out, exist_ok=True)
    # (the HUD draws this texture's colour: grey lines on black, alpha the same, as CMT's sprite has it)
    rgba = np.zeros((256 * SCALE, 256 * SCALE, 4), np.uint8)
    rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = v
    rgba[..., 3] = v
    Image.fromarray(rgba, "RGBA").save(os.path.join(out, "br_scope_details_hd.png"))
    rg = np.zeros((RING_SIZE, RING_SIZE, 4), np.uint8)
    rg[..., 0] = rg[..., 1] = rg[..., 2] = 255
    rg[..., 3] = g
    Image.fromarray(rg, "RGBA").save(os.path.join(out, "br_scope_ring_hd.png"))
    mk = np.zeros(mask.shape + (4,), np.uint8)
    mk[..., 0] = mk[..., 1] = mk[..., 2] = mask
    mk[..., 3] = mask
    Image.fromarray(mk, "RGBA").save(os.path.join(out, "br_scope_mask_noring.png"))
    print("details: level %.3f, ink %.0f (step %s %.0f)" % (level, drawn, ink, wanted))
    print("mask %dx%d: ring centre (%.2f, %.2f), inner edge %.2f; ring %dx%d over %.0f mask texels (scale %.4f)" % (
        measured["size"][0], measured["size"][1], measured["centre"][0], measured["centre"][1], measured["inner"],
        RING_SIZE, RING_SIZE, RING_SPAN, RING_SPAN / RING_SIZE))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
