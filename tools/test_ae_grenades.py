"""AE grenade types (port/linux/game/ae_grenades*.c): the source rules a game of grenade types keeps, read from the
sources (the pure rules are ae_grenade_catalogue_test.c's and ae_grenade_wire_rules_test.c's, in test_ae_units.py;
the game itself is the headless runs' and the TEST RUN list tools/ae_test/test_runs/grenades-emp.txt's)."""
import re
from pathlib import Path

from harness import function

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "port" / "linux" / "game"


def read(path):
    return (ROOT / path).read_text(encoding="latin-1")


def test_grenade_pack_is_added_last():
    """the grenade pack goes on after every other pack (the weapon, vehicle and bomb packs), so every other
    tag's index is the same on a machine that adds it and one that does not"""
    body = function(read("port/linux/game/ae_weapon_pack.c"), "placed_add_slots")
    grenade = body.index("placed_add_slot(GRENADE_PACK_SLOT")
    for other in ("placed_add_slot(slot, map_name", "placed_add_slot(VEHICLE_PACK_SLOT", "placed_add_slot(BOMB_PACK_SLOT"):
        assert body.index(other) < grenade, other
    assert grenade < body.rindex("ae_grenades_map_tags_added(")


def test_cache_grenade_tags_come_from_the_directory():
    """a cache game: the grenade layer runs right after the cache's one placement (the types' tags are always attached), the
    types' tags are the DIRECTORY entry's by index, and no name under ae\\grenades\\ is looked up"""
    body = function(read("port/linux/game/ae_weapon_pack.c"), "placed_add_slots")
    cache = body[body.index("if (ae_weapon_cache_pack())"):body.index("if (refusal_asked(")]
    assert cache.index("cache_attach(") < cache.index("ae_grenades_map_tags_added(") < cache.index("return;")
    assert "cache_has_grenades()" in cache
    found = function(read("port/linux/game/ae_grenades.c"), "id_tags_find")
    assert found.index("ae_weapon_cache_grenade_tag(") < found.index("pack_tag(names[index]")
    assert "return grenades.tags[id][_tag_projectile] != NONE" in found[:found.index("pack_tag(names[index]")]


def test_mask_zero_asks_for_nothing():
    """a game whose mask is 0 adds no grenade pack (ae_grenades_pack_asked) and sends no message 108 (the host's tick
    runs only with the layer on, which only a placed catalogue type turns on)"""
    source = read("port/linux/game/ae_grenades.c")
    asked = function(source, "ae_grenades_pack_asked")
    assert "ae_grenades_mask() != 0" in asked
    added = function(source, "ae_grenades_map_tags_added")
    assert added.index("if (!ae_grenades_pack_asked(scenario_tag_index))") < added.index("grenades.on = TRUE")
    assert "if (!grenades.slots.placed)" in added
    tick = function(read("port/linux/game/ae_grenades_network.c"), "ae_grenades_network_host_tick")
    assert re.search(r"if \(!ae_grenades_layer_on\(\) \|\| game_connection\(\) != _game_connection_network_server\)\s*"
                     r"return;", tick)


def test_message_108_reliable_from_the_host_only():
    source = read("port/linux/game/network_distributed.c")
    line = next(line for line in source.splitlines() if "case _distributed_message_ae_grenades: if" in line)
    assert "machine_index == NONE" in line and "!network_connection_last_read_was_unreliable()" in line


def test_stalled_vehicle_prediction_dropped_where_it_is_taken():
    body = function(read("port/linux/game/network_objects.c"), "network_objects_apply_vehicle_predictions")
    taken = body.index("objects_host_vehicle_predictions[player_index].valid = FALSE;")
    hook = body.index("if (ae_grenades_prediction_ignored(state->object_index)) continue; /* AE hook */", taken)
    assert hook - taken < 120


def test_effects_cleared_when_a_map_starts_again():
    body = function(read("source/units/units.c"), "units_initialize_for_new_map")
    assert "ae_grenades_new_map(); /* AE hook */" in body
    clear = function(read("port/linux/game/ae_grenades.c"), "ae_grenades_new_map")
    for field in ("ae_grenades_state_reset()", "grenades.throws", "grenades.last_detonated", "grenades.test_given_game",
                  "grenades.test_dropped_game", "grenades.test_me_at"):
        assert field in clear, field


def test_no_grenade_bits_in_the_start_bytes():
    """the mask lives in the weapon cache's byte 0x2B; nothing of the grenades writes or reads the START bytes' high
    bits (only the cache's own reader of what 0.7.6 test builds left there)"""
    for path in GAME.glob("ae_grenade*.[ch]"):
        text = path.read_text(encoding="latin-1")
        assert "pack_start_set_mask" not in text and "pack_start_keep" not in text, path.name
        assert not re.search(r"pack_start\[[01]\][^;\n]*(<<|>>) *5", text), path.name
    source = read("port/linux/game/ae_grenades.c")
    assert "AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET" in source
    assert "ae_weapon_selection_old_grenade_bits" in source


def test_the_override_is_the_hosts():
    """game.grenade_types: under the cache, written into the host's game options (so it travels) and never read as the
    mask (the mask is the byte); without the cache's byte, the override is the only mask, on every machine"""
    source = read("port/linux/game/ae_grenades.c")
    mask = function(source, "ae_grenades_mask_of")
    stored, unstored = mask.split("#else")
    assert "override" not in stored and "AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET" in stored
    assert "override_mask()" in unstored
    assert "ae_grenades_host_options(options); /* AE hook */" in function(
        read("source/networking/network_server_manager.c"), "network_game_server_variant_options")
    host = function(source, "ae_grenades_host_options")
    assert "AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET] = (unsigned char)override_mask()" in host


def test_join_tells_the_layer_only_with_the_pack():
    bits = function(read("port/linux/game/ae_grenades.c"), "ae_grenades_join_feature_bits")
    assert "ae_weapon_pack_grenade_pack_loaded()" in bits and "ae_weapon_pack_grenade_pack_hash_byte()" in bits
