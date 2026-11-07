/*
AE_VIEW_MODEL_RULES.C

The first-person weapon's placement: the node math and the table (ae_view_model_rules.h). No engine includes.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_view_model_rules.h"

/* the weapons whose model sits as the stock rifle's does only with these numbers (a pack weapon's name): the combat
assault rifle, found against the stock one by eye with DEV MODE's viewmodel command */
static struct { char const *key; float x, y, z, scale; } const defaults[] =
{
	{ "ar_modern", 1.4755f, 0.0f, 0.2957f, 0.78f },
};

struct ae_view_model ae_view_model_identity(void)
{
	struct ae_view_model model = { { 0.0f, 0.0f, 0.0f }, 1.0f };

	return model;
}

int ae_view_model_is_identity(struct ae_view_model const *model)
{
	return model->offset[0] == 0.0f && model->offset[1] == 0.0f && model->offset[2] == 0.0f && model->scale == 1.0f;
}

int ae_view_model_valid(struct ae_view_model const *model)
{
	int axis;

	for (axis = 0; axis < 3; axis++)
		if (!__builtin_isfinite(model->offset[axis]) || fabsf(model->offset[axis]) > AE_VIEW_MODEL_OFFSET_MAXIMUM)
			return 0;
	return __builtin_isfinite(model->scale) && model->scale >= AE_VIEW_MODEL_SCALE_MINIMUM &&
		model->scale <= AE_VIEW_MODEL_SCALE_MAXIMUM;
}

void ae_view_model_node(struct ae_view_model const *model, float const root_position[3],
	float const root_axes[3][3], float root_scale, float position[3], float *scale)
{
	int axis, column;

	for (axis = 0; axis < 3; axis++)
	{
		float moved = root_position[axis] + model->scale * (position[axis] - root_position[axis]);

		for (column = 0; column < 3; column++)
			moved += root_scale * root_axes[column][axis] * model->offset[column] * AE_VIEW_MODEL_UNIT;
		position[axis] = moved;
	}
	*scale *= model->scale;
}

static int same_key(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return !*a && !*b;
}

static int find(struct ae_view_model_table const *table, char const *key)
{
	int index;

	for (index = 0; index < table->count; index++)
		if (same_key(table->entry[index].key, key))
			return index;
	return -1;
}

void ae_view_model_table_defaults(struct ae_view_model_table *table)
{
	size_t index;

	memset(table, 0, sizeof *table);
	for (index = 0; index < sizeof defaults / sizeof defaults[0]; index++)
	{
		struct ae_view_model model = { { defaults[index].x, defaults[index].y, defaults[index].z },
			defaults[index].scale };

		ae_view_model_table_set(table, defaults[index].key, &model);
	}
}

int ae_view_model_table_get(struct ae_view_model_table const *table, char const *key, struct ae_view_model *out)
{
	int index = find(table, key);

	*out = index >= 0 ? table->entry[index].value : ae_view_model_identity();
	return index >= 0;
}

int ae_view_model_table_set(struct ae_view_model_table *table, char const *key, struct ae_view_model const *model)
{
	int index;

	if (!key[0] || strlen(key) >= AE_VIEW_MODEL_KEY || strpbrk(key, " \t\r\n#") || !ae_view_model_valid(model))
		return 0;
	index = find(table, key);
	if (index < 0)
	{
		if (table->count >= AE_VIEW_MODEL_ENTRIES)
			return 0;
		index = table->count++;
		snprintf(table->entry[index].key, sizeof table->entry[index].key, "%s", key);
	}
	table->entry[index].value = *model;
	return 1;
}

int ae_view_model_table_clear(struct ae_view_model_table *table, char const *key)
{
	int index = find(table, key);

	if (index < 0)
		return 0;
	table->entry[index] = table->entry[--table->count];
	return 1;
}

int ae_view_model_table_parse(struct ae_view_model_table *table, char const *text)
{
	int set = 0;

	while (*text)
	{
		char line[160], key[AE_VIEW_MODEL_KEY + 8];
		struct ae_view_model model;
		size_t length = strcspn(text, "\n");

		snprintf(line, sizeof line, "%.*s", (int)(length < sizeof line - 1 ? length : sizeof line - 1), text);
		text += length + (text[length] ? 1 : 0);
		if (line[0] == '#')
			continue;
		{
			/* (by hand: the key, then four numbers, nothing after) */
			char const *at = line;
			char *end;
			float number[4];
			size_t size = 0;
			int index, good = 1;

			while (*at == ' ' || *at == '\t')
				at++;
			while (at[size] && at[size] != ' ' && at[size] != '\t' && at[size] != '\r')
				size++;
			if (!size || size >= sizeof key)
				continue;
			memcpy(key, at, size);
			key[size] = 0;
			at += size;
			for (index = 0; index < 4 && good; index++)
			{
				number[index] = (float)strtod(at, &end);
				good = end != at;
				at = end;
			}
			while (*at == ' ' || *at == '\t' || *at == '\r')
				at++;
			model.offset[0] = number[0];
			model.offset[1] = number[1];
			model.offset[2] = number[2];
			model.scale = number[3];
			if (good && !*at && ae_view_model_table_set(table, key, &model))
				set++;
		}
	}
	return set;
}

int ae_view_model_table_format(struct ae_view_model_table const *table, char *text, int size)
{
	int index, used = 0;

	for (index = 0; index < table->count; index++)
	{
		struct ae_view_model_entry const *entry = &table->entry[index];
		int wrote = snprintf(text + used, (size_t)(size - used), "%s %g %g %g %g\n", entry->key,
			entry->value.offset[0], entry->value.offset[1], entry->value.offset[2], entry->value.scale);

		if (wrote < 0 || wrote >= size - used)
			return 0;
		used += wrote;
	}
	if (used >= size)
		return 0;
	text[used] = 0;
	return 1;
}
