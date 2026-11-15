#include <stdio.h>
#include <string.h>

#include "ae_split_graphics_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define SAME_TEXT(a, b) ((a) && (b) && !strcmp((a), (b)))

int main(void)
{
	int row;

	/* the setting's text */
	CHECK(ae_split_graphics_value("auto") == AE_SPLIT_GRAPHICS_AUTO);
	CHECK(ae_split_graphics_value("same") == AE_SPLIT_GRAPHICS_SAME);
	CHECK(ae_split_graphics_value("quality") == AE_SPLIT_GRAPHICS_QUALITY);
	CHECK(ae_split_graphics_value("performance") == AE_SPLIT_GRAPHICS_PERFORMANCE);
	CHECK(ae_split_graphics_value("ultra") == AE_SPLIT_GRAPHICS_AUTO);
	CHECK(ae_split_graphics_value(NULL) == AE_SPLIT_GRAPHICS_AUTO);

	/* AUTO and PERFORMANCE play PERFORMANCE, QUALITY QUALITY, SAME no preset */
	CHECK(ae_split_graphics_preset(AE_SPLIT_GRAPHICS_AUTO) == AE_GRAPHICS_PRESET_PERFORMANCE);
	CHECK(ae_split_graphics_preset(AE_SPLIT_GRAPHICS_PERFORMANCE) == AE_GRAPHICS_PRESET_PERFORMANCE);
	CHECK(ae_split_graphics_preset(AE_SPLIT_GRAPHICS_QUALITY) == AE_GRAPHICS_PRESET_QUALITY);
	CHECK(ae_split_graphics_preset(AE_SPLIT_GRAPHICS_SAME) == AE_GRAPHICS_PRESET_NONE);

	/* PERFORMANCE: the Xbox's look (AA off, anisotropic off, per-pixel lighting off, model detail stock) */
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_PERFORMANCE, "display.anti_aliasing"), "off"));
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_PERFORMANCE, "display.anisotropic"), "off"));
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_PERFORMANCE, "display.per_pixel_lighting"),
		"false"));
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_PERFORMANCE, "display.model_detail"), "stock"));
	/* QUALITY: the PC's defaults (port_config.c's DEFAULT_* block: this test is built as the PC's) */
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_QUALITY, "display.anti_aliasing"), "smaa"));
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_QUALITY, "display.anisotropic"), "16x"));
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_QUALITY, "display.per_pixel_lighting"), "true"));
	CHECK(SAME_TEXT(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_QUALITY, "display.model_detail"), "native"));

	/* settings the presets do not set, and no preset: the stored value plays (NULL) */
	CHECK(!ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_PERFORMANCE, "display.shadow_resolution"));
	CHECK(!ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_PERFORMANCE, "display.anisotropic_filtering"));
	CHECK(!ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_NONE, "display.anti_aliasing"));
	CHECK(!ae_split_graphics_preset_value(NUMBER_OF_AE_GRAPHICS_PRESETS, "display.anti_aliasing"));
	CHECK(!ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_QUALITY, NULL));

	/* sharp reflections: QUALITY sharp, PERFORMANCE the Xbox's half */
	CHECK(!strcmp(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_QUALITY, "display.sharp_reflections"), "true"));
	CHECK(!strcmp(ae_split_graphics_preset_value(AE_GRAPHICS_PRESET_PERFORMANCE, "display.sharp_reflections"), "false"));
	/* VIDEO's GRAPHICS PRESET reads the first four rows: they stay its four, in its order */
	CHECK(!strcmp(ae_split_graphics_presets[0].key, "display.anti_aliasing"));
	CHECK(!strcmp(ae_split_graphics_presets[3].key, "display.model_detail"));

	/* the table: every value set */
	for (row = 0; row < AE_SPLIT_GRAPHICS_PRESET_ROWS; row++)
	{
		CHECK(ae_split_graphics_presets[row].key != NULL);
		CHECK(ae_split_graphics_presets[row].values[AE_GRAPHICS_PRESET_QUALITY] != NULL);
		CHECK(ae_split_graphics_presets[row].values[AE_GRAPHICS_PRESET_PERFORMANCE] != NULL);
	}

	if (failures)
		return 1;
	printf("ae_split_graphics_rules: ok\n");
	return 0;
}
