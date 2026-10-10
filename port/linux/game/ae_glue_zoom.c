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

/* a bit of the input blob's player control flags past the engine's (player_control.c uses 0 to 5) */
#define HELD_BIT 12

static struct
{
	unsigned long read_at;
	int read;
	int mode;
	long hold_ms;
	/* a machine for each local player */
	struct ae_zoom_state states[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
} zoom = { (unsigned long)-1 };

/* the settings, read again when a setting has been written; a mode change lets go of the presses in progress */
static void zoom_read(void)
{
	int mode;
	short index;

	if (zoom.read && zoom.read_at == config_changes())
		return;
	zoom.read = 1;
	zoom.read_at = config_changes();
	mode = ae_zoom_mode_from_text(config_string("input.zoom_mode"));
	zoom.hold_ms = ae_zoom_hold_ms(config_real("input.zoom_hold_time"));
	if (mode != zoom.mode)
	{
		zoom.mode = mode;
		for (index = 0; index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; index++)
			ae_zoom_reset(&zoom.states[index]);
	}
}

void ae_zoom_note_held(unsigned long *player_control_flags, int held)
{
	if (held)
		*player_control_flags |= 1ul << HELD_BIT;
	else
		*player_control_flags &= ~(1ul << HELD_BIT);
}

void ae_zoom_filter_press(unsigned long *player_control_flags, int zoom_bit)
{
	zoom_read();
	if (zoom.mode != AE_ZOOM_TOGGLE)
		*player_control_flags &= ~(1ul << zoom_bit);
}

short ae_zoom_update(short local_player_index, short zoom_level, long weapon_index, int may_act,
	unsigned long player_control_flags)
{
	short count = 0;
	int blocked;

	zoom_read();
	if (zoom.mode == AE_ZOOM_TOGGLE || local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return zoom_level;
	if (weapon_index != NONE)
	{
		struct weapon_datum *weapon = weapon_get(weapon_index);

		count = weapon_definition_get(weapon->definition_index)->weapon.zoom_level_count;
	}
	blocked = weapon_index == NONE || !may_act || weapon_reloading(weapon_index);
	return ae_zoom_step(&zoom.states[local_player_index], zoom.mode, zoom.hold_ms,
		(player_control_flags >> HELD_BIT) & 1, (long)system_milliseconds(), zoom_level, count, blocked);
}

void ae_zoom_log_settings(void)
{
	zoom_read();
	platform_log("zoom: mode %s, hold time %g",
		zoom.mode == AE_ZOOM_HOLD ? "hold" : zoom.mode == AE_ZOOM_BOTH ? "both" : "toggle",
		(double)zoom.hold_ms / 1000.0);
}
