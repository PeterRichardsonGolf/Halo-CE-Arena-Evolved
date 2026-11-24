#include <stdio.h>
#include <string.h>
#include "ae_host_vote_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void make(struct ae_playlist *list, char const *const *maps, int count)
{
	int i;

	ae_playlist_init(list);
	for (i = 0; i < count; i++)
		ae_playlist_add(list, maps[i], "slayer");
}

static void choose(void)
{
	char const *maps[6] = { "a", "b", "c", "d", "a", "e" };
	struct ae_playlist list;
	unsigned long state = 4242;
	int choices[AE_HOST_VOTE_MAX], trial, n, i, j;

	make(&list, maps, 6);
	for (trial = 0; trial < 500; trial++)
	{
		n = ae_host_vote_choose(&list, 0, NULL, &state, choices);
		CHECK(n == 4);
		/* the last is SAME AGAIN, the three before are on distinct maps, none on the map just played */
		CHECK(choices[3] == 0);
		for (i = 0; i < 3; i++)
		{
			CHECK(strcmp(list.entries[choices[i]].map, "a"));
			for (j = i + 1; j < 3; j++)
				CHECK(strcmp(list.entries[choices[i]].map, list.entries[choices[j]].map));
		}
	}
	/* MAP VOTE off: no vote */
	list.map_vote = 0;
	CHECK(ae_host_vote_choose(&list, 0, NULL, &state, choices) == 0);
	list.map_vote = 1;
	/* two games on other maps are a vote (and SAME AGAIN), one is not */
	{
		char const *few[3] = { "a", "b", "c" };

		make(&list, few, 3);
		CHECK(ae_host_vote_choose(&list, 0, NULL, &state, choices) == 3);
		CHECK(choices[2] == 0);
		{
			unsigned char only_b[3] = { 1, 1, 0 };

			CHECK(ae_host_vote_choose(&list, 0, only_b, &state, choices) == 0);
		}
		/* no game just played: no SAME AGAIN */
		n = ae_host_vote_choose(&list, -1, NULL, &state, choices);
		CHECK(n == 3);
	}
	/* a playlist of one map cannot vote */
	{
		char const *same[3] = { "a", "a", "a" };

		make(&list, same, 3);
		CHECK(ae_host_vote_choose(&list, 0, NULL, &state, choices) == 0);
	}
}

int main(void)
{
	choose();
	if (failures)
		return 1;
	printf("ae_host_vote_rules ok\n");
	return 0;
}
