/* high-resolution HUD digits drawn at the stock size (port/linux/game/ae_hud_hires.c) */
#include <stdio.h>

#include "ae_hud_hires.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* (stock and near-stock tags: untouched) */
	AE_CHECK(ae_hud_number_scale(9) == 1.0f);
	AE_CHECK(ae_hud_number_scale(12) == 1.0f);
	AE_CHECK(ae_hud_number_scale(13) == 1.0f);
	AE_CHECK(ae_hud_number_scale(0) == 1.0f);
	AE_CHECK(ae_hud_number_scale(-5) == 1.0f);
	/* (a rebuilt Gearbox map's: twice the stock width, drawn at half) */
	AE_CHECK(ae_hud_number_scale(18) == 0.5f);
	AE_CHECK(ae_hud_number_scale(14) > 0.64f && ae_hud_number_scale(14) < 0.65f);
	AE_CHECK(ae_hud_number_scale(127) * 127.0f > 8.99f && ae_hud_number_scale(127) * 127.0f < 9.01f);
	/* (a HUD overlay's SUBTRACT: the Xbox's on its own maps, multiply on a Custom Edition map's) */
	AE_CHECK(ae_hud_multitexture_subtract(0, 2, 1) == 2);
	AE_CHECK(ae_hud_multitexture_subtract(1, 2, 1) == 1);
	if (failures)
		return 1;
	printf("ae_hud_hires: all passed\n");
	return 0;
}
