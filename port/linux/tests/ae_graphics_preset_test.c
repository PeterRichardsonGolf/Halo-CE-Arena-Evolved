#include <stdio.h>
#include <string.h>

#include "ae_graphics_preset.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_graphics_preset_values values;
	const char *quality[AE_GRAPHICS_PRESET_ROWS] = { "smaa", "16x", "true", "native" };
	const char *performance[AE_GRAPHICS_PRESET_ROWS] = { "off", "off", "false", "stock" };
	const char *one_by_hand[AE_GRAPHICS_PRESET_ROWS] = { "fxaa", "16x", "true", "native" };
	const char *two_by_hand[AE_GRAPHICS_PRESET_ROWS] = { "off", "8x", "false", "native" };
	const char *shouted[AE_GRAPHICS_PRESET_ROWS] = { "SMAA", "16X", "TRUE", "Native" };
	const char *empty[AE_GRAPHICS_PRESET_ROWS] = { NULL, "", NULL, "" };
	char text[200];
	int first;

	/* the values: QUALITY the PC defaults, PERFORMANCE the Xbox's */
	ae_graphics_preset_values(0, &values);
	CHECK(!strcmp(values.anti_aliasing, "smaa") && !strcmp(values.anisotropic, "16x") &&
		!strcmp(values.per_pixel_lighting, "true") && !strcmp(values.model_detail, "native"));
	ae_graphics_preset_values(1, &values);
	CHECK(!strcmp(values.anti_aliasing, "off") && !strcmp(values.anisotropic, "off") &&
		!strcmp(values.per_pixel_lighting, "false") && !strcmp(values.model_detail, "stock"));
	CHECK(!strcmp(ae_graphics_preset_value(0, "display.model_detail"), "native"));
	CHECK(!strcmp(ae_graphics_preset_value(7, "display.anti_aliasing"), "off"));
	CHECK(!ae_graphics_preset_value(0, "display.big_sprite_limit") && !ae_graphics_preset_value(0, NULL));
	CHECK(!strcmp(ae_graphics_preset_keys[2], "display.per_pixel_lighting") &&
		!strcmp(ae_graphics_preset_labels[3], "MODEL DETAIL"));

	/* which preset: exactly one's values, case aside; anything else CUSTOM */
	CHECK(ae_graphics_preset_of(quality) == AE_GRAPHICS_QUALITY);
	CHECK(ae_graphics_preset_of(performance) == AE_GRAPHICS_PERFORMANCE);
	CHECK(ae_graphics_preset_of(shouted) == AE_GRAPHICS_QUALITY);
	CHECK(ae_graphics_preset_of(one_by_hand) == AE_GRAPHICS_CUSTOM);
	CHECK(ae_graphics_preset_of(two_by_hand) == AE_GRAPHICS_CUSTOM);
	CHECK(ae_graphics_preset_of(empty) == AE_GRAPHICS_CUSTOM);

	/* the rows set by hand: from the nearer preset */
	CHECK(ae_graphics_preset_hand_set(quality, &first) == 0 && first == -1);
	CHECK(ae_graphics_preset_hand_set(one_by_hand, &first) == 1 && first == 0);
	CHECK(ae_graphics_preset_hand_set(two_by_hand, &first) == 2 && first == 1);
	CHECK(ae_graphics_preset_hand_set(two_by_hand, NULL) == 2);
	ae_graphics_preset_custom_help(one_by_hand, text, sizeof(text));
	CHECK(!strcmp(text, "CUSTOM: ANTI-ALIASING was set by hand. Pick QUALITY or PERFORMANCE to set the four rows again."));
	ae_graphics_preset_custom_help(two_by_hand, text, sizeof(text));
	CHECK(!strcmp(text, "CUSTOM: 2 rows were set by hand. Pick QUALITY or PERFORMANCE to set the four rows again."));
	ae_graphics_preset_custom_help(two_by_hand, text, 12);
	CHECK(strlen(text) == 11);

	/* two players: AUTO side by side from 21:9 on */
	CHECK(ae_split_two_players_side_by_side("side_by_side", 1920, 1080));
	CHECK(!ae_split_two_players_side_by_side("top_and_bottom", 5120, 1440));
	CHECK(!ae_split_two_players_side_by_side("auto", 1920, 1080));
	CHECK(!ae_split_two_players_side_by_side("auto", 1920, 1200));
	CHECK(ae_split_two_players_side_by_side("auto", 2560, 1080));
	CHECK(ae_split_two_players_side_by_side("auto", 3440, 1440));
	CHECK(ae_split_two_players_side_by_side("auto", 5120, 1440));
	CHECK(ae_split_two_players_side_by_side("auto", 2376, 1080));
	CHECK(!ae_split_two_players_side_by_side("auto", 2375, 1080));
	CHECK(!ae_split_two_players_side_by_side("auto", 0, 0));
	CHECK(!ae_split_two_players_side_by_side("auto", 2560, 0));
	CHECK(ae_split_two_players_side_by_side(NULL, 2560, 1080) && !ae_split_two_players_side_by_side("", 1920, 1080));

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_graphics_preset: ok\n");
	return failures != 0;
}
