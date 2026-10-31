"""Builds and runs AE's C unit tests (port/linux/tests/ae_*_test.c) with the build machine's compiler.

The units under test have no SDL or game dependency (pattern of tools/test_touch_menu.py), so they compile
anywhere, with warnings as errors."""
import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "port" / "linux" / "tests"
# AE's game-side units sit flat in port/linux/game with the ae_ prefix
UI = ROOT / "port" / "linux" / "game"

SRC = ROOT / "port" / "linux" / "src"

# each test: its sources besides the test file, extra compile flags, and libraries
UNITS = {
    # (the catalog's readers take any text: under the sanitizers)
    "ae_catalog_test.c": ([UI / "ae_catalog.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the check of a capture's explicit name: no "..", no absolute path)
    "capture_name_test.c": ([], [f"-I{SRC}"], []),
    "ae_draw_layout_test.c": ([SRC / "ae_layout.c"], [], []),
    # (AE's faces with stb_truetype; under the sanitizers, as it is handed garbage and truncated fonts)
    "ae_font_test.c": ([SRC / "ae_font.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (ae_ui.c starts motions and asks for sounds: ae_motion.c and ae_sound.c with it, P5)
    "ae_input_rules_test.c": ([UI / "ae_input_rules.c", UI / "ae_ui.c", UI / "ae_motion.c", UI / "ae_sound.c",
                               SRC / "ae_back_presses.c"], [], []),
    "ae_list_test.c": ([UI / "ae_list.c"], [], []),
    "ae_layout_numbers_test.c": ([UI / "ae_layout_numbers.c"], [], []),
    # (0.4.3 CREATE GAME's start countdown: the seconds followed into what the row shows, the bar, the beep rule, the
    # tones)
    "ae_lobby_countdown_test.c": ([UI / "ae_lobby_countdown.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (the game setup spike's GAME OPTIONS model: rows, groups, what shows; under the sanitizers)
    "ae_setup_options_test.c": ([UI / "ae_setup_options.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (the game setup spike's CHANGE MAP model: folders, filters, plays well with; under the sanitizers)
    "ae_setup_maps_test.c": ([UI / "ae_setup_maps.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                             []),
    # (CHANGE GAMETYPE's model: folders, battle rifle pairs, labels, filters, search, RECENT, PLAYS WELL ON; under the
    # sanitizers)
    "ae_gametype_picker_test.c": ([UI / "ae_gametype_picker.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (the pickers' own folders and RECENT's games: their files are the player's text; under the sanitizers)
    "ae_picker_folders_test.c": ([UI / "ae_picker_folders.c"], ["-fsanitize=address,undefined",
                                                                 "-fno-sanitize-recover=all"], []),
    "ae_recent_games_test.c": ([UI / "ae_recent_games.c"], ["-fsanitize=address,undefined",
                                                             "-fno-sanitize-recover=all"], []),
    # (the look's tokens, frame and densities, with the recording ae_draw stub that measures with the real fonts)
    "ae_style_test.c": ([UI / "ae_style.c", TESTS / "ae_draw_stub.c", SRC / "ae_font.c", SRC / "ae_layout.c"],
                        [f"-I{SRC}", f"-I{TESTS}"], []),
    # (the map reader with the system's zlib; under the sanitizers, as it reads corrupt files: the maps it writes
    # itself here, the stock maps in test_mapinfo_stock_maps)
    "ae_mapinfo_test.c": ([UI / "ae_mapinfo.c"],
                          ["-DAE_MAPINFO_SYSTEM_ZLIB", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                          ["-lz"]),
    "ae_motion_test.c": ([UI / "ae_motion.c"], [], []),
    # (the MCC mouse style's pure functions)
    "ae_mouse_test.c": ([UI / "ae_mouse.c"], [], []),
    # (the MCC files' detection over fake Steam trees; under the sanitizers, as Steam's files are untrusted text)
    "ae_mcc_test.c": ([SRC / "ae_mcc.c"], [f"-I{SRC}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (a Workshop item's ModInfo.json and map json readers, synthetic text; under the sanitizers, as the text is untrusted)
    "ae_mcc_workshop_test.c": ([SRC / "ae_mcc_workshop.c", SRC / "server_json.c", SRC / "ae_mcc.c"], [f"-I{SRC}",
                                "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (a map's kind by its identity: ae_map_kind.h; MCC's stock names are ae_mcc.c's)
    "ae_map_kind_test.c": ([UI / "ae_map_kind.c", SRC / "ae_mcc.c"], [f"-I{UI}", f"-I{SRC}", "-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], []),
    # (an MCC stock map's sounds by name in Custom Edition's sounds.map; under the sanitizers, as the file is the player's)
    "ae_mcc_sound_names_test.c": ([UI / "ae_mcc_sound_names.c"], [f"-I{UI}", "-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (map sharing's safety rules: LAN sources at local addresses only, a failed download's files removed and
    # nothing outside the downloads' folder; under the sanitizers, as the paths and addresses are a host's)
    "ae_map_share_rules_test.c": ([UI / "ae_map_share_rules.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (the server browser's pings under AE's rules: SERVER PINGS, REFRESH's limit, the text and colours)
    "ae_net_pings_test.c": ([], [f"-I{SRC}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (LAN broadcasts behind a VPN: the plain broadcast first, each local network's once, within the room given)
    "ae_lan_broadcast_test.c": ([], [f"-I{SRC}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (Linux PlayStation pads: whether the kernel has hid-playstation / hid-sony, from modules.dep and modules.builtin)
    "ae_gamepads_rules_test.c": ([SRC / "ae_gamepads_rules.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                                                                  "-fno-sanitize-recover=all"], []),
    # (split screen's seats: first to press is player 1, held seats, the same pad back, gaps closed)
    "ae_seats_rules_test.c": ([SRC / "ae_seats_rules.c"], ["-iquote", str(ROOT / "port" / "linux" / "include"), "-fsanitize=address,undefined",
                                                          "-fno-sanitize-recover=all"], []),
    # (the controller ports' pads: a pad keeps its port, a new one takes the first free one)
    # (the menus' mouse targets of widgets deleted since they were noted: ui_widget.c's AE hooks)
    # (the bots' power items, by level: ae_bots_power.inc)
    "ae_bots_power_rules_test.c": ([UI / "ae_bots_power_rules.c", UI / "ae_bots_combat_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the bots playing as a team: the numbers game, outnumbered, wingman; ae_bots_team.inc)
    "ae_bots_team_rules_test.c": ([UI / "ae_bots_team_rules.c", UI / "ae_bots_combat_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (ALLY RETICLE: who is an ally under the reticle, when the reticle turns green, the green with the reticle's alpha)
    "ae_ally_reticle_rules_test.c": ([UI / "ae_ally_reticle_rules.c"], ["-fsanitize=address,undefined",
                                                                         "-fno-sanitize-recover=all"], []),
    # (DEV MODE's hit markers: their timing, what counts, their size round the reticle)
    "ae_dev_hit_markers_rules_test.c": ([UI / "ae_dev_hit_markers_rules.c"], ["-fsanitize=address,undefined",
                                                                               "-fno-sanitize-recover=all"], ["-lm"]),
    # (SPCLib modes on OpenCE's gametypes: One in the Chamber's round for a kill, Frag Nation's grenades)
    "ae_modes_rules_test.c": ([UI / "ae_modes_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                              []),
    "ae_bots_revenge_rules_test.c": ([UI / "ae_bots_revenge_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (BACKPACK WEAPONS: a holstered weapon's kind by its tag path, and its place on the back or a thigh)
    "ae_backpack_rules_test.c": ([UI / "ae_backpack_rules.c"], ["-fsanitize=address,undefined",
                                                                 "-fno-sanitize-recover=all"], []),
    "ae_wide_rows_test.c": ([UI / "ae_wide_rows.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_ui_deleted_test.c": ([UI / "ae_ui_deleted.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (a modded disc in the join token: no disc the stock token, the same disc the same token, which maps count;
    # under the sanitizers, as the names are a folder's)
    "ae_disc_join_rules_test.c": ([UI / "ae_disc_join_rules.c", UI / "ae_map_share_rules.c"],
                                  ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_pad_ports_test.c": ([SRC / "ae_pad_ports.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                                                        "-fno-sanitize-recover=all"], []),
    # (gyro aiming on PC: a pad's rate and the time since its last read to a turn; drift at rest, long pauses)
    "ae_gyro_rules_test.c": ([SRC / "ae_gyro_rules.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                                                          "-fno-sanitize-recover=all"], ["-lm"]),
    # (the DualSense's profiles, weapon states, powerups, trigger effect bytes, report framing and CRC)
    "ae_ds5_rules_test.c": ([SRC / "ae_ds5_rules.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                                                        "-fno-sanitize-recover=all"], []),
    # (a map's own picture beside its file: its path, the PNGs drawn; under the sanitizers, as the names and files
    # are a player's)
    "ae_map_picture_rules_test.c": ([UI / "ae_map_picture_rules.c"], ["-fsanitize=address,undefined",
                                                                       "-fno-sanitize-recover=all"], []),
    # (look fixes: a fix's map by name, its field changed only from the value it expects and never outside the
    # tag, the table's entries; the bitmap type errors said once a bitmap and type; under the sanitizers, as the
    # names and tags are a map's)
    "ae_map_look_fixes_rules_test.c": ([UI / "ae_map_look_fixes_rules.c"], ["-fsanitize=address,undefined",
                                                                             "-fno-sanitize-recover=all"], []),
    # (the Settings list's pictures by entry)
    "ae_settings_test.c": ([UI / "ae_settings.c"], [], []),
    # (the 0.3.7 SETTINGS model: groups, rows.md's rows, what shows when, sections, values, the dialogs' texts)
    "ae_settings_rows_test.c": ([UI / "ae_settings_rows.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], ["-lm"]),
    # (KEY BINDINGS' model: the actions and sections, two bindings read and written, a key moved from another action,
    # the dialogs' texts; under the sanitizers)
    "ae_key_bindings_test.c": ([UI / "ae_key_bindings.c"], ["-fsanitize=address,undefined",
                                                             "-fno-sanitize-recover=all"], []),
    # (TEST RUN's list format: the sample list, bad lines kept with their line numbers, the limits; under the
    # sanitizers, as it reads any text)
    "ae_test_run_list_test.c": ([UI / "ae_test_run_list.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (TEST RUN's results file: escaping, a torn last line, pending and resume; under the sanitizers, as it reads any text)
    "ae_test_run_results_test.c": ([UI / "ae_test_run_results.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], []),
    # (TEST RUN's undo table: the first value of a key kept, undo last noted first, the limits; under the sanitizers)
    "ae_test_run_saved_test.c": ([UI / "ae_test_run_saved.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (TEST RUN's decision machine: keys while loading, a held key once, FAIL and NOTE through the note box, BACK,
    # RELOAD, the summary, the DEV MODE gate; under the sanitizers)
    "ae_test_run_test.c": ([UI / "ae_test_run_machine.c", UI / "ae_test_run_clip.c", UI / "ae_test_run_results.c", UI / "ae_test_run_list.c"],
                           ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (TEST RUN's note editing on a fixed buffer, whole UTF-8 characters, paste's first line, the card's text wrap with
    # "…"; under the sanitizers, as it takes a clipboard's text)
    "ae_test_run_card_test.c": ([UI / "ae_test_run_edit.c"], ["-fsanitize=address,undefined",
                                                             "-fno-sanitize-recover=all"], []),
    # (TEST RUN's pad keyboard: the cursor over the table, the case and page, A / X / Y / START / B on the note; under the
    # sanitizers)
    "ae_test_run_keyboard_test.c": ([UI / "ae_test_run_keyboard.c", UI / "ae_test_run_edit.c"], ["-fsanitize=address,undefined",
                                                                                              "-fno-sanitize-recover=all"], []),
    # (a player's own settings file, ae.toml: read, written, seeded, upgraded; under the sanitizers)
    "ae_player_settings_test.c": ([UI / "ae_player_settings.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (the item waypoints' pictures: the style, the fallback to text, a map's sprite checked, the sizes, a line's
    # parts, the glyphs' tables; under the sanitizers, as a map's sprite bounds are untrusted)
    "ae_waypoint_icons_test.c": ([UI / "ae_waypoint_icons.c", UI / "ae_waypoint_glyphs.c"],
                                 ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the LAN list's firewall hint: when it is due, and ufw and firewalld found in fake system trees; under the
    # sanitizers, as it reads the system's files)
    "ae_lan_hint_test.c": ([SRC / "ae_lan_hint.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                                                    "-fno-sanitize-recover=all"], []),
    # (the community services' opt-in: the setting's states, who is asked, what each answer writes, the input guard)
    "ae_community_rules_test.c": ([SRC / "ae_community_rules.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (the zoom action's hold mode: the state machine)
    "ae_zoom_test.c": ([UI / "ae_zoom.c"], [], []),
    # (the 2026-10-16 sweep: the lobby's SHUFFLE deal, the map list's WARM-UP group)
    "ae_sweep_rules_test.c": ([UI / "ae_team_shuffle_rules.c", UI / "ae_map_warm_up.c"],
                              ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the high-res text laid out by its own fonts: the game's fonts' tabular digits, the pen's fraction, line starts)
    "ae_text_layout_test.c": ([SRC / "ae_text_hires.c", UI / "ae_text_layout.c"],
                              [f"-I{SRC}", f"-I{UI}", f'-DAE_FONT_DIR="{ROOT / 'port' / 'assets' / 'fonts'}"',
                               "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                               "-Wno-unused-function"],  # (stb's static copy)
                              ["-lm"]),
    # (the bots of a LAN game: when the lobby has them, how many, their controllers on the host's machine, teams)
    "ae_lan_bots_rules_test.c": ([UI / "ae_lan_bots_rules.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (bots.count changed in a local game in progress: which bot joins or leaves, its team, the test's steps)
    "ae_bots_live_rules_test.c": ([UI / "ae_bots_live_rules.c"], ["-fsanitize=address,undefined",
                                                                "-fno-sanitize-recover=all"], []),
    # (the bots' fight by level: melee, grenades, headshots, confidence and retreat, the random delays)
    "ae_bots_combat_rules_test.c": ([UI / "ae_bots_combat_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], []),
    # (an objective carrier hides and holds: cover scores, the hide spot's score, the hold's end, by level)
    "ae_bots_cover_rules_test.c": ([UI / "ae_bots_cover_rules.c"], ["-fsanitize=address,undefined",
                                                                  "-fno-sanitize-recover=all"], []),
    # (the bots' objective play: every mode's utilities, the carrier's sacrifice, hysteresis, CTF defenders)
    "ae_bots_objective_rules_test.c": ([UI / "ae_bots_objective_rules.c"], ["-fsanitize=address,undefined",
                                                                          "-fno-sanitize-recover=all"], []),
    # (the bots' vehicles: which seat a bot goes for, when it gets out, the driving spike's controls)
    "ae_bots_vehicle_rules_test.c": ([UI / "ae_bots_vehicle_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], []),
    # (the bots' getting around: links' worth, no return, health packs, jump facing, stuck spots)
    "ae_bots_travel_rules_test.c": ([UI / "ae_bots_travel_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], []),
    # (the bots' deaths by their own hand: a death's cause, the edge ahead, a grenade too near)
    "ae_bots_falls_rules_test.c": ([UI / "ae_bots_falls_rules.c"], ["-fsanitize=address,undefined",
                                                                  "-fno-sanitize-recover=all"], ["-lm"]),
    # (SILENT MOVEMENT / SILENT WEAPONS: the byte, every mode x role x own/other sound, the join's mark and refusal,
    # the scopes and the tag table; under the sanitizers, as the join's name is a remote machine's)
    "ae_silent_rules_test.c": ([UI / "ae_silent_rules.c"], ["-fsanitize=address,undefined",
                                                            "-fno-sanitize-recover=all"], []),
    # (HIT FEEDBACK: the byte beside SILENT's, the pop line's cases found in play, the shapes' ranges)
    "ae_hit_feedback_rules_test.c": ([UI / "ae_hit_feedback_rules.c", UI / "ae_silent_rules.c"],
                                     ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (JUGGERNAUT: the byte beside COMBAT FEEDBACK's and SILENT's, every kill's score and succession, the
    # random pick)
    "ae_juggernaut_rules_test.c": ([UI / "ae_juggernaut_rules.c", UI / "ae_hit_feedback_rules.c",
                                    UI / "ae_silent_rules.c"],
                                   ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (PLAYERS ON RADAR ENEMIES: allies hidden in a team game only, the names over heads kept)
    "ae_radar_rules_test.c": ([UI / "ae_radar_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (HEALTH's REACH: the stops at the top of each third, the curve's rate, each third's time)
    "ae_reach_health_rules_test.c": ([UI / "ae_reach_health_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], []),
    # (CE's counter digits at any size: frames, advances, quads, ink bounds)
    "ae_hud_digit_cells_test.c": ([UI / "ae_hud_digit_cells.c"],
                                  ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the AE pause menu's rows: each game's, the host's, headers skipped)
    "ae_pause_rows_test.c": ([UI / "ae_pause_rows.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (CREATE GAME's BOTS row: a typed count's accepted text and the menu's limit)
    "ae_bot_settings_test.c": ([UI / "ae_bot_settings.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (the scoreboard header's text, HUD STYLE AE)
    "ae_header_lines_test.c": ([UI / "ae_header_lines.c"],
                               ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (HUD STYLE AE's CE message lines: pieces on a pen, broken between words, pictures that do not fit)
    "ae_hud_message_flow_test.c": ([UI / "ae_hud_message_flow.c"],
                                   ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the HUD's key prompts on mouse + keyboard: action -> setting, the first key, its glyph or plain keycap)
    "ae_key_prompts_rules_test.c": ([UI / "ae_key_prompts_rules.c"],
                                    ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the map lists' pages: folders and their names, the order, the first of a name, the chooser; under the
    # sanitizers, as the folder names are a player's)
    "ae_map_folders_rules_test.c": ([UI / "ae_map_folders_rules.c"], ["-fsanitize=address,undefined",
                                                                       "-fno-sanitize-recover=all"], []),
    # (AE map layout files: the reader, every refusal, the limits; under the sanitizers, as the file is the map pack's)
    "ae_bomb_snd_rules_test.c": ([], [], []),
    "ae_layout_rules_test.c": ([UI / "ae_layout_rules.c", UI / "ae_weapon_slots.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               []),
    # (layouts travel with maps: the frames, a guest's inbox, which layout it plays and writes; under the sanitizers, as
    # the frames are a host's)
    "ae_layout_share_rules_test.c": ([UI / "ae_layout_share_rules.c", UI / "ae_layout_rules.c", UI / "ae_weapon_slots.c", UI / "ae_map_share_rules.c"],
                                     ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (HUD STYLE: AE unless the setting reads "classic"; a fake config_string)
    "ae_hud_style_test.c": ([UI / "ae_hud_style.c"], [], []),
    # (bug 139: a mode's needs from a map's flags and spawns)
    "ae_mode_guard_rules_test.c": ([UI / "ae_mode_guard_rules.c"], [], []),
    # (Arena Evolved and ChupathingyCE 0.8 in one Delta game: the record by the host's number, its widening, the
    # HELLO check)
    "ae_chupa_link_rules_test.c": ([UI / "ae_chupa_link_rules.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (high-resolution HUD digits drawn at the stock size)
    "ae_hud_hires_test.c": ([UI / "ae_hud_hires.c"], [], []),
    # (the score strip's model: what it says in every mode, its colours and bars; under the sanitizers, as the
    # names are players')
    "ae_strip_model_test.c": ([UI / "ae_strip_model.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                              []),
    # (the score strip's layout: one bottom line, the gap rule, every mode's places; fake metrics)
    "ae_strip_layout_test.c": ([UI / "ae_strip_model.c", UI / "ae_strip_layout.c"],
                               ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (HUD STYLE AE's panels: what the ammo, grenade, shield and health panels say; under the sanitizers, as the
    # numbers come from any map's or pack's weapon)
    "ae_panel_model_test.c": ([UI / "ae_panel_model.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                              []),
    # (HUD STYLE AE's panel layout: lean lines, boxes, cells, rounds, bars, the right column; CE's real digit ink)
    "ae_panel_layout_test.c": ([UI / "ae_panel_model.c", UI / "ae_panel_layout.c", UI / "ae_hud_digit_cells.c"],
                               ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (HUD STYLE AE's motion sensor: the view's wedge, the rings, CE's blips' places, the sweep, the label)
    "ae_sensor_test.c": ([UI / "ae_sensor_geometry.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                         ["-lm"]),
    # (a charged trigger's magazine: none is no magazine, never magazines[NONE]; fake triggers)
    "ae_weapon_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (several weapon packs in one game: the gametype's slot mask, the loadout's sources, the join capability, map
    # spawns and the layout's pack weapons)
    "ae_weapon_slots_test.c": ([UI / "ae_weapon_slots.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (0.7.0: the AE rifles' profiles, by their pack behaviour: the burst BATTLE RIFLE, the EVO RIFLE, the DMR)
    "ae_rifle_profiles_test.c": ([UI / "ae_rifle_profiles.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                                 ["-lm"]),
    # (SENSOR RANGE: the variant's flags bits, the world units, the map's own range kept across games)
    "ae_sensor_range_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (CONTROLLER CROUCH: the variant's flags bit, the crouch gate's term, the log's words)
    "ae_crouch_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (MELEE UNZOOMS: the variant's flags bit beside CONTROLLER CROUCH's, the local zoom, the melee gate's term)
    "ae_melee_unzoom_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_splatter_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (MOUSE AIM ASSIST: the variant's flags bit after MELEE UNZOOMS', the host's policy for message 90)
    "ae_mouse_aim_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (VEHICLES DESTRUCTIBLE: the variant's flags bit after MOUSE AIM ASSIST's, the ratios, the hold)
    "ae_vehicle_death_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (whose vehicle a placement is: the nearest CTF starting location's team, with Sidewinder's own starts)
    "ae_vehicle_sides_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the vehicle damage pack's look: the kinds by the tag's name, the pack's names, the damaged threshold)
    "ae_vehicle_pack_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (a pack's tags on a Custom Edition map: the reads its memory lets through, its bitmaps' Xbox layout)
    "ae_pack_memory_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (a map's own battle rifle replaced by the pack's: the name rule, the reference redirect, the log's kinds)
    "ae_pack_rifle_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_sound_test.c": ([UI / "ae_sound.c"], [], []),
    # (editing a field's text: under the sanitizers, as it takes pasted text)
    "ae_text_edit_test.c": ([UI / "ae_text_edit.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_ui_test.c": ([UI / "ae_ui.c", UI / "ae_motion.c", UI / "ae_sound.c"], [], []),
    # (the platform layer's worker and lock, which the map facts' worker runs on: ae_glue_setup_maps.c)
    "ae_worker_test.c": ([SRC / "ae_worker.c"], [f"-I{SRC}", "-D_POSIX_C_SOURCE=200809L", "-pthread"], ["-pthread"]),
    # (the texts, checked against Overpass 900)
    "ae_strings_test.c": ([UI / "ae_strings.c", SRC / "ae_font.c"], [f"-I{SRC}"], []),
    # (the widget core with the recording ae_draw stub and the real fonts; under the sanitizers)
    # (tabs, page dots, prompts, the in-view panel: the widget core with them, the stub, the real fonts)
    "ae_widgets_nav_test.c": ([UI / "ae_widgets_nav.c", UI / "ae_widgets.c", UI / "ae_style.c", UI / "ae_list.c",
                               UI / "ae_motion.c", UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c",
                               TESTS / "ae_draw_stub.c", SRC / "ae_font.c", SRC / "ae_layout.c"],
                              [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                              []),
    # (the value picker, chips, the help panel: the widget core and navigation with them)
    "ae_widgets_pick_test.c": ([UI / "ae_widgets_pick.c", UI / "ae_widgets_nav.c", UI / "ae_widgets.c",
                                UI / "ae_style.c", UI / "ae_list.c", UI / "ae_motion.c", UI / "ae_sound.c",
                                UI / "ae_strings.c", UI / "ae_ui.c", TESTS / "ae_draw_stub.c", SRC / "ae_font.c",
                                SRC / "ae_layout.c"],
                               [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               []),
    # (the text field, typing, AE's keyboard)
    "ae_widgets_text_test.c": ([UI / "ae_widgets_text.c", UI / "ae_text_edit.c", UI / "ae_widgets_nav.c",
                                UI / "ae_widgets.c", UI / "ae_style.c", UI / "ae_list.c", UI / "ae_motion.c",
                                UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c", TESTS / "ae_draw_stub.c",
                                SRC / "ae_font.c", SRC / "ae_layout.c"],
                               [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               []),
    # (a Custom Edition map's pause widgets checked before they are patched; under the sanitizers, as the map is
    # anyone's: broken counts, addresses, indices and loops)
    "ae_ce_ui_check_test.c": ([UI / "ae_ce_ui_check.c"], [f"-I{UI}", "-fsanitize=address,undefined",
                                                          "-fno-sanitize-recover=all"], []),
    # (the lobby's roster order and names, preflight P18)
    "ae_lobby_roster_test.c": ([UI / "ae_lobby_roster.c"], [f"-I{UI}", "-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (the lobby joiner's profile choices: unused saved profiles and GUEST, with today's automatic choice focused)
    "ae_lobby_profile_choices_test.c": ([UI / "ae_lobby_profile_choices.c"], [f"-I{UI}",
                                                                                "-fsanitize=address,undefined",
                                                                                "-fno-sanitize-recover=all"], []),
    # (0.4.2 MY PARTY's rows: folded, the party view, a joiner's; found again; the window that shows the focus)
    "ae_party_list_test.c": ([UI / "ae_party_list.c", UI / "ae_lobby_roster.c"], [f"-I{UI}",
                                                                                   "-fsanitize=address,undefined",
                                                                                   "-fno-sanitize-recover=all"], []),
    # (an invite's code in a player's text: JOIN WITH INVITE, the lobby's join; under the sanitizers, as it is pasted)
    "ae_invite_rules_test.c": ([UI / "ae_invite_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               []),
    # (0.7.0 JOIN BY ADDRESS: the typed address, RECENT's list, the time words)
    "ae_address_rules_test.c": ([UI / "ae_address_rules.c"], ["-fsanitize=address,undefined",
                                                             "-fno-sanitize-recover=all"], []),
    # (0.7.0 the name lookup thread's bookkeeping: a cancelled lookup's late answer is dropped)
    "ae_resolve_state_test.c": ([UI / "ae_resolve_state.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (the lobby's host compatibility: the join's own test, halo_port_limits.h)
    "ae_lobby_compat_test.c": ([], [], []),
    "ae_updater_pick_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the dialog, roster cards)
    "ae_widgets_dialog_test.c": ([UI / "ae_widgets_dialog.c", UI / "ae_widgets_text.c", UI / "ae_text_edit.c",
                                  UI / "ae_widgets_nav.c", UI / "ae_widgets.c", UI / "ae_style.c", UI / "ae_list.c",
                                  UI / "ae_motion.c", UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c",
                                  TESTS / "ae_draw_stub.c", SRC / "ae_font.c", SRC / "ae_layout.c"],
                                 [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                                 []),
    "ae_widgets_test.c": ([UI / "ae_widgets.c", UI / "ae_widgets_nav.c", UI / "ae_style.c", UI / "ae_list.c", UI / "ae_motion.c",
                           UI / "ae_sound.c", UI / "ae_strings.c", UI / "ae_ui.c", TESTS / "ae_draw_stub.c",
                           SRC / "ae_font.c", SRC / "ae_layout.c"],
                          [f"-I{SRC}", f"-I{TESTS}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
}
# a test's exit code for "nothing to check here" (the real maps missing): pytest.skip
SKIPPED = 77
# a build failing only for what this machine lacks (zlib's headers, the sanitizers' runtimes): pytest.skip
MISSING = ("zlib.h: No such file", "cannot find -lz", "cannot find -lasan", "cannot find -lubsan",
           "libasan", "libubsan", "libclang_rt.asan", "libclang_rt.ubsan")


def compiler():
    """Finds a C compiler on the build machine: the path of gcc or clang (the first found), or None."""
    for name in ("gcc", "clang"):
        path = shutil.which(name)
        if path:
            return path
    return None


def build_unit(test, tmp_path):
    """Builds one unit with its test, warnings as errors: the binary (skips without a compiler or what it needs)."""
    cc = compiler()
    if not cc:
        pytest.skip("no C compiler")
    sources, flags, libraries = UNITS[test]
    binary = tmp_path / test.replace(".c", "")
    build = subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", *flags,
                            f"-I{UI}", str(TESTS / test), *map(str, sources), *libraries, "-lm", "-o", str(binary)],
                           capture_output=True, text=True)
    if build.returncode != 0 and any(missing in build.stderr for missing in MISSING):
        pytest.skip("this machine lacks what the test builds with: " + build.stderr.strip().splitlines()[-1])
    assert build.returncode == 0, build.stdout + build.stderr
    return binary


def run_unit(binary, tmp_path, *args):
    run = subprocess.run([str(binary), *args], capture_output=True, text=True, cwd=ROOT,
                         env=dict(os.environ, AE_TEST_SCRATCH=str(tmp_path)))
    if run.returncode == SKIPPED:
        pytest.skip(run.stdout.strip().splitlines()[-1] if run.stdout.strip() else "nothing to check")
    assert run.returncode == 0, run.stdout + run.stderr


@pytest.mark.parametrize("test", sorted(UNITS))
def test_unit(test, tmp_path):
    """Builds one unit with its test, warnings as errors, and runs it."""
    run_unit(build_unit(test, tmp_path), tmp_path)


def test_mapinfo_stock_maps(tmp_path):
    """The map reader against the stock maps' known facts: skipped when $AE_MAPINFO_MAPS or $HALO_DATA_ROOT/maps
    doesn't have them (as on CI); the reader's own maps are test_unit's."""
    run_unit(build_unit("ae_mapinfo_test.c", tmp_path), tmp_path, "stock")


def test_game_units_reach_threads_through_the_platform_layer():
    """AE's game units (port/linux/game) never include <pthread.h>: the Windows builds give them no POSIX headers
    (port/windows/include/posix is the platform layer's), and in the 32-bit Linux build it brings the C library's
    <limits.h>, whose LONG_MAX breaks cseries.h. Threads come from the platform layer (port/linux/src/ae_worker.h).
    And a game unit's fopen of a game path (d:\\maps\\...) is the port's wrapper on Windows too: the game units'
    <stdio.h> there (port/windows/include/crt) renames fopen to halo_windows_fopen, which translates the path."""
    offenders = [path.name for path in sorted(UI.glob("*.[ch]"))
                 if re.search(r"^\s*#\s*include\s*<pthread\.h>", path.read_text(errors="replace"), re.M)]
    assert not offenders, offenders
    crt = ROOT / "port" / "windows" / "include" / "crt"
    assert '#include "halo_windows_file_names.h"' in (crt / "stdio.h").read_text()
    assert "#define fopen halo_windows_fopen" in (crt / "halo_windows_file_names.h").read_text()
    windows_build = (ROOT / "tools" / "windows_build.py").read_text()
    game_flags = windows_build[windows_build.index("game_cflags = "):windows_build.index("for source in game_sources")]
    assert 'f"-I{crt_include}"' in game_flags and "posix_include" not in game_flags
