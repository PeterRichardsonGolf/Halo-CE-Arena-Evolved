/*
AE_VEHICLE_SIDES.C

Whose vehicle a map's vehicle placement is in a team game (ae_vehicle_sides.h; the rule ae_vehicle_sides_rules.h):
the team of the nearest of the map's CTF starting locations.
*/

#include "cseries/cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"

#include "ae_vehicle_sides.h"
#include "ae_vehicle_sides_rules.h"

/* (the rule reads a starting location's game types as these) */
typedef char ae_vehicle_sides_game_types_check[
	NUMBEROF(((struct player_starting_location *)0)->game_types) == AE_VEHICLE_SIDES_GAME_TYPES ? 1 : -1];

short ae_vehicle_sides_team(real_point3d const *point, short side)
{
	struct scenario *scenario = global_scenario_get();
	struct ae_vehicle_sides_pick pick;
	long index;

	if (!scenario || !point)
		return side;
	ae_vehicle_sides_begin(&pick, side);
	for (index = 0; index < scenario->players.count; index++)
	{
		struct player_starting_location const *location =
			TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);
		struct ae_vehicle_sides_start start;
		int type;

		start.x = location->position.x;
		start.y = location->position.y;
		start.z = location->position.z;
		start.team = location->team_index;
		for (type = 0; type < AE_VEHICLE_SIDES_GAME_TYPES; type++)
			start.game_types[type] = location->game_types[type];
		ae_vehicle_sides_consider(&pick, &start, point->x, point->y, point->z);
	}
	return pick.team;
}

/* (the vehicle types' names, as enum _variant_vehicle_*) */
static char const *const ae_vehicle_sides_type_names[NUMBER_OF_VARIANT_VEHICLES] =
	{ "warthog", "ghost", "scorpion", "rocket warthog", "banshee", "gun turret" };

void ae_vehicle_sides_log_placed(void)
{
	struct game_variant_options const *options = game_variant_options_get();
	boolean teams = game_engine_has_teams();
	struct object_iterator iterator;
	short side, type;

	/* (a team's custom set: the counts asked for) */
	for (side = 0; teams && options && side < 2; side++)
	{
		char counts[160];

		if (game_engine_effective_team_vehicle_set(side) != VARIANT_VEHICLE_SET_CUSTOM)
			continue;
		counts[0] = 0;
		for (type = 0; type < NUMBER_OF_VARIANT_VEHICLES; type++)
		{
			csprintf(counts + csstrlen(counts), "%s%s %d", type ? ", " : "", ae_vehicle_sides_type_names[type],
				(int)options->vehicle_counts[side][type]);
		}
		error(_error_silent, "vehicles asked (%s custom): %s", side ? "blue" : "red", counts);
	}
	/* (each one placed: its tag, its side, upright or not as placed) */
	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum *object = object_get(iterator.index);
		char const *name = tag_get_name(object->definition_index);

		side = teams ? ae_vehicle_sides_team(&object->object.position, 0) : NONE;
		error(_error_silent, "vehicle placed: %s, %s, up %.2f%s at (%.1f %.1f %.1f)", name ? name : "?",
			side == NONE ? "no team" : side ? "blue side" : "red side", object->object.up.k,
			object->object.up.k < 0.5f ? " NOT UPRIGHT" : "", object->object.position.x, object->object.position.y,
			object->object.position.z);
	}
}
