/*
AE_GLUE_ZOOM.C

The zoom action's mode and hold time, read from config.toml, and the functions
the hook lines call (ae_glue_zoom.h). The state machine is ae_zoom.c's. The
TOGGLE mode (the default) changes nothing: the engine steps the level itself,
and these functions return what they were given.
*/

#include <stdio.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "networking/network_connection.h"
#include "game/players.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "../src/port_config.h"
#include "../src/ae_platform.h"
#include "ae_glue_zoom.h"
#include "ae_zoom.h"

void platform_log(char const *format, ...);
boolean weapon_reloading(long weapon_index);

static struct
{
	unsigned long read_at;
	int read;
	long hold_ms;
	struct ae_zoom_bank bank;
	/* the mouse wheel's scroll to take for the zoom level (+1 up, -1 down, 0 none), and whether the zoom is held by
	the player on the first controller (the keyboard and mouse's), published each tick: for 100 ms, then not */
	int wheel_pending;
	int wheel_taken;
	unsigned long wheel_taken_at;
} zoom = { (unsigned long)-1 };

/* the settings, read again when a setting has been written */
static void zoom_read(void)
{
	if (zoom.read && zoom.read_at == config_changes())
		return;
	zoom.read = 1;
	zoom.read_at = config_changes();
	zoom.hold_ms = ae_zoom_hold_ms(config_real("input.zoom_hold_time"));
	ae_zoom_bank_set_mode(&zoom.bank, ae_zoom_mode_from_text(config_string("input.zoom_mode")));
	ae_zoom_bank_set_reset(&zoom.bank, ae_zoom_reset_from_text(config_string("input.zoom_reset")));
}

void ae_zoom_note_held(unsigned long *player_control_flags, int held, short gamepad_index)
{
	ae_zoom_flags_note_held(player_control_flags, held);
	if (gamepad_index == 0)
	{
		zoom_read();
		zoom.wheel_taken = held && zoom.bank.mode != AE_ZOOM_TOGGLE;
		zoom.wheel_taken_at = system_milliseconds();
		if (!zoom.wheel_taken)
			zoom.wheel_pending = 0;
	}
}

void ae_zoom_note_flash(unsigned long *player_control_flags, int down)
{
	if (down)
		*player_control_flags |= 1ul << AE_ZOOM_FLASH_BIT;
	else
		*player_control_flags &= ~(1ul << AE_ZOOM_FLASH_BIT);
}

void ae_zoom_filter_press(short local_player_index, unsigned long *player_control_flags, int zoom_bit,
	unsigned long *unit_control_flags, int light_bit)
{
	zoom_read();
	ae_zoom_flags_filter_press(zoom.bank.mode, player_control_flags, zoom_bit);
	ae_zoom_bank_filter_flash(&zoom.bank, local_player_index, player_control_flags, unit_control_flags, light_bit);
}

int ae_zoom_wheel_taken(void)
{
	return zoom.wheel_taken && system_milliseconds() - zoom.wheel_taken_at < 100;
}

void ae_zoom_wheel_scrolled(int direction)
{
	if (ae_zoom_wheel_taken())
		zoom.wheel_pending = direction;
}

short ae_zoom_update(short local_player_index, short zoom_level, long weapon_index, int may_act,
	unsigned long player_control_flags)
{
	short count = 0;
	int blocked;
	int wheel = 0;
	long player_index;

	zoom_read();
	if (zoom.bank.mode == AE_ZOOM_TOGGLE)
		return zoom_level;
	if (weapon_index != NONE)
	{
		struct weapon_datum *weapon = weapon_get(weapon_index);

		count = weapon_definition_get(weapon->definition_index)->weapon.zoom_level_count;
	}
	blocked = weapon_index == NONE || !may_act || weapon_reloading(weapon_index);
	/* the wheel is the first controller's (the keyboard and mouse's) player's alone */
	player_index = local_player_get_player_index(local_player_index);
	if (player_index != NONE && player_get(player_index)->local_player_index == 0)
	{
		wheel = zoom.wheel_pending;
		zoom.wheel_pending = 0;
	}
	return ae_zoom_bank_step_ex(&zoom.bank, local_player_index, zoom.hold_ms, player_control_flags,
		(long)system_milliseconds(), zoom_level, count, blocked, weapon_index, wheel);
}

void ae_zoom_log_settings(void)
{
	zoom_read();
	platform_log("zoom: mode %s, hold time %g, reset %s",
		zoom.bank.mode == AE_ZOOM_HOLD ? "hold" : zoom.bank.mode == AE_ZOOM_BOTH ? "both" : "toggle",
		(double)zoom.hold_ms / 1000.0, zoom.bank.reset == AE_ZOOM_RESET_LAST ? "last" : "first");
}
