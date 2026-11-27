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


def test_pad_glyphs_of_the_rules_are_drawn():
    """every glyph name ae_key_prompts_rules.c gives a pad button has a cell in both sheets (and the cells are the generator's)"""
    import re
    import ae_key_pictures

    rules = (ROOT / "port/linux/game/ae_key_prompts_rules.c").read_text()
    named = set(re.findall(r'"((?:xbox|playstation|playstation4|playstation5|wiiu)_[a-z0-9_]+)"', rules))
    assert named
    assert named <= set(ae_key_pictures.PAD_GLYPHS), sorted(named - set(ae_key_pictures.PAD_GLYPHS))
    assert set(ae_key_pictures.PAD_GLYPHS) <= named, sorted(set(ae_key_pictures.PAD_GLYPHS) - named)
    text = (ROOT / "port/linux/game/ae_key_sheet.inc").read_text()
    for name in named:
        assert f'{{ "{name}", ' in text, name


def test_ae_sheet_is_white_line_art():
    """HUD STYLE AE's sheet (the keys, the plain keycap and the pad glyphs): opaque texels are the one white or black, no navy fill,
    no colour (the CE sheet keeps its blue keycaps)"""
    pytest.importorskip("PIL.Image")
    import numpy as np
    from PIL import Image

    image = np.asarray(Image.open(ROOT / "port/assets/buttons/keys/ae_keys_ae.png").convert("RGBA")).astype(int)
    solid = image[image[..., 3] > 200]
    assert len(solid)
    assert (solid[:, :3].max(axis=1) - solid[:, :3].min(axis=1)).max() <= 16
    # (the white is #dde3e8, as AE_COLOR_PROMPT_WHITE; the rest black)
    light = image[(image[..., 3] > 250) & (image[..., :3].max(axis=2) > 200)]
    assert len(light) and abs(light[:, 0].mean() - 0xDD) < 3 and abs(light[:, 2].mean() - 0xE8) < 3


def test_pad_glyphs_are_white_line_art():
    """the pad cells hold white and black only (HUD STYLE AE's chips; the menus' glyphs are that white too)"""
    pytest.importorskip("PIL.Image")
    import numpy as np
    from PIL import Image

    text = (ROOT / "port/linux/game/ae_key_sheet.inc").read_text()
    for style, sheet in (("ae", "ae_keys_ae.png"), ("ce", "ae_keys_ce.png")):
        image = np.asarray(Image.open(ROOT / "port/assets/buttons/keys" / sheet).convert("RGBA")).astype(int)
        for line in text.splitlines():
            if not line.startswith(('\t{ "xbox', '\t{ "playstation', '\t{ "wiiu')):
                continue
            rects = line.split("{ {")[1].split("} }")[0].split("}, {")
            x, y, w, h = (int(v) for v in rects[0 if style == "ae" else 1].replace("{", "").replace("}", "").split(","))
            cell = image[y:y + h, x:x + w]
            solid = cell[cell[..., 3] > 200]
            assert len(solid), line
            # (opaque texels are white or black: no colour, whatever the family)
            assert (solid[:, :3].max(axis=1) - solid[:, :3].min(axis=1)).max() <= 16, line
