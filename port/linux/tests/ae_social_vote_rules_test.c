#include <stdio.h>
#include <string.h>
#include "ae_social_vote_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static struct ae_social_playlist list;
static int here_all(void *context, char const *map) { (void)context; (void)map; return 1; }
static int here_not_c(void *context, char const *map) { (void)context; return strcmp(map, "c@smp") != 0; }

static void add(char const *map, char const *gametype, int weight, int fallback)
{
	struct ae_social_entry *e = &list.entry[list.count++];

	snprintf(e->map, sizeof(e->map), "%s", map);
	snprintf(e->gametype, sizeof(e->gametype), "%s", gametype);
	e->weight = (short)weight;
	e->fallback = (short)fallback;
	e->pack = strchr(map, '@') != NULL;
}

static void make_list(void)
{
	memset(&list, 0, sizeof(list));
	add("a@smp", "CTF", 2, 0);
	add("b@smp", "KING", 2, 0);
	add("c@smp", "TERR", 2, 0);
	add("d@smp", "CTF", 2, 0);
	add("a@smp", "KING", 2, 0);
	add("prisoner", "CTF", 1, 1);
}

static void test_choices(void)
{
	struct ae_social_vote_choice out[AE_SOCIAL_VOTE_CHOICES];
	unsigned long seed;
	int count, index, other;

	make_list();
	for (seed = 1; seed <= 300; seed++)
	{
		count = ae_social_vote_pick_choices(&list, here_all, NULL, -1, seed, out, AE_SOCIAL_VOTE_CHOICES);
		EQ(count, 3);
		for (index = 0; index < count; index++)
		{
			CHECK(out[index].entry >= 0 && out[index].entry < list.count);
			CHECK(!strcmp(out[index].map, list.entry[out[index].entry].map));
			CHECK(!strcmp(out[index].gametype, list.entry[out[index].entry].gametype));
			EQ(out[index].weight, list.entry[out[index].entry].weight);
			CHECK(!out[index].map[0] || strcmp(out[index].map, "prisoner"));   /* 4 pack maps here: no fallbacks */
			for (other = 0; other < index; other++)
				CHECK(strcmp(out[index].map, out[other].map));
		}
	}
	/* the same seed, the same choices */
	{
		struct ae_social_vote_choice again[AE_SOCIAL_VOTE_CHOICES];

		ae_social_vote_pick_choices(&list, here_all, NULL, -1, 77, out, 3);
		ae_social_vote_pick_choices(&list, here_all, NULL, -1, 77, again, 3);
		CHECK(!memcmp(out, again, sizeof(out)));
	}
	/* never the map just played while others are on offer (entry 0 is a@smp: entry 4 is the same map) */
	for (seed = 1; seed <= 300; seed++)
	{
		count = ae_social_vote_pick_choices(&list, here_all, NULL, 0, seed, out, 3);
		EQ(count, 3);
		for (index = 0; index < count; index++)
			CHECK(strcmp(out[index].map, "a@smp"));
	}
	/* a map that is not here is not offered: three left (a, b, d) */
	count = ae_social_vote_pick_choices(&list, here_not_c, NULL, -1, 5, out, 3);
	EQ(count, 3);
	for (index = 0; index < count; index++)
		CHECK(strcmp(out[index].map, "c@smp"));
	/* max 2: two */
	EQ(ae_social_vote_pick_choices(&list, here_all, NULL, -1, 5, out, 2), 2);
}

static void test_choices_short(void)
{
	struct ae_social_vote_choice out[AE_SOCIAL_VOTE_CHOICES];

	/* two distinct maps: two choices; one map (two entries): no vote */
	memset(&list, 0, sizeof(list));
	add("a@smp", "CTF", 2, 0);
	add("b@smp", "CTF", 2, 0);
	add("a@smp", "KING", 2, 0);
	EQ(ae_social_vote_pick_choices(&list, here_all, NULL, -1, 3, out, 3), 2);
	memset(&list, 0, sizeof(list));
	add("a@smp", "CTF", 2, 0);
	add("a@smp", "KING", 2, 0);
	EQ(ae_social_vote_pick_choices(&list, here_all, NULL, -1, 3, out, 3), 1);
	memset(&list, 0, sizeof(list));
	EQ(ae_social_vote_pick_choices(&list, here_all, NULL, -1, 3, out, 3), 0);
	/* weights count: a weight 5 map is offered far more often than a weight 1 one */
	{
		int heavy = 0, light = 0;
		unsigned long seed;

		memset(&list, 0, sizeof(list));
		add("h@smp", "CTF", 5, 0);
		add("l@smp", "CTF", 1, 0);
		add("x@smp", "CTF", 5, 0);
		add("y@smp", "CTF", 5, 0);
		for (seed = 1; seed <= 400; seed++)
		{
			int count = ae_social_vote_pick_choices(&list, here_all, NULL, -1, seed, out, 3), each;

			for (each = 0; each < count; each++)
			{
				heavy += !strcmp(out[each].map, "h@smp");
				light += !strcmp(out[each].map, "l@smp");
			}
		}
		CHECK(heavy > light + 100);
	}
}

static void sample(struct ae_social_vote_choice *choices, int weight0, int weight1, int weight2)
{
	int weights[3], at;

	weights[0] = weight0; weights[1] = weight1; weights[2] = weight2;
	memset(choices, 0, 3 * sizeof(*choices));
	for (at = 0; at < 3; at++)
	{
		choices[at].entry = at;
		snprintf(choices[at].map, sizeof(choices[at].map), "m%d", at);
		snprintf(choices[at].gametype, sizeof(choices[at].gametype), "G%d", at);
		choices[at].weight = weights[at];
	}
}

static void test_bots(void)
{
	struct ae_social_vote_choice choices[3];
	struct ae_social_vote vote;
	unsigned long seed;
	int seat, bots_in_time, hits[3] = {0, 0, 0};

	sample(choices, 5, 1, 1);
	for (seed = 1; seed <= 200; seed++)
	{
		ae_social_vote_init(&vote, choices, 3, 8, 1, seed);
		EQ(vote.seats, 8);
		for (seat = 0; seat < 8; seat++)
			EQ(vote.ballot[seat], -1);
		EQ(ae_social_vote_voted(&vote), 0);
		/* every bot's time is 1.5 .. 7 s; nobody votes at 1.4 s, every bot has by 7 s */
		for (seat = 1; seat < 8; seat++)
			CHECK(vote.bot_ms[seat] >= 1500 && vote.bot_ms[seat] <= 7000);
		ae_social_vote_advance(&vote, 1400);
		EQ(ae_social_vote_voted(&vote), 0);
		ae_social_vote_advance(&vote, 7000);
		EQ(ae_social_vote_voted(&vote), 7);
		EQ(vote.ballot[0], -1);
		EQ(vote.closed, 0);
		for (seat = 1; seat < 8; seat++)
		{
			CHECK(vote.ballot[seat] >= 0 && vote.ballot[seat] < 3);
			hits[vote.ballot[seat]]++;
		}
	}
	/* by weight: the weight 5 choice gets most of the bots' votes */
	CHECK(hits[0] > hits[1] * 2 && hits[0] > hits[2] * 2);
	/* the same seed, the same votes; a bot's vote does not move later */
	ae_social_vote_init(&vote, choices, 3, 4, 1, 9);
	{
		struct ae_social_vote other;
		int before[4];

		ae_social_vote_init(&other, choices, 3, 4, 1, 9);
		CHECK(!memcmp(vote.bot_ms, other.bot_ms, sizeof(vote.bot_ms)) && !memcmp(vote.bot_pick, other.bot_pick, sizeof(vote.bot_pick)));
		ae_social_vote_advance(&vote, 7000);
		for (seat = 0; seat < 4; seat++)
			before[seat] = vote.ballot[seat];
		ae_social_vote_advance(&vote, 9000);
		for (seat = 0; seat < 4; seat++)
			EQ(vote.ballot[seat], before[seat]);
	}
	bots_in_time = 0;
	(void)bots_in_time;
}

static void test_cast_and_tally(void)
{
	struct ae_social_vote_choice choices[3];
	struct ae_social_vote vote;

	sample(choices, 2, 2, 2);
	ae_social_vote_init(&vote, choices, 3, 4, 2, 11);
	/* a bot's seat is not a human's to vote; bad choices and seats do nothing */
	EQ(ae_social_vote_cast(&vote, 2, 0), 0);
	EQ(ae_social_vote_cast(&vote, 0, 3), 0);
	EQ(ae_social_vote_cast(&vote, 0, -1), 0);
	EQ(ae_social_vote_cast(&vote, 9, 0), 0);
	EQ(ae_social_vote_cast(&vote, 0, 1), 1);
	EQ(ae_social_vote_cast(&vote, 0, 1), 0);          /* the same again: nothing */
	EQ(ae_social_vote_count(&vote, 1), 1);
	EQ(ae_social_vote_cast(&vote, 0, 2), 1);          /* moves */
	EQ(ae_social_vote_count(&vote, 1), 0);
	EQ(ae_social_vote_count(&vote, 2), 1);
	EQ(ae_social_vote_voted(&vote), 1);
	EQ(ae_social_vote_cast(&vote, 1, 2), 1);
	EQ(ae_social_vote_voter(&vote, 2, 0), 0);
	EQ(ae_social_vote_voter(&vote, 2, 1), 1);
	EQ(ae_social_vote_voter(&vote, 2, 2), -1);
	EQ(ae_social_vote_voter(&vote, 0, 0), -1);
	/* a player who moves comes last in the new choice's tally */
	EQ(ae_social_vote_cast(&vote, 0, 0), 1);
	EQ(ae_social_vote_cast(&vote, 1, 0), 1);
	EQ(ae_social_vote_voter(&vote, 0, 0), 0);
	EQ(ae_social_vote_voter(&vote, 0, 1), 1);
}

static void test_timer_and_close(void)
{
	struct ae_social_vote_choice choices[3];
	struct ae_social_vote vote;

	sample(choices, 2, 2, 2);
	ae_social_vote_init(&vote, choices, 3, 3, 1, 4);
	EQ(ae_social_vote_left_ms(&vote), 10000);
	ae_social_vote_advance(&vote, 2500);
	EQ(ae_social_vote_left_ms(&vote), 7500);
	ae_social_vote_advance(&vote, 2000);                /* never back */
	EQ(ae_social_vote_left_ms(&vote), 7500);
	EQ(vote.closed, 0);
	ae_social_vote_advance(&vote, 10000);
	EQ(vote.closed, 1);
	EQ(ae_social_vote_left_ms(&vote), 0);
	/* the bots all voted by the end; the winner is the most votes */
	EQ(ae_social_vote_voted(&vote), 2);
	CHECK(vote.winner >= 0 && vote.winner < 3);
	EQ(ae_social_vote_cast(&vote, 0, 1), 0);            /* closed */
	EQ(ae_social_vote_close(&vote), 0);

	/* early end: every seat voted, the linger after the last vote */
	ae_social_vote_init(&vote, choices, 3, 3, 1, 4);
	ae_social_vote_advance(&vote, 7000);
	EQ(ae_social_vote_voted(&vote), 2);
	EQ(ae_social_vote_cast(&vote, 0, 0), 1);
	EQ(vote.closed, 0);
	ae_social_vote_advance(&vote, 7999);
	EQ(vote.closed, 0);
	ae_social_vote_advance(&vote, 8000);
	EQ(vote.closed, 1);
	/* changing the vote restarts the linger */
	ae_social_vote_init(&vote, choices, 3, 3, 1, 4);
	ae_social_vote_advance(&vote, 7000);
	ae_social_vote_cast(&vote, 0, 0);
	ae_social_vote_advance(&vote, 7600);
	ae_social_vote_cast(&vote, 0, 1);
	ae_social_vote_advance(&vote, 8100);
	EQ(vote.closed, 0);
	ae_social_vote_advance(&vote, 8600);
	EQ(vote.closed, 1);
	EQ(vote.winner == 1 || ae_social_vote_count(&vote, vote.winner) >= 1, 1);
}

static void test_winner_and_tie(void)
{
	struct ae_social_vote_choice choices[3];
	struct ae_social_vote vote;
	int counts[3], seen[3] = {0, 0, 0}, tie_seen = 0;
	unsigned long seed;

	sample(choices, 2, 2, 2);
	/* the most votes wins, no tie: 3 / 1 / 0 of four bots */
	ae_social_vote_init(&vote, choices, 3, 5, 1, 1);
	counts[0] = 3; counts[1] = 1; counts[2] = 0;
	ae_social_vote_preset(&vote, counts, 3);
	EQ(ae_social_vote_count(&vote, 0), 3);
	EQ(ae_social_vote_count(&vote, 1), 1);
	EQ(ae_social_vote_voted(&vote), 4);
	EQ(ae_social_vote_close(&vote), 1);
	EQ(vote.winner, 0);
	EQ(vote.tie, 0);
	/* the human's vote counts: 2 / 2 with the human on the second: a tie drawn between them only, both seen over seeds */
	for (seed = 1; seed <= 200; seed++)
	{
		ae_social_vote_init(&vote, choices, 3, 4, 1, seed);
		counts[0] = 2; counts[1] = 1; counts[2] = 0;
		ae_social_vote_preset(&vote, counts, 3);
		ae_social_vote_cast(&vote, 0, 1);
		ae_social_vote_close(&vote);
		CHECK(vote.winner == 0 || vote.winner == 1);
		EQ(vote.tie, 1);
		seen[vote.winner]++;
	}
	CHECK(seen[0] > 40 && seen[1] > 40);
	/* a three way tie */
	seen[0] = seen[1] = seen[2] = 0;
	for (seed = 1; seed <= 300; seed++)
	{
		ae_social_vote_init(&vote, choices, 3, 4, 1, seed);
		counts[0] = counts[1] = counts[2] = 1;
		ae_social_vote_preset(&vote, counts, 3);
		ae_social_vote_close(&vote);
		tie_seen += vote.tie;
		seen[vote.winner]++;
	}
	EQ(tie_seen, 300);
	CHECK(seen[0] > 50 && seen[1] > 50 && seen[2] > 50);
	/* the same seed, the same draw */
	{
		struct ae_social_vote one, two;

		ae_social_vote_init(&one, choices, 3, 4, 1, 21);
		ae_social_vote_init(&two, choices, 3, 4, 1, 21);
		ae_social_vote_preset(&one, counts, 3);
		ae_social_vote_preset(&two, counts, 3);
		ae_social_vote_close(&one);
		ae_social_vote_close(&two);
		EQ(one.winner, two.winner);
	}
	/* no votes at all (a lone human): drawn at random from all */
	seen[0] = seen[1] = seen[2] = 0;
	for (seed = 1; seed <= 300; seed++)
	{
		ae_social_vote_init(&vote, choices, 3, 1, 1, seed);
		ae_social_vote_close(&vote);
		EQ(vote.tie, 1);
		seen[vote.winner]++;
	}
	CHECK(seen[0] > 50 && seen[1] > 50 && seen[2] > 50);
	/* close ends it at once: the bots that had not voted vote */
	ae_social_vote_init(&vote, choices, 3, 6, 1, 5);
	ae_social_vote_advance(&vote, 1000);
	EQ(ae_social_vote_voted(&vote), 0);
	ae_social_vote_close(&vote);
	EQ(ae_social_vote_voted(&vote), 5);
	EQ(vote.closed, 1);
}

static void test_fresh_and_preset(void)
{
	struct ae_social_vote_choice choices[3];
	struct ae_social_vote vote;
	int seat, fresh;

	sample(choices, 2, 2, 2);
	ae_social_vote_init(&vote, choices, 3, 4, 1, 8);
	CHECK(!ae_social_vote_fresh(&vote, 0));
	CHECK(!ae_social_vote_fresh(&vote, 1));
	ae_social_vote_advance(&vote, vote.bot_ms[1]);
	/* the bot that just voted is new, for 1.5 s */
	CHECK(vote.ballot[1] >= 0 ? ae_social_vote_fresh(&vote, 1) : 1);
	ae_social_vote_advance(&vote, 9000);
	fresh = 0;
	for (seat = 0; seat < 4; seat++)
		fresh += ae_social_vote_fresh(&vote, seat);
	EQ(fresh, 0);
	/* preset gives only as many as there are bots, and never a human's seat */
	ae_social_vote_init(&vote, choices, 3, 3, 1, 8);
	{
		int counts[3] = {5, 5, 5};

		ae_social_vote_preset(&vote, counts, 3);
	}
	EQ(ae_social_vote_voted(&vote), 2);
	EQ(vote.ballot[0], -1);
	/* a preset is not "new" */
	CHECK(!ae_social_vote_fresh(&vote, 1) && !ae_social_vote_fresh(&vote, 2));
}

int main(void)
{
	test_choices();
	test_choices_short();
	test_bots();
	test_cast_and_tally();
	test_timer_and_close();
	test_winner_and_tie();
	test_fresh_and_preset();
	if (failures)
		return 1;
	printf("ae_social_vote_rules_test: ok\n");
	return 0;
}
