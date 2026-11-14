/*
AE_HEIGHT_FADE.C

The numbers of ae_height_fade.h, as the game last set them.
*/

#include "ae_height_fade.h"

float ae_height_fade_value[5];

void ae_height_fade_set(float inverse_softness, float height, float offset, float scale, float band)
{
	ae_height_fade_value[0] = inverse_softness;
	ae_height_fade_value[1] = height;
	ae_height_fade_value[2] = offset;
	ae_height_fade_value[3] = scale;
	ae_height_fade_value[4] = band;
}

void ae_height_fade_clear(void)
{
	ae_height_fade_set(0.f, 0.f, 0.f, 0.f, 0.f);
}
