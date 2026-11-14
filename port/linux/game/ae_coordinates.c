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
#include "../src/port_config.h"

#include "ae_coordinates.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int ae_coordinates_shown(void)
{
	return config_boolean("display.coordinates") != 0;
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
