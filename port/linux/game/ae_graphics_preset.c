/*
AE_GRAPHICS_PRESET.C

VIDEO's GRAPHICS PRESET and SPLIT SCREEN's rules (ae_graphics_preset.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_graphics_preset.h"
#include "ae_split_graphics_rules.h"
#include "ae_split_layout_rules.h"

const char *const ae_graphics_preset_keys[AE_GRAPHICS_PRESET_ROWS] =
{
	"display.anti_aliasing", "display.anisotropic", "display.per_pixel_lighting", "display.model_detail"
};
const char *const ae_graphics_preset_labels[AE_GRAPHICS_PRESET_ROWS] =
{
	"ANTI-ALIASING", "ANISOTROPIC FILTERING", "PER-PIXEL LIGHTING", "MODEL DETAIL"
};

/* a preset's text of the four rows: the one table split screen's renderer plays too (ae_split_graphics_rules.c,
in ae_graphics_preset_keys' order; QUALITY there is each platform's own defaults) */
static const char *preset_text(int performance, int index)
{
	return ae_split_graphics_presets[index].values[performance ? AE_GRAPHICS_PRESET_PERFORMANCE :
		AE_GRAPHICS_PRESET_QUALITY];
}

void ae_graphics_preset_values(int performance, struct ae_graphics_preset_values *values)
{
	values->anti_aliasing = preset_text(performance, 0);
	values->anisotropic = preset_text(performance, 1);
	values->per_pixel_lighting = preset_text(performance, 2);
	values->model_detail = preset_text(performance, 3);
}

const char *ae_graphics_preset_value(int performance, const char *key)
{
	int index;

	for (index = 0; key && index < AE_GRAPHICS_PRESET_ROWS; index++)
	{
		if (!strcmp(key, ae_graphics_preset_keys[index]))
			return preset_text(performance, index);
	}
	return NULL;
}

static int same_text(const char *a, const char *b)
{
	if (!a)
		a = "";
	if (!b)
		b = "";
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

/* the rows that differ from a preset */
static int differences(const char *const now[AE_GRAPHICS_PRESET_ROWS], int performance, int *first)
{
	int index, count = 0;

	if (first)
		*first = -1;
	for (index = 0; index < AE_GRAPHICS_PRESET_ROWS; index++)
	{
		if (same_text(now[index], preset_text(performance, index)))
			continue;
		if (first && *first < 0)
			*first = index;
		count++;
	}
	return count;
}

int ae_graphics_preset_of(const char *const now[AE_GRAPHICS_PRESET_ROWS])
{
	if (!differences(now, 0, NULL))
		return AE_GRAPHICS_QUALITY;
	if (!differences(now, 1, NULL))
		return AE_GRAPHICS_PERFORMANCE;
	return AE_GRAPHICS_CUSTOM;
}

int ae_graphics_preset_hand_set(const char *const now[AE_GRAPHICS_PRESET_ROWS], int *first)
{
	int quality_first, performance_first;
	int quality = differences(now, 0, &quality_first), performance = differences(now, 1, &performance_first);

	if (performance < quality)
	{
		if (first)
			*first = performance_first;
		return performance;
	}
	if (first)
		*first = quality_first;
	return quality;
}

void ae_graphics_preset_custom_help(const char *const now[AE_GRAPHICS_PRESET_ROWS], char *text, size_t size)
{
	int first, count = ae_graphics_preset_hand_set(now, &first);

	if (!text || !size)
		return;
	if (count == 1 && first >= 0)
		snprintf(text, size, "CUSTOM: %s was set by hand. Pick QUALITY or PERFORMANCE to set the four rows again.",
			ae_graphics_preset_labels[first]);
	else
		snprintf(text, size, "CUSTOM: %d rows were set by hand. Pick QUALITY or PERFORMANCE to set the four rows again.",
			count);
}

int ae_split_two_players_side_by_side(const char *two_players, long width, long height)
{
	/* (the renderer's own rule, ae_split_layout_rules.c, so the row says what the game draws) */
	return ae_split_side_by_side(ae_split_two_players_value(two_players), (int)width, (int)height);
}
