/*
AE_JUGGERNAUT.C

JUGGERNAUT (ae_juggernaut.h; the rules: ae_juggernaut_rules.c): the
oddball engine's kill handler and a tick's check of who is the juggernaut,
under the gametype's AE rule. The host decides; a network client of the
distributed netcode shows the host's carriers, scores and passes
(game_engine_oddball.c's network state), as for stock Juggernaut.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "real_math.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/damage.h"
#include "units/units.h"

#include "ae_juggernaut.h"

/* network_game_globals.c's */
boolean network_game_distributed_client(void);

/* game_engine_oddball.c's enum oddball_message: "%s is it" to the others (the
new juggernaut's own "you are it" is his state message each tick) */
enum
{
	AE_ODDBALL_MESSAGE_ALLY_IS_IT = 34,
	AE_ODDBALL_MESSAGE_ENEMY_IS_IT = 35,
};

/* game_engine_oddball.c's enum oddball_ball_type: the terminator ball (Juggernaut) */
#define AE_ODDBALL_TERMINATOR 2

static struct
{
	long *ball_owners;
	unsigned char *transfers;
	unsigned char *transfer_player;
	void (*add_score)(long player_index);
	/* each ball's juggernaut's shield a tick ago and his wait after a hit (ae_juggernaut_shield_tick) */
	float shield_before[MAXIMUM_ODDBALLS];
	int shield_wait[MAXIMUM_ODDBALLS];
} ae_juggernaut;

void ae_juggernaut_attach(long *ball_owners, unsigned char *transfers, unsigned char *transfer_player,
	void (*add_score)(long player_index))
{
	ae_juggernaut.ball_owners = ball_owners;
	ae_juggernaut.transfers = transfers;
	ae_juggernaut.transfer_player = transfer_player;
	ae_juggernaut.add_score = add_score;
	csmemset(ae_juggernaut.shield_before, 0, sizeof(ae_juggernaut.shield_before));
	csmemset(ae_juggernaut.shield_wait, 0, sizeof(ae_juggernaut.shield_wait));

	return;
}

boolean ae_juggernaut_active(void)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;

	return variant && ae_juggernaut.ball_owners &&
		variant->game_engine_index == game_engine_oddball &&
		variant->game_engine_variant.oddball.oddball_ball_type == AE_ODDBALL_TERMINATOR &&
		ae_juggernaut_h2((unsigned char)AE_JUGGERNAUT_VARIANT_BYTE(variant));
}

char const *ae_juggernaut_log_name(void)
{
	return ae_juggernaut_rules_name(ae_juggernaut_active());
}

static long ae_juggernaut_ball_count(void)
{
	long count = game_engine_get_variant()->game_engine_variant.oddball.ball_spawn_count;

	return count < 0 ? 0 : count > MAXIMUM_ODDBALLS ? MAXIMUM_ODDBALLS : count;
}

/* the ball the player carries, or NONE */
static long ae_juggernaut_ball_of(long player_index)
{
	long ball_index;

	if (player_index == NONE)
		return NONE;
	for (ball_index = 0; ball_index < ae_juggernaut_ball_count(); ball_index++)
	{
		if (ae_juggernaut.ball_owners[ball_index] == player_index)
			return ball_index;
	}
	return NONE;
}

static boolean ae_juggernaut_alive(struct player_datum const *player)
{
	return player->unit_index != NONE &&
		!TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit);
}

/* a player's name in ASCII, for the log */
static char const *ae_juggernaut_name(long player_index, char *buffer, int size)
{
	struct player_datum *player = player_try_and_get(player_index);
	int index;

	buffer[0] = 0;
	if (!player)
		return buffer;
	for (index = 0; index < size - 1 && index < (int)NUMBEROF(player->name) && player->name[index]; index++)
		buffer[index] = player->name[index] >= 32 && player->name[index] < 127 ? (char)player->name[index] : '?';
	buffer[index] = 0;
	return buffer;
}

/* the host: CE's over-charge to 3 on the juggernaut's body (his shield state goes to every machine with the
damage's network state) */
static void ae_juggernaut_overcharge(long unit_index)
{
	struct object_datum *object = object_get(unit_index);

	if (object->object.shield_vitality <= 1.0f)
		object_double_charge_shield(unit_index);
	else
		SET_FLAG(object->object.damage_flags, _object_shield_over_charging_bit, TRUE);

	return;
}

/* the host, each tick: the juggernaut's overshield (ae_juggernaut_shield_tick) */
static void ae_juggernaut_shield(long ball_index, struct player_datum *player)
{
	struct object_datum *object;
	struct ae_juggernaut_shield decision;

	if (!ae_juggernaut_alive(player))
		return;
	object = object_get(player->unit_index);
	if (object->object.maximum_shield_vitality <= 0.0f)
		return;
	decision = ae_juggernaut_shield_tick(object->object.shield_vitality, ae_juggernaut.shield_before[ball_index],
		TEST_FLAG(object->object.damage_flags, _object_shield_over_charging_bit),
		ae_juggernaut.shield_wait[ball_index], object->object.shield_stun_ticks);
	ae_juggernaut.shield_wait[ball_index] = decision.wait;
	if (decision.action == AE_JUGGERNAUT_SHIELD_GIVE_BACK)
		object->object.shield_vitality += decision.shield_give;
	else if (decision.action == AE_JUGGERNAUT_SHIELD_OVERCHARGE)
		ae_juggernaut_overcharge(player->unit_index);
	ae_juggernaut.shield_before[ball_index] = object->object.shield_vitality;

	return;
}

/* the role to player_index (the host's): every machine's "%s is it" */
static void ae_juggernaut_give(long ball_index, long player_index, char const *why)
{
	char name[16];

	ae_juggernaut.ball_owners[ball_index] = player_index;
	if (network_game_distributed_client())
		return;
	/* (the new juggernaut's overshield, at once) */
	{
		struct player_datum *player = player_try_and_get(player_index);

		ae_juggernaut.shield_wait[ball_index] = 0;
		if (player && ae_juggernaut_alive(player))
		{
			ae_juggernaut_overcharge(player->unit_index);
			ae_juggernaut.shield_before[ball_index] = object_get(player->unit_index)->object.shield_vitality;
		}
	}
	game_show_score_you_ally_enemy(player_index, NONE, AE_ODDBALL_MESSAGE_ALLY_IS_IT, AE_ODDBALL_MESSAGE_ENEMY_IS_IT,
		player_index);
	(*ae_juggernaut.transfers)++;
	*ae_juggernaut.transfer_player = (unsigned char)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	error(_error_silent, "juggernaut: player %ld (%s) is the juggernaut (%s)",
		(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), ae_juggernaut_name(player_index, name, sizeof(name)), why);

	return;
}

/* the host: a random player who is alive, not quit, not the juggernaut already and not excluded; NONE if none */
static long ae_juggernaut_random_player(long excluded_player_index)
{
	long candidates[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	int count = 0;
	int pick;
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL &&
		count < (int)NUMBEROF(candidates))
	{
		if (ae_juggernaut_eligible(TRUE, ae_juggernaut_alive(player), player->quit_out_of_game,
			ae_juggernaut_ball_of(iterator.datum_index) != NONE, iterator.datum_index == excluded_player_index))
		{
			candidates[count++] = iterator.datum_index;
		}
	}
	/* (the host's own random number: the local seed, not the game's) */
	pick = ae_juggernaut_pick(count, (unsigned)seed_random(get_global_local_random_seed_address()));
	return pick < 0 ? NONE : candidates[pick];
}

boolean ae_juggernaut_player_killed(long killing_player_index, long dead_player_index, boolean friendly_fire)
{
	struct ae_juggernaut_kill decision;
	struct player_datum *killer;
	long victim_ball;
	long killer_ball;
	char name[16];

	if (!ae_juggernaut_active() || dead_player_index == NONE)
		return FALSE;

	killer = killing_player_index != NONE ? player_try_and_get(killing_player_index) : NULL;
	victim_ball = ae_juggernaut_ball_of(dead_player_index);
	killer_ball = killer ? ae_juggernaut_ball_of(killing_player_index) : NONE;
	decision = ae_juggernaut_kill_decide(killer != NULL, killing_player_index == dead_player_index, friendly_fire,
		killer_ball != NONE, victim_ball != NONE, killer && ae_juggernaut_alive(killer));

	/* (the statistics as stock's) */
	if (killer && !friendly_fire && killing_player_index != dead_player_index)
	{
		if (victim_ball != NONE)
			killer->statistics.multiplayer_statistics.oddball_statistics.ball_carrier_kills++;
		else if (killer_ball != NONE)
			killer->statistics.multiplayer_statistics.oddball_statistics.kills_as_ball_carrier++;
	}

	/* (a client has the host's scores, and its end of the game) */
	if (decision.killer_scores && game_engine_can_score() && !network_game_distributed_client())
	{
		ae_juggernaut.add_score(killing_player_index);
		error(_error_silent, "juggernaut: the juggernaut, player %ld (%s), scores a kill (his kills as the juggernaut: %d)",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(killing_player_index),
			ae_juggernaut_name(killing_player_index, name, sizeof(name)),
			(int)killer->statistics.multiplayer_statistics.oddball_statistics.kills_as_ball_carrier);
	}
	else if (killer && victim_ball == NONE && killer_ball == NONE && killing_player_index != dead_player_index &&
		!network_game_distributed_client())
	{
		char victim_name[16];

		error(_error_silent, "juggernaut: player %ld (%s) kills player %ld (%s): neither is the juggernaut, no score",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(killing_player_index),
			ae_juggernaut_name(killing_player_index, name, sizeof(name)),
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index),
			ae_juggernaut_name(dead_player_index, victim_name, sizeof(victim_name)));
	}

	if (victim_ball != NONE)
	{
		ae_juggernaut.ball_owners[victim_ball] = NONE;
		if (decision.succession == AE_JUGGERNAUT_TO_KILLER)
		{
			ae_juggernaut_give(victim_ball, killing_player_index, "killed the juggernaut");
		}
		else if (decision.succession == AE_JUGGERNAUT_TO_RANDOM && !network_game_distributed_client() &&
			game_engine_can_score())
		{
			long player_index = ae_juggernaut_random_player(dead_player_index);

			if (player_index != NONE)
			{
				ae_juggernaut_give(victim_ball, player_index, killing_player_index == dead_player_index ?
					"at random: the juggernaut killed himself" : "at random: the juggernaut died to no player");
			}
		}
	}

	return TRUE;
}

void ae_juggernaut_update(void)
{
	long ball_index;

	/* (a client shows the host's) */
	if (!ae_juggernaut_active() || network_game_distributed_client() || !game_engine_can_score())
		return;

	for (ball_index = 0; ball_index < ae_juggernaut_ball_count(); ball_index++)
	{
		long owner = ae_juggernaut.ball_owners[ball_index];

		if (owner != NONE)
		{
			struct player_datum *player = player_try_and_get(owner);

			/* (the juggernaut left the game) */
			if (player && !player->quit_out_of_game)
			{
				ae_juggernaut_shield(ball_index, player);
				continue;
			}
			ae_juggernaut.ball_owners[ball_index] = NONE;
		}
		owner = ae_juggernaut_random_player(NONE);
		if (owner != NONE)
			ae_juggernaut_give(ball_index, owner, "at random: there was no juggernaut");
	}

	return;
}

/* (a DualSense's lightbar flash on becoming the juggernaut: ae_ds5_game.c) */
boolean ae_juggernaut_player_is(long player_index)
{
	return ae_juggernaut_active() && ae_juggernaut_ball_of(player_index) != NONE;
}
