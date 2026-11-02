"""AE's saved-game limit (port/linux/game/ae_saved_limits.h, 0.2.6, seed revision 11): every place the memory unit's
limit or a gametype list's size applies uses AE_SAVED_GAMES_MAXIMUM, the number fits what holds it (the file index's
12 bits, the list widget's word-sized allocation), and the seeds leave room for a player's profiles and own
gametypes. Reads the sources only; no game."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIMITS = ROOT / "port" / "linux" / "game" / "ae_saved_limits.h"
SAVED = ROOT / "source" / "saved games" / "saved_game_files.c"
EVENTS = ROOT / "source" / "interface" / "ui_widget_event_handler_functions.c"
MENUS = ROOT / "port" / "linux" / "game" / "menu_functions.c"
NETWORK_TEST = ROOT / "port" / "linux" / "game" / "network_test.c"
ARENA = ROOT / "source" / "saved games" / "arena_gametypes.c"

# the built-in entries the memory unit lists after the saved ones: the 26 stock gametypes (default_playlist) and the
# 2 default profiles (saved_game_files.c enumerate_default_profiles; measured: a fresh root's hdmu.map)
BUILT_IN = 26 + 2
# room the limit must leave on a seeded root: 4 profiles and 20 of the player's own gametypes
ROOM_PROFILES, ROOM_OWN = 4, 20


def define(text, name):
    m = re.search(rf"#define {name} (\d+)", text)
    assert m, name
    return int(m[1])


def maximum():
    return define(LIMITS.read_text(), "AE_SAVED_GAMES_MAXIMUM")


def seeds():
    text = ARENA.read_text()
    table = text[text.index("arena_gametypes[] ="):text.index("arena_gametype_migrations[] =")]
    return re.findall(r'^\t\{ "([^"]+)"', table, re.M)


def test_the_limit():
    text = LIMITS.read_text()
    assert define(text, "AE_SAVED_GAMES_OLD_MAXIMUM") == 100
    assert maximum() == 200


def test_memory_unit_uses_it():
    """the mapfile's entries, the new-file check, the start-up folder count and the index asserts are one enum"""
    text = SAVED.read_text()
    assert '#include "ae_saved_limits.h" /* AE hook */' in text
    assert re.search(r"MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT = AE_SAVED_GAMES_MAXIMUM /\* AE hook \*/",
                     text)
    # (no other number bounds the memory unit's list)
    assert not re.search(r"MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT = \d", text)
    for use in ("number_of_saved_games >= MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT",
                "number_of_entries < MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT",
                "next_enumerated_profile_index < MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT"):
        assert use in text, use


def test_file_index_holds_it():
    """a saved game's index packs its place in the mapfile in 12 bits (SAVED_GAME_FILE_INDEX_BUILD), its entry's
    index field is a short"""
    text = SAVED.read_text()
    assert "#define SAVED_GAME_FILE_INDEX_FILE_INDEX(profile_index) (((profile_index)>>16)&0xFFF)" in text
    assert "(((((n)&0xFFF)<<8)" in text
    assert maximum() <= 0xFFF
    assert maximum() <= 0x7FFF


def test_xbox_menus_list_holds_it():
    """the multiplayer settings list (Xbox menus): its list items are a word-sized allocation of longs and it asks
    for as many gametypes as the memory unit lists"""
    text = EVENTS.read_text()
    assert ("ui_widget_realloc(widget->parameters.list.list_items, (word)(AE_SAVED_GAMES_MAXIMUM * sizeof(long)), "
            "/* AE hook */") in text
    assert "profile_count = AE_SAVED_GAMES_MAXIMUM; /* AE hook */" in text
    assert "profile_count = 128;" not in text
    assert maximum() * 8 <= 0xFFFF  # (even with an 8-byte long: still a word)
    assert '#include "ae_saved_limits.h"' in (ROOT / "port" / "linux" / "game" / "ae_hooks.h").read_text()


def test_pc_menus_and_test_launcher_hold_it():
    text = MENUS.read_text()
    assert "#define MAXIMUM_GAMETYPES AE_SAVED_GAMES_MAXIMUM /* AE hook */" in text
    # (the AE menus' gametype lists: no list of 128 (revision 26's 115 seeds and the 26 built-ins are 141))
    game = ROOT / "port" / "linux" / "game"
    for name in ("ae_glue_setup_options.c", "ae_screen_create_game.c", "ae_screen_main_menu.c"):
        source = (game / name).read_text()
        assert "ui_widget_port_gametypes(" in source or "ae_setup_gametypes(" in source, name
        assert not re.search(r"(indices|gametypes)\[128\]|GAMETYPES_MAXIMUM = 128|gametypes, 128,", source), name
    assert "GAMETYPES_MAXIMUM = AE_SAVED_GAMES_MAXIMUM," in (game / "ae_screen_create_game.c").read_text()
    assert not re.search(r"#define MAXIMUM_GAMETYPES \d", text)
    text = NETWORK_TEST.read_text()
    assert "long indices[AE_SAVED_GAMES_MAXIMUM]; /* AE hook */" in text
    assert "long indices[128];" not in text


def test_arena_gametypes_hold_it_and_warn():
    text = ARENA.read_text()
    assert "ARENA_GAMETYPES_MAXIMUM_SAVED = AE_SAVED_GAMES_MAXIMUM," in text
    assert "ARENA_GAMETYPES_SAVED_WARNING = AE_SAVED_GAMES_MAXIMUM - 10," in text
    # (the warning counts every entry: the built-in ones too, TRUE)
    check = text[text.index("static void arena_gametypes_self_check(\n\tvoid)\n{"):]
    check = check[:check.index("\n}\n")]
    assert "_saved_game_file_type_game_variant, &gametype_count, saved, TRUE);" in check
    assert "_saved_game_file_type_player_profile, &profile_count, saved, TRUE);" in check


def test_seeds_leave_room():
    """a seeded root (the seeds, the built-in entries, a few profiles and own gametypes) stays under the warning, and
    every gametype the memory unit can list fits the lists"""
    count = len(seeds())
    assert count == 115
    assert count + BUILT_IN + ROOM_PROFILES + ROOM_OWN <= maximum() - 10
    # (one profile: the start's own state is under the warning)
    assert count + BUILT_IN + 1 < maximum() - 10
    # (before revision 11's limit it would not have fit: the reason for the raise)
    assert count + BUILT_IN > 100


def test_record_fits():
    """the record of the seeded names (each "NAME\\n" after "#revision N\\n") fits its buffer. A fresh root's stayed
    under the length builds before the migrations read (ARENA_GAMETYPES_RECORD_OLD_LIMIT) until revision 17's four
    seeds took it past (arena_gametypes_seed logs a warning: such a build, 0.1.0-beta's, would seed its own gametypes
    again, never over a file). Revision 26's 115 seeds: about 1.3 KB of the 2 KB, every old name too, with room for
    another 40 or so"""
    text = ARENA.read_text()
    size = int(re.search(r"ARENA_GAMETYPES_RECORD_SIZE = (\d+)", text)[1])
    table = text[text.index("arena_gametype_migrations[] ="):text.index("arena_gametype_test_migration =")]
    old_names = set(re.findall(r'\.old_row = \{\s*\.name = "([^"]+)"', table)) - set(seeds())
    fresh = len("#revision 11\n") + sum(len(n) + 1 for n in seeds())
    assert fresh + sum(len(n) + 1 for n in old_names) < size
    # (and room left: 40 more 11-character seeds)
    assert fresh + sum(len(n) + 1 for n in old_names) + 40 * 12 < size
