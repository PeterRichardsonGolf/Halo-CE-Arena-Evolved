"""The port's settings screens, which tools/ce_menus.py writes in place of the
PC version's (port/assets/menus/ce): laid out as its are (rows of a label and
a spinner over its row backgrounds, a line of help for the row chosen,
Defaults, OK and Cancel), with what this port has to set instead of what the
PC version had (no connection speed or texture quality: a window or the full
screen, the frame rate, the keyboard's own controls).

A row's spinner sets one of config.toml's settings (setting=, values=), or of
the profile being edited ("profile.<field>": the controller's settings), by
port/linux/game/menu_functions.c: "port setting load" shows its value,
"port settings save" (OK) writes those changed and applies them, "port
settings defaults" shows the defaults. Controls Setup binds the keyboard and
mouse's controls, by the functions the PC version names for it.
"""

import re
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

PE = "main_menu/settings_select/player_setup/player_profile_edit"
# the mods' screen, and its rows (_mods); the main menu's items
# (ce/main_menu.xml): MODS in Credits' place (Credits is left out:
# tools/ce_menus.py's LEFT_OUT), Quit a row below it (its CHILD_OFFSETS)
MODS = "main_menu/mods"
MOD_ROWS = 11
# (AE: seven rows with MENUS, 35 apart from 211, so that QUIT ends at 454, inside the 5% action-safe area: 456)
MODS_ITEM_PLACE = (192, 386)
QUIT_ITEM_PLACE = (192, 421)
YES_NO = [("YES", "true"), ("NO", "false")]
ON_OFF = [("ON", "true"), ("OFF", "false")]
# (display.performance's levels: main.c's performance_level_names)
PERFORMANCE_LEVELS = [("OFF", "off"), ("FPS", "fps"), ("MINIMAL", "minimal"), ("FULL", "full")]
CORNERS = [("TOP LEFT", "top_left"), ("TOP RIGHT", "top_right")]
HEALTH_STYLES = [("HALO CE", "classic"), ("REACH", "reach"), ("HALO 2", "halo2"), ("HALO 3", "halo3")]
MATCH_CLOCKS = [("OFF", "off"), ("TIME REMAINING", "down"), ("TIME ELAPSED", "up"), ("BOTH", "both")]
HUD_AREAS = [("FULL", "full"), ("16:9", "16:9"), ("4:3", "4:3")]
# (HUD STYLE: the score strip and AE's look, or the HUD exactly as before: ae_hud_style.c)
HUD_STYLES = [("AE", "ae"), ("CLASSIC", "classic")]
SCOREBOARD_FADES = [("INSTANT", "instant"), ("FAST", "fast"), ("NORMAL", "normal"), ("SLOW", "slow")]
CALLOUTS = [("OFF", "off"), ("ITEMS", "items"), ("ITEMS+CLOCK", "items_clock")]
SPAWN_HEATS = [("OFF", "off"), ("MINE", "mine"), ("ENEMY", "enemy")]
CALLOUT_DETAILS = [("MINIMAL", "minimal"), ("STANDARD", "standard"), ("VERBOSE", "verbose")]
# the multiplayer armor colors, in the profile's order (player_profile.c)
SHIELD_COLORS = ["white", "black", "red", "blue", "gray", "yellow", "green", "pink", "purple", "cyan", "cobalt",
                 "orange", "teal", "sage", "brown", "tan", "maroon", "salmon"]
SENSITIVITIES = [(f"{value:g}", f"{value:g}") for value in (0.1, 0.15, 0.25, 0.4, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4)]
# (the MCC mouse style: ae_mouse.c; the values 0.1 to 10 in 0.1 steps, the scales 0.1 to 2)
MOUSE_STYLES = [("CLASSIC", "classic"), ("MCC", "mcc")]
MCC_SENSITIVITIES = [(f"{step / 10:.1f}", f"{step / 10:.1f}") for step in range(1, 101)]
MCC_SCALES = [(f"{step / 10:.1f}", f"{step / 10:.1f}") for step in range(1, 21)]
# (the zoom action's modes: ae_zoom.c)
ZOOM_MODES = [("TOGGLE", "toggle"), ("HOLD", "hold"), ("BOTH", "both")]
ZOOM_RESETS = [("FIRST LEVEL", "first"), ("LAST USED", "last")]
# (a DualSense's trigger strength and lightbar: port/linux/src/ae_ds5_rules.c ae_ds5_strength_percent, ae_ds5.c)
TRIGGER_STRENGTHS = [("LIGHT", "light"), ("NORMAL", "normal"), ("STRONG", "strong")]
STICK_DEADZONES = [("DEFAULT", "0")] + [(f"{n}%", str(n)) for n in (5, 10, 15, 20, 25, 30, 35, 40)]
LIGHTBARS = [("OFF", "off"), ("PLAYER COLOR", "player_color"), ("SHIELD", "shield")]
# (the item waypoints' labels: hud_item_timers.c, ae_waypoint_icons.c)
WAYPOINT_STYLES = [("TEXT", "text"), ("ICONS", "icons"), ("BOTH", "both")]
WAYPOINT_SIZES = [(f"{n}%", str(n)) for n in range(50, 101, 5)]
VOLUMES = [(str(step), f"{step / 10:g}") for step in range(11)]

# each screen: its folder below PE, its screen's widget (the name the profile
# menu opens), its header (widget, bitmap), the row spacing, and its rows:
# (label, setting, [(shown, value)], help, platform[, key]): platform is None
# for every platform, else the platforms the row is for, separated by spaces
# ("desktop", "android", "web": the menus' platform attribute,
# port/linux/src/menu_files.c); key names the row's widgets where two rows set
# one setting (one for each platform), else the setting's name does
SCREENS = {
    "video_settings": {
        "screen": "video_settings_screen",
        "header": ("header_profile_video_settings", f"{PE}/video_settings/header_profile_video_settings"),
        # the other screens' 30px step. each platform packs its own rows, so
        # a row hidden on this machine does not leave a gap. (Arena Evolved:
        # 28, so that the desktop's ten rows with OpenCE build-244's EXTRA
        # EFFECTS end 11 above the help, which 30 ran into)
        "spacing": 28,
        "help_top": 364,
        "platform_places": True,
        # (rows in the place of the row before them: Window Size in
        # Resolution's, port/linux/game/menu_functions.c showing the one the
        # display mode chosen uses; Android's anti-aliasing in the desktop's)
        "same_place": ["display.window_size", "anti_aliasing_android"],
        "rows": [
            ("DISPLAY MODE:", "display.mode",
             [("FULLSCREEN", "fullscreen"), ("BORDERLESS", "borderless"), ("WINDOWED", "windowed")],
             "Fullscreen takes the display; borderless covers it\nwith a window. F11 switches to the window.",
             "desktop"),
            # (port/linux/game/menu_tags.c adds the display's resolutions)
            ("RESOLUTION:", "display.resolution", [("NATIVE", "native")],
             "What fullscreen and borderless draw at. Fullscreen\nsets the display to it; borderless scales it.",
             "desktop"),
            # (port/linux/game/menu_tags.c puts the sizes that fit the desktop
            # in place of this one)
            ("WINDOW SIZE:", "display.window_size", [("1280 x 960", "1280x960")],
             "The window's size: 4:3, then 16:10, 16:9 and 21:9\n(its edges can also be dragged).", "desktop"),
            ("RESOLUTION SCALING:", "display.resolution_scaling",
             [("NATIVE", "native"), ("75%", "75%"), ("50%", "50%"), ("25%", "25%"), ("ORIGINAL", "original")],
             "Native draws at the resolution, 75% to 25% that\npart each way; Original, the Xbox's 480 lines.", None),
            ("V-SYNC:", "display.vsync", ON_OFF,
             "Wait for the display between frames, so that the\npicture never tears.", None),
            ("FRAME RATE LIMIT:", "display.max_fps",
             [("AUTO", "0"), ("30", "30"), ("60", "60"), ("120", "120"), ("144", "144"), ("165", "165"),
              ("240", "240"), ("NONE", "-1")],
             "With V-Sync off, the most frames a second. Auto:\ntwice the display's refresh rate.", "desktop web"),
            ("SMOOTH MOTION:", "display.interpolation", ON_OFF,
             "Draw a frame for every display refresh, blending\nbetween the game's 30 ticks a second.", None),
            ("INSTANT AIM:", "display.direct_camera", ON_OFF,
             "In first person, turn the view the moment the\nmouse moves, not up to two ticks later.", "desktop web"),
            ("HIGH-RES HUD:", "display.high_res_hud", ON_OFF,
             "Draw the HUD from the high-res redraws; off\ndraws the game's own pictures.", None),
            ("BUTTON STYLE:", "display.hud_button_style",
             [("CLASSIC", "classic"), ("GEMS", "gems"), ("DARK GEMS", "menu_gems")],
             "Classic: plain Xbox buttons. Gems: shaded Xbox\nbuttons. Dark Gems: the menu's button artwork.", None),
            ("HIGH-RES TEXT:", "display.high_res_text", ON_OFF,
             "Draw text and titles with high-res fonts; off\ndraws the game's own.", None),
            ("ANTI-ALIASING:", "display.anti_aliasing",
             [("OFF", "off"), ("FXAA", "fxaa"), ("SMAA", "smaa"), ("SSAA 2X", "ssaa2x"), ("MSAA 2X", "msaa2x"),
              ("MSAA 4X", "msaa4x"), ("MSAA 8X", "msaa8x")],
             "Smooth jagged edges, which the Xbox did not. FXAA\nand SMAA are cheap; SSAA and MSAA are sharper.",
             "desktop"),
            ("ANTI-ALIASING:", "display.anti_aliasing",
             [("OFF", "off"), ("FXAA", "fxaa"), ("MSAA 2X", "msaa2x"), ("MSAA 4X", "msaa4x")],
             "Smooth jagged edges, which the Xbox did not. FXAA\nis cheap; MSAA is sharper.",
             "android web", "anti_aliasing_android"),
            # (AE: ae_texture_filter.c; the game never asks for it, so OFF is the stock look)
            ("ANISOTROPIC FILTERING:", "display.anisotropic",
             [("OFF", "off"), ("2X", "2x"), ("4X", "4x"), ("8X", "8x"), ("16X", "16x")],
             "Sharper floors and walls seen at a grazing angle.\nOff blurs them as the Xbox did.", None),
            ("SHADOW RESOLUTION:", "display.shadow_resolution",
             [("128", "128"), ("256", "256"), ("512", "512"), ("1024", "1024")],
             "The size objects' shadows are drawn at: 128 as on\nthe Xbox; larger for smoother, as soft, edges.",
             None),
            ("PER-PIXEL LIGHTING:", "display.per_pixel_lighting", ON_OFF,
             "Light models for each pixel, without the facets\nof the Xbox's lighting for each vertex.", None),
            # (OpenCE build-244's MODEL DETAIL, with the graphics rows; its OVERSHIELD with the shield glow's, on
            # Extra Effects)
            ("MODEL DETAIL:", "display.model_detail", [("STOCK", "stock"), ("NATIVE", "native"), ("HIGHEST", "highest")],
             "Stock picks models' detail by the Xbox's 480 lines;\nnative by this display's; highest, always the best.", None),
            ("PERFORMANCE:", "display.performance", PERFORMANCE_LEVELS,
             "FPS, with Minimal the frame time, with Full the\n1% low and the draws, at the top of the screen.",
             None),
            ("POSITION:", "display.performance_position", CORNERS,
             "The corner the performance line is drawn in,\nabove the HUD.", None),
            # (AE, for map makers: ae_coordinates.c)
            ("SHOW COORDINATES:", "display.coordinates", ON_OFF,
             "For map makers: your position (X Y Z) and facing\nat the end of the performance line.", None),
            ("OVERSHIELD:", "display.overshield_effect", ON_OFF,
             "The overshield rises up the body and stays lit,\nin its own color. Online: the host's choice.", None),
            ("SHIELD GLOW:", "display.shield_glow", ON_OFF,
             "Shields light their surroundings as they flare.\nOnline: the host's choice.", None),
            ("GLOW INTENSITY:", "display.shield_glow_intensity", [("DEFAULT", "default"), ("LIGHT SHOW", "light_show")],
             "Light Show: three times as far, at full from a\nthird of the flare. Online: the host's choice.", None),
            ("HDR:", "display.hdr", ON_OFF,
             "Keep the light of explosions, plasma and the sun\npast white, and roll it off instead of clipping.", None),
            ("HDR DISPLAY:", "display.hdr_output", ON_OFF,
             "With HDR, on an HDR display, show that light\nbrighter than white. From the next start.", "desktop"),
        ],
    },
    # (Game Options: its title text, not a picture; the spinner's modded discs
    # are the data root's discs/ folders (and earlier releases' mods/ ones),
    # listed as the menus are read: menu_files.c)
    "mods_setup": {
        "screen": "mods_settings_screen",
        "header": ("header_mods", None),
        "title": "GAME OPTIONS",
        # (ten rows: the HUD's rows are the HUD screen's, hud_setup)
        "spacing": 27,
        # (the spinners wider, from further left: "TIME REMAINING")
        "spinner": (300, 187),
        "rows": [
            ("MODDED DISCS:", "game.disc", [("STOCK", "")],
             "The maps played: a modded disc's replace the\nstock maps it has. OK starts the game again with it.",
             "desktop"),
            ("WEAPON PACKS:", "game.weapon_packs", ON_OFF,
             "The battle rifle and other packs in the packs\nfolder. A gametype's ITEM OPTIONS give them to\nplayers. A change applies from the next start.", None),
            ("CAMPAIGN FALL DAMAGE:", "game.fall_damage", ON_OFF,
             "Off: landings never hurt in the campaign. Gametypes\nhave their own, in Arena Options.", None),
            ("CAMPAIGN HEALTH:", "game.health", HEALTH_STYLES,
             "How health comes back in the campaign. Gametypes\nhave their own, in Arena Options.", None),
            ("CALLOUTS:", "game.callouts", CALLOUTS,
             "Spoken power item spawns, in any gametype; with\nthe clock, Halo 1: NHE's talking timer too.", None),
            # (a help for each value)
            ("CALLOUT DETAIL:", "game.callout_detail", CALLOUT_DETAILS,
             ["One short line for the items spawning together,\nand the first one up. No beeps.",
              "Items spawning together in one line where the\nvoice has one, the item beep and five to one.",
              "Every item spawning together named, then each\none up in turn."], None),
            # (the spinner's voices are the data root's voices/ folders and
            # the disc's discs/<disc>/voices/ ones, listed as the menus are
            # read: menu_files.c)
            ("VOICE:", "game.callout_voice", [("NHE", "nhe")],
             "The callouts' voice: a folder of voices/, next\nto maps/.", None),
            # (TRAINING's spawn markers' colours: spawn_heat.c; a help for
            # each value)
            ("SPAWN HEAT:", "display.spawn_heat", SPAWN_HEATS,
             ["TRAINING's spawn markers stay plain green.",
              "TRAINING's spawn markers glow where you would\nspawn next: hot is likely, dark red can't.",
              "TRAINING's spawn markers glow where the other\nteam would spawn next (in team games)."], None),
            # (a help for each value; the mouse's and the controller's zoom alike)
            ("ZOOM:", "input.zoom_mode", ZOOM_MODES,
             ["Each press of the zoom button zooms in a level,\nthen out, as Halo does.",
              "Zoomed while held: the wheel or the flashlight steps\nthe level; a tap then a hold starts at the second.",
              "Tap to toggle, hold to zoom while held: the wheel or\nthe flashlight steps the level of a hold."],
             None),
            # (a help for each value: where a hold starts, ae_zoom.c)
            ("ZOOM RESET:", "input.zoom_reset", ZOOM_RESETS,
             ["A hold of the zoom button starts at the weapon's\nfirst zoom level.",
              "A hold of the zoom button starts at the level you\nlast used on that weapon."],
             None),
        ],
    },
    # (the HUD's own screen, the profile menu's HUD after VIDEO SETUP: _hud_item. Its rows were Game Options')
    "hud_setup": {
        "screen": "hud_settings_screen",
        "header": ("header_hud", None),
        "title": "HUD",
        "spacing": 30,
        # (the spinners wider, from further left: "TIME REMAINING")
        "spinner": (300, 187),
        "rows": [
            # (first: it changes how the rows under it look; a help for each value)
            ("HUD STYLE:", "display.hud_style", HUD_STYLES,
             ["The score strip under the view, with the clock,\nand Arena Evolved's look for its own HUD.",
              "The HUD exactly as before: the clock in the\ncorner, no score strip."], None),
            ("HUD AREA:", "display.hud_area", HUD_AREAS,
             "On a wider screen, keep the HUD in a 16:9 or 4:3\npart at the middle; the view stays wide.", None),
            ("COMPACT HUD:", "display.compact_hud", ON_OFF,
             "With one player, draw split screen's smaller HUD:\nmeters, motion sensor and messages.", None),
            ("SCOREBOARD FADE:", "display.scoreboard_fade", SCOREBOARD_FADES,
             "How fast the scoreboard fades in and out while\nBack (or Tab) is held.", None),
            ("CAMPAIGN TIMER:", "display.campaign_timer", ON_OFF,
             "A clock of the time played on the level, in the\ncampaign's corner by the motion sensor.", None),
            # (a help for each value, as the Master Chief Collection's)
            ("MULTIPLAYER GAME TIMER:", "display.match_clock", MATCH_CLOCKS,
             ["No clock in a multiplayer game's corner.",
              "Display the time remaining until the game ends.",
              "Display the time elapsed since the game started.",
              "Display the time remaining, with the time\nelapsed above it."], None),
            ("SHORT MESSAGES:", "display.short_messages", ON_OFF,
             "Shorter game messages, as Halo 1: NHE's: a\nkilling spree reads Killing Spree!", None),
            # (the gametype's item waypoints' labels: a help for each value)
            ("WAYPOINTS:", "display.waypoint_style", WAYPOINT_STYLES,
             ["Item waypoints show the item's name and the time\nto its spawn.",
              "Item waypoints show the item's picture, with the\ntime to its spawn in small text beside it.",
              "Item waypoints show the item's name and time,\nthen its picture."], None),
            # (the labels' size: split screen smaller still, far items gently smaller)
            ("WAYPOINT SIZE:", "display.waypoint_size", WAYPOINT_SIZES,
             "How large the item waypoints' text and pictures are.\nSplit screen is smaller still; far items gently so.", None),
        ],
    },
    # (MCC's Custom Edition files, read in place: port/linux/src/ae_mcc_platform.c; the profile menu's MAP FILES,
    # after GAME OPTIONS: _map_files_item. BROWSE runs "ae mcc browse"; the status line under the rows is "ae mcc
    # status"'s)
    "map_files": {
        "screen": "map_files_screen",
        "header": ("header_map_files", None),
        "title": "MAP FILES",
        "spacing": 30,
        "rows": [
            # (a help for each value; ASK is the default: DEFAULTS goes back to it)
            ("MCC FILES:", "game.mcc_use", [("ASK", "ask"), ("ON", "yes"), ("OFF", "no")],
             ["AE asks before it reads MCC's files, when you\npick a Custom Edition map that needs them.",
              "Custom Edition maps read their bitmaps, sounds\nand text from your MCC folder; never changed.",
              "MCC's files are not read, and no map looks for\nthem. Custom Edition maps need their files in maps_ce."],
             None),
        ],
        "actions": [
            ("BROWSE FOR THE MCC FOLDER", "ae mcc browse",
             "Choose the Halo: The Master Chief Collection\nfolder yourself. The folder is kept at once."),
        ],
        "status": {"name": "mcc_status", "input": "ae mcc status", "left": 68, "top": 150, "width": 540,
                   "height": 80},
    },
    "mouse_settings": {
        "screen": "mouse_settings_screen",
        "header": ("header_profile_mouse_settings", f"{PE}/mouse_settings/header_profile_mouse_settings"),
        "spacing": 30,
        # (MOUSE STYLE's help is three lines)
        "help_top": 333,
        "help_height": 72,
        "rows": [
            # (a help for each value)
            ("MOUSE STYLE:", "input.mouse_style", MOUSE_STYLES,
             ["Classic: this port's own sensitivity numbers, the\nrows marked CLASSIC.",
              "MCC: the sensitivity number of Halo: The Master\nChief Collection. 1.0 turns 1/45 degree a count,\nabout 1% more than CS:GO at sensitivity 1."], None),
            ("HORIZONTAL SENSITIVITY:", "input.mouse_sensitivity", SENSITIVITIES,
             "CLASSIC style only. How fast the view turns side to\nside for the mouse's movement.", None),
            ("VERTICAL SENSITIVITY:", "input.mouse_vertical_sensitivity", [("SAME", "0")] + SENSITIVITIES,
             "CLASSIC style only. How fast the view turns up and\ndown; Same turns it as fast as side to side.", None),
            ("MOUSE SENSITIVITY:", "input.mouse_mcc_sensitivity", MCC_SENSITIVITIES,
             "MCC style only. Halo: MCC's number, for turning and\nlooking up and down alike.", None),
            ("ZOOMED SENSITIVITY SCALE:", "input.mouse_zoom_scale", MCC_SCALES,
             "MCC style only. A scale on the mouse while zoomed,\non top of the weapon's own zoom.", None),
            ("VEHICLE SENSITIVITY SCALE:", "input.mouse_vehicle_scale", MCC_SCALES,
             "MCC style only. A scale on the mouse while seated\nin a vehicle (any seat).", None),
            ("INVERT VERTICAL AXIS:", "input.invert_mouse", YES_NO,
             "Moving the mouse forward looks down.", None),
            ("AIM ASSIST:", "input.mouse_aim_assist", ON_OFF,
             "Slow and drag the view along with a target while\naiming with the mouse, as with a controller.", None),
        ],
    },
    "audio_settings": {
        "screen": "audio_settings_screen",
        "header": ("header_profile_audio_settings", f"{PE}/audio_settings/header_profile_audio_settings"),
        "spacing": 30,
        # (the devices, desktop only, first: Android's rows from the top)
        "platform_places": True,
        "rows": [
            # (port/linux/game/menu_tags.c adds the devices SDL finds)
            ("OUTPUT DEVICE:", "audio.output_device", [("SYSTEM DEFAULT", "default")],
             "Where the game's sound and the voices play.", "desktop"),
            ("INPUT DEVICE:", "audio.input_device", [("SYSTEM DEFAULT", "default")],
             "The microphone voice chat listens to.", "desktop"),
            ("MASTER VOLUME:", "audio.volume", VOLUMES, "The volume of everything.", None),
            ("MUSIC VOLUME:", "audio.music_volume", VOLUMES, "The music's volume.", None),
            ("EFFECTS VOLUME:", "audio.effects_volume", VOLUMES,
             "The volume of every other sound: effects and\nspeech.", None),
            ("REVERB:", "audio.reverb", ON_OFF,
             "Echo sounds as the place you are in does, and\nmuffle those behind walls, as the Xbox did.", None),
            ("SOUND:", "audio.enabled", ON_OFF,
             "Play sound at all; from the next time the game\nstarts.", None),
            # voice chat: this machine's own (port/linux/game/network_voice.c)
            ("VOICE CHAT:", "audio.voice_chat",
             [("PUSH TO TALK", "push_to_talk"), ("OPEN MIC", "open_mic"), ("OFF", "off")],
             "Talk in network games: while PUSH TO TALK is held\n(Controls), whenever you speak, or never.", None),
            ("VOICE VOLUME:", "audio.voice_volume", VOLUMES, "The other players' voices.", None),
        ],
    },
    "network_setup": {
        "screen": "network_settings_screen",
        "header": ("header_profile_network_settings", f"{PE}/network_setup/header_profile_network_settings"),
        # (22, not 30: twelve rows and MAP SHARING, with the help line as low
        # as mods_setup's; AE: 20, thirteen with SERVER PINGS; 19, fifteen
        # with ChupathingyCE 0.7.5b's MAP DOWNLOADS, the last above the help)
        "spacing": 19,
        "help_top": 368,
        # (the router, the clipboard and the updates, which the web build
        # has not: the other rows from the top there)
        "platform_places": True,
        "rows": [
            # (AE 0.3.5: the community services' master switch, the first-run opt-in's answer: OFF first, so that
            # "ask" (not yet answered, which is off) shows OFF and is written only when turned ON)
            ("COMMUNITY SERVICES:", "network.community_services", [("OFF", "no"), ("ON", "yes")],
             "Use the game list and stats ChupathingyCE runs\nat halo.milenko.org. OFF: nothing goes there.", None),
            ("INTERNET PLAY:", "network.online", ON_OFF,
             "Host and join games over the internet by invite\nlinks; off keeps to the local network.",
             "desktop android web"),
            # (opt-ins, off by default in this build: port_config.c)
            ("LIST HOSTED GAMES:", "network.list_hosted_games", ON_OFF,
             "Show the internet games you host on the public\nlist at halo.milenko.org, for anyone to join.", None),
            ("SHARE GAME STATS:", "network.report_joined_games", ON_OFF,
             "Send the scores of internet games you join to\nhalo.milenko.org, with your player ID.", None),
            # (AE: DamnationCE's pings, an opt-in, off by default: ae_net_pings.h)
            ("SERVER PINGS:", "network.browser_pings", ON_OFF,
             "Time each Server Browser game's ping (each\nlisted host then sees your address).", None),
            # (Delta Stats, ChupathingyCE 0.7.1's: on by default there)
            ("SHARE HOSTED STATS:", "network.report_events", ON_OFF,
             "Send the games you host (kills, positions, medals)\nto halo.milenko.org, for its match pages.", None),
            # (ChupathingyCE 0.8's privacy setting: a listed game's carnage report and Delta Stats, or neither)
            ("HOSTED GAME STATS:", "network.report_hosted_games", ON_OFF,
             "Send the scores and stats of the games you host\nto the game list. Off: they stay listed.", None),
            ("UPNP PORT FORWARDING:", "network.allow_upnp", ON_OFF,
             "Let internet play ask the router to forward its\nport, for networks that stop connections.",
             "desktop android"),
            # (ChupathingyCE 0.8's three words: invite_prompt.c asks)
            ("JOIN FROM CLIPBOARD:", "network.join_from_clipboard",
             [("ASK", "ask"), ("ALWAYS", "always"), ("NEVER", "never")],
             "An invite link copied before switching to the\ngame: ask to join its game, always join, or not.",
             "desktop android"),
            ("CHECK FOR UPDATES:", "update.auto", ON_OFF,
             "Look for a new version when the game starts.", "desktop android"),
            ("PLAYER NAMES:", "display.player_names",
             [("ALL", "all"), ("ALLIES", "allies"), ("ENEMIES", "enemies"), ("NONE", "none")],
             "In multiplayer, whose names are drawn above their\nheads.", None),
            ("PLAYER NAME SIZE:", "display.player_name_scale",
             [(f"{value:g}x", f"{value:g}") for value in (0.5, 0.75, 1, 1.25, 1.5, 2)],
             "How large the players' names are drawn.", None),
            # (MCC's ENEMY PLAYER NAME COLOR: the name under the reticle,
            # game_engine.c internal_rasterize_target_name, and the names over heads)
            ("ENEMY NAME COLOR:", "display.enemy_name_color", [("CLASSIC", "classic"), ("RED", "red")],
             ["Enemies' names, aimed at and over heads, in the\nHUD's blue, as teammates'.",
              "Enemies' names, aimed at and over heads, in red,\nas the Master Chief Collection's."], None),
            ("SCOREBOARD LAYOUT:", "display.scoreboard_team_layout", [("TEAMS", "teams"), ("BY SCORE", "score")],
             "A team game's scoreboard: a column for each team,\nor every player in order of score.", None),
            ("SCOREBOARD PANEL:", "display.scoreboard_background", ON_OFF,
             "Draw a panel behind the scoreboard, for clearer\ntext.", None),
            ("MAP DOWNLOADS:", "network.map_downloads", [("ASK", "ask"), ("ALWAYS", "always"), ("NEVER", "never")],
             "A Custom Edition map a game needs, from the\ncommunity map repository: ask first, or not.", None),
        ],
    },
    "gamepad_setup": {
        "screen": "gamepad_setup_screen",
        "header": ("header_profile_controller_setting", f"{PE}/controller_edit/header_profile_controller_setting"),
        # (22, not 30: twelve rows with the DualSense's three, the help line where it was; 19 with the two
        # STICK DEADZONE rows)
        "spacing": 19,
        "rows": [
            ("LOOK SENSITIVITY:", "profile.look_sensitivity", [(str(step), str(step)) for step in range(1, 11)],
             "How fast the right stick turns the view.", None),
            ("INVERT LOOK:", "profile.invert_look", YES_NO, "Pushing the right stick up looks down.", None),
            ("INVERT FLIGHT:", "profile.flight_inversion", YES_NO,
             "Flying vehicles pitch as aircraft do: pushing up\nflies down.", None),
            ("AUTO-CENTER LOOK:", "profile.autocenter", YES_NO,
             "Level the view while walking.", None),
            ("BUTTON LAYOUT:", "profile.button_preset",
             [("DEFAULT", "0"), ("SWAP TRIGGERS", "1"), ("SWAP A, L TRIGGER", "2"), ("SWAP B, L TRIGGER", "3"),
              ("SWAP B, R STICK", "4"),
              # (Anniversary's and OpenCE's, player_ui.c; OpenCE's "Recon" is the Master Chief
              # Collection's Hero, and Arena Evolved's own follow from 8)
              ("BUMPER JUMPER", "5"), ("FISHSTICKS", "6"), ("HERO", "7"), ("RECON", "8"), ("THE DUKE", "9"),
              ("ANNIVERSARY", "10"), ("RECLAIMER", "11"), ("ZOOM & SHOOT", "12"),
              ("BUMP & JUMP", "13"), ("GREEN FINGERS", "14")],
             "Which of the controller's buttons does what (not\nthe keyboard's: Controls Setup sets those).", None),
            ("STICK LAYOUT:", "profile.joystick_preset",
             [("DEFAULT", "0"), ("SOUTHPAW", "1"), ("LEGACY", "2"), ("LEGACY SOUTHPAW", "3")],
             "Which stick moves and which looks.", None),
            ("VIBRATION:", "profile.vibration", ON_OFF, "Rumble the controller.", None),
            ("IN-GAME HELP:", "profile.ingame_help", ON_OFF, "Show the game's hints about its controls.", None),
            # (AE: every pad's, this machine's settings; DEFAULT keeps the game's 27%: ae_stick_dead_zones.c)
            ("LEFT STICK DEADZONE:", "input.left_stick_deadzone", STICK_DEADZONES,
             "How far the left stick moves before you do, on\nevery controller. Default: the game's own, 27%.", None),
            ("RIGHT STICK DEADZONE:", "input.right_stick_deadzone", STICK_DEADZONES,
             "How far the right stick moves before the view\nturns, on every controller. Default: the game's 27%.",
             None),
            # (AE 0.3.0: a PS5 DualSense's, port/linux/src/ae_ds5.c; this machine's settings, every profile's)
            ("ADAPTIVE TRIGGERS:", "input.adaptive_triggers", ON_OFF,
             "A DualSense's triggers: a feel for each weapon\nand a kick at each shot. Other pads: nothing.",
             "desktop"),
            ("TRIGGER STRENGTH:", "input.trigger_strength", TRIGGER_STRENGTHS,
             "How strong a DualSense's trigger effects are.", "desktop"),
            ("LIGHTBAR:", "input.lightbar", LIGHTBARS,
             ["A DualSense's lightbar and player lights are\nleft alone.",
              "A DualSense's lightbar in your color, with the\npowerups' glows; its lights your player number.",
              "A DualSense's lightbar shows your shield as the\nHUD's meter does; its lights your player number."],
             "desktop"),
        ],
    },
}

# Video Setup's categories open ordinary settings screens. Each has the same
# pending edits, Defaults, OK and Cancel as the other settings screens.
_video = SCREENS["video_settings"]
# (Arena Evolved's PERFORMANCE and POSITION, the performance line's, go with
# the graphics rows, with upstream's shield glow and HUD BUTTON STYLE: nine
# places there, Video Setup's nine as OpenCE's)
_graphics = {"display.high_res_hud", "display.hud_button_style", "display.high_res_text", "display.anti_aliasing",
             "display.anisotropic",
             "display.shadow_resolution", "display.per_pixel_lighting", "display.model_detail",
             "display.performance", "display.performance_position", "display.coordinates"}
# (eleven rows with ANISOTROPIC FILTERING, SHOW COORDINATES, OpenCE's HUD
# BUTTON STYLE and MODEL DETAIL: 22 apart, as the twelve were before the shield
# glow's two went to Extra Effects. OpenCE's MATCH TIMER row is left out, and
# its clock is not drawn (hud.c): the owner keeps Arena Evolved's clock, the
# HUD screen's MULTIPLAYER GAME TIMER, display.match_clock)
# (the visual modifiers: a screen of their own, Extra Effects, as OpenCE's)
_extra_effects = {"display.shield_glow", "display.shield_glow_intensity",
                  "display.overshield_effect", "display.hdr", "display.hdr_output"}
SCREENS["video_settings/graphics"] = {
    "screen": "graphics_settings_screen", "header": _video["header"], "spacing": 22,
    "same_place": ["anti_aliasing_android"],
    "rows": [row for row in _video["rows"] if row[1] in _graphics],
}
SCREENS["video_settings/extra_effects"] = {
    "screen": "extra_effects_settings_screen", "header": _video["header"], "spacing": 30,
    "rows": [row for row in _video["rows"] if row[1] in _extra_effects],
}
_video["rows"] = [row for row in _video["rows"] if row[1] not in _graphics | _extra_effects]
_video["categories"] = [
    ("GRAPHICS:", "video_settings/graphics",
     "The HUD, text, anti-aliasing, shadows,\nlighting and the performance line."),
    ("FOV AND VIEWMODELS:", "video_settings/fov_viewmodels",
     "The field of view and the first-person\nweapon. Their defaults keep the stock view."),
    ("EXTRA EFFECTS:", "video_settings/extra_effects",
     "Visual modifiers the stock game does not\nhave, such as the shield glow."),
]
SCREENS["video_settings/fov_viewmodels"] = {
    "screen": "fov_viewmodel_settings_screen", "header": _video["header"], "spacing": 30,
    "rows": [
        ("FOV:", "display.fov", [("DEFAULT", "0")] + [(str(n), str(n)) for n in range(80, 151, 5)],
         "On foot, horizontal at 16:9, in degrees.\nDefault keeps the stock view.", None),
        ("VIEWMODEL FOV:", "display.viewmodel_fov", [("DEFAULT", "0")] + [(str(n), str(n)) for n in range(80, 151, 5)],
         "The weapon and hands, horizontal at 16:9.\nDefault keeps the weapon's stock view.", None),
        ("VIEWMODELS:", "display.viewmodel_visible", ON_OFF,
         "Draw first-person weapons, hands and attached\nvisuals. Gameplay and sound continue when off.", None),
        ("PASSENGER THIRD PERSON:", "display.passenger_third_person", ON_OFF,
         "Third-person Warthog and Scorpion passengers in\nthe campaign and co-op. Weapon zoom is disabled.", None),
        ("ARM SHIELDS:", "display.viewmodel_shield", ON_OFF,
         "The energy shield's flare on your first-person\narms too, as on your body.", None),
        # (the weapon HUD's screen effect's edge blur while zoomed: interface.c, ae_zoom_blur_allowed)
        ("ZOOM BLUR:", "display.zoom_blur", ON_OFF,
         "The blur at the screen's edges while zoomed in,\nas Halo's scopes have. Off: clear to the edges.", None),
        ("LEGS:", "display.first_person_legs", ON_OFF,
         "Your legs in first person, seen looking down,\nmoving as you run. Your body as others see it.", None),
        ("SHIELD COLOR:", "display.shield_color", [("DEFAULT", "default")] + [(c.upper(), c) for c in SHIELD_COLORS],
         "Your shield's flare in an armor color, whatever\nyour armor. Only you see it. Default: its own.", None),
    ],
}

# (AE) Game Options' BOTS (OpenCE PR #367's bots.count, bots.skill and bots.teams: port/linux/game/bots.c): a
# screen of its own, a category after the rows (as Video Setup's); Game Options' rows 24 px apart for its place
# (up to 31, as the AE menus' BOTS: the games you host take them all, mid-game too; a split screen game up to 15)
BOT_COUNTS = [("OFF", "0")] + [(str(n), str(n)) for n in range(1, 32)]
# (AE, owner 2026-10-20: DEMON, the LEGENDARY of before, above a new LEGENDARY: ae_bots_skill.inc)
BOT_SKILLS = [("RECRUIT", "recruit"), ("EASY", "easy"), ("NORMAL", "normal"), ("HEROIC", "heroic"), ("LEGENDARY", "legendary"),
              ("DEMON", "demon")]
BOT_SKILL_PARTS = [("BOT SKILL", "same")] + BOT_SKILLS
BOT_TEAMS = [("EVEN", "even"), ("AGAINST YOU", "against")]
BOT_PLAYFIGHT = [("OFF", "false"), ("ON", "true")]
_game_options = SCREENS["mods_setup"]
_game_options["spacing"] = 24
_game_options["categories"] = [
    ("BOTS:", "mods_setup/bots",
     "Computer players in every mode: split screen\ngames and games you host, LAN or internet."),
]
SCREENS["mods_setup/bots"] = {
    "screen": "bots_settings_screen", "header": ("header_bots", None), "title": "BOTS", "spacing": 30,
    "rows": [
        ("BOTS:", "bots.count", BOT_COUNTS,
         "How many bots, up to 31, play LAN or internet\ngames you host, mid-game too (split screen: 15).",
         None),
        ("BOT SKILL:", "bots.skill", BOT_SKILLS,
         "How soon the bots react, how well they aim and\nhow far they see.", None),
        # (AE: the training mode's separate skills, queue 120: ae_bots_combat.h)
        ("SHOOTING SKILL:", "bots.shooting_skill", BOT_SKILL_PARTS,
         "The bots' aim, reactions, grenades and blows:\nas BOT SKILL, or a level of their own.", None),
        ("MOVEMENT SKILL:", "bots.movement_skill", BOT_SKILL_PARTS,
         "The bots' strafing, jumping, crouching and\nretreating: as BOT SKILL, or a level of their own.", None),
        # (AE: playfighting)
        ("PLAYFIGHT:", "bots.playfight", BOT_PLAYFIGHT,
         ["The bots fight back.", "The bots move, strafe and aim at you, but\nnever shoot: aim practice."], None),
        # (a help for each value)
        ("BOT TEAMS:", "bots.teams", BOT_TEAMS,
         ["In team games, the bots on both teams, evening\nthem with the players.",
          "In team games, every bot on the team the\nplayers are not on."], None),
    ],
}
# Audio Setup's sound processing (port/linux/src/dsound_sdl.c): the reverb
# and the resampler, a screen of their own (a category, as Video Setup's);
# the devices and the volumes stay on Audio Setup
_audio = SCREENS["audio_settings"]
_processing = {"audio.reverb"}
SCREENS["audio_settings/sound_processing"] = {
    "screen": "sound_processing_settings_screen", "header": _audio["header"], "spacing": 30,
    "rows": [row for row in _audio["rows"] if row[1] in _processing] + [
        ("RESAMPLER:", "audio.resampler", [("SINC", "sinc"), ("LINEAR", "linear")],
         "How sounds are converted to the output's rate:\nsinc is clean; linear is brighter, with aliasing.",
         None),
    ],
}
_audio["rows"] = [row for row in _audio["rows"] if row[1] not in _processing]

# (voice chat stays on Audio Setup; the text chat's rows are the multiplayer
# HUD's, on Network Setup)
_audio["categories"] = [
    ("SOUND PROCESSING:", "audio_settings/sound_processing",
     "The reverb, and how sounds are converted to the\noutput's rate."),
]

# Network Setup's map sharing (port/linux/game/map_share.c): this machine's
# part in another's game, a screen of its own (a category, as Video Setup's)
SEED_UPLOADS = (64, 128, 256, 512, 1024, 2048, 4096, 8192, 10240)
_network = SCREENS["network_setup"]
# (and the multiplayer HUD's rows, and streamer mode, screens of their own:
# Network Setup has room for nine places)
_hud_settings = ("display.player_names", "display.player_name_scale", "display.enemy_name_color",
                 "display.scoreboard_team_layout", "display.scoreboard_background")
_hud_rows = [row for row in _network["rows"] if row[1] in _hud_settings]
_network["rows"] = [row for row in _network["rows"] if row[1] not in _hud_settings]
_network["categories"] = [
    ("MAP SHARING:", "network_setup/map_sharing",
     "Downloading maps you do not have from a host,\nand sending yours to other players.",
     "desktop android"),
    ("MULTIPLAYER HUD:", "network_setup/multiplayer_hud",
     "Players' names above their heads, the\nscoreboard, the text chat and the match clock."),
    ("STREAMER MODE:", "network_setup/streamer_mode",
     "Hide offensive words in names and text from\nother players, maps and mods, as stars."),
    ("SERVER BROWSER:", "network_setup/server_browser",
     "What the Server Browser lists and its order: the\nsame settings as its Filters screen."),
]
SCREENS["network_setup/map_sharing"] = {
    "screen": "map_sharing_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("DOWNLOAD MAPS:", "network.map_download", ON_OFF,
         "Get maps you do not have from the host when you\njoin. OFF: you cannot play games on those maps.",
         None),
        ("HELP SEND MAPS:", "network.map_seed", ON_OFF,
         "In another's game, send its map to players who\njoin without it, as a torrent's peers do.", None),
        ("UPLOAD LIMIT:", "network.map_seed_upload",
         [(f"{kb} KB/S" if kb < 1024 else f"{kb // 1024} MB/S", str(kb)) for kb in SEED_UPLOADS],
         "The most you send of maps to other players, so\nthat the game keeps the rest of your upload.", None),
        # (Arena Evolved: internet games share and download maps only with both switches on, the host's and the
        # player's; ae_map_share_all.c)
        ("INTERNET DOWNLOADS:", "network.map_download_internet", ON_OFF,
         "Also download maps from a host that is not on\nyour local network (an internet game).", None),
        ("INTERNET SHARING:", "network.map_share_internet", ON_OFF,
         "Hosting: also send your maps to players who are\nnot on your local network (an internet game).", None),
    ],
}

# Network Setup's server browser (menu_functions.c's lobby_browser_update): where
# its games come from (network.listings, master_list.c), the Filters screen's
# settings and its order; the master servers that answered, shown, not set
# (a row with no values: its value's text, menu_functions.c's
# master_servers_show)
SCREENS["network_setup/server_browser"] = {
    "screen": "server_browser_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("LISTS:", "network.listings",
         [("ALL GAMES", "both"), ("DEDICATED", "masters"), ("P2P GAMES", "brokers")],
         "The master servers' dedicated servers, the games\nplayers host (through the brokers), or all of them.", None),
        ("SHOW EMPTY SERVERS:", "network.browser_show_empty", YES_NO, "List games no one is playing yet.", None),
        ("SHOW FULL SERVERS:", "network.browser_show_full", YES_NO, "List games with no room left.", None),
        ("SHOW MISSING MAPS:", "network.browser_maps",
         [("NO", "no"), ("YES", "yes"), ("IF DOWNLOADABLE", "downloadable")],
         "List games on maps you do not have: IF\nDOWNLOADABLE, those Map Sharing can download.", None),
        # (a column title chosen again writes the order reversed, "-players": the row shows its order's word,
        # menu_functions.c's setting_value_index, and writes it again only if another is chosen)
        ("SORT BY:", "network.browser_sort",
         [("DEDICATED FIRST", "servers"), ("PLAYERS", "players"), ("NAME", "name"), ("MAP", "map"),
          ("GAME TYPE", "type"), ("LOCKED FIRST", "locked")],
         "The list's order. Its column titles sort it too;\nchoosing one again reverses it.", None),
        ("MASTER SERVERS:", "network.masters_file", None,
         "The master servers (masters.txt) that answered\nwhen the Server Browser last asked.", None,
         "master_servers"),
    ],
}
# (the read-only row's words, its screen's words: menu_functions.c's
# master_servers_show)
SCREENS["network_setup/server_browser"]["words"] = ["OF", "NONE LISTED", "NOT ASKED YET", "OFF"]

# Network Setup's multiplayer HUD: its rows, moved from Network Setup, and the
# text chat's (port/linux/game/network_chat.c)
SCREENS["network_setup/multiplayer_hud"] = {
    "screen": "multiplayer_hud_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": _hud_rows + [
        ("TEXT CHAT:", "display.text_chat", [("SHOW", "true"), ("HIDE", "false")],
         "SHOW the text chat, or HIDE it: no lines, and\nits keys and D-pad Up open nothing.", None),
        ("CHAT TEXT:", "display.chat_style", [("COLORS", "colors"), ("WHITE", "white")],
         "The text chat's colors: COLORS, names in their\nteam's color; WHITE, as Custom Edition.", None),
        ("MATCH TIMER:", "display.match_timer", ON_OFF,
         "Draw a clock at the bottom right in multiplayer,\ncounting up from the start of the game.", None),
    ],
}

# Network Setup's streamer mode (port/linux/game/text_filter.c): words hidden
# in what other players, maps and mods show, each group from a severity of
# Surge AI's on (port/assets/text_filter)
STREAMER_LEVELS = [("OFF", "off"), ("SEVERE", "severe"), ("STRONG AND WORSE", "strong"), ("MILD AND WORSE", "mild")]
SCREENS["network_setup/streamer_mode"] = {
    "screen": "streamer_mode_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("STREAMER MODE:", "streamer.enabled", ON_OFF,
         "Hide offensive words in names and text from\nother players, maps and mods, as stars.", None),
        ("CENSOR SLURS:", "streamer.slurs", STREAMER_LEVELS,
         "Slurs about race, sexuality, gender and\ndisability.", None),
        ("CENSOR SWEARING:", "streamer.swearing", STREAMER_LEVELS,
         "Swearing and sexual words (STRONG AND WORSE\nleaves the mildest showing).", None),
        ("CENSOR INSULTS:", "streamer.insults", STREAMER_LEVELS,
         "Name-calling and insults that are not slurs.", None),
    ],
}

# Network Setup in a game (the pause menu's SETTINGS: port/linux/game/
# menu_tags.c's pause_network_patch): its pages alone, without the rows that
# would drop the game (internet play) or are for the main menu (the router,
# the clipboard, the updates)
SCREENS["network_setup/in_game"] = {
    "screen": "in_game_network_settings_screen", "header": _network["header"], "spacing": 30,
    "platform_places": True, "rows": [],
    "categories": [category for category in _network["categories"] if category[1] != "network_setup/server_browser"],
}

# Controls Setup: the keyboard and mouse's actions, in groups (the order of
# port/linux/game/menu_functions.c's table of them)
CONTROL_GROUPS = ["MOVEMENT", "WEAPONS", "ACTIONS", "COMMUNICATION"]
CONTROL_ROWS = 7

# the settings whose rows have the wider spinner (a device's name)
WIDE_SETTINGS = {"audio.output_device", "audio.input_device", "streamer.slurs", "streamer.swearing",
                 "streamer.insults", "network.listings", "network.browser_maps", "network.browser_sort"}

# Teamplay Options' rows of the port's (_teamplay_options_extras): the
# gametype's FORCE TEAM SPAWNS, then the host's voice chat and vote kicks;
# each row's key, its label, its choices' words (menu_functions.c's
# gametype_options give their values) and their helps
TEAMPLAY_EDIT = "main_menu/settings_select/multiplayer_setup/teamplay_options_edit"
VOICE_QUALITIES = (8, 12, 16, 24, 32, 48, 64)
# (the rows' step: closer than the PC's 30px, so that all of Server Setup's
# fit above the help text, which moves down to TEAMPLAY_HELP_TOP)
TEAMPLAY_STEP = 26
TEAMPLAY_HELP_TOP = 351
# (game_engine.c's game_engine_force_team_spawns: the gametype's own, so
# shown in Edit Gametypes' copy too, and not on Server Setup's VOICE OPTIONS)
TEAMPLAY_GAMETYPE_ROWS = [
    ("force_team_spawns", "FORCE TEAM SPAWNS:", ["OFF", "ON"], [
        "Players will spawn at any spawn point the\\ngametype allows.",
        "Players will only spawn at their own team's\\nCapture the Flag spawn points.",
    ]),
]
TEAMPLAY_ROWS = [
    ("voice_mode", "VOICE CHAT:", ["OFF", "TEAM NEAR", "ANYONE NEAR", "TEAM", "TEAM, ENEMIES NEAR"], [
        "No voice chat during the game.",
        "Players hear their teammates who are near them.",
        "Players hear everyone who is near them.",
        "Players hear all their teammates, wherever they\\nare.",
        "Players hear all their teammates, and the enemies\\nwho are near them.",
    ]),
    ("voice_lobby", "LOBBY VOICE CHAT:", ["ON", "OFF"], [
        "Everyone hears everyone in the lobby, before and\\nafter the game.",
        "No voice chat in the lobby.",
    ]),
    ("voice_kbps", "VOICE QUALITY:", [f"{kbps} KBPS" for kbps in VOICE_QUALITIES],
     [f"Voices at {kbps} kilobits a second, in the lobby\\nand in the game." for kbps in VOICE_QUALITIES]),
    ("voice_proximity", "VOICE NEAR DISTANCE:", ["15 M", "30 M", "45 M", "60 M", "90 M", "150 M"],
     [f"Players {metres} metres apart or less are near\\nfor voice chat." for metres in (15, 30, 45, 60, 90, 150)]),
    # (network_votekick.c)
    ("votekick", "VOTE KICK:", ["ON", "OFF"], [
        "Players can vote to kick a player, from the\\nscoreboard.",
        "Players cannot vote to kick anyone.",
    ]),
    # (network_chat.c: one row for the chat and its links, as Teamplay
    # Options has room for one more)
    ("text_chat", "TEXT CHAT:", ["ON", "WITH LINKS", "OFF"], [
        "Players can type to each other; lines with links\\nare refused. Off in a public internet game.",
        "Players can type to each other, links too. Off\\nin a public internet game.",
        "No text chat in the game.",
    ]),
]
# (Teamplay Options' rows of the port's, in order)
TEAMPLAY_EXTRA_ROWS = TEAMPLAY_GAMETYPE_ROWS + TEAMPLAY_ROWS

# the profile menu's words for what its items now open
STRING_OVERRIDES = {
    f"{PE}/profile_edit_options": ["CHANGE NAME", "CONTROLS SETUP", "GAMEPADS", "MOUSE SETUP", "AUDIO SETUP",
                                   "VIDEO SETUP", "CHANGE COLOR", "SAVE CHANGES ", "NETWORK SETUP ", "ABOUT",
                                   "GAME OPTIONS", "MAP FILES", "HUD"],
    f"{PE}/profile_edit_descriptions": [
        "Rename this profile.\\n\\n\\nProfile:",
        "Choose the keys and mouse\\nbuttons for each action.\\n\\nProfile:",
        "Set this profile's controller\\nsensitivity and layouts.\\n\\nProfile:",
        "Adjust the mouse's sensitivity\\nand aiming.\\n\\nProfile:",
        "Adjust the volume of the music\\nand of everything else.\\n\\nProfile:",
        "Choose a window or the full\\nscreen, the frame rate and more.\\n\\nProfile:",
        "The HUD's area, clocks and\\nmessages, and the waypoints.\\n\\nProfile:",
        "Internet play, updates and the\\nmultiplayer HUD.\\n\\nProfile:",
        "Mods, and how the game plays:\\nfall damage and more.\\n\\nProfile:",
        "Custom Edition maps' files,\\nread from the MCC folder.\\n\\nProfile:",
        "Free-for-all color; in a match,\\nfrom the next game you join.\\n\\nProfile:",
        "Halo: Combat Evolved, the Xbox\\ngame, on this computer.\\n\\nProfile:",
    ],
}


def attributes(pairs: list) -> str:
    return "".join(f" {key}={quoteattr(str(value))}" for key, value in pairs if value is not None)


def _widget(name: str, pairs: list, inner: list) -> list:
    return [f"\t<widget{attributes([('name', name)] + pairs)}>", *[f"\t\t{line}" for line in inner], "\t</widget>"]


def _strings(name: str, texts: list) -> list:
    return [f"\t<strings{attributes([('name', name)])}>",
            *[f"\t\t<string{attributes([('text', text)])}/>" for text in texts], "\t</strings>"]


def _button(name: str, caption: int, handlers: list) -> list:
    pairs = [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
             ("string_list", "strings/common_button_captions"), ("string_index", caption), ("font", "ui\\small_ui"),
             ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)]
    return _widget(name, pairs, handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])


def _screen(folder: str, spec: dict, rows: list, list_inputs: list, list_handlers: list, extra: list) -> list:
    """a screen: its header, the list of rows (with the help line as its
    description), and the buttons"""
    base = folder if folder.startswith("main_menu/") else f"{PE}/{folder}"
    header, header_bitmap = spec["header"]
    lines = []
    lines += _widget(f"{base}/{spec['screen']}",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child pause_game"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      f'<child{attributes([("widget", f"{base}/{header}")])}/>',
                      f'<child{attributes([("widget", f"{base}/options_menu")])}/>',
                      *spec.get("screen_children", [])])
    header_pairs = [("controller", 1), ("left", 35), ("top", 11), ("width", 605), ("height", 59)]
    # (a screen with no title picture: its title in text)
    header_pairs += ([("type", "text"), ("text", spec["title"]), ("font", "ui\\large_ui"), ("color", "#FFFFFFFF"),
                      ("text_x", 30), ("text_y", 20)] if spec.get("title") else [("bitmap", header_bitmap)])
    lines += _widget(f"{base}/{header}", header_pairs, [])
    lines += _widget(f"{base}/help", [("type", "text"), ("controller", 1), ("left", 68),
                                      ("top", spec.get("help_top", 350)), ("width", 482),
                                      ("height", spec.get("help_height", 60)), ("string_list", f"{base}/help_strings"),
                                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    children = [f'<data input="{name}"/>' for name in list_inputs] + list_handlers
    # (each row: its widget, platform, and the place it is in, else the next)
    for index, (row, platform, *place) in enumerate(rows):
        y = 73 + (place[0] if place else index) * spec["spacing"]
        children.append(f'<child{attributes([("widget", row), ("x", 54), ("y", y), ("platform", platform)])}/>')
    children.append(f'<child{attributes([("widget", f"{base}/button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/help")], children)
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child{attributes([("widget", f"{base}/button_defaults"), ("y", 1)])}/>',
                      f'<child{attributes([("widget", f"{base}/button_ok"), ("x", 380), ("y", 1)])}/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    return lines


def _placed(row: str, platform, members: list, places: dict) -> list:
    """a row's children (row, platform, place): one for its platforms where
    they have it in the same place, else one for each place, with the
    platforms whose place it is"""
    groups = {}
    for name in members:
        groups.setdefault(places[name], []).append(name)
    if len(groups) == 1:
        return [(row, platform, places[members[0]])]
    return [(row, " ".join(names) if len(names) < len(places) else None, place) for place, names in groups.items()]


def _setting_screen(folder: str, spec: dict) -> list:
    base = f"{PE}/{folder}"
    rows, extra = [], []
    place = -1
    # (the spinners' place in their rows and width: the arrows just past
    # their ends)
    spinner_x, spinner_width = spec.get("spinner", (320, 147))
    # (platform_places: each platform's rows in places of their own, with
    # no gap where the others' are; a row in one place for all its
    # platforms, else a child for each place)
    places = {"desktop": -1, "android": -1, "web": -1}
    for index, (label, setting, choices, _, platform, *named) in enumerate(spec["rows"]):
        key = named[0] if named else setting.split(".", 1)[1]
        row = f"{base}/op_{key}"
        shares = setting in spec.get("same_place", ()) or key in spec.get("same_place", ())
        if spec.get("platform_places"):
            members = platform.split() if platform else list(places)
            for name in members:
                if not shares:
                    places[name] += 1
            rows += _placed(row, platform, members, places)
        else:
            if setting not in spec.get("same_place", ()) and key not in spec.get("same_place", ()):
                place += 1
            rows.append((row, platform, place))
        # (a row with no values shows its value, set by the game: its text)
        if choices is None:
            extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                   ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                             [f'<child{attributes([("widget", f"{base}/{key}_label")])}/>',
                              f'<child{attributes([("widget", f"{base}/{key}_value"), ("x", 286), ("y", 1)])}/>'])
            extra += _widget(f"{base}/{key}_label",
                             [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                              ("string_list", f"{base}/labels"), ("string_index", index), ("font", "ui\\large_ui"),
                              ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
            extra += _widget(f"{base}/{key}_value",
                             [("type", "text"), ("controller", 1), ("width", 212), ("height", 22),
                              ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                              ("text_flags", "no_focus_test")], [])
            continue
        # (a device's name wants the wider spinner Server Setup's co-op rows
        # have: WIDE_SETTINGS)
        wide = setting in WIDE_SETTINGS
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                         [f'<child{attributes([("widget", f"{base}/{key}_label")])}/>',
                          f'<child{attributes([("widget", f"{base}/{key}_spinner"), ("x", 286 if wide else spinner_x), ("y", 1)])}/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                          ("string_list", f"{base}/labels"), ("string_index", index), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        extra += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("left", None if wide else 3), ("top", 2),
                          ("width", 206 if wide else spinner_width),
                          ("height", 20), ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("strings", "|".join(shown for shown, _ in choices)), ("setting", setting),
                          ("values", "|".join(value for _, value in choices)), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4 if wide else 1),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7" if wide else "7 -6 19 0"),
                          ("footer_bounds", "7 208 19 214" if wide
                           else f"7 {spinner_width + 3} 19 {spinner_width + 9}")],
                         ['<on event="created" run="port setting load"/>'])
    # (each category's row: its label, the screen it opens, its help, and the
    # platforms it is for, as a row's: every one's when it names none)
    for index, (label, category_folder, _, *category_platform) in enumerate(spec.get("categories", ())):
        key = category_folder.rsplit("/", 1)[-1]
        row = f"{base}/op_{key}"
        target = f"{PE}/{category_folder}/{SCREENS[category_folder]['screen']}"
        platform = category_platform[0] if category_platform else None
        if spec.get("platform_places"):
            members = platform.split() if platform else list(places)
            for name in members:
                places[name] += 1
            rows += _placed(row, platform, members, places)
        else:
            rows.append((row, platform, place + 1 + index))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                         [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 512), ("height", 22),
                          ("string_list", f"{base}/labels"), ("string_index", len(spec["rows"]) + index),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    # (rows that run a function: a label, as a category's, after the categories)
    actions = spec.get("actions", ())
    for index, (label, run, _) in enumerate(actions):
        key = run.replace(" ", "_")
        row = f"{base}/op_{key}"
        rows.append((row, None, place + 1 + len(spec.get("categories", ())) + index))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" run="{run}"/>', f'<on event="start" run="{run}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 512), ("height", 22),
                          ("string_list", f"{base}/labels"),
                          ("string_index", len(spec["rows"]) + len(spec.get("categories", ())) + index),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    # (a line of text the game fills each frame, under the rows: its game data input's)
    status = spec.get("status")
    if status:
        extra += _widget(f"{base}/{status['name']}",
                         [("type", "text"), ("controller", 1), ("left", status["left"]), ("top", status["top"]),
                          ("width", status["width"]), ("height", status["height"]), ("font", "ui\\large_ui"),
                          ("color", "#FFFFFFFF"), ("text_flags", "no_focus_test")],
                         [f'<data input="{status["input"]}"/>'])
        spec = dict(spec, screen_children=[f'<child widget="{base}/{status["name"]}"/>'])
    extra += _button(f"{base}/button_defaults", 3, ['<on event="a" run="port settings defaults"/>',
                                                    '<on event="start" run="port settings defaults"/>'])
    extra += _button(f"{base}/button_ok", 1, ['<on event="a" run="port settings save" back="true"/>',
                                              '<on event="start" run="port settings save" back="true"/>'])
    categories = spec.get("categories", ())
    extra += _strings(f"{base}/labels", [label for label, *_ in spec["rows"]] + [label for label, *_ in categories] +
                      [label for label, *_ in actions])
    if spec.get("words"):
        extra += _strings(f"{base}/words", spec["words"])
    # (the help of the row whose label is string n is n + 1: the buttons' is
    # 0, and a category's follows the rows'. A row with a help for each value
    # has "@first" there, the string its first value's is, after the rows'
    # and the categories': menu_functions.c's settings_help)
    helps, value_helps = [""], []
    for _, _, choices, help_text, *_ in spec["rows"]:
        if isinstance(help_text, list):
            assert len(help_text) == len(choices)
            helps.append(f"@{1 + len(spec['rows']) + len(categories) + len(actions) + len(value_helps)}")
            value_helps += help_text
        else:
            helps.append(help_text)
    helps += [help_text for _, _, help_text, *_ in categories]
    helps += [help_text for _, _, help_text in actions]
    extra += _strings(f"{base}/help_strings", [text.replace("\n", "\\n") for text in helps + value_helps])
    return _screen(folder, spec, rows, ["port settings help"], [], extra)


def _controls_screen() -> list:
    folder = "controls_setup"
    base = f"{PE}/{folder}"
    spec = {"screen": "controls_settings_screen", "spacing": 30,
            "header": ("header_profile_controls_settings", f"{base}/header_profile_controls_settings")}
    rows, extra = [(f"{base}/op_group", None)], []
    extra += _widget(f"{base}/op_group", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                          ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child{attributes([("widget", f"{base}/group_label")])}/>',
                      f'<child{attributes([("widget", f"{base}/group_spinner"), ("x", 200), ("y", 1)])}/>'])
    extra += _widget(f"{base}/group_label",
                     [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("text", "SHOW:"),
                      ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/group_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 297), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "|".join(CONTROL_GROUPS)), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                      ("align", "center"), ("text_y", 1), ("header_bitmap", "bitmaps/arrow_sm_left"),
                      ("footer_bitmap", "bitmaps/arrow_sm_right"), ("header_bounds", "7 -6 19 0"),
                      ("footer_bounds", "7 300 19 306")], [])
    for index in range(CONTROL_ROWS):
        row = f"{base}/op_command_{index + 1}"
        rows.append((row, None))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         ['<on event="a" run="controls begin binding"/>',
                          '<on event="start" run="controls begin binding"/>',
                          '<on event="left right" run="controls binding slot"/>',
                          f'<child{attributes([("widget", f"{base}/command_label")])}/>',
                          f'<child{attributes([("widget", f"{base}/command_binding"), ("x", 200)])}/>',
                          f'<child{attributes([("widget", f"{base}/command_binding"), ("x", 356)])}/>'])
    extra += _widget(f"{base}/command_label",
                     [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/command_binding",
                     [("type", "text"), ("controller", 1), ("width", 152), ("height", 22), ("font", "ui\\small_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 5), ("text_flags", "no_focus_test")],
                     [])
    extra += _button(f"{base}/button_defaults", 3, ['<on event="a" run="controls screen defaults"/>',
                                                    '<on event="start" run="controls screen defaults"/>'])
    extra += _button(f"{base}/button_ok", 1, ['<on event="a" run="controls screen change set" back="true"/>',
                                              '<on event="start" run="controls screen change set" back="true"/>'])
    extra += _strings(f"{base}/help_strings", [
        "Choose what to show, with left and right.",
        "Enter sets the key chosen; left and right choose\\nwhich of two. Then press the new key or button.",
        "Press a key, a mouse button or turn the wheel...\\nEscape cancels, Delete clears it.",
    ])
    return _screen(folder, spec, rows, ["controls update menu"], ['<on event="created" run="controls screen init"/>'],
                   extra)


def _mods_item() -> list:
    """the profile menu's MODS, after NETWORK SETUP (WIDGET_PATCHES), opening
    the Mods screen"""
    screen = f"{PE}/mods_setup/mods_settings_screen"
    return _widget(f"{PE}/mods_profile_item",
                   [("type", "text"), ("left", 51), ("top", _profile_row_top("mods_profile_item")), ("width", 232),
                    ("height", PROFILE_ROW_HEIGHT),
                    ("bitmap", "bitmaps/list_item_bkd"), ("string_list", f"{PE}/profile_edit_options"),
                    ("string_index", 10), ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                    ("text_y", PROFILE_ROW_TEXT_Y), ("platform", "desktop")],
                   [f'<on event="a" open="{screen}"/>', f'<on event="start" open="{screen}"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>'])


# the profile menu's rows (PROFILE_ROWS, top to bottom), 27 px apart from 78 so that its twelve (MAP FILES after
# GAME OPTIONS, HUD after VIDEO SETUP) fit above the buttons (the PC version's nine were 33 px apart;
# WIDGET_PATCHES sets their tops, heights and their text's place). A row's description is the string at its
# place here (profile_edit_descriptions); its picture goes by its name (ae_glue_settings.c)
PROFILE_ROWS = ["name_profile_item", "controller_setup_profile_item", "gamepads_profile_item",
                "mouse_settings_profile_item", "audio_settings_profile_item", "video_settings_profile_item",
                "hud_profile_item", "network_settings_profile_item", "mods_profile_item", "map_files_profile_item",
                "color_profile_item", "about_item"]
PROFILE_ROW_TOP, PROFILE_ROW_STEP, PROFILE_ROW_HEIGHT, PROFILE_ROW_TEXT_Y = 78, 27, 27, 4
# (the rows this port adds, each a function's below)
PROFILE_OWN_ROWS = ("mods_profile_item", "map_files_profile_item", "hud_profile_item")


def _profile_row_top(item: str) -> int:
    return PROFILE_ROW_TOP + PROFILE_ROWS.index(item) * PROFILE_ROW_STEP


def _map_files_item() -> list:
    """the profile menu's MAP FILES, after GAME OPTIONS (WIDGET_PATCHES), opening the MAP FILES screen"""
    screen = f"{PE}/map_files/map_files_screen"
    return _widget(f"{PE}/map_files_profile_item",
                   [("type", "text"), ("left", 51), ("top", _profile_row_top("map_files_profile_item")),
                    ("width", 232), ("height", PROFILE_ROW_HEIGHT),
                    ("bitmap", "bitmaps/list_item_bkd"), ("string_list", f"{PE}/profile_edit_options"),
                    ("string_index", 11), ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                    ("text_y", PROFILE_ROW_TEXT_Y), ("platform", "desktop")],
                   [f'<on event="a" open="{screen}"/>', f'<on event="start" open="{screen}"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>'])


def _hud_item() -> list:
    """the profile menu's HUD, after VIDEO SETUP (WIDGET_PATCHES), opening the HUD screen"""
    screen = f"{PE}/hud_setup/hud_settings_screen"
    return _widget(f"{PE}/hud_profile_item",
                   [("type", "text"), ("left", 51), ("top", _profile_row_top("hud_profile_item")),
                    ("width", 232), ("height", PROFILE_ROW_HEIGHT),
                    ("bitmap", "bitmaps/list_item_bkd"), ("string_list", f"{PE}/profile_edit_options"),
                    ("string_index", 12), ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                    ("text_y", PROFILE_ROW_TEXT_Y), ("platform", "desktop")],
                   [f'<on event="a" open="{screen}"/>', f'<on event="start" open="{screen}"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>'])


def settings_files() -> dict:
    """the files of the port's screens, by their names in ce/"""
    files = {}
    for folder, spec in SCREENS.items():
        files[f"{PE}/{folder}".replace("/", ".") + ".xml"] = _setting_screen(folder, spec) + (
            _mods_item() if folder == "mods_setup" else _map_files_item() if folder == "map_files" else
            _hud_item() if folder == "hud_setup" else [])
    files[f"{PE}/controls_setup".replace("/", ".") + ".xml"] = _controls_screen()
    return {name: ['<?xml version="1.0" encoding="UTF-8"?>',
                   "<!-- The port's settings screen, in the PC version's style (tools/port_settings.py) -->",
                   "<menus>", *lines, "</menus>", ""] for name, lines in files.items()}


def replaced(folder: str) -> bool:
    """whether the PC version's folder of widgets is replaced by ours (and
    those below it: the controls' advanced screen, the video test)"""
    return any(folder == path or folder.startswith(f"{path}/")
               for path in [*(f"{PE}/{name}" for name in [*SCREENS, "controls_setup"]), *REPLACED_FOLDERS])


# ---------- multiplayer (PLAN.md section 11)

MT = "main_menu/multiplayer_type_select"

# strings added to the PC version's lists: (at, strings) of each list. Slayer's
# kills to win goes up to 500, for big games (the Xbox editor's are
# source/interface/ui_widget.c's kills_to_win_extra_strings): the values,
# and their helps after the five of its own (its helps are the rows' values'
# in turn: menu_functions.c's gametype_option_help)
SLAYER_EDIT = "main_menu/settings_select/multiplayer_setup/playlist_edit/slayer_edit"
PLAYLIST_EDIT = "main_menu/settings_select/multiplayer_setup/playlist_edit"
ARENA_OPTIONS = "main_menu/settings_select/multiplayer_setup/arena_options_edit"
NHE_OPTIONS = "main_menu/settings_select/multiplayer_setup/nhe_options_edit"
INDICATOR_OPTIONS = "main_menu/settings_select/multiplayer_setup/indicator_options_edit"
PLAYER_OPTIONS = "main_menu/settings_select/multiplayer_setup/player_options_edit"
PLAYER_EDIT = PLAYER_OPTIONS
# (its rows, the PC version's, in order: ce_menus.py moves them 27 apart)
PLAYER_OPTIONS_ROWS = ['number_of_lives', 'maximum_health', 'shields', 'respawn_time', 'respawn_time_growth', 'odd_man_out', 'invisible_players', 'suicide_penalty']
# the vehicle sets' PC set (game_engine.h's VARIANT_VEHICLE_SET_PC), between
# GUN TURRETS and CUSTOM (menu_functions.c's VEHICLE_PRESET_PC), and its help
# among the sets' (its rows' values' in turn: gametype_option_help)
VEHICLE_EDIT = "main_menu/settings_select/multiplayer_setup/vehicle_options_edit"
AIM_ASSIST_HELP = [
    "Players keep their current mouse aim assist setting.",
    "Disable mouse aim assist for mouse/keyboard players.\\nController and touch aim assist are unchanged.",
]
# the host's shield effects, for every player of a network game
# (network_shield_effects.c): their words, then their helps
SHIELD_EFFECTS = ['STOCK', 'GLOW', 'LIGHT SHOW', 'OVERSHIELD', 'GLOW+OVERSHIELD', 'SHOW+OVERSHIELD',
                  "PLAYERS' OWN"]
SHIELD_EFFECTS_HELP = [
    "Everyone sees the stock game's shields and\\novershield, whatever their own settings.",
    "Everyone's shields light their surroundings as\\nthey flare.",
    "Everyone's shields light their surroundings as\\nthey flare, three times as far (Light Show).",
    "For everyone, the overshield rises up the body\\nand stays lit, in its own color.",
    "For everyone, shields light their surroundings,\\nand the overshield rises and stays lit.",
    "For everyone, Light Show's shield glow, and the\\novershield rising and staying lit.",
    "Not synced: each player sees the shield effects\\nof their own Video Setup settings.",
]

# OpenCE's gametypes' options (port/linux/game/game_modes.c), a screen of the
# gametype editor's (menu_functions.c's "mp profile init opence options"): the
# mode and Hardcore, then the mode's own rows, which menu_functions.c's
# gametype_option_help shows (op_inf_ for Infection, op_gg_ for Gun Game)
# below them; a row's values are menu_functions.c's gametype_options'
PLAYLIST_EDIT = "main_menu/settings_select/multiplayer_setup/playlist_edit"
OPENCE_EDIT = "main_menu/settings_select/multiplayer_setup/opence_options_edit"
OPENCE_ROWS = [
    ("oce_mode", "MODE:", ["NONE", "SWAT", "INFECTION", "GUN GAME"], [
        "The game type's own rules.",
        "No shields, and a shot to the head kills. Slayer only.",
        "Humans killed join the zombies, who win when\\nno human is left. Team Slayer only.",
        "Each kill gives the next weapon; a kill with the\\nlast wins. Free for all Slayer only."]),
    ("oce_hardcore", "HARDCORE:", ["OFF", "ON"], [
        "The motion sensor and aim assist as the game\\ntype has them.",
        "No motion sensor, no aim assist, and a kill\\nrestores the killer's health and shields."]),
    ("inf_zombies", "FIRST ZOMBIES:", ["1", "2", "3", "4", "8", "16"],
     ["The zombies chosen when the round starts\\n(a human is always left)."] * 6),
    ("inf_speed", "ZOMBIE SPEED:", ["100%", "110%", "125%", "150%", "175%", "200%"],
     ["How fast zombies move, of a human's speed."] * 6),
    ("inf_damage", "SKULL DAMAGE:", ["100%", "150%", "200%", "250%"],
     ["The damage of a zombie's skull's melee."] * 4),
    ("inf_respawn", "ZOMBIE RESPAWN:", ["1 SECOND", "3 SECONDS", "5 SECONDS", "10 SECONDS", "15 SECONDS",
                                        "30 SECONDS"],
     ["How long a dead zombie waits to respawn."] * 6),
    ("inf_countdown", "COUNTDOWN:", ["NONE", "5 SECONDS", "10 SECONDS", "15 SECONDS", "30 SECONDS"],
     ["How long the round waits, once two players are\\nalive, before the first zombies are chosen."] * 5),
    ("inf_shields", "ZOMBIE SHIELDS:", ["OFF", "ON"],
     ["Zombies have no shields.", "Zombies have shields, as humans do."]),
    ("gg_order", "WEAPON ORDER:", ["STRONGEST FIRST", "WEAKEST FIRST", "RANDOM"], [
        "From the rocket launcher to the pistol.", "From the pistol to the rocket launcher.",
        "In an order the host draws for each game."]),
    ("gg_melee", "MELEE DEMOTES:", ["OFF", "ON"], [
        "A melee kill takes nothing from the victim.",
        "A melee kill takes the victim back a weapon."]),
]
OPENCE_ITEM = f"{PLAYLIST_EDIT}/playlist_edit_opence_item"

STRING_INSERTS = {
    # (Player Options' RESPAWN TIME: Halo 1: NHE's 7.5 seconds, 225 ticks, after
    # 5 SECONDS, its help after that one's: menu_functions.c's respawn_time_spinner;
    # then OpenCE build-244's AIM ASSIST, the ninth row, and OpenCE's SHIELD EFFECTS,
    # the tenth: their helps after the eight rows' 29, with the respawn time's extra one)
    f"{PLAYER_OPTIONS}/var_respawn_time": [(2, ["7.5 SECONDS"])],
    f"{PLAYER_OPTIONS}/cap_player_options": [(14, [
        "When you die, there will be a seven and a half\\nsecond wait before you can rejoin the game, as in\\nHalo 1: NHE's objective gametypes.",
    ]), (29, AIM_ASSIST_HELP + SHIELD_EFFECTS_HELP)],
    # (Filters' SHOW MISSING MAPS' third value's help, after the PC version's:
    # menu_functions.c's filters_help)
    f"{MT}/join_game/filters/cap_filter_options": [(11, [
        "Show servers on maps you do not have if their\\nhost shares them: they download as you join."])],
    f"{PLAYER_EDIT}/player_options_labels": [(8, ["AIM ASSIST:", "SHIELD EFFECTS:"])],
    # (Teamplay Options' voice chat and vote kick rows, after its own:
    # TEAMPLAY_ROWS)
    f"{TEAMPLAY_EDIT}/teamplay_options_labels": [(3, [label for _, label, *_ in TEAMPLAY_EXTRA_ROWS])],
    f"{TEAMPLAY_EDIT}/cap_teamplay_options": [(10, [text for *_, helps in TEAMPLAY_EXTRA_ROWS for text in helps])],
    f"{SLAYER_EDIT}/var_kills_to_win": [(5, ["75", "100", "150", "200", "250", "500"])],
    f"{VEHICLE_EDIT}/var_vehicle_set": [(8, ["PC"])],
    f"{VEHICLE_EDIT}/cap_vehicle_options": [(17, [
        "Every vehicle the map places will spawn, as in\\nHalo PC.",
    ])],
    f"{SLAYER_EDIT}/cap_slayer": [(11, [
        "Seventy-five kills to win. Settle in for a long\\nfight.",
        "A hundred kills to win. Made for big games.",
        "A hundred and fifty kills to win. Only a crowded\\nserver gets there.",
        "Two hundred kills to win. Bring friends. Lots of\\nthem.",
        "Two hundred and fifty kills to win.",
        "Five hundred kills to win. You'll be here a while.",
    ])],
    # (the gametype editor's ARENA OPTIONS, NHE OPTIONS and OpenCE's OPENCE
    # OPTIONS, after TEAMPLAY OPTIONS: their items' captions, and their
    # descriptions before SAVE CHANGES' (the list's descriptions go by the
    # items' places): _arena_options, _nhe_options, _opence_options)
    f"{PLAYLIST_EDIT}/playlist_edit_options": [(8, ["ARENA OPTIONS", "NHE OPTIONS"])],
    f"{PLAYLIST_EDIT}/playlist_edit_list_extended_dsc_text": [(7, [
        "Time limit, fall damage, health\\nand Arena Evolved's competitive\\noptions for this gametype.\\nThis gametype:",
        "Halo 1: NHE's map mode and\\ncompetitive extras for this\\ngametype.\\nThis gametype:",
        # (OpenCE's OPENCE OPTIONS, the third: _opence_options)
        "Choose the rules of OpenCE's\\ngametypes: SWAT, Infection,\\nGun Game and Hardcore.\\nThis gametype:",
    ])],
    # (Indicator Options' TIMERS, TRAINING and SPAWN HEAT, after FRIEND
    # INDICATORS ON SCREEN: _indicator_options_extras)
    f"{INDICATOR_OPTIONS}/indicator_options_labels": [(4, ["TIMERS:", "TRAINING:", "SPAWN HEAT:"])],
    # (the objectives indicator's LINE OF SIGHT, after NONE: _option_goal_radar)
    f"{INDICATOR_OPTIONS}/var_radar display": [(3, ["LINE OF SIGHT"])],
    # (OTHER PLAYERS ON RADAR's ENEMIES, between FRIENDS and NO: the row's values in menu_functions.c, stored 3,
    # ae_radar.c; its help after FRIENDS')
    f"{INDICATOR_OPTIONS}/var_others_on_radar": [(2, ["ENEMIES"])],
    f"{INDICATOR_OPTIONS}/cap_indicator_options": [(3, [
        "The game's objectives will have special markers\\ndisplayed over them on screen, only while you\\ncan see them.",
    ]), (6, [
        "Only enemies will show up on your Motion Tracker\\nwhen mobile, never your teammates. In a free for\\nall, every other player.",
    ]), (10, [
        "No timers: learn the spawns the hard way.",
        "When the power items (rockets, sniper, shotgun,\\novershield, camo) spawn, in a list for\\neveryone.",
        "The power items' list, and waypoints over\\ntheir spots from 10 seconds before they\\nspawn, seen through walls.",
        "Waypoints over the power items' spots only\\nwhile you can see them, from 10 seconds before\\nthey spawn. No list, no item calls.",
        "No training aids.",
        "Waypoints over the power items as they come up,\\nand the player spawns. For learning a map.",
        "Spawn markers show where players are likely\\nto spawn next (with TRAINING). Each player picks\\nMINE or ENEMY in Settings.",
        "Plain spawn markers for everyone.",
    ])],
}


# a custom loadout's weapons (game_engine.h's _loadout_weapon_*)
LOADOUT_WEAPONS = ["NONE", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN", "SNIPER RIFLE", "ROCKET LAUNCHER",
                   "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER"]
# (after game_engine.h's own: the weapon pack's rifle, a choice the rows' spinners show last; ae_rifle_menu.c stores
# it as the slot's pack bit over an assault rifle, the weapon played where the pack is not)
LOADOUT_PACK_WEAPON = "BATTLE RIFLE"


def LOADOUT_HELPS(slot: str) -> list:
    """the helps of a custom loadout's weapon's values"""
    helps = [f"No {slot} weapon.", f"A random {slot} weapon, of those the map has,\\neach time you spawn."]
    for weapon in LOADOUT_WEAPONS[2:]:
        helps.append(f"Everyone spawns with a {weapon.lower()} as their {slot}\\nweapon (where the map has one).")
    helps.append(f"Everyone spawns with a battle rifle as their {slot}\\nweapon (a weapon pack: an assault rifle where\\nthe pack is not). Arena Evolved 0.2.5+ only.")
    return helps


STRING_OVERRIDES.update({
    f"{MT}/multiplayer_options": ["JOIN GAME", "CREATE GAME", "INTERNET", "LAN", "DIRECT LINK", "EDIT GAMETYPES",
                                  "SERVER BROWSER", "CO-OP CAMPAIGN", "SPLIT SCREEN"],
    f"{MT}/multiplayer_option_descriptions": [
        "Browse the games on the\\nInternet.",
        "Join a multiplayer game on\\nyour LAN.",
        "Join a game by its invite link,\\nor one a Discord invite\\nreached.",
        "Host a game on the Internet:\\nplayers join by its invite\\nlink or Discord.",
        "Host a game on your LAN\\nonly.",
        "Up to four players on this\\nPC in split screen, and up\\nto 15 bots.",
        "Play the campaign with a\\nfriend in split screen:\\nplayer 2 on a gamepad.",
        "Set all the attributes for your \\nmultiplayer gametypes and\\nkeep them for future use.",
    ],
    # (and the port's OPENCE: OpenCE's gametypes, port/linux/game/game_modes.c)
    "main_menu/gametype_select/var_gametype_banks": ["STANDARD", "OPENCE", "CUSTOM"],
    # (the PC's weapon sets, then the Xbox's NO GRENADES: menu_functions.c's
    # gametype_options maps them to the engine's)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_labels": ["INFINITE GRENADES:", "WEAPON SET:", "VEHICLE SET:", "STARTING EQUIPMENT:",
                                  "VEHICLE RESPAWN TIME:", "LOADOUT:", "PRIMARY WEAPON:", "SECONDARY WEAPON:",
                                  "MAP WEAPONS:"],
    # (the helps of each option's values in turn: menu_functions.c's
    # gametype_option_help)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/cap_item_options": [
        "Everyone has a limitless supply of grenades, which\\nmakes for loud, explosive and occasionally messy fun.",
        "Each player can only carry up to 4 grenades of each\\ntype at any given time.",
        "The map will contain whatever weapons the designers\\nplaced on it.",
        "All the weapons on the map will be replaced by\\npistols.",
        "All the weapons on the map will be replaced by\\nassault rifles and plasma rifles.",
        "All the weapons on the map will be replaced by\\nplasma weapons.",
        "The guns on the map will be replaced by weapons\\nwith sniper scopes.",
        "Sniper rifles and pistols will not appear on the map.",
        "All the weapons on the map will be replaced by\\nrocket launchers.",
        "All the weapons on the map will be replaced by\\nshotguns.",
        "All the weapons on the map are only effective at\\nshort ranges.",
        "There are no weapons on the map that were made in\\nCovenant sweat shops.",
        "Only weapons made by the Covenant will appear\\nin the map.",
        "The flamethrower and fuel rod gun will not appear\\nin the map.",
        "Only the most devastating and dangerous of weapons\\nwill appear in the map.",
        "The map's weapons, without any grenades.",
        "You will start the game with the weapon set that the \\nmap designers custom tailored for it.",
        "Use the generic starting weapons across all maps.",
        "The map's weapons appear where its designers\\nplaced them.",
        "No weapons appear on the map: with a loadout of\\nNONE and NONE, melee and grenades only.",
        "The weapons follow the weapon set above.",
        "Everyone starts with the primary and secondary\\nweapons below; the map's weapons are its own\\n(in AE's gametypes, the weapon set's).",
        *LOADOUT_HELPS("primary"),
        *LOADOUT_HELPS("secondary"),
    ],
    "main_menu/settings_select/multiplayer_setup/item_options_edit/var_weapon_set": [
        "NORMAL", "PISTOLS", "RIFLES", "PLASMA WEAPONS", "SNIPER", "NO SNIPING", "ROCKET LAUNCHERS", "SHOTGUNS",
        "SHORT RANGE", "HUMAN", "COVENANT", "CLASSIC", "HEAVY WEAPONS", "NO GRENADES"],
})

# the map lists' first row: SINGLEPLAYER or MULTIPLAYER maps (_map_kind)
# which of STRING_OVERRIDES' strings say what one of the PC version's own
# says: its index in the PC version's list, or None (the port's own). A map
# in another language than English has those in its language (map=,
# menu_tags.c's map_strings); the inserted strings (STRING_INSERTS) are the
# port's own, the rest of their lists the PC version's
OVERRIDE_ORIGINALS = {
    f"{PE}/profile_edit_descriptions": [0, None, None, None, None, None, None, 7, None],
    f"{MT}/multiplayer_options": [0, 1, 2, 3, None, 5, None, None],
    f"{MT}/multiplayer_option_descriptions": [0, 1, None, None, 4, None, 5],
    "main_menu/gametype_select/var_gametype_banks": [0, None, 2],
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_labels": [0, 1, 2, 3, 4] + [None] * 4,
    # (the first's note that infinite grenades work only with 4 players or
    # fewer is left out: not so here)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/cap_item_options":
        [None] + list(range(1, 15)) + [None, 15, 16] + [None] * 24,
    "main_menu/settings_select/multiplayer_setup/item_options_edit/var_weapon_set": list(range(13)) + [None],
}

MAP_KIND_CHOOSER = "main_menu/new_select/list_item_0_map_kind"

# changes to the PC version's widgets (by our names): attributes set, all
# their handlers replaced, children added, game data inputs added
WIDGET_PATCHES = {
    # The host returns here after a round with no menu history: B must reopen the main menu when it closes the last
    # screen, as the lobby does (from OpenCE pull request #374 by chestahh: leaving showed only Halo's background)
    f"{MT}/connected/connected_map_select_wrapper": {"set": {
        "flags": "pass_unhandled_to_focused_child pause_game main_menu_if_no_history tag_controller_index force_handle_mouse",
    }},
    # (MENUS (AE: _menus_select) and OpenCE's MODS (_mods), above QUIT; the column from 211, 35 apart, ce_menus.CHILD_OFFSETS)
    "main_menu/main_menu_select_list": {"insert_before": {
        "main_menu/main_menu_item_quit_game": [
            f'<child widget="main_menu/main_menu_item_menus" x="192" y="{MODS_ITEM_PLACE[1] - 35}"/>',
            f'<child widget="main_menu/main_menu_item_mods" x="{MODS_ITEM_PLACE[0]}" y="{MODS_ITEM_PLACE[1]}"/>'],
    }},
    # (Mods, after Network Setup: _mods_item; Change Color and About a row down,
    # ce_menus.CHILD_OFFSETS)
    f"{PE}/profile_edit_select_list": {"insert_before": {
        f"{PE}/network_settings_profile_item": [f'<child widget="{PE}/hud_profile_item"/>'],
        f"{PE}/color_profile_item": [f'<child widget="{PE}/mods_profile_item"/>',
                                     f'<child widget="{PE}/map_files_profile_item"/>'],
    },
        # (each row's picture by the row's name, none for a row with no picture of its own:
        # port/linux/game/ae_glue_settings.c)
        "inputs": ["ae profile picture"]},
    # (the rows 27 px apart, for MAP FILES and HUD: PROFILE_ROWS)
    **{f"{PE}/{item}": {"set": {"text_y": PROFILE_ROW_TEXT_Y, "top": _profile_row_top(item),
                                "height": PROFILE_ROW_HEIGHT}}
       for item in PROFILE_ROWS if item not in PROFILE_OWN_ROWS},
    # (OpenCE build-244's AIM ASSIST, Player Options' ninth row, and the help under it)
    f"{PLAYER_EDIT}/player_options_menu": {"insert_before": {
        f"{PLAYER_EDIT}/player_button_bar": [
            f'<child widget="{PLAYER_EDIT}/op_aim_assist" x="54" y="289"/>',
            f'<child widget="{PLAYER_EDIT}/op_shield_effects" x="54" y="316"/>']}},
    # (its rows 27 apart, not 30, so that ten fit above the help, which keeps
    # the three lines some have: ce_menus.py's CHILD_OFFSETS)
    f"{PLAYER_EDIT}/player_options_help": {"set": {"top": 345, "height": 60}},
    # (the profile settings' picture: on Gamepad Setup's row, the profile's
    # button settings, BITMAP_FRAMES; menu_functions.c's
    # profile_gamepad_layout)
    f"{PE}/profile_edit_extended_desc_pic": {"inputs": ["port gamepad layout preview"]},
    # (straight to their screens: no "checking for updates" dialog, which
    # asked the PC version's servers)
    f"{MT}/multiplayer_type_join_internet_item": {"set": {"string_index": 6}, "handlers": [
        f'<on event="a" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        f'<on event="start" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    f"{MT}/multiplayer_type_create_internet_item": {"handlers": [
        '<on event="a" run="join controller to mp game"/>',
        f'<on event="a" run="mp type set mode" open="{MT}/connected/connected_map_select_wrapper"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    f"{MT}/multiplayer_type_join_direct_item": {"handlers": [
        f'<on event="a" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        f'<on event="start" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    # (Co-op, after Create Game's: _coop)
    f"{MT}/multiplayer_type_select_list": {"insert_before": {
        f"{MT}/multiplayer_type_gametypes_item": [
            f'<child widget="{MT}/multiplayer_type_split_screen_item" y="302"/>',
            f'<child widget="{MT}/multiplayer_type_coop_item" y="334"/>'],
    }},
    # (the Map screen's and New Game's lists: SINGLEPLAYER or MULTIPLAYER
    # maps, the first row's chooser's (_map_kind); B on the Map screen goes
    # from a co-op level's difficulties back to the level, else out of the
    # screen: menu_functions.c's map list)
    f"{MT}/mp_map_select/mp_map_select_list_2": {"swap": {"main_menu/new_select/list_item_0": MAP_KIND_CHOOSER},
                                                 "handlers": [
        '<on event="created" run="mp level list initialize"/>',
        '<on event="deleted" run="mp level list dispose"/>',
        f'<on event="custom_activation" run="mp level select" open="{MT}/connected/gametype_select_screen_wrapper"/>',
        '<on event="b" run="port map list back"/>',
        '<on event="back" run="port map list back"/>',
    ]},
    "main_menu/solo_level_select/solo_level_select_list": {"swap": {"main_menu/new_select/list_item_0": MAP_KIND_CHOOSER}},
    # (each description shows the other kind's maps too: a campaign level's
    # picture, name and words on the Map screen, a multiplayer map's on New
    # Game's)
    f"{MT}/mp_map_select/mp_map_right_item": {"children": [
        '<child widget="main_menu/solo_level_select/replay_level_right_name"/>',
        '<child widget="main_menu/solo_level_select/replay_level_right_pic"/>',
        '<child widget="main_menu/solo_level_select/replay_level_right_data"/>',
    ]},
    "main_menu/solo_level_select/replay_level_right_item": {"children": [
        f'<child widget="{MT}/mp_map_select/mp_map_right_name"/>',
        f'<child widget="{MT}/mp_map_select/mp_map_right_pic"/>',
        f'<child widget="{MT}/mp_map_select/mp_map_right_data"/>',
    ]},
    f"{MT}/join_game/header_join_game": {"children": [
        f'<child widget="{MT}/join_game/header_server_browser"/>',
        f'<child widget="{MT}/join_game/header_direct_link"/>',
    ]},
    # (the small font's line does not fit below their 4 units of offset)
    f"{MT}/join_game/ticker_player_info": {"set": {"text_y": 1}},
    # (the server browser's top-left tab: which games it lists, not Internet
    # or LAN, which are menu items of their own here; wider for its words, in
    # the small font of the counts beside it, and moved left to stay centred
    # in its tab: ce_menus.py's CHILD_OFFSETS)
    # (the tab behind it without CE's rounded box: the screen's border
    # picture already outlines the tab, and the two outlines clashed)
    f"{MT}/join_game/op_browser_mode": {"set": {"bitmap": None}},
    f"{MT}/join_game/browser_mode_spinner": {"set": {
        "string_list": None, "strings": "ALL GAMES|DEDICATED|P2P GAMES", "setting": "network.browser_source",
        "values": "all|dedicated|players", "width": 117, "footer_bounds": "7 120 19 126", "font": "ui\\small_ui",
        "text_y": 3}},
    # (the server browser's error panel: its words in a box of their own in
    # the error box's place, under its title and button; menu_functions.c's
    # lobby_browser_error hides the error box)
    f"{MT}/join_game/gamespy_error_fullscreen": {"insert_before": {"error/error_modal": [
        f'<child widget="{MT}/join_game/error_words" x="192" y="176"/>']}},
    # (wider to the left, for "SERVERS: 8 (5 dedicated)": ce_menus.py's
    # CHILD_OFFSETS keeps its right edge)
    f"{MT}/join_game/server_count_label": {"set": {"width": 210}},
    # (the page, when there is more than one: centred in the counts' panel,
    # between the players and the servers, whatever their numbers' widths;
    # menu_functions.c's lobby_browser_update places it)
    f"{MT}/join_game/page_count_label": {"set": {"align": "center"}},
    # (Filters' rows set the network.browser_* settings, as Network Setup's
    # SERVER BROWSER does: menu_functions.c's filters_show and
    # "gamespy update filter settings". SHOW MISSING MAPS has a third value,
    # those a host shares: Map Sharing downloads them)
    f"{MT}/join_game/filters/empty_servers_spinner": {"set": {
        "string_list": None, "strings": "NO|YES", "setting": "network.browser_show_empty", "values": "false|true"}},
    f"{MT}/join_game/filters/full_servers_spinner": {"set": {
        "string_list": None, "strings": "NO|YES", "setting": "network.browser_show_full", "values": "false|true"}},
    f"{MT}/join_game/filters/game_type_spinner": {"set": {
        "string_list": None, "strings": "ANY|CTF|SLAYER|ODDBALL|KING|RACE", "setting": "network.browser_gametype",
        "values": "any|ctf|slayer|oddball|king|race"}},
    f"{MT}/join_game/filters/installed_maps_spinner": {"set": {
        "string_list": None, "strings": "NO|YES|IF DOWNLOADABLE", "setting": "network.browser_maps",
        "values": "no|yes|downloadable"}},
    f"{MT}/join_game/ticker_rules_info": {"set": {"text_y": 1}},
    # (Item Options' loadout rows, over its buttons)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_menu": {"insert_before": {
        "main_menu/settings_select/multiplayer_setup/item_options_edit/item_button_bar": [
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_map_weapons" x="54" y="163"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_loadout" x="54" y="193"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_primary_weapon" x="54" y="223"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_secondary_weapon" x="54" y="253"/>',
        ]}},
    # (the gametype editor's ARENA OPTIONS, NHE OPTIONS and OpenCE's OPENCE
    # OPTIONS, after TEAMPLAY OPTIONS, over its buttons: _arena_options,
    # _nhe_options, _opence_options; their descriptions go by their places,
    # so OpenCE's "port playlist edit desc" is not asked)
    f"{PLAYLIST_EDIT}/edit_playlist_select_list": {"insert_before": {
        f"{PLAYLIST_EDIT}/playlist_edit_button_bar": [f'<child widget="{PLAYLIST_EDIT}/playlist_edit_arena_item"/>',
                                                        f'<child widget="{PLAYLIST_EDIT}/playlist_edit_nhe_item"/>',
                                                        f'<child widget="{OPENCE_ITEM}"/>'],
    }},
    # (Indicator Options' TIMERS, TRAINING and SPAWN HEAT rows, over its
    # buttons, 30 apart)
    f"{INDICATOR_OPTIONS}/indicator_options_menu": {"insert_before": {
        f"{INDICATOR_OPTIONS}/indicator_button_bar": [
            f'<child widget="{INDICATOR_OPTIONS}/op_item_timers" x="54" y="163"/>',
            f'<child widget="{INDICATOR_OPTIONS}/op_training" x="54" y="193"/>',
            f'<child widget="{INDICATOR_OPTIONS}/op_spawn_heat" x="54" y="223"/>',
        ]}},
    # (Teamplay Options' voice chat and vote kick rows, over its buttons:
    # Server Setup's only, _teamplay_options_extras)
    f"{TEAMPLAY_EDIT}/teamplay_options_menu": {"insert_before": {
        f"{TEAMPLAY_EDIT}/teamplay_button_bar": [
            f'<child widget="{TEAMPLAY_EDIT}/op_{key}" x="54" y="{163 + TEAMPLAY_STEP * index}"/>'
            for index, (key, *_) in enumerate(TEAMPLAY_EXTRA_ROWS)]}},
    f"{TEAMPLAY_EDIT}/teamplay_options_help": {"set": {"top": TEAMPLAY_HELP_TOP,
                                                       "height": 414 - TEAMPLAY_HELP_TOP}},
    # (the PC's Vehicles row's Start opened Item Options)
    "main_menu/settings_select/multiplayer_setup/playlist_edit/playlist_edit_vehicles_list_item": {"handlers": [
        '<on event="a" open="main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen"/>',
        '<on event="start" open="main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    # (Direct Link's clipboard button in the place of the PC version's
    # Refresh, and Refresh in Get List's: the Server Browser's only, which
    # asks the public games' hosts again; "swap" puts another widget in a
    # child's place)
    f"{MT}/join_game/join_game_button_bar": {"swap": {
        f"{MT}/join_game/join_game_button_refresh": f"{MT}/join_game/button_clipboard",
        f"{MT}/join_game/join_game_button_update": f"{MT}/join_game/join_game_button_refresh",
    }},
}

# frames added after a PC bitmap's (by our names): (the Xbox map's bitmap,
# its frame, the size drawn at, where in the widget). The profile settings'
# picture gets the Xbox's Controller Setup's pictures of its five button
# settings (its first four frames are the thumbstick settings'), which show
# 512 by 235 of their 512 by 512: drawn at 279 wide, in the middle of the
# picture's 279 by 202
XBOX_CONTROLLER_PICTURES = f"ui\\shell\\{PE.replace('/', chr(92))}\\controller_edit\\config_controller"
BITMAP_FRAMES = {
    f"{PE}/profile_options": [(XBOX_CONTROLLER_PICTURES, 4 + preset, 279, 279, 0, 34) for preset in range(5)],
}

# (Arena Evolved) frames of our own drawing in place of the PC version's that were placeholders, by bitmap and frame
# number (a number past the PC version's frames is added after them): the Multiplayer screen's every entry, the Edit
# Gametype screen's CHANGE NAME, ITEM, VEHICLE, TEAMPLAY, ARENA and NHE OPTIONS (tools/ae_menu_pictures.py)
def own_frames() -> dict:
    import ae_menu_pictures
    return ae_menu_pictures.own_frames()

# the titles this port has that the PC version has not, set as its headers
# are (port/assets/menus/port_svg): bitmap name, text
# (AE) the COMMUNITY SERVICES screen, the first-run opt-in (_community_services)
COMMUNITY = "main_menu/community_services"
COMMUNITY_HEADER = f"{COMMUNITY}/header_community_services"

TITLES = {
    "main_menu/mods/header_mods": "MODS",
    f"{MT}/join_game/header_server_browser": "SERVER BROWSER",
    f"{MT}/join_game/header_direct_link": "DIRECT LINK",
    f"{MT}/join_game/header_password": "PASSWORD",
    f"{MT}/lobby/header_lobby": "GAME LOBBY",
    f"{MT}/coop/header_player_2": "PLAYER 2 PROFILE",
    f"{MT}/lobby/header_add_player": "ADD PLAYER",
    f"{ARENA_OPTIONS}/header_arena_options": "ARENA OPTIONS",
    f"{NHE_OPTIONS}/header_nhe_options": "NHE OPTIONS",
    # (Server Setup's title in a split screen game, a game on this PC: ae_split_screen.c shows it there)
    f"{MT}/server_settings/header_game_setup": "GAME SETUP",
    # (split screen's JOIN screen: who plays, on which device)
    f"{MT}/split_screen/header_split_screen": "SPLIT SCREEN",
    # (the community services' first-run opt-in: _community_services)
    COMMUNITY_HEADER: "COMMUNITY SERVICES",
}


def title_backdrop(width: float) -> str:
    """a header's dark backdrop, as the PC headers' redraws have it, for text
    width units wide (the text is set over it in OpenCE: ce_menus.py)"""
    return "\n".join([
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="64" viewBox="0 0 512 64">',
        "  <defs>",
        '    <filter id="soft" filterUnits="userSpaceOnUse" x="-60" y="-60" width="632" height="184">',
        '      <feGaussianBlur stdDeviation="14.23 13.06"/>', "    </filter>",
        '    <mask id="cut" maskUnits="userSpaceOnUse" x="0" y="0" width="512" height="64">',
        '      <rect width="512" height="59" fill="#fff"/>', "    </mask>", "  </defs>",
        '  <g mask="url(#cut)">',
        f'    <rect x="29.08" y="28.05" width="{width + 28:.2f}" height="40.95" fill="#021931" fill-opacity="0.893" '
        'filter="url(#soft)"/>',
        "  </g>", "</svg>", ""])


# the in-game pause menu's box, taller for the buttons this port adds
# (menu_tags.c's pause_patch: SETTINGS, the host's END GAME, and a team
# game's CHANGE TEAMS, ChupathingyCE 0.8's): the
# pausebox5 redraw's rounded box (ui-svg-handmade/shell/bitmaps), drawn to
# hold each number of buttons, one frame each (the Xbox's 2-button box is
# 159 units high, and each button 35 more)
PAUSE_BOX_BUTTONS = [3, 4, 5]
# their textures' height (menu_tags.c's pause_box_redraw sets the pieces'
# bounds to it): five buttons' box is 264 high, past a 256-high texture
PAUSE_BOX_TEXTURE = 512
# the campaign pause box, tall enough for five rows (SETTINGS added) or six
# (network co-op's FRIENDLY FIRE too) at the PC version's pause menu's pitch
# (30) with the prompts still under the last row: the stock box is 159, and
# those rows need 36 and 66 more (menu_tags.c's pause_campaign_patch); the
# mission objectives' box beside it grows with it
PAUSE_BOX_EXTRA_HEIGHTS = [195, 225]


def pause_box_height(buttons: int) -> int:
    return 159 + (buttons - 2) * 35


def pause_box_svg(piece: str, height: int, header: bool = False, texture: int = 256) -> str:
    """a piece of the pause box (left cap, centre strip, right cap; 9-slice:
    the strip tiles across), height units high in its texture-high texture"""
    width = 4 if piece == "center" else 16
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{texture}" viewBox="0 0 {width} {texture}">']
    fill = "#0a2649"
    body_clip = ""
    if not header:
        # Keep the translucent footer clear of the body: blending two
        # 75%-opaque fills would make it nearly opaque and tint it blue.
        lines += [f'  <defs><clipPath id="body-area"><rect x="-16" y="0" width="48" height="{height - 28}"/></clipPath></defs>']
        body_clip = ' clip-path="url(#body-area)"'
    if height in PAUSE_BOX_EXTRA_HEIGHTS or height in map(pause_box_height, PAUSE_BOX_BUTTONS):
        # The stock campaign body fades from blue to dark navy at 75% opacity.
        # Scale it within the body, excluding the objective header or key footer.
        top, bottom = (29, height - 2) if header else (2, height - 28)
        lines += [f'  <defs><linearGradient id="body" gradientUnits="userSpaceOnUse" x1="0" y1="{top}" x2="0" y2="{bottom}">',
                  '    <stop offset="0" stop-color="#0a2649"/>',
                  '    <stop offset="1" stop-color="#050d1c"/>',
                  '  </linearGradient></defs>']
        fill = "url(#body)"
    if piece == "center":
        lines += [f'  <rect x="0" y="2" width="4" height="{height - 4}" fill="{fill}" fill-opacity="0.75"{body_clip}/>',
                  '  <rect x="0" y="0" width="4" height="2" fill="#2896ff"/>',
                  f'  <rect x="0" y="{height - 2}" width="4" height="2" fill="#2896ff"/>']
    else:
        bottom, corner = height - 1, height - 9.5
        cap = [f'    <path d="M9.5,1 H17 V{bottom} H9.5 A8.5,8.5 0 0 1 1,{corner} V9.5 A8.5,8.5 0 0 1 9.5,1 Z" '
               f'fill="{fill}" fill-opacity="0.75" fill-rule="evenodd"{body_clip}/>',
               f'    <path d="M17,1 H9.5 A8.5,8.5 0 0 0 1,9.5 V{corner} A8.5,8.5 0 0 0 9.5,{bottom} H17" fill="none" '
               'stroke="#2896ff" stroke-width="2"/>']
        lines += (['  <g transform="matrix(-1 0 0 1 16 0)">'] + cap + ['  </g>']) if piece == "right" else \
            [line[2:] for line in cap]
    # The stock objectives have a 28-unit header; the button key has a
    # 28-unit footer. Clip the black fill to the rounded panel interior.
    top = 2 if header else height - 28
    divider = 28 if header else height - 28
    if piece == "left":
        lines += [f'  <defs><clipPath id="strip"><rect x="2" y="2" width="{width + 16}" height="{height - 4}" rx="7.5"/></clipPath></defs>']
    if piece == "right":
        lines += [f'  <defs><clipPath id="strip"><rect x="-16" y="2" width="{width + 14}" height="{height - 4}" rx="7.5"/></clipPath></defs>']
    clip = ' clip-path="url(#strip)"' if piece != "center" else ""
    opacity = ' fill-opacity="0.75"' if not header else ""
    lines += [f'  <g{clip}>',
              f'    <rect x="0" y="{top}" width="{width}" height="26" fill="black"{opacity}/>',
              f'    <rect x="0" y="{divider}" width="{width}" height="0.75" fill="#2896ff"/>',
              '  </g>']
    lines += ["</svg>", ""]
    return "\n".join(lines)


# where the PC headers' text is (units of their 512x64), and its colour
TITLE_LEFT, TITLE_TOP, TITLE_CAP = 30.68, 33.5, 21.5
TITLE_COLOR = (0x29, 0x95, 0xFD, 255)


def _header(name: str, bitmap: str, platform=None) -> list:
    return _widget(name, [("controller", 1), ("left", 35), ("top", 11), ("width", 363), ("height", 59),
                          ("bitmap", bitmap), ("platform", platform)], [])


# a text field's B and Back do nothing of their own (the engine would go
# back from the field): its screen's (SCREEN_BACK) cancel the edit, else
# leave
FIELD_BACK = ['<on event="b"/>', '<on event="back"/>']
SCREEN_BACK = ['<on event="b" run="gamespy back handler"/>', '<on event="back" run="gamespy back handler"/>']


# (AE: Server Setup's text values centred where its spinners' values are (x 320 + 3, 147 wide: 396.5), ending at the
# row's edge, as the owner saw the GAME NAME off the column, 2026-10-18)
SERVER_VALUE_X, SERVER_VALUE_WIDTH = 282, 230


def _value_row(base: str, key: str, label_index: int, run: str) -> list:
    """a row of a label and a value the player sets (A edits it)"""
    lines = _widget(f"{base}/op_{key}", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                    [f'<on event="a" run="{run}"/>', f'<on event="start" run="{run}"/>', *FIELD_BACK,
                     '<on event="left_mouse" run="mouse emit accept event"/>',
                     f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="{SERVER_VALUE_X}" y="1"/>'])
    lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                             ("string_list", f"{base}/labels"), ("string_index", label_index or None),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    lines += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", SERVER_VALUE_WIDTH), ("height", 22),
                                             ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                             ("text_y", 5), ("text_flags", "no_focus_test")], [])
    return lines


# the server browser's words (menu_functions.c's lobby_browser_update, in
# this order: its _browser_word_*): the line below the rows, the rows' status,
# the counts, and the error panel's
BROWSER_WORDS = [
    # (the chosen game's line: "12 of 16 players", and its state)
    "of", "players", "player", "under way", "full or starting",
    # (where it was listed, and what it needs)
    "Dedicated server by", "listed on", "master server", "master servers", "Dedicated server",
    "Hosted by a player (anonymous listing)", "Password required",
    "Needs the newer build: Settings > Network Setup > Check for updates",
    "Runs an older build: it can be joined once it updates",
    # (a join, as it goes: p2p_join_progress)
    "Asking the brokers for the host...", "The host answered: connecting (UDP hole punching)...",
    "Connected: joining the game...",
    "The host did not answer (offline, or its listing is stale)",
    "No route to the host: a forwarded port on either side helps",
    "The host refused: full", "The host refused: closed", "The host refused: banned",
    "The host refused: wrong password", "The host refused: another version", "The host refused: its invite changed",
    "Strict NAT: turn on UPnP, or forward a port",
    # (the list's state)
    "Internet play is off (Settings)", "The server browser is off (network.public_lobby)",
    "Looking for public games...", "Games on a newer version (update the game to join them):",
    "No public games found", "No games match the filters",
    "No master server answered: showing player-hosted games only",
    # (the counts: "SERVERS: 8 (3 dedicated)"; the rows' status)
    "dedicated", "JOINING", "NEWER", "OLDER", "FAILED", "LIVE", "CLOSED",
    # (the error panel: nothing can be listed)
    "Internet play is off:\\nSettings > Network Setup",
    "Neither the master servers\\nnor the brokers answer:\\ncheck the connection",
    # (a refusal added later, last so that the others keep their places)
    "The host refused: your player name",
]


def _join_game_extras() -> list:
    """the browser's added titles, Direct Link's clipboard button, the
    password screen, and the browser's words"""
    base = f"{MT}/join_game"
    lines = _password_screen()
    lines += _strings(f"{base}/browser_words", BROWSER_WORDS)
    # (the error panel's words, in a box as the error box: its own text is
    # the game's error messages')
    lines += _widget(f"{base}/error_words", [("type", "text"), ("controller", 1), ("width", 280), ("height", 159),
                                             ("bitmap", "bitmaps/alert_bkd"), ("font", "ui\\large_ui"),
                                             ("color", "#FF2896FF"), ("text_x", 9),
                                             ("text_y", 35), ("text_flags", "no_focus_test")], [])
    lines += _header(f"{base}/header_server_browser", f"{base}/header_server_browser")
    lines += _header(f"{base}/header_direct_link", f"{base}/header_direct_link")
    lines += _widget(f"{base}/button_clipboard", [("type", "text"), ("width", 128), ("height", 24),
                                                  ("bitmap", "bitmaps/text_button_background"),
                                                  ("text", "PASTE LINK"), ("font", "ui\\small_ui"),
                                                  ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="direct ip connect go"/>', '<on event="start" run="direct ip connect go"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def _password_screen() -> list:
    """the server browser's password screen: a game with a password chosen
    (menu_functions.c's lobby_browser_select), its password typed (shown as
    stars), JOIN GAME joins it if it is the game's; else its help says so"""
    base = f"{MT}/join_game/password"
    spec = {"screen": "password_screen", "spacing": 28, "help_top": 120,
            "header": ("header_password", f"{MT}/join_game/header_password")}
    extra = _value_row(base, "password", 0, "port password edit")
    extra += _button(f"{base}/button_defaults", 3, [])
    extra += _widget(f"{base}/button_ok", [("type", "text"), ("width", 128), ("height", 24),
                                           ("bitmap", "bitmaps/text_button_background"), ("text", "JOIN GAME"),
                                           ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                           ("text_y", 2)],
                     ['<on event="a" run="port password join"/>', '<on event="start" run="port password join"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    extra += _strings(f"{base}/labels", ["PASSWORD:"])
    # (by its state: menu_functions.c's password_screen_update)
    extra += _strings(f"{base}/help_strings", [
        "",
        "This game has a password. Type it, then press\\nEnter to join.",
        "That is not the game's password. Try again.",
        "Could not join the game.",
    ])
    lines = _screen(base, spec, [(f"{base}/op_password", None)], ["port password update"],
                    ['<on event="created" run="port password init"/>'], extra)
    # (no Defaults: the bar is JOIN GAME and CANCEL; B goes back at once,
    # even while typing, which the screen starts with)
    lines = [line.replace(f'<child widget="{base}/button_defaults" y="1"/>', "") for line in lines]
    lines = [line.replace('<on event="b" back="true"/>', '<on event="b" run="port password back"/>')
             .replace('<on event="back" back="true"/>', '<on event="back" run="port password back"/>')
             for line in lines]
    return lines


def _server_settings() -> list:
    """Create Game's server settings (in the PC version's place): the game's
    name, the most players (up to the port's 128), its invite link, and
    whether the server browser lists it (PUBLIC or PRIVATE: an internet
    game's)"""
    base = f"{MT}/server_settings"
    # (thirteen rows, with ARENA OPTIONS, PASSWORD and MAP SHARING: closer
    # together, the help lower, as video_settings)
    spec = {"screen": "server_settings_screen", "spacing": 22, "help_top": 368,
            "header": ("header_server_settings", f"{base}/header_server_settings"),
            # (GAME SETUP over SERVER SETTINGS in a split screen game: ae_split_screen.c shows one of the two)
            "screen_children": [f'<child widget="{base}/header_game_setup"/>']}
    rows, extra = [], []
    extra += _widget(f"{base}/header_game_setup", [("controller", 1), ("left", 35), ("top", 11), ("width", 605),
                                                    ("height", 59), ("bitmap", f"{base}/header_game_setup")], [])
    extra += _value_row(base, "server_name", 0, "ss edit server name")
    rows.append((f"{base}/op_server_name", None))
    extra += _widget(f"{base}/op_max_players", [("width", 512), ("height", 28),
                                                ("flags", "pass_unhandled_to_focused_child"),
                                                ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/max_players_label"/>',
                      f'<child widget="{base}/max_players_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/max_players_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                   ("height", 22), ("string_list", f"{base}/labels"),
                                                   ("string_index", 1), ("font", "ui\\large_ui"),
                                                   ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/max_players_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "|".join(str(count) for count in MAXIMUM_PLAYERS)), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")], [])
    rows.append((f"{base}/op_max_players", None))
    extra += _value_row(base, "invite", 2, "ss copy invite")
    rows.append((f"{base}/op_invite", None))
    # (PUBLIC or PRIVATE: menu_functions.c's server_settings_update)
    extra += _widget(f"{base}/op_listing", [("width", 512), ("height", 28),
                                            ("flags", "pass_unhandled_to_focused_child"),
                                            ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/listing_label"/>',
                      f'<child widget="{base}/listing_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/listing_label", [("type", "text"), ("controller", 1), ("width", 300),
                                               ("height", 22), ("string_list", f"{base}/labels"),
                                               ("string_index", 10), ("font", "ui\\large_ui"),
                                               ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/listing_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "PUBLIC|PRIVATE"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    rows.append((f"{base}/op_listing", None))
    # PASSWORD (a public internet game's: menu_functions.c's server_settings_update),
    # which players from the server browser must type
    # (AE: one past upstream's, after ARENA OPTIONS' label)
    extra += _value_row(base, "password", 11 + len(COOP_SETUP_SCREENS), "ss edit server password")
    rows.append((f"{base}/op_password", None))
    # MAP SHARING (map_share.c), a screen of its own as co-op's options are,
    # for any game: SETUP_OPTIONS_PLACE below it
    key, _, _, _ = MAP_SHARE_SETUP
    target = f"{base}/{key}/{key}_screen"
    extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                         ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>',
                      f'<child widget="{base}/{key}_label"/>'])
    extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                             ("string_list", f"{base}/labels"),
                                             # (AE: one past upstream's, after ARENA OPTIONS' label)
                                             ("string_index", 12 + len(COOP_SETUP_SCREENS)),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE - 1))
    # co-op's options, in the place of the gametype's rows, which co-op hides
    # as multiplayer hides co-op's (menu_functions.c's server_settings_update):
    # rows opening their screens, as the gametype's options' do
    # (_setup_option_screen; COOP_SETUP_SCREENS)
    for index, (key, title, _, _) in enumerate(COOP_SETUP_SCREENS):
        target = f"{base}/{key}/{key}_screen"
        extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child"),
                                             ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                                 ("string_list", f"{base}/labels"), ("string_index", 11 + index),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                 ("text_y", 4)], [])
        rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + index))
    # the gametype's options for this game (the gametype editor's screens,
    # editing a copy of the gametype chosen: "port setup edit")
    for index, (key, screen) in enumerate(SETUP_OPTION_SCREENS):
        target = f"main_menu/settings_select/multiplayer_setup/{screen}"
        extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child"),
                                             ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" run="port setup edit" open="{target}"/>',
                          f'<on event="start" run="port setup edit" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="{SERVER_VALUE_X}" y="1"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                                 ("string_list", f"{base}/labels"), ("string_index", 3 + index),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                 ("text_y", 4)], [])
        extra += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", SERVER_VALUE_WIDTH), ("height", 22),
                                                 ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                                 ("text_y", 5), ("text_flags", "no_focus_test")], [])
        rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + index))
    extra += _button(f"{base}/button_defaults", 3, [])
    extra += _widget(f"{base}/button_ok", [("type", "text"), ("width", 128), ("height", 24),
                                           ("bitmap", "bitmaps/text_button_background"), ("text", "START GAME"),
                                           ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                           ("text_y", 2)],
                     [f'<on event="a" run="ss start game" open="{MT}/lobby/lobby_screen"/>',
                      f'<on event="start" run="ss start game" open="{MT}/lobby/lobby_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    extra += _strings(f"{base}/labels", ["GAME NAME:", "MAXIMUM PLAYERS:", "INVITE LINK:", "GAME TYPE:",
                                         "PLAYER OPTIONS:", "ITEM OPTIONS:", "VEHICLE OPTIONS:", "INDICATOR OPTIONS:",
                                         "TEAMPLAY OPTIONS:", "ARENA OPTIONS:", "LISTING:",
                                         *(f"{title}:" for _, title, _, _ in COOP_SETUP_SCREENS), "PASSWORD:",
                                         f"{MAP_SHARE_SETUP[1]}:"])
    extra += _strings(f"{base}/help_strings", [
        "",
        "The name the game shows in the lists of games.\\nEnter changes it.",
        "The most players the game takes (up to 128).",
        "Players join this game by its invite link, or your\\nDiscord invite. Enter copies the link again.",
        "The gametype and its rules, for this game.",
        "Lives, health, shields and respawning, for this game.",
        "Weapons, grenades and starting equipment, for\\nthis game.",
        "Each team's vehicles and their respawn time, for\\nthis game.",
        "The motion tracker and nav points, for this game.",
        "Friendly fire and team balance, for this game.",
        "Time limit, fall damage, health and Halo 1: NHE's\\ncompetitive options, for this game.",
        # (LISTING's, by its choice; PUBLIC's with a publisher key is last)
        "Anyone can see and join your game: it is listed\\nfor everyone through the brokers.",
        "Only players with your invite link can join.",
        # (co-op's options' rows: COOP_SETUP_SCREENS)
        *(help_text for _, _, help_text, _ in COOP_SETUP_SCREENS),
        # (PASSWORD's)
        "Players from the Server Browser must type it to\\njoin (an invite link needs none). Enter sets it.",
        # (MAP SHARING's)
        MAP_SHARE_SETUP[2],
        # (LISTING's PUBLIC, on a machine with a publisher key: menu_functions.c's
        # server_settings_update)
        "Listed for everyone through the brokers, and on\\nthe master servers with your publisher key.",
    ])
    lines = _screen(base, spec, rows, ["server settings update"],
                    ['<on event="created" run="server settings init"/>'], extra)
    # (co-op's options' screens, and map sharing's)
    for key, title, _, screen_rows in [*COOP_SETUP_SCREENS, MAP_SHARE_SETUP]:
        lines += _setup_option_screen(f"{base}/{key}", key, screen_rows)
    # (no Defaults: the bar is START and CANCEL; B cancels the name's edit
    # first)
    lines = [line.replace(f'<child widget="{base}/button_defaults" y="1"/>', "") for line in lines]
    lines = [line.replace('<on event="b" back="true"/>', SCREEN_BACK[0])
             .replace('<on event="back" back="true"/>', SCREEN_BACK[1]) for line in lines]
    return lines


MAXIMUM_PLAYERS = [2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128]
# CO-OP OPTIONS' amounts of PER PLAYER (network.coop_enemies, percentages)
# and STATIC MULTIPLIER (network.coop_enemies_multiplier), as menu_functions.c's
# gametype_options have their values
COOP_ENEMIES_PERCENTAGES = [25, 50, 100, 150, 200]
COOP_ENEMIES_MULTIPLIERS = [2, 4, 8, 16, 32]
# co-op's Server Setup options, in screens of their own as the gametype's
# are (its rows: _server_settings), each its key, its title, its row's help
# and its rows (as TEAMPLAY_ROWS: each spinner's key, label, words and the
# help of each of them; menu_functions.c's gametype_options give their
# settings' values)
COOP_SETUP_SCREENS = [
    ("coop_options", "CO-OP OPTIONS", "Friendly fire, the enemies and player collisions,\\nfor this game.", [
        ("coop_friendly_fire", "FRIENDLY FIRE:", ["OFF", "ON", "SHIELD ONLY", "EXPLOSIVES ONLY"], [
            "Players can not be hurt by weapons and explosives\\nfired by the other players.",
            "Players can be hurt by weapons or explosives\\nfired by the other players.",
            "Damage from the other players will only reduce\\nshields. Health will be unaffected.",
            "Players can be hurt by damage from explosives\\nfired by the other players.",
        ]),
        ("coop_extra_enemies", "EXTRA ENEMIES:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Enemy squads grow with the players: by the value of\\n'Per Player' for each player past the first.",
            "Enemy squads use 'Multiplier', however many players\\nthere are.",
            "Enemy squads are as the campaign has them.",
        ]),
        ("coop_extra_pickups", "EXTRA PICKUPS:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Pickups grow with the players: by the value of\\n'Per Player' for each player past the first.",
            "Pickups use 'Multiplier', up to eight times as many,\\nhowever many players there are.",
            "Pickups are as the campaign has them.",
        ]),
        ("coop_extra_vehicles", "EXTRA VEHICLES:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Vehicles respawn after being driven a short distance\\nbased on the number of players: by two thirds the value of\\n'Per Player' for each player past the first.",
            "Vehicles respawn after being driven a short distance,\\nwith extra copies equal to ('Multiplier' - 1) * 2/3,\\nrounded down, however many players there are.",
            "Vehicles are as the campaign has them.",
        ]),
        ("coop_enemies_multiplier", "MULTIPLIER:", [f"{value}X" for value in COOP_ENEMIES_MULTIPLIERS],
         ["Static amounts for the options set to this mode.\\nPickups are capped at eight times as many."] * len(COOP_ENEMIES_MULTIPLIERS)),
        ("coop_enemies_per_player", "PER PLAYER:", [f"{value}%" for value in COOP_ENEMIES_PERCENTAGES],
         ["For each player past the first, options set to Per\\nPlayer get this much more (100%: as many again)."] *
         len(COOP_ENEMIES_PERCENTAGES)),
        ("coop_player_collisions", "PLAYER COLLISIONS:", ["ON", "OFF"], [
            "Players bump into each other, as in the campaign.",
            "Players walk through each other, so that no one\\nblocks a doorway. The AI's characters still block.",
        ]),
        # (network.coop_ally_friendly_fire: game_engine.c's
        # game_engine_friendly_damage)
        ("coop_ally_friendly_fire", "ALLY FRIENDLY FIRE:", ["ON", "OFF"], [
            "Players can hurt and kill their AI allies, as in\\nthe campaign.",
            "Players can not hurt their AI allies, so that no\\none can kill Keyes or the Marines a level needs.",
        ]),
    ]),
    ("voice_options", "VOICE AND VOTING", "Voice chat, text chat and votes to kick a player,\\nfor this game.",
     TEAMPLAY_ROWS),
]
# Server Setup's map sharing (port/linux/game/map_share.c), for any game,
# laid out as co-op's options are (its key, its title, its row's help and
# its rows: menu_functions.c's gametype_options give their settings' values)
MAP_SHARE_UPLOADS = (256, 512, 1024, 2048, 4096, 8192, 16384, 20480)
MAP_SHARE_SETUP = ("map_share_options", "MAP SHARING",
                   "Sending players who join without the game's\\nCustom Edition map the map, and how fast.", [
    ("map_share", "SHARE MAPS:", ["ON", "OFF"], [
        "Players who join without the map download it\\nfrom you and the other players.",
        "Players who join without the map cannot play.",
    ]),
    ("map_share_upload", "UPLOAD LIMIT:", [f"{kb} KB/S" if kb < 1024 else f"{kb // 1024} MB/S"
                                           for kb in MAP_SHARE_UPLOADS],
     ["The most you send of maps, so that the game\\nkeeps the rest of your upload."] * len(MAP_SHARE_UPLOADS)),
])
# (Server Setup's place of its options' rows, co-op's or the gametype's: after
# MAP SHARING's)
SETUP_OPTIONS_PLACE = 6
# (their screens' titles)
TITLES.update({f"{MT}/server_settings/{key}/header_{key}": title
               for key, title, _, _ in [*COOP_SETUP_SCREENS, MAP_SHARE_SETUP]})
# (the rows of a co-op options screen in one place, the one shown: PER
# PLAYER's and MULTIPLIER's, by EXTRA ENEMIES')
SETUP_OPTION_SAME_PLACE = set()


def _setup_option_screen(base: str, key: str, rows: list) -> list:
    """a screen of options laid out as the gametype's are (Teamplay
    Options'), its spinners setting config.toml's settings (menu_functions.c's
    "port setup options init" and "port setup options save", OK), each
    choice's help shown as the gametype's are ("game settings lists text
    update": the help strings, each spinner's choices' in turn)"""
    lines = _widget(f"{base}/{key}_screen", [("width", 640), ("height", 480),
                                            ("flags", "pass_unhandled_to_focused_child"),
                                            ("bitmap", "bitmaps/gradient")],
                    [f'<child widget="{base}/header_{key}"/>', f'<child widget="{base}/{key}_menu"/>'])
    lines += _header(f"{base}/header_{key}", f"{base}/header_{key}")
    lines += _widget(f"{base}/{key}_help", [("type", "text"), ("controller", 1), ("left", 68), ("top", 341),
                                           ("width", 482), ("height", 79), ("string_list", f"{base}/cap_{key}"),
                                           ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="game settings lists text update"/>',
                '<on event="created" run="port setup options init"/>']
    place = -1
    for row_key, *_ in rows:
        if row_key not in SETUP_OPTION_SAME_PLACE:
            place += 1
        children.append(f'<child widget="{base}/op_{row_key}" x="54" y="{73 + 30 * place}"/>')
    children.append(f'<child widget="{base}/{key}_button_bar" y="414"/>')
    lines += _widget(f"{base}/{key}_menu", [("type", "column_list"), ("width", 640), ("height", 480),
                                           ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                           ("description", f"{base}/{key}_help")], children)
    lines += _widget(f"{base}/{key}_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                 ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>', f'<child widget="{base}/{key}_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _button(f"{base}/{key}_button_ok", 1, ['<on event="a" run="port setup options save" back="true"/>',
                                                     '<on event="start" run="port setup options save" back="true"/>'])
    for index, (row_key, label, strings, _) in enumerate(rows):
        lines += _widget(f"{base}/op_{row_key}", [("width", 512), ("height", 28),
                                                 ("flags", "pass_unhandled_to_focused_child"),
                                                 ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{row_key}_label"/>',
                          f'<child widget="{base}/{row_key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{row_key}_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                     ("height", 22), ("string_list", f"{base}/{key}_labels"),
                                                     ("string_index", index), ("font", "ui\\large_ui"),
                                                     ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{row_key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/var_{row_key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{base}/var_{row_key}", strings)
    lines += _strings(f"{base}/{key}_labels", [label for _, label, *_ in rows])
    lines += _strings(f"{base}/cap_{key}", [text for *_, helps in rows for text in helps])
    return lines
# Server Setup's rows of the gametype's options: the gametype editor's
# screens
SETUP_OPTION_SCREENS = [
    ("game_type", "playlist_edit/game_type_select/gametype_select_screen"),
    ("player_options", "player_options_edit/player_options_screen"),
    ("item_options", "item_options_edit/item_options_screen"),
    ("vehicle_options", "vehicle_options_edit/vehicle_options_screen"),
    ("indicator_options", "indicator_options_edit/indicator_options_screen"),
    ("team_options", "teamplay_options_edit/teamplay_options_screen"),
    ("arena_options", "arena_options_edit/arena_options_screen"),
]


LOBBY_PICK_ROWS = 11


def lobby_pick_handler_names() -> list:
    """the lobby's controller picker's handlers (menu_tags.c lists them, on an AE hook line of their own)"""
    return [f"ae split lpick {kind} {row}" for kind in ("left", "right", "assign") for row in range(LOBBY_PICK_ROWS)] + [
        "ae split lobby assign"]


def _lobby_pick(base: str) -> list:
    """(AE) the controller picker on the lobby's rows of this machine's players: a column list of its own over
    the rows' right part (its children are the mouse's items), each row's arrows, value and ASSIGN / CANCEL;
    ae_split_screen.c shows a row's only when it is one of this machine's players, and the help line under them"""
    items, lines = [], []
    button = [("type", "text"), ("bitmap", "bitmaps/ae_seat_button_assign"), ("font", "ui\\small_ui"),
              ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)]

    def item(name: str, handler: str, x: int, y: int, width: int, height: int, pairs=None):
        nonlocal lines
        lines += _widget(f"{base}/{name}", [("width", width), ("height", height)] + (pairs or []),
                         [f'<on event="a" run="ae split lpick {handler}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
        items.append(f'<child widget="{base}/{name}" x="{x}" y="{y}"/>')

    for row in range(LOBBY_PICK_ROWS):
        top = 30 * row
        item(f"lobby_pick_l_{row}", f"left {row}", 156, top, 52, 28)
        item(f"lobby_pick_r_{row}", f"right {row}", 208, top, 70, 28)
        item(f"lobby_pick_assign_{row}", f"assign {row}", 282, top + 2, 56, 24, button + [("text", "ASSIGN")])
        item(f"lobby_pick_cancel_{row}", f"assign {row}", 282, top + 2, 56, 24, button + [("text", "CANCEL")])
        # (ASSIGN's wait: the arrows and value's whole width)
        lines += _widget(f"{base}/lobby_pick_wait_{row}",
                         [("type", "text"), ("controller", 1), ("width", 128), ("height", 28), ("text", "PRESS A BUTTON"),
                          ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                          ("text_flags", "no_focus_test"), ("text_y", 6)], [])
        items.append(f'<child widget="{base}/lobby_pick_wait_{row}" x="153" y="{top}"/>')
        for side, x in (("left", 162), ("right", 266)):
            lines += _widget(f"{base}/lobby_pick_arrow_{side}_{row}",
                             [("controller", 1), ("width", 12), ("height", 12), ("bitmap", f"bitmaps/arrow_sm_{side}")], [])
            items.append(f'<child widget="{base}/lobby_pick_arrow_{side}_{row}" x="{x}" y="{top + 8}"/>')
        for size, font, ty in (("large", "ui\\large_ui", 3), ("small", "ui\\small_ui", 6)):
            lines += _widget(f"{base}/lobby_pick_value_{size}_{row}",
                             [("type", "text"), ("controller", 1), ("width", 88), ("height", 28), ("font", font),
                              ("color", "#FFFFFFFF"), ("align", "center"), ("text_flags", "no_focus_test"),
                              ("text_y", ty)], [])
            items.append(f'<child widget="{base}/lobby_pick_value_{size}_{row}" x="176" y="{top}"/>')
    lines += _widget(f"{base}/lobby_pick_help", [("type", "text"), ("controller", 1), ("left", 22), ("top", 388),
                                                 ("width", 380), ("height", 24), ("font", "ui\\small_ui"),
                                                 ("color", "#FF2896FF"), ("align", "left"), ("text_y", 5),
                                                 ("text_flags", "no_focus_test")], [])
    return _widget(f"{base}/lobby_pick_list", [("type", "column_list"), ("width", 390), ("height", 330)], items) + lines


def _lobby() -> list:
    """the lobby the host's and the joining players wait in (the Xbox's
    pregame's functions, which need no widget of theirs): up to the port's
    128 players, scrolling; the game's map and gametype; the countdown"""
    base = f"{MT}/lobby"
    # (split screen: another controller's START joins it, its B leaves alone:
    # menu_functions.c's lobby_join. After a game it is the only screen,
    # network_game_reset_to_pregame_ui opening it in place of the Xbox's
    # pregame: leaving it goes to the main menu, as the PC version's postgame
    # screens do)
    lines = _widget(f"{base}/lobby_screen", [("width", 640), ("height", 480),
                                             ("flags", "pass_unhandled_to_focused_child main_menu_if_no_history"),
                                             ("bitmap", "bitmaps/gradient")],
                    ['<on event="created" run="port lobby open"/>',
                     '<on event="created" run="net server accept conx"/>',
                     '<on event="created" run="net server allow start"/>',
                     '<on event="b" run="port lobby leave" back="true"/>',
                     '<on event="back" run="port lobby leave" back="true"/>',
                     f'<on event="start" run="port lobby join" open="{base}/player_profile_screen" branch="true"/>',
                     # (AE: a player's own Y is its ASSIGN, the controller picker: ae_split_screen.c)
                     '<on event="y" run="ae split lobby assign"/>',
                     '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                     f'<child widget="{base}/lobby_list"/>',
                     f'<child widget="{base}/lobby_pick_list" x="20" y="73"/>',
                     f'<child widget="{base}/lobby_pick_help"/>',
                     f'<child widget="{base}/header_lobby"/>',
                     f'<child widget="{base}/lobby_join_help"/>'])
    lines += _header(f"{base}/header_lobby", f"{base}/header_lobby")
    lines += _widget(f"{base}/lobby_join_help", [("type", "text"), ("controller", 1), ("left", 355), ("top", 446),
                                                 ("width", 275), ("height", 24), ("font", "ui\\small_ui"),
                                                 ("color", "#FF2896FF"), ("align", "right"), ("text_y", 5),
                                                 ("text_flags", "no_focus_test")], [])
    # (its rows: any controller's left and right switch its own player's team,
    # on the rows only, so that they move along the buttons)
    rows = [f'<child widget="{base}/list_item_{index}" x="20" y="{73 + 30 * index}"/>' for index in range(11)]
    lines += _widget(f"{base}/lobby_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                            ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                            ("description", f"{base}/lobby_desc")],
                     ['<data input="net splitscreen prejoin players"/>', '<data input="port lobby update"/>',
                      *rows,
                      # (AE: a split screen game's bot rows, in the last four player rows' places, which it
                      # hides: ae_split_screen.c)
                      *[f'<child widget="{base}/ae_bots_{key}_row" x="20" y="{73 + 30 * (7 + index)}"/>'
                        for index, (key, _, _, _, _) in enumerate(LOBBY_BOT_ROWS)],
                      f'<child widget="{base}/lobby_button_bar" y="414"/>'])
    for index in range(11):
        lines += _widget(f"{base}/list_item_{index}",
                         [("width", 390), ("height", 28),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                         ['<on event="left right" run="swap player team"/>',
                          '<child widget="main_menu/new_select/list_item_text" x="25"/>',
                          '<child widget="main_menu/new_select/list_item_arrows"/>'])
    # (AE: this machine's players' rows with a controller picker have their names moved left at run time, as R4 of the
    # picker's design review has them: ae_split_screen.c; the others stay centred)
    lines += _lobby_pick(base)
    for key, caption, setting, strings, values in LOBBY_BOT_ROWS:
        lines += _widget(f"{base}/ae_bots_{key}_row",
                         # (Settings' option rows' look: the PC menus' own band, cut to the lobby's width
                         # (tools/ae_menu_pictures.py's lobby_band), outlined when focused; the label at 13, the
                         # spinner as far from the right end as Settings' is)
                         [("width", 366), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/ae_option_bkds_lobby"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/ae_bots_{key}_row_label"/>',
                          f'<child widget="{base}/ae_bots_{key}_row_spinner" x="174" y="1"/>'])
        lines += _widget(f"{base}/ae_bots_{key}_row_label",
                         [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("text", caption),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/ae_bots_{key}_row_spinner",
                         [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("strings", "|".join(strings)), ("setting", setting), ("values", "|".join(values)),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")],
                         ['<on event="created" run="port setting load"/>'])
    lines += _widget(f"{base}/lobby_desc", [("width", 640), ("height", 480)],
                     ['<child widget="main_menu/current_profile_name"/>',
                      f'<child widget="{base}/lobby_right_item" x="22" y="2"/>'])
    lines += _widget(f"{base}/lobby_right_item", [("controller", 1), ("left", 406), ("top", 75), ("width", 162),
                                                  ("height", 326),
                                                  ("bitmap", "bitmaps/spinner_list_3_wide_item_background")],
                     # (a campaign level's picture and name in place of the
                     # map's, for network co-op: menu_functions.c's
                     # lobby_map_show)
                     [f'<child widget="{base}/lobby_map_pic"/>', f'<child widget="{base}/lobby_map_name"/>',
                      '<child widget="main_menu/solo_level_select/replay_level_right_pic"/>',
                      '<child widget="main_menu/solo_level_select/replay_level_right_name"/>',
                      f'<child widget="{base}/lobby_game_data"/>'])
    lines += _widget(f"{base}/lobby_map_pic", [("controller", 1), ("left", 419), ("top", 87), ("width", 140),
                                               ("height", 114), ("bitmap", "ui\\shell\\bitmaps\\mp_map_grafix")], [])
    lines += _widget(f"{base}/lobby_map_name", [("type", "text"), ("controller", 1), ("left", 417), ("top", 204),
                                                ("width", 146), ("height", 43), ("string_list", "main_menu/mp_map_list"),
                                                ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/lobby_game_data", [("type", "text"), ("controller", 1), ("left", 417), ("top", 250),
                                                 ("width", 146), ("height", 144), ("font", "ui\\small_ui"),
                                                 ("color", "#FF2896FF")], [])
    # (its words: menu_functions.c's lobby_update. "<name> - dedicated": the
    # game's host is a dedicated server)
    lines += _strings(f"{base}/lobby_words", ["dedicated"])
    lines += _widget(f"{base}/lobby_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                  ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     # (SWITCH TEAM first: hidden in a game without teams, which
                     # moves the focus to the first shown, START NOW; at the end
                     # it traps none)
                     # (AE: SHUFFLE first, the host's in a team game (ae_team_shuffle.c), hidden as SWITCH
                     # TEAM is; the five 128 wide (the button picture's own width: narrower clips its right end),
                     # side by side across the 640)
                     [f'<child widget="{base}/lobby_button_shuffle" x="0" y="1"/>',
                      f'<child widget="{base}/lobby_button_team" x="128" y="1"/>',
                      f'<child widget="{base}/lobby_button_start" x="256" y="1"/>',
                      f'<child widget="{base}/lobby_button_add" x="384" y="1"/>',
                      f'<child widget="{base}/lobby_button_leave" x="512" y="1"/>'])
    for key, caption, handlers in (
        ("add", "ADD PLAYER", ['<on event="a" run="port lobby add player"/>',
                               '<on event="start" run="port lobby add player"/>']),
        ("team", "SWITCH TEAM", ['<on event="a" run="swap player team"/>', '<on event="start" run="swap player team"/>']),
        ("start", "START NOW", ['<on event="a" run="net game speed start"/>',
                                '<on event="start" run="net game speed start"/>']),
        ("leave", "LEAVE", ['<on event="a" run="mouse emit back event"/>',
                            '<on event="start" run="mouse emit back event"/>']),
        ("shuffle", "SHUFFLE", ['<on event="a" run="ae lobby shuffle teams"/>',
                                '<on event="start" run="ae lobby shuffle teams"/>'])):
        lines += _widget(f"{base}/lobby_button_{key}", [("type", "text"), ("width", 128), ("height", 24),
                                                       ("bitmap", "bitmaps/text_button_background"),
                                                       ("text", caption), ("font", "ui\\small_ui"),
                                                       ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                         handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])
    # a split screen player's profile, chosen as they join the lobby (any
    # controller's presses: Co-op's rows; the host's countdown waits)
    lines += _widget(f"{base}/player_profile_screen", [("width", 640), ("height", 480),
                                                       ("flags", "pass_unhandled_to_focused_child"),
                                                       ("bitmap", "bitmaps/gradient")],
                     ['<on event="created" run="net server defer start"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/player_profile_list"/>',
                      f'<child widget="{base}/header_add_player"/>',
                      f'<child widget="{base}/player_profile_help" x="20" y="416"/>'])
    lines += _header(f"{base}/header_add_player", f"{base}/header_add_player")
    lines += _widget(f"{base}/player_profile_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", "main_menu/profile_manager/player_profile_extended_desc")],
                     ['<data input="3wide player profile list update"/>',
                      '<on event="created" run="port lobby player list initialize"/>',
                      '<on event="deleted" run="player profile list dispose"/>',
                      '<on event="custom_activation" run="port lobby player choose" back="true" branch="true"/>',
                      *[f'<child widget="{MT}/coop/list_item_{index}" x="20" y="{73 + 30 * index}"/>'
                        for index in range(11)],
                      f'<child widget="{MT}/coop/player_2_button_bar" y="414"/>'])
    lines += _widget(f"{base}/player_profile_help", [("type", "text"), ("width", 350), ("height", 24),
                                                     ("text", "The new player's profile."),
                                                     ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_y", 5),
                                                     ("text_flags", "no_focus_test")], [])
    # a game under way's lobby, before joining it (the browser's rows of
    # games in progress): what its advertisement tells, JOIN GAME. Split
    # screen players join here, as in the lobby, before JOIN GAME: the game
    # starts at once for the machine, with the players it brings
    lines += _widget(f"{base}/preview_screen", [("width", 640), ("height", 480),
                                                ("flags", "pass_unhandled_to_focused_child"),
                                                ("bitmap", "bitmaps/gradient")],
                     ['<on event="created" run="port lobby open"/>',
                      '<on event="b" run="port lobby preview leave" back="true"/>',
                      '<on event="back" run="port lobby preview leave" back="true"/>',
                      f'<on event="start" run="port lobby preview add" open="{base}/player_profile_screen" branch="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/preview_list"/>',
                      f'<child widget="{base}/header_lobby"/>',
                      f'<child widget="{base}/lobby_join_help"/>'])
    lines += _widget(f"{base}/preview_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                              ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                              ("description", f"{base}/lobby_desc")],
                     ['<data input="port lobby preview update"/>',
                      f'<child widget="{base}/preview_status" x="30" y="75"/>',
                      f'<child widget="{base}/preview_button_bar" y="414"/>'])
    lines += _widget(f"{base}/preview_status", [("type", "text"), ("controller", 1), ("width", 360), ("height", 300),
                                                ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/preview_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                    ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/preview_button_join" x="250" y="1"/>',
                      f'<child widget="{base}/preview_button_add" x="380" y="1"/>',
                      f'<child widget="{base}/preview_button_back" x="510" y="1"/>'])
    for key, caption, run in (("join", "JOIN GAME", "port lobby preview join"),
                              ("add", "ADD PLAYER", "port lobby add player"),
                              ("back", "BACK", "mouse emit back event")):
        lines += _widget(f"{base}/preview_button_{key}", [("type", "text"), ("width", 128), ("height", 24),
                                                         ("bitmap", "bitmaps/text_button_background"),
                                                         ("text", caption), ("font", "ui\\small_ui"),
                                                         ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                         [f'<on event="a" run="{run}"/>', f'<on event="start" run="{run}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


# (AE) a split screen game's lobby rows for its bots: the settings of Settings > Game Options > BOTS (bots.c)
LOBBY_BOT_ROWS = [
    ("count", "BOTS:", "bots.count", ["OFF"] + [str(n) for n in range(1, 16)], [str(n) for n in range(16)]),
    ("skill", "BOT SKILL:", "bots.skill", ["RECRUIT", "EASY", "NORMAL", "HEROIC", "LEGENDARY", "DEMON"],
     ["recruit", "easy", "normal", "heroic", "legendary", "demon"]),
    ("playfight", "PLAYFIGHT:", "bots.playfight", ["OFF", "ON"], ["false", "true"]),
    ("teams", "BOT TEAMS:", "bots.teams", ["EVEN", "AGAINST YOU"], ["even", "against"]),
]


def _split_screen() -> list:
    """(AE) SPLIT SCREEN, after Create Game's LAN: the JOIN screen (who plays, on which device, with which profile:
    ae_split_screen.c, port/linux/src/ae_seats.c), then a game on this PC only, the map and the gametype, then the
    lobby"""
    return _widget(f"{MT}/multiplayer_type_split_screen_item",
                   [("type", "text"), ("left", 51), ("width", 232), ("height", 32), ("bitmap", "bitmaps/list_item_bkd"),
                    ("string_list", f"{MT}/multiplayer_options"), ("string_index", 8), ("font", "ui\\large_ui"),
                    ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                   [f'<on event="a" open="{JOIN}/join_screen"/>', f'<on event="start" open="{JOIN}/join_screen"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>']) + _join_screen()


# (AE) split screen's JOIN screen (spec notes/specs/2026-10-16-split-screen-pc.md 4.2; its pictures
# tools/ae_seat_pictures.py): four cards across the 640, one a seat
JOIN = f"{MT}/split_screen"
JOIN_CARD_LEFT, JOIN_CARD_TOP, JOIN_CARD_STEP, JOIN_CARD_WIDTH = 16, 102, 154, 146
JOIN_BLUE, JOIN_WHITE, JOIN_DIM, JOIN_NAVY = "#FF2896FF", "#FFFFFFFF", "#FF1B5A96", "#FF000E23"
# (an empty card's OR PICK ONE and its pill, units from the card's top: the label 19 below the legend's last row's
# text, 7 of air between its letters and the pill, the pill 17 above the card's inner edge (1080p, owner 2026-10-18:
# the label sat on the pill))
JOIN_PICK_LABEL, JOIN_PICK_TOP = 231, 251


def join_handler_names() -> list:
    """the JOIN screen's handlers with a seat in their names (menu_tags.c lists them, on an AE hook line of their own)"""
    kinds = ("dev left", "dev right", "dev pick", "prof left", "prof right", "prof pick", "assign", "leave",
             "cand left", "cand right", "cand take")
    return [f"ae split {kind} {seat}" for kind in kinds for seat in range(4)] + ["ae split row up", "ae split row down",
                                                                                 "ae split own assign"]


def _join_card_items(seat: int, x: int, y: int, w: int) -> list:
    """a card's mouse targets: a column list of its own (its children are items: a click gives one the focus and
    presses A on it), each item running its seat's handler"""
    items, lines = [], []

    def item(name: str, handler: str, ix: int, iy: int, width: int, height: int, pairs=None):
        nonlocal lines
        lines += _widget(f"{JOIN}/{name}_{seat}", [("width", width), ("height", height)] + (pairs or []),
                         [f'<on event="a" run="ae split {handler} {seat}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
        items.append(f'<child widget="{JOIN}/{name}_{seat}" x="{ix}" y="{iy}"/>')

    # (a pill: its left and right 40 % are its arrows, the middle 20 % picks)
    for pill, top in (("dev", 54), ("prof", 104), ("cand", JOIN_PICK_TOP)):
        left, right, pick = ("cand left", "cand right", "cand take") if pill == "cand" else (
            f"{pill} left", f"{pill} right", f"{pill} pick")
        item(f"join_hit_{pill}_l", left, 8, top, 52, 28)
        item(f"join_hit_{pill}_m", pick, 60, top, 26, 28)
        item(f"join_hit_{pill}_r", right, 86, top, 52, 28)
    button = [("type", "text"), ("bitmap", "bitmaps/ae_seat_button_assign"), ("font", "ui\\small_ui"),
              ("color", JOIN_WHITE), ("align", "center"), ("text_y", 2)]
    item("join_assign", "assign", 5, 272, 58, 24, button + [("text", "ASSIGN")])
    item("join_cancel", "assign", 5, 272, 58, 24, button + [("text", "CANCEL")])
    # (LEAVE: the device's own leave key just left of the button, as a legend reads: a pad's B chip or the keyboard's
    # ESC key; the button its text alone, so both fit at 1080p, where the text is widest)
    leave = [p for p in button if p[0] != "bitmap"] + [("bitmap", "bitmaps/ae_seat_button_leave"), ("text", "LEAVE")]
    item("join_leave", "leave", 89, 272, 54, 24, leave)
    lines += _join_picture(f"join_leavebtn_{seat}", "bitmaps/ae_seat_buttons", 16, 16)
    items.append(f'<child widget="{JOIN}/join_leavebtn_{seat}" x="69" y="276"/>')
    lines += _join_picture(f"join_leavekey_{seat}", "bitmaps/ae_seat_key_esc", 22, 14)
    items.append(f'<child widget="{JOIN}/join_leavekey_{seat}" x="65" y="277"/>')
    return _widget(f"{JOIN}/join_items_{seat}", [("type", "column_list"), ("width", w), ("height", 300)], items) + lines


def _join_text(name: str, text, font: str, color: str, x: int, y: int, width: int, align="center", height=None,
               text_y=None):
    """a line of the JOIN screen (its text given, or set by ae_split_screen.c)"""
    big = font == "large"
    return _widget(f"{JOIN}/{name}", [("type", "text"), ("controller", 1), ("width", width),
                                      ("height", height or (22 if big else 16)), ("text", text or None),
                                      ("font", "ui\\large_ui" if big else "ui\\small_ui"), ("color", color),
                                      ("align", align), ("text_y", text_y), ("text_flags", "no_focus_test")], [])


def _join_picture(name: str, bitmap: str, width: int, height: int):
    return _widget(f"{JOIN}/{name}", [("controller", 1), ("width", width), ("height", height), ("bitmap", bitmap)], [])


def _join_screen() -> list:
    """the screen, its cards (each piece the screen's own child, at its place: ae_split_screen.c shows a card's
    seated or empty pieces), CONTINUE and BACK"""
    children, lines = [], []

    def place(name: str, x: int, y: int):
        children.append(f'<child widget="{JOIN}/{name}" x="{x}" y="{y}"/>')

    lines += _header(f"{JOIN}/header_split_screen", f"{JOIN}/header_split_screen")
    lines += _join_picture("join_strip", "bitmaps/ae_seat_strip", 608, 28)
    place("join_strip", 16, 72)
    lines += _join_text("join_rule", "The first to press START (or ENTER) is PLAYER 1, the next PLAYER 2 ...", "small",
                        JOIN_WHITE, 0, 0, 608)
    place("join_rule", 16, 78)
    for seat in range(4):
        x, y, w = JOIN_CARD_LEFT + JOIN_CARD_STEP * seat, JOIN_CARD_TOP, JOIN_CARD_WIDTH
        title = f"PLAYER {seat + 1}"
        lines += _join_picture(f"join_card_{seat}", "bitmaps/ae_seat_card", w, 300)
        place(f"join_card_{seat}", x, y)
        # (the title: seated, empty, held, and white while the seat's device is pressed or its pill flashes)
        for key, color in (("on", JOIN_BLUE), ("off", JOIN_DIM), ("held", JOIN_DIM), ("flash", JOIN_WHITE)):
            lines += _join_text(f"join_title_{key}_{seat}", title, "large", color, 0, 0, w)
            place(f"join_title_{key}_{seat}", x, y + 7)
        # (a seated player: the device's picture, then CONTROLLER over its pill, PROFILE over its pill; the pills'
        # bars are frame 0 at rest, 1 the focus's ice-blue glow (and ASSIGN's wait, pulsing), 2 filled (a flash))
        # (sizes from 1080p, where the text is largest beside the pictures: the labels left over their pills, the
        # device's small picture right of CONTROLLER; the armour at its own size, which a picture always is)
        lines += _join_picture(f"join_device_{seat}", "bitmaps/ae_seat_devices", 30, 16)
        place(f"join_device_{seat}", x + w - 37, y + 36)
        lines += _join_text(f"join_devlabel_{seat}", "CONTROLLER", "small", JOIN_BLUE, 0, 0, 96, align="left")
        place(f"join_devlabel_{seat}", x + 12, y + 35)
        for pill, top in (("dev", 54), ("prof", 104)):
            lines += _join_picture(f"join_{pill}bar_{seat}", "bitmaps/ae_seat_profile_bar", 130, 28)
            place(f"join_{pill}bar_{seat}", x + 8, y + top)
            lines += _join_picture(f"join_{pill}arrow_l_{seat}", "bitmaps/arrow_sm_left", 12, 12)
            place(f"join_{pill}arrow_l_{seat}", x + 13, y + top + 8)
            lines += _join_picture(f"join_{pill}arrow_r_{seat}", "bitmaps/arrow_sm_right", 12, 12)
            # (the arrows' ink is the left 6 units of their pictures: the right one 19 in from the card's edge, so
            # each sits 5 units inside its pill's end and the value has the same room both sides)
            place(f"join_{pill}arrow_r_{seat}", x + w - 19, y + top + 8)
            # (the value: large when it fits, small when long; white at rest, navy on the filled flash)
            for size in ("large", "small"):
                for tone, color in (("", JOIN_WHITE), ("inv", JOIN_NAVY)):
                    name = f"join_{pill}{size}{tone}_{seat}"
                    lines += _join_text(name, "", size, color, 0, 0, 108)
                    place(name, x + 19, y + top + (4 if size == "large" else 7))
        # (ASSIGN's wait: the pill's whole width, no arrows)
        lines += _join_text(f"join_devwait_{seat}", "PRESS A BUTTON", "small", JOIN_WHITE, 0, 0, 126)
        place(f"join_devwait_{seat}", x + 10, y + 61)
        lines += _join_text(f"join_proflabel_{seat}", "PROFILE", "small", JOIN_BLUE, 0, 0, 96, align="left")
        place(f"join_proflabel_{seat}", x + 12, y + 85)
        lines += _join_picture(f"join_armour_{seat}", "ui\\shell\\bitmaps\\colors_sm", 112, 136)
        # (the figure is drawn left in its frame, the profile panel's picture: placed so its figure is centred)
        place(f"join_armour_{seat}", x + 29, y + 134)
        # (an empty seat: the ring is the card's own picture; then the join buttons' legend, then OR PICK ONE: a
        # free device's pill a click seats)
        lines += _join_text(f"join_press_{seat}", "PRESS START", "large", JOIN_WHITE, 0, 0, w)
        place(f"join_press_{seat}", x, y + 88)
        lines += _join_text(f"join_tojoin_{seat}", "TO JOIN", "small", JOIN_BLUE, 0, 0, w)
        place(f"join_tojoin_{seat}", x, y + 112)
        for row, label in enumerate(("START", "OPTIONS", "PLUS", "ENTER / SPACE")):
            lines += _join_picture(f"join_glyph{row}_{seat}", "bitmaps/ae_seat_buttons", 16, 16)
            place(f"join_glyph{row}_{seat}", x + 9, y + 134 + 22 * row)
            lines += _join_text(f"join_glyphtext{row}_{seat}", label, "small", JOIN_WHITE, 0, 0, w - 36, align="left")
            place(f"join_glyphtext{row}_{seat}", x + 30, y + 135 + 22 * row)
        lines += _join_text(f"join_pick_{seat}", "OR PICK ONE", "small", JOIN_BLUE, 0, 0, w)
        place(f"join_pick_{seat}", x, y + JOIN_PICK_LABEL)
        lines += _join_picture(f"join_candbar_{seat}", "bitmaps/ae_seat_profile_bar", 130, 28)
        place(f"join_candbar_{seat}", x + 8, y + JOIN_PICK_TOP)
        lines += _join_picture(f"join_candarrow_l_{seat}", "bitmaps/arrow_sm_left", 12, 12)
        place(f"join_candarrow_l_{seat}", x + 13, y + JOIN_PICK_TOP + 8)
        lines += _join_picture(f"join_candarrow_r_{seat}", "bitmaps/arrow_sm_right", 12, 12)
        place(f"join_candarrow_r_{seat}", x + w - 19, y + JOIN_PICK_TOP + 8)
        for size, color in (("large", JOIN_WHITE), ("small", JOIN_WHITE), ("none", JOIN_DIM)):
            lines += _join_text(f"join_cand{size}_{seat}", "NO FREE PAD" if size == "none" else "",
                                "small" if size != "large" else "large", color, 0, 0, 108)
            # (a unit higher than the other pills' values: the text's shadow falls below it, so centred it sat low)
            place(f"join_cand{size}_{seat}", x + 19, y + JOIN_PICK_TOP + (3 if size == "large" else 6))
        # (what the mouse clicks: a list of the card's own, its items over the pills' arrows and values and its
        # buttons; ae_split_screen.c hides the items of the state the card is not in)
        lines += _join_card_items(seat, x, y, w)
        children.append(f'<child widget="{JOIN}/join_items_{seat}" x="{x}" y="{y}"/>')
    # (the help lines as the lobby's: 24 high, text 5 down)
    lines += _join_text("join_help_ready", "PLAYER 1 picks the map and gametype next.", "small", JOIN_BLUE, 0, 0, 350,
                        align="left", height=24, text_y=5)
    place("join_help_ready", 20, 414)
    lines += _join_text("join_help_nobody", "Nobody has joined yet.", "small", JOIN_BLUE, 0, 0, 350, align="left",
                        height=24, text_y=5)
    place("join_help_nobody", 20, 414)
    lines += _join_text("join_message", "", "small", JOIN_WHITE, 0, 0, 600, align="left", height=24, text_y=5)
    place("join_message", 20, 441)
    lines += _join_text("join_more", "Up to 4 players on one PC.", "small", JOIN_BLUE, 0, 0, 280,
                        align="right", height=24, text_y=5)
    place("join_more", 350, 441)
    # (CONTINUE and BACK where the lobby's last two buttons are; left and right are each player's own profile, so
    # they do not move between the buttons: the mouse clicks BACK, a device with no seat's B goes back)
    profile_keys = ['<on event="left" run="ae split profile left"/>',
                    '<on event="right" run="ae split profile right"/>',
                    '<on event="stick_left" run="ae split profile left"/>',
                    '<on event="stick_right" run="ae split profile right"/>']
    lines += _widget(f"{JOIN}/join_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                 ("flags", "pass_unhandled_to_focused_child")],
                     [f'<child widget="{JOIN}/split_screen_continue" x="380" y="1"/>',
                      f'<child widget="{JOIN}/split_screen_back" x="510" y="1"/>'])
    # (CONTINUE dim while nobody sits: drawn over the live one, which a press then finds with no one to go on with)
    lines += _widget(f"{JOIN}/join_continue_off",
                     [("type", "text"), ("controller", 1), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "CONTINUE"), ("font", "ui\\small_ui"),
                      ("color", JOIN_DIM), ("align", "center"), ("text_y", 2), ("text_flags", "no_focus_test")], [])
    # (the button's name has "split_scr": menu_functions.c's multiplayer_mode_set reads the mode from it)
    lines += _widget(f"{JOIN}/split_screen_continue",
                     [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
                      ("text", "CONTINUE"), ("font", "ui\\small_ui"), ("color", JOIN_WHITE), ("align", "center"),
                      ("text_y", 2)],
                     ['<on event="a" run="ae split continue"/>',
                      f'<on event="a" run="mp type set mode" open="{MT}/connected/connected_map_select_wrapper"/>',
                      '<on event="start" run="ae split continue"/>',
                      f'<on event="start" run="mp type set mode" open="{MT}/connected/connected_map_select_wrapper"/>',
                      *profile_keys, '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{JOIN}/split_screen_back",
                     [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
                      ("text", "BACK"), ("font", "ui\\small_ui"), ("color", JOIN_WHITE), ("align", "center"),
                      ("text_y", 2)],
                     ['<on event="a" run="ae split back" back="true"/>', '<on event="start" run="ae split back" back="true"/>',
                      *profile_keys, '<on event="left_mouse" run="mouse emit accept event"/>'])
    screen = _widget(f"{JOIN}/join_screen", [("width", 640), ("height", 480),
                                              ("flags", "pass_unhandled_to_focused_child"),
                                              ("bitmap", "bitmaps/gradient")],
                     ['<data input="ae split join update"/>',
                      '<on event="created" run="ae split join open"/>',
                      '<on event="deleted" run="ae split join close"/>',
                      '<on event="b" run="ae split b" back="true"/>',
                      '<on event="back" run="ae split back key" back="true"/>',
                      '<on event="up" run="ae split row up"/>', '<on event="down" run="ae split row down"/>',
                      '<on event="stick_up" run="ae split row up"/>', '<on event="stick_down" run="ae split row down"/>',
                      '<on event="y" run="ae split own assign"/>',
                      *profile_keys,
                      # (the button bar first: the screen's first item takes the focus, CONTINUE's)
                      f'<child widget="{JOIN}/join_button_bar" y="414"/>',
                      f'<child widget="{JOIN}/header_split_screen"/>', *children,
                      f'<child widget="{JOIN}/join_continue_off" x="380" y="415"/>'])
    return screen + lines


def _coop() -> list:
    """Co-op, the campaign for two players on this machine in split screen
    (the Xbox's Cooperative Play): Multiplayer's CO-OP CAMPAIGN, then player
    2's profile, chosen with player 2's controller (its rows take any
    controller's presses, as the shared rows do only controller 1's), then
    New Game's levels and difficulty, which either player's controller uses
    (ui_widget.c's widget_takes_events_of_controller; menu_functions.c's
    coop_begin)"""
    base = f"{MT}/coop"
    lines = _widget(f"{MT}/multiplayer_type_coop_item",
                    [("type", "text"), ("left", 51), ("width", 232), ("height", 32), ("bitmap", "bitmaps/list_item_bkd"),
                     ("string_list", f"{MT}/multiplayer_options"), ("string_index", 7), ("font", "ui\\large_ui"),
                     ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                    [f'<on event="a" run="port coop begin" open="{base}/player_2_profile_screen" branch="true"/>',
                     f'<on event="start" run="port coop begin" open="{base}/player_2_profile_screen" branch="true"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/player_2_profile_screen", [("width", 640), ("height", 480),
                                                         ("flags", "pass_unhandled_to_focused_child"),
                                                         ("bitmap", "bitmaps/gradient")],
                     ['<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/player_2_profile_list"/>',
                      f'<child widget="{base}/header_player_2"/>',
                      f'<child widget="{base}/player_2_help" x="20" y="416"/>'])
    lines += _header(f"{base}/header_player_2", f"{base}/header_player_2")
    rows = [f'<child widget="{base}/list_item_{index}" x="20" y="{73 + 30 * index}"/>' for index in range(11)]
    lines += _widget(f"{base}/player_2_profile_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", "main_menu/profile_manager/player_profile_extended_desc")],
                     ['<data input="3wide player profile list update"/>',
                      '<on event="created" run="port coop player 2 list initialize"/>',
                      '<on event="deleted" run="player profile list dispose"/>',
                      '<on event="custom_activation" run="port coop player 2" '
                      'open="main_menu/solo_level_select/solo_level_select_screen" branch="true"/>',
                      *rows, f'<child widget="{base}/player_2_button_bar" y="414"/>'])
    for index in range(11):
        lines += _widget(f"{base}/list_item_{index}",
                         [("width", 390), ("height", 28),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                         ['<on event="a" run="single prev cl item activated"/>',
                          '<on event="start" run="single prev cl item activated"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          '<child widget="main_menu/new_select/list_item_text" x="25"/>',
                          '<child widget="main_menu/new_select/list_item_arrows"/>'])
    lines += _widget(f"{base}/player_2_help", [("type", "text"), ("width", 350), ("height", 24),
                                               ("text", "Player 2: choose with your controller."),
                                               ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_y", 5),
                                               ("text_flags", "no_focus_test")], [])
    lines += _widget(f"{base}/player_2_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                     ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      '<child widget="main_menu/profile_manager/profile_manager_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>'])
    return lines


def _item_options_extras() -> list:
    """Item Options' rows of the port's: the map's weapons (YES or NO), and
    the loadout, CATEGORY (the weapon set) or CUSTOM (each player's primary
    and secondary weapons)"""
    base = "main_menu/settings_select/multiplayer_setup/item_options_edit"
    lines = []
    for key, strings, label in (("map_weapons", "var_map_weapons", 8), ("loadout", "var_loadout", 5),
                                ("primary_weapon", "var_loadout_weapon", 6),
                                ("secondary_weapon", "var_loadout_weapon", 7)):
        lines += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                               ("flags", "pass_unhandled_to_focused_child"),
                                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{key}_label"/>',
                          f'<child widget="{base}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                                   ("string_list", f"{base}/item_options_labels"),
                                                   ("string_index", label), ("font", "ui\\large_ui"),
                                                   ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/{strings}"), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                          ("align", "center"), ("text_y", 4), ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    lines += _strings(f"{base}/var_map_weapons", ["YES", "NO"])
    lines += _strings(f"{base}/var_loadout", ["CATEGORY", "CUSTOM"])
    lines += _strings(f"{base}/var_loadout_weapon", LOADOUT_WEAPONS + [LOADOUT_PACK_WEAPON])
    return lines


# ARENA OPTIONS' rows: (key, label, values, their helps); each row's
# spinner is named for its option in menu_functions.c's gametype_options
# (KEY_spinner), and its helps are its values' in turn (gametype_option_help).
# TIME LIMIT is the same setting as the Halo PC gametype screens' own, here
# for every gametype, in finer steps and to an hour (menu_functions.c's
# ARENA_TIME_LIMITS, minutes)
ARENA_TIME_LIMITS = [*range(1, 11), 12, 15, 20, 25, 30, 45, 60]
# (the rows' pitch and the help's top: 20 for thirteen, the help just under the
# last row and its third line clear of the buttons' bar)
ARENA_ROW_TOP, ARENA_ROW_PITCH, ARENA_HELP_TOP, ARENA_HELP_HEIGHT = 68, 20, 340, 66
ARENA_ROWS = [
    ("arena_time_limit", "TIME LIMIT:",
     ["NONE", *[f"{minutes} MINUTE{'' if minutes == 1 else 'S'}" for minutes in ARENA_TIME_LIMITS]], [
        "No time limit: the game ends only when someone\\nreaches the score to win.",
        *[f"The game ends after {minutes} minute{'' if minutes == 1 else 's'}, or sooner\\nwhen someone "
          "reaches the score to win." for minutes in ARENA_TIME_LIMITS],
    ]),
    ("falling_damage", "FALL DAMAGE:", ["ON", "OFF"], [
        "A hard landing hurts, and a long enough fall kills.",
        "Landings never hurt, from any height. Pits and\\nleaving the map still kill.",
    ]),
    ("health_regeneration", "HEALTH:", ["HALO CE", "REACH", "HALO 2", "HALO 3"], [
        "Your health only comes back with a health pack.",
        "Health comes back 4 s after\\nyou were last hurt, up to the top of its third;\\nshields refill in 3 s. Packs fill the rest.",
        "Your health comes back 10 s after\\nyou were last hurt, with or without shields.",
        "The same as HALO 2: your health\\ncomes back 10 s after you were last hurt.",
    ]),
    # (COMBAT FEEDBACK: bits 0-1 of the variant's pad5, port/linux/game/ae_hit_feedback.c's
    # ae_hit_feedback_menu_rows; not in menu_functions.c's table)
    ("hit_feedback", "COMBAT FEEDBACK:", ["HALO CE", "ARENA EVOLVED"], [
        "Shields and hits look as in Halo CE.",
        "Shields show their state: a pop when one headshot\\nwill finish, a flicker while down, a sweep as they\\nrecharge; a blood pop on the kill. No damage change.",
    ]),
    ("no_spread", "NO SPREAD:", ["OFF", "FIRST SHOT", "FULL"], [
        "The pistol and sniper rifle spread as in stock Halo.",
        "The pistol's first shot from rest, and every\\nunzoomed sniper rifle shot, go exactly where\\nyou aim. Held pistol fire still spreads.",
        "Every pistol and sniper rifle shot goes exactly\\nwhere you aim, held fire too: no spread and no\\nbloom. Other weapons are unchanged.",
    ]),
    ("pregame_countdown", "PRE-GAME COUNTDOWN:", ["OFF", "ON"], [
        "The game starts at once.",
        "A 3-2-1 countdown on a black screen starts the\\ngame, so everyone sees the map at the same time.",
    ]),
    ("practice", "PRACTICE MODE:", ["OFF", "ON"], [
        "Weapons and powerups respawn as the map sets.",
        "Every weapon and powerup respawns every 30\\nseconds, for practising grenade tricks.",
    ]),
    # (game_engine.h's enum drop_secondary: bits 29-30; the host's rule)
    ("drop_secondary", "DROP SECONDARY:", ["HALO CE", "ALWAYS", "EXCEPT POWER"], [
        "As Halo CE: your spawn's second weapon vanishes\\nwhen you die; one you put away from hand stays\\n30 seconds.",
        "Both your weapons drop when you die, and stay\\n30 seconds.",
        "Both drop and stay 30 seconds, but a holstered\\nrocket launcher, sniper rifle or shotgun vanishes.",
    ]),
    # (SILENT MOVEMENT and SILENT WEAPONS: the variant's pad5 high nibble,
    # port/linux/game/ae_silent.c's ae_silent_menu_rows; not in
    # menu_functions.c's table. SOUND PRESET is not kept: it shows SPLIT
    # SCREEN while both are SILENT, and picking it sets both, as the vehicle
    # screen's PRESET row picks a set (ae_silent_menu_update))
    ("sound_preset", "SOUND PRESET:", ["CUSTOM", "SPLIT SCREEN"], [
        "SILENT MOVEMENT and SILENT WEAPONS as set\\nbelow.",
        "For players sharing speakers: SILENT MOVEMENT\\nand SILENT WEAPONS both SILENT, so nobody\\nhears the other side's steps or weapon swaps.",
    ]),
    ("silent_movement", "SILENT MOVEMENT:", ["NORMAL", "JUST ME", "SILENT"], [
        "Footsteps, jumps and landings sound as in Halo CE.",
        "You hear your own footsteps, jumps and landings;\\neveryone else moves silently. Only Arena Evolved\\nplayers can join.",
        "Nobody's footsteps, jumps or landings make a\\nsound, yours included. Only Arena Evolved\\nplayers can join.",
    ]),
    ("silent_weapons", "SILENT WEAPONS:", ["NORMAL", "JUST ME", "SILENT"], [
        "Readying and putting away weapons sound as in\\nHalo CE.",
        "You hear your own weapon swaps; everyone else's\\nare silent (firing and reloading still sound).\\nOnly Arena Evolved players can join.",
        "Nobody's weapon swaps make a sound, yours\\nincluded (firing and reloading still sound).\\nOnly Arena Evolved players can join.",
    ]),
    # (CONTROLLER CROUCH: bit 8 of the variant's flags, ae_crouch_rules.h;
    # menu_functions.c's gametype_options. STAYS DOWN first, as the AE
    # gametypes have it; a gametype without the bit shows HALO CE)
    ("controller_crouch", "CONTROLLER CROUCH:", ["STAYS DOWN", "HALO CE"], [
        "Holding crouch on a controller keeps you down,\\nstick pushed all the way too, as on a keyboard.",
        "As Halo CE: on a controller, pushing the stick\\nall the way stands you up from a crouch.",
    ]),
    # (universal_variant.ball_melee; Oddball only: the row is hidden for the
    # other gametype types, menu_functions.c gametype_option_help)
    ("ball_melee", "BALL MELEE:", ["STOCK", "LETHAL"], [
        "Oddball only: the ball carrier's melee is as in\\nHalo CE: it takes the shield and a little health,\\nand kills from behind.",
        "Oddball only: the ball carrier's melee kills in\\none hit, shields and all. Same\\nreach, no hits through walls.",
    ]),
]

# NHE OPTIONS' rows, moved from ARENA OPTIONS. Their spinner names and help
# stay unchanged, so menu_functions.c's generic gametype option loader and
# saver read and write the same fields.
NHE_ROWS = [
    # (game_engine.h's enum nhe_mode, in its order; only on Halo 1: NHE's
    # maps, where it picks the vehicle set their scripts read)
    ("nhe_mode", "NHE MODE:", ["BY VEHICLE SET", "VANILLA", "TIMER ONLY", "NHE & TIMER", "NHE & POWERUPS", "TRAINING"], [
        "Halo 1: NHE's maps only: the vehicle set picks\\nNHE's mode, as NHE did. On other maps the\\ngametype's vehicles apply.",
        "On Halo 1: NHE's maps: no timers, as retail\\nwith NHE's messages. On other maps the\\ngametype's vehicles apply.",
        "On Halo 1: NHE's maps: NHE's talking and\\non-screen timers. On other maps the\\ngametype's vehicles apply.",
        "On Halo 1: NHE's maps: NHE's talking and\\non-screen timers (its host teleport stays off:\\nAE hosts neutrally). Other maps: the gametype's.",
        "On Halo 1: NHE's maps: NHE's timers and its\\npowerup calls. On other maps the gametype's\\nvehicles apply.",
        "On Halo 1: NHE's maps: NHE's training: the\\ncountdown, spawn markers, randoms and waypoints.\\nOther maps: the gametype's vehicles.",
    ]),
    # (_game_variant_nhe_extras_bit, 28)
    ("nhe_extras", "NHE EXTRAS:", ["OFF", "ON"], [
        "Stock teams and camera.",
        "As Halo 1: NHE: everyone joins red and keeps their\\ncolour between games, the dead camera stays on you,\\nand the match clock counts up.",
    ]),
]
# (AE: NHE OPTIONS' own row of this machine's settings, after the gametype's: a setting spinner, as the settings
# screens' (its value loaded on "port setting load", written by OK's "port settings save"; nothing in the
# gametype, nothing over the network): ae_sound_filter.c)
NHE_SETTING_ROWS = [
    ("nhe_sound_filter", "NHE SOUND FILTER:", "audio.nhe_sound_filter", ON_OFF, [
        "Your game only, any map: mutes the plasma pistol's\\ncharging hum and the needler's shots, as Halo 1:\\nNHE's maps did.",
        "Your game only: the plasma pistol's charging hum\\nand the needler's shots play as usual.",
    ]),
]


def _arena_options() -> list:
    """ARENA OPTIONS, the gametype editor's screen of the port's gameplay
    options (game_engine.h's _game_variant_..._bit from 16), in Player
    Options' style: a row of a label and a spinner for each, the help of
    the value chosen, OK and CANCEL. Its list is created and saved as Player
    Options' is, by its spinners' names (menu_functions.c's gametype_options:
    "mp profile init player opts", "mp profile set player options"); and
    the edit list's item that opens it (WIDGET_PATCHES)"""
    base = ARENA_OPTIONS
    screen = f"{base}/arena_options_screen"
    lines = _widget(f"{PLAYLIST_EDIT}/playlist_edit_arena_item",
                    [("type", "text"), ("left", 51), ("top", 309), ("width", 232), ("height", 32),
                     ("bitmap", "bitmaps/list_item_bkd"), ("string_list", f"{PLAYLIST_EDIT}/playlist_edit_options"),
                     ("string_index", 8), ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                     ("text_y", 5)],
                    [f'<on event="a" open="{screen}"/>', f'<on event="start" open="{screen}"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(screen, [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                              ("bitmap", "bitmaps/gradient")],
                     [f'<child widget="{base}/header_arena_options"/>', f'<child widget="{base}/arena_options_menu"/>'])
    lines += _header(f"{base}/header_arena_options", f"{base}/header_arena_options")
    lines += _widget(f"{base}/arena_options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/arena_options_help")],
                     ['<data input="game settings lists text update"/>',
                      '<on event="created" run="mp profile init player opts"/>',
                      *[f'<child widget="{base}/op_{key}" x="54" y="{ARENA_ROW_TOP + ARENA_ROW_PITCH * index}"/>'
                        for index, (key, *_) in enumerate(ARENA_ROWS)],
                      f'<child widget="{base}/arena_button_bar" y="414"/>'])
    lines += _widget(f"{base}/arena_options_help",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", ARENA_HELP_TOP), ("width", 482),
                      ("height", ARENA_HELP_HEIGHT), ("string_list", f"{base}/cap_arena_options"), ("font", "ui\\large_ui"),
                      ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/arena_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      f'<child widget="{base}/arena_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _button(f"{base}/arena_button_ok", 1,
                     ['<on event="a" run="mp profile set player options" back="true"/>',
                      '<on event="start" run="mp profile set player options" back="true"/>'])
    for index, (key, _, values, _) in enumerate(ARENA_ROWS):
        lines += _widget(f"{base}/op_{key}",
                         [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{key}_label"/>',
                          f'<child widget="{base}/{key}_spinner" x="320" y="1"/>'])
        lines += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                          ("string_list", f"{base}/arena_options_labels"), ("string_index", index or None),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("left", 2), ("top", 2), ("width", 176), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/var_{key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 178 19 184")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{base}/var_{key}", values)
    lines += _strings(f"{base}/arena_options_labels", [label for _, label, _, _ in ARENA_ROWS])
    lines += _strings(f"{base}/cap_arena_options", [text for *_, helps in ARENA_ROWS for text in helps])
    return lines


def _nhe_options() -> list:
    """NHE OPTIONS, the gametype editor's Halo 1: NHE settings, loaded
    and saved by the same spinner-name functions as ARENA OPTIONS."""
    base = NHE_OPTIONS
    screen = f"{base}/nhe_options_screen"
    lines = _widget(f"{PLAYLIST_EDIT}/playlist_edit_nhe_item",
                    [("type", "text"), ("left", 51), ("top", 342), ("width", 232), ("height", 32),
                     ("bitmap", "bitmaps/list_item_bkd"), ("string_list", f"{PLAYLIST_EDIT}/playlist_edit_options"),
                     ("string_index", 9), ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                     ("text_y", 5)],
                    [f'<on event="a" open="{screen}"/>', f'<on event="start" open="{screen}"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(screen, [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                              ("bitmap", "bitmaps/gradient")],
                     [f'<child widget="{base}/header_nhe_options"/>', f'<child widget="{base}/nhe_options_menu"/>'])
    lines += _header(f"{base}/header_nhe_options", f"{base}/header_nhe_options")
    lines += _widget(f"{base}/nhe_options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/nhe_options_help")],
                     ['<data input="game settings lists text update"/>',
                      '<on event="created" run="mp profile init player opts"/>',
                      *[f'<child widget="{base}/op_{key}" x="54" y="{73 + 30 * index}"/>'
                        for index, (key, *_) in enumerate(NHE_ROWS + NHE_SETTING_ROWS)],
                      f'<child widget="{base}/nhe_button_bar" y="414"/>'])
    lines += _widget(f"{base}/nhe_options_help",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 321), ("width", 482),
                      ("height", 79), ("string_list", f"{base}/cap_nhe_options"), ("font", "ui\\large_ui"),
                      ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/nhe_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      f'<child widget="{base}/nhe_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _button(f"{base}/nhe_button_ok", 1,
                     ['<on event="a" run="port settings save"/>', '<on event="start" run="port settings save"/>',
                      '<on event="a" run="mp profile set player options" back="true"/>',
                      '<on event="start" run="mp profile set player options" back="true"/>'])
    for index, (key, _, values, _) in enumerate(NHE_ROWS):
        lines += _widget(f"{base}/op_{key}",
                         [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{key}_label"/>',
                          f'<child widget="{base}/{key}_spinner" x="320" y="1"/>'])
        lines += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                          ("string_list", f"{base}/nhe_options_labels"), ("string_index", index or None),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("left", 2), ("top", 2), ("width", 176), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/var_{key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 178 19 184")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{base}/var_{key}", values)
    for index, (key, _, setting, choices, _) in enumerate(NHE_SETTING_ROWS, len(NHE_ROWS)):
        lines += _widget(f"{base}/op_{key}",
                         [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{key}_label"/>',
                          f'<child widget="{base}/{key}_spinner" x="320" y="1"/>'])
        lines += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                          ("string_list", f"{base}/nhe_options_labels"), ("string_index", index),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("left", 2), ("top", 2), ("width", 176), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("strings", "|".join(shown for shown, _ in choices)), ("setting", setting),
                          ("values", "|".join(value for _, value in choices)), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 178 19 184")],
                         ['<on event="created" run="port setting load"/>',
                          '<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    lines += _strings(f"{base}/nhe_options_labels", [label for _, label, *_ in NHE_ROWS + NHE_SETTING_ROWS])
    lines += _strings(f"{base}/cap_nhe_options", [text for *_, helps in NHE_ROWS + NHE_SETTING_ROWS for text in helps])
    return lines


def _indicator_options_row(key: str, label: int, width: int = 46) -> list:
    """an Indicator Options row of the port's: its label (the labels' string
    label) and its spinner (strings var_KEY), 46 wide for OFF or ON, wider
    for longer values (centred where the narrow ones are)"""
    lines = _widget(f"{INDICATOR_OPTIONS}/op_{key}",
                    [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                     ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                    [f'<child widget="{INDICATOR_OPTIONS}/{key}_label"/>',
                     f'<child widget="{INDICATOR_OPTIONS}/{key}_spinner" x="{366 - (width - 46) // 2}" y="1"/>'])
    lines += _widget(f"{INDICATOR_OPTIONS}/{key}_label",
                     [("type", "text"), ("controller", 1), ("width", 320), ("height", 22),
                      ("string_list", f"{INDICATOR_OPTIONS}/indicator_options_labels"), ("string_index", label),
                      ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    lines += _widget(f"{INDICATOR_OPTIONS}/{key}_spinner",
                     [("type", "spinner"), ("left", 2), ("top", 2), ("width", width), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("string_list", f"{INDICATOR_OPTIONS}/var_{key}"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4), ("list_flags", "items_from_strings"),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", f"7 {width + 2} 19 {width + 8}")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    return lines


def _indicator_options_extras() -> list:
    """Indicator Options' rows of the port's: TIMERS, OFF / HUD / HUD +
    WAYPOINTS / LINE OF SIGHT (the gametype's bits 17 and 26), TRAINING, OFF
    or ON (_game_variant_training_bit), and
    SPAWN HEAT, ON or OFF (_game_variant_no_spawn_heat_bit, inverted: clear
    is ON)"""
    lines = _indicator_options_row("item_timers", 4, 176)
    lines += _strings(f"{INDICATOR_OPTIONS}/var_item_timers", ["OFF", "HUD", "HUD + WAYPOINTS", "LINE OF SIGHT"])
    lines += _indicator_options_row("training", 5)
    lines += _strings(f"{INDICATOR_OPTIONS}/var_training", ["OFF", "ON"])
    lines += _indicator_options_row("spawn_heat", 6)
    lines += _strings(f"{INDICATOR_OPTIONS}/var_spawn_heat", ["ON", "OFF"])
    return lines


def _player_options_extras() -> list:
    """The host's aim-assist policy and shield effects, hidden while editing
    saved gametypes."""
    base = PLAYER_EDIT
    lines = []
    for key, label_index, words in (("aim_assist", 8, ["ALLOWED", "BLOCK M+KB"]),
                                    ("shield_effects", 9, SHIELD_EFFECTS)):
        lines += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
            ("flags", "pass_unhandled_to_focused_child"), ("bitmap", "bitmaps/option_bkds"),
            ("color", "#FF2896FF")], [f'<child widget="{base}/{key}_label"/>',
            f'<child widget="{base}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1),
            ("width", 280), ("height", 22), ("string_list", f"{base}/player_options_labels"),
            ("string_index", label_index), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
            ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner", [("type", "spinner"), ("top", 2),
            ("width", 206), ("height", 20), ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
            ("string_list", f"{base}/var_{key}"), ("font", "ui\\large_ui"),
            ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
            ("list_flags", "items_from_strings"), ("header_bitmap", "bitmaps/arrow_sm_left"),
            ("footer_bitmap", "bitmaps/arrow_sm_right"), ("header_bounds", "7 -13 19 -7"),
            ("footer_bounds", "7 208 19 214")], ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    return lines + _strings(f"{base}/var_aim_assist", ["ALLOWED", "BLOCK M+KB"]) + \
        _strings(f"{base}/var_shield_effects", SHIELD_EFFECTS)


def _opence_options() -> list:
    """the gametype editor's OPENCE OPTIONS: its row, after the editor's own
    (as they are laid out), and its screen, as the gametype's options' are
    (_setup_option_screen), its spinners the gametype's (menu_functions.c's
    gametype_options), its header its words (no picture of its own)"""
    lines = _setup_option_screen(OPENCE_EDIT, "opence", OPENCE_ROWS)
    lines = [line.replace("port setup options init", "mp profile init opence options")
             .replace("port setup options save", "mp profile set opence options")
             .replace(f'bitmap="{OPENCE_EDIT}/header_opence"',
                      'type="text" text="OPENCE OPTIONS" font="ui\\large_ui" color="#FFFFFFFF" text_x="13" text_y="16"')
             for line in lines]
    # (Arena Evolved: the third row after the editor's own, below ARENA
    # OPTIONS' 309 and NHE OPTIONS' 342)
    lines += _widget(OPENCE_ITEM, [("type", "text"), ("left", 51), ("top", 375), ("width", 232), ("height", 32),
                                   ("bitmap", "bitmaps/list_item_bkd"), ("text", "OPENCE OPTIONS"),
                                   ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" open="{OPENCE_EDIT}/opence_screen"/>',
                      f'<on event="start" open="{OPENCE_EDIT}/opence_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def _teamplay_options_extras() -> list:
    """Teamplay Options' rows of the port's: the gametype's FORCE TEAM
    SPAWNS (game_engine.c), then the host's voice chat
    (port/linux/game/network_voice.c) and vote kicks (network_votekick.c),
    in Server Setup's copy only (menu_functions.c's gametype_options_init),
    kept in config.toml"""
    lines = []
    for index, (key, label, strings, _) in enumerate(TEAMPLAY_EXTRA_ROWS):
        lines += _widget(f"{TEAMPLAY_EDIT}/op_{key}", [("width", 512), ("height", 28),
                                                       ("flags", "pass_unhandled_to_focused_child"),
                                                       ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{TEAMPLAY_EDIT}/{key}_label"/>',
                          f'<child widget="{TEAMPLAY_EDIT}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{TEAMPLAY_EDIT}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                          ("height", 22),
                                                          ("string_list", f"{TEAMPLAY_EDIT}/teamplay_options_labels"),
                                                          ("string_index", 3 + index), ("font", "ui\\large_ui"),
                                                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{TEAMPLAY_EDIT}/{key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{TEAMPLAY_EDIT}/var_{key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{TEAMPLAY_EDIT}/var_{key}", strings)
    return lines


def _map_kind() -> list:
    """the map lists' first row (New Game's and the Map screen's), as the
    gametype list's chooser: a spinner of SINGLEPLAYER or MULTIPLAYER maps,
    for either controller (split screen co-op's New Game takes both)"""
    lines = _widget(MAP_KIND_CHOOSER, [("width", 256), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                       ("bitmap", "bitmaps/option_bkds_sm"), ("font", "ui\\large_ui"),
                                       ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                    [f'<child widget="{MAP_KIND_CHOOSER}_spinner" x="15" y="2"/>'])
    lines += _widget(f"{MAP_KIND_CHOOSER}_spinner",
                     [("type", "spinner"), ("width", 226), ("height", 22), ("flags", "left_right_tabs_items"),
                      ("string_list", "main_menu/new_select/var_map_kinds"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1), ("list_flags", "items_from_strings"),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 226 19 232")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    lines += _strings("main_menu/new_select/var_map_kinds", ["SINGLEPLAYER", "MULTIPLAYER"])
    return lines


# MCC's Custom Edition files (port/linux/game/ae_glue_mcc.c): the question a Halo PC map's pick asks when the
# player's folders lack the resource maps and MCC has them, in the Map screen's place (B is NOT NOW)
MCC_QUESTION = f"{MT}/mp_map_select/mcc_found_modal"
MCC_QUESTION_TEXT = ("Custom Edition maps need three files from Halo: The\\nMaster Chief Collection. AE can read them "
                     "from your\\nMCC folder. It only reads them and never changes\\nyour MCC files.")
MCC_QUESTION_BUTTONS = [("use", "USE MCC FILES", "ae mcc use"), ("not_now", "NOT NOW", "ae mcc not now"),
                        ("never", "NEVER ASK AGAIN", "ae mcc never ask")]


def _mcc_question() -> list:
    base = f"{MT}/mp_map_select"
    # (no_history: what USE MCC FILES opens, the gametypes, goes back to the Map screen, not to the question)
    lines = _widget(MCC_QUESTION, [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child no_history"),
                                   ("bitmap", "bitmaps/gradient")],
                    ['<on event="b" run="ae mcc not now" back="true"/>',
                     '<on event="back" run="ae mcc not now" back="true"/>',
                     f'<child widget="{base}/mcc_found_title"/>',
                     f'<child widget="{base}/mcc_found_text"/>',
                     f'<child widget="{base}/mcc_found_button_bar" y="414"/>'])
    lines += _widget(f"{base}/mcc_found_title",
                     [("type", "text"), ("controller", 1), ("left", 35), ("top", 11), ("width", 605), ("height", 59),
                      ("text", "HALO MASTER CHIEF COLLECTION FOUND"), ("font", "ui\\large_ui"),
                      ("color", "#FFFFFFFF"), ("text_x", 30), ("text_y", 20)], [])
    lines += _widget(f"{base}/mcc_found_text",
                     [("type", "text"), ("controller", 1), ("left", 68), ("top", 120), ("width", 504),
                      ("height", 120), ("text", MCC_QUESTION_TEXT), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_flags", "no_focus_test")], [])
    lines += _widget(f"{base}/mcc_found_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/mcc_found_{key}" x="{160 + 160 * index}" y="1"/>'
                      for index, (key, _, _) in enumerate(MCC_QUESTION_BUTTONS)])
    for key, caption, run in MCC_QUESTION_BUTTONS:
        # (USE MCC FILES picks the map, as the Map screen's OK does, and opens what that opens)
        then = (f'open="{MT}/connected/gametype_select_screen_wrapper"' if key == "use" else 'back="true"')
        lines += _widget(f"{base}/mcc_found_{key}",
                         [("type", "text"), ("width", 150), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
                          ("text", caption), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                          ("text_y", 2)],
                         [f'<on event="a" run="{run}" {then}/>', f'<on event="start" run="{run}" {then}/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


# COMMUNITY SERVICES (AE 0.3.5, port/linux/game/ae_community_screen.c): the first-run opt-in for the services the
# ChupathingyCE team runs at halo.milenko.org, opened in front of the main menu while network.community_services is
# "ask". The opt-in mockups' look A3 (the owner's pick, 2026-10-20): a settings screen's title and band; the LIST
# HOSTED GAMES row, its own opt-in (OFF until turned on; only USE writes it); the stock buttons USE COMMUNITY SERVICES
# and NOT NOW (in CANCEL's place), NOT NOW focused first ("ae community focus"). The text (what each service sends)
# and the card (who runs them, the thanks) are drawn by the screen's game data input with AE's renderer. B, BACK and
# Esc (START) are NOT NOW; nothing answers in the first half second. The pictures: tools/ae_optin_pictures.py
COMMUNITY_SCREEN = f"{COMMUNITY}/community_services_screen"
# the row: at 54 as the settings' rows, 187 down (its frame 8 px under GAME LIST's text, 8 over its help at 1080p),
# 328 wide (40 px short of the card); its value between arrows at 888 and 1049 px (the spinner at 237, 63 wide)
COMMUNITY_ROW_Y, COMMUNITY_ROW_WIDTH = 187, 328
COMMUNITY_SPINNER_X, COMMUNITY_SPINNER_WIDTH = 237, 63
# USE COMMUNITY SERVICES: its words and 40 px of face either side, ending where OK's button ends (380 + 128), 13 px
# before NOT NOW's at 1080p
COMMUNITY_USE_WIDTH = 216


def _community_services() -> list:
    base = COMMUNITY
    lines = _widget(COMMUNITY_SCREEN, [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                                       ("bitmap", "bitmaps/gradient")],
                    ['<data input="ae community update"/>',
                     '<on event="b" run="ae community not now"/>', '<on event="back" run="ae community not now"/>',
                     '<on event="start" run="ae community not now"/>',
                     f'<child widget="{COMMUNITY_HEADER}"/>', f'<child widget="{base}/options_menu"/>'])
    # (605 wide, as the password screen's title: the picture is drawn across it, a little wider than its 512, as the
    # mockup's title is, and COMMUNITY SERVICES is longer than the stock headers' 363)
    lines += _widget(COMMUNITY_HEADER, [("controller", 1), ("left", 35), ("top", 11), ("width", 605), ("height", 59),
                                        ("bitmap", COMMUNITY_HEADER)], [])
    lines += _widget(f"{base}/options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children")],
                     ['<on event="created" run="ae community focus"/>',
                      f'<child widget="{base}/op_list_hosted_games" x="54" y="{COMMUNITY_ROW_Y}"/>',
                      f'<child widget="{base}/button_bar" y="414"/>'])
    lines += _widget(f"{base}/op_list_hosted_games",
                     [("width", COMMUNITY_ROW_WIDTH), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/ae_option_bkds_optin"), ("color", "#FF2896FF")],
                     ['<on event="a" run="ae community row"/>',
                      f'<child widget="{base}/list_hosted_games_label"/>',
                      f'<child widget="{base}/list_hosted_games_spinner" x="{COMMUNITY_SPINNER_X}" y="1"/>'])
    lines += _widget(f"{base}/list_hosted_games_label",
                     [("type", "text"), ("controller", 1), ("width", 230), ("height", 22), ("text", "LIST HOSTED GAMES:"),
                      ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    # (no setting: the screen holds the value until an answer writes it, ae_community_screen.c)
    lines += _widget(f"{base}/list_hosted_games_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", COMMUNITY_SPINNER_WIDTH), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"), ("strings", "OFF|ON"),
                      ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"),
                      ("footer_bounds", f"7 {COMMUNITY_SPINNER_WIDTH + 3} 19 {COMMUNITY_SPINNER_WIDTH + 9}")], [])
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/button_use" x="{380 + 128 - COMMUNITY_USE_WIDTH}" y="1"/>',
                      f'<child widget="{base}/button_not_now" x="510" y="1"/>'])
    for key, caption, width, bitmap, run in (
            ("use", "USE COMMUNITY SERVICES", COMMUNITY_USE_WIDTH, "bitmaps/ae_optin_button_use", "ae community use"),
            ("not_now", "NOT NOW", 128, "bitmaps/text_button_background", "ae community not now")):
        # (A only: START is the screen's, NOT NOW)
        lines += _widget(f"{base}/button_{key}",
                         [("type", "text"), ("width", width), ("height", 24), ("bitmap", bitmap), ("text", caption),
                          ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                         [f'<on event="a" run="{run}"/>', '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def community_services_files() -> dict:
    """the COMMUNITY SERVICES screen (the first-run opt-in)"""
    head = ['<?xml version="1.0" encoding="UTF-8"?>',
            "<!-- The community services' first-run opt-in, in the PC version's style (tools/port_settings.py) -->",
            "<menus>"]
    return {COMMUNITY.replace("/", ".") + ".xml": head + _community_services() + ["</menus>", ""]}


def multiplayer_files() -> dict:
    """the port's multiplayer widgets: the browser's additions, the server
    settings, the lobby"""
    head = ['<?xml version="1.0" encoding="UTF-8"?>',
            "<!-- The port's multiplayer screens, in the PC version's style (tools/port_settings.py) -->", "<menus>"]
    return {
        f"{MT}/join_game".replace("/", ".") + ".port.xml": head + _join_game_extras() + ["</menus>", ""],
        f"{MT}/server_settings".replace("/", ".") + ".xml": head + _server_settings() + ["</menus>", ""],
        f"{MT}/lobby".replace("/", ".") + ".xml": head + _lobby() + ["</menus>", ""],
        f"{MT}/coop".replace("/", ".") + ".xml": head + _coop() + ["</menus>", ""],
        f"{MT}/split_screen".replace("/", ".") + ".port.xml": head + _split_screen() + ["</menus>", ""],
        ARENA_OPTIONS.replace("/", ".") + ".xml": head + _arena_options() + ["</menus>", ""],
        NHE_OPTIONS.replace("/", ".") + ".xml": head + _nhe_options() + ["</menus>", ""],
        INDICATOR_OPTIONS.replace("/", ".") + ".port.xml": head + _indicator_options_extras() + ["</menus>", ""],
        "main_menu/new_select".replace("/", ".") + ".port.xml": head + _map_kind() + ["</menus>", ""],
        "main_menu/settings_select/multiplayer_setup/item_options_edit".replace("/", ".") + ".port.xml": head + _item_options_extras() + ["</menus>", ""],
        PLAYER_EDIT.replace("/", ".") + ".port.xml": head + _player_options_extras() + ["</menus>", ""],
        TEAMPLAY_EDIT.replace("/", ".") + ".port.xml": head + _teamplay_options_extras() + ["</menus>", ""],
        f"{MT}/mp_map_select".replace("/", ".") + ".port.xml": head + _mcc_question() + ["</menus>", ""],
        OPENCE_EDIT.replace("/", ".") + ".port.xml": head + _opence_options() + ["</menus>", ""],
        MODS.replace("/", ".") + ".xml": [head[0], "<!-- The port's Mods screen, in the PC version's style "
                                          "(tools/port_settings.py) -->", head[2]] + _mods() + ["</menus>", ""],
    }


# MENUS (item 81): a row of the main menu above QUIT that turns the main menu's rows, in place, into the menu
# styles (port/linux/game/ae_menus_swap.c). The main menu's column moves up 18 to make room (ce_menus.CHILD_OFFSETS);
# the rows' pictures are tools/menu_row_art.py's (bitmaps main_menu/menu_*), the style in use marked
MENUS_SELECT = "main_menu/menus_select"
MAIN_MENU_ROW_X, MAIN_MENU_FIRST_ROW, MAIN_MENU_ROW_SPACING = 192, 211, 35
# the styles' rows: key, picture; BACK straight under the last, at the same pitch (ae_menus_swap.c takes ARENA
# EVOLVED out of a build without AE menus, and BACK up into its place)
MENUS_STYLE_ROWS = [("pc", "main_menu/menu_style_pc_current"), ("xbox", "main_menu/menu_style_xbox"),
                    ("arena", "main_menu/menu_style_arena")]


def _main_menu_row(name: str, bitmap: str, handlers: list) -> list:
    """a row as the main menu's own: a picture of its text, 256 by 33"""
    return _widget(name, [("type", "text"), ("width", 256), ("height", 33), ("bitmap", bitmap)],
                   handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])


def _menus_select() -> list:
    """MENUS, and the screen it opens: the main menu again (its logo and build number where they are) with the
    styles' rows where its own begin, the style in use focused ("ae menus focus"). MENUS's A asks "ae menus open",
    which opens the screen (its custom activation) or, away from the plain main menu, refuses quietly. A style's A
    switches to it ("ae menus pick"; the one in use goes back), B, Esc, BACK and the BACK row go back, MENUS
    focused"""
    screen = f"{MENUS_SELECT}/menus_screen"
    lines = _main_menu_row("main_menu/main_menu_item_menus", "main_menu/menu_menus", [
        '<on event="created" run="ae menus item"/>',
        '<on event="a" run="ae menus open"/>', '<on event="start" run="ae menus open"/>',
        f'<on event="custom_activation" open="{screen}"/>'])
    lines += _widget(screen, [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      '<child widget="main_menu/halo_logo" y="28"/>',
                      f'<child widget="{MENUS_SELECT}/menus_select_list"/>',
                      '<child widget="main_menu/build_number" x="450" y="430"/>'])
    rows = [f'<child widget="{MENUS_SELECT}/menus_item_{key}" x="{MAIN_MENU_ROW_X}" '
            f'y="{MAIN_MENU_FIRST_ROW + MAIN_MENU_ROW_SPACING * index}"/>'
            for index, (key, _) in enumerate(MENUS_STYLE_ROWS)]
    rows.append(f'<child widget="{MENUS_SELECT}/menus_item_back" x="{MAIN_MENU_ROW_X}" '
                f'y="{MAIN_MENU_FIRST_ROW + MAIN_MENU_ROW_SPACING * len(MENUS_STYLE_ROWS)}"/>')
    lines += _widget(f"{MENUS_SELECT}/menus_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_items"),
                      ("description", "main_menu/main_menu_list_ext_desc")],
                     ['<data input="main menu fake animate"/>', '<on event="created" run="ae menus focus"/>', *rows])
    for key, bitmap in MENUS_STYLE_ROWS:
        lines += _main_menu_row(f"{MENUS_SELECT}/menus_item_{key}", bitmap, [
            '<on event="a" run="ae menus pick"/>', '<on event="start" run="ae menus pick"/>'])
    lines += _main_menu_row(f"{MENUS_SELECT}/menus_item_back", "main_menu/menu_back", [
        '<on event="a" run="mouse emit back event"/>', '<on event="start" run="mouse emit back event"/>'])
    return lines


def main_menu_files() -> dict:
    """the port's own widgets of the main menu: MENUS and its styles"""
    head = ['<?xml version="1.0" encoding="UTF-8"?>',
            "<!-- The main menu's MENUS, in the PC version's style (tools/port_settings.py) -->", "<menus>"]
    return {MENUS_SELECT.replace("/", ".") + ".xml": head + _menus_select() + ["</menus>", ""]}


REPLACED_FOLDERS = [f"{MT}/server_settings"]


# ---------- the mods (port/linux/game/mods.c, docs/mods.md): the main menu's
# MODS, which opens the list of the mods folder's mods, in their load order,
# each turned on or off and moved up or down (menu_functions.c's "port mods"
# functions)

# the redraws' main menu items (svg/shell/main_menu/menu_*.svg): their
# letters' top and scale, and where their middle is
MENU_ITEM_TOP, MENU_ITEM_SCALE, MENU_ITEM_MIDDLE = 6.57, 0.9037, 130.0
# the space between two of the redraws' letters, in their units (their
# capitals are 21.5 high), for the pairs the port's items have: no redraw has
# these pairs, so each is the sum of a space after the first letter and one
# before the second, fitted (least squares) to the 53 pairs the redraws' items
# and headers have (D-I, of the main menu's Credits, gives D's right side)
MENU_ITEM_SPACES = {"MO": 3.17, "OD": 2.8, "DS": 2.3}
# the mods' status icons (port/assets/icons/lucide): a mod out of the order it
# asks for, and one that cannot be played (another mod says it cannot be
# played with it, or this build cannot play it): menu_functions.c's
# mods_update shows frame 1 or 2
MOD_STATUS_ICONS = [("triangle-alert", "#FFB020"), ("circle-x", "#FF4040")]


def redraw_letters(redraws: Path) -> dict:
    """the redraws' letters (their <path id="gA">s, one drawing of each
    letter in every item and header), by letter"""
    letters = {}
    for svg in sorted(redraws.rglob("*.svg")):
        if any(part.startswith("_") for part in svg.relative_to(redraws).parts):
            continue
        for letter, path in re.findall(r'<path id="g([A-Z])" d="([^"]+)"', svg.read_text()):
            if letters.setdefault(letter, path) != path:
                sys.exit(f"{svg}: the redraws draw {letter} two ways")
    return letters


def menu_item_frames(text: str, redraws: Path) -> list:
    """the frames (svg, width, height) of a main menu item of the port's: its
    text in the redraws' own letters (redraw_letters), spaced, placed and
    coloured as their items are, dim, then white with their blue glow"""
    from fontTools.pens.boundsPen import BoundsPen
    from fontTools.svgLib.path import parse_path

    letters = redraw_letters(redraws)
    widths = []
    for letter in text:
        pen = BoundsPen(None)
        parse_path(letters[letter], pen)
        widths.append(pen.bounds[2])
    spaces = [MENU_ITEM_SPACES[first + second] for first, second in zip(text, text[1:])]
    x = MENU_ITEM_MIDDLE - (sum(widths) + sum(spaces)) * MENU_ITEM_SCALE / 2
    uses = []
    for index, letter in enumerate(text):
        uses.append(f'<use href="#g{letter}" transform="translate({x:.2f},{MENU_ITEM_TOP}) scale({MENU_ITEM_SCALE})"/>')
        x += (widths[index] + (spaces[index] if index < len(spaces) else 0)) * MENU_ITEM_SCALE
    paths = [f'    <path id="g{letter}" d="{letters[letter]}"/>' for letter in sorted(set(text))]
    head = ['<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="256" '
            'height="64" viewBox="0 0 256 64">', "  <defs>", *paths]
    dim = [*head, "  </defs>", f'  <!-- "{text}" (normal state: dim blue) -->',
           '  <g id="text" fill="#2895ff" fill-opacity="0.467">', *[f"    {use}" for use in uses], "  </g>", "</svg>", ""]
    glow = [*head, '    <filter id="glow" filterUnits="userSpaceOnUse" x="0" y="0" width="256" height="64">',
            '      <feGaussianBlur stdDeviation="2.19"/>', "    </filter>", "  </defs>",
            f'  <!-- "{text}" (highlighted: white with blue glow) -->',
            '  <g fill="#2895ff" stroke="#2895ff" stroke-width="6.3" stroke-linejoin="round" opacity="0.650" '
            'filter="url(#glow)">', *[f"    {use}" for use in uses], "  </g>",
            '  <g id="text" fill="#fff">', *[f"    {use}" for use in uses], "  </g>", "</svg>", ""]
    return [("\n".join(dim), 256, 64), ("\n".join(glow), 256, 64)]


def mod_status_frames(icons: Path, size: int = 24) -> list:
    """the frames of the mods' status icon, `size` pixels square (the menus
    draw a bitmap at its size): none, out of order, incompatible"""
    blank = f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 24 24"/>\n'
    return [(blank, size, size)] + [
        ((icons / f"{icon}.svg").read_text().replace('stroke="currentColor"', f'stroke="{color}"')
         .replace('width="24"', f'width="{size}"').replace('height="24"', f'height="{size}"'), size, size)
        for icon, color in MOD_STATUS_ICONS]


def _text_button(name: str, text: str, handlers: list) -> list:
    """a button of the button bar with its own words"""
    pairs = [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
             ("text", text), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)]
    return _widget(name, pairs, handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])


def _mods() -> list:
    """the main menu's MODS and its screen, laid out as the lobby is: on the
    left two thirds the mods as a resource pack list has them (the one at the
    top playing over those below it: the load order upside down), a row each
    with its status icon, name and ON or OFF; on the right third, in the
    lobby's frame, the mod chosen: its name, what is wrong with it, whether it
    is on and its priority, and what its mod.toml says. A turns the mod chosen
    on or off, left and right (and MOVE UP and MOVE DOWN, for the mouse) move
    it; OK keeps it all (load_order.toml) and plays them, Cancel forgets it
    (menu_functions.c)"""
    base = MODS
    lines = _widget("main_menu/main_menu_item_mods",
                    [("type", "text"), ("width", 256), ("height", 33), ("bitmap", "main_menu/menu_mods")],
                    [f'<on event="a" open="{base}/mods_screen"/>', f'<on event="start" open="{base}/mods_screen"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/mods_screen", [("width", 640), ("height", 480),
                                             ("flags", "pass_unhandled_to_focused_child pause_game"),
                                             ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/mods_list"/>',
                      f'<child widget="{base}/header_mods"/>'])
    lines += _header(f"{base}/header_mods", f"{base}/header_mods")
    # (the lobby's rows, MOD_ROWS of them, scrolling over more mods)
    rows = [f'<child widget="{base}/op_mod_{index + 1}" x="20" y="{73 + 30 * index}"/>' for index in range(MOD_ROWS)]
    lines += _widget(f"{base}/mods_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                           ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                           ("description", f"{base}/mod_details")],
                     ['<data input="port mods update"/>', '<on event="created" run="port mods init"/>',
                      '<on event="deleted" run="port mods close"/>', *rows,
                      f'<child widget="{base}/button_bar" y="414"/>'])
    for index in range(MOD_ROWS):
        lines += _widget(f"{base}/op_mod_{index + 1}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="port mods toggle"/>', '<on event="start" run="port mods toggle"/>',
                          '<on event="left" run="port mods move up"/>', '<on event="right" run="port mods move down"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/mod_status" x="8" y="2"/>',
                          f'<child widget="{base}/mod_name" x="34"/>',
                          f'<child widget="{base}/mod_state" x="318"/>'])
    lines += _widget(f"{base}/mod_status", [("controller", 1), ("width", 24), ("height", 24),
                                            ("bitmap", "mods/status")], [])
    lines += _widget(f"{base}/mod_name",
                     [("type", "text"), ("controller", 1), ("width", 284), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/mod_state",
                     [("type", "text"), ("controller", 1), ("width", 66), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4), ("text_flags", "no_focus_test")], [])
    # (in the lobby's frame, sel_list_desc_bkd's: the mod chosen's name, the
    # icon of what is wrong with it, and the rest)
    lines += _widget(f"{base}/mod_details", [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/mod_details_name"/>', f'<child widget="{base}/mod_details_status"/>',
                      f'<child widget="{base}/mod_details_text"/>'])
    lines += _widget(f"{base}/mod_details_name",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200), ("height", 44),
                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/mod_details_status", [("controller", 1), ("left", 411), ("top", 132), ("width", 16),
                                                    ("height", 16), ("bitmap", "mods/status_small")], [])
    lines += _widget(f"{base}/mod_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 130), ("width", 200),
                      ("height", 272), ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/button_up" y="1"/>', f'<child widget="{base}/button_down" x="130" y="1"/>',
                      f'<child widget="{base}/button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _text_button(f"{base}/button_up", "MOVE UP", ['<on event="a" run="port mods move up"/>',
                                                          '<on event="start" run="port mods move up"/>'])
    lines += _text_button(f"{base}/button_down", "MOVE DOWN", ['<on event="a" run="port mods move down"/>',
                                                              '<on event="start" run="port mods move down"/>'])
    lines += _button(f"{base}/button_ok", 1, ['<on event="a" run="port mods save" back="true"/>',
                                              '<on event="start" run="port mods save" back="true"/>'])
    return lines
