#!/usr/bin/env python3
"""Makes Arena Evolved's desktop icons from its own artwork, the one the
Android launcher icon is made from (port/android/art/android-icon.png, the
emblem on its red background; tools/android_icon.py):

    python tools/ae_app_icon.py [--preview sheet.png]

- port/windows/ae-icon.ico: the Windows executable's icon (port/windows/halo.rc
  puts it in halo.exe), in the sizes Explorer and the taskbar ask for;
- port/assets/icon/ae-icon-256.png: the desktop builds' window icon
  (tools/embed_assets.py embeds it and sdl_platform.c gives it to the window).

Upstream's tools/app_icon.py makes the same two files from OpenCE's artwork
(port/assets/icon/opence-icon*.png, port/windows/opence-icon.ico), which this
tree keeps but does not use. --preview also writes a sheet of every size on a
light and a dark background, to judge the small ones by. Run it again when
the artwork changes. Needs Pillow.
"""

import argparse
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ARTWORK = ROOT / "port/android/art/android-icon.png"
WINDOW_ICON = ROOT / "port/assets/icon/ae-icon-256.png"
WINDOWS_ICON = ROOT / "port/windows/ae-icon.ico"
# (as tools/app_icon.py's)
WINDOWS_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def preview(icon: Image.Image, path: Path) -> None:
    """every size as it is, and the small ones again four times as large
    (their pixels kept), on a light and on a dark background"""
    gap = 16
    row_width = sum(WINDOWS_SIZES) + gap * (len(WINDOWS_SIZES) + 1)
    small = [size for size in WINDOWS_SIZES if size <= 48]
    large_width = sum(size * 4 for size in small) + gap * (len(small) + 1)
    width = max(row_width, large_width)
    band = gap + 256 + gap + 48 * 4 + gap
    sheet = Image.new("RGB", (width, band * 2), (255, 255, 255))
    for index, background in enumerate(((240, 240, 240), (32, 32, 36))):
        top = index * band
        sheet.paste(background, (0, top, width, top + band))
        x = gap
        for size in WINDOWS_SIZES:
            scaled = icon.resize((size, size), Image.LANCZOS)
            sheet.paste(scaled, (x, top + gap + 256 - size), scaled)
            x += size + gap
        x = gap
        for size in small:
            scaled = icon.resize((size, size), Image.LANCZOS).resize((size * 4, size * 4), Image.NEAREST)
            sheet.paste(scaled, (x, top + gap + 256 + gap + (48 - size) * 4), scaled)
            x += size * 4 + gap
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path, optimize=True)
    print(f"{path}: the sizes {', '.join(str(size) for size in WINDOWS_SIZES)}; below, to 48 at four times")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preview", type=Path, help="also write a sheet of every size here")
    arguments = parser.parse_args()
    icon = Image.open(ARTWORK).convert("RGBA")

    WINDOW_ICON.parent.mkdir(parents=True, exist_ok=True)
    icon.resize((256, 256), Image.LANCZOS).save(WINDOW_ICON, optimize=True)
    print(f"{WINDOW_ICON.relative_to(ROOT)}: 256x256")
    icon.save(WINDOWS_ICON, sizes=[(size, size) for size in WINDOWS_SIZES])
    print(f"{WINDOWS_ICON.relative_to(ROOT)}: {', '.join(str(size) for size in WINDOWS_SIZES)}")
    if arguments.preview:
        preview(icon, arguments.preview)


if __name__ == "__main__":
    main()
