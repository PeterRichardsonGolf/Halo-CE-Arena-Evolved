/*
AE_HUD_OFFSETS_RULES.C

The weapon HUD element mover: the numbers and the table (ae_hud_offsets_rules.h). No engine includes.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_hud_offsets_rules.h"

struct ae_hud_element ae_hud_element_identity(int index)
{
	struct ae_hud_element element = { 0, 0.0f, 0.0f, 1.0f, 0 };

	element.index = index;
	return element;
}

int ae_hud_element_is_identity(struct ae_hud_element const *element)
{
	return element->dx == 0.0f && element->dy == 0.0f && element->scale == 1.0f && !element->hide;
}

int ae_hud_element_valid(struct ae_hud_element const *element)
{
	return element->index >= 0 && element->index < AE_HUD_ELEMENTS && __builtin_isfinite(element->dx) &&
		__builtin_isfinite(element->dy) && fabsf(element->dx) <= AE_HUD_OFFSET_MAXIMUM &&
		fabsf(element->dy) <= AE_HUD_OFFSET_MAXIMUM && __builtin_isfinite(element->scale) &&
		element->scale >= AE_HUD_SCALE_MINIMUM && element->scale <= AE_HUD_SCALE_MAXIMUM;
}

static short rounded(float value)
{
	float result = value < 0.0f ? value - 0.5f : value + 0.5f;

	if (result > 32000.0f)
		return 32000;
	if (result < -32000.0f)
		return -32000;
	return (short)result;
}

void ae_hud_element_apply(struct ae_hud_element const *element, int corner, short *offset_x, short *offset_y,
	float *scale_x, float *scale_y)
{
	/* (a right anchor's x offset counts to the left, a bottom anchor's y offset upward) */
	float sign_x = corner >= 0 && corner < 4 && (corner & 1) ? -1.0f : 1.0f;
	float sign_y = corner >= 0 && corner < 4 && (corner & 2) ? -1.0f : 1.0f;
	float dx = element->dx, dy = element->dy;

	if (element->hide)
		dx += (float)AE_HUD_HIDE_OFFSET;
	*offset_x = rounded((float)*offset_x + dx * sign_x);
	*offset_y = rounded((float)*offset_y + dy * sign_y);
	*scale_x *= element->scale;
	*scale_y *= element->scale;
}

static int same_key(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return !*a && !*b;
}

static int find_weapon(struct ae_hud_table const *table, char const *key)
{
	int index;

	for (index = 0; index < table->count; index++)
		if (same_key(table->weapon[index].key, key))
			return index;
	return -1;
}

static int find_element(struct ae_hud_weapon const *weapon, int index)
{
	int at;

	for (at = 0; at < weapon->count; at++)
		if (weapon->element[at].index == index)
			return at;
	return -1;
}

void ae_hud_table_clear_all(struct ae_hud_table *table)
{
	memset(table, 0, sizeof *table);
}

int ae_hud_table_get(struct ae_hud_table const *table, char const *key, int index, struct ae_hud_element *out)
{
	int weapon = find_weapon(table, key), at = weapon >= 0 ? find_element(&table->weapon[weapon], index) : -1;

	*out = at >= 0 ? table->weapon[weapon].element[at] : ae_hud_element_identity(index);
	return at >= 0;
}

/* a weapon with no element left leaves the table */
static void weapon_drop_if_empty(struct ae_hud_table *table, int weapon)
{
	if (table->weapon[weapon].count == 0)
		table->weapon[weapon] = table->weapon[--table->count];
}

int ae_hud_table_set(struct ae_hud_table *table, char const *key, struct ae_hud_element const *element)
{
	int weapon, at;

	if (!key[0] || strlen(key) >= AE_HUD_KEY || strpbrk(key, " \t\r\n#") || !ae_hud_element_valid(element))
		return 0;
	weapon = find_weapon(table, key);
	if (ae_hud_element_is_identity(element))
	{
		if (weapon >= 0)
			ae_hud_table_clear(table, key, element->index);
		return 1;
	}
	if (weapon < 0)
	{
		if (table->count >= AE_HUD_WEAPONS)
			return 0;
		weapon = table->count++;
		memset(&table->weapon[weapon], 0, sizeof table->weapon[weapon]);
		snprintf(table->weapon[weapon].key, sizeof table->weapon[weapon].key, "%s", key);
	}
	at = find_element(&table->weapon[weapon], element->index);
	if (at < 0)
	{
		if (table->weapon[weapon].count >= AE_HUD_ELEMENTS)
			return 0;
		at = table->weapon[weapon].count++;
	}
	table->weapon[weapon].element[at] = *element;
	return 1;
}

int ae_hud_table_clear(struct ae_hud_table *table, char const *key, int index)
{
	int weapon = find_weapon(table, key), dropped = 0, at;

	if (weapon < 0)
		return 0;
	if (index < 0)
	{
		dropped = table->weapon[weapon].count;
		table->weapon[weapon].count = 0;
	}
	else if ((at = find_element(&table->weapon[weapon], index)) >= 0)
	{
		table->weapon[weapon].element[at] = table->weapon[weapon].element[--table->weapon[weapon].count];
		dropped = 1;
	}
	weapon_drop_if_empty(table, weapon);
	return dropped;
}

int ae_hud_table_parse(struct ae_hud_table *table, char const *text)
{
	int set = 0;

	while (*text)
	{
		char line[160], key[AE_HUD_KEY + 8];
		size_t length = strcspn(text, "\n");
		char const *at = line;
		char *end;
		struct ae_hud_element element;
		size_t size = 0;
		double number[5];
		int index, good = 1;

		snprintf(line, sizeof line, "%.*s", (int)(length < sizeof line - 1 ? length : sizeof line - 1), text);
		text += length + (text[length] ? 1 : 0);
		if (line[0] == '#')
			continue;
		while (*at == ' ' || *at == '\t')
			at++;
		while (at[size] && at[size] != ' ' && at[size] != '\t' && at[size] != '\r')
			size++;
		if (!size || size >= sizeof key)
			continue;
		memcpy(key, at, size);
		key[size] = 0;
		at += size;
		for (index = 0; index < 5 && good; index++)
		{
			number[index] = strtod(at, &end);
			good = end != at;
			at = end;
		}
		while (*at == ' ' || *at == '\t' || *at == '\r')
			at++;
		if (!good || *at || number[0] != floor(number[0]) || number[0] < 0 || number[0] >= AE_HUD_ELEMENTS)
			continue;
		element.index = (int)number[0];
		element.dx = (float)number[1];
		element.dy = (float)number[2];
		element.scale = (float)number[3];
		element.hide = number[4] != 0.0;
		if (ae_hud_table_set(table, key, &element))
			set++;
	}
	return set;
}

int ae_hud_table_format(struct ae_hud_table const *table, char *text, int size)
{
	int weapon, at, used = 0;

	for (weapon = 0; weapon < table->count; weapon++)
	{
		for (at = 0; at < table->weapon[weapon].count; at++)
		{
			struct ae_hud_element const *element = &table->weapon[weapon].element[at];
			int wrote = snprintf(text + used, (size_t)(size - used), "%s %d %g %g %g %d\n", table->weapon[weapon].key,
				element->index, element->dx, element->dy, element->scale, element->hide ? 1 : 0);

			if (wrote < 0 || wrote >= size - used)
				return 0;
			used += wrote;
		}
	}
	if (used >= size)
		return 0;
	text[used] = 0;
	return 1;
}
