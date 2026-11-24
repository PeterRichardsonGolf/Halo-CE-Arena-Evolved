/*
AE_HUD_HIRES.C

High-resolution HUD digits drawn at the stock size (ae_hud_hires.h).
*/

#include "ae_hud_hires.h"

float ae_hud_number_scale(int screen_digit_width)
{
	if (screen_digit_width < AE_HUD_HIRES_DIGIT_WIDTH)
		return 1.0f;
	return (float)AE_HUD_STOCK_DIGIT_WIDTH / (float)screen_digit_width;
}

int ae_hud_multitexture_subtract(int custom_edition, int xbox_subtract, int multiply)
{
	return custom_edition ? multiply : xbox_subtract;
}
