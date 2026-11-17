/*
AE_RECENT_GAMES.C

RECENT's games. See ae_recent_games.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_recent_games.h"

enum { LINE = 256 };

/* an id: 1 to 63 printable characters, no '|', no space at either end */
static int id_valid(const char *id)
{
	size_t length = id ? strlen(id) : 0, index;

	if (!length || length >= AE_RECENT_ID || id[0] == ' ' || id[length - 1] == ' ')
		return 0;
	for (index = 0; index < length; index++)
	{
		if (id[index] < 0x20 || id[index] > 0x7E || id[index] == '|')
			return 0;
	}
	return 1;
}

void ae_recent_games_init(struct ae_recent_games *recent)
{
	if (recent)
		memset(recent, 0, sizeof(*recent));
}

int ae_recent_games_add(struct ae_recent_games *recent, const char *map, const char *gametype)
{
	struct ae_recent_game game;
	short index, kept;

	if (!recent || !id_valid(map) || !id_valid(gametype))
		return 0;
	snprintf(game.map, sizeof(game.map), "%s", map);
	snprintf(game.gametype, sizeof(game.gametype), "%s", gametype);
	/* (the same pair older down the list: dropped, so it moves to the top) */
	for (index = 0, kept = 0; index < recent->count; index++)
	{
		if (!strcmp(recent->games[index].map, map) && !strcmp(recent->games[index].gametype, gametype))
			continue;
		recent->games[kept++] = recent->games[index];
	}
	if (kept >= AE_RECENT_KEPT)
		kept = AE_RECENT_KEPT - 1;
	memmove(&recent->games[1], &recent->games[0], sizeof(recent->games[0]) * (size_t)kept);
	recent->games[0] = game;
	recent->count = (short)(kept + 1);
	return 1;
}

/* a line's text (LF or CRLF) trimmed at both ends; NULL at the text's end */
static const char *next_line(const char *text, char *line)
{
	size_t length = 0, start = 0;

	if (!*text)
		return NULL;
	while (*text && *text != '\n')
	{
		if (length < LINE - 1)
			line[length++] = *text;
		text++;
	}
	if (*text == '\n')
		text++;
	while (length && (line[length - 1] == '\r' || line[length - 1] == ' ' || line[length - 1] == '\t'))
		length--;
	line[length] = 0;
	while (line[start] == ' ' || line[start] == '\t')
		start++;
	if (start)
		memmove(line, line + start, length - start + 1);
	return text;
}

/* a part of a line trimmed in place: its start */
static char *trimmed(char *from, char *to)
{
	while (from < to && (*from == ' ' || *from == '\t'))
		from++;
	while (to > from && (to[-1] == ' ' || to[-1] == '\t'))
		to--;
	*to = 0;
	return from;
}

int ae_recent_games_read(struct ae_recent_games *recent, const char *text)
{
	char line[LINE];
	struct ae_recent_games read;

	ae_recent_games_init(recent);
	if (!recent || !text)
		return 0;
	ae_recent_games_init(&read);
	if (!strncmp(text, "\xEF\xBB\xBF", 3))
		text += 3;
	while ((text = next_line(text, line)) != NULL)
	{
		char *equals, *bar, *key;

		if (!line[0] || line[0] == '#')
			continue;
		equals = strchr(line, '=');
		if (equals && !strncmp(line, "folded", 6))
		{
			/* (folded = maps | folded = gametypes: that picker's RECENT folded) */
			char *picker;

			key = trimmed(line, equals);
			picker = trimmed(equals + 1, equals + 1 + strlen(equals + 1));
			if (strcmp(key, "folded") || (strcmp(picker, "maps") && strcmp(picker, "gametypes")))
				return 0;
			if (!strcmp(picker, "maps"))
				read.maps_folded = 1;
			else
				read.gametypes_folded = 1;
			continue;
		}
		bar = equals ? strchr(equals, '|') : NULL;
		if (!equals || !bar)
			return 0;
		key = trimmed(line, equals);
		if (strcmp(key, "game"))
			return 0;
		/* (newest first in the file: kept in its order, a repeat dropped) */
		if (read.count < AE_RECENT_KEPT)
		{
			char *map = trimmed(equals + 1, bar), *gametype = trimmed(bar + 1, bar + 1 + strlen(bar + 1));
			short index;
			int seen = 0;

			if (!id_valid(map) || !id_valid(gametype))
				continue;
			for (index = 0; index < read.count && !seen; index++)
				seen = !strcmp(read.games[index].map, map) && !strcmp(read.games[index].gametype, gametype);
			if (seen)
				continue;
			snprintf(read.games[read.count].map, AE_RECENT_ID, "%s", map);
			snprintf(read.games[read.count].gametype, AE_RECENT_ID, "%s", gametype);
			read.count++;
		}
	}
	*recent = read;
	return 1;
}

long ae_recent_games_write(struct ae_recent_games const *recent, char *buffer, long size)
{
	long length;
	short index;

	if (!recent || !buffer || size <= 0)
		return -1;
	length = snprintf(buffer, (size_t)size, "# AE recent games\n");
	if (length < 0 || length >= size)
		return -1;
	for (index = 0; index < recent->count; index++)
	{
		int written = snprintf(buffer + length, (size_t)(size - length), "game = %s | %s\n", recent->games[index].map,
			recent->games[index].gametype);

		if (written < 0 || written >= size - length)
			return -1;
		length += written;
	}
	return length;
}

/* a game's map or gametype (by) */
static const char *game_id(struct ae_recent_game const *game, int by)
{
	return by == AE_RECENT_BY_GAMETYPE ? game->gametype : game->map;
}

short ae_recent_games_by(struct ae_recent_games const *recent, int by, int (*exists)(const char *id, void *context),
	void *context, short *indices, short maximum)
{
	short index, newer, count = 0;

	if (!recent || !indices || (by != AE_RECENT_BY_MAP && by != AE_RECENT_BY_GAMETYPE))
		return 0;
	if (maximum > AE_RECENT_SHOWN)
		maximum = AE_RECENT_SHOWN;
	for (index = 0; index < recent->count && count < maximum; index++)
	{
		const char *id = game_id(&recent->games[index], by);
		int seen = 0;

		/* (a newer game of the same map or gametype: this one is that row already, or was not there) */
		for (newer = 0; newer < index && !seen; newer++)
			seen = !strcmp(game_id(&recent->games[newer], by), id);
		if (!seen && (!exists || exists(id, context)))
			indices[count++] = index;
	}
	return count;
}

long ae_recent_folds_write(struct ae_recent_games const *recent, char *buffer, long size)
{
	int length;

	if (!recent || !buffer || size <= 0)
		return -1;
	length = snprintf(buffer, (size_t)size, "# AE recent folds\n%s%s", recent->maps_folded ? "folded = maps\n" : "",
		recent->gametypes_folded ? "folded = gametypes\n" : "");
	return length < 0 || length >= size ? -1 : length;
}
