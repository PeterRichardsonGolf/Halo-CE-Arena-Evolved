/* built with -DHALO_ANDROID (and HALO_WEB the same way): the platform's own defaults read as its default preset, with
the limits (not in Settings there) and HDR left alone */
#include <stdio.h>
#include <string.h>

#include "ae_graphics_preset.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* a fresh install: port_config.c's defaults (the Xbox's look, particle 4096, lights 0, HDR off) */
	const char *fresh[AE_GRAPHICS_PRESET_ROWS] = { "off", "off", "false", "stock", "128", "4096", "0", "false" };
	const char *hdr_on[AE_GRAPHICS_PRESET_ROWS] = { "off", "off", "false", "stock", "128", "4096", "0", "true" };
	const char *aa_by_hand[AE_GRAPHICS_PRESET_ROWS] = { "fxaa", "off", "false", "stock", "128", "4096", "0", "false" };

	CHECK(ae_graphics_preset_of(fresh) != AE_GRAPHICS_CUSTOM);
	CHECK(ae_graphics_preset_hand_set(fresh, NULL) == 0);
	/* the limits are not a preset's here: whatever they hold, in either preset */
	CHECK(!ae_graphics_preset_value(0, "display.particle_limit") && !ae_graphics_preset_value(1, "display.particle_limit"));
	CHECK(!ae_graphics_preset_value(0, "display.dynamic_light_limit") &&
		!ae_graphics_preset_value(1, "display.dynamic_light_limit"));
	CHECK(ae_graphics_preset_of(hdr_on) == AE_GRAPHICS_QUALITY);
	CHECK(ae_graphics_preset_of(aa_by_hand) == AE_GRAPHICS_CUSTOM);
	if (failures)
		return 1;
	printf("ae_graphics_preset_android: ok\n");
	return 0;
}
