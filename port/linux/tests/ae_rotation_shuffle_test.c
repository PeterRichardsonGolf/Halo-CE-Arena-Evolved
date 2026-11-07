#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include "ae_rotation_shuffle.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a pool like the official server's: 6 maps, each in 4 modes */
static char const *const maps6[] = { "a", "b", "c", "d", "e", "f" };
static char const *const modes4[] = { "ae_team_sly", "ae_ctf", "ev_team_sly", "ae_territ" };

static void pass_checks(int count, char const *const *map, char const *const *gt, char const *pm, char const *pg,
	unsigned long seed, int expect_no_repeat, int *mode_repeats)
{
	int order[AE_ROTATION_SHUFFLE_MAXIMUM], seen[AE_ROTATION_SHUFFLE_MAXIMUM] = { 0 }, i;
	int ok = ae_rotation_shuffle_order(count, map, gt, pm, pg, seed, order);
	char key_a[64], key_b[64];

	for (i = 0; i < count; i++)
	{
		CHECK(order[i] >= 0 && order[i] < count);
		if (order[i] >= 0 && order[i] < count)
			seen[order[i]]++;
	}
	for (i = 0; i < count; i++)
		CHECK(seen[i] == 1);
	if (expect_no_repeat)
	{
		CHECK(ok);
		for (i = 0; i < count; i++)
		{
			char const *before = i ? map[order[i - 1]] : pm;

			CHECK(!before || strcmp(before, map[order[i]]));
		}
	}
	for (i = 1; i < count; i++)
	{
		ae_rotation_shuffle_mode_key(gt[order[i - 1]], key_a, sizeof(key_a));
		ae_rotation_shuffle_mode_key(gt[order[i]], key_b, sizeof(key_b));
		if (!strcmp(key_a, key_b))
			(*mode_repeats)++;
	}
}

int main(void)
{
	char const *map[24], *gt[24];
	char key[64];
	unsigned long seed;
	int i, m, mode_repeats = 0, passes = 0, differs = 0, first[24] = { 0 };

	for (m = 0; m < 6; m++)
		for (i = 0; i < 4; i++)
		{
			map[m * 4 + i] = maps6[m];
			gt[m * 4 + i] = modes4[i];
		}
	/* every entry once per pass, never the same map twice in a row, across the boundary too (previous entry
	given), over many seeds and every possible previous entry */
	for (seed = 1; seed <= 400; seed++)
	{
		int previous = (int)(seed % 24);
		int order[24];

		pass_checks(24, map, gt, map[previous], gt[previous], seed, 1, &mode_repeats);
		pass_checks(24, map, gt, NULL, NULL, seed, 1, &mode_repeats);
		ae_rotation_shuffle_order(24, map, gt, NULL, NULL, seed, order);
		first[order[0]]++;
		passes += 2;
	}
	/* the order is a shuffle: the first place is taken by many different entries, and two seeds give different orders */
	for (i = 0; i < 24; i++)
		differs += first[i] > 0;
	CHECK(differs >= 12);
	/* modes: where the pool allows it, not the same mode twice in a row (6 maps x 4 modes allows it nearly always) */
	printf("mode repeats %d of %d neighbour pairs\n", mode_repeats, passes * 23);
	CHECK(mode_repeats < passes * 23 / 6);
	/* a small rotation: two maps */
	{
		char const *m2[] = { "x", "y", "x", "y" }, *g2[] = { "ae_ctf", "ae_ctf", "ae_team_sly", "ae_team_sly" };

		for (seed = 1; seed <= 100; seed++)
			pass_checks(4, m2, g2, "x", "ae_ctf", seed, 1, &mode_repeats);
	}
	/* one map alone cannot avoid a repeat: every entry still once, and it says so */
	{
		char const *m1[] = { "x", "x", "x" }, *g1[] = { "ae_ctf", "ae_team_sly", "ae_territ" };
		int order[3], seen[3] = { 0 }, ok;

		ok = ae_rotation_shuffle_order(3, m1, g1, "x", "ae_ctf", 7, order);
		CHECK(!ok);
		for (i = 0; i < 3; i++)
			seen[order[i]]++;
		CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
	}
	/* one entry */
	{
		char const *m1[] = { "x" }, *g1[] = { "ae_ctf" };
		int order[1];

		ae_rotation_shuffle_order(1, m1, g1, NULL, NULL, 3, order);
		CHECK(order[0] == 0);
		CHECK(!ae_rotation_shuffle_order(0, m1, g1, NULL, NULL, 3, order));
	}
	/* more than half one map: the best order found, still every entry once */
	{
		char const *m3[] = { "x", "x", "x", "y" }, *g3[] = { "a", "b", "c", "d" };
		int order[4], seen[4] = { 0 };

		CHECK(!ae_rotation_shuffle_order(4, m3, g3, NULL, NULL, 5, order));
		for (i = 0; i < 4; i++)
			seen[order[i]]++;
		CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1 && seen[3] == 1);
	}
	/* the vote: the entry just played is not a choice when there are two or more; with no votes the first choice
	that is not the entry just played wins */
	{
		long offered[4] = { 5, 2, 9, 4 }, same_first[3] = { 7, 3, 1 }, only[2] = { 6, 6 };

		CHECK(!ae_rotation_vote_drop_played(0) && !ae_rotation_vote_drop_played(1) && ae_rotation_vote_drop_played(2) &&
			ae_rotation_vote_drop_played(10));
		CHECK(ae_rotation_vote_default(offered, 4, 5) == 1 && ae_rotation_vote_default(offered, 4, 2) == 0 &&
			ae_rotation_vote_default(offered, 4, 9) == 0 && ae_rotation_vote_default(offered, 4, -1) == 0);
		CHECK(ae_rotation_vote_default(same_first, 3, 7) == 1 && ae_rotation_vote_default(only, 2, 6) == 0);
		CHECK(ae_rotation_vote_default(offered, 0, 5) == 0);
	}
	/* the rotation word of an AE gametype */
	{
		char word[12];

		CHECK(ae_rotation_word_of("AE TEAM SLY", word, sizeof(word)) == 11 && !strcmp(word, "ae_team_sly"));
		CHECK(ae_rotation_word_of("AE CTF", word, sizeof(word)) == 6 && !strcmp(word, "ae_ctf"));
		{
			char wide[24];

			CHECK(ae_rotation_word_of("AE BTB+ CTF", wide, sizeof(wide)) == 15 && !strcmp(wide, "ae_btb_plus_ctf"));
			CHECK(ae_rotation_word_of("AE BR BTB+C", wide, sizeof(wide)) == 15 && !strcmp(wide, "ae_br_btb_plusc"));
		}
		CHECK(ae_rotation_word_of("TOO LONG NAME", word, sizeof(word)) == 0 && !word[0]);
		CHECK(ae_rotation_word_of(NULL, word, sizeof(word)) == 0 && ae_rotation_word_of("A", word, 1) == 0);
	}
	/* mode keys */
	ae_rotation_shuffle_mode_key("ae_team_sly", key, sizeof(key)); CHECK(!strcmp(key, "slayer"));
	ae_rotation_shuffle_mode_key("AE_BR_TS", key, sizeof(key)); CHECK(!strcmp(key, "slayer"));
	ae_rotation_shuffle_mode_key("ev_team_sly", key, sizeof(key)); CHECK(!strcmp(key, "slayer"));
	ae_rotation_shuffle_mode_key("ae_ctf", key, sizeof(key)); CHECK(!strcmp(key, "ctf"));
	ae_rotation_shuffle_mode_key("ae_comp_ctf", key, sizeof(key)); CHECK(!strcmp(key, "ctf"));
	ae_rotation_shuffle_mode_key("ae_territ", key, sizeof(key)); CHECK(!strcmp(key, "territ"));
	ae_rotation_shuffle_mode_key("", key, sizeof(key)); CHECK(!strcmp(key, "slayer"));
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_rotation_shuffle_test: ok\n");
	return failures != 0;
}
