"""The stock maps' pictures reach AE's own screens (CHANGE MAP, the AE TOURNAMENT lobby, the server lobby).

They are not files: the loaded ui.map holds them (the Xbox maps' mp_map_grafix, Halo PC's pictures of its own maps), so a
screen that reads only previews/<file>.png shows none for chillout, damnation or hangemhigh. That went missing more than once;
this keeps the one path that has them (ae_map_stock_picture.c, asked by ae_glue_setup_maps.c's picture_load) in place, and
every screen that shows a map's picture behind that one path."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "port" / "linux" / "game"

# the 13 Xbox maps and Halo PC's six more: all 19 stock multiplayer maps
STOCK = ["beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout", "carousel",
         "boardingaction", "bloodgulch", "wizard", "putput", "longest", "icefields", "deathisland", "dangercanyon",
         "infinity", "timberland", "gephyrophobia"]


def body(text, start):
    at = text.index(start)
    return text[at:text.index("\n}\n", at)]


def test_every_stock_map_has_a_picture_in_the_menus_list():
    text = (GAME / "ui_map_list.c").read_text()
    xbox = set(re.findall(r'"(\w+)"', body(text, "static char const *const xbox_map_files[]")))
    pc = set(re.findall(r'\{ "(\w+)", L"', text))
    for name in STOCK:
        assert name in xbox or name in pc, f"{name}: no picture in ui_map_list.c's tables"


def test_picture_load_asks_the_loaded_ui_map_after_the_files():
    text = (GAME / "ae_glue_setup_maps.c").read_text()
    load = body(text, "static int picture_load(")
    assert "ae_map_stock_picture_rgba(" in load
    assert load.index("map_picture_bytes(") < load.index("ae_map_stock_picture_rgba(")
    assert "ae_map_stock_picture_rgba" not in text.replace(load, "")


def test_the_stock_picture_reads_both_pictures_kinds_and_not_the_unknown_level():
    text = (GAME / "ae_map_stock_picture.c").read_text()
    assert "mp_map_grafix" in text and "ui_map_list_picture(" in text and "ui_map_list_picture_index(" in text
    assert "ae_map_auto_picture_stock_frame(" in text and "&own" in text


def test_every_screen_with_a_map_picture_goes_through_the_one_path():
    screens = set()
    for path in GAME.glob("ae_*.c"):
        text = path.read_text(errors="replace")
        if re.search(r"\bae_setup_map_picture(_named)?\s*\(", text) and path.name != "ae_glue_setup_maps.c":
            screens.add(path.name)
    assert screens == {"ae_screen_change_map.c", "ae_screen_server_lobby.c", "ae_screen_social_lobby.c"}
    # nothing else loads a map's picture file for a screen on its own
    for path in GAME.glob("ae_screen_*.c"):
        text = path.read_text(errors="replace")
        assert "previews" not in text, f"{path.name} reads a previews folder itself"


def test_an_mcc_stock_row_and_halo_pcs_other_maps_get_a_picture():
    """"<file>@mcc" rows (all 19 MCC stock maps) and Halo PC's six BTB maps (no Xbox picture) used to fall out as a silent
    none; the stock path takes the tag off and reads Halo PC's own picture for those."""
    text = (GAME / "ae_map_stock_picture.c").read_text()
    code = body(text, "int ae_map_stock_picture_rgba(")
    assert "ae_map_stock_picture_file(" in code and "pc_picture(" in code
    rules = (GAME / "ae_map_auto_picture_rules.c").read_text()
    assert '"@mcc"' in rules or "'@'" in rules
    assert "ae_map_stock_picture_rgba" in body((GAME / "ae_glue_setup_maps.c").read_text(), "static int picture_load(")
