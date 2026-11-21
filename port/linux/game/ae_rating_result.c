/* The rated game's result record (see the header). Pure code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_rating_result.h"

void ae_rating_result_init(struct ae_rating_result *result)
{
	memset(result, 0, sizeof(*result));
	result->first_quit_ms = -1;
}

int ae_rating_result_entries(struct ae_rating_result const *result, struct ae_rating_entry *entries, int maximum)
{
	int index, count = result->count < maximum ? result->count : maximum;

	for (index = 0; index < count; index++)
	{
		struct ae_rating_result_player const *p = &result->player[index];

		entries[index].present = 1;
		entries[index].rated = p->rated;
		entries[index].level = p->level;
		entries[index].place = p->place;
		entries[index].team = p->team;
		entries[index].quit = p->quit && !p->rejoined;
		entries[index].quit_order = entries[index].quit ? p->quit_order : -1;
		entries[index].party = p->party;
	}
	return count;
}

long ae_rating_result_change(struct ae_rating_result const *result, int self, long points)
{
	struct ae_rating_entry entries[AE_RATING_RESULT_PLAYERS];
	int count = ae_rating_result_entries(result, entries, AE_RATING_RESULT_PLAYERS);

	if (self < 0 || self >= count || !result->player[self].rated)
		return 0;
	return ae_rating_protected_change(entries, count, self, points, result->first_quit_ms);
}

int ae_rating_result_counts_quit(struct ae_rating_result const *result, int self)
{
	struct ae_rating_entry entries[AE_RATING_RESULT_PLAYERS];
	int count = ae_rating_result_entries(result, entries, AE_RATING_RESULT_PLAYERS);

	return ae_rating_protected_counts_quit(entries, count, self);
}

int ae_rating_result_find(struct ae_rating_result const *result, char const *key)
{
	int index;

	for (index = 0; index < result->count; index++)
		if (!strcmp(result->player[index].key, key))
			return index;
	return -1;
}

static void safe(char const *from, char *to, size_t size)
{
	size_t at = 0;

	for (; *from && at + 1 < size; from++)
	{
		unsigned char c = (unsigned char)*from;

		to[at++] = (c == '|' || c < 32 || c == 127) ? '?' : (char)c;
	}
	to[at] = 0;
}

long ae_rating_result_write(struct ae_rating_result const *result, char *buffer, long size)
{
	long at = 0;
	int index, n;
	char name[AE_RATING_RESULT_KEY], playlist[AE_RATING_RESULT_PLAYLIST];

	safe(result->playlist, playlist, sizeof(playlist));
	n = snprintf(buffer, (size_t)size, "# AE rating result\nversion = %d\nplaylist = %s\nduration = %ld\nfirst_quit = %ld\n",
		AE_RATING_RESULT_VERSION, playlist, result->duration_ms, result->first_quit_ms);
	if (n < 0 || n >= size)
		return -1;
	at = n;
	for (index = 0; index < result->count; index++)
	{
		struct ae_rating_result_player const *p = &result->player[index];

		safe(p->key, name, sizeof(name));
		n = snprintf(buffer + at, (size_t)(size - at), "player = %s | %d %d %d %d %d %d %d %d\n", name, p->level, p->place,
			p->team, p->party, p->quit, p->quit_order, p->rejoined, p->rated);
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
	}
	return at;
}

static int numbers(char const *text, long *out, int count)
{
	char *end;
	int index;

	for (index = 0; index < count; index++)
	{
		out[index] = strtol(text, &end, 10);
		if (end == text)
			return 0;
		text = end;
	}
	return 1;
}

int ae_rating_result_read(struct ae_rating_result *result, char const *text)
{
	char line[512];
	int header = 0;

	ae_rating_result_init(result);
	while (*text)
	{
		size_t length = strcspn(text, "\n");
		char *equals, *key, *value, *end;

		if (length >= sizeof(line))
			goto bad;
		memcpy(line, text, length);
		line[length] = 0;
		text += length + (text[length] ? 1 : 0);
		if (length && line[length - 1] == '\r')
			line[length - 1] = 0;
		if (!line[0] || line[0] == '#')
			continue;
		equals = strchr(line, '=');
		if (!equals)
			goto bad;
		*equals = 0;
		key = line;
		value = equals + 1;
		while (*value == ' ')
			value++;
		end = key + strlen(key);
		while (end > key && end[-1] == ' ')
			*--end = 0;
		if (!header)
		{
			long version;

			if (strcmp(key, "version") || !numbers(value, &version, 1) || version < 1)
				goto bad;
			header = 1;
			result->newer = version > AE_RATING_RESULT_VERSION;
		}
		else if (!strcmp(key, "playlist"))
			safe(value, result->playlist, sizeof(result->playlist));
		else if (!strcmp(key, "duration"))
		{
			long v;

			if (numbers(value, &v, 1))
				result->duration_ms = v;
		}
		else if (!strcmp(key, "first_quit"))
		{
			long v;

			if (numbers(value, &v, 1))
				result->first_quit_ms = v;
		}
		else if (!strcmp(key, "player"))
		{
			char *bar = strchr(value, '|');
			long v[8];
			struct ae_rating_result_player *p;

			if (!bar || result->count >= AE_RATING_RESULT_PLAYERS)
				continue;
			*bar = 0;
			end = bar;
			while (end > value && end[-1] == ' ')
				*--end = 0;
			if (!value[0] || strlen(value) >= AE_RATING_RESULT_KEY || !numbers(bar + 1, v, 8))
				continue;
			p = &result->player[result->count++];
			snprintf(p->key, sizeof(p->key), "%s", value);
			p->level = v[0] < 0 ? 0 : v[0] > AE_RATING_LEVELS - 1 ? AE_RATING_LEVELS - 1 : (int)v[0];
			p->place = (int)v[1];
			p->team = (int)v[2];
			p->party = (int)v[3];
			p->quit = v[4] != 0;
			p->quit_order = (int)v[5];
			p->rejoined = v[6] != 0;
			p->rated = v[7] != 0;
		}
		/* (any other kind: a newer build's, skipped) */
	}
	if (!header)
		goto bad;
	return 1;
bad:
	ae_rating_result_init(result);
	return 0;
}
