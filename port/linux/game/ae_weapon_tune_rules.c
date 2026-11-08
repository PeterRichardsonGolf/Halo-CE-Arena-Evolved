/*
AE_WEAPON_TUNE_RULES.C

DEV MODE's tune keys, their recipe field paths and their engine limits (ae_weapon_tune_rules.h). No engine includes.
Key scheme: Halo Content Evolved (Watanoge/halo-content-evolved, AGPL-3.0-or-later).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_weapon_tune_rules.h"

#define FLOAT_MAXIMUM 3.4028234663852886e38
#define SHORT_MAXIMUM 32767.0

/* what one key text is: the word before the number, the number's presence and the word after the dot */
static struct
{
	enum ae_tune_field field;
	char const *prefix;
	int numbered;
	char const *name;
	enum ae_tune_tag_kind kind;
	int bounds;
	int count;
	char const *path_format;
	char const *second_format;
} const keys[AE_TUNE_FIELD_COUNT] =
{
	{ AE_TUNE_TRIGGER_RATE, "trigger", 1, "rate", AE_TUNE_IN_WEAPON, 1, 0, "triggers[%d].maximum_rate_of_fire", 0 },
	{ AE_TUNE_TRIGGER_SPREAD, "trigger", 1, "spread", AE_TUNE_IN_WEAPON, 1, 0, "triggers[%d].error_angle", 0 },
	{ AE_TUNE_TRIGGER_PROJECTILES, "trigger", 1, "projectiles", AE_TUNE_IN_WEAPON, 0, 1,
		"triggers[%d].projectiles_per_shot", 0 },
	{ AE_TUNE_TRIGGER_ROUNDS, "trigger", 1, "rounds", AE_TUNE_IN_WEAPON, 0, 1, "triggers[%d].rounds_per_shot", 0 },
	{ AE_TUNE_MAGAZINE_SIZE, "magazine", 1, "size", AE_TUNE_IN_WEAPON, 0, 1, "magazines[%d].rounds_loaded_maximum", 0 },
	{ AE_TUNE_MAGAZINE_TOTAL, "magazine", 1, "total", AE_TUNE_IN_WEAPON, 0, 1, "magazines[%d].rounds_reserved_maximum", 0 },
	{ AE_TUNE_MAGAZINE_RELOAD, "magazine", 1, "reload", AE_TUNE_IN_WEAPON, 0, 0, "magazines[%d].reload_time", 0 },
	{ AE_TUNE_PROJECTILE_SPEED, "projectile", 1, "speed", AE_TUNE_IN_PROJECTILE, 0, 0, "initial_velocity",
		"final_velocity" },
	{ AE_TUNE_IMPACT_DAMAGE, "projectile", 1, "impact.damage", AE_TUNE_IN_IMPACT_EFFECT, 1, 0, "damage_upper_bound", 0 },
	{ AE_TUNE_MELEE_DAMAGE, "melee", 0, "damage", AE_TUNE_IN_MELEE_EFFECT, 1, 0, "damage_upper_bound", 0 },
	{ AE_TUNE_ZOOM, "weapon", 0, "zoom", AE_TUNE_IN_WEAPON, 1, 0, "zoom_magnification_range", 0 },
	{ AE_TUNE_AUTOAIM, "weapon", 0, "autoaim", AE_TUNE_IN_WEAPON, 0, 0, "autoaim_angle", 0 },
};

int ae_tune_key_parse(char const *text, struct ae_tune_key *out)
{
	int i;

	if (!text || !out)
		return 0;
	for (i = 0; i < AE_TUNE_FIELD_COUNT; i++)
	{
		size_t prefix = strlen(keys[i].prefix);
		char const *at = text;
		int index = 0;

		if (strncmp(at, keys[i].prefix, prefix))
			continue;
		at += prefix;
		if (keys[i].numbered)
		{
			/* (one digit: a weapon has two triggers and two magazines) */
			if (*at < '0' || *at > '9')
				continue;
			index = *at++ - '0';
		}
		if (*at != '.' || strcmp(at + 1, keys[i].name))
			continue;
		out->field = keys[i].field;
		out->index = (short)index;
		return 1;
	}
	return 0;
}

void ae_tune_key_name(struct ae_tune_key key, char *out, int size)
{
	if (size <= 0)
		return;
	out[0] = 0;
	if (key.field < 0 || key.field >= AE_TUNE_FIELD_COUNT)
		return;
	if (keys[key.field].numbered)
		snprintf(out, (size_t)size, "%s%d.%s", keys[key.field].prefix, key.index, keys[key.field].name);
	else
		snprintf(out, (size_t)size, "%s.%s", keys[key.field].prefix, keys[key.field].name);
}

int ae_tune_key_is_bounds(struct ae_tune_key key)
{
	return key.field >= 0 && key.field < AE_TUNE_FIELD_COUNT && keys[key.field].bounds;
}

int ae_tune_key_is_count(struct ae_tune_key key)
{
	return key.field >= 0 && key.field < AE_TUNE_FIELD_COUNT && keys[key.field].count;
}

enum ae_tune_tag_kind ae_tune_key_tag_kind(struct ae_tune_key key)
{
	return key.field >= 0 && key.field < AE_TUNE_FIELD_COUNT ? keys[key.field].kind : AE_TUNE_IN_WEAPON;
}

static int path_from(char const *format, struct ae_tune_key key, char *out, int size)
{
	if (size <= 0 || !format)
		return 0;
	/* (a format with a %d takes the key's number; the others have none) */
	if (strchr(format, '%'))
		snprintf(out, (size_t)size, format, key.index);
	else
		snprintf(out, (size_t)size, "%s", format);
	return 1;
}

int ae_tune_invader_path(struct ae_tune_key key, char *out, int size)
{
	if (size > 0)
		out[0] = 0;
	if (key.field < 0 || key.field >= AE_TUNE_FIELD_COUNT)
		return 0;
	return path_from(keys[key.field].path_format, key, out, size);
}

int ae_tune_invader_path_second(struct ae_tune_key key, char *out, int size)
{
	if (size > 0)
		out[0] = 0;
	if (key.field < 0 || key.field >= AE_TUNE_FIELD_COUNT)
		return 0;
	return path_from(keys[key.field].second_format, key, out, size);
}

/* a number's text, the whole of it: 1 and the number, or 0 */
static int number(char const *text, size_t length, double *out)
{
	char buffer[64];
	char *end;

	if (!length || length >= sizeof buffer)
		return 0;
	memcpy(buffer, text, length);
	buffer[length] = 0;
	/* (strtod takes "inf", "nan" and hex: the limits refuse the first two; hex and spaces are no number here) */
	if (buffer[0] == ' ' || (buffer[0] == '0' && (buffer[1] == 'x' || buffer[1] == 'X')))
		return 0;
	*out = strtod(buffer, &end);
	return *end == 0 && end != buffer;
}

int ae_tune_value_parse(struct ae_tune_key key, char const *text, struct ae_tune_value *out)
{
	char const *dots;

	if (!text || !out || key.field < 0 || key.field >= AE_TUNE_FIELD_COUNT)
		return 0;
	dots = strstr(text, "..");
	if (!dots)
	{
		if (!number(text, strlen(text), &out->low))
			return 0;
		out->high = out->low;
		return 1;
	}
	if (!ae_tune_key_is_bounds(key))
		return 0;
	return number(text, (size_t)(dots - text), &out->low) && number(dots + 2, strlen(dots + 2), &out->high);
}

int ae_tune_value_allowed(struct ae_tune_key key, double value)
{
	if (key.field < 0 || key.field >= AE_TUNE_FIELD_COUNT)
		return 0;
	/* NaN is the one number not equal to itself */
	if (value != value || value > FLOAT_MAXIMUM || value < -FLOAT_MAXIMUM)
		return 0;
	if (keys[key.field].count)
		return value >= 0.0 && value <= SHORT_MAXIMUM && value == (double)(long)value;
	if (key.field == AE_TUNE_TRIGGER_RATE || key.field == AE_TUNE_PROJECTILE_SPEED)
		return value >= 0.0;
	return 1;
}

void ae_tune_value_text(struct ae_tune_key key, struct ae_tune_value value, char *out, int size)
{
	if (size <= 0)
		return;
	if (ae_tune_key_is_bounds(key))
		snprintf(out, (size_t)size, "%.9g %.9g", value.low, value.high);
	else
		snprintf(out, (size_t)size, "%.9g", value.low);
}

void ae_tune_safe_name(char const *text, char *out, int size)
{
	int used = 0;

	if (size <= 0)
		return;
	for (; text && *text && used < size - 1 && used < AE_TUNE_NAME_MOST; text++)
	{
		char c = *text;

		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		out[used++] = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ? c : '_';
	}
	out[used] = 0;
	if (!used)
		snprintf(out, (size_t)size, "%s", "weapon");
}

int ae_tune_json_escape(char const *text, char *out, int size)
{
	int used = 0;

	if (size <= 0)
		return 0;
	for (; text && *text; text++)
	{
		unsigned char c = (unsigned char)*text;
		char piece[8];
		int length;

		if (c == '"' || c == '\\')
			length = snprintf(piece, sizeof piece, "\\%c", c);
		else if (c < 0x20 || c >= 0x80)
			length = snprintf(piece, sizeof piece, "\\u%04x", c);
		else
		{
			piece[0] = (char)c;
			piece[1] = 0;
			length = 1;
		}
		if (used + length >= size)
		{
			out[used] = 0;
			return 0;
		}
		memcpy(out + used, piece, (size_t)length);
		used += length;
	}
	out[used] = 0;
	return 1;
}

int ae_tune_origin_keep(struct ae_tune_origins *table, unsigned long where, double a, double b, double *a_out,
	double *b_out)
{
	int i;

	for (i = 0; i < table->count; i++)
		if (table->origin[i].where == where)
		{
			*a_out = table->origin[i].before[0];
			*b_out = table->origin[i].before[1];
			return 1;
		}
	if (table->count >= AE_TUNE_ORIGINS_MOST)
		return 0;
	table->origin[table->count].where = where;
	table->origin[table->count].before[0] = a;
	table->origin[table->count].before[1] = b;
	table->count++;
	*a_out = a;
	*b_out = b;
	return 1;
}

void ae_tune_origin_drop(struct ae_tune_origins *table, unsigned long where)
{
	int i;

	for (i = 0; i < table->count; i++)
		if (table->origin[i].where == where)
		{
			table->origin[i] = table->origin[--table->count];
			return;
		}
}
