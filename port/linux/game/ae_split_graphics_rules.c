/*
AE_SPLIT_GRAPHICS_RULES.C

SPLIT GRAPHICS: the presets' table and the setting's rules (ae_split_graphics_rules.h).
*/

#include <string.h>

#include "ae_split_graphics_rules.h"

/* QUALITY is a computer's best look (0.7.8 to 0.8.0's defaults; 0.8.2 defaults to the Xbox's look, port_config.c's
DEFAULT_* block); Android, a browser and the dedicated server keep the Xbox's look as their QUALITY. PERFORMANCE is the
Xbox's look everywhere */
#if defined(HALO_ANDROID) || defined(HALO_WEB) || defined(HALO_SERVER)
#define QUALITY_ANTI_ALIASING "off"
#define QUALITY_ANISOTROPIC "off"
#define QUALITY_PER_PIXEL_LIGHTING "false"
#define QUALITY_MODEL_DETAIL "stock"
#else
#define QUALITY_ANTI_ALIASING "smaa"
#define QUALITY_ANISOTROPIC "16x"
#define QUALITY_PER_PIXEL_LIGHTING "true"
#define QUALITY_MODEL_DETAIL "native"
#endif

struct ae_split_graphics_preset_row const ae_split_graphics_presets[AE_SPLIT_GRAPHICS_PRESET_ROWS] =
{
	/* (key, QUALITY, PERFORMANCE) */
	{ "display.anti_aliasing", { QUALITY_ANTI_ALIASING, "off" } },
	{ "display.anisotropic", { QUALITY_ANISOTROPIC, "off" } },
	{ "display.per_pixel_lighting", { QUALITY_PER_PIXEL_LIGHTING, "false" } },
	{ "display.model_detail", { QUALITY_MODEL_DETAIL, "stock" } },
	/* (not one of VIDEO's GRAPHICS PRESET rows: ae_graphics_preset.c reads the first four) */
	{ "display.sharp_reflections", { "true", "false" } },
};

int ae_split_graphics_value(char const *text)
{
	if (text && !strcmp(text, "same"))
		return AE_SPLIT_GRAPHICS_SAME;
	if (text && !strcmp(text, "quality"))
		return AE_SPLIT_GRAPHICS_QUALITY;
	if (text && !strcmp(text, "performance"))
		return AE_SPLIT_GRAPHICS_PERFORMANCE;
	return AE_SPLIT_GRAPHICS_AUTO;
}

int ae_split_graphics_preset(int value)
{
	switch (value)
	{
	case AE_SPLIT_GRAPHICS_SAME:
		return AE_GRAPHICS_PRESET_NONE;
	case AE_SPLIT_GRAPHICS_QUALITY:
		return AE_GRAPHICS_PRESET_QUALITY;
	default:
		/* (AUTO plays PERFORMANCE for now: spec Q1) */
		return AE_GRAPHICS_PRESET_PERFORMANCE;
	}
}

char const *ae_split_graphics_preset_value(int preset, char const *key)
{
	int row;

	if (preset < 0 || preset >= NUMBER_OF_AE_GRAPHICS_PRESETS || !key)
		return NULL;
	for (row = 0; row < AE_SPLIT_GRAPHICS_PRESET_ROWS; row++)
	{
		if (!strcmp(ae_split_graphics_presets[row].key, key))
			return ae_split_graphics_presets[row].values[preset];
	}
	return NULL;
}
