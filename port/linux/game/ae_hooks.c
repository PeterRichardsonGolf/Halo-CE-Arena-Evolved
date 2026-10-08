/*
AE_HOOKS.C

The functions the AE hook lines in upstream files call (ae_hooks.txt lists
those lines; notes/ae-hooks.md says where each goes). Nothing here runs
unless display.arena_menus is on, and nothing is drawn or taken while no AE
screen is open.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "interface/event_manager.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_motion.h"
#include "ae_screen_test.h"
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

boolean ae_ui_takes_pointer(
	void)
{
	return ae_ui_up();
}

void ae_ui_replace_menus(
	void)
{
	ui_widgets_close_all();
	platform_log("ae menus: replaced the game's menus");
}

/* ---------- the settings the widgets read */

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
	(void)screen;
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

boolean ae_ui_process(
	void)
{
	static boolean test_screen_checked = FALSE;
	/* a screen was up last frame */
	static boolean was_up = FALSE;

	unsigned long now;

	if (!ae_menus_active())
		return FALSE;
	/* (the frame's clock and REDUCE MOTION, before anything opens, moves or draws) */
	now = system_milliseconds();
	ae_motion_set_now(now);
	ae_motion_set_reduced(ae_settings_reduce_motion());
	/* debug.ae_test_screen: the test screen, once, as the main menu first shows */
	if (!test_screen_checked && ae_draw_available() && main_menu_is_active())
	{
		long views = config_integer("debug.ae_test_screen");

		test_screen_checked = TRUE;
		/* (M1's test screen: 1, 2, 4, and 9 with the game's menus closed; the gallery's and the drives' values
		open nothing until their tasks route them) */
		if (views == 1 || views == 2 || views == 4 || views == 9)
			ae_screen_test_open((int)views);
		else if (views != 0)
			platform_log("ae menus: debug.ae_test_screen %ld: no such screen (yet)", views);
	}
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
		ae_ui_update();
		ae_input_poll();
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

void ae_ui_render(
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
	ae_ui_draw();
	if (ae_hits_overflowed())
	{
		static boolean logged = FALSE;

		if (!logged)
			platform_log("ae menus: more than %d hit rectangles in a frame: the last were dropped", AE_HITS_MAXIMUM);
		logged = TRUE;
	}
}

boolean ae_ui_pointer(
	struct halo_ui_pointer const *pointer)
{
	if (!ae_ui_up())
		return FALSE;
	/* (the clock for what the pointer starts: it comes before ae_ui_process this frame) */
	ae_motion_set_now(system_milliseconds());
	ae_input_pointer(pointer);

	return TRUE;
}
