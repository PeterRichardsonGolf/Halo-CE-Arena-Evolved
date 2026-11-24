"""Arena Evolved's own settings pages for the PC menus: the settings neither the PC version nor Content
Evolved has, in their style. tools/port_settings.py turns AE_GROUPS into screens (ARENA EVOLVED on the profile menu).

A row is (label, config.toml key, values, help), the values (shown text, written text). Per-player settings are
read and written by the menus' functions as they are everywhere else."""
import re
from pathlib import Path

ON_OFF = [("ON", "true"), ("OFF", "false")]
# (TARGET FRAMERATE: the registry's texts, words and help, port/linux/game/ae_target_framerate_rules.h, which the AE
# menus' rows read too)
_TARGET_FRAMERATE_H = (Path(__file__).resolve().parent.parent / "port" / "linux" / "game" /
                       "ae_target_framerate_rules.h").read_text()


def _registry_strings(name: str) -> list:
    """The C string literals of a #define in the registry (continued lines joined; escapes \\n and \\" read)."""
    match = re.search(rf"^#define {name} ((?:[^\n]*\\\n)*[^\n]*)$", _TARGET_FRAMERATE_H, re.M)
    if not match:
        raise KeyError(name)
    body = match.group(1).replace("\\\n", " ")
    return [literal.replace("\\n", "\n").replace('\\"', '"') for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', body)]


def _registry_values() -> list:
    """TARGET FRAMERATE's (shown text, written word) pairs, in the rows' order."""
    texts = _registry_strings("AE_TARGET_FRAMERATE_TEXTS")
    words = _registry_strings("AE_TARGET_FRAMERATE_WORDS")
    if len(texts) != len(words):
        raise ValueError("AE_TARGET_FRAMERATE_TEXTS and _WORDS differ in length")
    return list(zip(texts, words))


TARGET_FRAMERATE = _registry_values()
TARGET_FRAMERATE_HELP = "".join(_registry_strings("AE_TARGET_FRAMERATE_CLASSIC_HELP"))
TARGET_FRAMERATE_SPLIT_HELP = "".join(_registry_strings("AE_TARGET_FRAMERATE_CLASSIC_SPLIT_HELP"))
# (group folder, title, help of its row, [(page folder, title, help of its row, rows)])
AE_GROUPS = [
    ('controls', 'CONTROLS', "Controller, gyro, mouse and keyboard. Key bindings\nare in the AE menus' SETTINGS.", [
        ('controller', 'CONTROLLER', 'Aim control, zoom, dead zones and button\nprompts.', [
            ('A/B SWAP:', 'input.ab_swap', [('OFF', 'off'), ('IN GAME', 'in_game'), ('EVERYWHERE', 'everywhere')],
             'OFF: buttons act as labeled. IN GAME: Xbox\npositions in play. EVERYWHERE: menus too.'),
            ('BUTTON PROMPTS:', 'input.button_prompts', [('AUTO', 'auto'), ('XBOX', 'xbox'), ('PLAYSTATION', 'playstation'), ('NINTENDO', 'nintendo'), ('UNIVERSAL', 'universal')],
             "The pictures of your pad's buttons in menus.\nAUTO follows the pad you use."),
            ('AIM CONTROL:', 'input.aim_control', [('CLASSIC', 'classic'), ('MODERN', 'modern')],
             'MODERN: a smoother stick curve, and the turn\nspeeds up with the stick held all the way.'),
            ('LOOK ACCELERATION:', 'input.look_acceleration', [('OFF', '0'), ('1', '1'), ('2', '2'), ('3', '3'), ('4', '4'), ('5', '5'), ('6', '6'), ('7', '7'), ('8', '8'), ('9', '9'), ('10', '10')],
             'With MODERN: how fast the turn speeds up while\nthe stick is held all the way.'),
            ('VERTICAL LOOK:', 'input.look_sensitivity_vertical', [('SAME', 'same'), ('1', '1'), ('2', '2'), ('3', '3'), ('4', '4'), ('5', '5'), ('6', '6'), ('7', '7'), ('8', '8'), ('9', '9'), ('10', '10')],
             'With MODERN: how fast you look up and down.\nSAME uses your look sensitivity.'),
            ('LOOK DEAD ZONE:', 'input.look_radial_deadzone', [('SAME', 'same'), ('0%', '0'), ('5%', '5'), ('10%', '10'), ('15%', '15'), ('20%', '20')],
             "With MODERN: the look stick's round dead zone.\nSAME follows the right stick dead zone."),
            ('ZOOM HOLD TIME:', 'input.zoom_hold_time', [('0.10 S', '0.10'), ('0.15 S', '0.15'), ('0.20 S', '0.20'), ('0.25 S', '0.25'), ('0.30 S', '0.30'), ('0.35 S', '0.35'), ('0.40 S', '0.40'), ('0.45 S', '0.45'), ('0.50 S', '0.50'), ('0.55 S', '0.55'), ('0.60 S', '0.60'), ('0.65 S', '0.65'), ('0.70 S', '0.70'), ('0.75 S', '0.75'), ('0.80 S', '0.80'), ('0.85 S', '0.85'), ('0.90 S', '0.90'), ('0.95 S', '0.95'), ('1.00 S', '1.00')],
             "With GAME OPTIONS' ZOOM on BOTH: how long a\npress counts as a hold."),
        ]),
        ('gyro', 'GYRO AND TRACKPAD', "Gyro aiming and the Steam Controller's\ntrackpad.", [
            ('GYRO AIMING:', 'input.gyro_aim', [('OFF', 'false'), ('ALWAYS', 'true'), ('ZOOMED IN', 'zoom')],
             'Aim by moving the controller (DualSense, Pro\nController, Joy-Con). ZOOMED IN: only in a scope.'),
            ('TURN SENSITIVITY:', 'input.gyro_sensitivity_horizontal', [('0.25', '0.25'), ('0.5', '0.5'), ('0.75', '0.75'), ('1', '1.0'), ('1.25', '1.25'), ('1.5', '1.5'), ('2', '2.0'), ('2.5', '2.5'), ('3', '3.0'), ('4', '4.0'), ('5', '5.0'), ('6', '6.0'), ('8', '8.0'), ('10', '10.0')],
             'With GYRO AIMING on. 1 turns you exactly as\nfar as you turn the controller.'),
            ('TILT SENSITIVITY:', 'input.gyro_sensitivity_vertical', [('0.25', '0.25'), ('0.5', '0.5'), ('0.75', '0.75'), ('1', '1.0'), ('1.25', '1.25'), ('1.5', '1.5'), ('2', '2.0'), ('2.5', '2.5'), ('3', '3.0'), ('4', '4.0'), ('5', '5.0'), ('6', '6.0'), ('8', '8.0'), ('10', '10.0')],
             'With GYRO AIMING on. 1 looks up or down as\nfar as you tilt the controller.'),
            ('INVERT TURN:', 'input.gyro_invert_horizontal', [('ON', 'true'), ('OFF', 'false')],
             'With GYRO AIMING on: turning the controller right\nlooks left.'),
            ('INVERT TILT:', 'input.gyro_invert_vertical', [('ON', 'true'), ('OFF', 'false')],
             'With GYRO AIMING on: tilting the controller up\nlooks down.'),
            ('TRACKPAD AIM:', 'input.trackpad_aim', [('OFF', 'false'), ('AIM', 'true')],
             "Aim with a Steam Controller's right trackpad as\nwell as the stick."),
            ('TRACKPAD SPEED:', 'input.trackpad_sensitivity', [('0.25', '0.25'), ('0.5', '0.5'), ('0.75', '0.75'), ('1', '1.0'), ('1.25', '1.25'), ('1.5', '1.5'), ('2', '2.0'), ('2.5', '2.5'), ('3', '3.0'), ('4', '4.0'), ('5', '5.0'), ('6', '6.0'), ('8', '8.0'), ('10', '10.0')],
             'With TRACKPAD AIM on: how far a swipe across\nthe right trackpad turns you.'),
        ]),
        ('mouse', 'MOUSE AND KEYBOARD', "The mouse's zoom and the keyboard's prompts.", [
            ('MOUSE ZOOM:', 'input.mouse_zoom_mode', [('TOGGLE', 'toggle'), ('HOLD', 'hold'), ('BOTH', 'both')],
             "TOGGLE, HOLD or BOTH, for the mouse only. The\ncontroller's zoom is in GAME OPTIONS."),
            ('ZOOM HOLD TIME:', 'input.mouse_zoom_hold_time', [('0.10 S', '0.10'), ('0.15 S', '0.15'), ('0.20 S', '0.20'), ('0.25 S', '0.25'), ('0.30 S', '0.30'), ('0.35 S', '0.35'), ('0.40 S', '0.40'), ('0.45 S', '0.45'), ('0.50 S', '0.50'), ('0.55 S', '0.55'), ('0.60 S', '0.60'), ('0.65 S', '0.65'), ('0.70 S', '0.70'), ('0.75 S', '0.75'), ('0.80 S', '0.80'), ('0.85 S', '0.85'), ('0.90 S', '0.90'), ('0.95 S', '0.95'), ('1.00 S', '1.00')],
             'With MOUSE ZOOM on BOTH: how long a click\ncounts as a hold.'),
            ('ZOOM RESET:', 'input.mouse_zoom_reset', [('FIRST LEVEL', 'first'), ('LAST USED', 'last')],
             "Where the mouse's zoom starts the next time you\nzoom in."),
            ('WHEEL ZOOM:', 'input.wheel_zoom', [('ON', 'true'), ('OFF', 'false')],
             'The wheel zooms in and out instead of switching\nweapons, unless the weapon cannot zoom.'),
            ('KEY PROMPTS:', 'input.keyboard_prompts', [('ON', 'true'), ('OFF', 'false')],
             "Show your keys, not a controller's buttons, in\nthe game's hints."),
        ]),
    ]),
    ('hud', 'HUD', "HUD, reticles and training aids. The crosshair\neditor is in the AE menus' SETTINGS.", [
        ('hud', 'HUD ELEMENTS', 'Safe area, clock, medals, kill feed and the\nscoreboard.', [
            ('HUD SAFE AREA:', 'display.hud_safe_area', [('OFF', '0'), ('1%', '1'), ('2%', '2'), ('3%', '3'), ('4%', '4'), ('5%', '5'), ('6%', '6'), ('7%', '7'), ('8%', '8'), ('9%', '9'), ('10%', '10')],
             "Pull the HUD in from the screen's edges, for\nTVs that cut the picture."),
            ('CLOCK LABELS:', 'display.match_clock_labels', [('ON', 'true'), ('OFF', 'false')],
             'With the multiplayer game timer on: words under\nthe clock, LEFT and PLAYED.'),
            ('POWERUP TIMERS:', 'display.powerup_timers', [('ON', 'true'), ('OFF', 'false')],
             'A timer for your own camo or overshield.'),
            ('MEDALS:', 'display.medals', [('ON', 'true'), ('OFF', 'false')],
             'Medals you earn, shown in your view.'),
            ('MEDAL STYLE:', 'display.medal_style', [('UNIVERSAL', 'modern'), ('MCC', 'mcc')],
             'With MEDALS on. UNIVERSAL: by your reticle.\nMCC: larger, on the left.'),
            ('KILL FEED:', 'display.kill_feed', [('ON', 'true'), ('OFF', 'false')],
             "Everyone's kills listed on your HUD, in place of\nthe game's kill messages."),
            ('DEAD ALLY MARKS:', 'display.dead_ally_markers', [('ON', 'true'), ('OFF', 'false')],
             'Team games: a cross where a teammate fell, until\nthey respawn.'),
            ('SHOW QUITTERS:', 'display.show_quit_players', [('ON', 'true'), ('OFF', 'false')],
             'Players who left stay on the scoreboard, greyed.'),
            ('SCOREBOARD COLOR:', 'display.scoreboard_background_color', [('DARK', '16, 16, 16, 150'), ('BLACK', '0, 0, 0, 200'), ('BLUE', '16, 32, 64, 160'), ('RED', '64, 16, 16, 160'), ('GREEN', '16, 48, 24, 160')],
             "The color of the scoreboard's backing panel."),
        ]),
        ('reticle', 'CROSSHAIR', 'What your crosshair does over allies and enemies.', [
            ('IN SCOPE:', 'display.crosshair_in_scope', [('KEEP MINE', 'same'), ("WEAPON'S", 'scope')],
             "Zoomed in: KEEP MINE shows your crosshair,\nWEAPON'S shows the weapon's own reticle."),
            ('ALLY RETICLE:', 'display.ally_reticle', [('ON', 'true'), ('OFF', 'false')],
             'Your reticle turns green over an ally.'),
            ('ALLY COLOR:', 'display.reticle_ally_color', [('RED', 'red'), ('ORANGE', 'orange'), ('YELLOW', 'yellow'), ('GREEN', 'green'), ('CYAN', 'cyan'), ('BLUE', 'blue'), ('MAGENTA', 'magenta'), ('WHITE', 'white')],
             "With ALLY RETICLE on: your crosshair's color\nover an ally."),
            ('ENEMY COLOR:', 'display.reticle_enemy_color', [('RED', 'red'), ('ORANGE', 'orange'), ('YELLOW', 'yellow'), ('GREEN', 'green'), ('CYAN', 'cyan'), ('BLUE', 'blue'), ('MAGENTA', 'magenta'), ('WHITE', 'white')],
             "The color your crosshair and a weapon's reticle\ntake over an enemy in range."),
            ('MARK PROMPT:', 'display.mark_prompt', [('ON', 'true'), ('OFF', 'false')],
             'The button and word over what a mark would act on.'),
        ]),
        ('stock', 'WEAPON RETICLES', "Size, scaling, color and motion of the\nweapons' own reticles.", [
            ('RETICLE SIZE:', 'display.reticle_size', [('50%', '50'), ('55%', '55'), ('60%', '60'), ('65%', '65'), ('70%', '70'), ('75%', '75'), ('80%', '80'), ('85%', '85'), ('90%', '90'), ('95%', '95'), ('100%', '100'), ('105%', '105'), ('110%', '110'), ('115%', '115'), ('120%', '120'), ('125%', '125'), ('130%', '130'), ('135%', '135'), ('140%', '140'), ('145%', '145'), ('150%', '150'), ('155%', '155'), ('160%', '160'), ('165%', '165'), ('170%', '170'), ('175%', '175'), ('180%', '180'), ('185%', '185'), ('190%', '190'), ('195%', '195'), ('200%', '200')],
             "How big a weapon's own reticle is."),
            ('RETICLE SCALING:', 'display.reticle_scaling', [('FOLLOWS FOV', 'fov'), ('FIXED', 'fixed')],
             'FOLLOWS FOV: a reticle covers the same part of\nthe world at any FOV. FIXED: same screen size.'),
            ('FOV BASE:', 'display.reticle_fov_base', [('78', '78'), ('70 (STOCK)', '70')],
             "With FOLLOWS FOV: the field of view where a\nweapon's reticle is its stock size."),
            ('RETICLE COLOR:', 'display.reticle_color', [('ORIGINAL', 'original'), ('SAME AS HUD', 'hud'), ('WHITE', 'white'), ('ICE', 'ice'), ('CYAN', 'cyan'), ('PINK', 'pink'), ('YELLOW', 'yellow')],
             "The color of a weapon's own reticle. Red over an\nenemy and green over an ally stay."),
            ('RETICLE MOTION:', 'display.reticle_motion', [('RIFLES', 'rifles'), ('ALL WEAPONS', 'all'), ('OFF', 'off')],
             'RIFLES: AE rifle reticles grow a little each\nshot. ALL WEAPONS: every one. OFF: none move.'),
        ]),
        ('training', 'TRAINING AND OVERLAYS', 'Power weapon lists and the ping.', [
            ('POWER LIST:', 'display.power_list', [('CLOCK', 'clock'), ('TOP LEFT', 'top_left')],
             'Where power item spawn times are listed: over\nthe clock, or top left.'),
            ('SHOTGUN AS POWER:', 'display.shotgun_power', [('ON', 'true'), ('OFF', 'false')],
             'List the shotgun with the power weapons.'),
            ('PING:', 'display.ping', [('OFF', 'off'), ('NUMBER', 'number'), ('NUMBER + BARS', 'number_and_bars')],
             'Your ping to the host in a network game, on the\noverlay line.'),
        ]),
    ]),
    ('view', 'VIEW', 'Weapon sway, pistol colors, backpack weapons,\nblood and ragdolls.', [
        ('view', 'VIEW', None, [
            ('WEAPON SWAY:', 'display.viewmodel_sway', [('OFF', '0'), ('25%', '25'), ('50%', '50'), ('75%', '75'), ('STOCK', '100')],
             'How far your weapon leans as you turn and walk.\nOFF keeps it locked to the crosshair.'),
            ('PISTOL COLOR:', 'display.pistol_color', [('DEFAULT', 'default'), ('WHITE', 'white'), ('BLACK', 'black'), ('RED', 'red'), ('BLUE', 'blue'), ('GRAY', 'gray'), ('YELLOW', 'yellow'), ('GREEN', 'green'), ('PINK', 'pink'), ('PURPLE', 'purple'), ('CYAN', 'cyan'), ('COBALT', 'cobalt'), ('ORANGE', 'orange'), ('TEAL', 'teal'), ('SAGE', 'sage'), ('BROWN', 'brown'), ('TAN', 'tan'), ('MAROON', 'maroon'), ('SALMON', 'salmon')],
             "Cosmetic, on your screen only: your pistol's\nbody in first person."),
            ('PISTOL TRIM:', 'display.pistol_trim', [('DEFAULT', 'default'), ('WHITE', 'white'), ('BLACK', 'black'), ('RED', 'red'), ('BLUE', 'blue'), ('GRAY', 'gray'), ('YELLOW', 'yellow'), ('GREEN', 'green'), ('PINK', 'pink'), ('PURPLE', 'purple'), ('CYAN', 'cyan'), ('COBALT', 'cobalt'), ('ORANGE', 'orange'), ('TEAL', 'teal'), ('SAGE', 'sage'), ('BROWN', 'brown'), ('TAN', 'tan'), ('MAROON', 'maroon'), ('SALMON', 'salmon')],
             "Cosmetic, on your screen only: your pistol's\ndark parts in first person."),
            ('BACKPACK GUNS:', 'display.backpack_weapons', [('ON', 'true'), ('OFF', 'false')],
             'A weapon a player carries but does not hold\nshows on their back or thigh.'),
            ('PLAYER RAGDOLLS:', 'game.ragdoll_players', [('NONE', 'none'), ('WESTERN', 'spaghetti_western'), ('EXAGGERATED', 'exaggerated'), ('MODERN', 'modern'), ('MIXED', 'mixed')],
             "Experimental. How players' and bots' bodies\nfall. NONE is the original."),
            ('AI RAGDOLLS:', 'game.ragdoll_ai', [('NONE', 'none'), ('WESTERN', 'spaghetti_western'), ('EXAGGERATED', 'exaggerated'), ('MODERN', 'modern'), ('MIXED', 'mixed')],
             "Experimental. How the campaign's enemies and\nallies fall. Only this PC sees it."),
            ('DYNAMIC BLOOD:', 'game.dynamic_blood', [('ON', 'true'), ('OFF', 'false')],
             'Hits throw blood onto the walls behind, and it\nruns down them. Only you see it.'),
        ]),
    ]),
    ('video', 'VIDEO', 'Frame rate, draw distance, split screen and the\ngraphics extras.', [
        ('video', 'GRAPHICS AND LOADING', 'Latency, draw distance, effects limits and the\nloading screens.', [
            ('LOW LATENCY:', 'display.low_latency', [('OFF', 'off'), ('ON', 'on'), ('BOOST', 'boost')],
             "Each frame ends just before the screen's refresh:\nless delay. BOOST can lower fps on a weak GPU."),
            ('DRAW DISTANCE:', 'display.far_clip_distance', [('1024', '1024'), ('2048', '2048'), ('4096', '4096'), ('10000', '10000')],
             'How far the game draws. Bigger maps can need more.'),
            ('TARGET FRAMERATE:', 'display.target_framerate', TARGET_FRAMERATE, TARGET_FRAMERATE_HELP),
            ('PARTICLE LIMIT:', 'display.particle_limit', [('1024', '1024'), ('2048', '2048'), ('4096', '4096'), ('8192', '8192')],
             'The most sparks, smoke and debris alive at once.\nFewer is easier on the PC.'),
            ('LIGHT LIMIT:', 'display.dynamic_light_limit', [('4', '4'), ('8', '8'), ('16', '16'), ('NO LIMIT', '0')],
             'The most muzzle flashes, plasma and explosions\nthat light the level at once.'),
            ('CAMPAIGN LOADING:', 'display.loading_screen_campaign_seconds', [('NO WAIT', '0'), ('2 S', '2'), ('5 S', '5'), ('10 S', '10'), ('15 S', '15'), ('20 S', '20'), ('30 S', '30')],
             "The least time a campaign level's loading screen\nstays up."),
            ('MATCH LOADING:', 'display.loading_screen_multiplayer_seconds', [('NO WAIT', '0'), ('1 S', '1'), ('2 S', '2'), ('3 S', '3'), ('5 S', '5')],
             "The least time any other game's loading screen\nstays up. Network games wait for it."),
        ]),
        ('advanced', 'SPLIT SCREEN AND ADVANCED', "Split screen's graphics and layout, depth and\nshader cache.", [
            ('HDR BRIGHTNESS:', 'display.hdr_exposure', [('0.25', '0.25'), ('0.5', '0.5'), ('0.75', '0.75'), ('1.0', '1'), ('1.25', '1.25'), ('1.5', '1.5'), ('2.0', '2'), ('2.5', '2.5'), ('3.0', '3'), ('4.0', '4')],
             "With HDR on: the picture's brightness."),
            ('REVERSED DEPTH:', 'display.reversed_depth', [('ON', 'true'), ('OFF', 'false')],
             'Precise depth far away, so close surfaces stop\nflickering. Takes effect after a restart.'),
            ('SHADER CACHE:', 'display.shader_cache', [('ON', 'true'), ('OFF', 'false')],
             'Keeps the graphics programs for next time, so\nfewer stalls. OFF builds them again each run.'),
            ('SKIP HIDDEN AREAS:', 'display.structure_culling', [('ON', '31'), ('OFF', '0')],
             'Skips drawing map parts the camera cannot see.\nOFF draws everything, as the console did.'),
            ('SPLIT GRAPHICS:', 'display.split_graphics', [('AUTO', 'auto'), ('SAME AS SOLO', 'same'), ('QUALITY', 'quality'), ('PERFORMANCE', 'performance')],
             'The graphics while two or more players share the\nscreen. AUTO plays at PERFORMANCE then.'),
            ('SPLIT TARGET FRAMERATE:', 'display.split_target_framerate', TARGET_FRAMERATE, TARGET_FRAMERATE_SPLIT_HELP),
            ('TWO PLAYERS:', 'display.split_two_players', [('AUTO', 'auto'), ('TOP/BOTTOM', 'top_and_bottom'), ('SIDE BY SIDE', 'side_by_side')],
             'How two players share the screen. AUTO goes\nSIDE BY SIDE on a 21:9 or wider screen.'),
            ('SIDE BARS:', 'display.split_side_bars', [('ON', 'true'), ('OFF', 'false')],
             'With TOP/BOTTOM: black bars at the sides give\neach view more height, at fewer pixels.'),
        ]),
    ]),
    ('audio', 'AUDIO', 'Reverb, speakers and the NHE sound filter.', [
        ('audio', 'AUDIO', None, [
            ('SPEAKERS:', 'audio.channels', [('STEREO', 'stereo'), ('5.1', '5.1'), ('AUTO', 'auto')],
             'STEREO for headphones and TVs, 5.1 for surround\nspeakers. Takes effect after a restart.'),
            ('REVERB LEVEL:', 'audio.reverb_level', [('XBOX', '100'), ('75%', '75'), ('50%', '50'), ('35%', '35'), ('25%', '25')],
             "How loud and long rooms echo. XBOX is the\nconsole's own; lower is quieter."),
            ('NHE SOUND FILTER:', 'audio.nhe_sound_filter', [('ON', 'true'), ('OFF', 'false')],
             "Mutes the plasma pistol's charge hum and the\nneedler's shots on every map, as NHE maps do."),
        ]),
    ]),
    ('online', 'ONLINE', "Map downloads, Discord and what you host. Banned\nplayers are in the AE menus' SETTINGS.", [
        ('maps', 'CONNECTION AND MAPS', 'Rejoin, Steam Workshop, the map archive and the\nprotocol.', [
            ('REJOIN AFTER DROP:', 'network.rejoin_after_drop', [('ON', 'true'), ('OFF', 'false')],
             'If your connection drops, join the game again by\nitself: 3 tries in 30 seconds.'),
            ('WORKSHOP DOWNLOADS:', 'network.map_download_steam', [('ON', 'true'), ('OFF', 'false')],
             "Download a server's Workshop maps from Steam.\nNeeds a Steam account."),
            ('MAP ARCHIVE:', 'game.ce_archive', [('ON', 'true'), ('OFF', 'false')],
             'Show the online map archive in MAPS & MODS.\nOFF: the game never contacts it.'),
            ('DOWNLOAD LIMIT:', 'network.map_download_limit', [('256 MB', '256'), ('512 MB', '512'), ('1 GB', '1024'), ('2 GB', '2048'), ('4 GB', '4096'), ('8 GB', '8192'), ('16 GB', '16384'), ('64 GB', '65536')],
             'The most map data this PC takes from one host.'),
            ('PROTOCOL:', 'network.protocol', [('AUTO', 'auto'), ('OPENCE', 'opence'), ('DELTA', 'delta')],
             'How this PC talks to other players. Leave on\nAUTO unless a host asks for OPENCE or DELTA.'),
        ]),
        ('discord', 'DISCORD', 'What your Discord status shows.', [
            ('DISCORD STATUS:', 'discord.presence', [('FULL', 'full'), ('MINIMAL', 'minimal'), ('OFF', 'off')],
             'What your Discord profile shows you playing:\nFULL detail, MINIMAL, or nothing.'),
            ('DISCORD NAME:', 'discord.show_server_name', [('ON', 'true'), ('OFF', 'false')],
             "With DISCORD STATUS on: show the server's\nname in your status."),
            ('DISCORD JOIN:', 'discord.show_join', [('ON', 'true'), ('OFF', 'false')],
             'With DISCORD STATUS on: friends can join you\nfrom Discord.'),
        ]),
        ('hosting', 'HOSTING', 'Rules for games you host.', [
            ('SHIELD EFFECTS:', 'network.shield_effects', [('STOCK', 'stock'), ('GLOW', 'glow'), ('LIGHT SHOW', 'light_show'), ('OVERSHIELD', 'overshield'), ('GLOW+OVER', 'glow_overshield'), ('SHOW+OVER', 'light_show_overshield'), ("PLAYERS' OWN", 'players')],
             "Shield effects in games you host. +OVER adds the\novershield look. PLAYERS' OWN: each picks their own."),
            ('MARKS:', 'network.marks', [('ON', 'true'), ('OFF', 'false')],
             'Players can mark and call on the wheel in games\nyou host.'),
            ('VEHICLE RIDING:', 'network.vehicle_riding', [('ON', 'true'), ('OFF', 'false')],
             'Players standing on a moving vehicle ride it, in\ngames you host.'),
        ]),
    ]),
    ('system', 'SYSTEM', 'Language, MCC and recording. Weapon packs and\nmaps are in the AE menus.', [
        ('game', 'GAME AND MCC', 'Language, intro movies, Workshop and MCC maps.', [
            ('LANGUAGE:', 'game.language', [('ENGLISH', ''), ('DEUTSCH', 'de'), ('FRANCAIS', 'fr'), ('ESPANOL', 'es'), ('ITALIANO', 'it'), ('JAPANESE', 'ja')],
             "The game's text and voices, when the maps have\nthat language. Takes effect after a restart."),
            ('MY WORKSHOP MAPS:', 'game.workshop_maps', [('ON', 'true'), ('OFF', 'false')],
             'List the Workshop maps you subscribe to in\nSteam. Read in place, never copied.'),
            ('MCC CAMPAIGNS:', 'game.mcc_campaigns', [('ON', 'true'), ('OFF', 'false')],
             "Order MCC's missions as its campaigns have them,\nwith their progression."),
            ('MCC COLLECTIONS:', 'game.mcc_collections', [('ON', 'true'), ('OFF', 'false')],
             'Group the MCC map packages you have into their\ncollections.'),
            ('MCC TEXTURES:', 'display.mcc_texture_prepare_mb', [('OFF', '0'), ('64 MB', '64'), ('128 MB', '128'), ('256 MB', '256'), ('512 MB', '512')],
             'MCC maps only: prepare large textures while\nloading, within this much graphics memory.'),
            ('INTRO MOVIES:', 'game.intro_movies', [('ON', 'true'), ('OFF', 'false')],
             'The opening movies before the main menu. OFF goes\nstraight to the menu.'),
        ]),
        ('recording', 'RECORDING', 'F9 recordings, the recording dot and the version\nmark.', [
            ('RECORDING DOT:', 'capture.record_indicator', [('ON', 'true'), ('OFF', 'false')],
             'A red dot at the top right while recording. It is\nnever in the recording.'),
            ('VERSION MARK:', 'display.version_watermark', [('ON', 'true'), ('OFF', 'false')],
             'The version in a corner, for recordings and bug\nreports.'),
            ('RECORD QUALITY:', 'capture.record_quality', [('LOW', 'low'), ('MEDIUM', 'medium'), ('HIGH', 'high')],
             "F9's recordings: LOW makes small files, HIGH the\nsharpest."),
            ('RECORD FRAME RATE:', 'capture.record_fps', [('30', '30'), ('60', '60')],
             "The recordings' frames a second, whatever the\ngame's frame rate."),
        ]),
    ]),
]
