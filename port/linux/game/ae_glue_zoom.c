/*
AE_GLUE_ZOOM.C

The zoom action's mode and hold time, read from config.toml, and the functions
the hook lines call (ae_glue_zoom.h). The state machine is ae_zoom.c's. The
TOGGLE mode (the default) changes nothing: the engine steps the level itself,
and these functions return what they were given.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "networking/network_connection.h"
#include "game/players.h"
#include "halo_keyboard.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "../src/port_config.h"
#include "../src/ae_config_players.h"
#include "../src/ae_platform.h"
#include "ae_seats.h"
#include "ae_glue_zoom.h"
#include "ae_zoom.h"

void platform_log(char const *format, ...);
int platform_input_scheme(void);
boolean weapon_reloading(long weapon_index);

static struct
{
	/* each local player's settings as read (their own, of the input they play with now) */
	unsigned long read_at[AE_ZOOM_PLAYERS];
	int read[AE_ZOOM_PLAYERS];
	/* WHEEL ZOOM (input.wheel_zoom): the player plays with the mouse and has it on */
	int wheel_zoom[AE_ZOOM_PLAYERS];
	long hold_ms[AE_ZOOM_PLAYERS];
	struct ae_zoom_bank bank;
	/* the mouse wheel's scroll to take for the zoom level (+1 up, -1 down, 0 none), and whether the zoom is held by
	the player on the first controller (the keyboard and mouse's), published each tick: for 100 ms, then not */
	int wheel_pending;
	int wheel_taken;
	unsigned long wheel_taken_at;
} zoom;

/* the keyboard and mouse's player: the JOIN screen's seat of the keyboard, else player 1 (the first controller's) */
static short keyboard_player(void)
{
	struct ae_seats seats;

	if (!ae_seats_active())
		return 0;
	ae_seats_get(&seats);
	return (short)ae_seats_keyboard(&seats);
}

/* the zoom settings of the input this player plays with now (0.3.7: the keyboard and mouse have their own, "same" is
the controller's), the player's own (their profile's); read again when a setting changes, or the player or input */
static void zoom_read(short local_player_index)
{
	int previous, mouse;
	unsigned long read_at;
	char const *mode, *reset;
	double hold;

	if (local_player_index < 0 || local_player_index >= AE_ZOOM_PLAYERS)
		return;
	previous = ae_config_player_enter(local_player_index);
	mouse = local_player_index == keyboard_player() && platform_input_scheme() == 0;
	read_at = ae_config_player_changes() * 2 + (unsigned long)mouse;
	if (zoom.read[local_player_index] && zoom.read_at[local_player_index] == read_at)
	{
		ae_config_player_leave(previous);
		return;
	}
	zoom.read[local_player_index] = 1;
	zoom.read_at[local_player_index] = read_at;
	mode = mouse ? config_string("input.mouse_zoom_mode") : "same";
	reset = mouse ? config_string("input.mouse_zoom_reset") : "same";
	hold = mouse ? config_real("input.mouse_zoom_hold_time") : 0.0;
	if (!strcmp(mode, "same") || !*mode)
		mode = config_string("input.zoom_mode");
	if (!strcmp(reset, "same") || !*reset)
		reset = config_string("input.zoom_reset");
	if (!(hold > 0.0))
		hold = config_real("input.zoom_hold_time");
	zoom.hold_ms[local_player_index] = ae_zoom_hold_ms(hold);
	zoom.wheel_zoom[local_player_index] = mouse && config_boolean("input.wheel_zoom") != 0;
	/* (that player's own: a change lets go of their press only, never another player's) */
	ae_zoom_bank_set_player(&zoom.bank, local_player_index, ae_zoom_mode_from_text(mode),
		ae_zoom_reset_from_text(reset));
	ae_config_player_leave(previous);
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
	zoom_read(local_player_index);
	ae_zoom_flags_filter_press(ae_zoom_bank_mode(&zoom.bank, local_player_index), player_control_flags, zoom_bit);
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
	int wheel_zoom;
	int toggle;
	long player_index;
	short level;

	zoom_read(local_player_index);
	/* the wheel is the first controller's (the keyboard and mouse's) player's alone */
	player_index = local_player_get_player_index(local_player_index);
	first = player_index != NONE && player_get(player_index)->local_player_index == 0;
	wheel_zoom = first && zoom.wheel_zoom[local_player_index];
	toggle = ae_zoom_bank_mode(&zoom.bank, local_player_index) == AE_ZOOM_TOGGLE;
	if (toggle && !wheel_zoom)
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
	/* (TOGGLE: the engine stepped the level itself) */
	level = toggle ? zoom_level : ae_zoom_bank_step_ex(&zoom.bank, local_player_index, zoom.hold_ms[local_player_index],
		player_control_flags,
		(long)system_milliseconds(), zoom_level, count, blocked, weapon_index, wheel);
	/* WHEEL ZOOM: a notch with the zoom not held steps the level by itself (held, the mode's machine took it) */
	if (wheel_zoom && wheel && !((player_control_flags >> AE_ZOOM_HELD_BIT) & 1) &&
		ae_zoom_wheel_free(wheel_zoom, count, blocked))
	{
		short before = level;

		level = ae_zoom_wheel_step(level, count, wheel);
		platform_log("zoom: wheel zoom %s, level %d to %d", wheel > 0 ? "up" : "down", (int)before, (int)level);
	}
	if (first)
	{
		/* the wheel is the zoom's only while a held zoom can act (the final blob: a menu that takes the controller
		zeroed it), or with WHEEL ZOOM while the weapon in hand zooms (reloading too: a notch then does nothing, never
		a weapon switch) */
		zoom.wheel_taken = (!toggle && ae_zoom_wheel_wanted(ae_zoom_bank_mode(&zoom.bank, local_player_index),
			player_control_flags, count, blocked, level)) || ae_zoom_wheel_free(wheel_zoom, count, FALSE);
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
	zoom_read(0);
	ae_zoom_new_game();
	platform_log("zoom: mode %s, hold time %g, reset %s, wheel zoom %s",
		zoom.bank.modes[0] == AE_ZOOM_HOLD ? "hold" : zoom.bank.modes[0] == AE_ZOOM_BOTH ? "both" : "toggle",
		(double)zoom.hold_ms[0] / 1000.0, zoom.bank.resets[0] == AE_ZOOM_RESET_LAST ? "last" : "first",
		zoom.wheel_zoom[0] ? "on" : "off");
}

int ae_zoom_blur_allowed(void)
{
	return config_boolean("display.zoom_blur") != 0;
}
