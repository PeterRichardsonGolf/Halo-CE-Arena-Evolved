"""Arena Evolved's own pictures for the PC menus' placeholder frames (tools/ae_menu_pictures.py): the hand-edited
bitmaps.xml, menus.json and the drawn files agree with the generator data, as tools/ce_menus.py would write them."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port" / "assets" / "menus"
sys.path.insert(0, str(ROOT / "tools"))
import ae_menu_pictures  # noqa: E402


def frames_of(bitmap):
    text = (MENUS / "ce" / "bitmaps.xml").read_text()
    block = re.search(rf'\t<bitmap name="{re.escape(bitmap)}">\n(.*?)\t</bitmap>', text, re.S)
    assert block, bitmap
    return re.findall(r"<frame ([^>]*)/>", block.group(1))


def test_the_own_frames_are_in_bitmaps_xml_where_the_generator_puts_them():
    for bitmap, own in ae_menu_pictures.own_frames().items():
        frames = frames_of(bitmap)
        assert len(frames) > max(own)
        for number, (_, width, height) in own.items():
            _, png = ae_menu_pictures.frame_paths(bitmap, number)
            assert frames[number] == f'png="{png}" width="{width}" height="{height}"', (bitmap, number)


def test_every_entry_of_the_multiplayer_screen_and_nhe_options_have_their_own():
    # (AE: SPLIT SCREEN's, before CO-OP's: 8)
    assert len(frames_of(ae_menu_pictures.MULTIPLAYER)) == 8
    assert sorted(ae_menu_pictures.own_frames()[ae_menu_pictures.MULTIPLAYER]) == list(range(8))
    # (CHANGE NAME 0 ... NHE OPTIONS 8: GAME, PLAYER and INDICATOR OPTIONS keep theirs)
    gametype = frames_of(ae_menu_pictures.GAMETYPE)
    assert len(gametype) == 9
    assert sorted(ae_menu_pictures.own_frames()[ae_menu_pictures.GAMETYPE]) == [0, 3, 4, 6, 7, 8]
    assert "gametype_options__1.png" in gametype[1] and 'index="2"' in gametype[2] and 'index="4"' in gametype[5]


def test_the_svgs_are_the_generator_s_and_the_pngs_are_embedded():
    files = set(json.loads((MENUS / "menus.json").read_text())["files"])
    for bitmap, own in ae_menu_pictures.own_frames().items():
        for number, (text, _, _) in own.items():
            svg, png = ae_menu_pictures.frame_paths(bitmap, number)
            assert (MENUS / svg).read_text() == text, svg
            assert (MENUS / png).is_file() and png in files, png
            # (8-bit RGBA, 4 times the frame: Art.own_frame's SCALE)
            data = (MENUS / png).read_bytes()
            assert data[:8] == b"\x89PNG\r\n\x1a\n" and data[24:26] == b"\x08\x06"
            assert int.from_bytes(data[16:20], "big") == 2048 and int.from_bytes(data[20:24], "big") == 1024


def test_no_placeholder_is_left_on_those_screens():
    files = json.loads((MENUS / "menus.json").read_text())["files"]
    for bitmap in (ae_menu_pictures.MULTIPLAYER, ae_menu_pictures.GAMETYPE):
        for frame in frames_of(bitmap):
            png = re.search(r'png="([^"]*)"', frame)
            if png:
                assert png.group(1) in files and (MENUS / png.group(1)).is_file()
    report = (MENUS / "NON_HANDDRAWN.md").read_text()
    assert "mp_options__" not in report.split("## Placeholders")[1]
    assert "gametype_options__" not in report.split("## Placeholders")[1]


def test_ink_is_each_drawing_s_measured_ink():
    # (the frames place and size each drawing by its ink: a drawing changed without its INK row would sit wrong)
    import shutil
    import pytest
    if not shutil.which("rsvg-convert"):
        pytest.skip("needs rsvg-convert")
    pytest.importorskip("PIL")
    drawings = dict(ae_menu_pictures.MULTIPLAYER_ICONS) | dict(ae_menu_pictures.GAMETYPE_ICONS.values())
    assert set(drawings) == set(ae_menu_pictures.INK)
    for name, shapes in drawings.items():
        measured = ae_menu_pictures.ink(shapes)
        assert all(abs(a - b) <= 0.25 for a, b in zip(measured, ae_menu_pictures.INK[name])), (name, measured)



def test_every_drawing_is_centred_in_its_zone_and_no_bigger_than_it():
    # (owner 10-15: placement, sizing and padding alike on every entry: the ink's middle on the zone's, so the space
    # above it is the space between it and the description's text)
    for bitmap, names in ((ae_menu_pictures.MULTIPLAYER, [n for n, _ in ae_menu_pictures.MULTIPLAYER_ICONS]),
                          (ae_menu_pictures.GAMETYPE, [n for n, _ in ae_menu_pictures.GAMETYPE_ICONS.values()])):
        zone = ae_menu_pictures.ZONES[bitmap]
        for name in names:
            left, top, right, bottom = ae_menu_pictures.INK[name]
            text = ae_menu_pictures.svg([], (left, top, right, bottom), zone)
            x, y, scale = map(float, re.search(r"translate\(([-0-9.]+) ([-0-9.]+)\) scale\(([0-9.]+)\)", text).groups())
            assert abs(x + scale * (left + right) / 2 - zone["centre"][0]) < 0.02, name
            assert abs(y + scale * (top + bottom) / 2 - zone["centre"][1]) < 0.02, name
            assert scale * (bottom - top) <= zone["height"] + 0.01 and scale * (right - left) <= zone["width"] + 0.01
            assert scale * ((bottom - top) * (right - left)) ** 0.5 <= zone["mass"] + 0.01, name


def test_the_lobby_band_is_the_option_band_cut_to_the_lobby_width():
    import pytest
    Image = pytest.importorskip("PIL.Image")
    for frame in (0, 1):
        band = Image.open(MENUS / f"ce/port/{ae_menu_pictures.LOBBY_BAND}__{frame}.png")
        source = Image.open(MENUS / f"ce/shell/bitmaps/option_bkds__{frame}.png")
        assert band.size == (ae_menu_pictures.LOBBY_BAND_WIDTH * 4, source.height)
        # (its ends are the game's own)
        assert band.crop((0, 0, 96, band.height)).tobytes() == source.convert("RGBA").crop((0, 0, 96, band.height)).tobytes()
