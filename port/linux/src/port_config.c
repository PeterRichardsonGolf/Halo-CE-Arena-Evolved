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
#include "capture.h"
#include "posix.h"
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

static const struct config_setting config_settings[] =
{
	{ "display.fullscreen", _config_boolean, DEFAULT_FULLSCREEN, "HALO_FULLSCREEN", _environment_value, _platform_desktop,
		"Where display.mode is empty: start borderless over the whole display;\n"
		"false starts in a window. F11 switches." },
	{ "display.mode", _config_string, "\"\"", "HALO_DISPLAY_MODE", _environment_value, _platform_desktop,
		"\"fullscreen\" takes the display (at display.resolution's mode),\n"
		"\"borderless\" is a window over the whole desktop, \"windowed\" a window\n"
		"(display.window_size). Empty: display.fullscreen's (true: borderless).\n"
		"F11 switches to the window and back." },
	{ "display.resolution", _config_string, "\"native\"", "HALO_RESOLUTION", _environment_value, _platform_desktop,
		"What fullscreen and borderless draw at: \"native\", the display's own, or\n"
		"\"<width>x<height>\" (\"1920x1080\"), 640x480 or more. Fullscreen sets the\n"
		"display to it; borderless draws it scaled to the display." },
	{ "display.resolution_scaling", _config_string, "\"native\"", "HALO_RESOLUTION_SCALING", _environment_value,
		_platform_desktop,
		"\"native\" draws at the window's resolution (fullscreen, the display's or\n"
		"display.resolution); \"original\" draws the Xbox's 640x480 and scales it\n"
		"up." },
	{ "display.window_size", _config_string, "\"\"", "HALO_WINDOW_SIZE", _environment_value, _platform_desktop,
		"The window's size, \"<width>x<height>\" (\"1920x1080\"), 640x480 or more (it\n"
		"can be resized). Empty: display.window_scale's." },
	{ "display.window_scale", _config_integer, "2", "HALO_WINDOW_SCALE", _environment_value, _platform_desktop,
		"Where display.window_size is empty: the window's size as a multiple of\n"
		"640x480." },
	{ "display.screen_width", _config_integer, "0", "HALO_SCREEN_WIDTH", _environment_value, _platform_android,
		"Columns of the 480-line picture: 0 for the display's shape, 640 for the\n"
		"Xbox's 4:3." },
	{ "display.vsync", _config_boolean, "true", "HALO_NO_VSYNC", _environment_set_is_false, _platform_all,
		"Wait for the display between frames; false draws as fast as possible." },
	{ "display.max_fps", _config_integer, "0", "HALO_MAX_FPS", _environment_value, _platform_desktop,
		"With vsync off, the most frames a second: 0 for twice the display's\n"
		"refresh rate, -1 for no limit (which can hang some Intel graphics)." },
	{ "display.performance", _config_string, "\"off\"", "HALO_PERFORMANCE", _environment_value, _platform_all,
		"A line of performance at the top of the screen, above the HUD: \"fps\"\n"
		"(the frames a second), \"minimal\" (and the frame time), \"full\" (and\n"
		"the frame rate's 1% low and the draws of a frame), or \"off\"." },
	{ "display.performance_position", _config_string, "\"top_right\"", "HALO_PERFORMANCE_POSITION",
		_environment_value, _platform_all,
		"Where display.performance's line is: \"top_left\" or \"top_right\"." },
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
		"a pixel. Android has \"fxaa\" for \"smaa\", and no \"ssaa2x\"." },
	{ "display.interpolation", _config_boolean, "true", "HALO_INTERPOLATION", _environment_value, _platform_all,
		"Draw a frame for every display refresh, blending between the game's 30\n"
		"ticks a second; false keeps the original 30 frames a second." },
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
	{ "display.match_timer", _config_boolean, "false", "HALO_MATCH_TIMER", _environment_value, _platform_all,
		"In a multiplayer game with a time limit, show the time left at the top\n"
		"of the screen; false shows it only on the scoreboard, as the Xbox did." },
	{ "display.high_res_hud", _config_boolean, "true", "HALO_HIGH_RES_HUD", _environment_value, _platform_all,
		"Draw the HUD (meters, counters, panels, motion sensor, reticles,\n"
		"waypoints, scopes) from the high-res assets (8x the maps' bitmaps);\n"
		"false draws the maps' own bitmaps." },
	{ "display.high_res_text", _config_boolean, "true", "HALO_HIGH_RES_TEXT", _environment_value, _platform_all,
		"Draw the menus' and HUD's text with the fonts in port/assets/fonts\n"
		"(Overpass) at the resolution the game draws at, and the menus' titles\n"
		"from port/assets/titles; false draws the maps' bitmap fonts and titles.\n"
		"The Server Browser and moderation overlays use the system's fonts." },
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
	{ "display.arena_menus", _config_boolean, "false", "HALO_ARENA_MENUS", _environment_value, /* AE hook */
		_platform_all,
		"Arena Evolved's own menus (experimental, not finished): true shows\n"
		"them in place of the game's; false keeps the menus display.menus\n"
		"picks. Read at start-up." },
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
	{ "display.player_names", _config_string, "\"all\"", "HALO_PLAYER_NAMES", _environment_value, _platform_all,
		"In multiplayer, whose names are drawn above their heads: \"all\",\n"
		"\"allies\", \"enemies\" or \"none\". An enemy's shows only within the\n"
		"motion sensor's reach, in sight and not camouflaged; none show if the\n"
		"gametype's motion tracker shows no players, only allies' if it shows\n"
		"only friends." },
	{ "display.player_name_scale", _config_real, "1.0", "HALO_PLAYER_NAME_SCALE", _environment_value, _platform_all,
		"How large the players' names are drawn: 1.0 three quarters of the size of\n"
		"the HUD's text, 0.25 to 4." },
	{ "display.scoreboard_team_layout", _config_string, "\"teams\"", "HALO_SCOREBOARD_TEAM_LAYOUT", _environment_value,
		_platform_all,
		"How the scoreboard lists a team game's players: \"teams\" in a column for\n"
		"each team (red on the left, blue on the right), \"score\" all in order of\n"
		"score." },
	{ "display.scoreboard_background", _config_boolean, "true", "HALO_SCOREBOARD_BACKGROUND", _environment_value,
		_platform_all,
		"Draw a panel behind the multiplayer scoreboard, for clearer text." },
	{ "display.show_quit_players", _config_boolean, "true", "HALO_SHOW_QUIT_PLAYERS", _environment_value,
		_platform_all,
		"Keep players who quit on the multiplayer scoreboard, as the original game\n"
		"did; false leaves them off it, as OpenCE does." },
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
	{ "display.server_messages", _config_boolean, "true", "HALO_SERVER_MESSAGES", _environment_value, _platform_all,
		"Draw the banner for a warning or notice a Delta server sends; false hides\n"
		"it. Either way the message is written to debug.txt." },
	{ "display.server_message_text_color", _config_string, "\"\"", "HALO_SERVER_MESSAGE_TEXT_COLOR",
		_environment_value, _platform_all,
		"The banner's text colour, \"#RRGGBB\" or \"#RRGGBBAA\". Empty (or a value\n"
		"that does not parse): the default, light for a notice and amber for a\n"
		"warning. It applies to both kinds." },
	{ "display.server_message_border_color", _config_string, "\"\"", "HALO_SERVER_MESSAGE_BORDER_COLOR",
		_environment_value, _platform_all,
		"The banner's outline colour, \"#RRGGBB\" or \"#RRGGBBAA\". Empty (or a\n"
		"value that does not parse): the default, blue for a notice and amber for\n"
		"a warning. It applies to both kinds." },
	{ "display.server_message_background_color", _config_string, "\"\"", "HALO_SERVER_MESSAGE_BACKGROUND_COLOR",
		_environment_value, _platform_all,
		"The banner's background colour, \"#RRGGBB\" or \"#RRGGBBAA\". Empty (or a\n"
		"value that does not parse): the default, dark blue." },
	{ "display.per_pixel_lighting", _config_boolean, "false", "HALO_PER_PIXEL_LIGHTING", _environment_value,
		_platform_all,
		"Light the models (characters, weapons, vehicles, scenery) for each\n"
		"pixel by the lights the game gives them, without the facets the light\n"
		"of each vertex shows across curved surfaces; false lights each vertex,\n"
		"as the Xbox does." },

	{ "audio.enabled", _config_boolean, "true", "HALO_NO_AUDIO", _environment_set_is_false, _platform_all,
		"Play sound." },
	{ "audio.volume", _config_real, "1.0", "HALO_VOLUME", _environment_value, _platform_all,
		"The volume of everything, 0.0 to 1.0." },
	{ "audio.music_volume", _config_real, "1.0", "HALO_MUSIC_VOLUME", _environment_value, _platform_all,
		"The music's volume, 0.0 to 1.0 (of audio.volume)." },
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
	{ "audio.reverb", _config_boolean, "true", "HALO_REVERB", _environment_value, _platform_all,
		"Reverberate the world's sounds as the place the player is in does (the\n"
		"maps' sound environments, as the Xbox's I3DL2 reverb did); false keeps\n"
		"them dry." },
	{ "audio.nhe_sound_filter", _config_boolean, "false", "HALO_NHE_SOUND_FILTER", _environment_value, /* AE hook */
		_platform_all,
		"Mute the sounds Halo 1: NHE 1.0 took out of its maps, on any map, for\n"
		"this machine: the plasma pistol's charging hum and the needler's firing\n"
		"(NHE SOUND FILTER on NHE OPTIONS)." },
	{ "audio.resampling", _config_string, "\"sinc\"", "HALO_AUDIO_RESAMPLING", _environment_value, _platform_all,
		"How sounds recorded at another rate (most are 22 kHz) are played at the\n"
		"output's 48 kHz: \"sinc\" keeps their band and nothing above it;\n"
		"\"linear\" interpolates between their samples, as the game did before\n"
		"OpenCE's build 130: their top octave duller, and images of their band\n"
		"above it (a brighter, grainier sound)." },
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
	{ "audio.loose_sounds", _config_boolean, "false", "HALO_LOOSE_SOUNDS", _environment_value, _platform_all,
		"For those making sounds: play each of a map's sounds that has a Halo PC\n"
		"sound tag file of its name under the data root's tags folder\n"
		"(tags/sound/.../name.sound) from that file. At the console,\n"
		"loose_sounds_reload reads the files again and loose_sounds false gives\n"
		"the map's sounds back." },

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
		"How far the view turns for the mouse's movement." },
	{ "input.invert_mouse", _config_boolean, "false", "HALO_MOUSE_INVERT", _environment_set_is_true, _platform_desktop,
		"Moving the mouse forward looks down." },
	{ "input.mouse_aim_assist", _config_boolean, "false", "HALO_MOUSE_AIM_ASSIST", _environment_value, _platform_desktop,
		"Magnetism while aiming with the mouse, as with a controller: the view\n"
		"slowed and dragged along by a target. The last of the mouse and the\n"
		"right stick to move decides. The bullets' autoaim (bent toward the\n"
		"target) stays either way." },
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
	{ "input.zoom_hold_time", _config_real, "0.25", "HALO_ZOOM_HOLD_TIME", _environment_value, /* AE hook */
		_platform_all,
		"The most seconds a press of the zoom action lasts to be a tap in the\n"
		"\"both\" mode, 0.1 to 1: held longer, it is a hold." },

	/* the keyboard and mouse's own controls (port/linux/src/xinput_sdl.c) */
	{ "controls.move_forward", _config_string, "\"W\"", "HALO_KEY_MOVE_FORWARD", _environment_value, _platform_all,
		"The keyboard and mouse's controls, which Settings > Controls Setup\n"
		"changes: up to two keys or buttons each, separated by a comma. Keys by\n"
		"their names on a US keyboard (\"W\", \"Space\", \"Left Ctrl\", \"F1\"): a key\n"
		"is the one in that place on any keyboard, which the menus show by its\n"
		"own label. Buttons: \"Mouse Left\", \"Mouse Right\", \"Mouse Middle\",\n"
		"\"Mouse 4\", \"Mouse 5\", \"Wheel\" (either way), \"Wheel Up\" and \"Wheel\n"
		"Down\"; empty for none. Moving forward:" },
	{ "controls.move_backward", _config_string, "\"S\"", "HALO_KEY_MOVE_BACKWARD", _environment_value, _platform_all,
		"Moving backward." },
	{ "controls.strafe_left", _config_string, "\"A\"", "HALO_KEY_STRAFE_LEFT", _environment_value, _platform_all,
		"Moving left." },
	{ "controls.strafe_right", _config_string, "\"D\"", "HALO_KEY_STRAFE_RIGHT", _environment_value, _platform_all,
		"Moving right." },
	{ "controls.jump", _config_string, "\"Space\"", "HALO_KEY_JUMP", _environment_value, _platform_all,
		"Jumping (and skipping cutscenes)." },
	{ "controls.crouch", _config_string, CROUCH_KEYS, "HALO_KEY_CROUCH", _environment_value, _platform_all,
		"Crouching." },
	{ "controls.fire", _config_string, "\"Mouse Left\"", "HALO_KEY_FIRE", _environment_value, _platform_all,
		"Firing." },
	{ "controls.throw_grenade", _config_string, "\"Mouse Right, G\"", "HALO_KEY_THROW_GRENADE", _environment_value,
		_platform_all,
		"Throwing a grenade." },
	{ "controls.melee", _config_string, "\"F, Mouse 4\"", "HALO_KEY_MELEE", _environment_value, _platform_all,
		"Melee attack." },
	{ "controls.reload", _config_string, "\"R\"", "HALO_KEY_RELOAD", _environment_value, _platform_all,
		"Reloading." },
	{ "controls.zoom", _config_string, "\"Z, Mouse Middle\"", "HALO_KEY_ZOOM", _environment_value, _platform_all,
		"Zooming the scope." },
	{ "controls.switch_weapon", _config_string, "\"Wheel, 1\"", "HALO_KEY_SWITCH_WEAPON", _environment_value,
		_platform_all,
		"Switching weapons." },
	{ "controls.switch_grenade", _config_string, "\"X\"", "HALO_KEY_SWITCH_GRENADE", _environment_value, _platform_all,
		"Switching grenades." },
	{ "controls.action", _config_string, "\"E\"", "HALO_KEY_ACTION", _environment_value, _platform_all,
		"The action: picking up (held: swapping weapons), entering and leaving\n"
		"vehicles, pressing switches; never reloading (the controller's X does\n"
		"when there is nothing to act on)." },
	{ "controls.flashlight", _config_string, "\"Q\"", "HALO_KEY_FLASHLIGHT", _environment_value, _platform_all,
		"The flashlight." },
	{ "controls.scoreboard", _config_string, "\"Tab\"", "HALO_KEY_SCOREBOARD", _environment_value, _platform_all,
		"Showing the scores (the controller's Back)." },
	{ "controls.pause", _config_string, "\"Escape\"", "HALO_KEY_PAUSE", _environment_value, _platform_all,
		"The pause menu (the controller's Start)." },
	{ "controls.screenshot", _config_string, "\"F10\"", "HALO_KEY_SCREENSHOT", _environment_value, _platform_all,
		"Save a PNG screenshot in screenshots/ beside config.toml (press once per\n"
		"capture). F9, the recording's key, cannot be used here." },
	{ "controls.push_to_talk", _config_string, "\"V\"", "HALO_KEY_PUSH_TO_TALK", _environment_value, _platform_all,
		"Voice chat: talk while it is held (audio.voice_chat \"push_to_talk\")." },

	{ "game.console_log", _config_string, "\"important\"", "HALO_CONSOLE_LOG", _environment_value, _platform_all,
		"What the game's console shows on screen of what it logs: \"important\"\n"
		"(bans, players dropped for cheating, what refuses a command, and the\n"
		"asserts that stop the game), \"all\" (every line, the game's own\n"
		"chatter too), or \"none\" (the asserts that stop the game only). What\n"
		"a command prints shows whatever this is, and debug.txt has every line." },

	{ "game.language", _config_string, "\"\"", "HALO_LANGUAGE", _environment_value, _platform_all,
		"The language the game asks the Xbox for: \"ja\", \"de\", \"fr\", \"es\" or \"it\";\n"
		"empty for English. The game data decides what is translated." },
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
		"health packs; \"reach\" once the shields are full, to the top of the third\n"
		"it is in; \"halo3\" once the shields are full, all of it; \"halo2\" Halo 2's\n"
		"timers (shields 5 s then 2 s; health 10 s after the last body damage, then\n"
		"5 s). Multiplayer goes by the gametype's HEALTH." },
	{ "game.mod", _config_string, "\"\"", "HALO_MOD", _environment_value, _platform_desktop,
		"The mod played: a folder of mods/ (next to maps/), whose maps/ holds the\n"
		"maps it replaces (the others are maps/'s); empty for none. Settings >\n"
		"Mods chooses it, and the game starts again with it." },
	{ "game.mcc_path", _config_string, "\"\"", NULL, _environment_value, /* AE hook */
		_platform_desktop,
		"The folder of Halo: The Master Chief Collection (the one holding halo1/)\n"
		"whose Custom Edition files (halo1/maps/custom_edition: bitmaps.map,\n"
		"sounds.map, loc.map) Custom Edition maps read when maps_ce has none; empty:\n"
		"found in Steam's libraries. Settings > MAP FILES' BROWSE\n"
		"sets it. HALO_MCC_PATH names one for a run (after this one). The files\n"
		"are only read, never changed or copied." },
	{ "game.mcc_use", _config_string, "\"ask\"", "HALO_MCC_USE", _environment_value, /* AE hook */
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
		"named for what they say (one.wav, rockets.wav, ...); the mod played's\n"
		"own mods/<mod>/voices/<folder>/ clips come first. With no such folder,\n"
		"the first folder of voices/ speaks (this setting stays)." },

	{ "game.unlock_all_levels", _config_boolean, "false", "HALO_UNLOCK_ALL_LEVELS", _environment_value, _platform_all,
		"true: a profile made from now on starts with every campaign level\n"
		"finished on every difficulty, so any level can be played. false: a new\n"
		"profile starts at The Pillar of Autumn and opens each level as the one\n"
		"before it is finished, as on the Xbox. Profiles already made keep their\n"
		"progress; the console's profile_reset_campaign starts one over." },

	{ "game.downloaded_maps", _config_string, "\"\"", "HALO_DOWNLOADED_MAPS", _environment_value, _platform_all,
		"Maps played as downloaded ones (until the game downloads maps itself):\n"
		"their names, as the game names them (bloodgulch, hugeass@ce), separated\n"
		"by commas, or \"*\" for every map. A downloaded map's scripts may not\n"
		"change the player's settings or other players' games (debug.txt names\n"
		"what they were refused)." },

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
		"Computer players (bots) in a local (split screen) Slayer game: 0 to 15\n"
		"join its lobby with the players, as far as the game has room; up to 3\n"
		"in a LAN game you host, none in internet games. 0: none, as in the stock\n"
		"game." },
	{ "bots.skill", _config_string, "\"normal\"", "HALO_BOT_SKILL", _environment_value, _platform_all, /* AE hook */
		"How well the bots play: \"recruit\" (for new players), \"easy\", \"normal\",\n"
		"\"heroic\" or \"legendary\" (how soon they react, how well they aim, how\n"
		"far they see)." },
	{ "bots.shooting_skill", _config_string, "\"same\"", "HALO_BOT_SHOOTING_SKILL", _environment_value, _platform_all, /* AE hook */
		"SHOOTING SKILL: the bots' aim, reactions, sight, grenades, blows and\n"
		"headshots at a level of their own: \"recruit\" .. \"legendary\", or\n"
		"\"same\" (the default) for BOT SKILL's." },
	{ "bots.movement_skill", _config_string, "\"same\"", "HALO_BOT_MOVEMENT_SKILL", _environment_value, _platform_all, /* AE hook */
		"MOVEMENT SKILL: the bots' strafing, jumping, crouching, closing in and\n"
		"retreating at a level of their own: \"recruit\" .. \"legendary\", or\n"
		"\"same\" (the default) for BOT SKILL's." },
	{ "bots.teams", _config_string, "\"even\"", "HALO_BOT_TEAMS", _environment_value, _platform_all,
		"In team games: \"even\" puts the bots on both teams, evening them with\n"
		"the players; \"against\" puts them all on the team the players are not on." },
	{ "bots.playfight", _config_boolean, "false", "HALO_BOT_PLAYFIGHT", _environment_value, _platform_all, /* AE hook */
		"PLAYFIGHT, as Halo Infinite's Training Mode: the bots move, strafe,\n"
		"jump and aim at you, but never shoot, throw a grenade or strike (aim\n"
		"practice for new players). Takes effect at once, in game too." },
	{ "display.viewmodel_shield", _config_boolean, "false", "HALO_VIEWMODEL_SHIELD", _environment_value, _platform_all,
		"The energy shield's flare on the first-person arms too, as on the body\n"
		"(the stock game draws it on the body only)." },
	{ "display.first_person_legs", _config_boolean, "false", "HALO_FIRST_PERSON_LEGS", _environment_value, _platform_all,
		"Your own legs in first person, seen looking down: your body as others\n"
		"see it, drawn from the waist down, moving as it moves." },
	{ "display.shield_glow", _config_boolean, "false", "HALO_SHIELD_GLOW", _environment_value, _platform_all,
		"Energy shields light what is around them as they flare, as plasma\n"
		"does, in the shield's own color." },
	{ "display.shield_glow_intensity", _config_string, "\"default\"", "HALO_SHIELD_GLOW_INTENSITY", _environment_value, _platform_all,
		"How far the shield glow reaches: \"default\" or \"light_show\" (three\n"
		"times as far, and at full strength from a third of the flare)." },
	{ "display.shield_color", _config_string, "\"default\"", "HALO_SHIELD_COLOR", _environment_value, _platform_all,
		"Your energy shield's color, as this machine draws it: \"default\" (the\n"
		"shield's own) or a multiplayer armor color: white, black, red, blue,\n"
		"gray, yellow, green, pink, purple, cyan, cobalt, orange, teal, sage,\n"
		"brown, tan, maroon or salmon." },

	{ "paths.data", _config_string, "\"\"", "HALO_DATA_ROOT", _environment_value, _platform_desktop,
		"The folder holding the game data's maps folder; empty looks in the\n"
		"working directory and its assets folder. Windows paths are easiest in\n"
		"single quotes: 'C:\\Games\\Halo'." },
	{ "paths.saves", _config_string, "\"\"", "HALO_SAVE_ROOT", _environment_value, _platform_desktop,
		"Where saved games and profiles go; empty for the usual place\n"
		"(~/.local/share/halo-linux, or %APPDATA%\\halo on Windows)." },

	{ "network.address", _config_string, "\"\"", "HALO_NET_ADDRESS", _environment_value, _platform_all,
		"This machine's IPv4 address for system link, for a machine on several\n"
		"networks; empty chooses one." },
	{ "network.broadcast", _config_string, "\"\"", "HALO_NET_BROADCAST", _environment_value, _platform_all,
		"Comma-separated IPv4 addresses system link sends its announcements to\n"
		"instead of the local network's broadcast address (for VPNs); empty for\n"
		"the local network." },
	{ "network.online", _config_boolean, "true", "HALO_NET_ONLINE", _environment_value, _platform_all,
		"Internet play: hosting makes an invite link (logged, and put on the\n"
		"clipboard) that lets whoever has it join over the internet; opening a\n"
		"link (or copying one before switching to the game) joins. Only people\n"
		"with the invite can join. Off keeps system link to the local network." },
	{ "network.join_from_clipboard", _config_boolean, "true", "HALO_NET_JOIN_FROM_CLIPBOARD", _environment_value,
		_platform_all,
		"Join the game of an invite link found on the clipboard when the game\n"
		"comes to the front." },
	{ "network.map_downloads", _config_string, "\"ask\"", "HALO_NET_MAP_DOWNLOADS", _environment_value,
		_platform_all,
		"A Custom Edition map a game you join needs and you don't have, from the\n"
		"community map repositories (maps.halonet.net and its mirrors, as\n"
		"Chimera downloads from): \"ask\" offers to download it, \"always\"\n"
		"downloads it without asking, \"never\" only says which map is missing.\n"
		"A downloaded map is checked (its size, its CRC-32, its header, its name\n"
		"and the host's version of it) before it goes in maps_ce." },
	{ "network.tunnel_port", _config_integer, "0", "HALO_NET_TUNNEL_PORT", _environment_value, _platform_all,
		"The UDP port internet play uses; 0 picks one. A fixed one can be\n"
		"forwarded on the router, for networks whose NAT stops connections." },
	{ "network.allow_upnp", _config_boolean, "true", "HALO_NET_ALLOW_UPNP", _environment_value, _platform_all,
		"Let internet play ask the router (UPnP) to forward its port, for\n"
		"networks whose NAT stops connections: when a player joins this\n"
		"machine's game, and when joining a game takes too long. False never\n"
		"asks." },
	{ "network.protocol", _config_string, "\"auto\"", "HALO_NET_PROTOCOL", _environment_value, _platform_all,
		"The protocol between ChupathingyCE machines beside OpenCE's game\n"
		"protocol (Delta Peer, docs/delta.md): \"auto\" speaks Delta with the\n"
		"machines that do and plain OpenCE with the rest (each connection\n"
		"falls back on its own, and nobody waits for it); \"opence\" turns\n"
		"Delta off (OpenCE's protocol alone, as an OpenCE build). \"delta\"\n"
		"plays as auto for now: Delta-only games come later." },
	{ "network.share_profile", _config_boolean, "false", "HALO_NET_SHARE_PROFILE", _environment_value,
		_platform_all,
		"Show the other ChupathingyCE players of a game this copy's player ID\n"
		"(the game list's, which links to its profile), over Delta. Off by\n"
		"default: the ID is the same in every game." },
	{ "network.platform_limits", _config_string, "\"on\"", "HALO_NET_PLATFORM_LIMITS", _environment_value,
		_platform_all,
		"Delta's platform limits for this machine: \"on\" has hosts keep a\n"
		"game to the players this platform takes (an original Xbox: 16);\n"
		"\"off\" joins games of any size the host runs (you can roast your\n"
		"Xbox with 128 players if you want). Only with Delta hosts." },
	{ "network.host_platform_limits", _config_boolean, "true", "HALO_NET_HOST_PLATFORM_LIMITS", _environment_value,
		_platform_all,
		"Whether a game this machine hosts keeps to the players its Delta\n"
		"machines' platforms take (their platform limits); false ignores\n"
		"them, for testing." },
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
		"65536), so that the game's own traffic keeps the rest of the upload." },
	{ "network.map_seed", _config_boolean, "true", "HALO_NET_MAP_SEED", _environment_value, _platform_all,
		"Playing in another's game: help send its map to players who join\n"
		"without it, as a torrent's peers do (the pieces this machine has, at\n"
		"most network.map_seed_upload)." },
	{ "network.map_seed_upload", _config_integer, "256", "HALO_NET_MAP_SEED_UPLOAD", _environment_value,
		_platform_all,
		"Playing in another's game: the most kilobytes a second this machine\n"
		"sends of maps to other players (16 to 65536)." },
	{ "network.map_download", _config_boolean, "true", "HALO_NET_MAP_DOWNLOAD", _environment_value, _platform_all,
		"Joining a game on a Custom Edition map this machine does not have:\n"
		"download it (and the resource maps it needs) from the game's players\n"
		"into maps_ce, if its host shares it (network.map_share); false\n"
		"leaves the game, saying which map is missing." },
	{ "network.map_share_internet", _config_boolean, "false", "HALO_NET_MAP_SHARE_INTERNET", _environment_value, _platform_all, /* AE hook */
		"Hosting: also send maps to players who are not on the local network\n"
		"(an internet game's). Off, they get no map from this host; players on\n"
		"the local network get them as network.map_share says." },
	{ "network.map_download_internet", _config_boolean, "false", "HALO_NET_MAP_DOWNLOAD_INTERNET", _environment_value, _platform_all, /* AE hook */
		"Joining: also download maps from a host that is not on the local\n"
		"network (an internet game's). Off, only a host on the local network is\n"
		"asked for maps (network.map_download)." },
	{ "network.map_download_limit", _config_integer, "4096", "HALO_NET_MAP_DOWNLOAD_LIMIT", _environment_value, _platform_all, /* AE hook */
		"Joining: the most megabytes of maps taken from one host (0 to 65536)." },
	{ "debug.map_share_test", _config_string, "\"\"", "HALO_MAP_SHARE_TEST", _environment_value, _platform_all, /* AE hook */
		"Tests: the player's answer to map sharing's question, given at once:\n"
		"\"yes\", \"no\", \"all\" (yes, and all of the host's maps fetched in the\n"
		"lobby), \"internet\" (as all, the host taken for an internet one). A host's:\n"
		"\"lie-size\", \"lie-hash\" (its list is not what it offers), \"lie-map\" (it\n"
		"lists files whose headers are not their names'). Read only in a network\n"
		"test (debug.network_test)." },
	{ "network.coop_public", _config_boolean, "false", "HALO_NET_COOP_PUBLIC", _environment_value, _platform_all,
		"Whether an online co-op game (Create Game > Internet, a SINGLEPLAYER\n"
		"map) starts as PUBLIC or, false, PRIVATE: Server Setup's LISTING in\n"
		"co-op, which writes its choice here." },
	{ "network.coop_friendly_fire", _config_string, "\"on\"", "HALO_NET_COOP_FRIENDLY_FIRE", _environment_value,
		_platform_all,
		"Whether the players of an online co-op game hurt each other: \"off\",\n"
		"\"on\", \"shields_only\" or \"explosives_only\" (FRIENDLY FIRE in\n"
		"co-op's Server Setup > Co-op Options writes its choice here). Their AI\n"
		"allies they always can, as in the campaign." },
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
	{ "network.coop_enemies", _config_integer, "50", "HALO_NET_COOP_ENEMIES", _environment_value, _platform_all,
		"Online co-op's extra enemies per player, a percentage: for each player\n"
		"past the first, each squad of enemies a level places gets this much of\n"
		"itself more (100: as many again; 25 to 200). PER PLAYER in co-op's\n"
		"Server Setup > Co-op Options writes its choice here." },
	{ "network.coop_enemies_multiplier", _config_integer, "2", "HALO_NET_COOP_ENEMIES_MULTIPLIER", _environment_value,
		_platform_all,
		"Online co-op's static multiplier of its enemies: each squad of enemies\n"
		"a level places is this many times as large (2 to 32). MULTIPLIER in\n"
		"co-op's Server Setup > Co-op Options writes its choice here." },
	{ "network.brokers_file", _config_string, "\"brokers.txt\"",
		"HALO_NET_BROKERS_FILE", _environment_value, _platform_all,
		"The file of the public MQTT brokers through which the machines of an\n"
		"invite find each other (its messages are encrypted), beside this file\n"
		"unless a full path: one host:port on each line, up to 4. Updates\n"
		"replace brokers.txt: keep a list of your own under another name." },
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
		"For testing, and for admins who know better: a legacy table file\n"
		"(docs/delta.md, \"The legacy table as config\"), beside this file unless a\n"
		"full path, whose row for this build's wire sets the OpenCE network\n"
		"versions it announces and joins. It is not signed: it replaces the\n"
		"signed tables, which are then neither fetched nor passed on, and the\n"
		"log says so at start. Empty for none." },
#ifdef HALO_GAME_BROWSER
	{ "network.browser_url", _config_string, "\"https://halo.milenko.org\"", "HALO_NET_BROWSER", _environment_value,
		_platform_all,
		"The game list server (configure.py --game-browser): hosted games are\n"
		"listed there, and System Link shows its games; empty for none." },
	{ "network.list_hosted_games", _config_boolean, "false", "HALO_NET_LIST_GAMES", _environment_value, _platform_all,
		"List the system link games this machine hosts on network.browser_url,\n"
		"where anyone can find and join them, and send each one's carnage report\n"
		"there when it ends. False (this build's default: an opt-in, Settings >\n"
		"Network) keeps them to invites and the local network, and sends no report." },
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
		"A dedicated server's Delta Stats token, from the game list's operator:\n"
		"its games count as a trusted server's. Empty: the game must be listed\n"
		"there, from this machine, for its events to be taken." },
	{ "network.events_positions", _config_integer, "2", "HALO_EVENTS_POSITIONS", _environment_value, _platform_all,
		"Delta Stats: seconds between the samples of each player's position\n"
		"(1 to 60; 0 none). Fewer make smaller batches and coarser heatmaps." },
	{ "network.events_limit", _config_integer, "40000", "HALO_EVENTS_LIMIT", _environment_value, _platform_all,
		"Delta Stats: the most events a game keeps (64 to 200000, about 60\n"
		"bytes each); past it the position samples thin out first." },
	{ "network.events_part_minutes", _config_integer, "30", "HALO_EVENTS_PART_MINUTES", _environment_value,
		_platform_all,
		"Delta Stats: a long game is sent as it stands every this many minutes\n"
		"too (the end replaces it); 0 only at its end." },
	{ "network.events_folder", _config_string, "\"\"", "HALO_EVENTS_FOLDER", _environment_value, _platform_all,
		"Delta Stats: a folder to keep a copy of each batch sent, a JSON file each\n"
		"(a full path is best: a relative one is from the working folder); empty\n"
		"for none." },
#endif
	{ "discord.application_id", _config_string, "\"1553978809840050229\"", "HALO_DISCORD_APPLICATION",
		_environment_value, _platform_desktop,
		"The Discord application internet play invites go through while the\n"
		"Discord desktop client runs; empty for none." },

	{ "update.auto", _config_boolean, "true", "HALO_UPDATE_AUTO", _environment_value, _platform_all,
		"No effect: Arena Evolved never looks for a new version (its updater is\n"
		"off). Where the updater is on: look for a new version when the game\n"
		"starts, and offer to update to it; false never looks." },
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
		"co-op, if it can); empty for none." },
	{ "debug.network_test_start", _config_real, "15.0", "HALO_NETWORK_TEST_START", _environment_value, _platform_all,
		"Seconds after hosting that an automated test game starts." },
	{ "debug.network_test_kill", _config_real, "0.0", "HALO_NETWORK_TEST_KILL", _environment_value, _platform_all,
		"Every this many seconds an automated test host's own player (else the first)\n"
		"kills the last other player; 0 never." },
	{ "debug.network_test_score", _config_integer, "0", "HALO_NETWORK_TEST_SCORE", _environment_value, _platform_all,
		"The score an automated test host's game type plays to (a short game, to\n"
		"test the next); 0 the game type's own." },
	{ "debug.network_test_time_limit", _config_integer, "0", "HALO_NETWORK_TEST_TIME_LIMIT", _environment_value,
		_platform_all,
		"The time limit, in minutes, of an automated test host's game type (the\n"
		"match clock's time left); 0 the game type's own." },
	{ "debug.network_test_shoot", _config_real, "0.0", "HALO_NETWORK_TEST_SHOOT", _environment_value, _platform_all,
		"Every this many seconds each automated test player hits the next with\n"
		"their weapon, within its reach (the host brings far players near the\n"
		"first a second before); 0 never." },
	{ "debug.network_test_vehicle", _config_real, "0.0", "HALO_NETWORK_TEST_VEHICLE", _environment_value, _platform_all,
		"This many seconds into an automated test game the host seats its last\n"
		"player as a vehicle's driver (and out 15 seconds on); 0 never." },
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
		"and a pistol, the fallbacks). Empty for none." },
	{ "debug.network_test_pack_map_spawns", _config_boolean, "false", "HALO_NETWORK_TEST_PACK_MAP_SPAWNS", _environment_value, /* AE hook */
		_platform_all,
		"An automated test host's gametype uses the weapon pack and has BATTLE RIFLE\n"
		"ON MAP (the map's assault rifles spawn the pack's weapon)." },
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
		"shield_down, overshield1, overshield2, camo, buffs, health_low)." },
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
	{ "debug.weapon_log", _config_boolean, "false", "HALO_WEAPON_LOG", _environment_value, /* AE hook */
		_platform_all,
		"For automated tests of the battle rifle (ae_burst_rifle.c): each round it\n"
		"fires (its burst, the tick, the magazine), each burst's distance from the\n"
		"last, and the zoomed view's angle. Any machine; only this machine's log." },
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
		"hud (a HUD message), start, pickup, kill, death, postgame. A kind may need a\n"
		"text: hud:Spree." },
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
	{ "debug.network_test_all_weapons", _config_boolean, "false", "HALO_NETWORK_TEST_ALL_WEAPONS", _environment_value, /* AE hook */
		_platform_all,
		"An automated test host runs cheat_all_weapons for its first local player a second after\n"
		"each spawn (the map's weapon list around them), and logs the list." },
	{ "debug.network_test_quit", _config_real, "0.0", "HALO_NETWORK_TEST_QUIT", _environment_value, _platform_all,
		"This many seconds into an automated test game controller 1's player\n"
		"quits, as their pause menu's QUIT does (split screen: the others stay); 0 never." },
	{ "debug.network_test_pickup_weapon", _config_string, "\"\"", "HALO_NETWORK_TEST_PICKUP_WEAPON", _environment_value,
		_platform_all,
		"The weapon network_test_pickup stands the player on: the first whose tag\n"
		"name has this in it (\"sniper\", say); empty any." },
	{ "debug.network_test_public", _config_boolean, "false", "HALO_NETWORK_TEST_PUBLIC", _environment_value,
		_platform_all,
		"The network test's host lists its game in the server browser, as Create\n"
		"Game > Internet's PUBLIC does (debug.network_test \"browse\" joins it)." },
	{ "debug.telnet_console", _config_boolean, "false", "HALO_TELNET_CONSOLE", _environment_set_is_true, _platform_all,
		"Listen on 127.0.0.1 (port telnet_console_port) for a script console that\n"
		"runs what it is sent as the game's console does, with no password; false\n"
		"none." },
	{ "debug.telnet_console_port", _config_integer, "2323", "HALO_TELNET_CONSOLE_PORT", _environment_value,
		_platform_all,
		"The port of the script console (telnet_console); the Xbox's was 23, which\n"
		"only the administrator can listen on." },
	{ "debug.log_addresses", _config_boolean, "false", "HALO_LOG_ADDRESSES", _environment_set_is_true, _platform_all,
		"Write other machines' internet addresses whole in debug.txt and the\n"
		"console, for debugging your own network. False writes each public\n"
		"address as a tag (addr#3f2a9c) that only matches up within one run;\n"
		"local network addresses are always whole. Do not post a debug.txt\n"
		"written with this on: it has the address of everyone you played with." },
	{ "debug.touch_targets", _config_boolean, "false", "HALO_TOUCH_TARGETS", _environment_set_is_true, _platform_all,
		"Outline the menus' tap targets (item green, value blue, list slot yellow,\n"
		"legend button red, the band beside a list's slots orange; the virtual\n"
		"keyboard's keys white), mark where the last finger went down and the\n"
		"last tap landed for 3 seconds, and log each tap with the target it hit\n"
		"(and a value's split); to judge touch accuracy." },
	{ "debug.solo_game", _config_boolean, "false", "HALO_SOLO_GAME", _environment_set_is_true, _platform_all,
		"Let a system link or split screen game start with one player (to test\n"
		"multiplayer maps without a second machine)." },
	{ "debug.network_latency", _config_real, "0.0", "HALO_NETWORK_LATENCY", _environment_value, _platform_all,
		"Milliseconds everything received is held back (a round trip between two\n"
		"machines of twice it), to test the netcode as over the internet; 0 none." },
	{ "debug.network_loss", _config_real, "0.0", "HALO_NETWORK_LOSS", _environment_value, _platform_all,
		"Percent of datagrams received that are dropped, for the same; 0 none." },
	{ "debug.network_corrupt", _config_real, "0.0", "HALO_NETWORK_CORRUPT", _environment_value, _platform_all,
		"Percent of the datagrams received that are damaged at random, to test\n"
		"that nothing a machine sends can crash the game; 0 none." },
	{ "debug.network_corrupt_stream", _config_real, "0.0", "HALO_NETWORK_CORRUPT_STREAM", _environment_value,
		_platform_all,
		"Percent of the reads of streams that are damaged at random, for the\n"
		"same (a damaged stream is closed, so a little goes a long way); 0 none." },
	{ "debug.network_corrupt_after", _config_real, "0.0", "HALO_NETWORK_CORRUPT_AFTER", _environment_value,
		_platform_all,
		"Seconds after the start before anything is damaged, so that a game can\n"
		"be set up and started first (a host's messages to its own client are\n"
		"damaged too)." },
	{ "debug.test_input", _config_string, "\"\"", "HALO_TEST_INPUT", _environment_value, _platform_all,
		"\"bot:<seed>\" plays controller 1 with a scripted pattern (automated\n"
		"network tests); \"look:<seed>\" stands still, only turning and looking\n"
		"up and down; empty for none." },
	{ "debug.bot_trace", _config_integer, "0", "HALO_BOT_TRACE", _environment_value, _platform_all,
		"Log each bot's state (bots.count) every this many seconds: where it is\n"
		"and goes, its path, target and weapon; 0 never." },
	{ "debug.update_answer", _config_string, "\"\"", "HALO_UPDATE_ANSWER", _environment_value, _platform_desktop,
		"The answer to the new version question, for automated tests: \"yes\",\n"
		"\"no\" or \"never\" (do not ask again, confirmed); empty asks." },
	{ "debug.exit_after", _config_real, "0.0", "HALO_EXIT_AFTER", _environment_value, _platform_all,
		"Quit this many seconds after the window opens; 0 never." },
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
	{ "debug.gpu_flush_draws", _config_integer, "-1", "HALO_GPU_FLUSH_DRAWS", _environment_value, _platform_desktop,
		"Flush the GPU's pipeline every this many draws: -1 for every 3 on Intel\n"
		"graphics with Mesa's driver (which can hang without), 0 never." },
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
	{ "debug.texture_dump_directory", _config_string, "\"\"", "HALO_TEXTURE_DUMP", _environment_value, _platform_all,
		"A folder to write every texture to as it is uploaded; empty none." },
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
#endif
};

#define NUMBER_OF_CONFIG_SETTINGS (sizeof(config_settings) / sizeof(config_settings[0]))

#ifdef HALO_ANDROID
#define CONFIG_PLATFORM _platform_android
#elif defined(HALO_WEB)
#define CONFIG_PLATFORM (_platform_desktop | _platform_web)
#elif defined(_WIN32)
#define CONFIG_PLATFORM (_platform_desktop | _platform_windows)
#else
#define CONFIG_PLATFORM _platform_desktop
#endif

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
static int config_loaded = 0;
static pthread_mutex_t config_lock = PTHREAD_MUTEX_INITIALIZER;

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
#else
	(void)path;
	(void)size;
	return 0;
#endif
}

static void config_path(char *path, size_t size)
{
#ifdef __APPLE__
	char folder[1024];
	const char *save_root = getenv("HALO_SAVE_ROOT");

	/* (a copy given a save root of its own, a test's, keeps its settings
	there too, not in the player's) */
	if (save_root && *save_root)
	{
		snprintf(path, size, "%s/config.toml", save_root);
		return;
	}
	/* (the application's folder: not the application, which is signed) */
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
	/* the executable's folder, with its separator */
	const char *base = SDL_GetBasePath();

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
#else
	return SDL_SaveFile(path, text, strlen(text));
#endif
}

/* the file's layout: 2 writes a setting at its default commented out (1,
before config_version, wrote every value, which kept a default a later
version changed) */
#define CONFIG_VERSION 2
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
			config_text_has_key(current, setting->name, 1))
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
		else if (datum.type == TOML_BOOLEAN && setting->platforms == _platform_none)
		{
			/* (an earlier version's string setting that was true or false
			before that, read as the text: config_carry_over) */
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
} config_old_defaults[] =
{
	/* upstream's, which ChupathingyCE kept until v0.5.2b */
	{ "display.menus", "\"pc\"" },
	/* ChupathingyCE's from v0.5.2b, before Arena Evolved went back to "pc" */
	{ "display.menus", "\"xbox\"" },
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
			if (toml_seek(table, setting->name).type != TOML_UNKNOWN)
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
version's default (the player may have chosen it: display.menus = "pc"),
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
	size_t index;

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

/* ---------- words */

/* the settings that take one of a few words: one in any case is taken (as
written here), and another is said once in the log */
static const struct
{
	const char *name;
	/* NULL ended; "" where empty is one */
	const char *words[8];
	/* what the game does with another, for the log (NULL: what reads the
	setting says it) */
	const char *otherwise;
} config_words[] =
{
	{ "display.mode", { "", "fullscreen", "borderless", "windowed", NULL }, "display.fullscreen's" },
	{ "display.resolution_scaling", { "native", "original", NULL }, "\"native\"" },
	{ "display.anti_aliasing", { "off", "fxaa", "smaa", "ssaa2x", "msaa2x", "msaa4x", "msaa8x", NULL }, NULL },
	{ "display.menus", { "xbox", "pc", NULL }, NULL },
	{ "display.player_names", { "all", "allies", "allys", "enemies", "none", NULL }, "\"all\"" },
	{ "display.scoreboard_team_layout", { "teams", "score", NULL }, "\"teams\"" },
	{ "audio.resampling", { "sinc", "linear", NULL }, "\"sinc\"" },
	{ "input.touch_controls", { "on", "auto", "off", NULL }, "\"on\"" },
	{ "game.console_log", { "important", "all", "none", NULL }, "\"important\"" },
	{ "game.move_old_map_folders", { "ask", "yes", "no", NULL }, "\"ask\"" },
	{ "network.protocol", { "auto", "opence", "delta", NULL }, NULL },
	{ "network.map_downloads", { "ask", "always", "never", NULL }, "\"ask\"" },
	{ "network.platform_limits", { "on", "off", NULL }, "\"on\"" },
	{ "network.coop_friendly_fire", { "off", "on", "shields_only", "explosives_only", NULL }, NULL },
	{ "network.coop_enemies_mode", { "none", "per_player", "multiplier", NULL }, "\"none\"" },
	{ "crash_reports.upload", { "ask", "yes", "no", NULL }, "\"ask\"" },
};

#define NUMBER_OF_CONFIG_WORDS (sizeof(config_words) / sizeof(config_words[0]))

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
		char list[160] = "";
		size_t word;

		if (index < 0 || !(config_settings[index].platforms & CONFIG_PLATFORM))
			continue;
		value = &config_values[index];
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
	if (number != line_number || !*line)
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

#ifndef HALO_ANDROID
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
#ifndef HALO_ANDROID
	config_report_unread(path);
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

			if (updated)
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
			for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
			{
				config_set_from_file(&config_values[index], &config_settings[index], result.toptab);
				config_given[index] = toml_seek(result.toptab, config_settings[index].name).type != TOML_UNKNOWN;
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
			else if ((completed || updated || sections_joined) && !config_write_file(path, completed ? completed : text))
			{
				platform_log("settings: cannot write %s", path);
			}
			else if (sections_joined)
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
	else
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
		const char *environment = setting->environment ? getenv(setting->environment) : NULL;

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

static const struct config_value *config_value(const char *name, enum config_type type)
{
	static const struct config_value none = { 0, 0, 0.0, "" };
	long index;

	pthread_mutex_lock(&config_lock);
	if (!config_loaded)
	{
		config_load();
		config_loaded = 1;
	}
	pthread_mutex_unlock(&config_lock);
	index = config_setting_index(name);
	if (index < 0 || config_settings[index].type != type)
	{
		platform_log("settings: no %s setting %s", type == _config_string ? "string" : "such", name);
		return &none;
	}
	return &config_values[index];
}

/* ---------- writing a setting */

/* sets a setting, for now and in config.toml, from its value as text
("true", "60", "1.5", "all"): its line there is changed (or added, or the
line at its default taken out of its comment, now that the player chose
it), the rest of the file kept as it is */
int config_write(const char *name, const char *value)
{
	const char *dot = strchr(name, '.');
	long index = config_setting_index(name);
	char section[64], key[64], wanted[80], current[64] = "", line_text[600], path[1024];
	struct config_text out = { 0 };
	size_t size = 0;
	char *text;
	const char *line;
	int written = 0, in_section = 0, has_value, succeeded;

	if (index < 0 || !dot || (size_t)(dot - name) >= sizeof(section) || strlen(value) > 256)
		return 0;
	/* (the file read first, as the other settings are) */
	config_value(name, config_settings[index].type);
	pthread_mutex_lock(&config_lock);
	config_set_from_text(&config_values[index], config_settings[index].type, value);
	snprintf(section, sizeof(section), "%.*s", (int)(dot - name), name);
	snprintf(key, sizeof(key), "%s", dot + 1);
	switch (config_settings[index].type)
	{
	case _config_boolean:
		snprintf(line_text, sizeof(line_text), "%s = %s\n", key, config_values[index].boolean ? "true" : "false");
		break;
	case _config_integer:
		snprintf(line_text, sizeof(line_text), "%s = %ld\n", key, config_values[index].integer);
		break;
	case _config_real:
		/* (with its point: TOML reads 1 as an integer) */
		snprintf(line_text, sizeof(line_text), "%s = %.15g", key, config_values[index].real);
		if (!strpbrk(line_text + strlen(key) + 3, ".en"))
			strcat(line_text, ".0");
		strcat(line_text, "\n");
		break;
	case _config_string:
	{
		char *end = line_text + snprintf(line_text, sizeof(line_text), "%s = \"", key);
		const char *character;

		for (character = config_values[index].string; *character; character++)
		{
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
	return succeeded;
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

unsigned long config_changes(void)
{
	return config_change_count;
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

long config_integer(const char *name)
{
	return config_value(name, _config_integer)->integer;
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
