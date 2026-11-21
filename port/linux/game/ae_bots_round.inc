/*
AE_BOTS_ROUND.INC

The bots in a round mode (ELIMINATION: ae_round.h), kept simple, all the host's (included by ae_bots_lan.inc, so it has
bots.c's statics):
- a downed teammate within reach of its travel (25 units): it goes to the spot and holds the action button there until the
  revive is done, over the squad's orders and roles and over a fight it merely sees; hurt in the last 1.5 s or with an enemy
  within 5 wu it fights first; only the nearest free bot of the team goes (ae_bots_round_revive_call);
- outside the danger zone, or outside the next ring with its closing near: it goes toward the zone's middle;
- in the frozen countdown it stands (bots_bot_walk's own test of the countdown).
ae_bots_objective_goal and ae_bots_objective_move ask here first.
*/

#include "ae_round.h"
#include "ae_round_rules.h"

/* (the last tick each bot saw an enemy, for "safe": no enemy seen in the last 3 seconds) */
/* standing this near the spot (across), it holds */
#define AE_BOTS_ROUND_HOLD_DISTANCE 0.7f

/* the distance to the nearest living player not of the team (an enemy standing), -1 when none */
static real ae_bots_round_nearest_enemy(short team_index, real_point3d const *position)
{
	struct data_iterator iterator;
	struct player_datum *other;
	real enemy = -1.0f;

	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d at;
		real d;
		struct object_datum const *unit;

		if (other->team_index == team_index || other->unit_index == NONE ||
			!(unit = object_try_and_get_and_verify_type(other->unit_index, _object_mask_unit)) ||
			TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			continue;
		}
		object_get_origin(other->unit_index, &at);
		d = bots_distance3d(position, &at);
		if (enemy < 0.0f || d < enemy)
			enemy = d;
	}
	return enemy;
}

/* the bot's call on a downed teammate's orb this tick (ae_round_rules.h: ae_round_bot_revive_decide): no look at what it sees
(PLAYFIGHT bots never kill each other and see each other for ever); hurt in the last 1.5 s or an enemy within 5 wu: the fight;
only the nearest free bot of the team goes. Kept for a tick; the start, the abort and the why-not are logged (ev=revive) */
static int ae_bots_round_revive_call(struct bots_bot *bot, real_point3d const *position, real_point3d *out_spot, boolean *out_human)
{
	static struct { long now; int reason; real_point3d spot; boolean human; real distance; } cache[BOTS_BRAINS];
	static int last[BOTS_BRAINS];
	long bot_index = (long)(bot - bots_globals.bots), now = game_time_get();
	struct player_datum *me;
	real_point3d spot;
	boolean human = FALSE;
	real distance = -1.0f, enemy = -1.0f;
	int reason, nearer = 0;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return AE_ROUND_REVIVE_NO_ORB;
	if (cache[bot_index].now == now + 1)
	{
		*out_spot = cache[bot_index].spot;
		if (out_human)
			*out_human = cache[bot_index].human;
		return cache[bot_index].reason;
	}
	me = ae_round_on() ? player_try_and_get(bot->player_index) : NULL;
	if (me && bot->unit_index != NONE && ae_round_bot_revive_spot_for(bot->player_index, position, &spot, &human))
	{
		long index;

		distance = bots_distance3d(position, &spot);
		/* (the nearest enemy standing) */
		enemy = ae_bots_round_nearest_enemy(me->team_index, position);
		/* (the free bots of its team nearer to the orb) */
		for (index = 0; index < BOTS_BRAINS; index++)
		{
			struct bots_bot const *mate = &bots_globals.bots[index];
			struct player_datum *mate_player;
			struct object_datum const *mate_unit;
			real_point3d at;
			real d;

			if (index == bot_index || mate->player_index == NONE || mate->unit_index == NONE ||
				!(mate_player = player_try_and_get(mate->player_index)) || mate_player->team_index != me->team_index ||
				!(mate_unit = object_try_and_get_and_verify_type(mate->unit_index, _object_mask_unit)) ||
				TEST_FLAG(mate_unit->object.damage_flags, _object_dead_bit) || mate_unit->object.parent_object_index != NONE ||
				(mate->hurt_tick != 0 && now - mate->hurt_tick < AE_ROUND_REVIVE_FIRE_TICKS))
			{
				continue;
			}
			object_get_origin(mate->unit_index, &at);
			d = bots_distance3d(&at, &spot);
			/* (only a mate that would itself go, to this same orb, counts: one with an enemy near, or on its way to
			another orb, would leave this one unrevived) */
			{
				real_point3d mate_spot;
				int same_orb = ae_round_bot_revive_spot_for(mate->player_index, &at, &mate_spot, NULL) &&
					bots_distance3d(&mate_spot, &spot) < 0.5f;

				if (!ae_round_revive_mate_competes(same_orb, ae_bots_round_nearest_enemy(mate_player->team_index, &at)))
					continue;
			}
			if (d <= AE_ROUND_BOT_REVIVE_RANGE &&
				ae_round_revive_mate_is_nearer(d, distance, index < bot_index, last[bot_index] - 1 == AE_ROUND_REVIVE_GO))
				nearer++;
		}
	}
	reason = ae_round_bot_revive_decide(distance, bot->hurt_tick == 0 ? -1 : now - bot->hurt_tick, enemy, nearer,
		last[bot_index] - 1 == AE_ROUND_REVIVE_GO);
	cache[bot_index].now = now + 1;
	cache[bot_index].reason = reason;
	cache[bot_index].spot = spot;
	cache[bot_index].human = human;
	cache[bot_index].distance = distance;
	if (reason != last[bot_index] - 1 && ae_bots_events_on())
	{
		int before = last[bot_index] - 1;

		if (reason == AE_ROUND_REVIVE_GO)
			platform_log("bots: ev=revive bot=%ld what=start dist=%.1f enemy=%.1f", bot_index + 1, distance, enemy);
		else if (before == AE_ROUND_REVIVE_GO)
			platform_log("bots: ev=revive bot=%ld what=abort reason=%s dist=%.1f enemy=%.1f", bot_index + 1,
				ae_round_revive_reason_name(reason), distance, enemy);
		else if (reason != AE_ROUND_REVIVE_NO_ORB)
			platform_log("bots: ev=revive bot=%ld what=why_not reason=%s dist=%.1f enemy=%.1f nearer=%d", bot_index + 1,
				ae_round_revive_reason_name(reason), distance, enemy, nearer);
	}
	last[bot_index] = reason + 1;
	*out_spot = spot;
	if (out_human)
		*out_human = human;
	return reason;
}

/* TRUE: this bot goes to a downed teammate's orb (breaks off a fight, outranks the squad's orders and roles) */
static boolean ae_bots_round_revive_due(struct bots_bot *bot, real_point3d const *position)
{
	real_point3d spot;

	return ae_round_on() && ae_bots_round_revive_call(bot, position, &spot, NULL) == AE_ROUND_REVIVE_GO;
}

/* a goal point on the walkable ground below (or at) the point: FALSE when there is none */
static boolean ae_bots_round_set_goal(struct bots_bot *bot, real_point3d const *point)
{
	real_point3d above = *point, ground;
	long surface_index;

	above.z += 1.0f;
	surface_index = bots_surface_below(&above, &ground);
	if (surface_index == NONE)
		return FALSE;
	bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
	return TRUE;
}

/* the zone's call, ahead of every other goal (fireteam orders, marks, the objective, a hunt): outside the ring, or
outside the one it closes to with the closing near, a bot goes toward the middle. Not in a vehicle (its driver's own) */
static boolean ae_bots_round_zone_first(struct bots_bot *bot, long player_index, real_point3d const *position)
{
	real_point3d goal, spot;

	/* (a downed teammate's orb: above the squad's orders and roles, below the zone) */
	if (ae_round_on() && !ae_round_bot_zone_goal(player_index, position, &goal) &&
		ae_bots_round_revive_call(bot, position, &spot, NULL) == AE_ROUND_REVIVE_GO && ae_bots_round_set_goal(bot, &spot))
	{
		return TRUE;
	}
	if (!ae_round_on() || !ae_round_bot_zone_goal(player_index, position, &goal))
		return FALSE;
	if (goal.z < position->z)
		goal.z = position->z;
	goal.z += 2.0f;
	{
		static long last_log[BOTS_BRAINS];
		long bot_index = (long)(bot - bots_globals.bots), now = game_time_get();

		if (bot_index >= 0 && bot_index < BOTS_BRAINS && now - last_log[bot_index] > 10 * TICKS_PER_SECOND)
		{
			last_log[bot_index] = now;
			platform_log("bots: ev=zone_call bot=%ld at %.1f %.1f goal %.1f %.1f", bot_index, position->x, position->y, goal.x,
				goal.y);
		}
	}
	return ae_bots_round_set_goal(bot, &goal);
}

/* (the squads' ground: the call as it stood this tick, from the bot's own position) */
static boolean ae_bots_round_revive_pending(struct bots_bot *bot)
{
	struct object_datum const *unit;
	real_point3d at;

	if (!ae_round_on() || bot->unit_index == NONE || !(unit = object_try_and_get_and_verify_type(bot->unit_index, _object_mask_unit)) ||
		unit->object.parent_object_index != NONE)
	{
		return FALSE;
	}
	object_get_origin(bot->unit_index, &at);
	return ae_bots_round_revive_due(bot, &at);
}

/* TRUE: this bot breaks off the fight it sees: a downed teammate's orb is its call (not hurt, no enemy near, the nearest) */
static boolean ae_bots_round_revive_leash(struct bots_bot *bot, struct bots_tick const *tick)
{
	return ae_round_on() && tick->object->object.parent_object_index == NONE && ae_bots_round_revive_due(bot, &tick->position);
}

/* TRUE: this bot is outside the ring (or near a closing edge): no fight this tick, the zone goal walks it back in. The
fight chases the enemy where the enemy goes; the ring does not wait for it */
static boolean ae_bots_round_zone_leash(struct bots_bot *bot, struct bots_tick const *tick)
{
	if (!ae_round_on() || tick->object->object.parent_object_index != NONE)
		return FALSE;
	return ae_round_bot_zone_leash(bot->player_index, &tick->position);
}

static boolean ae_bots_round_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting)
{
	real_point3d goal;
	if (!ae_round_on() || fighting)
		return FALSE;
	{
		if (ae_bots_round_revive_call(bot, position, &goal, NULL) == AE_ROUND_REVIVE_GO && ae_bots_round_set_goal(bot, &goal))
			return TRUE;
	}
	if (ae_round_bot_zone_goal(player_index, position, &goal))
	{
		/* (the goal's height: the bot's own, or the zone's middle when higher; the ground found below it) */
		if (goal.z < position->z)
			goal.z = position->z;
		goal.z += 2.0f;
		if (ae_bots_round_set_goal(bot, &goal))
			return TRUE;
	}
	/* (EXTERMINATION: one enemy left standing, the team pushes to it) */
	if (ae_round_bot_hunt_goal(player_index, position, &goal) && ae_bots_round_set_goal(bot, &goal))
		return TRUE;
	return FALSE;
}

/* TRUE when it holds at a downed teammate's spot this tick (its move and buttons set) */
static boolean ae_bots_round_move(struct bots_bot *bot, struct bots_tick *tick)
{
	real_point3d spot;
	real across;

	if (!ae_round_on() || tick->player->unit_index == NONE ||
		ae_bots_round_revive_call(bot, &tick->position, &spot, NULL) != AE_ROUND_REVIVE_GO)
	{
		return FALSE;
	}
	across = bots_distance2d(&spot, &tick->position);
	if (across > 2.0f || fabs(spot.z - tick->position.z) > 1.5f)
		return FALSE;
	if (across > AE_BOTS_ROUND_HOLD_DISTANCE)
	{
		/* (the last steps straight on: a path ends a unit short) */
		tick->move_direction = (real)atan2(spot.y - tick->position.y, spot.x - tick->position.x);
		tick->moving = TRUE;
		tick->throttle_scale = across < 1.2f ? 0.5f : 1.0f;
		return TRUE;
	}
	tick->moving = FALSE;
	tick->flags &= ~FLAG(_unit_control_jump_bit);
	tick->flags |= FLAG(_unit_control_swap_weapons_bit);
	return TRUE;
}
