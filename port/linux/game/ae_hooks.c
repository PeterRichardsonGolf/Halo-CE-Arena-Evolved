/*
AE_HOOKS.C

The functions the AE hook lines in upstream files call (ae_hooks.txt lists
those lines; notes/ae-hooks.md says where each goes). Nothing here runs
unless display.arena_menus is on, and nothing is drawn or taken while no AE
screen is open.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "interface/event_manager.h"
#include "ae_hooks.h"
#include "ae_community_screen.h"
#include "ae_glue_player_settings.h"
#include "ae_screen_settings.h"
#include "ae_input.h"
#include "ae_motion.h"
#include "ae_screen_gallery.h"
#include "ae_screen_game_options.h"
#include "ae_screen_change_gametype.h"
#include "ae_screen_change_map.h"
#include "ae_screen_create_game.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_pause.h"
#include "ae_screen_postgame.h"
#include "ae_glue_ce_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_glue_lobby.h"
#include "ae_screen_lobby_test.h"
#include "ae_screen_test.h"
#include "ae_screen_test_run.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

/* port_config.c (declared here as the game's other port units do: its header
is the platform side's) */
int config_boolean(char const *name);
long config_integer(char const *name);
double config_real(char const *name);
void platform_log(char const *format, ...);
/* interface/virtual_keyboard.c and ui_widget.c (their headers need the game's math types first) */
boolean virtual_keyboard_active(void);
/* interface/progress_bar.c: the loading screen is up */
boolean progress_bar_is_active(void);
boolean main_menu_is_active(void);
void ui_widgets_close_all(void);
#ifdef HALO_GAME_BROWSER
/* port/linux/game/browser_screen.c: upstream's server browser (only with the game browser; ui_widget.c declares it
the same way) */
boolean browser_screen_active(void);
#endif

boolean ae_menus_active(
	void)
{
	static int cached = -1;

	if (cached < 0)
	{
		cached = config_boolean("display.arena_menus") ? 1 : 0;
		if (cached)
		{
			platform_log("arena menus: on (display.arena_menus)");
			/* (the engine contact the pure widgets use: typing, the clipboard, the log) */
			ae_glue_text_install();
		}
	}

	return cached != 0;
}

/* whether upstream's server browser is up (it wins while it is: AE's stack waits under it and returns when it
closes; M1 review M8) */
static boolean server_browser_up(
	void)
{
#ifdef HALO_GAME_BROWSER
	return browser_screen_active();
#else
	return FALSE;
#endif
}

/* whether AE's screens are up: the flag, a screen open, a renderer to draw
them (without one, the game's own menus stay), and no server browser over
them */
static boolean ae_ui_up(
	void)
{
	static boolean waiting = FALSE;

	if (!ae_menus_active())
		return FALSE;
	/* (no screen: the next time the browser is over one logs again) */
	if (!ae_ui_depth() || !ae_draw_available())
	{
		waiting = FALSE;
		return FALSE;
	}
	if (server_browser_up())
	{
		if (!waiting)
			platform_log("ae menus: the server browser is up: AE waits");
		waiting = TRUE;
		return FALSE;
	}
	waiting = FALSE;
	return TRUE;
}

/* how long AE's main menu waits for the game's (ms): past it, AE's opens on its own, so the main menu is never empty */
#define MAIN_MENU_WAIT_MS 3000

/* the game loaded its main menu (main_screen_shell_load) since the main menu began: AE's may take its place */
static boolean game_main_menu_loaded = FALSE;

/* AE's process and draw in the last completed frame (the overlay is drawn before this frame's menus). */
static double ui_timing_building, ui_timing_complete;
static boolean ui_timing_started = FALSE;

static double ui_timing_now(void)
{
	LARGE_INTEGER count, frequency;

	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart ? (double)count.QuadPart / (double)frequency.QuadPart : 0.0;
}

static void ui_timing_frame_begin(void)
{
	if (ui_timing_started)
		ui_timing_complete = ui_timing_building;
	ui_timing_building = 0.0;
	ui_timing_started = TRUE;
}

void ae_ui_timing_append(char *line, long size, boolean full)
{
	size_t length;

	if (!full || !line || size <= 0)
		return;
	length = strlen(line);
	if (length < (size_t)size)
		snprintf(line + length, (size_t)size - length, "  MENU %.1f MS", ui_timing_complete * 1000.0);
}

void ae_ui_timing_extent(short *bottom, short top, short line_height, float scale, const char *line)
{
	int lines = 1;
	const char *at;

	if (!bottom || !line)
		return;
	for (at = line; *at; at++)
		lines += *at == '\n';
	*bottom = (short)(top + (float)(line_height * lines) * scale + 0.5f);
}

/* (0.4.0) the game loaded its main menu since the last frame: under AE's screens already up (a join that failed left
the game for its main menu while AE's stayed) it is closed at once, as AE's main menu's opening closes it */
static boolean game_main_menu_reloaded = FALSE;

void ae_ui_game_main_menu_loaded(
	void)
{
	game_main_menu_loaded = TRUE;
	game_main_menu_reloaded = TRUE;
}

/* whether AE's main menu may take the game's main menu's place now: once the game has loaded its main menu, so the
replacing closes it. At the start that is the case at once; back from a game the ui scenario (main_menu_is_active)
comes a little before the game loads its main menu, which then drew under AE's (0.3.7.1). Nothing of the game's
there for MAIN_MENU_WAIT_MS: AE's opens anyway */
static boolean main_menu_ready(
	unsigned long now)
{
	static boolean waiting = FALSE;
	static unsigned long since = 0, last = 0;

	/* (a wait left off more than a moment ago, while AE's screens were up: starts over) */
	if (waiting && now - last > 250)
		waiting = FALSE;
	last = now;
	if (game_main_menu_loaded)
	{
		waiting = FALSE;
		return TRUE;
	}
	if (!waiting)
	{
		waiting = TRUE;
		since = now;
		return FALSE;
	}
	if (now - since < MAIN_MENU_WAIT_MS)
		return FALSE;
	waiting = FALSE;
	platform_log("ae menus: the game's main menu did not load: AE's opens without it");
	return TRUE;
}

/* a start was asked from AE's screens at this time (ms), 0 none: see ae_ui_game_starting */
static unsigned long game_starting_since = 0;
/* the longest the main menu stays out for a start that never leaves the main menu */
#define GAME_STARTING_HOLD_MS 8000

void ae_ui_game_starting(
	void)
{
	game_starting_since = system_milliseconds();
	if (!game_starting_since)
		game_starting_since = 1;
}

static boolean game_starting(
	unsigned long now)
{
	if (game_starting_since && now - game_starting_since > GAME_STARTING_HOLD_MS)
	{
		/* (the level never took the game out of its main menu: AE's opens again) */
		platform_log("ae menus: the game did not start within %d s: the main menu opens again",
			GAME_STARTING_HOLD_MS / 1000);
		game_starting_since = 0;
	}
	return game_starting_since != 0;
}

boolean ae_ui_takes_pointer(
	void)
{
	/* (the pause of a split-screen player with a pad: the mouse stays player 1's, who plays on) */
	return ae_ui_up() && !ae_screen_pause_leaves_pointer();
}

void ae_ui_replace_menus(
	void)
{
	ui_widgets_close_all();
	platform_log("ae menus: replaced the game's menus");
}

/* ---------- the settings the widgets read */

/* UI SCALE inside an inset (ae_screen_pause_inset): its largest step */
#define INSET_UI_SCALE 1.3f

static struct
{
	boolean read;
	float ui_scale;
	boolean reduce_motion;
	float menu_volume;
} ae_settings;

void ae_settings_refresh(
	void)
{
	/* (UI SCALE snapped to its steps, 90 / 100 / 115 / 130: ae_style.c, preflight P27) */
	ae_settings.ui_scale = ae_ui_scale_from_percent((int)config_integer("display.arena_menus_scale"));
	ae_settings.reduce_motion = config_boolean("display.arena_menus_reduce_motion") ? TRUE : FALSE;
	/* (0..1, NaN 0: ae_style.c) */
	ae_settings.menu_volume = ae_volume_from_config(config_real("audio.arena_menus_volume"));
	ae_settings.read = TRUE;
	platform_log("ae menus: settings: scale %.2f, reduce motion %d, volume %.2f", ae_settings.ui_scale,
		ae_settings.reduce_motion ? 1 : 0, ae_settings.menu_volume);
}

static void settings_read(
	void)
{
	if (!ae_settings.read)
		ae_settings_refresh();
}

float ae_settings_ui_scale(
	void)
{
	settings_read();
	/* (a full screen drawn into a split-screen view, the pause menu's SETTINGS: at the largest UI SCALE, 130 %, as it
	is half the size or less) */
	if (ae_draw_inset_active() && ae_settings.ui_scale < INSET_UI_SCALE)
		return INSET_UI_SCALE;
	return ae_settings.ui_scale;
}

boolean ae_settings_reduce_motion(
	void)
{
	settings_read();
	return ae_settings.reduce_motion;
}

float ae_settings_menu_volume(
	void)
{
	settings_read();
	return ae_settings.menu_volume;
}

/* before each screen draws (ae_ui.h): the whole frame as the view, no clip, opaque, so nothing a screen set leaks
into the next (M1 review M3); then its motion: a sliding screen's view moved by its offset (u: layout units at UI
SCALE), a popover's scaled about the frame's centre, and its alpha */
static void before_draw(
	struct ae_screen const *screen,
	int index,
	float offset_x_u,
	float alpha,
	float scale)
{
	float inset[4];

	/* (the pause menu's SETTINGS, GAME OPTIONS, CHANGE MAP and their dialogs in split screen: in the view of the
	player who opened them; any other screen, the whole frame) */
	if (ae_screen_pause_inset(screen ? screen->data : NULL, inset))
		ae_draw_inset(inset[0], inset[1], inset[2], inset[3]);
	else
		ae_draw_inset(0.0f, 0.0f, 0.0f, 0.0f);
	ae_draw_view_full();
	/* (the hits the screen records are its layer's) */
	ae_hits_layer((short)index);
	if (offset_x_u != 0.0f || scale != 1.0f)
	{
		struct ae_layout layout;
		float width, height;

		ae_draw_current_layout(&layout);
		width = layout.width * scale;
		height = layout.height * scale;
		ae_draw_view((layout.width - width) * 0.5f + offset_x_u * ae_settings_ui_scale(), (layout.height - height) * 0.5f,
			width, height);
	}
	ae_draw_set_alpha(alpha);
}

/* typing mode left on: an edit whose screen left the stack is cancelled (ae_field_edit_guard); typing mode on with no
edit owning it (the game's typing mode, ae_glue_text.c) is turned off, logged once */
static void typing_guard(
	void)
{
	static boolean logged = FALSE;

	ae_field_edit_guard();
	if (ae_glue_text_typing() && !ae_field_edit_live())
	{
		ae_glue_text_end();
		if (!logged)
			platform_log("ae menus: typing mode was on with no field: turned off");
		logged = TRUE;
	}
}

/* the top screen's inset (ae_screen_pause_inset) while its input and update run: what it measures is in the units it
draws in */
static void top_inset(
	void)
{
	float inset[4];

	if (ae_ui_top() && ae_screen_pause_inset(ae_ui_top()->data, inset))
		ae_draw_inset(inset[0], inset[1], inset[2], inset[3]);
}


static boolean test_screen_checked = FALSE;
/* debug.ae_test_screen as read (a test screen up: AE's main menu stays out of its way) */
static long test_screen = 0;

/* (0.7.0, the owner: "the classic menu flashes at start") the game's main menu is not drawn while AE's is about to take
its place: AE's menus on, the main menu up with no AE screen yet, nothing else of the game's wanted there (a test
screen, the server browser, the on-screen keyboard, a game starting). At most HIDE_MS from when that began, so a main
menu AE never takes shows again (render_ui_widgets) */
boolean ae_hooks_classic_main_menu_hidden(void)
{
	enum { HIDE_MS = 3000 };
	static unsigned long since = 0;
	unsigned long now = system_milliseconds();
	/* (the test screen read here too when the tick has not yet: the first main menu frame is drawn before it) */
	boolean no_test_screen = test_screen_checked ? !test_screen : config_integer("debug.ae_test_screen") == 0;
	boolean hide = ae_menus_active() && no_test_screen && ae_draw_available() &&
		main_menu_is_active() && !ae_ui_depth() && !server_browser_up() && !virtual_keyboard_active() &&
		!game_starting(now);

	if (!hide)
	{
		since = 0;
		return FALSE;
	}
	if (!since)
		since = now ? now : 1;
	return now - since < HIDE_MS;
}

void ae_ui_test_screen_rearm(
	void)
{
	/* (TEST RUN: AE's screens closed, debug.ae_test_screen read again as the main menu next shows) */
	short pops;

	for (pops = 0; pops < 32 && ae_ui_depth(); pops++)
		ae_ui_pop();
	test_screen_checked = FALSE;
	test_screen = 0;
}

int ae_ui_test_screen_open(
	void)
{
	return test_screen_checked && test_screen && ae_ui_depth() > 0 && main_menu_is_active();
}

static boolean ae_ui_process_body(
	void)
{
	/* a screen was up last frame */
	static boolean was_up = FALSE;

	unsigned long now;

	/* (each local player's own settings, their profile's: AE's menus on or off) */
	ae_player_settings_update();
	ae_ce_map_update();
	if (!ae_menus_active())
	{
		/* (a timed revert left open as AE's menus went off still reverts at 10 s) */
		ae_dialog_menus_off(system_milliseconds());
		return FALSE;
	}
	/* (the frame's clock and REDUCE MOTION, before anything opens, moves or draws) */
	now = system_milliseconds();
	ae_motion_set_now(now);
	ae_motion_set_reduced(ae_settings_reduce_motion());
	/* debug.ae_test_screen: the test screen, once, as the main menu first shows */
	if (!test_screen_checked && ae_draw_available() && main_menu_is_active())
	{
		long views = config_integer("debug.ae_test_screen");

		test_screen_checked = TRUE;
		test_screen = views;
		/* (M1's test screen: 1, 2, 4, and 9 with the game's menus closed) */
		if (views == 1 || views == 2 || views == 4 || views == 9)
			ae_screen_test_open((int)views);
		/* (the widget gallery: 11-15 one view, 21 / 23 two, 41 four) */
		else if ((views >= 11 && views <= 15) || views == 21 || views == 23 || views == 41)
			ae_screen_gallery_open((int)views);
		/* (the lobby drives: 90 hosts LAN with bots, 91 joins, 92 hosts for a joiner, 93 hosts LOCAL, 94 the
		profiles, 95 ONLINE) */
		else if (views >= 90 && views <= 95)
			ae_screen_lobby_test_open((int)views);
		/* (the game setup spike's GAME OPTIONS: the last used gametype, or $HALO_AE_TEST_GAMETYPE's) */
		else if (views == 30)
			ae_screen_game_options_open(NULL);
		/* (its CREATE GAME, as AE's MULTIPLAYER opens it) */
		else if (views == 32)
			ae_screen_create_game_open();
		/* (its CHANGE MAP: the filter $HALO_AE_TEST_MAP_FILTER, 0-6, else ALL, the search $HALO_AE_TEST_MAP_SEARCH) */
		else if (views == 31)
			ae_screen_change_map_open(getenv("HALO_AE_TEST_MAP_FILTER") ? atoi(getenv("HALO_AE_TEST_MAP_FILTER")) : 0,
				getenv("HALO_AE_TEST_MAP_SEARCH"));
		/* (its CHANGE GAMETYPE: $HALO_AE_TEST_GAMETYPE's (else the last used) focused, the filter
		$HALO_AE_TEST_GAMETYPE_FILTER, 0-5, else ALL, the search $HALO_AE_TEST_GAMETYPE_SEARCH) */
		else if (views == 33)
		{
			if (!ae_setup_edit_open())
				ae_setup_edit_begin_named(getenv("HALO_AE_TEST_GAMETYPE"));
			ae_screen_change_gametype_open(getenv("HALO_AE_TEST_GAMETYPE_FILTER") ?
				atoi(getenv("HALO_AE_TEST_GAMETYPE_FILTER")) : 0, getenv("HALO_AE_TEST_GAMETYPE_SEARCH"));
		}
		/* (0.3.7 SETTINGS: 34 its top level, 35 a group, $HALO_AE_TEST_SETTINGS_GROUP 0-8; with
		$HALO_AE_TEST_SETTINGS_PLAYERS 2 a second local player joins first, on a profile of its own) */
		else if (views == 34 || views == 35)
			ae_screen_settings_test_open(views == 35, getenv("HALO_AE_TEST_SETTINGS_GROUP") ?
				atoi(getenv("HALO_AE_TEST_SETTINGS_GROUP")) : 0, getenv("HALO_AE_TEST_SETTINGS_PLAYERS") ?
				atoi(getenv("HALO_AE_TEST_SETTINGS_PLAYERS")) : 1);
		/* (TEST RUN's list screen: needs DEV MODE) */
		else if (views == 36)
		{
			if (ae_screen_test_run_open())
				ae_ui_replace_menus();
		}
		else if (views != 0)
			platform_log("ae menus: debug.ae_test_screen %ld: no such screen (yet)", views);
	}
	/* (0.4.0: the game's main menu loaded under AE's screens: closed, else it drew through them, the HALO logo and
	CAMPAIGN .. QUIT under LAN GAMES; its error is AE's dialog now, ae_glue_errors.c) */
	if (game_main_menu_reloaded)
	{
		game_main_menu_reloaded = FALSE;
		if (ae_ui_depth() && ae_draw_available() && main_menu_is_active())
			ae_ui_replace_menus();
	}
	/* (the 0.3.7 menus: AE's main menu takes the game's main menu's place, whenever no AE screen is up there) */
	if (test_screen_checked && !test_screen && ae_draw_available() && main_menu_is_active() && !ae_ui_depth() &&
		!server_browser_up() && !virtual_keyboard_active() && !game_starting(now) && !ae_community_screen_asking())
	{
		/* (once the game's main menu is there to be replaced) */
		if (main_menu_ready(now))
			ae_screen_main_menu_open();
	}
	/* (and never outside it: CAMPAIGN's level loads a frame or more after its screens closed, and the main menu can
	open again in between, over the level) */
	else if (!main_menu_is_active())
	{
		ae_screen_main_menu_close();
		game_starting_since = 0;
		/* (the next main menu waits for the game's again) */
		game_main_menu_loaded = FALSE;
	}
	/* (the game's error taken before AE's screens were there: shown over them) */
	ae_errors_tick();
	/* (a timed revert counts down and reverts covered or hidden too; a waiting error opens) */
	ae_dialog_tick();
	/* (the lobby glue's join going on, its session's owner checked; the lobby drives: in the menus and in the game) */
	ae_lobby_update(now);
	ae_screen_lobby_test_tick(now);
	/* (the pause menu's: a game that ended or a player who left closes theirs; CHANGE MAP's pick asks) */
	ae_screen_pause_tick();
	/* (0.4.0: AE's post-game in place of the game's carnage report: opens as it is due, closes after it) */
	ae_screen_postgame_tick();
	/* (closed since: by the pointer, which comes before this) */
	if (was_up && !ae_ui_depth())
		ae_input_hold_begin();
	if (ae_ui_up())
	{
		/* (opened since the last frame, over none: its first poll takes nothing held as a press, and nothing asked
		before it plays) */
		if (!was_up)
		{
			ae_input_screen_opened();
			ae_sound_reset();
		}
		ae_platform_arm_back_presses(TRUE);
		ae_platform_arm_tab_presses(TRUE);
		/* (an edit whose screen left the stack ends; typing mode on with no edit owning it is turned off) */
		typing_guard();
		top_inset();
		ae_ui_update();
		ae_input_poll();
		ae_draw_inset(0.0f, 0.0f, 0.0f, 0.0f);
		/* (the frame's one sound: ae_sound.h) */
		ae_glue_sound_play(ae_sound_end_frame(TRUE, TRUE, now));
		was_up = ae_ui_depth() != 0;
		if (!was_up)
			ae_input_hold_begin();
		ae_platform_arm_back_presses(was_up);
		ae_platform_arm_tab_presses(was_up);
		return TRUE;
	}
	/* (AE's screens gone: nothing is being typed into; only hidden (the server browser, no renderer): kept) */
	ae_field_edit_screens_hidden();
	typing_guard();
	/* (the last screen closed by the pointer before this frame's input: its BACK plays now; else nothing is kept) */
	ae_glue_sound_play(ae_sound_end_frame(was_up, FALSE, now));
	was_up = FALSE;
	/* (mouse button 4 is counted for AE only while one of its screens is open) */
	ae_platform_arm_back_presses(FALSE);
	ae_platform_arm_tab_presses(FALSE);
	/* the inputs held as the last screen closed reach the game's menus only
	once let go of (each; at most 2 s): a B that closed it is not also their
	B, nor a held direction their direction */
	if (ae_input_holding())
	{
		event_manager_flush();
		return TRUE;
	}

	return FALSE;
}

boolean ae_ui_process(void)
{
	double started;
	boolean result;

	ui_timing_frame_begin();
	started = ui_timing_now();
	result = ae_ui_process_body();
	if (ae_ui_depth())
		ui_timing_building += ui_timing_now() - started;
	return result;
}

static void ae_ui_render_body(
	short local_player_index,
	union rectangle2d const *window_bounds)
{
	static boolean drawn = FALSE;
	static boolean before_draw_set = FALSE;
	static unsigned int drawn_frame = 0;
	unsigned int frame;

	(void)window_bounds;
	if (!ae_ui_up())
		return;
	/* the virtual keyboard is drawn by the game, under AE's drawing (which
	goes over the whole picture at Present): while it is up, AE draws nothing
	so that it shows (ae_hooks.md) */
	if (virtual_keyboard_active())
	{
		/* (nothing drawn: no targets of an earlier frame stay live) */
		ae_hits_clear();
		return;
	}
	/* the loading screen is the whole picture while it is up (progress_bar.c,
	held for display.loading_screen's seconds: game.c): the screen that started
	the game, which takes no input then, is not drawn still over it, as the
	classic menus' overlay is not (ui_overlay.c) */
	if (progress_bar_is_active())
	{
		ae_hits_clear();
		return;
	}
	/* M1: the screens are drawn once a frame, over the first player's view or
	the whole screen. render_ui_widgets runs once per view in split screen,
	and once more for a mirror's view, which arrives as player 0 too (with the
	virtual keyboard down the index is pinned to 0..3): the frame (AE's count
	of Presents) decides */
	if (local_player_index != NONE && local_player_index != 0)
		return;
	frame = ae_draw_frame();
	if (drawn && drawn_frame == frame)
		return;
	drawn = TRUE;
	drawn_frame = frame;
	if (!before_draw_set)
	{
		ae_ui_set_before_draw(before_draw);
		before_draw_set = TRUE;
	}
	/* (the pointer's targets are this frame's drawing's) */
	ae_hits_clear();
	/* (the pause menu's full screens over the game: a backdrop under them) */
	ae_screen_pause_backdrop();
	ae_ui_draw();
	ae_draw_inset(0.0f, 0.0f, 0.0f, 0.0f);
	if (ae_hits_overflowed())
	{
		static boolean logged = FALSE;

		if (!logged)
			platform_log("ae menus: more than %d hit rectangles in a frame: the last were dropped", AE_HITS_MAXIMUM);
		logged = TRUE;
	}
}

void ae_ui_render(short local_player_index, union rectangle2d const *window_bounds)
{
	double started = ui_timing_now();
	boolean up = ae_ui_up();

	ae_ui_render_body(local_player_index, window_bounds);
	if (up)
		ui_timing_building += ui_timing_now() - started;
}

boolean ae_ui_pointer(
	struct halo_ui_pointer const *pointer)
{
	if (!ae_ui_up() || ae_screen_pause_leaves_pointer())
		return FALSE;
	/* (the clock for what the pointer starts: it comes before ae_ui_process this frame) */
	ae_motion_set_now(system_milliseconds());
	top_inset();
	ae_input_pointer(pointer);
	ae_draw_inset(0.0f, 0.0f, 0.0f, 0.0f);

	return TRUE;
}
