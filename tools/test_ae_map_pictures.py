"""A map's own picture (port/linux/game/ae_map_pictures.c) is the PC menus' alone (owner, 0.3.0): the Xbox menus are
upstream's and keep their pictures. Only the PC menus' code (menu_functions.c) asks for one, and the picture is
refused unless display.menus is "pc"."""
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


def test_only_the_pc_menus_ask_for_a_map_picture():
    assert callers("ae_map_picture_frame") == {"port/linux/game/menu_functions.c"}


def test_the_picture_is_refused_outside_the_pc_menus():
    text = (GAME / "ae_map_pictures.c").read_text()
    body = text[text.index("short ae_map_picture_frame("):]
    body = body[:body.index("\n}\n")]
    guard = body.index('config_string("display.menus")')
    assert re.search(r'_stricmp\(menus, "pc"\)\)\s*return NONE;', body)
    # (the guard comes before any file is looked for)
    assert guard < body.index("picture_entry(")


def test_the_bitmap_and_png_are_only_this_list_s_and_the_art_s():
    assert callers("ae_map_picture_bitmap") == {"port/linux/game/ui_map_list.c"}
    assert callers("ae_map_picture_png") == {"port/linux/src/menu_files.c"}
