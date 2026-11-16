#include <stdio.h>
#include <string.h>

#include "ae_graphics_preset.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_graphics_preset_values values;
	const char *quality[AE_GRAPHICS_PRESET_ROWS] = { "smaa", "16x", "true", "native", "128", "4096", "0", "false" };
	const char *performance[AE_GRAPHICS_PRESET_ROWS] = { "off", "off", "false", "stock", "128", "1024", "4", "false" };
	const char *one_by_hand[AE_GRAPHICS_PRESET_ROWS] = { "fxaa", "16x", "true", "native", "128", "4096", "0", "false" };
	const char *two_by_hand[AE_GRAPHICS_PRESET_ROWS] = { "off", "8x", "false", "native", "128", "1024", "4", "false" };
	const char *new_row_by_hand[AE_GRAPHICS_PRESET_ROWS] = { "off", "off", "false", "stock", "128", "2048", "4", "false" };
	const char *hdr_by_hand[AE_GRAPHICS_PRESET_ROWS] = { "off", "off", "false", "stock", "128", "1024", "4", "true" };
	const char *quality_hdr_on[AE_GRAPHICS_PRESET_ROWS] = { "smaa", "16x", "true", "native", "128", "4096", "0", "true" };
	const char *shouted[AE_GRAPHICS_PRESET_ROWS] = { "SMAA", "16X", "TRUE", "Native", "128", "4096", "0", "FALSE" };
	const char *empty[AE_GRAPHICS_PRESET_ROWS] = { NULL, "", NULL, "", NULL, "", NULL, "" };
	char text[200];
	int first;

	/* the values: QUALITY the PC defaults, PERFORMANCE the Xbox's */
	ae_graphics_preset_values(0, &values);
	CHECK(!strcmp(values.anti_aliasing, "smaa") && !strcmp(values.anisotropic, "16x") &&
		!strcmp(values.per_pixel_lighting, "true") && !strcmp(values.model_detail, "native"));
	ae_graphics_preset_values(1, &values);
	CHECK(!strcmp(values.anti_aliasing, "off") && !strcmp(values.anisotropic, "off") &&
		!strcmp(values.per_pixel_lighting, "false") && !strcmp(values.model_detail, "stock"));
	CHECK(!strcmp(values.shadow_resolution, "128") && !strcmp(values.particle_limit, "1024") &&
		!strcmp(values.dynamic_light_limit, "4") && !strcmp(values.hdr, "false"));
	ae_graphics_preset_values(0, &values);
	CHECK(!strcmp(values.shadow_resolution, "128") && !strcmp(values.particle_limit, "4096") &&
		!strcmp(values.dynamic_light_limit, "0") && !values.hdr);
	CHECK(!strcmp(ae_graphics_preset_value(0, "display.model_detail"), "native"));
	CHECK(!strcmp(ae_graphics_preset_value(1, "display.particle_limit"), "1024"));
	CHECK(!strcmp(ae_graphics_preset_value(1, "display.dynamic_light_limit"), "4"));
	/* HDR is opt-in: QUALITY leaves it alone (NULL), PERFORMANCE turns it off */
	CHECK(!ae_graphics_preset_value(0, "display.hdr") && !strcmp(ae_graphics_preset_value(1, "display.hdr"), "false"));
	/* DRAW DISTANCE is not a preset's: it changes what a player sees */
	CHECK(!ae_graphics_preset_value(0, "display.far_clip_distance") && !ae_graphics_preset_value(1, "display.far_clip_distance"));
	CHECK(AE_GRAPHICS_PRESET_ROWS == 8 && !strcmp(ae_graphics_preset_labels[7], "HDR"));
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
	CHECK(ae_graphics_preset_of(new_row_by_hand) == AE_GRAPHICS_CUSTOM);
	CHECK(ae_graphics_preset_of(hdr_by_hand) == AE_GRAPHICS_CUSTOM);
	/* QUALITY with HDR on still reads QUALITY (it leaves HDR alone); PERFORMANCE with it on is CUSTOM */
	CHECK(ae_graphics_preset_of(quality_hdr_on) == AE_GRAPHICS_QUALITY);
	CHECK(ae_graphics_preset_hand_set(quality_hdr_on, &first) == 0 && first == -1);
	CHECK(ae_graphics_preset_of(empty) == AE_GRAPHICS_CUSTOM);

	/* the rows set by hand: from the nearer preset */
	CHECK(ae_graphics_preset_hand_set(quality, &first) == 0 && first == -1);
	CHECK(ae_graphics_preset_hand_set(one_by_hand, &first) == 1 && first == 0);
	CHECK(ae_graphics_preset_hand_set(two_by_hand, &first) == 2 && first == 1);
	CHECK(ae_graphics_preset_hand_set(two_by_hand, NULL) == 2);
	CHECK(ae_graphics_preset_hand_set(hdr_by_hand, &first) == 1 && first == 7);
	CHECK(ae_graphics_preset_hand_set(new_row_by_hand, &first) == 1 && first == 5);
	ae_graphics_preset_custom_help(one_by_hand, text, sizeof(text));
	CHECK(!strcmp(text, "CUSTOM: ANTI-ALIASING was set by hand. Pick QUALITY or PERFORMANCE to set the eight rows again."));
	ae_graphics_preset_custom_help(hdr_by_hand, text, sizeof(text));
	CHECK(!strcmp(text, "CUSTOM: HDR was set by hand. Pick QUALITY or PERFORMANCE to set the eight rows again."));
	ae_graphics_preset_custom_help(two_by_hand, text, sizeof(text));
	CHECK(!strcmp(text, "CUSTOM: 2 rows were set by hand. Pick QUALITY or PERFORMANCE to set the eight rows again."));
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
