#include <stdio.h>
#include <string.h>

#include "ae_graphics_preset.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_auto_performance(void)
{
	/* weak: software, integrated, phones, MX */
	CHECK(ae_graphics_auto_performance("Mesa", "llvmpipe (LLVM 19.1.0, 256 bits)"));
	CHECK(ae_graphics_auto_performance("Intel", "Mesa Intel(R) Iris(R) Xe Graphics (TGL GT2)"));
	CHECK(ae_graphics_auto_performance("Intel", "Intel(R) UHD Graphics 620"));
	CHECK(ae_graphics_auto_performance("AMD", "AMD Radeon(TM) Graphics"));
	CHECK(ae_graphics_auto_performance("ATI Technologies Inc.", "AMD Radeon(TM) Vega 8 Graphics"));
	CHECK(ae_graphics_auto_performance("ARM", "Mali-G78"));
	CHECK(ae_graphics_auto_performance("Qualcomm", "Adreno (TM) 740"));
	CHECK(ae_graphics_auto_performance("NVIDIA Corporation", "NVIDIA GeForce MX450/PCIe/SSE2"));
	CHECK(ae_graphics_auto_performance(NULL, NULL) == 0);
	/* real GPUs keep QUALITY */
	CHECK(!ae_graphics_auto_performance("NVIDIA Corporation", "NVIDIA GeForce RTX 4070 Ti SUPER/PCIe/SSE2"));
	CHECK(!ae_graphics_auto_performance("NVIDIA Corporation", "NVIDIA GeForce RTX 5060/PCIe/SSE2"));
	CHECK(!ae_graphics_auto_performance("AMD", "AMD Radeon RX 7800 XT (radeonsi, navi32, LLVM 19.1.0)"));
	CHECK(!ae_graphics_auto_performance("Intel", "Mesa Intel(R) Arc(tm) A770 Graphics (DG2)"));
	/* (whole words: desktop Vega cards are not APUs; a laptop's bare Arc Graphics is integrated) */
	CHECK(!ae_graphics_auto_performance("AMD", "Radeon RX Vega 64"));
	CHECK(!ae_graphics_auto_performance("AMD", "AMD Radeon RX Vega (vega10, LLVM 17.0.6, DRM 3.57)"));
	CHECK(!ae_graphics_auto_performance("AMD", "Radeon Vega Frontier Edition"));
	CHECK(ae_graphics_auto_performance("AMD", "AMD Radeon Vega 8 Graphics (raven, LLVM 17.0.6, DRM 3.57)"));
	CHECK(ae_graphics_auto_performance("ATI Technologies Inc.", "AMD Radeon(TM) Vega 6 Graphics"));
	CHECK(ae_graphics_auto_performance("AMD", "AMD Radeon(TM) Vega Graphics"));
	CHECK(ae_graphics_auto_performance("Intel", "Mesa Intel(R) Arc(tm) Graphics (MTL)"));
	CHECK(!ae_graphics_auto_performance("Intel", "Intel(R) Arc(TM) B580 Graphics"));
	CHECK(!ae_graphics_auto_performance("Intel", "Intel(R) Arc(TM) Pro A60 Graphics"));
	CHECK(!ae_graphics_auto_performance("Apple", "Apple M2"));
}

int main(void)
{
	test_auto_performance();
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
