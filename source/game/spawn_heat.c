/*
SPAWN_HEAT.C

port: TRAINING's live spawn heat, for learning CE's spawn logic: how likely
each player spawn is to be the next one picked, which the spawn markers
(render_spawn_markers.c) show as each marker's colour, and a flash where a
player really spawns.

find_best_starting_location_index (players.c) picks the spawn whose rating
(game_engine_get_starting_location_rating: the game type's spawns, none
with a vehicle on it, CTF's own team's; no enemy within 2 world units, less
for one within 5; up to 10 times more with teammates within 6) times the
square root of a random number from 0 to 1 is highest. Here the ratings are
worked out the same way (game_engine_get_starting_location_rating_ex, one
copy of the rules), and each spawn's chance of winning that draw computed
(spawn_heat_probabilities), never drawn: the game's random numbers are not
touched, nothing is networked, every machine works it out from what it
has (players' units and vehicles, which the host sends), as near the
host's as the network's lag.

SPAWN HEAT (display.spawn_heat, this machine's own): MINE rates each view's
own player's spawns, as if they were dead now (their own unit left out, as
the engine has none for a dead player); ENEMY rates an enemy's (any player
of the other team: their spawns' ratings differ only by their own unit,
left out), in a team game; without teams ENEMY is as MINE (every other
player is an enemy, each with spawns of their own). OFF leaves the plain
markers. When the spawns that can be picked are near uniform (the least
likely at least SPAWN_HEAT_UNIFORM_RATIO as likely as the most: free for
all, mostly) they are drawn alike, in the scale's middle, none pulsing.
Worked out every SPAWN_HEAT_UPDATE_TICKS ticks for each local
player's view while the markers show (TRAINING's rules:
item_timers_training_shown, and not in a cinematic).
*/

/* ---------- headers */

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"

#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "game/players.h"
#include "game/spawn_heat.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

#include <math.h>

/* ---------- constants */

/* a unit attached to a player (a distributed client's spawn) is at the
starting location the host spawned it at within this (the host places it
on the point); or, as it may have fallen a little by the time this machine
has it, within this across and this up or down */
#define SPAWN_HEAT_ATTACH_DISTANCE 0.1f
#define SPAWN_HEAT_ATTACH_FALL 0.5f

/* the log has each view's heat every this many ticks (10 s) */
#define SPAWN_HEAT_LOG_TICKS 300

/* the most likely spawn is "the hottest" (pulsing) only when it is more
likely than the next by more than this part */
#define SPAWN_HEAT_HOTTEST_MARGIN 1.01

/* ---------- prototypes */

void platform_log(char const *format, ...);
char const *config_string(char const *name);
unsigned long config_changes(void);

static boolean spawn_heat_rate(long player_index, long ignore_unit_index, struct spawn_heat_view *view);

/* ---------- globals */

static struct
{
	/* each local player's view's heat, and whether it holds one */
	struct spawn_heat_view views[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	boolean valid[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	short setting_shown[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	long logged_at[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

	/* the flashes of the last real spawns */
	struct
	{
		long spawn_index;
		long player_index;
		long tick;
		short team_index;
	} flashes[SPAWN_HEAT_MAXIMUM_FLASHES];
	short flash_next;

	/* what working the heat out costs (for the log) */
	long computed;
	double compute_seconds;
	double compute_slowest;

	boolean too_many_logged;
} spawn_heat_globals;

/* (scratch for spawn_heat_rate and spawn_heat_probabilities) */
static struct spawn_heat_view spawn_heat_scratch_view;
static real spawn_heat_ratings[SPAWN_HEAT_MAXIMUM_SPAWNS];
static real spawn_heat_chances[SPAWN_HEAT_MAXIMUM_SPAWNS];

/* ---------- public code */

short spawn_heat_setting(
	void)
{
	static unsigned long read_at = (unsigned long)-1;
	static short setting = _spawn_heat_mine;

	/* (read again when Settings changes it) */
	if (read_at != config_changes())
	{
		char const *value = config_string("display.spawn_heat");

		read_at = config_changes();
		if (value && !csstrcmp(value, "off"))
			setting = _spawn_heat_off;
		else if (value && !csstrcmp(value, "enemy"))
			setting = _spawn_heat_enemy;
		else
			setting = _spawn_heat_mine;
	}

	return setting;
}

boolean spawn_heat_shown(
	void)
{
	/* (the host's gametype allows it: SPAWN HEAT, INDICATOR OPTIONS; then
	each player's own MINE / ENEMY / OFF) */
	return spawn_heat_setting() != _spawn_heat_off && game_engine_spawn_heat_allowed() && !cinematic_in_progress() &&
		item_timers_training_shown();
}

void spawn_heat_initialize_for_new_map(
	void)
{
	short local_player_index;

	csmemset(&spawn_heat_globals, 0, sizeof(spawn_heat_globals));
	for (local_player_index = 0; local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local_player_index++)
	{
		spawn_heat_globals.setting_shown[local_player_index] = NONE;
		spawn_heat_globals.logged_at[local_player_index] = NONE;
	}
	for (local_player_index = 0; local_player_index < SPAWN_HEAT_MAXIMUM_FLASHES; local_player_index++)
		spawn_heat_globals.flashes[local_player_index].spawn_index = NONE;

	return;
}

void spawn_heat_update(
	void)
{
	long now = game_time_get();
	boolean shown = spawn_heat_shown();
	short setting = spawn_heat_setting();
	short local_player_index;

	for (local_player_index = 0; local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local_player_index++)
	{
		struct spawn_heat_view *view = &spawn_heat_globals.views[local_player_index];
		long player_index = shown ? local_player_get_player_index(local_player_index) : NONE;
		struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
		long rated_player_index;
		long ignore_unit_index;
		LARGE_INTEGER started;
		LARGE_INTEGER ended;
		LARGE_INTEGER frequency;
		double seconds;

		if (!player)
		{
			if (spawn_heat_globals.valid[local_player_index])
				platform_log("spawn heat: view %d off at tick %ld", local_player_index, now);
			spawn_heat_globals.valid[local_player_index] = FALSE;
			spawn_heat_globals.setting_shown[local_player_index] = NONE;
			continue;
		}

		/* (every SPAWN_HEAT_UPDATE_TICKS ticks, and at once as it starts,
		SPAWN HEAT changes or a new game's time starts over) */
		if (spawn_heat_globals.valid[local_player_index] &&
			spawn_heat_globals.setting_shown[local_player_index] == setting &&
			now >= view->computed_at && now - view->computed_at < SPAWN_HEAT_UPDATE_TICKS)
		{
			continue;
		}

		/* whose spawns: MINE this view's player's (their unit left out);
		ENEMY in a team game the first player of another team's (their
		unit left out), else as MINE */
		rated_player_index = player_index;
		ignore_unit_index = player->unit_index;
		if (setting == _spawn_heat_enemy && game_engine_has_teams())
		{
			struct data_iterator iterator;
			struct player_datum *other_player;

			data_iterator_new(&iterator, player_data);
			while ((other_player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
			{
				if (other_player->team_index != player->team_index)
				{
					rated_player_index = iterator.datum_index;
					ignore_unit_index = other_player->unit_index;
					break;
				}
			}
		}

		QueryPerformanceCounter(&started);
		if (!spawn_heat_rate(rated_player_index, ignore_unit_index, view))
		{
			spawn_heat_globals.valid[local_player_index] = FALSE;
			continue;
		}
		QueryPerformanceCounter(&ended);
		QueryPerformanceFrequency(&frequency);
		seconds = frequency.QuadPart > 0 ? (double)(ended.QuadPart - started.QuadPart) / (double)frequency.QuadPart : 0.0;
		spawn_heat_globals.computed++;
		spawn_heat_globals.compute_seconds += seconds;
		if (seconds > spawn_heat_globals.compute_slowest)
			spawn_heat_globals.compute_slowest = seconds;

		view->computed_at = now;
		view->enemy = rated_player_index != player_index;
		if (!spawn_heat_globals.valid[local_player_index] ||
			spawn_heat_globals.setting_shown[local_player_index] != setting)
		{
			platform_log("spawn heat: view %d on at tick %ld, %s, rating player %ld's spawns (team %d), %ld spawns",
				local_player_index, now, setting == _spawn_heat_enemy ? (view->enemy ? "ENEMY" : "ENEMY as MINE") : "MINE",
				(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(rated_player_index), view->rated_team_index, view->count);
			spawn_heat_globals.logged_at[local_player_index] = NONE;
		}
		spawn_heat_globals.valid[local_player_index] = TRUE;
		spawn_heat_globals.setting_shown[local_player_index] = setting;

		/* (the log has each view's heat now and then: the likeliest
		spawns, how many rate 0, and the cost so far) */
		if (spawn_heat_globals.logged_at[local_player_index] == NONE ||
			now - spawn_heat_globals.logged_at[local_player_index] >= SPAWN_HEAT_LOG_TICKS ||
			now < spawn_heat_globals.logged_at[local_player_index])
		{
			long best[3] = { NONE, NONE, NONE };
			long index;
			long zero_count = 0;
			long rated_count = 0;
			short rank;

			for (index = 0; index < view->count; index++)
			{
				struct spawn_heat_spawn const *spawn = &view->spawns[index];

				if (spawn->zero_reason == _spawn_rating_zero_game_type)
					continue;
				if (spawn->zero_reason != _spawn_rating_rated)
				{
					zero_count++;
					continue;
				}
				rated_count++;
				for (rank = 0; rank < 3; rank++)
				{
					if (best[rank] == NONE || spawn->probability > view->spawns[best[rank]].probability)
					{
						short shift;

						for (shift = 2; shift > rank; shift--)
							best[shift] = best[shift - 1];
						best[rank] = index;
						break;
					}
				}
			}
			platform_log("spawn heat: view %d tick %ld: %ld rated%s, %ld at 0; likeliest %ld %.1f%%, %ld %.1f%%, %ld %.1f%%; "
				"%ld updates, %.1f us average, %.1f us slowest",
				local_player_index, now, rated_count, view->near_uniform ? " (near uniform)" : "", zero_count,
				best[0], best[0] != NONE ? view->spawns[best[0]].probability * 100.0f : 0.0f,
				best[1], best[1] != NONE ? view->spawns[best[1]].probability * 100.0f : 0.0f,
				best[2], best[2] != NONE ? view->spawns[best[2]].probability * 100.0f : 0.0f,
				spawn_heat_globals.computed,
				spawn_heat_globals.computed ? spawn_heat_globals.compute_seconds * 1e6 / spawn_heat_globals.computed : 0.0,
				spawn_heat_globals.compute_slowest * 1e6);
			spawn_heat_globals.logged_at[local_player_index] = now;
		}
	}

	return;
}

struct spawn_heat_view const *spawn_heat_get_view(
	short local_player_index)
{
	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS ||
		!spawn_heat_globals.valid[local_player_index] || !spawn_heat_shown())
	{
		return NULL;
	}

	return &spawn_heat_globals.views[local_player_index];
}

short spawn_heat_get_flashes(
	struct spawn_heat_flash *flashes,
	short maximum)
{
	long now = game_time_get();
	short count = 0;
	short index;

	if (!spawn_heat_shown())
		return 0;
	for (index = 0; index < SPAWN_HEAT_MAXIMUM_FLASHES && count < maximum; index++)
	{
		long age = now - spawn_heat_globals.flashes[index].tick;

		if (spawn_heat_globals.flashes[index].spawn_index == NONE || age < 0 || age >= SPAWN_HEAT_FLASH_TICKS)
			continue;
		flashes[count].spawn_index = spawn_heat_globals.flashes[index].spawn_index;
		flashes[count].team_index = spawn_heat_globals.flashes[index].team_index;
		flashes[count].age = (real)age / (real)SPAWN_HEAT_FLASH_TICKS;
		count++;
	}

	return count;
}

/* a real spawn: its flash, and in the log the chance it had, as worked
out now for the player who spawned (on the host, the moment it is picked:
the ratings find_best_starting_location_index has just used; on a client,
the moment it learns of it, with the new unit left out) */
static void spawn_heat_spawned(
	long player_index,
	long starting_location_index,
	long ignore_unit_index)
{
	struct player_datum *player = player_try_and_get(player_index);
	long now = game_time_get();
	short index;

	if (!player || starting_location_index < 0 || !spawn_heat_shown())
		return;

	/* (once: a machine that both spawns a player and is told of it) */
	for (index = 0; index < SPAWN_HEAT_MAXIMUM_FLASHES; index++)
	{
		if (spawn_heat_globals.flashes[index].spawn_index == starting_location_index &&
			spawn_heat_globals.flashes[index].player_index == player_index &&
			now - spawn_heat_globals.flashes[index].tick >= 0 &&
			now - spawn_heat_globals.flashes[index].tick < SPAWN_HEAT_FLASH_TICKS)
		{
			return;
		}
	}

	index = spawn_heat_globals.flash_next;
	spawn_heat_globals.flash_next = (short)((index + 1) % SPAWN_HEAT_MAXIMUM_FLASHES);
	spawn_heat_globals.flashes[index].spawn_index = starting_location_index;
	spawn_heat_globals.flashes[index].player_index = player_index;
	spawn_heat_globals.flashes[index].tick = now;
	spawn_heat_globals.flashes[index].team_index = game_engine_has_teams() ? (short)player->team_index : (short)NONE;

	if (spawn_heat_rate(player_index, ignore_unit_index, &spawn_heat_scratch_view) &&
		starting_location_index < spawn_heat_scratch_view.count &&
		spawn_heat_scratch_view.any_rated)
	{
		platform_log("spawn: player %ld at spawn %ld (P was %.1f%%)",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), starting_location_index,
			spawn_heat_scratch_view.spawns[starting_location_index].probability * 100.0f);
	}
	else
	{
		platform_log("spawn: player %ld at spawn %ld (P was unknown)",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), starting_location_index);
	}

	return;
}

void spawn_heat_note_spawn(
	long player_index,
	long starting_location_index)
{
	spawn_heat_spawned(player_index, starting_location_index, NONE);

	return;
}

void spawn_heat_note_unit_attached(
	long player_index,
	long unit_index)
{
	struct scenario *scenario;
	real_point3d origin;
	long best = NONE;
	real best_distance = 0.0f;
	long index;

	if (!spawn_heat_shown() || unit_index == NONE)
		return;

	scenario = global_scenario_get();
	object_get_origin(unit_index, &origin);
	for (index = 0; index < scenario->players.count; index++)
	{
		struct player_starting_location const *location =
			TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);
		real dx = origin.x - location->position.x;
		real dy = origin.y - location->position.y;
		real dz = origin.z - location->position.z;
		real across = (real)sqrt(dx * dx + dy * dy);
		real distance = (real)sqrt(dx * dx + dy * dy + dz * dz);

		if (!game_engine_matches_game_type(location->game_types))
			continue;
		if (distance > SPAWN_HEAT_ATTACH_DISTANCE &&
			(across > SPAWN_HEAT_ATTACH_DISTANCE || dz > 0.0f || -dz > SPAWN_HEAT_ATTACH_FALL))
		{
			continue;
		}
		if (best == NONE || distance < best_distance)
		{
			best = index;
			best_distance = distance;
		}
	}

	if (best != NONE)
		spawn_heat_spawned(player_index, best, unit_index);

	return;
}

/* the chance of each rating winning find_best_starting_location_index's
draw (the highest rating * sqrt(U), U uniform on 0..1). Squared, that is
the highest w * U with w = rating^2, so with the positive weights sorted,
v_0 <= .. <= v_(m-1), spawn k wins with probability

	P_k = (1 / v_k) * integral over y from 0 to v_k of product over j != k of min(1, y / v_j)

which on each piece [v_(s-1), v_s] (v_(-1) = 0) is a power of y, adding

	T_s = (v_s^(m-s) - v_(s-1)^(m-s)) / ((m - s) * product over j >= s of v_j)

for every s <= k, the same for each k: P_k is the sum of T_0 .. T_k. In
logarithms (the exponent at most 0: no overflow). Pure: no game state */
void spawn_heat_probabilities(
	long count,
	real const *ratings,
	real *probabilities)
{
	static long order[SPAWN_HEAT_MAXIMUM_SPAWNS];
	static double weights[SPAWN_HEAT_MAXIMUM_SPAWNS];
	static double suffix_logs[SPAWN_HEAT_MAXIMUM_SPAWNS + 1];
	long positive = 0;
	long index;
	long sorted;
	double total = 0.0;

	if (count > SPAWN_HEAT_MAXIMUM_SPAWNS)
		count = SPAWN_HEAT_MAXIMUM_SPAWNS;
	for (index = 0; index < count; index++)
	{
		probabilities[index] = 0.0f;
		if (ratings[index] > 0.0f)
		{
			double weight = (double)ratings[index] * (double)ratings[index];

			/* (insertion: at most a few hundred, five times a second) */
			for (sorted = positive; sorted > 0 && weights[sorted - 1] > weight; sorted--)
			{
				weights[sorted] = weights[sorted - 1];
				order[sorted] = order[sorted - 1];
			}
			weights[sorted] = weight;
			order[sorted] = index;
			positive++;
		}
	}
	if (positive == 0)
		return;

	suffix_logs[positive] = 0.0;
	for (sorted = positive - 1; sorted >= 0; sorted--)
		suffix_logs[sorted] = suffix_logs[sorted + 1] + log(weights[sorted]);

	for (sorted = 0; sorted < positive; sorted++)
	{
		double upper = weights[sorted];
		double lower = sorted > 0 ? weights[sorted - 1] : 0.0;
		double power = (double)(positive - sorted);
		double piece = exp(power * log(upper) - suffix_logs[sorted]) * (1.0 - pow(lower / upper, power)) / power;

		/* (a run of equal weights: the pieces between them are 0, so each
		of them has the same sum) */
		total += piece;
		probabilities[order[sorted]] = (real)total;
	}

	return;
}

boolean spawn_heat_near_uniform(
	long count,
	real const *probabilities)
{
	real least = 0.0f;
	real most = 0.0f;
	long pickable = 0;
	long index;

	for (index = 0; index < count; index++)
	{
		real probability = probabilities[index];

		if (!(probability > 0.0f))
			continue;
		pickable++;
		if (most == 0.0f || probability < least)
			least = probability;
		if (probability > most)
			most = probability;
	}

	/* (one spawn that can be picked is certain, not uniform) */
	return pickable >= 2 && least >= most * SPAWN_HEAT_UNIFORM_RATIO;
}

/* ---------- private code */

/* every spawn's rating and chance for the player, the unit left out (NONE
for none) into view; FALSE for no player or no scenario */
static boolean spawn_heat_rate(
	long player_index,
	long ignore_unit_index,
	struct spawn_heat_view *view)
{
	struct scenario *scenario = global_scenario_get();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	real probability_maximum = 0.0f;
	real probability_second = 0.0f;
	long hottest = NONE;
	long index;

	if (!player || !scenario)
		return FALSE;

	view->rated_player_index = player_index;
	view->rated_team_index = (short)player->team_index;
	view->count = scenario->players.count;
	if (view->count > SPAWN_HEAT_MAXIMUM_SPAWNS)
	{
		if (!spawn_heat_globals.too_many_logged)
		{
			platform_log("spawn heat: %ld spawns, the first %d rated", view->count, SPAWN_HEAT_MAXIMUM_SPAWNS);
			spawn_heat_globals.too_many_logged = TRUE;
		}
		view->count = SPAWN_HEAT_MAXIMUM_SPAWNS;
	}

	view->any_rated = FALSE;
	for (index = 0; index < view->count; index++)
	{
		struct player_starting_location const *location =
			TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);
		short zero_reason = _spawn_rating_rated;
		real rating = game_engine_get_starting_location_rating_ex(player_index, location, ignore_unit_index,
			&zero_reason);

		if (!(rating > 0.0f))
		{
			rating = 0.0f;
			if (zero_reason == _spawn_rating_rated)
				zero_reason = _spawn_rating_zero_other;
		}
		else
		{
			view->any_rated = TRUE;
		}
		view->spawns[index].rating = rating;
		view->spawns[index].zero_reason = zero_reason;
		spawn_heat_ratings[index] = rating;
	}

	spawn_heat_probabilities(view->count, spawn_heat_ratings, spawn_heat_chances);
	for (index = 0; index < view->count; index++)
	{
		real probability = spawn_heat_chances[index];

		view->spawns[index].probability = probability;
		if (hottest == NONE || probability > probability_maximum)
		{
			if (hottest != NONE)
				probability_second = probability_maximum;
			probability_maximum = probability;
			hottest = index;
		}
		else if (probability > probability_second)
		{
			probability_second = probability;
		}
	}
	view->probability_maximum = probability_maximum;
	view->near_uniform = spawn_heat_near_uniform(view->count, spawn_heat_chances);
	view->hottest = !view->near_uniform && probability_maximum > 0.0f &&
		(double)probability_maximum > (double)probability_second * SPAWN_HEAT_HOTTEST_MARGIN ? hottest : NONE;

	return TRUE;
}
