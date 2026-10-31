/*
AE_VEHICLE_SIDES_RULES.H

Whose vehicle a map's vehicle placement is, for a gametype's vehicles of each team (RED VEHICLES / BLUE VEHICLES:
a set, the map's own, or CUSTOM counts; game_engine_vehicle_placement_allowed), pure
(port/linux/tests/ae_vehicle_sides_rules_test.c): ae_vehicle_sides.c reads it for the AE hook in game_engine.c.

The Xbox maps' placements have no team of their own (their multiplayer team bytes are 0), so the team is the one
whose starting locations are nearest. The port took the nearest starting location of ANY game type, but a map's
team is only real in its CTF starting locations: its race, slayer and king ones are team 0 (red) wherever they are,
and Sidewinder's race starts line blue's base. Every vehicle of Sidewinder was red's, and of Blood Gulch most of
blue's: a gametype with CUSTOM counts placed red's counts over the whole map and none of blue's (the owner's 0.3.7
play-test: one scorpion, nothing else).

The rule: the nearest of the starting locations a CTF game uses (game type CTF or ALL, team red or blue); a map
with none of those keeps the port's own answer (the nearest of any game type).
*/

#ifndef __AE_VEHICLE_SIDES_RULES_H
#define __AE_VEHICLE_SIDES_RULES_H

/* the starting locations' game types, numbered as game_engine.h's game engines (and game_engine.c's "all") */
enum
{
	AE_VEHICLE_SIDES_GAME_CTF = 1,
	AE_VEHICLE_SIDES_GAME_ALL = 12,
	AE_VEHICLE_SIDES_GAME_TYPES = 4
};

/* a map's starting location as the rule reads it */
struct ae_vehicle_sides_start
{
	float x, y, z;
	short team;
	short game_types[AE_VEHICLE_SIDES_GAME_TYPES];
};

/* the nearest team start so far: the team, else the fallback given (none considered yet) */
struct ae_vehicle_sides_pick
{
	float best;
	short team;
};

/* whether a starting location's team is a real one: red or blue, and used by a CTF game */
static inline int ae_vehicle_sides_start_has_team(struct ae_vehicle_sides_start const *start)
{
	int index;

	if (start->team != 0 && start->team != 1)
		return 0;
	for (index = 0; index < AE_VEHICLE_SIDES_GAME_TYPES; index++)
	{
		if (start->game_types[index] == AE_VEHICLE_SIDES_GAME_CTF || start->game_types[index] == AE_VEHICLE_SIDES_GAME_ALL)
			return 1;
	}
	return 0;
}

static inline void ae_vehicle_sides_begin(struct ae_vehicle_sides_pick *pick, short fallback)
{
	pick->best = -1.0f;
	pick->team = fallback;
}

/* one starting location against the placement at (x, y, z) */
static inline void ae_vehicle_sides_consider(struct ae_vehicle_sides_pick *pick, struct ae_vehicle_sides_start const *start,
	float x, float y, float z)
{
	float dx = start->x - x, dy = start->y - y, dz = start->z - z;
	float distance = dx * dx + dy * dy + dz * dz;

	if (ae_vehicle_sides_start_has_team(start) && (pick->best < 0.0f || distance < pick->best))
	{
		pick->best = distance;
		pick->team = start->team;
	}
}

#endif
