/*
AE_SPAWNER_POOL_RULES.C

A spawner's pool draws (ae_spawner_pool_rules.h).
*/

#include <string.h>

#include "ae_spawner_pool_rules.h"
#include "ae_spawner_logic_rules.h"

unsigned long ae_spawner_pool_seed(unsigned long game_seed, int spawner_number)
{
	unsigned long seed = (game_seed ^ 0x9E3779B9ul) + (unsigned long)(spawner_number + 1) * 0x85EBCA6Bul;

	seed &= 0xFFFFFFFFul;
	return seed ? seed : 1;
}

unsigned long ae_spawner_pool_random(unsigned long *seed)
{
	*seed = (*seed * 1664525ul + 1013904223ul) & 0xFFFFFFFFul;
	return (*seed >> 8) & 0xFFFFFFul;
}

static void shuffle(struct ae_spawner_pool *pool)
{
	int index;

	for (index = 0; index < pool->count; index++)
		pool->order[index] = (short)index;
	for (index = pool->count - 1; index > 0; index--)
	{
		int k = (int)(ae_spawner_pool_random(&pool->seed) % (unsigned long)(index + 1));
		short swap = pool->order[index];

		pool->order[index] = pool->order[k];
		pool->order[k] = swap;
	}
	/* no weapon twice in a row across a reshuffle */
	if (pool->count > 1 && pool->order[0] == pool->last)
	{
		short swap = pool->order[0];

		pool->order[0] = pool->order[1];
		pool->order[1] = swap;
	}
	pool->next = 0;
}

void ae_spawner_pool_begin(struct ae_spawner_pool *pool, int count, unsigned char const *weights, unsigned long seed)
{
	int index;

	memset(pool, 0, sizeof(*pool));
	if (count < 0) count = 0;
	if (count > AE_SPAWNER_POOL_MAXIMUM) count = AE_SPAWNER_POOL_MAXIMUM;
	pool->count = (short)count;
	for (index = 0; index < count; index++)
	{
		unsigned char w = weights ? weights[index] : 1;

		pool->weights[index] = w < 1 ? 1 : (w > 9 ? 9 : w);
	}
	pool->seed = seed ? seed : 1;
	pool->last = -1;
	pool->ahead = -1;
	shuffle(pool);
}

static int draw_now(struct ae_spawner_pool *pool, int draw);

int ae_spawner_pool_draw(struct ae_spawner_pool *pool, int draw)
{
	int index = pool->ahead;

	if (pool->count <= 0) return -1;
	if (index >= 0 && index < pool->count)
	{
		pool->ahead = -1;
		return index;
	}
	pool->ahead = -1;
	return draw_now(pool, draw);
}

int ae_spawner_pool_peek(struct ae_spawner_pool *pool, int draw)
{
	if (pool->count <= 0) return -1;
	if (pool->ahead < 0 || pool->ahead >= pool->count)
		pool->ahead = (short)draw_now(pool, draw);
	return pool->ahead;
}

static int draw_now(struct ae_spawner_pool *pool, int draw)
{
	int index;

	if (pool->count <= 0) return -1;
	if (pool->count == 1)
	{
		pool->last = 0;
		return 0;
	}
	if (draw == 1)
	{
		unsigned long total = 0, pick;

		for (index = 0; index < pool->count; index++)
			total += pool->weights[index];
		pick = ae_spawner_pool_random(&pool->seed) % total;
		for (index = 0; index < pool->count; index++)
		{
			if (pick < pool->weights[index]) break;
			pick -= pool->weights[index];
		}
		pool->last = (short)index;
		return index;
	}
	if (pool->next >= pool->count)
		shuffle(pool);
	index = pool->order[pool->next++];
	pool->last = (short)index;
	return index;
}

int ae_spawner_pool_tier(unsigned long *seed, short const tier_weight[AE_SPAWNER_TIER_COUNT])
{
	long base = tier_weight[AE_SPAWNER_BASE] < 0 ? 0 : tier_weight[AE_SPAWNER_BASE];
	long power = tier_weight[AE_SPAWNER_POWER] < 0 ? 0 : tier_weight[AE_SPAWNER_POWER];
	unsigned long pick;

	if (base + power <= 0) return AE_SPAWNER_BASE;
	pick = ae_spawner_pool_random(seed) % (unsigned long)(base + power);
	return pick < (unsigned long)base ? AE_SPAWNER_BASE : AE_SPAWNER_POWER;
}

long ae_spawner_pool_beat(unsigned long *seed, long every_ticks, int variance_percent)
{
	long floor_ticks = AE_SPAWNER_RESPAWN_FLOOR_SECONDS * AE_SPAWNER_TICKS_PER_SECOND;
	long spread, beat;

	if (variance_percent < 0) variance_percent = 0;
	if (variance_percent > 90) variance_percent = 90;
	spread = every_ticks * variance_percent / 100;
	beat = every_ticks;
	if (spread > 0)
		beat += (long)(ae_spawner_pool_random(seed) % (unsigned long)(2 * spread + 1)) - spread;
	return beat < floor_ticks ? floor_ticks : beat;
}

static void channel_refill(struct ae_spawner_channel *channel)
{
	int index;

	for (index = 0; index < channel->count; index++)
		channel->order[index] = (short)index;
	for (index = channel->count - 1; index > 0; index--)
	{
		int k = (int)(ae_spawner_pool_random(&channel->seed) % (unsigned long)(index + 1));
		short swap = channel->order[index];

		channel->order[index] = channel->order[k];
		channel->order[k] = swap;
	}
	/* (the next to fire is the end of the list: never the member fired last, with two or more) */
	if (channel->count > 1 && channel->order[channel->count - 1] == channel->last)
	{
		short swap = channel->order[channel->count - 1];

		channel->order[channel->count - 1] = channel->order[0];
		channel->order[0] = swap;
	}
	channel->left = channel->count;
}

void ae_spawner_channel_begin(struct ae_spawner_channel *channel, int count, long every_ticks, int variance_percent,
	long first_at, unsigned long seed)
{
	memset(channel, 0, sizeof(*channel));
	if (count < 0) count = 0;
	if (count > AE_SPAWNER_CHANNEL_MEMBERS) count = AE_SPAWNER_CHANNEL_MEMBERS;
	channel->count = (short)count;
	channel->every_ticks = every_ticks;
	channel->variance_percent = variance_percent;
	channel->beat_at = count > 0 ? (first_at < 0 ? 0 : first_at) : -1;
	channel->last = -1;
	channel->seed = seed ? seed : 1;
	channel_refill(channel);
}

/* the free member nearest the end of the list, taken out of it; -1 none */
static int channel_take(struct ae_spawner_channel *channel, unsigned char const *busy)
{
	int at;

	for (at = channel->left - 1; at >= 0; at--)
	{
		int member = channel->order[at];

		if (busy && busy[member])
			continue;
		memmove(&channel->order[at], &channel->order[at + 1], sizeof(channel->order[0]) * (size_t)(channel->left - 1 - at));
		channel->left--;
		channel->last = (short)member;
		return member;
	}
	return -1;
}

int ae_spawner_channel_step(struct ae_spawner_channel *channel, long now, unsigned char const *busy)
{
	int member;

	if (channel->count <= 0 || channel->beat_at < 0 || now < channel->beat_at)
		return -1;
	channel->beat_at = now + ae_spawner_pool_beat(&channel->seed, channel->every_ticks, channel->variance_percent);
	member = channel_take(channel, busy);
	if (member < 0)
	{
		channel_refill(channel);
		member = channel_take(channel, busy);
	}
	return member;
}
