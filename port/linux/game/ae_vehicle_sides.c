/*
AE_VEHICLE_SIDES.C

Whose vehicle a map's vehicle placement is in a team game (ae_vehicle_sides.h; the rule ae_vehicle_sides_rules.h):
the team of the nearest of the map's CTF starting locations.
*/

#include "cseries/cseries.h"
#include "scenario/scenario.h"
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
