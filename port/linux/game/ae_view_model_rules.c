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
assault rifle, its gun's own numbers found against the stock one by eye with DEV MODE's viewmodel command */
static struct { char const *key; float x, y, z, scale, place[3], turn[3], place_scale; } const defaults[] =
{
	/* (the one-piece placement fitted to the stock rifle's markers with tools/viewmodel_fit.py: the muzzle, the ejection
	port and the left hand's two, within about 1.5 model units; owner-approved 2026-11-07) */
	{ "ar_modern", 1.4755f, 0.0f, 0.2957f, 0.78f, { 0.156f, 0.710f, -5.016f }, { 1.591f, -1.426f, -7.047f }, 1.0f },
};

struct ae_view_model ae_view_model_identity(void)
{
	struct ae_view_model model = { { 0.0f, 0.0f, 0.0f }, 1.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f };

	return model;
}

/* (a place_scale of 0 is one the entry never set: 1) */
static float place_scale(struct ae_view_model const *model)
{
	return model->place_scale == 0.0f ? 1.0f : model->place_scale;
}

int ae_view_model_is_placed(struct ae_view_model const *model)
{
	int axis;

	for (axis = 0; axis < 3; axis++)
		if (model->place[axis] != 0.0f || model->turn[axis] != 0.0f)
			return 1;
	return place_scale(model) != 1.0f;
}

int ae_view_model_is_identity(struct ae_view_model const *model)
{
	return model->offset[0] == 0.0f && model->offset[1] == 0.0f && model->offset[2] == 0.0f && model->scale == 1.0f &&
		!ae_view_model_is_placed(model);
}

int ae_view_model_valid(struct ae_view_model const *model)
{
	int axis;

	for (axis = 0; axis < 3; axis++)
	{
		if (!__builtin_isfinite(model->offset[axis]) || fabsf(model->offset[axis]) > AE_VIEW_MODEL_OFFSET_MAXIMUM)
			return 0;
		if (!__builtin_isfinite(model->place[axis]) || fabsf(model->place[axis]) > AE_VIEW_MODEL_PLACE_MAXIMUM ||
			!__builtin_isfinite(model->turn[axis]) || fabsf(model->turn[axis]) > AE_VIEW_MODEL_TURN_MAXIMUM)
			return 0;
	}
	if (!__builtin_isfinite(model->place_scale) || place_scale(model) < AE_VIEW_MODEL_SCALE_MINIMUM ||
		place_scale(model) > AE_VIEW_MODEL_SCALE_MAXIMUM)
		return 0;
	return __builtin_isfinite(model->scale) && model->scale >= AE_VIEW_MODEL_SCALE_MINIMUM &&
		model->scale <= AE_VIEW_MODEL_SCALE_MAXIMUM;
}

/* ---------- the arms and gun as one piece */

static void normalize(float v[3])
{
	float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	if (length > 0.0f)
	{
		v[0] /= length;
		v[1] /= length;
		v[2] /= length;
	}
}

static void cross(float const a[3], float const b[3], float out[3])
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

/* the turn in the camera's frame (x forward, y left, z up), as columns forward, left, up: the muzzle yawed to the
left, pitched up, then its top rolled to the right about where it points */
static void local_turn(float const turn[3], float out[3][3])
{
	float const radians = 3.14159265358979f / 180.0f;
	float cy = cosf(turn[0] * radians), sy = sinf(turn[0] * radians);
	float cp = cosf(turn[1] * radians), sp = sinf(turn[1] * radians);
	float cr = cosf(turn[2] * radians), sr = sinf(turn[2] * radians);
	float forward[3], up[3], right[3], rolled[3], left[3];
	int row;

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = sp;
	up[0] = -sp * cy;
	up[1] = -sp * sy;
	up[2] = cp;
	cross(forward, up, right);
	for (row = 0; row < 3; row++)
		rolled[row] = up[row] * cr + right[row] * sr;
	cross(rolled, forward, left);
	for (row = 0; row < 3; row++)
	{
		out[row][0] = forward[row];
		out[row][1] = left[row];
		out[row][2] = rolled[row];
	}
}

int ae_view_model_place_frame(struct ae_view_model const *model, float const camera_forward[3],
	float const camera_up[3], float const pivot[3], struct ae_view_model_frame *frame)
{
	float basis[3][3], turn[3][3], half[3][3], offset_local[3], offset[3];
	float forward[3], up[3], left[3], dot;
	float scale = place_scale(model);
	int row, column, k;

	memset(frame, 0, sizeof *frame);
	for (row = 0; row < 3; row++)
		frame->rotation[row][row] = 1.0f;
	frame->scale = 1.0f;
	if (!ae_view_model_is_placed(model))
		return 0;
	/* (the camera's frame: forward, left, up as columns, up made square to forward) */
	memcpy(forward, camera_forward, sizeof forward);
	normalize(forward);
	dot = camera_up[0] * forward[0] + camera_up[1] * forward[1] + camera_up[2] * forward[2];
	for (row = 0; row < 3; row++)
		up[row] = camera_up[row] - dot * forward[row];
	normalize(up);
	cross(up, forward, left);
	for (row = 0; row < 3; row++)
	{
		basis[row][0] = forward[row];
		basis[row][1] = left[row];
		basis[row][2] = up[row];
	}
	/* (the world's turn: basis * turn * basis transposed) */
	local_turn(model->turn, turn);
	for (row = 0; row < 3; row++)
		for (column = 0; column < 3; column++)
		{
			half[row][column] = 0.0f;
			for (k = 0; k < 3; k++)
				half[row][column] += basis[row][k] * turn[k][column];
		}
	for (row = 0; row < 3; row++)
		for (column = 0; column < 3; column++)
		{
			frame->rotation[row][column] = 0.0f;
			for (k = 0; k < 3; k++)
				frame->rotation[row][column] += half[row][k] * basis[column][k];
		}
	frame->scale = scale;
	/* (the move in the camera's frame: forward, left (right's other way), up) */
	offset_local[0] = model->place[2] * AE_VIEW_MODEL_UNIT;
	offset_local[1] = -model->place[0] * AE_VIEW_MODEL_UNIT;
	offset_local[2] = model->place[1] * AE_VIEW_MODEL_UNIT;
	for (row = 0; row < 3; row++)
		offset[row] = basis[row][0] * offset_local[0] + basis[row][1] * offset_local[1] + basis[row][2] * offset_local[2];
	/* (the pivot turns and scales about itself, then moves: translation = pivot - scale * rotation * pivot + offset) */
	for (row = 0; row < 3; row++)
	{
		float turned = 0.0f;

		for (k = 0; k < 3; k++)
			turned += frame->rotation[row][k] * pivot[k];
		frame->translation[row] = pivot[row] - scale * turned + offset[row];
	}
	return 1;
}

void ae_view_model_place_node(struct ae_view_model_frame const *frame, float position[3], float axes[3][3],
	float *scale)
{
	float moved[3], turned[3][3];
	int row, axis, k;

	for (row = 0; row < 3; row++)
	{
		moved[row] = frame->translation[row];
		for (k = 0; k < 3; k++)
			moved[row] += frame->scale * frame->rotation[row][k] * position[k];
	}
	for (axis = 0; axis < 3; axis++)
		for (row = 0; row < 3; row++)
		{
			turned[axis][row] = 0.0f;
			for (k = 0; k < 3; k++)
				turned[axis][row] += frame->rotation[row][k] * axes[axis][k];
		}
	memcpy(position, moved, sizeof moved);
	memcpy(axes, turned, sizeof turned);
	*scale *= frame->scale;
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
		struct ae_view_model model = ae_view_model_identity();

		model.offset[0] = defaults[index].x;
		model.offset[1] = defaults[index].y;
		model.offset[2] = defaults[index].z;
		model.scale = defaults[index].scale;
		memcpy(model.place, defaults[index].place, sizeof model.place);
		memcpy(model.turn, defaults[index].turn, sizeof model.turn);
		model.place_scale = defaults[index].place_scale;

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
		char line[240], key[AE_VIEW_MODEL_KEY + 8];
		struct ae_view_model model;
		size_t length = strcspn(text, "\n");

		snprintf(line, sizeof line, "%.*s", (int)(length < sizeof line - 1 ? length : sizeof line - 1), text);
		text += length + (text[length] ? 1 : 0);
		if (line[0] == '#')
			continue;
		{
			/* (by hand: the key, then four numbers (the gun alone) or eleven (and the arms and gun as one piece),
			nothing after) */
			char const *at = line;
			char *end;
			float number[11];
			size_t size = 0;
			int index, good = 1, numbers = 0;

			while (*at == ' ' || *at == '\t')
				at++;
			while (at[size] && at[size] != ' ' && at[size] != '\t' && at[size] != '\r')
				size++;
			if (!size || size >= sizeof key)
				continue;
			memcpy(key, at, size);
			key[size] = 0;
			at += size;
			for (index = 0; index < 11 && good; index++)
			{
				while (*at == ' ' || *at == '\t' || *at == '\r')
					at++;
				if (!*at)
					break;
				number[index] = (float)strtod(at, &end);
				good = end != at;
				at = end;
				numbers += good;
			}
			while (*at == ' ' || *at == '\t' || *at == '\r')
				at++;
			good = good && (numbers == 4 || numbers == 11);
			/* (four numbers change the gun alone: the arms and gun's placement the table has stays) */
			ae_view_model_table_get(table, key, &model);
			if (good)
			{
				model.offset[0] = number[0];
				model.offset[1] = number[1];
				model.offset[2] = number[2];
				model.scale = number[3];
				if (numbers == 11)
				{
					model.place[0] = number[4];
					model.place[1] = number[5];
					model.place[2] = number[6];
					model.turn[0] = number[7];
					model.turn[1] = number[8];
					model.turn[2] = number[9];
					model.place_scale = number[10];
				}
			}
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
		struct ae_view_model const *value = &table->entry[index].value;
		char const *key = table->entry[index].key;
		int wrote = ae_view_model_is_placed(value) ?
			snprintf(text + used, (size_t)(size - used), "%s %g %g %g %g %g %g %g %g %g %g %g\n", key,
				value->offset[0], value->offset[1], value->offset[2], value->scale, value->place[0], value->place[1],
				value->place[2], value->turn[0], value->turn[1], value->turn[2], place_scale(value)) :
			snprintf(text + used, (size_t)(size - used), "%s %g %g %g %g\n", key,
				value->offset[0], value->offset[1], value->offset[2], value->scale);

		if (wrote < 0 || wrote >= size - used)
			return 0;
		used += wrote;
	}
	if (used >= size)
		return 0;
	text[used] = 0;
	return 1;
}
