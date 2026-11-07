/* The service record (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_service_record.h"

static char const *const mode_names[AE_SR_MODES] = { "SLAYER", "FLAG", "BALL", "HILL", "RACE", "BOMB", "OTHER" };

void ae_sr_init(struct ae_service_record *record)
{
	memset(record, 0, sizeof(*record));
}

char const *ae_sr_mode_name(int mode)
{
	return mode >= 0 && mode < AE_SR_MODES ? mode_names[mode] : "OTHER";
}

void ae_sr_key(char const *name, char *key, size_t size)
{
	size_t at = 0;

	if (size == 0)
		return;
	while (name && *name == ' ')
		name++;
	for (; name && *name && at + 1 < size && at + 1 < AE_SR_KEY; name++)
	{
		unsigned char c = (unsigned char)*name;

		if (c < 32 || c == 127 || c == '|')
			c = '?';
		else if (c >= 'A' && c <= 'Z')
			c = (unsigned char)(c + 32);
		key[at++] = (char)c;
	}
	while (at > 0 && key[at - 1] == ' ')
		at--;
	key[at] = 0;
}

/* a name as it is kept: "|" and control characters turned to "?", cut */
static void clean_name(char const *name, char *out, size_t size)
{
	size_t at = 0;

	for (; name && *name && at + 1 < size; name++)
	{
		unsigned char c = (unsigned char)*name;

		out[at++] = (char)(c < 32 || c == 127 || c == '|' ? '?' : c);
	}
	out[at] = 0;
}

struct ae_sr_player const *ae_sr_find(struct ae_service_record const *record, char const *key)
{
	int index;

	for (index = 0; index < record->player_count; index++)
	{
		if (!strcmp(record->player[index].key, key))
			return &record->player[index];
	}
	return NULL;
}

/* whether a key is an owner, a rival or in a versus: never pushed out */
static int held(struct ae_service_record const *record, char const *key)
{
	int index;

	for (index = 0; index < record->rival_count; index++)
	{
		if (!strcmp(record->rival[index].owner, key) || !strcmp(record->rival[index].key, key))
			return 1;
	}
	return 0;
}

static struct ae_sr_player *take(struct ae_service_record *record, char const *key, char const *name, int protect)
{
	struct ae_sr_player *player = (struct ae_sr_player *)ae_sr_find(record, key);
	int index, oldest = -1;

	if (player)
		return player;
	if (record->player_count < AE_SR_PLAYERS)
		player = &record->player[record->player_count++];
	else
	{
		for (index = 0; index < record->player_count; index++)
		{
			if (!held(record, record->player[index].key) &&
				(oldest < 0 || record->player[index].seen < record->player[oldest].seen))
				oldest = index;
		}
		if (oldest < 0)
			return NULL;
		player = &record->player[oldest];
	}
	(void)protect;
	memset(player, 0, sizeof(*player));
	snprintf(player->key, sizeof(player->key), "%s", key);
	clean_name(name, player->name, sizeof(player->name));
	return player;
}

static void add_line(struct ae_sr_line *line, int won, long kills, long deaths, long assists)
{
	line->games++;
	line->wins += won ? 1 : 0;
	line->kills += kills > 0 ? kills : 0;
	line->deaths += deaths > 0 ? deaths : 0;
	line->assists += assists > 0 ? assists : 0;
}

static struct ae_sr_versus *versus_slot(struct ae_service_record *record, char const *owner, char const *rival)
{
	int index;

	for (index = 0; index < record->versus_count; index++)
	{
		if (!strcmp(record->versus[index].owner, owner) && !strcmp(record->versus[index].rival, rival))
			return &record->versus[index];
	}
	if (record->versus_count >= AE_SR_RIVALS)
		return NULL;
	index = record->versus_count++;
	memset(&record->versus[index], 0, sizeof(record->versus[index]));
	snprintf(record->versus[index].owner, AE_SR_KEY, "%s", owner);
	snprintf(record->versus[index].rival, AE_SR_KEY, "%s", rival);
	return &record->versus[index];
}

struct ae_sr_versus const *ae_sr_versus_of(struct ae_service_record const *record, char const *owner, char const *rival)
{
	int index;
	char owner_key[AE_SR_KEY], rival_key[AE_SR_KEY];

	ae_sr_key(owner, owner_key, sizeof(owner_key));
	ae_sr_key(rival, rival_key, sizeof(rival_key));
	for (index = 0; index < record->versus_count; index++)
	{
		if (!strcmp(record->versus[index].owner, owner_key) && !strcmp(record->versus[index].rival, rival_key))
			return &record->versus[index];
	}
	return NULL;
}

int ae_sr_is_rival(struct ae_service_record const *record, char const *owner, char const *name)
{
	char owner_key[AE_SR_KEY], key[AE_SR_KEY];
	int index;

	ae_sr_key(owner, owner_key, sizeof(owner_key));
	ae_sr_key(name, key, sizeof(key));
	if (!key[0] || !owner_key[0])
		return 0;
	for (index = 0; index < record->rival_count; index++)
	{
		if (!strcmp(record->rival[index].owner, owner_key) && !strcmp(record->rival[index].key, key))
			return 1;
	}
	return 0;
}

int ae_sr_rival_set(struct ae_service_record *record, char const *owner, char const *name, int on)
{
	char owner_key[AE_SR_KEY], key[AE_SR_KEY];
	int index, mine = 0;

	ae_sr_key(owner, owner_key, sizeof(owner_key));
	ae_sr_key(name, key, sizeof(key));
	if (!key[0] || !owner_key[0] || !strcmp(owner_key, key))
		return 0;
	for (index = 0; index < record->rival_count; index++)
	{
		if (strcmp(record->rival[index].owner, owner_key))
			continue;
		mine++;
		if (!strcmp(record->rival[index].key, key))
		{
			if (on)
				return 1;
			/* (taken off: the versus too, nothing else is kept of it) */
			memmove(&record->rival[index], &record->rival[index + 1],
				(size_t)(record->rival_count - index - 1) * sizeof(record->rival[0]));
			record->rival_count--;
			for (index = 0; index < record->versus_count; index++)
			{
				if (!strcmp(record->versus[index].owner, owner_key) && !strcmp(record->versus[index].rival, key))
				{
					memmove(&record->versus[index], &record->versus[index + 1],
						(size_t)(record->versus_count - index - 1) * sizeof(record->versus[0]));
					record->versus_count--;
					break;
				}
			}
			return 1;
		}
	}
	if (!on)
		return 1;
	if (mine >= AE_SR_RIVALS_PER_OWNER || record->rival_count >= AE_SR_RIVALS)
		return 0;
	index = record->rival_count++;
	snprintf(record->rival[index].owner, AE_SR_KEY, "%s", owner_key);
	snprintf(record->rival[index].key, AE_SR_KEY, "%s", key);
	clean_name(name, record->rival[index].name, AE_SR_NAME);
	return 1;
}

int ae_sr_record_game(struct ae_service_record *record, struct ae_sr_game const *game)
{
	int index, other, added = 0, mode = game->mode >= 0 && game->mode < AE_SR_MODES ? game->mode : AE_SR_MODES - 1;
	char keys[AE_PGCR_PLAYERS][AE_SR_KEY];

	if (game->count <= 0)
		return 0;
	record->clock++;
	for (index = 0; index < game->count && index < AE_PGCR_PLAYERS; index++)
	{
		struct ae_sr_game_player const *in = &game->player[index];
		struct ae_sr_player *player;
		int medal;

		ae_sr_key(in->name, keys[index], sizeof(keys[index]));
		if (!keys[index][0])
			continue;
		player = take(record, keys[index], in->name, in->me);
		if (!player)
		{
			keys[index][0] = 0;
			continue;
		}
		added++;
		player->seen = record->clock;
		clean_name(in->name, player->name, sizeof(player->name));
		add_line(&player->total, in->won && !in->quit, in->kills, in->deaths, in->assists);
		add_line(&player->mode[mode], in->won && !in->quit, in->kills, in->deaths, in->assists);
		for (medal = 0; medal < in->medal_count && medal < AE_PGCR_SUMMARY_MEDALS; medal++)
		{
			unsigned int id = in->medals[medal].id;

			if (id < AE_MEDAL_COUNT)
				player->medals[id] += in->medals[medal].count;
		}
	}
	/* the owners' rivals in this game */
	for (index = 0; index < game->count && index < AE_PGCR_PLAYERS; index++)
	{
		struct ae_sr_game_player const *mine = &game->player[index];

		if (!mine->me || !keys[index][0])
			continue;
		for (other = 0; other < game->count && other < AE_PGCR_PLAYERS; other++)
		{
			struct ae_sr_versus *versus;
			int entry;

			if (other == index || !keys[other][0] || !ae_sr_is_rival(record, keys[index], keys[other]))
				continue;
			versus = versus_slot(record, keys[index], keys[other]);
			if (!versus)
				continue;
			versus->games++;
			for (entry = 0; entry < mine->versus_count && entry < AE_PGCR_SUMMARY_VERSUS; entry++)
			{
				if (mine->versus[entry].slot == game->player[other].slot)
				{
					versus->kills += mine->versus[entry].killed;
					versus->deaths += mine->versus[entry].killed_by;
				}
			}
		}
	}
	return added > 0;
}

/* ---------- the file */

static long put(char *buffer, long size, long at, char const *format, ...) __attribute__((format(printf, 4, 5)));

#include <stdarg.h>
static long put(char *buffer, long size, long at, char const *format, ...)
{
	va_list args;
	int written;

	if (at < 0 || at >= size)
		return -1;
	va_start(args, format);
	written = vsnprintf(buffer + at, (size_t)(size - at), format, args);
	va_end(args);
	if (written < 0 || at + written >= size)
		return -1;
	return at + written;
}

long ae_sr_write(struct ae_service_record const *record, char *buffer, long size)
{
	long at = 0;
	int index, mode, medal;

	at = put(buffer, size, at, "# AE service record\nversion = %d\nclock = %lu\n", AE_SR_VERSION, record->clock);
	for (index = 0; index < record->player_count && at >= 0; index++)
	{
		struct ae_sr_player const *p = &record->player[index];
		int first = 1;

		at = put(buffer, size, at, "player = %s | %s | %lu | %ld %ld %ld %ld %ld |", p->key, p->name, p->seen, p->total.games,
			p->total.wins, p->total.kills, p->total.deaths, p->total.assists);
		for (mode = 0; mode < AE_SR_MODES && at >= 0; mode++)
			at = put(buffer, size, at, " %ld %ld %ld %ld %ld", p->mode[mode].games, p->mode[mode].wins, p->mode[mode].kills,
				p->mode[mode].deaths, p->mode[mode].assists);
		at = put(buffer, size, at, " |");
		for (medal = 0; medal < AE_MEDAL_COUNT && at >= 0; medal++)
		{
			if (!p->medals[medal])
				continue;
			at = put(buffer, size, at, "%s%d:%lu", first ? " " : ",", medal, p->medals[medal]);
			first = 0;
		}
		at = put(buffer, size, at, "\n");
	}
	for (index = 0; index < record->rival_count && at >= 0; index++)
		at = put(buffer, size, at, "rival = %s | %s | %s\n", record->rival[index].owner, record->rival[index].key,
			record->rival[index].name);
	for (index = 0; index < record->versus_count && at >= 0; index++)
		at = put(buffer, size, at, "versus = %s | %s | %ld %ld %ld\n", record->versus[index].owner, record->versus[index].rival,
			record->versus[index].games, record->versus[index].kills, record->versus[index].deaths);
	return at;
}

/* a line's "|" fields, trimmed, in place; the count */
static int split(char *text, char **fields, int maximum)
{
	int count = 0;
	char *at = text;

	while (count < maximum)
	{
		char *bar = strchr(at, '|');
		char *end;

		while (*at == ' ')
			at++;
		fields[count++] = at;
		if (bar)
			*bar = 0;
		end = at + strlen(at);
		while (end > at && end[-1] == ' ')
			*--end = 0;
		if (!bar)
			break;
		at = bar + 1;
	}
	return count;
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

int ae_sr_read(struct ae_service_record *record, char const *text)
{
	char line[1024];
	int header = 0;
	long version = 0;

	ae_sr_init(record);
	while (*text)
	{
		size_t length = strcspn(text, "\n");
		char *equals, *key, *value, *fields[8];
		int count;

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
		{
			char *end = key + strlen(key);

			while (end > key && end[-1] == ' ')
				*--end = 0;
		}
		if (!header)
		{
			if (strcmp(key, "version") || !numbers(value, &version, 1) || version < 1)
				goto bad;
			header = 1;
			record->newer = version > AE_SR_VERSION;
			continue;
		}
		if (!strcmp(key, "clock"))
		{
			long clock;

			if (!numbers(value, &clock, 1))
				goto bad;
			record->clock = (unsigned long)clock;
		}
		else if (!strcmp(key, "player"))
		{
			long v[5 + 5 * AE_SR_MODES + 1];
			struct ae_sr_player *p;
			int mode;
			char *medal;

			count = split(value, fields, 8);
			if (count < 6 || record->player_count >= AE_SR_PLAYERS || !fields[0][0] || strlen(fields[0]) >= AE_SR_KEY)
				continue;
			if (!numbers(fields[2], v, 1) || !numbers(fields[3], v + 1, 5) || !numbers(fields[4], v + 6, 5 * AE_SR_MODES))
				continue;
			p = &record->player[record->player_count++];
			memset(p, 0, sizeof(*p));
			snprintf(p->key, sizeof(p->key), "%s", fields[0]);
			snprintf(p->name, sizeof(p->name), "%s", fields[1]);
			p->seen = (unsigned long)v[0];
			p->total.games = v[1]; p->total.wins = v[2]; p->total.kills = v[3]; p->total.deaths = v[4]; p->total.assists = v[5];
			for (mode = 0; mode < AE_SR_MODES; mode++)
			{
				p->mode[mode].games = v[6 + mode * 5]; p->mode[mode].wins = v[7 + mode * 5];
				p->mode[mode].kills = v[8 + mode * 5]; p->mode[mode].deaths = v[9 + mode * 5];
				p->mode[mode].assists = v[10 + mode * 5];
			}
			/* (the medals: id:count, the ones this build knows) */
			for (medal = fields[5]; *medal;)
			{
				char *colon;
				long id = strtol(medal, &colon, 10), amount;

				if (colon == medal || *colon != ':')
					break;
				amount = strtol(colon + 1, &medal, 10);
				if (id >= 0 && id < AE_MEDAL_COUNT && amount >= 0)
					p->medals[id] = (unsigned long)amount;
				if (*medal == ',')
					medal++;
				else
					break;
			}
		}
		else if (!strcmp(key, "rival"))
		{
			count = split(value, fields, 8);
			if (count < 3 || record->rival_count >= AE_SR_RIVALS || !fields[0][0] || !fields[1][0] ||
				strlen(fields[0]) >= AE_SR_KEY || strlen(fields[1]) >= AE_SR_KEY)
				continue;
			snprintf(record->rival[record->rival_count].owner, AE_SR_KEY, "%s", fields[0]);
			snprintf(record->rival[record->rival_count].key, AE_SR_KEY, "%s", fields[1]);
			snprintf(record->rival[record->rival_count].name, AE_SR_NAME, "%s", fields[2]);
			record->rival_count++;
		}
		else if (!strcmp(key, "versus"))
		{
			long v[3];

			count = split(value, fields, 8);
			if (count < 3 || record->versus_count >= AE_SR_RIVALS || !fields[0][0] || !fields[1][0] ||
				strlen(fields[0]) >= AE_SR_KEY || strlen(fields[1]) >= AE_SR_KEY || !numbers(fields[2], v, 3))
				continue;
			snprintf(record->versus[record->versus_count].owner, AE_SR_KEY, "%s", fields[0]);
			snprintf(record->versus[record->versus_count].rival, AE_SR_KEY, "%s", fields[1]);
			record->versus[record->versus_count].games = v[0];
			record->versus[record->versus_count].kills = v[1];
			record->versus[record->versus_count].deaths = v[2];
			record->versus_count++;
		}
		/* (any other kind: a newer build's, skipped) */
	}
	if (!header)
		goto bad;
	return 1;
bad:
	ae_sr_init(record);
	return 0;
}
