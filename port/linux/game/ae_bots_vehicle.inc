/*
AE_BOTS_VEHICLE.INC

Included by ae_bots_lan.inc, so at the end of bots.c (ae_bots_vehicle.h): the
bots' vehicles with bots.c's own brain, its static functions and state
unchanged. Its AE hook lines call in:

- bots_bot_update, after the tick's position: ae_bots_vehicle_origin, a
  rider's place in the world (bots.c reads the object's position, which for a
  rider is its place in the vehicle's frame).
- bots_bot_update, after the tick's weapon: ae_bots_vehicle_weapon, a gunner's
  weapon is the vehicle's (the chain gun's bursts, reach and projectile speed
  for bots.c's fight).
- bots_bot_look, a target seen: where it is, a rider too.
- bots_choose_goal, first: ae_bots_vehicle_goal, the seat's entrance while
  the bot goes for one (bots.c walks it there).
- bots_bot_update, after the objective's walk: ae_bots_vehicle_move. On foot:
  the seat to go for (looked for once a second, from the pure rules), the last
  steps to its entrance and the action button there, once the game offers
  that seat (players.c offers the seat whose entrance is nearest). In a seat:
  the gunner's look about, the rules' leave (the action button again), and the
  driving spike.

Everything a bot decides comes from the tick's game state and its own random
numbers (bots_random), as bots.c's rule is.
*/

#include "units/unit_definitions.h"
#include "units/vehicles.h"
#include "game/game_allegiance.h"
#include "ae_bots_vehicle.h"
#include "ae_bots_combat_rules.h"

/* a bot on foot looks for a seat once a second */
#define AE_BOTS_VEHICLE_LOOK_TICKS TICKS_PER_SECOND
/* a seat not reached in this long is given up; one not boarded this long after coming near */
#define AE_BOTS_VEHICLE_GO_TICKS (12 * TICKS_PER_SECOND)
#define AE_BOTS_VEHICLE_BOARD_TICKS (4 * TICKS_PER_SECOND)
/* this near the seat's entrance: straight at it; this near: standing, the action button (players.c offers a seat
whose entrance is within 1 of the unit's centre) */
#define AE_BOTS_VEHICLE_APPROACH 2.5f
#define AE_BOTS_VEHICLE_AT_ENTRANCE 0.6f
/* a vehicle it left (or did not get into) is left alone this long; one it left for standing with no driver, or
parked, longer (a bot walking past a parked Warthog took its gun again and again: box games on Sidewinder) */
#define AE_BOTS_VEHICLE_LEFT_TICKS (6 * TICKS_PER_SECOND)
/* LOADING UP: a call (a honk, a bot's human at the wheel) reaches this far; in a fight, this near the seat it walks straight at it; a
bot called stays called this long, a flag's defender too */
#define AE_BOTS_VEHICLE_CALL_REACH 30.0f
#define AE_BOTS_VEHICLE_FIGHT_STRAIGHT 15.0f
#define AE_BOTS_VEHICLE_CALLED_TICKS (30 * TICKS_PER_SECOND)
#define AE_BOTS_VEHICLE_LEFT_IDLE_TICKS (30 * TICKS_PER_SECOND)
/* upright while its up vector's height is above this (about 66 degrees of tilt) */
#define AE_BOTS_VEHICLE_UPRIGHT 0.4f
/* the fight's confidence (ae_bots_combat_rules' scale: 0.5 even) over which the bot is winning */
#define AE_BOTS_VEHICLE_WINNING 0.6f
/* the driving spike: a waypoint this near is passed; a new path every this long; the most it backs off before it
gives up; how long it waits for a teammate bot on its way to the gunner seat */
#define AE_BOTS_VEHICLE_WAYPOINT_REACHED 2.5f
#define AE_BOTS_VEHICLE_DRIVE_PATH_TICKS (3 * TICKS_PER_SECOND)
#define AE_BOTS_VEHICLE_REVERSES 4
#define AE_BOTS_VEHICLE_WAIT_TICKS (4 * TICKS_PER_SECOND)

struct ae_bots_vehicle_bot
{
	long player_index;
	long unit_index;
	/* the seat it goes for (vehicle NONE: none) */
	long target_vehicle_index;
	short target_seat_index;
	short target_seat;
	long target_tick;
	long order_tick;
	long called_tick;
	long near_tick;
	long next_look_tick;
	/* the tick the action button was last held (a press is one tick held, then let go) */
	long press_tick;
	/* the seat it rides (vehicle NONE: on foot) */
	long vehicle_index;
	short seat_index;
	short seat;
	int vehicle_class;
	long seated_tick;
	/* the last tick it had a driver, the vehicle moved, it fought; ticks going away from its goal */
	long driver_tick;
	/* (the UT spike's run-over charge: the last tick it charged) */
	long charge_tick;
	/* (the UT spike: the open ground a vehicle stops at for an indoor goal, the goal it is for) */
	boolean parking_valid, parked_goal;
	real_point3d parking_for;
	long parking_index;
	/* (the UT spike: the last waypoint checked for room, and whether it had it) */
	real_point3d checked_waypoint;
	boolean checked_open;
	long active_tick;
	long fight_tick;
	long away_ticks;
	/* ticks the vehicle has been tipped over past upright (a Ghost banks hard in a turn: a second of it is a flip) */
	long tipped_ticks;
	int leaving;
	long kills;
	/* the vehicle it left (or did not get into), left alone until then */
	long left_vehicle_index;
	long left_until_tick;
	/* driving: the path's age, ticks stalled, backing off until, times backed off, waiting until */
	long drive_path_tick;
	long stall_ticks;
	long reverse_until_tick;
	short reverses;
	long wait_until_tick;
	/* Slayer's driver: the roaming place it drives to, kept until there */
	boolean drive_goal_valid;
	real_point3d drive_goal;
	long drive_goal_surface_index;
	long drive_goal_tick;
};

static struct ae_bots_vehicle_bot ae_bots_vehicle_bots[BOTS_BRAINS];

/* (ae_bots_ut.inc, included after the falls guard) */
static boolean ae_bots_ut_charge(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_vehicle_bot *state,
	struct object_datum *vehicle, real heading, real_point3d *waypoint);
static void ae_bots_ut_gunner(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_vehicle_bot *state);
static boolean ae_bots_ut_spare(struct bots_bot *bot, struct bots_tick *tick, struct object_datum *vehicle,
	real_point3d *waypoint);
static boolean ae_bots_ut_wait_riders(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_vehicle_bot *state,
	struct object_datum *vehicle);
static boolean ae_bots_ut_give_seat(struct bots_bot *bot, struct bots_tick *tick, long vehicle_index, short seat_index);
static void ae_bots_ut_traction(struct object_datum *vehicle, real heading, real *throttle, real *steer);
static void ae_bots_ut_wall_margin(struct bots_bot *bot, struct object_datum *vehicle, long vehicle_index, real heading,
	real_point3d *waypoint);
static boolean ae_bots_ut_on(void);
static void ae_bots_ut_slot(struct bots_bot *bot, long bot_index);
static void ae_bots_ut_flipped(struct bots_bot *bot, long vehicle_index, long now);
static boolean ae_bots_ut_flip_goal(struct bots_bot *bot, real_point3d const *position);
static boolean ae_bots_ut_flip_move(struct bots_bot *bot, long bot_index, struct bots_tick *tick,
	struct ae_bots_vehicle_bot *state);


static long ae_bots_vehicle_index(struct bots_bot *bot)
{
	return (long)(bot - bots_globals.bots);
}

static struct object_datum *ae_bots_vehicle_get(long vehicle_index)
{
	return vehicle_index == NONE ? NULL :
		(struct object_datum *)object_try_and_get_and_verify_type(vehicle_index, _object_mask_vehicle);
}

static real ae_bots_vehicle_speed(struct object_datum *vehicle)
{
	real_vector3d const *v = &vehicle->object.translational_velocity;

	return (real)sqrt(v->i * v->i + v->j * v->j + v->k * v->k) * TICKS_PER_SECOND;
}

static boolean ae_bots_vehicle_upright(struct object_datum *vehicle)
{
	return vehicle->object.up.k > AE_BOTS_VEHICLE_UPRIGHT;
}

static boolean ae_bots_vehicle_alive(struct object_datum *vehicle)
{
	return !TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit);
}

/* the 2d distance from a point to the segment a..b */
static real ae_bots_vehicle_segment_distance(real_point3d const *point, real_point3d const *a, real_point3d const *b)
{
	real dx = b->x - a->x, dy = b->y - a->y;
	real length = dx * dx + dy * dy;
	real t = length > 0.0001f ? ((point->x - a->x) * dx + (point->y - a->y) * dy) / length : 0.0f;
	real_point3d nearest;

	t = PIN(t, 0.0f, 1.0f);
	nearest.x = a->x + dx * t;
	nearest.y = a->y + dy * t;
	nearest.z = 0.0f;
	{
		real ex = point->x - nearest.x, ey = point->y - nearest.y;

		return (real)sqrt(ex * ex + ey * ey);
	}
}

static struct ae_bots_vehicle_bot *ae_bots_vehicle_state(struct bots_bot *bot, long bot_index)
{
	struct ae_bots_vehicle_bot *state = &ae_bots_vehicle_bots[bot_index];

	if (state->player_index != bot->player_index || state->unit_index != bot->unit_index)
	{
		/* (died riding: said, its ride over) */
		if (state->player_index == bot->player_index && state->vehicle_index != NONE)
		{
			platform_log("bots: bot %ld died in the %s after %.1f s", bot_index + 1,
				ae_bots_vehicle_class_name(state->vehicle_class),
				(real)(game_time_get() - state->seated_tick) / TICKS_PER_SECOND);
		}
		csmemset(state, 0, sizeof(*state));
		state->player_index = bot->player_index;
		state->unit_index = bot->unit_index;
		state->target_vehicle_index = NONE;
		state->vehicle_index = NONE;
		state->left_vehicle_index = NONE;
		state->press_tick = NONE;
	}
	return state;
}

/* the objective's behaviour this tick (FIGHT outside the objective modes) */
static short ae_bots_vehicle_behaviour(struct bots_bot *bot, long bot_index)
{
	struct ae_bots_objective_bot const *objective = &ae_bots_objective_bots[bot_index];

	return objective->player_index == bot->player_index ? objective->behaviour : AE_BOTS_FIGHT;
}

static boolean ae_bots_vehicle_carrying(struct bots_bot *bot, long bot_index)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();

	return (ae_bots_objective_bots[bot_index].player_index == bot->player_index && ae_bots_objective_bots[bot_index].in_hands) ||
		(snapshot && snapshot->mode == AE_BOTS_MODE_CTF && ae_bots_objective_player_carries(snapshot, bot->player_index));
}

/* debug.bot_vehicles and the map being played: a network game's name first */
static boolean ae_bots_vehicle_drives_here(void)
{
	char const *name = main_get_multiplayer_map_name();

	return ae_bots_vehicle_may_drive(ae_bots_vehicle_setting(), name && name[0] ? name : main_get_map_name()) != 0;
}

static boolean ae_bots_vehicle_holds_place(short behaviour)
{
	return behaviour == AE_BOTS_CTF_DEFEND || behaviour == AE_BOTS_HILL_HOLD;
}

/* a place held is left for a call */
static boolean ae_bots_vehicle_holds_now(struct ae_bots_vehicle_bot const *state, short behaviour, long now)
{
	return ae_bots_vehicle_holds_place(behaviour) &&
		!(state->called_tick > 0 && now - state->called_tick < AE_BOTS_VEHICLE_CALLED_TICKS);
}

/* whether a human drives the vehicle */
static boolean ae_bots_vehicle_human_drives(long vehicle_index)
{
	struct unit_datum *vehicle = (struct unit_datum *)ae_bots_vehicle_get(vehicle_index);
	struct data_iterator iterator;
	struct player_datum *player;

	if (!vehicle || vehicle->unit.driver_object_index == NONE)
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index == vehicle->unit.driver_object_index)
			return ae_bots_team_bot_of(iterator.datum_index) == NONE;
	}
	return FALSE;
}

/* the vehicle calling this bot: a teammate's honk, or its human at the wheel */
static long ae_bots_vehicle_called(struct bots_bot *bot, struct bots_tick *tick)
{
	long honked = ae_horn_recent(tick->unit->object.owner_team_index, &tick->position, AE_BOTS_VEHICLE_CALL_REACH);
	long human = ae_bots_team_wingman_of(bot);
	struct player_datum const *human_player;
	long vehicle_index;

	if (honked != NONE || human == NONE || (human_player = player_try_and_get(human)) == NULL || !ae_bots_team_alive(human_player))
		return honked;
	vehicle_index = object_get(human_player->unit_index)->object.parent_object_index;
	if (vehicle_index != NONE && ae_bots_vehicle_get(vehicle_index) && ae_bots_vehicle_human_drives(vehicle_index) &&
		bots_distance3d(&object_get(vehicle_index)->object.position, &tick->position) <= AE_BOTS_VEHICLE_CALL_REACH &&
		ae_bots_vehicle_speed(ae_bots_vehicle_get(vehicle_index)) <= AE_BOTS_VEHICLE_ENTER_SPEED * 2.0f)
	{
		return vehicle_index;
	}
	return NONE;
}

/* free teammates on foot nearer the vehicle than this bot */
static int ae_bots_vehicle_nearer_free(struct bots_bot *bot, long bot_index, struct bots_tick *tick, long vehicle_index)
{
	real_point3d const *at = &object_get(vehicle_index)->object.position;
	real mine = bots_distance3d(&tick->position, at);
	int count = 0;
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *mate = &bots_globals.bots[index];
		struct player_datum const *player = mate->player_index != NONE ? player_try_and_get(mate->player_index) : NULL;

		if (index == bot_index || !player || player->team_index != tick->player->team_index || !ae_bots_team_alive(player) ||
			object_get(player->unit_index)->object.parent_object_index != NONE ||
			ae_bots_vehicle_bots[index].target_vehicle_index != NONE)
		{
			continue;
		}
		if (bots_distance3d(&object_get(player->unit_index)->object.position, at) < mine)
			count++;
	}
	(void)bot;
	return count;
}

static int ae_bots_vehicle_driver(struct player_datum *player, struct unit_datum *unit, long vehicle_index);

/* (the log) who drives the vehicle */
static char const *ae_bots_vehicle_driver_name(struct bots_tick *tick, long vehicle_index)
{
	if (ae_bots_vehicle_driver(tick->player, tick->unit, vehicle_index) == AE_BOTS_DRIVER_NONE)
		return "none";
	return ae_bots_vehicle_human_drives(vehicle_index) ? "human" : "bot";
}

/* whether it is winning the fight it is in (its confidence, as the fight by level reckons it) */
static boolean ae_bots_vehicle_winning(struct bots_bot *bot, struct bots_tick *tick)
{
	struct player_datum *target = bot->target_player_index != NONE ? player_try_and_get(bot->target_player_index) : NULL;
	struct object_datum *object;

	if (!tick->fighting || !target || target->unit_index == NONE)
		return FALSE;
	object = object_get(target->unit_index);
	return ae_bots_combat_confidence(tick->object->object.shield_vitality, tick->object->object.body_vitality,
		object->object.shield_vitality, object->object.body_vitality, tick->target_distance,
		bots_weapon_range(tick->weapon_index), tick->weapon_index != NONE ? bots_weapon_loaded(tick->weapon_index) : 1.0f,
		0.0f) >= AE_BOTS_VEHICLE_WINNING;
}

/* whether another bot goes for that seat */
static boolean ae_bots_vehicle_reserved(long bot_index, long vehicle_index, short seat_index)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct ae_bots_vehicle_bot const *other = &ae_bots_vehicle_bots[index];

		if (index != bot_index && other->player_index != NONE && other->player_index == bots_globals.bots[index].player_index &&
			other->target_vehicle_index == vehicle_index && other->target_seat_index == seat_index)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* a teammate bot goes for the vehicle's gunner seat */
static boolean ae_bots_vehicle_gunner_coming(long bot_index, long vehicle_index)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct ae_bots_vehicle_bot const *other = &ae_bots_vehicle_bots[index];

		if (index != bot_index && other->player_index != NONE && other->player_index == bots_globals.bots[index].player_index &&
			other->target_vehicle_index == vehicle_index && other->target_seat == AE_BOTS_SEAT_GUNNER)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* who drives the vehicle, for the bot: no one, a teammate, an enemy */
static int ae_bots_vehicle_driver(struct player_datum *player, struct unit_datum *unit, long vehicle_index)
{
	struct unit_datum *vehicle = (struct unit_datum *)ae_bots_vehicle_get(vehicle_index);
	struct object_datum *driver;

	if (!vehicle || vehicle->unit.driver_object_index == NONE)
		return AE_BOTS_DRIVER_NONE;
	driver = (struct object_datum *)object_try_and_get_and_verify_type(vehicle->unit.driver_object_index, _object_mask_unit);
	if (!driver || driver->object.parent_object_index != vehicle_index)
		return AE_BOTS_DRIVER_NONE;
	(void)player;
	return game_team_is_enemy(unit->object.owner_team_index, driver->object.owner_team_index) ?
		AE_BOTS_DRIVER_ENEMY : AE_BOTS_DRIVER_TEAMMATE;
}

/* ---------- on foot: the seat to go for */

static void ae_bots_vehicle_look(struct bots_bot *bot, long bot_index, struct bots_tick *tick,
	struct ae_bots_vehicle_bot *state)
{
	struct ae_bots_vehicle_situation situation;
	struct object_iterator iterator;
	struct object_datum *object;
	real best_score = 0.0f;
	long best_vehicle = NONE;
	short best_seat_index = NONE, best_seat = AE_BOTS_SEAT_NONE;
	short behaviour = ae_bots_vehicle_behaviour(bot, bot_index);
	boolean goal = bot->goal_kind != _bots_goal_none;

	csmemset(&situation, 0, sizeof(situation));
	situation.fighting = tick->fighting;
	situation.winning = ae_bots_vehicle_winning(bot, tick);
	situation.carrying = ae_bots_vehicle_carrying(bot, bot_index);
	situation.holds_place = ae_bots_vehicle_holds_now(state, behaviour, tick->now);
	/* (PLAYFIGHT's bots never drive: a Warthog runs people over) */
	situation.may_drive = ae_bots_vehicle_drives_here() && !ae_bots_playfight();
	situation.goal_distance = goal ? (float)bots_distance3d(&tick->position, &bot->goal_point) : -1.0f;
	/* (the horn spike: a teammate honked near by: GET IN, an open seat of that vehicle, the gunner's first) */
	{
		long honked = ae_bots_vehicle_called(bot, tick);
		short seat_count, seat_index, pick = NONE, pick_seat = AE_BOTS_SEAT_NONE;
		int open_seats = 0;
		boolean carrying = ae_bots_vehicle_carrying(bot, bot_index);

		if (honked != NONE && state->target_vehicle_index != honked)
		{
			seat_count = ae_bots_vehicle_seat_count(honked);
			for (seat_index = 0; seat_index < seat_count; seat_index++)
			{
				short role = (short)ae_bots_vehicle_seat_role(honked, seat_index);
				long occupant = NONE;

				/* (the flag's carrier: the passenger seat, whoever was going for it, a trigger pull drops the flag) */
				if (role == AE_BOTS_SEAT_NONE || role == AE_BOTS_SEAT_DRIVER || (carrying && role != AE_BOTS_SEAT_PASSENGER) ||
					(!carrying && ae_bots_vehicle_reserved(bot_index, honked, seat_index)) ||
					!unit_can_enter_seat(tick->player->unit_index, honked, seat_index, &occupant))
				{
					continue;
				}
				open_seats++;
				if (pick == NONE || (role == AE_BOTS_SEAT_GUNNER && pick_seat != AE_BOTS_SEAT_GUNNER))
				{
					pick = seat_index;
					pick_seat = role;
				}
			}
			/* (its human's squad and the carrier first, then the nearest) */
			if (pick != NONE && (carrying || ae_bots_team_wingman_of(bot) != NONE ||
				ae_bots_vehicle_called_in_turn(ae_bots_vehicle_nearer_free(bot, bot_index, tick, honked), open_seats)))
			{
				state->target_vehicle_index = honked;
				state->target_seat_index = pick;
				state->target_seat = pick_seat;
				state->target_tick = tick->now;
				state->called_tick = tick->now;
				state->near_tick = NONE;
				bot->path.valid = FALSE;
				if (ae_bots_events_on())
					platform_log("bots: ev=horn_get_in bot=%ld seat %d", bot_index + 1, (int)pick);
				return;
			}
		}
	}
	if (situation.holds_place || (situation.fighting && situation.winning))
		return;
	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		int vehicle_class;
		short seat_count, seat_index;
		boolean enemy_aboard = FALSE;
		real speed;

		if (object->object.parent_object_index != NONE ||
			bots_distance3d(&object->object.position, &tick->position) > AE_BOTS_VEHICLE_NOTICE_DRIVE + 4.0f)
		{
			continue;
		}
		vehicle_class = ae_bots_vehicle_class(iterator.index);
		seat_count = ae_bots_vehicle_seat_count(iterator.index);
		speed = ae_bots_vehicle_speed(object);
		for (seat_index = 0; seat_index < seat_count && !enemy_aboard; seat_index++)
		{
			struct ae_bots_seat_offer offer;
			real_point3d entrance, seat_point;
			long occupant = NONE;
			float score;

			csmemset(&offer, 0, sizeof(offer));
			offer.seat = ae_bots_vehicle_seat_role(iterator.index, seat_index);
			if (offer.seat == AE_BOTS_SEAT_NONE)
				continue;
			if (!unit_can_enter_seat(tick->player->unit_index, iterator.index, seat_index, &occupant))
			{
				/* (taken; no one in the seat yet it cannot be entered: an enemy rides it) */
				if (occupant == NONE)
					enemy_aboard = TRUE;
				continue;
			}
			if (!unit_get_seat_entrance_point(tick->player->unit_index, iterator.index, seat_index, &entrance, &seat_point, NULL))
				continue;
			offer.vehicle_class = vehicle_class;
			offer.distance = (float)bots_distance3d(&entrance, &tick->position);
			offer.off_route = goal ? (float)ae_bots_vehicle_segment_distance(&entrance, &tick->position, &bot->goal_point) : -1.0f;
			offer.detour = goal ? (float)(bots_distance3d(&tick->position, &entrance) + bots_distance3d(&entrance, &bot->goal_point) -
				bots_distance3d(&tick->position, &bot->goal_point)) : -1.0f;
			offer.speed = (float)speed;
			offer.alive = ae_bots_vehicle_alive(object);
			offer.upright = ae_bots_vehicle_upright(object);
			offer.driver = ae_bots_vehicle_driver(tick->player, tick->unit, iterator.index);
			offer.reserved = ae_bots_vehicle_reserved(bot_index, iterator.index, seat_index);
			offer.recently_left = iterator.index == state->left_vehicle_index && tick->now < state->left_until_tick;
			score = ae_bots_vehicle_seat_score(&situation, &offer);
			if (ae_bots_trace_level() > 0)
			{
				platform_log("bots: trace %ld bot %ld vehicle %lx %s seat %d %s: distance %.1f off its way %.1f detour %.1f speed %.1f "
					"alive %d upright %d driver %d reserved %d left %d goal %.1f fighting %d winning %d carrying %d holds %d: "
					"score %.2f", tick->now, bot_index + 1, iterator.index, ae_bots_vehicle_class_name(vehicle_class),
					(int)seat_index, ae_bots_vehicle_seat_name(offer.seat), offer.distance, offer.off_route, offer.detour, offer.speed,
					offer.alive, offer.upright, offer.driver, offer.reserved, offer.recently_left, situation.goal_distance,
					situation.fighting, situation.winning, situation.carrying, situation.holds_place, score);
			}
			if (score > best_score)
			{
				best_score = score;
				best_vehicle = iterator.index;
				best_seat_index = seat_index;
				best_seat = (short)offer.seat;
			}
		}
	}
	/* (the UT spike, Snow Grove CTF 10-26: "still struggling loading up these warthogs at the start": gunners got in
	first and waited 12 s for drivers who went elsewhere: a vehicle with no driver and no bot going for its wheel: the
	wheel first, when it may drive) */
	if (best_vehicle != NONE && best_seat != AE_BOTS_SEAT_DRIVER && situation.may_drive && ae_bots_ut_on() &&
		ae_bots_vehicle_driver(tick->player, tick->unit, best_vehicle) == AE_BOTS_DRIVER_NONE)
	{
		short seat_count = ae_bots_vehicle_seat_count(best_vehicle), seat_index;

		for (seat_index = 0; seat_index < seat_count; seat_index++)
		{
			long occupant = NONE;

			if (ae_bots_vehicle_seat_role(best_vehicle, seat_index) == AE_BOTS_SEAT_DRIVER &&
				!ae_bots_vehicle_reserved(bot_index, best_vehicle, seat_index) &&
				unit_can_enter_seat(tick->player->unit_index, best_vehicle, seat_index, &occupant))
			{
				best_seat_index = seat_index;
				best_seat = AE_BOTS_SEAT_DRIVER;
				break;
			}
		}
	}
	if (best_vehicle != NONE)
	{
		state->target_vehicle_index = best_vehicle;
		state->target_seat_index = best_seat_index;
		state->target_seat = best_seat;
		state->target_tick = tick->now;
		state->near_tick = NONE;
		bot->path.valid = FALSE;
	}
}

static void ae_bots_vehicle_drop_target(struct bots_bot *bot, struct ae_bots_vehicle_bot *state, long now)
{
	if (state->target_vehicle_index == NONE)
		return;
	state->left_vehicle_index = state->target_vehicle_index;
	state->left_until_tick = now + AE_BOTS_VEHICLE_LEFT_TICKS;
	state->target_vehicle_index = NONE;
	if (bot->goal_kind == _bots_goal_roam)
		bot->goal_kind = _bots_goal_none;
}

/* the seat it goes for is still worth going for; its entrance */
static boolean ae_bots_vehicle_target_valid(struct bots_bot *bot, long bot_index, struct bots_tick *tick,
	struct ae_bots_vehicle_bot *state, real_point3d *entrance)
{
	struct object_datum *vehicle = ae_bots_vehicle_get(state->target_vehicle_index);
	real_point3d seat_point;
	long occupant = NONE;

	if (!vehicle || !ae_bots_vehicle_alive(vehicle) || !ae_bots_vehicle_upright(vehicle) ||
		ae_bots_vehicle_speed(vehicle) > AE_BOTS_VEHICLE_ENTER_SPEED * 2.0f ||
		tick->now - state->target_tick > AE_BOTS_VEHICLE_GO_TICKS ||
		(state->near_tick != NONE && tick->now - state->near_tick > AE_BOTS_VEHICLE_BOARD_TICKS) ||
		ae_bots_vehicle_holds_now(state, ae_bots_vehicle_behaviour(bot, bot_index), tick->now) ||
		(state->target_seat != AE_BOTS_SEAT_PASSENGER && ae_bots_vehicle_carrying(bot, bot_index)) ||
		!unit_can_enter_seat(tick->player->unit_index, state->target_vehicle_index, state->target_seat_index, &occupant) ||
		!unit_get_seat_entrance_point(tick->player->unit_index, state->target_vehicle_index, state->target_seat_index, entrance,
			&seat_point, NULL))
	{
		return FALSE;
	}
	/* (a passenger's ride: only while a teammate drives it) */
	if (state->target_seat == AE_BOTS_SEAT_PASSENGER &&
		ae_bots_vehicle_driver(tick->player, tick->unit, state->target_vehicle_index) != AE_BOTS_DRIVER_TEAMMATE)
	{
		return FALSE;
	}
	return TRUE;
}

/* (the UT spike, Sidewinder 10-25) where a vehicle may drive: a goal out in the open, a point on the way under a roof
only in a through hall (rules below, owner's words from games 3 and 4); HALO_BOTS_UT=0 keeps the old goals */
static boolean ae_bots_vehicle_open_ground(real_point3d const *point, boolean goal)
{
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit);
	struct collision_result collision;
	real_point3d start = *point;
	real_vector3d vector = { 0.0f, 0.0f, 8.0f };
	unsigned walls = 0, count = 0;
	boolean roof;
	long side;
	char const *ut = getenv("HALO_BOTS_UT");

	if (ut && ut[0] == '0')
		return TRUE;
	/* (a vehicle's own probe: a roof within 8 units, walls within 6 at a hood's height, eight sides) */
	start.z += 0.1f;
	roof = collision_test_vector(flags, &start, &vector, NONE, &collision) && collision.type == _collision_result_structure;
	start.z = point->z + 0.6f;
	for (side = 0; side < 8; side++)
	{
		real angle = (real)side * BOTS_PI * 0.25f;

		vector.i = (real)cos(angle) * 6.0f;
		vector.j = (real)sin(angle) * 6.0f;
		vector.k = 0.0f;
		if (collision_test_vector(flags, &start, &vector, NONE, &collision))
		{
			walls |= 1u << side;
			count++;
		}
	}
	/* (a goal: open sky, at most four walls near (owner, Sidewinder game 3: "straight into the interior instead of
	driving around the big space outside")) */
	if (goal)
		return !roof && count <= 4;
	/* (under a roof only a through hall: no walls near, or one opposite pair, a tunnel's sides (owner, Sidewinder game
	4: a warthog driven into a base's barrier: "only in the one middle hall that can cross the map faster")) */
	if (roof)
		return count == 0 || (count == 2 && ((walls >> 4) & 0x0F) == (walls & 0x0F));
	return count <= 6;
}

/* (the UT spike) a vehicle's stop for an indoor goal: the open ground nearest it within 30 units (cached while the goal
stays put); FALSE when the goal is open ground itself, or none is near */
static boolean ae_bots_vehicle_parking(struct ae_bots_vehicle_bot *state, real_point3d const *goal, real_point3d *stop,
	long *surface_index)
{
	long index, best = NONE;
	real best_distance = 30.0f;

	if (state->parking_valid && bots_distance3d(&state->parking_for, goal) < 3.0f)
	{
		if (state->parking_index == NONE)
			return FALSE;
		*stop = bots_globals.roam_points[state->parking_index];
		*surface_index = bots_globals.roam_surfaces[state->parking_index];
		return TRUE;
	}
	state->parking_valid = TRUE;
	state->parking_for = *goal;
	state->parking_index = NONE;
	{
		char const *ut = getenv("HALO_BOTS_UT");

		if ((ut && ut[0] == '0') || ae_bots_vehicle_open_ground(goal, TRUE))
			return FALSE;
	}
	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real distance = bots_distance3d(&bots_globals.roam_points[index], goal);

		if (distance < best_distance && ae_bots_vehicle_open_ground(&bots_globals.roam_points[index], TRUE))
		{
			best_distance = distance;
			best = index;
		}
	}
	state->parking_index = best;
	if (best == NONE)
		return FALSE;
	*stop = bots_globals.roam_points[best];
	*surface_index = bots_globals.roam_surfaces[best];
	return TRUE;
}

boolean ae_bots_vehicle_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting)
{
	long bot_index = ae_bots_vehicle_index(bot);
	struct ae_bots_vehicle_bot *state;
	struct object_datum *vehicle;
	real_point3d entrance, ground, seat_point;
	long surface_index;

	(void)player_index;
	(void)position;
	(void)fighting;
	/* (an Infection zombie fights on foot: infection.c's seats refuse it) */
	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !ae_bots_vehicle_setting() || ae_bots_zombie(bot->unit_index))
		return FALSE;
	state = ae_bots_vehicle_state(bot, bot_index);
	/* (Slayer's driver: a roaming place far off, kept until it is there: a goal that changed each time an enemy was
	seen or a nearer place came up had it turning in circles) */
	if (state->vehicle_index != NONE && state->seat == AE_BOTS_SEAT_DRIVER && bot->unit_index != NONE &&
		object_get(bot->unit_index)->object.parent_object_index == state->vehicle_index &&
		ae_bots_vehicle_behaviour(bot, bot_index) == AE_BOTS_FIGHT)
	{
		long now = game_time_get();

		if (!state->drive_goal_valid || bots_distance3d(&state->drive_goal, position) < 8.0f ||
			now - state->drive_goal_tick > 40 * TICKS_PER_SECOND)
		{
			short tries;

			state->drive_goal_valid = FALSE;
			for (tries = 0; tries < 6; tries++)
			{
				bots_pick_roam_goal(bot, position);
				if (bot->goal_kind == _bots_goal_roam && bots_distance3d(&bot->goal_point, position) >= AE_BOTS_VEHICLE_DRIVE_GOAL &&
					ae_bots_vehicle_open_ground(&bot->goal_point, TRUE))
				{
					break;
				}
			}
			if (bot->goal_kind == _bots_goal_roam && ae_bots_vehicle_open_ground(&bot->goal_point, TRUE))
			{
				state->drive_goal_valid = TRUE;
				state->drive_goal = bot->goal_point;
				state->drive_goal_surface_index = bot->goal_surface_index;
				state->drive_goal_tick = now;
			}
		}
		if (state->drive_goal_valid)
			bots_set_goal(bot, _bots_goal_roam, &state->drive_goal, state->drive_goal_surface_index);
		return state->drive_goal_valid;
	}
	/* (a rider: its objective's goal (the flag, the hill...), where its driver goes or it gets out; never bots.c's
	weapon close by, which made a rider 'there' at once; Slayer's gunner and passenger have none) */
	if (state->vehicle_index != NONE && bot->unit_index != NONE &&
		object_get(bot->unit_index)->object.parent_object_index == state->vehicle_index)
	{
		struct ae_bots_objective_bot const *objective = &ae_bots_objective_bots[bot_index];

		if (ae_bots_vehicle_behaviour(bot, bot_index) != AE_BOTS_FIGHT && objective->target_valid)
		{
			real_point3d ground;
			long surface_index = bots_surface_below(&objective->target, &ground);

			if (surface_index == NONE)
				ground = objective->target;
			/* (the UT spike, Snow Grove CTF 10-26: "still trying to drive in the interior": the flag sits inside a
			base: the vehicle stops at the open ground nearest it (within 30 units) and all get out there) */
			if (ae_bots_vehicle_parking(state, &ground, &ground, &surface_index))
				state->parked_goal = TRUE;
			bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
		}
		else if (state->seat != AE_BOTS_SEAT_DRIVER)
			bot->goal_kind = _bots_goal_none;
		else
			return FALSE;
		return TRUE;
	}
	if (state->target_vehicle_index == NONE && state->vehicle_index == NONE && bot->unit_index != NONE &&
		object_get(bot->unit_index)->object.parent_object_index == NONE && ae_bots_ut_flip_goal(bot, position))
	{
		return TRUE;
	}
	if (state->target_vehicle_index == NONE || state->vehicle_index != NONE)
		return FALSE;
	vehicle = ae_bots_vehicle_get(state->target_vehicle_index);
	if (!vehicle || bot->unit_index == NONE || object_get(bot->unit_index)->object.parent_object_index != NONE ||
		!unit_get_seat_entrance_point(bot->unit_index, state->target_vehicle_index, state->target_seat_index, &entrance,
			&seat_point, NULL))
	{
		return FALSE;
	}
	/* (the ground under the entrance, as the walkable surfaces know it; a vehicle's surface beside it) */
	entrance.z += 0.3f;
	surface_index = bots_surface_below(&entrance, &ground);
	if (surface_index == NONE)
		ground = entrance;
	bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
	return TRUE;
}

/* the last steps to the seat's entrance, and the action button there */
static void ae_bots_vehicle_board(struct bots_bot *bot, long bot_index, struct bots_tick *tick,
	struct ae_bots_vehicle_bot *state)
{
	real_point3d entrance;
	real distance;

	if (!ae_bots_vehicle_target_valid(bot, bot_index, tick, state, &entrance))
	{
		if (ae_bots_events_on())
		{
			struct object_datum *vehicle = ae_bots_vehicle_get(state->target_vehicle_index);
			long occupant = NONE;
			char extra[120];

			snprintf(extra, sizeof(extra), "walked=%.1f near=%.1f speed=%.1f alive=%d up=%d holds=%d carrying=%d free=%d",
				(double)(tick->now - state->order_tick) / TICKS_PER_SECOND,
				state->near_tick != NONE ? (double)(tick->now - state->near_tick) / TICKS_PER_SECOND : -1.0,
				vehicle ? (double)ae_bots_vehicle_speed(vehicle) : -1.0, vehicle ? ae_bots_vehicle_alive(vehicle) != 0 : 0, vehicle ? ae_bots_vehicle_upright(vehicle) != 0 : 0,
				ae_bots_vehicle_holds_now(state, ae_bots_vehicle_behaviour(bot, bot_index), tick->now) != 0, ae_bots_vehicle_carrying(bot, bot_index) != 0,
				vehicle ? unit_can_enter_seat(tick->player->unit_index, state->target_vehicle_index, state->target_seat_index, &occupant) != 0 : 0);
			ae_bots_event_line(bot_index, tick->now, "seat_lost", -1, -1.0f, extra);
		}
		ae_bots_vehicle_drop_target(bot, state, tick->now);
		return;
	}
	distance = bots_distance2d(&entrance, &tick->position);
	if (distance > AE_BOTS_VEHICLE_APPROACH)
	{
		/* (while it fights, it still makes for the seat: losing, a gun's seat may save it) */
		real direction;

		/* (a fight sets no goal: a path for somewhere else is no way to the seat; near, straight at it) */
		if (tick->fighting && distance <= AE_BOTS_VEHICLE_FIGHT_STRAIGHT &&
			(bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &entrance) > 4.0f))
		{
			tick->move_direction = (real)atan2(entrance.y - tick->position.y, entrance.x - tick->position.x);
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
		}
		else if (tick->fighting && bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
		{
			tick->move_direction = direction;
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
		}
		return;
	}
	if (state->near_tick == NONE)
		state->near_tick = tick->now;
	/* (the game offers the seat it wants: still, the action button, a tick held, a tick let go) */
	if (tick->player->action_result == _player_action_result_enter_vehicle &&
		tick->player->action_object_index == state->target_vehicle_index &&
		tick->player->action_seat_index == state->target_seat_index)
	{
		tick->moving = FALSE;
		if (state->press_tick != tick->now - 1)
		{
			tick->flags |= FLAG(_unit_control_action_bit);
			state->press_tick = tick->now;
		}
		tick->flags &= ~FLAG(_unit_control_swap_weapons_bit);
		return;
	}
	/* (on to the entrance, slower near it) */
	if (distance > AE_BOTS_VEHICLE_AT_ENTRANCE * 0.5f)
	{
		tick->move_direction = (real)atan2(entrance.y - tick->position.y, entrance.x - tick->position.x);
		tick->moving = TRUE;
		tick->throttle_scale = distance < 1.0f ? 0.4f : 0.7f;
	}
	else
		tick->moving = FALSE;
}

/* ---------- in a seat */

/* the gunner's or the passenger's look while it does not fight: about the way the vehicle goes, a sweep to each side */
static void ae_bots_vehicle_look_about(struct bots_bot *bot, struct bots_tick *tick, struct object_datum *vehicle)
{
	real heading = (real)atan2(vehicle->object.forward.j, vehicle->object.forward.i);
	real sweep = (real)sin((real)tick->now * 0.04f + (real)(bot->player_index & 0xFF)) * 1.3f;
	real off = bots_angle_difference(heading + sweep, bot->yaw);
	real turn = 6.0f * BOTS_DEGREES;

	bot->yaw += PIN(off, -turn, turn);
	bot->pitch += PIN(-bot->pitch, -0.05f, 0.05f);
}

/* the driving spike: steering for the route's next point, the throttle from the rules */
static void ae_bots_vehicle_drive_tick(struct bots_bot *bot, long bot_index, struct bots_tick *tick,
	struct ae_bots_vehicle_bot *state, struct object_datum *vehicle)
{
	struct ae_bots_drive_input input;
	struct ae_bots_drive_output output;
	real heading = (real)atan2(vehicle->object.forward.j, vehicle->object.forward.i);
	real speed = ae_bots_vehicle_speed(vehicle);
	real forward_speed = (vehicle->object.translational_velocity.i * vehicle->object.forward.i +
		vehicle->object.translational_velocity.j * vehicle->object.forward.j) * TICKS_PER_SECOND;
	real_point3d const *position = &vehicle->object.position;
	real_point3d waypoint = bot->goal_point, after = bot->goal_point;
	real fight_yaw = bot->yaw, desired;
	boolean have_after = FALSE, spare;

	csmemset(&input, 0, sizeof(input));
	if (bot->goal_kind == _bots_goal_none)
	{
		tick->moving = FALSE;
		return;
	}
	/* (the UT spike: a teammate on foot coming to a seat: stopped for it) */
	if (ae_bots_ut_wait_riders(bot, tick, state, vehicle))
	{
		tick->moving = FALSE;
		return;
	}
	/* (a teammate bot on its way to the gunner seat: a moment for it) */
	if (ae_bots_vehicle_gunner_coming(bot_index, state->vehicle_index) && tick->now < state->wait_until_tick)
	{
		tick->moving = FALSE;
		return;
	}
	/* (the route from where the vehicle is: the walkable surfaces' path, searched from it, its points as waypoints) */
	if (!bot->path.valid || tick->now - state->drive_path_tick > AE_BOTS_VEHICLE_DRIVE_PATH_TICKS)
	{
		if (tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
		{
			bots_search(bot, state->vehicle_index);
			state->drive_path_tick = tick->now;
		}
	}
	if (bot->path.valid)
	{
		while (bot->path.step_index < bot->path.step_count &&
			bots_distance2d(&bot->path.steps[(long)bot->path.step_index].point, position) < AE_BOTS_VEHICLE_WAYPOINT_REACHED)
		{
			bot->path.step_index++;
		}
		if (bot->path.step_index < bot->path.step_count)
		{
			waypoint = bot->path.steps[(long)bot->path.step_index].point;
			if (bot->path.step_index + 1 < bot->path.step_count)
			{
				after = bot->path.steps[(long)bot->path.step_index + 1].point;
				have_after = TRUE;
			}
		}
		else
			bot->path.valid = FALSE;
	}
	/* (the UT spike: an enemy on foot ahead, driven at and past, not to) */
	if (ae_bots_ut_charge(bot, tick, state, vehicle, heading, &waypoint))
		have_after = FALSE;
	/* (the UT spike: off the walls) */
	ae_bots_ut_wall_margin(bot, vehicle, state->vehicle_index, heading, &waypoint);
	/* (the UT spike: never through a teammate on foot) */
	spare = ae_bots_ut_spare(bot, tick, vehicle, &waypoint);
	if (spare)
		have_after = FALSE;
	/* (the UT spike: the route's next point indoors: a new goal, another way round) */
	/* (the check once a waypoint (review: nine rays per driver per tick)) */
	if (bots_distance2d(&waypoint, &state->checked_waypoint) > 1.0f)
	{
		state->checked_waypoint = waypoint;
		state->checked_open = ae_bots_vehicle_open_ground(&waypoint, FALSE);
	}
	if (bot->path.valid && tick->now - state->drive_goal_tick > 3 * TICKS_PER_SECOND && !state->checked_open)
	{
		state->drive_goal_valid = FALSE;
		bot->path.valid = FALSE;
		bot->goal_kind = _bots_goal_none;
		state->drive_goal_tick = tick->now;
		tick->moving = FALSE;
		return;
	}
	desired = (real)atan2(waypoint.y - position->y, waypoint.x - position->x);
	input.heading_error = (float)bots_angle_difference(desired, heading);
	input.waypoint_distance = (float)bots_distance2d(&waypoint, position);
	input.next_turn = have_after ? (float)bots_angle_difference((real)atan2(after.y - waypoint.y, after.x - waypoint.x), desired) : 0.0f;
	input.speed = (float)forward_speed;
	/* (stalled: the throttle open and the vehicle still a while) */
	if (speed < 0.5f && tick->now >= state->reverse_until_tick)
		state->stall_ticks++;
	else
		state->stall_ticks = 0;
	input.stalled = (float)state->stall_ticks / TICKS_PER_SECOND;
	input.reversing = tick->now < state->reverse_until_tick ? (float)(state->reverse_until_tick - tick->now) / TICKS_PER_SECOND : 0.0f;
	ae_bots_vehicle_drive(&input, &output);
	if (ae_bots_trace_level() > 0 && tick->now % TICKS_PER_SECOND == bot_index)
	{
		platform_log("bots: trace %ld bot %ld drives at (%.1f %.1f %.1f) speed %.1f heading %.0f to waypoint (%.1f %.1f) "
			"%.1f off, error %.0f, turn after %.0f, goal (%.1f %.1f) %.1f off, path %s %d/%d, stalled %.1f, reversing %.1f "
			"(%d times): throttle %.2f", tick->now, bot_index + 1, position->x, position->y, position->z, forward_speed,
			heading / BOTS_DEGREES, waypoint.x, waypoint.y, input.waypoint_distance, input.heading_error / BOTS_DEGREES,
			input.next_turn / BOTS_DEGREES, bot->goal_point.x, bot->goal_point.y, bots_distance2d(&bot->goal_point, position),
			bot->path.valid ? "yes" : "no", (int)bot->path.step_index, (int)bot->path.step_count, input.stalled,
			input.reversing, (int)state->reverses, output.throttle);
	}
	if (output.reverse_for > 0.0f)
	{
		state->reverse_until_tick = tick->now + (long)(output.reverse_for * TICKS_PER_SECOND);
		state->stall_ticks = 0;
		state->reverses++;
		bot->path.valid = FALSE;
	}
	/* (the UT spike: in a slide, gentler) */
	{
		real throttle = output.throttle, steer = output.steer;

		ae_bots_ut_traction(vehicle, heading, &throttle, &steer);
		output.throttle = (float)throttle;
		output.steer = (float)steer;
	}
	/* (its facing is its steering, a third-person seat's aim: the vehicle turns towards it) */
	bot->yaw = bots_angle_difference(desired + output.steer, 0.0f);
	bot->pitch = 0.0f;
	tick->move_direction = bot->yaw;
	tick->moving = output.throttle != 0.0f;
	tick->throttle_scale = spare ? PIN(output.throttle, -0.5f, 0.5f) : output.throttle;
	/* (a Ghost's guns point where it goes: it fires only at a target ahead) */
	if (fabs(bots_angle_difference(fight_yaw, bot->yaw)) > 12.0f * BOTS_DEGREES)
		tick->flags &= ~(FLAG(_unit_control_weapon_primary_trigger_bit) | FLAG(_unit_control_weapon_secondary_trigger_bit));
	(void)speed;
}

static void ae_bots_vehicle_ride(struct bots_bot *bot, long bot_index, struct bots_tick *tick,
	struct ae_bots_vehicle_bot *state, long vehicle_index)
{
	struct object_datum *vehicle = ae_bots_vehicle_get(vehicle_index);
	struct unit_datum *vehicle_unit = (struct unit_datum *)vehicle;
	struct ae_bots_ride ride;
	short behaviour = ae_bots_vehicle_behaviour(bot, bot_index);
	boolean has_driver, parked;
	real speed;
	int reason;

	if (!vehicle)
		return;
	/* (just in: its seat) */
	if (state->vehicle_index != vehicle_index)
	{
		state->vehicle_index = vehicle_index;
		state->seat_index = tick->unit->unit.parent_seat_index;
		state->seat = (short)ae_bots_vehicle_seat_role(vehicle_index, state->seat_index);
		if (state->seat == AE_BOTS_SEAT_NONE)
			state->seat = AE_BOTS_SEAT_PASSENGER;
		state->vehicle_class = ae_bots_vehicle_class(vehicle_index);
		if (ae_bots_events_on())
		{
			char extra[80];

			snprintf(extra, sizeof(extra), "seat=%s driver=%s wait=%.1f", ae_bots_vehicle_seat_name(state->seat), ae_bots_vehicle_driver_name(tick, vehicle_index),
				state->order_tick > 0 ? (double)(tick->now - state->order_tick) / TICKS_PER_SECOND : -1.0);
			ae_bots_event_line(bot_index, tick->now, "seat", -1, -1.0f, extra);
		}
		state->order_tick = 0;
		state->seated_tick = state->driver_tick = state->active_tick = tick->now;
		state->fight_tick = tick->now;
		state->away_ticks = 0;
		state->tipped_ticks = 0;
		state->leaving = AE_BOTS_LEAVE_STAY;
		state->kills = tick->player->statistics.kills[0];
		state->stall_ticks = 0;
		state->reverses = 0;
		state->reverse_until_tick = 0;
		state->wait_until_tick = tick->now + AE_BOTS_VEHICLE_WAIT_TICKS;
		state->drive_path_tick = 0;
		state->drive_goal_valid = FALSE;
		state->target_vehicle_index = NONE;
		/* (the seat's entrance was its goal: a goal of its own again, bots.c's or the objective's) */
		bot->goal_kind = _bots_goal_none;
		bot->path.valid = FALSE;
		ae_bots_vehicle_count_entry(bot_index, state->vehicle_class, state->seat);
	}
	speed = ae_bots_vehicle_speed(vehicle);
	has_driver = vehicle_unit->unit.driver_object_index != NONE;
	if (has_driver || state->seat == AE_BOTS_SEAT_DRIVER)
		state->driver_tick = tick->now;
	if (speed > AE_BOTS_VEHICLE_ACTIVE_SPEED)
		state->active_tick = tick->now;
	if (tick->fighting)
		state->fight_tick = tick->now;
	parked = tick->now - state->active_tick > TICKS_PER_SECOND;
	/* (away from its goal: the vehicle goes the other way) */
	if (bot->goal_kind != _bots_goal_none && speed > AE_BOTS_VEHICLE_ACTIVE_SPEED &&
		(vehicle->object.translational_velocity.i * (bot->goal_point.x - tick->position.x) +
			vehicle->object.translational_velocity.j * (bot->goal_point.y - tick->position.y)) < 0.0f)
	{
		state->away_ticks++;
	}
	else
		state->away_ticks = 0;
	if (tick->player->statistics.kills[0] > state->kills)
	{
		long kill;

		for (kill = state->kills; kill < tick->player->statistics.kills[0]; kill++)
			ae_bots_vehicle_count_kill(bot_index, state->seat);
	}
	state->kills = tick->player->statistics.kills[0];
	ae_bots_vehicle_count_tick(bot_index, state->seat, parked, tick->fighting, speed);

	csmemset(&ride, 0, sizeof(ride));
	ride.seat = state->seat;
	ride.alive = ae_bots_vehicle_alive(vehicle);
	state->tipped_ticks = ae_bots_vehicle_upright(vehicle) ? 0 : state->tipped_ticks + 1;
	ride.upright = state->tipped_ticks < TICKS_PER_SECOND;
	ride.seated = (float)(tick->now - state->seated_tick) / TICKS_PER_SECOND;
	ride.without_driver = (float)(tick->now - state->driver_tick) / TICKS_PER_SECOND;
	ride.parked = (float)(tick->now - state->active_tick) / TICKS_PER_SECOND;
	ride.since_fight = (float)(tick->now - state->fight_tick) / TICKS_PER_SECOND;
	ride.going_away = (float)state->away_ticks / TICKS_PER_SECOND;
	/* (the objective modes' goal; Slayer's gunner fights wherever it is driven) */
	ride.goal_distance = behaviour != AE_BOTS_FIGHT && bot->goal_kind != _bots_goal_none ?
		(float)bots_distance3d(&tick->position, &bot->goal_point) : -1.0f;
	ride.holds_place = ae_bots_vehicle_holds_now(state, behaviour, tick->now);
	/* (a human at the wheel: it goes where they go, however long they wait to load up; a passenger on its way to the flag gets out for it
	at the flag; the gunner stays on the gun) */
	if (state->seat != AE_BOTS_SEAT_DRIVER && ae_bots_vehicle_human_drives(state->vehicle_index))
	{
		if (!(state->seat == AE_BOTS_SEAT_PASSENGER && behaviour == AE_BOTS_CTF_TAKE))
			ride.goal_distance = -1.0f;
		ride.parked = 0.0f;
	}
	ride.stationary = state->vehicle_class == AE_BOTS_VEHICLE_TURRET;
	ride.patient_gunner = ae_bots_ut_on();
	reason = ae_bots_vehicle_leave(&ride);
	/* (the driver: at its objective's goal it gets out (the rules); at a roaming place, on to the next one; stuck
	past backing off, out) */
	if (reason == AE_BOTS_LEAVE_STAY && state->seat == AE_BOTS_SEAT_DRIVER)
	{
		if (state->reverses > AE_BOTS_VEHICLE_REVERSES)
			reason = AE_BOTS_LEAVE_STUCK;
	}
	/* (the UT spike: a teammate held the action button at its seat: its seat now) */
	if (reason == AE_BOTS_LEAVE_STAY && ae_bots_ut_give_seat(bot, tick, vehicle_index, state->seat_index))
		reason = AE_BOTS_LEAVE_HOLD;
	/* (no driving with the spike off: a driver's seat taken some other way is left) */
	if (state->seat == AE_BOTS_SEAT_DRIVER && !ae_bots_vehicle_drives_here() && reason == AE_BOTS_LEAVE_STAY && ride.seated >= 1.5f)
		reason = AE_BOTS_LEAVE_HOLD;

	/* a rider neither swaps, reloads, throws nor strikes (the chain gun has no grenade; a seat no blow) */
	tick->flags &= ~(FLAG(_unit_control_swap_weapons_bit) | FLAG(_unit_control_throw_grenade_bit) |
		FLAG(_unit_control_use_equipment_bit) | FLAG(_unit_control_jump_bit) | FLAG(_unit_control_crouch_modifier_bit));
	tick->action->desired_weapon_index = NONE;
	if (state->seat == AE_BOTS_SEAT_DRIVER && ae_bots_vehicle_drives_here() && reason == AE_BOTS_LEAVE_STAY)
		ae_bots_vehicle_drive_tick(bot, bot_index, tick, state, vehicle);
	else
	{
		tick->moving = FALSE;
		if (!tick->fighting)
			ae_bots_vehicle_look_about(bot, tick, vehicle);
		else if (state->seat == AE_BOTS_SEAT_GUNNER)
			ae_bots_ut_gunner(bot, tick, state);
	}
	if (reason != AE_BOTS_LEAVE_STAY)
	{
		if (state->leaving == AE_BOTS_LEAVE_STAY)
		{
			state->leaving = reason;
			ae_bots_vehicle_count_exit(bot_index, state->vehicle_class, reason, ride.seated);
			if (ae_bots_events_on())
			{
				char extra[140];

				snprintf(extra, sizeof(extra), "reason=%s seat=%s driver=%s seated=%.1f goal=%.1f parked=%.1f no_driver=%.1f going_away=%.1f",
					ae_bots_vehicle_leave_name(reason), ae_bots_vehicle_seat_name(state->seat), ae_bots_vehicle_driver_name(tick, state->vehicle_index),
					(double)ride.seated, (double)ride.goal_distance, (double)ride.parked, (double)ride.without_driver, (double)ride.going_away);
				ae_bots_event_line(bot_index, tick->now, "seat_leave", -1, -1.0f, extra);
			}
		}
		tick->moving = FALSE;
		if (state->press_tick != tick->now - 1)
		{
			tick->flags |= FLAG(_unit_control_action_bit);
			state->press_tick = tick->now;
		}
	}
}

void ae_bots_vehicle_origin(struct bots_tick *tick)
{
	if (tick->object && tick->object->object.parent_object_index != NONE)
		ae_bots_vehicle_world_point(tick->player->unit_index, &tick->position);
}

void ae_bots_vehicle_weapon(struct bots_bot *bot, struct bots_tick *tick)
{
	struct unit_datum *vehicle;
	long weapon_index;

	(void)bot;
	if (!tick->object || tick->object->object.parent_object_index == NONE)
		return;
	vehicle = (struct unit_datum *)ae_bots_vehicle_get(tick->object->object.parent_object_index);
	/* (its seat drives the vehicle's gun: the gunner's, a Ghost's driver) */
	if (!vehicle || vehicle->unit.gunner_object_index != tick->player->unit_index)
		return;
	weapon_index = bots_current_weapon(vehicle);
	if (weapon_index != NONE)
		tick->weapon_index = weapon_index;
}

void ae_bots_vehicle_move(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_bots_vehicle_bot *state;
	long parent;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->unit)
		return;
	ae_bots_ut_slot(bot, bot_index);
	ae_bots_vehicle_report();
	state = ae_bots_vehicle_state(bot, bot_index);
	parent = tick->object->object.parent_object_index;
	if (parent != NONE && ae_bots_vehicle_get(parent))
	{
		ae_bots_vehicle_ride(bot, bot_index, tick, state, parent);
		return;
	}
	/* (out: its ride over, the vehicle left alone a moment) */
	if (state->vehicle_index != NONE)
	{
		/* (put out by the game: the vehicle wrecked, or flipped and its riders thrown) */
		state->left_vehicle_index = state->vehicle_index;
		state->left_until_tick = tick->now + (state->leaving == AE_BOTS_LEAVE_NO_DRIVER || state->leaving == AE_BOTS_LEAVE_PARKED ?
			AE_BOTS_VEHICLE_LEFT_IDLE_TICKS : AE_BOTS_VEHICLE_LEFT_TICKS);
		if (state->leaving == AE_BOTS_LEAVE_STAY)
		{
			struct object_datum *vehicle = ae_bots_vehicle_get(state->vehicle_index);

			ae_bots_vehicle_count_exit(bot_index, state->vehicle_class,
				vehicle && ae_bots_vehicle_alive(vehicle) && !ae_bots_vehicle_upright(vehicle) ? AE_BOTS_LEAVE_FLIPPED :
				AE_BOTS_LEAVE_WRECKED, (real)(tick->now - state->seated_tick) / TICKS_PER_SECOND);
		}
		/* (the UT spike: an upturned vehicle it was in, alive: it goes back to flip it once the fight is over) */
		{
			struct object_datum *left = ae_bots_vehicle_get(state->vehicle_index);

			if (left && ae_bots_vehicle_alive(left) && !ae_bots_vehicle_upright(left))
				ae_bots_ut_flipped(bot, state->vehicle_index, tick->now);
		}
		state->vehicle_index = NONE;
		bot->path.valid = FALSE;
		bot->goal_kind = _bots_goal_none;
	}
	if (!ae_bots_vehicle_setting())
	{
		state->target_vehicle_index = NONE;
		return;
	}
	if (ae_bots_ut_flip_move(bot, bot_index, tick, state))
		return;
	/* (called again: the time to walk there starts over; called by another: there instead) */
	if (state->target_vehicle_index != NONE)
	{
		long called = ae_bots_vehicle_called(bot, tick);

		if (called == state->target_vehicle_index)
		{
			state->target_tick = tick->now;
			state->called_tick = tick->now;
		}
		else if (called != NONE && state->called_tick != tick->now && tick->now - state->called_tick >= AE_BOTS_VEHICLE_CALLED_TICKS)
			state->target_vehicle_index = NONE;
	}
	if (state->target_vehicle_index == NONE && tick->now >= state->next_look_tick)
	{
		state->next_look_tick = tick->now + AE_BOTS_VEHICLE_LOOK_TICKS;
		ae_bots_vehicle_look(bot, bot_index, tick, state);
		if (state->target_vehicle_index != NONE)
		{
			state->order_tick = tick->now;
			if (ae_bots_events_on())
			{
				char extra[64];
				boolean called = state->called_tick == tick->now;

				snprintf(extra, sizeof(extra), "kind=%s seat=%s far=%.1f", !called ? "own" :
					ae_horn_recent(tick->unit->object.owner_team_index, &tick->position, AE_BOTS_VEHICLE_CALL_REACH) == state->target_vehicle_index ? "honk" : "squad",
					ae_bots_vehicle_seat_name(state->target_seat),
					(double)bots_distance3d(&object_get(state->target_vehicle_index)->object.position, &tick->position));
				ae_bots_event_line(bot_index, tick->now, "seat_order", -1, -1.0f, extra);
			}
		}
	}
	if (state->target_vehicle_index != NONE)
		ae_bots_vehicle_board(bot, bot_index, tick, state);
}

/* THREATS (ae_bots_combat_rules.h): bots_target_score's ask */
static boolean ae_bots_vehicle_on_gun(struct unit_datum const *unit, short role)
{
	long vehicle = unit->object.parent_object_index;

	return vehicle != NONE && (short)ae_bots_vehicle_seat_role(vehicle, unit->unit.parent_seat_index) == role;
}

static void ae_bots_vehicle_threat_view(struct player_datum *player, struct player_datum *other, long player_index, real distance,
	struct ae_bots_threat_view *out)
{
	struct unit_datum *unit = unit_get(other->unit_index);
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	struct ae_bots_threat_view view;
	long weapon = bots_current_weapon(unit);
	real_vector3d to_bot;
	real_point3d const *self = &object_get(player->unit_index)->object.position;

	view.level = ae_bots_level_of_skill(bots_globals.skill);
	view.on_gun = ae_bots_vehicle_on_gun(unit, AE_BOTS_SEAT_GUNNER);
	view.self_on_gun = ae_bots_vehicle_on_gun(unit_get(player->unit_index), AE_BOTS_SEAT_GUNNER);
	view.carries_objective = snapshot && ae_bots_objective_player_carries(snapshot, player_index);
	view.power_weapon = weapon != NONE && bots_weapon_value(weapon_get(weapon)->definition_index) >= 8;
	view.driving = ae_bots_vehicle_on_gun(unit, AE_BOTS_SEAT_DRIVER);
	to_bot.i = self->x - unit->object.position.x;
	to_bot.j = self->y - unit->object.position.y;
	to_bot.k = self->z - unit->object.position.z;
	view.aiming_at_me = distance > 0.5f &&
		(to_bot.i * unit->unit.aiming_vector.i + to_bot.j * unit->unit.aiming_vector.j + to_bot.k * unit->unit.aiming_vector.k) / distance > 0.97f;
	*out = view;
}

static real ae_bots_fight_threat_score(struct bots_bot *bot, struct player_datum *player, struct player_datum *other,
	long player_index, real distance)
{
	struct ae_bots_threat_view view;

	(void)bot;
	ae_bots_vehicle_threat_view(player, other, player_index, distance, &view);
	return ae_bots_combat_threat_score(&view);
}

/* (the log) the target changed: what ranked it */
static void ae_bots_vehicle_threat_note(struct bots_bot *bot, struct bots_tick *tick, long target)
{
	static long last[BOTS_BRAINS];
	long bot_index = (long)(bot - bots_globals.bots);
	struct player_datum *other;
	struct ae_bots_threat_view view;
	char extra[96];
	real distance;

	if (!ae_bots_events_on() || bot_index < 0 || bot_index >= BOTS_BRAINS || last[bot_index] == target + 1 ||
		(other = player_try_and_get(target)) == NULL || other->unit_index == NONE)
	{
		return;
	}
	last[bot_index] = target + 1;
	distance = bots_distance3d(&object_get(other->unit_index)->object.position, &tick->position);
	ae_bots_vehicle_threat_view(tick->player, other, target, distance, &view);
	snprintf(extra, sizeof(extra), "gun=%d self_gun=%d carrier=%d aiming=%d power=%d driving=%d score=%.1f", view.on_gun != 0, view.self_on_gun != 0,
		view.carries_objective != 0, view.aiming_at_me != 0, view.power_weapon != 0, view.driving != 0, (double)ae_bots_combat_threat_score(&view));
	ae_bots_event_line(bot_index, tick->now, "threat", ae_bots_event_absolute(target), distance, extra);
}

/* (the log, ev=pos) what the bot is about */
static void ae_bots_events_role(struct bots_bot *bot, long bot_index, struct bots_tick *tick, char *text, size_t size)
{
	struct ae_bots_vehicle_bot const *state = &ae_bots_vehicle_bots[bot_index];
	long human = ae_bots_team_wingman_of(bot);
	long weapon = bots_current_weapon(tick->unit);
	char const *name = weapon != NONE ? tag_get_name(weapon_get(weapon)->definition_index) : NULL;
	char const *last = name ? strrchr(name, '\\') : NULL;
	char held[32];
	size_t index;

	snprintf(held, sizeof(held), "%s", last ? last + 1 : name ? name : "none");
	for (index = 0; held[index]; index++)
	{
		if (held[index] == ' ')
			held[index] = '_';
	}
	snprintf(text, size, " job=%s crew=%d route=%s with=%ld seat=%s weapon=%s", ae_bots_behaviour_name(ae_bots_objective_bots[bot_index].behaviour),
		ae_squads.bots[bot_index].crew, ae_squads.bots[bot_index].active ? ae_squads_kind_names[ae_squads.bots[bot_index].kind] : "none",
		ae_bots_event_absolute(human), tick->unit->object.parent_object_index != NONE ? ae_bots_vehicle_seat_name(state->seat) : "none", held);
}

static boolean ae_bots_fight_threat_noticed(struct player_datum const *other, real distance)
{
	return distance <= AE_BOTS_COMBAT_THREAT_NOTICE && ae_bots_level_of_skill(bots_globals.skill) >= AE_BOTS_LEVEL_NORMAL &&
		ae_bots_vehicle_on_gun(unit_get(other->unit_index), AE_BOTS_SEAT_GUNNER);
}
