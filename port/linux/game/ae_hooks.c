/*
AE_HOOKS.C

The functions the AE hook lines in upstream files call (ae_hooks.txt lists
those lines; notes/ae-hooks.md says where each goes). Nothing here runs
unless display.arena_menus is on, and nothing is drawn or taken while no AE
screen is open.
*/

#include "cseries.h"
#include "../src/ae_draw.h"
#include "interface/event_manager.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_screen_test.h"
#include "ae_ui.h"

/* port_config.c (declared here as the game's other port units do: its header
is the platform side's) */
int config_boolean(char const *name);
long config_integer(char const *name);
void platform_log(char const *format, ...);
/* interface/virtual_keyboard.c and ui_widget.c (their headers need the game's math types first) */
boolean virtual_keyboard_active(void);
boolean main_menu_is_active(void);

boolean ae_menus_active(
	void)
{
	static int cached = -1;

	if (cached < 0)
	{
		cached = config_boolean("display.arena_menus") ? 1 : 0;
		if (cached)
			platform_log("arena menus: on (display.arena_menus)");
	}

	return cached != 0;
}

/* whether AE's screens are up: the flag, a screen open, and a renderer to
draw them (without one, the game's own menus stay) */
static boolean ae_ui_up(
	void)
{
	return ae_menus_active() && ae_ui_depth() && ae_draw_available();
}

boolean ae_ui_process(
	void)
{
	static boolean test_screen_checked = FALSE;
	/* a screen was up last frame */
	static boolean was_up = FALSE;

	if (!ae_menus_active())
		return FALSE;
	/* debug.ae_test_screen: the test screen, once, as the main menu first shows */
	if (!test_screen_checked && ae_draw_available() && main_menu_is_active())
	{
		long views = config_integer("debug.ae_test_screen");

		test_screen_checked = TRUE;
		if (views > 0)
			ae_screen_test_open((int)views);
	}
	/* (closed since: by the pointer, which comes before this) */
	if (was_up && !ae_ui_depth())
		ae_input_hold_begin();
	was_up = FALSE;
	if (ae_ui_up())
	{
		ae_input_poll();
		was_up = ae_ui_depth() != 0;
		if (!was_up)
			ae_input_hold_begin();
		return TRUE;
	}
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
	static unsigned int drawn_frame = 0;
	unsigned int frame;

	(void)window_bounds;
	if (!ae_ui_up())
		return;
	/* the virtual keyboard is drawn by the game, under AE's drawing (which
	goes over the whole picture at Present): while it is up, AE draws nothing
	so that it shows (ae_hooks.md) */
	if (virtual_keyboard_active())
		return;
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
	ae_ui_draw();
}

boolean ae_ui_pointer(
	struct halo_ui_pointer const *pointer)
{
	if (!ae_ui_up())
		return FALSE;
	ae_input_pointer(pointer);

	return TRUE;
}
