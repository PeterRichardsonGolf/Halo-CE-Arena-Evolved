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
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "../src/port_config.h"
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
}

void ae_zoom_note_held(unsigned long *player_control_flags, int held)
{
	ae_zoom_flags_note_held(player_control_flags, held);
}

void ae_zoom_filter_press(unsigned long *player_control_flags, int zoom_bit)
{
	zoom_read();
	ae_zoom_flags_filter_press(zoom.bank.mode, player_control_flags, zoom_bit);
}

short ae_zoom_update(short local_player_index, short zoom_level, long weapon_index, int may_act,
	unsigned long player_control_flags)
{
	short count = 0;
	int blocked;

	zoom_read();
	if (zoom.bank.mode == AE_ZOOM_TOGGLE)
		return zoom_level;
	if (weapon_index != NONE)
	{
		struct weapon_datum *weapon = weapon_get(weapon_index);

		count = weapon_definition_get(weapon->definition_index)->weapon.zoom_level_count;
	}
	blocked = weapon_index == NONE || !may_act || weapon_reloading(weapon_index);
	return ae_zoom_bank_step(&zoom.bank, local_player_index, zoom.hold_ms, player_control_flags,
		(long)system_milliseconds(), zoom_level, count, blocked);
}

void ae_zoom_log_settings(void)
{
	zoom_read();
	platform_log("zoom: mode %s, hold time %g",
		zoom.bank.mode == AE_ZOOM_HOLD ? "hold" : zoom.bank.mode == AE_ZOOM_BOTH ? "both" : "toggle",
		(double)zoom.hold_ms / 1000.0);
}
