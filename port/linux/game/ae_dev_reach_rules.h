/*
AE_DEV_REACH_RULES.H

DEV MODE's can't-reach report (`dummy reach`, ae_dev_reach.h; owner 2026-11-15 ask-me: "Can't-reach report"), as pure
rules: walking dummies (the bots' own routes) try every start to every goal of the loaded map, and each walk ends as
reached, stuck, lost, slow or died. No engine includes: port/linux/tests/ae_dev_reach_rules_test.c compiles these alone.

- STARTS: the map's player spawns, a spawn within AE_REACH_MERGE of one kept being that one; at most AE_REACH_STARTS.
- GOALS: flag stands, ball spawns, hill centres, race checkpoints, power items; one within AE_REACH_SAME of a kept goal
  is that one; at most AE_REACH_GOALS.
- PAIRS: each goal from every start (goal by goal, so a cut-short run has still tried every goal from some starts), not
  from a start within AE_REACH_MERGE of it; at most AE_REACH_PAIRS.
- A WALK (its samples, one a tick, from the tick its walker stood at the start):
  - reached: within AE_REACH_AT across and AE_REACH_AT_UP up or down of the goal;
  - stuck: it moved under AE_REACH_MOVED in AE_REACH_STILL_SECONDS (it stands: the spot is where);
  - lost: it moves, but came no AE_REACH_PROGRESS closer to the goal in AE_REACH_LOST_SECONDS (it walks round);
  - slow: its time is over (AE_REACH_BASE_SECONDS + the straight distance at AE_REACH_PACE, at most
    AE_REACH_MOST_SECONDS), with how close it came;
  - died: its unit died on the way.
*/

#ifndef __AE_DEV_REACH_RULES_H
#define __AE_DEV_REACH_RULES_H

#define AE_REACH_TICKS 30
#define AE_REACH_STARTS 12
#define AE_REACH_GOALS 16
#define AE_REACH_PAIRS 96
#define AE_REACH_WALKERS 4
#define AE_REACH_MERGE 8.0f
#define AE_REACH_SAME 1.5f
#define AE_REACH_AT 1.5f
#define AE_REACH_AT_UP 2.0f
#define AE_REACH_MOVED 1.5f
#define AE_REACH_STILL_SECONDS 10.0f
#define AE_REACH_PROGRESS 1.0f
#define AE_REACH_LOST_SECONDS 30.0f
#define AE_REACH_BASE_SECONDS 20.0f
#define AE_REACH_PACE 1.0f
#define AE_REACH_MOST_SECONDS 120.0f
#define AE_REACH_NAME 32

enum
{
	AE_REACH_GOING = 0,
	AE_REACH_REACHED,
	AE_REACH_STUCK,
	AE_REACH_LOST,
	AE_REACH_SLOW,
	AE_REACH_DIED,
	AE_REACH_OUTCOMES
};

struct ae_reach_point
{
	float at[3];
	char name[AE_REACH_NAME];
};

struct ae_reach_plan
{
	int start_count;
	struct ae_reach_point starts[AE_REACH_STARTS];
	int goal_count;
	struct ae_reach_point goals[AE_REACH_GOALS];
	int pair_count;
	unsigned char pair_start[AE_REACH_PAIRS];
	unsigned char pair_goal[AE_REACH_PAIRS];
};

struct ae_reach_walk
{
	int outcome;
	long start_tick;
	long end_tick;
	/* the closest it came to the goal; the distance it was at when it last came AE_REACH_PROGRESS closer, and when */
	float best;
	float mark;
	long closer_tick;
	/* where it last stood still from, and the tick it last moved AE_REACH_MOVED from there */
	float moved_from[3];
	long moved_tick;
	/* its last sample (where a walk ended) */
	float at[3];
};

/* the plan empty; a spawn (kept or merged into a kept one: 1 kept, 0 not); a goal of this name (likewise) */
void ae_reach_clear(struct ae_reach_plan *plan);
int ae_reach_add_start(struct ae_reach_plan *plan, float x, float y, float z);
int ae_reach_add_goal(struct ae_reach_plan *plan, float x, float y, float z, char const *name);
/* the pairs of the starts and goals kept (the rules above): how many */
int ae_reach_make_pairs(struct ae_reach_plan *plan);

/* a walk's time in ticks, from the straight distance of its start to its goal */
long ae_reach_walk_ticks(float const *start, float const *goal);
/* a walk begun at `start` at tick now; then one sample a tick: its outcome so far (AE_REACH_GOING while it goes) */
void ae_reach_walk_begin(struct ae_reach_walk *walk, float const *start, float const *goal, long now);
int ae_reach_walk_sample(struct ae_reach_walk *walk, float const *start, float const *goal, float x, float y, float z,
	int alive, long now);

/* the outcome's word ("reached", "stuck", ...) */
char const *ae_reach_outcome_name(int outcome);
/* a walk's report line (no end of line): the characters written */
int ae_reach_line(char *text, int size, char const *start_name, char const *goal_name, struct ae_reach_walk const *walk);

#endif
