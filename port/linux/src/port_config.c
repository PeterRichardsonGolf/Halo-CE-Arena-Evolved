/*
PORT_CONFIG.C

The native ports' settings (port_config.h), parsed with tomlc17
(port/third_party/tomlc17). Every setting is in the table below with its
type, default, the HALO_* environment variable that overrides it and the
comment written into a new file. The file is read once, on the first
question; unknown keys and values of the wrong type are reported in the log
and the defaults used instead. A new file has every setting commented out at
its default, so that a later version's default reaches it; a setting is
written as a value only when the player chooses one (config_write). Once
the file exists only those lines, the settings a newer version adds and an
older file's layout (config_version) are changed in it, so that the player's
edits and comments stay.
*/

#include "platform.h"
#include "port_config.h"
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
/* (the command line: --config, --server, --probe...) */
#include "posix.h"
#include "posix_server.h"
#endif
#include "ae_config_players.h" /* AE hook */
#include "capture.h"
#include "posix.h"

#include <time.h>
#include "tomlc17.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* ---------- the settings */

enum config_type
{
	_config_boolean,
	_config_integer,
	_config_real,
	_config_string,
};

/* how the setting's environment variable sets it */
enum config_environment
{
	/* the variable's text is the value ("0", "false", "no" and "off" are
	false for a boolean) */
	_environment_value,
	/* the variable being set (not empty, "0", "false", "no" or "off") makes it true */
	_environment_set_is_true,
	/* the variable being set (not empty, "0", "false", "no" or "off") makes it false */
	_environment_set_is_false,
};

/* the builds a setting means something in, and is written for */
enum
{
	/* an earlier version's setting, which a later one replaced: read from an
	older file or the environment (config_carry_over), never written */
	_platform_none = 0,
	_platform_desktop = 1,
	_platform_android = 2,
	_platform_all = _platform_desktop | _platform_android,
	/* (of the desktop builds, only Windows) */
	_platform_windows = 4,
	/* (the web build, which has the desktop's settings, and these) */
	_platform_web = 8,
	/* (a dedicated server's, of the Linux and Windows builds: written into
	config.toml only in server mode) */
	_platform_server = 16,
	/* the original Xbox (ChupathingyCE's port/xbox): not in _platform_all, each
	of its settings marked for it; never this build's (CONFIG_PLATFORM) */
	_platform_xbox = 32,
};

struct config_setting
{
	const char *name;
	enum config_type type;
	/* as it is written in the file */
	const char *default_value;
	/* NULL: none, for a setting the Android app reads from the file itself */
	const char *environment;
	enum config_environment environment_style;
	unsigned platforms;
	const char *comment;
};

/* macOS starts in a window, as its games do */
#ifdef __APPLE__
#define DEFAULT_FULLSCREEN "false"
#else
#define DEFAULT_FULLSCREEN "true"
#endif

/* macOS's audio cuts out with SDL's 512-frame buffer when the mixer is late
(a 10.6 ms budget at 48 kHz); 2048 frames (43 ms) rides that out. Elsewhere
512 keeps the latency low; Android's callback is handed to a thread that
can run the guest (host_sdl.c), which 512 left too little time (the menus'
music broke up) and 1024 does not. */
#ifdef __APPLE__
#define DEFAULT_AUDIO_BUFFER_FRAMES "2048"
#elif defined(HALO_ANDROID)
#define DEFAULT_AUDIO_BUFFER_FRAMES "1024"
#else
#define DEFAULT_AUDIO_BUFFER_FRAMES "512"
#endif

/* crouching's keys (in a browser C alone: a page cannot keep Ctrl+W, Ctrl+S or
Ctrl+D, which moving and crouching would press, from the browser) */
#ifdef HALO_WEB
#define CROUCH_KEYS "\"C\""
#else
#define CROUCH_KEYS "\"Left Ctrl, C\""
#endif

/* display.resolution_scaling's default: on Android the Xbox's 480 lines, which
a phone's GPU draws with time to spare, elsewhere the window's resolution */
#ifdef HALO_ANDROID
#define RESOLUTION_SCALING "\"original\""
#else
#define RESOLUTION_SCALING "\"native\""
#endif

static const struct config_setting config_settings[] =
{
	{ "display.fullscreen", _config_boolean, DEFAULT_FULLSCREEN, "HALO_FULLSCREEN", _environment_value, _platform_desktop,
		"When display.mode is empty: true starts borderless, false in a window.\n"
		"F11 switches." },
	{ "display.mode", _config_string, "\"\"", "HALO_DISPLAY_MODE", _environment_value, _platform_desktop,
		"\"fullscreen\", \"borderless\" or \"windowed\". Empty: display.fullscreen's.\n"
		"F11 switches to the window and back." },
	{ "display.resolution", _config_string, "\"native\"", "HALO_RESOLUTION", _environment_value, _platform_desktop,
		"What fullscreen and borderless draw at: \"native\", the display's own, or\n"
		"\"<width>x<height>\" (\"1920x1080\"), 640x480 or more. Fullscreen sets the\n"
		"display to it; borderless draws it scaled to the display." },
	{ "display.resolution_scaling", _config_string, RESOLUTION_SCALING, "HALO_RESOLUTION_SCALING", _environment_value,
		_platform_all,
		"\"native\" draws at the window's resolution (fullscreen, the display's or\n"
		"display.resolution; on Android, the display's); \"75%\", \"50%\" or \"25%\"\n"
		"draw that part of it each way and scale it up; \"original\" draws the\n"
		"Xbox's 480 lines (640x480; on Android, at the display's shape) and\n"
		"scales them up, as Android does unless this is set." },
	{ "display.window_size", _config_string, "\"\"", "HALO_WINDOW_SIZE", _environment_value, _platform_desktop,
		"The window's size, \"<width>x<height>\" (\"1920x1080\"), 640x480 or more.\n"
		"Empty: display.window_scale's." },
	{ "display.window_scale", _config_integer, "2", "HALO_WINDOW_SCALE", _environment_value, _platform_desktop,
		"When display.window_size is empty: the window's size as a multiple of\n"
		"640x480." },
	{ "display.screen_width", _config_integer, "0", "HALO_SCREEN_WIDTH", _environment_value, _platform_android,
		"The picture's width in columns: 0 matches the display, 640 is 4:3." },
	{ "display.texture_decode", _config_string, "\"auto\"", "HALO_TEXTURE_DECODE", _environment_value,
		_platform_android,
		"Where compressed textures are unpacked: \"gpu\", \"cpu\" (more memory) or\n"
		"\"auto\" (\"cpu\" on Adreno graphics, \"gpu\" on others)." },
	{ "display.vsync", _config_boolean, "true", "HALO_NO_VSYNC", _environment_set_is_false, _platform_all,
		"Wait for the display between frames." },
	{ "display.max_fps", _config_integer, "0", "HALO_MAX_FPS", _environment_value, _platform_desktop,
		"With vsync off, the most frames a second: 0 for twice the display's\n"
		"refresh rate, -1 for no limit (which can hang some Intel graphics)." },
	{ "display.frames_in_flight", _config_integer, "3", "HALO_FRAMES_IN_FLIGHT", _environment_value,
		_platform_desktop,
		"The most frames the GPU has queued (1 to 8): a frame waits for the GPU\n"
		"to finish the one this many before it, so that frames come evenly when\n"
		"the GPU is the slower; 0 leaves it to the driver." },
	{ "display.performance", _config_string, "\"off\"", "HALO_PERFORMANCE", _environment_value, _platform_all,
		"A line of performance at the top of the screen, above the HUD: \"fps\"\n"
		"(the frames a second), \"minimal\" (and the frame time), \"full\" (and\n"
		"the frame rate's 1% low and the draws of a frame), or \"off\"." },
	{ "display.performance_position", _config_string, "\"top_right\"", "HALO_PERFORMANCE_POSITION",
		_environment_value, _platform_all,
		"Where display.performance's line is: \"top_left\" or \"top_right\"." },
	{ "display.performance_ping", _config_boolean, "true", "HALO_PERFORMANCE_PING", _environment_value, /* AE hook */
		_platform_all,
		"In a network game, display.performance's line ends with this machine's\n"
		"ping to the host (the host: MAX PING, its slowest client's)." },
	{ "display.coordinates", _config_boolean, "false", "HALO_COORDINATES", _environment_value, /* AE hook */
		_platform_all,
		"For map makers: the performance line (alone when display.performance\n"
		"is off) ends with player 1's position in world units and facing in\n"
		"degrees: \"X 97.65 Y -169.62 Z 0.26  FACING 21\" (the camera's\n"
		"position while they have no body)." },
	/* (display.performance's before it: config_carry_over) */
	{ "display.fps_counter", _config_string, "\"off\"", "HALO_FPS_COUNTER", _environment_value, _platform_none,
		"The frames a second, at the top of the screen: \"top_left\",\n"
		"\"top_right\", or \"off\" (before that, true for the top right)." },
	{ "display.performance_overlay", _config_string, "\"off\"", "HALO_PERFORMANCE_OVERLAY", _environment_value,
		_platform_none,
		"The frame rate, its 1% low, the frame time and the draws of a frame, at\n"
		"the top of the screen: \"top_left\", \"top_right\", or \"off\" (before\n"
		"that, true for the top right)." },
	{ "display.anti_aliasing", _config_string, "\"off\"", "HALO_ANTI_ALIASING", _environment_value, _platform_all,
		"Smoothing of jagged edges, which the Xbox did not have: \"off\"; \"fxaa\"\n"
		"or \"smaa\" smooth the 3D view once it is drawn (the HUD and menus stay\n"
		"sharp); \"ssaa2x\" draws at twice the resolution each way (four times\n"
		"the work); \"msaa2x\", \"msaa4x\" or \"msaa8x\" draw with that many samples\n"
		"a pixel. Android has \"fxaa\" for \"smaa\", and no \"ssaa2x\". The default:\n"
		"\"msaa2x\" on 64-bit computers, \"off\" on 32-bit ones and Android." },
	{ "display.interpolation", _config_boolean, "true", "HALO_INTERPOLATION", _environment_value, _platform_all,
		"Draw a frame for every display refresh, blending between the game's 30\n"
		"ticks a second." },
	{ "display.direct_camera", _config_boolean, "true", "HALO_DIRECT_CAMERA", _environment_value, _platform_desktop,
		"In first person, point the view where the player aims now instead of\n"
		"where the last tick left it: the view turns the frame the mouse moves,\n"
		"not up to two ticks (66 ms) later." },
	{ "display.fov", _config_real, "0.0", "HALO_FOV", _environment_value, _platform_all,
		"The first-person view's field of view on foot, in degrees across at\n"
		"16:9 (20 to 150); 0 keeps the stock view. Vehicles, cinematics and\n"
		"scripted cameras keep their own." },
	{ "display.viewmodel_fov", _config_real, "0.0", "HALO_VIEWMODEL_FOV", _environment_value, _platform_all,
		"The first-person weapon's and hands' field of view, in degrees across\n"
		"at 16:9 (20 to 150); 0 keeps the weapon's stock view, also when\n"
		"display.fov widens the world." },
	{ "display.viewmodel_visible", _config_boolean, "true", "HALO_VIEWMODEL_VISIBLE", _environment_value, _platform_all,
		"Draw the first-person weapon, hands and what is attached to them.\n"
		"Off, they are not drawn; firing, animation, sound and lights go on." },
	{ "display.anisotropic", _config_string, "\"off\"", "HALO_ANISOTROPIC", _environment_value, /* AE hook */
		_platform_all,
		"Anisotropic filtering of the world's textures: \"off\" (as the Xbox:\n"
		"floors and walls blur at a grazing angle), \"2x\", \"4x\", \"8x\" or \"16x\"\n"
		"(sharper, at a little cost; the graphics card's most at most)." },
	{ "display.zoom_blur", _config_boolean, "true", "HALO_ZOOM_BLUR", _environment_value, /* AE hook */
		_platform_all,
		"The blur a zoomed weapon's scope puts at the screen's edges (the weapon's\n"
		"HUD screen effect, single view only), as Halo does; false: a clear view\n"
		"to the edges. The scope's own mask and its tint stay." },
	{ "display.far_clip_distance", _config_real, "1024.0", "HALO_FAR_CLIP_DISTANCE", _environment_value, _platform_all,
		"How far the world is drawn, up to 10000. 1024: the Xbox's." },
	{ "display.match_timer", _config_boolean, "false", "HALO_MATCH_TIMER", _environment_value, _platform_all,
		"In a multiplayer game with a time limit, show the time left at the top\n"
		"of the screen; false shows it only on the scoreboard, as the Xbox did.\n"
		"(OpenCE build-244's match clock of this name is never drawn in Arena\n"
		"Evolved, whose own is display.match_clock.)" },
	{ "display.passenger_third_person", _config_boolean, "false", "HALO_PASSENGER_THIRD_PERSON", _environment_value, _platform_all,
		"Third-person Warthog and Scorpion passengers. Weapon zoom is disabled." },
	{ "display.high_res_hud", _config_boolean, "true", "HALO_HIGH_RES_HUD", _environment_value, _platform_all,
		"Draw the HUD (meters, counters, panels, motion sensor, reticles,\n"
		"waypoints, scopes) from the high-res assets (8x the maps' bitmaps);\n"
		"false draws the maps' own bitmaps." },
	{ "display.hud_button_style", _config_string, "\"classic\"", "HALO_HUD_BUTTON_STYLE", _environment_value, _platform_all,
		"High-res gameplay controller buttons: classic uses plain Xbox faces;\n"
		"gems uses shaded Xbox faces; menu_gems uses the menu's button art.\n"
		"Requires display.high_res_hud." },
	{ "display.ally_reticle", _config_boolean, "true", "HALO_ALLY_RETICLE", _environment_value, _platform_all, /* AE hook */
		"ALLY RETICLE: your reticle turns green when you aim at an ally (a\n"
		"teammate, a co-op partner, a marine), as it turns red over an enemy.\n"
		"Only what this machine draws changes." },
	{ "display.high_res_text", _config_boolean, "true", "HALO_HIGH_RES_TEXT", _environment_value, _platform_all,
		"Draw the menus' and HUD's text and titles with high-res fonts." },
	{ "display.high_res_font", _config_string, "\"\"", "HALO_HIGH_RES_FONT", _environment_value, _platform_all,
		"A font file (.ttf or .otf) for the high-res text and Online Games. A\n"
		"name alone is a file in data/fonts beside the maps folder. Empty: the\n"
		"built-in fonts." },
	{ "display.high_res_font_large", _config_string, "\"\"", "HALO_HIGH_RES_FONT_LARGE", _environment_value,
		_platform_all,
		"A font file for the large text only, as display.high_res_font. Empty:\n"
		"display.high_res_font's." },
	{ "display.version_watermark", _config_boolean, "false", "HALO_VERSION_WATERMARK", _environment_value,
		_platform_all,
		"Show this build's version (as the main menu does) in the bottom right\n"
		"corner while playing too: handy for screenshots and bug reports." },
	{ "display.shadow_resolution", _config_integer, "128", "HALO_SHADOW_RESOLUTION", _environment_value,
		_platform_all,
		"The size the objects' shadows are drawn at, in pixels each way: 128 as\n"
		"on the Xbox, or 256, 512 or 1024 for smoother edges, as soft." },
	{ "display.menus", _config_string, "\"pc\"", "HALO_MENUS", _environment_value, _platform_all,
		"The menus: \"pc\" (the default) for the PC version's main menu\n"
		"(port/assets/menus, and a menus folder here for your own), where the\n"
		"Arena Evolved settings are; \"xbox\" for the Xbox's (with Online Games)." },
	{ "display.arena_menus", _config_boolean, "true", "HALO_ARENA_MENUS", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved's own menus (the default since 0.3.7): true shows them in\n"
		"place of the game's; false keeps the menus display.menus picks (the\n"
		"older PC or Xbox menus). Read at start-up." },
	{ "display.arena_menus_scale", _config_integer, "100", "HALO_ARENA_MENUS_SCALE", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved's menus: their size, 90, 100, 115 or 130 (percent)." },
	{ "display.arena_menus_reduce_motion", _config_boolean, "false", "HALO_ARENA_MENUS_REDUCE_MOTION", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved's menus: true draws every change at once (no sliding\n"
		"or fading; the text caret stays on)." },
	{ "display.enemy_name_color", _config_string, "\"classic\"", "HALO_ENEMY_NAME_COLOR", _environment_value,
		_platform_all,
		"Enemies' names (anyone's in a free for all), the one under your\n"
		"reticle and those over heads (display.player_names): \"red\" in red,\n"
		"as the Master Chief Collection's ENEMY PLAYER NAME COLOR; \"classic\"\n"
		"in the HUD's blue, as the game drew its name under the reticle. Only\n"
		"this machine's look." },
	{ "display.dead_ally_markers", _config_boolean, "true", "HALO_DEAD_ALLY_MARKERS", _environment_value, _platform_all, /* AE hook */
		"DEAD ALLY MARKERS: in a team game, a cross over a fallen\n"
		"teammate's body, seen through walls, until they respawn. It takes their\n"
		"name's colour, WAYPOINTS' rows and HUD STYLE's look. Only what this\n"
		"machine draws changes." },
	{ "display.menu_language", _config_string, "\"maps\"", "HALO_MENU_LANGUAGE", _environment_value, _platform_all,
		"The PC menus' language: \"maps\" for the language of the Custom Edition\n"
		"whose loc.map is in custom_maps (as that edition shows them; English\n"
		"without one), \"english\" for English. The port's own lines are English." },
	{ "display.player_names", _config_string, "\"all\"", "HALO_PLAYER_NAMES", _environment_value, _platform_all,
		"In multiplayer, whose names are drawn above their heads: \"all\",\n"
		"\"allies\", \"enemies\" or \"none\"." },
	{ "display.player_name_scale", _config_real, "1.0", "HALO_PLAYER_NAME_SCALE", _environment_value, _platform_all,
		"The size of player names, 0.25 to 4." },
	{ "display.follow_camera", _config_string, "\"long_waits\"", "HALO_FOLLOW_CAMERA", _environment_value,
		_platform_all,
		"After a death, follow a living player's view: \"long_waits\" (only\n"
		"during a long respawn wait), \"always\" or \"off\". LB and RB, or Q and E,\n"
		"switch players." },
	{ "display.follow_camera_after", _config_real, "5.0", "HALO_FOLLOW_CAMERA_AFTER", _environment_value,
		_platform_all,
		"For \"long_waits\": the seconds of respawn wait left that start the\n"
		"follow camera." },
	{ "display.powerup_timers", _config_boolean, "false", "HALO_POWERUP_TIMERS", _environment_value, _platform_all,
		"Show the seconds left on your active camouflage, overshield and full\n"
		"spectrum vision." },
	{ "display.loading_screen_min_seconds", _config_real, "0.0", "HALO_LOADING_SCREEN_MIN_SECONDS",
		_environment_value, _platform_none,
		"ChupathingyCE 0.8.0's name of display.loading_screen_campaign_seconds,\n"
		"read into it once (config_carry_over)." },
	{ "display.scoreboard_team_layout", _config_string, "\"teams\"", "HALO_SCOREBOARD_TEAM_LAYOUT", _environment_value,
		_platform_all,
		"A team game's scoreboard: \"teams\" (a column for each team) or \"score\"\n"
		"(everyone in order of score)." },
	{ "display.scoreboard_background", _config_boolean, "true", "HALO_SCOREBOARD_BACKGROUND", _environment_value,
		_platform_all,
		"Draw a panel behind the multiplayer scoreboard." },
	{ "display.scoreboard_platforms", _config_string, "\"auto\"", "HALO_SCOREBOARD_PLATFORMS", _environment_value,
		_platform_all,
		"Show each Chupathingy player's platform logo on the scoreboard: \"auto\"\n"
		"(Chupathingy menus only), \"on\" or \"off\"." },
	{ "display.show_quit_players", _config_boolean, "true", "HALO_SHOW_QUIT_PLAYERS", _environment_value,
		_platform_all,
		"Keep players who quit on the multiplayer scoreboard." },
	{ "display.scoreboard_background_color", _config_string, "\"16, 16, 16, 150\"", "HALO_SCOREBOARD_BACKGROUND_COLOR",
		_environment_value, _platform_all,
		"The scoreboard panel's colour: \"red, green, blue, alpha\", each 0 to 255\n"
		"(alpha 0 is see-through, 255 solid)." },
	{ "display.hud_style", _config_string, "\"ae\"", "HALO_HUD_STYLE", _environment_value, /* AE hook */
		_platform_all,
		"HUD STYLE: \"ae\" draws the score strip and Arena Evolved's look for its\n"
		"own HUD elements; \"classic\" draws the HUD exactly as before. A setting\n"
		"of this machine's display only: nothing goes over the network." },
	{ "display.match_clock", _config_string, "\"down\"", "HALO_MATCH_CLOCK", _environment_value, _platform_all,
		"A clock in the bottom right corner of a multiplayer game's view: \"down\"\n"
		"the time left of the gametype's time limit (with none, the time played),\n"
		"\"up\" the time played, \"both\" the time left with the time played smaller\n"
		"above it (with no time limit, the time played alone), or \"off\". On Halo\n"
		"1: NHE's maps their own clock is never shown: this one is in its place,\n"
		"and off shows none." },
	{ "display.match_clock_labels", _config_boolean, "false", "HALO_MATCH_CLOCK_LABELS", _environment_value,
		_platform_all,
		"A small heading over each line of the match clock in the corner: TIME\n"
		"LEFT over the time left, ELAPSED over the time played (with BOTH, each\n"
		"line its own); the power column stays a cap above the top one." },
	{ "display.shotgun_power", _config_boolean, "true", "HALO_SHOTGUN_POWER", _environment_value, _platform_all,
		"SHOTGUN AS POWER: the shotgun is a power item, with its row in TIMERS'\n"
		"power column, TRAINING's waypoints and the callouts; false a weapon as\n"
		"any other, from the next game. Only this machine's timers: nothing is\n"
		"sent." },
	{ "display.power_list", _config_string, "\"clock\"", "HALO_POWER_LIST", _environment_value, _platform_all,
		"Where the gametype's TIMERS and TRAINING list the power items' next\n"
		"spawns with one player: \"clock\" a column over the match clock in the\n"
		"bottom right corner, the soonest at the foot, or \"top_left\" one line in\n"
		"the top left corner. Split screen's views always have the column." },
	{ "display.campaign_timer", _config_boolean, "false", "HALO_CAMPAIGN_TIMER", _environment_value, _platform_all,
		"A clock in the bottom right corner of each view in the campaign, as the\n"
		"Master Chief Collection's: the time played on the level, in game time\n"
		"(stopped in the pause menu; the cutscenes count). A revert to a\n"
		"checkpoint keeps it; the level started again starts it at 0:00." },
	{ "display.hud_area", _config_string, "\"full\"", "HALO_HUD_AREA", _environment_value, _platform_all,
		"Where the HUD is drawn on a wider screen: \"full\" out to the screen's\n"
		"edges, \"16:9\" or \"4:3\" in a part of the screen of that shape at\n"
		"its middle (a split screen view's sides at the screen's edges move\n"
		"in). The view itself stays the full width." },
	{ "display.compact_hud", _config_boolean, "false", "HALO_COMPACT_HUD", _environment_value, _platform_all,
		"With one view, draw split screen's HUD: the game's own smaller meters,\n"
		"motion sensor and messages. The reticle stays as it is." },
	{ "display.scoreboard_fade", _config_string, "\"normal\"", "HALO_SCOREBOARD_FADE", _environment_value,
		_platform_all,
		"How fast the in-game scoreboard (hold BACK, or tab) fades in and out:\n"
		"\"instant\", \"fast\" (a quarter of a second), \"normal\" (half a\n"
		"second, as the original game) or \"slow\" (a second)." },
	{ "display.spawn_heat", _config_string, "\"mine\"", "HALO_SPAWN_HEAT", _environment_value, _platform_all,
		"The gametype's TRAINING's spawn markers coloured by how likely each\n"
		"spawn is to be picked next (CE's own rules, worked out on this machine;\n"
		"nothing is sent): \"mine\" where your player would spawn, \"enemy\"\n"
		"where the other team's would (in a team game; without teams as\n"
		"\"mine\"), \"off\" the plain green markers. Cold blue-grey is unlikely,\n"
		"hot orange-red the likeliest; dark red with a cross can't be picked\n"
		"now (an enemy within 6 m, a vehicle on it); a flash where a player\n"
		"really spawns." },
	{ "display.waypoint_style", _config_string, "\"both\"", "HALO_WAYPOINT_STYLE", _environment_value, /* AE hook */
		_platform_all,
		"What the gametype's item waypoints (TIMERS with waypoints, TRAINING) show\n"
		"by their arrows: \"text\", the item's name and the time to its spawn;\n"
		"\"icons\", the item's picture with the time in small text beside it;\n"
		"\"both\", the name and time, then the picture. A weapon's picture is\n"
		"the pickup prompt's, read from the map being played (a map or mod that\n"
		"has none shows that item's text); the overshield's and the active\n"
		"camouflage's are Arena Evolved's own. A setting of this machine's\n"
		"display only: nothing goes over the network." },
	{ "display.waypoint_size", _config_real, "70.0", "HALO_WAYPOINT_SIZE", _environment_value, /* AE hook */
		_platform_all,
		"The size of the gametype's item waypoints' labels (their name, distance,\n"
		"timer and picture), in percent of the full size: 50 to 100, in steps of 5.\n"
		"In split screen the size is smaller still, by the view's size; far items\n"
		"shrink gently (to three quarters of it at 60 metres). The power list,\n"
		"the clock and the overlay keep their sizes. A setting of this machine's\n"
		"display only: nothing goes over the network." },
	{ "display.short_messages", _config_boolean, "false", "HALO_SHORT_MESSAGES", _environment_value, _platform_all,
		"Shorter game messages, as Halo 1: NHE's: a killing spree reads\n"
		"\"Killing Spree!\" (with the count, if there is one). false keeps the\n"
		"game's own text." },
	{ "display.pause_change_teams", _config_boolean, "true", "HALO_PAUSE_CHANGE_TEAMS", _environment_value,
		_platform_all,
		"Show CHANGE TEAMS in the pause menu of a team game." },
	{ "display.server_messages", _config_boolean, "true", "HALO_SERVER_MESSAGES", _environment_value, _platform_all,
		"Show the banner for a Delta server's warnings and notices." },
	{ "display.server_message_text_color", _config_string, "\"\"", "HALO_SERVER_MESSAGE_TEXT_COLOR",
		_environment_value, _platform_all,
		"The banner's text colour, \"#RRGGBB\" or \"#RRGGBBAA\". Empty: the default." },
	{ "display.server_message_border_color", _config_string, "\"\"", "HALO_SERVER_MESSAGE_BORDER_COLOR",
		_environment_value, _platform_all,
		"The banner's outline colour, \"#RRGGBB\" or \"#RRGGBBAA\". Empty: the\n"
		"default." },
	{ "display.server_message_background_color", _config_string, "\"\"", "HALO_SERVER_MESSAGE_BACKGROUND_COLOR",
		_environment_value, _platform_all,
		"The banner's background colour, \"#RRGGBB\" or \"#RRGGBBAA\". Empty (or a\n"
		"value that does not parse): the default, dark blue." },
	{ "display.anisotropic_filtering", _config_integer, "0", "HALO_ANISOTROPIC_FILTERING", _environment_value, /* AE hook */
		_platform_all,
		"Sharpen textures seen at a grazing angle (floors, walls, terrain), which\n"
		"the Xbox did not: 2, 4, 8 or 16 times; 0 filters as the Xbox did. 4 keeps\n"
		"most of 16's sharpness at less cost on slower GPUs. (Arena Evolved:\n"
		"Settings' ANISOTROPIC FILTERING is display.anisotropic; this is OpenCE's\n"
		"own, also read: the higher of the two filters.)" },
	{ "display.dxt1_astc", _config_boolean, "true", "HALO_DXT1_ASTC", _environment_value, _platform_android,
		"On a GPU without DXT textures (Mali), give it the DXT1 textures as ASTC,\n"
		"which holds them nearly as they are in a quarter of the memory of the\n"
		"32-bit texels they are otherwise decoded to; false decodes them." },
	{ "display.latency_meter", _config_boolean, "false", "HALO_LATENCY_METER", _environment_value, _platform_all,
		"In a network game, your round trip to the host in milliseconds at the\n"
		"screen's top right, with bars (the host: its slowest player's), and\n"
		"\"Connection problem\" when the host has sent nothing for two seconds;\n"
		"the scoreboard's pings in the same colours, as this machine times them." },
	{ "display.shader_cache", _config_boolean, "true", "HALO_SHADER_CACHE", _environment_value, _platform_desktop,
		"Keep the GPU programs the game links under the save root (shader_cache),\n"
		"and load them from there next time instead of linking them again, which\n"
		"stalls a frame on some drivers; false links each every run." },
	{ "display.per_pixel_lighting", _config_boolean, "false", "HALO_PER_PIXEL_LIGHTING", _environment_value,
		_platform_all,
		"Light the models (characters, weapons, vehicles, scenery) for each\n"
		"pixel by the lights the game gives them, without the facets the light\n"
		"of each vertex shows across curved surfaces; false lights each vertex,\n"
		"as the Xbox does." },
	{ "display.structure_culling", _config_integer, "31", "HALO_STRUCTURE_CULLING", _environment_value,
		_platform_all,
		"Which tests skip the level's triangles the camera cannot see, as bits\n"
		"added together: 1 outside the view, 2 groups facing away, 4 triangles\n"
		"facing away, 8 smaller groups, 16 nearer rooms first; 0 walks them as\n"
		"the Xbox does. The console's structure_culling changes it until the\n"
		"game quits." },
	{ "display.model_detail", _config_string, "\"stock\"", "HALO_MODEL_DETAIL", _environment_value, _platform_all,
		"Which of their levels of detail models are drawn at: \"stock\" chooses by\n"
		"their size on the Xbox's 480 lines, at any resolution; \"native\" by their\n"
		"size on the display's, so that more detail is kept at a distance at high\n"
		"resolutions; \"highest\" draws the models \"native\" does, each at its most\n"
		"detailed level." },
	{ "display.hdr", _config_boolean, "false", "HALO_HDR", _environment_value, _platform_all,
		"Draw the 3D view into a floating-point back buffer, which keeps what\n"
		"the game's additive effects (explosions, plasma, lights, the sun's\n"
		"glare) add past white, and tone map it before the HUD is drawn:\n"
		"display.hdr_tone_mapping brings it back to the display's range\n"
		"instead of clipping it. false draws into 8 bits a channel, as the\n"
		"Xbox does." },
	{ "display.hdr_exposure", _config_real, "1.0", "HALO_HDR_EXPOSURE", _environment_value, _platform_all,
		"With display.hdr, the 3D view's brightness in linear light before it\n"
		"is tone mapped, 0.25 to 4.0." },
	{ "display.hdr_tone_mapping", _config_string, "\"neutral\"", "HALO_HDR_TONE_MAPPING", _environment_value,
		_platform_all,
		"With display.hdr, the curve that brings what is past white into the\n"
		"display's range: \"neutral\" (the picture as drawn, with only its\n"
		"brightest parts rolled off), \"aces\" (filmic, more contrast) or\n"
		"\"reinhard\" (softer, and darker throughout)." },
	{ "display.hdr_output", _config_boolean, "false", "HALO_HDR_OUTPUT", _environment_value, _platform_desktop,
		"With display.hdr, ask for a floating-point window (scRGB), so that an\n"
		"HDR display shows what is past white brighter than the desktop's\n"
		"white rather than tone mapped down to it; the operating system's HDR\n"
		"must be on. Taken up when the game starts." },

	{ "audio.enabled", _config_boolean, "true", "HALO_NO_AUDIO", _environment_set_is_false, _platform_all,
		"Play sound." },
	{ "audio.volume", _config_real, "1.0", "HALO_VOLUME", _environment_value, _platform_all,
		"The volume of everything, 0.0 to 1.0." },
	{ "audio.music_volume", _config_real, "1.0", "HALO_MUSIC_VOLUME", _environment_value, _platform_all,
		"The music's volume, 0.0 to 1.0." },
	{ "audio.effects_volume", _config_real, "1.0", "HALO_EFFECTS_VOLUME", _environment_value, _platform_all,
		"The volume of every other sound (effects and speech), 0.0 to 1.0 (of\n"
		"audio.volume)." },
	{ "audio.arena_menus_volume", _config_real, "1.0", "HALO_ARENA_MENUS_VOLUME", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved's menus: their sounds' volume, 0.0 to 1.0." },
	{ "audio.buffer_frames", _config_integer, DEFAULT_AUDIO_BUFFER_FRAMES, "HALO_AUDIO_BUFFER_FRAMES", _environment_value,
		_platform_all,
		"The audio device's buffer, in sample frames at 48 kHz (64 to 8192): larger\n"
		"rides out stalls that cut the sound out, smaller has less delay. 2048\n"
		"(43 ms) on macOS, 1024 on Android, 512 (11 ms) elsewhere." },
	{ "audio.channels", _config_string, "\"stereo\"", "HALO_AUDIO_CHANNELS", _environment_value, _platform_desktop,
		"\"stereo\", \"5.1\" (surround, as the Xbox's Dolby Digital), or \"auto\"\n"
		"for 5.1 when the sound device has six or more channels." },
	{ "audio.reverb", _config_boolean, "true", "HALO_REVERB", _environment_value, _platform_all,
		"Reverberate the world's sounds as the place the player is in does (the\n"
		"maps' sound environments, as the Xbox's I3DL2 reverb did); false keeps\n"
		"them dry." },
	{ "audio.nhe_sound_filter", _config_boolean, "false", "HALO_NHE_SOUND_FILTER", _environment_value, /* AE hook */
		_platform_all,
		"Mute the sounds Halo 1: NHE 1.0 took out of its maps, on any map, for\n"
		"this machine: the plasma pistol's charging hum and the needler's firing\n"
		"(NHE SOUND FILTER on NHE OPTIONS)." },
	{ "audio.resampler", _config_string, "\"sinc\"", "HALO_AUDIO_RESAMPLER", _environment_value, _platform_all,
		"How sounds are converted to the output's rate: \"sinc\" with a\n"
		"band-limited low pass; \"linear\" between neighbouring samples,\n"
		"brighter for the aliasing it lets through." },
	{ "audio.resampling", _config_string, "\"\"", "HALO_AUDIO_RESAMPLING", _environment_value, _platform_none, /* AE hook */
		"Earlier releases' name of audio.resampler (ChupathingyCE's), read into it once\n"
		"(config_carry_over)." },
	{ "audio.voice_chat", _config_string, "\"push_to_talk\"", "HALO_VOICE_CHAT", _environment_value, _platform_all,
		"Talking in network games' voice chat: \"push_to_talk\" (while\n"
		"controls.push_to_talk is held; the microphone opens the first time),\n"
		"\"open_mic\" (whenever the microphone hears speech), or \"off\". Others'\n"
		"voices play whatever this is (audio.voice_volume 0 silences them)." },
	{ "audio.voice_volume", _config_real, "1.0", "HALO_VOICE_VOLUME", _environment_value, _platform_all,
		"The volume of the other players' voices (0 to 2)." },
	{ "audio.output_device", _config_string, "\"default\"", "HALO_AUDIO_OUTPUT_DEVICE", _environment_value,
		_platform_desktop,
		"Where the game's sound and the voices play: a device's name as Settings >\n"
		"Audio lists it, or \"default\" for the system's (also if it is not found)." },
	{ "audio.input_device", _config_string, "\"default\"", "HALO_AUDIO_INPUT_DEVICE", _environment_value,
		_platform_desktop,
		"The microphone voice chat listens to: a device's name as Settings >\n"
		"Audio lists it, or \"default\" for the system's (also if it is not found)." },

	{ "capture.ffmpeg_path", _config_string, "\"\"", "HALO_FFMPEG", _environment_value, _platform_desktop,
		"The ffmpeg program F9's recordings are made with (it is not shipped\n"
		"with the game): its path, or a name looked for on the PATH. Empty:\n"
		"ffmpeg beside the game, else on the PATH." },
	{ "capture.record_fps", _config_integer, "60", "HALO_RECORD_FPS", _environment_value, _platform_desktop,
		"The recordings' frames a second, 60 or 30 (anything else is 60),\n"
		"whatever the game's frame rate: frames are repeated or left out to\n"
		"keep to real time." },
	{ "capture.record_quality", _config_string, "\"medium\"", "HALO_RECORD_QUALITY", _environment_value,
		_platform_desktop,
		"The recordings' quality and size: \"low\", \"medium\" or \"high\" (H.264\n"
		"at CRF 28, 23 or 18)." },
	{ "capture.record_indicator", _config_boolean, "true", "HALO_RECORD_INDICATOR", _environment_value,
		_platform_desktop,
		"A red dot at the top right while recording (never in the recording)." },
	{ "capture.record_directory", _config_string, "\"\"", "HALO_RECORD_DIR", _environment_value, _platform_desktop,
		"The folder recordings go to. Empty: recordings/ beside this file." },

	{ "input.touch_controls", _config_string, "\"auto\"", "HALO_TOUCH_CONTROLS", _environment_value, _platform_android | _platform_web,
		"The on-screen touch controls in a game: \"auto\" shows them on a\n"
		"touchscreen while no controller is connected, \"on\" also with a\n"
		"controller, \"off\" never. A device without a touchscreen never shows\n"
		"them. The menus take taps in any case." },
	{ "input.touch_aim_assist", _config_boolean, "true", "HALO_TOUCH_AIM_ASSIST", _environment_value, _platform_android | _platform_web,
		"The touch controls' swipe aiming gets a controller's aim assist: the\n"
		"aim slows over a target and follows a moving one. false: none, as a\n"
		"mouse (the bullets' own autoaim stays)." },
	{ "input.mouse_sensitivity", _config_real, "1.0", "HALO_MOUSE_SENSITIVITY", _environment_value, _platform_desktop,
		"How fast the view turns with the mouse." },
	{ "input.invert_mouse", _config_boolean, "false", "HALO_MOUSE_INVERT", _environment_set_is_true, _platform_desktop,
		"Moving the mouse forward looks down." },
	{ "network.block_mouse_aim_assist", _config_boolean, "false", "HALO_NET_BLOCK_MOUSE_AIM_ASSIST", _environment_value,
		_platform_all, "Host: disable optional mouse magnetism for mouse/keyboard players. Controllers and touch keep their own assist." },
	{ "network.shield_effects", _config_string, "\"stock\"", "HALO_NET_SHIELD_EFFECTS", _environment_value, _platform_all,
		"Host: the shield effects every player of a network game sees, whatever\n"
		"their own: \"stock\", \"glow\", \"light_show\", \"overshield\",\n"
		"\"glow_overshield\" or \"light_show_overshield\" (display.shield_glow,\n"
		"its light_show intensity, display.overshield_effect); \"players\": not\n"
		"synced, each player sees their own settings'. SHIELD EFFECTS in Server\n"
		"Setup > Player Options writes its choice here." },
	{ "network.fire_origins", _config_boolean, "true", "HALO_NET_FIRE_ORIGINS", _environment_value, _platform_all,
		"Host: whether a reported hit the shooter's camera had no line of sight\n"
		"to may still count from where their machine says the shot left, when\n"
		"that is not the camera (a VR build's hand: past a corner the camera is\n"
		"behind), no further than a player leaning out and an arm, and in front\n"
		"of the eye. false: from the camera and vehicles' guns only." },
	{ "input.keyboard_prompts", _config_boolean, "true", "HALO_KEYBOARD_PROMPTS", _environment_value, _platform_desktop,
		"Show keys in button prompts while using the keyboard and mouse." },
	{ "input.mouse_aim_assist", _config_boolean, "false", "HALO_MOUSE_AIM_ASSIST", _environment_value, _platform_desktop,
		"Aim assist for the mouse, as with a controller." },
	{ "input.mouse_vertical_sensitivity", _config_real, "0.0", "HALO_MOUSE_VERTICAL_SENSITIVITY", _environment_value,
		_platform_desktop,
		"How far the view turns up and down for the mouse's movement; 0 for the\n"
		"same as input.mouse_sensitivity." },
	{ "input.mouse_style", _config_string, "\"classic\"", "HALO_MOUSE_STYLE", _environment_value, /* AE hook */
		_platform_desktop,
		"How the mouse's sensitivity numbers work: \"classic\" (this port's own\n"
		"input.mouse_sensitivity and input.mouse_vertical_sensitivity) or \"mcc\",\n"
		"where input.mouse_mcc_sensitivity matches Halo: The Master Chief\n"
		"Collection's number (the same for turning and looking up and down; 1.0\n"
		"is about 1% above CS:GO at sensitivity 1). Only the MCC style reads\n"
		"input.mouse_zoom_scale and input.mouse_vehicle_scale." },
	{ "input.mouse_mcc_sensitivity", _config_real, "0.0", "HALO_MOUSE_MCC_SENSITIVITY", _environment_value, /* AE hook */
		_platform_desktop,
		"The MCC style's sensitivity, 0.1 to 10: the view turns this / 45 degrees\n"
		"for each count of the mouse. 0: not set yet; the first time the style is\n"
		"mcc it becomes input.mouse_sensitivity x 5.6723 (one decimal), which keeps\n"
		"the feel." },
	{ "input.mouse_zoom_scale", _config_real, "1.0", "HALO_MOUSE_ZOOM_SCALE", _environment_value, /* AE hook */
		_platform_desktop,
		"The MCC style's zoomed sensitivity scale, 0.1 to 2: multiplies the mouse\n"
		"while a weapon is zoomed, on top of the weapon's own magnification\n"
		"(the view already turns slower the more it zooms). Not read in the\n"
		"classic style." },
	{ "input.mouse_vehicle_scale", _config_real, "1.0", "HALO_MOUSE_VEHICLE_SCALE", _environment_value, /* AE hook */
		_platform_desktop,
		"The MCC style's vehicle sensitivity scale, 0.1 to 2: multiplies the mouse\n"
		"while seated in a vehicle (any seat: driver, gunner or passenger). Not\n"
		"read in the classic style." },
	{ "input.zoom_mode", _config_string, "\"toggle\"", "HALO_ZOOM_MODE", _environment_value, /* AE hook */
		_platform_all,
		"How the zoom action (a mouse button, a key or a controller button) works:\n"
		"\"toggle\", each press steps the zoom level in, to the next level and out, as\n"
		"Halo does; \"hold\", zoomed to the first level while it is held (no second\n"
		"level); \"both\", a tap steps the level as toggle does and a hold zooms\n"
		"while it is held, then returns to unzoomed." },
	{ "input.adaptive_triggers", _config_boolean, "true", "HALO_ADAPTIVE_TRIGGERS", _environment_value, /* AE hook */
		_platform_desktop,
		"A PS5 DualSense's adaptive triggers (ae_ds5.c): R2 a feel for each\n"
		"weapon and a kick at each shot, free while reloading, switching or\n"
		"empty; L2 light while zoomed. Other pads are never touched. On Linux\n"
		"it needs write access to the pad's hidraw node (Steam's udev rules,\n"
		"or the download's 70-ae-dualsense.rules)." },
	{ "input.trigger_strength", _config_string, "\"normal\"", "HALO_TRIGGER_STRENGTH", _environment_value, /* AE hook */
		_platform_desktop,
		"How strong a DualSense's trigger effects are: \"light\" (a hair\n"
		"trigger: every hold or click a short light break at the top of the\n"
		"pull, the rest at three quarters), \"normal\", \"strong\" (every effect x1.75\n"
		"and at least a step up, the motors x1.4)." },
	{ "input.lightbar", _config_string, "\"player_color\"", "HALO_LIGHTBAR", _environment_value, /* AE hook */
		_platform_desktop,
		"A PS5 DualSense's lightbar and player lights: \"player_color\", the\n"
		"lightbar in your color (your team's in a team game) with the powerups'\n"
		"glows (black armour: as \"shield\"); \"shield\", the lightbar your\n"
		"shield as the HUD's meter shows it;\n"
		"either way the player lights your split-screen number. \"off\": left alone." },
	{ "input.left_stick_deadzone", _config_integer, "0", "HALO_LEFT_STICK_DEADZONE", _environment_value, /* AE hook */
		_platform_all,
		"Every controller's left stick's dead zone, a percent of its throw on\n"
		"each axis: how far it moves before the player does. 0: the game's own\n"
		"(9000 of 32767, about 27%)." },
	{ "input.right_stick_deadzone", _config_integer, "0", "HALO_RIGHT_STICK_DEADZONE", _environment_value, /* AE hook */
		_platform_all,
		"Every controller's right stick's dead zone, a percent of its throw on\n"
		"each axis: how far it moves before the view turns. 0: the game's own\n"
		"(9000 of 32767, about 27%)." },
	{ "input.zoom_reset", _config_string, "\"first\"", "HALO_ZOOM_RESET", _environment_value, /* AE hook */
		_platform_all,
		"Where a hold of the zoom action (the \"hold\" mode, and the hold of\n"
		"\"both\") starts: \"first\", at the weapon's first zoom level; \"last\", at\n"
		"the last level used on that weapon in hand, for each local player. While\n"
		"zoom is held, the mouse wheel steps the level up and down (it does not\n"
		"switch weapons) and the flashlight button steps to the next level, and\n"
		"back to the first after the last. In \"hold\", a tap and then a hold\n"
		"within the hold time starts at the second level." },
	{ "input.back_right", _config_string, "\"none\"", "HALO_BACK_RIGHT", _environment_value, /* AE hook */
		_platform_desktop,
		"What a controller's right back button does (a Switch 2 Pro\n"
		"Controller's GR, an Elite's or DualSense Edge's upper right paddle): the\n"
		"Xbox button it presses, so it follows your profile's controller layout:\n"
		"\"a\", \"b\", \"x\", \"y\", \"black\", \"white\", \"left_stick\",\n"
		"\"right_stick\", \"back\", \"start\", \"up\", \"down\", \"left\", \"right\";\n"
		"\"none\": nothing." },
	{ "input.back_left", _config_string, "\"none\"", "HALO_BACK_LEFT", _environment_value, /* AE hook */
		_platform_desktop,
		"What a controller's left back button does (GL, the upper left\n"
		"paddle): as input.back_right." },
	{ "input.zoom_hold_time", _config_real, "0.25", "HALO_ZOOM_HOLD_TIME", _environment_value, /* AE hook */
		_platform_all,
		"The most seconds a press of the zoom action lasts to be a tap in the\n"
		"\"both\" mode, 0.1 to 1: held longer, it is a hold." },
	{ "input.mouse_zoom_mode", _config_string, "\"same\"", "HALO_MOUSE_ZOOM_MODE", _environment_value, /* AE hook */
		_platform_all,
		"The zoom action's mode while you play with the keyboard and mouse:\n"
		"\"toggle\", \"hold\" or \"both\" (as input.zoom_mode), or \"same\", the\n"
		"controller's (input.zoom_mode). Each player has their own (SETTINGS:\n"
		"CONTROLS, in their profile's ae.toml)." },
	{ "input.wheel_zoom", _config_boolean, "false", "HALO_WHEEL_ZOOM", _environment_value, _platform_all, /* AE hook */
		"WHEEL ZOOM: with the keyboard and mouse, the mouse wheel zooms by itself\n"
		"while the weapon in hand zooms: up a level in, down a level out. It then\n"
		"never switches weapons (the switch weapon key still does); with a weapon\n"
		"that does not zoom it switches weapons as before. Each player has their\n"
		"own (SETTINGS: CONTROLS, in their profile's ae.toml)." },
	{ "input.mouse_zoom_reset", _config_string, "\"same\"", "HALO_MOUSE_ZOOM_RESET", _environment_value, /* AE hook */
		_platform_all,
		"Where a hold of the mouse's zoom starts: \"first\" or \"last\" (as\n"
		"input.zoom_reset), or \"same\", the controller's." },
	{ "input.mouse_zoom_hold_time", _config_real, "0.0", "HALO_MOUSE_ZOOM_HOLD_TIME", _environment_value, /* AE hook */
		_platform_all,
		"The mouse's hold time in the \"both\" mode, 0.1 to 1 seconds; 0: the\n"
		"controller's (input.zoom_hold_time)." },
	/* gyro aiming (port/linux/src/xinput_sdl.c): the controller's gyroscope,
	each local player from their own */
	{ "input.gyro_aim", _config_string, "\"false\"", "HALO_GYRO_AIM", _environment_value, _platform_all, /* AE hook */
		"Aim with the controller's gyroscope, each local player from their own (a\n"
		"DualSense, a DualShock 4, a Pro Controller, a Joy-Con; on the Switch the\n"
		"console held in the hands): \"true\" always, \"zoom\" only while zoomed in\n"
		"(a scope), \"false\" never. SETTINGS > CONTROLS > GYRO. Changes at once." },
	{ "input.gyro_invert_horizontal", _config_boolean, "false", "HALO_GYRO_INVERT_HORIZONTAL",
		_environment_value, _platform_all, /* AE hook */
		"Invert the gyroscope's yaw. false: turning the controller right looks\n"
		"right. true: turning it right looks left. The stick's invert is its own." },
	{ "input.gyro_invert_vertical", _config_boolean, "false", "HALO_GYRO_INVERT_VERTICAL",
		_environment_value, _platform_all, /* AE hook */
		"Invert the gyroscope's pitch. false: tilting the controller up looks up.\n"
		"true: tilting it up looks down. The stick's invert is its own." },
	{ "input.gyro_sensitivity_horizontal", _config_real, "1.0", "HALO_GYRO_SENSITIVITY_HORIZONTAL",
		_environment_value, _platform_all, /* AE hook */
		"How far the view turns left and right for the gyroscope, when\n"
		"input.gyro_aim is on. 1 is the sensor's own turn. 0.25 to 10." },
	{ "input.gyro_sensitivity_vertical", _config_real, "1.0", "HALO_GYRO_SENSITIVITY_VERTICAL",
		_environment_value, _platform_all, /* AE hook */
		"How far the view turns up and down for the gyroscope, when\n"
		"input.gyro_aim is on. 1 is the sensor's own turn. 0.25 to 10." },

	/* the keyboard and mouse's own controls (port/linux/src/xinput_sdl.c) */
	{ "chat.enabled", _config_boolean, "false", "HALO_CHAT", _environment_value, _platform_all, /* AE hook */
		"ChupathingyCE's text chat, with its players over Delta (on a host that\n"
		"speaks it; controls.chat_all says a line to everyone, controls.chat_team\n"
		"to your team). Arena Evolved's text chat is OpenCE's (controls.chat,\n"
		"controls.team_chat, display.text_chat), which every machine has; false\n"
		"(the default) shows and says no ChupathingyCE line; a server's own lines\n"
		"still show." },
	{ "chat.show", _config_string, "\"all\"", "HALO_CHAT_SHOW", _environment_value, _platform_all,
		"Whose chat lines show: \"all\", \"team\" or \"off\"." },
	{ "chat.filter", _config_string, "\"mask\"", "HALO_CHAT_FILTER", _environment_value, _platform_all,
		"Blocked words in chat: \"mask\" stars them out, \"hide\" hides the line,\n"
		"\"show\" shows it." },
	{ "chat.filter_file", _config_string, "\"chat_filter.txt\"", "HALO_CHAT_FILTER_FILE", _environment_value,
		_platform_all,
		"Your own blocked words, one per line, in the save folder. \"whole <word>\"\n"
		"blocks only the whole word. Lines starting with # are comments." },
	{ "chat.streamer_mode", _config_boolean, "false", "HALO_CHAT_STREAMER_MODE", _environment_value, _platform_all,
		"Show other players in chat by team name, and always star out blocked\n"
		"words." },
	{ "chat.quick_messages", _config_string,
		"\"Nice shot|Need backup|On my way|Thanks|Good game|Sorry\"", "HALO_CHAT_QUICK_MESSAGES",
		_environment_value, _platform_all,
		"Controller quick messages, separated by |. D-pad up picks one, D-pad\n"
		"down sends it. Empty: none." },
	{ "controls.move_forward", _config_string, "\"W\"", "HALO_KEY_MOVE_FORWARD", _environment_value, _platform_all,
		"Keyboard and mouse controls (Settings > Controls Setup): up to two\n"
		"each, separated by a comma. Keys by their US keyboard names (\"W\",\n"
		"\"Space\", \"Left Ctrl\", \"F1\"). Mouse: \"Mouse Left\", \"Mouse Right\",\n"
		"\"Mouse Middle\", \"Mouse 4\", \"Mouse 5\", \"Wheel\", \"Wheel Up\", \"Wheel Down\".\n"
		"Empty: none. Move forward:" },
	{ "controls.move_backward", _config_string, "\"S\"", "HALO_KEY_MOVE_BACKWARD", _environment_value, _platform_all,
		"Move backward." },
	{ "controls.strafe_left", _config_string, "\"A\"", "HALO_KEY_STRAFE_LEFT", _environment_value, _platform_all,
		"Move left." },
	{ "controls.strafe_right", _config_string, "\"D\"", "HALO_KEY_STRAFE_RIGHT", _environment_value, _platform_all,
		"Move right." },
	{ "controls.jump", _config_string, "\"Space\"", "HALO_KEY_JUMP", _environment_value, _platform_all,
		"Jumping (and skipping cutscenes)." },
	{ "controls.crouch", _config_string, CROUCH_KEYS, "HALO_KEY_CROUCH", _environment_value, _platform_all,
		"Crouching." },
	{ "controls.fire", _config_string, "\"Mouse Left\"", "HALO_KEY_FIRE", _environment_value, _platform_all,
		"Fire." },
	{ "controls.throw_grenade", _config_string, "\"Mouse Right, G\"", "HALO_KEY_THROW_GRENADE", _environment_value,
		_platform_all,
		"Throw grenade." },
	{ "controls.melee", _config_string, "\"F, Mouse 4\"", "HALO_KEY_MELEE", _environment_value, _platform_all,
		"Melee attack." },
	{ "controls.reload", _config_string, "\"R\"", "HALO_KEY_RELOAD", _environment_value, _platform_all,
		"Reload." },
	{ "controls.zoom", _config_string, "\"Z, Mouse Middle\"", "HALO_KEY_ZOOM", _environment_value, _platform_all,
		"Zoom." },
	{ "controls.switch_weapon", _config_string, "\"Wheel, 1\"", "HALO_KEY_SWITCH_WEAPON", _environment_value,
		_platform_all,
		"Switch weapon." },
	{ "controls.switch_grenade", _config_string, "\"X\"", "HALO_KEY_SWITCH_GRENADE", _environment_value, _platform_all,
		"Switch grenade." },
	{ "controls.action", _config_string, "\"E\"", "HALO_KEY_ACTION", _environment_value, _platform_all,
		"Action: pick up, swap weapons, enter and exit vehicles, press switches." },
	{ "controls.flashlight", _config_string, "\"Q\"", "HALO_KEY_FLASHLIGHT", _environment_value, _platform_all,
		"Flashlight." },
	{ "controls.scoreboard", _config_string, "\"Tab\"", "HALO_KEY_SCOREBOARD", _environment_value, _platform_all,
		"Show scores." },
	{ "controls.pause", _config_string, "\"Escape\"", "HALO_KEY_PAUSE", _environment_value, _platform_all,
		"The pause menu (the controller's Start)." },
	{ "controls.screenshot", _config_string, "\"F10\"", "HALO_KEY_SCREENSHOT", _environment_value, _platform_all,
		"Save a PNG screenshot in screenshots/ beside config.toml (press once per\n"
		"capture). F9, the recording's key, cannot be used here." },
	{ "controls.push_to_talk", _config_string, "\"V\"", "HALO_KEY_PUSH_TO_TALK", _environment_value, _platform_all,
		"Voice chat: talk while it is held (audio.voice_chat \"push_to_talk\")." },
	{ "controls.chat", _config_string, "\"T\"", "HALO_KEY_CHAT", _environment_value, _platform_all,
		"Network games' text chat: type a line to everyone (Enter sends it,\n"
		"Escape lets it go)." },
	{ "controls.team_chat", _config_string, "\"Y\"", "HALO_KEY_TEAM_CHAT", _environment_value, _platform_all,
		"Network games' text chat: type a line to your team." },
	/* (Arena Evolved: ChupathingyCE's Delta text chat (chat.c), off by default
	(chat.enabled): AE's is OpenCE's, controls.chat and controls.team_chat above;
	no keys of its own, so that T and Y open one chat) */
	{ "controls.chat_all", _config_string, "\"\"", "HALO_KEY_CHAT_ALL", _environment_value, _platform_all, /* AE hook */
		"ChupathingyCE's text chat (chat.enabled): saying a line to everyone\n"
		"(Enter sends, Escape cancels)." },
	{ "controls.chat_team", _config_string, "\"\"", "HALO_KEY_CHAT_TEAM", _environment_value, _platform_all, /* AE hook */
		"ChupathingyCE's text chat: saying a line to your team." },
	{ "controls.chat_log", _config_string, "\"\"", "HALO_KEY_CHAT_LOG", _environment_value, _platform_all, /* AE hook */
		"ChupathingyCE's text chat: its recent lines and its players, where M\n"
		"mutes one for you." },
	{ "controls.dev_camera", _config_string, "\"F2\"", "HALO_KEY_DEV_CAMERA", _environment_value, _platform_all, /* AE hook */
		"With system.dev_mode: the third-person camera over the shoulder, on or off\n"
		"(a controller: hold BACK, press the d-pad's down)." },
	{ "controls.dev_shoulder", _config_string, "\"F3\"", "HALO_KEY_DEV_SHOULDER", _environment_value, _platform_all, /* AE hook */
		"With system.dev_mode: the third-person camera's other shoulder (BACK and\n"
		"the d-pad's right)." },
	{ "controls.dev_free_camera", _config_string, "\"F6\"", "HALO_KEY_DEV_FREE_CAMERA", _environment_value, /* AE hook */
		_platform_all,
		"With system.dev_mode: the free camera, on or off (BACK and the d-pad's\n"
		"left). It moves and looks as you do; jump and crouch raise and lower it." },
	{ "controls.test_pass", _config_string, "\"F7\"", "HALO_KEY_TEST_PASS", _environment_value, _platform_all, /* AE hook */
		"TEST RUN: the verdict PASS for the test on the card (a controller: hold BACK, press A)." },
	{ "controls.test_fail", _config_string, "\"F8\"", "HALO_KEY_TEST_FAIL", _environment_value, _platform_all, /* AE hook */
		"TEST RUN: the verdict FAIL (BACK and B)." },
	{ "controls.test_note", _config_string, "\"F4\"", "HALO_KEY_TEST_NOTE", _environment_value, _platform_all, /* AE hook */
		"TEST RUN: a note on the test (BACK and X)." },
	{ "controls.test_skip", _config_string, "\"F5\"", "HALO_KEY_TEST_SKIP", _environment_value, _platform_all, /* AE hook */
		"TEST RUN: skip the test (BACK and Y)." },
	{ "controls.test_reload", _config_string, "\"F1\"", "HALO_KEY_TEST_RELOAD", _environment_value, _platform_all, /* AE hook */
		"TEST RUN: reload the test's setup (BACK and the d-pad's up)." },
	{ "controls.test_back", _config_string, "\"Home\"", "HALO_KEY_TEST_BACK", _environment_value, _platform_all, /* AE hook */
		"TEST RUN: go back one test (BACK and LB)." },
	{ "controls.test_hide_card", _config_string, "\"End\"", "HALO_KEY_TEST_HIDE_CARD", _environment_value, _platform_all, /* AE hook */
		"TEST RUN: hide the card while it is held (BACK and RB)." },
	{ "controls.controller_layout", _config_string, "\"\"", "HALO_CONTROLLER_LAYOUT", _environment_value,
		_platform_all,
		"The Custom button layout, set on the Controls screen: \"action=button\"\n"
		"pairs separated by commas (\"jump=B,melee=RB\"). Empty: the default." },

	{ "game.console_log", _config_string, "\"none\"", "HALO_CONSOLE_LOG", _environment_value, _platform_all,
		"What the console shows on screen: \"important\", \"all\" or \"none\".\n"
		"debug.txt has every line." },

	{ "game.language", _config_string, "\"\"", "HALO_LANGUAGE", _environment_value, _platform_all,
		"The language the game asks the Xbox for: \"ja\", \"de\", \"fr\", \"es\" or \"it\";\n"
		"empty for English. The game data decides what is translated." },
	{ "display.text_chat", _config_boolean, "true", "HALO_TEXT_CHAT", _environment_value, _platform_all,
		"Show the text chat of network games: false hides its lines and keeps\n"
		"its keys (and a controller's D-pad Up) from opening it, as for purists\n"
		"and streamers." },
	{ "display.chat_style", _config_string, "\"colors\"", "HALO_CHAT_STYLE", _environment_value, _platform_all,
		"The text chat's colours: \"colors\" (names in their team's colour, the\n"
		"text a pale blue) or \"white\" (all white, as Custom Edition)." },
	{ "game.enhanced_animations", _config_boolean, "true", "HALO_ENHANCED_ANIMATIONS", _environment_value, _platform_all,
		"The player bipeds' grenade throws keep their legs moving (crouched,\n"
		"in the air and in a vehicle's seat too), riders' hands leave the grips\n"
		"to throw and reload, and a player turns with the aim while throwing;\n"
		"false: the original animations, which freeze the legs and stand a\n"
		"rider up." },
	{ "game.fall_damage", _config_boolean, "true", "HALO_FALL_DAMAGE", _environment_value, _platform_all,
		"In the campaign, players hurt by falls; false: landings never hurt, from\n"
		"any height (a pit and the map's kill volumes still kill). Multiplayer\n"
		"goes by the gametype's FALL DAMAGE (Arena Options) instead." },
	{ "game.weapon_packs", _config_boolean, "true", "HALO_WEAPON_PACKS", _environment_value, _platform_all, /* AE hook */
		"Weapon packs (the packs folder beside maps: the battle rifle) read when the\n"
		"game starts. false: none is read: a gametype that gives a pack's weapon\n"
		"plays its stock weapon here, and this machine cannot join a game that\n"
		"uses one. A change applies from the next start." },
	{ "game.health", _config_string, "\"classic\"", "HALO_HEALTH", _environment_value, _platform_all,
		"How players' health comes back in the campaign: \"classic\" only from\n"
		"health packs; \"reach\" the REACH timers (shields 6 s then 3 s; health\n"
		"4 s after the last body damage, then to the top of the third it is in; a\n"
		"health pack also ends the shields' wait); \"halo2\" and \"halo3\" the HALO 2\n"
		"timers, the same (shields 5 s then 2 s; health 10 s after the\n"
		"last body damage, then 5 s). Multiplayer goes by the gametype's HEALTH." },
	{ "game.disc", _config_string, "\"\"", "HALO_DISC", _environment_value, /* AE hook */
		_platform_desktop,
		"The modded disc played (a whole set of maps, as Halo 1: NHE's or CE+ X's):\n"
		"a folder of discs/ (next to maps/), whose maps/ holds the maps it replaces\n"
		"(the others are maps/'s); empty for none. A disc only in mods/, where\n"
		"earlier releases kept them, is played from there. Settings > Game Options\n"
		"> MODDED DISCS chooses it, and the game starts again with it." },
	{ "game.mod", _config_string, "\"\"", "HALO_MOD", _environment_value, _platform_none, /* AE hook */
		"Earlier releases' name of game.disc, read into it once (config_carry_over)." },
	{ "game.mcc_path", _config_string, "\"\"", NULL, _environment_value, /* AE hook */
		_platform_desktop,
		"The folder of Halo: The Master Chief Collection (the one holding halo1/)\n"
		"whose Custom Edition files (halo1/maps/custom_edition: bitmaps.map,\n"
		"sounds.map, loc.map) Custom Edition maps read when maps_ce has none; empty:\n"
		"found in Steam's libraries. Settings > MAP FILES' BROWSE\n"
		"sets it. HALO_MCC_PATH names one for a run (after this one). The files\n"
		"are only read, never changed or copied." },
	{ "game.mcc_use", _config_string, "\"yes\"", "HALO_MCC_USE", _environment_value, /* AE hook */
		_platform_desktop,
		"Whether Custom Edition maps read MCC's Custom Edition files (game.mcc_path)\n"
		"when maps_ce, maps/ce and custom_maps lack them: \"yes\"; \"no\"; \"ask\" (the\n"
		"menus ask when such a map is picked; elsewhere it counts as no). With \"no\"\n"
		"Steam's folders are looked in only by Settings > MAP FILES." },
	{ "game.callouts", _config_string, "\"off\"", "HALO_CALLOUTS", _environment_value, _platform_all, /* AE hook */
		"Spoken callouts in multiplayer, in game.callout_voice's voice, on this\n"
		"machine only: \"items\" (where the gametype's TIMERS show the items, or\n"
		"with TRAINING) each power item's call 10\n"
		"seconds before it spawns (\"rockets in ten\", else the name), five to\n"
		"one before the rockets, and \"<item> is up\" as it spawns (when the\n"
		"voice has it); \"items_clock\" also Halo 1: NHE's talking timer (the\n"
		"minutes, thirty and twenty seconds left, ten to one, its beeps); \"off\".\n"
		"Silent on Halo 1: NHE's maps, whose own scripts talk." },
	{ "game.callout_detail", _config_string, "\"standard\"", "HALO_CALLOUT_DETAIL", _environment_value, _platform_all,
		"How much the callouts say of the power items spawning together (a\n"
		"wave): \"minimal\" one line a wave (\"power items in ten\", an item alone\n"
		"its own) and the first item's \"is up\", no beeps, no five to one, and\n"
		"of the clock only the minutes and thirty seconds left; \"standard\" the\n"
		"item beep, an overshield and camo as one line, three items or more as\n"
		"the line of their kinds, else each item's, and each \"is up\";\n"
		"\"verbose\" every item named (\"rockets, sniper and overshield in ten\")\n"
		"and each item's own \"is up\" (a wave that can't be said so in 8 seconds\n"
		"as \"standard\"). Waves start early enough to end by their 10 seconds\n"
		"mark." },
	{ "game.callout_voice", _config_string, "\"cori\"", "HALO_CALLOUT_VOICE", _environment_value, _platform_all,
		"The callouts' voice: a folder of voices/ (next to maps/) holding WAVs\n"
		"named for what they say (one.wav, rockets.wav, ...); the modded disc\n"
		"played's own discs/<disc>/voices/<folder>/ clips come first. With no such folder,\n"
		"the first folder of voices/ speaks (this setting stays)." },

	{ "game.unlock_all_levels", _config_boolean, "false", "HALO_UNLOCK_ALL_LEVELS", _environment_value, _platform_all,
		"New profiles start with every campaign level unlocked." },

	{ "game.downloaded_maps", _config_string, "\"\"", "HALO_DOWNLOADED_MAPS", _environment_value, _platform_all,
		"Maps treated as downloaded, separated by commas (bloodgulch,\n"
		"hugeass@ce), or \"*\" for all. Their scripts can't change your settings\n"
		"or other players' games." },

	{ "game.move_old_map_folders", _config_string, "\"no\"", "HALO_MOVE_OLD_MAP_FOLDERS", _environment_value,
		_platform_all,
		"Halo PC maps have folders of their own beside maps: maps_ce (Custom\n"
		"Edition, with its bitmaps.map, sounds.map and loc.map), maps_md (HaloMD)\n"
		"and maps_pc (Halo PC). The older maps/ce and md_maps are still played\n"
		"from. \"no\" (Arena Evolved's default): left where they are, so builds that\n"
		"know only maps/ce keep working; \"ask\": the game offers once to move them into the new folders\n"
		"(each folder moved whole, never copied; one that cannot be moved stays\n"
		"where it is); \"yes\": moved without asking; \"no\": left where they are.\n"
		"Android moves them unless this is \"no\"." },
	{ "bots.count", _config_integer, "0", "HALO_BOTS", _environment_value, _platform_all, /* AE hook */
		"Computer players (bots) in a local (split screen) game of any mode\n"
		"(Slayer, CTF, Oddball, King of the Hill, Race) and in the LAN and\n"
		"internet games you host (debug.network_bots): they join its lobby with\n"
		"the players, as far as the game has room. The menus' BOTS row goes to\n"
		"31, this to 127. 0: none, as in the stock game." },
	{ "bots.skill", _config_string, "\"normal\"", "HALO_BOT_SKILL", _environment_value, _platform_all, /* AE hook */
		"How well the bots play: \"recruit\" (for new players), \"easy\", \"normal\",\n"
		"\"heroic\", \"legendary\" or \"demon\" (how soon they react, how well they\n"
		"aim, how far they see)." },
	{ "bots.shooting_skill", _config_string, "\"same\"", "HALO_BOT_SHOOTING_SKILL", _environment_value, _platform_all, /* AE hook */
		"SHOOTING SKILL: the bots' aim, reactions, sight, grenades, blows and\n"
		"headshots at a level of their own: \"recruit\" .. \"demon\", or\n"
		"\"same\" (the default) for BOT SKILL's." },
	{ "bots.movement_skill", _config_string, "\"same\"", "HALO_BOT_MOVEMENT_SKILL", _environment_value, _platform_all, /* AE hook */
		"MOVEMENT SKILL: the bots' strafing, jumping, crouching, closing in and\n"
		"retreating at a level of their own: \"recruit\" .. \"demon\", or\n"
		"\"same\" (the default) for BOT SKILL's; \"standing\" (for tests): they stay\n"
		"where they spawn and only turn, aim and shoot." },
	{ "bots.teams", _config_string, "\"even\"", "HALO_BOT_TEAMS", _environment_value, _platform_all,
		"In team games: \"even\" puts the bots on both teams, evening them with\n"
		"the players; \"against\" puts them all on the team the players are not on." },
	{ "bots.playfight", _config_boolean, "false", "HALO_BOT_PLAYFIGHT", _environment_value, _platform_all, /* AE hook */
		"PLAYFIGHT: the bots move, strafe,\n"
		"jump and aim at you, but never shoot, throw a grenade or strike (aim\n"
		"practice for new players). Takes effect at once, in game too." },
	{ "display.viewmodel_shield", _config_boolean, "false", "HALO_VIEWMODEL_SHIELD", _environment_value, _platform_all,
		"The energy shield's flare on the first-person arms too, as on the body\n"
		"(the stock game draws it on the body only)." },
	{ "display.first_person_legs", _config_boolean, "false", "HALO_FIRST_PERSON_LEGS", _environment_value, _platform_all,
		"Your own legs in first person, seen looking down: your body as others\n"
		"see it, drawn from the waist down, moving as it moves." },
	{ "display.backpack_weapons", _config_boolean, "true", "HALO_BACKPACK_WEAPONS", _environment_value, _platform_all, /* AE hook */
		"BACKPACK WEAPONS: the weapons a player carries but does not hold, drawn\n"
		"on their body: a long one across the back, a\n"
		"small one on the thigh. Only what this machine draws changes." },
	{ "display.overshield_effect", _config_boolean, "false", "HALO_OVERSHIELD_EFFECT", _environment_value, _platform_all,
		"The overshield as this machine draws it: its flare rising up the body as\n"
		"it charges (2 seconds, 3 from a shield that was down) and staying lit,\n"
		"its energy slower, in its own color (the powerup's), on the first-person\n"
		"arms too with viewmodel_shield; a hit on it plays what the projectile\n"
		"plays on an Elite's shield. Only what is drawn and heard changes.\n"
		"In a network game, the host's network.shield_effects decides, unless\n"
		"it is \"players\" (each player's own)." },
	{ "display.shield_glow", _config_boolean, "false", "HALO_SHIELD_GLOW", _environment_value, _platform_all,
		"Energy shields light what is around them as they flare, as plasma\n"
		"does, in the shield's own color. In a network game, the host's\n"
		"network.shield_effects decides, unless it is \"players\" (each\n"
		"player's own)." },
	{ "display.shield_glow_intensity", _config_string, "\"default\"", "HALO_SHIELD_GLOW_INTENSITY", _environment_value, _platform_all,
		"How far the shield glow reaches: \"default\" or \"light_show\" (three\n"
		"times as far, and at full strength from a third of the flare).\n"
		"In a network game, the host's network.shield_effects decides, unless\n"
		"it is \"players\" (each player's own)." },
	{ "display.pistol_color", _config_string, "\"default\"", "HALO_PISTOL_COLOR", _environment_value, _platform_all, /* AE hook */
		"PISTOL COLOR (cosmetic, your own view only): your pistol's silver body in\n"
		"first person in a multiplayer armor color: \"default\" (the pistol's own)\n"
		"or white, black, red, blue, gray, yellow, green, pink, purple, cyan,\n"
		"cobalt, orange, teal, sage, brown, tan, maroon or salmon." },
	{ "display.pistol_trim", _config_string, "\"default\"", "HALO_PISTOL_TRIM", _environment_value, _platform_all, /* AE hook */
		"PISTOL TRIM (cosmetic, your own view only): your pistol's dark parts in\n"
		"first person, in a color as display.pistol_color." },
	{ "display.shield_color", _config_string, "\"default\"", "HALO_SHIELD_COLOR", _environment_value, _platform_all,
		"Your energy shield's color, as this machine draws it: \"default\" (the\n"
		"shield's own) or a multiplayer armor color: white, black, red, blue,\n"
		"gray, yellow, green, pink, purple, cyan, cobalt, orange, teal, sage,\n"
		"brown, tan, maroon or salmon." },
	{ "display.big_sprite_limit", _config_integer, "8", "HALO_BIG_SPRITE_LIMIT", _environment_value, _platform_android,
		"The most sprites covering more than half the view drawn in a frame (the\n"
		"Xbox's 11): each is a layer over most of the screen, and the Halo level's\n"
		"falls have 11 sheets of mist. Changes at once." },
#ifdef HALO_SWITCH_CONSOLE
	{ "display.particle_limit", _config_integer, "2048", "HALO_PARTICLE_LIMIT", _environment_value, _platform_all,
#else
	{ "display.particle_limit", _config_integer, "4096", "HALO_PARTICLE_LIMIT", _environment_value, _platform_all,
#endif
		"The most particles (sparks, smoke, casings, debris) alive at once; past\n"
		"it the oldest goes to make room for each new one. The Xbox's limit was\n"
		"1024. 0 for the port's own 8192 (a 64-player firefight fills them, and\n"
		"every one costs the processor and the GPU: 2048 on the Switch, 4096\n"
		"elsewhere). Changes at once." },
#ifdef HALO_CONSOLE_PARTICLE_TICKS
	{ "display.particle_ticks", _config_boolean, "true", "HALO_PARTICLE_TICKS", _environment_value, _platform_all,
#else
	{ "display.particle_ticks", _config_boolean, "false", "HALO_PARTICLE_TICKS", _environment_value, _platform_all,
#endif
		"Move the particles that are not attached to an object (sparks, casings,\n"
		"debris) once a 30 Hz tick, as on the Xbox, and draw them between their\n"
		"ticks: two in three of their collision tests spared at 60 frames a\n"
		"second, more at 120. false moves them at every frame drawn, smooth at\n"
		"high refresh rates: the desktop's default; the consoles' is true.\n"
		"Takes effect at the next map." },
#ifdef HALO_CONSOLE_LIGHT_LIMITS
	{ "display.dynamic_light_limit", _config_integer, "8", "HALO_DYNAMIC_LIGHT_LIMIT", _environment_value, _platform_all,
#else
	{ "display.dynamic_light_limit", _config_integer, "0", "HALO_DYNAMIC_LIGHT_LIMIT", _environment_value, _platform_all,
#endif
		"The most dynamic lights (muzzle flashes, plasma, explosions) that light\n"
		"the level at once: the biggest on the screen keep their light on the\n"
		"walls and floor, the rest light only the objects near them. 0 for no\n"
		"limit, as on the Xbox (each redraws the level it reaches twice: a\n"
		"firefight of many is the most of a busy frame's GPU time): the\n"
		"desktop's default; the consoles' is 8. Changes at once." },
#ifdef HALO_CONSOLE_LIGHT_LIMITS
	{ "display.specular_light_limit", _config_integer, "2", "HALO_SPECULAR_LIGHT_LIMIT", _environment_value, _platform_all,
#else
	{ "display.specular_light_limit", _config_integer, "0", "HALO_SPECULAR_LIGHT_LIMIT", _environment_value, _platform_all,
#endif
		"Of those, the most that add their specular highlights (the shine on\n"
		"metal and wet floors): the biggest on the screen, the first-person\n"
		"weapon's first. The others still light the level, without the shine.\n"
		"0 for no limit, as on the Xbox (a near light's highlights cover most\n"
		"of the screen: the costliest pass of a firefight): the desktop's\n"
		"default; the consoles' is 2. Changes at once." },
	{ "display.delta_profile", _config_boolean, "true", "HALO_DELTA_PROFILE", _environment_value, _platform_all,
		"Show Delta Profile in the menus and Online Games." },
	{ "display.full_model_detail", _config_boolean, "false", "HALO_FULL_MODEL_DETAIL", _environment_value,
		_platform_all,
		"Draw every model at full detail at any distance." },
	{ "display.loading_screen", _config_boolean, "true", "HALO_LOADING_SCREEN", _environment_value,
		_platform_all,
		"Keep the original loading screen up for a while when a level loads,\n"
		"as loading from the Xbox's disc did, for the times below; false loads\n"
		"every level as fast as it can." },
	{ "display.loading_screen_campaign_seconds", _config_real, "0.0", "HALO_LOADING_SCREEN_CAMPAIGN_SECONDS",
		_environment_value, _platform_all,
		"The least seconds the loading screen stays up when a campaign level\n"
		"loads on this machine alone, split screen co-op too (up to 30); 0 for\n"
		"no wait. A longer load shows for as long as it takes." },
	{ "display.loading_screen_multiplayer_seconds", _config_real, "0.0", "HALO_LOADING_SCREEN_MULTIPLAYER_SECONDS",
		_environment_value, _platform_all,
		"The same for every other game: split screen multiplayer, system link,\n"
		"online and network co-op, up to 5: the network waits while the\n"
		"loading screen is held." },
	{ "display.profile_names", _config_boolean, "true", "HALO_PROFILE_NAMES", _environment_value, _platform_all,
		"Show players by their shared Delta profile name in Delta games." },

	{ "game.defaults_answered", _config_integer, "0", "HALO_DEFAULTS_ANSWERED", _environment_value, _platform_all,
		"Set by the game after it asks about changed defaults. Do not edit." },
	{ "game.bomb_arm_time", _config_real, "1.0", "HALO_BOMB_ARM_TIME", _environment_value, _platform_all,
		"In a Bomb or Grifball game you host, the seconds the bomb arms once it\n"
		"is planted in the enemy goal (up to 10): a defender who reaches it in\n"
		"that time saves it (it resets, no score); else it goes off and scores.\n"
		"0: it scores the moment it is planted." },
	{ "game.bomb_open_pitch_carrier_health", _config_integer, "0", "HALO_BOMB_OPEN_PITCH_CARRIER_HEALTH",
		_environment_value, _platform_all,
		"The bomb carrier's health in a Grifball: Open Pitch game you host, in\n"
		"percent of everyone's (10 to 400). 0: the game type's own, 400 (with\n"
		"the resistance's 400, it lives through one melee blow, not two)." },
	{ "game.bomb_open_pitch_carrier_resistance", _config_integer, "0",
		"HALO_BOMB_OPEN_PITCH_CARRIER_RESISTANCE", _environment_value, _platform_all,
		"The bomb carrier's damage resistance in a Grifball: Open Pitch game you\n"
		"host, in percent (25 to 400; 150 takes two thirds of the damage). 0: the\n"
		"game type's own, 400." },
	{ "game.bomb_open_pitch_carrier_shields", _config_string, "\"none\"", "HALO_BOMB_OPEN_PITCH_CARRIER_SHIELDS",
		_environment_value, _platform_all,
		"The bomb carrier's shields in a Grifball: Open Pitch game you host:\n"
		"\"none\" (everyone's: none), \"normal\" or \"overshield\"." },
	{ "game.bomb_open_pitch_fumble", _config_boolean, "true", "HALO_BOMB_OPEN_PITCH_FUMBLE", _environment_value,
		_platform_all,
		"true: in a Grifball: Open Pitch game you host, a melee blow that doesn't\n"
		"kill the bomb carrier knocks the bomb out of their hands." },
	{ "game.bomb_open_pitch_speed", _config_integer, "0", "HALO_BOMB_OPEN_PITCH_SPEED", _environment_value,
		_platform_all,
		"Everyone's speed in a Grifball: Open Pitch game you host, in percent\n"
		"(50 to 200; the carrier's 115% on top stops at 200). 0: the game\n"
		"type's own, 175." },
	{ "game.bomb_spawns", _config_string, "\"\"", "HALO_BOMB_SPAWNS", _environment_value, _platform_all,
		"Where a Bomb or Grifball game you host puts the bomb on a map, for maps\n"
		"whose spot halfway between the goals plays badly: \"<map> <x> <y> [<z>]\"\n"
		"(the map as a playlist names it: bloodgulch, timberland@ce), entries\n"
		"separated by commas. The bomb rests on the first ground under the point\n"
		"(from a little over its height, when given). A map not named has the\n"
		"game's own spot: Blood Gulch's halfway between its teleporters' exits,\n"
		"any other map's halfway between the goals." },
	{ "game.bomb_flag_spawns", _config_boolean, "true", "HALO_BOMB_FLAG_SPAWNS", _environment_value, _platform_all, /* AE hook */
		"In a Bomb game you host (not melee only): the bomb spawns at one team's\n"
		"flag and that team attacks (the other can't pick it up); the teams swap\n"
		"each round, red first. false: the bomb halfway between the goals,\n"
		"anyone's (game.bomb_spawns)." },
	{ "game.bomb_defuse_time", _config_real, "3.0", "HALO_BOMB_DEFUSE_TIME", _environment_value, _platform_all, /* AE hook */
		"In a Bomb game you host (not melee only), the seconds a defender stands\n"
		"on the planted bomb to save it; leaving it, or dying, starts it over.\n"
		"0: a touch saves it." },
	{ "game.bomb_fuse_time", _config_real, "5.0", "HALO_BOMB_FUSE_TIME", _environment_value, _platform_all, /* AE hook */
		"In a Bomb game you host (not melee only), the seconds the planted bomb\n"
		"takes to go off and score (up to 10); a melee-only game's is\n"
		"game.bomb_arm_time's." },
	{ "game.bomb_snd", _config_boolean, "false", "HALO_BOMB_SND", _environment_value, _platform_all, /* AE hook */
		"A test override: SEARCH AND DESTROY in any Bomb game you host (not\n"
		"melee only, with game.bomb_flag_spawns), as the AE SND gametype plays\n"
		"it without this row: one life a round; a round goes to the attackers\n"
		"when the bomb goes off or every defender is dead, to the defenders when\n"
		"it is defused, every attacker is dead before the plant, or the time\n"
		"runs out with none planted. A round won is a point." },
	{ "game.bomb_snd_drop", _config_boolean, "false", "HALO_BOMB_SND_DROP", _environment_value, _platform_all, /* AE hook */
		"A test override: BOMB DROP in any SEARCH AND DESTROY game you host, as\n"
		"the gametype's BOMB DROP row turns it on: the carrier drops the bomb at\n"
		"their feet by holding the action button with nothing in reach." },
	{ "game.bomb_snd_round_time", _config_real, "150.0", "HALO_BOMB_SND_ROUND_TIME", _environment_value, _platform_all, /* AE hook */
		"SEARCH AND DESTROY: the seconds a round lasts before the bomb is\n"
		"planted (the defenders win it when they run out)." },
	{ "game.bomb_snd_fuse_time", _config_real, "45.0", "HALO_BOMB_SND_FUSE_TIME", _environment_value, _platform_all, /* AE hook */
		"SEARCH AND DESTROY: the seconds the planted bomb takes to go off (up to\n"
		"120)." },
	{ "game.bomb_snd_defuse_time", _config_real, "5.0", "HALO_BOMB_SND_DEFUSE_TIME", _environment_value, _platform_all, /* AE hook */
		"SEARCH AND DESTROY: the seconds a defender stands on the planted bomb\n"
		"to defuse it." },
	{ "game.bomb_snd_countdown", _config_real, "6.0", "HALO_BOMB_SND_COUNTDOWN", _environment_value, _platform_all, /* AE hook */
		"SEARCH AND DESTROY with the gametype's PRE-GAME COUNTDOWN on: the\n"
		"seconds of every round's countdown (the first round's too), the\n"
		"players frozen (strategy time). Other Bomb games count down 3." },
	{ "game.bomb_snd_round_switch", _config_integer, "3", "HALO_BOMB_SND_ROUND_SWITCH", _environment_value, _platform_all, /* AE hook */
		"SEARCH AND DESTROY: the teams swap sides (attack, defend) every this\n"
		"many rounds, red attacking first. AE BOMB without\n"
		"it swaps every round." },
	{ "game.bomb_snd_plant_time", _config_real, "5.0", "HALO_BOMB_SND_PLANT_TIME", _environment_value, _platform_all, /* AE hook */
		"SEARCH AND DESTROY: the seconds the carrier stands in the enemy goal to\n"
		"plant the bomb (the gametype's arm time otherwise). 0: the gametype's." },
	{ "game.bomb_snd_rounds_to_win", _config_integer, "4", "HALO_BOMB_SND_ROUNDS_TO_WIN", _environment_value, _platform_all, /* AE hook */
		"SEARCH AND DESTROY: the rounds a team wins the game with. 0: the\n"
		"gametype's score to win." },
	{ "game.pack_menus", _config_string, "\"\"", "HALO_PACK_MENUS", _environment_value, _platform_all | _platform_xbox,
		"Play with a map pack's own menus, by the pack's folder name: \"cex\" uses\n"
		"maps_cex/ui.map (Halo CE+'s) in place of the game's; \"\" (the default)\n"
		"keeps the game's. Takes effect after a restart." },
	{ "game.start_with_one_player", _config_boolean, "false", "HALO_START_WITH_ONE_PLAYER", _environment_value,
		_platform_all | _platform_xbox,
		"Let a game you host start with only you. Others can join in progress." },

	{ "paths.data", _config_string, "\"\"", "HALO_DATA_ROOT", _environment_value, _platform_desktop,
		"The folder holding the maps folder; empty for the working folder and\n"
		"its assets folder. Use single quotes for Windows paths: 'C:\\Games\\Halo'." },
	{ "paths.saves", _config_string, "\"\"", "HALO_SAVE_ROOT", _environment_value, _platform_desktop,
		"Where saved games and profiles go; empty for the usual place\n"
		"(~/.local/share/halo-linux, or %APPDATA%\\halo on Windows)." },
	{ "paths.custom_edition", _config_string, "\"\"", "HALO_CUSTOM_EDITION_ROOT", _environment_value, _platform_desktop,
		"A Halo Custom Edition install whose maps folder is looked in after the\n"
		"custom_maps folder for Custom Edition maps and their bitmaps.map,\n"
		"sounds.map and loc.map (game.custom_edition); empty for none." },
	{ "paths.logs", _config_string, "\"\"", "HALO_LOG_ROOT", _environment_value, _platform_desktop,
		"Where the logs go (debug.txt, halo.log and crash reports waiting to be\n"
		"sent); empty for the data folder itself, beside maps (Arena Evolved)." },
	{ "paths.cache", _config_string, "\"\"", "HALO_CACHE_ROOT", _environment_value, _platform_desktop,
		"Where the game keeps its cache; empty for cache/ in the save folder." },
	{ "paths.custom_maps", _config_string, "\"\"", "HALO_CUSTOM_MAPS_ROOT", _environment_value, _platform_desktop,
		"The custom_maps folder (Custom Edition maps, and their bitmaps.map,\n"
		"sounds.map and loc.map); empty for the one in the data folder." },

	{ "network.address", _config_string, "\"\"", "HALO_NET_ADDRESS", _environment_value, _platform_all,
		"This machine's IPv4 address for system link; empty chooses one." },
	{ "network.broadcast", _config_string, "\"\"", "HALO_NET_BROADCAST", _environment_value, _platform_all,
		"IPv4 addresses for system link announcements, separated by commas\n"
		"(for VPNs); empty for the local network." },
	{ "network.online", _config_boolean, "true", "HALO_NET_ONLINE", _environment_value, _platform_all,
		"Host and join games over the internet by invite links. false: local\n"
		"network only." },
	{ "network.join_from_clipboard", _config_string, "\"never\"", "HALO_NET_JOIN_FROM_CLIPBOARD", _environment_value,
		_platform_all,
		"An invite link on the clipboard when the game comes to the front:\n"
		"\"ask\", \"always\" (join it) or \"never\"." },
	{ "network.map_downloads", _config_string, "\"ask\"", "HALO_NET_MAP_DOWNLOADS", _environment_value,
		_platform_all,
		"Download a missing Custom Edition map from the community repositories:\n"
		"\"ask\", \"always\" or \"never\"." },
	{ "network.tunnel_port", _config_integer, "0", "HALO_NET_TUNNEL_PORT", _environment_value, _platform_all,
		"The UDP port for internet play; 0 picks one." },
	{ "network.client_timeout_seconds", _config_integer, "15", "HALO_NET_CLIENT_TIMEOUT_SECONDS", _environment_value,
		_platform_all,
		"Seconds a host waits for a machine it hears nothing from before it\n"
		"drops it (5 to 300; the stock game's 15). Longer keeps players whose\n"
		"connection stalls (a browser tab in the background) at the cost of\n"
		"frozen players for longer when one really goes." },
	{ "network.allow_upnp", _config_boolean, "true", "HALO_NET_ALLOW_UPNP", _environment_value, _platform_all,
		"Ask the router to forward internet play's port (UPnP)." },
	{ "network.protocol", _config_string, "\"auto\"", "HALO_NET_PROTOCOL", _environment_value, _platform_all,
		"Talk to other Chupathingy machines: \"auto\" (Delta where it can,\n"
		"OpenCE otherwise), \"opence\" (OpenCE only) or \"delta\" (as \"auto\" for\n"
		"now)." },
	{ "network.chat", _config_string, "\"on\"", "HALO_NET_CHAT", _environment_value, _platform_all,
		"Text chat in games you host: \"on\", \"team\" or \"off\"." },
	{ "network.team_change", _config_string, "\"balanced\"", "HALO_NET_TEAM_CHANGE", _environment_value,
		_platform_all,
		"Team changes in team games you host: \"balanced\", \"allow\" or \"off\"." },
	{ "network.team_change_margin", _config_integer, "1", "HALO_NET_TEAM_CHANGE_MARGIN", _environment_value,
		_platform_all,
		"For \"balanced\": how many more players one team may have, 0 to 16." },
	{ "network.chat_filter", _config_boolean, "true", "HALO_NET_CHAT_FILTER", _environment_value, _platform_all,
		"In games you host, block chat lines with blocked words." },
	{ "network.share_profile", _config_boolean, "false", "HALO_NET_SHARE_PROFILE", _environment_value,
		_platform_all,
		"Show your Chupathingy player ID to other Chupathingy players over\n"
		"Delta." },
	{ "network.platform_limits", _config_string, "\"on\"", "HALO_NET_PLATFORM_LIMITS", _environment_value,
		_platform_all,
		"Delta hosts keep games to this platform's player limit: \"on\" or \"off\"." },
	{ "network.host_platform_limits", _config_boolean, "true", "HALO_NET_HOST_PLATFORM_LIMITS", _environment_value,
		_platform_all,
		"Games you host keep to their Delta players' platform limits. false is\n"
		"for testing." },
	{ "network.public_lobby", _config_boolean, "true", "HALO_NET_PUBLIC_LOBBY", _environment_value, _platform_all,
		"The server browser: public games are listed through the signalling\n"
		"brokers, and Join Game > Server Browser shows them. False lists no\n"
		"game of this machine's and shows none." },
	{ "network.browser_pings", _config_boolean, "false", "HALO_NET_BROWSER_PINGS", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved (DamnationCE's): the Server Browser times each public\n"
		"game's ping and shows it, coloured. Timing it makes the same\n"
		"connection a join makes, so each listed host sees this machine's\n"
		"address. False (the default: an opt-in, Settings > Network SERVER\n"
		"PINGS) contacts no host until you join one." },
	{ "network.ping_games", _config_boolean, "false", "HALO_NET_PING_GAMES", _environment_value, _platform_all, /* AE hook */
		"Online Games pings the listed games whose hosts answer pings, before\n"
		"joining (the host sees this machine's public address, as when\n"
		"joining). False (Arena Evolved's default: an opt-in, as SERVER PINGS):\n"
		"no pings; the games' estimated pings (~) only." },
	{ "network.host_public", _config_boolean, "true", "HALO_NET_HOST_PUBLIC", _environment_value, _platform_all,
		"Whether a new game of Create Game > Internet starts as PUBLIC (listed\n"
		"in everyone's server browser: anyone can see and join it) or, false,\n"
		"PRIVATE (only players with its invite link can join). Server Setup's\n"
		"LISTING changes it for each game." },
	{ "network.voice_lobby", _config_boolean, "true", "HALO_NET_VOICE_LOBBY", _environment_value, _platform_all,
		"Hosting: voice chat in the lobby, before and after a game, where every\n"
		"player hears every other." },
	{ "network.voice_mode", _config_string, "\"team_global_enemy_proximity\"", "HALO_NET_VOICE_MODE",
		_environment_value, _platform_all,
		"Hosting: voice chat in a game: \"off\"; \"team_proximity\" (teammates\n"
		"near); \"team_enemy_proximity\" (anyone near); \"team_global\" (all\n"
		"teammates); or \"team_global_enemy_proximity\" (all teammates, and\n"
		"enemies near). A game without teams has only enemies; co-op only\n"
		"teammates." },
	{ "network.voice_kbps", _config_integer, "24", "HALO_NET_VOICE_KBPS", _environment_value, _platform_all,
		"Hosting: the voices' quality, in the lobby and in a game, in kilobits a\n"
		"second (8 to 64)." },
	{ "network.voice_proximity", _config_real, "15.0", "HALO_NET_VOICE_PROXIMITY", _environment_value,
		_platform_all,
		"Hosting: how near (world units: 1 is about 3 metres) a player must be\n"
		"to be heard by proximity voice chat (5 to 100)." },
	{ "network.text_chat", _config_string, "\"on\"", "HALO_NET_TEXT_CHAT", _environment_value, _platform_all,
		"Hosting: the text chat: \"on\" (links in lines are refused), \"links\"\n"
		"(links allowed) or \"off\", in either case. Always off in a game a\n"
		"player hosts that is listed in the server browser (a public internet\n"
		"game); a dedicated server's follows this setting, listed or not." },
	{ "network.votekick", _config_boolean, "true", "HALO_NET_VOTEKICK", _environment_value, _platform_all,
		"Hosting: let the players vote to kick a player (the scoreboard's\n"
		"right-click, or the console's votekick). More than half of the players\n"
		"must vote, counted once per address." },
	{ "network.votekick_minutes", _config_integer, "5", "HALO_NET_VOTEKICK_MINUTES", _environment_value,
		_platform_all,
		"Hosting: the minutes a player must have played on this server to start\n"
		"a vote to kick (0 to 60); to vote, 2 minutes or this, the less." },
	{ "network.votekick_ban_minutes", _config_integer, "30", "HALO_NET_VOTEKICK_BAN_MINUTES", _environment_value,
		_platform_all,
		"Hosting: the minutes a player kicked by a vote cannot join again (1 to\n"
		"1440)." },
	{ "network.map_share", _config_boolean, "true", "HALO_NET_MAP_SHARE", _environment_value, _platform_all,
		"Hosting: send a player who joins without the game's Custom Edition map\n"
		"the map (from maps_ce or custom_maps), and the resource maps it needs\n"
		"that the player lacks (bitmaps.map, sounds.map, loc.map). The game's other\n"
		"players help (network.map_seed)." },
	{ "network.map_share_upload", _config_integer, "1024", "HALO_NET_MAP_SHARE_UPLOAD", _environment_value,
		_platform_all,
		"Hosting: the most kilobytes a second the host sends of maps (16 to\n"
		"20480: 20 MB/s), so that the game's own traffic keeps the rest of the\n"
		"upload." },
	{ "network.map_share_lan_upload", _config_integer, "16384", "HALO_NET_MAP_SHARE_LAN_UPLOAD", _environment_value, /* AE hook */
		_platform_all,
		"Hosting a game whose players are all on the local network: the most\n"
		"kilobytes a second the host sends of maps (16 to 20480), when more\n"
		"than network.map_share_upload (which a game with a player on the\n"
		"internet keeps)." },
	{ "network.map_seed", _config_boolean, "true", "HALO_NET_MAP_SEED", _environment_value, _platform_all,
		"Playing in another's game: help send its map to players who join\n"
		"without it, as a torrent's peers do (the pieces this machine has, at\n"
		"most network.map_seed_upload)." },
	{ "network.map_seed_upload", _config_integer, "256", "HALO_NET_MAP_SEED_UPLOAD", _environment_value,
		_platform_all,
		"Playing in another's game: the most kilobytes a second this machine\n"
		"sends of maps to other players (16 to 10240: 10 MB/s)." },
	{ "network.map_download", _config_boolean, "true", "HALO_NET_MAP_DOWNLOAD", _environment_value, _platform_all,
		"Joining a game on a map this machine does not have: get it from its\n"
		"host (and the resource maps it needs), if the host shares it\n"
		"(network.map_share), with no question; in the lobby the host's other\n"
		"maps follow, up to network.map_download_limit. false: nothing is\n"
		"downloaded, and a game on a map you lack is left, saying which map." },
	{ "network.map_share_internet", _config_boolean, "false", "HALO_NET_MAP_SHARE_INTERNET", _environment_value, _platform_all, /* AE hook */
		"Hosting: also send maps to players who are not on the local network\n"
		"(an internet game's). Off, they get no map from this host; players on\n"
		"the local network get them as network.map_share says." },
	{ "network.map_download_internet", _config_boolean, "true", "HALO_NET_MAP_DOWNLOAD_INTERNET", _environment_value, _platform_all, /* AE hook */
		"Joining: also download maps from a host that is not on the local\n"
		"network (an internet game's). Off, only a host on the local network is\n"
		"asked for maps (network.map_download); a game on an internet host's\n"
		"map you lack is then left, saying so." },
	{ "network.map_download_limit", _config_integer, "4096", "HALO_NET_MAP_DOWNLOAD_LIMIT", _environment_value, _platform_all, /* AE hook */
		"Joining: the most megabytes of maps taken from one host (0 to 65536)." },
	{ "debug.map_share_test", _config_string, "\"\"", "HALO_MAP_SHARE_TEST", _environment_value, _platform_all, /* AE hook */
		"Tests: \"internet\" (the host taken for an internet one; the host's\n"
		"other maps are fetched in the lobby by themselves). A host's:\n"
		"\"lie-size\", \"lie-hash\" (its list is not what it offers), \"lie-map\" (it\n"
		"lists files whose headers are not their names'). Read only in a network\n"
		"test (debug.network_test)." },
	{ "network.coop_public", _config_boolean, "false", "HALO_NET_COOP_PUBLIC", _environment_value, _platform_all,
		"New online co-op games start as PUBLIC (true) or PRIVATE (false)." },
	{ "network.coop_friendly_fire", _config_string, "\"on\"", "HALO_NET_COOP_FRIENDLY_FIRE", _environment_value,
		_platform_all,
		"Whether the players of an online co-op game hurt each other: \"off\",\n"
		"\"on\", \"shields_only\" or \"explosives_only\" (FRIENDLY FIRE in\n"
		"co-op's Server Setup > Co-op Options writes its choice here). Their AI\n"
		"allies they hurt as network.coop_ally_friendly_fire says." },
	{ "network.coop_ally_friendly_fire", _config_boolean, "true", "HALO_NET_COOP_ALLY_FRIENDLY_FIRE",
		_environment_value, _platform_all,
		"Whether the players of an online co-op game hurt their AI allies (the\n"
		"Marines, Captain Keyes, the Sentinels while they are allies), as in the\n"
		"campaign; false, the players' weapons, grenades and vehicles do them\n"
		"no harm. ALLY FRIENDLY FIRE in co-op's Server Setup > Co-op Options\n"
		"writes its choice here." },
	{ "network.coop_own_friendly_fire", _config_boolean, "true", "HALO_NET_COOP_OWN_FRIENDLY_FIRE",
		_environment_value, _platform_all,
		"Whether this machine's players hurt, and are hurt by, the other players\n"
		"of an online co-op game: two players hurt each other only if both have\n"
		"it true. FRIENDLY FIRE in that game's pause menu writes its choice\n"
		"here. A player away from the controls for over 70 seconds is hurt by\n"
		"every player, whatever either chose." },
	{ "network.coop_player_collisions", _config_boolean, "true", "HALO_NET_COOP_PLAYER_COLLISIONS", _environment_value,
		_platform_all,
		"Whether the players of an online co-op game bump into each other;\n"
		"false, they walk through each other (the AI's characters they still\n"
		"bump into). PLAYER COLLISIONS in co-op's Server Setup > Co-op Options\n"
		"writes its choice here." },
	{ "network.coop_enemies_mode", _config_string, "\"per_player\"", "HALO_NET_COOP_ENEMIES_MODE", _environment_value,
		_platform_all,
		"Online co-op's extra enemies: \"none\", \"per_player\" (each squad of\n"
		"enemies grows by coop_enemies for each player past the first) or\n"
		"\"multiplier\" (each is coop_enemies_multiplier times as large, for any\n"
		"number of players). EXTRA ENEMIES in co-op's Server Setup > Co-op\n"
		"Options writes its choice here." },
	{ "network.coop_pickups", _config_string, "\"none\"", "HALO_NET_COOP_PICKUPS", _environment_value, _platform_all,
		"Extra campaign weapons, grenades, health kits, overshields and camouflage.\n"
		"Modes: \"none\", \"per_player\" (coop_enemies percent per extra player),\n"
		"or \"multiplier\" (coop_enemies_multiplier times as many), up to 8 times\n"
		"the original pickups. Only safe nearby places receive copies." },
	{ "network.coop_vehicles", _config_string, "\"none\"", "HALO_NET_COOP_VEHICLES", _environment_value, _platform_all,
		"Extra campaign vehicles: \"none\", \"per_player\" or \"multiplier\".\n"
		"After a player clears its unloaded starting place by both bounding\n"
		"radii plus 15 percent, make one more there, up to\n"
		"floor((players - 1) * coop_enemies * 2 / 300) extras, or floor((coop_enemies_multiplier - 1) * 2 / 3) in static mode.\n"
		"The host alone creates them, and waits while the starting place is blocked." },
	{ "network.coop_enemies", _config_integer, "50", "HALO_NET_COOP_ENEMIES", _environment_value, _platform_all,
		"Online co-op's extra enemies per player, a percentage: for each player\n"
		"past the first, each squad of enemies a level places gets this much of\n"
		"itself more (100: as many again; 25 to 200). PER PLAYER in co-op's\n"
		"Server Setup > Co-op Options writes its choice here." },
	{ "network.coop_enemies_multiplier", _config_integer, "2", "HALO_NET_COOP_ENEMIES_MULTIPLIER", _environment_value,
		_platform_all,
		"Online co-op's shared static multiplier (2 to 32). Pickups are capped\n"
		"at 8 times their original count; extra vehicle growth uses two thirds\n"
		"of (multiplier - 1), rounded down. MULTIPLIER in\n"
		"co-op's Server Setup > Co-op Options writes its choice here." },
	{ "network.brokers_file", _config_string, "\"brokers.txt\"",
		"HALO_NET_BROKERS_FILE", _environment_value, _platform_all,
		"The file of MQTT brokers that connect invites, one host:port per line,\n"
		"up to 4. Updates replace brokers.txt, so keep your own under another\n"
		"name." },
	{ "network.public_address", _config_string, "\"\"", "HALO_NET_PUBLIC_ADDRESS", _environment_value,
		_platform_desktop,
		"Hosting: the internet address joiners reach this machine at, as\n"
		"host[:port] (the port: the one forwarded to network.tunnel_port), for a\n"
		"server behind a NAT with one forwarded port; offered first, its name\n"
		"looked up again every 5 minutes. Empty: what STUN and UPnP find." },
	{ "network.listings", _config_string, "\"both\"", "HALO_NET_LISTINGS", _environment_value, _platform_all,
		"What the server browser lists: \"both\" (dedicated servers from the\n"
		"master servers first, then the games players host, from the brokers),\n"
		"\"masters\" (the master servers' alone) or \"brokers\" (the brokers'\n"
		"alone)." },
	{ "network.masters_file", _config_string, "\"masters.txt\"", "HALO_NET_MASTERS_FILE", _environment_value,
		_platform_all,
		"The file of the master servers the server browser asks for dedicated\n"
		"servers, beside this file unless a full path: an https:// URL and its\n"
		"key (ed25519:<hex>) on each line, up to 4. None named (no file, or\n"
		"comments alone): the official master, built in." },
	{ "network.browser_source", _config_string, "\"all\"", "HALO_NET_BROWSER_SOURCE", _environment_value,
		_platform_all,
		"The server browser's games: \"all\" (ALL GAMES: the dedicated\n"
		"servers first, then the games players host), \"dedicated\" (the\n"
		"master servers' dedicated servers) or \"players\" (P2P GAMES: games\n"
		"players host). Its top-left tab changes it." },
	{ "network.browser_show_empty", _config_boolean, "true", "HALO_NET_BROWSER_SHOW_EMPTY", _environment_value,
		_platform_all,
		"The server browser lists games with no player (its Filters screen)." },
	{ "network.browser_show_full", _config_boolean, "true", "HALO_NET_BROWSER_SHOW_FULL", _environment_value,
		_platform_all,
		"The server browser lists full games (its Filters screen)." },
	{ "network.browser_maps", _config_string, "\"yes\"", "HALO_NET_BROWSER_MAPS", _environment_value, _platform_all,
		"The server browser lists games on maps this machine lacks: \"yes\", \"no\",\n"
		"or \"downloadable\" (those of a Custom Edition map its host shares)." },
	{ "network.browser_gametype", _config_string, "\"any\"", "HALO_NET_BROWSER_GAMETYPE", _environment_value,
		_platform_all,
		"The server browser lists games of one kind only: \"any\", \"ctf\",\n"
		"\"slayer\", \"oddball\", \"king\" or \"race\" (its Filters screen)." },
	{ "network.browser_sort", _config_string, "\"servers\"", "HALO_NET_BROWSER_SORT", _environment_value,
		_platform_all,
		"The server browser's order: \"servers\" (dedicated servers first, then by\n"
		"players), \"players\", \"name\", \"map\", \"type\" or \"locked\" (a password's\n"
		"first); \"-\" before one reverses it (\"-players\"). A column title sorts by\n"
		"it, and again the other way." },
	{ "network.stun_servers", _config_string, "\"stun.l.google.com:19302,stun.cloudflare.com:3478\"",
		"HALO_NET_STUN", _environment_value, _platform_all,
		"Public STUN servers that tell this machine its internet address;\n"
		"comma-separated host:port." },
	{ "network.legacy_table_fetch", _config_boolean, "false", "HALO_LEGACY_TABLE_FETCH", _environment_value,
		_platform_all,
		"Fetch ChupathingyCE's signed legacy table (Delta) from network.browser_url\n"
		"(else GitHub) at start and every few hours. Off in Arena Evolved: its\n"
		"own wire has no row in those tables, so they change nothing here." },
	{ "network.legacy_table", _config_string, "\"\"", "HALO_LEGACY_TABLE", _environment_value, _platform_all,
		"Testing only: an unsigned legacy table file (docs/delta.md) that sets\n"
		"the OpenCE network versions. Empty for none." },
#ifdef HALO_GAME_BROWSER
	{ "network.browser_url", _config_string, "\"https://halo.milenko.org\"", "HALO_NET_BROWSER", _environment_value,
		_platform_all,
		"The game list server (configure.py --game-browser): hosted games are\n"
		"listed there, and System Link shows its games; empty for none." },
	{ "network.community_services", _config_string, "\"ask\"", "HALO_COMMUNITY_SERVICES", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved: the community services the ChupathingyCE team runs at\n"
		"network.browser_url (halo.milenko.org): System Link's list of internet\n"
		"games, LIST HOSTED GAMES, SHARE GAME STATS and SHARE HOSTED STATS (Link\n"
		"Profile and the profile page too). \"yes\" uses them (each of those\n"
		"settings still says whether its part runs); \"no\" never contacts that\n"
		"server; \"ask\" (a new install's, and an upgrade's from 0.3.4) asks once,\n"
		"on the PC menus' first main menu, and is off until then. Settings >\n"
		"Network Setup > COMMUNITY SERVICES changes it." },
	{ "network.list_hosted_games", _config_boolean, "false", "HALO_NET_LIST_GAMES", _environment_value, _platform_all,
		"List the system link games this machine hosts on network.browser_url,\n"
		"where anyone can find and join them, and send each one's carnage report\n"
		"there when it ends. False (this build's default: an opt-in, Settings >\n"
		"Network) keeps them to invites and the local network, and sends no report." },
	{ "network.report_hosted_games", _config_boolean, "true", "HALO_NET_REPORT_HOSTED", _environment_value,
		_platform_all,
		"Send network.browser_url the stats of the games this machine hosts: a\n"
		"listed game's carnage report and its Delta Stats (network.report_events).\n"
		"False sends neither, and the game stays listed (network.list_hosted_games)." },
	{ "network.report_joined_games", _config_boolean, "false", "HALO_NET_REPORT_GAMES", _environment_value,
		_platform_all,
		"When an internet game this machine joined ends, send network.browser_url\n"
		"its scores as this machine saw them, with this copy's player ID, so that\n"
		"games whose host does not report them are recorded too, and confirm\n"
		"this machine's players' lines in the host's report of it (/v1/claim),\n"
		"which puts the game on their profile. False (this build's default: an\n"
		"opt-in, Settings > Network) sends neither." },
	{ "network.report_events", _config_boolean, "false", "HALO_NET_REPORT_EVENTS", _environment_value, _platform_all,
		"Delta Stats: record the games this machine hosts (kills with weapons and\n"
		"positions, accuracy, medals, objectives, vehicles, pickups, positions a\n"
		"few seconds apart) and send them to network.browser_url when each ends,\n"
		"for the site's match pages, heatmaps and leaderboards. Players are their\n"
		"names, never a hardware ID or an address; an OpenCE player is only their\n"
		"name, team, score, place, kills and deaths. False (this build's default:\n"
		"an opt-in, Settings > Network; ChupathingyCE's is true) records and sends\n"
		"no events; a listed game's carnage report still goes (list_hosted_games)." },
	{ "network.events_token", _config_string, "\"\"", "HALO_EVENTS_TOKEN", _environment_value, _platform_all,
		"A dedicated server's Delta Stats token, from the game list's operator." },
	{ "network.events_positions", _config_integer, "2", "HALO_EVENTS_POSITIONS", _environment_value, _platform_all,
		"Delta Stats: seconds between position samples, 1 to 60; 0 for none." },
	{ "network.events_limit", _config_integer, "40000", "HALO_EVENTS_LIMIT", _environment_value, _platform_all,
		"Delta Stats: the most events kept for a game, 64 to 200000." },
	{ "network.events_part_minutes", _config_integer, "30", "HALO_EVENTS_PART_MINUTES", _environment_value,
		_platform_all,
		"Delta Stats: send a long game in parts every this many minutes; 0\n"
		"sends it only at the end." },
	{ "network.events_folder", _config_string, "\"\"", "HALO_EVENTS_FOLDER", _environment_value, _platform_all,
		"Delta Stats: a folder to keep a copy of each batch sent; empty for\n"
		"none." },
	{ "network.chat_legacy", _config_boolean, "true", "HALO_NET_CHAT_LEGACY", _environment_value, _platform_all,
		"In games you host, let OpenCE players chat too (over their own\n"
		"text chat, bridged), while network.chat is on." },
	{ "network.copy_to_clipboard", _config_boolean, "false", "HALO_NET_COPY_TO_CLIPBOARD", _environment_value,
		_platform_all,
		"Put a game's invite link on the clipboard when you start to host it.\n"
		"false: the log has it, and the Gearbox menus' Server Settings copy it." },
#endif
#ifndef HALO_CONSOLE_NO_DISCORD
	/* (Arena Evolved: OpenCE's Discord application, as it had before; not
	ChupathingyCE's, "Chupathingy") */
	{ "discord.application_id", _config_string, "\"1553978809840050229\"", "HALO_DISCORD_APPLICATION", /* AE hook */
		_environment_value, _platform_desktop,
		"The Discord application the game shows as while the Discord desktop\n"
		"client runs (its name is what Discord says is being played); empty\n"
		"for none." },
	/* (Arena Evolved, owner 2026-10-20: off by default until AE has a Discord application of its own; "full"
	shows OpenCE's application with ChupathingyCE's map pictures and join page) */
	{ "discord.presence", _config_string, "\"off\"", "HALO_DISCORD_PRESENCE", _environment_value, /* AE hook */
		_platform_desktop,
		"What Discord shows of the game: \"full\" (the game type, map, score,\n"
		"players and time, the map's picture, and a public game's Join game\n"
		"button), \"minimal\" (only the application's name) or \"off\"." },
	{ "discord.show_server_name", _config_boolean, "true", "HALO_DISCORD_SHOW_SERVER_NAME", _environment_value,
		_platform_desktop,
		"Show the network game's name on Discord." },
	{ "discord.show_join", _config_boolean, "true", "HALO_DISCORD_SHOW_JOIN", _environment_value,
		_platform_desktop,
		"Show a Join game button on Discord while you host a public game." },
#endif

#ifndef HALO_CONSOLE_NO_UPDATER
	{ "update.auto", _config_boolean, "true", "HALO_UPDATE_AUTO", _environment_value, _platform_all, /* AE hook */
		"A release's build (from 0.4.3, Linux and Windows) looks for a new\n"
		"version on GitHub when the game starts and asks before it updates;\n"
		"false never looks. Do not ask again writes false." },
#endif
	/* (a dedicated server's: halo --server, port/linux/src/server_mode.c;
	written into the file only by one) */
	{ "server.enabled", _config_boolean, "false", "HALO_SERVER", _environment_set_is_true, _platform_server,
		"Run as a dedicated server (as halo --server does): host games with no\n"
		"window, sound or player of its own, one after another from\n"
		"server.rotation (docs/dedicated_server.md)." },
	{ "server.name", _config_string, "\"OpenCE Server\"", "HALO_SERVER_NAME", _environment_value, _platform_server,
		"The game's name in the lobby and the server browser: 15 characters of\n"
		"printable ASCII." },
	{ "server.max_players", _config_integer, "16", "HALO_SERVER_MAX_PLAYERS", _environment_value, _platform_server,
		"The most players (2 to 127)." },
	{ "server.min_players", _config_integer, "1", "HALO_SERVER_MIN_PLAYERS", _environment_value, _platform_server,
		"Players in the lobby before it counts down to a game (1 to 16)." },
	{ "server.password", _config_string, "\"\"", "HALO_SERVER_PASSWORD", _environment_value, _platform_server,
		"The password the server browser asks for (the invite link still joins);\n"
		"empty for none. Never logged." },
	{ "server.public", _config_boolean, "true", "HALO_SERVER_PUBLIC", _environment_value, _platform_server,
		"List the server in everyone's server browser (on the master servers,\n"
		"with network.online); false: joined by its invite link and on the LAN." },
	{ "server.rotation", _config_string, "\"bloodgulch:ctf, sidewinder:team_slayer, prisoner:slayer\"",
		"HALO_SERVER_ROTATION", _environment_value, _platform_server,
		"The games played one after another: map[:gametype[+rule=value...]],\n"
		"separated by commas, 64 at most. A map is a multiplayer map's name\n"
		"(bloodgulch), custom_maps\\<name> or a level's path; a gametype a\n"
		"built-in's name (slayer, team_slayer, ctf...), an OpenCE gametype's\n"
		"(swat...), a gametypes/<name>.toml or file=<saved gametype>." },
	{ "server.rotation_fallback", _config_boolean, "false", "HALO_SERVER_ROTATION_FALLBACK", _environment_value,
		_platform_server,
		"With no playable rotation entry, play bloodgulch:slayer rather than\n"
		"stop (exit code 3)." },
	{ "server.lobby_seconds", _config_integer, "15", "HALO_SERVER_LOBBY_SECONDS", _environment_value,
		_platform_server,
		"Seconds the lobby counts down once min_players are in (0 to 300; 0\n"
		"starts at once)." },
	{ "server.postgame_seconds", _config_integer, "12", "HALO_SERVER_POSTGAME_SECONDS", _environment_value,
		_platform_server,
		"Seconds the scores show after a game (0 to 300)." },
	{ "server.empty_seconds", _config_integer, "30", "HALO_SERVER_EMPTY_SECONDS", _environment_value,
		_platform_server,
		"Seconds a game nobody is in goes on before it ends (0: never). A game\n"
		"whose last player leaves ends at once, as the game ends it." },
	{ "server.idle_minutes", _config_integer, "0", "HALO_SERVER_IDLE_MINUTES", _environment_value, _platform_server,
		"Minutes a game goes on with no score before it ends (0: never)." },
	{ "server.team_start", _config_string, "\"one\"", "HALO_SERVER_TEAM_START", _environment_value, _platform_server,
		"A team game: \"one\" starts with one team's players (the next joiners\n"
		"balance), \"both\" waits for a player on each team, \"skip\" skips team\n"
		"games while one player waits." },
	{ "server.motd", _config_string, "\"\"", "HALO_SERVER_MOTD", _environment_value, _platform_server,
		"A notice each machine is shown as it joins (127 characters); empty for\n"
		"none." },
	{ "server.frame_rate", _config_integer, "60", "HALO_SERVER_FRAME_RATE", _environment_value, _platform_server,
		"Frames a second (30 to 120); the game's ticks stay 30 a second. A\n"
		"server with no other machine runs 10." },
	{ "server.console", _config_boolean, "true", "HALO_SERVER_CONSOLE", _environment_value, _platform_server,
		"Take commands from the standard input." },
	{ "server.console_exit_on_eof", _config_boolean, "false", "HALO_SERVER_CONSOLE_EXIT_ON_EOF", _environment_value,
		_platform_server,
		"Stop when the standard input ends; false goes on (a panel may close\n"
		"it)." },
	{ "server.init_file", _config_string, "\"init.txt\"", "HALO_SERVER_INIT_FILE", _environment_value,
		_platform_server,
		"Commands run once at the start, one a line: in the data folder unless a\n"
		"full path; empty for none." },
	{ "server.web_port", _config_integer, "8080", "HALO_SERVER_WEB_PORT", _environment_value, _platform_server,
		"The TCP port of the web page, its API and the remote console (RCON);\n"
		"0 for none." },
	{ "server.web_bind", _config_string, "\"127.0.0.1\"", "HALO_SERVER_WEB_BIND", _environment_value,
		_platform_server,
		"The address the web port listens on. Any but 127.0.0.1 needs\n"
		"web_password (behind a reverse proxy with TLS)." },
	{ "server.web_password", _config_string, "\"\"", "HALO_SERVER_WEB_PASSWORD", _environment_value,
		_platform_server,
		"The web page's admin password (8 characters or more). Empty: one is\n"
		"made at the first start, printed once and written here." },
	{ "server.web_owner_password", _config_string, "\"\"", "HALO_SERVER_WEB_OWNER_PASSWORD", _environment_value,
		_platform_server,
		"A password with the owner role (quit, sv_restart, sv_stop, the owner's\n"
		"settings, the game's console), for a supervisor on this machine: it\n"
		"signs in from 127.0.0.0/8 alone. 8 characters or more; empty for none." },
	{ "server.web_moderator_password", _config_string, "\"\"", "HALO_SERVER_WEB_MODERATOR_PASSWORD",
		_environment_value, _platform_server,
		"A second password, with the moderator role (kick, mute, say, the next\n"
		"map, bans of a day at most); empty for none." },
	{ "server.web_viewer_password", _config_string, "\"\"", "HALO_SERVER_WEB_VIEWER_PASSWORD", _environment_value,
		_platform_server,
		"A password with the viewer role (the status, the players, the log: no\n"
		"changes); empty for none." },
	{ "server.web_ui", _config_boolean, "true", "HALO_SERVER_WEB_UI", _environment_value, _platform_server,
		"Serve the admin page on the web port; false: its API and RCON only." },
	{ "server.web_trusted_proxy", _config_string, "\"\"", "HALO_SERVER_WEB_TRUSTED_PROXY", _environment_value,
		_platform_server,
		"The address of a reverse proxy in front of the web port, whose\n"
		"X-Forwarded-For (its last entry, the proxy's), -Host and -Proto are\n"
		"believed; empty for none." },
	{ "server.metrics", _config_boolean, "true", "HALO_SERVER_METRICS", _environment_value, _platform_server,
		"Serve /metrics (Prometheus) on the web port without a password." },
	{ "server.log_format", _config_string, "\"text\"", "HALO_SERVER_LOG_FORMAT", _environment_value,
		_platform_server,
		"The standard output's lines: \"text\" or \"json\"." },
	{ "server.log_addresses", _config_boolean, "false", "HALO_SERVER_LOG_ADDRESSES", _environment_value,
		_platform_server,
		"Players' internet addresses and hardware ids in the log, the events and\n"
		"the API for every role (the owner's console has them always)." },
	{ "server.log_debug_echo", _config_boolean, "false", "HALO_SERVER_LOG_DEBUG_ECHO", _environment_value,
		_platform_server,
		"Also write debug.txt's lines to the standard error." },
	{ "server.status_file", _config_string, "\"\"", "HALO_SERVER_STATUS_FILE", _environment_value,
		_platform_server,
		"A file the status (as /api/status) is written to every 2 seconds, for a\n"
		"panel without HTTP; empty for none." },
	{ "server.auto_update", _config_string, "\"off\"", "HALO_SERVER_AUTO_UPDATE", _environment_value,
		_platform_server,
		"\"off\"; \"check\" logs a newer release; \"install\" installs it once the\n"
		"server has been empty for 10 minutes, and stops (exit code 5) to be\n"
		"started again." },
	{ "server.upnp", _config_boolean, "false", "HALO_SERVER_UPNP", _environment_value, _platform_server,
		"Ask the router (UPnP) to forward the tunnel's port (network.allow_upnp,\n"
		"for a server at home)." },
	{ "server.coop_difficulty", _config_string, "\"normal\"", "HALO_SERVER_COOP_DIFFICULTY", _environment_value,
		_platform_server,
		"Co-op rotation entries' (<level>:coop) difficulty: \"easy\", \"normal\",\n"
		"\"hard\" or \"impossible\"." },
	{ "server.list_on_brokers", _config_boolean, "false", "HALO_SERVER_LIST_ON_BROKERS", _environment_value,
		_platform_server,
		"Also list the server among the games players host (through the\n"
		"signalling brokers), for server browsers with no master server." },
	{ "server.master_publish", _config_boolean, "true", "HALO_SERVER_MASTER_PUBLISH", _environment_value,
		_platform_server,
		"Publish a public server to the official master servers, signed with\n"
		"the publisher key (publisher_key_file)." },
	{ "server.masters_file", _config_string, "\"masters.txt\"", "HALO_SERVER_MASTERS_FILE", _environment_value,
		_platform_server,
		"The master servers published to, beside this file unless a full path:\n"
		"an https:// URL and its key (ed25519:<hex>) on each line. None named:\n"
		"the official master, built in." },
	{ "server.publisher_key_file", _config_string, "\"publisher.key\"", "HALO_SERVER_PUBLISHER_KEY_FILE",
		_environment_value, _platform_server,
		"The operator's Ed25519 key for the master servers (halo --server\n"
		"--keygen makes it), beside this file unless a full path.\n"
		"HALO_SERVER_PUBLISHER_KEY holds the key itself for containers." },
	{ "server.instance", _config_string, "\"\"", "HALO_SERVER_INSTANCE", _environment_value, _platform_server,
		"A short name telling this server from the operator's others on the\n"
		"master servers (letters, digits, - and _); empty: from server.name." },
	{ "server.region", _config_string, "\"\"", "HALO_SERVER_REGION", _environment_value, _platform_server,
		"A region shown in the server browser (eu, na-east...): 16 characters;\n"
		"empty for none." },
	{ "server.chat_filter", _config_string, "\"off\"", "HALO_SERVER_CHAT_FILTER", _environment_value, _platform_server,
		"The words the server keeps out of its chat and its players' names\n"
		"(streamer mode's list, by chat_filter_slurs, _swearing and _insults):\n"
		"\"off\"; \"censor\", a line's words shown as stars; \"block\", a line\n"
		"with them refused (its sender is told why) and a player whose name has\n"
		"them refused at the door. Links are network.text_chat's." },
	{ "server.chat_filter_slurs", _config_string, "\"mild\"", "HALO_SERVER_CHAT_FILTER_SLURS", _environment_value,
		_platform_server,
		"With server.chat_filter, the slurs (race, sexuality, gender, disability)\n"
		"kept out: \"off\", \"severe\" (the worst), \"strong\" or \"mild\" (all)." },
	{ "server.chat_filter_swearing", _config_string, "\"off\"", "HALO_SERVER_CHAT_FILTER_SWEARING",
		_environment_value, _platform_server,
		"With server.chat_filter, the swearing and sexual words kept out: \"off\",\n"
		"\"severe\", \"strong\" or \"mild\" (all)." },
	{ "server.chat_filter_insults", _config_string, "\"off\"", "HALO_SERVER_CHAT_FILTER_INSULTS",
		_environment_value, _platform_server,
		"With server.chat_filter, the insults that are not slurs kept out: \"off\",\n"
		"\"severe\", \"strong\" or \"mild\" (all)." },
	{ "server.master_publish_address", _config_boolean, "false", "HALO_SERVER_MASTER_PUBLISH_ADDRESS",
		_environment_value, _platform_server,
		"Let the master servers give this server's address to players; false:\n"
		"only their reachability probe uses it." },
	{ "streamer.enabled", _config_boolean, "false", "HALO_STREAMER_MODE", _environment_value, _platform_all,
		"Hide offensive words in names and text from other players, maps and mods\n"
		"as stars, at the levels below (Settings > Network Setup > Streamer Mode)." },
	{ "streamer.slurs", _config_string, "\"mild\"", "HALO_STREAMER_SLURS", _environment_value, _platform_all,
		"With streamer.enabled, the slurs (race, sexuality, gender, disability)\n"
		"hidden: \"off\", \"severe\", \"strong\" (and severe) or \"mild\" (all)." },
	{ "streamer.swearing", _config_string, "\"strong\"", "HALO_STREAMER_SWEARING", _environment_value, _platform_all,
		"With streamer.enabled, the swearing and sexual words hidden: \"off\",\n"
		"\"severe\", \"strong\" (and severe) or \"mild\" (all)." },
	{ "streamer.insults", _config_string, "\"off\"", "HALO_STREAMER_INSULTS", _environment_value, _platform_all,
		"With streamer.enabled, the insults that are not slurs hidden: \"off\",\n"
		"\"severe\", \"strong\" (and severe) or \"mild\" (all)." },
	{ "system.dev_mode", _config_boolean, "false", "HALO_DEV_MODE", _environment_set_is_true, _platform_all, /* AE hook */
		"DEV MODE: this PC's tools for testing AE (port/linux/game/ae_dev_mode.h):\n"
		"F2 a third-person camera over the shoulder, F3 the other shoulder, F6 a\n"
		"free camera (controls.dev_*; a controller: BACK with the d-pad's down,\n"
		"right, left). On foot.\n"
		"Hit markers: brackets round the reticle when your damage lands (red: a kill).\n"
		"Never on in another machine's network game, where the host's rules hold." },
	{ "system.dev_camera_distance", _config_real, "100", "HALO_DEV_CAMERA_DISTANCE", _environment_value, _platform_all, /* AE hook */
		"With system.dev_mode, the third-person dev camera's distance behind the\n"
		"shoulder, in percent (25 to 400)." },
	{ "crash_reports.upload", _config_string, "\"ask\"", "HALO_CRASH_REPORTS", _environment_value, _platform_desktop,
		"No effect: Arena Evolved's builds send no crash reports and keep none\n"
		"to send (ChupathingyCE's releases send theirs through\n"
		"network.browser_url, port/linux/src/crash_report.h). Where they are on:\n"
		"\"yes\" sends them, \"no\" never does, \"ask\" asks after the next crash\n"
		"and writes the answer here." },

	{ "debug.network_test", _config_string, "\"\"", "HALO_NETWORK_TEST", _environment_value, _platform_all,
		"Automated system link sessions for testing (port/linux/game/network_test.c):\n"
		"\"host:<map>\" hosts a game on that map, \"local:<map>\" plays a local (split\n"
		"screen) game on it, \"join\" joins the first game found,\n"
		"\"browse\" the one with the most players the server browser lists (not\n"
		"co-op, if it can), \"log\" logs the game that runs (and runs its cues);\n"
		"empty for none." },
	{ "debug.network_test_start", _config_real, "15.0", "HALO_NETWORK_TEST_START", _environment_value, _platform_all,
		"Seconds after hosting that an automated test game starts." },
	{ "debug.network_test_kill", _config_real, "0.0", "HALO_NETWORK_TEST_KILL", _environment_value, _platform_all,
		"Every this many seconds an automated test host's own player (else the first)\n"
		"kills the last other player; 0 never." },
	{ "debug.network_test_team_change", _config_real, "0.0", "HALO_NETWORK_TEST_TEAM_CHANGE", _environment_value,
		_platform_all,
		"Every this many seconds a test player changes teams; 0 never." },
	{ "debug.network_test_pause", _config_real, "0.0", "HALO_NETWORK_TEST_PAUSE", _environment_value, _platform_all,
		"That many seconds into an automated test game, the first player's pause\n"
		"menu opens (to see it); 0 never." },
	{ "debug.network_test_score", _config_integer, "0", "HALO_NETWORK_TEST_SCORE", _environment_value, _platform_all,
		"The score an automated test host's game type plays to (a short game, to\n"
		"test the next); 0 the game type's own." },
	{ "debug.network_test_time_limit", _config_integer, "0", "HALO_NETWORK_TEST_TIME_LIMIT", _environment_value,
		_platform_all,
		"The time limit, in minutes, of an automated test host's game type (the\n"
		"match clock's time left); 0 the game type's own." },
	{ "debug.network_test_shoot", _config_real, "0.0", "HALO_NETWORK_TEST_SHOOT", _environment_value, _platform_all,
		"Every this many seconds each test player hits the next; 0 never." },
	{ "debug.network_test_vehicle", _config_real, "0.0", "HALO_NETWORK_TEST_VEHICLE", _environment_value, _platform_all,
		"Seconds into a test game to seat the last player in a vehicle; 0\n"
		"never." },
	{ "debug.network_test_pickup", _config_real, "0.0", "HALO_NETWORK_TEST_PICKUP", _environment_value, _platform_all,
		"This many seconds into an automated test game the host stands its last\n"
		"player on a weapon, which a joining player then picks up; 0 never." },
	{ "debug.network_test_hurt", _config_real, "0.0", "HALO_NETWORK_TEST_HURT", _environment_value, _platform_all,
		"An automated test host leaves the first player 40% of their health, shields\n"
		"full, this many seconds into the game; 0 never." },
	{ "debug.network_test_damage", _config_string, "\"\"", "HALO_NETWORK_TEST_DAMAGE", _environment_value,
		_platform_all,
		"An automated test host hits its first player with its weapon's bullet at\n"
		"given game times: \"seconds:scale,seconds:scale\" (the damage times the\n"
		"scale), and logs every change of their health and shields with its tick." },
	{ "debug.network_test_ball", _config_string, "\"\"", "HALO_NETWORK_TEST_BALL", _environment_value,
		_platform_all,
		"An automated test host puts a ball in its first local player's hand at a game\n"
		"time, once: \"seconds[:shield[:offset]]\": the last other player's shields\n"
		"set (3 is an overshield) and stood offset (default 1.2) along x from the\n"
		"first (they look along +x: negative faces the blow). For BALL MELEE tests." },
	{ "debug.network_test_ball_melee", _config_boolean, "false", "HALO_NETWORK_TEST_BALL_MELEE", _environment_value,
		_platform_all,
		"An automated test host's gametype gets BALL MELEE LETHAL (for built-in\n"
		"gametypes, such as a juggernaut game)." },
	{ "debug.network_test_silent", _config_string, "\"\"", "HALO_NETWORK_TEST_SILENT", _environment_value, /* AE hook */
		_platform_all,
		"An automated test host's gametype's SILENT MOVEMENT and SILENT WEAPONS:\n"
		"\"MOVEMENT,WEAPONS\" or one for both, each normal, just_me or silent;\n"
		"empty leaves the gametype's (the muted counts are logged as they grow)." },
	{ "debug.bot_links", _config_boolean, "true", "HALO_BOT_LINKS", _environment_value, _platform_all, /* AE hook */
		"Bots take teleporters and the map's layout jumps (bot jump lines) on\n"
		"their way when that is shorter; false for tests of how they did without\n"
		"(the teleports are still counted on halo.log's \"bots: travel\" line)." },
	{ "debug.bot_hints", _config_boolean, "false", "HALO_BOT_HINTS", _environment_set_is_true, _platform_all, /* AE hook */
		"For map layout authors: log the map's layout header line, each of\n"
		"player 1's jumps as a ready \"bot jump at ... to ...\" line and a\n"
		"\"bot avoid\" line where player 1 crouches still a second." },
	{ "debug.bot_events", _config_boolean, "false", "HALO_BOT_EVENTS", _environment_set_is_true, _platform_all, /* AE hook */
		"Log each bot's fight events (target seen, shots, hits, melee, strafe, crouch,\n"
		"kills, deaths, a position each second) as \"bot event:\" lines, for\n"
		"tools/ae_test/bot_report.py." },
	{ "debug.hit_feedback", _config_boolean, "false", "HALO_HIT_FEEDBACK", _environment_set_is_true, _platform_all, /* AE hook */
		"For tests and quick launches: every multiplayer game plays HIT FEEDBACK:\n"
		"ARENA EVOLVED, whatever its gametype says (this machine's drawing only)." },
	{ "debug.arena_audit", _config_boolean, "false", "HALO_ARENA_AUDIT", _environment_set_is_true, _platform_all, /* AE hook */
		"For checking the gametypes: once the arena gametypes are seeded, write\n"
		"every field of each one (and of the stock gametypes) to the save root's\n"
		"z/arena_audit.tsv, for tools/ae_gametype_audit.py." },
	{ "debug.network_test_melee", _config_boolean, "false", "HALO_NETWORK_TEST_MELEE", _environment_value,
		_platform_all,
		"With debug.network_test_shoot, each player strikes with the weapon's melee\n"
		"blow instead of its projectile." },
	{ "debug.network_test_flags", _config_integer, "0", "HALO_NETWORK_TEST_FLAGS", _environment_value,
		_platform_all,
		"Bits an automated test host sets in its game variant's flags (the port's\n"
		"gametype options, game_engine.h: 65536 no fall damage; health 524288 REACH,\n"
		"1048576 HALO 3, 1572864 HALO 2)." },
	{ "debug.network_test_gametype", _config_string, "\"\"", "HALO_NETWORK_TEST_GAMETYPE", _environment_value,
		_platform_all,
		"An automated test host's gametype: a custom one of the save root, by its\n"
		"stored name (with its PC options, as picking it in the menus does), in\n"
		"place of the built-in one; not found, the built-in one. Empty for none.\n"
		"(Looking seeds the Arena Evolved gametypes into the save root in use:\n"
		"set HALO_SAVE_ROOT for tests.)" },
	{ "debug.waypoint_icons", _config_string, "\"\"", "HALO_WAYPOINT_ICONS", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of the item waypoints' pictures\n"
		"(display.waypoint_style): \"no_sheet\" plays as a map whose HUD has no\n"
		"messaging icons, \"bad_sequence\" as one whose weapons name a picture\n"
		"its sheet has not: every weapon's waypoint shows its text. Empty for\n"
		"the map's own." },
	{ "debug.network_test_pack_loadout", _config_string, "\"\"", "HALO_NETWORK_TEST_PACK_LOADOUT", _environment_value, /* AE hook */
		_platform_all,
		"An automated test host's gametype uses the weapon pack in its custom\n"
		"loadout: primary, secondary or both (the stock values stay an assault rifle\n"
		"and a pistol, the fallbacks); primary2, secondary2 the second named pack's\n"
		"weapon; first_second the first's and the second's; second_first the second's\n"
		"and the first's. Empty for none." },
	{ "debug.network_test_pack_map_spawns", _config_boolean, "false", "HALO_NETWORK_TEST_PACK_MAP_SPAWNS", _environment_value, /* AE hook */
		_platform_all,
		"An automated test host's gametype uses the weapon packs and has their MAP\n"
		"SPAWNS (the map's pistols spawn the battle rifle, its assault rifles the\n"
		"SMG, where those packs are named). A test switch: no gametype has it." },
	{ "debug.network_test_weapon_packs", _config_integer, "0", "HALO_NETWORK_TEST_WEAPON_PACKS", _environment_value, /* AE hook */
		_platform_all,
		"An automated test host's gametype names these weapon packs, a mask of\n"
		"their slots: 1 the battle rifle, 2 the SMG, 4 the silenced SMG, 8 the\n"
		"carbine (3: the BR and the SMG). 0: the battle rifle alone, when a pack\n"
		"option above is set." },
	{ "debug.weapon_pack_file", _config_string, "\"\"", "HALO_WEAPON_PACK_FILE", _environment_value, /* AE hook */
		_platform_all,
		"A weapon pack read in place of packs\\ae_battle_rifle.aepack (tests: variants\n"
		"and refusals). An Xbox path, not a path of this computer: it starts d:\\, the\n"
		"data root (d:\\pack_file\\x.aepack is pack_file/x.aepack beside maps/). Its\n"
		"SHA-256 names it as any pack's does. Empty for the packs folder's." },
	{ "debug.weapon_pack_ground", _config_boolean, "false", "HALO_WEAPON_PACK_GROUND", _environment_value, /* AE hook */
		_platform_all,
		"For automated tests' pictures: one more of the weapon pack's weapon lies a\n"
		"step ahead of each local player who spawns with it, and of one who spawns\n"
		"without it in a game that asks for the pack (BATTLE RIFLE ON MAP: the swap\n"
		"prompt, or a pickup). Only on a host or with no network game; a client in\n"
		"another's game ignores it." },
	{ "debug.weapon_pack_table_limit", _config_integer, "0", "HALO_WEAPON_PACK_TABLE_LIMIT", _environment_value, /* AE hook */
		_platform_all,
		"For automated tests of a full tag table: the most tags a map and a weapon\n"
		"pack's own may have together, below the built-in 49152 (0xC000); a map\n"
		"past it refuses the pack. 0 for the built-in limit." },
	{ "debug.digits", _config_string, "\"\"", "HALO_DEBUG_DIGITS", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of the HUD's counter digits (ae_hud_digits.c): the\n"
		"text (digits, \":\" and spaces) drawn in magenta at 24 px in the first view's\n"
		"top left, and its measures in the log." },
	{ "debug.lone_pad_first", _config_boolean, "false", "HALO_DEBUG_LONE_PAD_FIRST", _environment_value, /* AE hook */
		_platform_all,
		"For test launches (ae_platform.c): in split screen a lone gamepad stays\n"
		"player 1's, with the keyboard, and player 2 stands with no input. Off:\n"
		"the keyboard is player 1 and the lone gamepad player 2." },
	{ "debug.ds5_log", _config_boolean, "false", "HALO_DEBUG_DS5_LOG", _environment_value, /* AE hook */
		_platform_all,
		"A DualSense's effects in debug.txt (ae_ds5.c): the bytes written, what\n"
		"each player's triggers and lightbar are asked for as it changes, the\n"
		"rumble, each weapon tag's kind once a map." },
	{ "debug.ae_panel_primitives", _config_boolean, "false", "HALO_DEBUG_AE_PANEL_PRIMITIVES", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of the AE panels (ae_panel_draw.c): one of each of\n"
		"their primitives on a grey plate in the first view, and their places in the\n"
		"log." },
	{ "debug.ae_panel_log", _config_boolean, "false", "HALO_DEBUG_AE_PANEL_LOG", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of the AE panels (ae_panel_engine.c): every 2\n"
		"seconds, one line per local player of what the ammo, grenade, shield and\n"
		"health panels read from the game. Only this machine's log." },
	{ "debug.ae_panel_powerups", _config_string, "\"\"", "HALO_DEBUG_AE_PANEL_POWERUPS", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of the AE panels (ae_panel_engine.c): an automated\n"
		"test host gives its first player the overshield or camo at these seconds of\n"
		"game time (\"overshield:10,camo:20\"); empty never." },
	{ "debug.ae_panel_state", _config_string, "\"\"", "HALO_DEBUG_AE_PANEL_STATE", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests and pictures of the AE panels (ae_panels.c): a\n"
		"state drawn over the game's (low, empty, overheated, shield_low,\n"
		"shield_down, overshield1, overshield2, camo, buffs, health_low,\n"
		"script_blink: the scripts' health, shield and sensor blink)." },
	{ "debug.ae_message_test", _config_string, "\"\"", "HALO_DEBUG_AE_MESSAGE_TEST", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests and pictures of HUD STYLE AE's message lines\n"
		"(ae_hud_messages.c): a comma list of the game's own messages to set:\n"
		"vehicle (a vehicle's enter prompt), swap (a swap prompt for the player's\n"
		"weapon), long (a long custom prompt with a button picture), pickup (an\n"
		"ammunition line every 4 s). wrap2x breaks lines at twice the ammo\n"
		"group's width; bigicons draws the pictures 1.36 x taller; ce draws the\n"
		"lines as before (CE's own, smaller). A log line of the lines' places\n"
		"every 2 seconds." },
	{ "debug.ae_panel_seat", _config_string, "\"\"", "HALO_DEBUG_AE_PANEL_SEAT", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of the AE panels (ae_panel_engine.c): an automated\n"
		"test host seats its first player in a vehicle (\"ghost:driver:8\",\n"
		"\"warthog:gunner:8\": the vehicle's tag, the seat, the second)." },
	{ "debug.score_strip_log", _config_boolean, "false", "HALO_DEBUG_SCORE_STRIP_LOG", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of the score strip (ae_strip_engine.c): every 5\n"
		"seconds of game time, one line per local player of what the strip says.\n"
		"Any machine; only this machine's log." },
	{ "debug.bomb_bar", _config_string, "\"\"", "HALO_DEBUG_BOMB_BAR", _environment_value, /* AE hook */
		_platform_all,
		"For the automated tests of Bomb's PLANTING / DEFUSING bar (HUD STYLE AE,\n"
		"ae_score_strip.c): the first view shows it whenever its strip does\n"
		"(\"planting:0.6\", \"defusing:0.35\": the label, how far)." },
	{ "debug.bomb_blast_look", _config_integer, "0", "HALO_BOMB_BLAST_LOOK", _environment_value, /* AE hook */
		_platform_all,
		"For the owner's pick of SEARCH AND DESTROY's blast (ae_bomb_pack.c):\n"
		"1 the tank shell, 2 the super rocket, 3 the warthog's fireball, from\n"
		"the bomb pack (0: the owner's pick, 3). Any machine; only what it draws." },
	{ "debug.bomb_pickup_sound", _config_string, "\"\"", "HALO_BOMB_PICKUP_SOUND", _environment_value, /* AE hook */
		_platform_all,
		"For the owner's pick of SEARCH AND DESTROY's pickup tone\n"
		"(ae_bomb_feedback.c): a sound tag of the map's, as\n"
		"sound/sfx/ui/pickup_health; empty, the bomb pack's chirp-chirp, else\n"
		"the bomb's beeps (debug.bomb_pickup_beeps)." },
	{ "debug.bomb_pickup_beeps", _config_string, "\"\"", "HALO_BOMB_PICKUP_BEEPS", _environment_value, /* AE hook */
		_platform_all,
		"For the owner's pick of SEARCH AND DESTROY's pickup beeps\n"
		"(ae_bomb_feedback.c): the ticks after the pickup each beep plays\n"
		"(\"0 4\"), without the bomb pack's chirp-chirp; empty, the default." },
	{ "debug.weapon_log", _config_boolean, "false", "HALO_WEAPON_LOG", _environment_value, /* AE hook */
		_platform_all,
		"For automated tests of the battle rifle (ae_burst_rifle.c): each round it\n"
		"fires (its burst, the tick, the magazine), each burst's distance from the\n"
		"last, and the zoomed view's angle. Any machine; only this machine's log." },
	{ "debug.reticle_growth", _config_integer, "-1", "HALO_RETICLE_GROWTH", _environment_value, /* AE hook */
		_platform_all,
		"For the owner's tests of the AE rifles' reticle growth a shot\n"
		"(ae_reticle_growth.c): its peak, a percent of the reticle's size (0 none);\n"
		"-1, the rifle's own (the default). The HUD only: the spread is not changed." },
	{ "debug.burst_autofire", _config_integer, "0", "HALO_BURST_AUTOFIRE", _environment_value, /* AE hook */
		_platform_all,
		"For automated tests of the battle rifle's cadence: a local player holding\n"
		"it pulls the trigger by itself, 1 again as soon as the rifle allows, 2 once\n"
		"in the middle of each recovery (lost) and again half a second after it.\n"
		"Only on a host or with no network game; a client in another's game ignores\n"
		"it (logged). The pulls are the host's own, not its player's input: other\n"
		"machines in a network game do not see these bursts. 0 off." },
	{ "debug.weapon_hit_log", _config_boolean, "false", "HALO_WEAPON_HIT_LOG", _environment_value, /* AE hook */
		_platform_all,
		"For automated tests of the battle rifle's damage: each round of this\n"
		"machine's players that hits an object (head or not, shield and body\n"
		"before and after, dead). Any machine; only this machine's log. The\n"
		"numbers are a host's or a game without network: a client deals no damage." },
	{ "debug.waypoint_log", _config_boolean, "false", "HALO_WAYPOINT_LOG", _environment_value, _platform_all,
		"For the automated tests of the power items' waypoints: each second, per\n"
		"view and entry in its window, whether it is on screen, LINE OF SIGHT's\n"
		"ray, and whether it is drawn; and each item call muted at LINE OF SIGHT." },
	{ "debug.network_test_loadout", _config_string, "\"\"", "HALO_NETWORK_TEST_LOADOUT", _environment_value,
		_platform_all,
		"An automated test host's custom loadout: \"primary,secondary\" as the\n"
		"gametype editor's weapon numbers (0 none, 2 assault rifle, 3 pistol, 4\n"
		"shotgun, 5 sniper rifle, 6 rocket launcher...); empty: the gametype's." },
	{ "debug.network_test_auto_balance", _config_boolean, "false", "HALO_NETWORK_TEST_AUTO_BALANCE",
		_environment_value, _platform_all,
		"An automated test host turns its gametype's AUTO TEAM BALANCE on (a team\n"
		"game whose players all joined red, NHE EXTRAS, then starts)." },
	{ "debug.network_test_pickup_give", _config_boolean, "false", "HALO_NETWORK_TEST_PICKUP_GIVE",
		_environment_value, _platform_all,
		"With debug.network_test_pickup, an automated test host puts the weapon the\n"
		"last player stands on in its inventory two seconds on (a host pickup)." },
	{ "debug.network_test_kill_host", _config_boolean, "false", "HALO_NETWORK_TEST_KILL_HOST", _environment_value,
		_platform_all,
		"An automated test host's kill (debug.network_test_kill) kills its local player\n"
		"1 instead, by another of its local players (split screen)." },
	{ "debug.item_log", _config_boolean, "false", "HALO_ITEM_LOG", _environment_value, _platform_all,
		"For the automated tests of DROP SECONDARY: each item a unit drops (its\n"
		"owned time, the rule) and each item the 30 second purge deletes." },
	{ "debug.los_test_blink", _config_boolean, "false", "HALO_LOS_TEST_BLINK", _environment_value, _platform_all,
		"For the automated tests of LINE OF SIGHT: every other second of game time\n"
		"nothing counts as in sight, so the logs show the 0.3 s hold ending." },
	{ "debug.set_display_name", _config_string, "\"\"", "HALO_DEBUG_SET_DISPLAY_NAME", _environment_value,
		_platform_all,
		"For the automated tests of own gametypes' display names: \"STORED=DISPLAY\"\n"
		"gives the saved gametype of that stored name that display name, once at the\n"
		"start (logged). Empty: nothing. Never on a real save root." },
	{ "debug.display_name_log", _config_boolean, "false", "HALO_DISPLAY_NAME_LOG", _environment_value,
		_platform_all,
		"For the automated tests of own gametypes' display names: a log line each time a\n"
		"gametype file is read for one, and at the start of a game the name the\n"
		"lobby and scoreboard show for its gametype." },
	{ "debug.arena_test_migration", _config_integer, "0", "HALO_ARENA_TEST_MIGRATION", _environment_value,
		_platform_all,
		"For the automated tests of the seeded gametypes' migrations: 1 adds a\n"
		"value update after the last revision (AE TEAM SLY's score 50 -> 51, in\n"
		"place); 2 the same, its write failing on purpose. 0: none. Never on a\n"
		"real save root." },
	{ "debug.network_test_local_players", _config_integer, "1", "HALO_NETWORK_TEST_LOCAL_PLAYERS", _environment_value,
		_platform_all,
		"The players an automated test host has on its own machine (split screen,\n"
		"1 to 4; the last controller's added first)." },
	{ "debug.network_test_profiles", _config_boolean, "true", "HALO_NETWORK_TEST_NO_PROFILES", _environment_set_is_false,
		_platform_all,
		"An automated test host's local players play with a profile, as a lobby's\n"
		"do: player 1's last used, else the first saved; false: with none (the\n"
		"default profile's settings, players 2-4 with no profile of their own)." },
	{ "debug.capture_on_event", _config_string, "\"\"", "HALO_CAPTURE_ON_EVENT", _environment_value, /* AE hook */
		_platform_all,
		"Test hook, off while empty: a comma list of game events that each save a\n"
		"screenshot shortly after (screenshots/, a \"test capture\" line in the log):\n"
		"hud (a HUD message), start, pickup, kill, death, postgame, dead_ally (a\n"
		"DEAD ALLY MARKER first seen), hit_marker, hit_marker_kill (DEV MODE's). A\n"
		"kind may need a text: hud:Spree." },
	{ "debug.test_verdict_file", _config_string, "\"\"", "HALO_TEST_VERDICT_FILE", _environment_value, /* AE hook */
		_platform_all,
		"Test hook, off while empty: F7 (PASS) and F8 (FAIL) append a line \"<date> <time>\n"
		"<test name> PASS|FAIL\" to this file and quit the game (the batch runner then\n"
		"starts the next test); the name is debug.test_name." },
	{ "debug.test_verdict_auto", _config_string, "\"\"", "HALO_TEST_VERDICT_AUTO", _environment_value, /* AE hook */
		_platform_all,
		"With debug.test_verdict_file: \"PASS:30\" or \"FAIL:30\" writes that verdict by itself\n"
		"30 seconds after the start (the batch runner's own checks, no key to press)." },
	{ "debug.test_name", _config_string, "\"\"", "HALO_TEST_NAME", _environment_value, /* AE hook */
		_platform_all,
		"The test's name in the line debug.test_verdict_file's keys write." },
	{ "debug.scoreboard_shown", _config_boolean, "false", "HALO_DEBUG_SCOREBOARD_SHOWN", _environment_value, /* AE hook */
		_platform_all,
		"Test hook: every view's scoreboard stays shown in a game, as with BACK (Tab)\n"
		"held, for screenshots of it." },
	{ "debug.network_test_all_weapons", _config_boolean, "false", "HALO_NETWORK_TEST_ALL_WEAPONS", _environment_value, /* AE hook */
		_platform_all,
		"An automated test host runs cheat_all_weapons for its first local player a second after\n"
		"each spawn (the map's weapon list around them), and logs the list." },
	{ "debug.network_test_quit", _config_real, "0.0", "HALO_NETWORK_TEST_QUIT", _environment_value, _platform_all,
		"This many seconds into an automated test game controller 1's player\n"
		"quits, as their pause menu's QUIT does (split screen: the others stay); 0 never." },
	{ "debug.network_test_chat", _config_real, "0.0", "HALO_NETWORK_TEST_CHAT", _environment_value, _platform_all,
		"This many seconds into an automated test game each machine's player\n"
		"says a chat line to everyone, then to the team and to the vehicle (the\n"
		"host seats the last two players in one first), then the host mutes and\n"
		"unmutes a player, refuses a link and turns the chat off, and the\n"
		"clients type on the on-screen keyboard (port/linux/NETCODE.md); 0 never." },
	{ "debug.network_test_name", _config_string, "\"\"", "HALO_NETWORK_TEST_NAME", _environment_value, _platform_all,
		"The name this machine's player takes in an automated test game (ASCII);\n"
		"empty keeps its profile's." },
	{ "debug.network_test_pickup_weapon", _config_string, "\"\"", "HALO_NETWORK_TEST_PICKUP_WEAPON", _environment_value,
		_platform_all,
		"The weapon network_test_pickup stands the player on: the first whose tag\n"
		"name has this in it (\"sniper\", say); empty any." },
	{ "debug.network_test_cues", _config_string, "\"\"", "HALO_NETWORK_TEST_CUES", _environment_value, _platform_all,
		"Console lines an automated test runs at a game second, from the first tick\n"
		"every player of debug.network_test_cue_players has a unit:\n"
		"\"<second> <line>|<second> <line>...\" (the host's, or a game of one machine;\n"
		"not on the web)." },
	{ "debug.network_test_cue_players", _config_integer, "1", "HALO_NETWORK_TEST_CUE_PLAYERS", _environment_value,
		_platform_all,
		"The players debug.network_test_cues waits for." },
	{ "debug.network_test_public", _config_boolean, "false", "HALO_NETWORK_TEST_PUBLIC", _environment_value,
		_platform_all,
		"The network test's host lists its game in the server browser, as Create\n"
		"Game > Internet's PUBLIC does (debug.network_test \"browse\" joins it)." },
	{ "debug.network_bots", _config_boolean, "true", "HALO_NETWORK_BOTS", _environment_value, _platform_all, /* AE hook */
		"The host's bots (bots.count, up to 127) in every network game it hosts,\n"
		"LAN and internet: past its three free controllers they sit on machines of\n"
		"the host's own (four each), \"BOT \" before their names; a team game whose\n"
		"players are odd gets one more at the start, for even teams. Off: up to 3,\n"
		"in LAN games only (0.3.6). halo.log has their brain's time a minute." },
	{ "debug.telnet_console", _config_boolean, "false", "HALO_TELNET_CONSOLE", _environment_set_is_true, _platform_all,
		"A console on 127.0.0.1 (telnet_console_port), with no password." },
	{ "debug.telnet_console_port", _config_integer, "2323", "HALO_TELNET_CONSOLE_PORT", _environment_value,
		_platform_all,
		"The telnet console's port." },
	{ "debug.log_addresses", _config_boolean, "false", "HALO_LOG_ADDRESSES", _environment_set_is_true, _platform_all,
		"Write full internet addresses in debug.txt and the console. Do not post\n"
		"a debug.txt written with this on." },
	{ "debug.touch_targets", _config_boolean, "false", "HALO_TOUCH_TARGETS", _environment_set_is_true, _platform_all,
		"Outline the menus' tap targets and log each tap." },
	{ "debug.solo_game", _config_boolean, "false", "HALO_SOLO_GAME", _environment_set_is_true, _platform_all,
		"Let a system link or split screen game start with one player." },
	{ "debug.network_latency", _config_real, "0.0", "HALO_NETWORK_LATENCY", _environment_value, _platform_all,
		"Milliseconds to delay everything received; 0 none." },
	{ "debug.network_loss", _config_real, "0.0", "HALO_NETWORK_LOSS", _environment_value, _platform_all,
		"Percent of received datagrams to drop; 0 none." },
	{ "debug.network_corrupt", _config_real, "0.0", "HALO_NETWORK_CORRUPT", _environment_value, _platform_all,
		"Percent of received datagrams to damage; 0 none." },
	{ "debug.network_corrupt_stream", _config_real, "0.0", "HALO_NETWORK_CORRUPT_STREAM", _environment_value,
		_platform_all,
		"Percent of stream reads to damage; 0 none." },
	{ "debug.network_corrupt_after", _config_real, "0.0", "HALO_NETWORK_CORRUPT_AFTER", _environment_value,
		_platform_all,
		"Seconds after the start before anything is damaged." },
	{ "debug.test_input", _config_string, "\"\"", "HALO_TEST_INPUT", _environment_value, _platform_all,
		"\"bot:<seed>\" plays controller 1 with a scripted pattern (automated\n"
		"network tests); \"look:<seed>\" stands still, only turning and looking\n"
		"up and down; empty for none." },
	{ "debug.bot_trace", _config_integer, "0", "HALO_BOT_TRACE", _environment_value, _platform_all,
		"Log each bot's state (bots.count) every this many seconds: where it is\n"
		"and goes, its path, target and weapon; 0 never." },
	{ "debug.bot_vehicles", _config_integer, "1", "HALO_BOT_VEHICLES", _environment_value, _platform_all, /* AE hook */
		"Bots and vehicles: 0 they leave them alone, 1 they take a Warthog's or a\n"
		"turret's gun and ride with a teammate (the default), 2 they also drive\n"
		"a Warthog or Ghost (a test: not good enough to play yet)." },
#ifndef HALO_CONSOLE_NO_UPDATER
	{ "debug.update_answer", _config_string, "\"\"", "HALO_UPDATE_ANSWER", _environment_value, _platform_desktop,
		"The answer to the update question, for tests: \"yes\", \"no\" or \"never\";\n"
		"empty asks." },
#endif
	{ "debug.fixed_step", _config_boolean, "false", "HALO_FIXED_STEP", _environment_set_is_true, _platform_all,
		"A game of this machine alone runs one tick a frame, whatever time has\n"
		"passed, so that it plays the same every time (tests); game_speed has no\n"
		"effect." },
	{ "debug.chat_test", _config_string, "\"\"", "HALO_CHAT_TEST", _environment_value, _platform_all,
		"Scripted chat for tests: steps of\n"
		"\"<seconds>:<all|team|server|mute|unmute|localmute|log|close|type>[:<text>]\"\n"
		"separated by ;. Empty: none." },
	{ "debug.dev_camera", _config_string, "\"\"", "HALO_DEV_CAMERA", _environment_value, _platform_all, /* AE hook */
		"For tests, with system.dev_mode: the dev camera on from the start, as\n"
		"controls.dev_camera (\"third\"), it and controls.dev_shoulder\n"
		"(\"third_left\") or controls.dev_free_camera (\"free\") would; empty none." },
	{ "debug.camera_black_switch", _config_boolean, "false", "HALO_CAMERA_BLACK_SWITCH", _environment_value, _platform_all, /* AE hook */
		"The Xbox's debug camera switch: holding a gamepad's black button (RB) a\n"
		"second turns a host's game camera (first person, following, flying).\n"
		"Off, RB held does nothing to the camera (DEV MODE's cameras have their\n"
		"own keys and chords)." },
	{ "debug.exit_after", _config_real, "0.0", "HALO_EXIT_AFTER", _environment_value, _platform_all,
		"Quit this many seconds after the window opens (without a window, after\n"
		"the first frame); 0 never." },
	{ "debug.hidden_window", _config_boolean, "false", "HALO_HIDDEN_WINDOW", _environment_set_is_true, _platform_desktop,
		"Keep the window hidden (and never fullscreen)." },
	{ "debug.voice_test", _config_boolean, "false", "HALO_VOICE_TEST", _environment_set_is_true, _platform_all,
		"Voice chat's automated tests: a tone instead of the microphone, and\n"
		"each voice heard logged once a second." },
	{ "debug.null_renderer", _config_boolean, "false", "HALO_NULL_RENDERER", _environment_set_is_true, _platform_all,
		"Run without a window, drawing nothing." },
	{ "debug.gl_debug", _config_boolean, "false", "HALO_GL_DEBUG", _environment_set_is_true, _platform_all,
		"Report OpenGL errors in the log." },
	{ "debug.menu_open", _config_string, "\"\"", "HALO_MENU_OPEN", _environment_value, _platform_all,
		"Start on this screen of the menus (port/assets/menus) instead of the main\n"
		"menu, a player profile being edited; empty for the main menu." },
	{ "debug.ae_test_screen", _config_integer, "0", "HALO_AE_TEST_SCREEN", _environment_value, /* AE hook */
		_platform_all,
		"With display.arena_menus: open Arena Evolved's debug screens at the main\n"
		"menu (for the automated tests). 0: none; 1, 2 or 4: the test screen over\n"
		"the whole screen or drawn into that many split-screen views; 9: the test\n"
		"screen with the game's menus closed; 11 to 15: the widget gallery's page\n"
		"1 to 5; 21, 23: the gallery in 2 views (pages 1-2, 3-4); 41: the gallery\n"
		"in 4 views (pages 1-4); 90: host a LAN game (with bots); 91: join the\n"
		"first LAN game, then leave; 92: host a LAN game for one AE player, end it,\n"
		"back; 93: host a local game; 94: the profiles; 95: host an internet\n"
		"game." },
	{ "debug.ae_lobby_countdown_ms", _config_integer, "5999", "HALO_AE_LOBBY_COUNTDOWN_MS", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved's CREATE GAME: the countdown START GAME runs before the\n"
		"game starts, in milliseconds (5999: 5, 4, 3, 2, 1, then the start); 0\n"
		"starts at once (for the automated tests)." },
	{ "debug.test_run", _config_string, "\"\"", "HALO_TEST_RUN", _environment_value, _platform_all, /* AE hook */
		"TEST RUN (with system.dev_mode): start this list (test-runs/<id>.txt in the data folder) once the main\n"
		"menu has loaded, and play its pending tests (port/linux/game/ae_test_run.h)." },
	{ "debug.test_run_auto", _config_string, "\"\"", "HALO_TEST_RUN_AUTO", _environment_value, _platform_all, /* AE hook */
		"TEST RUN, for the automated tests: the verdicts to give, each after this many seconds with the\n"
		"test ready, cycling, e.g. \"PASS:6,FAIL:6,NOTE:6,SKIP:6\"; a note is typed as \"auto\"." },
	{ "debug.gpu_flush_draws", _config_integer, "-1", "HALO_GPU_FLUSH_DRAWS", _environment_value, _platform_desktop,
		"Flush the GPU every this many draws: -1 for every 3 on Intel with Mesa,\n"
		"0 never." },
	{ "debug.gpu_stats", _config_boolean, "false", "HALO_GPU_STATS", _environment_set_is_true, _platform_all,
		"Log the renderer's draw counts once a second." },
	{ "debug.gpu_trace_frame", _config_integer, "-1", "HALO_GPU_TRACE", _environment_value, _platform_all,
		"Log every draw of this frame; -1 none." },
	{ "debug.gpu_trace_constants", _config_boolean, "false", "HALO_GPU_TRACE_CONSTANTS", _environment_set_is_true, _platform_all,
		"With gpu_trace_frame, also the vertex shader constants." },
	{ "debug.gpu_skip_vertex_shaders", _config_string, "\"\"", "HALO_GPU_SKIP_VS", _environment_value, _platform_all,
		"Comma-separated ids of vertex shaders not to draw with." },
	{ "debug.gpu_dump_shaders", _config_string, "\"\"", "HALO_GPU_DUMP_SHADERS", _environment_value, _platform_all,
		"A folder to write the generated GLSL to; empty none." },
	{ "debug.gpu_debug_expression", _config_string, "\"\"", "HALO_GPU_DEBUG_EXPR", _environment_value, _platform_all,
		"A GLSL expression every pixel shader shows instead of its result." },
	{ "debug.gpu_debug_texture0", _config_boolean, "false", "HALO_GPU_DEBUG_T0", _environment_set_is_true, _platform_all,
		"Pixel shaders show their first texture." },
	{ "debug.gpu_debug_flat", _config_boolean, "false", "HALO_GPU_DEBUG_FLAT", _environment_set_is_true, _platform_all,
		"Pixel shaders show their vertex colour." },
	{ "debug.screenshot_directory", _config_string, "\"\"", "HALO_SCREENSHOT_DIR", _environment_value, _platform_all,
		"A folder to save frames to (with screenshot_every); empty none." },
	{ "debug.screenshot_every", _config_integer, "0", "HALO_SCREENSHOT_EVERY", _environment_value, _platform_all,
		"Save every this many frames to screenshot_directory; 0 none." },
	{ "debug.screenshot_format", _config_string, "\"bmp\"", "HALO_SCREENSHOT_FORMAT", _environment_value,
		_platform_all,
		"The screenshot_every frames' format: \"bmp\" or \"png\"." },
	{ "debug.record_seconds", _config_real, "0.0", "HALO_RECORD_SECONDS", _environment_value, _platform_desktop,
		"Record this many seconds (as F9 does) from the first frame of play\n"
		"(a map other than the main menu's), once; 0 never." },
	{ "debug.screenshot_window", _config_boolean, "false", "HALO_SCREENSHOT_WINDOW", _environment_set_is_true,
		_platform_all,
		"With screenshot_every, save the window as shown instead of the game's\n"
		"frame." },
	{ "debug.texture_dump_directory", _config_string, "\"\"", "HALO_TEXTURE_DUMP", _environment_value, _platform_all,
		"A folder to write every texture to as it is uploaded; empty none." },
#ifndef HALO_CONSOLE_NO_DISCORD
	{ "debug.discord_log", _config_boolean, "false", "HALO_DISCORD_LOG", _environment_set_is_true,
		_platform_desktop,
		"Log each activity sent to Discord." },
#endif
	{ "debug.texture_log", _config_boolean, "false", "HALO_TEXTURE_LOG", _environment_set_is_true, _platform_all,
		"Log texture uploads." },
	{ "debug.texture_no_cache", _config_boolean, "false", "HALO_TEXTURE_NO_CACHE", _environment_set_is_true, _platform_all,
		"Upload textures again every time they are used." },
	{ "debug.sample_seconds", _config_real, "0.0", "HALO_SAMPLE", _environment_value, _platform_android,
		"Log where every game thread is this often, in seconds (read by the\n"
		"app, port/android/host/host_debug.c); 0 never." },
	{ "debug.memory_watch", _config_boolean, "true", NULL, _environment_value, _platform_android,
		"Notice the game's writes to cached textures and vertices by page\n"
		"protection; false compares page contents once a frame instead, which is\n"
		"slower. Under ARM translation (the x86 emulator) the app always compares\n"
		"contents. Read by the app from the file (port/android/host/host_main.c)." },
#ifdef HALO_PROFILE
	{ "debug.profile_record", _config_boolean, "false", "HALO_PROFILE_RECORD", _environment_value, _platform_all,
		"Record a profile with no command (configure.py --profile builds): from\n"
		"when profile_record_when says until profile_stop, a map change or the\n"
		"end, into numbered part files in the data folder's profiles folder\n"
		"(tools/net_report.py reads it)." },
	{ "debug.profile_record_when", _config_string, "\"start\"", "HALO_PROFILE_RECORD_WHEN", _environment_value,
		_platform_all,
		"\"start\": from the first frame until the first map change; \"game\":\n"
		"each game that is not the main menu, a recording each, until its map\n"
		"goes." },
	{ "debug.profile_memory", _config_integer, "256", "HALO_PROFILE_MEMORY", _environment_value, _platform_all,
		"Megabytes a recording keeps in memory, 4 to 1024: two halves, each\n"
		"written out as a part when it fills." },
	{ "debug.audio_capture_seconds", _config_real, "0.0", "HALO_AUDIO_CAPTURE_SECONDS", _environment_value,
		_platform_all | _platform_xbox,
		"Record this many seconds of the sound of each game played (solo,\n"
		"system link or online), with a log of the player's position and every\n"
		"sound started, to d:\\audio_probe\\capture-<map>-<n>-* (that folder must\n"
		"be there; port/linux/game/audio_probe.c); 0 none." },
	{ "debug.audio_probe", _config_string, "\"\"", "HALO_AUDIO_PROBE", _environment_value, _platform_all | _platform_xbox,
		"A probe file (port/linux/game/audio_probe.c), named as the game names\n"
		"its files (d:\\ is the game's folder: on the desktop, the data folder):\n"
		"the game loads its map, plays its sounds at its spots, records what it\n"
		"plays beside the file (to compare with tools/audio_probe_compare.py),\n"
		"and quits. Empty: none." },
#endif
	{ "console.debug_geometry", _config_boolean, "false", NULL, _environment_value, _platform_xbox,
		"For development: the debug build's debug geometry (lines and triangles\n"
		"drawn by debug commands), whose buffers take 1440 KB. Off, as the retail\n"
		"game had none; a stock 64 MB console may not have the room." },
	{ "console.delta", _config_string, "\"auto\"", NULL, _environment_value, _platform_xbox,
		"Delta Peer: when joining a game whose host offers it, tell the host\n"
		"this is an Xbox (its platform, memory and nothing else), so the stats\n"
		"site, the scoreboard and the server show Xbox. \"auto\": on; \"off\" for\n"
		"the legacy protocol alone. The game is the same either way." },
	{ "console.log_flush", _config_string, "\"buffered\"", NULL, _environment_value, _platform_xbox,
		"When debug.txt is written. \"buffered\": the game's lines wait in memory\n"
		"and are written together, once a second, when 8 KB are waiting, and\n"
		"at once for an assert, an exit or a crash. \"every_line\": each line is\n"
		"written as it comes (slow, a file opened for each), for a crash hunt\n"
		"where even the last line before the console stops must be there." },
};

#define NUMBER_OF_CONFIG_SETTINGS (sizeof(config_settings) / sizeof(config_settings[0]))

#ifdef HALO_ANDROID
#define CONFIG_BUILD_PLATFORM _platform_android
#elif defined(HALO_WEB)
#define CONFIG_BUILD_PLATFORM (_platform_desktop | _platform_web)
#elif defined(_WIN32)
#define CONFIG_BUILD_PLATFORM (_platform_desktop | _platform_windows)
#else
#define CONFIG_BUILD_PLATFORM _platform_desktop
#endif

/* a dedicated server (server.enabled, --server, HALO_SERVER: decided as the
file is read): its settings are written into the file too */
static int config_server_mode;
/* a run that only answers its command line (--version...): nothing written */
static int config_answer_only;
/* halo --probe [<invite>]: a list site's look at a game (network_test.c's
"probe"), headless */
static int config_probe_mode;
#define CONFIG_PLATFORM (CONFIG_BUILD_PLATFORM | (config_server_mode ? _platform_server : 0))

struct config_value
{
	int boolean;
	long integer;
	double real;
	char *string;
	/* set by the file or the environment, not the default (config_carry_over) */
	int chosen;
};

static struct config_value config_values[NUMBER_OF_CONFIG_SETTINGS];
/* whether the file or the environment gives the setting a value */
static int config_given[NUMBER_OF_CONFIG_SETTINGS];
static __thread struct config_value ae_player_value; /* AE hook */
/* the settings an existing file would see change with this version's
defaults, until the player is asked (config_find_new_defaults): 1 for one
that follows the default (commented out, or not there), 2 for one that an
older version wrote as a value beside the commented one; and the old
default each had, as written */
static int config_pending[NUMBER_OF_CONFIG_SETTINGS];
static const char *config_pending_old[NUMBER_OF_CONFIG_SETTINGS];
static int config_loaded = 0;
static pthread_mutex_t config_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- the command line */

#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
/* whether the command line has the argument name ("--server"), and with
value, the one after it (or after "=": --config=<path>) */
static int config_argument(const char *name, char *value, size_t size)
{
	char text[1024];
	size_t length = strlen(name);
	int index;

	for (index = 1; posix_command_line_argument(index, text, (posix_ulong)sizeof(text)); index++)
	{
		if (!strcmp(text, name))
			return !value || posix_command_line_argument(index + 1, value, (posix_ulong)size);
		if (value && !strncmp(text, name, length) && text[length] == '=')
		{
			snprintf(value, size, "%s", text + length + 1);
			return 1;
		}
	}
	return 0;
}
#endif

/* ---------- the file */

int platform_app_folder(char *path, unsigned long size)
{
#if defined(__APPLE__)
	const char *base = SDL_GetBasePath();
	const char *home = getenv("HOME");

	if (!base || !strstr(base, ".app/Contents/") || !home || !*home)
		return 0;
	snprintf(path, (size_t)size, "%s/Library/Application Support/ChupathingyCE", home);
	SDL_CreateDirectory(path);
	return 1;
#elif defined(__linux__) && !defined(HALO_ANDROID)
	/* an AppImage (which sets APPIMAGE) runs from its own file, mounted
	read-only: its data and settings go in the save root's usual place
	(Arena Evolved's, halo-linux: xbox_files.c), or a test's HALO_SAVE_ROOT */
	const char *appimage = getenv("APPIMAGE");
	const char *save_root = getenv("HALO_SAVE_ROOT");
	const char *data_home = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");

	if (!appimage || !*appimage)
		return 0;
	if (save_root && *save_root)
		snprintf(path, (size_t)size, "%s", save_root);
	else if (data_home && *data_home)
		snprintf(path, (size_t)size, "%s/halo-linux", data_home);
	else if (home && *home)
		snprintf(path, (size_t)size, "%s/.local/share/halo-linux", home);
	else
		return 0;
	SDL_CreateDirectory(path);
	return 1;
#else
	(void)path;
	(void)size;
	return 0;
#endif
}

static void config_path(char *path, size_t size)
{
#ifndef HALO_ANDROID
	char folder[1024];
#endif
#ifdef __APPLE__
	const char *save_root = getenv("HALO_SAVE_ROOT");

	/* (a copy given a save root of its own, a test's, keeps its settings
	there too, not in the player's) */
	if (save_root && *save_root)
	{
		snprintf(path, size, "%s/config.toml", save_root);
		return;
	}
#endif
#ifndef HALO_ANDROID
	/* (the application's folder: not the application, which is signed, or
	the AppImage, which cannot be written) */
	if (platform_app_folder(folder, sizeof(folder)))
	{
		snprintf(path, size, "%s/config.toml", folder);
		return;
	}
#endif
#if defined(HALO_ANDROID) || defined(HALO_WEB)
	/* the data folder, which the app names (port/android/host/host_main.c;
	on the web, port/web/src/web_main.c) */
	const char *root = getenv("HALO_DATA_ROOT");

	snprintf(path, size, "%s/config.toml", root && *root ? root : ".");
#else
	/* HALO_CONFIG_FILE or --config <path> (a dedicated server's instance's
	own), else the executable's folder's, with its separator */
	const char *file = getenv("HALO_CONFIG_FILE");
	const char *base;

	if (file && *file)
	{
		snprintf(path, size, "%s", file);
		return;
	}
	if (config_argument("--config", path, size) && *path)
		return;
	base = SDL_GetBasePath();
	snprintf(path, size, "%sconfig.toml", base ? base : "");
#endif
}

/* the whole file, NUL terminated, or NULL; free() it */
static char *config_read_file(const char *path, size_t *size)
{
#ifdef HALO_ARM64_GUEST
	FILE *file = fopen(path, "rb");
	char *text = NULL;
	long length;

	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) >= 0 && fseek(file, 0, SEEK_SET) == 0)
	{
		text = malloc((size_t)length + 1);
		if (text && fread(text, 1, (size_t)length, file) == (size_t)length)
		{
			text[length] = 0;
			*size = (size_t)length;
		}
		else
		{
			free(text);
			text = NULL;
		}
	}
	fclose(file);
	return text;
#else
	/* SDL's, for UTF-8 paths on Windows */
	void *data = SDL_LoadFile(path, size);
	char *text;

	if (!data)
		return NULL;
	text = malloc(*size + 1);
	if (text)
	{
		memcpy(text, data, *size);
		text[*size] = 0;
	}
	SDL_free(data);
	return text;
#endif
}

static int config_write_file(const char *path, const char *text)
{
#ifdef HALO_ARM64_GUEST
	FILE *file = fopen(path, "wb");
	int written;

	if (!file)
		return 0;
	written = fwrite(text, 1, strlen(text), file) == strlen(text);
	return fclose(file) == 0 && written;
#elif !defined(HALO_ANDROID) && !defined(HALO_WEB)
	/* (a dedicated server's holds its passwords: its user's to read alone) */
	if (!SDL_SaveFile(path, text, strlen(text)))
		return 0;
	if (config_server_mode)
		posix_server_private_file(path);
	return 1;
#else
	return SDL_SaveFile(path, text, strlen(text));
#endif
}

/* the file's layout: 2 writes a setting at its default commented out (1,
before config_version, wrote every value, which kept a default a later
version changed) */
#define CONFIG_VERSION 2
/* the defaults the game has asked about (game.defaults_answered): the ones
of v0.8.0 */
#define CONFIG_DEFAULTS_GENERATION 1
#define CONFIG_VERSION_LINE "config_version = 2\n"
#define CONFIG_VERSION_LINES "# The layout of this file, for the game: leave it.\n" CONFIG_VERSION_LINE
#define CONFIG_VERSION_TEXT "\n" CONFIG_VERSION_LINES

struct config_text
{
	char *buffer;
	size_t length, capacity;
};

static void config_append(struct config_text *text, const char *string)
{
	size_t length = strlen(string);

	if (text->length + length + 1 > text->capacity)
	{
		size_t capacity = (text->capacity ? text->capacity : 4096) * 2 + length;
		char *buffer = realloc(text->buffer, capacity);

		if (!buffer)
			return;
		text->buffer = buffer;
		text->capacity = capacity;
	}
	memcpy(text->buffer + text->length, string, length + 1);
	text->length += length;
}

/* the first length characters of text, as a string of their own */
static char *config_copy(const char *text, size_t length)
{
	char *copy = malloc(length + 1);

	if (copy)
	{
		memcpy(copy, text, length);
		copy[length] = 0;
	}
	return copy;
}

/* the line's key, if it is "key = ..." (after spaces), in key */
static int config_line_key(const char *line, const char *end, const char *key)
{
	size_t length = strlen(key);

	while (line < end && (*line == ' ' || *line == '\t'))
		line++;
	if ((size_t)(end - line) <= length || strncmp(line, key, length) != 0)
		return 0;
	line += length;
	while (line < end && (*line == ' ' || *line == '\t'))
		line++;
	return line < end && *line == '=';
}

/* the section the line opens, if it is "[section]" (after spaces) */
static int config_line_section(const char *line, const char *end, char *section, size_t size)
{
	const char *close;

	while (line < end && (*line == ' ' || *line == '\t'))
		line++;
	if (line >= end || *line != '[')
		return 0;
	close = memchr(line, ']', (size_t)(end - line));
	if (!close || (size_t)(close - line - 1) >= size)
		return 0;
	memcpy(section, line + 1, (size_t)(close - line - 1));
	section[close - line - 1] = 0;
	return 1;
}

/* the line's key, if it is a setting written at its default, "# key = ..."
(config_append_setting) */
static int config_line_commented_key(const char *line, const char *end, const char *key)
{
	while (line < end && (*line == ' ' || *line == '\t'))
		line++;
	return line < end && *line == '#' && config_line_key(line + 1, end, key);
}

/* one setting as the file holds it: its comment, and its key at the
default, commented out, so that it follows the default of whichever version
reads the file until the player chooses a value (config_write) */
static void config_append_setting(struct config_text *text, const struct config_setting *setting)
{
	const char *dot = strchr(setting->name, '.');
	const char *line;
	char buffer[256];

	config_append(text, "\n");
	for (line = setting->comment; *line;)
	{
		size_t length = strcspn(line, "\n");

		snprintf(buffer, sizeof(buffer), "# %.*s\n", (int)length, line);
		config_append(text, buffer);
		line += length;
		if (*line)
			line++;
	}
#ifndef HALO_ANDROID
	/* (Android apps have no environment to set) */
	switch (setting->environment_style)
	{
	case _environment_value:
		snprintf(buffer, sizeof(buffer), "# (for one run: %s=<value>)\n", setting->environment);
		break;
	case _environment_set_is_true:
		snprintf(buffer, sizeof(buffer), "# (for one run: %s=1 makes it true)\n", setting->environment);
		break;
	case _environment_set_is_false:
		snprintf(buffer, sizeof(buffer), "# (for one run: %s=1 makes it false)\n", setting->environment);
		break;
	}
	config_append(text, buffer);
#endif
	snprintf(buffer, sizeof(buffer), "# %s = %s\n", dot + 1, setting->default_value);
	config_append(text, buffer);
}

/* the file with every setting of this build at its default */
static char *config_default_text(void)
{
	struct config_text text = { NULL, 0, 0 };
	char section[32] = "";
	size_t index;

#ifdef HALO_ANDROID
	config_append(&text,
		"# Halo settings\n"
		"#\n"
		"# The game writes this file with the defaults when it is missing. A\n"
		"# setting at its default is commented out (\"# key = value\") and follows\n"
		"# the default of the version that runs: take out the \"# \" and change the\n"
		"# value to choose another. A line starting with # does nothing. The game\n"
		"# reads this file when it starts: restart it after an edit. Delete the\n"
		"# file to go back to the defaults.\n"
		CONFIG_VERSION_TEXT);
#else
	config_append(&text,
		"# Halo settings\n"
		"#\n"
		"# The game writes this file with the defaults when it is missing. A\n"
		"# setting at its default is commented out (\"# key = value\") and follows\n"
		"# the default of the version that runs: take out the \"# \" and change the\n"
		"# value to choose another. A line starting with # does nothing. The game\n"
		"# reads this file when it starts: restart it after an edit. Delete the\n"
		"# file to go back to the defaults.\n"
		"# Each setting can also be set for one run with the environment variable\n"
		"# named with it, which wins over this file.\n"
		CONFIG_VERSION_TEXT);
#endif
	/* each section once, where its first setting is, with all of its
	settings: TOML refuses a section twice, and the whole file would go
	unread (config_join_sections) */
	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const struct config_setting *setting = &config_settings[index];
		const char *dot = strchr(setting->name, '.');
		size_t length, other;
		char buffer[64];

		if (!(setting->platforms & CONFIG_PLATFORM) || !dot)
			continue;
		length = (size_t)(dot - setting->name);
		for (other = 0; other < index; other++)
		{
			if ((config_settings[other].platforms & CONFIG_PLATFORM) &&
				!strncmp(config_settings[other].name, setting->name, length + 1))
				break;
		}
		if (other < index)
			continue;
		snprintf(section, sizeof(section), "%.*s", (int)length, setting->name);
		snprintf(buffer, sizeof(buffer), "\n[%s]\n", section);
		config_append(&text, buffer);
		for (other = index; other < NUMBER_OF_CONFIG_SETTINGS; other++)
		{
			if ((config_settings[other].platforms & CONFIG_PLATFORM) &&
				!strncmp(config_settings[other].name, setting->name, length + 1))
				config_append_setting(&text, &config_settings[other]);
		}
	}
	return text.buffer;
}

/* whether text has the setting's line in its section: as a value, or (with
commented) also commented out at its default */
static int config_text_has_key(const char *text, const char *name, int commented)
{
	const char *dot = strchr(name, '.');
	char section[64], current[64] = "";
	const char *line;

	snprintf(section, sizeof(section), "%.*s", (int)(dot - name), name);
	for (line = text; *line;)
	{
		const char *end = line + strcspn(line, "\n");

		if (!config_line_section(line, end, current, sizeof(current)) && !strcmp(current, section) &&
			(config_line_key(line, end, dot + 1) || (commented && config_line_commented_key(line, end, dot + 1))))
		{
			return 1;
		}
		line = *end ? end + 1 : end;
	}
	return 0;
}

/* the settings of this build that text (the file, parsed as table) lacks,
neither a value nor commented out at a default, added to it in their
sections (commented out), keeping the rest as it is: a newer version's
settings appear in an older file. Returns the new text, or NULL if nothing
was missing */
static char *config_add_missing(const char *text, toml_datum_t table)
{
	char *result = NULL;
	size_t index;

	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const struct config_setting *setting = &config_settings[index];
		const char *dot = strchr(setting->name, '.');
		const char *current = result ? result : text;
		struct config_text block = { NULL, 0, 0 };
		struct config_text updated = { NULL, 0, 0 };
		char header[40];
		const char *line;
		const char *insert = NULL;

		if (!(setting->platforms & CONFIG_PLATFORM) || !dot || toml_seek(table, setting->name).type != TOML_UNKNOWN ||
			config_pending[index] || config_text_has_key(current, setting->name, 1))
			continue;
		snprintf(header, sizeof(header), "[%.*s]", (int)(dot - setting->name), setting->name);
		/* the end of the section's last line that is not blank */
		for (line = current; *line; )
		{
			const char *start = line;
			size_t length = strcspn(line, "\n");

			while (*start == ' ' || *start == '\t')
				start++;
			if (insert && *start == '[')
				break;
			if (!insert && !strncmp(start, header, strlen(header)))
				insert = line + length;
			else if (insert && start < line + length && *start != '\r')
				insert = line + length;
			line += length;
			if (*line)
				line++;
		}
		if (insert)
		{
			if (*insert)
				insert++;
			config_append_setting(&block, setting);
		}
		else
		{
			/* no such section: a new one at the end */
			insert = current + strlen(current);
			config_append(&block, current[0] && insert[-1] != '\n' ? "\n\n" : "\n");
			config_append(&block, header);
			config_append(&block, "\n");
			config_append_setting(&block, setting);
		}
		if (!block.buffer)
			continue;
		{
			char *before = config_copy(current, (size_t)(insert - current));

			if (before)
				config_append(&updated, before);
			free(before);
		}
		if (insert > current && insert[-1] != '\n')
			config_append(&updated, "\n");
		config_append(&updated, block.buffer);
		config_append(&updated, insert);
		free(block.buffer);
		if (updated.buffer)
		{
			free(result);
			result = updated.buffer;
			platform_log("settings: added %s (new in this version) at its default", setting->name);
		}
	}
	return result;
}

static int config_line_section(const char *line, const char *end, char *section, size_t size);

/* text from start to end, appended */
static void config_append_range(struct config_text *text, const char *start, const char *end)
{
	char *copy = config_copy(start, (size_t)(end - start));

	if (copy)
		config_append(text, copy);
	free(copy);
}

/* a file with a section twice, which TOML refuses (the defaults of the
versions from 2026-10-10 had a second [display] after [game]): each
section's later parts moved to the end of its first, the rest kept as it
is. Returns the new text, or NULL if no section is there twice */
static char *config_join_sections(const char *text)
{
	enum { PARTS_MAXIMUM = 64 };
	struct
	{
		char name[64];
		const char *start, *end;
	} parts[PARTS_MAXIMUM];
	struct config_text joined = { NULL, 0, 0 };
	const char *line;
	const char *first_part = NULL;
	int count = 0, repeated = 0, index, other;

	/* the parts, each from its section's line to the next one's */
	for (line = text; *line; )
	{
		size_t length = strcspn(line, "\n");
		char name[64];

		if (config_line_section(line, line + length, name, sizeof(name)))
		{
			if (count == PARTS_MAXIMUM)
				return NULL;
			if (count)
				parts[count - 1].end = line;
			else
				first_part = line;
			for (other = 0; other < count; other++)
				repeated |= !strcmp(parts[other].name, name);
			strcpy(parts[count].name, name);
			parts[count].start = line;
			count++;
		}
		line += length;
		if (*line)
			line++;
	}
	if (!repeated)
		return NULL;
	parts[count - 1].end = line;

	config_append_range(&joined, text, first_part);
	for (index = 0; index < count; index++)
	{
		for (other = 0; other < index && strcmp(parts[other].name, parts[index].name); other++)
			;
		if (other < index)
			continue;
		config_append_range(&joined, parts[index].start, parts[index].end);
		for (other = index + 1; other < count; other++)
		{
			const char *body;

			if (strcmp(parts[other].name, parts[index].name))
				continue;
			/* (its settings, without its section's line) */
			body = memchr(parts[other].start, '\n', (size_t)(parts[other].end - parts[other].start));
			if (!body)
				continue;
			if (joined.length && joined.buffer[joined.length - 1] != '\n')
				config_append(&joined, "\n");
			config_append_range(&joined, body + 1, parts[other].end);
		}
	}
	return joined.buffer;
}

/* ---------- values */

static int config_text_is_false(const char *text)
{
	char lower[8];
	size_t index;

	for (index = 0; index + 1 < sizeof(lower) && text[index]; index++)
		lower[index] = (char)tolower((unsigned char)text[index]);
	lower[index] = 0;
	return !strcmp(lower, "0") || !strcmp(lower, "false") || !strcmp(lower, "no") || !strcmp(lower, "off");
}

/* whether a variable that only has to be set (_environment_set_is_true or
_environment_set_is_false) is: empty, "0", "false", "no" or "off" is not */
static int config_environment_set(const char *text)
{
	return text[0] && !config_text_is_false(text);
}

static void config_set_from_text(struct config_value *value, enum config_type type, const char *text)
{
	switch (type)
	{
	case _config_boolean:
		value->boolean = !config_text_is_false(text);
		break;
	case _config_integer:
		value->integer = strtol(text, NULL, 10);
		break;
	case _config_real:
		value->real = strtod(text, NULL);
		break;
	case _config_string:
		/* (the old string is kept, not freed: config_string's callers hold
		its pointer, and Settings writes few, seldom) */
		value->string = strdup(text);
		break;
	}
}

static int config_was_boolean(const char *name);

/* the value in the file, if it is there and of the setting's type */
static void config_set_from_file(struct config_value *value, const struct config_setting *setting,
	toml_datum_t table)
{
	toml_datum_t datum = toml_seek(table, setting->name);
	int wrong_type = 0;

	if (datum.type == TOML_UNKNOWN)
		return;
	switch (setting->type)
	{
	case _config_boolean:
		if (datum.type == TOML_BOOLEAN)
			value->boolean = datum.u.boolean;
		else
			wrong_type = 1;
		break;
	case _config_integer:
		if (datum.type == TOML_INT64)
			value->integer = (long)datum.u.int64;
		else
			wrong_type = 1;
		break;
	case _config_real:
		if (datum.type == TOML_FP64)
			value->real = datum.u.fp64;
		else if (datum.type == TOML_INT64)
			value->real = (double)datum.u.int64;
		else
			wrong_type = 1;
		break;
	case _config_string:
		if (datum.type == TOML_STRING)
		{
			free(value->string);
			value->string = strdup(datum.u.s);
		}
		/* (true or false: an earlier version's string setting that was true or
		false before that, read as the text: config_carry_over; or a word
		setting that once was true or false: config_old_words) */
		else if (datum.type == TOML_BOOLEAN &&
			(setting->platforms == _platform_none || config_was_boolean(setting->name)))
		{
			free(value->string);
			value->string = strdup(datum.u.boolean ? "true" : "false");
		}
		else
		{
			wrong_type = 1;
		}
		break;
	}
	if (wrong_type)
	{
		static const char *const expected[] = { "true or false", "a whole number", "a number", "a quoted string" };

		platform_log("config.toml line %d: %s should be %s; using %s", datum.lineno, setting->name,
			expected[setting->type], setting->default_value);
	}
	else
	{
		value->chosen = 1;
	}
}

/* how many times a setting has been written (config_write): readers that
keep a setting watch this, to read it again */
static volatile unsigned long config_change_count;

static long config_setting_index(const char *name)
{
	size_t index;

	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		if (!strcmp(config_settings[index].name, name))
			return (long)index;
	}
	return -1;
}

/* a key in the file that is no setting, likely misspelt or in the wrong
section: said in the log, with the section of a setting of that name */
static void config_report_unknown_key(int line, const char *name)
{
	const char *key = strrchr(name, '.');
	const char *found = NULL;
	size_t index, count = 0;

	key = key ? key + 1 : name;
	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const char *dot = strchr(config_settings[index].name, '.');

		if (dot && !strcmp(dot + 1, key))
		{
			found = config_settings[index].name;
			count++;
		}
	}
	/* (only a name one setting has: the section is then sure) */
	if (count == 1)
	{
		platform_log("config.toml line %d: unknown setting %s, ignored (%s goes under [%.*s])", line, name, key,
			(int)(strchr(found, '.') - found), found);
	}
	else
	{
		platform_log("config.toml line %d: unknown setting %s, ignored", line, name);
	}
}

/* keys in the file that are no setting, likely misspelt */
static void config_report_unknown_keys(toml_datum_t table)
{
	int section_index;

	for (section_index = 0; section_index < table.u.tab.size; section_index++)
	{
		toml_datum_t section = table.u.tab.value[section_index];
		int key_index;

		if (!strcmp(table.u.tab.key[section_index], "config_version"))
			continue;
		if (section.type != TOML_TABLE)
		{
			config_report_unknown_key(section.lineno, table.u.tab.key[section_index]);
			continue;
		}
		for (key_index = 0; key_index < section.u.tab.size; key_index++)
		{
			char name[128];

			snprintf(name, sizeof(name), "%s.%s", table.u.tab.key[section_index], section.u.tab.key[key_index]);
			if (config_setting_index(name) < 0)
				config_report_unknown_key(section.u.tab.value[key_index].lineno, name);
		}
	}
}

/* ---------- older files */

/* the settings whose default an earlier version had otherwise, with that
default as it was written */
static const struct
{
	const char *name;
	const char *default_value;
	/* whether a file of the current layout holding it is moved to the
	current default too (the value was never a choice a player made) */
	int follow;
	/* for an existing file that has the setting at the old default (or not
	set): the change in words, to ask about it (config_find_new_defaults) */
	const char *change;
} config_old_defaults[] =
{
	/* upstream's, which ChupathingyCE kept until v0.5.2b */
	{ "display.menus", "\"pc\"", 0, NULL },
	/* ChupathingyCE's from v0.5.2b, before Arena Evolved went back to "pc" */
	{ "display.menus", "\"xbox\"", 0, NULL },
};

#define NUMBER_OF_CONFIG_OLD_DEFAULTS (sizeof(config_old_defaults) / sizeof(config_old_defaults[0]))

/* whether the file's value is the default written as text */
static int config_datum_is(toml_datum_t datum, enum config_type type, const char *text)
{
	struct config_value value = { 0, 0, 0.0, NULL };
	size_t length = strlen(text);

	switch (type)
	{
	case _config_boolean:
		config_set_from_text(&value, type, text);
		return datum.type == TOML_BOOLEAN && !datum.u.boolean == !value.boolean;
	case _config_integer:
		config_set_from_text(&value, type, text);
		return datum.type == TOML_INT64 && datum.u.int64 == value.integer;
	case _config_real:
		config_set_from_text(&value, type, text);
		return (datum.type == TOML_FP64 && datum.u.fp64 == value.real) ||
			(datum.type == TOML_INT64 && (double)datum.u.int64 == value.real);
	case _config_string:
		/* (a TOML basic string without escapes) */
		return datum.type == TOML_STRING && length >= 2 && strlen(datum.u.s) == length - 2 &&
			!strncmp(datum.u.s, text + 1, length - 2);
	}
	return 0;
}

/* a setting's line the player changed but left commented out, "# key =
value" with a value other than the default written there: it does nothing,
which the log says (the file as the game writes it, every line at its
default, says nothing). A setting given a value elsewhere in the file is
left alone */
static void config_report_commented(const char *text, toml_datum_t table)
{
	char section[64] = "";
	const char *line;
	int line_number = 1;

	for (line = text; *line; line_number++)
	{
		const char *end = line + strcspn(line, "\n");
		const char *next = *end ? end + 1 : end;
		size_t index;

		for (index = 0; !config_line_section(line, end, section, sizeof(section)) &&
			index < NUMBER_OF_CONFIG_SETTINGS; index++)
		{
			const struct config_setting *setting = &config_settings[index];
			const char *dot = strchr(setting->name, '.');
			const char *value;
			char *probe;
			size_t probe_size;
			toml_result_t parsed;
			int same;

			if (!(setting->platforms & CONFIG_PLATFORM) || !dot ||
				strncmp(section, setting->name, (size_t)(dot - setting->name)) ||
				section[dot - setting->name] != 0 || !config_line_commented_key(line, end, dot + 1))
			{
				continue;
			}
			if (toml_seek(table, setting->name).type != TOML_UNKNOWN || config_pending[index])
				break;
			/* (the value read as TOML: 1.0 is 1.00) */
			value = (const char *)memchr(line, '=', (size_t)(end - line)) + 1;
			probe_size = (size_t)(end - value) + 8;
			probe = malloc(probe_size);
			if (!probe)
				break;
			snprintf(probe, probe_size, "v = %.*s\n", (int)(end - value), value);
			parsed = toml_parse(probe, (int)strlen(probe));
			same = parsed.ok && config_datum_is(toml_seek(parsed.toptab, "v"), setting->type, setting->default_value);
			toml_free(parsed);
			free(probe);
			if (!same)
			{
				platform_log("config.toml line %d: %s is commented out, so it does nothing; remove the '# ' to use it",
					line_number, setting->name);
			}
			break;
		}
		line = next;
	}
}

/* a file from before config_version (text, parsed as table) wrote every
setting at the default of the version that wrote it, so that a default
changed since never reached it: its lines holding this version's default
are commented out at it, as a new file has them, and config_version
written. Any other value stays, being the player's, even an older
version's default (the player may have chosen it: display.menus = "gearbox"),
which the log says. Returns the new text, or NULL if the file has its
version */
static char *config_update_layout(const char *text, toml_datum_t table)
{
	toml_datum_t version = toml_seek(table, "config_version");
	int lines[NUMBER_OF_CONFIG_SETTINGS];
	struct config_text out = { NULL, 0, 0 };
	int line_number = 1, defaults = 0, version_written;
	const char *line;
	size_t index;

	if (version.type == TOML_INT64 && version.u.int64 >= CONFIG_VERSION)
		return NULL;
	/* (an older version's line is changed where it is) */
	version_written = version.type != TOML_UNKNOWN;
	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const struct config_setting *setting = &config_settings[index];
		toml_datum_t datum = toml_seek(table, setting->name);
		size_t old;

		lines[index] = 0;
		if (datum.type == TOML_UNKNOWN)
			continue;
		if (config_datum_is(datum, setting->type, setting->default_value))
		{
			lines[index] = datum.lineno;
			continue;
		}
		for (old = 0; old < NUMBER_OF_CONFIG_OLD_DEFAULTS; old++)
		{
			if (!strcmp(config_old_defaults[old].name, setting->name) &&
				config_datum_is(datum, setting->type, config_old_defaults[old].default_value))
			{
				platform_log("settings: %s = %s is kept (an older version's default; this version's is %s, which "
					"commenting the line out follows)", setting->name,
					config_old_defaults[old].default_value, setting->default_value);
			}
		}
	}
	for (line = text; *line; line_number++)
	{
		const char *end = line + strcspn(line, "\n");
		const char *next = *end ? end + 1 : end;
		char section[64];
		long setting_index = -1;

		for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
		{
			/* (only the line "key = value" in the section: not a dotted key
			or an inline table, which hold more) */
			if (lines[index] == line_number &&
				config_line_key(line, end, strchr(config_settings[index].name, '.') + 1))
			{
				setting_index = (long)index;
			}
		}
		if (version.type != TOML_UNKNOWN && version.lineno == line_number &&
			config_line_key(line, end, "config_version"))
		{
			config_append(&out, CONFIG_VERSION_LINE);
		}
		else if (setting_index >= 0)
		{
			const struct config_setting *setting = &config_settings[setting_index];
			char buffer[256];

			snprintf(buffer, sizeof(buffer), "# %s = %s\n", strchr(setting->name, '.') + 1, setting->default_value);
			config_append(&out, buffer);
			defaults++;
		}
		else
		{
			char *copy;

			/* (the version goes before the first section, as TOML has it,
			after the blank line that is there) */
			if (!version_written && config_line_section(line, end, section, sizeof(section)))
			{
				config_append(&out, CONFIG_VERSION_LINES "\n");
				version_written = 1;
			}
			copy = config_copy(line, (size_t)(next - line));
			if (copy)
				config_append(&out, copy);
			free(copy);
		}
		line = next;
	}
	if (!version_written)
	{
		if (out.length && out.buffer[out.length - 1] != '\n')
			config_append(&out, "\n");
		config_append(&out, CONFIG_VERSION_TEXT);
	}
	platform_log("settings: config.toml updated (config_version %d): %d lines at this version's default commented "
		"out, to follow it; every other value is kept", CONFIG_VERSION, defaults);
	return out.buffer;
}

/* settings that replaced an earlier version's, which an older file (or the
environment) still sets instead: display.performance and
display.performance_position from display.performance_overlay (its line:
"full") or else display.fps_counter (the frame rate: "fps"), each
"top_left", "top_right" or "off" (true the top right, false off). The
replacing setting keeps its value where it is set itself, as it is once
Settings writes it; the earlier ones are left in the file, and not written
in a new one (_platform_none) */
/* (AE) audio.resampling, ChupathingyCE's name of OpenCE's audio.resampler: its value, unless audio.resampler is
set itself */
static void ae_config_carry_over_resampling(void) /* AE hook */
{
	struct config_value *resampler = &config_values[config_setting_index("audio.resampler")];
	const struct config_value *resampling = &config_values[config_setting_index("audio.resampling")];

	if (resampler->chosen || !resampling->chosen || !resampling->string[0])
		return;
	free(resampler->string);
	resampler->string = strdup(resampling->string);
	platform_log("settings: audio.resampling (an earlier version's setting) is \"%s\": audio.resampler \"%s\"",
		resampling->string, resampler->string);
}

static void config_carry_over(void)
{
	static const struct
	{
		const char *name;
		const char *level;
	} earlier[] =
	{
		{ "display.performance_overlay", "full" },
		{ "display.fps_counter", "fps" },
	};
	struct config_value *level = &config_values[config_setting_index("display.performance")];
	struct config_value *position = &config_values[config_setting_index("display.performance_position")];
	struct config_value *disc = &config_values[config_setting_index("game.disc")]; /* AE hook */
	const struct config_value *mod = &config_values[config_setting_index("game.mod")];
	size_t index;

	/* (game.mod, earlier releases' game.disc: the disc it names, unless game.disc is set itself) */
	if (!disc->chosen && mod->chosen && mod->string[0])
	{
		free(disc->string);
		disc->string = strdup(mod->string);
		platform_log("settings: game.mod (an earlier version's setting) is \"%s\": game.disc \"%s\"", mod->string,
			disc->string);
	}
	ae_config_carry_over_resampling(); /* AE hook */
	/* (ChupathingyCE 0.8.0b's display.loading_screen_min_seconds: a campaign
	level's least time now, unless that is set itself) */
	{
		const struct config_value *old = &config_values[config_setting_index("display.loading_screen_min_seconds")];
		struct config_value *campaign = &config_values[config_setting_index("display.loading_screen_campaign_seconds")];

		if (old->chosen && !campaign->chosen)
		{
			campaign->real = old->real;
			platform_log("settings: display.loading_screen_min_seconds (an earlier version's setting) is %.1f: "
				"display.loading_screen_campaign_seconds", old->real);
		}
	}
	if (level->chosen && position->chosen)
		return;
	for (index = 0; index < sizeof(earlier) / sizeof(earlier[0]); index++)
	{
		const struct config_value *value = &config_values[config_setting_index(earlier[index].name)];

		if (!value->chosen || !value->string[0] || config_text_is_false(value->string))
			continue;
		/* (nothing holds the defaults' strings yet) */
		if (!level->chosen)
		{
			free(level->string);
			level->string = strdup(earlier[index].level);
		}
		if (!position->chosen)
		{
			free(position->string);
			position->string = strdup(!strcmp(value->string, "top_left") ? "top_left" : "top_right");
		}
		platform_log("settings: %s (an earlier version's setting) is %s: display.performance \"%s\", "
			"display.performance_position \"%s\"", earlier[index].name, value->string, level->string,
			position->string);
		break;
	}
}

/* whether the value is the setting's default, or one an earlier version had */
static int config_datum_is_a_default(toml_datum_t datum, const struct config_setting *setting)
{
	size_t old;

	if (config_datum_is(datum, setting->type, setting->default_value))
		return 1;
	for (old = 0; old < NUMBER_OF_CONFIG_OLD_DEFAULTS; old++)
	{
		if (!strcmp(config_old_defaults[old].name, setting->name) &&
			config_datum_is(datum, setting->type, config_old_defaults[old].default_value))
		{
			return 1;
		}
	}
	return 0;
}

/* whether text has the setting commented out at a default in its section,
"# key = ..." */
static int config_text_has_commented_key(const char *text, const char *name)
{
	const char *dot = strchr(name, '.');
	char section[64], current[64] = "";
	const char *line;

	snprintf(section, sizeof(section), "%.*s", (int)(dot - name), name);
	for (line = text; *line;)
	{
		const char *end = line + strcspn(line, "\n");

		if (!config_line_section(line, end, current, sizeof(current)) && !strcmp(current, section) &&
			config_line_commented_key(line, end, dot + 1))
		{
			return 1;
		}
		line = *end ? end + 1 : end;
	}
	return 0;
}

/* whether the line (from its '#') is a setting commented out, "# key = ..." */
static int config_line_is_commented_setting(const char *line)
{
	const char *start;

	line++;
	while (*line == ' ' || *line == '\t')
		line++;
	start = line;
	while ((*line >= 'a' && *line <= 'z') || (*line >= '0' && *line <= '9') || *line == '_')
		line++;
	if (line == start)
		return 0;
	while (*line == ' ' || *line == '\t')
		line++;
	return *line == '=';
}

/* a version from before config_version, run on a file of this layout, sees
the settings commented out at their default as missing, and adds each as a
value at its own default (with its comment, after a blank line) at the end
of its section: the file then holds the setting twice, "# key = default"
from this layout and "key = value" from the older version, and the value
pins the older version's default. Such a value that is a default, this
version's or an older one, is taken out with its comment, so that the
setting follows this version's default again; a value that is no default
stays, being the player's. Returns the new text, or NULL if there was none */
static char *config_drop_added_defaults(const char *text, toml_datum_t table)
{
	toml_datum_t version = toml_seek(table, "config_version");
	int lines[NUMBER_OF_CONFIG_SETTINGS];
	struct config_text out = { NULL, 0, 0 };
	int line_number = 1, dropped = 0, any = 0;
	const char *line;
	size_t index;

	if (version.type != TOML_INT64 || version.u.int64 < 2)
		return NULL;
	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const struct config_setting *setting = &config_settings[index];
		toml_datum_t datum = toml_seek(table, setting->name);

		lines[index] = 0;
		if (datum.type != TOML_UNKNOWN && config_datum_is_a_default(datum, setting) &&
			config_text_has_commented_key(text, setting->name))
		{
			lines[index] = datum.lineno;
			any = 1;
		}
	}
	if (!any)
		return NULL;
	for (line = text; *line; line_number++)
	{
		const char *end = line + strcspn(line, "\n");
		const char *next = *end ? end + 1 : end;
		int drop = 0;

		for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS && !drop; index++)
		{
			drop = lines[index] == line_number &&
				config_line_key(line, end, strchr(config_settings[index].name, '.') + 1);
		}
		if (drop)
		{
			/* (its comment above it, and the blank line before that) */
			while (out.length)
			{
				size_t start = out.length - 1;
				const char *first;

				while (start > 0 && out.buffer[start - 1] != '\n')
					start--;
				for (first = out.buffer + start; *first == ' ' || *first == '\t'; first++)
				{
				}
				/* (not another setting's line, "# key = value") */
				if (*first != '#' || config_line_is_commented_setting(first))
					break;
				out.length = start;
				out.buffer[start] = 0;
			}
			if (out.length >= 2 && out.buffer[out.length - 1] == '\n' && out.buffer[out.length - 2] == '\n')
				out.buffer[--out.length] = 0;
			dropped++;
		}
		else
		{
			char *copy = config_copy(line, (size_t)(next - line));

			if (copy)
				config_append(&out, copy);
			free(copy);
		}
		line = next;
	}
	platform_log("settings: config.toml held %d settings twice, commented out and as a default an older version "
		"added; those values are taken out, to follow this version's defaults", dropped);
	return out.buffer;
}

/* a default's text without its quotes, as config_write takes it */
static void config_unquote(const char *text, char *out, size_t size)
{
	size_t length = strlen(text);

	if (length >= 2 && text[0] == '"')
	{
		text++;
		length -= 2;
	}
	if (length >= size)
		length = size - 1;
	memcpy(out, text, length);
	out[length] = 0;
}

#ifndef HALO_SERVER
/* whether the setting is commented out in text ("# key = ...") at the value
(a default as written): 1 if so, 0 if at another, -1 if it is not there */
static int config_commented_value_is(const char *text, const struct config_setting *setting, const char *value_text)
{
	const char *dot = strchr(setting->name, '.');
	char section[64], current[64] = "";
	const char *line;

	snprintf(section, sizeof(section), "%.*s", (int)(dot - setting->name), setting->name);
	for (line = text; *line;)
	{
		const char *end = line + strcspn(line, "\n");

		if (!config_line_section(line, end, current, sizeof(current)) && !strcmp(current, section) &&
			config_line_commented_key(line, end, dot + 1))
		{
			const char *value = (const char *)memchr(line, '=', (size_t)(end - line)) + 1;
			size_t probe_size = (size_t)(end - value) + 8;
			char *probe = malloc(probe_size);
			toml_result_t parsed;
			int same;

			if (!probe)
				return 0;
			snprintf(probe, probe_size, "v = %.*s\n", (int)(end - value), value);
			parsed = toml_parse(probe, (int)strlen(probe));
			same = parsed.ok && config_datum_is(toml_seek(parsed.toptab, "v"), setting->type, value_text);
			toml_free(parsed);
			free(probe);
			return same;
		}
		line = *end ? end + 1 : end;
	}
	return -1;
}

#endif

/* An existing file (text, parsed as table) whose settings this version's
defaults would change: those at an older default that follow the default (at
the old one, commented out, or not there at all, the file being from before
the setting), and those an older version wrote as a value beside the
commented one (config_drop_added_defaults). They are listed in
config_pending, to be asked about once (config_new_defaults_pending); the
file is left as it is, and the settings keep their old values, until the
player answers. A file that was answered (game.defaults_answered) has none.
The dedicated server keeps its own defaults and is never asked. */
static void config_find_new_defaults(const char *text, toml_datum_t table)
{
#ifndef HALO_SERVER
	toml_datum_t version = toml_seek(table, "config_version");
	toml_datum_t answered = toml_seek(table, "game.defaults_answered");
	size_t old;

	if (answered.type == TOML_INT64 && answered.u.int64 >= CONFIG_DEFAULTS_GENERATION)
		return;
	for (old = 0; old < NUMBER_OF_CONFIG_OLD_DEFAULTS; old++)
	{
		long index = config_old_defaults[old].change ? config_setting_index(config_old_defaults[old].name) : -1;
		const struct config_setting *setting;
		toml_datum_t datum;

		if (index < 0)
			continue;
		setting = &config_settings[index];
		if (!(setting->platforms & CONFIG_PLATFORM) || !strcmp(config_old_defaults[old].default_value, setting->default_value))
			continue;
		datum = toml_seek(table, setting->name);
		if (datum.type != TOML_UNKNOWN)
		{
			if (version.type == TOML_INT64 && version.u.int64 >= 2 &&
				config_datum_is(datum, setting->type, config_old_defaults[old].default_value) &&
				config_text_has_commented_key(text, setting->name))
			{
				config_pending[index] = 2;
			}
		}
		else if (config_commented_value_is(text, setting, config_old_defaults[old].default_value) != 0)
		{
			config_pending[index] = 1;
		}
		if (config_pending[index])
			config_pending_old[index] = config_old_defaults[old].default_value;
	}
#else
	(void)text;
	(void)table;
#endif
}

/* whether the line is blank */
static int config_line_blank(const char *line, const char *end)
{
	while (line < end && (*line == ' ' || *line == '\t' || *line == '\r'))
		line++;
	return line == end;
}

/* whether the line is a comment */
static int config_line_comment(const char *line, const char *end)
{
	while (line < end && (*line == ' ' || *line == '\t'))
		line++;
	return line < end && *line == '#';
}

/* a file that holds, for a setting whose old default is followed, that
default as a value: the setting's lines are taken out of it (the value, and
a copy commented out at that default with its help text of that day, each
with the comment above it and one blank line), and config_add_missing then
writes it again as this version has it, commented out at the current
default, so that it follows. Returns the new text, or NULL if there was
nothing to change */
static char *config_follow_old_defaults(const char *text, toml_datum_t table)
{
	char *result = NULL;
	size_t old;

	for (old = 0; old < NUMBER_OF_CONFIG_OLD_DEFAULTS; old++)
	{
		const char *source = result ? result : text;
		const struct config_setting *setting;
		struct config_text out = { NULL, 0, 0 };
		toml_datum_t datum;
		const char *dot, *line;
		const char **starts;
		size_t count = 0, capacity = 0, line_index, found = 0;
		unsigned char *drop;
		char section[64], current[64] = "";
		long index = config_old_defaults[old].follow ? config_setting_index(config_old_defaults[old].name) : -1;

		if (index < 0)
			continue;
		setting = &config_settings[index];
		datum = toml_seek(table, setting->name);
		if (!config_datum_is(datum, setting->type, config_old_defaults[old].default_value) ||
			config_datum_is(datum, setting->type, setting->default_value))
			continue;
		dot = strchr(setting->name, '.');
		snprintf(section, sizeof(section), "%.*s", (int)(dot - setting->name), setting->name);
		for (line = source; *line; line += strcspn(line, "\n") + (line[strcspn(line, "\n")] ? 1 : 0))
			capacity++;
		starts = malloc((capacity + 1) * sizeof(*starts));
		drop = calloc(capacity + 1, 1);
		if (!starts || !drop)
		{
			free(starts);
			free(drop);
			continue;
		}
		for (line = source; *line; line += strcspn(line, "\n") + (line[strcspn(line, "\n")] ? 1 : 0))
			starts[count++] = line;
		starts[count] = line;
		for (line_index = 0; line_index < count; line_index++)
		{
			const char *start = starts[line_index];
			const char *end = start + strcspn(start, "\n");

			if (config_line_section(start, end, current, sizeof(current)))
				continue;
			if (!strcmp(current, section) &&
				(config_line_key(start, end, dot + 1) || config_line_commented_key(start, end, dot + 1)))
			{
				size_t back = line_index;

				drop[line_index] = 1;
				found++;
				while (back > 0 && config_line_comment(starts[back - 1], starts[back - 1] + strcspn(starts[back - 1], "\n")))
					drop[--back] = 1;
				if (back > 0 && config_line_blank(starts[back - 1], starts[back - 1] + strcspn(starts[back - 1], "\n")))
					drop[back - 1] = 1;
			}
		}
		if (found)
		{
			for (line_index = 0; line_index < count; line_index++)
			{
				if (!drop[line_index])
				{
					char *copy = config_copy(starts[line_index], (size_t)(starts[line_index + 1] - starts[line_index]));

					if (copy)
						config_append(&out, copy);
					free(copy);
				}
			}
			if (out.buffer)
			{
				free(result);
				result = out.buffer;
				platform_log("settings: %s = %s was an older version's default; it follows this version's (%s) now",
					setting->name, config_old_defaults[old].default_value, setting->default_value);
			}
		}
		free(starts);
		free(drop);
	}
	return result;
}

/* ---------- words */

/* the settings that take one of a few words: one in any case is taken (as
written here), and another is said once in the log */
static const struct
{
	const char *name;
	/* NULL ended; "" where empty is one */
	const char *words[24];
	/* what the game does with another, for the log (NULL: what reads the
	setting says it) */
	const char *otherwise;
} config_words[] =
{
	{ "display.mode", { "", "fullscreen", "borderless", "windowed", NULL }, "display.fullscreen's" },
	{ "display.resolution_scaling", { "native", "75%", "50%", "25%", "original", NULL }, "\"native\"" },
	{ "display.anti_aliasing", { "off", "fxaa", "smaa", "ssaa2x", "msaa2x", "msaa4x", "msaa8x", NULL }, NULL },
	{ "display.menus", { "xbox", "pc", "gearbox", NULL }, NULL },
	{ "display.player_names", { "all", "allies", "allys", "enemies", "none", NULL }, "\"all\"" },
	{ "display.scoreboard_team_layout", { "teams", "score", NULL }, "\"teams\"" },
	{ "display.hud_button_style", { "classic", "gems", "menu_gems", NULL }, NULL },
	{ "display.chat_style", { "colors", "white", NULL }, NULL },
	{ "display.shield_glow_intensity", { "default", "light_show", NULL }, NULL },
	{ "display.shield_color", { "default", "white", "black", "red", "blue", "gray", "yellow", "green", "pink",
		"purple", "cyan", "cobalt", "orange", "teal", "sage", "brown", "tan", "maroon", "salmon", NULL }, NULL },
	{ "display.pistol_color", { "default", "white", "black", "red", "blue", "gray", "yellow", "green", "pink", /* AE hook */
		"purple", "cyan", "cobalt", "orange", "teal", "sage", "brown", "tan", "maroon", "salmon", NULL }, NULL }, /* AE hook */
	{ "display.pistol_trim", { "default", "white", "black", "red", "blue", "gray", "yellow", "green", "pink", /* AE hook */
		"purple", "cyan", "cobalt", "orange", "teal", "sage", "brown", "tan", "maroon", "salmon", NULL }, NULL }, /* AE hook */
	{ "audio.resampler", { "sinc", "linear", NULL }, "\"sinc\"" }, /* AE hook */
	{ "audio.voice_chat", { "push_to_talk", "open_mic", "off", NULL }, NULL },
	{ "display.scoreboard_platforms", { "auto", "on", "off", NULL }, "\"auto\"" },
	{ "display.follow_camera", { "long_waits", "always", "off", NULL }, "\"long_waits\"" },
	{ "display.texture_decode", { "auto", "gpu", "cpu", NULL }, "\"auto\"" },
	{ "input.touch_controls", { "on", "auto", "off", NULL }, "\"on\"" },
	{ "game.console_log", { "important", "all", "none", NULL }, "\"none\"" },
	{ "game.language", { "", "ja", "de", "fr", "es", "it", NULL }, NULL },
	{ "game.move_old_map_folders", { "ask", "yes", "no", NULL }, "\"ask\"" },
	{ "network.protocol", { "auto", "opence", "delta", NULL }, NULL },
	{ "network.map_downloads", { "ask", "always", "never", NULL }, "\"ask\"" },
	{ "network.join_from_clipboard", { "ask", "always", "never", NULL }, "\"never\"" },
	{ "network.team_change", { "balanced", "allow", "off", NULL }, "\"balanced\"" },
#ifndef HALO_CONSOLE_NO_DISCORD
	{ "discord.presence", { "full", "minimal", "off", NULL }, "\"off\"" }, /* AE hook */
#endif
	{ "network.platform_limits", { "on", "off", NULL }, "\"on\"" },
	{ "network.coop_friendly_fire", { "off", "on", "shields_only", "explosives_only", NULL }, NULL },
	{ "network.shield_effects", { "stock", "glow", "light_show", "overshield", "glow_overshield",
		"light_show_overshield", "players", NULL }, NULL },
	{ "network.coop_enemies_mode", { "none", "per_player", "multiplier", NULL }, "\"none\"" },
	{ "network.text_chat", { "on", "links", "off", NULL }, NULL },
	{ "server.chat_filter", { "off", "censor", "block", NULL }, NULL },
	{ "server.chat_filter_slurs", { "off", "severe", "strong", "mild", NULL }, NULL },
	{ "server.chat_filter_swearing", { "off", "severe", "strong", "mild", NULL }, NULL },
	{ "server.chat_filter_insults", { "off", "severe", "strong", "mild", NULL }, NULL },
	{ "network.voice_mode", { "off", "team_proximity", "team_enemy_proximity", "team_global",
		"team_global_enemy_proximity", NULL }, NULL },
	{ "streamer.slurs", { "off", "severe", "strong", "mild", NULL }, NULL },
	{ "streamer.swearing", { "off", "severe", "strong", "mild", NULL }, NULL },
	{ "streamer.insults", { "off", "severe", "strong", "mild", NULL }, NULL },
	{ "crash_reports.upload", { "ask", "yes", "no", NULL }, "\"ask\"" },
	{ "network.community_services", { "ask", "yes", "no", NULL }, "\"ask\"" }, /* AE hook */
};

#define NUMBER_OF_CONFIG_WORDS (sizeof(config_words) / sizeof(config_words[0]))

/* an older version's value of a word setting, read as the word that took
its place (a setting that was true or false: the file's true and false are
taken as these words' "true" and "false": config_set_from_file) */
static const struct
{
	const char *name;
	const char *old_word;
	const char *word;
} config_old_words[] =
{
	{ "network.join_from_clipboard", "true", "always" },
	{ "network.join_from_clipboard", "false", "never" },
};

#define NUMBER_OF_CONFIG_OLD_WORDS (sizeof(config_old_words) / sizeof(config_old_words[0]))

/* whether the setting once was true or false, which the file may still say */
static int config_was_boolean(const char *name)
{
	size_t index;

	for (index = 0; index < NUMBER_OF_CONFIG_OLD_WORDS; index++)
	{
		if (!strcmp(config_old_words[index].name, name))
			return 1;
	}
	return 0;
}

/* the word settings' values as their readers compare them (lowercase:
"Borderless" is "borderless"), and the log told of one that is none of
its words */
static void config_check_words(void)
{
	size_t word_index;

	for (word_index = 0; word_index < NUMBER_OF_CONFIG_WORDS; word_index++)
	{
		long index = config_setting_index(config_words[word_index].name);
		const char *const *words = config_words[word_index].words;
		struct config_value *value;
		char list[400] = "";
		size_t word;

		if (index < 0 || !(config_settings[index].platforms & CONFIG_PLATFORM))
			continue;
		value = &config_values[index];
		for (word = 0; word < NUMBER_OF_CONFIG_OLD_WORDS; word++)
		{
			if (!strcmp(config_old_words[word].name, config_words[word_index].name) &&
				!SDL_strncasecmp(value->string, config_old_words[word].old_word, strlen(config_old_words[word].old_word) + 1))
			{
				free(value->string);
				value->string = strdup(config_old_words[word].word);
				break;
			}
		}
		for (word = 0; words[word] && SDL_strncasecmp(value->string, words[word], strlen(words[word]) + 1); word++)
		{
		}
		if (words[word])
		{
			if (strcmp(value->string, words[word]))
			{
				free(value->string);
				value->string = strdup(words[word]);
			}
			continue;
		}
		if (!config_words[word_index].otherwise)
			continue;
		/* ("a", "b" or "c", and "or empty" for "") */
		for (word = 0; words[word]; word++)
		{
			size_t length = strlen(list);

			if (!words[word][0])
				continue;
			snprintf(list + length, sizeof(list) - length, "%s\"%s\"",
				!length ? "" : !words[word + 1] && words[0][0] ? " or " : ", ", words[word]);
		}
		if (!words[0][0])
			snprintf(list + strlen(list), sizeof(list) - strlen(list), " or empty");
		platform_log("settings: %s = \"%s\" is not %s; using %s", config_words[word_index].name, value->string, list,
			config_words[word_index].otherwise);
	}
}

/* the most lines of a file that cannot be read that are skipped to read
the rest */
#define CONFIG_MAXIMUM_SKIPPED_LINES 16

/* the line a parse error is on ("(line 12) ..."), or 0 */
static int config_error_line(const char *message)
{
	const char *at = strstr(message, "(line ");
	int line = 0;

	if (!at || sscanf(at, "(line %d)", &line) != 1)
		return 0;
	return line;
}

/* the line of text that cannot be read, said in the log with what likely
makes it so, and emptied (its number kept for the lines after it): returns
whether it was there */
static int config_skip_line(char *text, int line_number, const char *message)
{
	char *line = text, *end, *value, *value_end;
	const char *reason = strchr(message, ')');
	char key[64] = "";
	char hint[160] = "";
	int number;

	for (number = 1; number < line_number && *line; number++)
		line += strcspn(line, "\n") + (line[strcspn(line, "\n")] ? 1 : 0);
	if (line_number < 1 || number != line_number || !*line)
		return 0;
	end = line + strcspn(line, "\n");
	reason = reason ? reason + 1 : message;
	while (*reason == ' ')
		reason++;
	/* "key = value": the value as written, to the end of the line or a
	comment */
	value = memchr(line, '=', (size_t)(end - line));
	if (value)
	{
		char *key_start = line, *key_end = value;

		while (key_start < key_end && (*key_start == ' ' || *key_start == '\t'))
			key_start++;
		while (key_end > key_start && (key_end[-1] == ' ' || key_end[-1] == '\t'))
			key_end--;
		snprintf(key, sizeof(key), "%.*s", (int)(key_end - key_start), key_start);
		value++;
		while (value < end && (*value == ' ' || *value == '\t'))
			value++;
		value_end = value + strcspn(value, "#\n");
		if (value_end > end)
			value_end = end;
		while (value_end > value && (value_end[-1] == ' ' || value_end[-1] == '\t' || value_end[-1] == '\r'))
			value_end--;
		if ((value_end - value == 4 && !SDL_strncasecmp(value, "true", 4)) ||
			(value_end - value == 5 && !SDL_strncasecmp(value, "false", 5)))
		{
			snprintf(hint, sizeof(hint), ": true and false are lowercase, %s = %.*s", key, (int)(value_end - value),
				value_end - value == 4 ? "true" : "false");
		}
		else if (value < value_end && isalpha((unsigned char)*value))
		{
			snprintf(hint, sizeof(hint), ": text needs quotes, %s = \"%.*s\"", key, (int)(value_end - value), value);
		}
	}
	platform_log("config.toml line %d cannot be read (%s), so it is skipped%s", line_number, reason, hint);
	memset(line, ' ', (size_t)(end - line));
	return 1;
}

#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
/* a config.toml the game never reads, likely edited in its place: said in
the log, with the one it reads (path) */
static void config_report_unread(const char *path)
{
	char other[1100];
#ifdef __APPLE__
	/* beside the application, or in it (the game's is in Application
	Support) */
	const char *base = SDL_GetBasePath();
	const char *bundle = base ? strstr(base, ".app/Contents/") : NULL;

	if (bundle)
	{
		const char *folder_end = bundle;

		while (folder_end > base && folder_end[-1] != '/')
			folder_end--;
		snprintf(other, sizeof(other), "%sconfig.toml", base);
		if (SDL_GetPathInfo(other, NULL))
			platform_log("settings: %s is not read; the game's settings are %s", other, path);
		snprintf(other, sizeof(other), "%.*sconfig.toml", (int)(folder_end - base), base);
		if (SDL_GetPathInfo(other, NULL))
			platform_log("settings: %s is not read; the game's settings are %s", other, path);
	}
#elif !defined(_WIN32)
	/* in the folder the game was started from, when that is not the
	executable's (the game's is beside the executable) */
	char folder[1024];

	if (getcwd(folder, sizeof(folder)))
	{
		snprintf(other, sizeof(other), "%s/config.toml", folder);
		if (strcmp(other, path) && SDL_GetPathInfo(other, NULL))
		{
			SDL_PathInfo mine, theirs;

			/* (the same file by another name is the one read) */
			if (!SDL_GetPathInfo(path, &mine) || !SDL_GetPathInfo(other, &theirs) ||
				mine.size != theirs.size || mine.modify_time != theirs.modify_time)
			{
				platform_log("settings: %s is not read; the game's settings are %s", other, path);
			}
		}
	}
#endif
	/* the file saved with .txt after it, by an editor that adds it (and
	Windows Explorer hides it) */
	snprintf(other, sizeof(other), "%s.txt", path);
	if (SDL_GetPathInfo(other, NULL))
	{
#ifdef _WIN32
		platform_log("settings: %s is not read: Windows saved it as config.toml.txt; rename it to config.toml", other);
#else
		platform_log("settings: %s is not read; rename it to config.toml", other);
#endif
	}
}
#endif

/* the passwords a file may hold instead of their variable (a container's
secrets): <variable>_FILE names it, and its first line is the value */
static const char *const config_secret_files[] =
{
	"server.password", "server.web_password", "server.web_owner_password", "server.web_moderator_password",
	"server.web_viewer_password",
};
#define NUMBER_OF_CONFIG_SECRET_FILES (sizeof(config_secret_files) / sizeof(config_secret_files[0]))

/* the value of the setting's <variable>_FILE (read once, at the first
question: a file changed after the start is not read again), NULL if
there is none, it cannot be read or its first line is empty */
static const char *config_environment_file(const struct config_setting *setting)
{
	static char values[NUMBER_OF_CONFIG_SECRET_FILES][512];
	static signed char read[NUMBER_OF_CONFIG_SECRET_FILES];
	size_t index;

	for (index = 0; index < NUMBER_OF_CONFIG_SECRET_FILES && strcmp(config_secret_files[index], setting->name); index++)
	{
	}
	if (index == NUMBER_OF_CONFIG_SECRET_FILES || !setting->environment)
		return NULL;
	if (!read[index])
	{
		char variable[96];
		const char *path;
		char *text;
		size_t size = 0, length;

		read[index] = -1;
		snprintf(variable, sizeof(variable), "%s_FILE", setting->environment);
		path = getenv(variable);
		if (!path || !path[0])
			return NULL;
		text = config_read_file(path, &size);
		if (!text)
		{
			platform_log("settings: %s names %s, which cannot be read: %s is not set by it", variable, path,
				setting->name);
			return NULL;
		}
		/* (the first line, its CR and LF off) */
		length = strcspn(text, "\n");
		if (length && text[length - 1] == '\r')
			length--;
		if (length >= sizeof(values[index]))
		{
			platform_log("settings: the first line of %s (%s) is longer than %d characters: %s is not set by it",
				path, variable, (int)sizeof(values[index]) - 1, setting->name);
			length = 0;
		}
		memcpy(values[index], text, length);
		values[index][length] = 0;
		memset(text, 0, size);
		free(text);
		read[index] = length ? 1 : -1;
	}
	return read[index] > 0 ? values[index] : NULL;
}

/* a setting's environment variable, NULL if it is not set; a dedicated
server's settings' empty ones are not set either (a panel passes every
variable it has, empty ones too: an empty password must not stand for one).
A password's variable that is not set may be read from its file
(<variable>_FILE: config_environment_file) */
static const char *config_environment(const struct config_setting *setting)
{
	const char *environment = setting->environment ? getenv(setting->environment) : NULL;

	if (environment && !environment[0] &&
		(setting->platforms == _platform_server || !strcmp(setting->name, "network.public_address")))
	{
		environment = NULL;
	}
	if (!environment)
		environment = config_environment_file(setting);
	return environment;
}

#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
/* a dedicated server (port/linux/src/server_mode.c) has no window, sound,
microphone or player, never asks anything, and is reached only as its
settings say, whatever the file and the environment say of these (for this
run: nothing of it is written) */
/* halo --probe: no window, sound, update, Discord or menus' test; the
network test's probe, and a deadline (internet play as set: on by default,
which an invite needs) */
static void config_apply_probe_mode(void)
{
	static const char *const forced[][2] =
	{
		{ "debug.null_renderer", "true" },
		{ "debug.hidden_window", "true" },
		{ "display.fullscreen", "false" },
		{ "audio.enabled", "false" },
		{ "audio.voice_chat", "off" },
		{ "network.join_from_clipboard", "false" },
		{ "update.auto", "false" },
		{ "crash_reports.upload", "no" },
		{ "discord.application_id", "" },
		{ "debug.network_test", "probe" },
		{ "server.enabled", "false" },
		{ "debug.test_input", "" },
		{ "debug.menu_open", "" },
		{ "debug.exit_after", "45" },
	};
	size_t index;

	for (index = 0; index < sizeof(forced) / sizeof(forced[0]); index++)
	{
		long found = config_setting_index(forced[index][0]);

		if (found >= 0)
			config_set_from_text(&config_values[found], config_settings[found].type, forced[index][1]);
	}
}

static void config_apply_server_mode(void)
{
	static const char *const forced[][2] =
	{
		{ "server.enabled", "true" },
		{ "debug.null_renderer", "true" },
		{ "debug.hidden_window", "true" },
		{ "display.fullscreen", "false" },
		{ "display.vsync", "false" },
		{ "display.interpolation", "false" },
		{ "display.high_res_hud", "false" },
		{ "display.high_res_text", "false" },
		{ "display.anti_aliasing", "off" },
		{ "audio.enabled", "false" },
		{ "audio.voice_chat", "off" },
		{ "network.join_from_clipboard", "false" },
		{ "update.auto", "false" },
		{ "crash_reports.upload", "no" },
		{ "discord.application_id", "" },
		{ "debug.network_test", "" },
		{ "debug.test_input", "" },
		{ "debug.menu_open", "" },
	};
	const char *tunnel_port = getenv("HALO_NET_TUNNEL_PORT");
	size_t index;

	for (index = 0; index < sizeof(forced) / sizeof(forced[0]); index++)
	{
		long found = config_setting_index(forced[index][0]);

		if (found >= 0)
			config_set_from_text(&config_values[found], config_settings[found].type, forced[index][1]);
	}
	/* (the tunnel's port fixed, Halo PC's 2302, unless one is given: a
	server's players and firewall need to know it; a router asked to
	forward it only as server.upnp says) */
	if ((!tunnel_port || !*tunnel_port) && !config_values[config_setting_index("network.tunnel_port")].integer)
		config_values[config_setting_index("network.tunnel_port")].integer = 2302;
	config_values[config_setting_index("network.allow_upnp")].boolean =
		config_values[config_setting_index("server.upnp")].boolean;
}
#endif

static void config_load(void)
{
	char path[1024];
	size_t size = 0;
	char *text;
	size_t index;

	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const char *default_value = config_settings[index].default_value;

		if (config_settings[index].type == _config_string)
		{
			/* written as a TOML basic string without escapes */
			size_t length = strlen(default_value);

			config_values[index].string = length >= 2 ? config_copy(default_value + 1, length - 2) : strdup("");
		}
		else
		{
			config_set_from_text(&config_values[index], config_settings[index].type, default_value);
		}
	}

	config_path(path, sizeof(path));
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	config_report_unread(path);
	/* (a dedicated server's file has its settings too: asked for by the
	command line or the environment, or by the file's server.enabled; a
	probe is never one, whatever they say) */
	config_probe_mode = config_argument("--probe", NULL, 0);
	{
		const char *environment = getenv("HALO_SERVER");

		config_server_mode = !config_probe_mode && (config_argument("--server", NULL, 0) ||
			(environment && config_environment_set(environment)));
	}
	/* (a run that only answers its command line, halo --version, writes no
	settings: the file read is left as it is) */
	config_answer_only = config_argument("--help", NULL, 0) || config_argument("--version", NULL, 0) ||
		config_argument("--keygen", NULL, 0) || config_argument("--sign", NULL, 0) || config_probe_mode;
#endif
	text = config_read_file(path, &size);
	if (text)
	{
		char *joined = config_join_sections(text);
		/* (a file with a section twice is written back joined, below) */
		int sections_joined = joined != NULL;
		toml_result_t result;
		int skipped = 0;

		if (joined)
		{
			free(text);
			text = joined;
			size = strlen(text);
		}
		result = toml_parse(text, (int)size);
		/* (a line that cannot be read is skipped, in memory, so that the
		rest is) */
		while (!result.ok && skipped < CONFIG_MAXIMUM_SKIPPED_LINES &&
			config_skip_line(text, config_error_line(result.errmsg), result.errmsg))
		{
			toml_free(result);
			result = toml_parse(text, (int)strlen(text));
			skipped++;
		}
		if (result.ok)
		{
			char *updated = config_update_layout(text, result.toptab);
			char *completed;

			if (updated && !config_answer_only)
			{
				/* (read as it is now, the defaults it gave up taking effect) */
				toml_result_t updated_result = toml_parse(updated, (int)strlen(updated));

				if (updated_result.ok)
				{
					toml_free(result);
					free(text);
					result = updated_result;
					text = updated;
				}
				else
				{
					toml_free(updated_result);
					free(updated);
					updated = NULL;
				}
			}
			{
				/* (a value that was an older version's default, followed) */
				char *followed = config_follow_old_defaults(text, result.toptab);
				toml_result_t followed_result;

				if (followed)
				{
					followed_result = toml_parse(followed, (int)strlen(followed));
					if (followed_result.ok)
					{
						toml_free(result);
						free(text);
						result = followed_result;
						text = followed;
						updated = text;
					}
					else
					{
						toml_free(followed_result);
						free(followed);
					}
				}
			}
			config_find_new_defaults(text, result.toptab);
			for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
			{
				config_set_from_file(&config_values[index], &config_settings[index], result.toptab);
				config_given[index] = toml_seek(result.toptab, config_settings[index].name).type != TOML_UNKNOWN;
			}
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
			if (config_values[config_setting_index("server.enabled")].boolean && !config_probe_mode)
				config_server_mode = 1;
#endif
			/* (until the player is asked, the values stay what they were: the
			new defaults take effect only when they say so) */
			for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
			{
				if (config_pending[index])
				{
					char old[64];

					config_unquote(config_pending_old[index], old, sizeof(old));
					config_set_from_text(&config_values[index], config_settings[index].type, old);
				}
			}
			config_report_unknown_keys(result.toptab);
			platform_log("settings: %s", path);
			completed = skipped ? NULL : config_add_missing(text, result.toptab);
			config_report_commented(completed ? completed : text, result.toptab);
			/* (a file with lines skipped is left as the player wrote it, to
			be fixed) */
			if (skipped)
			{
				platform_log("settings: %d line%s of config.toml skipped; every other setting in it is used", skipped,
					skipped == 1 ? "" : "s");
			}
			else if (!config_answer_only && (completed || updated || sections_joined) &&
				!config_write_file(path, completed ? completed : text))
			{
				platform_log("settings: cannot write %s", path);
			}
			else if (sections_joined && !config_answer_only)
				platform_log("settings: joined the sections %s had twice", path);
			free(completed);
		}
		else
		{
			platform_log("config.toml cannot be read (%s): every setting is at its default until that is fixed",
				result.errmsg);
		}
		toml_free(result);
		free(text);
	}
	else if (!config_answer_only)
	{
		char *defaults = config_default_text();

		if (defaults && config_write_file(path, defaults))
			platform_log("settings: wrote the defaults to %s", path);
		else
			platform_log("settings: cannot write %s; using the defaults", path);
		free(defaults);
	}

	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const struct config_setting *setting = &config_settings[index];
		const char *environment = config_environment(setting);

		/* (one set to "", "0", "false", "no" or "off" is as if it were not:
		HALO_LOG_ADDRESSES=0 never turns a setting on) */
		if (!environment || (setting->environment_style != _environment_value && !config_environment_set(environment)))
			continue;
		config_given[index] = 1;
		switch (setting->environment_style)
		{
		case _environment_value:
			config_set_from_text(&config_values[index], setting->type, environment);
			break;
		case _environment_set_is_true:
			config_values[index].boolean = 1;
			break;
		case _environment_set_is_false:
			config_values[index].boolean = 0;
			break;
		}
		config_values[index].chosen = 1;
	}
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	if (config_server_mode)
		config_apply_server_mode();
	else if (config_probe_mode)
		config_apply_probe_mode();
#endif
	config_carry_over();
	config_check_words();
#ifndef HALO_ANDROID
	{
		long mode = config_setting_index("display.mode");
		long fullscreen = config_setting_index("display.fullscreen");

		/* (said once: which of the two the window follows) */
		const char *value = config_values[mode].string;

		if ((!strcmp(value, "fullscreen") || !strcmp(value, "borderless") || !strcmp(value, "windowed")) &&
			config_given[fullscreen])
		{
			platform_log("settings: display.mode = \"%s\" is used; display.fullscreen is ignored",
				config_values[mode].string);
		}
	}
#endif
}

/* the file read, the first time a setting is asked for */
static void config_ensure_loaded(void)
{
	pthread_mutex_lock(&config_lock);
	if (!config_loaded)
	{
		config_load();
		config_loaded = 1;
	}
	pthread_mutex_unlock(&config_lock);
}

static const struct config_value *config_value(const char *name, enum config_type type)
{
	static const struct config_value none = { 0, 0, 0.0, "" };
	long index;

	config_ensure_loaded();
	index = config_setting_index(name);
	if (index < 0 || config_settings[index].type != type)
	{
		platform_log("settings: no %s setting %s", type == _config_string ? "string" : "such", name);
		return &none;
	}
	if (ae_config_player_value(name, config_settings[index].environment, (int)type, &ae_player_value.boolean, &ae_player_value.integer, &ae_player_value.real, &ae_player_value.string)) /* AE hook */
		return &ae_player_value;
	return &config_values[index];
}

/* sets a setting for this run only, as its environment variable does (config.toml is not changed) */
int config_set(const char *name, const char *value) { long index = config_setting_index(name); if (index < 0 || !value) return 0; config_value(name, config_settings[index].type); pthread_mutex_lock(&config_lock); config_set_from_text(&config_values[index], config_settings[index].type, value); config_values[index].chosen = 1; config_change_count++; pthread_mutex_unlock(&config_lock); return 1; } /* AE hook */

/* ---------- writing a setting */

/* the longest string config_write takes */
#define CONFIG_MAXIMUM_WRITTEN_STRING 4096

/* sets a setting, for now and in config.toml, from its value as text
("true", "60", "1.5", "all"): its line there is changed (or added, or the
line at its default taken out of its comment, now that the player chose
it), the rest of the file kept as it is */
int config_write(const char *name, const char *value)
{
	const char *dot = strchr(name, '.');
	long index = config_setting_index(name);
	char section[64], key[64], wanted[80], current[64] = "", path[1024];
	struct config_text out = { 0 };
	size_t size = 0, line_size;
	char *text, *line_text;
	const char *line;
	int written = 0, in_section = 0, has_value, succeeded;

	/* (a string as long as a server's rotation, server_rotation.h's
	SERVER_ROTATION_MAXIMUM_TEXT; any other value is a word or a number) */
	if (index < 0 || !dot || (size_t)(dot - name) >= sizeof(section) ||
		strlen(value) > (config_settings[index].type == _config_string ? CONFIG_MAXIMUM_WRITTEN_STRING : 256))
	{
		return 0;
	}
	/* (the line: its key, and its value with each character escaped as
	\uXXXX at most) */
	line_size = strlen(dot + 1) + 6 * strlen(value) + 64;
	line_text = malloc(line_size);
	if (!line_text)
		return 0;
	/* (the file read first, as the other settings are) */
	config_value(name, config_settings[index].type);
	pthread_mutex_lock(&config_lock);
	config_set_from_text(&config_values[index], config_settings[index].type, value);
	/* (a run that only answers its command line, a probe's too, writes
	nothing: the value is this run's alone) */
	if (config_answer_only)
	{
		config_change_count++;
		pthread_mutex_unlock(&config_lock);
		free(line_text);
		return 1;
	}
	snprintf(section, sizeof(section), "%.*s", (int)(dot - name), name);
	snprintf(key, sizeof(key), "%s", dot + 1);
	switch (config_settings[index].type)
	{
	case _config_boolean:
		snprintf(line_text, line_size, "%s = %s\n", key, config_values[index].boolean ? "true" : "false");
		break;
	case _config_integer:
		snprintf(line_text, line_size, "%s = %ld\n", key, config_values[index].integer);
		break;
	case _config_real:
		/* (with its point: TOML reads 1 as an integer) */
		snprintf(line_text, line_size, "%s = %.15g", key, config_values[index].real);
		if (!strpbrk(line_text + strlen(key) + 3, ".en"))
			strcat(line_text, ".0");
		strcat(line_text, "\n");
		break;
	case _config_string:
	{
		char *end = line_text + snprintf(line_text, line_size, "%s = \"", key);
		const char *character;

		/* (a TOML basic string: quotes and backslashes escaped, and control
		characters, so that a value cannot end its line) */
		for (character = config_values[index].string; *character; character++)
		{
			if ((unsigned char)*character < 0x20 || *character == 0x7F)
			{
				end += sprintf(end, "\\u%04x", (unsigned)(unsigned char)*character);
				continue;
			}
			if (*character == '"' || *character == '\\')
				*end++ = '\\';
			*end++ = *character;
		}
		strcpy(end, "\"\n");
		break;
	}
	}
	snprintf(wanted, sizeof(wanted), "%s", section);
	config_path(path, sizeof(path));
	text = config_read_file(path, &size);
	/* (the commented-out line is the one replaced only when there is no
	value) */
	has_value = text && config_text_has_key(text, name, 0);
	for (line = text ? text : ""; *line;)
	{
		const char *end = line + strcspn(line, "\n");
		const char *next = *end ? end + 1 : end;

		if (config_line_section(line, end, current, sizeof(current)))
		{
			/* (leaving the section without the key: it goes at its end) */
			if (in_section && !written)
			{
				config_append(&out, line_text);
				written = 1;
			}
			in_section = !strcmp(current, wanted);
		}
		else if (in_section && !written &&
			(has_value ? config_line_key(line, end, key) : config_line_commented_key(line, end, key)))
		{
			config_append(&out, line_text);
			written = 1;
			line = next;
			continue;
		}
		{
			char *copy = config_copy(line, (size_t)(next - line));

			if (copy)
			{
				config_append(&out, copy);
				free(copy);
			}
		}
		line = next;
	}
	if (!written)
	{
		if (out.length && out.buffer[out.length - 1] != '\n')
			config_append(&out, "\n");
		if (!in_section)
		{
			char header[80];

			snprintf(header, sizeof(header), "\n[%s]\n", section);
			config_append(&out, header);
		}
		config_append(&out, line_text);
	}
	succeeded = out.buffer && config_write_file(path, out.buffer);
	config_change_count++;
	pthread_mutex_unlock(&config_lock);
	free(out.buffer);
	free(text);
	free(line_text);
	ae_config_player_written(name, value); /* AE hook */
	return succeeded;
}

/* the file's lines "# key = <an older default>" for a setting, at the
current default again, so that the file reads as this version writes it
(NULL if there is none) */
static char *config_refresh_commented(const char *text, const struct config_setting *setting)
{
	const char *dot = strchr(setting->name, '.');
	struct config_text out = { NULL, 0, 0 };
	char section[64], current[64] = "", buffer[256];
	const char *line;
	int changed = 0;

	snprintf(section, sizeof(section), "%.*s", (int)(dot - setting->name), setting->name);
	for (line = text; *line;)
	{
		const char *end = line + strcspn(line, "\n");
		const char *next = *end ? end + 1 : end;

		config_line_section(line, end, current, sizeof(current));
		if (!strcmp(current, section) && config_line_commented_key(line, end, dot + 1))
		{
			snprintf(buffer, sizeof(buffer), "# %s = %s\n", dot + 1, setting->default_value);
			config_append(&out, buffer);
			changed = 1;
		}
		else
		{
			char *copy = config_copy(line, (size_t)(next - line));

			if (copy)
				config_append(&out, copy);
			free(copy);
		}
		line = next;
	}
	if (!changed)
	{
		free(out.buffer);
		return NULL;
	}
	return out.buffer;
}

int config_new_defaults_pending(char *text, size_t size)
{
	size_t old;
	int count = 0;

	if (size)
		text[0] = 0;
	/* (the file read first) */
	config_value("game.defaults_answered", _config_integer);
	pthread_mutex_lock(&config_lock);
	for (old = 0; old < NUMBER_OF_CONFIG_OLD_DEFAULTS; old++)
	{
		long index = config_old_defaults[old].change ? config_setting_index(config_old_defaults[old].name) : -1;
		size_t used = strlen(text);

		if (index >= 0 && config_pending[index] && config_pending_old[index] == config_old_defaults[old].default_value)
		{
			snprintf(text + used, size - used, "%s%s", count ? "\n" : "", config_old_defaults[old].change);
			count++;
		}
	}
	pthread_mutex_unlock(&config_lock);
	return count;
}

void config_new_defaults_answer(int use_new)
{
	int pending[NUMBER_OF_CONFIG_SETTINGS];
	const char *old_value[NUMBER_OF_CONFIG_SETTINGS];
	size_t index;
	char path[1024];

	config_value("game.defaults_answered", _config_integer);
	pthread_mutex_lock(&config_lock);
	memcpy(pending, config_pending, sizeof(pending));
	memcpy(old_value, config_pending_old, sizeof(old_value));
	memset(config_pending, 0, sizeof(config_pending));
	pthread_mutex_unlock(&config_lock);
	if (use_new)
	{
		size_t size = 0;
		char *text;

		config_path(path, sizeof(path));
		text = config_read_file(path, &size);
		if (text)
		{
			toml_result_t result = toml_parse(text, (int)size);
			char *changed = NULL;

			/* (the values an older version added beside the commented ones,
			which pinned the old defaults, taken out; the commented lines
			at the old defaults written at the new ones) */
			if (result.ok)
				changed = config_drop_added_defaults(text, result.toptab);
			toml_free(result);
			for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
			{
				if (pending[index])
				{
					char *refreshed = config_refresh_commented(changed ? changed : text, &config_settings[index]);

					if (refreshed)
					{
						free(changed);
						changed = refreshed;
					}
				}
			}
			if (changed && !config_write_file(path, changed))
				platform_log("settings: cannot write %s", path);
			free(changed);
			free(text);
		}
		pthread_mutex_lock(&config_lock);
		for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
		{
			if (pending[index])
			{
				char value[64];

				config_unquote(config_settings[index].default_value, value, sizeof(value));
				config_set_from_text(&config_values[index], config_settings[index].type, value);
				platform_log("settings: %s follows this version's default (%s) now", config_settings[index].name,
					config_settings[index].default_value);
			}
		}
		config_change_count++;
		pthread_mutex_unlock(&config_lock);
	}
	else
	{
		for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
		{
			if (pending[index])
			{
				char value[64];

				config_unquote(old_value[index], value, sizeof(value));
				/* (a value an older version wrote is there already) */
				if (pending[index] == 1)
					config_write(config_settings[index].name, value);
				platform_log("settings: %s = %s is kept", config_settings[index].name, old_value[index]);
			}
		}
	}
	config_write("game.defaults_answered", "1");
}

int config_write_boolean(const char *name, int value)
{
	long index = config_setting_index(name);

	return index >= 0 && config_settings[index].type == _config_boolean && config_write(name, value ? "true" : "false");
}

/* a setting's value as text ("true", "60", "1.5", "all"): 0 if there is no
such setting */
int config_text(const char *name, char *text, size_t size)
{
	long index = config_setting_index(name);
	const struct config_value *value;

	if (index < 0)
		return 0;
	value = config_value(name, config_settings[index].type);
	switch (config_settings[index].type)
	{
	case _config_boolean:
		snprintf(text, size, "%s", value->boolean ? "true" : "false");
		break;
	case _config_integer:
		snprintf(text, size, "%ld", value->integer);
		break;
	case _config_real:
		snprintf(text, size, "%.15g", value->real);
		break;
	case _config_string:
		snprintf(text, size, "%s", value->string ? value->string : "");
		break;
	}
	return 1;
}

void config_folder(char *path, size_t size)
{
	char file[1024];
	char *separator;

	config_path(file, sizeof(file));
	separator = strrchr(file, '/');
#ifndef HALO_ANDROID
	if (!separator || (strrchr(file, '\\') && strrchr(file, '\\') > separator))
		separator = strrchr(file, '\\');
#endif
	if (separator)
		separator[1] = 0;
	else
		file[0] = 0;
	snprintf(path, size, "%s", file);
}

/* ---------- public code */

char *config_file_read(const char *path, size_t *size)
{
	return config_read_file(path, size);
}

/* the settings config.toml changes while the game runs, edited by hand
(Settings writes them through config_write): the volumes, which their
readers read again when config_changes moves (dsound_sdl.c,
sound_manager.c). The rest still take effect from the next start */
static const char *const config_live_settings[] =
{
	"audio.volume",
	"audio.music_volume",
	"audio.effects_volume",
};

enum
{
	/* config.toml's time looked at no more often than this (seconds; the
	C library's clock, which the dedicated server, without SDL, has too) */
	CONFIG_FOLLOW_INTERVAL_SECONDS = 1
};

/* config.toml changed on disk since it was last looked at: its live
settings read again from it (one given by the environment kept), and
config_changes moved if any is different */
static void config_follow_file(void)
{
	static time_t looked_at;
	static posix_ulong modified, modified_nanoseconds, modified_size;
	static int looked;
	static char path[1024];
	time_t now = time(NULL);
	struct posix_file_information info;
	size_t size = 0;
	char *text;
	toml_result_t result;
	size_t live;
	int changed = 0;

	if (!config_loaded || (looked && now - looked_at < CONFIG_FOLLOW_INTERVAL_SECONDS))
		return;
	looked_at = now;
	if (!path[0])
		config_path(path, sizeof(path));
	if (posix_stat(path, &info) != 0)
		return;
	/* (the first look keeps the file as it was read; the size too, for a
	file system whose times are whole seconds) */
	if (!looked || (info.modification_seconds == modified &&
		info.modification_nanoseconds == modified_nanoseconds && info.size_low == modified_size))
	{
		looked = 1;
		modified = info.modification_seconds;
		modified_nanoseconds = info.modification_nanoseconds;
		modified_size = info.size_low;
		return;
	}
	modified = info.modification_seconds;
	modified_nanoseconds = info.modification_nanoseconds;
	modified_size = info.size_low;
	text = config_read_file(path, &size);
	if (!text)
		return;
	result = toml_parse(text, (int)size);
	if (result.ok)
	{
		pthread_mutex_lock(&config_lock);
		for (live = 0; live < sizeof(config_live_settings) / sizeof(config_live_settings[0]); live++)
		{
			long index = config_setting_index(config_live_settings[live]);
			const struct config_setting *setting;
			struct config_value value;

			if (index < 0)
				continue;
			setting = &config_settings[index];
			if (getenv(setting->environment))
				continue;
			memset(&value, 0, sizeof(value));
			config_set_from_text(&value, setting->type, setting->default_value);
			config_set_from_file(&value, setting, result.toptab);
			if (value.real != config_values[index].real)
			{
				config_values[index].real = value.real;
				platform_log("settings: config.toml changed: %s = %g", setting->name, value.real);
				changed = 1;
			}
		}
		if (changed)
			config_change_count++;
		pthread_mutex_unlock(&config_lock);
	}
	toml_free(result);
	free(text);
}

unsigned long config_changes(void)
{
	config_follow_file();
	return config_change_count + ae_config_player_value_changes(); /* AE hook */
}

int config_server_mode_enabled(void)
{
	config_value("server.enabled", _config_boolean);
	return config_server_mode;
}

int config_setting_info(int index, struct config_setting_info *info)
{
	static const char *const types[] = { "boolean", "integer", "real", "string" };
	const struct config_setting *setting;
	const char *environment;

	if (index < 0 || (size_t)index >= NUMBER_OF_CONFIG_SETTINGS)
		return 0;
	setting = &config_settings[index];
	memset(info, 0, sizeof(*info));
	info->name = setting->name;
	info->type = types[setting->type];
	info->environment = setting->environment;
	info->comment = setting->comment;
	config_default(setting->name, info->default_text, sizeof(info->default_text));
	environment = config_environment(setting);
	info->pinned = environment && (setting->environment_style == _environment_value || config_environment_set(environment));
	info->written = (setting->platforms & CONFIG_PLATFORM) != 0;
	return 1;
}

int config_reload(const char *prefix)
{
	char path[1024];
	size_t size = 0;
	size_t length = strlen(prefix);
	char *text;
	toml_result_t result;
	size_t index;

	config_ensure_loaded();
	config_path(path, sizeof(path));
	text = config_read_file(path, &size);
	if (!text)
		return 0;
	result = toml_parse(text, (int)size);
	if (!result.ok)
	{
		platform_log("settings: %s cannot be read again (%s); the settings stay as they were", path, result.errmsg);
		toml_free(result);
		free(text);
		return 0;
	}
	pthread_mutex_lock(&config_lock);
	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const struct config_setting *setting = &config_settings[index];
		const char *environment = config_environment(setting);
		struct config_value value;

		/* (the setting's own: those of the environment stay its) */
		if (strncmp(setting->name, prefix, length) ||
			(environment && (setting->environment_style == _environment_value || config_environment_set(environment))))
		{
			continue;
		}
		memset(&value, 0, sizeof(value));
		if (setting->type == _config_string)
		{
			size_t default_length = strlen(setting->default_value);

			value.string = default_length >= 2 ? config_copy(setting->default_value + 1, default_length - 2) : strdup("");
		}
		else
		{
			config_set_from_text(&value, setting->type, setting->default_value);
		}
		config_set_from_file(&value, setting, result.toptab);
		/* (the old string kept, as config_set_from_text keeps it) */
		config_values[index] = value;
	}
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	/* (what the run forces, forced again: a server started by --server
	stays one whatever the file's server.enabled says, and its router asked
	as its server.upnp now says) */
	if (config_server_mode)
		config_apply_server_mode();
	else if (config_probe_mode)
		config_apply_probe_mode();
#endif
	config_change_count++;
	pthread_mutex_unlock(&config_lock);
	toml_free(result);
	free(text);
	config_check_words();
	return 1;
}

int config_default(const char *name, char *text, size_t size)
{
	long index = config_setting_index(name);
	const char *value;
	size_t length;

	if (index < 0)
		return 0;
	value = config_settings[index].default_value;
	length = strlen(value);
	/* (a string's without its quotes: the defaults have no escapes) */
	if (config_settings[index].type == _config_string && length >= 2 && value[0] == '"')
	{
		value++;
		length -= 2;
	}
	snprintf(text, size, "%.*s", (int)length, value);
	return 1;
}

int config_boolean(const char *name)
{
	return config_value(name, _config_boolean)->boolean;
}

int halo_fixed_step(void)
{
	static int fixed_step = -1;

	if (fixed_step < 0)
		fixed_step = config_boolean("debug.fixed_step") != 0;
	return fixed_step;
}

long config_integer(const char *name)
{
	return config_value(name, _config_integer)->integer;
}

unsigned long network_client_timeout_milliseconds(void)
{
	long seconds = config_integer("network.client_timeout_seconds");

	if (seconds < 5)
		seconds = 5;
	if (seconds > 300)
		seconds = 300;
	return (unsigned long)seconds * 1000UL;
}

double config_real(const char *name)
{
	return config_value(name, _config_real)->real;
}

const char *config_string(const char *name)
{
	const char *string = config_value(name, _config_string)->string;

	return string ? string : "";
}

/* ---------- folders (platform.h) */

int platform_folder_exists(const char *path)
{
	struct posix_file_information information;
	void *opened;

	/* (a folder, and one that opens, as opendir() alone told before) */
	if (posix_stat(path, &information) != 0 || !(information.flags & _posix_file_is_directory))
		return 0;
	opened = posix_directory_open(path);
	if (!opened)
		return 0;
	posix_directory_close(opened);
	return 1;
}

int platform_folder_list(const char *path, void (*visit)(const char *name, void *context), void *context)
{
	void *opened = posix_directory_open(path);
	char name[260];

	if (!opened)
		return 0;
	while (posix_directory_next(opened, name, sizeof(name)))
		visit(name, context);
	posix_directory_close(opened);
	return 1;
}

/* ---------- starting again (Settings > Mods: game.mod) */

#if defined(__linux__) && !defined(HALO_ANDROID) && !defined(__ANDROID__)
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

/* starts the game again in this process, with the arguments it was started
with (/proc/self/cmdline), its files and sockets closed as it does so that the
new game binds the same ports: 0 where it cannot (and returns), as on Windows */
int platform_restart(void)
{
#if defined(__linux__) && !defined(HALO_ANDROID) && !defined(__ANDROID__)
	static char line[8192];
	char *arguments[64];
	int argument_count = 0;
	ssize_t length = 0, got;
	int file = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
	ssize_t index;

	if (file < 0)
		return 0;
	while (length < (ssize_t)sizeof(line) - 1 && (got = read(file, line + length, sizeof(line) - 1 - length)) > 0)
		length += got;
	close(file);
	line[length] = 0;
	for (index = 0; index < length && argument_count < 63; index += (ssize_t)strlen(line + index) + 1)
		arguments[argument_count++] = line + index;
	arguments[argument_count] = NULL;
	if (!argument_count)
		return 0;
	/* (a recording saved, the screenshots written: atexit is not run) */
	capture_shutdown();
	fflush(NULL);
	/* (closed only if the new game starts: one that cannot leaves this one
	as it was) */
#ifdef SYS_close_range
	if (syscall(SYS_close_range, 3u, ~0u, 4u /* CLOSE_RANGE_CLOEXEC */) != 0)
#endif
	{
		for (index = 3; index < 1024; index++)
			fcntl((int)index, F_SETFD, FD_CLOEXEC);
	}
	execv("/proc/self/exe", arguments);
	/* (it did not start: this game goes on, capture with it) */
	capture_resume();
	return 0;
#else
	return 0;
#endif
}
