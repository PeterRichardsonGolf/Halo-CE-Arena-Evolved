/*
AE_SPLIT_GRAPHICS_RULES.C

SPLIT GRAPHICS and VIDEO's GRAPHICS PRESET: the presets' table and the setting's rules (ae_split_graphics_rules.h).
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

/* (0.9.3) the rows only VIDEO's GRAPHICS PRESET sets: QUALITY is each platform's default (port_config.c's: the Switch's particle
limit and the consoles' light limit differ; the consoles' macros are halo_console_features.h's, which this pure file
does not include), PERFORMANCE the Xbox's own figure where it is known (particles:
1024) and else the lowest the row offers that still looks normal (lights: 8, as a console plays; shadows: 128 is both
the default and the least, so PERFORMANCE only takes a raised one back). HDR is off both ways. DRAW DISTANCE is not a
preset's: it changes what a player sees */
#define QUALITY_SHADOW_RESOLUTION "128"
/* (a NULL value: the preset leaves the row alone. HDR is a player's choice, not quality against speed; a row a
platform does not list in Settings (the particle and light limits on Android and in a browser, ae_settings_rows.c) is
not a preset's there) */
#if defined(HALO_ANDROID) || defined(HALO_WEB)
#define QUALITY_PARTICLE_LIMIT NULL
#define PERFORMANCE_PARTICLE_LIMIT NULL
#define QUALITY_DYNAMIC_LIGHT_LIMIT NULL
#define PERFORMANCE_DYNAMIC_LIGHT_LIMIT NULL
#else
#ifdef HALO_SWITCH_CONSOLE
#define QUALITY_PARTICLE_LIMIT "2048"
#else
#define QUALITY_PARTICLE_LIMIT "4096"
#endif
#if defined(HALO_SWITCH_CONSOLE) || defined(HALO_WIIU_CONSOLE) || defined(HALO_XBOX_CONSOLE) || defined(HALO_XBOX360_CONSOLE)
#define QUALITY_DYNAMIC_LIGHT_LIMIT "8"
#else
#define QUALITY_DYNAMIC_LIGHT_LIMIT "0"
#endif
#define PERFORMANCE_PARTICLE_LIMIT "1024"
#define PERFORMANCE_DYNAMIC_LIGHT_LIMIT "4"
#endif

struct ae_split_graphics_preset_row const ae_split_graphics_presets[AE_SPLIT_GRAPHICS_PRESET_ROWS] =
{
	/* (key, QUALITY, PERFORMANCE, in VIDEO's GRAPHICS PRESET, in SPLIT GRAPHICS) */
	{ "display.anti_aliasing", { QUALITY_ANTI_ALIASING, "off" }, 1, 1 },
	{ "display.anisotropic", { QUALITY_ANISOTROPIC, "off" }, 1, 1 },
	{ "display.per_pixel_lighting", { QUALITY_PER_PIXEL_LIGHTING, "false" }, 1, 1 },
	{ "display.model_detail", { QUALITY_MODEL_DETAIL, "stock" }, 1, 1 },
	/* (VIDEO's GRAPHICS PRESET only: split screen's renderer reads these from config.toml, not through
	ae_split_graphics.c, and a shadow map's size and HDR's buffer are not changed per view) */
	{ "display.shadow_resolution", { QUALITY_SHADOW_RESOLUTION, "128" }, 1, 0 },
	{ "display.particle_limit", { QUALITY_PARTICLE_LIMIT, PERFORMANCE_PARTICLE_LIMIT }, 1, 0 },
	{ "display.dynamic_light_limit", { QUALITY_DYNAMIC_LIGHT_LIMIT, PERFORMANCE_DYNAMIC_LIGHT_LIMIT }, 1, 0 },
	{ "display.hdr", { NULL, "false" }, 1, 0 },
	/* (SPLIT GRAPHICS only) */
	{ "display.sharp_reflections", { "true", "false" }, 0, 1 },
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
		if (ae_split_graphics_presets[row].split && !strcmp(ae_split_graphics_presets[row].key, key))
			return ae_split_graphics_presets[row].values[preset];
	}
	return NULL;
}
