"""A map's own picture (port/linux/game/ae_map_pictures.c), in both menu styles (owner, 2026-10-16: the map pack's
pictures show in the Xbox menus too). The PC menus' code (menu_functions.c) and the Xbox menus' map list
(ui_map_list.c's picture index, which their map select and lobby read) ask for one; no menu style refuses it."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "port" / "linux" / "game"


def callers(name):
    found = set()
    for top in ("source", "port", "server"):
        for path in (ROOT / top).rglob("*.c"):
            if path.name == "ae_map_pictures.c" or "tests" in path.relative_to(ROOT).parts:
                continue
            if re.search(rf"\b{name}\s*\(", path.read_text(errors="replace")):
                found.add(path.relative_to(ROOT).as_posix())
    return found


def test_the_pc_and_xbox_menus_ask_for_a_map_picture():
    assert callers("ae_map_picture_frame") == {"port/linux/game/menu_functions.c", "port/linux/game/ui_map_list.c"}


def test_no_menu_style_refuses_the_picture():
    text = (GAME / "ae_map_pictures.c").read_text()
    body = text[text.index("short ae_map_picture_frame("):]
    body = body[:body.index("\n}\n")]
    assert 'config_string("display.menus")' not in body


def test_the_bitmap_and_png_are_only_this_list_s_and_the_art_s():
    assert callers("ae_map_picture_bitmap") == {"port/linux/game/ui_map_list.c"}
    assert callers("ae_map_picture_png") == {"port/linux/src/menu_files.c"}
