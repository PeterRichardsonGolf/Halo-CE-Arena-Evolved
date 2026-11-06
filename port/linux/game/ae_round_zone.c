/*
AE_ROUND_ZONE.C

The round modes' DANGER ZONE on the host (ae_round_zone_rules.h has the rules): each round's centre and start
radius, its phases from the round's LIVE start, and its damage once a second to every living player outside. The
damage goes straight to the shield and the health (the host's, which every client reads from the units' states), and
while a player stays outside neither comes back past what it left; a death from it is the game's act-of-god death (no
killer), and leaves no revive spot.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "game/players_runtime.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"

#include "ae_dev_events.h"
#include "ae_layout.h"
#include "ae_layout_rules.h"
#include "ae_round_internal.h"
#include "ae_variant_ext.h"

#include <math.h>
#include <string.h>

enum
{
	/* the most spawns and hills a centre's choice reads */
	ZONE_POINTS = 256,
	ZONE_HILLS = 32,
};

/* a team's start spawns' centre (CTF's starting locations of the team): FALSE without one */
static boolean team_centre(long team_index, real_point3d *centre)
{
	short count = player_get_starting_location_count();
	short index;
	long found = 0;

	centre->x = centre->y = centre->z = 0.0f;
	for (index = 0; index < count; index++)
	{
		struct player_starting_location const *location = player_get_starting_location(index);

		if (location->team_index != team_index || !match_game_type(game_engine_ctf, 4, location->game_types))
			continue;
		centre->x += location->position.x;
		centre->y += location->position.y;
		centre->z += location->position.z;
		found++;
	}
	if (!found)
		return FALSE;
	centre->x /= (real)found;
	centre->y /= (real)found;
	centre->z /= (real)found;
	return TRUE;
}

/* the spawns this game uses (Slayer's, and the teams' starts), across: their count */
static int spawn_points(real (*points)[2], int maximum, real_point3d *middle)
{
	short count = player_get_starting_location_count();
	short index;
	int found = 0;

	middle->x = middle->y = middle->z = 0.0f;
	for (index = 0; index < count && found < maximum; index++)
	{
		struct player_starting_location const *location = player_get_starting_location(index);

		if (!match_game_type(game_engine_slayer, 4, location->game_types) &&
			!((location->team_index == _team_red || location->team_index == _team_blue) &&
				match_game_type(game_engine_ctf, 4, location->game_types)))
		{
			continue;
		}
		points[found][0] = location->position.x;
		points[found][1] = location->position.y;
		middle->x += location->position.x;
		middle->y += location->position.y;
		middle->z += location->position.z;
		found++;
	}
	if (found)
	{
		middle->x /= (real)found;
		middle->y /= (real)found;
		middle->z /= (real)found;
	}
	return found;
}

void ae_round_zone_new_round(void)
{
	static real points[ZONE_POINTS][2];
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct ae_layout const *layout = ae_layout_zones();
	real_point3d red, blue, middle;
	real radius = 0.0f;
	char const *source;
	int count;

	ae_round_host.zone_ready = FALSE;
	ae_round_host.zone_phase = AE_ZONE_PHASE_WAIT;
	ae_round_host.zone_start = NONE;
	if (!ae_round_host.zone)
		return;
	count = spawn_points(points, ZONE_POINTS, &middle);
	if (layout)
	{
		struct ae_layout_zone const *zone = &layout->zones[ae_round_random(layout->zone_count)];

		ae_round_host.zone_centre.x = zone->x;
		ae_round_host.zone_centre.y = zone->y;
		ae_round_host.zone_centre.z = zone->z;
		radius = zone->radius;
		source = "the map layout's zone line";
	}
	else if (ae_round_host.teams && team_centre(_team_red, &red) && team_centre(_team_blue, &blue))
	{
		struct scenario *scenario = global_scenario_get();
		long fair[ZONE_HILLS];
		long fair_count = 0, index;

		for (index = 0; scenario && index < scenario->netgame_flags.count && fair_count < ZONE_HILLS; index++)
		{
			struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index,
				struct scenario_netgame_flag);

			if (flag->type == _netgame_flag_hill &&
				ae_zone_fair_hill(flag->position.x, flag->position.y, red.x, red.y, blue.x, blue.y))
			{
				fair[fair_count++] = index;
			}
		}
		if (fair_count)
		{
			struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags,
				fair[ae_round_random(fair_count)], struct scenario_netgame_flag);

			ae_round_host.zone_centre = flag->position;
			source = "a hill marker between the teams' starts";
		}
		else
		{
			ae_round_host.zone_centre.x = (red.x + blue.x) * 0.5f;
			ae_round_host.zone_centre.y = (red.y + blue.y) * 0.5f;
			ae_round_host.zone_centre.z = (red.z + blue.z) * 0.5f;
			source = "the middle of the teams' starts";
		}
	}
	else
	{
		ae_round_host.zone_centre = middle;
		source = "the middle of the spawns";
	}
	if (!(radius > 0.0f))
		radius = ae_zone_start_radius(ae_round_host.zone_centre.x, ae_round_host.zone_centre.y,
			(real const (*)[2])points, count);
	ae_zone_plan_make(&ae_round_host.zone_plan, radius, ae_round_zone_wait_seconds(ext),
		ae_round_zone_shrink_seconds(ext));
	ae_round_host.zone_ready = TRUE;
	/* (the start radius holds every start spot a round can use (slayer's, the teams' CTF starts: the others, race's, are
	never picked, and a dev goto start can still put a player on one); one of these outside is logged) */
	{
		short total = player_get_starting_location_count(), spot;
		int outside = 0;

		for (spot = 0; spot < total; spot++)
		{
			struct player_starting_location const *location = player_get_starting_location(spot);
			real dx = location->position.x - ae_round_host.zone_centre.x;
			real dy = location->position.y - ae_round_host.zone_centre.y;
			real distance = (real)sqrt(dx * dx + dy * dy);

			if (distance > ae_round_host.zone_plan.start_radius && outside++ < 6 &&
				(match_game_type(game_engine_slayer, 4, location->game_types) ||
					((location->team_index == _team_red || location->team_index == _team_blue) &&
						match_game_type(game_engine_ctf, 4, location->game_types))))
				ae_round_log("start spot %d (%.1f %.1f, team %d, types %d %d %d %d) is %.1f from the zone's centre, outside "
					"its start radius %.1f", (int)spot, location->position.x, location->position.y, (int)location->team_index,
					(int)location->game_types[0], (int)location->game_types[1], (int)location->game_types[2],
					(int)location->game_types[3], distance, ae_round_host.zone_plan.start_radius);
		}
	}
	ae_round_log("round %d's zone: centre %.1f %.1f %.1f (%s), start radius %.1f, closings of %d s and %d s",
		ae_round_host.round, ae_round_host.zone_centre.x, ae_round_host.zone_centre.y, ae_round_host.zone_centre.z,
		source, ae_round_host.zone_plan.start_radius, ae_round_host.zone_plan.closing_seconds,
		ae_round_host.zone_plan.closing_2_seconds);
}

static boolean zone_running(long now)
{
	return ae_round_host.zone && ae_round_host.zone_ready && !ae_round_host.zone_disabled &&
		ae_round_host.phase == AE_ROUND_PHASE_LIVE && ae_round_host.zone_start != NONE && now >= ae_round_host.zone_start;
}

real ae_round_zone_radius(long now)
{
	if (!ae_round_host.zone || !ae_round_host.zone_ready)
		return 0.0f;
	if (!zone_running(now))
		return ae_round_host.zone_plan.start_radius;
	return ae_zone_radius_at(&ae_round_host.zone_plan, now - ae_round_host.zone_start);
}

boolean ae_round_zone_inside(real_point3d const *point, long now)
{
	if (!zone_running(now))
		return TRUE;
	return ae_zone_inside(ae_round_host.zone_centre.x, ae_round_host.zone_centre.y, ae_round_zone_radius(now),
		point->x, point->y);
}

void ae_round_zone_start_now(long now, boolean small)
{
	if (!ae_round_host.zone_ready)
		return;
	/* (the first closing begins now; or it is over, the small ring's wait begins) */
	ae_round_host.zone_start = now - (long)ae_round_host.zone_plan.wait_seconds * TICKS_PER_SECOND;
	if (small)
		ae_round_host.zone_start -= (long)ae_round_host.zone_plan.closing_seconds * TICKS_PER_SECOND;
	ae_round_log("dev: the zone %s now", small ? "stands at its small ring" : "closes");
}

/* one second outside, to a living player's unit */
static void damage(long player_index, struct ae_round_slot *slot, long unit_index)
{
	struct object_datum *object = object_get(unit_index);
	real shield = object->object.shield_vitality, body = object->object.body_vitality;

	if (ae_zone_damage(object->object.maximum_shield_vitality, object->object.maximum_body_vitality, &shield, &body))
	{
		slot->zone_death = TRUE;
		object->object.shield_vitality = 0.0f;
		ae_round_log("player %ld dies in the zone", DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
		unit_kill(unit_index);
		return;
	}
	object->object.shield_vitality = shield;
	object->object.body_vitality = body;
	/* (no recharge while outside) */
	if (object->object.shield_stun_ticks < 2 * TICKS_PER_SECOND)
		object->object.shield_stun_ticks = 2 * TICKS_PER_SECOND;
	slot->zone_capped = TRUE;
	slot->zone_shield = shield;
	slot->zone_body = body;
}

void ae_round_zone_tick(long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long elapsed;
	int phase;
	real radius;
	boolean second;

	if (!zone_running(now))
		return;
	elapsed = now - ae_round_host.zone_start;
	phase = ae_zone_phase_at(&ae_round_host.zone_plan, elapsed);
	radius = ae_zone_radius_at(&ae_round_host.zone_plan, elapsed);
	if (phase != ae_round_host.zone_phase)
	{
		ae_round_host.zone_phase = (short)phase;
		ae_round_log("round %d's zone: phase %d (radius %.1f, %ld s left in it)", ae_round_host.round, phase, radius,
			ae_zone_phase_ticks_left(&ae_round_host.zone_plan, elapsed) / TICKS_PER_SECOND);
		if (ae_dev_events_on())
			ae_dev_event("zone", "\"phase\":%d,\"radius\":%.2f", phase, radius);
	}
	second = elapsed > 0 && elapsed % TICKS_PER_SECOND == 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_round_slot *slot = ae_round_slot_of(iterator.datum_index);
		long unit_index = ae_round_living_unit(iterator.datum_index);
		struct object_datum *object;
		real_point3d at;

		if (!slot || unit_index == NONE || player->quit_out_of_game)
			continue;
		object_get_origin(unit_index, &at);
		if (ae_zone_inside(ae_round_host.zone_centre.x, ae_round_host.zone_centre.y, radius, at.x, at.y))
		{
			slot->zone_capped = FALSE;
			continue;
		}
		object = object_get(unit_index);
		if (slot->zone_capped)
		{
			if (object->object.shield_vitality > slot->zone_shield)
				object->object.shield_vitality = slot->zone_shield;
			if (object->object.body_vitality > slot->zone_body)
				object->object.body_vitality = slot->zone_body;
		}
		if (second)
			damage(iterator.datum_index, slot, unit_index);
	}
}

/* whether the zone calls a bot at this point in: outside the ring (its radius times margin), or, with the closing under way
or within 10 s, outside the ring it closes to */
static boolean bot_zone_calls(real_point3d const *position, real margin, long *out_elapsed, real *out_radius, real *out_next)
{
	long now = game_time_get(), elapsed;
	real radius, next;

	if (!zone_running(now))
		return FALSE;
	elapsed = now - ae_round_host.zone_start;
	radius = ae_zone_radius_at(&ae_round_host.zone_plan, elapsed);
	next = ae_zone_next_radius(&ae_round_host.zone_plan, elapsed);
	*out_elapsed = elapsed;
	*out_radius = radius;
	*out_next = next;
	if (ae_zone_inside(ae_round_host.zone_centre.x, ae_round_host.zone_centre.y, radius * margin, position->x, position->y))
	{
		int phase = ae_zone_phase_at(&ae_round_host.zone_plan, elapsed);
		boolean near = phase == AE_ZONE_PHASE_CLOSING || phase == AE_ZONE_PHASE_CLOSING_2 ||
			ae_zone_phase_ticks_left(&ae_round_host.zone_plan, elapsed) < 10 * TICKS_PER_SECOND;

		if (!near || ae_zone_inside(ae_round_host.zone_centre.x, ae_round_host.zone_centre.y, next, position->x,
			position->y))
		{
			return FALSE;
		}
	}
	return TRUE;
}

boolean ae_round_bot_zone_leash(long player_index, real_point3d const *position)
{
	long now = game_time_get(), elapsed;
	int phase;
	real edge, radius;

	if (!ae_round_on() || network_game_distributed_client() || player_index == NONE || !zone_running(now))
		return FALSE;
	elapsed = now - ae_round_host.zone_start;
	phase = ae_zone_phase_at(&ae_round_host.zone_plan, elapsed);
	radius = ae_zone_radius_at(&ae_round_host.zone_plan, elapsed);
	edge = ae_zone_edge_distance(ae_round_host.zone_centre.x, ae_round_host.zone_centre.y, radius, position->x, position->y);
	/* (outside the ring: back in. While it closes, near its edge too: a ring that moves on catches a bot in a fight) */
	return (boolean)ae_zone_leashes(phase, edge, radius);
}

boolean ae_round_bot_zone_goal(long player_index, real_point3d const *position, real_point3d *goal)
{
	long elapsed;
	real radius, next, target;

	if (!ae_round_on() || network_game_distributed_client() || player_index == NONE)
		return FALSE;
	/* (outside now; or outside the next ring with its closing near: 10 s before it, or under way) */
	if (!bot_zone_calls(position, 0.9f, &elapsed, &radius, &next))
		return FALSE;
	target = (next > 0.5f ? next : radius) * 0.5f;
	ae_zone_point_toward(ae_round_host.zone_centre.x, ae_round_host.zone_centre.y, position->x, position->y, target,
		&goal->x, &goal->y);
	goal->z = ae_round_host.zone_centre.z;
	return TRUE;
}
