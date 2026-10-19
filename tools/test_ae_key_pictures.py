"""The HUD's key prompts' pictures (tools/ae_key_pictures.py): the sheets, keys.json and ae_key_sheet.inc are what
the generator draws, every cell lies inside its sheet, the sheets are embedded as AE's own pictures, and no amber
anywhere (the OG theme's standard). The keys' rules themselves: port/linux/tests/ae_key_prompts_rules_test.c."""
import colorsys
import io
import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import ae_button_glyphs  # noqa: E402


def test_sheets_listed_as_own_pictures():
    own = [asset for asset in ae_button_glyphs.assets() if asset.get("own")]
    assert sorted(asset["tag"] for asset in own) == ["ae\\key_prompts\\ae", "ae\\key_prompts\\ce"]
    for asset in own:
        png = ROOT / asset["folder"] / f"{asset['name']}.png"
        assert png.is_file()
        assert asset["folder"] / f"{asset['name']}.png" in ae_button_glyphs.inputs()


def test_cells_inside_their_sheets():
    listing = {asset["name"]: asset for asset in json.loads(
        (ROOT / "port/assets/buttons/keys/keys.json").read_text())["assets"]}
    text = (ROOT / "port/linux/game/ae_key_sheet.inc").read_text()
    for style in ("ae", "ce"):
        width = int(text.split(f"#define AE_KEY_SHEET_{style.upper()}_WIDTH ")[1].split()[0])
        height = int(text.split(f"#define AE_KEY_SHEET_{style.upper()}_HEIGHT ")[1].split()[0])
        assert (listing[f"ae_keys_{style}"]["width"], listing[f"ae_keys_{style}"]["height"]) == (width, height)
    for line in text.splitlines():
        if not line.startswith("\t{ \"") or "{ {" not in line:
            continue
        rects = line.split("{ {")[1].split("} }")[0]
        for index, rect in enumerate(rects.split("}, {")):
            x, y, w, h = (int(v) for v in rect.replace("{", "").replace("}", "").split(","))
            sheet = ("ae", "ce")[index]
            assert x >= 0 and y >= 0 and w > 0 and h > 0
            assert x + w <= listing[f"ae_keys_{sheet}"]["width"] and y + h <= listing[f"ae_keys_{sheet}"]["height"]


def test_files_are_what_the_generator_draws():
    pytest.importorskip("PIL.Image")
    pytest.importorskip("scipy")
    pytest.importorskip("fontTools")
    import numpy as np
    from PIL import Image
    import ae_key_pictures

    for path, data in ae_key_pictures.outputs().items():
        if path.suffix == ".png":
            drawn = np.asarray(Image.open(io.BytesIO(data)).convert("RGBA")).astype(int)
            committed = np.asarray(Image.open(path).convert("RGBA")).astype(int)
            assert drawn.shape == committed.shape, path.name
            # (another Pillow or SciPy may round a texel differently)
            assert np.abs(drawn - committed).max() <= 2, path.name
        else:
            assert path.read_bytes() == data, path.name


def test_no_amber():
    pytest.importorskip("PIL.Image")
    from PIL import Image

    for png in sorted((ROOT / "port/assets/buttons/keys").glob("*.png")):
        image = Image.open(png).convert("RGBA")
        image = image.resize((image.width // 8, image.height // 8))
        for r, g, b, a in image.getdata():
            if a < 128:
                continue
            hue, saturation, value = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            assert not (25 / 360 <= hue <= 65 / 360 and saturation > 0.5 and value > 0.5), (png.name, (r, g, b))
