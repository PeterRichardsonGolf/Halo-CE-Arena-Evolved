"""The EVO rifle's zoom overlay (our own drawing, the owner's pick D8, 10-30: Halo CE's own scope look, its soft black
surround, on a wide rounded window, with black glass marks and two readouts at the window's sides). The game draws the
readouts' values live (port/linux/game/ae_scope_readout.c: the zoom, "2x", and the rounds loaded); this art has their
brackets and labels.

  mask     2048 x 512 RGBA, white, alpha the darkness (the HUD draws it black): clear in a 4:3 rounded window, fading
           to 0.88 across its edge (a third of the fade inside, so the clear glass is the window's size), as dark past
           it; one texel a HUD unit (480 to the screen's height), as wide as CMT's masks were carried out
  details  1024 x 1024 RGBA, two texels a HUD unit (the overlay's scale 0.5): the glass marks black (the level line,
           its ladder, the posts, the brackets round the reticle) with a faint light edge under them, the readouts'
           brackets and labels light (the HUD's colour tints them) with a dark edge under them; colour premultiplied
           by alpha (the HUD adds it: black where nothing is drawn)

Everything is placed in HUD units from the centre (WINDOW, MARKS, READOUT below); ae_scope_readout.c places the values
by the same numbers. Drawn 4 x supersampled.
Usage: evo_scope.py <out mask png> <out details png>"""
import os
import sys

WINDOW = (186.2, 140.0, 44.8, 26.7)       # half width, half height, corner radius, the edge's fade (HUD units)
DARK = 0.936                              # the surround's darkness (the HUD draws the mask at 0.94: 0.88 seen)
LEVEL = (74.7, 119.6)                     # the level line, a segment a side: from, to
LADDER = (52.4, 4.0, 5, 2.7)              # the ladder: first tick, spacing, ticks, half height
POSTS = (62.2, 124.0)                     # the posts above and below: from, to
BRACKET = (40.9, 36.0, 110.0)             # the brackets round the reticle: radius, its end ticks' inner radius, degrees
READOUT = (171.1, 20.4, 5.8, 164.9, -11.6, 6.2)   # the readouts: bracket spine, half height, angled end; label's
                                          # outer x, baseline, em
THIN, THICK = 0.89, 1.33                  # the marks' two weights (2 and 3 px at 1080p)
FONTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "port", "assets", "fonts")
SS = 4


def mask():
    import numpy as np
    from PIL import Image
    hw, hh, rad, soft = WINDOW
    y, x = np.mgrid[0:512, 0:2048].astype(np.float64)
    dx, dy = np.abs(x + 0.5 - 1024), np.abs(y + 0.5 - 256)
    qx, qy = dx - (hw - rad), dy - (hh - rad)
    d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rad
    t = np.clip((d + soft / 3) / soft, 0, 1)
    alpha = (t * t * (3 - 2 * t)) * DARK
    out = np.dstack([np.full(alpha.shape, 255.0)] * 3 + [alpha * 255]).round().astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def details():
    import numpy as np
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
    n = 1024 * SS
    k = 2 * SS                            # supersampled texels a HUD unit
    c = n / 2

    def p(x, y):
        return (c + x * k, c + y * k)
    black = Image.new("L", (n, n), 0)     # the black marks' coverage
    light = Image.new("L", (n, n), 0)     # the light readouts' coverage
    gb, gl = ImageDraw.Draw(black), ImageDraw.Draw(light)
    thin, thick = int(round(THIN * k)), int(round(THICK * k))
    for sx in (-1, 1):
        gb.line((*p(sx * LEVEL[0], 0), *p(sx * LEVEL[1], 0)), fill=255, width=thin)
        first, step, count, half = LADDER
        for i in range(count):
            x = sx * (first + i * step)
            gb.line((*p(x, -half), *p(x, half)), fill=255, width=thin)
    for sy in (-1, 1):
        gb.line((*p(0, sy * POSTS[0]), *p(0, sy * POSTS[1])), fill=255, width=thick)
    radius, inner, sweep = BRACKET
    box = (*p(-radius, -radius), *p(radius, radius))
    for start in (180 - sweep / 2, -sweep / 2):
        gb.arc(box, start, start + sweep, fill=255, width=thick)
    for degrees in (180 - sweep / 2, 180 + sweep / 2, -sweep / 2, sweep / 2):
        a = np.radians(degrees)
        gb.line((*p(radius * np.cos(a), radius * np.sin(a)), *p(inner * np.cos(a), inner * np.sin(a))), fill=255,
                width=thick)
    spine, half, end, label_x, label_y, em = READOUT
    font = ImageFont.truetype(os.path.join(FONTS, "Overpass-750.ttf"), int(round(em * k)))
    for sx, label in ((-1, "MAG"), (1, "AMMO")):
        x = sx * spine
        gl.line((*p(x - sx * end, -half - end), *p(x, -half), *p(x, half), *p(x - sx * end, half + end)), fill=255,
                width=thick, joint="curve")
        gl.text(p(sx * label_x, label_y), label, font=font, fill=255, anchor="rs" if sx > 0 else "ls")

    def down(img):
        return np.asarray(img.resize((1024, 1024), Image.LANCZOS)).astype(np.float64) / 255

    def edge(img):
        grown = img.filter(ImageFilter.MaxFilter(4 * SS + 1)).filter(ImageFilter.GaussianBlur(1.2 * SS))
        return down(grown)
    b, l = down(black), down(light)
    under_b, under_l = edge(black) * 0.22, edge(light) * 0.5
    # (back to front: the light edge under the black marks, the black marks, the dark edge under the readouts, the
    # readouts; straight colour, alpha over)
    rgb = np.zeros((1024, 1024)) + 1.0
    alpha = np.zeros((1024, 1024))
    for value, cover in ((1.0, under_b), (0.0, b * 0.92), (0.0, under_l), (1.0, l * 0.9)):
        out_alpha = cover + alpha * (1 - cover)
        rgb = np.where(out_alpha > 0, (value * cover + rgb * alpha * (1 - cover)) / np.maximum(out_alpha, 1e-9), rgb)
        alpha = out_alpha
    # (the HUD adds a crosshair's colour over the view and takes its alpha's share away, so the colour is the
    # premultiplied one: black where nothing is drawn, as CMT's scope pictures are)
    out = np.dstack([rgb * alpha * 255] * 3 + [alpha * 255]).round().clip(0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


if __name__ == "__main__":
    mask().save(sys.argv[1])
    details().save(sys.argv[2])
