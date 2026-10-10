#!/usr/bin/env python3
"""Arena Evolved's logo, ORBIT (a band ring seen at an angle, its near side
lit amber and its far side in burnt shade), and every file made from it:

    python tools/ae_logo.py [--root DIR]

The mark is drawn here from two ellipses and two clip rectangles; nothing is
traced or downloaded. The lockups' text is Overpass (port/assets/fonts,
SIL Open Font License), converted to outlines, so no SVG needs the font.
Writes, under the repository (or under DIR, with the same layout):

- port/assets/branding/: the vector sources (the mark, its small master for
  32 px and below, its light and one-colour versions, the app tiles, the
  wordmark lockups and the Android layers) and the GitHub pictures (the
  social preview, 1280x640, and the README's banner, 1280x320, dark and
  light), each as SVG and PNG;
- port/windows/ae-icon.ico: halo.exe's icon (port/windows/halo.rc), and the
  Windows window's (SDL takes the executable's first icon);
- port/assets/icon/ae-icon-256.png: the other desktops' window icon
  (tools/embed_assets.py embeds it; sdl_platform.c), and the Linux
  download's arena-evolved.png (tools/ci_build.py);
- port/macos/AppIcon.icns: the macOS application's icon;
- port/android/app/src/main/res/: the Android launcher icon, an adaptive one
  (mipmap-anydpi-v26/ic_launcher.xml) with its foreground, background and
  monochrome (themed icon) layers for every screen density.

At 32 px and below every icon uses the small master (a heavier ring, larger
on its tile). tools/test_ae_logo.py checks that the committed files are what
this makes. Needs Pillow, fontTools and rsvg-convert (librsvg).

The palette (hex): amber #FFA51F, signal #FF6A13, burnt #C2510E (the mark
on light backgrounds), ember #5A2208, black #0A0A0B (the app tile), near
#141417, graphite #2A2A2F, steel #8E8E96 ("HALO CE:"), bone #F3EFE7 (warm
white). The mark is Arena Evolved's own work.
"""

import argparse
import io
import math
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
FONTS = ROOT / "port/assets/fonts"

BRANDING = Path("port/assets/branding")
WINDOW_ICON = Path("port/assets/icon/ae-icon-256.png")
WINDOWS_ICON = Path("port/windows/ae-icon.ico")
MACOS_ICON = Path("port/macos/AppIcon.icns")
RESOURCES = Path("port/android/app/src/main/res")

PAL = {
    "amber": "#FFA51F",
    "signal": "#FF6A13",
    "burnt": "#C2510E",
    "ember": "#5A2208",
    "black": "#0A0A0B",
    "near": "#141417",
    "graphite": "#2A2A2F",
    "steel": "#8E8E96",
    "bone": "#F3EFE7",
}
# the mark's two tones: on dark backgrounds, and on light ones
DARK = (PAL["amber"], PAL["burnt"])
LIGHT = (PAL["burnt"], PAL["ember"])
# the "HALO CE:" line on light backgrounds
SUB_LIGHT = "#5C5C64"

# halo.exe's icon sizes (Explorer, the taskbar and the title bar ask for
# these, 20 and 40 at 125 % scale); the small master up to SMALL
WINDOWS_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
SMALL = 32
# the macOS icon's entries: type, pixels
MACOS_ENTRIES = [("icp4", 16), ("icp5", 32), ("ic11", 32), ("icp6", 64), ("ic12", 64), ("ic07", 128),
                 ("ic13", 256), ("ic08", 256), ("ic14", 512), ("ic09", 512), ("ic10", 1024)]
# Android: the adaptive layers' size in dp, the box the mark is drawn in
# (centred; its ring, 118/256 of the box from the middle, stays inside the
# 66 dp safe zone no mask cuts into), and each density's pixels per dp
LAYER_DP = 108
ANDROID_MARK_DP = 71
DENSITIES = {"mdpi": 1.0, "hdpi": 1.5, "xhdpi": 2.0, "xxhdpi": 3.0, "xxxhdpi": 4.0}


def f(value: float) -> str:
    """a number as the SVGs write it: two decimals at most"""
    text = f"{value:.2f}".rstrip("0").rstrip(".")
    return "0" if text == "-0" else text


# ------------------------------------------------------------------ the mark

def orbit(near: str, far: str, small: bool = False) -> tuple:
    """The mark in a 256 x 256 box: a band ring (an ellipse less an inner one
    raised by `lift`), tilted 22 degrees, its far half (above the cut)
    `far`, its near half `near`. The small master is heavier. Returns
    (body, defs)."""
    ry, hole_ry, lift = (60, 30, 12) if not small else (68, 30, 18)
    rx, hole_rx = (118, 88) if not small else (120, 82)
    band = (f"M{128 - rx},128A{rx},{ry} 0 1 1 {128 + rx},128A{rx},{ry} 0 1 1 {128 - rx},128Z"
            f"M{128 - hole_rx},{128 - lift}A{hole_rx},{hole_ry} 0 1 0 {128 + hole_rx},{128 - lift}"
            f"A{hole_rx},{hole_ry} 0 1 0 {128 - hole_rx},{128 - lift}Z")
    defs = (f'<clipPath id="far"><rect x="0" y="0" width="256" height="{128 - lift / 2}"/></clipPath>'
            f'<clipPath id="near"><rect x="0" y="{128 - lift / 2}" width="256" height="256"/></clipPath>')
    body = (f'<g transform="rotate(-22 128 128)">'
            f'<path fill-rule="evenodd" fill="{far}" clip-path="url(#far)" d="{band}"/>'
            f'<path fill-rule="evenodd" fill="{near}" clip-path="url(#near)" d="{band}"/></g>')
    return body, defs


def svg(body: str, defs: str = "", size: int = 256) -> str:
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 256 256">\n'
            f'{("<defs>" + defs + "</defs>") if defs else ""}\n{body}\n</svg>\n')


def mark_svg(near: str, far: str, small: bool = False, tile: bool = False) -> str:
    """The mark alone, or on the app icon's black rounded tile (at 74 % of it;
    the small master at 86 %, on a tighter corner)."""
    body, defs = orbit(near, far, small)
    if tile:
        scale = 0.74 if not small else 0.86
        body = (f'<rect x="4" y="4" width="248" height="248" rx="{56 if not small else 40}" fill="{PAL["black"]}"/>'
                f'<g transform="translate({128 - 128 * scale:.2f},{128 - 128 * scale:.2f}) scale({scale})">{body}</g>')
    return svg(body, defs)


def place(near: str, far: str, x: float, y: float, size: float, uid: str, small: bool = False) -> tuple:
    """(defs, body) of the mark at (x, y), `size` across, its ids made
    unique, for a larger SVG"""
    body, defs = orbit(near, far, small)
    for name in ("far", "near"):
        defs = defs.replace(f'id="{name}"', f'id="{uid}-{name}"')
        body = body.replace(f"url(#{name})", f"url(#{uid}-{name})")
    return defs, f'<g transform="translate({f(x)},{f(y)}) scale({size / 256:.5f})">{body}</g>'


# ------------------------------------------------------------------ text as outlines

class Face:
    def __init__(self, path: Path):
        self.font = TTFont(path)
        self.glyphs = self.font.getGlyphSet()
        self.cmap = self.font.getBestCmap()
        self.units = self.font["head"].unitsPerEm
        self.cap = self.font["OS/2"].sCapHeight / self.units

    def width(self, text: str, size: float, track: float = 0.0) -> float:
        return sum(self.glyphs[self.cmap[ord(c)]].width / self.units * size + track * size for c in text) - track * size

    def path(self, text: str, x: float, baseline: float, size: float, track: float = 0.0) -> str:
        scale = size / self.units
        pen = SVGPathPen(self.glyphs, ntos=f)
        for character in text:
            glyph = self.glyphs[self.cmap[ord(character)]]
            glyph.draw(TransformPen(pen, (scale, 0, 0, -scale, x, baseline)))
            x += glyph.width * scale + track * size
        return pen.getCommands()


_faces = {}


def faces() -> tuple:
    """Overpass Black (900, "ARENA EVOLVED") and ExtraBold (750, "HALO CE:")"""
    if not _faces:
        _faces[900] = Face(FONTS / "Overpass-900.ttf")
        _faces[750] = Face(FONTS / "Overpass-750.ttf")
    return _faces[900], _faces[750]


# letter spacing, in em: the main line, and the "HALO CE:" line
TRACK_MAIN, TRACK_SMALL = 0.045, 0.30


def lockup(near: str, far: str, text: str, sub: str, kind: str, uid: str) -> str:
    """The mark with the name: "full" (HALO CE: over ARENA EVOLVED),
    "wordmark" (ARENA EVOLVED) or "one-line" (HALO CE: ARENA EVOLVED)"""
    heavy, bold = faces()
    size = 100.0
    cap = heavy.cap * size
    if kind == "full":
        small_size = size * 0.34
        small_cap = bold.cap * small_size
        block = small_cap + cap * 0.30 + cap
        mark = block / 0.80
        mark_y = block / 2 - mark / 2
        x = mark * 1.12
        defs, body = place(near, far, 0, mark_y, mark, uid)
        parts = [body,
                 f'<path fill="{sub}" d="{bold.path("HALO CE:", x + size * 0.02, small_cap, small_size, TRACK_SMALL)}"/>',
                 f'<path fill="{text}" d="{heavy.path("ARENA EVOLVED", x, block, size, TRACK_MAIN)}"/>']
        width = x + heavy.width("ARENA EVOLVED", size, TRACK_MAIN)
        top = min(mark_y, 0)
        height = max(mark_y + mark, block) - top
    else:
        mark = cap / 0.80 * 1.08
        mark_y = cap / 2 - mark / 2
        x = mark * 1.12
        defs, body = place(near, far, 0, mark_y, mark, uid)
        parts = [body]
        if kind == "one-line":
            parts.append(f'<path fill="{sub}" d="{bold.path("HALO CE:", x, cap, size, TRACK_MAIN * 2)}"/>')
            x += bold.width("HALO CE:", size, TRACK_MAIN * 2) + size * 0.36
        parts.append(f'<path fill="{text}" d="{heavy.path("ARENA EVOLVED", x, cap, size, TRACK_MAIN)}"/>')
        width = x + heavy.width("ARENA EVOLVED", size, TRACK_MAIN)
        top, height = mark_y, mark
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{f(width)}" height="{f(height)}" '
            f'viewBox="0 {f(top)} {f(width)} {f(height)}"><defs>{defs}</defs>{"".join(parts)}</svg>\n')


# ------------------------------------------------------------------ Android, GitHub

def android_svgs() -> dict:
    """the adaptive icon's layers on the 108 dp canvas"""
    offset = (LAYER_DP - ANDROID_MARK_DP) / 2

    def layer(near, far, uid):
        defs, body = place(near, far, offset, offset, ANDROID_MARK_DP, uid)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="432" height="432" viewBox="0 0 108 108">'
                f'<defs>{defs}</defs>{body}</svg>\n')
    background = ('<svg xmlns="http://www.w3.org/2000/svg" width="432" height="432" viewBox="0 0 108 108">'
                  '<defs><radialGradient id="g" cx="0.5" cy="0.62" r="0.62">'
                  f'<stop offset="0" stop-color="#2A1408"/><stop offset="1" stop-color="{PAL["black"]}"/>'
                  '</radialGradient></defs><rect width="108" height="108" fill="url(#g)"/></svg>\n')
    return {"android-foreground.svg": layer(*DARK, "fg"), "android-background.svg": background,
            "android-monochrome.svg": layer("#FFFFFF", "#FFFFFF", "mo")}


def social_svg() -> str:
    """GitHub's social preview, 1280x640: the name on the left, the mark
    large on the right, an ember glow; all inside a 40 px margin"""
    heavy, bold = faces()
    defs, mark = place(*DARK, 850, 130, 380, "big")
    size = 92
    cap = heavy.cap * size
    x, base = 88, 330
    line1 = bold.path("HALO CE:", x + 3, base - cap - 34, size * 0.34, TRACK_SMALL)
    line2 = heavy.path("ARENA EVOLVED", x, base, size, TRACK_MAIN)
    line3 = bold.path("Arena play, modern options. Free and open source.", x + 2, base + 70, 28, 0.01)
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="640" viewBox="0 0 1280 640">'
            f'<defs>{defs}<radialGradient id="glow" cx="0.80" cy="0.5" r="0.55">'
            f'<stop offset="0" stop-color="#2A1408"/><stop offset="1" stop-color="{PAL["black"]}"/></radialGradient></defs>'
            f'<rect width="1280" height="640" fill="url(#glow)"/>{mark}'
            f'<path fill="{PAL["steel"]}" d="{line1}"/><path fill="{PAL["bone"]}" d="{line2}"/>'
            f'<rect x="{x}" y="{base + 22}" width="120" height="6" fill="{DARK[0]}"/>'
            f'<path fill="{PAL["steel"]}" d="{line3}"/></svg>\n')


def banner_svg(light: bool = False) -> str:
    """the README's banner, 1280x320: the mark and the name in one row; the
    light one (for GitHub's light theme) in the mark's light colourway"""
    heavy, bold = faces()
    near, far = LIGHT if light else DARK
    defs, mark = place(near, far, 96, 60, 200, "bn")
    size = 84
    cap = heavy.cap * size
    x = 340
    base = 160 + cap / 2 + 14
    line1 = bold.path("HALO CE:", x + 3, base - cap - 26, size * 0.34, TRACK_SMALL)
    line2 = heavy.path("ARENA EVOLVED", x, base, size, TRACK_MAIN)
    glow, page, sub, text = (("#F9E6D8", "#FFFFFF", SUB_LIGHT, PAL["black"]) if light
                             else ("#1E0F06", PAL["black"], PAL["steel"], PAL["bone"]))
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="320" viewBox="0 0 1280 320">'
            f'<defs>{defs}<linearGradient id="gl" x1="0" x2="1"><stop offset="0" stop-color="{glow}"/>'
            f'<stop offset="0.55" stop-color="{page}"/></linearGradient></defs>'
            f'<rect width="1280" height="320" fill="url(#gl)"/>{mark}'
            f'<path fill="{sub}" d="{line1}"/><path fill="{text}" d="{line2}"/>'
            f'<rect x="{x}" y="{f(base + 20)}" width="100" height="5" fill="{near}"/></svg>\n')


def sources() -> dict:
    """every vector source, by file name (port/assets/branding)"""
    result = {
        "mark.svg": mark_svg(*DARK),
        "mark-small.svg": mark_svg(*DARK, small=True),
        "mark-light.svg": mark_svg(*LIGHT),
        "mark-orange.svg": mark_svg(DARK[0], DARK[0]),
        "mark-white.svg": mark_svg(PAL["bone"], PAL["bone"]),
        "mark-black.svg": mark_svg(PAL["black"], PAL["black"]),
        "app-icon-tile.svg": mark_svg(*DARK, tile=True),
        "app-icon-tile-small.svg": mark_svg(*DARK, small=True, tile=True),
    }
    for kind in ("full", "wordmark", "one-line"):
        name = f"lockup-{kind}"
        result[f"{name}-dark.svg"] = lockup(*DARK, PAL["bone"], PAL["steel"], kind, name)
        result[f"{name}-light.svg"] = lockup(*LIGHT, PAL["black"], SUB_LIGHT, kind, name)
    result["lockup-wordmark-orange.svg"] = lockup(DARK[0], DARK[0], DARK[0], DARK[0], "wordmark", "wo")
    result.update(android_svgs())
    result["social-preview-1280x640.svg"] = social_svg()
    result["readme-banner-1280x320.svg"] = banner_svg()
    result["readme-banner-light-1280x320.svg"] = banner_svg(light=True)
    return result


# ------------------------------------------------------------------ rasters and containers

def rasterize(text: str, width: int, height: int) -> Image.Image:
    """an SVG as RGBA pixels (rsvg-convert)"""
    result = subprocess.run(["rsvg-convert", "-w", str(width), "-h", str(height), "-f", "png"],
                            input=text.encode(), capture_output=True, check=True)
    return Image.open(io.BytesIO(result.stdout)).convert("RGBA")


def png(image: Image.Image) -> bytes:
    out = io.BytesIO()
    image.save(out, format="PNG", optimize=True)
    return out.getvalue()


def ico(images: dict) -> bytes:
    """a Windows icon of PNG images (Windows Vista and later read them at
    every size), by size"""
    sizes = sorted(images)
    header = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    entries, data = b"", b""
    for size in sizes:
        blob = images[size]
        side = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", side, side, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return header + entries + data


def icns(entries: list) -> bytes:
    """a macOS icon of PNG images: (type, PNG) pairs"""
    body = b"".join(kind.encode() + struct.pack(">I", 8 + len(blob)) + blob for kind, blob in entries)
    return b"icns" + struct.pack(">I", 8 + len(body)) + body


ADAPTIVE_ICON = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    "<!-- the launcher icon, Arena Evolved's logo (tools/ae_logo.py) -->\n"
    '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
    '    <background android:drawable="@mipmap/ic_launcher_background" />\n'
    '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
    '    <monochrome android:drawable="@mipmap/ic_launcher_monochrome" />\n'
    "</adaptive-icon>\n")


def outputs() -> dict:
    """every file this makes, by its path in the repository: bytes"""
    files = {}
    vectors = sources()
    for name, text in vectors.items():
        files[BRANDING / name] = text.encode()
    flat = lambda image: image.convert("RGB")  # noqa: E731 (the GitHub pictures have no transparency)
    files[BRANDING / "social-preview-1280x640.png"] = png(flat(rasterize(vectors["social-preview-1280x640.svg"], 1280, 640)))
    for name in ("readme-banner-1280x320", "readme-banner-light-1280x320"):
        files[BRANDING / f"{name}.png"] = png(flat(rasterize(vectors[f"{name}.svg"], 1280, 320)))

    tiles = {}

    def tile(size):
        if size not in tiles:
            tiles[size] = png(rasterize(vectors["app-icon-tile-small.svg" if size <= SMALL else "app-icon-tile.svg"],
                                        size, size))
        return tiles[size]
    files[WINDOW_ICON] = tile(256)
    files[WINDOWS_ICON] = ico({size: tile(size) for size in WINDOWS_SIZES})
    files[MACOS_ICON] = icns([(kind, tile(size)) for kind, size in MACOS_ENTRIES])

    for density, scale in DENSITIES.items():
        size = round(LAYER_DP * scale)
        for layer in ("foreground", "background", "monochrome"):
            image = rasterize(vectors[f"android-{layer}.svg"], size, size)
            files[RESOURCES / f"mipmap-{density}" / f"ic_launcher_{layer}.png"] = png(
                flat(image) if layer == "background" else image)
    files[RESOURCES / "mipmap-anydpi-v26" / "ic_launcher.xml"] = ADAPTIVE_ICON.encode()
    return files


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                     epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT,
                        help="write under this folder instead of the repository (same layout)")
    arguments = parser.parse_args()
    if not shutil.which("rsvg-convert"):
        sys.exit("rsvg-convert (librsvg) is needed")
    for path, data in outputs().items():
        target = arguments.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        print(f"{path}: {len(data)} bytes")


if __name__ == "__main__":
    main()
