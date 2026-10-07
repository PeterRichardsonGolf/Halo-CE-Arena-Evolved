/*
AE_HOOKS.C

The functions the AE hook lines in upstream files call (ae_hooks.txt lists
those lines; notes/ae-hooks.md says where each goes). Nothing here runs
unless display.arena_menus is on, and nothing is drawn or taken while no AE
screen is open.
*/

#include "cseries.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_ui.h"

/* port_config.c (declared here as the game's other port units do: its header
is the platform side's) */
int config_boolean(char const *name);
void platform_log(char const *format, ...);

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

boolean ae_ui_process(
	void)
{
	if (!ae_menus_active() || !ae_ui_depth())
		return FALSE;
	ae_input_poll();

	return TRUE;
}

void ae_ui_render(
	short local_player_index,
	union rectangle2d const *window_bounds)
{
	(void)window_bounds;
	if (!ae_menus_active() || !ae_ui_depth())
		return;
	/* M1: the screens are drawn once a frame, over the frame's first view
	(render_ui_widgets runs once per view in split screen) */
	if (local_player_index != NONE && local_player_index != 0)
		return;
	ae_ui_draw();
}

boolean ae_ui_pointer(
	struct halo_ui_pointer const *pointer)
{
	if (!ae_menus_active() || !ae_ui_depth())
		return FALSE;
	ae_input_pointer(pointer);

	return TRUE;
}
