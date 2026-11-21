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


def test_a_workshop_picture_reads_the_mcc_maps_when_the_list_was_never_built():
    # the social lobby asks for a @ws map's picture before CHANGE MAP's list fills glue.mcc (Digsite tiles had none)
    text = (GAME / "ae_glue_setup_maps.c").read_text()
    body = text[text.index("static int picture_load("):]
    body = body[:body.index("\n}\n")]
    assert "mcc_map_for_picture(list_name)" in body and "mcc_map_of(list_name)" not in body
    helper = text[text.index("mcc_map_for_picture(char const"):]
    helper = helper[:helper.index("\n}\n")]
    assert '"@ws"' in helper and "ae_mcc_menu_maps(glue.mcc" in helper


def test_every_picture_none_path_says_why():
    text = (GAME / "ae_glue_setup_maps.c").read_text()
    for name in ("static int workshop_picture_bytes(", "static int map_picture_bytes("):
        body = text[text.index(name):]
        body = body[:body.index("\n}\n")]
        assert len(re.findall(r"return 0;", body)) == len(re.findall(r"picture '%s' none", body)), name
