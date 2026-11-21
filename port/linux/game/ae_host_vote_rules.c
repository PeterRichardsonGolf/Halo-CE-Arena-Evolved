/*
AE_HOST_VOTE_RULES.C

See ae_host_vote_rules.h.
*/

#include <string.h>

#include "ae_host_vote_rules.h"
#include "ae_playlist_rules.h"

int ae_host_vote_choose(struct ae_playlist const *list, int current, unsigned char const *playable,
	unsigned long *state, int *choices)
{
	unsigned char eligible[AE_PLAYLIST_ENTRIES];
	int weights[AE_PLAYLIST_ENTRIES];
	int count = 0, index, picked;
	int has_current = current >= 0 && current < list->count;

	if (!list->map_vote || list->count < 2)
		return 0;
	for (index = 0; index < list->count; index++)
	{
		eligible[index] = (unsigned char)((!playable || playable[index]) &&
			!(has_current && !strcmp(list->entries[index].map, list->entries[current].map)));
		weights[index] = list->order == AE_PLAYLIST_ORDER_RANDOM ? list->entries[index].weight : 1;
	}
	while (count < AE_PLAYLIST_VOTE_GAMES)
	{
		int out[1];

		if (ae_pl_sample(weights, eligible, list->count, 1, state, out) < 1)
			break;
		picked = out[0];
		choices[count++] = picked;
		/* (a map once) */
		for (index = 0; index < list->count; index++)
			if (!strcmp(list->entries[index].map, list->entries[picked].map))
				eligible[index] = 0;
	}
	if (count < 2)
		return 0;
	if (has_current && (!playable || playable[current]))
		choices[count++] = current;
	return count;
}
