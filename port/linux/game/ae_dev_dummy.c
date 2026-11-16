/*
AE_DEV_DUMMY.C

DEV MODE's target dummies (ae_dev_dummy.h; the rules: ae_dev_dummy_rules.c). Each dummy is a flag on a bot's number
(machine - BOTS_FIRST_MACHINE) with the spot it was put at: its bot comes in as bots.count rises (ae_bots_live.c), and each
tick the dummy's player is put at the spot when its unit is new, healed once it has not been hurt for a while, and brought back
soon after a death.
*/

#include <math.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "networking/network_game_globals.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "units/units.h"

#include "bots.h"
#include "ae_dev_dummy.h"
#include "ae_dev_dummy_rules.h"
#include "ae_dev_reach.h"

void platform_log(char const *format, ...);

struct dummy
{
	boolean flagged;
	/* its bot was seen in the game once */
	boolean seen;
	long join_tick;
	struct ae_dummy_spot spot;
	/* the unit it was last put at its spot with, none for the next one */
	long placed_unit;
	/* the vehicle its unit sits in (the seat command), NONE on foot: a dummy in a seat stays there, not put at its spot */
	long seated_in;
	/* the unit whose missing room was last said (once, not every tick) */
	long no_room_unit;
	/* the tick it was last hurt, and its vitality then */
	long hurt_tick;
	float last_vitality;
	/* the tick its death was first seen, -1 while it lives */
	long dead_tick;
	/* a walking dummy: its goal (when has_goal), the tick it was last put at its start, and what its log said this life
	(AE_DUMMY_WALK_SAY_* bits) */
	boolean walks;
	boolean has_goal;
	float goal[3];
	long placed_tick;
	int said;
};

static struct dummy dummies[BOTS_MAXIMUM];

static boolean dummy_player(struct player_datum const *player, long *bot_index)
{
	if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
		player->quit_out_of_game_time != NONE || player->quit_out_of_game)
		return FALSE;
	*bot_index = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
	return *bot_index >= 0 && *bot_index < BOTS_MAXIMUM;
}

/* the bots of the game (non-zero each), and how many */
static int bots_present(unsigned char *present)
{
	struct data_iterator iterator;
	struct player_datum *player;
	int count = 0;

	memset(present, 0, BOTS_MAXIMUM);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long bot;

		if (dummy_player(player, &bot) && !present[bot])
		{
			present[bot] = 1;
			count++;
		}
	}
	return count;
}

int ae_dev_dummy_count(void)
{
	int index, count = 0;

	for (index = 0; index < BOTS_MAXIMUM; index++)
		count += dummies[index].flagged;
	return count;
}

int ae_dev_dummy_add(long player_index, int count, boolean here, float distance, long *bots_wanted, char *error,
	int error_size)
{
	unsigned char present[BOTS_MAXIMUM];
	int picked[AE_DUMMY_MAXIMUM], found, index, in_game, claimed;
	long wanted = bots_wanted_count();
	struct player_datum *player;
	real_point3d origin;
	real_vector3d aim;

	if (!network_game_is_splitscreen_local())
	{
		snprintf(error, (size_t)error_size, "dummies are bots: a local game only");
		return 0;
	}
	if (count < 1 || count > AE_DUMMY_MAXIMUM || ae_dev_dummy_count() + count > AE_DUMMY_MAXIMUM)
	{
		snprintf(error, (size_t)error_size, "at most %d dummies at a time", AE_DUMMY_MAXIMUM);
		return 0;
	}
	if (player_index == NONE || (player = player_get(player_index))->unit_index == NONE ||
		TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
	{
		snprintf(error, (size_t)error_size, "no living player to put the dummy by");
		return 0;
	}
	in_game = bots_present(present);
	claimed = (int)wanted > in_game ? (int)wanted - in_game : 0;
	found = ae_dummy_pick(present, BOTS_MAXIMUM, claimed, count, picked);
	if (found < count || (int)MAX(wanted, (long)in_game) + count > BOTS_MAXIMUM)
	{
		snprintf(error, (size_t)error_size, "no room for %d more bots in the game", count);
		return 0;
	}
	object_get_origin(player->unit_index, &origin);
	unit_get_aiming_vector(player->unit_index, &aim);
	for (index = 0; index < count; index++)
	{
		struct dummy *dummy = &dummies[picked[index]];

		memset(dummy, 0, sizeof *dummy);
		dummy->flagged = TRUE;
		dummy->join_tick = game_time_get();
		dummy->placed_unit = NONE;
		dummy->seated_in = NONE;
		dummy->no_room_unit = NONE;
		dummy->dead_tick = -1;
		dummy->spot = ae_dummy_spot(index, count, origin.x, origin.y, origin.z, aim.i, aim.j, distance, here);
	}
	*bots_wanted = MAX(wanted, (long)in_game) + count;
	return 1;
}

static void make_walker(struct dummy *dummy, real_point3d const *origin, boolean here, boolean has_goal, float const *goal)
{
	dummy->walks = TRUE;
	dummy->spot.x = origin->x;
	dummy->spot.y = origin->y;
	dummy->spot.z = here ? origin->z + 0.5f : origin->z;
	dummy->has_goal = has_goal;
	if (has_goal)
	{
		dummy->goal[0] = goal[0];
		dummy->goal[1] = goal[1];
		dummy->goal[2] = goal[2];
		dummy->spot.yaw = ae_dummy_yaw_toward(dummy->spot.x, dummy->spot.y, goal[0], goal[1]);
	}
}

int ae_dev_dummy_add_walker(long player_index, boolean here, float const *at, boolean has_goal, float const *goal,
	long *bots_wanted, char *error, int error_size)
{
	int picked = -1, index;
	unsigned char before[BOTS_MAXIMUM];
	struct dummy *dummy;
	real_point3d origin;

	if (!here)
	{
		origin.x = at[0];
		origin.y = at[1];
		origin.z = at[2];
	}
	else if (player_index != NONE && player_get(player_index)->unit_index != NONE)
		object_get_origin(player_get(player_index)->unit_index, &origin);
	for (index = 0; index < BOTS_MAXIMUM; index++)
		before[index] = (unsigned char)dummies[index].flagged;
	/* (the room, the count and a living player: as a standing dummy's, one bot) */
	if (!ae_dev_dummy_add(player_index, 1, TRUE, AE_DUMMY_DISTANCE_DEFAULT, bots_wanted, error, error_size))
		return 0;
	/* (the one the add just flagged) */
	for (index = 0; index < BOTS_MAXIMUM && picked < 0; index++)
	{
		if (dummies[index].flagged && !before[index])
			picked = index;
	}
	if (picked < 0)
	{
		snprintf(error, (size_t)error_size, "the walking dummy was not made");
		return 0;
	}
	dummy = &dummies[picked];
	make_walker(dummy, &origin, here, has_goal, goal);
	return 1;
}

int ae_dev_dummy_add_walkers(long player_index, int count, float const (*starts)[3], float const (*goals)[3],
	long *bot_indices, long *bots_wanted, char *error, int error_size)
{
	int made = 0, index;
	unsigned char before[BOTS_MAXIMUM];

	for (index = 0; index < BOTS_MAXIMUM; index++)
		before[index] = (unsigned char)dummies[index].flagged;
	/* (all in one add: each add picks the first free bots, so adds one by one before bots.count rises pick the same) */
	if (!ae_dev_dummy_add(player_index, count, TRUE, AE_DUMMY_DISTANCE_DEFAULT, bots_wanted, error, error_size))
		return 0;
	for (index = 0; index < BOTS_MAXIMUM && made < count; index++)
	{
		real_point3d origin;

		if (!dummies[index].flagged || before[index])
			continue;
		origin.x = starts[made][0];
		origin.y = starts[made][1];
		origin.z = starts[made][2];
		make_walker(&dummies[index], &origin, FALSE, TRUE, goals[made]);
		bot_indices[made++] = index;
	}
	return made;
}

void ae_dev_dummy_walk_retarget(long bot_index, float const *start, float const *goal)
{
	struct dummy *dummy;

	if (!ae_dev_dummy_walks(bot_index))
		return;
	dummy = &dummies[bot_index];
	dummy->spot.x = start[0];
	dummy->spot.y = start[1];
	dummy->spot.z = start[2];
	dummy->has_goal = TRUE;
	dummy->goal[0] = goal[0];
	dummy->goal[1] = goal[1];
	dummy->goal[2] = goal[2];
	dummy->spot.yaw = ae_dummy_yaw_toward(dummy->spot.x, dummy->spot.y, goal[0], goal[1]);
	/* (put at the new start on the next tick, as a new unit is) */
	dummy->placed_unit = NONE;
}

boolean ae_dev_dummy_walks(long bot_index)
{
	return bot_index >= 0 && bot_index < BOTS_MAXIMUM && dummies[bot_index].flagged && dummies[bot_index].walks;
}

boolean ae_dev_dummy_walk_goal(long bot_index, float *x, float *y, float *z)
{
	struct dummy const *dummy;

	if (!ae_dev_dummy_walks(bot_index) || !dummies[bot_index].has_goal)
		return FALSE;
	dummy = &dummies[bot_index];
	*x = dummy->goal[0];
	*y = dummy->goal[1];
	*z = dummy->goal[2];
	return TRUE;
}

/* a walking dummy's lines for this tick: left its start, reached its goal, still at its start */
static void walk_news(struct dummy *dummy, long unit_index, long now)
{
	real_point3d at;
	int news;
	float seconds = (float)(now - dummy->placed_tick) / 30.0f;

	object_get_origin(unit_index, &at);
	news = ae_dummy_walk_news(at.x, at.y, dummy->spot.x, dummy->spot.y, dummy->has_goal, dummy->goal[0], dummy->goal[1],
		dummy->placed_tick, now, dummy->said);
	if (news & AE_DUMMY_WALK_SAY_LEFT)
		platform_log("dev walker: left its start (%.1f %.1f %.1f) after %.1f s", dummy->spot.x, dummy->spot.y,
			dummy->spot.z, (double)seconds);
	if (news & AE_DUMMY_WALK_SAY_STILL)
		platform_log("dev walker: still within %.0f units of its start (%.1f %.1f %.1f) after %.0f s, at (%.1f %.1f %.1f)",
			(double)AE_DUMMY_WALK_LEFT, dummy->spot.x, dummy->spot.y, dummy->spot.z, (double)seconds, at.x, at.y, at.z);
	if (news & AE_DUMMY_WALK_SAY_REACHED)
		platform_log("dev walker: reached its goal (%.1f %.1f %.1f) after %.1f s", dummy->goal[0], dummy->goal[1],
			dummy->goal[2], (double)seconds);
	dummy->said |= news;
}

int ae_dev_dummy_clear(long *bots_wanted)
{
	int index, count = ae_dev_dummy_count();

	ae_dev_reach_stop();

	*bots_wanted = bots_wanted_count() - count;
	if (*bots_wanted < 0)
		*bots_wanted = 0;
	/* (the flags stay on the bots in the game until they leave; the ones not in it yet are not coming) */
	{
		unsigned char present[BOTS_MAXIMUM];

		bots_present(present);
		for (index = 0; index < BOTS_MAXIMUM; index++)
		{
			if (dummies[index].flagged && !present[index])
				dummies[index].flagged = FALSE;
		}
	}
	return count;
}

void ae_dev_dummy_forget(void)
{
	ae_dev_reach_stop();
	memset(dummies, 0, sizeof dummies);
}

/* the dummy's player at its spot, facing as it was put */
static void place(struct dummy *dummy, long player_index, long unit_index)
{
	real_point3d at;
	real_vector3d forward;
	struct unit_datum *unit = unit_get(unit_index);

	at.x = dummy->spot.x;
	at.y = dummy->spot.y;
	at.z = dummy->spot.z;
	if (!player_teleport(player_index, NONE, &at))
	{
		if (dummy->no_room_unit != unit_index)
			platform_log("dev dummy: no room at %.1f %.1f %.1f", at.x, at.y, at.z);
		dummy->no_room_unit = unit_index;
		return;
	}
	if (dummy->walks)
	{
		if (dummy->has_goal)
			platform_log("dev walker: put at %.1f %.1f %.1f, walking to %.1f %.1f %.1f", at.x, at.y, at.z, dummy->goal[0],
				dummy->goal[1], dummy->goal[2]);
		else
			platform_log("dev walker: put at %.1f %.1f %.1f, roaming", at.x, at.y, at.z);
		dummy->placed_tick = game_time_get();
		dummy->said = 0;
	}
	else
		platform_log("dev dummy: put at %.1f %.1f %.1f facing %.0f degrees", at.x, at.y, at.z,
			(double)dummy->spot.yaw * 57.29578);
	forward.i = (real)cos(dummy->spot.yaw);
	forward.j = (real)sin(dummy->spot.yaw);
	forward.k = 0.0f;
	unit->unit.desired_facing_vector = forward;
	unit->unit.desired_aiming_vector = forward;
	unit->unit.desired_looking_vector = forward;
	dummy->placed_unit = unit_index;
}

/* the body back to full and the shield's wait cut short: the shield's own recharge brings it up from where it is (its
effect, parts and the recharge band all shown, as on a player; owner 11-09: the band never showed on a dummy whose shield
was set to full at once) */
static void heal(long unit_index)
{
	struct object_datum *object = object_get(unit_index);

	object_restore_body(unit_index);
	object->object.shield_stun_ticks = 0;
}

/* whether the dummy still wants healing: a hurt body, or a shield still waiting to recharge (one recharging is left be) */
static boolean wants_heal(struct object_datum const *object)
{
	return (boolean)(object->object.body_vitality < 0.999f ||
		(object->object.shield_vitality < 0.999f && object->object.shield_stun_ticks > 0));
}

void ae_dev_dummy_tick(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	unsigned char present[BOTS_MAXIMUM];
	long now = game_time_get();
	int index;

	if (!ae_dev_dummy_count())
		return;
	memset(present, 0, sizeof present);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long bot;
		struct dummy *dummy;
		struct object_datum *object;
		long unit_index;
		float vitality;

		if (!dummy_player(player, &bot) || !dummies[bot].flagged)
			continue;
		dummy = &dummies[bot];
		present[bot] = 1;
		dummy->seen = TRUE;
		unit_index = player->unit_index;
		object = unit_index != NONE ? object_try_and_get(unit_index) : NULL;
		if (!object || TEST_FLAG(object->object.damage_flags, _object_dead_bit))
		{
			if (dummy->dead_tick < 0)
				dummy->dead_tick = now;
			if (dummy->walks)
				ae_dev_reach_sample(bot, 0.0f, 0.0f, 0.0f, FALSE, dummy->placed_tick, now);
			/* (a player with no unit waits for the timer: it is cut short; 1 is ready) */
			if (unit_index == NONE)
				player->respawn_timer = ae_dummy_respawn_timer(dummy->dead_tick, now, player->respawn_timer);
			dummy->placed_unit = NONE;
			dummy->seated_in = NONE;
			continue;
		}
		dummy->dead_tick = -1;
		vitality = object->object.body_vitality + object->object.shield_vitality;
		if (object->object.parent_object_index != dummy->seated_in)
		{
			platform_log("dev dummy: %s", object->object.parent_object_index != NONE ? "in a seat" : "out of its seat");
			dummy->seated_in = object->object.parent_object_index;
		}
		/* (seated before it was put: it stays in the seat, and the spot is no longer its own) */
		if (dummy->seated_in != NONE && dummy->placed_unit != unit_index)
		{
			dummy->placed_unit = unit_index;
			dummy->hurt_tick = now - AE_DUMMY_HEAL_TICKS;
			dummy->last_vitality = vitality;
			continue;
		}
		if (dummy->placed_unit != unit_index)
		{
			place(dummy, iterator.datum_index, unit_index);
			dummy->hurt_tick = now - AE_DUMMY_HEAL_TICKS;
			dummy->last_vitality = vitality;
			continue;
		}
		if (dummy->walks)
		{
			real_point3d at;

			walk_news(dummy, unit_index, now);
			object_get_origin(unit_index, &at);
			ae_dev_reach_sample(bot, at.x, at.y, at.z, TRUE, dummy->placed_tick, now);
		}
		if (vitality < dummy->last_vitality - 0.001f)
			dummy->hurt_tick = now;
		dummy->last_vitality = vitality;
		if (wants_heal(object))
		{
			if (ae_dummy_heal_due(dummy->hurt_tick, now))
			{
				heal(unit_index);
				dummy->last_vitality = object->object.body_vitality + object->object.shield_vitality;
				platform_log("dev dummy: healed at tick %ld", now);
			}
		}
	}
	for (index = 0; index < BOTS_MAXIMUM; index++)
	{
		if (dummies[index].flagged && ae_dummy_given_up(present[index], dummies[index].seen, dummies[index].join_tick, now))
			dummies[index].flagged = FALSE;
	}
}

boolean ae_dev_dummy_player(long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	long bot;

	return player && dummy_player(player, &bot) && dummies[bot].flagged;
}

boolean ae_dev_dummy_flagged(long bot_index)
{
	return bot_index >= 0 && bot_index < BOTS_MAXIMUM && dummies[bot_index].flagged;
}

boolean ae_dev_dummy_placed(long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	long bot;

	if (!player || !dummy_player(player, &bot) || !dummies[bot].flagged)
		return TRUE;
	return player->unit_index != NONE && dummies[bot].placed_unit == player->unit_index;
}

boolean ae_dev_dummy_hold(long bot_index, struct player_action *action, unsigned long *flags)
{
	struct dummy const *dummy;

	if (bot_index < 0 || bot_index >= BOTS_MAXIMUM || !dummies[bot_index].flagged)
		return FALSE;
	dummy = &dummies[bot_index];
	if (dummy->walks)
	{
		/* (a blow is the equipment bit in this engine, bots.c:2702) */
		*flags &= ~(FLAG(_unit_control_weapon_primary_trigger_bit) | FLAG(_unit_control_weapon_secondary_trigger_bit) |
			FLAG(_unit_control_throw_grenade_bit) | FLAG(_unit_control_use_equipment_bit));
		return FALSE;
	}
	action->throttle.i = action->throttle.j = 0.0f;
	*flags = 0;
	action->desired_facing.yaw = dummy->spot.yaw;
	action->desired_facing.pitch = 0.0f;
	return TRUE;
}

int ae_dev_dummy_leaver(unsigned char const *in_game, int maximum, int fallback)
{
	unsigned char flag[BOTS_MAXIMUM];
	int index;

	for (index = 0; index < BOTS_MAXIMUM; index++)
		flag[index] = (unsigned char)dummies[index].flagged;
	return ae_dummy_leaver(flag, in_game, maximum < BOTS_MAXIMUM ? maximum : BOTS_MAXIMUM, fallback);
}
