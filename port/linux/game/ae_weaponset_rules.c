/*
AE_WEAPONSET_RULES.C

The weapon set reader and its rules (ae_weaponset_rules.h).
*/

#include <string.h>

#include "ae_weaponset_rules.h"
#include "ae_spawner_text.h"

/* the built-in sets' texts (generated: tools/weapon_cache/weaponsets.py gen) */
#define AE_WEAPONSET_TEXT(name, text) { name, text },
static struct { char const *name; char const *text; } const builtin[] =
{
#include "ae_weaponsets.inc"
};
#undef AE_WEAPONSET_TEXT
#define BUILTIN_COUNT ((int)(sizeof(builtin) / sizeof(builtin[0])))

static int same(char const *a, char const *b)
{
	return ae_spawner_text_same(a, b);
}

static int class_or_any(char const *word, int *cls)
{
	*cls = ae_spawner_class_of_word(word);
	return *cls >= 0;
}

static int tier_or_any(char const *word, int *tier)
{
	*tier = ae_spawner_tier_of_word(word);
	return *tier >= 0;
}

static int ref_of(char const *word, char *out, int size, unsigned char *weight)
{
	char copy[AE_WEAPONSET_REF_SIZE + 4];
	char *star;
	long number = 1;

	if (!word || strlen(word) >= sizeof copy) return 0;
	strcpy(copy, word);
	star = strchr(copy, '*');
	if (star)
	{
		*star = 0;
		if (!ae_spawner_text_whole(star + 1, 1, AE_WEAPONSET_WEIGHT_MAXIMUM, &number)) return 0;
	}
	if (!ae_spawner_text_name(copy, out, size)) return 0;
	*weight = (unsigned char)number;
	return 1;
}

/* a field/value pair of a row or an override: 1 taken (two words), 0 not a field, -1 a bad value */
static int field_of(char const *field, char const *value, unsigned char *fields, signed char *logic, short *seconds,
	short *delay, short *ammo)
{
	long number;

	if (same(field, "logic"))
	{
		int l = ae_spawner_logic_of_word(value);

		if (l < 0) return -1;
		*logic = (signed char)l;
		*fields |= AE_WEAPONSET_FIELD_LOGIC;
		return 1;
	}
	if (same(field, "respawn"))
	{
		if (!ae_spawner_text_whole(value, 0, AE_WEAPONSET_RESPAWN_MAXIMUM, &number)) return -1;
		*seconds = (short)ae_spawner_respawn_seconds(number);
		*fields |= AE_WEAPONSET_FIELD_RESPAWN;
		return 1;
	}
	if (same(field, "delay"))
	{
		if (!ae_spawner_text_whole(value, 0, AE_WEAPONSET_RESPAWN_MAXIMUM, &number)) return -1;
		*delay = (short)number;
		*fields |= AE_WEAPONSET_FIELD_DELAY;
		return 1;
	}
	if (same(field, "ammo"))
	{
		if (!ae_spawner_text_whole(value, 0, 100, &number)) return -1;
		*ammo = (short)number;
		*fields |= AE_WEAPONSET_FIELD_AMMO;
		return 1;
	}
	return 0;
}

/* row <CLASS> <TIER> mute | logic <L> [respawn S] [delay S] [ammo P] [channel N] weapons <ref>... */
static int parse_row(char **words, int count, struct ae_weaponset *set, char *error, int error_size)
{
	struct ae_weaponset_row *row;
	int cls, tier, at = 3, index;
	unsigned char fields = 0;

	if (count < 4 || !class_or_any(words[1], &cls) || !tier_or_any(words[2], &tier))
	{
		ae_spawner_text_say(error, error_size, "row: a class and a tier are needed", NULL);
		return 0;
	}
	for (index = 0; index < set->row_count; index++)
		if (set->rows[index].cls == cls && set->rows[index].tier == tier)
			break;
	if (index == set->row_count)
	{
		if (set->row_count == AE_WEAPONSET_ROWS_MAXIMUM)
		{
			ae_spawner_text_say(error, error_size, "too many rows", NULL);
			return 0;
		}
		set->row_count++;
	}
	row = &set->rows[index];
	memset(row, 0, sizeof(*row));
	row->cls = (signed char)cls;
	row->tier = (signed char)tier;
	row->logic = -1;
	if (same(words[3], "mute"))
	{
		if (count != 4)
		{
			ae_spawner_text_say(error, error_size, "row: nothing goes after mute", NULL);
			return 0;
		}
		row->mute = 1;
		return 1;
	}
	while (at < count)
	{
		int taken;

		if (same(words[at], "weapons")) break;
		if (at + 1 >= count)
		{
			ae_spawner_text_say(error, error_size, "row: a value is missing after", words[at]);
			return 0;
		}
		if (same(words[at], "channel"))
		{
			if (!ae_spawner_text_name(words[at + 1], row->channel, sizeof row->channel))
			{
				ae_spawner_text_say(error, error_size, "row: a bad channel name", words[at + 1]);
				return 0;
			}
			at += 2;
			continue;
		}
		taken = field_of(words[at], words[at + 1], &fields, &row->logic, &row->seconds, &row->delay, &row->ammo_percent);
		if (taken < 0)
		{
			ae_spawner_text_say(error, error_size, "row: a bad value for", words[at]);
			return 0;
		}
		if (!taken)
		{
			ae_spawner_text_say(error, error_size, "row: an unknown word", words[at]);
			return 0;
		}
		at += 2;
	}
	if (!(fields & AE_WEAPONSET_FIELD_LOGIC))
	{
		ae_spawner_text_say(error, error_size, "row: logic is needed", NULL);
		return 0;
	}
	if (row->logic == AE_SPAWNER_TRIGGER)
	{
		if (!row->channel[0])
		{
			ae_spawner_text_say(error, error_size, "row: a trigger row needs a channel", NULL);
			return 0;
		}
	}
	else if (!(fields & AE_WEAPONSET_FIELD_RESPAWN))
	{
		ae_spawner_text_say(error, error_size, "row: respawn is needed", NULL);
		return 0;
	}
	if (at >= count || !same(words[at], "weapons") || at + 1 >= count)
	{
		ae_spawner_text_say(error, error_size, "row: weapons are needed", NULL);
		return 0;
	}
	for (at++; at < count; at++)
	{
		if (row->weapon_count == AE_WEAPONSET_POOL_MAXIMUM)
		{
			ae_spawner_text_say(error, error_size, "row: more than 12 weapons", NULL);
			return 0;
		}
		if (!ref_of(words[at], row->weapons[row->weapon_count], AE_WEAPONSET_REF_SIZE, &row->weights[row->weapon_count]))
		{
			ae_spawner_text_say(error, error_size, "row: a bad weapon", words[at]);
			return 0;
		}
		row->weapon_count++;
	}
	return 1;
}

/* channel <NAME> every <S> variance <pct> */
static int parse_channel(char **words, int count, struct ae_weaponset *set, char *error, int error_size)
{
	struct ae_weaponset_channel *channel;
	long every, variance;
	int index;

	if (count != 6 || !same(words[2], "every") || !same(words[4], "variance") ||
		!ae_spawner_text_whole(words[3], AE_SPAWNER_RESPAWN_FLOOR_SECONDS, AE_WEAPONSET_RESPAWN_MAXIMUM, &every) ||
		!ae_spawner_text_whole(words[5], 0, 90, &variance))
	{
		ae_spawner_text_say(error, error_size, "channel: <NAME> every <seconds> variance <percent>", NULL);
		return 0;
	}
	for (index = 0; index < set->channel_count; index++)
		if (same(set->channels[index].name, words[1]))
			break;
	if (index == set->channel_count)
	{
		if (set->channel_count == AE_WEAPONSET_CHANNELS_MAXIMUM)
		{
			ae_spawner_text_say(error, error_size, "too many channels", NULL);
			return 0;
		}
		set->channel_count++;
	}
	channel = &set->channels[index];
	if (!ae_spawner_text_name(words[1], channel->name, sizeof channel->name))
	{
		ae_spawner_text_say(error, error_size, "channel: a bad name", words[1]);
		return 0;
	}
	channel->every_seconds = (short)every;
	channel->variance_percent = (short)variance;
	return 1;
}

/* override global|tier <T>|class <C>|cell <C> <T> <field> <value> ... */
static int parse_override(char **words, int count, struct ae_weaponset *set, char *error, int error_size)
{
	struct ae_weaponset_override *over;
	int at, cls = -1, tier = -1, scope;

	if (count < 4)
	{
		ae_spawner_text_say(error, error_size, "override: a scope and a field are needed", NULL);
		return 0;
	}
	if (same(words[1], "global")) { scope = AE_WEAPONSET_SCOPE_GLOBAL; at = 2; }
	else if (same(words[1], "tier") && tier_or_any(words[2], &tier)) { scope = AE_WEAPONSET_SCOPE_TIER; at = 3; }
	else if (same(words[1], "class") && class_or_any(words[2], &cls)) { scope = AE_WEAPONSET_SCOPE_CLASS; at = 3; }
	else if (same(words[1], "cell") && count > 4 && class_or_any(words[2], &cls) && tier_or_any(words[3], &tier))
	{
		scope = AE_WEAPONSET_SCOPE_CELL;
		at = 4;
	}
	else
	{
		ae_spawner_text_say(error, error_size, "override: an unknown scope", words[1]);
		return 0;
	}
	if (set->override_count == AE_WEAPONSET_OVERRIDES_MAXIMUM)
	{
		ae_spawner_text_say(error, error_size, "too many overrides", NULL);
		return 0;
	}
	over = &set->overrides[set->override_count];
	memset(over, 0, sizeof(*over));
	over->scope = (signed char)scope;
	over->cls = (signed char)cls;
	over->tier = (signed char)tier;
	over->logic = -1;
	if (at >= count || (count - at) % 2)
	{
		ae_spawner_text_say(error, error_size, "override: field and value pairs are needed", NULL);
		return 0;
	}
	for (; at < count; at += 2)
	{
		int taken = field_of(words[at], words[at + 1], &over->fields, &over->logic, &over->seconds, &over->delay,
			&over->ammo_percent);

		if (taken < 0)
		{
			ae_spawner_text_say(error, error_size, "override: a bad value for", words[at]);
			return 0;
		}
		if (!taken)
		{
			ae_spawner_text_say(error, error_size, "override: an unknown field", words[at]);
			return 0;
		}
	}
	set->override_count++;
	return 1;
}

static int on_off(char const *word, unsigned char *value)
{
	if (same(word, "on")) { *value = 1; return 1; }
	if (same(word, "off")) { *value = 0; return 1; }
	return 0;
}

int ae_weaponset_read(char const *text, long length, struct ae_weaponset *out, char *error, int error_size)
{
	struct ae_spawner_text reader;
	boolean header = FALSE, format = FALSE;
	int result;

	memset(out, 0, sizeof(*out));
	out->tier_weight[AE_SPAWNER_BASE] = 70;
	out->tier_weight[AE_SPAWNER_POWER] = 30;
	out->map_time = 1;
	out->scan = 1;
	if (error && error_size > 0) error[0] = 0;
	ae_spawner_text_begin(&reader, text, length);
	while ((result = ae_spawner_text_next(&reader, error, error_size)) == 1)
	{
		char **words = reader.words;
		int count = reader.count;
		long number;

		if (!header)
		{
			if (count != 3 || !same(words[0], "weaponset") || !ae_spawner_text_whole(words[1], 1, 1, &number) ||
				!ae_spawner_text_name(words[2], out->name, sizeof out->name))
			{
				ae_spawner_text_say(error, error_size, "the first line must be: weaponset 1 <name>", NULL);
				return reader.line_number;
			}
			header = TRUE;
			continue;
		}
		if (same(words[0], "format"))
		{
			if (count != 2 || !ae_spawner_text_whole(words[1], AE_WEAPONSET_VERSION, AE_WEAPONSET_VERSION, &number))
			{
				ae_spawner_text_say(error, error_size, "format: only 1 is known", NULL);
				return reader.line_number;
			}
			format = TRUE;
			continue;
		}
		if (same(words[0], "extends"))
		{
			if (count != 2 || !ae_spawner_text_name(words[1], out->extends, sizeof out->extends) || same(out->extends, out->name))
			{
				ae_spawner_text_say(error, error_size, "extends: one other set's name", NULL);
				return reader.line_number;
			}
			continue;
		}
		if (same(words[0], "draw"))
		{
			if (count != 2 || !(same(words[1], "rotate") || same(words[1], "random")))
			{
				ae_spawner_text_say(error, error_size, "draw: rotate or random", NULL);
				return reader.line_number;
			}
			out->draw = same(words[1], "random") ? AE_WEAPONSET_RANDOM : AE_WEAPONSET_ROTATE;
			out->has |= AE_WEAPONSET_HAS_DRAW;
			continue;
		}
		if (same(words[0], "tier_weight"))
		{
			int at;

			if (count < 3 || (count - 1) % 2)
			{
				ae_spawner_text_say(error, error_size, "tier_weight: tier and weight pairs", NULL);
				return reader.line_number;
			}
			for (at = 1; at < count; at += 2)
			{
				int tier = ae_spawner_tier_of_word(words[at]);

				if (tier < 0 || tier == AE_SPAWNER_TIER_ANY || !ae_spawner_text_whole(words[at + 1], 0, 1000, &number))
				{
					ae_spawner_text_say(error, error_size, "tier_weight: a bad pair at", words[at]);
					return reader.line_number;
				}
				out->tier_weight[tier] = (short)number;
			}
			out->has |= AE_WEAPONSET_HAS_TIER_WEIGHT;
			continue;
		}
		if (same(words[0], "map_time") || same(words[0], "scan"))
		{
			unsigned char *value = same(words[0], "scan") ? &out->scan : &out->map_time;

			if (count != 2 || !on_off(words[1], value))
			{
				ae_spawner_text_say(error, error_size, "on or off is needed after", words[0]);
				return reader.line_number;
			}
			out->has |= same(words[0], "scan") ? AE_WEAPONSET_HAS_SCAN : AE_WEAPONSET_HAS_MAP_TIME;
			continue;
		}
		if (same(words[0], "rack_ammo"))
		{
			if (count != 2 || !(same(words[1], "fresh") || same(words[1], "topup")))
			{
				ae_spawner_text_say(error, error_size, "rack_ammo: fresh or topup", NULL);
				return reader.line_number;
			}
			out->rack_ammo_fresh = same(words[1], "fresh");
			out->has |= AE_WEAPONSET_HAS_RACK_AMMO;
			continue;
		}
		if (same(words[0], "rules"))
		{
			if (count != 2 || !(same(words[1], "ae") || same(words[1], "reference")))
			{
				ae_spawner_text_say(error, error_size, "rules: ae or reference", NULL);
				return reader.line_number;
			}
			out->rules_reference = same(words[1], "reference");
			out->has |= AE_WEAPONSET_HAS_RULES;
			continue;
		}
		if (same(words[0], "swap"))
		{
			unsigned char weight;

			if (count != 3 || out->swap_count == AE_WEAPONSET_SWAPS_MAXIMUM ||
				!ref_of(words[1], out->swaps[out->swap_count].from, AE_WEAPONSET_REF_SIZE, &weight) || weight != 1 ||
				!ref_of(words[2], out->swaps[out->swap_count].to, AE_WEAPONSET_REF_SIZE, &weight) || weight != 1)
			{
				ae_spawner_text_say(error, error_size, "swap: two weapons", NULL);
				return reader.line_number;
			}
			out->swap_count++;
			continue;
		}
		if (same(words[0], "row"))
		{
			if (!parse_row(words, count, out, error, error_size)) return reader.line_number;
			continue;
		}
		if (same(words[0], "channel"))
		{
			if (!parse_channel(words, count, out, error, error_size)) return reader.line_number;
			continue;
		}
		if (same(words[0], "override"))
		{
			if (!parse_override(words, count, out, error, error_size)) return reader.line_number;
			continue;
		}
		ae_spawner_text_say(error, error_size, "an unknown line", words[0]);
		return reader.line_number;
	}
	if (result < 0) return reader.line_number > 0 ? reader.line_number : 1;
	if (!header)
	{
		ae_spawner_text_say(error, error_size, "no weaponset line", NULL);
		return 1;
	}
	if (!format)
	{
		ae_spawner_text_say(error, error_size, "no format line", NULL);
		return 1;
	}
	return 0;
}

int ae_weaponset_extend(struct ae_weaponset *child, struct ae_weaponset const *parent)
{
	int index, k;

	if (!child || !parent || !child->extends[0] || !same(child->extends, parent->name)) return 0;
	for (index = 0; index < parent->row_count; index++)
	{
		struct ae_weaponset_row const *row = &parent->rows[index];

		for (k = 0; k < child->row_count; k++)
			if (child->rows[k].cls == row->cls && child->rows[k].tier == row->tier)
				break;
		if (k < child->row_count || child->row_count == AE_WEAPONSET_ROWS_MAXIMUM) continue;
		child->rows[child->row_count++] = *row;
	}
	for (index = 0; index < parent->channel_count; index++)
	{
		if (ae_weaponset_channel_of(child, parent->channels[index].name) || child->channel_count == AE_WEAPONSET_CHANNELS_MAXIMUM)
			continue;
		child->channels[child->channel_count++] = parent->channels[index];
	}
	for (index = 0; index < parent->swap_count; index++)
	{
		for (k = 0; k < child->swap_count; k++)
			if (same(child->swaps[k].from, parent->swaps[index].from))
				break;
		if (k < child->swap_count || child->swap_count == AE_WEAPONSET_SWAPS_MAXIMUM) continue;
		child->swaps[child->swap_count++] = parent->swaps[index];
	}
	/* the parent's overrides apply first (the child's, later in the list, win where both speak) */
	if (parent->override_count)
	{
		struct ae_weaponset_override merged[AE_WEAPONSET_OVERRIDES_MAXIMUM];
		int count = 0;

		for (index = 0; index < parent->override_count && count < AE_WEAPONSET_OVERRIDES_MAXIMUM; index++)
			merged[count++] = parent->overrides[index];
		for (index = 0; index < child->override_count && count < AE_WEAPONSET_OVERRIDES_MAXIMUM; index++)
			merged[count++] = child->overrides[index];
		memcpy(child->overrides, merged, sizeof(merged[0]) * (size_t)count);
		child->override_count = (short)count;
	}
	if (!(child->has & AE_WEAPONSET_HAS_DRAW)) child->draw = parent->draw;
	if (!(child->has & AE_WEAPONSET_HAS_TIER_WEIGHT))
		memcpy(child->tier_weight, parent->tier_weight, sizeof child->tier_weight);
	if (!(child->has & AE_WEAPONSET_HAS_MAP_TIME)) child->map_time = parent->map_time;
	if (!(child->has & AE_WEAPONSET_HAS_RACK_AMMO)) child->rack_ammo_fresh = parent->rack_ammo_fresh;
	if (!(child->has & AE_WEAPONSET_HAS_SCAN)) child->scan = parent->scan;
	if (!(child->has & AE_WEAPONSET_HAS_RULES)) child->rules_reference = parent->rules_reference;
	child->has |= parent->has;
	child->extends[0] = 0;
	return 1;
}

static struct ae_weaponset_row const *exact_row(struct ae_weaponset const *set, int cls, int tier)
{
	int index;

	for (index = 0; index < set->row_count; index++)
		if (set->rows[index].cls == cls && set->rows[index].tier == tier)
			return &set->rows[index];
	return 0;
}

struct ae_weaponset_row const *ae_weaponset_row_of(struct ae_weaponset const *set, int cls, int tier)
{
	struct ae_weaponset_row const *row;

	if (!set) return 0;
	if ((row = exact_row(set, cls, tier))) return row;
	if ((row = exact_row(set, cls, AE_SPAWNER_TIER_ANY))) return row;
	if ((row = exact_row(set, AE_SPAWNER_CLASS_ANY, tier))) return row;
	return exact_row(set, AE_SPAWNER_CLASS_ANY, AE_SPAWNER_TIER_ANY);
}

static void apply_override(struct ae_weaponset_row *row, struct ae_weaponset_override const *over)
{
	if (over->fields & AE_WEAPONSET_FIELD_LOGIC) row->logic = over->logic;
	if (over->fields & AE_WEAPONSET_FIELD_RESPAWN) row->seconds = over->seconds;
	if (over->fields & AE_WEAPONSET_FIELD_DELAY) row->delay = over->delay;
	if (over->fields & AE_WEAPONSET_FIELD_AMMO) row->ammo_percent = over->ammo_percent;
}

int ae_weaponset_cell(struct ae_weaponset const *set, int cls, int tier, struct ae_weaponset_row *out)
{
	struct ae_weaponset_row const *row = ae_weaponset_row_of(set, cls, tier);
	int scope, index;

	if (!row) return 0;
	*out = *row;
	if (out->mute) return 1;
	for (scope = AE_WEAPONSET_SCOPE_GLOBAL; scope <= AE_WEAPONSET_SCOPE_CELL; scope++)
		for (index = 0; index < set->override_count; index++)
		{
			struct ae_weaponset_override const *over = &set->overrides[index];

			if (over->scope != scope) continue;
			if (scope == AE_WEAPONSET_SCOPE_TIER && over->tier != tier) continue;
			if (scope == AE_WEAPONSET_SCOPE_CLASS && over->cls != cls) continue;
			if (scope == AE_WEAPONSET_SCOPE_CELL && (over->cls != cls || over->tier != tier)) continue;
			apply_override(out, over);
		}
	return 1;
}

char const *ae_weaponset_swapped(struct ae_weaponset const *set, char const *ref)
{
	int index;

	if (!set || !ref) return ref;
	for (index = 0; index < set->swap_count; index++)
		if (same(set->swaps[index].from, ref))
			return set->swaps[index].to;
	return ref;
}

struct ae_weaponset_channel const *ae_weaponset_channel_of(struct ae_weaponset const *set, char const *name)
{
	int index;

	if (!set || !name) return 0;
	for (index = 0; index < set->channel_count; index++)
		if (same(set->channels[index].name, name))
			return &set->channels[index];
	return 0;
}

char const *ae_weaponsets_builtin_text(char const *name)
{
	int index;

	for (index = 0; index < BUILTIN_COUNT; index++)
		if (same(builtin[index].name, name))
			return builtin[index].text;
	return 0;
}

int ae_weaponsets_builtin_id(char const *name)
{
	int index;

	for (index = 0; index < BUILTIN_COUNT; index++)
		if (same(builtin[index].name, name))
			return index;
	return -1;
}

char const *ae_weaponsets_builtin_name(int id)
{
	return id >= 0 && id < BUILTIN_COUNT ? builtin[id].name : "";
}
