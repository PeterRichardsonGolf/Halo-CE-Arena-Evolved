/*
AE_SPLIT_GRAPHICS_RULES.H

SPLIT GRAPHICS (Settings > SPLIT SCREEN > GRAPHICS; display.split_graphics): the graphics the renderer plays with while
two to four local players share the screen, in place of the one-player values (VIDEO's ANTI-ALIASING, ANISOTROPIC
FILTERING, PER-PIXEL LIGHTING and MODEL DETAIL, and display.sharp_reflections), never written to config.toml. AUTO and PERFORMANCE play the PERFORMANCE
preset, QUALITY the QUALITY preset (a computer's best look), SAME the stored values.

The presets' values are one table, ae_split_graphics_presets (ae_split_graphics_rules.c), so VIDEO's GRAPHICS PRESET
can take the same values. Pure: no engine includes (port/linux/tests/ae_split_graphics_rules_test.c);
ae_split_graphics.c applies them for the renderer (d3d8_gl.c, ae_texture_filter.c).
*/

#ifndef AE_SPLIT_GRAPHICS_RULES_H
#define AE_SPLIT_GRAPHICS_RULES_H

/* display.split_graphics */
enum
{
	AE_SPLIT_GRAPHICS_AUTO,
	AE_SPLIT_GRAPHICS_SAME,
	AE_SPLIT_GRAPHICS_QUALITY,
	AE_SPLIT_GRAPHICS_PERFORMANCE
};

/* the presets */
enum
{
	AE_GRAPHICS_PRESET_NONE = -1,
	AE_GRAPHICS_PRESET_QUALITY,
	AE_GRAPHICS_PRESET_PERFORMANCE,
	NUMBER_OF_AE_GRAPHICS_PRESETS
};

/* a setting a preset sets, and its value (config.toml's text, unquoted) in each preset */
struct ae_split_graphics_preset_row
{
	char const *key;
	char const *values[NUMBER_OF_AE_GRAPHICS_PRESETS];
};

/* the rows: anti-aliasing, anisotropic filtering, per-pixel lighting and model detail (VIDEO's GRAPHICS PRESET's four, in
this order), then sharp reflections (split screen only) */
#define AE_SPLIT_GRAPHICS_PRESET_ROWS 5
extern struct ae_split_graphics_preset_row const ae_split_graphics_presets[AE_SPLIT_GRAPHICS_PRESET_ROWS];

/* the setting's value from its text ("auto", "same", "quality", "performance"; anything else AUTO) */
int ae_split_graphics_value(char const *text);

/* the preset this value plays with two or more views (AUTO: PERFORMANCE), AE_GRAPHICS_PRESET_NONE for SAME */
int ae_split_graphics_preset(int value);

/* the preset's value of a setting, NULL for a setting the presets do not set (or no preset) */
char const *ae_split_graphics_preset_value(int preset, char const *key);

#endif
