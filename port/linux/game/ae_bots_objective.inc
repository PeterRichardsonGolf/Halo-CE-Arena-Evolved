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
#include "ae_bomb_assault.h"

/* the behaviour chosen keeps a bonus a while (no flip-flop between two close utilities): two seconds, three at
HEROIC and LEGENDARY (the same timeouts by level, a smaller bonus at the top) */
#define AE_BOTS_OBJECTIVE_HYSTERESIS_TICKS(skill) (((skill) == _bots_skill_heroic || (skill) == _bots_skill_legendary || (skill) == AE_BOTS_SKILL_DEMON ? 3 : 2) * TICKS_PER_SECOND)
#define AE_BOTS_OBJECTIVE_BONUS(skill) ((skill) == _bots_skill_heroic || (skill) == _bots_skill_legendary || (skill) == AE_BOTS_SKILL_DEMON ? 0.10f : 0.12f)
/* a guard stops at each spot it patrols to a moment (0.2 to 1 second), looking about */
#define AE_BOTS_OBJECTIVE_PAUSE_TICKS(bot) (TICKS_PER_SECOND / 5 + (long)(bots_random(bot) % (TICKS_PER_SECOND * 4 / 5 + 1)))
/* the enemies a holder keeps away from: those its motion tracker shows (25 world units) */
#define AE_BOTS_OBJECTIVE_TRACKER_RANGE 25.0f
/* a goal that moves (a carrier, the ball's holder) is set again when it has gone this far from the bot's goal */
#define AE_BOTS_OBJECTIVE_MOVED 3.0f
/* near enough to a place it holds (a stand, the hill's centre) to stand and look about */
#define AE_BOTS_OBJECTIVE_THERE 0.8f
/* how close a teammate it escorts may be before it stops following */
#define AE_BOTS_OBJECTIVE_ESCORT_CLOSE 3.0f
/* Bomb: a teammate carrying the bomb this near the enemy goal is escorted from ahead (its escorts at the goal) */
#define AE_BOTS_OBJECTIVE_BOMB_AHEAD 15.0f
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
	/* Bomb's carrier standing in the enemy goal since then (the arming's bound), and stepping to its centre until then */
	boolean bomb_standing;
	long bomb_stand_tick;
	long bomb_replan_until_tick;
	/* a spot about a place it holds (defending a stand, the hill), and until when */
	real_point3d patrol_point;
	long patrol_until_tick;
	short patrol_behaviour;
	/* standing at the spot until then */
	long pause_until_tick;
	/* an Oddball or SEARCH AND DESTROY bomb deliberately left for a fight */
	long fight_object_index;
	long fight_clear_tick;
	long fight_drop_again_tick;
	long fight_missing_tick;
	long fight_team_index;
	boolean fight_drop_requested;
	boolean wingman;
	boolean snd_hold;
	/* SEARCH AND DESTROY: this round's role (AE_BOTS_SND_ROLE_*) */
	short snd_role;
	long snd_role_round;
	short snd_role_logged;
	short snd_role_noted;
	/* where a bot standing at its post would look (a defender, the enemy's way; the planted bomb) */
	boolean hold_yaw_valid;
	real hold_yaw;
	real_point3d snd_forward;
	/* the side it plays, the lurker waiting for the push, a lane it holds a waypoint of */
	boolean snd_attacking;
	boolean snd_wait;
	boolean lane_held;
	long lane_tick;
	/* the spots it has held in this life (each pick turns the choice: a holder moves between its spots, not stands on one) */
	long patrol_serial;
	short lane_behaviour;
	boolean lane_done;
	real_point3d lane_point;
};

/* the lurker waits on its lane while the carrier is this far from the goal, at most this many seconds */
#define AE_BOTS_OBJECTIVE_LURK_FAR 22.0f
#define AE_BOTS_OBJECTIVE_LURK_SECONDS 40
/* a lane's waypoint is given up this long after it was chosen */
#define AE_BOTS_OBJECTIVE_LANE_SECONDS 18

static struct ae_bots_objective_bot ae_bots_objective_bots[BOTS_BRAINS];
/* the tick a bot last walked its objective's path while it fought (ae_bots_travel_may_stick: a move it meant) */
static long ae_bots_objective_routed_tick[BOTS_BRAINS];

static struct
{
	boolean valid;
	long tick;
	long carrier_player_index;
	long bot_index;
} ae_bots_wingmen[2];

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
	long team_size = 0, humans = 0, rank = 0, index;

	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (other->team_index == player->team_index)
		{
			team_size++;
			humans += ae_bots_team_bot_of(iterator.datum_index) == NONE;
		}
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
	return rank < (humans > 0 ? ae_bots_ctf_defenders_with_human((int)team_size) : ae_bots_ctf_defenders((int)team_size));
}

/* SEARCH AND DESTROY's first living bot of a team. When its site holder or defuser dies, the next takes over; a
last survivor therefore keeps the site or takes the planted bomb instead of roaming. */
static boolean ae_bots_objective_first_living_bot(struct bots_bot *bot, struct player_datum *player)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *mate = &bots_globals.bots[index];
		struct player_datum *mate_player;
		struct object_datum *mate_unit;

		if (mate->player_index == NONE || (mate_player = player_try_and_get(mate->player_index)) == NULL ||
			mate_player->team_index != player->team_index || mate_player->unit_index == NONE)
		{
			continue;
		}
		mate_unit = object_get(mate_player->unit_index);
		if (!TEST_FLAG(mate_unit->object.damage_flags, _object_dead_bit))
			return mate == bot;
	}
	return FALSE;
}

/* whether SEARCH AND DESTROY's roles play (HALO_BOTS_SND_ROLES=0 turns them off, for the before and after of a test) */
static boolean ae_bots_objective_roles_on(void)
{
	char const *setting = getenv("HALO_BOTS_SND_ROLES");

	return !setting || setting[0] != '0';
}

/* SEARCH AND DESTROY's role of a bot for this round (ae_bots_snd_roles): its team's bots in slot order, the living
ones, the round. The roles are dealt as if all lived, so a death changes only the dead bot's and its promotion. */
static short ae_bots_objective_snd_role_of(struct bots_bot *bot, struct player_datum *player, boolean attacking,
	long *round)
{
	struct ae_bomb_state bomb;
	unsigned long alive = 0;
	int roles[8], size = 0, mine = -1;
	long index;

	for (index = 0; index < BOTS_BRAINS && size < 8; index++)
	{
		struct bots_bot *mate = &bots_globals.bots[index];
		struct player_datum *mate_player;

		if (mate->player_index == NONE || (mate_player = player_try_and_get(mate->player_index)) == NULL ||
			mate_player->team_index != player->team_index)
		{
			continue;
		}
		if (mate == bot)
			mine = size;
		if (mate_player->unit_index != NONE &&
			!TEST_FLAG(object_get(mate_player->unit_index)->object.damage_flags, _object_dead_bit))
		{
			alive |= 1UL << size;
		}
		size++;
	}
	ae_bomb_state_get(&bomb);
	*round = bomb.round;
	if (mine < 0)
		return AE_BOTS_SND_ROLE_NONE;
	if (!ae_bots_objective_roles_on())
	{
		/* (HALO_BOTS_SND_ROLES=0, to compare with the old play: no roles but the last living defender forward at
		HEROIC and up, as SEARCH AND DESTROY had it) */
		int living = 0, rank = 0, slot;

		for (slot = 0; slot < size; slot++)
		{
			if (slot == mine)
				rank = living;
			living += (int)((alive >> slot) & 1);
		}
		return attacking ? AE_BOTS_SND_ROLE_NONE : ae_bots_level_of_skill(bots_globals.skill) >= 3 && living >= 3 &&
			rank == living - 1 ? AE_BOTS_SND_ROLE_FORWARD : AE_BOTS_SND_ROLE_HOLDER;
	}
	ae_bots_snd_roles(attacking, size, alive, bomb.round, roles);
	return (short)roles[mine];
}

/* A forward defender's point: the navigation point nearest 55% of the way from its goal to the enemy's, within 8
units of that. FALSE for none (it holds the site). */
static boolean ae_bots_objective_forward_point(real_point3d const *own, real_point3d const *other, real_point3d *point)
{
	real_point3d ideal;
	long index, best = NONE;
	real best_distance = 8.0f;

	ideal.x = own->x + (other->x - own->x) * 0.55f;
	ideal.y = own->y + (other->y - own->y) * 0.55f;
	ideal.z = own->z;
	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real distance = bots_distance2d(&bots_globals.roam_points[index], &ideal);

		if (distance < best_distance && (real)fabs(bots_globals.roam_points[index].z - own->z) < 4.0f)
		{
			best_distance = distance;
			best = index;
		}
	}
	if (best == NONE)
		return FALSE;
	*point = bots_globals.roam_points[best];
	return TRUE;
}

/* The stable nearest free living bot beside a team's carrier. CTF's site defender is not free; neither is a bot
already carrying or saving an objective. */
static long ae_bots_objective_wingman(struct ae_bots_objective_snapshot const *snapshot, long carrier_player_index,
	long team_index)
{
	struct ae_bots_wingman_candidate candidates[BOTS_BRAINS];
	struct player_datum *carrier = player_try_and_get(carrier_player_index);
	real_point3d carrier_position;
	long index, current;

	if (!snapshot->teams || !VALID_INDEX(team_index, 2) || !carrier || carrier->unit_index == NONE)
		return NONE;
	if (snapshot->tick < ae_bots_wingmen[team_index].tick)
		ae_bots_wingmen[team_index].valid = FALSE;
	if (ae_bots_wingmen[team_index].valid && ae_bots_wingmen[team_index].tick == snapshot->tick &&
		ae_bots_wingmen[team_index].carrier_player_index == carrier_player_index)
	{
		return ae_bots_wingmen[team_index].bot_index;
	}
	carrier_position = ae_bots_vehicle_point(carrier->unit_index);
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *mate = &bots_globals.bots[index];
		struct player_datum *mate_player = mate->player_index != NONE ? player_try_and_get(mate->player_index) : NULL;
		struct object_datum *unit = mate_player && mate_player->unit_index != NONE ? object_get(mate_player->unit_index) : NULL;
		short doing = ae_bots_objective_bots[index].behaviour;

		candidates[index].eligible = mate_player && unit && mate_player->team_index == team_index &&
			mate->player_index != carrier_player_index && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit) &&
			doing != AE_BOTS_BOMB_SAVE && doing != AE_BOTS_CTF_RETURN && doing != AE_BOTS_CTF_CARRY &&
			doing != AE_BOTS_BOMB_CARRY && doing != AE_BOTS_BALL_HOLD &&
			!(snapshot->mode == AE_BOTS_MODE_CTF && ae_bots_objective_defender(mate, mate_player));
		candidates[index].distance = candidates[index].eligible ?
			(float)bots_distance3d(&carrier_position, &unit->object.position) : 0.0f;
	}
	current = ae_bots_wingmen[team_index].carrier_player_index == carrier_player_index ?
		ae_bots_wingmen[team_index].bot_index : NONE;
	ae_bots_wingmen[team_index].tick = snapshot->tick;
	ae_bots_wingmen[team_index].valid = TRUE;
	ae_bots_wingmen[team_index].carrier_player_index = carrier_player_index;
	ae_bots_wingmen[team_index].bot_index = ae_bots_wingman_pick(candidates, BOTS_BRAINS, (int)current, 2.0f);
	return ae_bots_wingmen[team_index].bot_index;
}

static real_point3d ae_bots_objective_wingman_point(long carrier_unit_index, long bot_index)
{
	struct object_datum *carrier = object_get(carrier_unit_index);
	real side = (bot_index & 1) ? 1.2f : -1.2f;
	real_point3d point = ae_bots_vehicle_point(carrier_unit_index);

	point.x -= carrier->object.forward.i * 2.5f;
	point.y -= carrier->object.forward.j * 2.5f;
	point.x -= carrier->object.forward.j * side;
	point.y += carrier->object.forward.i * side;
	return point;
}

static long ae_bots_objective_living_team(struct player_datum *player, real_point3d const *position, boolean teams,
	real *nearest)
{
	struct data_iterator iterator;
	struct player_datum *other;
	long count = 0;

	*nearest = -1.0f;
	if (!teams)
		return 1;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *object;
		real distance;

		if (other->team_index != player->team_index || other->unit_index == NONE)
			continue;
		object = object_get(other->unit_index);
		if (TEST_FLAG(object->object.damage_flags, _object_dead_bit))
			continue;
		count++;
		if (other == player)
			continue;
		distance = bots_distance3d(position, &object->object.position);
		if (*nearest < 0.0f || distance < *nearest)
			*nearest = distance;
	}
	return count;
}

/* A loaded ordinary weapon the carrier can draw after dropping the objective; NONE means it must keep carrying. */
static long ae_bots_objective_carrier_gun(struct unit_datum *unit, long objective_index)
{
	long best = NONE;
	short slot;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon_index = unit->unit.weapon_object_indices[slot];

		if (weapon_index == NONE || weapon_index == objective_index || bots_weapon_value(weapon_get(weapon_index)->definition_index) < 0 ||
			bots_weapon_loaded(weapon_index) <= 0.0f)
		{
			continue;
		}
		if (best == NONE || bots_weapon_value(weapon_get(weapon_index)->definition_index) >
			bots_weapon_value(weapon_get(best)->definition_index))
		{
			best = weapon_index;
		}
	}
	return best;
}

/* whether the bot's team has the enemy spotted (bots hunt a flag carrier only while it is): a bot of
the team saw it in the last 3 seconds, or a teammate is within 10 units of it (the shared awareness distance) */
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

static boolean ae_bots_objective_free_position(struct ae_bots_objective_snapshot const *snapshot, long object_index,
	real_point3d *position)
{
	long index;

	if (snapshot->mode == AE_BOTS_MODE_BOMB && snapshot->bomb_weapon_index == object_index &&
		snapshot->bomb_carrier_player_index == NONE && snapshot->bomb_takeable)
	{
		*position = snapshot->bomb_position;
		return TRUE;
	}
	if (snapshot->mode == AE_BOTS_MODE_ODDBALL)
	{
		for (index = 0; index < snapshot->ball_count; index++)
		{
			if (snapshot->ball_indices[index] == object_index)
			{
				*position = snapshot->ball_positions[index];
				return TRUE;
			}
		}
	}
	return FALSE;
}

static long ae_bots_objective_weapon_holder(long weapon_index)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit;
		short slot;

		if (player->unit_index == NONE)
			continue;
		unit = unit_get(player->unit_index);
		for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
			if (unit->unit.weapon_object_indices[slot] == weapon_index)
				return iterator.datum_index;
	}
	return NONE;
}

/* A dead dropper's nearest living teammate inherits the free objective. */
static void ae_bots_objective_recovery_handoff(struct bots_bot *bot, long bot_index,
	struct ae_bots_objective_snapshot const *snapshot, real_point3d const *position)
{
	struct player_datum *player = player_try_and_get(bot->player_index);
	long source;

	if (!player)
		return;
	for (source = 0; source < BOTS_BRAINS; source++)
	{
		struct ae_bots_objective_bot *from = &ae_bots_objective_bots[source];
		struct player_datum *dropper;
		real_point3d dropped;
		long candidate, nearest = NONE;
		real nearest_distance = REAL_MAX;

		if (source == bot_index || from->fight_object_index == NONE ||
			(snapshot->teams && from->fight_team_index != player->team_index) ||
			!ae_bots_objective_free_position(snapshot, from->fight_object_index, &dropped))
		{
			continue;
		}
		dropper = player_try_and_get(from->player_index);
		if (dropper && dropper->unit_index != NONE &&
			!TEST_FLAG(object_get(dropper->unit_index)->object.damage_flags, _object_dead_bit))
		{
			continue;
		}
		if (!snapshot->teams)
		{
			if (ae_bots_events_on())
				ae_bots_event_line(source, game_time_get(), "objective_recovery_abandoned", -1, 0.0f,
					"reason=dropper_dead");
			from->fight_object_index = NONE;
			from->fight_team_index = NONE;
			continue;
		}
		for (candidate = 0; candidate < BOTS_BRAINS; candidate++)
		{
			struct bots_bot *mate = &bots_globals.bots[candidate];
			struct player_datum *mate_player = mate->player_index != NONE ? player_try_and_get(mate->player_index) : NULL;
			real_point3d mate_position;
			real distance;

			if (!mate_player || mate_player->team_index != from->fight_team_index || mate_player->unit_index == NONE ||
				TEST_FLAG(object_get(mate_player->unit_index)->object.damage_flags, _object_dead_bit))
			{
				continue;
			}
			mate_position = ae_bots_vehicle_point(mate_player->unit_index);
			distance = bots_distance3d(&mate_position, &dropped);
			if (distance < nearest_distance)
			{
				nearest_distance = distance;
				nearest = candidate;
			}
		}
		if (nearest != bot_index)
			continue;
		if (nearest_distance > 20.0f)
		{
			if (ae_bots_events_on())
				ae_bots_event_line(source, game_time_get(), "objective_recovery_abandoned", -1,
					nearest_distance, "reason=dropper_dead no_near_teammate=1");
			from->fight_object_index = NONE;
			from->fight_team_index = NONE;
			continue;
		}
		ae_bots_objective_bots[bot_index].fight_object_index = from->fight_object_index;
		ae_bots_objective_bots[bot_index].fight_team_index = from->fight_team_index;
		ae_bots_objective_bots[bot_index].fight_clear_tick = game_time_get();
		ae_bots_objective_bots[bot_index].fight_missing_tick = NONE;
		from->fight_object_index = NONE;
		from->fight_team_index = NONE;
		if (ae_bots_events_on())
			ae_bots_event_line(bot_index, game_time_get(), "objective_recovery", -1,
				bots_distance3d(position, &dropped), "reason=dropper_dead action=handoff");
		return;
	}
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
		state->fight_object_index = NONE;
		state->fight_team_index = NONE;
		state->lane_behaviour = NONE;
	}
	if (state->think_tick == now)
		return state->behaviour;
	state->think_tick = now;
	state->target_valid = FALSE;
	state->in_hands = FALSE;
	state->in_hill = FALSE;
	state->fight_drop_requested = FALSE;
	state->wingman = FALSE;
	state->snd_hold = FALSE;
	state->snd_wait = FALSE;
	state->hold_yaw_valid = FALSE;
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
	situation.bomb = situation.enemy_goal = situation.own_goal = situation.bomb_planted_own =
		situation.bomb_planted_enemy = -1.0f;
	if ((fighting || now - bot->hurt_tick < TICKS_PER_SECOND) && bot->target_player_index != NONE)
	{
		struct player_datum *target = player_try_and_get(bot->target_player_index);

		if (target && target->unit_index != NONE)
		{
			real_point3d target_position = ae_bots_vehicle_point(target->unit_index);

			situation.enemy_distance = (float)bots_distance3d(position, &target_position);
		}
	}
	situation.carrying = ae_bots_objective_player_carries(snapshot, bot->player_index);
	ae_bots_objective_recovery_handoff(bot, bot_index, snapshot, position);

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
					situation.wingman = ae_bots_objective_wingman(snapshot, flag->carrier_player_index, own) == bot_index;
					targets[AE_BOTS_ESCORT] = situation.wingman ?
						ae_bots_objective_wingman_point(carrier->unit_index, bot_index) :
						ae_bots_vehicle_point(carrier->unit_index);
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
			situation.wingman = ae_bots_objective_wingman(snapshot, holder, player->team_index) == bot_index;
			situation.ally_carrier = (float)distance;
			targets[AE_BOTS_ESCORT] = situation.wingman ?
				ae_bots_objective_wingman_point(player_get(holder)->unit_index, bot_index) :
				ae_bots_vehicle_point(player_get(holder)->unit_index);
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
	case AE_BOTS_MODE_BOMB:
	{
		long own = player->team_index, other = 1 - player->team_index;

		if (!VALID_INDEX(own, 2) || snapshot->bomb_weapon_index == NONE)
			break;
		situation.bomb_snd = ae_bomb_snd_on();
		situation.bomb_attacking = situation.bomb_snd && snapshot->bomb_attacking_team == own;
		if (situation.bomb_snd)
		{
			real_point3d const *watch = snapshot->bomb_planted_goal != NONE ? &snapshot->bomb_position :
				snapshot->bomb_has_goal[other] ? &snapshot->bomb_goals[other] : NULL;

			if (watch)
			{
				state->hold_yaw = (real)atan2(watch->y - position->y, watch->x - position->x);
				state->hold_yaw_valid = TRUE;
			}
		}
		if (situation.bomb_snd)
		{
			long round;
			short role = ae_bots_objective_snd_role_of(bot, player, situation.bomb_attacking, &round);
			real_point3d forward;

			state->snd_attacking = situation.bomb_attacking;
			situation.snd_role = state->snd_role = role;
			if (!situation.bomb_attacking && role == AE_BOTS_SND_ROLE_FORWARD && snapshot->bomb_has_goal[own] &&
				snapshot->bomb_has_goal[other] &&
				ae_bots_objective_forward_point(&snapshot->bomb_goals[own], &snapshot->bomb_goals[other], &forward))
			{
				situation.snd_forward_point = TRUE;
				state->snd_forward = forward;
			}
			/* (a forward defender with no point down the lane holds the site) */
			state->snd_hold = !situation.bomb_attacking && !situation.snd_forward_point &&
				(role != AE_BOTS_SND_ROLE_FORWARD || ae_bots_objective_roles_on());
			if (state->snd_role_round != round || state->snd_role_logged != role)
			{
				platform_log("bots: snd round %ld bot %ld %s: %s%s", round, bot_index + 1,
					situation.bomb_attacking ? "attacks" : "defends", ae_bots_snd_role_name(role),
					state->snd_role_round == round && state->snd_role_logged != AE_BOTS_SND_ROLE_NONE ?
					" (promoted)" : "");
				state->snd_role_round = round;
				state->snd_role_logged = role;
			}
		}
		situation.defender = situation.bomb_snd ? state->snd_hold : ae_bots_objective_defender(bot, player);
		state->in_hands = situation.carrying;
		/* the enemy goal: where its carrier arms the bomb */
		if (snapshot->bomb_has_goal[other])
		{
			situation.enemy_goal = (float)bots_distance3d(position, &snapshot->bomb_goals[other]);
			targets[AE_BOTS_BOMB_CARRY] = snapshot->bomb_goals[other];
		}
		/* the own goal: kept; the bomb planted there: saved by stepping onto it */
		if (snapshot->bomb_has_goal[own])
		{
			situation.own_goal = (float)bots_distance3d(position, &snapshot->bomb_goals[own]);
			targets[AE_BOTS_BOMB_DEFEND] = situation.snd_forward_point ? state->snd_forward : snapshot->bomb_goals[own];
			if (snapshot->bomb_planted_goal == own)
			{
				situation.bomb_planted_own = (float)bots_distance3d(position, &snapshot->bomb_position);
				targets[AE_BOTS_BOMB_SAVE] = snapshot->bomb_position;
				target_objects[AE_BOTS_BOMB_SAVE] = snapshot->bomb_weapon_index;
				if (situation.bomb_snd)
				{
					boolean held = ae_bomb_assault_defuse_held_by_other(bot->player_index, &snapshot->bomb_position,
						0.6f);

					situation.bomb_defuser = !held && (state->behaviour == AE_BOTS_BOMB_SAVE ||
						ae_bots_objective_first_living_bot(bot, player));
					targets[AE_BOTS_BOMB_DEFEND] = snapshot->bomb_position;
				}
			}
		}
		if (situation.bomb_snd && snapshot->bomb_planted_goal == other)
		{
			situation.bomb_planted_enemy = (float)bots_distance3d(position, &snapshot->bomb_position);
			targets[AE_BOTS_BOMB_DEFEND] = snapshot->bomb_position;
		}
		/* the bomb lying loose: fetched (by its team only, AE's FLAG SPAWNS) */
		if (snapshot->bomb_takeable && (snapshot->bomb_attacking_team == NONE || snapshot->bomb_attacking_team == own))
		{
			situation.bomb = (float)bots_distance3d(position, &snapshot->bomb_position);
			targets[AE_BOTS_BOMB_TAKE] = snapshot->bomb_position;
			target_objects[AE_BOTS_BOMB_TAKE] = snapshot->bomb_weapon_index;
		}
		/* its carrier: escorted by its team, hunted by the other */
		if (snapshot->bomb_carrier_player_index != NONE && snapshot->bomb_carrier_player_index != bot->player_index)
		{
			struct player_datum *carrier = player_try_and_get(snapshot->bomb_carrier_player_index);

			if (carrier && carrier->unit_index != NONE)
			{
				real_point3d carrier_position = ae_bots_vehicle_point(carrier->unit_index);
				short behaviour_index = bots_is_enemy(player, carrier) ? AE_BOTS_BALL_CHASE : AE_BOTS_ESCORT;

				targets[behaviour_index] = carrier_position;
				target_players[behaviour_index] = snapshot->bomb_carrier_player_index;
				/* (a teammate's carrier this near the enemy goal: its escorts go on ahead to the goal and clear it) */
				if (behaviour_index == AE_BOTS_ESCORT && snapshot->bomb_has_goal[other] &&
					bots_distance2d(&carrier_position, &snapshot->bomb_goals[other]) < AE_BOTS_OBJECTIVE_BOMB_AHEAD)
				{
					targets[behaviour_index] = snapshot->bomb_goals[other];
					target_players[behaviour_index] = NONE;
				}
				if (behaviour_index == AE_BOTS_BALL_CHASE)
					situation.enemy_carrier = (float)bots_distance3d(position, &carrier_position);
				else
				{
					situation.wingman = ae_bots_objective_wingman(snapshot,
						snapshot->bomb_carrier_player_index, own) == bot_index;
					if (situation.wingman)
					{
						targets[behaviour_index] = ae_bots_objective_wingman_point(carrier->unit_index, bot_index);
						target_players[behaviour_index] = snapshot->bomb_carrier_player_index;
					}
					else if (situation.bomb_snd && snapshot->bomb_has_goal[other])
					{
						targets[behaviour_index] = snapshot->bomb_goals[other];
						target_players[behaviour_index] = NONE;
						/* (the lurker keeps its lane's waypoint until the carrier is nearly at the goal) */
						state->snd_wait = state->snd_role == AE_BOTS_SND_ROLE_LURKER &&
							bots_distance2d(&carrier_position, &snapshot->bomb_goals[other]) > AE_BOTS_OBJECTIVE_LURK_FAR;
					}
					situation.ally_carrier = (float)bots_distance3d(position, &carrier_position);
				}
			}
		}
		break;
	}
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
	/* A stock Oddball or SEARCH AND DESTROY carrier may put the objective at its feet and draw a loaded gun. The
	pure rule also holds a remembered drop out of the pickup loop until the threat has been gone for a while. */
	if (snapshot->mode == AE_BOTS_MODE_ODDBALL || (snapshot->mode == AE_BOTS_MODE_BOMB && situation.bomb_snd))
	{
		struct ae_bots_carrier_situation carrier;
		struct unit_datum *unit = unit_get(player->unit_index);
		long objective_index = NONE, gun_index;
		real teammate_distance, cover_distance = -1.0f;
		boolean in_cover = FALSE, remembered_free = FALSE;
		long living;
		int choice;

		if (snapshot->mode == AE_BOTS_MODE_BOMB)
			objective_index = snapshot->bomb_weapon_index;
		else if (situation.carrying)
		{
			short slot;

			for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
			{
				long weapon_index = unit->unit.weapon_object_indices[slot];

				if (weapon_index != NONE && weapon_is_flag(weapon_index))
				{
					objective_index = weapon_index;
					break;
				}
			}
		}
		if (state->fight_object_index != NONE)
		{
			struct ae_bots_recovery_situation recovery;
			real_point3d recovery_position, recovery_ground;
			long holder = snapshot->mode == AE_BOTS_MODE_BOMB ? snapshot->bomb_carrier_player_index : NONE;
			long replacement = NONE;
			boolean recovered = FALSE;
			int recovery_choice;

			if (situation.carrying && objective_index == state->fight_object_index)
			{
				holder = bot->player_index;
			}
			else if (snapshot->mode == AE_BOTS_MODE_ODDBALL)
				holder = ae_bots_objective_weapon_holder(state->fight_object_index);
			remembered_free = ae_bots_objective_free_position(snapshot, state->fight_object_index, &recovery_position);
			if (!remembered_free)
			{
				if (snapshot->mode == AE_BOTS_MODE_BOMB && snapshot->bomb_takeable)
					replacement = snapshot->bomb_weapon_index;
				else if (snapshot->mode == AE_BOTS_MODE_ODDBALL && snapshot->ball_count > 0)
					replacement = snapshot->ball_indices[0];
			}
			if (!remembered_free && holder == NONE && state->fight_missing_tick == NONE)
				state->fight_missing_tick = now;
			csmemset(&recovery, 0, sizeof(recovery));
			recovery.dropper_alive = TRUE;
			recovery.team_game = snapshot->teams;
			recovery.remembered_free = remembered_free;
			recovery.replacement_free = replacement != NONE;
			recovery.objective_carried = holder != NONE;
			recovery.goal_valid = remembered_free && bots_surface_below(&recovery_position, &recovery_ground) != NONE;
			recovery.enemy_engaging = situation.enemy_distance >= 0.0f && fighting;
			recovery.clear_seconds = (float)(now - state->fight_clear_tick) / TICKS_PER_SECOND;
			recovery.missing_seconds = state->fight_missing_tick == NONE ? 0.0f :
				(float)(now - state->fight_missing_tick) / TICKS_PER_SECOND;
			recovery_choice = ae_bots_recovery_choice(&recovery);
			if (holder != NONE)
			{
				struct player_datum *holder_player = player_try_and_get(holder);

				recovered = holder == bot->player_index || (snapshot->teams && holder_player &&
					holder_player->team_index == state->fight_team_index);
				if (recovered)
					ae_bots_objective_fight_drop_note(bot_index, TRUE);
				if (ae_bots_events_on())
					ae_bots_event_line(bot_index, now, recovered ? "objective_pickup" : "objective_recovery_abandoned",
						ae_bots_event_absolute(holder), 0.0f, recovered ?
						(snapshot->mode == AE_BOTS_MODE_BOMB ? "objective=bomb reason=recovered" :
						"objective=ball reason=recovered") : "reason=taken");
				state->fight_object_index = NONE;
				state->fight_team_index = NONE;
				state->fight_drop_again_tick = now + 3 * TICKS_PER_SECOND;
			}
			else if (recovery_choice == AE_BOTS_RECOVERY_RESET)
			{
				state->fight_object_index = replacement;
				state->fight_missing_tick = NONE;
				if (ae_bots_events_on())
					ae_bots_event_line(bot_index, now, "objective_recovery", -1, 0.0f, "reason=reset action=adopt");
			}
			else if (recovery_choice == AE_BOTS_RECOVERY_ABANDON_GONE ||
				recovery_choice == AE_BOTS_RECOVERY_ABANDON_NO_GOAL)
			{
				if (ae_bots_events_on())
					ae_bots_event_line(bot_index, now, "objective_recovery_abandoned", -1, 0.0f,
						recovery_choice == AE_BOTS_RECOVERY_ABANDON_NO_GOAL ? "reason=no_goal" : "reason=gone");
				state->fight_object_index = NONE;
				state->fight_team_index = NONE;
			}
			else if (remembered_free)
			{
				short take = snapshot->mode == AE_BOTS_MODE_BOMB ? AE_BOTS_BOMB_TAKE : AE_BOTS_BALL_TAKE;

				targets[take] = recovery_position;
				target_objects[take] = state->fight_object_index;
				state->fight_missing_tick = NONE;
			}
		}
		gun_index = ae_bots_objective_carrier_gun(unit, objective_index);
		living = ae_bots_objective_living_team(player, position, snapshot->teams, &teammate_distance);
		if (snapshot->mode == AE_BOTS_MODE_ODDBALL)
			ae_bots_cover_carrier_place(bot, position, &cover_distance, &in_cover);
		csmemset(&carrier, 0, sizeof(carrier));
		carrier.mode = snapshot->mode;
		carrier.skill_level = ae_bots_level_of_skill(bots_globals.skill);
		carrier.bomb_snd = situation.bomb_snd;
		carrier.carrying = situation.carrying;
		carrier.dropped = state->fight_object_index != NONE;
		carrier.under_fire = now - bot->hurt_tick < TICKS_PER_SECOND * 3 / 4;
		carrier.gun_loaded = gun_index != NONE;
		carrier.last_alive = living == 1;
		carrier.in_cover = in_cover;
		carrier.enemy_distance = situation.enemy_distance;
		carrier.gun_range = bots_weapon_range(gun_index);
		carrier.enemy_engaging = situation.enemy_distance >= 0.0f &&
			(carrier.under_fire || (fighting && situation.enemy_distance <= carrier.gun_range));
		carrier.teammate_distance = teammate_distance;
		carrier.goal_distance = snapshot->mode == AE_BOTS_MODE_BOMB ? situation.enemy_goal : -1.0f;
		carrier.cover_distance = cover_distance;
		if (carrier.dropped)
		{
			if (carrier.enemy_engaging)
				state->fight_clear_tick = now;
			carrier.clear_seconds = (float)(now - state->fight_clear_tick) / TICKS_PER_SECOND;
		}
		carrier.drop_cooldown_seconds = now < state->fight_drop_again_tick ?
			(float)(state->fight_drop_again_tick - now) / TICKS_PER_SECOND : 0.0f;
		choice = ae_bots_carrier_choice(&carrier);
		if (choice == AE_BOTS_CARRIER_DROP)
		{
			state->fight_drop_requested = TRUE;
			state->target_object_index = objective_index;
			for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
				utility[index] = 0.0f;
		}
		else if (choice == AE_BOTS_CARRIER_WAIT)
		{
			for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
				utility[index] = 0.0f;
		}
		else if (choice == AE_BOTS_CARRIER_RECOVER)
		{
			short take = snapshot->mode == AE_BOTS_MODE_BOMB ? AE_BOTS_BOMB_TAKE : AE_BOTS_BALL_TAKE;

			for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
				utility[index] = 0.0f;
			utility[take] = 1.0f;
		}
	}

	behaviour = (short)ae_bots_objective_pick(utility, state->behaviour, now - state->chosen_tick,
		AE_BOTS_OBJECTIVE_HYSTERESIS_TICKS(bots_globals.skill), AE_BOTS_OBJECTIVE_BONUS(bots_globals.skill));
	if (behaviour != state->behaviour)
	{
		state->behaviour = behaviour;
		state->chosen_tick = now;
		state->patrol_until_tick = 0;
		state->lane_behaviour = NONE;
	}
	state->own_flag_carried = situation.own_flag_carried;
	state->wingman = situation.wingman;
	if (behaviour != AE_BOTS_FIGHT)
		ae_bots_objective_target(state, &targets[behaviour], target_objects[behaviour], target_players[behaviour]);
	ae_bots_objective_note(bot_index, bot->player_index, behaviour);
	ae_bots_objective_strategy_note(player->team_index, situation.ally_carrier >= 0.0f, situation.wingman,
		situation.bomb_snd && !situation.bomb_attacking, state->snd_hold && behaviour == AE_BOTS_BOMB_DEFEND);
	if (situation.bomb_snd)
	{
		short role = situation.carrying ? AE_BOTS_SND_ROLE_CARRIER : situation.bomb_attacking ?
			(situation.wingman ? AE_BOTS_SND_ROLE_SUPPORT : state->snd_role == AE_BOTS_SND_ROLE_LURKER ?
			AE_BOTS_SND_ROLE_LURKER : AE_BOTS_SND_ROLE_ENTRY) :
			(state->snd_hold && state->snd_role == AE_BOTS_SND_ROLE_FORWARD ? AE_BOTS_SND_ROLE_HOLDER : state->snd_role);
		int zone = situation.enemy_goal >= 0.0f && situation.enemy_goal < 12.0f ? 2 :
			situation.own_goal >= 0.0f && situation.own_goal < 12.0f ? 0 : 1;

		ae_bots_objective_snd_note(situation.bomb_attacking, zone, role, role != state->snd_role_noted);
		state->snd_role_noted = role;
		if (situation.own_goal < 0.0f || situation.enemy_goal < 0.0f)
			ae_bots_objective_snd_event(AE_BOTS_SND_EVENT_NO_GOAL);
	}
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

/* Bomb: a carrier where it arms the bomb in the enemy goal: the engine's own test (ae_bots_bomb_in_goal, a hook in
game_engine_bomb.c: a court's goal volume, else 1 world unit from the goal's marker) */
static boolean ae_bots_objective_bomb_in_goal(struct bots_bot *bot)
{
#ifdef HALO_GAME_BROWSER
	struct player_datum *player = player_try_and_get(bot->player_index);

	return player && player->unit_index != NONE && VALID_INDEX(player->team_index, 2) &&
		ae_bots_bomb_in_goal(player->unit_index, 1 - player->team_index);
#else
	(void)bot;
	return FALSE;
#endif
}

/* A first waypoint through a different side of the map, chosen from the map's item and spawn navigation points.
It is held until reached, so path searches stay cheap and the squad does not change lanes each tick. */
/* a number for this life of the bot: its body is a new object each life (one a round in SEARCH AND DESTROY), so the
lane and the site spot it picks change from round to round */
static unsigned long ae_bots_objective_life_seed(struct bots_bot const *bot, long bot_index)
{
	struct player_datum const *player = player_try_and_get(bot->player_index);
	unsigned long unit = player ? (unsigned long)player->unit_index : 0;

	return ((unit >> 16) ^ unit) * 2654435761UL + (unsigned long)bot_index * 7;
}

static boolean ae_bots_objective_lane(struct bots_bot *bot, long bot_index, real_point3d const *position,
	real_point3d const *target, real_point3d *lane)
{
	long index, other, best = NONE;
	float best_score = -1.0f;
	int side = ae_bots_objective_bots[bot_index].snd_role == AE_BOTS_SND_ROLE_ENTRY ? 1 :
		ae_bots_objective_bots[bot_index].snd_role == AE_BOTS_SND_ROLE_LURKER ? -1 :
		(int)(ae_bots_objective_life_seed(bot, bot_index) % 3) - 1;

	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real_point3d const *point = &bots_globals.roam_points[index];
		float taken_squared = REAL_MAX;
		float score;

		if ((real)fabs(point->z - position->z) > 3.0f)
			continue;
		for (other = 0; other < BOTS_BRAINS; other++)
		{
			struct ae_bots_objective_bot const *mate = &ae_bots_objective_bots[other];
			float dx, dy, distance_squared;

			if (other == bot_index || mate->lane_behaviour == NONE || mate->lane_done)
				continue;
			dx = point->x - mate->lane_point.x;
			dy = point->y - mate->lane_point.y;
			distance_squared = dx * dx + dy * dy;
			if (distance_squared < taken_squared)
				taken_squared = distance_squared;
		}
		score = ae_bots_lane_score(position->x, position->y, target->x, target->y, point->x, point->y,
			side, taken_squared);
		if (score > best_score)
		{
			best_score = score;
			best = index;
		}
	}
	if (best == NONE)
		return FALSE;
	*lane = bots_globals.roam_points[best];
	return TRUE;
}

/* A defender's own cover angle about the site, from a nearby navigation point and away from another holder's. */
static boolean ae_bots_objective_site_spot(long bot_index, unsigned long seed, real_point3d const *site,
	real_point3d *spot)
{
	struct ae_bots_objective_bot const *self = &ae_bots_objective_bots[bot_index];
	/* the anchor's ring is tight about the site, the holder's wider; attackers about a planted bomb stand farther off */
	real inner = self->snd_attacking ? 4.0f : self->snd_role == AE_BOTS_SND_ROLE_HOLDER ? 3.0f : 1.5f;
	real outer = self->snd_attacking ? 14.0f : self->snd_role == AE_BOTS_SND_ROLE_HOLDER ? 10.0f : 6.0f;

	long index, other, best = NONE;
	real best_score = -REAL_MAX;

	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real_point3d const *point = &bots_globals.roam_points[index];
		real distance = bots_distance2d(point, site);
		real separation = 12.0f;
		real score;

		if (distance < inner || distance > outer || (real)fabs(point->z - site->z) > 2.0f)
			continue;
		for (other = 0; other < BOTS_BRAINS; other++)
		{
			struct ae_bots_objective_bot const *mate = &ae_bots_objective_bots[other];
			real apart;

			if (other == bot_index || mate->snd_attacking != self->snd_attacking ||
				(!mate->snd_hold && !mate->snd_attacking) || mate->patrol_behaviour != AE_BOTS_BOMB_DEFEND)
				continue;
			apart = bots_distance2d(point, &mate->patrol_point);
			if (apart < separation)
				separation = apart;
		}
		if (separation < 2.5f)
			continue;
		/* this life's preference: a different spot each round, not every holder at the nearest door */
		score = separation * 2.0f - distance + (real)(((unsigned long)index * 31 + seed) % 11) * 0.4f;
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

/* a carrier stands in the goal at most the variant's arm time and this long more (an arming the engine does not count,
a goal it does not reach: it steps to the goal's centre again, then stands again) */
#define AE_BOTS_OBJECTIVE_BOMB_STAND_EXTRA_TICKS TICKS_PER_SECOND
#define AE_BOTS_OBJECTIVE_BOMB_REPLAN_TICKS TICKS_PER_SECOND

/* the variant's arm time (the carrier's seconds in the goal before it plants), in ticks */
static long ae_bots_objective_bomb_arm_ticks(void)
{
#ifdef HALO_GAME_BROWSER
	if (game_engine && game_engine_get_variant()->game_engine_index == game_engine_bomb)
		return (long)game_engine_get_variant()->game_engine_variant.bomb.arm_time * TICKS_PER_SECOND;
#endif
	return 0;
}

/* the bot's goal for its behaviour (bots_set_goal), FALSE for none now (it stands and looks about) */
static boolean ae_bots_objective_set_goal(struct bots_bot *bot, long bot_index, real_point3d const *position)
{
	struct ae_bots_objective_bot *state = &ae_bots_objective_bots[bot_index];
	long now = game_time_get();
	real_point3d point, ground;
	long surface_index;
	short kind = _bots_goal_roam;
	boolean lane_waypoint = FALSE;

	if (!state->target_valid)
		return FALSE;
	point = state->target;
	{
		struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
		boolean snd_lane = snapshot && snapshot->mode == AE_BOTS_MODE_BOMB && ae_bomb_snd_on() &&
			(state->behaviour == AE_BOTS_BOMB_CARRY || (state->behaviour == AE_BOTS_ESCORT && !state->wingman) ||
			(state->behaviour == AE_BOTS_BALL_CHASE && !state->snd_hold));

		if (snd_lane && state->lane_behaviour != state->behaviour)
		{
			state->lane_behaviour = state->behaviour;
			state->lane_done = !ae_bots_objective_lane(bot, bot_index, position, &point, &state->lane_point);
			state->lane_held = !state->lane_done;
			state->lane_tick = now;
			ae_bots_objective_snd_event(state->lane_done ? AE_BOTS_SND_EVENT_LANE_FAILED : AE_BOTS_SND_EVENT_LANE_MADE);
		}
		if (snd_lane && !state->lane_done)
		{
			if (bots_distance2d(position, &state->lane_point) < 1.5f)
				state->lane_done = TRUE;
			else if (now - state->lane_tick > TICKS_PER_SECOND * AE_BOTS_OBJECTIVE_LANE_SECONDS)
			{
				/* (a waypoint it has not come to in this long is one the walk does not reach: on to the goal) */
				state->lane_done = TRUE;
				state->lane_held = FALSE;
				ae_bots_objective_snd_event(AE_BOTS_SND_EVENT_LANE_FAILED);
			}
			else
			{
				point = state->lane_point;
				lane_waypoint = TRUE;
			}
		}
		/* (MINI SQUADS: a big team's bots by their squad's route, unless they carry, defend or stay with a player) */
		if (!snd_lane && !state->wingman && (state->behaviour == AE_BOTS_CTF_TAKE || state->behaviour == AE_BOTS_ESCORT ||
			state->behaviour == AE_BOTS_HILL_GO || state->behaviour == AE_BOTS_BALL_TAKE || state->behaviour == AE_BOTS_JUGG_HUNT))
		{
			real_point3d route;
			long route_surface;

			if (ae_bots_squads_route(bot, bot_index, position, &state->target, &route, &route_surface))
			{
				point = route;
				lane_waypoint = TRUE;
			}
		}
	}
	switch (state->behaviour)
	{
	case AE_BOTS_BOMB_CARRY:
		/* (in the goal: it stands there while the bomb arms, game_engine_bomb.c's BOMB_GOAL_RADIUS, 1 unit from the
		goal's marker) */
		if (ae_bots_objective_bomb_in_goal(bot) && game_time_get() >= state->bomb_replan_until_tick)
		{
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		break;
	case AE_BOTS_CTF_CARRY:
		/* (at its stand, the own flag gone: it waits there) */
		if (bots_distance2d(&point, position) < AE_BOTS_OBJECTIVE_THERE && fabs(point.z - position->z) < 1.5f)
		{
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		break;
	case AE_BOTS_ESCORT:
		if (lane_waypoint)
			break;
		/* (the lurker, on its lane: it stands there and looks about, till the carrier is near the goal) */
		if (state->snd_wait && state->lane_held && now - state->chosen_tick < TICKS_PER_SECOND * AE_BOTS_OBJECTIVE_LURK_SECONDS)
		{
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		/* (close to the one it follows: it looks about; a carrier holding a hidden spot: at its open side) */
		if (state->target_player_index != NONE)
		{
			real facing;
			real_point3d guard;

			if (ae_bots_cover_escort_point(state->target_player_index, &guard, &facing))
			{
				if (bots_distance2d(&guard, position) < AE_BOTS_OBJECTIVE_THERE && fabs(guard.z - position->z) < 1.5f)
				{
					bot->goal_kind = _bots_goal_none;
					return FALSE;
				}
				point = guard;
				break;
			}
			if (bots_distance3d(&point, position) < AE_BOTS_OBJECTIVE_ESCORT_CLOSE)
			{
				bot->goal_kind = _bots_goal_none;
				return FALSE;
			}
			break;
		}
		/* (Bomb's carrier near the enemy goal: its escorts ahead of it, about the goal, as its guards are: they clear it
		for the 3 seconds it arms) */
		/* FALLTHROUGH */
	case AE_BOTS_CTF_DEFEND:
	case AE_BOTS_BOMB_DEFEND:
	case AE_BOTS_HILL_HOLD:
	{
		boolean defend = state->behaviour != AE_BOTS_HILL_HOLD;

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
			state->pause_until_tick = now + (state->snd_hold || state->snd_attacking ?
				TICKS_PER_SECOND * 2 + (long)(bots_random(bot) % (TICKS_PER_SECOND * 4)) : AE_BOTS_OBJECTIVE_PAUSE_TICKS(bot));
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
			if ((state->snd_hold || (state->snd_attacking && ae_bots_objective_roles_on())) && state->behaviour == AE_BOTS_BOMB_DEFEND &&
				ae_bots_objective_site_spot(bot_index, ae_bots_objective_life_seed(bot, bot_index) +
					(unsigned long)state->patrol_serial++ * 40503UL, &point, &spot))
			{
				state->patrol_point = spot;
			}
			else
			{
				if ((state->snd_hold || (state->snd_attacking && ae_bots_objective_roles_on())) && state->behaviour == AE_BOTS_BOMB_DEFEND)
					ae_bots_objective_snd_event(AE_BOTS_SND_EVENT_NO_SPOT);
				if (ae_bots_objective_spot(bot, &point, defend ? 1.5f : 0.0f, defend ? 6.0f : 1.2f, &spot, &spot_surface))
					state->patrol_point = spot;
				else
					state->patrol_point = point;
			}
		}
		point = state->patrol_point;
		break;
	}
	case AE_BOTS_BALL_HOLD:
	case AE_BOTS_JUGG_FLEE:
	{
		/* (a covered spot the enemies it knows of do not see, held while it stays hidden: ae_bots_cover.inc; none
		there: the old roaming away) */
		int cover = ae_bots_cover_goal(bot, position, &point);

		if (cover == 2)
		{
			bot->goal_kind = _bots_goal_none;
			return FALSE;
		}
		if (cover == 1)
		{
			state->patrol_behaviour = AE_BOTS_FIGHT;
			break;
		}
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
	}
	case AE_BOTS_BALL_TAKE:
	case AE_BOTS_BOMB_TAKE:
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
	/* (an Infection zombie hunts: ae_bots_combat.inc) */
	if (ae_bots_zombie_goal(bot, position, fighting))
		return TRUE;
	behaviour = ae_bots_objective_think(bot, bot_index, position, fighting);
	if (behaviour == AE_BOTS_FIGHT)
		return FALSE;
	state = &ae_bots_objective_bots[bot_index];
	/* a better weapon close by: bots.c's own choice fetches it (not with the objective in its hands, nor while it
	carries, nor at the ball) */
	if (!ae_bots_objective_player_carries(ae_bots_objective_snapshot(), player_index) && behaviour != AE_BOTS_BALL_TAKE &&
		behaviour != AE_BOTS_BOMB_TAKE && behaviour != AE_BOTS_BOMB_SAVE)
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
	/* (with a human: at their side unless it carries, defends or saves; the numbers of bots keep the rest on the objective) */
	if (!state->wingman && (behaviour == AE_BOTS_CTF_TAKE || behaviour == AE_BOTS_ESCORT || behaviour == AE_BOTS_HILL_GO ||
		behaviour == AE_BOTS_BALL_TAKE || behaviour == AE_BOTS_JUGG_HUNT))
	{
		long human;
		struct player_datum const *human_player;

		/* (a bot not carrying or defending goes for the power items too; asked here, as this ask comes before bots.c's) */
		if (ae_bots_power_goal(bot, player_index, position, fighting))
			return TRUE;
		ae_bots_team_update(now);
		human = ae_bots_team_wingman_of(bot);
		if (human != NONE && (human_player = player_try_and_get(human)) != NULL && ae_bots_team_alive(human_player) &&
			ae_bots_team_goal(bot, player_index, position, fighting))
		{
			return TRUE;
		}
	}
	if (!ae_bots_objective_set_goal(bot, bot_index, position) && bot->goal_kind != _bots_goal_none)
		bot->goal_kind = _bots_goal_none;
	return TRUE;
}

/* (ae_bots_team.inc) whether this bot may go with a human: CTF's site defender stays at the flag */
static boolean ae_bots_team_free_for_human(struct bots_bot *bot, struct player_datum *player)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();

	return !(snapshot && snapshot->mode == AE_BOTS_MODE_CTF && ae_bots_objective_defender(bot, player));
}

void ae_bots_objective_move(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_bots_objective_bot *state;
	struct ae_bots_objective_snapshot const *snapshot;
	short behaviour;
	real direction;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->unit)
		return;
	behaviour = ae_bots_objective_think(bot, bot_index, &tick->position, tick->fighting);
	state = &ae_bots_objective_bots[bot_index];
	snapshot = ae_bots_objective_snapshot();
	/* (the log: the flag or ball gone from its hands, in this life: not a death, and a capture says so by its own line) */
	{
		static struct { long unit_index; boolean had; } hands[BOTS_BRAINS];

		if (ae_bots_events_on() && hands[bot_index].had && hands[bot_index].unit_index == tick->player->unit_index && !state->in_hands)
		{
			char extra[96];

			snprintf(extra, sizeof(extra), "flags=0x%lx vitality=%.2f seat=%d job=%s", ae_bots_event_last_flags[bot_index],
				(double)ae_bots_event_vitality(tick->object), tick->unit->object.parent_object_index != NONE, ae_bots_behaviour_name(behaviour));
			ae_bots_event_line(bot_index, tick->now, "objective_lost", ae_bots_event_absolute(bot->target_player_index), tick->target_distance, extra);
		}
		hands[bot_index].unit_index = tick->player->unit_index;
		hands[bot_index].had = state->in_hands;
	}
	if (state->fight_drop_requested && state->target_object_index != NONE &&
		unit_drop_weapon(tick->player->unit_index, state->target_object_index))
	{
		state->fight_object_index = state->target_object_index;
		state->fight_team_index = tick->player->team_index;
		state->fight_clear_tick = tick->now;
		state->fight_missing_tick = NONE;
		state->fight_drop_requested = FALSE;
		state->in_hands = FALSE;
		ae_bots_objective_fight_drop_note(bot_index, FALSE);
		if (ae_bots_events_on())
			ae_bots_event_line(bot_index, tick->now, "objective_drop", ae_bots_event_absolute(bot->target_player_index),
			tick->target_distance, snapshot && snapshot->mode == AE_BOTS_MODE_BOMB ? "objective=bomb" : "objective=ball");
	}
	/* (the carriers' hiding: counted, posed, the escorts facing out: ae_bots_cover.inc) */
	{
		if (snapshot && (snapshot->mode == AE_BOTS_MODE_ODDBALL || snapshot->mode == AE_BOTS_MODE_TAG ||
			snapshot->mode == AE_BOTS_MODE_JUGGERNAUT))
		{
			ae_bots_cover_count(bot, tick, ae_bots_objective_player_carries(snapshot, bot->player_index));
			if (behaviour == AE_BOTS_BALL_HOLD || behaviour == AE_BOTS_JUGG_FLEE)
				ae_bots_cover_pose(bot, tick);
			else if (behaviour == AE_BOTS_ESCORT && state->target_player_index != NONE)
				ae_bots_cover_escort_pose(bot, tick, state->target_player_index);
		}
	}
	/* (an Infection zombie's skull, the only weapon it may hold: as the ball in its hands; its blow is still struck; a
	melee-only Bomb game's fists or sword the same: nothing there is fired, its blow alone) */
	if (ae_bots_brawler(tick->player->unit_index))
	{
		tick->action->desired_weapon_index = NONE;
		tick->flags &= ~(FLAG(_unit_control_swap_weapons_bit) | FLAG(_unit_control_weapon_reload_bit) |
			FLAG(_unit_control_weapon_primary_trigger_bit) | FLAG(_unit_control_weapon_secondary_trigger_bit));
	}
	/* (the flag or the ball in its hands: it neither swaps nor reloads it away, nor pulls a trigger, which drops a
	weapon that must be readied, as a player's does: players.c; it still strikes with it) */
	if (state->in_hands)
	{
		tick->action->desired_weapon_index = NONE;
		tick->flags &= ~(FLAG(_unit_control_swap_weapons_bit) | FLAG(_unit_control_weapon_reload_bit) |
			FLAG(_unit_control_weapon_primary_trigger_bit) | FLAG(_unit_control_weapon_secondary_trigger_bit));
	}
	/* (Bomb's carrier in the enemy goal while the bomb arms, a saver on the planted bomb: it stands still there,
	fighting from where it is, no sidestep out of it; the last steps to either straight on, as a path ends a unit
	short: game_engine_bomb.c arms in its goal (ae_bots_bomb_in_goal) and saves within 0.6 of the bomb. A carrier stands
	the arm time and a second at most, then steps to the goal's centre a second, and stands again) */
	if (behaviour != AE_BOTS_BOMB_CARRY)
		state->bomb_standing = FALSE;
	if ((behaviour == AE_BOTS_BOMB_CARRY || behaviour == AE_BOTS_BOMB_SAVE) && state->target_valid)
	{
		real across = bots_distance2d(&state->target, &tick->position);
		boolean level = fabs(state->target.z - tick->position.z) < 1.0f;
		boolean there;

		if (behaviour == AE_BOTS_BOMB_CARRY)
		{
			there = tick->now >= state->bomb_replan_until_tick && ae_bots_objective_bomb_in_goal(bot);
			if (!there)
				state->bomb_standing = FALSE;
			else if (!state->bomb_standing)
			{
				state->bomb_standing = TRUE;
				state->bomb_stand_tick = tick->now;
			}
			else if (tick->now - state->bomb_stand_tick > ae_bots_objective_bomb_arm_ticks() +
				AE_BOTS_OBJECTIVE_BOMB_STAND_EXTRA_TICKS)
			{
				state->bomb_standing = FALSE;
				state->bomb_replan_until_tick = tick->now + AE_BOTS_OBJECTIVE_BOMB_REPLAN_TICKS;
				there = FALSE;
			}
		}
		else
			/* (anywhere inside game_engine_bomb.c's save radius, 0.6: a defuse starts there and restarts once the
			saver steps or jumps out, so it stands from the radius's edge on, owner 2026-10-26: the HUD's DEFUSING
			came and went as a fighting saver strafed and jumped in and out of it) */
			there = across < 0.5f && level;
		if (there)
		{
			tick->moving = FALSE;
			tick->flags &= ~FLAG(_unit_control_jump_bit);
			return;
		}
		if (across < 2.0f && level)
		{
			tick->move_direction = (real)atan2(state->target.y - tick->position.y, state->target.x - tick->position.x);
			tick->moving = TRUE;
			tick->throttle_scale = across < 0.8f ? 0.5f : 1.0f;
			return;
		}
	}
	if (!tick->fighting || behaviour == AE_BOTS_FIGHT)
		return;
	/* (in the hill, it fights as bots.c does; out of it, the hill's centre again) */
	if (behaviour == AE_BOTS_HILL_HOLD && state->in_hill)
		return;
	if (!ae_bots_objective_runs_while_fighting(behaviour,
		snapshot != NULL && snapshot->mode == AE_BOTS_MODE_BOMB && ae_bomb_snd_on()) &&
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
		ae_bots_objective_routed_tick[bot_index] = tick->now;
	}
}

/* where the objective has a bot watch while it stands at its post (SEARCH AND DESTROY: the enemy goal's way, or the planted
bomb); FALSE for anywhere */
static boolean ae_bots_objective_hold_yaw(long bot_index, real *yaw)
{
	struct ae_bots_objective_bot const *state = &ae_bots_objective_bots[bot_index];

	if (!state->hold_yaw_valid || state->think_tick < game_time_get() - TICKS_PER_SECOND)
		return FALSE;
	*yaw = state->hold_yaw;
	return TRUE;
}
