/*
AE_SCOPE_MASK_RULES.C

Which crosshair item is a scope's mask (ae_scope_mask_rules.h).
*/

#include "ae_scope_mask_rules.h"

static int own_size(float scale)
{
	return scale == 0.0f || scale == 1.0f;
}

int ae_scope_mask_fills_view(int zoom_state, int not_a_sprite, float scale_x, float scale_y, int width, int height)
{
	return zoom_state && not_a_sprite && own_size(scale_x) && own_size(scale_y) && height > 0 && width >= 2 * height;
}

void ae_scope_mask_clip(int interface_bitmap, int width, int height, float *x0, float *x1, float *y0, float *y1)
{
	float w = interface_bitmap ? (float)width : 1.0f;
	float h = interface_bitmap ? (float)height : 1.0f;
	float extra = (AE_SCOPE_MASK_SPAN - 1) * 0.5f;

	*x0 = -w * extra;
	*x1 = w * (1.0f + extra);
	*y0 = -h * extra;
	*y1 = h * (1.0f + extra);
}
