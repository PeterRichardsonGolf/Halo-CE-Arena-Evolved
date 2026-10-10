"""Arena Evolved's logo files (tools/ae_logo.py): the committed ones are what the generator makes now, and each is the
shape its user needs.

- The vector sources (port/assets/branding/*.svg) and the Android icon's XML: byte for byte.
- The pictures (PNG, and the PNGs inside the .ico and .icns): the same size and pixels, within a tolerance for another
  librsvg's anti-aliasing: every channel within 2 of the fresh export, except at most 0.5 % of the pixels (edges),
  which stay within 48.
- halo.exe's icon has every size from 16 to 256; the small master is used up to 32 px; the macOS icon has its 11
  entries; the Android layers have each density's size (108 dp) and the adaptive icon names all three layers.
- Every picture of the logo is listed in port/assets/branding/LICENSE.md (the logo is not CC0), and LICENSE.md
  points there.

Needs Pillow, fontTools and rsvg-convert (librsvg), as the generator does: skipped without them."""
import io
import math
import shutil
import struct
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

pytest.importorskip("PIL", reason="needs Pillow, as tools/ae_logo.py does")
pytest.importorskip("fontTools", reason="needs fontTools, as tools/ae_logo.py does")
if not shutil.which("rsvg-convert"):
    pytest.skip("needs rsvg-convert (librsvg), as tools/ae_logo.py does", allow_module_level=True)

from PIL import Image, ImageChops  # noqa: E402

from tools import ae_logo  # noqa: E402

CLOSE, EDGE_SHARE, EDGE_LIMIT = 2, 0.005, 48


@pytest.fixture(scope="module")
def fresh():
    return ae_logo.outputs()


def ico_images(data: bytes) -> dict:
    reserved, kind, count = struct.unpack("<HHH", data[:6])
    assert (reserved, kind) == (0, 1)
    images = {}
    for index in range(count):
        width, height, _, _, planes, bits, size, offset = struct.unpack("<BBBBHHII", data[6 + 16 * index:22 + 16 * index])
        image = Image.open(io.BytesIO(data[offset:offset + size]))
        assert image.size == (width or 256, height or 256) and planes == 1 and bits == 32
        images[image.size[0]] = image
    return images


def icns_images(data: bytes) -> list:
    assert data[:4] == b"icns" and struct.unpack(">I", data[4:8])[0] == len(data)
    entries, position = [], 8
    while position < len(data):
        kind, length = data[position:position + 4].decode(), struct.unpack(">I", data[position + 4:position + 8])[0]
        entries.append((kind, Image.open(io.BytesIO(data[position + 8:position + length]))))
        position += length
    return entries


def assert_close(committed: Image.Image, made: Image.Image, what: str) -> None:
    assert committed.size == made.size, f"{what}: {committed.size}, a fresh export is {made.size}"
    mode = "RGBA" if "A" in committed.mode or "A" in made.mode else "RGB"
    difference = ImageChops.difference(committed.convert(mode), made.convert(mode))
    largest = max(band.getextrema()[1] for band in difference.split())
    channels = len(mode)
    data = difference.tobytes()
    off = sum(1 for index in range(0, len(data), channels) if max(data[index:index + channels]) > CLOSE) \
        if largest > CLOSE else 0
    share = off / (made.size[0] * made.size[1])
    assert largest <= EDGE_LIMIT and share <= EDGE_SHARE, \
        f"{what}: {off} pixels ({share:.2%}) off by more than {CLOSE}, at most {largest}: run tools/ae_logo.py"


def test_every_file_is_committed_and_current(fresh):
    for path, data in fresh.items():
        committed = ROOT / path
        assert committed.is_file(), f"{path}: not there: run tools/ae_logo.py"
        old = committed.read_bytes()
        if path.suffix in (".svg", ".xml"):
            assert old == data, f"{path}: not what tools/ae_logo.py makes: run it"
        elif path.suffix == ".png":
            assert_close(Image.open(io.BytesIO(old)), Image.open(io.BytesIO(data)), str(path))
        elif path.suffix == ".ico":
            old_images, new_images = ico_images(old), ico_images(data)
            assert sorted(old_images) == sorted(new_images)
            for size in new_images:
                assert_close(old_images[size], new_images[size], f"{path} {size}x{size}")
        elif path.suffix == ".icns":
            old_entries, new_entries = icns_images(old), icns_images(data)
            assert [kind for kind, _ in old_entries] == [kind for kind, _ in new_entries]
            for (kind, old_image), (_, new_image) in zip(old_entries, new_entries):
                assert_close(old_image, new_image, f"{path} {kind}")
        else:
            pytest.fail(f"{path}: no check for this kind of file")


def test_branding_folder_has_nothing_stale(fresh):
    made = {path.name for path in fresh if path.parent == ae_logo.BRANDING}
    there = {path.name for path in (ROOT / ae_logo.BRANDING).iterdir()}
    assert there == made | {"LICENSE.md"}


def test_every_logo_file_is_excluded_from_cc0(fresh):
    """port/assets/branding/LICENSE.md lists every picture of the logo (the Android icon's XML is code)"""
    terms = (ROOT / ae_logo.BRANDING / "LICENSE.md").read_text()
    assert "LICENSE.md" in (ROOT / "LICENSE.md").read_text().split("Creative Commons Legal Code")[0]
    for path in fresh:
        if path.suffix == ".xml":
            continue
        if path.parent == ae_logo.BRANDING:
            assert f"`{path.name}`" in terms, f"{path}: not in {ae_logo.BRANDING}/LICENSE.md"
        elif ae_logo.RESOURCES in path.parents:
            assert f"`{path.name}`" in terms and f"{path.parent.name}/" in terms, f"{path}: not in LICENSE.md"
        else:
            assert f"`{path.as_posix()}`" in terms, f"{path}: not in {ae_logo.BRANDING}/LICENSE.md"
    assert "`arena-evolved.png`" in terms


def test_windows_icon_sizes():
    images = ico_images((ROOT / ae_logo.WINDOWS_ICON).read_bytes())
    assert sorted(images) == [16, 20, 24, 32, 40, 48, 64, 128, 256]
    # the small master up to 32 px: its ring is drawn larger on the tile (86 %, not 74 %), so it reaches nearer the
    # tile's edge: compare where the tile's first non-black pixel is in each, scaled to 64
    small = ae_logo.rasterize(ae_logo.mark_svg(*ae_logo.DARK, small=True, tile=True), 32, 32)
    large = ae_logo.rasterize(ae_logo.mark_svg(*ae_logo.DARK, tile=True), 32, 32)
    assert_close(images[32], small, "the 32 px icon (the small master)")
    assert ImageChops.difference(images[32].convert("RGBA"), large).getbbox() is not None
    assert_close(images[40], ae_logo.rasterize(ae_logo.mark_svg(*ae_logo.DARK, tile=True), 40, 40), "the 40 px icon")


def test_window_icon():
    image = Image.open(ROOT / ae_logo.WINDOW_ICON)
    assert image.format == "PNG" and image.size == (256, 256) and image.mode == "RGBA"
    # transparent outside the tile, the tile's black at its middle's edge
    assert image.getpixel((0, 0))[3] == 0 and image.getpixel((128, 12))[:3] == (0x0A, 0x0A, 0x0B)


def test_macos_icon():
    entries = icns_images((ROOT / ae_logo.MACOS_ICON).read_bytes())
    assert [(kind, image.size[0]) for kind, image in entries] == ae_logo.MACOS_ENTRIES
    assert all(image.format == "PNG" and image.size[0] == image.size[1] for _, image in entries)


def test_android_icon():
    resources = ROOT / ae_logo.RESOURCES
    for density, scale in ae_logo.DENSITIES.items():
        for layer in ("foreground", "background", "monochrome"):
            image = Image.open(resources / f"mipmap-{density}" / f"ic_launcher_{layer}.png")
            side = round(108 * scale)
            assert image.size == (side, side), f"{density} {layer}"
            if layer == "background":
                assert image.mode == "RGB"
            else:
                # the mark inside the 66 dp safe zone: nothing drawn further than 33 dp from the middle
                alpha = image.getchannel("A")
                middle = side / 2
                for y in range(side):
                    for x in range(side):
                        if alpha.getpixel((x, y)) > 8:
                            # (a pixel's middle: up to half its diagonal beyond what it covers)
                            assert math.hypot(x + 0.5 - middle, y + 0.5 - middle) <= 33 * scale + 0.71, \
                                f"{density} {layer}: ({x}, {y}) outside the safe zone"
            if layer == "monochrome":
                # one colour: only its opacity is read
                data = image.convert("RGBA").tobytes()
                assert {data[index:index + 3] for index in range(0, len(data), 4) if data[index + 3] > 128} == \
                    {b"\xff\xff\xff"}
    adaptive = ElementTree.parse(resources / "mipmap-anydpi-v26" / "ic_launcher.xml").getroot()
    android = "{http://schemas.android.com/apk/res/android}"
    assert adaptive.tag == "adaptive-icon"
    assert {child.tag: child.get(f"{android}drawable") for child in adaptive} == {
        "background": "@mipmap/ic_launcher_background", "foreground": "@mipmap/ic_launcher_foreground",
        "monochrome": "@mipmap/ic_launcher_monochrome"}
    manifest = (ROOT / "port/android/app/src/main/AndroidManifest.xml").read_text()
    assert 'android:icon="@mipmap/ic_launcher"' in manifest
    # no layer left over from the old icon (its background was a colour)
    assert not (resources / "values" / "ic_launcher_background.xml").exists()


def test_github_pictures():
    for name, size in (("social-preview-1280x640.png", (1280, 640)), ("readme-banner-1280x320.png", (1280, 320)),
                       ("readme-banner-light-1280x320.png", (1280, 320))):
        path = ROOT / ae_logo.BRANDING / name
        image = Image.open(path)
        assert image.size == size and image.mode == "RGB"
        # GitHub's limit for a social preview is 1 MB
        assert path.stat().st_size < 1_000_000
    readme = (ROOT / "README.md").read_text()
    assert "port/assets/branding/readme-banner-1280x320.png" in readme
    assert "port/assets/branding/readme-banner-light-1280x320.png" in readme


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-q"]))
