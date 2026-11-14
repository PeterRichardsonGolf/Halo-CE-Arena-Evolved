/* The career record (see the header). Pure code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_career_record.h"

static char const *const ladder_key[2] = {"career", "online"};

void ae_career_init(struct ae_career *career)
{
	memset(career, 0, sizeof(*career));
}

void ae_career_id(char const *playlist, char *id, size_t size)
{
	size_t at = 0;

	if (!size)
		return;
	for (; playlist && *playlist && at + 1 < size && at + 1 < AE_CAREER_ID; playlist++)
	{
		unsigned char c = (unsigned char)*playlist;

		id[at++] = (c == '|' || c < 32 || c == 127) ? '?' : (char)c;
	}
	id[at] = 0;
}

struct ae_career_entry *ae_career_find(struct ae_career *career, int ladder, char const *playlist)
{
	char id[AE_CAREER_ID];
	int index;

	if (ladder < 0 || ladder > 1)
		return NULL;
	ae_career_id(playlist, id, sizeof(id));
	for (index = 0; index < career->count[ladder]; index++)
		if (!strcmp(career->entry[ladder][index].playlist, id))
			return &career->entry[ladder][index];
	return NULL;
}

struct ae_career_entry *ae_career_entry(struct ae_career *career, int ladder, char const *playlist)
{
	struct ae_career_entry *entry = ae_career_find(career, ladder, playlist);

	if (entry)
		return entry;
	if (ladder < 0 || ladder > 1 || career->count[ladder] >= AE_CAREER_PLAYLISTS)
		return NULL;
	entry = &career->entry[ladder][career->count[ladder]++];
	memset(entry, 0, sizeof(*entry));
	ae_career_id(playlist, entry->playlist, sizeof(entry->playlist));
	return entry;
}

static void note_quit(struct ae_career_entry *entry, unsigned long clock)
{
	entry->quits++;
	if (entry->quit_stamps >= AE_CAREER_QUIT_HISTORY)
	{
		memmove(entry->quit_stamp, entry->quit_stamp + 1, sizeof(entry->quit_stamp[0]) * (AE_CAREER_QUIT_HISTORY - 1));
		entry->quit_stamps = AE_CAREER_QUIT_HISTORY - 1;
	}
	entry->quit_stamp[entry->quit_stamps++] = clock;
}

int ae_career_record_game(struct ae_career *career, int ladder, char const *playlist, long change, int won, unsigned long clock)
{
	struct ae_career_entry *entry = ae_career_entry(career, ladder, playlist);

	if (!entry || !entry->playlist[0])
		return 0;
	ae_rating_apply(&entry->rating, change);
	entry->games++;
	if (won)
		entry->wins++;
	entry->last = clock;
	return 1;
}

int ae_career_record_quit(struct ae_career *career, int ladder, char const *playlist, long change, unsigned long clock)
{
	struct ae_career_entry *entry = ae_career_entry(career, ladder, playlist);

	if (!entry || !entry->playlist[0])
		return 0;
	ae_rating_apply(&entry->rating, change);
	entry->games++;
	note_quit(entry, clock);
	entry->last = clock;
	return 1;
}

int ae_career_record_result(struct ae_career *career, int ladder, struct ae_rating_result const *result, int self,
	unsigned long clock)
{
	struct ae_career_entry *entry;
	long change;

	if (!result || self < 0 || self >= result->count || !result->player[self].rated)
		return 0;
	entry = ae_career_entry(career, ladder, result->playlist);
	if (!entry || !entry->playlist[0])
		return 0;
	change = ae_rating_result_change(result, self, entry->rating.points);
	ae_rating_apply(&entry->rating, change);
	entry->games++;
	if (result->player[self].place == 0 && !(result->player[self].quit && !result->player[self].rejoined))
		entry->wins++;
	if (ae_rating_result_counts_quit(result, self))
		note_quit(entry, clock);
	entry->last = clock;
	return 1;
}

int ae_career_overall_level(struct ae_career const *career, int ladder)
{
	int index, best = 0;

	if (ladder < 0 || ladder > 1)
		return 0;
	for (index = 0; index < career->count[ladder]; index++)
		if (career->entry[ladder][index].rating.highest > best)
			best = career->entry[ladder][index].rating.highest;
	return best;
}

static unsigned long checksum(char const *text, size_t length)
{
	unsigned long hash = 2166136261u;
	size_t index;

	for (index = 0; index < length; index++)
		hash = ((hash ^ (unsigned char)text[index]) * 16777619u) & 0xffffffffu;
	return hash;
}

long ae_career_write(struct ae_career const *career, char *buffer, long size)
{
	long at = 0;
	int ladder, index;
	int n;

	n = snprintf(buffer, (size_t)size, "# AE career\nversion = %d\n", AE_CAREER_VERSION);
	if (n < 0 || n >= size)
		return -1;
	at = n;
	for (ladder = 0; ladder < 2; ladder++)
		for (index = 0; index < career->count[ladder]; index++)
		{
			struct ae_career_entry const *e = &career->entry[ladder][index];

			int stamp;

			n = snprintf(buffer + at, (size_t)(size - at), "%s = %s | %ld %d %d %ld %ld %ld %lu", ladder_key[ladder], e->playlist,
				e->rating.points, e->rating.level, e->rating.highest, e->games, e->wins, e->quits, e->last);
			if (n < 0 || n >= size - at)
				return -1;
			at += n;
			for (stamp = 0; stamp < e->quit_stamps; stamp++)
			{
				n = snprintf(buffer + at, (size_t)(size - at), "%s%lu", stamp ? " " : " | ", e->quit_stamp[stamp]);
				if (n < 0 || n >= size - at)
					return -1;
				at += n;
			}
			n = snprintf(buffer + at, (size_t)(size - at), "\n");
			if (n < 0 || n >= size - at)
				return -1;
			at += n;
		}
	n = snprintf(buffer + at, (size_t)(size - at), "checksum = %08lx\n", checksum(buffer, (size_t)at));
	if (n < 0 || n >= size - at)
		return -1;
	return at + n;
}

static int numbers(char const *text, long *out, int count)
{
	char *end;
	int index;

	for (index = 0; index < count; index++)
	{
		out[index] = strtol(text, &end, 10);
		if (end == text || out[index] < 0)
			return 0;
		text = end;
	}
	return 1;
}

int ae_career_read(struct ae_career *career, char const *text)
{
	char line[512];
	char const *start = text;
	int header = 0, summed = 0;

	ae_career_init(career);
	while (*text)
	{
		size_t length = strcspn(text, "\n");
		char const *line_start = text;
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
			career->newer = version > AE_CAREER_VERSION;
			continue;
		}
		if (!strcmp(key, "checksum"))
		{
			char *stop;
			unsigned long given = strtoul(value, &stop, 16);

			if (stop == value || given != checksum(start, (size_t)(line_start - start)))
				goto edited;
			summed = 1;
			continue;
		}
		else
		{
			int ladder = !strcmp(key, "career") ? 0 : !strcmp(key, "online") ? 1 : -1;
			char *bar, *name;
			long v[7];
			struct ae_career_entry *entry;

			if (ladder < 0)
				continue;
			bar = strchr(value, '|');
			if (!bar || career->count[ladder] >= AE_CAREER_PLAYLISTS)
				continue;
			*bar = 0;
			name = value;
			end = bar;
			while (end > name && end[-1] == ' ')
				*--end = 0;
			if (!name[0] || strlen(name) >= AE_CAREER_ID || !numbers(bar + 1, v, 7))
				continue;
			entry = &career->entry[ladder][career->count[ladder]++];
			memset(entry, 0, sizeof(*entry));
			snprintf(entry->playlist, sizeof(entry->playlist), "%s", name);
			ae_rating_state_init(&entry->rating, v[0] > AE_RATING_POINTS_MAX ? AE_RATING_POINTS_MAX : v[0]);
			entry->rating.level = v[1] < AE_RATING_LEVELS ? (int)v[1] : AE_RATING_LEVELS - 1;
			entry->rating.highest = v[2] < AE_RATING_LEVELS ? (int)v[2] : AE_RATING_LEVELS - 1;
			if (entry->rating.highest < entry->rating.level)
				entry->rating.highest = entry->rating.level;
			entry->games = v[3];
			entry->wins = v[4];
			entry->quits = v[5];
			entry->last = (unsigned long)v[6];
			/* (the quit clocks after a second "|") */
			bar = strchr(bar + 1, '|');
			if (bar)
			{
				char *p = bar + 1, *stop;

				while (entry->quit_stamps < AE_CAREER_QUIT_HISTORY)
				{
					unsigned long stamp = strtoul(p, &stop, 10);

					if (stop == p)
						break;
					entry->quit_stamp[entry->quit_stamps++] = stamp;
					p = stop;
				}
			}
		}
	}
	if (!header)
		goto bad;
	if (!summed && (career->count[0] || career->count[1]))
		goto edited;
	return AE_CAREER_READ_OK;
edited:
	ae_career_init(career);
	career->edited = 1;
	return AE_CAREER_READ_EDITED;
bad:
	ae_career_init(career);
	return AE_CAREER_READ_BAD;
}
