/*
AE_DISC_JOIN_RULES.C

Arena Evolved: a modded disc in the join token (ae_disc_join_rules.h).
*/

#include "ae_disc_join_rules.h"
#include "ae_map_share_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

unsigned long long ae_disc_join_identity(char const *disc)
{
	/* (the fold's own prefix: no plain name's hash) */
	static char const prefix[] = "arena evolved disc:";
	unsigned long long hash = 0xcbf29ce484222325ULL;
	char const *c;

	if (!disc || !disc[0])
		return 0;
	for (c = prefix; *c; c++)
	{
		hash ^= (unsigned char)*c;
		hash *= 0x100000001b3ULL;
	}
	for (c = disc; *c; c++)
	{
		hash ^= (unsigned char)tolower((unsigned char)*c);
		hash *= 0x100000001b3ULL;
	}
	return hash ? hash : 1;
}

/* two names the same, letters in any case (no strcasecmp on Windows) */
static int same_name(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return !*a && !*b;
}

int ae_disc_join_replaces_stock_map(char const *file_name)
{
	static char const *const no_game[] = { "ui", "bitmaps", "sounds", "loc", NULL };
	char name[64];
	size_t length;
	int index;

	if (!file_name)
		return 0;
	length = strlen(file_name);
	if (length <= 4 || length - 4 >= sizeof(name) || !same_name(file_name + length - 4, ".map"))
		return 0;
	snprintf(name, sizeof(name), "%.*s", (int)(length - 4), file_name);
	for (index = 0; no_game[index]; index++)
	{
		if (same_name(name, no_game[index]))
			return 0;
	}
	return ae_map_share_stock_name(name);
}

void ae_disc_join_fold(unsigned char *token, int size, unsigned long long identity)
{
	int index;

	if (!token || !identity)
		return;
	for (index = 0; index < size; index++)
		token[index] ^= (unsigned char)(identity >> ((index % 8) * 8));
}
