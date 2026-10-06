/*
SPAWN_HEAT_CHECK.C

A check of TRAINING's spawn heat (source/game/spawn_heat.c) and the spawn
ratings it reads (source/game/game_engine.c), built with the game's flags
by tools/test_linux_port.py with those two files and stand-ins for the
players, objects and vehicles they look at:

- spawn_heat_probabilities: the chances of find_best_starting_location_index's
  draw (the highest rating * sqrt(random)) worked out, not drawn: the
  examples of notes on it (equal ratings 50/50, 0.7 of the other 24.5%, 0.5
  12.5%, a teammate's bonus of 4 against 20 plain spawns 94%), every set
  summing to 1, and the engine's own draw (its random numbers, 16 bits,
  its float sums) run 200000 times on random sets agreeing within 4.5
  standard errors;
- the ratings: game_engine_get_starting_location_rating bit for bit the
  stock rules (a copy of them here, as they were before the _ex
  variants) in thousands of random games (free for all, team slayer,
  CTF with a gametype rating, no game engine), with players near the
  spawns at the rules' distances and vehicles on some;
  game_engine_get_starting_location_rating_ex with no unit left out the
  same, and with a living player's own unit left out the same as that
  player's rating with no unit (as if dead); and the reason a spawn rates
  0 the copy's.

Prints the failures, or PASS. With "dump" it prints every rating (hex),
for comparing with a build of another game_engine.c (SPAWN_HEAT_CHECK_BASE:
the stock one, without the _ex variants or spawn_heat.c).
*/

#define BUILDING_CSERIES
#include "cseries/cseries.h"

#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#ifndef SPAWN_HEAT_CHECK_BASE
#include "game/spawn_heat.h"
#endif

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (the C library's own, not the game's stand-ins for Xbox paths and MSVC's
formats, port/linux/include/stdio.h) */
#undef fopen
#undef snprintf
#undef vsnprintf
#undef sprintf
#undef vsprintf
#undef printf
#undef vprintf

#define CHECK_PLAYERS 16
#define CHECK_SPAWNS 48
#define CHECK_OBJECTS (CHECK_PLAYERS + 8)
#define CHECK_GAMES 3000

extern struct game_variant global_variant;

static int failures;
static int dump;

static void fail(char const *format, ...)
{
	va_list arguments;

	if (failures++ < 40)
	{
		va_start(arguments, format);
		vprintf(format, arguments);
		va_end(arguments);
		printf("\n");
	}
}

/* ---------- a small random generator of the check's own */

static unsigned long check_seed = 12345;

static unsigned long check_random(void)
{
	check_seed ^= check_seed << 13;
	check_seed ^= check_seed >> 17;
	check_seed ^= check_seed << 5;
	return check_seed & 0xFFFFFFFFUL;
}

static double check_uniform(void)
{
	return (double)(check_random() & 0xFFFFFF) / (double)0x1000000;
}

/* ---------- stand-ins for the game: players, objects (their units and
vehicles), the game engine */

static struct player_datum check_players[CHECK_PLAYERS];
static int check_player_used[CHECK_PLAYERS];
static struct data_array check_player_array;
struct data_array *player_data = &check_player_array;

static struct object_datum check_objects[CHECK_OBJECTS];
static real_point3d check_object_origins[CHECK_OBJECTS];
static int check_object_used[CHECK_OBJECTS];

static struct player_starting_location check_spawns[CHECK_SPAWNS];
static int check_spawn_count;

static struct game_engine check_engine;

void *datum_get(struct data_array *data, long index)
{
	if (data != player_data || (index & 0xFFFF) >= CHECK_PLAYERS || !check_player_used[index & 0xFFFF])
	{
		printf("datum_get(%ld) of no player\n", index);
		exit(2);
	}
	return &check_players[index & 0xFFFF];
}

void *datum_try_and_get(struct data_array *data, long index)
{
	if (data != player_data || index == NONE || (index & 0xFFFF) >= CHECK_PLAYERS ||
		!check_player_used[index & 0xFFFF])
	{
		return NULL;
	}
	return &check_players[index & 0xFFFF];
}

void data_iterator_new(struct data_iterator *iterator, struct data_array *data)
{
	iterator->data = data;
	iterator->absolute_index = 0;
	iterator->datum_index = NONE;
}

void *data_iterator_next(struct data_iterator *iterator)
{
	while (iterator->absolute_index < CHECK_PLAYERS)
	{
		short index = iterator->absolute_index++;

		if (check_player_used[index])
		{
			iterator->datum_index = index;
			return &check_players[index];
		}
	}
	return NULL;
}

real_point3d *object_get_origin(long object_index, real_point3d *origin)
{
	*origin = check_object_origins[object_index];
	return origin;
}

void *object_get_and_verify_type(long object_index, unsigned long valid_type_flags)
{
	if (object_index < 0 || object_index >= CHECK_OBJECTS || !check_object_used[object_index] ||
		!TEST_FLAG(valid_type_flags, check_objects[object_index].object.type))
	{
		return NULL;
	}
	return &check_objects[object_index];
}

void scenario_location_from_point(struct location *location, const real_point3d *point)
{
	(void)point;
	memset(location, 0, sizeof(*location));
}

/* (as objects.c's: each object of the types whose bounding sphere,
grown by radius, holds the centre) */
short objects_in_sphere(unsigned long class_flags, unsigned long type_flags, struct location const *location,
	real_point3d const *center, real radius, long *object_indices, short maximum_count)
{
	short count = 0;
	long index;

	(void)class_flags;
	(void)location;
	for (index = 0; index < CHECK_OBJECTS && count < maximum_count; index++)
	{
		struct object_datum *object = &check_objects[index];

		if (check_object_used[index] && TEST_FLAG(type_flags, object->object.type) &&
			point_in_sphere(center, &object->object.bounding_sphere_center,
				object->object.bounding_sphere_radius + radius))
		{
			object_indices[count++] = index;
		}
	}
	return count;
}

void release_assert_failed(char const *information, char const *file, long line, boolean fatal)
{
	(void)fatal;
	printf("assert %s (%s:%ld)\n", information, file, line);
	exit(2);
}

void display_assert(char *information, char *file, long line, boolean fatal)
{
	(void)fatal;
	printf("assert %s (%s:%ld)\n", information, file, line);
	exit(2);
}

static boolean check_ctf_test_flag(long flag)
{
	return flag == 0;
}

/* (a gametype's own rating, as CTF assault's: 0.5 to 2, by where the spawn is) */
static real check_gametype_rating(long player_index, struct player_starting_location const *location)
{
	(void)player_index;
	return 0.5f + 1.5f * (real)fabs(sin((double)location->position.x * 0.37 + (double)location->position.y));
}

/* ---------- the stock rules, as game_engine.c had them before the _ex
variants (the same float steps) */

static real stock_friendly_bonus(long player_index, real_point3d const *position)
{
	struct player_datum *player = player_get(player_index);
	struct data_iterator iterator;
	struct player_datum *other_player;
	real rating = 0.0f;

	data_iterator_new(&iterator, player_data);
	other_player = (struct player_datum *)data_iterator_next(&iterator);
	while (other_player)
	{
		if (player->team_index == other_player->team_index && other_player->unit_index != NONE)
		{
			real_point3d origin;
			real distance;

			object_get_origin(other_player->unit_index, &origin);
			distance = distance3d(&origin, position);
			if (distance >= 1.0f && distance <= 6.0f)
				rating += (real)pow((double)(1.0f - (distance - 1.0f) * 0.2f), (double)0.6f);
		}
		other_player = (struct player_datum *)data_iterator_next(&iterator);
	}
	if (rating > 3.0f)
		rating = 3.0f;
	return rating * 3.0f + 1.0f;
}

/* (and why it is 0: the first rule to make it so) */
static real stock_distance_rating(long player_index, real_point3d const *position, short *reason)
{
	boolean has_teams = game_engine ? global_variant.universal_variant.teams : FALSE;
	struct player_datum *player = player_get(player_index);
	struct data_iterator iterator;
	struct player_datum *other_player;
	real rating = 1.0f;

	data_iterator_new(&iterator, player_data);
	other_player = (struct player_datum *)data_iterator_next(&iterator);
	while (other_player)
	{
		if (other_player->unit_index != NONE)
		{
			real_point3d origin;
			real distance;

			object_get_origin(other_player->unit_index, &origin);
			distance = distance3d(&origin, position);
			if (!has_teams || other_player->team_index != player->team_index || !(distance > 0.25f))
			{
				if (distance < 0.25f)
				{
					rating = 0.0f;
					if (*reason == _spawn_rating_rated)
					{
						*reason = other_player->team_index != player->team_index ?
							_spawn_rating_zero_enemy : _spawn_rating_zero_teammate;
					}
				}
				else if (distance < 1.0f)
					rating *= 0.1f;
				if (other_player->team_index != player->team_index)
				{
					if (distance < 2.0f)
					{
						rating = 0.0f;
						if (*reason == _spawn_rating_rated)
							*reason = _spawn_rating_zero_enemy;
					}
					else if (!(distance > 5.0f))
					{
						rating = (distance - 2.0f) * rating * 0.33333334f;
						if (!(rating > 0.0f) && *reason == _spawn_rating_rated)
							*reason = _spawn_rating_zero_enemy;
					}
				}
			}
		}
		other_player = (struct player_datum *)data_iterator_next(&iterator);
	}
	return rating;
}

static real stock_rating(long player_index, struct player_starting_location const *location, short *reason)
{
	struct player_datum *player = player_get(player_index);
	long type = game_engine ? game_engine->type : NONE;
	real rating = 1.0f;
	long object_indices[16];
	short object_count;
	short index;
	struct location where;

	*reason = _spawn_rating_rated;
	if (!match_game_type(type, 4, location->game_types))
	{
		*reason = _spawn_rating_zero_game_type;
		return 0.0f;
	}
	scenario_location_from_point(&where, &location->position);
	object_count = objects_in_sphere(0, 0x11F, &where, &location->position, 0.1f, object_indices, 16);
	for (index = 0; index < object_count; index++)
	{
		if (check_objects[object_indices[index]].object.type == _object_type_vehicle)
		{
			*reason = _spawn_rating_zero_vehicle;
			return 0.0f;
		}
	}
	if (game_engine && game_engine->test_flag && game_engine->test_flag(0) &&
		player->team_index != location->team_index)
	{
		rating = 0.0f;
		*reason = _spawn_rating_zero_team;
	}
	if (rating > 0.0f)
		rating *= stock_distance_rating(player_index, &location->position, reason);
	if (game_engine && rating > 0.0f && global_variant.universal_variant.teams)
		rating *= stock_friendly_bonus(player_index, &location->position);
	if (game_engine && game_engine->starting_location_rating)
		rating *= game_engine->starting_location_rating(player_index, location);
	if (!(rating > 0.0f) && *reason == _spawn_rating_rated)
		*reason = _spawn_rating_zero_other;
	return rating;
}

/* ---------- a random game: its spawns, players (some dead), their units
near the spawns at the rules' distances, and a vehicle or two */

static real check_offsets[] = { 0.0f, 0.1f, 0.25f, 0.5f, 0.9f, 1.0f, 1.5f, 2.0f, 2.5f, 3.5f, 5.0f, 5.5f, 6.0f, 7.0f };

static void check_game(int mode)
{
	long index;
	int player_count = 1 + (int)(check_random() % CHECK_PLAYERS);
	int vehicle_count = (int)(check_random() % 3);
	long object_index = 0;

	memset(check_players, 0, sizeof(check_players));
	memset(check_player_used, 0, sizeof(check_player_used));
	memset(check_objects, 0, sizeof(check_objects));
	memset(check_object_used, 0, sizeof(check_object_used));

	memset(&check_engine, 0, sizeof(check_engine));
	memset(&global_variant, 0, sizeof(global_variant));
	game_engine = &check_engine;
	switch (mode)
	{
	case 0:	/* free for all slayer */
		check_engine.type = game_engine_slayer;
		break;
	case 1:	/* team slayer */
		check_engine.type = game_engine_slayer;
		global_variant.universal_variant.teams = TRUE;
		break;
	case 2:	/* CTF, with a gametype rating */
		check_engine.type = game_engine_ctf;
		check_engine.test_flag = check_ctf_test_flag;
		check_engine.starting_location_rating = check_gametype_rating;
		global_variant.universal_variant.teams = TRUE;
		break;
	default:	/* no game engine (a multiplayer map played alone) */
		game_engine = NULL;
		break;
	}

	check_spawn_count = 1 + (int)(check_random() % CHECK_SPAWNS);
	for (index = 0; index < check_spawn_count; index++)
	{
		struct player_starting_location *spawn = &check_spawns[index];
		static short const types[] = { 12, 13, 14, game_engine_slayer, game_engine_ctf, game_engine_king, 0 };
		short slot;

		memset(spawn, 0, sizeof(*spawn));
		spawn->position.x = (real)(check_uniform() * 40.0 - 20.0);
		spawn->position.y = (real)(check_uniform() * 40.0 - 20.0);
		spawn->position.z = (real)(check_uniform() * 4.0);
		spawn->facing = (real)(check_uniform() * 6.28);
		spawn->team_index = (short)(check_random() % 4 == 0 ? 3 : check_random() % 2);
		for (slot = 0; slot < 4; slot++)
			spawn->game_types[slot] = slot == 0 || check_random() % 3 == 0 ? types[check_random() % NUMBEROF(types)] : 0;
	}

	for (index = 0; index < player_count; index++)
	{
		struct player_datum *player = &check_players[index];

		check_player_used[index] = 1;
		player->team_index = mode == 1 || mode == 2 ? (long)(check_random() % 2) : index;
		player->local_player_index = NONE;
		player->unit_index = NONE;
		if (check_random() % 5 != 0)
		{
			struct player_starting_location const *near = &check_spawns[check_random() % check_spawn_count];
			real offset = check_offsets[check_random() % NUMBEROF(check_offsets)];
			double angle = check_uniform() * 6.283;

			player->unit_index = object_index;
			check_object_used[object_index] = 1;
			check_objects[object_index].object.type = _object_type_biped;
			check_object_origins[object_index].x = near->position.x + offset * (real)cos(angle);
			check_object_origins[object_index].y = near->position.y + offset * (real)sin(angle);
			check_object_origins[object_index].z = near->position.z;
			check_objects[object_index].object.bounding_sphere_center = check_object_origins[object_index];
			check_objects[object_index].object.bounding_sphere_center.z += 0.3f;
			check_objects[object_index].object.bounding_sphere_radius = 0.35f;
			object_index++;
		}
	}

	for (index = 0; index < vehicle_count; index++)
	{
		struct player_starting_location const *near = &check_spawns[check_random() % check_spawn_count];

		check_object_used[object_index] = 1;
		check_objects[object_index].object.type = _object_type_vehicle;
		check_objects[object_index].object.bounding_sphere_center = near->position;
		check_objects[object_index].object.bounding_sphere_center.x += (real)(check_uniform() * 4.0 - 2.0);
		check_objects[object_index].object.bounding_sphere_radius = (real)(0.5 + check_uniform() * 1.5);
		check_object_origins[object_index] = check_objects[object_index].object.bounding_sphere_center;
		object_index++;
	}
}

static int same_bits(real a, real b)
{
	return !memcmp(&a, &b, sizeof(real));
}

static void check_ratings(void)
{
	int game;
	unsigned long digest = 2166136261UL;
	long rated = 0;
	long zero_reasons[NUMBER_OF_SPAWN_RATING_ZERO_REASONS] = { 0 };
	long as_if_dead = 0;

	for (game = 0; game < CHECK_GAMES; game++)
	{
		int mode = game % 4;
		long player_index;
		long spawn_index;

		check_game(mode);
		for (player_index = 0; player_index < CHECK_PLAYERS; player_index++)
		{
			if (!check_player_used[player_index])
				continue;
			for (spawn_index = 0; spawn_index < check_spawn_count; spawn_index++)
			{
				struct player_starting_location const *spawn = &check_spawns[spawn_index];
				real engine = game_engine_get_starting_location_rating(player_index, spawn);
				short stock_reason;
				real stock = stock_rating(player_index, spawn, &stock_reason);
				unsigned char const *bytes = (unsigned char const *)&engine;
				size_t byte;

				for (byte = 0; byte < sizeof(engine); byte++)
					digest = (digest ^ bytes[byte]) * 16777619UL;
				if (dump)
					printf("%d %ld %ld %a\n", game, player_index, spawn_index, (double)engine);
				if (!same_bits(engine, stock))
				{
					fail("game %d (mode %d) player %ld spawn %ld: rated %a, the stock rules %a", game, mode,
						player_index, spawn_index, (double)engine, (double)stock);
				}
				if (engine > 0.0f)
					rated++;
				zero_reasons[stock_reason]++;
#ifndef SPAWN_HEAT_CHECK_BASE
				{
					short reason = NONE;
					real ex = game_engine_get_starting_location_rating_ex(player_index, spawn, NONE, &reason);
					real ex_no_reason = game_engine_get_starting_location_rating_ex(player_index, spawn, NONE, NULL);
					long unit_index = check_players[player_index].unit_index;

					if (!same_bits(ex, engine) || !same_bits(ex_no_reason, engine))
					{
						fail("game %d player %ld spawn %ld: _ex with no unit left out %a / %a, not %a", game,
							player_index, spawn_index, (double)ex, (double)ex_no_reason, (double)engine);
					}
					if (reason != stock_reason || (reason == _spawn_rating_rated) != (ex > 0.0f))
					{
						fail("game %d (mode %d) player %ld spawn %ld: reason %d, the stock rules' %d (rated %a)", game,
							mode, player_index, spawn_index, reason, stock_reason, (double)ex);
					}
					/* (a living player's own unit left out: as if dead) */
					if (unit_index != NONE)
					{
						real dead;
						real left_out;
						short dead_reason;

						check_players[player_index].unit_index = NONE;
						dead = game_engine_get_starting_location_rating(player_index, spawn);
						stock_rating(player_index, spawn, &dead_reason);
						check_players[player_index].unit_index = unit_index;
						left_out = game_engine_get_starting_location_rating_ex(player_index, spawn, unit_index, &reason);
						if (!same_bits(dead, left_out) || reason != dead_reason)
						{
							fail("game %d (mode %d) player %ld spawn %ld: own unit left out %a (reason %d), as if dead "
								"%a (reason %d)", game, mode, player_index, spawn_index, (double)left_out, reason,
								(double)dead, dead_reason);
						}
						if (!same_bits(dead, engine))
							as_if_dead++;
					}
				}
#endif
			}
		}
	}

	printf("ratings: %d games, %ld rated above 0; at 0: game type %ld, vehicle %ld, team %ld, enemy %ld, teammate %ld, "
		"other %ld; %ld differ with the player's own unit left out; digest %08lx\n",
		CHECK_GAMES, rated, zero_reasons[_spawn_rating_zero_game_type], zero_reasons[_spawn_rating_zero_vehicle],
		zero_reasons[_spawn_rating_zero_team], zero_reasons[_spawn_rating_zero_enemy],
		zero_reasons[_spawn_rating_zero_teammate], zero_reasons[_spawn_rating_zero_other], as_if_dead, digest);
	/* (each rule is met: the random games reach them all) */
	if (rated == 0 || !zero_reasons[_spawn_rating_zero_game_type] || !zero_reasons[_spawn_rating_zero_vehicle] ||
		!zero_reasons[_spawn_rating_zero_team] || !zero_reasons[_spawn_rating_zero_enemy] ||
		!zero_reasons[_spawn_rating_zero_teammate]
#ifndef SPAWN_HEAT_CHECK_BASE
		|| !as_if_dead
#endif
		)
	{
		fail("the random games miss a rule");
	}
}

#ifndef SPAWN_HEAT_CHECK_BASE

/* ---------- the chances */

static void expect(char const *what, double got, double wanted, double tolerance)
{
	if (!(fabs(got - wanted) <= tolerance))
		fail("%s: %.6f, not %.6f", what, got, wanted);
}

/* (the engine's draw: random_math.c's generator, 16 bits, and
find_best_starting_location_index's float steps, strict >) */
static unsigned long engine_seed = 0x2F00D;

static real engine_random(void)
{
	engine_seed = engine_seed * 1664525UL + 1013904223UL;
	engine_seed &= 0xFFFFFFFFUL;
	return (real)(engine_seed >> 16) / 65535.0f;
}

static long engine_pick(long count, real const *ratings)
{
	long best = NONE;
	real best_rating = 0.0f;
	long index;

	for (index = 0; index < count; index++)
	{
		real rating = ratings[index] * (real)pow((double)engine_random(), 0.5);

		if (rating > best_rating)
		{
			best_rating = rating;
			best = index;
		}
	}
	return best;
}

static void check_probabilities(void)
{
	real ratings[SPAWN_HEAT_MAXIMUM_SPAWNS];
	real chances[SPAWN_HEAT_MAXIMUM_SPAWNS];
	long index;
	int set;

	/* the notes' examples */
	ratings[0] = 1.0f; ratings[1] = 1.0f;
	spawn_heat_probabilities(2, ratings, chances);
	expect("equal ratings", chances[0], 0.5, 1e-6);
	expect("equal ratings (second)", chances[1], 0.5, 1e-6);
	ratings[1] = 0.7f;
	spawn_heat_probabilities(2, ratings, chances);
	expect("0.7 of the other", chances[1], 0.245, 1e-6);
	expect("0.7 of the other (first)", chances[0], 0.755, 1e-6);
	ratings[1] = 0.5f;
	spawn_heat_probabilities(2, ratings, chances);
	expect("half the other", chances[1], 0.125, 1e-6);
	{
		/* a teammate's bonus (3 s + 1, s = (1 - (d - 1) 0.2)^0.6) against
		20 plain spawns: 1 wu 94%, 3.5 wu 89%, 5 wu 79%, 5.9 wu 43% */
		static double const distances[] = { 1.0, 3.5, 5.0, 5.9 };
		static double const wanted[] = { 0.9405, 0.89, 0.79, 0.43 };
		int example;

		for (example = 0; example < 4; example++)
		{
			char what[64];

			for (index = 0; index < 21; index++)
				ratings[index] = 1.0f;
			ratings[7] = (real)(3.0 * pow(1.0 - (distances[example] - 1.0) * 0.2, 0.6) + 1.0);
			spawn_heat_probabilities(21, ratings, chances);
			snprintf(what, sizeof(what), "a teammate at %.1f wu against 20", distances[example]);
			expect(what, chances[7], wanted[example], 0.006);
			expect("a plain spawn against it", chances[0], (1.0 - chances[7]) / 20.0, 1e-6);
		}
		for (index = 0; index < 21; index++)
			ratings[index] = 1.0f;
		ratings[7] = 4.0f;
		spawn_heat_probabilities(21, ratings, chances);
		expect("exactly (1/16)(1/21 + 15)", chances[7], (1.0 / 16.0) * (1.0 / 21.0 + 15.0), 1e-6);
	}

	/* none rated: all 0; a rating of 0 never picked */
	for (index = 0; index < 5; index++)
		ratings[index] = 0.0f;
	spawn_heat_probabilities(5, ratings, chances);
	for (index = 0; index < 5; index++)
		expect("none rated", chances[index], 0.0, 0.0);
	ratings[2] = 0.3f;
	spawn_heat_probabilities(5, ratings, chances);
	expect("the one rated", chances[2], 1.0, 1e-6);
	expect("one at 0", chances[0], 0.0, 0.0);

	/* near uniform: the least chance above 0 at least 0.8 of the most */
	{
		static real const equal[] = { 0.25f, 0.25f, 0.0f, 0.25f, 0.25f };
		static real const close[] = { 0.21f, 0.26f, 0.0f };
		static real const apart[] = { 0.3f, 0.25f, 0.45f };
		static real const none[] = { 0.0f, 0.0f };

		if (!spawn_heat_near_uniform(5, equal) || !spawn_heat_near_uniform(3, close) ||
			spawn_heat_near_uniform(3, apart) || spawn_heat_near_uniform(2, none))
		{
			fail("spawn_heat_near_uniform: equal %d close %d apart %d none %d", spawn_heat_near_uniform(5, equal),
				spawn_heat_near_uniform(3, close), spawn_heat_near_uniform(3, apart), spawn_heat_near_uniform(2, none));
		}
		/* (free for all, one spawn of 16 a little lower: rated 0.995, near
		uniform; rated 0.9 (an enemy 4.7 wu from it), not: against 15
		others rated 1 its chance is about rating^30 of theirs, 86% and 4%) */
		for (index = 0; index < 16; index++)
			ratings[index] = 1.0f;
		ratings[3] = 0.995f;
		spawn_heat_probabilities(16, ratings, chances);
		if (!spawn_heat_near_uniform(16, chances))
			fail("one spawn of 16 rated 0.995: not near uniform");
		ratings[3] = 0.9f;
		spawn_heat_probabilities(16, ratings, chances);
		if (spawn_heat_near_uniform(16, chances))
			fail("one spawn of 16 rated 0.9: near uniform");
	}

	/* sets of every size: the chances sum to 1, a higher rating never less
	likely, equal ratings as likely; and the engine's own draw agrees */
	for (set = 0; set < 400; set++)
	{
		long count = 1 + (long)(check_random() % (set < 380 ? 40 : SPAWN_HEAT_MAXIMUM_SPAWNS));
		double sum = 0.0;
		long other;

		for (index = 0; index < count; index++)
		{
			switch (check_random() % 6)
			{
			case 0: ratings[index] = 0.0f; break;
			case 1: ratings[index] = 1.0f; break;
			case 2: ratings[index] = (real)(check_uniform() * 10.0); break;
			case 3: ratings[index] = (real)(check_uniform() * 0.01); break;
			default: ratings[index] = (real)check_uniform(); break;
			}
		}
		spawn_heat_probabilities(count, ratings, chances);
		for (index = 0; index < count; index++)
		{
			if (!(chances[index] >= 0.0f && chances[index] <= 1.0f + 1e-6f))
				fail("set %d: chance %ld is %f", set, index, (double)chances[index]);
			sum += chances[index];
			for (other = 0; other < count; other++)
			{
				if (ratings[index] > ratings[other] && chances[index] < chances[other])
					fail("set %d: rating %f less likely than %f", set, (double)ratings[index], (double)ratings[other]);
				if (ratings[index] == ratings[other] && chances[index] != chances[other])
					fail("set %d: equal ratings %f unequally likely", set, (double)ratings[index]);
			}
		}
		for (index = 0; index < count && ratings[index] <= 0.0f; index++)
			;
		expect("the chances' sum", sum, index < count ? 1.0 : 0.0, 1e-4);

		if (set < 12)
		{
			static long picks[SPAWN_HEAT_MAXIMUM_SPAWNS];
			long trials = 200000;
			long trial;

			memset(picks, 0, sizeof(picks));
			for (trial = 0; trial < trials; trial++)
			{
				long pick = engine_pick(count, ratings);

				if (pick != NONE)
					picks[pick]++;
			}
			for (index = 0; index < count; index++)
			{
				double frequency = (double)picks[index] / (double)trials;
				double error = sqrt(chances[index] * (1.0 - chances[index]) / (double)trials);
				char what[64];

				snprintf(what, sizeof(what), "set %d spawn %ld: the engine's draw", set, index);
				expect(what, frequency, chances[index], 4.5 * error + 2e-4);
			}
		}
	}
	printf("chances: examples, near uniform, 400 sets (sums, order, ties), the engine's draw 200000 times on 12\n");
}

#endif

int main(int argc, char **argv)
{
	dump = argc > 1 && !strcmp(argv[1], "dump");
#ifndef SPAWN_HEAT_CHECK_BASE
	if (!dump)
		check_probabilities();
#endif
	check_ratings();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	if (!dump)
		printf("PASS\n");
	return 0;
}
