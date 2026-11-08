/*
AE_AIM_MODERN.C

AIM CONTROL: MODERN in the game (ae_aim_modern.h).
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "math/integer_math.h"
#include "input/input.h"
#include "input/input_abstraction.h"
#include "../src/port_config.h"
#include "../src/ae_config_players.h"

#include "ae_aim_modern_rules.h"
#include "ae_aim_modern.h"

enum { TIMERS = 4, JOYSTICK_DEFAULT = 0, JOYSTICK_SOUTHPAW = 1 };

static float yaw_timers[TIMERS], pitch_timers[TIMERS];

static int modern_set(void)
{
	/* (read again only when a setting has changed) */
	static unsigned long changes_seen = (unsigned long)-1;
	static int modern;

	if (ae_config_player_changes() != changes_seen)
	{
		const char *text = config_string("input.aim_control");

		changes_seen = ae_config_player_changes();
		modern = text && !strcmp(text, "modern");
	}
	return modern;
}

static long acceleration_menu(void)
{
	static unsigned long changes_seen = (unsigned long)-1;
	static long menu = 1;

	if (ae_config_player_changes() != changes_seen)
	{
		changes_seen = ae_config_player_changes();
		menu = config_integer("input.look_acceleration");
	}
	return menu;
}

static long string_menu(const char *name, long same, long maximum)
{
	const char *text = config_string(name);
	char *end;
	long menu;

	if (!strcmp(text, "same"))
		return same;
	menu = strtol(text, &end, 10);
	if (!text[0] || *end || menu < 0 || menu > maximum)
		return same;
	return menu;
}

static long radial_menu(void)
{
	static unsigned long changes_seen = (unsigned long)-1;
	static long menu = -1;

	if (ae_config_player_changes() != changes_seen)
	{
		changes_seen = ae_config_player_changes();
		menu = string_menu("input.look_radial_deadzone", -1, 20);
	}
	return menu;
}

static long vertical_menu(void)
{
	static unsigned long changes_seen = (unsigned long)-1;
	static long menu;

	if (ae_config_player_changes() != changes_seen)
	{
		changes_seen = ae_config_player_changes();
		menu = string_menu("input.look_sensitivity_vertical", 0, 10);
	}
	return menu;
}

static int layout_of(short gamepad_index)
{
	struct game_input_preferences preferences;

	input_abstraction_get_local_player_preferences(gamepad_index, &preferences);
	return preferences.joystick_controls;
}

int ae_aim_modern_active(short gamepad_index)
{
	int layout;

	if (gamepad_index < 0 || gamepad_index >= MAXIMUM_GAMEPADS || !modern_set())
		return 0;
	layout = layout_of(gamepad_index);
	return layout == JOYSTICK_DEFAULT || layout == JOYSTICK_SOUTHPAW;
}

int ae_aim_modern_unsquared(short controller_index, int stick)
{
	if (!ae_aim_modern_active(controller_index))
		return 0;
	return layout_of(controller_index) == JOYSTICK_DEFAULT ? stick == _gamepad_stick_right
		: stick == _gamepad_stick_left;
}

int ae_aim_modern_dead_zones(short gamepad_index, short *left_x, short *left_y, short *right_x, short *right_y,
	long left_percent, long right_percent)
{
	if (!ae_aim_modern_active(gamepad_index))
		return 0;
	ae_aim_modern_dead_zone_sticks(left_x, left_y, right_x, right_y, left_percent, right_percent, radial_menu(),
		layout_of(gamepad_index) == JOYSTICK_SOUTHPAW);
	return 1;
}

void ae_aim_modern_rate(short gamepad_index, float stick_yaw, float stick_pitch, float yaw_spin_scale,
	float pitch_spin_scale, float yaw_rate, float pitch_rate, float *look_yaw, float *look_pitch)
{
	float curve_yaw;
	float curve_pitch;
	struct game_input_preferences preferences;

	if (!ae_aim_modern_active(gamepad_index))
		return;
	ae_aim_modern_stick(stick_yaw, stick_pitch, &curve_yaw, &curve_pitch);
	input_abstraction_get_local_player_preferences(gamepad_index, &preferences);
	*look_yaw = curve_yaw * yaw_spin_scale * yaw_rate;
	*look_pitch = curve_pitch * pitch_spin_scale * pitch_rate *
		ae_aim_modern_pitch_ratio(vertical_menu(), preferences.pitch_rate);
}

void ae_aim_modern_ramp_apply(short gamepad_index, short local_player_index, float scaled_yaw, float scaled_pitch,
	float time_delta_sec, float *look_yaw, float *look_pitch)
{
	float k;

	if (local_player_index < 0 || local_player_index >= TIMERS)
		return;
	if (!ae_aim_modern_active(gamepad_index))
	{
		yaw_timers[local_player_index] = 0.f;
		pitch_timers[local_player_index] = 0.f;
		return;
	}
	k = ae_aim_modern_k(acceleration_menu());
	*look_yaw *= ae_aim_modern_ramp(&yaw_timers[local_player_index], (float)fabs(scaled_yaw), AE_AIM_RAMP_YAW, k,
		time_delta_sec);
	*look_pitch *= ae_aim_modern_ramp(&pitch_timers[local_player_index], (float)fabs(scaled_pitch), AE_AIM_RAMP_PITCH, k,
		time_delta_sec);
}
