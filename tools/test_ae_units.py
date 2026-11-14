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
    # (0.9.0 VIDEO's GRAPHICS PRESET: its values, QUALITY / PERFORMANCE / CUSTOM, the rows set by hand; TWO PLAYERS'
    # AUTO, the renderer's own tables and rule; under the sanitizers)
    "ae_graphics_preset_test.c": ([UI / "ae_graphics_preset.c", UI / "ae_split_graphics_rules.c",
                                    UI / "ae_split_layout_rules.c"], ["-fsanitize=address,undefined",
        "-fno-sanitize-recover=all"], ["-lm"]),
    # (the host's head check of a reported head hit: under the sanitizers)
    "ae_head_check_rules_test.c": ([UI / "ae_head_check_rules.c"], ["-fsanitize=address,undefined",
        "-fno-sanitize-recover=all"], []),
    # (the catalog's readers take any text: under the sanitizers)
    "ae_catalog_test.c": ([UI / "ae_catalog.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (0.9.0 TARGET 60 FPS: the scale's steps, its hysteresis, its floor and the forced percent)
    "ae_dynamic_resolution_rules_test.c": ([UI / "ae_dynamic_resolution_rules.c"], ["-fsanitize=address,undefined",
                                                                                    "-fno-sanitize-recover=all"], ["-lm"]),
    # (0.9.0 LOW LATENCY: the period from the swaps' gaps, the cost, the wait before input, the margin, the safety rules)
    "ae_frame_pacer_rules_test.c": ([UI / "ae_frame_pacer_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], []),
    # (the Steam Deck's frame limit in step with the display: the interval a limit at the refresh rate or a fraction gets)
    "ae_swap_interval_rules_test.c": ([UI / "ae_swap_interval_rules.c"], [], []),
    # (0.9.0 SPLIT SCREEN: two players side by side or with side bars, the safe frames, the widened field of view, the
    # HUD's share; the presets SPLIT GRAPHICS plays)
    "ae_split_layout_rules_test.c": ([UI / "ae_split_layout_rules.c"], ["-fsanitize=address,undefined",
                                                                        "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_split_graphics_rules_test.c": ([UI / "ae_split_graphics_rules.c"], [], []),
    # (the check of a capture's explicit name: no "..", no absolute path)
    "capture_name_test.c": ([], [f"-I{SRC}"], []),
    "ae_draw_layout_test.c": ([SRC / "ae_layout.c"], [], []),
    # (AE's faces with stb_truetype; under the sanitizers, as it is handed garbage and truncated fonts)
    "ae_font_test.c": ([SRC / "ae_font.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (ae_ui.c starts motions and asks for sounds: ae_motion.c and ae_sound.c with it, P5)
    "ae_input_rules_test.c": ([UI / "ae_input_rules.c", UI / "ae_ui.c", UI / "ae_motion.c", UI / "ae_sound.c",
                               SRC / "ae_back_presses.c"], [], []),
    "ae_list_test.c": ([UI / "ae_list.c"], [], []),
    # (0.7.8 a controller's D-pad on the open scoreboard: pages when pressed and held, none of it to the game)
    "ae_scoreboard_pad_rules_test.c": ([UI / "ae_scoreboard_pad_rules.c", UI / "ae_ui.c", UI / "ae_motion.c",
                                        UI / "ae_sound.c", SRC / "ae_back_presses.c"], [], []),
    # (the sword lunge's carry/strike duration, bounded target turn and end action)
    # (0.7.6 the first-person weapon's placement: the node math, the table and its file's text)
    "ae_view_model_rules_test.c": ([UI / "ae_view_model_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_view_sway_rules_test.c": ([UI / "ae_view_sway_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_sword_lunge_rules_test.c": ([UI / "ae_sword_lunge_rules.c"], [], ["-lm"]),
    # (0.7.6 a pack weapon's tracked grenade: tracked, its fuse held back while held, set off on release)
    "ae_tracked_grenade_rules_test.c": ([UI / "ae_tracked_grenade_rules.c"], [], []),
    # (0.7.6 a pack weapon with no picture: its name as text in the kill feed and its HUD)
    "ae_pack_names_rules_test.c": ([UI / "ae_pack_names_rules.c"], ["-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], []),
    # (which sound a shield pop plays: the MCC install's, else the stock game's)
    "ae_shield_pop_sound_rules_test.c": ([UI / "ae_shield_pop_sound_rules.c"], [], []),
    # (0.8.0 when the early shield pop shows: one more headshot of the weapon in use finishes)
    "ae_shield_pop_rules_test.c": ([UI / "ae_shield_pop_rules.c"], [], []),
    "ae_scope_mask_rules_test.c": ([UI / "ae_scope_mask_rules.c"], [], []),
    # (a map's collision planes whose normals are not unit length: rescaled, or the surface left out)
    "ae_plane_rules_test.c": ([UI / "ae_plane_rules.c"], [], []),
    "ae_profiles_cache_rules_test.c": ([UI / "ae_profiles_cache_rules.c"], [], []),
    "ae_master_backoff_rules_test.c": ([UI / "ae_master_backoff_rules.c"], [], []),
    "ae_layout_numbers_test.c": ([UI / "ae_layout_numbers.c"], [], []),
    # (a vehicle search pays more for narrow edges; on foot nothing changes)
    "ae_path_cost_test.c": ([UI / "ae_path_cost.c"], [], ["-lm"]),
    # (0.4.3 CREATE GAME's start countdown: the seconds followed into what the row shows, the bar, the beep rule, the
    # tones)
    "ae_lobby_countdown_test.c": ([UI / "ae_lobby_countdown.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (the game setup spike's GAME OPTIONS model: rows, groups, what shows; under the sanitizers)
    "ae_setup_options_test.c": ([UI / "ae_setup_options.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (0.7.6 the typed whole-number fields: ranges, texts, and GAME OPTIONS' rows against their stored fields)
    "ae_number_field_test.c": ([UI / "ae_number_field.c", UI / "ae_setup_options.c"],
                               ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
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
    # (0.7.8 the menus' MCC music: a sound bank's streams, songs, play order, Ogg pages; under the sanitizers)
    "ae_mcc_music_rules_test.c": ([UI / "ae_mcc_music_rules.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    "ae_section_folds_test.c": ([UI / "ae_section_folds.c"], ["-fsanitize=address,undefined",
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
    # (Steam's Workshop collections as text: bodies, answers, cache, lookup; under the sanitizers, as Steam's answer is untrusted)
    "ae_workshop_collections_test.c": ([SRC / "ae_workshop_collections.c", SRC / "server_json.c"], [f"-I{SRC}",
                                       "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (a map's kind by its identity: ae_map_kind.h; MCC's stock names are ae_mcc.c's)
    # (the gametype's JUMP byte and a map layout's jump line)
    "ae_jump_rules_test.c": ([], [f"-I{UI}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_map_kind_test.c": ([UI / "ae_map_kind.c", SRC / "ae_mcc.c"], [f"-I{UI}", f"-I{SRC}", "-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], []),
    # (an MCC stock map's sounds by name in Custom Edition's sounds.map; under the sanitizers, as the file is the player's)
    "ae_mcc_sound_names_test.c": ([UI / "ae_mcc_sound_names.c"], [f"-I{UI}", "-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (map sharing's safety rules: LAN sources at local addresses only, a failed download's files removed and
    # nothing outside the downloads' folder; under the sanitizers, as the paths and addresses are a host's)
    "ae_map_share_rules_test.c": ([UI / "ae_map_share_rules.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (map sharing's connection over the game's own: its frame, the window bounding what is in flight, the inbox; under
    # the sanitizers, as the frames are a peer's)
    "ae_map_share_channel_rules_test.c": ([UI / "ae_map_share_channel_rules.c"], ["-fsanitize=address,undefined",
                                                                           "-fno-sanitize-recover=all"], []),
    # (the dedicated server's shuffled rotation: every entry once a pass, no map twice in a row, the boundary too)
    "ae_rotation_shuffle_test.c": ([UI / "ae_rotation_shuffle.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], []),
    # (the dedicated server's playlist rules: weights, no-repeat, nominations, rock the vote, audiences, the listing's range)
    "ae_playlist_rules_test.c": ([UI / "ae_playlist_rules.c", UI / "ae_server_listing_rules.c", UI / "ae_rotation_shuffle.c"],
                                 ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (a secret the dedicated server makes goes to a 0600 file, not a journal)
    "ae_server_secret_rules_test.c": ([UI / "ae_server_secret_rules.c"], ["-DAE_SERVER_SECRET_STANDALONE", "-fsanitize=address,undefined",
                                                                         "-fno-sanitize-recover=all"], []),
    # (the AE master listing: a placeholder key is never used, the AE extras block in the master's canonical form,
    # the browser's merge of what masters say of a game)
    "ae_server_listing_rules_test.c": ([UI / "ae_server_listing_rules.c"], ["-fsanitize=address,undefined",
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
    # (0.8.0 a split-screen player who quits comes off the local list while others stay on it)
    "ae_split_quit_rules_test.c": ([UI / "ae_split_quit_rules.c"], [], []),
    # (menus phase 1: the main menu's and MULTIPLAYER's rows, the first focus, COMING SOON)
    "ae_main_menu_rules_test.c": ([UI / "ae_main_menu_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (menus phase 1: MATCHMAKING's playlists file, the server a party is sent to)
    "ae_matchmaking_rules_test.c": ([UI / "ae_matchmaking_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (menus phase 1: the PARTY manager's B: a tap is Back, a 0.8 s hold removes a guest)
    "ae_party_rules_test.c": ([UI / "ae_party_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (menus phase 1: WHAT'S NEW's releases from the shipped CHANGELOG, the unseen count)
    "ae_changelog_rules_test.c": ([UI / "ae_changelog_rules.c", UI / "ae_motd_rules.c", ROOT / "port" / "third_party" / "monocypher" / "monocypher.c",
                                   ROOT / "port" / "third_party" / "monocypher" / "monocypher-ed25519.c"],
                                  ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (menus phase 1: MESSAGE OF THE DAY's show rule, its text, the master's signature over it, its state file)
    "ae_motd_rules_test.c": ([UI / "ae_motd_rules.c", ROOT / "port" / "third_party" / "monocypher" / "monocypher.c",
                              ROOT / "port" / "third_party" / "monocypher" / "monocypher-ed25519.c"],
                             ["-iquote", str(ROOT / "port" / "third_party" / "monocypher"),
                              "-iquote", str(ROOT / "port" / "third_party" / "jsmn"),
                              "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (0.8.0 a split-screen party on one team: the join pick, the balance, the shuffle's deal, the death balance)
    "ae_party_teams_rules_test.c": ([UI / "ae_party_teams_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the bots playing as a team: the numbers game, outnumbered, wingman; ae_bots_team.inc)
    "ae_bots_team_rules_test.c": ([UI / "ae_bots_team_rules.c", UI / "ae_bots_combat_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the bots' side lanes: the walkable graph's searches, the lane choice, a waypoint walk; ae_bots_lanes.c)
    "ae_bots_lanes_rules_test.c": ([UI / "ae_bots_lanes_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the bots' vantage spots: the distance band, the candidates' spacing, the score, the choice, the height grid; ae_bots_vantage.c)
    "ae_follow_camera_rules_test.c": ([UI / "ae_follow_camera_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_bots_vantage_rules_test.c": ([UI / "ae_bots_vantage_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (ALLY RETICLE: who is an ally under the reticle, when the reticle turns green, the green with the reticle's alpha)
    # (MARKS: the shared button's tap and hold, a mark's life, its place at the view's edge)
    "ae_mark_rules_test.c": ([UI / "ae_mark_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the callout wheel: the press's tap, double and hold, the wheel's pick and stickiness, the mouse's travel)
    "ae_wheel_rules_test.c": ([UI / "ae_wheel_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (fireteams: the even split of a team's bots over its leaders, squads of 3 to 5, recruiting, a leader leaving)
    "ae_fireteam_rules_test.c": ([UI / "ae_fireteam_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the callout wheel's ring: radii, gaps, the split view's numbers, where each caption and icon sits, the legend's and
    # the word's cells, the send's flash)
    "ae_wheel_layout_test.c": ([UI / "ae_wheel_layout.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the seat panel: a name cut to the bar, the pips, where each seat sits)
    "ae_seat_panel_rules_test.c": ([UI / "ae_seat_panel_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the aim prompt: the words of each target and answer, the row's sizes, where it sits against the target, the name
    # over it, the reticle's zone, the text rows and the cap from the middle)
    "ae_mark_prompt_rules_test.c": ([UI / "ae_mark_prompt_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], ["-lm"]),
    # (the item prompt: its words, sizes by WAYPOINT SIZE, the fit to a width, its place under the reticle, the fade, the wide keys' words)
    "ae_item_prompt_rules_test.c": ([UI / "ae_item_prompt_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], ["-lm"]),
    # (the markers' size by where they sit, the feed's merged lines and words, the message slot a line takes, and which
    # text rows an open wheel hides)
    "ae_marker_rules_test.c": ([UI / "ae_marker_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               ["-lm"]),
    # (MEDALS: who earns which medal, from a kill's facts: counters, windows, order, resets, the end of a game's; under
    # the sanitizers)
    "ae_medal_rules_test.c": ([UI / "ae_medal_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the post-game report: accuracy by rounds, the pair table, sprees and multikills, medal and weapon lists, the summary's
    # wire form and the kept report; under the sanitizers)
    "ae_pgcr_rules_test.c": ([UI / "ae_pgcr_rules.c", UI / "ae_pgcr_data.c"], ["-fsanitize=address,undefined",
                                                                              "-fno-sanitize-recover=all"], []),
    # (0.7.7 the rating core: levels, points, a game's change, quit protection, the window and search, scores, start delays,
    # team balance, bot anchors and the offline roster; the career record's text; under the sanitizers)
    "ae_rating_rules_test.c": ([UI / "ae_rating_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_rating_result_test.c": ([UI / "ae_rating_result.c", UI / "ae_rating_rules.c"], ["-fsanitize=address,undefined",
                                                                                    "-fno-sanitize-recover=all"], []),
    "ae_career_record_test.c": ([UI / "ae_career_record.c", UI / "ae_rating_result.c", UI / "ae_rating_rules.c"],
                                ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the service record: players kept by key, the games added once, medals, kinds, rivals and their versus, the file's
    # round trip and a newer version; under the sanitizers)
    "ae_service_record_test.c": ([UI / "ae_service_record.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (the post-game report screen: the cursor over the board, the medal strip's order and rest, the medals' family colours,
    # the words of K/D, accuracy and weapons, the points that add up; under the sanitizers)
    "ae_pgcr_screen_rules_test.c": ([UI / "ae_pgcr_screen_rules.c", UI / "ae_medal_pack_rules.c"],
                                    ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (0.8.1 SERVICE RECORD's MEDALS page: every kind in the sorts and categories, the earned first, the grid's cursor and
    # window under the fade, the counts' commas, DEV MODE's made-up record; under the sanitizers)
    "ae_record_medals_rules_test.c": ([UI / "ae_record_medals_rules.c", UI / "ae_pgcr_screen_rules.c",
                                       UI / "ae_medal_pack_rules.c"],
                                      ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (MEDALS' records and what reads them: shots, the damage log of a life, steps, pickups; NO SCOPE, QUICK DRAW, REVERSAL,
    # NADE SHOT, the 360, the NINJA, OFF THE RACK and PERFECT; under the sanitizers)
    "ae_medal_history_test.c": ([UI / "ae_medal_history.c", UI / "ae_medal_rules.c"], ["-fsanitize=address,undefined",
                                                                                      "-fno-sanitize-recover=all"], ["-lm"]),
    # (the medals' pictures: the pack's index and pixels, the career list's rows, the block decode, names from keys; under
    # the sanitizers, as the files are the player's)
    "ae_medal_pack_rules_test.c": ([UI / "ae_medal_pack_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], []),
    # (the medals on screen: slots, time line, how many show, and picture, holder text or message list line)
    "ae_medal_draw_rules_test.c": ([UI / "ae_medal_draw_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], ["-lm"]),
    # (the kill feed: weapons by their damage's path, rows' life and overflow, colours, layout, the split-screen line)
    "ae_killfeed_rules_test.c": ([UI / "ae_killfeed_rules.c", UI / "ae_marker_rules.c", UI / "ae_weapon_registry.c"],
                                 ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the MODERN medal stack: its metrics, rows, the limit, the pops, the gaps)
    "ae_medal_modern_rules_test.c": ([UI / "ae_medal_modern_rules.c", UI / "ae_marker_rules.c"], ["-fsanitize=address,undefined",
                                                                         "-fno-sanitize-recover=all"], ["-lm"]),
    # (0.7.7 CROSSHAIR v3: the shape library, the share code read strictly, the profile strings, the pieces drawn, the
    # host rule's bit; a million fuzzed inputs; under the sanitizers)
    "ae_crosshair_rules_test.c": ([UI / "ae_crosshair_rules.c"], [f"-I{UI}", "-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], ["-lm"]),
    # (0.7.7 EDIT CROSSHAIR's rows against an in-memory profile: each row changes its own field only, a long weapons table is
    # kept, the lists are the player's, DELETE leaves its slot, UNDO puts the whole slot back)
    "ae_crosshair_edit_test.c": ([UI / "ae_crosshair_edit.c", UI / "ae_crosshair_rules.c"], [f"-I{UI}", "-fsanitize=address,undefined",
                                                                                               "-fno-sanitize-recover=all"], ["-lm"]),
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
    # (DEV MODE's command grammar: the words, the arguments' shapes, completion; under the sanitizers, as the lines are
    # typed or come over the telnet console)
    "ae_dev_cmd_rules_test.c": ([UI / "ae_dev_cmd_rules.c", UI / "ae_weapon_tune_rules.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (0.7.8 the benchmark's stage seconds: a finite number above 0 and at most 600, nothing after it)
    "ae_benchmark_rules_test.c": ([UI / "ae_benchmark_rules.c"], ["-fsanitize=address,undefined",
                                                                 "-fno-sanitize-recover=all"], []),
    # (0.7.8 DEV MODE's ttk readout: a killer's first damage to a kill, per life; the line)
    # (0.7.9 DEV MODE's view bots: the board's layout, the strip, the tags and pips, the names; under the sanitizers)
    "ae_view_bots_rules_test.c": ([UI / "ae_view_bots_rules.c"], ["-fsanitize=address,undefined",
                                                                  "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_dev_ttk_rules_test.c": ([UI / "ae_dev_ttk_rules.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (0.8.1 a crouched jump's clips: a crouch clip not its weapon class's own is the stand seat's)
    "ae_crouch_air_rules_test.c": ([UI / "ae_crouch_air_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], []),
    # (0.8.1 DEV MODE's INFINITE AMMO and BOTTOMLESS CLIP: a player's weapon only, never a bot's)
    "ae_dev_ammo_rules_test.c": ([UI / "ae_dev_ammo_rules.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (0.7.8 DEV MODE's tune command: the weapon number keys, their recipe field paths and their engine limits)
    "ae_weapon_tune_rules_test.c": ([UI / "ae_weapon_tune_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], []),
    # (0.7.8 DEV MODE's target dummies: their spots, the heal and respawn timers, which bots they are)
    "ae_dev_dummy_rules_test.c": ([UI / "ae_dev_dummy_rules.c"], ["-fsanitize=address,undefined",
                                                                  "-fno-sanitize-recover=all"], ["-lm"]),
    # (0.7.8 the weapon HUD element mover: the move by anchor, the table and its file's text)
    "ae_hud_offsets_rules_test.c": ([UI / "ae_hud_offsets_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], ["-lm"]),
    # (DEV MODE's weapon packs: every installed pack added only where nobody can join; the played mask the gametype's)
    "ae_dev_packs_rules_test.c": ([UI / "ae_dev_packs_rules.c"], [], []),
    # (DEV MODE's command state: a reset and the kept rules of their own game only, the settings to put back)
    "ae_dev_cmd_state_test.c": ([UI / "ae_dev_cmd_state.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (DEV MODE's event files: only the stream's own names, the newest 50 kept; under the sanitizers, as the names
    # are a folder's)
    "ae_dev_events_rules_test.c": ([UI / "ae_dev_events_rules.c"], ["-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], []),
    # (TEST RUN's live agent on the card: each check's verdict and reason, its message)
    "ae_test_run_agent_test.c": ([UI / "ae_test_run_agent.c"], ["-fsanitize=address,undefined",
                                                                 "-fno-sanitize-recover=all"], []),
    # (a map's own picture beside its file: its path, the PNGs drawn; under the sanitizers, as the names and files
    # are a player's)
    "ae_map_picture_rules_test.c": ([UI / "ae_map_picture_rules.c"], ["-fsanitize=address,undefined",
                                                                       "-fno-sanitize-recover=all"], []),
    # (the automatic map picture: which maps, when the shot is due, the 16:9 crop and scale, its PNG; under the sanitizers)
    "ae_map_auto_picture_rules_test.c": ([UI / "ae_map_auto_picture_rules.c", UI / "ae_map_picture_rules.c"],
                                         ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (look fixes: a fix's map by name, its field changed only from the value it expects and never outside the
    # tag, the table's entries; the bitmap type errors said once a bitmap and type; under the sanitizers, as the
    # names and tags are a map's)
    "ae_map_look_fixes_rules_test.c": ([UI / "ae_map_look_fixes_rules.c"], ["-fsanitize=address,undefined",
                                                                             "-fno-sanitize-recover=all"], []),
    # (CUSTOM LAYOUT's rows, what counts as changed, the longest text, the wait for a press; under the sanitizers)
    "ae_custom_layout_rules_test.c": ([UI / "ae_custom_layout_rules.c", ROOT / "port/linux/src/controller_layout.c"],
                                      ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the Settings list's pictures by entry)
    "ae_settings_test.c": ([UI / "ae_settings.c"], [], []),
    # (the 0.3.7 SETTINGS model: groups, rows.md's rows, what shows when, sections, values, the dialogs' texts)
    "ae_settings_rows_test.c": ([UI / "ae_settings_rows.c", UI / "ae_number_field.c", UI / "ae_setup_options.c",
                                 UI / "ae_graphics_preset.c", UI / "ae_split_graphics_rules.c",
        UI / "ae_split_layout_rules.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], ["-lm"]),
    # (KEY BINDINGS' model: the actions and sections, two bindings read and written, a key moved from another action,
    # the dialogs' texts; under the sanitizers)
    "ae_key_bindings_test.c": ([UI / "ae_key_bindings.c"], ["-fsanitize=address,undefined",
                                                             "-fno-sanitize-recover=all"], []),
    # (0.7.7.1 the vote popover: a press selects, a second casts, a cast can change)
    "ae_vote_select_rules_test.c": ([UI / "ae_vote_select_rules.c"], [], []),
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
    # (AIM CONTROL: MODERN's rules: the dead zones, the curve and the ramp, against the study's tables)
    "ae_aim_modern_rules_test.c": ([UI / "ae_aim_modern_rules.c"], ["-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], ["-lm"]),
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
    # (0.7.7 bots on a dedicated server: the fill rules, replace at death, the held slot of a dropped human)
    "ae_server_bots_rules_test.c": ([UI / "ae_server_bots_rules.c", UI / "ae_lan_bots_rules.c"],
                                    ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (0.7.7 dedicated server admin: API keys, restart warnings, the daily restart, backups' names)
    "ae_server_admin_rules_test.c": ([UI / "ae_server_admin_rules.c"], ["-fsanitize=address,undefined",
                                                                       "-fno-sanitize-recover=all"], []),
    # (0.7.7 security wave 2: the web port's sign-in budgets, the Host allowlist, the RCON's socket caps and quoting)
    "ae_server_sec_rules_test.c": ([UI / "ae_server_sec_rules.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], []),
    # (0.7.7 running a server from inside the game: roles and actions, ban lengths, the card's words, key rows, the
    # admin message 113 on the wire, UP NEXT)
    "ae_server_ingame_rules_test.c": ([UI / "ae_server_ingame_rules.c", UI / "mcc_sha256.c"], ["-fsanitize=address,undefined",
                                                                         "-fno-sanitize-recover=all"], []),
    # (0.7.7 a dedicated server's lobby page: the lobby info message 115 on the wire, its entries, the page's words, the
    # title's two faces)
    "ae_server_lobby_rules_test.c": ([UI / "ae_server_lobby_rules.c"], ["-fsanitize=address,undefined",
                                                                       "-fno-sanitize-recover=all"], []),
    # (0.7.7 HOST A SERVER: the settings file, free ports, the start's states, running.txt, the status file, the quit
    # choices, the data folder's links)
    "ae_host_dedicated_rules_test.c": ([UI / "ae_host_dedicated_rules.c"], ["-fsanitize=address,undefined",
                                                                           "-fno-sanitize-recover=all"], []),
    # (bot_names.txt: one name a line, comments, the clip, the pick past the last name)
    "ae_bot_names_rules_test.c": ([UI / "ae_bot_names_rules.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (0.7.7 dedicated server moderation depth and abuse limits: admin rows, immunity, whitelist, bans by network and
    # name, chat flood, ping watch, event hooks, alerts, claim)
    "ae_server_mod_rules_test.c": ([UI / "ae_server_mod_rules.c", UI / "ae_server_admin_rules.c"],
                                   ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (bots.count changed in a local game in progress: which bot joins or leaves, its team, the test's steps)
    "ae_bots_live_rules_test.c": ([UI / "ae_bots_live_rules.c"], ["-fsanitize=address,undefined",
                                                                "-fno-sanitize-recover=all"], []),
    # (the bots' fight by level: melee, grenades, headshots, confidence and retreat, the random delays)
    "ae_bots_combat_rules_test.c": ([UI / "ae_bots_combat_rules.c"], ["-fsanitize=address,undefined",
                                                                    "-fno-sanitize-recover=all"], []),
    # (AE's six bot level names: the engine ids, the AE rows and their field order, and level/id round trips)
    "ae_bots_skill_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
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
    # (what a gametype names of the weapon cache: sets, the legacy bytes, the AECW block, the START rules, the old
    # grenade bits)
    "ae_weapon_selection_test.c": ([UI / "ae_weapon_selection.c"], ["-fsanitize=address,undefined",
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
    # (DEV MODE's weapon wheel: the page groups, the pages' sizes, the sticky pick, a name's margin in its slice)
    "ae_dev_wheel_rules_test.c": ([UI / "ae_dev_wheel_rules.c", UI / "ae_weapon_slots.c", UI / "ae_weapon_registry.c"],
                                  ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the AE pause menu's rows: each game's, the host's, headers skipped)
    "ae_pause_rows_test.c": ([UI / "ae_pause_rows.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (CREATE GAME's BOTS row: a typed count's accepted text and the menu's limit)
    "ae_bot_settings_test.c": ([UI / "ae_bot_settings.c", UI / "ae_number_field.c", UI / "ae_setup_options.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (the scoreboard header's text, HUD STYLE AE)
    "ae_header_lines_test.c": ([UI / "ae_header_lines.c"],
                               ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (HUD STYLE AE's CE message lines: pieces on a pen, broken between words, pictures that do not fit)
    "ae_hud_message_flow_test.c": ([UI / "ae_hud_message_flow.c"],
                                   ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the HUD's key prompts on mouse + keyboard: action -> setting, the first key, its glyph or plain keycap)
    # (and the pad buttons' glyphs for each controller kind, and their sizes in both sheets)
    "ae_key_prompts_rules_test.c": ([UI / "ae_key_prompts_rules.c"],
                                    ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the map lists' pages: folders and their names, the order, the first of a name, the chooser; under the
    # sanitizers, as the folder names are a player's)
    "ae_map_folders_rules_test.c": ([UI / "ae_map_folders_rules.c"], ["-fsanitize=address,undefined",
                                                                       "-fno-sanitize-recover=all"], []),
    # (AE map layout files: the reader, every refusal, the limits; under the sanitizers, as the file is the map pack's)
    # (whose body a ragdoll is: a dead player's body still a player's, by the players' unit lists)
    "ae_ragdoll_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_bomb_snd_rules_test.c": ([], [], []),
    # (the announcer's rules: game.announcer "auto"'s pick, a key's takes)
    "ae_announcer_rules_test.c": ([UI / "ae_announcer_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_layout_rules_test.c": ([UI / "ae_layout_rules.c", UI / "ae_weapon_slots.c", UI / "ae_weapon_registry.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                               []),
    # (layouts travel with maps: the frames, a guest's inbox, which layout it plays and writes; under the sanitizers, as
    # the frames are a host's)
    "ae_layout_share_rules_test.c": ([UI / "ae_layout_share_rules.c", UI / "ae_layout_rules.c", UI / "ae_weapon_slots.c", UI / "ae_weapon_registry.c", UI / "ae_map_share_rules.c"],
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
    # (the objective log's format: levels, mode words, every event's keys pinned, the line builder)
    # (the match film's setting values, file names and budget)
    "ae_match_film_rules_test.c": ([UI / "ae_match_film_rules.c"], ["-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], []),
    "ae_objective_log_rules_test.c": ([UI / "ae_objective_log_rules.c"], ["-fsanitize=address,undefined",
                                      "-fno-sanitize-recover=all"], []),
    # (the score strip's mode choice per engine and gametype: a round mode is always the rounds strip)
    "ae_strip_mode_rules_test.c": ([UI / "ae_strip_mode_rules.c"], ["-fsanitize=address,undefined",
                                   "-fno-sanitize-recover=all"], []),
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
    # (which seats decide whether a pack's weapon can be held by its label: every one that holds weapons but the alert seat)
    "ae_weapon_seats_rules_test.c": ([UI / "ae_weapon_seats_rules.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (several weapon packs in one game: the gametype's slot mask, the loadout's sources, the join capability, map
    # spawns and the layout's pack weapons)
    # (the weapon cache's registry rows as the consumers read them: the 0.7.5 packs' bot, feed, medal and trigger pins, the
    # layout words, no empty column)
    "ae_weapon_registry_test.c": ([UI / "ae_weapon_registry.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_weapon_slots_test.c": ([UI / "ae_weapon_slots.c", UI / "ae_weapon_registry.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (0.7.6 the grenade catalogue: rows, the slot map, the join's layer version, the effects' numbers, the mask in the
    # START bytes)
    "ae_grenade_catalogue_test.c": ([UI / "ae_grenade_catalogue.c"], ["-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], []),
    # (0.7.6 AE's grenade message's entry: its bytes, and the entries no host writes refused)
    "ae_grenade_wire_rules_test.c": ([UI / "ae_grenade_wire_rules.c"], ["-fsanitize=address,undefined",
                                                                         "-fno-sanitize-recover=all"], []),
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
    # (the vehicle balance: the weapon classes of a damage effect, their hits, the shots to destroy)
    "ae_vehicle_balance_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_ghost_tune_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_fp_model_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the AE smoke grenade's volume: times, radius over the fade, the chord a line has inside the sphere)
    "ae_smoke_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (the second-generation gametype settings block: defaults, ranges, rules; under the sanitizers)
    "ae_variant_ext_rules_test.c": ([UI / "ae_variant_ext_rules.c"], ["-fsanitize=address,undefined",
                                                                        "-fno-sanitize-recover=all"], []),
    # (HIJACK: who may throw a driver out, the speed gate, mixed riders, the times; the block's byte)
    "ae_hijack_rules_test.c": ([UI / "ae_hijack_rules.c", UI / "ae_variant_ext_rules.c"],
                               ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (SHIELD BLEED: the leftover of a shield's pop, Halo CE's or without the shield's multiplier; the block's byte)
    "ae_shield_bleed_rules_test.c": ([UI / "ae_shield_bleed_rules.c", UI / "ae_variant_ext_rules.c"],
                                     ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (FLAG RETURN's modes: the time a loose flag lies, a tick's count, the touch; the block's byte)
    "ae_ctf_return_rules_test.c": ([UI / "ae_ctf_return_rules.c", UI / "ae_variant_ext_rules.c"],
                                   ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (MELEE LUNGE's presets, the lunge window and step, the host gate's speed; the block's byte)
    "ae_melee_lunge_rules_test.c": ([UI / "ae_melee_lunge_rules.c", UI / "ae_variant_ext_rules.c"],
                                    ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (SHOOTABLE GRENADES: the hit size and test, the pickup's kind, the host's checks of a report)
    "ae_grenade_shot_rules_test.c": ([UI / "ae_grenade_shot_rules.c"], ["-fsanitize=address,undefined",
                                                                         "-fno-sanitize-recover=all"], []),
    # (0.9.0 CLOCK SLEW: the median of the host's times, the error, the factor's bounds, the burst bound)
    "ae_clock_slew_rules_test.c": ([UI / "ae_clock_slew_rules.c"], ["-fsanitize=address,undefined",
                                                                     "-fno-sanitize-recover=all"], []),
    # (0.8.1 HIT COVER: a reported explosion's or melee blow's lines to the target, and the refusal rule)
    "ae_hit_cover_rules_test.c": ([UI / "ae_hit_cover_rules.c"], ["-fsanitize=address,undefined",
                                                                   "-fno-sanitize-recover=all"], ["-lm"]),
    # (the NEW MODES preset gametypes' table: names, ranges, a few presets' rules; under the sanitizers)
    "ae_h2_presets_test.c": ([UI / "ae_h2_presets_rules.c", UI / "ae_variant_ext_rules.c"],
                             ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (FALL DAMAGE's three values, HALO 2's bit in the gametype's byte, the velocities)
    "ae_fall_damage_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-lm"]),
    # (whose vehicle a placement is: the nearest CTF starting location's team, with Sidewinder's own starts)
    "ae_vehicle_sides_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the vehicle damage pack's look: the kinds by the tag's name, the pack's names, the damaged threshold)
    "ae_vehicle_pack_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (which config view of a weapon a game uses: START pair, then the set's table)
    "ae_cache_config_rules_test.c": ([UI / "ae_weapon_selection.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the cache's tag sets: how a set's tag is named for a lookup, prefixes aside)
    "ae_cache_sets_rules_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
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
    # (0.7.6 a hosted playlist: weights, order, the weighted pick, the vote with SAME AGAIN, the rotation file)
    "ae_playlist_test.c": ([UI / "ae_playlist.c"], [f"-I{UI}", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                           []),
    # (0.7.0 JOIN BY ADDRESS: the typed address, RECENT's list, the time words)
    # (0.7.1: which network interfaces are a VPN's, for CREATE GAME's warning; under the sanitizers)
    "ae_vpn_rules_test.c": ([SRC / "ae_vpn_rules.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                             "-fno-sanitize-recover=all"], []),
    # (0.7.5 the macOS download's packs/, voices/ and sounds/: where the application keeps them, which files are copied)
    "ae_extras_rules_test.c": ([SRC / "ae_extras_rules.c"], [f"-I{SRC}", "-fsanitize=address,undefined",
                                "-fno-sanitize-recover=all"], []),
    # (0.7.1: the host's line for a machine that joins without AE; under the sanitizers)
    # (0.7.5 CREATE GAME's HOST RULES rows: their words, the value a key's text is, what a pick writes)
    "ae_host_rules_test.c": ([UI / "ae_host_rules.c"], ["-fsanitize=address,undefined",
                                                        "-fno-sanitize-recover=all"], []),
    # (0.7.5 the join helper's words: the cause of each rejection code and failure reason, the one action each offers)
    "ae_join_helper_rules_test.c": ([UI / "ae_join_helper_rules.c", UI / "ae_silent_rules.c"], ["-fsanitize=address,undefined",
                                                                      "-fno-sanitize-recover=all"], []),
    # (0.7.9 the words of a removal: the title and sentence of each reason the server writes, the wait it names)
    "ae_removal_rules_test.c": ([UI / "ae_removal_rules.c"], ["-fsanitize=address,undefined",
                                                             "-fno-sanitize-recover=all"], []),
    "ae_joiner_notice_rules_test.c": ([UI / "ae_joiner_notice_rules.c"], ["-fsanitize=address,undefined",
                                       "-fno-sanitize-recover=all"], []),
    # (the second-generation match rules: rounds, ties, active players, slayer bonus points)
    "ae_h2_match_rules_test.c": ([UI / "ae_h2_match_rules.c"], ["-fsanitize=address,undefined",
                                  "-fno-sanitize-recover=all"], []),
    # (the round modes: the round machine's rules, the wipe judge, the match's end, revive times, the settings; the
    # danger zone's closing and damage; the round state's wire, its reader under garbage; under the sanitizers)
    "ae_round_rules_test.c": ([UI / "ae_round_rules.c", UI / "ae_variant_ext_rules.c"],
                              ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    "ae_round_zone_rules_test.c": ([UI / "ae_round_zone_rules.c"], ["-fsanitize=address,undefined",
                                   "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_revive_orb_rules_test.c": ([UI / "ae_revive_orb_rules.c"], ["-fsanitize=address,undefined",
                                   "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_round_wire_test.c": ([UI / "ae_round_wire.c"], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
                             []),
    # (0.7.9 the round card: verdict, reason, MVP, footer, fade, which builds read the END tail)
    "ae_round_card_rules_test.c": ([UI / "ae_round_card_rules.c"], ["-fsanitize=address,undefined",
                                   "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_revive_hud_rules_test.c": ([UI / "ae_revive_hud_rules.c"], ["-fsanitize=address,undefined",
                                   "-fno-sanitize-recover=all"], ["-lm"]),
    "ae_team_wait_rules_test.c": ([UI / "ae_team_wait_rules.c"], ["-fsanitize=address,undefined",
                                  "-fno-sanitize-recover=all"], []),
    # (0.7.7-preview2.4.2 which machine a datagram is from when machines share an address)
    "ae_net_flow_rules_test.c": ([UI / "ae_net_flow_rules.c"], ["-fsanitize=address,undefined",
                                                               "-fno-sanitize-recover=all"], []),
    # (0.7.9 a host's named sleep_forever on a map's script, sent to clients)
    "ae_script_sleep_rules_test.c": ([UI / "ae_script_sleep_rules.c"], ["-fsanitize=address,undefined",
                                                                       "-fno-sanitize-recover=all"], []),
    "ae_address_rules_test.c": ([UI / "ae_address_rules.c"], ["-fsanitize=address,undefined",
                                                             "-fno-sanitize-recover=all"], []),
    # (0.7.9 join race: a machine downloading the map is kept in the lobby while the transfer moves)
    "ae_join_hold_rules_test.c": ([UI / "ae_join_hold_rules.c"], ["-fsanitize=address,undefined",
                                                                 "-fno-sanitize-recover=all"], []),
    # (0.8.0 join diagnostics: the host's counts of what it heard from a machine, the late start's sends)
    "ae_join_diag_rules_test.c": ([UI / "ae_join_diag_rules.c"], ["-fsanitize=address,undefined",
                                                                 "-fno-sanitize-recover=all"], []),
    # (0.7.0 the name lookup thread's bookkeeping: a cancelled lookup's late answer is dropped)
    "ae_resolve_state_test.c": ([UI / "ae_resolve_state.c"], ["-fsanitize=address,undefined",
                                                              "-fno-sanitize-recover=all"], []),
    # (the lobby's host compatibility: the join's own test, halo_port_limits.h)
    "ae_lobby_compat_test.c": ([], [], []),
    "ae_updater_pick_test.c": ([], ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    # (the dialog, roster cards)
    # ((0.7.7) the dialog's chips: ae_widgets_pick.c's)
    "ae_widgets_dialog_test.c": ([UI / "ae_widgets_dialog.c", UI / "ae_widgets_text.c", UI / "ae_text_edit.c",
                                  UI / "ae_widgets_pick.c",
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


def test_medal_art_matches_the_medals_keys():
    """port/assets/medals: one 80 by 80 RGBA picture for each of AE's own medals, named by a key of ae_medal_ids.h
    (three_sixty is medal_360), embedded as raw pixels that agree with the PNG's."""
    import sys
    sys.path.insert(0, str(ROOT / "tools"))
    import ae_medal_art

    keys = set(re.findall(r'AE_MEDAL\(\w+, "medal_(\w+)"', (UI / "ae_medal_ids.h").read_text()))
    names = [path.stem for path in ae_medal_art.files()]
    assert len(names) == 31
    for name in names:
        assert ("360" if name == "three_sixty" else name) in keys, name
    pixels = ae_medal_art.rgba((ROOT / ae_medal_art.FOLDER / "bomber.png").read_bytes(), "bomber.png")
    assert len(pixels) == 80 * 80 * 4 and any(pixels[3::4])
    lines = ae_medal_art.embedded_lines(lambda data: ["\t" + str(len(data)) + ","], lambda data, name: (80, 80))
    assert lines.count("const struct ae_medal_art ae_medal_art[] =") == 1
    assert "const unsigned int ae_medal_art_count = 31;" in lines
    assert '\t{ "three_sixty", ae_medal_art%d, 80, 80 },' % names.index("three_sixty") in lines


def test_auto_picture_png_decodes_to_the_pixels(tmp_path):
    """The automatic map picture's PNG (ae_map_auto_picture_rules.c) is a real compressed PNG: zlib inflates its IDAT,
    undoing the row filters gives exactly the RGBA pixels the unit wrote beside it, for a 3D-like frame (far smaller than
    the 921,600 bytes of stored blocks) and for plain noise (no larger than the bound)."""
    import struct
    import zlib

    run_unit(build_unit("ae_map_auto_picture_rules_test.c", tmp_path), tmp_path)
    for name, limit in (("gradient", 300000), ("noise", 921600 + 4096)):
        png = (tmp_path / (name + ".png")).read_bytes()
        pixels = (tmp_path / (name + ".rgba")).read_bytes()
        assert png[:8] == b"\x89PNG\r\n\x1a\n"
        chunks, at = {}, 8
        while at < len(png):
            length, kind = struct.unpack(">I4s", png[at:at + 8])
            body = png[at + 8:at + 8 + length]
            assert zlib.crc32(kind + body) == struct.unpack(">I", png[at + 8 + length:at + 12 + length])[0]
            chunks.setdefault(kind, []).append(body)
            at += 12 + length
        assert [k for k in chunks] == [b"IHDR", b"IDAT", b"IEND"]
        assert struct.unpack(">IIBBBBB", chunks[b"IHDR"][0]) == (640, 360, 8, 6, 0, 0, 0)
        assert len(png) < limit, len(png)
        raw = zlib.decompress(chunks[b"IDAT"][0])
        stride = 640 * 4
        assert len(raw) == 360 * (stride + 1)
        previous = bytearray(stride)
        for row in range(360):
            line = raw[row * (stride + 1):(row + 1) * (stride + 1)]
            kind, data, out = line[0], line[1:], bytearray(stride)
            assert kind <= 4
            for i in range(stride):
                a = out[i - 4] if i >= 4 else 0
                b = previous[i]
                c = previous[i - 4] if i >= 4 else 0
                if kind == 0:
                    p = 0
                elif kind == 1:
                    p = a
                elif kind == 2:
                    p = b
                elif kind == 3:
                    p = (a + b) // 2
                else:
                    q = a + b - c
                    pa, pb, pc = abs(q - a), abs(q - b), abs(q - c)
                    p = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                out[i] = (data[i] + p) & 255
            assert bytes(out) == pixels[row * stride:(row + 1) * stride], (name, row)
            previous = out
