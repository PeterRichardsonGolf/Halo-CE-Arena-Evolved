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
	{ "display.high_res_hud", _config_boolean, "true", "HALO_HIGH_RES_HUD", _environment_value, _platform_all,
		"Draw the HUD (meters, counters, panels, motion sensor, reticles,\n"
		"waypoints, scopes) from the high-res assets (8x the maps' bitmaps);\n"
		"false draws the maps' own bitmaps." },
	{ "display.high_res_text", _config_boolean, "true", "HALO_HIGH_RES_TEXT", _environment_value, _platform_all,
		"Draw the menus' and HUD's text with the fonts in port/assets/fonts\n"
		"(Overpass) at the resolution the game draws at, and the menus' titles\n"
		"from port/assets/titles; false draws the maps' bitmap fonts and titles." },
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
	{ "display.short_messages", _config_boolean, "false", "HALO_SHORT_MESSAGES", _environment_value, _platform_all,
		"Shorter game messages, as Halo 1: NHE's: a killing spree reads\n"
		"\"Killing Spree!\" (with the count, if there is one). false keeps the\n"
		"game's own text." },
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

	{ "input.touch_controls", _config_string, "\"auto\"", "HALO_TOUCH_CONTROLS", _environment_value, _platform_android,
		"The on-screen touch controls in a game: \"auto\" shows them on a\n"
		"touchscreen while no controller is connected, \"on\" also with a\n"
		"controller, \"off\" never. A device without a touchscreen never shows\n"
		"them. The menus take taps in any case." },
	{ "input.touch_aim_assist", _config_boolean, "true", "HALO_TOUCH_AIM_ASSIST", _environment_value, _platform_android,
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
	{ "controls.crouch", _config_string, "\"Left Ctrl, C\"", "HALO_KEY_CROUCH", _environment_value, _platform_all,
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
		"found in Steam's libraries. Settings > Game Options > MAP FILES' BROWSE\n"
		"sets it. HALO_MCC_PATH names one for a run (after this one). The files\n"
		"are only read, never changed or copied." },
	{ "game.mcc_use", _config_string, "\"ask\"", "HALO_MCC_USE", _environment_value, /* AE hook */
		_platform_desktop,
		"Whether Custom Edition maps read MCC's Custom Edition files (game.mcc_path)\n"
		"when maps_ce, maps/ce and custom_maps lack them: \"yes\"; \"no\"; \"ask\" (the\n"
		"menus ask the first time such a map is picked; elsewhere it counts as no)." },
	{ "game.callouts", _config_string, "\"off\"", "HALO_CALLOUTS", _environment_value, _platform_all,
		"Spoken callouts in multiplayer, in game.callout_voice's voice, on this\n"
		"machine only, in any gametype: \"items\" each power item's call 10\n"
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
		"where anyone can find and join them. False (this build's default: an\n"
		"opt-in, Settings > Network) keeps them to invites and the local network." },
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
		"for the site's match pages, heatmaps and leaderboards. Players' names\n"
		"and a hash of their hardware ID; never an address. False (this build's\n"
		"default: an opt-in, Settings > Network; ChupathingyCE's is true)\n"
		"records and sends nothing." },
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
		"\"host:<map>\" hosts a game on that map, \"join\" joins the first game found;\n"
		"empty for none." },
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
	{ "debug.network_test_quit", _config_real, "0.0", "HALO_NETWORK_TEST_QUIT", _environment_value, _platform_all,
		"This many seconds into an automated test game controller 1's player\n"
		"quits, as their pause menu's QUIT does (split screen: the others stay); 0 never." },
	{ "debug.network_test_pickup_weapon", _config_string, "\"\"", "HALO_NETWORK_TEST_PICKUP_WEAPON", _environment_value,
		_platform_all,
		"The weapon network_test_pickup stands the player on: the first whose tag\n"
		"name has this in it (\"sniper\", say); empty any." },
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
#ifdef HALO_ANDROID
	/* the data folder, which the app names (port/android/host/host_main.c) */
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
		"# value to choose another. Delete the file to go back to the defaults.\n"
		CONFIG_VERSION_TEXT);
#else
	config_append(&text,
		"# Halo settings\n"
		"#\n"
		"# The game writes this file with the defaults when it is missing. A\n"
		"# setting at its default is commented out (\"# key = value\") and follows\n"
		"# the default of the version that runs: take out the \"# \" and change the\n"
		"# value to choose another. Delete the file to go back to the defaults.\n"
		"# Each setting can also be set for one run with the environment variable\n"
		"# named with it, which wins over this file.\n"
		CONFIG_VERSION_TEXT);
#endif
	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		const struct config_setting *setting = &config_settings[index];
		const char *dot = strchr(setting->name, '.');
		char buffer[64];

		if (!(setting->platforms & CONFIG_PLATFORM) || !dot)
			continue;
		if (strncmp(section, setting->name, (size_t)(dot - setting->name)) ||
			section[dot - setting->name] != 0)
		{
			snprintf(section, sizeof(section), "%.*s", (int)(dot - setting->name), setting->name);
			snprintf(buffer, sizeof(buffer), "\n[%s]\n", section);
			config_append(&text, buffer);
		}
		config_append_setting(&text, setting);
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

/* a file from before config_version (text, parsed as table) wrote every
setting at the default of the version that wrote it, so that a default
changed since never reached it: its lines holding this version's default or
an older one are commented out at this version's default, as a new file has
them, and config_version written. Values that are no default stay, being
the player's. Returns the new text, or NULL if the file has its version */
static char *config_update_layout(const char *text, toml_datum_t table)
{
	toml_datum_t version = toml_seek(table, "config_version");
	int lines[NUMBER_OF_CONFIG_SETTINGS];
	struct config_text out = { NULL, 0, 0 };
	struct config_text changed = { NULL, 0, 0 };
	int line_number = 1, defaults = 0, version_written, written_by_game;
	const char *line;
	size_t index, present = 0;

	if (version.type == TOML_INT64 && version.u.int64 >= CONFIG_VERSION)
		return NULL;
	/* (an older version's line is changed where it is) */
	version_written = version.type != TOML_UNKNOWN;
	/* (the builds before config_version wrote every setting as a value: a
	file holding fewer than half of them was written by a person, whose
	values, an older default's included, are theirs) */
	for (index = 0; index < NUMBER_OF_CONFIG_SETTINGS; index++)
	{
		if (toml_seek(table, config_settings[index].name).type != TOML_UNKNOWN)
			present++;
	}
	written_by_game = present * 2 >= NUMBER_OF_CONFIG_SETTINGS;
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
		for (old = 0; written_by_game && old < NUMBER_OF_CONFIG_OLD_DEFAULTS; old++)
		{
			if (!strcmp(config_old_defaults[old].name, setting->name) &&
				config_datum_is(datum, setting->type, config_old_defaults[old].default_value))
			{
				char buffer[256];

				lines[index] = datum.lineno;
				snprintf(buffer, sizeof(buffer), "%s%s (%s, now %s)", changed.length ? ", " : "", setting->name,
					config_old_defaults[old].default_value, setting->default_value);
				config_append(&changed, buffer);
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
	platform_log("settings: config.toml updated (config_version %d): %d lines at a default commented out, to follow "
		"this version's%s%s", CONFIG_VERSION, defaults, changed.length ? "; changed defaults: " : "",
		changed.length ? changed.buffer : "");
	free(changed.buffer);
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
#ifdef __APPLE__
	{
		/* a config.toml beside the application, or in it, which the game never
		reads (its is in Application Support): said, for whoever edits it */
		const char *base = SDL_GetBasePath();
		const char *bundle = base ? strstr(base, ".app/Contents/") : NULL;

		if (bundle)
		{
			const char *folder_end = bundle;
			char other[1024];

			while (folder_end > base && folder_end[-1] != '/')
				folder_end--;
			snprintf(other, sizeof(other), "%sconfig.toml", base);
			if (SDL_GetPathInfo(other, NULL))
				platform_log("settings: %s is not read; the game's settings are %s", other, path);
			snprintf(other, sizeof(other), "%.*sconfig.toml", (int)(folder_end - base), base);
			if (SDL_GetPathInfo(other, NULL))
				platform_log("settings: %s is not read; the game's settings are %s", other, path);
		}
	}
#endif
	text = config_read_file(path, &size);
	if (text)
	{
		toml_result_t result = toml_parse(text, (int)size);

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
				config_set_from_file(&config_values[index], &config_settings[index], result.toptab);
			config_report_unknown_keys(result.toptab);
			platform_log("settings: %s", path);
			completed = config_add_missing(text, result.toptab);
			if ((completed || updated) && !config_write_file(path, completed ? completed : text))
				platform_log("settings: cannot write %s", path);
			free(completed);
		}
		else
		{
			platform_log("config.toml cannot be read (%s): every setting is at its default until that is fixed "
				"(a word value needs quotes: menus = \"pc\")", result.errmsg);
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
