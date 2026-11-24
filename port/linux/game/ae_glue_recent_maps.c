/* The recent-maps memory in the private files (see ae_glue_recent_maps.h). */
#include <stdio.h>
#include <string.h>

#include "ae_glue_recent_maps.h"
#include "ae_glue_social.h"
#include "ae_recent_maps_rules.h"
#include "../src/ae_private_files.h"

void platform_log(char const *format, ...);

#define FILE_NAME "recent_maps.txt"

static struct
{
	struct ae_recent_maps recent;
	char text[AE_RECENT_TEXT];
	int read;
} memory;

static void memory_read(void)
{
	long length;

	if (memory.read)
		return;
	memory.read = 1;
	memset(&memory.recent, 0, sizeof(memory.recent));
	length = ae_private_file_read(FILE_NAME, memory.text, sizeof(memory.text));
	if (length > 0)
	{
		memory.text[length < (long)sizeof(memory.text) ? length : (long)sizeof(memory.text) - 1] = 0;
		ae_recent_read(&memory.recent, memory.text);
	}
	/* (a playlist the game no longer has: its memory goes) */
	{
		int at = 0;

		while (at < memory.recent.count)
		{
			if (ae_social_playlist_by_id(memory.recent.list[at].playlist))
				at++;
			else
			{
				memmove(&memory.recent.list[at], &memory.recent.list[at + 1], sizeof(memory.recent.list[0]) *
					(size_t)(memory.recent.count - at - 1));
				memory.recent.count--;
			}
		}
	}
	platform_log("ae social: recent maps: %d playlists remembered", memory.recent.count);
}

static void memory_write(void)
{
	int length = ae_recent_write(&memory.recent, memory.text, (int)sizeof(memory.text));

	if (length < 0 || !ae_private_file_write(FILE_NAME, memory.text, length))
		platform_log("ae social: recent maps: %s could not be written", FILE_NAME);
}

/* the maps a pick leaves out: the keys of the hidden entries, and a present() that denies them (a stand-in with its map) */
struct filter
{
	struct ae_social_playlist const *playlist;
	ae_social_present_fn present;
	void *context;
	char key[AE_SOCIAL_ENTRIES][AE_RECENT_KEY];
	int count;
};

static int filter_present(void *context, char const *map)
{
	struct filter const *filter = context;
	char key[AE_RECENT_KEY];
	int entry, at;

	for (entry = 0; entry < filter->playlist->count; entry++)
		if (!strcmp(filter->playlist->entry[entry].map, map))
			break;
	if (entry < filter->playlist->count)
	{
		ae_recent_key_of(filter->playlist, entry, key, (int)sizeof(key));
		for (at = 0; at < filter->count; at++)
			if (!strcmp(filter->key[at], key))
				return 0;
	}
	return filter->present(filter->context, map);
}

/* the filter for an offer of `wanted` maps, logged */
static void filter_make(struct filter *filter, struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context,
	int wanted)
{
	int candidates[AE_SOCIAL_ENTRIES], hidden[AE_SOCIAL_ENTRIES], count, at, other;
	char line[256];

	memory_read();
	filter->playlist = playlist;
	filter->present = present;
	filter->context = context;
	filter->count = 0;
	count = ae_social_playlist_candidates(playlist, present, context, candidates);
	line[0] = 0;
	ae_recent_hidden(playlist, candidates, count, ae_recent_list_get(&memory.recent, playlist->id, 0), wanted, hidden);
	for (at = 0; at < count; at++)
	{
		char key[AE_RECENT_KEY];
		int seen = 0;

		if (!hidden[at])
			continue;
		ae_recent_key_of(playlist, candidates[at], key, (int)sizeof(key));
		for (other = 0; other < filter->count; other++)
			seen |= !strcmp(filter->key[other], key);
		if (seen)
			continue;
		snprintf(filter->key[filter->count++], AE_RECENT_KEY, "%s", key);
		snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s%s", line[0] ? ", " : "", key);
	}
	platform_log("ae social: recent maps %s (window %d, offer %d): skipped %s", playlist->id,
		ae_recent_window(ae_recent_playable(playlist, candidates, count)), wanted, line[0] ? line : "none");
}

int ae_glue_recent_pick(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long random)
{
	static struct filter filter;
	int pick;

	filter_make(&filter, playlist, present, context, 1);
	pick = ae_social_playlist_pick(playlist, filter_present, &filter, last, random);
	/* (nothing left: the memory is not an obstacle to a game) */
	return pick >= 0 ? pick : ae_social_playlist_pick(playlist, present, context, last, random);
}

int ae_glue_recent_vote(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long seed, struct ae_social_vote_choice *out, int max)
{
	static struct filter filter;
	int count;

	filter_make(&filter, playlist, present, context, AE_RECENT_OFFER);
	count = ae_social_vote_pick_choices(playlist, filter_present, &filter, last, seed, out, max);
	return count >= 2 ? count : ae_social_vote_pick_choices(playlist, present, context, last, seed, out, max);
}

void ae_glue_recent_played(struct ae_social_playlist const *playlist, int entry)
{
	struct ae_recent_list *list;
	char key[AE_RECENT_KEY];

	if (entry < 0 || entry >= playlist->count)
		return;
	memory_read();
	list = ae_recent_list_get(&memory.recent, playlist->id, 1);
	if (!list)
		return;
	ae_recent_key_of(playlist, entry, key, (int)sizeof(key));
	ae_recent_push(list, key);
	memory_write();
	platform_log("ae social: recent maps %s: %s played", playlist->id, key);
}

static void show(struct ae_recent_list const *list, char *text, int size)
{
	int at;

	snprintf(text, (size_t)size, "social recent %s:", list ? list->playlist : "(none)");
	for (at = 0; list && at < list->count; at++)
		snprintf(text + strlen(text), (size_t)size - strlen(text), " %s", list->key[at]);
	if (!list || !list->count)
		snprintf(text + strlen(text), (size_t)size - strlen(text), " empty");
}

int ae_glue_recent_dev(char const *verb, char const *playlist, char const *maps, char *text, int size)
{
	struct ae_recent_list *list;

	memory_read();
	if (!strcmp(verb, "show"))
	{
		show(ae_recent_list_get(&memory.recent, playlist, 0), text, size);
		return 1;
	}
	if (!strcmp(verb, "clear"))
	{
		if (!strcmp(playlist, "all"))
			memset(&memory.recent, 0, sizeof(memory.recent));
		else if ((list = ae_recent_list_get(&memory.recent, playlist, 0)) != NULL)
			list->count = 0;
		memory_write();
		snprintf(text, (size_t)size, "social recent: %s cleared", playlist);
		return 1;
	}
	if (!strcmp(verb, "set"))
	{
		char copy[256], *at, *word, *keys[AE_RECENT_MAX];
		int count = 0;

		list = ae_recent_list_get(&memory.recent, playlist, 1);
		if (!list)
		{
			snprintf(text, (size_t)size, "social recent set: no room for %s", playlist);
			return 0;
		}
		snprintf(copy, sizeof(copy), "%s", maps);
		for (at = copy; *at; at++)
			if (*at == ',')
				*at = ' ';
		/* (the words, cut at the blanks the commas became) */
		at = copy;
		while (*at && count < AE_RECENT_MAX)
		{
			while (*at == ' ')
				at++;
			if (!*at)
				break;
			word = at;
			while (*at && *at != ' ')
				at++;
			if (*at)
				*at++ = 0;
			keys[count++] = word;
		}
		list->count = 0;
		while (count > 0)
		{
			/* (a name the playlist knows, "ascetic" or "ascetic@smp": the map it stands for, as a game would have left it) */
			struct ae_social_playlist const *playlist_data = ae_social_playlist_by_id(playlist);
			char key[AE_RECENT_KEY];
			int entry, found = -1;

			count--;
			for (entry = 0; playlist_data && entry < playlist_data->count && found < 0; entry++)
			{
				char const *map = playlist_data->entry[entry].map;
				size_t length = strlen(keys[count]);

				if (!strcmp(map, keys[count]) || (!strncmp(map, keys[count], length) && (map[length] == '@' || map[length] == ' ')))
					found = entry;
			}
			if (found >= 0)
				ae_recent_key_of(playlist_data, found, key, (int)sizeof(key));
			else
				snprintf(key, sizeof(key), "%s", keys[count]);
			ae_recent_push(list, key);
		}
		memory_write();
		show(list, text, size);
		return 1;
	}
	snprintf(text, (size_t)size, "social recent: show|clear|set");
	return 0;
}
