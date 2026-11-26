#include <stdio.h>
#include <string.h>

#include "ae_spawner_pool_rules.h"
#include "ae_spawner_logic_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* A rotation: every weapon once before any twice; never the same twice in a row across the reshuffle; a seed repeats. */
static void test_rotation(void)
{
	struct ae_spawner_pool pool, again;
	int counts[4], index, draws[40], last = -1;

	ae_spawner_pool_begin(&pool, 4, 0, ae_spawner_pool_seed(1234, 7));
	ae_spawner_pool_begin(&again, 4, 0, ae_spawner_pool_seed(1234, 7));
	for (index = 0; index < 40; index++)
	{
		draws[index] = ae_spawner_pool_draw(&pool, 0);
		CHECK(draws[index] >= 0 && draws[index] < 4);
		CHECK(draws[index] != last);
		CHECK(ae_spawner_pool_draw(&again, 0) == draws[index]);	/* the same seed, the same sequence */
		last = draws[index];
	}
	/* every block of 4 holds each weapon once */
	for (index = 0; index < 40; index += 4)
	{
		int k;

		memset(counts, 0, sizeof counts);
		for (k = 0; k < 4; k++) counts[draws[index + k]]++;
		CHECK(counts[0] == 1 && counts[1] == 1 && counts[2] == 1 && counts[3] == 1);
	}
	/* another spawner's seed gives another order (not a proof, a smoke check over 40 draws) */
	{
		struct ae_spawner_pool other;
		int same = 1;

		ae_spawner_pool_begin(&other, 4, 0, ae_spawner_pool_seed(1234, 8));
		for (index = 0; index < 40; index++)
			if (ae_spawner_pool_draw(&other, 0) != draws[index]) same = 0;
		CHECK(!same);
	}
	/* one weapon: always it; none: -1; more than the maximum: cut */
	ae_spawner_pool_begin(&pool, 1, 0, 5);
	CHECK(ae_spawner_pool_draw(&pool, 0) == 0 && ae_spawner_pool_draw(&pool, 0) == 0);
	ae_spawner_pool_begin(&pool, 0, 0, 5);
	CHECK(ae_spawner_pool_draw(&pool, 0) == -1 && ae_spawner_pool_draw(&pool, 1) == -1);
	ae_spawner_pool_begin(&pool, 40, 0, 5);
	CHECK(pool.count == AE_SPAWNER_POOL_MAXIMUM);
	ae_spawner_pool_begin(&pool, 2, 0, 0);	/* a zero seed still runs */
	CHECK(pool.seed != 0);
	for (index = 0; index < 20; index++)
	{
		int d = ae_spawner_pool_draw(&pool, 0);

		CHECK(d != last || index == 0);
		last = d;
	}
}

/* A weighted random draw: a weight-3 entry comes up about three times as often. */
static void test_random(void)
{
	struct ae_spawner_pool pool;
	unsigned char weights[3] = { 3, 1, 0 };	/* 0 reads as 1 */
	int counts[3] = { 0, 0, 0 }, index;

	ae_spawner_pool_begin(&pool, 3, weights, 99);
	CHECK(pool.weights[2] == 1);
	for (index = 0; index < 500; index++)
	{
		int d = ae_spawner_pool_draw(&pool, 1);

		CHECK(d >= 0 && d < 3);
		counts[d]++;
	}
	CHECK(counts[0] > 240 && counts[0] < 360);	/* 3 of 5 = 300 */
	CHECK(counts[1] > 50 && counts[1] < 150);	/* 1 of 5 = 100 */
	CHECK(counts[2] > 50 && counts[2] < 150);
	weights[0] = 20;
	ae_spawner_pool_begin(&pool, 1, weights, 99);
	CHECK(pool.weights[0] == 9);
}

/* The tier of a tier-ANY spawner by the set's weights; the channel's beat within its variance. */
static void test_tier_and_beat(void)
{
	short weight[AE_SPAWNER_TIER_COUNT] = { 70, 30, 0 };
	unsigned long seed = 7;
	int power = 0, index;

	for (index = 0; index < 1000; index++)
		if (ae_spawner_pool_tier(&seed, weight) == AE_SPAWNER_POWER) power++;
	CHECK(power > 240 && power < 360);
	weight[0] = 0;
	weight[1] = 0;
	CHECK(ae_spawner_pool_tier(&seed, weight) == AE_SPAWNER_BASE);
	weight[1] = 5;
	CHECK(ae_spawner_pool_tier(&seed, weight) == AE_SPAWNER_POWER);
	weight[0] = -3;
	CHECK(ae_spawner_pool_tier(&seed, weight) == AE_SPAWNER_POWER);
	for (index = 0; index < 200; index++)
	{
		long beat = ae_spawner_pool_beat(&seed, 120 * AE_SPAWNER_TICKS_PER_SECOND, 25);

		CHECK(beat >= 90 * AE_SPAWNER_TICKS_PER_SECOND && beat <= 150 * AE_SPAWNER_TICKS_PER_SECOND);
	}
	CHECK(ae_spawner_pool_beat(&seed, 120 * AE_SPAWNER_TICKS_PER_SECOND, 0) == 120 * AE_SPAWNER_TICKS_PER_SECOND);
	CHECK(ae_spawner_pool_beat(&seed, 60, 50) == 5 * AE_SPAWNER_TICKS_PER_SECOND);	/* the floor */
	CHECK(ae_spawner_pool_beat(&seed, 100 * AE_SPAWNER_TICKS_PER_SECOND, 200) <= 190 * AE_SPAWNER_TICKS_PER_SECOND);
	CHECK(ae_spawner_pool_seed(0, 0) != 0);
	CHECK(ae_spawner_pool_seed(5, 1) != ae_spawner_pool_seed(5, 2));
	CHECK(ae_spawner_pool_seed(5, 1) == ae_spawner_pool_seed(5, 1));
}

/* A weapon drawn ahead (the ghost shows it while the count runs) is the one the spawn gives; peeking twice is once; a
pool drawn with peeks between gives the same sequence as one drawn plainly, so the rotation's rules hold. */
static void test_peek(void)
{
	int draw;

	for (draw = 0; draw < 2; draw++)
	{
		struct ae_spawner_pool plain, ahead;
		int index;

		ae_spawner_pool_begin(&plain, 5, 0, ae_spawner_pool_seed(99, 3));
		ae_spawner_pool_begin(&ahead, 5, 0, ae_spawner_pool_seed(99, 3));
		CHECK(ahead.ahead == -1);
		for (index = 0; index < 50; index++)
		{
			int next = ae_spawner_pool_peek(&ahead, draw);

			CHECK(next >= 0 && next < 5);
			CHECK(ae_spawner_pool_peek(&ahead, draw) == next);
			CHECK(ae_spawner_pool_draw(&ahead, draw) == next);
			CHECK(ahead.ahead == -1);
			CHECK(ae_spawner_pool_draw(&plain, draw) == next);
		}
	}
	{
		struct ae_spawner_pool pool;

		ae_spawner_pool_begin(&pool, 0, 0, 4);
		CHECK(ae_spawner_pool_peek(&pool, 0) == -1);
		ae_spawner_pool_begin(&pool, 1, 0, 4);
		CHECK(ae_spawner_pool_peek(&pool, 0) == 0 && ae_spawner_pool_draw(&pool, 0) == 0);
		/* a begin forgets what was drawn ahead */
		ae_spawner_pool_begin(&pool, 3, 0, 4);
		(void)ae_spawner_pool_peek(&pool, 0);
		ae_spawner_pool_begin(&pool, 3, 0, 4);
		CHECK(pool.ahead == -1);
	}
}

/* A timed-drop channel: the first beat at its start, then every * (1 +- variance); each member once a round from the
end of a shuffled list; a busy member is passed over; a beat with none free passes and the next is still set. */
static void test_channel(void)
{
	struct ae_spawner_channel channel;
	unsigned char busy[AE_SPAWNER_CHANNEL_MEMBERS];
	int seen[3], index, member, round;
	long now;
	long const every = 120 * AE_SPAWNER_TICKS_PER_SECOND;

	memset(busy, 0, sizeof(busy));
	ae_spawner_channel_begin(&channel, 3, every, 25, 0, 11);
	CHECK(channel.beat_at == 0 && channel.left == 3);
	/* a round: each member exactly once, one a beat; nothing between the beats */
	for (round = 0; round < 4; round++)
	{
		memset(seen, 0, sizeof(seen));
		for (index = 0; index < 3; index++)
		{
			now = channel.beat_at;
			CHECK(ae_spawner_channel_step(&channel, now - 1, busy) == -1);
			member = ae_spawner_channel_step(&channel, now, busy);
			CHECK(member >= 0 && member < 3);
			if (member >= 0 && member < 3) seen[member]++;
			CHECK(channel.beat_at >= now + 90 * AE_SPAWNER_TICKS_PER_SECOND &&
				channel.beat_at <= now + 150 * AE_SPAWNER_TICKS_PER_SECOND);
			CHECK(ae_spawner_channel_step(&channel, now, busy) == -1);	/* once a beat */
		}
		CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
	}
	/* no member twice in a row across a refill */
	ae_spawner_channel_begin(&channel, 2, every, 0, 0, 5);
	member = -1;
	for (index = 0; index < 40; index++)
	{
		int fired = ae_spawner_channel_step(&channel, channel.beat_at, busy);

		CHECK(fired != member);
		member = fired;
	}
	/* a busy member is passed over: the free one fires */
	ae_spawner_channel_begin(&channel, 3, every, 0, 30, 9);
	CHECK(ae_spawner_channel_step(&channel, 29, busy) == -1);
	busy[0] = busy[2] = 1;
	CHECK(ae_spawner_channel_step(&channel, 30, busy) == 1);
	CHECK(channel.beat_at == 30 + every);
	/* every member busy: the beat passes, the next is set */
	busy[1] = 1;
	CHECK(ae_spawner_channel_step(&channel, 30 + every, busy) == -1);
	CHECK(channel.beat_at == 30 + 2 * every);
	/* the one that fired this round is free again (its weapon taken): the list refills for it */
	busy[1] = 0;
	CHECK(ae_spawner_channel_step(&channel, 30 + 2 * every, busy) == 1);
	/* one member: it drops on every beat it is free */
	memset(busy, 0, sizeof(busy));
	ae_spawner_channel_begin(&channel, 1, every, 0, 0, 3);
	CHECK(ae_spawner_channel_step(&channel, 0, busy) == 0);
	CHECK(ae_spawner_channel_step(&channel, every, busy) == 0);
	busy[0] = 1;
	CHECK(ae_spawner_channel_step(&channel, 2 * every, busy) == -1);
	/* no members: never */
	ae_spawner_channel_begin(&channel, 0, every, 0, 0, 3);
	CHECK(channel.beat_at == -1 && ae_spawner_channel_step(&channel, 0, busy) == -1);
	/* the floor on a beat */
	busy[0] = 0;
	ae_spawner_channel_begin(&channel, 1, 10, 0, 0, 3);
	CHECK(ae_spawner_channel_step(&channel, 0, busy) == 0 && channel.beat_at == 5 * AE_SPAWNER_TICKS_PER_SECOND);
}

/* RANDOMIZE's shared bag: the racks of one class draw in turn from one rotation, so four racks over a four weapon
class hold four different weapons, and a fifth rack starts the next turn of the bag */
static void test_shared_bag(void)
{
	struct ae_spawner_pool bag;
	int seen[4] = { 0, 0, 0, 0 }, rack, pick;

	ae_spawner_pool_begin(&bag, 4, NULL, ae_spawner_pool_seed(12345UL, 300));
	for (rack = 0; rack < 4; rack++)
	{
		pick = ae_spawner_pool_draw(&bag, 0);
		CHECK(pick >= 0 && pick < 4);
		if (pick >= 0 && pick < 4)
			seen[pick]++;
	}
	CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1 && seen[3] == 1);
	pick = ae_spawner_pool_draw(&bag, 0);
	CHECK(pick >= 0 && pick < 4);
	/* (the same game and round seed: the same draws on every machine) */
	{
		struct ae_spawner_pool a, b;
		int k;

		ae_spawner_pool_begin(&a, 6, NULL, ae_spawner_pool_seed(777UL, 301));
		ae_spawner_pool_begin(&b, 6, NULL, ae_spawner_pool_seed(777UL, 301));
		for (k = 0; k < 6; k++)
			CHECK(ae_spawner_pool_draw(&a, 0) == ae_spawner_pool_draw(&b, 0));
	}
}

int main(void)
{
	test_shared_bag();
	test_rotation();
	test_random();
	test_tier_and_beat();
	test_channel();
	test_peek();
	printf("%s: %d checks, %d failures\n", __FILE__, checks, failures);
	return failures ? 1 : 0;
}
