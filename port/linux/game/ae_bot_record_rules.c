/*
AE_BOT_RECORD_RULES.C

Bot service records, as pure rules (ae_bot_record_rules.h).
*/

#include "ae_bot_record_rules.h"
#include "ae_service_record.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ae_bot_records_init(struct ae_bot_records *records)
{
	memset(records, 0, sizeof(*records));
}

void ae_bot_record_key(char const *name, char *key, size_t size)
{
	ae_sr_key(name, key, size < AE_BOT_RECORD_KEY ? size : AE_BOT_RECORD_KEY);
}

/* a name or id made safe for the file: "|", ",", ":" and control characters as "?", spaces at the ends gone, cut */
static void safe(char const *name, char *out, size_t size)
{
	size_t at = 0;

	if (!size)
		return;
	while (name && *name == ' ')
		name++;
	for (; name && *name && at + 1 < size; name++)
	{
		unsigned char c = (unsigned char)*name;

		out[at++] = (char)(c == '|' || c == ',' || c == ':' || c < 32 || c == 127 ? '?' : c);
	}
	while (at > 0 && out[at - 1] == ' ')
		at--;
	out[at] = 0;
}

static struct ae_bot_record *find_key(struct ae_bot_records *records, char const *key)
{
	int index;

	for (index = 0; key[0] && index < records->count; index++)
		if (!strcmp(records->bot[index].key, key))
			return &records->bot[index];
	return NULL;
}

struct ae_bot_record *ae_bot_record_find(struct ae_bot_records *records, char const *name)
{
	char key[AE_BOT_RECORD_KEY];

	ae_bot_record_key(name, key, sizeof(key));
	return find_key(records, key);
}

static struct ae_bot_versus *versus_key(struct ae_bot_records *records, char const *profile, char const *bot)
{
	int index;

	for (index = 0; profile[0] && bot[0] && index < records->versus_count; index++)
		if (!strcmp(records->versus[index].profile, profile) && !strcmp(records->versus[index].bot, bot))
			return &records->versus[index];
	return NULL;
}

struct ae_bot_versus *ae_bot_record_versus(struct ae_bot_records *records, char const *profile, char const *bot)
{
	char p[AE_BOT_RECORD_KEY], b[AE_BOT_RECORD_KEY];

	ae_bot_record_key(profile, p, sizeof(p));
	ae_bot_record_key(bot, b, sizeof(b));
	return versus_key(records, p, b);
}

int ae_bot_record_result(struct ae_bot_game const *game, int team, int place)
{
	if (game->teams)
	{
		long mine = team == 1 ? game->blue_score : game->red_score, theirs = team == 1 ? game->red_score : game->blue_score;

		return mine > theirs ? AE_BOT_RECORD_WIN : mine < theirs ? AE_BOT_RECORD_LOSS : AE_BOT_RECORD_TIE;
	}
	return place >= 0 && game->players > 0 && place < (game->players + 1) / 2 ? AE_BOT_RECORD_WIN : AE_BOT_RECORD_LOSS;
}

/* a versus line taken away (the table's order kept) */
static void versus_remove(struct ae_bot_records *records, int index)
{
	memmove(&records->versus[index], &records->versus[index + 1],
		sizeof(records->versus[0]) * (size_t)(records->versus_count - index - 1));
	records->versus_count--;
}

/* a bot's versus lines taken away */
static void versus_forget_bot(struct ae_bot_records *records, char const *key)
{
	int index = 0;

	while (index < records->versus_count)
		if (!strcmp(records->versus[index].bot, key))
			versus_remove(records, index);
		else
			index++;
}

/* a bot's record made: the next free place, or the place of the bot with the oldest last game (the fewest games on a tie),
   whose versus lines go with it */
static struct ae_bot_record *record_make(struct ae_bot_records *records, char const *key)
{
	struct ae_bot_record *record;
	int index, oldest = 0;

	if (records->count < AE_BOT_RECORD_BOTS)
		record = &records->bot[records->count++];
	else
	{
		for (index = 1; index < records->count; index++)
		{
			struct ae_bot_record const *a = &records->bot[index], *b = &records->bot[oldest];

			if (a->last < b->last || (a->last == b->last && a->games < b->games))
				oldest = index;
		}
		record = &records->bot[oldest];
		versus_forget_bot(records, record->key);
	}
	memset(record, 0, sizeof(*record));
	snprintf(record->key, sizeof(record->key), "%s", key);
	record->style = -1;
	record->rank = AE_BOT_RECORD_NO_RANK;
	return record;
}

static struct ae_bot_versus *versus_make(struct ae_bot_records *records, char const *profile, char const *bot)
{
	struct ae_bot_versus *versus;
	int index, oldest = 0;

	if (records->versus_count < AE_BOT_RECORD_VERSUS)
		versus = &records->versus[records->versus_count++];
	else
	{
		for (index = 1; index < records->versus_count; index++)
			if (records->versus[index].last < records->versus[oldest].last)
				oldest = index;
		versus = &records->versus[oldest];
	}
	memset(versus, 0, sizeof(*versus));
	snprintf(versus->profile, sizeof(versus->profile), "%s", profile);
	snprintf(versus->bot, sizeof(versus->bot), "%s", bot);
	return versus;
}

static void weapon_add(struct ae_bot_record *record, struct ae_bot_weapon const *weapon)
{
	char name[AE_BOT_RECORD_WEAPON];
	int index, fewest = 0;

	safe(weapon->name, name, sizeof(name));
	if (!name[0] || weapon->kills <= 0)
		return;
	for (index = 0; index < record->weapon_count; index++)
		if (!strcmp(record->weapons[index].name, name))
		{
			record->weapons[index].kills += weapon->kills;
			break;
		}
	if (index == record->weapon_count)
	{
		if (record->weapon_count < AE_BOT_RECORD_WEAPONS)
			index = record->weapon_count++;
		else
		{
			for (index = 1; index < AE_BOT_RECORD_WEAPONS; index++)
				if (record->weapons[index].kills < record->weapons[fewest].kills)
					fewest = index;
			if (record->weapons[fewest].kills >= weapon->kills)
				return;
			index = fewest;
		}
		snprintf(record->weapons[index].name, sizeof(record->weapons[index].name), "%s", name);
		record->weapons[index].kills = weapon->kills;
	}
	/* (the most kills first) */
	while (index > 0 && record->weapons[index].kills > record->weapons[index - 1].kills)
	{
		struct ae_bot_weapon swap = record->weapons[index];

		record->weapons[index] = record->weapons[index - 1];
		record->weapons[index - 1] = swap;
		index--;
	}
}

static long positive(long value)
{
	return value > 0 ? value : 0;
}

int ae_bot_records_add_game(struct ae_bot_records *records, struct ae_bot_game const *game, struct ae_bot_game_bot const *bots,
	int bot_count)
{
	int index, profile, added = 0;

	records->clock++;
	for (index = 0; index < bot_count; index++)
	{
		struct ae_bot_game_bot const *bot = &bots[index];
		char key[AE_BOT_RECORD_KEY];
		struct ae_bot_record *record;
		int result, weapon;

		ae_bot_record_key(bot->name, key, sizeof(key));
		if (!key[0])
			continue;
		record = find_key(records, key);
		if (!record)
		{
			record = record_make(records, key);
			record->first = records->clock;
			safe(game->playlist, record->first_playlist, sizeof(record->first_playlist));
			record->tier_low = record->tier_high = bot->tier;
		}
		safe(bot->name, record->name, sizeof(record->name));
		result = ae_bot_record_result(game, bot->team, bot->place);
		record->games++;
		if (result == AE_BOT_RECORD_WIN)
			record->wins++;
		else if (result == AE_BOT_RECORD_TIE)
			record->ties++;
		record->kills += positive(bot->kills);
		record->deaths += positive(bot->deaths);
		record->assists += positive(bot->assists);
		if (bot->spree > record->best_spree)
			record->best_spree = bot->spree;
		record->last = records->clock;
		record->shooting = bot->shooting;
		record->movement = bot->movement;
		if (bot->tier > 0 && (record->tier_low <= 0 || bot->tier < record->tier_low))
			record->tier_low = bot->tier;
		if (bot->tier > record->tier_high)
			record->tier_high = bot->tier;
		record->style = bot->style;
		record->favourite = bot->favourite;
		for (weapon = 0; weapon < bot->weapon_count && weapon < AE_BOT_GAME_WEAPONS; weapon++)
			weapon_add(record, &bot->weapons[weapon]);
		/* (each profile of the game: against (the other team; a free for all always) or with it) */
		for (profile = 0; profile < game->profile_count && profile < AE_BOT_GAME_PROFILES; profile++)
		{
			struct ae_bot_game_profile const *who = &game->profile[profile];
			char profile_key[AE_BOT_RECORD_KEY];
			struct ae_bot_versus *versus;
			int with = game->teams && who->team == bot->team;

			ae_bot_record_key(who->key, profile_key, sizeof(profile_key));
			if (!profile_key[0])
				continue;
			versus = versus_key(records, profile_key, key);
			if (!versus)
				versus = versus_make(records, profile_key, key);
			if (with)
			{
				versus->with++;
				if (result == AE_BOT_RECORD_WIN)
					versus->with_won++;
			}
			else
			{
				int you = game->teams ? ae_bot_record_result(game, who->team, who->place) : AE_BOT_RECORD_TIE;

				versus->against++;
				if (game->teams)
				{
					if (you == AE_BOT_RECORD_WIN)
						versus->you_won++;
					else if (you == AE_BOT_RECORD_LOSS)
						versus->it_won++;
				}
				else if (who->place >= 0 && bot->place >= 0 && who->place != bot->place)
				{
					if (who->place < bot->place)
						versus->you_won++;
					else
						versus->it_won++;
				}
			}
			versus->your_kills += positive(bot->killed_by[profile]);
			versus->its_kills += positive(bot->kills_on[profile]);
			versus->last = records->clock;
			versus->lastgame.has = 1;
			safe(game->playlist, versus->lastgame.playlist, sizeof(versus->lastgame.playlist));
			safe(game->map, versus->lastgame.map, sizeof(versus->lastgame.map));
			versus->lastgame.result = result;
			versus->lastgame.kills = positive(bot->kills);
			versus->lastgame.deaths = positive(bot->deaths);
			versus->lastgame.assists = positive(bot->assists);
			versus->lastgame.spree = positive(bot->spree);
			versus->lastgame.its_kills = positive(bot->kills_on[profile]);
			versus->lastgame.place = game->teams ? -1 : bot->place;
		}
		added++;
	}
	return added;
}

int ae_bot_records_forget(struct ae_bot_records *records, char const *name)
{
	struct ae_bot_record *record;
	int count;

	if (!name)
	{
		count = records->count;
		records->count = 0;
		records->versus_count = 0;
		return count;
	}
	record = ae_bot_record_find(records, name);
	if (!record)
		return 0;
	versus_forget_bot(records, record->key);
	count = (int)(record - records->bot);
	memmove(record, record + 1, sizeof(*record) * (size_t)(records->count - count - 1));
	records->count--;
	return 1;
}

long ae_bot_record_kd100(long kills, long deaths)
{
	kills = positive(kills);
	return deaths > 0 ? (kills * 100 + deaths / 2) / deaths : kills * 100;
}

int ae_bot_record_is_rival(struct ae_bot_versus const *versus)
{
	long gap;

	if (!versus || versus->against < AE_BOT_RIVAL_GAMES)
		return 0;
	gap = versus->you_won - versus->it_won;
	return (gap < 0 ? -gap : gap) <= AE_BOT_RIVAL_WITHIN;
}

static char const *times(long count)
{
	return count == 1 ? "time" : "times";
}

void ae_bot_record_rival_line(struct ae_bot_versus const *versus, char *head, size_t head_size, char *sentence,
	size_t sentence_size, char *kills_sentence, size_t kills_size)
{
	char kills[128];
	char top[64];
	char text[256];

	top[0] = text[0] = kills[0] = 0;
	if (versus)
	{
		if (versus->its_kills == versus->your_kills)
			snprintf(kills, sizeof(kills), "Kills %ld each way.", versus->its_kills);
		else
			snprintf(kills, sizeof(kills), "It has killed you %ld %s; you have killed it %ld.", versus->its_kills,
				times(versus->its_kills), versus->your_kills);
		if (versus->against + versus->with == 1)
		{
			snprintf(top, sizeof(top), "FIRST MEETING");
			if (versus->with)
				snprintf(text, sizeof(text), "Your first game with it: on your team.");
			else
				snprintf(text, sizeof(text), "Your first game against it: %s.", versus->you_won ? "you placed above it" :
					versus->it_won ? "it placed above you" : "neither placed above");
		}
		else if (versus->against == 0 && versus->with > 0)
		{
			snprintf(top, sizeof(top), "TEAMMATE");
			snprintf(text, sizeof(text), "Only on your team so far: %ld %s, %ld %s.", versus->with,
				versus->with == 1 ? "game" : "games", versus->with_won, versus->with_won == 1 ? "win" : "wins");
		}
		else if (versus->you_won > versus->it_won)
		{
			snprintf(top, sizeof(top), "YOU LEAD %ld-%ld", versus->you_won, versus->it_won);
			snprintf(text, sizeof(text), "You have the edge. %s", kills);
		}
		else if (versus->it_won > versus->you_won)
		{
			snprintf(top, sizeof(top), "IT LEADS %ld-%ld", versus->it_won, versus->you_won);
			snprintf(text, sizeof(text), "It has the edge on you. %s", kills);
		}
		else if (versus->against > 0 || versus->with > 0)
		{
			snprintf(top, sizeof(top), "EVEN %ld-%ld", versus->you_won, versus->it_won);
			snprintf(text, sizeof(text), "Nothing between you. %s", kills);
		}
		else
			kills[0] = 0;
	}
	if (head && head_size)
		snprintf(head, head_size, "%s", top);
	if (sentence && sentence_size)
		snprintf(sentence, sentence_size, "%s", text);
	if (kills_sentence && kills_size)
		snprintf(kills_sentence, kills_size, "%s", kills);
}

/* ---- the file */

static unsigned long checksum(char const *text, size_t length)
{
	unsigned long hash = 2166136261u;
	size_t index;

	for (index = 0; index < length; index++)
		hash = ((hash ^ (unsigned char)text[index]) * 16777619u) & 0xffffffffu;
	return hash;
}

static int put(char *buffer, long size, long *at, char const *format, ...) __attribute__((format(printf, 4, 5)));

/* text appended at buffer + *at: 0 when it does not fit */
static int put(char *buffer, long size, long *at, char const *format, ...)
{
	va_list arguments;
	int n;

	if (*at >= size)
		return 0;
	va_start(arguments, format);
	n = vsnprintf(buffer + *at, (size_t)(size - *at), format, arguments);
	va_end(arguments);
	if (n < 0 || n >= size - *at)
		return 0;
	*at += n;
	return 1;
}

long ae_bot_records_write(struct ae_bot_records const *records, char *buffer, long size)
{
	long at = 0;
	int index, item;

	if (!put(buffer, size, &at, "# AE bot records\nversion = %d\nclock = %ld\n", AE_BOT_RECORD_VERSION, records->clock))
		return -1;
	for (index = 0; index < records->count; index++)
	{
		struct ae_bot_record const *r = &records->bot[index];

		if (!put(buffer, size, &at, "bot = %s | %s | %ld %ld %s | %ld %ld %ld %ld %ld %ld %ld | %d %d %d %d %d %d %d | ",
			r->key, r->name, r->first, r->last, r->first_playlist[0] ? r->first_playlist : "?", r->games, r->wins, r->ties,
			r->kills, r->deaths, r->assists, r->best_spree, r->shooting, r->movement, r->tier_low, r->tier_high, r->style,
			r->favourite, r->rank))
			return -1;
		for (item = 0; item < r->weapon_count; item++)
			if (!put(buffer, size, &at, "%s%s:%ld", item ? "," : "", r->weapons[item].name, r->weapons[item].kills))
				return -1;
		if (!put(buffer, size, &at, "\n"))
			return -1;
	}
	for (index = 0; index < records->versus_count; index++)
	{
		struct ae_bot_versus const *v = &records->versus[index];
		struct ae_bot_lastgame const *g = &v->lastgame;

		if (!put(buffer, size, &at, "versus = %s | %s | %ld %ld %ld %ld %ld %ld %ld %ld\n", v->profile, v->bot, v->against,
			v->you_won, v->it_won, v->with, v->with_won, v->your_kills, v->its_kills, v->last))
			return -1;
		if (g->has && !put(buffer, size, &at, "lastgame = %s | %s | %s | %s | %d %ld %ld %ld %ld %ld %d\n", v->profile, v->bot,
			g->playlist[0] ? g->playlist : "?", g->map[0] ? g->map : "?", g->result, g->kills, g->deaths, g->assists, g->spree,
			g->its_kills, g->place))
			return -1;
	}
	if (!put(buffer, size, &at, "checksum = %08lx\n", checksum(buffer, (size_t)at)))
		return -1;
	return at;
}

/* count whole numbers from text into out (signed when negatives are allowed): the text after them, NULL when it has fewer */
static char *numbers(char *text, long *out, int count, int negatives)
{
	char *end;
	int index;

	for (index = 0; index < count; index++)
	{
		out[index] = strtol(text, &end, 10);
		if (end == text || (!negatives && out[index] < 0))
			return NULL;
		text = end;
	}
	return text;
}

/* "a | b | rest": fields split at the bars and trimmed into out (each under size, none empty): the rest after the last bar
   (with a bar more when more is wanted), NULL when the value is not that */
static char *split(char *value, char **out, int fields, size_t size)
{
	int index;

	for (index = 0; index < fields; index++)
	{
		char *bar = strchr(value, '|'), *end;

		if (!bar)
			return NULL;
		*bar = 0;
		while (*value == ' ')
			value++;
		end = bar;
		while (end > value && end[-1] == ' ')
			*--end = 0;
		if (!value[0] || strlen(value) >= size)
			return NULL;
		out[index] = value;
		value = bar + 1;
	}
	return value;
}

static int clamp(long value, long low, long high)
{
	return (int)(value < low ? low : value > high ? high : value);
}

/* the bot line's "| f1 f2 f3 | ..." groups: the text of group n (the bars cut), NULL when there is none */
static char *group(char **rest)
{
	char *start = *rest, *bar;

	if (!start)
		return NULL;
	bar = strchr(start, '|');
	if (bar)
	{
		*bar = 0;
		*rest = bar + 1;
	}
	else
		*rest = NULL;
	return start;
}

static int bot_line(struct ae_bot_records *records, char *value)
{
	char *field[2], *rest, *g, *stop, *last_field;
	long v[7];
	struct ae_bot_record *record;

	rest = split(value, field, 2, AE_BOT_RECORD_KEY);
	if (!rest)
		return 1;
	if (find_key(records, field[0]))
		return -1;
	if (records->count >= AE_BOT_RECORD_BOTS)
		return 1;
	record = &records->bot[records->count];
	memset(record, 0, sizeof(*record));
	snprintf(record->key, sizeof(record->key), "%s", field[0]);
	snprintf(record->name, sizeof(record->name), "%s", field[1]);
	record->style = -1;
	record->rank = AE_BOT_RECORD_NO_RANK;
	/* (first last first_playlist) */
	g = group(&rest);
	if (!g || !(stop = numbers(g, v, 2, 0)))
		return 1;
	record->first = v[0];
	record->last = v[1];
	while (*stop == ' ')
		stop++;
	last_field = stop + strcspn(stop, " ");
	*last_field = 0;
	snprintf(record->first_playlist, sizeof(record->first_playlist), "%s", stop);
	/* (games wins ties kills deaths assists best_spree) */
	g = group(&rest);
	if (!g || !numbers(g, v, 7, 0))
		return 1;
	record->games = v[0];
	record->wins = v[1] < v[0] ? v[1] : v[0];
	record->ties = v[2] < v[0] - record->wins ? v[2] : v[0] - record->wins;
	record->kills = v[3];
	record->deaths = v[4];
	record->assists = v[5];
	record->best_spree = v[6];
	/* (shooting movement tier_low tier_high, then style favourite rank: a reader may find fewer) */
	g = group(&rest);
	if (g && numbers(g, v, 4, 0))
	{
		record->shooting = clamp(v[0], 0, 7);
		record->movement = clamp(v[1], 0, 7);
		record->tier_low = clamp(v[2], 0, 99);
		record->tier_high = clamp(v[3], record->tier_low, 99);
		if (numbers(g, v, 7, 1))
		{
			record->style = clamp(v[4], -1, 99);
			record->favourite = clamp(v[5], 0, 99);
			record->rank = clamp(v[6], AE_BOT_RECORD_NO_RANK, 50);
		}
	}
	/* (weapons: name:kills, ","-separated) */
	g = group(&rest);
	while (g && *g && record->weapon_count < AE_BOT_RECORD_WEAPONS)
	{
		char *comma = strchr(g, ','), *colon;

		if (comma)
			*comma = 0;
		colon = strrchr(g, ':');
		if (colon)
		{
			char *end;
			long kills;

			*colon = 0;
			while (*g == ' ')
				g++;
			kills = strtol(colon + 1, &end, 10);
			if (g[0] && strlen(g) < AE_BOT_RECORD_WEAPON && end != colon + 1 && kills > 0)
			{
				snprintf(record->weapons[record->weapon_count].name, sizeof(record->weapons[0].name), "%s", g);
				record->weapons[record->weapon_count++].kills = kills;
			}
		}
		g = comma ? comma + 1 : NULL;
	}
	records->count++;
	return 1;
}

static int versus_line(struct ae_bot_records *records, char *value)
{
	char *field[2], *rest;
	long v[8];
	struct ae_bot_versus *versus;

	rest = split(value, field, 2, AE_BOT_RECORD_KEY);
	if (!rest || !numbers(rest, v, 8, 0) || !find_key(records, field[1]))
		return 1;
	if (versus_key(records, field[0], field[1]))
		return -1;
	if (records->versus_count >= AE_BOT_RECORD_VERSUS)
		return 1;
	versus = &records->versus[records->versus_count++];
	memset(versus, 0, sizeof(*versus));
	snprintf(versus->profile, sizeof(versus->profile), "%s", field[0]);
	snprintf(versus->bot, sizeof(versus->bot), "%s", field[1]);
	versus->against = v[0];
	versus->you_won = v[1] < v[0] ? v[1] : v[0];
	versus->it_won = v[2] < v[0] - versus->you_won ? v[2] : v[0] - versus->you_won;
	versus->with = v[3];
	versus->with_won = v[4] < v[3] ? v[4] : v[3];
	versus->your_kills = v[5];
	versus->its_kills = v[6];
	versus->last = v[7];
	return 1;
}

static int lastgame_line(struct ae_bot_records *records, char *value)
{
	char *field[4], *rest;
	long v[7];
	struct ae_bot_versus *versus;
	struct ae_bot_lastgame *g;

	rest = split(value, field, 4, AE_BOT_RECORD_KEY);
	if (!rest || !numbers(rest, v, 7, 1) || !(versus = versus_key(records, field[0], field[1])))
		return 1;
	g = &versus->lastgame;
	if (g->has)
		return -1;
	g->has = 1;
	snprintf(g->playlist, sizeof(g->playlist), "%s", field[2]);
	snprintf(g->map, sizeof(g->map), "%s", field[3]);
	g->result = clamp(v[0], AE_BOT_RECORD_WIN, AE_BOT_RECORD_TIE);
	g->kills = v[1] > 0 ? v[1] : 0;
	g->deaths = v[2] > 0 ? v[2] : 0;
	g->assists = v[3] > 0 ? v[3] : 0;
	g->spree = v[4] > 0 ? v[4] : 0;
	g->its_kills = v[5] > 0 ? v[5] : 0;
	g->place = clamp(v[6], -1, 127);
	return 1;
}

int ae_bot_records_read(struct ae_bot_records *records, char const *text)
{
	static char line[AE_BOT_RECORD_LINE];
	char const *start = text;
	int header = 0, summed = 0, result;

	ae_bot_records_init(records);
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

			if (strcmp(key, "version") || !numbers(value, &version, 1, 0) || version < 1)
				goto bad;
			header = 1;
			records->newer = version > AE_BOT_RECORD_VERSION;
			continue;
		}
		/* (the checksum is the last line: anything after it, a second header or a second checksum is an edited file) */
		if (summed || !strcmp(key, "version"))
			goto edited;
		if (!strcmp(key, "checksum"))
		{
			char *stop;
			unsigned long given = strtoul(value, &stop, 16);

			if (stop == value || given != checksum(start, (size_t)(line_start - start)))
				goto edited;
			summed = 1;
			continue;
		}
		result = 1;
		if (!strcmp(key, "clock"))
		{
			long clock;

			if (numbers(value, &clock, 1, 0))
				records->clock = clock;
		}
		else if (!strcmp(key, "bot"))
			result = bot_line(records, value);
		else if (!strcmp(key, "versus"))
			result = versus_line(records, value);
		else if (!strcmp(key, "lastgame"))
			result = lastgame_line(records, value);
		/* (a kind this reader does not know: skipped) */
		if (result < 0)
			goto edited;
	}
	if (!header)
		goto bad;
	if (!summed && (records->count || records->versus_count || records->clock))
		goto edited;
	return AE_BOT_RECORD_READ_OK;
edited:
	ae_bot_records_init(records);
	records->edited = 1;
	return AE_BOT_RECORD_READ_EDITED;
bad:
	ae_bot_records_init(records);
	return AE_BOT_RECORD_READ_BAD;
}

/* ---------- CAREER's BOTS list */

/* whether bot a goes before bot b in the sort (their versus lines with the profile) */
static int list_before(struct ae_bot_record const *a, struct ae_bot_versus const *va, struct ae_bot_record const *b,
	struct ae_bot_versus const *vb, int sort)
{
	long games_a = va->against + va->with, games_b = vb->against + vb->with;
	long lead_a = va->it_won - va->you_won, lead_b = vb->it_won - vb->you_won;

	if (sort == AE_BOT_RECORD_SORT_RECORD && lead_a != lead_b)
		return lead_a > lead_b;
	if (sort == AE_BOT_RECORD_SORT_LAST && va->last != vb->last)
		return va->last > vb->last;
	if (games_a != games_b)
		return games_a > games_b;
	if (va->last != vb->last)
		return va->last > vb->last;
	return strcmp(a->key, b->key) < 0;
}

int ae_bot_records_list(struct ae_bot_records const *records, char const *profile, int sort, int *out, int most)
{
	static struct ae_bot_versus const *line[AE_BOT_RECORD_BOTS];
	char key[AE_BOT_RECORD_KEY];
	int index, count = 0;

	ae_bot_record_key(profile, key, sizeof(key));
	if (!records || !key[0] || !out || most <= 0)
		return 0;
	if (sort < 0 || sort >= AE_BOT_RECORD_SORTS)
		sort = AE_BOT_RECORD_SORT_GAMES;
	for (index = 0; index < records->count && count < most && count < AE_BOT_RECORD_BOTS; index++)
	{
		struct ae_bot_versus const *versus = ae_bot_record_versus((struct ae_bot_records *)records, key,
			records->bot[index].key);
		int at;

		if (!versus || versus->against + versus->with <= 0)
			continue;
		/* (an insertion: the list is at most AE_BOT_RECORD_BOTS long) */
		for (at = count; at > 0 && list_before(&records->bot[index], versus, &records->bot[out[at - 1]], line[at - 1], sort);
			at--)
		{
			out[at] = out[at - 1];
			line[at] = line[at - 1];
		}
		out[at] = index;
		line[at] = versus;
		count++;
	}
	return count;
}

char const *ae_bot_record_sort_name(int sort)
{
	return sort == AE_BOT_RECORD_SORT_LAST ? "LAST MET" : sort == AE_BOT_RECORD_SORT_RECORD ? "RECORD" : "GAMES";
}
