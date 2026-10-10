/*
AE_MOUSE.C

The mouse look of Arena Evolved's MCC style (ae_mouse.h): pure functions.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_mouse.h"

int ae_mouse_style_from_text(char const *text)
{
	static char const mcc[] = "mcc";
	size_t index;

	if (!text)
		return AE_MOUSE_CLASSIC;
	for (index = 0; index < sizeof(mcc) - 1; index++)
	{
		if (tolower((unsigned char)text[index]) != mcc[index])
			return AE_MOUSE_CLASSIC;
	}
	return text[index] ? AE_MOUSE_CLASSIC : AE_MOUSE_MCC;
}

float ae_mouse_classic_radians(float multiplier)
{
	return AE_MOUSE_CLASSIC_SCALE * multiplier;
}

float ae_mouse_mcc_radians(float value)
{
	/* (pi / 8100 = value / 45 degrees) */
	return value * AE_MOUSE_MCC_UNIT;
}

float ae_mouse_clamp_sensitivity(float value)
{
	if (!(value >= AE_MOUSE_MCC_MINIMUM))
		return AE_MOUSE_MCC_MINIMUM;
	return value > AE_MOUSE_MCC_MAXIMUM ? AE_MOUSE_MCC_MAXIMUM : value;
}

float ae_mouse_clamp_scale(float value)
{
	if (!(value >= AE_MOUSE_SCALE_MINIMUM))
		return AE_MOUSE_SCALE_MINIMUM;
	return value > AE_MOUSE_SCALE_MAXIMUM ? AE_MOUSE_SCALE_MAXIMUM : value;
}

float ae_mouse_seed(float classic_multiplier)
{
	/* (in double: 1.0 x 5.6723 must round the same everywhere) */
	double value;

	/* (no usable classic value: the default 1.0's) */
	if (!(classic_multiplier > 0.0f))
		classic_multiplier = 1.0f;
	value = floor((double)classic_multiplier * (double)AE_MOUSE_SEED_RATIO * 10.0 + 0.5) / 10.0;
	return ae_mouse_clamp_sensitivity((float)value);
}

float ae_mouse_zoom_factor(int style, float scale, int zoomed)
{
	return style == AE_MOUSE_MCC && zoomed ? ae_mouse_clamp_scale(scale) : 1.0f;
}

float ae_mouse_vehicle_factor(int style, float scale, int in_vehicle)
{
	return style == AE_MOUSE_MCC && in_vehicle ? ae_mouse_clamp_scale(scale) : 1.0f;
}

void ae_mouse_mcc_look(float value, float x, float y, int invert, float *yaw, float *pitch)
{
	float radians = ae_mouse_mcc_radians(ae_mouse_clamp_sensitivity(value));

	*yaw = -x * radians;
	*pitch = (invert ? y : -y) * radians;
}

int ae_mouse_test_parse(char const *setting, float *dx, float *dy, int *zoom)
{
	float x, y;
	int used = 0;

	if (!setting || strncmp(setting, "mouse:", 6))
		return 0;
	if (sscanf(setting + 6, "%f,%f%n", &x, &y, &used) < 2)
		return 0;
	*zoom = 0;
	if (!strcmp(setting + 6 + used, ",zoom"))
		*zoom = 1;
	else if (setting[6 + used])
		return 0;
	*dx = x;
	*dy = y;
	return 1;
}
