/*
AE_ROTATION_SHUFFLE.C

See ae_rotation_shuffle.h. A random order is made, then built again entry by entry: each place takes, at random,
an entry of another map and another mode than the one before it; failing that, of another map; failing that, any
(the pool had nothing else). A last repair swaps a neighbouring pair of one map with an entry that fixes it.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_rotation_shuffle.h"

void ae_rotation_shuffle_mode_key(
	char const *gametype,
	char *key,
	int key_size)
{
	char lower[96];
	int length = 0;
	int index = 0;

	if (key_size < 1)
		return;
	key[0] = 0;
	if (!gametype)
		return;
	for (; gametype[index] && index < (int)sizeof(lower) - 1; index++)
		lower[index] = (char)tolower((unsigned char)gametype[index]);
	lower[index] = 0;
	for (index = 0; lower[index] && length < key_size - 1;)
	{
		static char const *const marks[] = { "ae", "ev", "br", "comp", "btb", "pro", "team" };
		char token[96];
		char const *word;
		int size = 0;
		int skip = 0;
		int mark;

		while (lower[index] == '_' || lower[index] == ' ' || lower[index] == '-')
			index++;
		while (lower[index] && lower[index] != '_' && lower[index] != ' ' && lower[index] != '-')
			token[size++] = lower[index++];
		token[size] = 0;
		if (!size)
			continue;
		word = token;
		for (mark = 0; mark < (int)(sizeof(marks) / sizeof(marks[0])); mark++)
			skip |= !strcmp(token, marks[mark]);
		if (skip)
			continue;
		if (!strcmp(token, "ts") || !strcmp(token, "sly"))
			word = "slayer";
		length += snprintf(key + length, (size_t)(key_size - length), "%s%s", length ? "_" : "", word);
	}
	if (!key[0])
		snprintf(key, (size_t)key_size, "%s", "slayer");
}

static unsigned long next_random(
	unsigned long *state)
{
	/* (xorshift32, in 32 bits whatever long is) */
	unsigned long x = *state & 0xFFFFFFFFUL;

	x ^= (x << 13) & 0xFFFFFFFFUL;
	x ^= x >> 17;
	x ^= (x << 5) & 0xFFFFFFFFUL;
	*state = x ? x : 0x9E3779B9UL;
	return *state;
}

static int same_text(
	char const *a,
	char const *b)
{
	if (!a || !b)
		return 0;
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return !*a && !*b;
}

static int entry_weight(
	int const *weights,
	int index)
{
	return !weights ? 1 : weights[index] < 1 ? 1 : weights[index] > 5 ? 5 : weights[index];
}

int ae_rotation_shuffle_order_weighted(
	int count,
	char const *const *map,
	char const *const *gametype,
	int const *weights,
	char const *previous_map,
	char const *previous_gametype,
	unsigned long seed,
	int *order)
{
	char modes[AE_ROTATION_SHUFFLE_MAXIMUM][64];
	char previous_mode[64];
	int used[AE_ROTATION_SHUFFLE_MAXIMUM] = { 0 };
	int place, index, ok = 1;

	if (count < 1 || count > AE_ROTATION_SHUFFLE_MAXIMUM)
		return 0;
	if (!seed)
		seed = 0x2545F491UL;
	for (index = 0; index < count; index++)
		ae_rotation_shuffle_mode_key(gametype[index], modes[index], sizeof(modes[index]));
	ae_rotation_shuffle_mode_key(previous_gametype, previous_mode, sizeof(previous_mode));
	for (place = 0; place < count; place++)
	{
		char const *last_map = place ? map[order[place - 1]] : previous_map;
		char const *last_mode = place ? modes[order[place - 1]] : (previous_map || previous_gametype ? previous_mode : NULL);
		int tier, best_tier = 3, chosen = -1, found = 0;

		/* (the best tier any unused entry reaches, then a random one of it; an entry that would strand the end
		of the pass, the last map's remaining entries all left for the end, loses to a different map's) */
		for (tier = 0; tier < 3 && !found; tier++)
		{
			for (index = 0; index < count; index++)
			{
				int different_map, different_mode;

				if (used[index])
					continue;
				different_map = !same_text(map[index], last_map);
				different_mode = !last_mode || strcmp(modes[index], last_mode);
				if ((tier == 0 && different_map && different_mode) || (tier == 1 && different_map) || tier == 2)
				{
					found = 1;
					best_tier = tier;
					break;
				}
			}
		}
		if (found)
		{
			int candidates = 0, pick;

			for (index = 0; index < count; index++)
			{
				int different_map, different_mode;

				if (used[index])
					continue;
				different_map = !same_text(map[index], last_map);
				different_mode = !last_mode || strcmp(modes[index], last_mode);
				if ((best_tier == 0 && different_map && different_mode) || (best_tier == 1 && different_map) || best_tier == 2)
					candidates += entry_weight(weights, index);
			}
			pick = (int)(next_random(&seed) % (unsigned long)candidates);
			for (index = 0; index < count; index++)
			{
				int different_map, different_mode;

				if (used[index])
					continue;
				different_map = !same_text(map[index], last_map);
				different_mode = !last_mode || strcmp(modes[index], last_mode);
				if (((best_tier == 0 && different_map && different_mode) || (best_tier == 1 && different_map) ||
					best_tier == 2))
				{
					pick -= entry_weight(weights, index);
					if (pick < 0)
					{
						chosen = index;
						break;
					}
				}
			}
		}
		used[chosen] = 1;
		order[place] = chosen;
	}
	/* repair: a place that repeats the map before it (the greedy walk ran out of other maps) swaps with an entry
	elsewhere that leaves both places without a repeat */
	for (place = 0; place < count; place++)
	{
		char const *before = place ? map[order[place - 1]] : previous_map;
		int other;

		if (!same_text(map[order[place]], before))
			continue;
		for (other = 0; other < count; other++)
		{
			int swapped_ok;
			int saved = order[place];

			if (other == place)
				continue;
			order[place] = order[other];
			order[other] = saved;
			swapped_ok = 1;
			for (index = 0; index < count && swapped_ok; index++)
			{
				char const *b = index ? map[order[index - 1]] : previous_map;

				if (same_text(map[order[index]], b))
					swapped_ok = 0;
			}
			if (swapped_ok)
				break;
			saved = order[place];
			order[place] = order[other];
			order[other] = saved;
		}
	}
	for (index = 0; index < count; index++)
	{
		char const *b = index ? map[order[index - 1]] : previous_map;

		if (same_text(map[order[index]], b))
			ok = 0;
	}
	return ok;
}

int ae_rotation_shuffle_order(
	int count,
	char const *const *map,
	char const *const *gametype,
	char const *previous_map,
	char const *previous_gametype,
	unsigned long seed,
	int *order)
{
	return ae_rotation_shuffle_order_weighted(count, map, gametype, NULL, previous_map, previous_gametype, seed, order);
}

int ae_rotation_vote_drop_played(
	int candidate_count)
{
	return candidate_count >= 2;
}

int ae_rotation_vote_default(
	long const *entries,
	int count,
	long played)
{
	int index;

	for (index = 0; index < count; index++)
		if (entries[index] != played)
			return index;
	return 0;
}

int ae_rotation_word_of(
	char const *stored_name,
	char *word,
	int word_size)
{
	int index, written = 0;

	if (word_size < 2)
		return 0;
	word[0] = 0;
	if (!stored_name)
		return 0;
	for (index = 0; stored_name[index]; index++)
	{
		if (written >= word_size - 1)
		{
			word[0] = 0;
			return 0;
		}
		if (stored_name[index] == '+')
		{
			/* (a '+' starts a rule in a rotation text: "AE BTB+ CTF" is ae_btb_plus_ctf) */
			if (written + 5 >= word_size)
			{
				word[0] = 0;
				return 0;
			}
			memcpy(word + written, "_plus", 5);
			written += 5;
			continue;
		}
		word[written++] = stored_name[index] == ' ' ? '_' : (char)tolower((unsigned char)stored_name[index]);
	}
	word[written] = 0;
	return written;
}
