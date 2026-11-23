/* The recent-maps memory (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_recent_maps_rules.h"

static void copy(char *out, int size, char const *text)
{
	snprintf(out, (size_t)size, "%s", text);
}

void ae_recent_key_of(struct ae_social_playlist const *playlist, int entry, char *key, int size)
{
	int hops = 0;

	key[0] = 0;
	if (entry < 0 || entry >= playlist->count)
		return;
	while (playlist->entry[entry].stand_for > 0 && playlist->entry[entry].stand_for - 1 < playlist->count && hops++ < 4)
		entry = playlist->entry[entry].stand_for - 1;
	copy(key, size, playlist->entry[entry].map);
}

int ae_recent_playable(struct ae_social_playlist const *playlist, int const *candidates, int count)
{
	char first[AE_SOCIAL_ENTRIES][AE_RECENT_KEY];
	int at, other, distinct = 0;

	for (at = 0; at < count && at < AE_SOCIAL_ENTRIES; at++)
	{
		int seen = 0;

		ae_recent_key_of(playlist, candidates[at], first[at], AE_RECENT_KEY);
		for (other = 0; other < at; other++)
			seen |= !strcmp(first[other], first[at]);
		distinct += !seen;
	}
	return distinct;
}

int ae_recent_window(int playable)
{
	int window = playable / 3;

	if (window > AE_RECENT_MAX)
		window = AE_RECENT_MAX;
	if (playable - window < AE_RECENT_OFFER)
		window = playable - AE_RECENT_OFFER;
	return window < 0 ? 0 : window;
}

struct ae_recent_list *ae_recent_list_get(struct ae_recent_maps *recent, char const *playlist, int create)
{
	int at;

	for (at = 0; at < recent->count; at++)
		if (!strcmp(recent->list[at].playlist, playlist))
			return &recent->list[at];
	if (!create || !playlist[0])
		return NULL;
	if (recent->count >= AE_RECENT_LISTS)
	{
		/* (all taken: the oldest list, the first, makes room: a new playlist always gets a memory) */
		memmove(&recent->list[0], &recent->list[1], sizeof(recent->list[0]) * (AE_RECENT_LISTS - 1));
		recent->count--;
	}
	memset(&recent->list[recent->count], 0, sizeof(recent->list[0]));
	copy(recent->list[recent->count].playlist, AE_SOCIAL_ID, playlist);
	return &recent->list[recent->count++];
}

void ae_recent_push(struct ae_recent_list *list, char const *key)
{
	int at, from = list->count;

	if (!key[0])
		return;
	for (at = 0; at < list->count; at++)
		if (!strcmp(list->key[at], key))
		{
			from = at;
			break;
		}
	if (from == list->count && list->count < AE_RECENT_MAX)
		list->count++;
	else if (from == list->count)
		from = AE_RECENT_MAX - 1;
	for (at = from; at > 0; at--)
		memcpy(list->key[at], list->key[at - 1], AE_RECENT_KEY);
	copy(list->key[0], AE_RECENT_KEY, key);
}

int ae_recent_age(struct ae_recent_list const *list, char const *key, int window)
{
	int at;

	if (!list)
		return -1;
	for (at = 0; at < list->count && at < window; at++)
		if (!strcmp(list->key[at], key))
			return at;
	return -1;
}

int ae_recent_hidden(struct ae_social_playlist const *playlist, int const *candidates, int count, struct ae_recent_list const *list,
	int wanted, int *hidden)
{
	char key[AE_SOCIAL_ENTRIES][AE_RECENT_KEY];
	int age[AE_SOCIAL_ENTRIES], window, other_maps = 0, at, other, hidden_count = 0, back;

	memset(hidden, 0, sizeof(int) * (size_t)(count > 0 ? count : 0));
	if (!list || count <= 0 || count > AE_SOCIAL_ENTRIES)
		return 0;
	window = ae_recent_window(ae_recent_playable(playlist, candidates, count));
	for (at = 0; at < count; at++)
	{
		int seen = 0;

		ae_recent_key_of(playlist, candidates[at], key[at], AE_RECENT_KEY);
		age[at] = ae_recent_age(list, key[at], window);
		for (other = 0; other < at; other++)
			seen |= !strcmp(key[other], key[at]);
		other_maps += age[at] < 0 && !seen;
		hidden[at] = age[at] >= 0;
	}
	/* (too few others: the oldest recent maps come back, one map at a time, until `wanted` maps are on offer) */
	for (back = window - 1; other_maps < wanted && back >= 0; back--)
	{
		int returned = 0;

		for (at = 0; at < count; at++)
			if (age[at] == back)
			{
				hidden[at] = 0;
				returned = 1;
			}
		other_maps += returned;
	}
	for (at = 0; at < count; at++)
		hidden_count += hidden[at];
	return hidden_count;
}

int ae_recent_write(struct ae_recent_maps const *recent, char *text, int size)
{
	int at, used = 0, key;

	if (size < 1)
		return -1;
	text[0] = 0;
	for (at = 0; at < recent->count; at++)
	{
		struct ae_recent_list const *list = &recent->list[at];
		int wrote = snprintf(text + used, (size_t)(size - used), "%s", list->playlist);

		if (wrote < 0 || used + wrote >= size)
			return -1;
		used += wrote;
		for (key = 0; key < list->count; key++)
		{
			wrote = snprintf(text + used, (size_t)(size - used), "\t%s", list->key[key]);
			if (wrote < 0 || used + wrote >= size)
				return -1;
			used += wrote;
		}
		if (used + 2 >= size)
			return -1;
		text[used++] = '\n';
		text[used] = 0;
	}
	return used;
}

/* the next field of a line (a tab ends it; a map's name may hold blanks), NULL at its end */
static char *next_word(char **cursor)
{
	char *word;

	if (!**cursor)
		return NULL;
	word = *cursor;
	while (**cursor && **cursor != '\t')
		(*cursor)++;
	if (**cursor)
		*(*cursor)++ = 0;
	return word[0] ? word : next_word(cursor);
}

int ae_recent_read(struct ae_recent_maps *recent, char const *text)
{
	memset(recent, 0, sizeof(*recent));
	while (*text)
	{
		char line[AE_SOCIAL_ID + 1 + AE_RECENT_MAX * (AE_RECENT_KEY + 1) + 8], *word, *save;
		size_t length = strcspn(text, "\r\n");
		struct ae_recent_list *list;

		snprintf(line, sizeof(line), "%.*s", (int)(length < sizeof(line) - 1 ? length : sizeof(line) - 1), text);
		text += length;
		while (*text == '\r' || *text == '\n')
			text++;
		save = line;
		word = next_word(&save);
		if (!word || (list = ae_recent_list_get(recent, word, 1)) == NULL)
			continue;
		/* (the file's order is the newest first: pushed from the oldest) */
		{
			char keys[AE_RECENT_MAX][AE_RECENT_KEY];
			int count = 0, at;

			while (count < AE_RECENT_MAX && (word = next_word(&save)) != NULL)
				copy(keys[count++], AE_RECENT_KEY, word);
			list->count = 0;
			for (at = count - 1; at >= 0; at--)
				ae_recent_push(list, keys[at]);
		}
	}
	return recent->count;
}
