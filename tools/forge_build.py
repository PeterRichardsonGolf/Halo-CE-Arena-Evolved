"""Content Evolved's Forge (port/forge: the 3D Monitor editor, its runtime
tags, its map cache builder and its network sync) in the builds that take it.

Forge takes over some of the game's functions: each one's definition and the
calls to it in its own source are renamed (orig_..., or Forge's own
replacements of the C library's file calls), and Forge's definition of the
name takes its place. Every build renames them in the source as it is
compiled, with -D (forge_source_flags). A build without Forge compiles none
of this and OPENCE_ENABLE_FORGE (port/linux/include/opence_features.h) is 0
there: the game's own functions keep their names, no hook of the engine's
calls Forge, and the units that ask about it are told there is no session.

Builds with Forge: ``ninja linux64`` (FORGE_HOSTS). The other builds leave it
out until each has been built and run with it.
"""

from pathlib import Path
from typing import Dict, List, Tuple

FORGE_DIR = Path("port/forge")
FORGE_NET_OVERRIDE = FORGE_DIR / "include" / "forge_net_override.h"

# the 64-bit hosts (tools/lp64_build.py Lp64Host.name) built with Forge
FORGE_HOSTS = ("linux64",)

# Forge's data files, beside the game (config.toml's folder): its marker meshes, and the props it and Firefight place
FORGE_DATA_FILES = (Path("port/assets/forge/forge_markers.bin"), Path("port/assets/props/tag_props.bin"))

# the renames: a source's function (its definition, its calls and its
# declarations in that source) and the name it takes
FORGE_RENAMES: Dict[str, List[Tuple[str, str]]] = {
    "port/linux/game/menu_functions.c": [
        ("pc_menu_event_function_invoke", "orig_pc_menu_event_function_invoke"),
        ("pc_menu_game_data_function_invoke", "orig_pc_menu_game_data_function_invoke"),
        ("player_profile_get", "forge_player_profile_get"),
    ],
    "port/linux/game/network_chat.c": [
        ("network_chat_update", "orig_network_chat_update"),
        ("network_chat_draw", "orig_network_chat_draw"),
        ("network_chat_typing", "orig_network_chat_typing"),
        ("network_chat_handle_up", "orig_network_chat_handle_up"),
        ("network_chat_handle_down", "orig_network_chat_handle_down"),
    ],
    "port/linux/game/network_objects.c": [
        ("network_objects_handle_changes", "orig_network_objects_handle_changes"),
    ],
    "source/camera/director.c": [
        ("director_update", "orig_director_update"),
        ("director_get_perspective", "orig_director_get_perspective"),
    ],
    "port/linux/game/render_interpolation.c": [
        ("render_interpolation_camera", "orig_render_interpolation_camera"),
    ],
    "source/interface/first_person_weapons.c": [
        ("first_person_weapon_draw", "orig_first_person_weapon_draw"),
    ],
    "source/interface/hud_draw.c": [
        ("hud_draw_bitmap_direct", "orig_hud_draw_bitmap_direct"),
        ("hud_draw_numbers", "orig_hud_draw_numbers"),
    ],
    "source/interface/hud_weapon.c": [
        ("hud_render_weapon_interface", "orig_hud_render_weapon_interface"),
    ],
    "source/hs/hs.c": [
        ("hs_compile_and_evaluate", "orig_hs_compile_and_evaluate"),
    ],
    "source/hs/hs_runtime.c": [
        ("hs_runtime_update", "orig_hs_runtime_update"),
    ],
    "port/linux/game/mod_sounds.c": [
        ("mod_sounds_read", "orig_mod_sounds_read"),
    ],
    "port/linux/src/sdl_platform.c": [
        ("platform_next_keystroke", "orig_platform_next_keystroke"),
        ("platform_ui_pointer_read", "orig_platform_ui_pointer_read"),
        ("platform_input_read", "orig_platform_input_read"),
    ],
    "port/linux/src/xinput_sdl.c": [
        ("XInputGetState", "orig_XInputGetState"),
    ],
    "source/cutscene/cinematics.c": [
        ("cinematic_start", "orig_cinematic_start"),
    ],
    "source/scenario/scenario.c": [
        ("scenario_trigger_volume_test_object", "orig_scenario_trigger_volume_test_object"),
    ],
    "source/models/models.c": [
        ("render_model", "orig_render_model"),
    ],
    "source/game/game_engine.c": [
        ("game_engine_post_rasterize_objects", "orig_game_engine_post_rasterize_objects"),
        ("game_engine_remap_object_definition", "orig_game_engine_remap_object_definition"),
    ],
    "port/linux/game/menu_tags.c": [
        ("menu_tags_loaded", "orig_menu_tags_loaded"),
        ("menu_tags_unloaded", "orig_menu_tags_unloaded"),
    ],
}

# the game's sources that ask for the network version through Forge's
# (forge_get_network_version: its own in a Forge game)
FORGE_NET_OVERRIDE_SOURCES = {
    "source/networking/network_server_message_handler.c",
    "source/networking/network_client_manager.c",
    "port/linux/game/menu_functions.c",
    "port/linux/src/p2p_lobby.c",
}


def forge_sources() -> List[Path]:
    """Forge's own sources"""
    return sorted((FORGE_DIR / "src").glob("*.c"))


def forge_source_flags(source: Path) -> str:
    """the flags a game or platform source is compiled with for Forge: its
    renames (-D); the network version's override is not used yet (a Forge
    session is local)"""
    return " ".join(f"-D{old}={new}" for old, new in FORGE_RENAMES.get(source.as_posix(), []))


def forge_cflags(game_cflags: str, zlib_dir: Path) -> str:
    """Forge's own sources' flags: the game's, its headers and zlib's"""
    return f"{game_cflags} -I{FORGE_DIR / 'include'} -I{zlib_dir} -DHALO_RELEASE -DOPENCE_ENABLE_FORGE=1"
