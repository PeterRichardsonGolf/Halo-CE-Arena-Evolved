/*
AE_MATCH_FILM_RULES.C

See ae_match_film_rules.h.
*/

#include "ae_match_film_rules.h"

#include <stdio.h>
#include <string.h>

int ae_match_film_rules_mode(char const *text)
{
	if (!text)
		return AE_MATCH_FILM_OFF;
	if (!strcmp(text, "people"))
		return AE_MATCH_FILM_PEOPLE;
	if (!strcmp(text, "all") || !strcmp(text, "on"))
		return AE_MATCH_FILM_ALL;
	return AE_MATCH_FILM_OFF;
}

static int digits(char const *text, int count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (text[index] < '0' || text[index] > '9')
			return 0;
	}
	return 1;
}

int ae_match_film_name_is_film(char const *name)
{
	size_t length, rest;

	if (!name || strncmp(name, "film-", 5))
		return 0;
	length = strlen(name);
	/* film-<8>-<6>-<x>.txt: 5 + 8 + 1 + 6 + 1 + at least 1 + 4 */
	if (length < 26 || strcmp(name + length - 4, ".txt"))
		return 0;
	if (!digits(name + 5, 8) || name[13] != '-' || !digits(name + 14, 6) || name[20] != '-')
		return 0;
	rest = length - 4 - 21;
	return rest >= 1;
}

static void word(char *out, size_t size, char const *text)
{
	size_t length = strlen(out), index;

	for (index = 0; text && text[index] && length + 1 < size; index++)
	{
		char c = text[index];

		out[length++] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ? c : '_';
	}
	out[length] = 0;
}

int ae_match_film_file_name(char *out, size_t size, char const *stamp, char const *map, char const *engine, char const *unique)
{
	int written = snprintf(out, size, "film-%s-", stamp ? stamp : "");

	if (written < 0 || (size_t)written >= size)
		return 0;
	word(out, size, map && map[0] ? map : "map");
	if (strlen(out) + 2 >= size)
		return 0;
	strcat(out, "-");
	word(out, size, engine && engine[0] ? engine : "game");
	if (strlen(out) + 2 >= size)
		return 0;
	strcat(out, "-");
	word(out, size, unique && unique[0] ? unique : "0");
	if (strlen(out) + 5 > size)
		return 0;
	strcat(out, ".txt");
	return ae_match_film_name_is_film(out);
}

int ae_match_film_pick_removals(long long const *sizes, int count, long long budget_bytes, int current_index, int *out)
{
	long long total = 0;
	int index, picked = 0;

	for (index = 0; index < count; index++)
		total += sizes[index];
	for (index = 0; index < count && total > budget_bytes; index++)
	{
		if (index == current_index)
			continue;
		out[picked++] = index;
		total -= sizes[index];
	}
	return picked;
}

int ae_match_film_name_compare(char const *a, char const *b)
{
	int stamps = strncmp(a + 5, b + 5, 15);

	return stamps ? stamps : strcmp(a, b);
}

long ae_match_film_budget_mb(long asked)
{
	return asked < AE_MATCH_FILM_BUDGET_MINIMUM ? AE_MATCH_FILM_BUDGET_MINIMUM : asked;
}
