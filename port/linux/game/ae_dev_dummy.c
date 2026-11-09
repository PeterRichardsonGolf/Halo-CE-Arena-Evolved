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
	/* the tick it was last hurt, and its vitality then */
	long hurt_tick;
	float last_vitality;
	/* the tick its death was first seen, -1 while it lives */
	long dead_tick;
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
		dummy->dead_tick = -1;
		dummy->spot = ae_dummy_spot(index, count, origin.x, origin.y, origin.z, aim.i, aim.j, distance, here);
	}
	*bots_wanted = MAX(wanted, (long)in_game) + count;
	return 1;
}

int ae_dev_dummy_clear(long *bots_wanted)
{
	int index, count = ae_dev_dummy_count();

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
		platform_log("dev dummy: no room at %.1f %.1f %.1f", at.x, at.y, at.z);
		return;
	}
	platform_log("dev dummy: put at %.1f %.1f %.1f facing %.0f degrees", at.x, at.y, at.z, (double)dummy->spot.yaw * 57.29578);
	forward.i = (real)cos(dummy->spot.yaw);
	forward.j = (real)sin(dummy->spot.yaw);
	forward.k = 0.0f;
	unit->unit.desired_facing_vector = forward;
	unit->unit.desired_aiming_vector = forward;
	unit->unit.desired_looking_vector = forward;
	dummy->placed_unit = unit_index;
}

/* the body and the shield back to full: the shield's own recharge does the rest in its next tick (a depleted shield's
effect and parts included) */
static void heal(long unit_index)
{
	struct object_datum *object = object_get(unit_index);

	object_restore_body(unit_index);
	object->object.shield_stun_ticks = 0;
	if (object->object.shield_vitality < 0.999f)
		object->object.shield_vitality = 0.999f;
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
			/* (a player with no unit waits for the timer: it is cut short; 1 is ready) */
			if (unit_index == NONE)
				player->respawn_timer = ae_dummy_respawn_timer(dummy->dead_tick, now, player->respawn_timer);
			dummy->placed_unit = NONE;
			continue;
		}
		dummy->dead_tick = -1;
		vitality = object->object.body_vitality + object->object.shield_vitality;
		if (dummy->placed_unit != unit_index)
		{
			place(dummy, iterator.datum_index, unit_index);
			dummy->hurt_tick = now - AE_DUMMY_HEAL_TICKS;
			dummy->last_vitality = vitality;
			continue;
		}
		if (vitality < dummy->last_vitality - 0.001f)
			dummy->hurt_tick = now;
		dummy->last_vitality = vitality;
		if (object->object.body_vitality < 0.999f || object->object.shield_vitality < 0.999f)
		{
			if (ae_dummy_heal_due(dummy->hurt_tick, now))
			{
				heal(unit_index);
				dummy->last_vitality = 2.0f;
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

boolean ae_dev_dummy_hold(long bot_index, struct player_action *action, unsigned long *flags)
{
	struct dummy const *dummy;

	if (bot_index < 0 || bot_index >= BOTS_MAXIMUM || !dummies[bot_index].flagged)
		return FALSE;
	dummy = &dummies[bot_index];
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
