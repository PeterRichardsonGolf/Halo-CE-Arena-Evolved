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
	"display.anti_aliasing", "display.anisotropic", "display.per_pixel_lighting", "display.model_detail",
	"display.shadow_resolution", "display.particle_limit", "display.dynamic_light_limit", "display.hdr"
};
const char *const ae_graphics_preset_labels[AE_GRAPHICS_PRESET_ROWS] =
{
	"ANTI-ALIASING", "ANISOTROPIC FILTERING", "PER-PIXEL LIGHTING", "MODEL DETAIL",
	"SHADOW RESOLUTION", "PARTICLE LIMIT", "DYNAMIC LIGHT LIMIT", "HDR"
};

/* a preset's text of the eight rows: the one table split screen's renderer plays the first four of (ae_split_graphics_rules.c,
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
	values->shadow_resolution = preset_text(performance, 4);
	values->particle_limit = preset_text(performance, 5);
	values->dynamic_light_limit = preset_text(performance, 6);
	values->hdr = preset_text(performance, 7);
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
		/* (a NULL value: the preset leaves the row alone, whatever it holds) */
		if (!preset_text(performance, index) || same_text(now[index], preset_text(performance, index)))
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
		snprintf(text, size, "CUSTOM: %s was set by hand. Pick QUALITY or PERFORMANCE to set the eight rows again.",
			ae_graphics_preset_labels[first]);
	else
		snprintf(text, size, "CUSTOM: %d rows were set by hand. Pick QUALITY or PERFORMANCE to set the eight rows again.",
			count);
}

int ae_split_two_players_side_by_side(const char *two_players, long width, long height)
{
	/* (the renderer's own rule, ae_split_layout_rules.c, so the row says what the game draws) */
	return ae_split_side_by_side(ae_split_two_players_value(two_players), (int)width, (int)height);
}

/* (a case-blind search) */
static int text_has(const char *text, const char *word)
{
	size_t length = strlen(word);

	for (; *text; text++)
	{
		size_t index;

		for (index = 0; index < length && text[index]; index++)
		{
			char a = text[index], b = word[index];

			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');
			if (a != b)
				break;
		}
		if (index == length)
			return 1;
	}
	return 0;
}

/* (a letter or digit: a word's inside) */
static int word_letter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

/* (a case-blind search for a whole word or words: "vega 6" in "Radeon Vega 6 Graphics", not in "RX Vega 64"); the
place after it, else NULL */
static const char *text_word(const char *text, const char *word)
{
	size_t length = strlen(word);
	const char *at;

	for (at = text; *at; at++)
	{
		size_t index;

		if (at > text && word_letter(at[-1]) && word_letter(word[0]))
			continue;
		for (index = 0; index < length && at[index]; index++)
		{
			char a = at[index], b = word[index];

			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');
			if (a != b)
				break;
		}
		if (index == length && !(word_letter(word[length - 1]) && word_letter(at[length])))
			return at + length;
	}
	return NULL;
}

/* (an Intel Arc card: "Arc(tm) A770", "Arc B580", "Arc Pro A60"; a bare "Arc Graphics" is a laptop chip's) */
static int intel_arc_card(const char *renderer)
{
	const char *at = renderer;

	while ((at = text_word(at, "arc")) != NULL)
	{
		if (!strncmp(at, "(tm)", 4) || !strncmp(at, "(TM)", 4))
			at += 4;
		else if (!strncmp(at, "(r)", 3) || !strncmp(at, "(R)", 3))
			at += 3;
		while (*at == ' ')
			at++;
		if (text_word(at, "pro") == at + 3)
			for (at += 3; *at == ' '; at++)
				;
		if ((*at == 'A' || *at == 'a' || *at == 'B' || *at == 'b') && at[1] >= '0' && at[1] <= '9')
			return 1;
	}
	return 0;
}

int ae_graphics_auto_performance(const char *vendor, const char *renderer)
{
	/* (whole words: "vega 6" is an APU's, "RX Vega 64" and "Vega Frontier" are cards) */
	static const char *const weak[] =
	{
		"llvmpipe", "softpipe", "swiftshader", "software", "mali", "adreno", "powervr", "radeon graphics",
		"radeon(tm) graphics", "vega graphics", "vega 3", "vega 6", "vega 8", "vega 10", "vega 11"
	};
	size_t index;

	if (!vendor)
		vendor = "";
	if (!renderer)
		renderer = "";
	for (index = 0; index < sizeof(weak) / sizeof(weak[0]); index++)
		if (text_word(renderer, weak[index]))
			return 1;
	/* (a model number right after it: "GeForce MX450") */
	if (text_has(renderer, "geforce mx"))
		return 1;
	/* (Intel: its integrated GPUs, HD / UHD / Iris / a laptop's Arc Graphics; its Arc cards are real ones) */
	if ((text_has(vendor, "intel") || text_has(renderer, "intel")) && !intel_arc_card(renderer))
		return 1;
	return 0;
}
