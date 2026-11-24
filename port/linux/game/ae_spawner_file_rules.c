/*
AE_SPAWNER_FILE_RULES.C

The spawner sidecar reader (ae_spawner_file_rules.h).
*/

#include <string.h>

#include "ae_spawner_file_rules.h"
#include "ae_spawner_text.h"

static char const *const kind_words[AE_SPAWNER_KIND_COUNT] = { "rack", "pad", "crate", "hidden" };
static char const *const games_words[5] = { "ctf", "slayer", "oddball", "king", "race" };
static char const *const sets_words[3] = { "casual", "comp", "btb" };

static int same(char const *a, char const *b)
{
	return ae_spawner_text_same(a, b);
}

char const *ae_spawner_kind_word(int kind)
{
	return kind >= 0 && kind < AE_SPAWNER_KIND_COUNT ? kind_words[kind] : "";
}

int ae_spawner_kind_of_word(char const *word)
{
	int index;

	for (index = 0; index < AE_SPAWNER_KIND_COUNT; index++)
		if (same(kind_words[index], word))
			return index;
	return -1;
}

int ae_spawner_kind_tier(int kind)
{
	if (kind == AE_SPAWNER_RACK) return AE_SPAWNER_BASE;
	if (kind == AE_SPAWNER_PAD) return AE_SPAWNER_POWER;
	return AE_SPAWNER_TIER_ANY;
}

unsigned long ae_spawner_games_bit(char const *word)
{
	int index;

	if (same(word, "all")) return AE_SPAWNER_GAMES_ALL;
	for (index = 0; index < 5; index++)
		if (same(games_words[index], word))
			return 1ul << index;
	return 0;
}

unsigned long ae_spawner_sets_bit(char const *word)
{
	int index;

	if (same(word, "custom")) return 1ul << 15;
	for (index = 0; index < 3; index++)
		if (same(sets_words[index], word))
			return 1ul << index;
	return 0;
}

/* the options of a retag or add line from words[at..count-1]: 1 ok, 0 refused (error says) */
static int parse_options(char **words, int count, int at, struct ae_spawner_edit *edit, char *error, int error_size)
{
	long number;

	while (at < count)
	{
		char const *key = words[at];

		if (same(key, "sets") || same(key, "games"))
		{
			unsigned long bits = 0;
			int taken = 0;

			for (at++; at < count; at++, taken++)
			{
				unsigned long bit = same(key, "sets") ? ae_spawner_sets_bit(words[at]) : ae_spawner_games_bit(words[at]);

				if (!bit) break;
				bits |= bit;
			}
			if (!taken)
			{
				ae_spawner_text_say(error, error_size, "spawner: no known names after", key);
				return 0;
			}
			if (same(key, "sets")) { edit->sets = bits; edit->has |= AE_SPAWNER_EDIT_HAS_SETS; }
			else { edit->games = bits; edit->has |= AE_SPAWNER_EDIT_HAS_GAMES; }
			continue;
		}
		if (at + 1 >= count)
		{
			ae_spawner_text_say(error, error_size, "spawner: a value is missing after", key);
			return 0;
		}
		if (same(key, "kind"))
		{
			edit->kind = (signed char)ae_spawner_kind_of_word(words[at + 1]);
			if (edit->kind < 0) goto bad;
			edit->has |= AE_SPAWNER_EDIT_HAS_KIND;
		}
		else if (same(key, "class"))
		{
			edit->cls = (signed char)ae_spawner_class_of_word(words[at + 1]);
			if (edit->cls < 0) goto bad;
			edit->has |= AE_SPAWNER_EDIT_HAS_CLASS;
		}
		else if (same(key, "tier"))
		{
			edit->tier = (signed char)ae_spawner_tier_of_word(words[at + 1]);
			if (edit->tier < 0) goto bad;
			edit->has |= AE_SPAWNER_EDIT_HAS_TIER;
		}
		else if (same(key, "logic"))
		{
			edit->logic = (signed char)ae_spawner_logic_of_word(words[at + 1]);
			if (edit->logic < 0) goto bad;
			edit->has |= AE_SPAWNER_EDIT_HAS_LOGIC;
		}
		else if (same(key, "respawn"))
		{
			if (!ae_spawner_text_whole(words[at + 1], 0, AE_SPAWNER_FILE_RESPAWN_MAXIMUM, &number)) goto bad;
			edit->respawn = (short)ae_spawner_respawn_seconds(number);
			edit->has |= AE_SPAWNER_EDIT_HAS_RESPAWN;
		}
		else if (same(key, "delay"))
		{
			if (!ae_spawner_text_whole(words[at + 1], 0, AE_SPAWNER_FILE_RESPAWN_MAXIMUM, &number)) goto bad;
			edit->delay = (short)number;
			edit->has |= AE_SPAWNER_EDIT_HAS_DELAY;
		}
		else if (same(key, "channel"))
		{
			if (!ae_spawner_text_name(words[at + 1], edit->channel, sizeof edit->channel)) goto bad;
			edit->has |= AE_SPAWNER_EDIT_HAS_CHANNEL;
		}
		else
		{
			ae_spawner_text_say(error, error_size, "spawner: an unknown word", key);
			return 0;
		}
		at += 2;
		continue;
	bad:
		ae_spawner_text_say(error, error_size, "spawner: a bad value for", key);
		return 0;
	}
	return 1;
}

static struct ae_spawner_edit *new_edit(struct ae_spawner_file *out, int kind, char *error, int error_size)
{
	struct ae_spawner_edit *edit;

	if (out->edit_count == AE_SPAWNER_FILE_EDITS_MAXIMUM)
	{
		ae_spawner_text_say(error, error_size, "more than 256 spawner lines", NULL);
		return 0;
	}
	edit = &out->edits[out->edit_count];
	memset(edit, 0, sizeof(*edit));
	edit->edit = (signed char)kind;
	edit->n = -1;
	edit->kind = AE_SPAWNER_KIND_NONE;
	edit->cls = edit->tier = edit->logic = -1;
	edit->sets = AE_SPAWNER_SETS_ALL;
	edit->games = AE_SPAWNER_GAMES_ALL;
	return edit;
}

/* spawner ... (words[0] = spawner) */
static int parse_spawner(char **words, int count, struct ae_spawner_file *out, char *error, int error_size)
{
	struct ae_spawner_edit *edit;
	long n;
	int at;

	if (count < 2)
	{
		ae_spawner_text_say(error, error_size, "spawner: what?", NULL);
		return 0;
	}
	if (same(words[1], "skip"))
	{
		if (count != 3 || !ae_spawner_text_whole(words[2], 0, 4095, &n))
		{
			ae_spawner_text_say(error, error_size, "spawner skip <n>", NULL);
			return 0;
		}
		if (!(edit = new_edit(out, AE_SPAWNER_EDIT_SKIP, error, error_size))) return 0;
		edit->n = (short)n;
		out->edit_count++;
		return 1;
	}
	if (same(words[1], "at"))
	{
		real x, y, z, facing = 0;
		boolean has_facing = FALSE;

		if (count < 5 || !ae_spawner_text_real(words[2], &x) || !ae_spawner_text_real(words[3], &y) ||
			!ae_spawner_text_real(words[4], &z))
		{
			ae_spawner_text_say(error, error_size, "spawner at <x> <y> <z> ...", NULL);
			return 0;
		}
		at = 5;
		if (at + 1 < count && same(words[at], "facing"))
		{
			if (!ae_spawner_text_real(words[at + 1], &facing) || facing < -360.0f || facing > 360.0f)
			{
				ae_spawner_text_say(error, error_size, "spawner at: a bad facing", words[at + 1]);
				return 0;
			}
			has_facing = TRUE;
			at += 2;
		}
		if (!(edit = new_edit(out, AE_SPAWNER_EDIT_ADD, error, error_size))) return 0;
		edit->position[0] = x;
		edit->position[1] = y;
		edit->position[2] = z;
		edit->facing = facing;
		if (has_facing) edit->has |= AE_SPAWNER_EDIT_HAS_FACING;
		if (!parse_options(words, count, at, edit, error, error_size)) return 0;
		if (!(edit->has & AE_SPAWNER_EDIT_HAS_KIND) || !(edit->has & AE_SPAWNER_EDIT_HAS_CLASS) ||
			!(edit->has & AE_SPAWNER_EDIT_HAS_TIER))
		{
			ae_spawner_text_say(error, error_size, "spawner at: kind, class and tier are needed", NULL);
			return 0;
		}
		out->edit_count++;
		return 1;
	}
	if (!ae_spawner_text_whole(words[1], 0, 4095, &n))
	{
		ae_spawner_text_say(error, error_size, "spawner: an item number is needed, or skip, or at", words[1]);
		return 0;
	}
	if (count == 3 && same(words[2], "split"))
	{
		if (!(edit = new_edit(out, AE_SPAWNER_EDIT_SPLIT, error, error_size))) return 0;
		edit->n = (short)n;
		out->edit_count++;
		return 1;
	}
	if (count >= 3 && same(words[2], "weapon"))
	{
		char weapon[AE_SPAWNER_FILE_REF_SIZE];

		/* (the name read into a buffer of its own: the edit is made only when the list has room) */
		if (count != 4 || !ae_spawner_text_name(words[3], weapon, AE_SPAWNER_FILE_REF_SIZE))
		{
			ae_spawner_text_say(error, error_size, "spawner <n> weapon <ref>", NULL);
			return 0;
		}
		if (!(edit = new_edit(out, AE_SPAWNER_EDIT_PIN, error, error_size))) return 0;
		strcpy(edit->weapon, weapon);
		edit->n = (short)n;
		out->edit_count++;
		return 1;
	}
	if (count < 3)
	{
		ae_spawner_text_say(error, error_size, "spawner <n>: an option is needed", NULL);
		return 0;
	}
	if (!(edit = new_edit(out, AE_SPAWNER_EDIT_RETAG, error, error_size))) return 0;
	edit->n = (short)n;
	if (!parse_options(words, count, 2, edit, error, error_size)) return 0;
	out->edit_count++;
	return 1;
}

int ae_spawner_file_read(char const *text, long length, struct ae_spawner_file *out, char *error, int error_size)
{
	struct ae_spawner_text reader;
	boolean header = FALSE, format = FALSE;
	int result;

	memset(out, 0, sizeof(*out));
	out->auto_convert = 1;
	if (error && error_size > 0) error[0] = 0;
	ae_spawner_text_begin(&reader, text, length);
	while ((result = ae_spawner_text_next(&reader, error, error_size)) == 1)
	{
		char **words = reader.words;
		int count = reader.count;
		long number;

		if (!header)
		{
			if (count != 4 || !same(words[0], "spawners") || !ae_spawner_text_whole(words[1], 1, 1, &number) ||
				strlen(words[2]) >= AE_SPAWNER_FILE_MAP_SIZE || !words[2][0] || !ae_spawner_text_hash(words[3], &out->hash))
			{
				ae_spawner_text_say(error, error_size, "the first line must be: spawners 1 <map file> <hash8>", NULL);
				return reader.line_number;
			}
			strcpy(out->map, words[2]);
			header = TRUE;
			continue;
		}
		if (same(words[0], "format"))
		{
			if (count != 2 || !ae_spawner_text_whole(words[1], AE_SPAWNER_FILE_VERSION, AE_SPAWNER_FILE_VERSION, &number))
			{
				ae_spawner_text_say(error, error_size, "format: only 1 is known", NULL);
				return reader.line_number;
			}
			format = TRUE;
			continue;
		}
		if (same(words[0], "auto"))
		{
			if (count != 2 || !(same(words[1], "on") || same(words[1], "off")))
			{
				ae_spawner_text_say(error, error_size, "auto: on or off", NULL);
				return reader.line_number;
			}
			out->auto_convert = same(words[1], "on");
			continue;
		}
		if (same(words[0], "spawner"))
		{
			if (!parse_spawner(words, count, out, error, error_size)) return reader.line_number;
			continue;
		}
		if (same(words[0], "channel"))
		{
			if (count != 3 || !same(words[2], "symmetrical") ||
				out->symmetrical_count == AE_SPAWNER_FILE_CHANNELS_MAXIMUM ||
				!ae_spawner_text_name(words[1], out->symmetrical[out->symmetrical_count], AE_SPAWNER_FILE_NAME_SIZE))
			{
				ae_spawner_text_say(error, error_size, "channel <NAME> symmetrical", NULL);
				return reader.line_number;
			}
			out->symmetrical_count++;
			continue;
		}
		ae_spawner_text_say(error, error_size, "an unknown line", words[0]);
		return reader.line_number;
	}
	if (result < 0) return reader.line_number > 0 ? reader.line_number : 1;
	if (!header)
	{
		ae_spawner_text_say(error, error_size, "no spawners line", NULL);
		return 1;
	}
	if (!format)
	{
		ae_spawner_text_say(error, error_size, "no format line", NULL);
		return 1;
	}
	return 0;
}

boolean ae_spawner_file_for_map(struct ae_spawner_file const *file, char const *map_file, unsigned long map_hash)
{
	return file && map_file && same(file->map, map_file) && file->hash == map_hash;
}

boolean ae_spawner_file_symmetrical(struct ae_spawner_file const *file, char const *channel)
{
	int index;

	if (!file || !channel) return FALSE;
	for (index = 0; index < file->symmetrical_count; index++)
		if (same(file->symmetrical[index], channel))
			return TRUE;
	return FALSE;
}
