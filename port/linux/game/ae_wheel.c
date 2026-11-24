/*
AE_WHEEL.C

The CALLOUT WHEEL's input (ae_wheel.h): each local player's press, which entry the stick or the mouse has focused, and
the view held still while it is open. The rules (the press, the pick, the grace) are ae_wheel_rules.c's.

The stick: the player's look stick as input_abstraction has it (so southpaw follows), turned back to the stick's own
direction (the look's yaw is the stick's x reversed, its pitch the stick's y, reversed again when the look is inverted).
The mouse: its raw counts (xinput_sdl.c's read, before the sensitivity, the mouse style, the zoom and the vehicle scales),
which add up in a pointer held within AE_WHEEL_MOUSE_RADIUS; the look's own delta carries all of those scales and a
pad's stick besides.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "math/real_math.h"
#include "game/game.h"
#include "input/input.h"
#include "input/input_abstraction.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"

#include "ae_wheel.h"
#include "ae_wheel_layout.h"
#include "ae_wheel_rules.h"

void platform_log(char const *format, ...);

#define WHEEL_PLAYERS 4

/* the mouse's counts to the wheel's pixels: unplayed, a first guess (a 1 cm flick at 800 dpi is about 300 counts) */
#define WHEEL_MOUSE_SCALE 0.5f

/* the opening fade */
#define WHEEL_OPEN_MS 80

/* input_abstraction.c's, which keeps it to itself (player_control.c has this copy too) */
struct game_input_state
{
	byte buttons[12];
	real forward_movement;
	real strafe;
	real yaw;
	real pitch;
};

typedef char wheel_input_state_size_assert[sizeof(struct game_input_state) == 0x1C ? 1 : -1];

struct wheel
{
	struct ae_wheel_press press;
	struct ae_wheel_focus held;
	int focus;
	/* the dead state of the last tick, once there was one */
	int known;
	int dead;
	/* opened by the mouse: the pointer, in pixels, y up; else by the stick */
	int from_mouse;
	float mouse_seen;
	float x, y;
	unsigned long opened_ms;
	/* the last send: when (0 for none), which entry, and whether the sender was dead: the ring flashes and closes */
	unsigned long sent_ms;
	int sent_entry;
	int sent_dead;
};

static struct wheel wheels[WHEEL_PLAYERS];
/* the calls each local player has sent are in their profile (display.wheel_calls_sent in ae.toml, to the legend's limit) */
#define WHEEL_CALLS_SETTING "display.wheel_calls_sent"
int ae_config_player_text(int player, char const *name, char *text, size_t size);
int ae_player_setting_put(short local_player, char const *name, char const *value);
static float mouse_x, mouse_y;

/* ---------- private code */

static int wheel_calls_read(short local_player_index)
{
	char text[16];

	text[0] = 0;
	if (!ae_config_player_text(local_player_index, WHEEL_CALLS_SETTING, text, sizeof(text)))
		return 0;
	return ae_wheel_calls_from_text(text);
}

/* a call sent: the profile's count goes up one, to the limit (nothing is written once it is there) */
static void wheel_count_call(short local_player_index)
{
	int before = wheel_calls_read(local_player_index);
	int after = ae_wheel_calls_after(before);
	char text[16];

	if (after == before)
		return;
	snprintf(text, sizeof(text), "%d", after);
	ae_player_setting_put(local_player_index, WHEEL_CALLS_SETTING, text);
}

static boolean wheel_valid(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < WHEEL_PLAYERS;
}

/* the stick's direction, x right and y up, each -1 to 1 */
static void wheel_stick(short local_player_index, float *x, float *y)
{
	struct game_input_state const *state = input_abstraction_get_input_state(local_player_index);

	*x = 0.0f;
	*y = 0.0f;
	if (!state)
		return;
	*x = -state->yaw;
	*y = input_abstraction_port_look_inverted(local_player_index) ? -state->pitch : state->pitch;
}

/* this tick's focus, with the wheel open */
static void wheel_aim(struct wheel *wheel, short local_player_index, int keyboard, long now)
{
	int picked;

	/* (the keyboard player's mouse picks whichever button opened the wheel) */
	if (keyboard && (mouse_x != 0.0f || mouse_y != 0.0f))
	{
		wheel->from_mouse = 1;
		wheel->mouse_seen += fabsf(mouse_x) + fabsf(mouse_y);
	}
	if (wheel->from_mouse)
	{
		/* (a screen's y runs down) */
		if (keyboard)
			ae_wheel_mouse_add(&wheel->x, &wheel->y, mouse_x * WHEEL_MOUSE_SCALE, -mouse_y * WHEEL_MOUSE_SCALE);
		picked = ae_wheel_pick_mouse(wheel->focus, wheel->x, wheel->y);
	}
	else
	{
		float x, y;

		wheel_stick(local_player_index, &x, &y);
		picked = ae_wheel_pick(wheel->focus, x, y);
	}
	if (!ae_wheel_entry_shown(picked, wheel->dead))
		picked = AE_WHEEL_NONE;
	wheel->focus = ae_wheel_focus_held(&wheel->held, picked, !wheel->from_mouse, now);
}

static void wheel_clear_aim(struct wheel *wheel)
{
	wheel->focus = AE_WHEEL_NONE;
	csmemset(&wheel->held, 0, sizeof(wheel->held));
	wheel->x = 0.0f;
	wheel->y = 0.0f;
}

/* ---------- public code */

void ae_wheel_note_pad(unsigned long *player_control_flags, int gamepad_index)
{
	struct gamepad_state const *gamepad = gamepad_index >= 0 ? input_get_gamepad_state((short)gamepad_index) : NULL;

	/* (the analog button counts as down where the game's own buttons do: past its threshold) */
	if (gamepad && gamepad->buttons[_gamepad_analog_button_white])
		*player_control_flags |= 1ul << AE_MARK_PAD_BIT;
	else
		*player_control_flags &= ~(1ul << AE_MARK_PAD_BIT);
}

int ae_wheel_own_button(short button_preset)
{
	return button_preset == _button_preset_universal_reclaimer || button_preset == _button_preset_universal_zoom_and_shoot;
}

int ae_wheel_pad_owns(short local_player_index)
{
	struct player_profile profile;

	if (!wheel_valid(local_player_index))
		return 0;
	player_ui_get_active_player_profile(local_player_index, &profile);
	return ae_wheel_own_button((short)profile.controller_settings.button_preset);
}

void ae_wheel_filter(short local_player_index, real_euler_angles2d *facing_delta)
{
	if (wheel_valid(local_player_index) && wheels[local_player_index].press.open)
	{
		facing_delta->yaw = 0.0f;
		facing_delta->pitch = 0.0f;
	}
}

void ae_wheel_reset(void)
{
	short index;

	csmemset(wheels, 0, sizeof(wheels));
	mouse_x = 0.0f;
	mouse_y = 0.0f;
	for (index = 0; index < WHEEL_PLAYERS; index++)
	{
		wheels[index].focus = AE_WHEEL_NONE;
		/* (a button held across the new game: nothing of that press counts) */
		wheels[index].press.ignore = 1;
	}
}

int ae_wheel_step(short local_player_index, int pad_down, int key_down, int keyboard, int interrupted, int dead, long now,
	int *entry)
{
	struct wheel *wheel;
	int result;

	*entry = AE_WHEEL_NONE;
	if (!wheel_valid(local_player_index))
		return AE_PRESS_NOTHING;
	wheel = &wheels[local_player_index];
	if (wheel->known && dead != wheel->dead)
		interrupted = 1;
	wheel->known = 1;
	wheel->dead = dead;
	if (wheel->press.open && !interrupted)
		wheel_aim(wheel, local_player_index, keyboard, now);
	result = ae_wheel_press_step(&wheel->press, pad_down || key_down, interrupted, now, wheel->focus);
	switch (result)
	{
	case AE_PRESS_OPEN:
		wheel_clear_aim(wheel);
		wheel->mouse_seen = 0.0f;
		wheel->from_mouse = !pad_down;
		wheel->opened_ms = system_milliseconds();
		wheel->sent_ms = 0;
		break;
	case AE_PRESS_SEND:
		*entry = wheel->focus;
		wheel->sent_ms = system_milliseconds();
		wheel->sent_entry = wheel->focus;
		wheel->sent_dead = wheel->dead;
		wheel_count_call(local_player_index);
		wheel_clear_aim(wheel);
		break;
	case AE_PRESS_CANCEL:
		platform_log("wheel: view %d: closed: by mouse %d, keyboard %d, counts seen %.0f, pointer %.0f %.0f", local_player_index,
			wheel->from_mouse, keyboard, wheel->mouse_seen, wheel->x, wheel->y);
		wheel_clear_aim(wheel);
		break;
	}
	/* (the counts of this tick are spent, whether the wheel is open or not) */
	if (keyboard)
	{
		mouse_x = 0.0f;
		mouse_y = 0.0f;
	}
	return result;
}

boolean ae_wheel_open(short local_player_index)
{
	return wheel_valid(local_player_index) && wheels[local_player_index].press.open;
}

int ae_wheel_focus(short local_player_index)
{
	if (!ae_wheel_open(local_player_index))
		return AE_WHEEL_NONE;
	return wheels[local_player_index].focus;
}

real ae_wheel_open_part(short local_player_index)
{
	unsigned long elapsed;

	if (!ae_wheel_open(local_player_index))
		return 0.0f;
	elapsed = system_milliseconds() - wheels[local_player_index].opened_ms;
	return elapsed >= WHEEL_OPEN_MS ? 1.0f : (real)elapsed / (real)WHEEL_OPEN_MS;
}

boolean ae_wheel_is_dead(short local_player_index)
{
	return wheel_valid(local_player_index) && wheels[local_player_index].press.open && wheels[local_player_index].dead;
}

boolean ae_wheel_closing(short local_player_index, int *entry, boolean *dead, real *flash, real *alpha)
{
	struct wheel const *wheel;
	float flash_part, alpha_part;

	if (!wheel_valid(local_player_index))
		return FALSE;
	wheel = &wheels[local_player_index];
	if (!wheel->sent_ms || wheel->press.open ||
		!ae_wheel_motion((long)(system_milliseconds() - wheel->sent_ms), &flash_part, &alpha_part))
	{
		return FALSE;
	}
	*entry = wheel->sent_entry;
	*dead = wheel->sent_dead != 0;
	*flash = flash_part;
	*alpha = alpha_part;
	return TRUE;
}

int ae_wheel_calls_sent(short local_player_index)
{
	return wheel_valid(local_player_index) ? wheel_calls_read(local_player_index) : 0;
}

void ae_wheel_note_mouse(float x, float y)
{
	mouse_x += x;
	mouse_y += y;
}
