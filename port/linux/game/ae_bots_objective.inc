/*
AE_BOTS_OBJECTIVE.INC

Included by ae_bots_lan.inc, so at the end of bots.c (ae_bots_objective.h):
the bots' objective play with bots.c's own brain, its static functions and
state unchanged. Two AE hook lines call in:

- bots_choose_goal, first: ae_bots_objective_goal. The bot weighs fighting
  against its mode's goals (the pure rules); when a goal wins, it is where
  the bot walks (bots_set_goal), and bots.c's own choice (the enemy last seen,
  a weapon, roaming) is skipped. A bot without the objective in its hands
  still fetches a better weapon close by, as bots.c's choice does.
- bots_bot_update, after its weapons: ae_bots_objective_move. A carrier (the
  flag home, the ball away from enemies), a bot going for a flag or ball it
  chose over the fight, for the hill or the race's next flag walks its goal's
  path while it fights: it still aims, shoots and strikes what it sees
  (bots_bot_fight), but does not chase or keep its weapon's distance. A bot
  with the flag or the ball in its hands never swaps or reloads it away.

Everything a bot decides comes from the tick's game state and its own random
numbers (bots_random), as bots.c's rule is.
*/

#include "ae_bots_objective.h"

/* the behaviour chosen keeps a bonus a while (no flip-flop between two close utilities): two seconds, three at
HEROIC and LEGENDARY (Halo Infinite's bot tuning has the same timeouts by level, a smaller bonus at the top) */
#define AE_BOTS_OBJECTIVE_HYSTERESIS_TICKS(skill) (((skill) == _bots_skill_heroic || (skill) == _bots_skill_legendary || (skill) == AE_BOTS_SKILL_DEMON ? 3 : 2) * TICKS_PER_SECOND)
#define AE_BOTS_OBJECTIVE_BONUS(skill) ((skill) == _bots_skill_heroic || (skill) == _bots_skill_legendary || (skill) == AE_BOTS_SKILL_DEMON ? 0.10f : 0.12f)
/* a guard stops at each spot it patrols to a moment (0.2 to 1 second, as Infinite's guards), looking about */
#define AE_BOTS_OBJECTIVE_PAUSE_TICKS(bot) (TICKS_PER_SECOND / 5 + (long)(bots_random(bot) % (TICKS_PER_SECOND * 4 / 5 + 1)))
/* the enemies a holder keeps away from: those its motion tracker shows (25 world units) */
#define AE_BOTS_OBJECTIVE_TRACKER_RANGE 25.0f
/* a goal that moves (a carrier, the ball's holder) is set again when it has gone this far from the bot's goal */
#define AE_BOTS_OBJECTIVE_MOVED 3.0f
/* near enough to a place it holds (a stand, the hill's centre) to stand and look about */
#define AE_BOTS_OBJECTIVE_THERE 0.8f
/* how close a teammate it escorts may be before it stops following */
#define AE_BOTS_OBJECTIVE_ESCORT_CLOSE 3.0f
/* a weapon this near and better than its own is still fetched (bots.c's choice) by a bot without the objective */
#define AE_BOTS_OBJECTIVE_WEAPON_NEAR 10.0f

struct ae_bots_objective_bot
{
	long player_index;
	/* the tick last thought, and what */
	long think_tick;
	short behaviour;
	long chosen_tick;
	/* its goal's point and object (a ball, a player) for the behaviour; valid when found */
	boolean target_valid;
	real_point3d target;
	long target_object_index;
	long target_player_index;
	/* own flag carried (CTF: RETURN hunts its carrier, not runs to it) */
	boolean own_flag_carried;
	/* it holds the objective in its hands (the flag, the stock ball) */
	boolean in_hands;
	boolean in_hill;
	/* a spot about a place it holds (defending a stand, the hill), and until when */
	real_point3d patrol_point;
	long patrol_until_tick;
	short patrol_behaviour;
	/* standing at the spot until then */
	long pause_until_tick;
};

static struct ae_bots_objective_bot ae_bots_objective_bots[BOTS_BRAINS];

static real ae_bots_objective_health(struct object_datum *object)
{
	real shield = object->object.shield_vitality > 1.0f ? 1.0f : object->object.shield_vitality;
	real health = (object->object.body_vitality + shield) * 0.5f;

	return health < 0.0f ? 0.0f : health > 1.0f ? 1.0f : health;
}

/* a CTF team's defenders: its first bots (by their index), as many as the team's size calls for */
static boolean ae_bots_objective_defender(struct bots_bot *bot, struct player_datum *player)
{
	struct data_iterator iterator;
	struct player_datum *other;
	long team_size = 0, rank = 0, index;

	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (other->team_index == player->team_index)
			team_size++;
	}
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *mate = &bots_globals.bots[index];
		struct player_datum *mate_player;

		if (mate == bot)
			break;
		if (mate->player_index == NONE || (mate_player = player_try_and_get(mate->player_index)) == NULL)
			continue;
		if (mate_player->team_index == player->team_index)
			rank++;
	}
	return rank < ae_bots_ctf_defenders((int)team_size);
}

/* whether the bot's team has the enemy spotted (as Infinite's bots hunt a flag carrier only while it is): a bot of
the team saw it in the last 3 seconds, or a teammate is within 10 units of it (Infinite's shared awareness distance) */
#define AE_BOTS_OBJECTIVE_SPOTTED_TICKS (3 * TICKS_PER_SECOND)
#define AE_BOTS_OBJECTIVE_SHARED_DISTANCE 10.0f

static boolean ae_bots_objective_spotted(struct player_datum *player, long enemy_player_index, long now)
{
	struct player_datum *enemy = player_try_and_get(enemy_player_index);
	struct data_iterator iterator;
	struct player_datum *other;
	real_point3d enemy_position;
	long index;

	if (!enemy || enemy->unit_index == NONE)
		return FALSE;
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *mate = &bots_globals.bots[index];
		struct player_datum *mate_player;

		if (mate->player_index == NONE || (mate_player = player_try_and_get(mate->player_index)) == NULL ||
			mate_player->team_index != player->team_index)
		{
			continue;
		}
		if (mate->target_player_index == enemy_player_index && now - mate->target_seen_tick < AE_BOTS_OBJECTIVE_SPOTTED_TICKS)
			return TRUE;
	}
	/* (where they are in the world: a rider's object position is its place in its vehicle, ae_bots_vehicle.c) */
	enemy_position = ae_bots_vehicle_point(enemy->unit_index);
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d other_position;

		if (other->unit_index == NONE)
			continue;
		other_position = ae_bots_vehicle_point(other->unit_index);
		if (other->team_index == player->team_index &&
			bots_distance3d(&other_position, &enemy_position) < AE_BOTS_OBJECTIVE_SHARED_DISTANCE)
		{
			return TRUE;
		}
	}
	return FALSE;
}

static void ae_bots_objective_target(struct ae_bots_objective_bot *state, real_point3d const *point, long object_index,
	long player_index)
{
	state->target_valid = TRUE;
	state->target = *point;
	state->target_object_index = object_index;
	state->target_player_index = player_index;
}

/* the nearest player who holds the objective, an enemy's or an ally's (NONE for none) */
static long ae_bots_objective_nearest_holder(struct player_datum *player, struct ae_bots_objective_snapshot const *snapshot,
	real_point3d const *position, boolean enemy, real *distance)
{
	struct data_iterator iterator;
	struct player_datum *other;
	long best = NONE;

	*distance = -1.0f;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d other_position;
		real d;

		if (other == player || other->unit_index == NONE ||
			(enemy ? !bots_is_enemy(player, other) : (bots_is_enemy(player, other) || !snapshot->teams)) ||
			!ae_bots_objective_player_carries(snapshot, iterator.datum_index))
		{
			continue;
		}
		other_position = ae_bots_vehicle_point(other->unit_index);
		d = bots_distance3d(position, &other_position);
		if (best == NONE || d < *distance)
		{
			best = iterator.datum_index;
			*distance = d;
		}
	}
	return best;
}

/* the bot's behaviour this tick (thought once a tick), its goal's target found with it */
static short ae_bots_objective_think(struct bots_bot *bot, long bot_index, real_point3d const *position, boolean fighting)
{
	struct ae_bots_objective_bot *state = &ae_bots_objective_bots[bot_index];
	struct ae_bots_objective_snapshot const *snapshot;
	struct player_datum *player = player_try_and_get(bot->player_index);
	struct ae_bots_situation situation;
	float utility[AE_BOTS_BEHAVIOURS];
	long now = game_time_get();
	short behaviour;
	/* each behaviour's target, as found */
	real_point3d targets[AE_BOTS_BEHAVIOURS];
	long target_objects[AE_BOTS_BEHAVIOURS], target_players[AE_BOTS_BEHAVIOURS];
	short index;

	if (state->player_index != bot->player_index)
	{
		csmemset(state, 0, sizeof(*state));
		state->player_index = bot->player_index;
		state->think_tick = NONE;
		state->behaviour = AE_BOTS_FIGHT;
		state->chosen_tick = now;
	}
	if (state->think_tick == now)
		return state->behaviour;
	state->think_tick = now;
	state->target_valid = FALSE;
	state->in_hands = FALSE;
	state->in_hill = FALSE;
	snapshot = ae_bots_objective_snapshot();
	if (!snapshot || !player || player->unit_index == NONE || snapshot->mode == AE_BOTS_MODE_SLAYER)
	{
		state->behaviour = AE_BOTS_FIGHT;
		return state->behaviour;
	}
	for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
	{
		target_objects[index] = NONE;
		target_players[index] = NONE;
		targets[index] = *position;
	}

	csmemset(&situation, 0, sizeof(situation));
	situation.mode = snapshot->mode;
	situation.health = (float)ae_bots_objective_health(object_get(player->unit_index));
	situation.fighting = fighting;
	situation.enemy_distance = -1.0f;
	situation.enemy_flag = situation.own_flag_away = situation.own_stand = situation.ally_carrier = -1.0f;
	situation.ball = situation.enemy_carrier = situation.hill = situation.race_flag = -1.0f;
	if (fighting && bot->target_player_index != NONE)
	{
		struct player_datum *target = player_try_and_get(bot->target_player_index);

		if (target && target->unit_index != NONE)
		{
			real_point3d target_position = ae_bots_vehicle_point(target->unit_index);

			situation.enemy_distance = (float)bots_distance3d(position, &target_position);
		}
	}
	situation.carrying = ae_bots_objective_player_carries(snapshot, bot->player_index);

	switch (snapshot->mode)
	{
	case AE_BOTS_MODE_CTF:
	{
		long own = player->team_index, other = 1 - player->team_index;

		if (!VALID_INDEX(own, 2))
			break;
		situation.defender = ae_bots_objective_defender(bot, player);
		state->in_hands = situation.carrying;
		/* the enemy flag: free (at its stand, or dropped), or a teammate's to escort */
		if (snapshot->flags[other].exists && snapshot->flags[other].weapon_index != NONE)
		{
			struct ae_bots_objective_flag const *flag = &snapshot->flags[other];

			if (flag->carrier_player_index == NONE)
			{
				situation.enemy_flag = (float)bots_distance3d(position, &flag->position);
				targets[AE_BOTS_CTF_TAKE] = flag->position;
				target_objects[AE_BOTS_CTF_TAKE] = flag->weapon_index;
			}
			else if (flag->carrier_player_index != bot->player_index)
			{
				struct player_datum *carrier = player_try_and_get(flag->carrier_player_index);

				if (carrier && carrier->unit_index != NONE && !bots_is_enemy(player, carrier))
				{
					targets[AE_BOTS_ESCORT] = ae_bots_vehicle_point(carrier->unit_index);
					target_players[AE_BOTS_ESCORT] = flag->carrier_player_index;
					situation.ally_carrier = (float)bots_distance3d(position, &targets[AE_BOTS_ESCORT]);
				}
			}
		}
		/* the own flag: away from its stand (dropped, or an enemy's in its hands) */
		if (snapshot->flags[own].exists && snapshot->flags[own].weapon_index != NONE && !snapshot->flags[own].home)
		{
			struct ae_bots_objective_flag const *flag = &snapshot->flags[own];

			situation.own_flag_carried = flag->carrier_player_index != NONE;
			targets[AE_BOTS_CTF_RETURN] = flag->position;
			target_objects[AE_BOTS_CTF_RETURN] = flag->weapon_index;
			target_players[AE_BOTS_CTF_RETURN] = flag->carrier_player_index;
			/* (a carrier no one of its team has spotted: to where it will capture, to cut it off) */
			if (situation.own_flag_carried && !ae_bots_objective_spotted(player, flag->carrier_player_index, now) &&
				snapshot->flags[other].exists)
			{
				targets[AE_BOTS_CTF_RETURN] = snapshot->flags[other].stand;
				target_players[AE_BOTS_CTF_RETURN] = NONE;
			}
			situation.own_flag_away = (float)bots_distance3d(position, &targets[AE_BOTS_CTF_RETURN]);
		}
		/* the own stand: where it captures, and defends */
		if (snapshot->flags[own].exists)
		{
			situation.own_stand = (float)bots_distance3d(position, &snapshot->flags[own].stand);
			targets[AE_BOTS_CTF_CARRY] = targets[AE_BOTS_CTF_DEFEND] = snapshot->flags[own].stand;
		}
		break;
	}
	case AE_BOTS_MODE_ODDBALL:
	case AE_BOTS_MODE_TAG:
	case AE_BOTS_MODE_JUGGERNAUT:
	{
		real distance;
		long holder;

		state->in_hands = situation.carrying && snapshot->mode == AE_BOTS_MODE_ODDBALL;
		for (index = 0; index < snapshot->ball_count; index++)
		{
			real d = bots_distance3d(position, &snapshot->ball_positions[index]);

			if (situation.ball < 0.0f || d < situation.ball)
			{
				situation.ball = (float)d;
				targets[AE_BOTS_BALL_TAKE] = snapshot->ball_positions[index];
				target_objects[AE_BOTS_BALL_TAKE] = snapshot->ball_indices[index];
			}
		}
		holder = ae_bots_objective_nearest_holder(player, snapshot, position, TRUE, &distance);
		if (holder != NONE)
		{
			situation.enemy_carrier = (float)distance;
			targets[AE_BOTS_BALL_CHASE] = targets[AE_BOTS_JUGG_HUNT] = ae_bots_vehicle_point(player_get(holder)->unit_index);
			target_players[AE_BOTS_BALL_CHASE] = target_players[AE_BOTS_JUGG_HUNT] = holder;
		}
		holder = ae_bots_objective_nearest_holder(player, snapshot, position, FALSE, &distance);
		if (holder != NONE)
		{
			situation.ally_carrier = (float)distance;
			targets[AE_BOTS_ESCORT] = ae_bots_vehicle_point(player_get(holder)->unit_index);
			target_players[AE_BOTS_ESCORT] = holder;
		}
		break;
	}
	case AE_BOTS_MODE_KING:
		if (snapshot->hill)
		{
			situation.hill = (float)bots_distance3d(position, &snapshot->hill_centre);
			situation.in_hill = ae_bots_objective_player_in_hill(bot->player_index);
			state->in_hill = situation.in_hill;
			targets[AE_BOTS_HILL_GO] = targets[AE_BOTS_HILL_HOLD] = snapshot->hill_centre;
		}
		break;
	case AE_BOTS_MODE_RACE:
		for (index = 0; index < snapshot->race_flag_count; index++)
		{
			real d;

			if (!ae_bots_objective_race_may_touch(bot->player_index, snapshot->race_flag_orders[index]))
				continue;
			d = bots_distance3d(position, &snapshot->race_flag_positions[index]);
			if (situation.race_flag < 0.0f || d < situation.race_flag)
			{
				situation.race_flag = (float)d;
				targets[AE_BOTS_RACE_NEXT] = snapshot->race_flag_positions[index];
			}
		}
		break;
	default:
		break;
	}

	ae_bots_objective_utilities(&situation, utility);
	behaviour = (short)ae_bots_objective_pick(utility, state->behaviour, now - state->chosen_tick,
		AE_BOTS_OBJECTIVE_HYSTERESIS_TICKS(bots_globals.skill), AE_BOTS_OBJECTIVE_BONUS(bots_globals.skill));
	if (behaviour != state->behaviour)
	{
		state->behaviour = behaviour;
		state->chosen_tick = now;
		state->patrol_until_tick = 0;
	}
	state->own_flag_carried = situation.own_flag_carried;
	if (behaviour != AE_BOTS_FIGHT)
		ae_bots_objective_target(state, &targets[behaviour], target_objects[behaviour], target_players[behaviour]);
	ae_bots_objective_note(bot_index, bot->player_index, behaviour);
	return behaviour;
}

/* a spot on the ground about a place (inner to outer away, at about its height); FALSE for none found */
static boolean ae_bots_objective_spot(struct bots_bot *bot, real_point3d const *centre, real inner, real outer,
	real_point3d *spot, long *surface_index)
{
	short tries;

	for (tries = 0; tries < 6; tries++)
	{
		real angle = bots_random_real(bot) * 2.0f * BOTS_PI;
		real radius = inner + bots_random_real(bot) * (outer - inner);
		real_point3d point = *centre;
		real_point3d ground;
		long surface;

		point.x += (real)cos(angle) * radius;
		point.y += (real)sin(angle) * radius;
		point.z += 0.5f;
		surface = bots_surface_below(&point, &ground);
		if (surface != NONE && fabs(ground.z - centre->z) < 1.2f)
		{
			*spot = ground;
			*surface_index = surface;
			return TRUE;
		}
	}
	return FALSE;
}

/* the roaming place to keep away at: far from the enemies the tracker shows, not far from the bot */
static boolean ae_bots_objective_away(struct bots_bot *bot, real_point3d const *position, real_point3d *spot)
{
	struct player_datum *player = player_get(bot->player_index);
	real_point3d enemies[BOTS_BRAINS + MAXIMUM_LOCAL_PLAYERS];
	long enemy_count = 0, index, best = NONE;
	real best_score = -REAL_MAX;
	struct data_iterator iterator;
	struct player_datum *other;

	if (bots_globals.roam_point_count <= 0)
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL && enemy_count < (long)NUMBEROF(enemies))
	{
		real_point3d other_position;

		if (!bots_is_enemy(player, other))
			continue;
		other_position = ae_bots_vehicle_point(other->unit_index);
		if (bots_distance3d(&other_position, position) < AE_BOTS_OBJECTIVE_TRACKER_RANGE)
			enemies[enemy_count++] = other_position;
	}
	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real_point3d const *point = &bots_globals.roam_points[index];
		real own = bots_distance2d(point, position);
		real nearest = AE_BOTS_OBJECTIVE_TRACKER_RANGE;
		real score;
		long enemy;

		if (own < 4.0f || own > 30.0f)
			continue;
		for (enemy = 0; enemy < enemy_count; enemy++)
		{
			real d = bots_distance2d(point, &enemies[enemy]);

			if (d < nearest)
				nearest = d;
		}
		/* (away from the enemies first; then, the nearer the better, with a little chance) */
		score = nearest * 2.0f - own * 0.5f + bots_random_real(bot) * 3.0f;
		if (score > best_score)
		{
			best_score = score;
			best = index;
		}
	}
	if (best == NONE)
		return FALSE;
	*spot = bots_globals.roam_points[best];
	return TRUE;
}

/* the bot's goal for its behaviour (bots_set_goal), FALSE for none now (it stands and looks about) */
static boolean ae_bots_objective_set_goal(struct bots_bot *bot, long bot_index, real_point3d const *position)
{
	struct ae_bots_objective_bot *state = &ae_bots_objective_bots[bot_index];
	long now = game_time_get();
	real_point3d point, ground;
	long surface_index;
	short kind = _bots_goal_roam;

	if (!state->target_valid)
		return FALSE;
	point = state->target;
	switch (state->behaviour)
	{
	case AE_BOTS_CTF_CARRY:
		/* (at its stand, the own flag gone: it waits there) */
		if (bots_distance2d(&point, position) < AE_BOTS_OBJECTIVE_THERE && fabs(point.z - position->z) < 1.5f)
		{
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		break;
	case AE_BOTS_CTF_DEFEND:
	case AE_BOTS_HILL_HOLD:
	{
		boolean defend = state->behaviour == AE_BOTS_CTF_DEFEND;

		/* (out of the hill: its centre) */
		if (!defend && !state->in_hill)
			break;
		/* (about the place: a spot, a moment there, then another, a few seconds each) */
		if (now < state->pause_until_tick)
		{
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		if (state->patrol_behaviour == state->behaviour && now < state->patrol_until_tick &&
			bots_distance2d(&state->patrol_point, position) < AE_BOTS_OBJECTIVE_THERE && state->pause_until_tick < 0)
		{
			state->pause_until_tick = now + AE_BOTS_OBJECTIVE_PAUSE_TICKS(bot);
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		if (state->patrol_behaviour != state->behaviour || now >= state->patrol_until_tick ||
			bots_distance2d(&state->patrol_point, position) < AE_BOTS_OBJECTIVE_THERE)
		{
			long spot_surface;
			real_point3d spot;

			state->pause_until_tick = -1;
			state->patrol_behaviour = state->behaviour;
			state->patrol_until_tick = now + TICKS_PER_SECOND * (3 + (long)(bots_random(bot) % 4));
			if (ae_bots_objective_spot(bot, &point, defend ? 1.5f : 0.0f, defend ? 6.0f : 1.2f, &spot, &spot_surface))
				state->patrol_point = spot;
			else
				state->patrol_point = point;
		}
		point = state->patrol_point;
		break;
	}
	case AE_BOTS_ESCORT:
		/* (close to the one it follows: it looks about) */
		if (bots_distance3d(&point, position) < AE_BOTS_OBJECTIVE_ESCORT_CLOSE)
		{
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		break;
	case AE_BOTS_BALL_HOLD:
	case AE_BOTS_JUGG_FLEE:
		if (state->patrol_behaviour != state->behaviour || now >= state->patrol_until_tick ||
			bots_distance2d(&state->patrol_point, position) < 1.5f)
		{
			real_point3d spot;

			state->patrol_behaviour = state->behaviour;
			state->patrol_until_tick = now + TICKS_PER_SECOND * 4;
			state->patrol_point = ae_bots_objective_away(bot, position, &spot) ? spot : *position;
		}
		point = state->patrol_point;
		break;
	case AE_BOTS_BALL_TAKE:
		/* (close, on its floor: on to it, the action button held as for a weapon: bots_bot_weapons; on a floor the
		walkable surfaces do not reach, Prisoner's top: a place to go to, which bots.c climbs to with jumps) */
		if (state->target_object_index != NONE && bots_distance2d(&point, position) < 2.0f &&
			fabs(point.z - position->z) < 1.0f)
		{
			kind = _bots_goal_item;
		}
		break;
	default:
		break;
	}
	/* (a goal that moves: kept until it has gone on a way, so that the path stands) */
	if (state->target_player_index != NONE && bot->goal_kind == _bots_goal_roam &&
		bots_distance3d(&bot->goal_point, &point) < AE_BOTS_OBJECTIVE_MOVED)
	{
		return TRUE;
	}
	surface_index = bots_surface_below(&point, &ground);
	if (surface_index == NONE)
		ground = point;
	bots_set_goal(bot, kind, &ground, surface_index);
	if (kind == _bots_goal_item)
		bot->item_index = state->target_object_index;
	return TRUE;
}

boolean ae_bots_objective_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_bots_objective_bot *state;
	struct player_datum *player = player_try_and_get(player_index);
	long now = game_time_get();
	short behaviour;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !player || player->unit_index == NONE)
		return FALSE;
	behaviour = ae_bots_objective_think(bot, bot_index, position, fighting);
	if (behaviour == AE_BOTS_FIGHT)
		return FALSE;
	state = &ae_bots_objective_bots[bot_index];
	/* a better weapon close by: bots.c's own choice fetches it (not with the objective in its hands, nor while it
	carries, nor at the ball) */
	if (!ae_bots_objective_player_carries(ae_bots_objective_snapshot(), player_index) && behaviour != AE_BOTS_BALL_TAKE)
	{
		/* (the goal is set here, not left to bots.c: its own choice chases an enemy seen in the last 8 seconds
		before it looks at weapons, which took the bot off its objective, and looked again every tick) */
		if (bot->goal_kind == _bots_goal_item && bot->item_index != state->target_object_index)
		{
			struct item_datum *item = (struct item_datum *)object_try_and_get_and_verify_type(bot->item_index,
				_object_mask_weapon);

			/* (on its way to one, as bots.c's: given up after 15 seconds, and left be a while) */
			if (item && item->object.parent_object_index == NONE && now - bot->goal_tick < TICKS_PER_SECOND * 15)
				return TRUE;
			if (item && item->object.parent_object_index == NONE)
				bots_item_set_unreachable(bot, bot->item_index);
			bot->goal_kind = _bots_goal_none;
			bot->item_check_tick = now + TICKS_PER_SECOND * 2;
		}
		else if (now >= bot->item_check_tick)
		{
			long item_index = bots_find_item(bot, unit_get(player->unit_index), position, AE_BOTS_OBJECTIVE_WEAPON_NEAR);

			bot->item_check_tick = now + TICKS_PER_SECOND;
			if (item_index != NONE)
			{
				real_point3d ground;
				long surface_index = bots_surface_below(&object_get(item_index)->object.position, &ground);

				if (surface_index != NONE)
				{
					bots_set_goal(bot, _bots_goal_item, &ground, surface_index);
					bot->item_index = item_index;
					return TRUE;
				}
			}
		}
	}
	if (!ae_bots_objective_set_goal(bot, bot_index, position) && bot->goal_kind != _bots_goal_none)
		bot->goal_kind = _bots_goal_none;
	return TRUE;
}

void ae_bots_objective_move(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_bots_objective_bot *state;
	short behaviour;
	real direction;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->unit)
		return;
	behaviour = ae_bots_objective_think(bot, bot_index, &tick->position, tick->fighting);
	state = &ae_bots_objective_bots[bot_index];
	/* (the flag or the ball in its hands: it neither swaps nor reloads it away, nor pulls a trigger, which drops a
	weapon that must be readied, as a player's does: players.c; it still strikes with it) */
	if (state->in_hands)
	{
		tick->action->desired_weapon_index = NONE;
		tick->flags &= ~(FLAG(_unit_control_swap_weapons_bit) | FLAG(_unit_control_weapon_reload_bit) |
			FLAG(_unit_control_weapon_primary_trigger_bit) | FLAG(_unit_control_weapon_secondary_trigger_bit));
	}
	if (!tick->fighting || behaviour == AE_BOTS_FIGHT)
		return;
	/* (in the hill, it fights as bots.c does; out of it, the hill's centre again) */
	if (behaviour == AE_BOTS_HILL_HOLD && state->in_hill)
		return;
	if (!ae_bots_objective_runs_while_fighting(behaviour) &&
		!(behaviour == AE_BOTS_CTF_RETURN && !state->own_flag_carried))
	{
		return;
	}
	if (!ae_bots_objective_set_goal(bot, bot_index, &tick->position))
		return;
	if (bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
	{
		tick->move_direction = direction;
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
	}
}
