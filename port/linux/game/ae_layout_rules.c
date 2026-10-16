/*
AE_LAYOUT_RULES.C

The map layout file read and checked (ae_layout_rules.h). Pure: no engine includes.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_layout_rules.h"

#define MAXIMUM_WORDS 24

static char const *const alias_names[NUMBER_OF_AE_LAYOUT_ALIASES] =
{
	"rockets", "sniper", "shotgun", "pistol", "overshield", "camo"
};

char const *ae_layout_alias_name(short alias)
{
	return alias >= 0 && alias < NUMBER_OF_AE_LAYOUT_ALIASES ? alias_names[alias] : "?";
}

static int same_word(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

/* a whole word as a number */
static int real_of(char const *word, real *value)
{
	char *end;
	double number = strtod(word, &end);

	if (end == word || *end || number != number || number > 1.0e7 || number < -1.0e7)
		return 0;
	*value = (real)number;
	return 1;
}

static int whole_of(char const *word, long low, long high, long *value)
{
	char *end;
	long number = strtol(word, &end, 10);

	if (end == word || *end || number < low || number > high)
		return 0;
	*value = number;
	return 1;
}

static int hash_of(char const *word, unsigned long *value)
{
	char *end;
	unsigned long number;

	if (strlen(word) != 8)
		return 0;
	number = strtoul(word, &end, 16);
	if (*end)
		return 0;
	*value = number;
	return 1;
}

static void say(char *error, int error_size, char const *text, char const *word)
{
	if (error && error_size > 0)
		snprintf(error, (size_t)error_size, "%s%s%s%s", text, word ? " \"" : "", word ? word : "", word ? "\"" : "");
}

/* item <what> at x y z [facing d] [respawn s] [team t]: words[1..count-1] */
static int parse_item(char **words, int count, struct ae_layout_item *item, char *error, int error_size)
{
	int at = 1;
	long number;
	short alias;

	memset(item, 0, sizeof(*item));
	item->respawn_seconds = -1;
	item->team = _ae_layout_team_unset;
	if (count < 2)
	{
		say(error, error_size, "item: what?", NULL);
		return 0;
	}
	if (same_word(words[at], "like"))
	{
		if (count < 3 || !whole_of(words[at + 1], 0, 65535, &number))
		{
			say(error, error_size, "item like: not an item number", count < 3 ? NULL : words[at + 1]);
			return 0;
		}
		item->what = _ae_layout_like;
		item->like = number;
		at += 2;
	}
	else
	{
		for (alias = 0; alias < NUMBER_OF_AE_LAYOUT_ALIASES; alias++)
			if (same_word(words[at], alias_names[alias]))
				break;
		if (alias < NUMBER_OF_AE_LAYOUT_ALIASES)
		{
			item->what = _ae_layout_alias;
			item->alias = alias;
		}
		else
		{
			if (strlen(words[at]) >= sizeof(item->tag) || !strchr(words[at], '\\'))
			{
				say(error, error_size, "item: not an alias, like <n> or a tag path", words[at]);
				return 0;
			}
			item->what = _ae_layout_tag;
			strcpy(item->tag, words[at]);
		}
		at++;
	}
	if (at + 4 > count || !same_word(words[at], "at") || !real_of(words[at + 1], &item->x) ||
		!real_of(words[at + 2], &item->y) || !real_of(words[at + 3], &item->z))
	{
		say(error, error_size, "item: wants at <x> <y> <z>", at < count ? words[at] : NULL);
		return 0;
	}
	at += 4;
	while (at < count)
	{
		if (same_word(words[at], "facing") && at + 1 < count && real_of(words[at + 1], &item->facing_degrees) &&
			!item->has_facing)
		{
			item->has_facing = TRUE;
		}
		else if (same_word(words[at], "respawn") && at + 1 < count && whole_of(words[at + 1], 1, 3600, &number) &&
			item->respawn_seconds < 0)
		{
			item->respawn_seconds = (short)number;
		}
		else if (same_word(words[at], "games") && at + 1 < count && same_word(words[at + 1], "all") && !item->games_all)
		{
			item->games_all = TRUE;
		}
		else if (same_word(words[at], "team") && at + 1 < count && item->team == _ae_layout_team_unset &&
			(same_word(words[at + 1], "red") || same_word(words[at + 1], "blue") || same_word(words[at + 1], "neutral")))
		{
			item->team = (short)(same_word(words[at + 1], "red") ? _ae_layout_team_red :
				same_word(words[at + 1], "blue") ? _ae_layout_team_blue : _ae_layout_team_neutral);
		}
		else
		{
			say(error, error_size, "item: not understood (or given twice)", words[at]);
			return 0;
		}
		at += 2;
	}
	return 1;
}

/* remove vehicle <turret|n> [near x y z r] */
static int parse_vehicle(char **words, int count, struct ae_layout_vehicle_rule *rule, char *error, int error_size)
{
	long number;

	memset(rule, 0, sizeof(*rule));
	if (count < 3)
	{
		say(error, error_size, "remove vehicle: which?", NULL);
		return 0;
	}
	if (same_word(words[2], "turret"))
		rule->turret = TRUE;
	else if (whole_of(words[2], 0, 65535, &number))
		rule->palette_index = number;
	else
	{
		say(error, error_size, "remove vehicle: not turret or a palette number", words[2]);
		return 0;
	}
	if (count == 3)
		return 1;
	if (count != 8 || !same_word(words[3], "near") || !real_of(words[4], &rule->x) || !real_of(words[5], &rule->y) ||
		!real_of(words[6], &rule->z) || !real_of(words[7], &rule->radius) || rule->radius <= 0.0f)
	{
		say(error, error_size, "remove vehicle: wants near <x> <y> <z> <radius>", words[3]);
		return 0;
	}
	rule->near = TRUE;
	return 1;
}

int ae_layout_parse(char const *text, long length, struct ae_layout *out, char *error, int error_size)
{
	char line[AE_LAYOUT_MAXIMUM_LINE + 2];
	char *words[MAXIMUM_WORDS];
	long position = 0;
	int line_number = 0;
	boolean header = FALSE;

	memset(out, 0, sizeof(*out));
	if (error && error_size > 0)
		error[0] = 0;
	if (!text || length < 0 || length > AE_LAYOUT_MAXIMUM_FILE_SIZE)
	{
		say(error, error_size, "the file is bigger than 64 KB", NULL);
		return 1;
	}
	while (position < length)
	{
		long start = position, size;
		int count = 0;
		char *walk, *hash;

		while (position < length && text[position] != '\n')
			position++;
		size = position - start;
		position++;
		line_number++;
		if (line_number > AE_LAYOUT_MAXIMUM_LINES)
		{
			say(error, error_size, "more than 512 lines", NULL);
			goto refused;
		}
		if (size > 0 && text[start + size - 1] == '\r')
			size--;
		if (size > AE_LAYOUT_MAXIMUM_LINE)
		{
			say(error, error_size, "a line longer than 200 characters", NULL);
			goto refused;
		}
		memcpy(line, text + start, (size_t)size);
		line[size] = 0;
		if (memchr(line, 0, (size_t)size))
		{
			say(error, error_size, "a NUL in the line", NULL);
			goto refused;
		}
		hash = strchr(line, '#');
		if (hash)
			*hash = 0;
		for (walk = line; *walk;)
		{
			while (*walk == ' ' || *walk == '\t')
				*walk++ = 0;
			if (!*walk)
				break;
			if (count == MAXIMUM_WORDS)
			{
				say(error, error_size, "too many words", NULL);
				goto refused;
			}
			if (*walk == '"')
			{
				/* (a quoted word: a tag path with spaces in it) */
				char *close = strchr(walk + 1, '"');

				if (!close)
				{
					say(error, error_size, "a quote not closed", NULL);
					goto refused;
				}
				*close = 0;
				words[count++] = walk + 1;
				walk = close + 1;
				if (*walk && *walk != ' ' && *walk != '\t')
				{
					say(error, error_size, "a quoted word runs into the next", NULL);
					goto refused;
				}
				continue;
			}
			words[count++] = walk;
			while (*walk && *walk != ' ' && *walk != '\t')
				walk++;
		}
		if (!count)
			continue;
		if (!header)
		{
			long version;

			if (count != 4 || !same_word(words[0], "layout") || !whole_of(words[1], 0, 1000, &version) ||
				strlen(words[2]) >= sizeof(out->map_file) || !hash_of(words[3], &out->map_hash))
			{
				say(error, error_size, "the first line must be: layout 1 <map file name> <8 hex digits>", words[0]);
				goto refused;
			}
			if (version != AE_LAYOUT_VERSION)
			{
				say(error, error_size, "a layout of another version", words[1]);
				goto refused;
			}
			strcpy(out->map_file, words[2]);
			header = TRUE;
		}
		else if (count == 2 && same_word(words[0], "clear") && same_word(words[1], "items"))
		{
			out->clear_items = TRUE;
		}
		else if (count >= 2 && same_word(words[0], "remove") && same_word(words[1], "item"))
		{
			long number;

			if (count != 3 || !whole_of(words[2], 0, 65535, &number))
			{
				say(error, error_size, "remove item: not an item number", count > 2 ? words[2] : NULL);
				goto refused;
			}
			if (out->removed_count == AE_LAYOUT_MAXIMUM_REMOVED)
			{
				say(error, error_size, "more than 256 items removed", NULL);
				goto refused;
			}
			out->removed[out->removed_count++] = number;
		}
		else if (count >= 2 && same_word(words[0], "remove") && same_word(words[1], "vehicle"))
		{
			if (out->vehicle_rule_count == AE_LAYOUT_MAXIMUM_VEHICLE_RULES)
			{
				say(error, error_size, "more than 32 vehicle lines", NULL);
				goto refused;
			}
			if (!parse_vehicle(words, count, &out->vehicle_rules[out->vehicle_rule_count], error, error_size))
				goto refused;
			out->vehicle_rules[out->vehicle_rule_count++].line = (short)line_number;
		}
		else if (count >= 2 && same_word(words[0], "name") && same_word(words[1], "item"))
		{
			long number;

			if (count != 4 || !whole_of(words[2], 0, 65535, &number) || !words[3][0] ||
				strlen(words[3]) >= AE_LAYOUT_LABEL_SIZE)
			{
				say(error, error_size, "name item: wants <n> <label of up to 15 characters>", count > 3 ? words[3] : NULL);
				goto refused;
			}
			if (out->name_count == AE_LAYOUT_MAXIMUM_NAMES)
			{
				say(error, error_size, "more than 64 names", NULL);
				goto refused;
			}
			out->names[out->name_count].item = number;
			strcpy(out->names[out->name_count].label, words[3]);
			out->name_count++;
		}
		else if (same_word(words[0], "item"))
		{
			if (out->item_count == AE_LAYOUT_MAXIMUM_ITEMS)
			{
				say(error, error_size, "more than 256 items", NULL);
				goto refused;
			}
			if (!parse_item(words, count, &out->items[out->item_count], error, error_size))
				goto refused;
			out->items[out->item_count++].line = (short)line_number;
		}
		else
		{
			say(error, error_size, "not a layout line", words[0]);
			goto refused;
		}
	}
	if (!header)
	{
		say(error, error_size, "no layout line", NULL);
		return line_number ? line_number : 1;
	}
	return 0;

refused:
	memset(out, 0, sizeof(*out));
	return line_number;
}

boolean ae_layout_for_map(struct ae_layout const *layout, char const *map_file, unsigned long map_hash)
{
	return (boolean)(layout && map_file && same_word(layout->map_file, map_file) && layout->map_hash == map_hash);
}
