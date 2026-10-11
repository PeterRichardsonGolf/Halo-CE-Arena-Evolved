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
#include "halo_keyboard.h"
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

void ae_zoom_note_held(unsigned long *player_control_flags, int held)
{
	ae_zoom_flags_note_held(player_control_flags, held);
}

/* the flashlight button: the controller's own, not the keyboard's key (halo_keyboard_actions): the key keeps toggling
the light while zoomed, as the keyboard player has the wheel to step the level with */
void ae_zoom_note_flash(unsigned long *player_control_flags, int down, short gamepad_index)
{
	if (down && (halo_keyboard_actions(gamepad_index) & (1UL << HALO_KEYBOARD_FLASHLIGHT)))
		down = 0;
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

/* a few frames' hitch is still the zoom's */
#define WHEEL_TAKEN_MS 250

int ae_zoom_wheel_taken(void)
{
	return zoom.wheel_taken && system_milliseconds() - zoom.wheel_taken_at < WHEEL_TAKEN_MS;
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
	int first;
	long player_index;
	short level;

	zoom_read();
	/* the wheel is the first controller's (the keyboard and mouse's) player's alone */
	player_index = local_player_get_player_index(local_player_index);
	first = player_index != NONE && player_get(player_index)->local_player_index == 0;
	if (zoom.bank.mode == AE_ZOOM_TOGGLE)
	{
		if (first)
			zoom.wheel_taken = 0;
		return zoom_level;
	}
	if (weapon_index != NONE)
	{
		struct weapon_datum *weapon = weapon_get(weapon_index);

		count = weapon_definition_get(weapon->definition_index)->weapon.zoom_level_count;
	}
	blocked = weapon_index == NONE || !may_act || weapon_reloading(weapon_index);
	if (first)
	{
		wheel = zoom.wheel_pending;
		zoom.wheel_pending = 0;
	}
	level = ae_zoom_bank_step_ex(&zoom.bank, local_player_index, zoom.hold_ms, player_control_flags,
		(long)system_milliseconds(), zoom_level, count, blocked, weapon_index, wheel);
	if (first)
	{
		/* the wheel is the zoom's only while a held zoom can act (the final blob: a menu that takes the controller
		zeroed it) */
		zoom.wheel_taken = ae_zoom_wheel_wanted(zoom.bank.mode, player_control_flags, count, blocked, level);
		zoom.wheel_taken_at = system_milliseconds();
		if (!zoom.wheel_taken)
			zoom.wheel_pending = 0;
	}
	return level;
}

void ae_zoom_new_game(void)
{
	ae_zoom_bank_forget(&zoom.bank);
	zoom.wheel_taken = 0;
	zoom.wheel_pending = 0;
}

void ae_zoom_log_settings(void)
{
	zoom_read();
	ae_zoom_new_game();
	platform_log("zoom: mode %s, hold time %g, reset %s",
		zoom.bank.mode == AE_ZOOM_HOLD ? "hold" : zoom.bank.mode == AE_ZOOM_BOTH ? "both" : "toggle",
		(double)zoom.hold_ms / 1000.0, zoom.bank.reset == AE_ZOOM_RESET_LAST ? "last" : "first");
}
