/* ae_catalog.c: Arena Evolved menus, the catalog files (see ae_catalog.h). No engine includes. */

#include <stddef.h>
#include <string.h>

#include "ae_catalog.h"

enum
{
	/* (a line longer than this is cut before it is read: no field of any format needs more) */
	MAXIMUM_LINE = 1024,
	MAXIMUM_PLAYERS = 128,
	FACT_FIELDS = 6,
	KIND_MAP = 0,
	KIND_GAMETYPE = 1,
	KIND_NONE = -1
};

static void (*catalog_log)(char const *what, char const *line);

void ae_catalog_set_log(void (*log)(char const *what, char const *line))
{
	catalog_log = log;
}

static void skip(struct ae_catalog *catalog, char const *what, char const *line)
{
	if (catalog->skipped_lines < 0x7FFF)
		catalog->skipped_lines++;
	if (catalog_log)
		catalog_log(what, line);
}

/* copies text (length bytes) into out (size bytes with its NUL), cut at a UTF-8 character's boundary */
static void copy_field(char *out, size_t size, char const *text, size_t length)
{
	if (length > size - 1)
	{
		length = size - 1;
		/* (not in the middle of a character: back to its first byte) */
		while (length > 0 && ((unsigned char)text[length] & 0xC0) == 0x80)
			length--;
	}
	memcpy(out, text, length);
	out[length] = 0;
}

/* the next line of text (without its CR LF / LF) into line; the text after it, or NULL at the end */
static char const *next_line(char const *text, char *line)
{
	size_t length;
	char const *end;

	if (!text || !*text)
		return NULL;
	end = strchr(text, '\n');
	length = end ? (size_t)(end - text) : strlen(text);
	if (length > 0 && text[length - 1] == '\r')
		length--;
	if (length > MAXIMUM_LINE - 1)
		length = MAXIMUM_LINE - 1;
	memcpy(line, text, length);
	line[length] = 0;
	return end ? end + 1 : text + strlen(text);
}

static int blank_or_comment(char const *line)
{
	return !line[0] || line[0] == '#';
}

/* an entry's kind ("map:<file>" or "gametype:<name>", a name after the prefix, no control characters, within its
room), or KIND_NONE */
static int entry_kind(char const *entry)
{
	size_t length, index;
	int kind;

	if (!entry)
		return KIND_NONE;
	length = strlen(entry);
	if (length >= AE_CATALOG_NAME)
		return KIND_NONE;
	for (index = 0; index < length; index++)
	{
		if ((unsigned char)entry[index] < 0x20 || entry[index] == 0x7F)
			return KIND_NONE;
	}
	if (!strncmp(entry, "map:", 4))
		kind = KIND_MAP;
	else if (!strncmp(entry, "gametype:", 9))
		kind = KIND_GAMETYPE;
	else
		return KIND_NONE;
	return entry[kind == KIND_MAP ? 4 : 9] ? kind : KIND_NONE;
}

/* ---------- map facts */

/* "a-b", 1 <= a <= b <= MAXIMUM_PLAYERS */
static int parse_players(char const *text, short *minimum, short *maximum)
{
	long a = 0, b = 0;
	char const *cursor = text;

	if (*cursor < '0' || *cursor > '9')
		return 0;
	while (*cursor >= '0' && *cursor <= '9' && a <= MAXIMUM_PLAYERS)
		a = a * 10 + (*cursor++ - '0');
	if (*cursor++ != '-' || *cursor < '0' || *cursor > '9')
		return 0;
	while (*cursor >= '0' && *cursor <= '9' && b <= MAXIMUM_PLAYERS)
		b = b * 10 + (*cursor++ - '0');
	if (*cursor || a < 1 || b > MAXIMUM_PLAYERS || a > b)
		return 0;
	*minimum = (short)a;
	*maximum = (short)b;
	return 1;
}

void ae_catalog_read_facts(struct ae_catalog *catalog, const char *text)
{
	char line[MAXIMUM_LINE];

	while ((text = next_line(text, line)) != NULL)
	{
		char *fields[FACT_FIELDS];
		char *cursor = line;
		struct ae_map_facts facts, *slot = NULL;
		int count = 0, index;

		if (blank_or_comment(line))
			continue;
		/* the tab-separated fields (more than six: the rest ignored) */
		while (count < FACT_FIELDS)
		{
			char *tab = strchr(cursor, '\t');

			fields[count++] = cursor;
			if (!tab)
				break;
			*tab = 0;
			cursor = tab + 1;
		}
		if (count < FACT_FIELDS)
		{
			skip(catalog, "maps.txt: fewer than 6 fields", line);
			continue;
		}
		memset(&facts, 0, sizeof(facts));
		if (!fields[0][0] || strlen(fields[0]) >= AE_CATALOG_NAME ||
			!parse_players(fields[2], &facts.players_min, &facts.players_max) ||
			strlen(fields[3]) != 1 || !strchr("sml-", fields[3][0]))
		{
			skip(catalog, "maps.txt: a bad file, players or size", fields[0]);
			continue;
		}
		copy_field(facts.file, sizeof(facts.file), fields[0], strlen(fields[0]));
		copy_field(facts.name, sizeof(facts.name), fields[1], strlen(fields[1]));
		facts.size = fields[3][0] == '-' ? 0 : fields[3][0];
		copy_field(facts.tags, sizeof(facts.tags), fields[4], strlen(fields[4]));
		copy_field(facts.credits, sizeof(facts.credits), fields[5], strlen(fields[5]));
		for (index = 0; index < catalog->facts_count; index++)
		{
			if (!strcmp(catalog->facts[index].file, facts.file))
				slot = &catalog->facts[index];
		}
		if (!slot)
		{
			if (catalog->facts_count >= AE_CATALOG_MAXIMUM_ITEMS)
			{
				skip(catalog, "maps.txt: more maps than the catalog holds", facts.file);
				continue;
			}
			slot = &catalog->facts[catalog->facts_count++];
		}
		*slot = facts;
	}
}

struct ae_map_facts const *ae_catalog_facts(struct ae_catalog const *catalog, const char *file)
{
	int index;

	for (index = 0; file && index < catalog->facts_count; index++)
	{
		if (!strcmp(catalog->facts[index].file, file))
			return &catalog->facts[index];
	}
	return NULL;
}

/* ---------- favourites */

static int favourite_index(struct ae_catalog const *catalog, char const *entry)
{
	int index;

	for (index = 0; index < catalog->favourite_count; index++)
	{
		if (!strcmp(catalog->favourites[index], entry))
			return index;
	}
	return -1;
}

void ae_catalog_read_favourites(struct ae_catalog *catalog, const char *text)
{
	char line[MAXIMUM_LINE];

	while ((text = next_line(text, line)) != NULL)
	{
		if (blank_or_comment(line))
			continue;
		if (entry_kind(line) == KIND_NONE)
		{
			skip(catalog, "favourites: not a map: or gametype: entry", line);
			continue;
		}
		if (favourite_index(catalog, line) >= 0)
			continue;
		if (catalog->favourite_count >= AE_CATALOG_MAXIMUM_ITEMS)
		{
			skip(catalog, "favourites: more than the catalog holds", line);
			continue;
		}
		strcpy(catalog->favourites[catalog->favourite_count++], line);
	}
}

int ae_catalog_favourite_toggle(struct ae_catalog *catalog, const char *entry)
{
	int index;

	if (entry_kind(entry) == KIND_NONE)
		return 0;
	index = favourite_index(catalog, entry);
	if (index >= 0)
	{
		memmove(catalog->favourites[index], catalog->favourites[index + 1],
			(size_t)(catalog->favourite_count - index - 1) * AE_CATALOG_NAME);
		catalog->favourite_count--;
		return 0;
	}
	if (catalog->favourite_count >= AE_CATALOG_MAXIMUM_ITEMS)
		return 0;
	strcpy(catalog->favourites[catalog->favourite_count++], entry);
	return 1;
}

/* ---------- recent */

static void recent_remove(struct ae_catalog *catalog, int index)
{
	memmove(catalog->recent[index], catalog->recent[index + 1],
		(size_t)(catalog->recent_count - index - 1) * AE_CATALOG_NAME);
	catalog->recent_count--;
}

/* how many of a kind the list holds */
static int recent_of_kind(struct ae_catalog const *catalog, int kind)
{
	int index, count = 0;

	for (index = 0; index < catalog->recent_count; index++)
		count += entry_kind(catalog->recent[index]) == kind;
	return count;
}

void ae_catalog_recent_push(struct ae_catalog *catalog, const char *entry)
{
	int kind = entry_kind(entry), index;

	if (kind == KIND_NONE)
		return;
	for (index = 0; index < catalog->recent_count; index++)
	{
		if (!strcmp(catalog->recent[index], entry))
		{
			recent_remove(catalog, index);
			break;
		}
	}
	/* (the oldest of its kind goes when it already has its 10) */
	if (recent_of_kind(catalog, kind) >= AE_CATALOG_RECENT)
	{
		for (index = catalog->recent_count - 1; index >= 0; index--)
		{
			if (entry_kind(catalog->recent[index]) == kind)
			{
				recent_remove(catalog, index);
				break;
			}
		}
	}
	memmove(catalog->recent[1], catalog->recent[0], (size_t)catalog->recent_count * AE_CATALOG_NAME);
	strcpy(catalog->recent[0], entry);
	catalog->recent_count++;
}

void ae_catalog_read_recent(struct ae_catalog *catalog, const char *text)
{
	char line[MAXIMUM_LINE];

	while ((text = next_line(text, line)) != NULL)
	{
		int kind, index, known = 0;

		if (blank_or_comment(line))
			continue;
		kind = entry_kind(line);
		if (kind == KIND_NONE)
		{
			skip(catalog, "recent: not a map: or gametype: entry", line);
			continue;
		}
		/* (newest first: a line read later is older, kept only while its kind has room) */
		for (index = 0; index < catalog->recent_count; index++)
			known |= !strcmp(catalog->recent[index], line);
		if (known)
			continue;
		if (recent_of_kind(catalog, kind) >= AE_CATALOG_RECENT)
		{
			skip(catalog, "recent: more than 10 of a kind", line);
			continue;
		}
		strcpy(catalog->recent[catalog->recent_count++], line);
	}
}

/* ---------- playlists */

void ae_catalog_read_playlists(struct ae_catalog *catalog, const char *text)
{
	char line[MAXIMUM_LINE];
	struct ae_playlist *playlist = NULL;

	while ((text = next_line(text, line)) != NULL)
	{
		size_t length = strlen(line);

		if (blank_or_comment(line))
			continue;
		if (line[0] == '[')
		{
			playlist = NULL;
			if (length < 3 || line[length - 1] != ']')
			{
				skip(catalog, "playlists: a bad [name]", line);
				continue;
			}
			if (catalog->playlist_count >= AE_CATALOG_PLAYLISTS)
			{
				skip(catalog, "playlists: more playlists than the catalog holds", line);
				continue;
			}
			playlist = &catalog->playlists[catalog->playlist_count++];
			memset(playlist, 0, sizeof(*playlist));
			copy_field(playlist->name, sizeof(playlist->name), line + 1, length - 2);
			continue;
		}
		if (!playlist)
		{
			skip(catalog, "playlists: a map outside a playlist", line);
			continue;
		}
		if (length >= AE_CATALOG_NAME || strchr(line, '\t'))
		{
			skip(catalog, "playlists: a bad map file", line);
			continue;
		}
		if (playlist->count >= AE_CATALOG_PLAYLIST_MAPS)
		{
			skip(catalog, "playlists: more maps than a playlist holds", line);
			continue;
		}
		strcpy(playlist->maps[playlist->count++], line);
	}
}

/* ---------- writers */

/* appends text and a newline at *used; 0 when it doesn't fit (with its NUL) */
static int append_line(char *buffer, long size, long *used, char const *before, char const *text, char const *after)
{
	size_t length = strlen(before) + strlen(text) + strlen(after) + 1;

	if (!buffer || size <= 0 || (long)length >= size - *used)
		return 0;
	strcpy(buffer + *used, before);
	strcat(buffer + *used, text);
	strcat(buffer + *used, after);
	strcat(buffer + *used, "\n");
	*used += (long)length;
	return 1;
}

static long write_entries(char const (*entries)[AE_CATALOG_NAME], int count, char const *heading, char *buffer,
	long size)
{
	long used = 0;
	int index;

	if (!append_line(buffer, size, &used, "", heading, ""))
		return -1;
	for (index = 0; index < count; index++)
	{
		if (!append_line(buffer, size, &used, "", entries[index], ""))
			return -1;
	}
	return used;
}

long ae_catalog_write_favourites(struct ae_catalog const *catalog, char *buffer, long size)
{
	return write_entries(catalog->favourites, catalog->favourite_count,
		"# Arena Evolved favourites: map:<file> or gametype:<name>, one a line", buffer, size);
}

long ae_catalog_write_recent(struct ae_catalog const *catalog, char *buffer, long size)
{
	return write_entries(catalog->recent, catalog->recent_count,
		"# Arena Evolved recent maps and gametypes, newest first", buffer, size);
}

long ae_catalog_write_playlists(struct ae_catalog const *catalog, char *buffer, long size)
{
	long used = 0;
	int playlist, map;

	if (!append_line(buffer, size, &used, "", "# Arena Evolved playlists: [name], then its map files", ""))
		return -1;
	for (playlist = 0; playlist < catalog->playlist_count; playlist++)
	{
		struct ae_playlist const *list = &catalog->playlists[playlist];

		if (!append_line(buffer, size, &used, "[", list->name, "]"))
			return -1;
		for (map = 0; map < list->count; map++)
		{
			if (!append_line(buffer, size, &used, "", list->maps[map], ""))
				return -1;
		}
	}
	return used;
}
