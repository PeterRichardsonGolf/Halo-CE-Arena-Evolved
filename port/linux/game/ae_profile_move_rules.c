/* Moving a profile's progress between PCs: the rules (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_bot_record_rules.h"
#include "ae_career_record.h"
#include "ae_profile_move_rules.h"
#include "ae_service_record.h"
#include "mcc_sha256.h"

enum { FIELD = 160, KIND = 16 };

/* ---------- text: lines, "kind = value" and the " | " fields ---------- */

struct sink
{
	char *out;
	size_t size, length;
	int over;
};

static void put(struct sink *sink, char const *text, size_t length)
{
	if (sink->over || sink->length + length + 1 > sink->size)
	{
		sink->over = 1;
		return;
	}
	memcpy(sink->out + sink->length, text, length);
	sink->length += length;
	sink->out[sink->length] = 0;
}

static void put_text(struct sink *sink, char const *text)
{
	put(sink, text, strlen(text));
}

static void put_line(struct sink *sink, char const *line, size_t length)
{
	put(sink, line, length);
	put(sink, "\n", 1);
}

static int next_line(char const *text, size_t length, size_t *at, char const **line, size_t *count)
{
	size_t start;

	if (!text || *at >= length)
		return 0;
	start = *at;
	while (*at < length && text[*at] != '\n')
		(*at)++;
	*line = text + start;
	*count = *at - start;
	if (*at < length)
		(*at)++;
	if (*count && (*line)[*count - 1] == '\r')
		(*count)--;
	return 1;
}

/* a "kind = value" line: its kind and its value (a comment, a blank line or a line without " = " is none) */
struct rec
{
	char kind[KIND];
	char const *value;
	size_t length;
	char const *whole;
	size_t whole_length;
};

static int split(char const *line, size_t count, struct rec *rec)
{
	size_t at;

	if (!count || line[0] == '#' || line[0] == '[')
		return 0;
	for (at = 0; at + 2 < count; at++)
	{
		if (line[at] == ' ' && line[at + 1] == '=' && line[at + 2] == ' ')
			break;
	}
	if (at + 3 > count || at == 0 || at >= KIND)
		return 0;
	memcpy(rec->kind, line, at);
	rec->kind[at] = 0;
	rec->value = line + at + 3;
	rec->length = count - at - 3;
	rec->whole = line;
	rec->whole_length = count;
	return 1;
}

static int is_kind(struct rec const *rec, char const *kind)
{
	return !strcmp(rec->kind, kind);
}

/* the index-th field of a value as it is written (no trimming): where it starts and its length; 0 when there is none */
static int raw_field(struct rec const *rec, int index, char const **start, size_t *length)
{
	size_t pos = 0, end;
	int number = 0;

	for (;;)
	{
		end = pos;
		while (end < rec->length && !(end + 2 < rec->length && rec->value[end] == ' ' && rec->value[end + 1] == '|' &&
			rec->value[end + 2] == ' '))
			end++;
		if (number == index)
			break;
		if (end >= rec->length)
			return 0;
		pos = end + 3;
		number++;
	}
	*start = rec->value + pos;
	*length = end - pos;
	return 1;
}

/* the index-th field of a value, trimmed (0 when there is none) */
static int field(struct rec const *rec, int index, char *out, size_t size)
{
	char const *start;
	size_t length;

	if (!raw_field(rec, index, &start, &length))
	{
		out[0] = 0;
		return 0;
	}
	while (length && *start == ' ')
		start++, length--;
	while (length && start[length - 1] == ' ')
		length--;
	if (length >= size)
		length = size - 1;
	memcpy(out, start, length);
	out[length] = 0;
	return 1;
}

/* whether a field has no space round it as written */
static int plain(struct rec const *rec, int index)
{
	char const *start;
	size_t length;

	return !raw_field(rec, index, &start, &length) || !length || (start[0] != ' ' && start[length - 1] != ' ');
}

/* the index-th number of a space separated list (-1 when there is none, it is no number, or it does not fit 31 bits: a
long is 32 bits where the game's longs are) */
static long number_at(char const *text, int index)
{
	int at = 0;

	while (*text)
	{
		long long value = 0;
		int digits = 0, negative = 0;

		while (*text == ' ')
			text++;
		if (*text == '-')
			negative = 1, text++;
		while (*text >= '0' && *text <= '9')
		{
			if (++digits > 10)
				return -1;
			value = value * 10 + (*text++ - '0');
		}
		if (!digits || value > 2147483647LL)
			return -1;
		if (at == index)
			return (long)(negative ? -value : value);
		at++;
		while (*text && *text != ' ')
			text++;
	}
	return -1;
}

/* a field's index-th number */
static long field_number(struct rec const *rec, int field_index, int number_index)
{
	char text[FIELD];

	if (!field(rec, field_index, text, sizeof(text)))
		return -1;
	return number_at(text, number_index);
}

/* a text's "clock = n" line */
static long text_clock(char const *text, size_t length)
{
	size_t at = 0;
	char const *line;
	size_t count;
	struct rec rec;

	while (next_line(text, length, &at, &line, &count))
		if (split(line, count, &rec) && is_kind(&rec, "clock"))
		{
			char value[32];

			if (rec.length < sizeof(value))
			{
				memcpy(value, rec.value, rec.length);
				value[rec.length] = 0;
				return number_at(value, 0);
			}
		}
	return 0;
}

unsigned long ae_pm_fnv(char const *text, size_t length)
{
	unsigned long hash = 2166136261UL;
	size_t at;

	for (at = 0; at < length; at++)
		hash = ((hash ^ (unsigned char)text[at]) * 16777619UL) & 0xffffffffUL;
	return hash;
}

char const *ae_pm_error_text(int error)
{
	switch (error)
	{
	case AE_PM_OK: return "";
	case AE_PM_ERR_EMPTY: return "Nothing of this profile to export yet.";
	case AE_PM_ERR_SIZE: return "The file is too large to be a profile.";
	case AE_PM_ERR_FORMAT: return "Not a profile file, or it is damaged.";
	case AE_PM_ERR_NEWER: return "Made by a newer AE: update this one first.";
	case AE_PM_ERR_CHECK: return "The file was damaged or edited.";
	case AE_PM_ERR_VALUE: return "The file holds numbers that cannot be real.";
	case AE_PM_ERR_ROOM: return "The result does not fit.";
	case AE_PM_ERR_KEY: return "That profile name cannot be used.";
	case AE_PM_ERR_READ: return "The file could not be read.";
	case AE_PM_ERR_BACKUP: return "The backup is damaged or is not one.";
	}
	return "";
}

static int clean_text(char const *text, size_t length)
{
	size_t at;

	for (at = 0; at < length; at++)
		if ((unsigned char)text[at] < 0x20 || (unsigned char)text[at] == 0x7f)
			return 0;
	return 1;
}

/* ---------- the file ---------- */

static char const *const section_name[3] = { "[career]", "[service]", "[bots]" };

/* a line's keyed field 0 equals key */
static int keyed(struct rec const *rec, char const *key)
{
	char const *start;
	size_t length;

	return raw_field(rec, 0, &start, &length) && length == strlen(key) && !memcmp(start, key, length);
}

static int key_usable(char const *key)
{
	size_t at, length = key ? strlen(key) : 0;

	if (!length || length >= AE_PM_KEY)
		return 0;
	for (at = 0; at < length; at++)
		if ((unsigned char)key[at] < 0x20 || key[at] == '|' || key[at] == 0x7f)
			return 0;
	return 1;
}

/* the units a section has seen, by a 64 bit hash of the kind and the first fields (at most AE_PM_LINES_MAX of them) */
enum { SEEN = 32768 };
static unsigned long long seen[SEEN];

static void seen_clear(void)
{
	memset(seen, 0, sizeof(seen));
}

/* whether the line's unit (its kind and its first fields) was seen before; the unit is noted */
static int pm_dup(struct rec const *rec, int fields)
{
	unsigned long long hash = 1469598103934665603ULL;
	size_t at;
	int index;

	for (at = 0; rec->kind[at]; at++)
		hash = (hash ^ (unsigned char)rec->kind[at]) * 1099511628211ULL;
	for (index = 0; index < fields; index++)
	{
		char text[FIELD];

		field(rec, index, text, sizeof(text));
		hash = (hash ^ 0x1f) * 1099511628211ULL;
		for (at = 0; text[at]; at++)
			hash = (hash ^ (unsigned char)text[at]) * 1099511628211ULL;
	}
	if (!hash)
		hash = 1;
	for (at = (size_t)(hash & (SEEN - 1)); seen[at]; at = (at + 1) & (SEEN - 1))
		if (seen[at] == hash)
			return 1;
	seen[at] = hash;
	return 0;
}

static int in_games(long games)
{
	return games >= 0 && games <= AE_PM_GAMES_MAX;
}

static int in_clock(long clock)
{
	return clock >= 0 && clock <= AE_PM_CLOCK_MAX;
}

/* set when a section has more of a kind of line than the record readers keep (the file is then too large, not wrong) */
static int cap_hit;

/* a line counted against a cap: 0 when the cap is passed (and cap_hit is set) */
static int under(int *counter, int cap)
{
	if (++*counter > cap)
	{
		cap_hit = 1;
		return 0;
	}
	return 1;
}

/* a section's lines checked: the key (exactly, no space round it), the numbers, the clocks, the units once, and each kind of line
no more often than the record readers keep it (so that a merge of a file within the caps is bounded) */
static int check_section(int which, char const *text, size_t length, char const *key, unsigned long now)
{
	size_t at = 0;
	char const *line;
	size_t count;
	struct rec rec;
	int ladders = 0, matches = 0, socials = 0, rivals = 0, versus = 0, maps = 0, bots = 0, botx = 0, kinds = 0, medals = 0,
		botmaps = 0, lastgames = 0;

	seen_clear();
	cap_hit = 0;
	while (next_line(text, length, &at, &line, &count))
	{
		long games, wins;

		if (!clean_text(line, count))
			return 0;
		if (!split(line, count, &rec))
			continue;
		if (which == 0)
		{
			if (is_kind(&rec, "career") || is_kind(&rec, "online"))
			{
				long level = field_number(&rec, 1, 1), quits;

				if (!under(&ladders, 2 * AE_CAREER_PLAYLISTS))
					return 0;

				games = field_number(&rec, 1, 3);
				wins = field_number(&rec, 1, 4);
				quits = field_number(&rec, 1, 5);
				if (!plain(&rec, 0) || !in_games(games) || wins < 0 || wins > games || quits < 0 || quits > games || level < 0 ||
					level > 49 || !in_clock(field_number(&rec, 1, 6)) || pm_dup(&rec, 1))
					return 0;
			}
			else if (is_kind(&rec, "match") || is_kind(&rec, "social") || is_kind(&rec, "experience"))
			{
				long last;

				if (!keyed(&rec, key) || !plain(&rec, 1))
					return 0;
				if (is_kind(&rec, "match") && !under(&matches, AE_CAREER_SOCIAL))
					return 0;
				if (is_kind(&rec, "social") && !under(&socials, AE_CAREER_SOCIAL))
					return 0;
				if (is_kind(&rec, "experience"))
				{
					if (pm_dup(&rec, 1))
						return 0;
					continue;
				}
				games = field_number(&rec, 2, 1);
				last = field_number(&rec, 2, is_kind(&rec, "match") ? 2 : 5);
				if (!in_games(games) || last < 0 || (now && (unsigned long)last > now + AE_PM_CLOCK_SLACK) || pm_dup(&rec, 2))
					return 0;
				if (is_kind(&rec, "social"))
				{
					long ties = field_number(&rec, 2, 3), lost = field_number(&rec, 2, 4);

					wins = field_number(&rec, 2, 2);
					if (wins < 0 || ties < 0 || lost < 0 || wins + ties + lost > games)
						return 0;
				}
			}
		}
		else if (which == 1)
		{
			if (!strcmp(rec.kind, "clock"))
			{
				char value[32];

				if (rec.length >= sizeof(value))
					return 0;
				memcpy(value, rec.value, rec.length);
				value[rec.length] = 0;
				if (!in_clock(number_at(value, 0)))
					return 0;
				continue;
			}
			if (!keyed(&rec, key) || !plain(&rec, 1))
				return 0;
			if (is_kind(&rec, "player"))
			{
				games = field_number(&rec, 3, 0);
				wins = field_number(&rec, 3, 1);
				if (!in_games(games) || wins < 0 || wins > games || !in_clock(field_number(&rec, 2, 0)) || pm_dup(&rec, 1))
					return 0;
			}
			else if (is_kind(&rec, "rival") || is_kind(&rec, "versus"))
			{
				char opponent[FIELD];

				if (!under(is_kind(&rec, "rival") ? &rivals : &versus, AE_SR_RIVALS_PER_OWNER))
					return 0;
				if (!field(&rec, 1, opponent, sizeof(opponent)) || !*opponent || pm_dup(&rec, 2))
					return 0;
				if (is_kind(&rec, "versus") && !in_games(field_number(&rec, 2, 0)))
					return 0;
			}
			else if (is_kind(&rec, "map"))
			{
				if (!under(&maps, AE_SR_MAPS))
					return 0;
				games = field_number(&rec, 2, 0);
				wins = field_number(&rec, 2, 1);
				if (!in_games(games) || wins < 0 || wins > games)
					return 0;
			}
		}
		else
		{
			if (!strcmp(rec.kind, "clock"))
			{
				char value[32];

				if (rec.length >= sizeof(value))
					return 0;
				memcpy(value, rec.value, rec.length);
				value[rec.length] = 0;
				if (!in_clock(number_at(value, 0)))
					return 0;
				continue;
			}
			if (is_kind(&rec, "bot"))
			{
				long ties;

				if (!under(&bots, AE_BOT_RECORD_BOTS))
					return 0;

				games = field_number(&rec, 3, 0);
				wins = field_number(&rec, 3, 1);
				ties = field_number(&rec, 3, 2);
				if (!plain(&rec, 0) || !in_games(games) || wins < 0 || ties < 0 || wins + ties > games ||
					!in_clock(field_number(&rec, 2, 0)) || !in_clock(field_number(&rec, 2, 1)) || pm_dup(&rec, 1))
					return 0;
			}
			else if (is_kind(&rec, "versus") || is_kind(&rec, "lastgame"))
			{
				char bot[FIELD];

				if (!under(is_kind(&rec, "versus") ? &versus : &lastgames, AE_BOT_RECORD_VERSUS))
					return 0;
				if (!keyed(&rec, key) || !plain(&rec, 1) || !field(&rec, 1, bot, sizeof(bot)) || !*bot || pm_dup(&rec, 2))
					return 0;
				if (is_kind(&rec, "versus") && (!in_games(field_number(&rec, 2, 0)) || !in_games(field_number(&rec, 2, 3)) ||
					!in_clock(field_number(&rec, 2, 7))))
					return 0;
			}
			else if (is_kind(&rec, "botx") || is_kind(&rec, "botkinds") || is_kind(&rec, "botmedals") || is_kind(&rec, "botmap"))
			{
				if (!under(is_kind(&rec, "botx") ? &botx : is_kind(&rec, "botkinds") ? &kinds :
					is_kind(&rec, "botmedals") ? &medals : &botmaps, is_kind(&rec, "botmedals") ? AE_BOT_RECORD_BOTS * 16 :
					is_kind(&rec, "botmap") ? AE_BOT_RECORD_BOTS * AE_BOT_RECORD_MAPS : AE_BOT_RECORD_BOTS))
					return 0;
				if (!plain(&rec, 0))
					return 0;
			}
		}
	}
	return 1;
}

/* a value copied into out (up to a newline) */
static int header_value(char const *line, size_t count, char const *name, char *out, size_t size)
{
	size_t length = strlen(name);

	if (count < length + 3 || memcmp(line, name, length) || memcmp(line + length, " = ", 3))
		return 0;
	if (count - length - 3 >= size)
		return -1;
	memcpy(out, line + length + 3, count - length - 3);
	out[count - length - 3] = 0;
	return 1;
}

int ae_pm_parse(char const *text, size_t length, unsigned long now, struct ae_pm_file *file)
{
	size_t at = 0, check_from = 0, start[3] = { 0, 0, 0 }, end[3] = { 0, 0, 0 };
	char const *line;
	size_t count;
	int lines = 0, section = -1, seen_end = 0, header = 0;
	char value[AE_PM_LINE_MAX];
	int have_version = 0, have_format = 0, have_key = 0;
	struct mcc_sha256 context;
	unsigned char digest[32];
	char hex[65];
	int index;

	memset(file, 0, sizeof(*file));
	if (!text || length > AE_PM_FILE_MAX)
		return AE_PM_ERR_SIZE;
	while (next_line(text, length, &at, &line, &count))
	{
		size_t offset = (size_t)(line - text);

		if (++lines > AE_PM_LINES_MAX || count > AE_PM_LINE_MAX)
			return AE_PM_ERR_SIZE;
		if (!clean_text(line, count))
			return AE_PM_ERR_FORMAT;
		if (!header)
		{
			if (count != 12 || memcmp(line, "# AE profile", 12))
				return AE_PM_ERR_FORMAT;
			header = 1;
			continue;
		}
		if (seen_end)
		{
			if (seen_end != 1 || header_value(line, count, "check", value, sizeof(value)) != 1)
				return AE_PM_ERR_FORMAT;
			seen_end = 2;
			mcc_sha256_begin(&context);
			mcc_sha256_add(&context, text, (int)check_from);
			mcc_sha256_end(&context, digest);
			for (index = 0; index < 32; index++)
			{
				hex[index * 2] = "0123456789abcdef"[digest[index] >> 4];
				hex[index * 2 + 1] = "0123456789abcdef"[digest[index] & 15];
			}
			hex[64] = 0;
			if (strcmp(hex, value))
				return AE_PM_ERR_CHECK;
			continue;
		}
		if (line[0] == '[')
		{
			if (count == 5 && !memcmp(line, "[end]", 5))
			{
				if (section != 2)
					return AE_PM_ERR_FORMAT;
				end[2] = offset;
				seen_end = 1;
				check_from = at;
				continue;
			}
			for (index = 0; index < 3; index++)
				if (strlen(section_name[index]) == count && !memcmp(line, section_name[index], count))
					break;
			if (index >= 3 || index != section + 1)
				return AE_PM_ERR_FORMAT;
			if (section >= 0)
				end[section] = offset;
			section = index;
			start[section] = at;
			continue;
		}
		if (section >= 0 || line[0] == '#')
			continue;
		/* (the header's own lines) */
		if (header_value(line, count, "version", value, sizeof(value)) == 1)
		{
			long version = number_at(value, 0);

			if (version < 1)
				return AE_PM_ERR_FORMAT;
			if (version > 1)
				return AE_PM_ERR_NEWER;
			have_version = 1;
		}
		else if (header_value(line, count, "format", value, sizeof(value)) == 1)
		{
			if (strcmp(value, "aeprofile"))
				return AE_PM_ERR_FORMAT;
			have_format = 1;
		}
		else if (header_value(line, count, "profile", file->profile, sizeof(file->profile)) == 1)
			;
		else if (header_value(line, count, "key", file->key, sizeof(file->key)) == 1)
			have_key = key_usable(file->key);
		else if (header_value(line, count, "exported", value, sizeof(value)) == 1)
			file->exported = number_at(value, 0) < 0 ? 0 : (unsigned long)number_at(value, 0);
		else if (header_value(line, count, "build", file->build, sizeof(file->build)) == 1)
			;
		else
			return AE_PM_ERR_FORMAT;
	}
	if (seen_end != 2 || !have_version || !have_format || !have_key || !file->profile[0])
		return AE_PM_ERR_FORMAT;
	for (index = 0; index < 3; index++)
	{
		file->section[index] = text + start[index];
		file->length[index] = end[index] - start[index];
		if (!check_section(index, file->section[index], file->length[index], file->key, now))
			return cap_hit ? AE_PM_ERR_SIZE : AE_PM_ERR_VALUE;
	}
	file->lines = lines;
	return AE_PM_OK;
}

/* ---------- export ---------- */

int ae_pm_export(char const *name, char const *key, char const *career, char const *service, char const *bots,
	unsigned long now, char const *build, char *out, size_t size, int counts[3])
{
	struct sink sink = { out, size, 0, 0 };
	char const *texts[3];
	int which, keyed_lines = 0, lines = 0, total[3] = { 0, 0, 0 };
	char number[48], digest_text[65];
	struct mcc_sha256 context;
	unsigned char digest[32];
	int index;

	if (!name || !*name || strlen(name) >= AE_PM_NAME || !key_usable(key) || !clean_text(name, strlen(name)))
		return AE_PM_ERR_KEY;
	texts[0] = career;
	texts[1] = service;
	texts[2] = bots;
	out[0] = 0;
	put_text(&sink, "# AE profile\nversion = 1\nformat = aeprofile\nprofile = ");
	put_text(&sink, name);
	put_text(&sink, "\nkey = ");
	put_text(&sink, key);
	snprintf(number, sizeof(number), "\nexported = %lu\nbuild = ", now);
	put_text(&sink, number);
	put_text(&sink, build ? build : "");
	put_text(&sink, "\n");
	for (which = 0; which < 3; which++)
	{
		size_t at = 0, length = texts[which] ? strlen(texts[which]) : 0;
		char const *line;
		size_t count;
		struct rec rec;

		put_text(&sink, section_name[which]);
		put_text(&sink, "\n");
		while (next_line(texts[which], length, &at, &line, &count))
		{
			int take = 0, mine;

			if (!split(line, count, &rec))
				continue;
			mine = keyed(&rec, key);
			if (which == 0)
			{
				take = is_kind(&rec, "career") || is_kind(&rec, "online") || (mine && (is_kind(&rec, "match") ||
					is_kind(&rec, "social") || is_kind(&rec, "experience")));
				if (take && mine)
					keyed_lines++;
			}
			else if (which == 1)
			{
				take = is_kind(&rec, "clock") || (mine && !is_kind(&rec, "clock"));
				if (take && mine)
					keyed_lines++;
			}
			else
			{
				take = is_kind(&rec, "clock") || is_kind(&rec, "bot") || is_kind(&rec, "botx") || is_kind(&rec, "botkinds") ||
					is_kind(&rec, "botmedals") || is_kind(&rec, "botmap") || (mine && (is_kind(&rec, "versus") ||
					is_kind(&rec, "lastgame")));
				if (take && mine)
					keyed_lines++;
			}
			if (take)
			{
				put_line(&sink, line, count);
				total[which]++;
				lines++;
			}
		}
	}
	put_text(&sink, "[end]\n");
	if (sink.over)
		return AE_PM_ERR_ROOM;
	if (!keyed_lines)
		return AE_PM_ERR_EMPTY;
	if (sink.length > AE_PM_FILE_MAX || lines + 12 > AE_PM_LINES_MAX)
		return AE_PM_ERR_SIZE;
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, out, (int)sink.length);
	mcc_sha256_end(&context, digest);
	for (index = 0; index < 32; index++)
	{
		digest_text[index * 2] = "0123456789abcdef"[digest[index] >> 4];
		digest_text[index * 2 + 1] = "0123456789abcdef"[digest[index] & 15];
	}
	digest_text[64] = 0;
	put_text(&sink, "check = ");
	put_text(&sink, digest_text);
	put_text(&sink, "\n");
	if (sink.over)
		return AE_PM_ERR_ROOM;
	if (counts)
		memcpy(counts, total, sizeof(total));
	return AE_PM_OK;
}

/* ---------- the merge ---------- */

/* whether the file's unit is kept over this PC's: by the games, then the clock; this PC's wins a tie; REPLACE takes the file's */
static int file_wins(int mode, int has_mine, long mine_games, long mine_clock, long their_games, long their_clock)
{
	if (!has_mine || mode == AE_PM_REPLACE)
		return 1;
	if (their_games != mine_games)
		return their_games > mine_games;
	return their_clock > mine_clock;
}

static void row_add(struct ae_pm_summary *summary, char const *name, long mine, long theirs, int from_file)
{
	if (!summary)
		return;
	if (summary->rows >= AE_PM_ROWS)
	{
		summary->rows_dropped++;
		return;
	}
	snprintf(summary->row[summary->rows].name, sizeof(summary->row[0].name), "%s", name);
	summary->row[summary->rows].mine = mine;
	summary->row[summary->rows].theirs = theirs;
	summary->row[summary->rows].from_file = from_file;
	summary->rows++;
}

static void tally(struct ae_pm_summary *summary, int from_file)
{
	if (!summary)
		return;
	if (from_file)
		summary->from_file++;
	else
		summary->kept++;
}

/* a keyed line of the file into the sink with its key changed to to_key: the rest of the line from where its first field ends as
written (the first " | ", else the line's end), never from the key's length */
static void put_keyed(struct sink *sink, struct rec const *rec, char const *to_key)
{
	char const *start;
	size_t length;

	if (!raw_field(rec, 0, &start, &length))
		length = 0;
	put_text(sink, rec->kind);
	put_text(sink, " = ");
	put_text(sink, to_key);
	put(sink, rec->value + (size_t)(start - rec->value) + length, rec->length - (size_t)(start - rec->value) - length);
	put(sink, "\n", 1);
}

/* a text read by length (the file's sections are not ended) */
struct view
{
	char const *text;
	size_t length;
};

/* whether a line is of kind and its first field (and second, when given) are exactly those: no splitting, no copies */
static int line_is(char const *line, size_t count, char const *kind, char const *first, char const *second)
{
	size_t kind_length = strlen(kind), first_length = strlen(first), at;

	if (count < kind_length + 3 + first_length || memcmp(line, kind, kind_length) || memcmp(line + kind_length, " = ", 3))
		return 0;
	at = kind_length + 3;
	if (memcmp(line + at, first, first_length))
		return 0;
	at += first_length;
	if (at < count && !(at + 2 < count && !memcmp(line + at, " | ", 3)))
		return 0;
	if (second)
	{
		size_t second_length = strlen(second);

		if (at >= count)
			return 0;
		at += 3;
		if (at + second_length > count || memcmp(line + at, second, second_length))
			return 0;
		at += second_length;
		if (at < count && !(at + 2 < count && !memcmp(line + at, " | ", 3)))
			return 0;
	}
	return 1;
}

/* the same search over a length-bounded text (the file's section): the first line of kind whose fields 0 (and 1) are these */
static int find_in(struct view view, char const *kind, char const *first, char const *second, struct rec *found)
{
	size_t at = 0;
	char const *line;
	size_t count_;

	while (next_line(view.text, view.length, &at, &line, &count_))
	{
		if (line_is(line, count_, kind, first, second))
			return split(line, count_, found);
	}
	return 0;
}

static struct view target_view(char const *target)
{
	struct view view;

	view.text = target ? target : "";
	view.length = strlen(view.text);
	return view;
}

static struct view file_view(struct ae_pm_file const *file, int which)
{
	struct view view;

	view.text = file->section[which] ? file->section[which] : "";
	view.length = file->section[which] ? file->length[which] : 0;
	return view;
}

static int finish(struct sink *sink, int checksum, size_t *length)
{
	if (checksum && !sink->over)
	{
		char line[32];

		snprintf(line, sizeof(line), "checksum = %08lx\n", ae_pm_fnv(sink->out, sink->length));
		put_text(sink, line);
	}
	if (sink->over)
		return AE_PM_ERR_ROOM;
	*length = sink->length;
	return AE_PM_OK;
}

/* ladders (career, online): by playlist id; the keyed kinds by the profile and playlist */
static void career_stats(struct rec const *rec, long *games, long *clock)
{
	if (is_kind(rec, "career") || is_kind(rec, "online"))
	{
		*games = field_number(rec, 1, 3);
		*clock = field_number(rec, 1, 6);
	}
	else if (is_kind(rec, "match"))
	{
		*games = field_number(rec, 2, 1);
		*clock = field_number(rec, 2, 2);
	}
	else if (is_kind(rec, "social"))
	{
		*games = field_number(rec, 2, 1);
		*clock = field_number(rec, 2, 5);
	}
	else
		*games = *clock = 0;
}

static int career_unit(struct rec const *rec, char const *key)
{
	return is_kind(rec, "career") || is_kind(rec, "online") || (keyed(rec, key) && (is_kind(rec, "match") ||
		is_kind(rec, "social") || is_kind(rec, "experience")));
}

static void career_row(struct ae_pm_summary *summary, struct rec const *rec, long mine, long theirs, int from_file)
{
	char id[FIELD], name[48];

	if (is_kind(rec, "match") || is_kind(rec, "experience"))
		return;
	if (is_kind(rec, "social"))
		field(rec, 1, id, sizeof(id));
	else
		field(rec, 0, id, sizeof(id));
	snprintf(name, sizeof(name), "%s %.30s", is_kind(rec, "social") ? "SKILL" : is_kind(rec, "online") ? "ONLINE" : "CAREER", id);
	row_add(summary, name, mine, theirs, from_file);
}

int ae_pm_merge_career(int mode, struct ae_pm_file const *file, char const *to_key, char const *target, char *out, size_t size,
	size_t *length, struct ae_pm_summary *summary)
{
	static char const *const order[] = { "career", "online", "match", "social", "pending", "experience" };
	struct sink sink = { out, size, 0, 0 };
	struct view mine = target_view(target), theirs = file_view(file, 0);
	int kind_index;

	if (!key_usable(to_key))
		return AE_PM_ERR_KEY;
	out[0] = 0;
	{
		char head[64];

		snprintf(head, sizeof(head), "# AE career\nversion = %d\n", AE_CAREER_VERSION);
		put_text(&sink, head);
	}
	for (kind_index = 0; kind_index < 6; kind_index++)
	{
		char const *kind = order[kind_index];
		size_t at = 0;
		char const *line;
		size_t count_;
		struct rec rec, other;

		while (next_line(mine.text, mine.length, &at, &line, &count_))
		{
			char id[FIELD], playlist[FIELD];
			int found;

			if (!split(line, count_, &rec) || !is_kind(&rec, kind))
				continue;
			if (!career_unit(&rec, to_key))
			{
				put_line(&sink, line, count_);
				continue;
			}
			/* (REPLACE: the profile's units are the file's alone, written in the file's order below; the ladders are this
			PC's and keep what the file does not have) */
			if (mode == AE_PM_REPLACE && !is_kind(&rec, "career") && !is_kind(&rec, "online"))
				continue;
			field(&rec, 0, id, sizeof(id));
			field(&rec, 1, playlist, sizeof(playlist));
			if (is_kind(&rec, "career") || is_kind(&rec, "online"))
				found = find_in(theirs, kind, id, NULL, &other);
			else if (is_kind(&rec, "experience"))
				found = find_in(theirs, kind, file->key, NULL, &other);
			else
				found = find_in(theirs, kind, file->key, playlist, &other);
			if (!found)
			{
				put_line(&sink, line, count_);
				tally(summary, 0);
				continue;
			}
			{
				long mine_games, mine_clock, their_games, their_clock;
				int take;

				career_stats(&rec, &mine_games, &mine_clock);
				career_stats(&other, &their_games, &their_clock);
				take = is_kind(&rec, "experience") ? (mode == AE_PM_REPLACE) :
					file_wins(mode, 1, mine_games, mine_clock, their_games, their_clock);
				if (take)
				{
					if (is_kind(&other, "career") || is_kind(&other, "online"))
						put_line(&sink, other.whole, other.whole_length);
					else
						put_keyed(&sink, &other, to_key);
				}
				else
					put_line(&sink, line, count_);
				tally(summary, take);
				career_row(summary, &rec, mine_games, their_games, take);
			}
		}
		/* (the file's units this PC has no line for) */
		at = 0;
		while (strcmp(kind, "pending") && next_line(theirs.text, theirs.length, &at, &line, &count_))
		{
			char id[FIELD], playlist[FIELD];
			int has;

			if (!split(line, count_, &rec) || !is_kind(&rec, kind) || !career_unit(&rec, file->key))
				continue;
			field(&rec, 0, id, sizeof(id));
			field(&rec, 1, playlist, sizeof(playlist));
			if (is_kind(&rec, "career") || is_kind(&rec, "online"))
				has = find_in(mine, kind, id, NULL, &other);
			else if (is_kind(&rec, "experience"))
				has = find_in(mine, kind, to_key, NULL, &other);
			else
				has = find_in(mine, kind, to_key, playlist, &other);
			if (has && (is_kind(&rec, "career") || is_kind(&rec, "online") || mode != AE_PM_REPLACE))
				continue;
			if (is_kind(&rec, "career") || is_kind(&rec, "online"))
				put_line(&sink, line, count_);
			else
				put_keyed(&sink, &rec, to_key);
			{
				long games, clock, mine_games = -1, mine_clock;

				career_stats(&rec, &games, &clock);
				if (has)
					career_stats(&other, &mine_games, &mine_clock);
				tally(summary, 1);
				career_row(summary, &rec, mine_games, games, 1);
			}
		}
	}
	/* (the career XP and challenge lines of this PC stay as they are: a profile import does not carry them) */
	{
		size_t at = 0;
		char const *line;
		size_t count_;
		struct rec rec;

		while (next_line(mine.text, mine.length, &at, &line, &count_))
			if (split(line, count_, &rec) && (is_kind(&rec, "xp") || is_kind(&rec, "challenges")))
				put_line(&sink, line, count_);
	}
	return finish(&sink, 1, length);
}

/* the service record: the profile's player lines as a group, its rivals with their versus lines */
static int group_kind(struct rec const *rec)
{
	return is_kind(rec, "player") || is_kind(rec, "playerx") || is_kind(rec, "map") || is_kind(rec, "lastgame");
}

struct rival
{
	char opponent[FIELD];
	int from_file;
};

int ae_pm_merge_service(int mode, struct ae_pm_file const *file, char const *to_key, char const *target, char *out, size_t size,
	size_t *length, struct ae_pm_summary *summary)
{
	static char const *const order[] = { "player", "playerx", "map", "lastgame", "rival", "versus" };
	struct sink sink = { out, size, 0, 0 };
	struct view mine = target_view(target), theirs = file_view(file, 1);
	struct rival rivals[AE_PM_RIVALS];
	int rival_count = 0, group_from_file = 0, kind_index, has_mine, has_theirs;
	struct rec mine_player, their_player;
	long clock = text_clock(mine.text, mine.length), their_clock = text_clock(theirs.text, theirs.length);
	char number[48];
	size_t at;
	char const *line;
	size_t count_;
	struct rec rec, other;

	if (!key_usable(to_key))
		return AE_PM_ERR_KEY;
	has_mine = find_in(mine, "player", to_key, NULL, &mine_player);
	has_theirs = find_in(theirs, "player", file->key, NULL, &their_player);
	if (has_theirs)
	{
		long mine_games = has_mine ? field_number(&mine_player, 3, 0) : 0, mine_clock = has_mine ? field_number(&mine_player, 2, 0) : 0;
		long their_games = field_number(&their_player, 3, 0), their_clock_value = field_number(&their_player, 2, 0);

		group_from_file = file_wins(mode, has_mine, mine_games, mine_clock, their_games, their_clock_value);
		tally(summary, group_from_file);
		row_add(summary, "SERVICE RECORD", has_mine ? mine_games : -1, their_games, group_from_file);
	}
	/* (the rivals: this PC's first, then the file's up to the cap; REPLACE: the file's alone) */
	at = 0;
	while (mode == AE_PM_MERGE && next_line(mine.text, mine.length, &at, &line, &count_) && rival_count < AE_PM_RIVALS)
		if (split(line, count_, &rec) && is_kind(&rec, "rival") && keyed(&rec, to_key))
		{
			field(&rec, 1, rivals[rival_count].opponent, sizeof(rivals[0].opponent));
			rivals[rival_count++].from_file = 0;
		}
	at = 0;
	while (next_line(theirs.text, theirs.length, &at, &line, &count_) && rival_count < AE_PM_RIVALS)
		if (split(line, count_, &rec) && is_kind(&rec, "rival") && keyed(&rec, file->key))
		{
			int index, present = 0;
			char opponent[FIELD];

			field(&rec, 1, opponent, sizeof(opponent));
			for (index = 0; index < rival_count; index++)
				present |= !strcmp(rivals[index].opponent, opponent);
			if (!present)
			{
				snprintf(rivals[rival_count].opponent, sizeof(rivals[0].opponent), "%s", opponent);
				rivals[rival_count++].from_file = 1;
			}
		}
	/* (the file's rivals and versus lines this PC does not take: past the cap, or of a rival not taken) */
	if (summary)
	{
		at = 0;
		while (next_line(theirs.text, theirs.length, &at, &line, &count_))
			if (split(line, count_, &rec) && (is_kind(&rec, "rival") || is_kind(&rec, "versus")) && keyed(&rec, file->key))
			{
				int index, present = 0;
				char opponent[FIELD];

				field(&rec, 1, opponent, sizeof(opponent));
				for (index = 0; index < rival_count; index++)
					present |= !strcmp(rivals[index].opponent, opponent);
				if (!present)
					summary->dropped++;
			}
	}
	if (summary)
		summary->rivals = rival_count;
	/* (REPLACE with a file that has no rival: this PC's are kept) */
	if (mode == AE_PM_REPLACE && !rival_count)
	{
		at = 0;
		while (next_line(mine.text, mine.length, &at, &line, &count_) && rival_count < AE_PM_RIVALS)
			if (split(line, count_, &rec) && is_kind(&rec, "rival") && keyed(&rec, to_key))
			{
				field(&rec, 1, rivals[rival_count].opponent, sizeof(rivals[0].opponent));
				rivals[rival_count++].from_file = 0;
			}
	}
	out[0] = 0;
	snprintf(number, sizeof(number), "%ld", clock > their_clock ? clock : their_clock);
	{
		char head[64];

		snprintf(head, sizeof(head), "# AE service record\nversion = %d\nclock = ", AE_SR_VERSION);
		put_text(&sink, head);
	}
	put_text(&sink, number);
	put_text(&sink, "\n");
	for (kind_index = 0; kind_index < 6; kind_index++)
	{
		char const *kind = order[kind_index];

		at = 0;
		while (next_line(mine.text, mine.length, &at, &line, &count_))
		{
			if (!split(line, count_, &rec) || !is_kind(&rec, kind))
				continue;
			if (group_kind(&rec) && keyed(&rec, to_key) && group_from_file)
				continue;
			if ((is_kind(&rec, "rival") || is_kind(&rec, "versus")) && keyed(&rec, to_key))
				continue;
			put_line(&sink, line, count_);
		}
		if (!strcmp(kind, "player") || !strcmp(kind, "playerx") || !strcmp(kind, "map") || !strcmp(kind, "lastgame"))
		{
			at = 0;
			while (group_from_file && next_line(theirs.text, theirs.length, &at, &line, &count_))
				if (split(line, count_, &rec) && is_kind(&rec, kind) && keyed(&rec, file->key))
					put_keyed(&sink, &rec, to_key);
		}
		else if (!strcmp(kind, "rival"))
		{
			int index;

			for (index = 0; index < rival_count; index++)
			{
				struct view view = rivals[index].from_file ? theirs : mine;

				if (find_in(view, "rival", rivals[index].from_file ? file->key : to_key, rivals[index].opponent, &other))
				{
					if (rivals[index].from_file)
						put_keyed(&sink, &other, to_key);
					else
						put_line(&sink, other.whole, other.whole_length);
				}
			}
		}
		else
		{
			int index;

			for (index = 0; index < rival_count; index++)
			{
				struct rec a, b;
				int in_mine = find_in(mine, "versus", to_key, rivals[index].opponent, &a);
				int in_theirs = find_in(theirs, "versus", file->key, rivals[index].opponent, &b);
				int take_file = 0;

				if (!in_mine && !in_theirs)
					continue;
				if (in_theirs && in_mine)
					take_file = file_wins(mode, 1, field_number(&a, 2, 0), 0, field_number(&b, 2, 0), 0);
				else
					take_file = in_theirs;
				if (take_file)
					put_keyed(&sink, &b, to_key);
				else
					put_line(&sink, a.whole, a.whole_length);
			}
		}
	}
	return finish(&sink, 0, length);
}

/* the bot records */
static void put_group(struct sink *sink, struct view view, char const *key, char const *to_key)
{
	static char const *const kinds[] = { "bot", "botx", "botkinds", "botmedals", "botmap" };
	int index;

	for (index = 0; index < 5; index++)
	{
		size_t at = 0;
		char const *line;
		size_t count_;
		struct rec rec;

		while (next_line(view.text, view.length, &at, &line, &count_))
			if (line_is(line, count_, kinds[index], key, NULL) && split(line, count_, &rec))
			{
				if (to_key)
					put_keyed(sink, &rec, to_key);
				else
					put_line(sink, line, count_);
			}
	}
}

/* which side's (profile, bot) pair of versus and lastgame is kept: 1 the file's, 0 this PC's, -1 neither */
static int versus_side(int mode, struct view mine, struct view theirs, char const *to_key, char const *file_key, char const *bot)
{
	struct rec a, b;
	int in_mine = find_in(mine, "versus", to_key, bot, &a), in_theirs = find_in(theirs, "versus", file_key, bot, &b);

	if (!in_mine && !in_theirs)
		return -1;
	if (!in_theirs)
		return mode == AE_PM_REPLACE ? -1 : 0;
	if (!in_mine)
		return 1;
	return file_wins(mode, 1, field_number(&a, 2, 0) + field_number(&a, 2, 3), field_number(&a, 2, 7),
		field_number(&b, 2, 0) + field_number(&b, 2, 3), field_number(&b, 2, 7));
}

int ae_pm_merge_bots(int mode, struct ae_pm_file const *file, char const *to_key, char const *target, char *out, size_t size,
	size_t *length, struct ae_pm_summary *summary)
{
	struct sink sink = { out, size, 0, 0 };
	struct view mine = target_view(target), theirs = file_view(file, 2);
	long clock = text_clock(mine.text, mine.length), their_clock = text_clock(theirs.text, theirs.length);
	char number[48];
	size_t at;
	char const *line;
	size_t count_;
	struct rec rec, other;
	int pass;

	if (!key_usable(to_key))
		return AE_PM_ERR_KEY;
	out[0] = 0;
	snprintf(number, sizeof(number), "%ld", clock > their_clock ? clock : their_clock);
	{
		char head[64];

		snprintf(head, sizeof(head), "# AE bot records\nversion = %d\nclock = ", AE_BOT_RECORD_VERSION);
		put_text(&sink, head);
	}
	put_text(&sink, number);
	put_text(&sink, "\n");
	/* (this PC's bots, each from the side the rule keeps; then the file's bots this PC has none of) */
	for (pass = 0; pass < 2; pass++)
	{
		struct view from = pass ? theirs : mine;

		at = 0;
		while (next_line(from.text, from.length, &at, &line, &count_))
		{
			char bot[FIELD];
			int has;

			if (!split(line, count_, &rec) || !is_kind(&rec, "bot"))
				continue;
			field(&rec, 0, bot, sizeof(bot));
			if (pass == 0)
			{
				has = find_in(theirs, "bot", bot, NULL, &other);
				if (has)
				{
					int take = file_wins(mode, 1, field_number(&rec, 3, 0), field_number(&rec, 2, 1), field_number(&other, 3, 0),
						field_number(&other, 2, 1));

					put_group(&sink, take ? theirs : mine, bot, NULL);
					if (summary)
					{
						if (take)
							summary->bots_from_file++;
						else
							summary->bots_kept++;
					}
				}
				else
				{
					put_group(&sink, mine, bot, NULL);
					if (summary)
						summary->bots_kept++;
				}
			}
			else if (!find_in(mine, "bot", bot, NULL, &other))
			{
				put_group(&sink, theirs, bot, NULL);
				if (summary)
					summary->bots_from_file++;
			}
		}
	}
	if (summary)
	{
		tally(summary, 0);
		summary->kept--;
		summary->from_file += summary->bots_from_file ? 1 : 0;
		summary->kept += summary->bots_kept ? 1 : 0;
		row_add(summary, "BOT RECORDS", summary->bots_kept + summary->bots_from_file, summary->bots_from_file, summary->bots_from_file > 0);
	}
	/* versus lines, then lastgame lines: other profiles' as they are, the profile's from the side kept */
	for (pass = 0; pass < 2; pass++)
	{
		char const *kind = pass ? "lastgame" : "versus";
		int round;

		for (round = 0; round < 2; round++)
		{
			struct view from = round ? theirs : mine;

			at = 0;
			while (next_line(from.text, from.length, &at, &line, &count_))
			{
				char bot[FIELD];
				int side;

				if (!split(line, count_, &rec) || !is_kind(&rec, kind))
					continue;
				if (round == 0 && !keyed(&rec, to_key))
				{
					put_line(&sink, line, count_);
					continue;
				}
				if (round == 1 && !keyed(&rec, file->key))
					continue;
				field(&rec, 1, bot, sizeof(bot));
				if (!find_in(mine, "bot", bot, NULL, &other) && !find_in(theirs, "bot", bot, NULL, &other))
				{
					if (summary)
						summary->dropped++;
					continue;
				}
				side = versus_side(mode, mine, theirs, to_key, file->key, bot);
				if (side != round)
					continue;
				if (round)
					put_keyed(&sink, &rec, to_key);
				else
					put_line(&sink, line, count_);
			}
		}
	}
	return finish(&sink, 1, length);
}

/* ---------- the backup of the three record files ---------- */

int ae_pm_backup_write(struct ae_pm_raw const raw[3], char const *const names[3], char *out, size_t size, size_t *length)
{
	struct sink sink = { out, size, 0, 0 };
	struct mcc_sha256 context;
	unsigned char digest[32];
	char line[160];
	int which, index;

	out[0] = 0;
	put_text(&sink, "# AE records backup\nversion = 1\n");
	for (which = 0; which < 3; which++)
	{
		snprintf(line, sizeof(line), "file = %s | %s | %ld\n", names[which], raw[which].length >= 0 ? "present" : "absent",
			raw[which].length >= 0 ? raw[which].length : 0L);
		put_text(&sink, line);
		if (raw[which].length >= 0)
		{
			put(&sink, raw[which].text, (size_t)raw[which].length);
			put_text(&sink, "\n");
		}
	}
	put_text(&sink, "[end]\n");
	if (sink.over)
		return AE_PM_ERR_ROOM;
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, out, (int)sink.length);
	mcc_sha256_end(&context, digest);
	put_text(&sink, "check = ");
	for (index = 0; index < 32; index++)
	{
		line[0] = "0123456789abcdef"[digest[index] >> 4];
		line[1] = "0123456789abcdef"[digest[index] & 15];
		line[2] = 0;
		put_text(&sink, line);
	}
	put_text(&sink, "\n");
	if (sink.over)
		return AE_PM_ERR_ROOM;
	*length = sink.length;
	return AE_PM_OK;
}

int ae_pm_backup_read(char const *text, size_t length, char const *const names[3], struct ae_pm_raw raw[3])
{
	static char const head[] = "# AE records backup\nversion = 1\n";
	size_t at = sizeof(head) - 1, end_at;
	struct mcc_sha256 context;
	unsigned char digest[32];
	char hex[65];
	int which, index;

	if (length < at || memcmp(text, head, at))
		return AE_PM_ERR_BACKUP;
	for (which = 0; which < 3; which++)
	{
		size_t line_end = at, name_length = strlen(names[which]);
		long size = 0;
		int present;
		char const *p;

		while (line_end < length && text[line_end] != '\n')
			line_end++;
		if (line_end >= length || line_end - at < name_length + 14 || memcmp(text + at, "file = ", 7) ||
			memcmp(text + at + 7, names[which], name_length) || memcmp(text + at + 7 + name_length, " | ", 3))
			return AE_PM_ERR_BACKUP;
		p = text + at + 7 + name_length + 3;
		{
			size_t left = (size_t)(text + line_end - p);

			if (left >= 10 && !memcmp(p, "present | ", 10))
				present = 1, p += 10;
			else if (left >= 9 && !memcmp(p, "absent | ", 9))
				present = 0, p += 9;
			else
				return AE_PM_ERR_BACKUP;
		}
		{
			int digits = 0;

			while (p < text + line_end && *p >= '0' && *p <= '9' && digits < 9)
				size = size * 10 + (*p++ - '0'), digits++;
			if (!digits || p != text + line_end)
				return AE_PM_ERR_BACKUP;
		}
		at = line_end + 1;
		if (!present)
		{
			if (size)
				return AE_PM_ERR_BACKUP;
			raw[which].text = "";
			raw[which].length = -1;
			continue;
		}
		if (at + (size_t)size + 1 > length || text[at + (size_t)size] != '\n')
			return AE_PM_ERR_BACKUP;
		raw[which].text = text + at;
		raw[which].length = size;
		at += (size_t)size + 1;
	}
	if (length < at + 6 || memcmp(text + at, "[end]\n", 6))
		return AE_PM_ERR_BACKUP;
	end_at = at + 6;
	if (length != end_at + 8 + 64 + 1 || memcmp(text + end_at, "check = ", 8) || text[length - 1] != '\n')
		return AE_PM_ERR_BACKUP;
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, text, (int)end_at);
	mcc_sha256_end(&context, digest);
	for (index = 0; index < 32; index++)
	{
		hex[index * 2] = "0123456789abcdef"[digest[index] >> 4];
		hex[index * 2 + 1] = "0123456789abcdef"[digest[index] & 15];
	}
	hex[64] = 0;
	if (memcmp(text + end_at + 8, hex, 64))
		return AE_PM_ERR_BACKUP;
	return AE_PM_OK;
}

void ae_pm_backup_stem(char const *key, char *stem, size_t size)
{
	char part[24];
	size_t at = 0;
	char const *c;

	for (c = key; *c && at + 1 < sizeof(part) && at < 20; c++)
	{
		char x = *c;
		int fine = (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9') || x == '-' || x == '_' ||
			(x == '.' && at > 0);

		part[at++] = fine ? x : '_';
	}
	part[at] = 0;
	if (!at)
		snprintf(part, sizeof(part), "profile");
	snprintf(stem, size, "%s-%08lx-", part, ae_pm_fnv(key, strlen(key)));
}

int ae_pm_backup_name(char const *stem, char const *stamp, int clash, char const *suffix, char *out, size_t size)
{
	int length;

	if (clash >= 2)
		length = snprintf(out, size, "%s%s-%d%s", stem, stamp, clash, suffix);
	else
		length = snprintf(out, size, "%s%s%s", stem, stamp, suffix);
	return length > 0 && (size_t)length < size;
}

static int digits_at(char const *text, int count)
{
	int index;

	for (index = 0; index < count; index++)
		if (text[index] < '0' || text[index] > '9')
			return 0;
	return 1;
}

int ae_pm_backup_matches(char const *name, char const *stem, char const *suffix)
{
	size_t stem_length = strlen(stem), suffix_length = strlen(suffix), length = strlen(name);
	char const *at;

	if (length < stem_length + 15 + suffix_length || memcmp(name, stem, stem_length) ||
		strcmp(name + length - suffix_length, suffix))
		return 0;
	at = name + stem_length;
	if (!digits_at(at, 8) || at[8] != '-' || !digits_at(at + 9, 6))
		return 0;
	at += 15;
	if (at == name + length - suffix_length)
		return 1;
	return *at == '-' && at + 1 < name + length - suffix_length && digits_at(at + 1, (int)(name + length - suffix_length - at - 1));
}

/* whether name a is a newer backup than b (both this stem's): the stamp, then the clash number as a number (no suffix: 1) */
static int newer_backup(char const *a, char const *b, size_t stem_length, char const *suffix)
{
	size_t suffix_length = strlen(suffix);
	int cmp = memcmp(a + stem_length, b + stem_length, 15), clash_a = 1, clash_b = 1;

	if (cmp)
		return cmp > 0;
	if (a[stem_length + 15] == '-')
		clash_a = atoi(a + stem_length + 16);
	if (b[stem_length + 15] == '-')
		clash_b = atoi(b + stem_length + 16);
	(void)suffix_length;
	return clash_a > clash_b;
}

int ae_pm_prune_select(char const *const *names, int count, char const *stem, char const *suffix, int keep, char const *protect,
	int *victims, int max)
{
	int order[256], found = 0, index, other, out = 0;
	size_t stem_length = strlen(stem);

	for (index = 0; index < count && found < 256; index++)
		if (ae_pm_backup_matches(names[index], stem, suffix))
			order[found++] = index;
	/* (the newest first) */
	for (index = 1; index < found; index++)
	{
		int moving = order[index];

		for (other = index; other > 0 && newer_backup(names[moving], names[order[other - 1]], stem_length, suffix); other--)
			order[other] = order[other - 1];
		order[other] = moving;
	}
	/* (the backup just written is never one of them, whatever its stamp says) */
	for (index = keep; index < found && out < max; index++)
		if (!protect || strcmp(names[order[index]], protect))
			victims[out++] = order[index];
	return out;
}

/* ---------- the write ---------- */

int ae_pm_rollback(struct ae_pm_ops const *ops, void *context, struct ae_pm_raw const raw[3])
{
	int which, ok = 1;

	for (which = 0; which < 3; which++)
		if (!ops->put(context, which, raw[which].text, raw[which].length))
			ok = 0;
	return ok;
}

int ae_pm_apply(struct ae_pm_ops const *ops, void *context, char const *const texts[3], struct ae_pm_raw const raw[3])
{
	int which, done;

	for (which = 0; which < 3; which++)
	{
		if (!ops->stage(context, which, texts[which]))
		{
			for (done = 0; done < 3; done++)
				ops->discard(context, done);
			return AE_PM_APPLY_UNCHANGED;
		}
	}
	if (!ops->marker(context, 1))
	{
		for (done = 0; done < 3; done++)
			ops->discard(context, done);
		return AE_PM_APPLY_UNCHANGED;
	}
	for (which = 0; which < 3; which++)
	{
		if (!ops->commit(context, which, texts[which] == NULL))
		{
			int ok = 1;

			/* (the files already changed go back to their bytes; the staged ones are dropped) */
			for (done = 0; done < which; done++)
				if (!ops->put(context, done, raw[done].text, raw[done].length))
					ok = 0;
			for (done = 0; done < 3; done++)
				ops->discard(context, done);
			if (!ok)
				return AE_PM_APPLY_BROKEN;
			ops->marker(context, 0);
			return AE_PM_APPLY_ROLLED_BACK;
		}
	}
	/* (done; a mark that cannot be taken away is told: the next start sees the files are the new ones and only drops it) */
	if (!ops->marker(context, 0))
		return AE_PM_APPLY_MARKER_LEFT;
	return AE_PM_APPLY_OK;
}

int ae_pm_file_url(char const *path, char *out, size_t size)
{
	size_t at = 0;
	char const *c;

	if (!path || !*path || size < 16)
		return 0;
	at = (size_t)snprintf(out, size, "file://%s", path[0] == '/' || path[0] == '\\' ? "" : "/");
	for (c = path; *c; c++)
	{
		unsigned char x = (unsigned char)(*c == '\\' ? '/' : *c);

		if (at + 4 >= size)
			return 0;
		if ((x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9') || x == '-' || x == '.' || x == '_' ||
			x == '~' || x == '/' || x == ':')
			out[at++] = (char)x;
		else
		{
			out[at++] = '%';
			out[at++] = "0123456789ABCDEF"[x >> 4];
			out[at++] = "0123456789ABCDEF"[x & 15];
		}
	}
	out[at] = 0;
	return 1;
}

/* ---------- the mark of a write under way ---------- */

static void hash_hex(char const *text, long length, char *hex)
{
	struct mcc_sha256 context;
	unsigned char digest[32];
	int index;

	if (length < 0)
	{
		snprintf(hex, 65, "absent");
		return;
	}
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, text, (int)length);
	mcc_sha256_end(&context, digest);
	for (index = 0; index < 32; index++)
	{
		hex[index * 2] = "0123456789abcdef"[digest[index] >> 4];
		hex[index * 2 + 1] = "0123456789abcdef"[digest[index] & 15];
	}
	hex[64] = 0;
}

int ae_pm_marker_write(char const *backup, char const *const texts[3], char *out, size_t size)
{
	static char const *const kinds[3] = { "career", "service", "bots" };
	struct sink sink = { out, size, 0, 0 };
	char hex[65];
	int which;

	out[0] = 0;
	put_text(&sink, "# AE profile move\nversion = 1\nbackup = ");
	put_text(&sink, backup);
	put_text(&sink, "\n");
	for (which = 0; which < 3; which++)
	{
		hash_hex(texts[which], texts[which] ? (long)strlen(texts[which]) : -1, hex);
		put_text(&sink, kinds[which]);
		put_text(&sink, " = ");
		put_text(&sink, hex);
		put_text(&sink, "\n");
	}
	return sink.over ? AE_PM_ERR_ROOM : AE_PM_OK;
}

int ae_pm_marker_decide(char const *marker, size_t length, struct ae_pm_raw const now[3], char *backup, size_t size)
{
	static char const *const kinds[3] = { "career", "service", "bots" };
	static char const head[] = "# AE profile move\nversion = 1\n";
	char hashes[3][65], have[65];
	size_t at = 0;
	char const *line;
	size_t count;
	int which, seen = 0, named = 0;

	if (backup && size)
		backup[0] = 0;

	if (!marker || length < sizeof(head) - 1 || memcmp(marker, head, sizeof(head) - 1))
		return AE_PM_RECOVER_BAD;
	while (next_line(marker, length, &at, &line, &count))
	{
		struct rec rec;

		if (!split(line, count, &rec))
			continue;
		if (is_kind(&rec, "backup") && rec.length > 0 && rec.length < size && backup)
		{
			memcpy(backup, rec.value, rec.length);
			backup[rec.length] = 0;
			named = 1;
			continue;
		}
		for (which = 0; which < 3; which++)
			if (is_kind(&rec, kinds[which]) && rec.length <= 64)
			{
				memcpy(hashes[which], rec.value, rec.length);
				hashes[which][rec.length] = 0;
				seen |= 1 << which;
			}
	}
	if (!named || seen != 7)
		return AE_PM_RECOVER_BAD;
	/* (the files on disk are the new texts: the write was done and only its mark is left) */
	for (which = 0; which < 3; which++)
	{
		hash_hex(now[which].text, now[which].length, have);
		if (strcmp(have, hashes[which]))
			return AE_PM_RECOVER_ROLLBACK;
	}
	return AE_PM_RECOVER_DONE;
}

int ae_pm_retry_due(unsigned long *last, unsigned long now, unsigned long every)
{
	if (*last && now >= *last && now - *last < every)
		return 0;
	*last = now;
	return 1;
}
