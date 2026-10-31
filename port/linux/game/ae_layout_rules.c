/*
AE_LAYOUT_RULES.C

The map layout file read and checked (ae_layout_rules.h). Pure: no engine includes.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_layout_rules.h"
#include "ae_weapon_slots.h"

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

/* item <what> at x y z [facing d] [respawn s] [team t] [games all] [drop | float]: words[1..count-1] */
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
		else if (ae_weapon_slots_alias(words[at]) != AE_WEAPON_SLOT_NONE)
		{
			/* (a pack weapon: battle_rifle, smg, smg_silenced, carbine, ar_silenced; ae_weapon_slots.h) */
			item->what = _ae_layout_pack;
			item->pack_slot = ae_weapon_slots_alias(words[at]);
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
		/* (the one word without a value) */
		if (same_word(words[at], "drop") && !item->drop && !item->float_here)
		{
			item->drop = TRUE;
			at++;
			continue;
		}
		if (same_word(words[at], "float") && !item->float_here && !item->drop)
		{
			item->float_here = TRUE;
			at++;
			continue;
		}
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

/* the flag types' names, by enum ae_layout_flag_type (5, the vegas bank, is no line's) */
static struct { char const *name; short type; } const flag_types[] =
{
	{ "ctf", _ae_layout_flag_ctf }, { "ctf_vehicle", _ae_layout_flag_ctf_vehicle }, { "ball", _ae_layout_flag_ball },
	{ "race", _ae_layout_flag_race }, { "race_vehicle", _ae_layout_flag_race_vehicle },
	{ "teleport_from", _ae_layout_flag_teleport_from }, { "teleport_to", _ae_layout_flag_teleport_to },
	{ "hill", _ae_layout_flag_hill }
};

/* a spawn's game type words, as the scenario's numbers */
static struct { char const *name; short games; } const game_words[] =
{
	{ "all", AE_LAYOUT_GAMES_ALL }, { "ctf", 1 }, { "slayer", 2 }, { "oddball", 3 }, { "king", 4 }, { "race", 5 }
};

char const *ae_layout_flag_type_name(short type)
{
	unsigned int index;

	for (index = 0; index < sizeof(flag_types) / sizeof(flag_types[0]); index++)
		if (flag_types[index].type == type)
			return flag_types[index].name;
	return "?";
}

/* at x y z [facing d] then the line's own words: words[at] is "at"; the next word past the point and facing into *next */
static int parse_point(char **words, int count, int at, real *x, real *y, real *z, boolean *has_facing, real *facing,
	int *next)
{
	if (at + 4 > count || !same_word(words[at], "at") || !real_of(words[at + 1], x) || !real_of(words[at + 2], y) ||
		!real_of(words[at + 3], z))
	{
		return 0;
	}
	at += 4;
	if (at + 1 < count && same_word(words[at], "facing"))
	{
		if (!real_of(words[at + 1], facing))
			return 0;
		*has_facing = TRUE;
		at += 2;
	}
	*next = at;
	return 1;
}

/* spawn at x y z [facing d] [team red|blue] [games ...] */
static int parse_spawn(char **words, int count, struct ae_layout_spawn *spawn, char *error, int error_size)
{
	int at, team_given = FALSE, games_given = FALSE;

	memset(spawn, 0, sizeof(*spawn));
	if (!parse_point(words, count, 1, &spawn->x, &spawn->y, &spawn->z, &spawn->has_facing, &spawn->facing_degrees, &at))
	{
		say(error, error_size, "spawn: wants at <x> <y> <z> [facing <degrees>]", count > 1 ? words[1] : NULL);
		return 0;
	}
	spawn->games[0] = AE_LAYOUT_GAMES_ALL;
	while (at < count)
	{
		if (same_word(words[at], "team") && at + 1 < count && !team_given &&
			(same_word(words[at + 1], "red") || same_word(words[at + 1], "blue")))
		{
			spawn->team = (short)(same_word(words[at + 1], "blue") ? 1 : 0);
			team_given = TRUE;
			at += 2;
		}
		else if (same_word(words[at], "games") && at + 1 < count && !games_given)
		{
			int game_count = 0;

			games_given = TRUE;
			spawn->games[0] = 0;
			at++;
			while (at < count && !same_word(words[at], "team"))
			{
				unsigned int index;

				for (index = 0; index < sizeof(game_words) / sizeof(game_words[0]); index++)
					if (same_word(words[at], game_words[index].name))
						break;
				if (index == sizeof(game_words) / sizeof(game_words[0]) || game_count == 4)
				{
					say(error, error_size, "spawn games: up to 4 of all ctf slayer oddball king race", words[at]);
					return 0;
				}
				spawn->games[game_count++] = game_words[index].games;
				at++;
			}
		}
		else
		{
			say(error, error_size, "spawn: not understood (or given twice)", words[at]);
			return 0;
		}
	}
	return 1;
}

/* flag <type> at x y z [facing d] [team red|blue|n] */
static int parse_flag(char **words, int count, struct ae_layout_flag *flag, char *error, int error_size)
{
	unsigned int index;
	int at;
	long number;

	memset(flag, 0, sizeof(*flag));
	for (index = 0; count > 1 && index < sizeof(flag_types) / sizeof(flag_types[0]); index++)
		if (same_word(words[1], flag_types[index].name))
			break;
	if (count < 2 || index == sizeof(flag_types) / sizeof(flag_types[0]))
	{
		say(error, error_size, "flag: not a flag type (ctf ctf_vehicle ball race race_vehicle teleport_from teleport_to "
			"hill)", count > 1 ? words[1] : NULL);
		return 0;
	}
	flag->type = flag_types[index].type;
	if (!parse_point(words, count, 2, &flag->x, &flag->y, &flag->z, &flag->has_facing, &flag->facing_degrees, &at))
	{
		say(error, error_size, "flag: wants at <x> <y> <z> [facing <degrees>]", count > 2 ? words[2] : NULL);
		return 0;
	}
	if (at < count)
	{
		if (at + 2 != count || !same_word(words[at], "team"))
		{
			say(error, error_size, "flag: not understood", words[at]);
			return 0;
		}
		if (same_word(words[at + 1], "red"))
			flag->team = 0;
		else if (same_word(words[at + 1], "blue"))
			flag->team = 1;
		else if (whole_of(words[at + 1], 0, 15, &number))
			flag->team = (short)number;
		else
		{
			say(error, error_size, "flag team: red, blue or 0 to 15", words[at + 1]);
			return 0;
		}
	}
	return 1;
}

/* games <up to 4 words> from words[at]: the scenario's numbers into games; *next the word after them (the stop word,
or the line's end, ends the list) */
static int parse_games(char **words, int count, int at, char const *stop, short games[4], int *next, char *error,
	int error_size, char const *what)
{
	int game_count = 0;

	games[0] = games[1] = games[2] = games[3] = 0;
	while (at < count && !same_word(words[at], stop))
	{
		unsigned int index;

		for (index = 0; index < sizeof(game_words) / sizeof(game_words[0]); index++)
			if (same_word(words[at], game_words[index].name))
				break;
		if (index == sizeof(game_words) / sizeof(game_words[0]) || game_count == 4)
		{
			say(error, error_size, what, words[at]);
			return 0;
		}
		games[game_count++] = game_words[index].games;
		at++;
	}
	if (!game_count)
	{
		say(error, error_size, what, NULL);
		return 0;
	}
	*next = at;
	return 1;
}

static char const *const vehicle_kind_names[NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS] =
{
	"warthog", "ghost", "scorpion", "rocket_warthog", "banshee", "turret"
};

char const *ae_layout_vehicle_kind_name(short kind)
{
	return kind >= 0 && kind < NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS ? vehicle_kind_names[kind] :
		kind == _ae_layout_vehicle_palette ? "palette entry" : "?";
}

short ae_layout_vehicle_kind_of_name(char const *tag_name)
{
	if (!tag_name)
		return NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS;
	/* (game_engine_variant_vehicle_type's words, in its order) */
	if (strstr(tag_name, "rwarthog") || strstr(tag_name, "rocket"))
		return _ae_layout_vehicle_rocket_warthog;
	if (strstr(tag_name, "banshee"))
		return _ae_layout_vehicle_banshee;
	if (strstr(tag_name, "turret"))
		return _ae_layout_vehicle_turret;
	if (strstr(tag_name, "warthog"))
		return _ae_layout_vehicle_warthog;
	if (strstr(tag_name, "ghost"))
		return _ae_layout_vehicle_ghost;
	if (strstr(tag_name, "scorpion"))
		return _ae_layout_vehicle_scorpion;
	return NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS;
}

boolean ae_layout_games_take(short const games[4], long game_engine_index)
{
	int index;

	for (index = 0; index < 4; index++)
		if (games[index] == AE_LAYOUT_GAMES_ALL || (games[index] && games[index] == game_engine_index))
			return TRUE;
	return FALSE;
}

/* vehicle <kind|n> at x y z [facing d] [team red|blue|neutral] [games ...] */
static int parse_vehicle_add(char **words, int count, struct ae_layout_vehicle *vehicle, char *error, int error_size)
{
	int at, team_given = FALSE, games_given = FALSE;
	long number;
	short kind;

	memset(vehicle, 0, sizeof(*vehicle));
	vehicle->team = _ae_layout_team_unset;
	vehicle->games[0] = AE_LAYOUT_GAMES_ALL;
	if (count < 2)
	{
		say(error, error_size, "vehicle: which?", NULL);
		return 0;
	}
	for (kind = 0; kind < NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS; kind++)
		if (same_word(words[1], vehicle_kind_names[kind]))
			break;
	if (kind < NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS)
		vehicle->kind = kind;
	else if (whole_of(words[1], 0, 65535, &number))
	{
		vehicle->kind = _ae_layout_vehicle_palette;
		vehicle->palette_index = number;
	}
	else
	{
		say(error, error_size, "vehicle: not warthog, rocket_warthog, ghost, scorpion, banshee, turret or a palette "
			"number", words[1]);
		return 0;
	}
	if (!parse_point(words, count, 2, &vehicle->x, &vehicle->y, &vehicle->z, &vehicle->has_facing,
		&vehicle->facing_degrees, &at))
	{
		say(error, error_size, "vehicle: wants at <x> <y> <z> [facing <degrees>]", count > 2 ? words[2] : NULL);
		return 0;
	}
	while (at < count)
	{
		if (same_word(words[at], "team") && at + 1 < count && !team_given &&
			(same_word(words[at + 1], "red") || same_word(words[at + 1], "blue") || same_word(words[at + 1], "neutral")))
		{
			vehicle->team = (short)(same_word(words[at + 1], "red") ? _ae_layout_team_red :
				same_word(words[at + 1], "blue") ? _ae_layout_team_blue : _ae_layout_team_neutral);
			team_given = TRUE;
			at += 2;
		}
		else if (same_word(words[at], "games") && !games_given)
		{
			games_given = TRUE;
			if (!parse_games(words, count, at + 1, "team", vehicle->games, &at, error, error_size,
				"vehicle games: up to 4 of all ctf slayer oddball king race"))
			{
				return 0;
			}
		}
		else
		{
			say(error, error_size, "vehicle: not understood (or given twice)", words[at]);
			return 0;
		}
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

static char const *const object_type_names[NUMBER_OF_AE_LAYOUT_OBJECT_TYPES] =
{
	"scenery", "machine", "control", "light_fixture", "sound_scenery", "biped", "weapon", "equipment"
};

char const *ae_layout_object_type_name(short type)
{
	return type >= 0 && type < NUMBER_OF_AE_LAYOUT_OBJECT_TYPES ? object_type_names[type] : "?";
}

/* remove scenery <name|n> [near x y z r] | remove object <type> <name|n> [near x y z r]: at is the word of the name or
number (2 for scenery, 3 for object) */
static int parse_object(char **words, int count, int at, short type, struct ae_layout_object_rule *rule, char *error,
	int error_size)
{
	long number;

	memset(rule, 0, sizeof(*rule));
	rule->type = type;
	if (count <= at)
	{
		say(error, error_size, "remove scenery/object: which?", NULL);
		return 0;
	}
	if (whole_of(words[at], 0, 65535, &number))
		rule->palette_index = number;
	else if (strlen(words[at]) < sizeof(rule->name) && words[at][0])
	{
		rule->by_name = TRUE;
		strcpy(rule->name, words[at]);
	}
	else
	{
		say(error, error_size, "remove scenery/object: not a name or a palette number", words[at]);
		return 0;
	}
	if (count == at + 1)
		return 1;
	if (count != at + 6 || !same_word(words[at + 1], "near") || !real_of(words[at + 2], &rule->x) ||
		!real_of(words[at + 3], &rule->y) || !real_of(words[at + 4], &rule->z) || !real_of(words[at + 5], &rule->radius) ||
		rule->radius <= 0.0f)
	{
		say(error, error_size, "remove scenery/object: wants near <x> <y> <z> <radius>", words[at + 1]);
		return 0;
	}
	rule->near = TRUE;
	return 1;
}

/* bot jump at x y z to x y z | bot avoid x y z radius r */
static int parse_bot(char **words, int count, struct ae_layout *out, short line_number, char *error, int error_size)
{
	if (same_word(words[1], "jump"))
	{
		struct ae_layout_bot_jump jump;
		real dx, dy, dz, length;

		memset(&jump, 0, sizeof(jump));
		if (count != 10 || !same_word(words[2], "at") || !real_of(words[3], &jump.from_x) ||
			!real_of(words[4], &jump.from_y) || !real_of(words[5], &jump.from_z) || !same_word(words[6], "to") ||
			!real_of(words[7], &jump.to_x) || !real_of(words[8], &jump.to_y) || !real_of(words[9], &jump.to_z))
		{
			say(error, error_size, "bot jump: wants at <x> <y> <z> to <x> <y> <z>", count > 2 ? words[2] : NULL);
			return 0;
		}
		dx = jump.to_x - jump.from_x;
		dy = jump.to_y - jump.from_y;
		dz = jump.to_z - jump.from_z;
		length = (real)sqrt(dx * dx + dy * dy + dz * dz);
		if (length < AE_LAYOUT_BOT_JUMP_SHORTEST || length > AE_LAYOUT_BOT_JUMP_LONGEST)
		{
			say(error, error_size, "bot jump: the points must be 0.5 to 20 units apart", NULL);
			return 0;
		}
		if (out->bot_jump_count == AE_LAYOUT_MAXIMUM_BOT_JUMPS)
		{
			say(error, error_size, "more than 64 bot jumps", NULL);
			return 0;
		}
		jump.line = line_number;
		out->bot_jumps[out->bot_jump_count++] = jump;
		return 1;
	}
	if (same_word(words[1], "avoid"))
	{
		struct ae_layout_bot_avoid avoid;

		memset(&avoid, 0, sizeof(avoid));
		if (count != 7 || !real_of(words[2], &avoid.x) || !real_of(words[3], &avoid.y) || !real_of(words[4], &avoid.z) ||
			!same_word(words[5], "radius") || !real_of(words[6], &avoid.radius))
		{
			say(error, error_size, "bot avoid: wants <x> <y> <z> radius <r>", count > 2 ? words[2] : NULL);
			return 0;
		}
		if (!(avoid.radius > 0.0f) || avoid.radius > AE_LAYOUT_BOT_AVOID_LARGEST)
		{
			say(error, error_size, "bot avoid: the radius must be more than 0 and at most 50", words[6]);
			return 0;
		}
		if (out->bot_avoid_count == AE_LAYOUT_MAXIMUM_BOT_AVOIDS)
		{
			say(error, error_size, "more than 64 bot avoids", NULL);
			return 0;
		}
		avoid.line = line_number;
		out->bot_avoids[out->bot_avoid_count++] = avoid;
		return 1;
	}
	say(error, error_size, "bot: not jump or avoid", words[1]);
	return 0;
}

int ae_layout_parse(char const *text, long length, struct ae_layout *out, char *error, int error_size)
{
	return ae_layout_parse_for(text, length, FALSE, out, error, error_size);
}

int ae_layout_parse_for(char const *text, long length, int snd, struct ae_layout *out, char *error, int error_size)
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
		/* (snd <line>: the line only in a SEARCH AND DESTROY game, read as the rest of it; else left out unread) */
		if (header && same_word(words[0], "snd"))
		{
			if (count < 2)
			{
				say(error, error_size, "snd: a line must follow it", NULL);
				goto refused;
			}
			if (!snd)
				continue;
			memmove(words, words + 1, (size_t)(count - 1) * sizeof(words[0]));
			count--;
		}
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
		else if (count == 2 && same_word(words[0], "clear") && same_word(words[1], "spawns"))
		{
			out->clear_spawns = TRUE;
		}
		else if (count == 2 && same_word(words[0], "clear") && same_word(words[1], "flags"))
		{
			out->clear_flags = TRUE;
		}
		else if (count >= 2 && same_word(words[0], "remove") && (same_word(words[1], "spawn") || same_word(words[1], "flag")))
		{
			boolean spawn = same_word(words[1], "spawn");
			long number;

			if (count != 3 || !whole_of(words[2], 0, 65535, &number))
			{
				say(error, error_size, spawn ? "remove spawn: not a spawn number" : "remove flag: not a flag number",
					count > 2 ? words[2] : NULL);
				goto refused;
			}
			if (spawn ? out->removed_spawn_count == AE_LAYOUT_MAXIMUM_SPAWNS :
				out->removed_flag_count == AE_LAYOUT_MAXIMUM_FLAGS)
			{
				say(error, error_size, spawn ? "more than 256 spawns removed" : "more than 64 flags removed", NULL);
				goto refused;
			}
			if (spawn)
				out->removed_spawns[out->removed_spawn_count++] = number;
			else
				out->removed_flags[out->removed_flag_count++] = number;
		}
		else if (same_word(words[0], "spawn"))
		{
			if (out->spawn_count == AE_LAYOUT_MAXIMUM_SPAWNS)
			{
				say(error, error_size, "more than 256 spawns", NULL);
				goto refused;
			}
			if (!parse_spawn(words, count, &out->spawns[out->spawn_count], error, error_size))
				goto refused;
			out->spawns[out->spawn_count++].line = (short)line_number;
		}
		else if (same_word(words[0], "flag"))
		{
			if (out->flag_count == AE_LAYOUT_MAXIMUM_FLAGS)
			{
				say(error, error_size, "more than 64 flags", NULL);
				goto refused;
			}
			if (!parse_flag(words, count, &out->flags[out->flag_count], error, error_size))
				goto refused;
			out->flags[out->flag_count++].line = (short)line_number;
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
		else if (count >= 2 && same_word(words[0], "remove") &&
			(same_word(words[1], "scenery") || same_word(words[1], "object")))
		{
			short type = _ae_layout_object_scenery;
			int at = 2;

			if (out->object_rule_count == AE_LAYOUT_MAXIMUM_OBJECT_RULES)
			{
				say(error, error_size, "more than 32 scenery and object lines", NULL);
				goto refused;
			}
			if (same_word(words[1], "object"))
			{
				if (count < 3)
				{
					say(error, error_size, "remove object: which type?", NULL);
					goto refused;
				}
				for (type = 0; type < NUMBER_OF_AE_LAYOUT_OBJECT_TYPES; type++)
					if (same_word(words[2], object_type_names[type]))
						break;
				if (type == NUMBER_OF_AE_LAYOUT_OBJECT_TYPES)
				{
					say(error, error_size, "remove object: not a type (scenery machine control light_fixture "
						"sound_scenery biped weapon equipment)", words[2]);
					goto refused;
				}
				at = 3;
			}
			if (!parse_object(words, count, at, type, &out->object_rules[out->object_rule_count], error, error_size))
				goto refused;
			out->object_rules[out->object_rule_count++].line = (short)line_number;
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
		else if (count >= 2 && same_word(words[0], "bot"))
		{
			if (!parse_bot(words, count, out, (short)line_number, error, error_size))
				goto refused;
		}
		else if (same_word(words[0], "jump"))
		{
			long percent;

			if (count != 2 || !whole_of(words[1], AE_LAYOUT_JUMP_LOWEST, AE_LAYOUT_JUMP_HIGHEST, &percent))
			{
				say(error, error_size, "jump: wants a percent from 50 to 200", count > 1 ? words[1] : NULL);
				goto refused;
			}
			if (out->jump_percent)
			{
				say(error, error_size, "jump: given twice", NULL);
				goto refused;
			}
			out->jump_percent = (short)percent;
		}
		else if (same_word(words[0], "vehicle"))
		{
			if (out->vehicle_count == AE_LAYOUT_MAXIMUM_VEHICLES)
			{
				say(error, error_size, "more than 32 vehicles", NULL);
				goto refused;
			}
			if (!parse_vehicle_add(words, count, &out->vehicles[out->vehicle_count], error, error_size))
				goto refused;
			out->vehicles[out->vehicle_count++].line = (short)line_number;
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
