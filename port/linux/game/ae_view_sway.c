/*
AE_VIEW_SWAY.C

The first-person weapon's sway, the game's side (ae_view_sway.h).
*/

#include <math.h>

#include "cseries.h"
#include "main/main.h"
#include "math/real_math.h"
#include "../src/port_config.h"

#include "ae_view_sway.h"
#include "ae_view_sway_rules.h"

#define PLAYERS 4

static struct
{
	int valid;
	real yaw, pitch;
	real throttle_i, throttle_j;
} frames[PLAYERS];

static int stock_setting(real *strength, real *acceleration_scale, real *speed_scale)
{
	float a, s;
	double percent = config_real("display.viewmodel_sway");
	double speed = config_real("display.viewmodel_sway_speed");

	*strength = ae_view_sway_strength((float)percent);
	ae_view_sway_speed((float)speed, &a, &s);
	*acceleration_scale = a;
	*speed_scale = s;
	return *strength == 1.0f && a == 1.0f && s == 1.0f;
}

int ae_view_sway_tick(short local_player_index, real throttle_i, real throttle_j)
{
	real strength, acceleration_scale, speed_scale;

	if (local_player_index >= 0 && local_player_index < PLAYERS)
	{
		frames[local_player_index].throttle_i = throttle_i;
		frames[local_player_index].throttle_j = throttle_j;
	}
	return stock_setting(&strength, &acceleration_scale, &speed_scale);
}

/* a facing's change, the short way round */
static real turned(real from, real to)
{
	real change = (real)fmod((double)(to - from), 2.0 * 3.14159265358979);

	if (change > 3.14159265f)
		change -= 2.0f * 3.14159265f;
	else if (change < -3.14159265f)
		change += 2.0f * 3.14159265f;
	return change;
}

/* one spring of a pair, its velocity kept a tick's in the game's own field (as the stock step keeps it) */
static void step(real *position, real *velocity, real target, real acceleration, real speed, real seconds)
{
	float p = *position, v = *velocity * AE_VIEW_SWAY_TICKS_PER_SECOND;

	ae_view_sway_step(&p, &v, target, acceleration, speed, seconds, -1.0f, 1.0f);
	*position = p;
	*velocity = v / AE_VIEW_SWAY_TICKS_PER_SECOND;
}

void ae_view_sway_frame(short local_player_index, union real_vector2d *position, union real_vector2d *position_velocity,
	union real_vector2d *turning, union real_vector2d *turning_velocity, real yaw, real pitch)
{
	real strength, acceleration_scale, speed_scale, seconds;

	if (local_player_index < 0 || local_player_index >= PLAYERS)
		return;
	if (stock_setting(&strength, &acceleration_scale, &speed_scale))
	{
		frames[local_player_index].valid = FALSE;
		return;
	}
	seconds = main_get_seconds_elapsed();
	if (frames[local_player_index].valid && seconds > 0.0f)
	{
		real turn_i = ae_view_sway_turn_target(turned(frames[local_player_index].yaw, yaw), seconds);
		real turn_j = -ae_view_sway_turn_target(turned(frames[local_player_index].pitch, pitch), seconds);

		step(&position->i, &position_velocity->i, frames[local_player_index].throttle_i * strength,
			AE_VIEW_SWAY_WALK_ACCELERATION * acceleration_scale, AE_VIEW_SWAY_WALK_SPEED * speed_scale, seconds);
		step(&position->j, &position_velocity->j, frames[local_player_index].throttle_j * strength,
			AE_VIEW_SWAY_WALK_ACCELERATION * acceleration_scale, AE_VIEW_SWAY_WALK_SPEED * speed_scale, seconds);
		step(&turning->i, &turning_velocity->i, turn_i * strength, AE_VIEW_SWAY_TURN_ACCELERATION * acceleration_scale,
			AE_VIEW_SWAY_TURN_SPEED * speed_scale, seconds);
		step(&turning->j, &turning_velocity->j, turn_j * strength, AE_VIEW_SWAY_TURN_ACCELERATION * acceleration_scale,
			AE_VIEW_SWAY_TURN_SPEED * speed_scale, seconds);
	}
	frames[local_player_index].yaw = yaw;
	frames[local_player_index].pitch = pitch;
	frames[local_player_index].valid = TRUE;
}
