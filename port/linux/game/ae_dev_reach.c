/*
AE_DEV_REACH.C

DEV MODE's can't-reach report (ae_dev_reach.h; the rules: ae_dev_reach_rules.c): the map's starts and goals, walkers
given their walks, each walk's samples, the log lines and the report file.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/item_timers.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "../src/port_config.h"
#include "../src/posix.h"

/* (the config folder is a host path: not the game's Xbox-path file calls) */
#undef fopen

#include "ae_layout.h"
#include "ae_dev_dummy.h"
#include "ae_dev_reach.h"
#include "ae_dev_reach_rules.h"

void platform_log(char const *format, ...);

#define LINE 160
#define HEADER 96
/* a walker not put at its start this long after it was given the walk: the walk skipped */
#define NOT_PUT_TICKS (10 * AE_REACH_TICKS)

static struct
{
	boolean running;
	struct ae_reach_plan plan;
	char header[HEADER];
	int next_pair;
	int done;
	int counts[AE_REACH_OUTCOMES];
	int walker_count;
	struct
	{
		long bot_index;
		/* its walk (a pair), -1 when it has none left */
		int pair;
		/* the tick it was given the walk: the walk begins once it was put at the start after it */
		long given_tick;
		boolean walking;
		struct ae_reach_walk walk;
	} walkers[AE_REACH_WALKERS];
	int outcome[AE_REACH_PAIRS];
	char lines[AE_REACH_PAIRS][LINE];
} reach;

boolean ae_dev_reach_running(void)
{
	return reach.running;
}

/* the hills' flags averaged, hill by hill (a hill's flags share its team index) */
static void add_hills(struct scenario *scenario)
{
	short hill;

	for (hill = 0; hill < 16; hill++)
	{
		float sum[3] = { 0.0f, 0.0f, 0.0f };
		long index, count = 0;
		char name[AE_REACH_NAME];

		for (index = 0; index < scenario->netgame_flags.count; index++)
		{
			struct scenario_netgame_flag *flag =
				TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

			if (flag->type != _netgame_flag_hill || flag->team_index != hill)
				continue;
			sum[0] += flag->position.x;
			sum[1] += flag->position.y;
			sum[2] += flag->position.z;
			count++;
		}
		if (!count)
			continue;
		snprintf(name, sizeof name, "hill %d", hill + 1);
		ae_reach_add_goal(&reach.plan, sum[0] / count, sum[1] / count, sum[2] / count, name);
	}
}

static void add_flags_of(struct scenario *scenario, short type)
{
	long index, number = 0;

	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag *flag =
			TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);
		char name[AE_REACH_NAME];

		if (flag->type != type)
			continue;
		number++;
		if (type == _netgame_flag_ctf_flag)
			snprintf(name, sizeof name, "%s flag", flag->team_index == 0 ? "red" : flag->team_index == 1 ? "blue" : "other");
		else if (type == _netgame_flag_oddball_ball_spawn)
			snprintf(name, sizeof name, "ball %ld", number);
		else
			snprintf(name, sizeof name, "race %d", flag->team_index + 1);
		ae_reach_add_goal(&reach.plan, flag->position.x, flag->position.y, flag->position.z, name);
	}
}

static void add_power_items(void)
{
	short index, count = item_timers_count();

	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		char name[AE_REACH_NAME];
		int letter;

		if (!timer)
			continue;
		/* (the power list's label, in ASCII: platform_log has no wide strings) */
		for (letter = 0; letter < 23 && timer->label[letter]; letter++)
			name[letter] = timer->label[letter] < 128 ? (char)timer->label[letter] : '?';
		name[letter] = 0;
		if (timer->number > 0 && letter < AE_REACH_NAME - 4)
			snprintf(name + letter, sizeof name - (size_t)letter, " %d", timer->number);
		ae_reach_add_goal(&reach.plan, timer->position.x, timer->position.y, timer->position.z, name[0] ? name : "item");
	}
}

static void make_plan(void)
{
	struct scenario *scenario = global_scenario_get();
	long index;

	ae_reach_clear(&reach.plan);
	for (index = 0; index < scenario->players.count; index++)
	{
		struct player_starting_location *start =
			TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);

		ae_reach_add_start(&reach.plan, start->position.x, start->position.y, start->position.z);
	}
	/* (the objectives first: the goals' list is capped) */
	add_flags_of(scenario, _netgame_flag_ctf_flag);
	add_hills(scenario);
	add_flags_of(scenario, _netgame_flag_oddball_ball_spawn);
	add_power_items();
	add_flags_of(scenario, _netgame_flag_race_track);
	ae_reach_make_pairs(&reach.plan);
}

static void pair_points(int pair, float const **start, float const **goal)
{
	*start = reach.plan.starts[reach.plan.pair_start[pair]].at;
	*goal = reach.plan.goals[reach.plan.pair_goal[pair]].at;
}

int ae_dev_reach_start(long player_index, int walkers, long *bots_wanted, char *error, int error_size)
{
	int index;

	if (reach.running)
	{
		snprintf(error, (size_t)error_size, "a reach run is going: dummy clear stops it");
		return 0;
	}
	if (ae_dev_dummy_count())
	{
		snprintf(error, (size_t)error_size, "dummies are in the game: dummy clear first");
		return 0;
	}
	memset(&reach, 0, sizeof reach);
	make_plan();
	if (!reach.plan.pair_count)
	{
		snprintf(error, (size_t)error_size, "the map has no spawn and goal to walk between (%d spawns, %d goals)",
			reach.plan.start_count, reach.plan.goal_count);
		return 0;
	}
	if (!ae_layout_map_header(reach.header, (long)sizeof reach.header))
		snprintf(reach.header, sizeof reach.header, "layout ? map");
	if (walkers < 1)
		walkers = 1;
	if (walkers > AE_REACH_WALKERS)
		walkers = AE_REACH_WALKERS;
	if (walkers > reach.plan.pair_count)
		walkers = reach.plan.pair_count;
	{
		float starts[AE_REACH_WALKERS][3], goals[AE_REACH_WALKERS][3];
		long bots[AE_REACH_WALKERS];
		int made;

		for (index = 0; index < walkers; index++)
		{
			float const *start, *goal;

			pair_points(index, &start, &goal);
			memcpy(starts[index], start, sizeof starts[index]);
			memcpy(goals[index], goal, sizeof goals[index]);
		}
		made = ae_dev_dummy_add_walkers(player_index, walkers, (float const (*)[3])starts, (float const (*)[3])goals, bots,
			bots_wanted, error, error_size);
		if (!made)
			return 0;
		for (index = 0; index < made; index++)
		{
			reach.walkers[index].bot_index = bots[index];
			reach.walkers[index].pair = index;
			reach.walkers[index].given_tick = game_time_get();
		}
		walkers = made;
	}
	reach.walker_count = walkers;
	reach.next_pair = walkers;
	reach.running = TRUE;
	platform_log("dev reach: %s: %d walks (%d spawns, %d goals), %d walkers", reach.header, reach.plan.pair_count,
		reach.plan.start_count, reach.plan.goal_count, reach.walker_count);
	for (index = 0; index < reach.plan.goal_count; index++)
		platform_log("dev reach: goal %s at (%.1f %.1f %.1f)", reach.plan.goals[index].name, reach.plan.goals[index].at[0],
			reach.plan.goals[index].at[1], reach.plan.goals[index].at[2]);
	return reach.plan.pair_count;
}

/* the walks skipped (their walker not put at the spawn) */
static int skipped(void)
{
	int outcome, counted = 0;

	for (outcome = 0; outcome < AE_REACH_OUTCOMES; outcome++)
		counted += reach.counts[outcome];
	return reach.done - counted;
}

/* the report in the config folder's reach/<map file>.txt */
static void write_report(void)
{
	char folder[400], made[420], file[64], path[500];
	int version, index;
	unsigned int hash;
	char *c, *dot;
	FILE *out;

	if (sscanf(reach.header, "layout %d %63s %x", &version, file, &hash) != 3)
		snprintf(file, sizeof file, "map");
	for (c = file; *c; c++)
	{
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-' ||
			*c == '.'))
			*c = '_';
	}
	dot = strrchr(file, '.');
	if (dot && !strcmp(dot, ".map"))
		*dot = 0;
	config_folder(folder, sizeof folder);
	snprintf(made, sizeof made, "%s/reach", folder);
	posix_make_directory(made);
	snprintf(path, sizeof path, "%s/%s.txt", made, file);
	out = fopen(path, "wb");
	if (!out)
	{
		platform_log("dev reach: %s could not be written", path);
		return;
	}
	fprintf(out, "%s\n", reach.header);
	fprintf(out, "%d of %d walks: %d reached, %d stuck, %d lost, %d slow, %d died, %d skipped\n", reach.done,
		reach.plan.pair_count, reach.counts[AE_REACH_REACHED], reach.counts[AE_REACH_STUCK], reach.counts[AE_REACH_LOST],
		reach.counts[AE_REACH_SLOW], reach.counts[AE_REACH_DIED], skipped());
	fprintf(out, "\nnot reached:\n");
	for (index = 0; index < reach.plan.pair_count; index++)
	{
		if (reach.lines[index][0] && reach.outcome[index] != AE_REACH_REACHED)
			fprintf(out, "%s\n", reach.lines[index]);
	}
	fprintf(out, "\nreached:\n");
	for (index = 0; index < reach.plan.pair_count; index++)
	{
		if (reach.lines[index][0] && reach.outcome[index] == AE_REACH_REACHED)
			fprintf(out, "%s\n", reach.lines[index]);
	}
	fclose(out);
	platform_log("dev reach: the report is %s", path);
}

static void finish(char const *why)
{
	int index;

	if (!reach.running)
		return;
	reach.running = FALSE;
	platform_log("dev reach: %s: %d of %d walks: %d reached, %d stuck, %d lost, %d slow, %d died, %d skipped", why,
		reach.done, reach.plan.pair_count, reach.counts[AE_REACH_REACHED], reach.counts[AE_REACH_STUCK],
		reach.counts[AE_REACH_LOST], reach.counts[AE_REACH_SLOW], reach.counts[AE_REACH_DIED], skipped());
	for (index = 0; index < reach.plan.pair_count; index++)
	{
		if (reach.lines[index][0] && reach.outcome[index] != AE_REACH_REACHED)
			platform_log("dev reach: not reached: %s", reach.lines[index]);
	}
	if (reach.done)
		write_report();
}

void ae_dev_reach_stop(void)
{
	finish("stopped");
}

/* the walker's next walk, or none left (and the run done when no walker has one) */
static void next_walk(int index, long bot_index, long now)
{
	float const *start, *goal;

	reach.walkers[index].walking = FALSE;
	if (reach.next_pair < reach.plan.pair_count)
	{
		reach.walkers[index].pair = reach.next_pair++;
		reach.walkers[index].given_tick = now + 1;
		pair_points(reach.walkers[index].pair, &start, &goal);
		ae_dev_dummy_walk_retarget(bot_index, start, goal);
		return;
	}
	reach.walkers[index].pair = -1;
	for (index = 0; index < reach.walker_count; index++)
	{
		if (reach.walkers[index].pair >= 0)
			return;
	}
	finish("done");
}

void ae_dev_reach_sample(long bot_index, float x, float y, float z, boolean alive, long placed_tick, long now)
{
	int index, outcome;
	float const *start, *goal;

	if (!reach.running)
		return;
	for (index = 0; index < reach.walker_count && reach.walkers[index].bot_index != bot_index; index++)
		;
	if (index >= reach.walker_count || reach.walkers[index].pair < 0)
		return;
	pair_points(reach.walkers[index].pair, &start, &goal);
	if (!reach.walkers[index].walking)
	{
		/* (the walk begins once it stands at its start, put there after it was given the walk) */
		if (alive && placed_tick >= reach.walkers[index].given_tick)
		{
			ae_reach_walk_begin(&reach.walkers[index].walk, start, goal, now);
			reach.walkers[index].walking = TRUE;
		}
		/* (never put at its start: no room there, ae_dev_dummy's place) */
		else if (now - reach.walkers[index].given_tick > NOT_PUT_TICKS)
		{
			int pair = reach.walkers[index].pair;

			snprintf(reach.lines[pair], LINE, "%s -> %s: skipped (the walker could not be put at the spawn)",
				reach.plan.starts[reach.plan.pair_start[pair]].name, reach.plan.goals[reach.plan.pair_goal[pair]].name);
			reach.outcome[pair] = AE_REACH_GOING;
			reach.done++;
			platform_log("dev reach: %d/%d %s", reach.done, reach.plan.pair_count, reach.lines[pair]);
			next_walk(index, bot_index, now);
		}
		return;
	}
	outcome = ae_reach_walk_sample(&reach.walkers[index].walk, start, goal, x, y, z, alive ? 1 : 0, now);
	if (outcome == AE_REACH_GOING)
		return;
	{
		int pair = reach.walkers[index].pair;

		ae_reach_line(reach.lines[pair], LINE, reach.plan.starts[reach.plan.pair_start[pair]].name,
			reach.plan.goals[reach.plan.pair_goal[pair]].name, &reach.walkers[index].walk);
		reach.outcome[pair] = outcome;
		reach.counts[outcome]++;
		reach.done++;
		platform_log("dev reach: %d/%d %s", reach.done, reach.plan.pair_count, reach.lines[pair]);
	}
	next_walk(index, bot_index, now);
}
