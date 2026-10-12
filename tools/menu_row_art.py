#!/usr/bin/env python3
"""Draws the main menu's rows this port adds (MENUS, and the menu styles it
switches between: item 81) as the main menu's own rows are drawn: pictures of
their text in OpenCE (port/assets/fonts/OpenCE-Regular.ttf), two frames each,
unselected and selected (white, with the rows' blue glow):

    python tools/menu_row_art.py

Each menu style has its own rows, so each gets its own pictures, matched to
its SETTINGS row: the PC version's (port/assets/menus/ce/shell/main_menu/
menu_settings__0/1.png, tools/ce_menus.py) and the Xbox's (port/assets/titles/
main_menu__menu_settings__0/1.png, tools/title_assets.py). From that picture
the text's size, place, colour and opacity are measured, the letters'
tracking fitted to its width, and the selected frame's glow fitted to its
glow; the new texts are then set the same way, centred as the stock rows are.

The style in use is marked by a dot left of its row's text, in the rows' own
blue (the unselected text's colour and opacity; the glow's blue, solid, when
selected).

Writes port/assets/menus/ce/port/main_menu/ (the PC rows, named in
bitmaps.xml) and its xbox_style/ folder (the Xbox's rows, which
port/linux/game/ae_menus_swap.c names). tools/ce_menus.py calls
draw_all() too, after it clears ce/. Needs Pillow, NumPy and SciPy.
"""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "port/assets/fonts/OpenCE-Regular.ttf"
MENUS = ROOT / "port/assets/menus"
# each style's SETTINGS row (the reference) and where its pictures go (relative to port/assets/menus)
STYLES = {
    "pc": (MENUS / "ce/shell/main_menu/menu_settings", "ce/port/main_menu"),
    "xbox": (ROOT / "port/assets/titles/main_menu__menu_settings", "ce/port/main_menu/xbox_style"),
}
REFERENCE_TEXT = "SETTINGS"
# the rows: picture name, text, marked (the style in use)
ROWS = [
    ("menu_menus", "MENUS", False),
    ("menu_style_pc", "PC", False),
    ("menu_style_pc_current", "PC", True),
    ("menu_style_xbox", "XBOX", False),
    ("menu_style_xbox_current", "XBOX", True),
    ("menu_style_arena", "ARENA EVOLVED", False),
    ("menu_back", "BACK", False),
]
WIDTH, HEIGHT = 1024, 256
# the dot: its diameter, and its gap to the text, in the text's cap heights
DOT_SIZE, DOT_GAP = 0.34, 0.5


def _image(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGBA")).astype(float)


class Style:
    """one style's rows, measured from its SETTINGS picture"""

    def __init__(self, reference: Path):
        self.frame0 = _image(Path(f"{reference}__0.png"))
        self.frame1 = _image(Path(f"{reference}__1.png"))
        alpha = self.frame0[..., 3]
        self.alpha0 = float(alpha.max())
        solid = alpha > self.alpha0 * 0.5
        ys, xs = np.where(solid)
        self.top, self.bottom = int(ys.min()), int(ys.max()) + 1
        self.cap = self.bottom - self.top
        self.colour0 = np.median(self.frame0[alpha > self.alpha0 * 0.95][:, :3], axis=0)
        probe = ImageFont.truetype(str(FONT), 1000)
        left, top, right, bottom = probe.getbbox("H")
        self.font = ImageFont.truetype(str(FONT), round(self.cap * 1000 / (bottom - top)))
        self.shift = (0, 0)
        # the tracking that sets SETTINGS as wide as the picture's
        width = xs.max() - xs.min()
        self.tracking = min(np.arange(-8, 12, 0.25),
                            key=lambda tracking: abs(self._width(self.mask(REFERENCE_TEXT, tracking)) - width))
        # the glow: its colour (around the white letters) and the blur and gain that match it best
        alpha1 = self.frame1[..., 3]
        white = (self.frame1[..., :3].min(axis=-1) > 235) & (alpha1 > 250)
        around = ~ndimage.binary_dilation(white, iterations=6) & (alpha1 > 40)
        self.glow_colour = np.median(self.frame1[around][:, :3], axis=0)
        # where the stock rows sit: a little right of the frame's middle (and the letters' own shapes a texel
        # or so off the font's): the shift that covers SETTINGS best, which every row is set with
        mask = self.mask(REFERENCE_TEXT, self.tracking)
        self.shift = max(((dx, dy) for dx in range(-16, 17) for dy in range(-6, 7)),
                         key=lambda shift: self._overlap(np.roll(np.roll(mask, shift[0], 1), shift[1], 0), solid))
        mask = self.mask(REFERENCE_TEXT, self.tracking)
        self.sigma, self.gain = min(((sigma, gain) for sigma in np.arange(4, 20, 1.0) for gain in np.arange(1, 4, 0.25)),
                                    key=lambda pair: np.abs(self._glow(mask, *pair) - alpha1).mean())
        self.fit = self._overlap(mask, solid)

    @staticmethod
    def _overlap(mask: np.ndarray, solid: np.ndarray) -> float:
        return float(((mask > 0.5) & solid).sum() / ((mask > 0.5) | solid).sum())

    @staticmethod
    def _width(mask: np.ndarray) -> int:
        xs = np.where(mask.max(0) > 0.5)[0]
        return int(xs.max() - xs.min())

    def _glow(self, mask: np.ndarray, sigma: float, gain: float) -> np.ndarray:
        return np.maximum(np.minimum(1, gain * ndimage.gaussian_filter(mask, sigma)), mask) * 255

    def mask(self, text: str, tracking: float) -> np.ndarray:
        """the text's coverage (0-1), letter by letter with the tracking, centred across the frame as the
        stock rows are, its caps where the reference's are"""
        canvas = Image.new("L", (WIDTH * 2, HEIGHT), 0)
        draw = ImageDraw.Draw(canvas)
        cap_top = self.font.getbbox("H")[1]
        x = 0.0
        for index, character in enumerate(text):
            draw.text((WIDTH // 2 + x, self.top - cap_top), character, font=self.font, fill=255)
            x += self.font.getlength(text[:index + 1]) - self.font.getlength(text[:index]) + tracking
        mask = np.asarray(canvas).astype(float) / 255
        xs = np.where(mask.max(0) > 0.5)[0]
        shift = int(round((WIDTH - (xs.max() + 1 - xs.min())) / 2)) - xs.min() + self.shift[0]
        return np.roll(np.roll(mask, shift, axis=1)[:, :WIDTH], self.shift[1], axis=0)

    def dot(self, mask: np.ndarray) -> np.ndarray:
        """a dot left of the text, as high as its caps' middle"""
        xs = np.where(mask.max(0) > 0.5)[0]
        radius = self.cap * DOT_SIZE / 2
        centre_x = xs.min() - self.cap * DOT_GAP - radius
        centre_y = (self.top + self.bottom) / 2
        scale = 4
        canvas = Image.new("L", (WIDTH * scale, HEIGHT * scale), 0)
        ImageDraw.Draw(canvas).ellipse([(centre_x - radius) * scale, (centre_y - radius) * scale,
                                        (centre_x + radius) * scale, (centre_y + radius) * scale], fill=255)
        return np.asarray(canvas.resize((WIDTH, HEIGHT), Image.LANCZOS)).astype(float) / 255

    def frames(self, text: str, marked: bool) -> tuple:
        mask = self.mask(text, self.tracking)
        dot = self.dot(mask) if marked else np.zeros_like(mask)
        if mask.max(0)[:8].max() > 0.05 or mask.max(0)[-8:].max() > 0.05:
            raise ValueError(f"{text!r} is wider than a row's picture")
        # unselected: the text and the dot in the rows' colour, at their opacity
        first = np.zeros(mask.shape + (4,))
        first[..., :3] = self.colour0
        first[..., 3] = np.maximum(mask, dot) * self.alpha0
        # selected: white letters over their glow; the dot in the glow's blue, solid, glowing too
        both = np.maximum(mask, dot)
        glow = self._glow(both, self.sigma, self.gain) / 255
        second = np.zeros(mask.shape + (4,))
        second[..., :3] = self.glow_colour
        letters = mask[..., None]
        second[..., :3] = second[..., :3] * (1 - letters) + 255 * letters
        second[..., 3] = np.maximum(glow, both) * 255
        return tuple(Image.fromarray(np.clip(np.round(frame), 0, 255).astype(np.uint8), "RGBA") for frame in (first, second))


def draw_all(menus: Path = MENUS) -> list:
    """draws every style's rows; returns their PNGs' paths, relative to the menus' folder"""
    written = []
    for name, (reference, folder) in STYLES.items():
        style = Style(reference)
        (menus / folder).mkdir(parents=True, exist_ok=True)
        for row, text, marked in ROWS:
            for index, frame in enumerate(style.frames(text, marked)):
                png = f"{folder}/{row}__{index}.png"
                frame.save(menus / png, optimize=True)
                written.append(png)
    return written


def bitmaps_xml(folder: str = STYLES["pc"][1], prefix: str = "main_menu/") -> list:
    """the PC rows' <bitmap>s for bitmaps.xml (tools/ce_menus.py), named main_menu/<row>"""
    lines = []
    for row, _, _ in ROWS:
        lines.append(f'\t<bitmap name="{prefix}{row}">')
        lines += [f'\t\t<frame png="{folder}/{row}__{index}.png" width="256" height="64"/>' for index in range(2)]
        lines.append("\t</bitmap>")
    return lines


if __name__ == "__main__":
    for name, (reference, _) in STYLES.items():
        style = Style(reference)
        print(f"{name}: cap {style.cap} px, tracking {style.tracking}, shift {style.shift}, glow sigma {style.sigma} gain {style.gain}, "
              f"SETTINGS overlap {style.fit:.3f}")
    print(f"{len(draw_all())} pictures")
