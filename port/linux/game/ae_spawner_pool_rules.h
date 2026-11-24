/*
AE_SPAWNER_POOL_RULES.H

port: AE weapon spawners (0.7.8; design 3.3 `draw`, 3.1 tier ANY, 9.5). How a spawner draws its next weapon from its
pool, pure: a rotation (the pool shuffled, no repeat until it is used up, never the same weapon twice in a row across a
reshuffle) or a weighted random draw; and the tier a spawner of tier ANY plays each spawn, by the set's tier weights.
Every spawner keeps its own random stream (seeded from the game's seed and its number, so a replay and a test repeat),
and the game's random is never touched. No engine includes; port/linux/tests/ae_spawner_pool_rules_test.c compiles this
alone.
*/

#ifndef __AE_SPAWNER_POOL_RULES_H
#define __AE_SPAWNER_POOL_RULES_H

#include "ae_pure_types.h"
#include "ae_spawner_rules.h"

#define AE_SPAWNER_POOL_MAXIMUM 12

struct ae_spawner_pool
{
	short count;
	unsigned char weights[AE_SPAWNER_POOL_MAXIMUM]; /* 1-9 each; a rotation ignores them */
	short order[AE_SPAWNER_POOL_MAXIMUM]; /* the rotation's shuffled indexes */
	short next; /* the rotation's cursor; count = time to reshuffle */
	short last; /* the index drawn last (-1 none) */
	short ahead; /* the next weapon, drawn ahead (ae_spawner_pool_peek; -1 none) */
	unsigned long seed; /* the stream */
};

/* a stream's seed from the game's seed and the spawner's number */
unsigned long ae_spawner_pool_seed(unsigned long game_seed, int spawner_number);

/* the next number of a stream (a linear congruential step; the caller keeps `seed`) */
unsigned long ae_spawner_pool_random(unsigned long *seed);

/* a pool of `count` entries with these weights (NULL: all 1); the rotation starts shuffled */
void ae_spawner_pool_begin(struct ae_spawner_pool *pool, int count, unsigned char const *weights, unsigned long seed);

/* the index of the next weapon: rotate = the design's default, random = weighted; -1 for an empty pool. A weapon drawn
ahead by ae_spawner_pool_peek is the one returned (the stream does not move again) */
int ae_spawner_pool_draw(struct ae_spawner_pool *pool, int draw);

/* the weapon the next draw gives, drawn now and kept (pool.ahead): the spawner's count shows what is coming, and the
rotation's rules hold as if it were drawn at the spawn. -1 for an empty pool */
int ae_spawner_pool_peek(struct ae_spawner_pool *pool, int draw);

/* the tier a tier-ANY spawner plays this spawn, by the set's weights (BASE, POWER); BASE when both are 0 */
int ae_spawner_pool_tier(unsigned long *seed, short const tier_weight[AE_SPAWNER_TIER_COUNT]);

/* a timed-drop channel's next beat in ticks: every * (1 +- variance), never under the floor */
long ae_spawner_pool_beat(unsigned long *seed, long every_ticks, int variance_percent);

#define AE_SPAWNER_CHANNEL_MEMBERS 32

/* a timed-drop channel (design 3.4): its members (the trigger spawners that name it) wait in a shuffled list. On each
beat the last member of the list that is free (no weapon of its own on the map, no spawn pending) is fired and leaves the
list; when no member left in the list is free the list refills, shuffled, and is searched again (never the member fired
last first, when there are two or more). A beat with no free member passes; the next beat is set all the same, every *
(1 +- variance) from this one. */
struct ae_spawner_channel
{
	short count; /* members, 0..AE_SPAWNER_CHANNEL_MEMBERS */
	short order[AE_SPAWNER_CHANNEL_MEMBERS]; /* the members still to fire this round, the next at the end */
	short left; /* how many of order are still to fire */
	short last; /* the member fired last (-1 none) */
	long every_ticks;
	int variance_percent;
	long beat_at; /* the tick of the next beat (-1 none) */
	unsigned long seed;
};

/* a channel of `count` members whose first beat is at `first_at` */
void ae_spawner_channel_begin(struct ae_spawner_channel *channel, int count, long every_ticks, int variance_percent,
	long first_at, unsigned long seed);

/* one tick at `now`, busy[m] nonzero for a member that is not free: the member (0..count-1) to fire now, else -1 */
int ae_spawner_channel_step(struct ae_spawner_channel *channel, long now, unsigned char const *busy);

#endif /* __AE_SPAWNER_POOL_RULES_H */
