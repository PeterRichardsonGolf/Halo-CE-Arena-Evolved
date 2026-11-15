/*
AE_PLAYLIST.C

A hosted playlist (ae_playlist.h). Pure.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_playlist.h"

static void copy(char *to, int size, char const *from)
{
	snprintf(to, (size_t)size, "%s", from ? from : "");
}

void ae_playlist_init(struct ae_playlist *playlist)
{
	memset(playlist, 0, sizeof(*playlist));
	copy(playlist->name, AE_PLAYLIST_NAME, "MY PLAYLIST");
	playlist->map_vote = 1;
	playlist->no_repeats = 1;
}

int ae_playlist_add(struct ae_playlist *playlist, char const *map, char const *gametype)
{
	struct ae_playlist_entry *entry;

	if (!map || !map[0] || !gametype || !gametype[0] || playlist->count >= AE_PLAYLIST_ENTRIES)
		return -1;
	entry = &playlist->entries[playlist->count];
	copy(entry->map, AE_PLAYLIST_MAP, map);
	copy(entry->gametype, AE_PLAYLIST_GAMETYPE, gametype);
	entry->weight = 2;
	return playlist->count++;
}

void ae_playlist_remove(struct ae_playlist *playlist, int index)
{
	int at;

	if (index < 0 || index >= playlist->count)
		return;
	for (at = index; at + 1 < playlist->count; at++)
		playlist->entries[at] = playlist->entries[at + 1];
	playlist->count--;
}

int ae_playlist_weight_step(struct ae_playlist *playlist, int index, int direction)
{
	int weight;

	if (index < 0 || index >= playlist->count)
		return 0;
	weight = playlist->entries[index].weight + (direction < 0 ? -1 : 1);
	if (weight < 1 || weight > AE_PLAYLIST_WEIGHT_MAX)
		return 0;
	playlist->entries[index].weight = (short)weight;
	return 1;
}

int ae_playlist_move(struct ae_playlist *playlist, int from, int to)
{
	struct ae_playlist_entry held;
	int at;

	if (from < 0 || from >= playlist->count || to < 0 || to >= playlist->count || from == to)
		return 0;
	held = playlist->entries[from];
	for (at = from; at < to; at++)
		playlist->entries[at] = playlist->entries[at + 1];
	for (at = from; at > to; at--)
		playlist->entries[at] = playlist->entries[at - 1];
	playlist->entries[to] = held;
	return 1;
}

static int total_weight(struct ae_playlist const *playlist, int skip)
{
	int index, total = 0;

	for (index = 0; index < playlist->count; index++)
		if (index != skip)
			total += playlist->entries[index].weight;
	return total;
}

int ae_playlist_chance(struct ae_playlist const *playlist, int index)
{
	int total = total_weight(playlist, -1);

	if (index < 0 || index >= playlist->count || total <= 0)
		return 0;
	return (playlist->entries[index].weight * 200 + total) / (total * 2);
}

/* the entry the number lands on among the weights, an index skipped */
static int weighted(struct ae_playlist const *playlist, int skip, unsigned long random)
{
	int total = total_weight(playlist, skip), at = 0, index;

	if (total <= 0)
		return -1;
	at = (int)(random % (unsigned long)total);
	for (index = 0; index < playlist->count; index++)
	{
		if (index == skip)
			continue;
		if (at < playlist->entries[index].weight)
			return index;
		at -= playlist->entries[index].weight;
	}
	return -1;
}

/* the next random number from one (a small generator: the picks only need to differ) */
static unsigned long next_random(unsigned long random)
{
	return (random * 1103515245ul + 12345ul) & 0x7FFFFFFFul;
}

int ae_playlist_pick(struct ae_playlist const *playlist, int last, unsigned long random)
{
	if (playlist->count <= 0)
		return -1;
	if (playlist->no_repeats && playlist->count > 1 && last >= 0 && last < playlist->count)
		return weighted(playlist, last, random);
	return weighted(playlist, -1, random);
}

int ae_playlist_vote_choices(struct ae_playlist const *playlist, int current, unsigned long random, int *choices)
{
	struct ae_playlist working = *playlist;
	int count = 0;

	if (!playlist->map_vote || current < 0 || current >= playlist->count || playlist->count < 2)
		return 0;
	/* (each game once: a picked one's weight leaves the draw) */
	while (count < AE_PLAYLIST_VOTE_GAMES)
	{
		int picked = weighted(&working, current, random);

		if (picked < 0)
			break;
		choices[count++] = picked;
		working.entries[picked].weight = 0;
		random = next_random(random);
	}
	if (!count)
		return 0;
	choices[count++] = current;
	return count;
}

int ae_playlist_vote_is_same_again(int count, int choice)
{
	return count >= 2 && choice == count - 1;
}

long ae_playlist_write(struct ae_playlist const *playlist, char *text, long size)
{
	long used = 0;
	int index;

#define PUT(...) \
	do { \
		int wrote = snprintf(text + used, (size_t)(size - used), __VA_ARGS__); \
		if (wrote < 0 || used + wrote >= size) \
			return -1; \
		used += wrote; \
	} while (0)
	if (size <= 0)
		return -1;
	text[0] = 0;
	PUT("# playlist: %s\n# map vote: %d\n# no repeats: %d\n", playlist->name, playlist->map_vote ? 3 : 0,
		playlist->no_repeats ? 1 : 0);
	for (index = 0; index < playlist->count; index++)
		PUT("%s:file=%s  # weight %d\n", playlist->entries[index].map, playlist->entries[index].gametype,
			(int)playlist->entries[index].weight);
#undef PUT
	return used;
}

/* a line's text trimmed, ended at its end */
static char *trim(char *text)
{
	char *end;

	while (*text == ' ' || *text == '\t' || *text == '\r')
		text++;
	end = text + strlen(text);
	while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
		*--end = 0;
	return text;
}

int ae_playlist_read(struct ae_playlist *playlist, char const *text)
{
	char line[512];
	int read = 0;

	playlist->count = 0;
	while (text && *text)
	{
		size_t length = strcspn(text, "\n");
		char *body, *comment, *colon;
		int weight = 2;

		snprintf(line, sizeof(line), "%.*s", (int)(length < sizeof(line) - 1 ? length : sizeof(line) - 1), text);
		text += length + (text[length] == '\n');
		body = trim(line);
		if (body[0] == '#')
		{
			body = trim(body + 1);
			if (!strncmp(body, "playlist:", 9))
				copy(playlist->name, AE_PLAYLIST_NAME, trim(body + 9));
			else if (!strncmp(body, "map vote:", 9))
				playlist->map_vote = atoi(trim(body + 9)) > 0;
			else if (!strncmp(body, "no repeats:", 11))
				playlist->no_repeats = atoi(trim(body + 11)) > 0;
			continue;
		}
		comment = strchr(body, '#');
		if (comment)
		{
			*comment++ = 0;
			comment = trim(comment);
			if (!strncmp(comment, "weight", 6))
				weight = atoi(trim(comment + 6));
			body = trim(body);
		}
		colon = strchr(body, ':');
		if (!body[0] || !colon || colon == body || !colon[1])
			continue;
		*colon++ = 0;
		colon = trim(colon);
		if (!strncmp(colon, "file=", 5))
			colon = trim(colon + 5);
		if (ae_playlist_add(playlist, trim(body), colon) >= 0)
		{
			playlist->entries[playlist->count - 1].weight =
				(short)(weight < 1 ? 1 : weight > AE_PLAYLIST_WEIGHT_MAX ? AE_PLAYLIST_WEIGHT_MAX : weight);
			read++;
		}
	}
	return read;
}
