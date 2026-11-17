/*
AE_MAP_VOLUMES.C

MAP VOLUMES (ae_map_volumes.h, ae_map_volumes_rules.h). Log: "map volumes: ...".
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"

#include "ae_map_volumes.h"
#include "ae_map_volumes_rules.h"

#include <string.h>

void platform_log(char const *format, ...);

#define AE_MAP_VOLUMES_KILLS 16
#define AE_MAP_VOLUMES_LIFTS 8
#define AE_MAP_VOLUMES_PLAYERS 16

static struct
{
	struct scenario *scenario;
	long count;
	long last_now;
	short kills[AE_MAP_VOLUMES_KILLS];
	long kill_count;
	short lift_from[AE_MAP_VOLUMES_LIFTS];
	short lift_to[AE_MAP_VOLUMES_LIFTS];
	long lift_count;
	long last_lift[AE_MAP_VOLUMES_PLAYERS];
	long last_kill[AE_MAP_VOLUMES_PLAYERS];
} map_volumes;

static struct scenario_trigger_volume *volume_get(struct scenario *scenario, long index)
{
	return TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, index, struct scenario_trigger_volume);
}

/* the map's kill zones and lifts, read when the scenario or the game is new */
static void map_volumes_read(struct scenario *scenario, long now)
{
	long index, other;

	if (scenario == map_volumes.scenario && scenario->trigger_volumes.count == map_volumes.count &&
		now >= map_volumes.last_now)
	{
		map_volumes.last_now = now;
		return;
	}
	memset(&map_volumes, 0, sizeof(map_volumes));
	map_volumes.scenario = scenario;
	map_volumes.count = scenario->trigger_volumes.count;
	map_volumes.last_now = now;
	for (index = 0; index < AE_MAP_VOLUMES_PLAYERS; index++)
		map_volumes.last_lift[index] = map_volumes.last_kill[index] = -1000000L;
	for (index = 0; index < scenario->trigger_volumes.count; index++)
	{
		struct scenario_trigger_volume *volume = volume_get(scenario, index);
		char exit_name[8];

		switch (ae_map_volume_kind(volume->name))
		{
		case AE_MAP_VOLUME_KILL:
			if (map_volumes.kill_count < AE_MAP_VOLUMES_KILLS)
			{
				map_volumes.kills[map_volumes.kill_count++] = (short)index;
				platform_log("map volumes: '%s' kills who falls in", volume->name);
			}
			break;
		case AE_MAP_VOLUME_LIFT:
			if (!ae_map_volume_lift_exit(volume->name, exit_name, (int)sizeof(exit_name)) ||
				map_volumes.lift_count >= AE_MAP_VOLUMES_LIFTS)
			{
				break;
			}
			for (other = 0; other < scenario->trigger_volumes.count; other++)
			{
				if (other != index && ae_map_volume_same_name(volume_get(scenario, other)->name, exit_name))
				{
					map_volumes.lift_from[map_volumes.lift_count] = (short)index;
					map_volumes.lift_to[map_volumes.lift_count++] = (short)other;
					platform_log("map volumes: '%s' lifts to '%s'", volume->name, exit_name);
					break;
				}
			}
			break;
		default:
			break;
		}
	}
}

boolean ae_map_volumes_kills(long volume)
{
	struct scenario *scenario = global_scenario_get();

	return (boolean)(scenario && game_engine_running() && VALID_INDEX(volume, scenario->trigger_volumes.count) &&
		ae_map_volume_kind(volume_get(scenario, volume)->name) == AE_MAP_VOLUME_KILL);
}

/* a lift's exit: the middle of its volume's floor, a little up */
static void lift_exit_point(struct scenario_trigger_volume const *volume, real_point3d *point)
{
	real_matrix4x3 matrix;
	real_point3d local;

	if (volume->type != _scenario_trigger_volume_type_oriented)
	{
		point->x = 0.5f * (volume->bounds.x0 + volume->bounds.x1);
		point->y = 0.5f * (volume->bounds.y0 + volume->bounds.y1);
		point->z = volume->bounds.z0 + 0.3f;
		return;
	}
	matrix4x3_from_point_and_vectors(&matrix, &volume->position, &volume->forward, &volume->up);
	local.x = 0.5f * volume->extents.i;
	local.y = 0.5f * volume->extents.j;
	local.z = 0.3f;
	matrix4x3_transform_point(&matrix, &local, point);
}

void ae_map_volumes_player_update(long player_index)
{
	struct scenario *scenario = global_scenario_get();
	struct player_datum *player;
	struct object_datum *object;
	long now = game_time_get(), slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), index;

	if (!scenario || !game_engine_running() || slot < 0 || slot >= AE_MAP_VOLUMES_PLAYERS)
		return;
	map_volumes_read(scenario, now);
	if (map_volumes.kill_count == 0 && map_volumes.lift_count == 0)
		return;
	player = player_get(player_index);
	if (player->unit_index == NONE)
		return;
	object = object_get(player->unit_index);
	/* (in a seat: the vehicle's own fate) */
	if (object->object.parent_object_index != NONE)
		return;
	/* (the kill: the host's; once, its death follows) */
	if (!network_game_distributed_client() && now - map_volumes.last_kill[slot] >= TICKS_PER_SECOND * 3)
	{
		for (index = 0; index < map_volumes.kill_count; index++)
		{
			if (scenario_trigger_volume_test_object(map_volumes.kills[index], player->unit_index))
			{
				map_volumes.last_kill[slot] = now;
				platform_log("map volumes: player %ld fell into '%s'", slot, volume_get(scenario, map_volumes.kills[index])->name);
				unit_kill(player->unit_index);
				return;
			}
		}
	}
	/* (the lift: the host's too; a client sees the unit where the host puts it, and a stock host moves no one) */
	if (network_game_distributed_client())
		return;
	if (now - map_volumes.last_lift[slot] < AE_MAP_VOLUME_LIFT_GAP_TICKS && now >= map_volumes.last_lift[slot])
		return;
	for (index = 0; index < map_volumes.lift_count; index++)
	{
		if (scenario_trigger_volume_test_object(map_volumes.lift_from[index], player->unit_index))
		{
			real_point3d exit;

			lift_exit_point(volume_get(scenario, map_volumes.lift_to[index]), &exit);
			map_volumes.last_lift[slot] = now;
			object_set_position(player->unit_index, &exit, NULL, NULL);
			platform_log("map volumes: player %ld lifted from '%s' to (%.1f %.1f %.1f)", slot,
				volume_get(scenario, map_volumes.lift_from[index])->name, exit.x, exit.y, exit.z);
			return;
		}
	}
}
