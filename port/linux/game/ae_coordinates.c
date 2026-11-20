/*
AE_COORDINATES.C

SHOW COORDINATES (ae_coordinates.h).
*/

#include "cseries.h"
#include "camera/observer.h"
#include "game/game.h"
#include "game/players.h"
#include "interface/ui_widget.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "units/bipeds.h"
#include "units/units.h"
#include "../src/port_config.h"

#include "ae_coordinates.h"
#include "ae_dev_mode.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* DEV MODE's speed readout (debug.speed_readout): player 1's ground speed, how far the move stick is pushed and how
far down the crouch is, after the coordinates' place on the performance line */
static int speed_readout_shown(void)
{
	return ae_dev_mode_on() && config_boolean("debug.speed_readout") != 0;
}

int ae_coordinates_shown(void)
{
	return config_boolean("display.coordinates") != 0 || speed_readout_shown();
}

void ae_coordinates_speed_format(char *text, long size, float units_a_second, float stick, float crouch)
{
	long stick_percent = (long)(stick * 100.0f + 0.5f), crouch_percent = (long)(crouch * 100.0f + 0.5f);

	if (stick_percent > 100)
		stick_percent = 100;
	snprintf(text, (size_t)size, "SPEED %.2f WU/S  STICK %ld%%  CROUCH %ld%%", (double)units_a_second, stick_percent,
		crouch_percent);
}

/* the readout's text: the speed over the ground is the body's move between two game ticks (a tick is 1/30 s), held
while the game's clock stands still */
static void speed_readout_append(char *line, long size)
{
	static long last_tick = NONE, last_unit = NONE;
	static real_point3d last_origin;
	static float speed = 0.0f;
	long player_index = local_player_get_player_index(0);
	struct player_datum *player = player_index != NONE ? player_get(player_index) : NULL;
	struct biped_datum *biped = player && player->unit_index != NONE ? biped_try_and_get(player->unit_index) : NULL;
	real_point3d origin;
	char text[72];
	size_t length;
	long tick = game_time_get();
	float stick;

	if (!biped)
	{
		last_unit = NONE;
		return;
	}
	object_get_origin(player->unit_index, &origin);
	if (last_unit != player->unit_index || tick < last_tick)
	{
		speed = 0.0f;
	}
	else if (tick > last_tick)
	{
		double dx = (double)origin.x - (double)last_origin.x, dy = (double)origin.y - (double)last_origin.y;

		speed = (float)(sqrt(dx * dx + dy * dy) * 30.0 / (double)(tick - last_tick));
	}
	if (last_unit != player->unit_index || tick != last_tick)
	{
		last_unit = player->unit_index;
		last_tick = tick;
		last_origin = origin;
	}
	stick = (float)sqrt((double)biped->unit.throttle.i * (double)biped->unit.throttle.i +
		(double)biped->unit.throttle.j * (double)biped->unit.throttle.j);
	ae_coordinates_speed_format(text, (long)sizeof(text), speed, stick, biped->biped.crouch);
	length = strlen(line);
	if ((long)length < size)
		snprintf(line + length, (size_t)size - length, "%s%s", length ? "  " : "", text);
}

void ae_coordinates_format(char *text, long size, float x, float y, float z, float forward_i, float forward_j)
{
	double facing = atan2((double)forward_j, (double)forward_i) * 180.0 / 3.14159265358979;
	long degrees;

	if (facing < 0.0)
		facing += 360.0;
	degrees = (long)(facing + 0.5) % 360;
	snprintf(text, (size_t)size, "X %.2f Y %.2f Z %.2f  FACING %ld", (double)x, (double)y, (double)z, degrees);
}

void ae_coordinates_append(char *line, long size)
{
	long player_index;
	struct player_datum *player;
	struct observer_result const *camera;
	real_point3d origin;
	char text[64];
	size_t length;

	/* (not at the main menu: its background map's camera is no place a player stands) */
	if (!ae_coordinates_shown() || !game_in_progress() || main_menu_is_active() || size <= 0)
		return;
	if (speed_readout_shown())
		speed_readout_append(line, size);
	if (!config_boolean("display.coordinates"))
		return;
	player_index = local_player_get_player_index(0);
	player = player_index != NONE ? player_get(player_index) : NULL;
	camera = observer_get_camera(0);
	if (!camera)
		return;
	origin = camera->position;
	if (player && player->unit_index != NONE)
		object_get_origin(player->unit_index, &origin);
	ae_coordinates_format(text, (long)sizeof(text), origin.x, origin.y, origin.z, camera->forward.i,
		camera->forward.j);
	length = strlen(line);
	snprintf(line + length, (size_t)size - length, "%s%s", length ? "  " : "", text);
}
