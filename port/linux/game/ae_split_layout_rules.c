/*
AE_SPLIT_LAYOUT_RULES.C

SPLIT SCREEN's layout: the pure rules (ae_split_layout_rules.h).
*/

#include <math.h>
#include <string.h>

#include "ae_split_layout_rules.h"

int ae_split_two_players_value(char const *text)
{
	if (text && !strcmp(text, "top_and_bottom"))
		return AE_SPLIT_TWO_TOP_AND_BOTTOM;
	if (text && !strcmp(text, "side_by_side"))
		return AE_SPLIT_TWO_SIDE_BY_SIDE;
	return AE_SPLIT_TWO_AUTO;
}

int ae_split_side_by_side(int value, int screen_width, int screen_height)
{
	if (value == AE_SPLIT_TWO_SIDE_BY_SIDE)
		return 1;
	if (value == AE_SPLIT_TWO_TOP_AND_BOTTOM || screen_width <= 0 || screen_height <= 0)
		return 0;
	/* (AUTO: width / height >= 11 / 5, in whole numbers) */
	return (long)screen_width * AE_SPLIT_AUTO_WIDE_DENOMINATOR >=
		(long)screen_height * AE_SPLIT_AUTO_WIDE_NUMERATOR;
}

void ae_split_counts(int players, int side_by_side, long *columns, long *rows)
{
	if (players == 2 && side_by_side)
	{
		*columns = 2;
		*rows = 1;
	}
}

int ae_split_bars_narrow(struct ae_split_rect *view)
{
	long width = view->x1 - view->x0;
	long height = view->y1 - view->y0;
	long most = height * AE_SPLIT_BARS_WIDTH / AE_SPLIT_BARS_HEIGHT;
	long margin;

	if (height <= 0 || width <= most)
		return 0;
	margin = (width - most) / 2;
	view->x0 += (int)margin;
	view->x1 = (int)(view->x0 + most);
	return 1;
}

int ae_split_safe_frame(struct ae_split_rect const *view, int inset_height, struct ae_split_rect *safe)
{
	long width = view->x1 - view->x0;
	long height = view->y1 - view->y0;
	/* (main.c's rounding: (short)(size*0.075f+0.5f)) */
	int inset_x = (int)((float)width * 0.075f + 0.5f);
	int inset_y = (int)((float)inset_height * 0.075f + 0.5f);

	if (width <= 2 * inset_x || height <= 2 * inset_y)
		return 0;
	safe->x0 = view->x0 + inset_x;
	safe->x1 = view->x1 - inset_x;
	safe->y0 = view->y0 + inset_y;
	safe->y1 = view->y1 - inset_y;
	return 1;
}

int ae_split_quads(struct ae_split_rect const *screen, struct ae_split_rect const *frame, int side_by_side,
	struct ae_split_rect const *bars_view, struct ae_split_rect *quads)
{
	int middle_x = frame->x0 + (frame->x1 - frame->x0) / 2;
	int middle_y = frame->y0 + (frame->y1 - frame->y0) / 2;
	int count = 0;

	if (side_by_side)
	{
		quads[count].x0 = middle_x - 1;
		quads[count].x1 = middle_x + 1;
		quads[count].y0 = screen->y0;
		quads[count].y1 = screen->y1;
		count++;
		return count;
	}
	quads[count].x0 = screen->x0;
	quads[count].x1 = screen->x1;
	quads[count].y0 = middle_y - 1;
	quads[count].y1 = middle_y + 1;
	count++;
	if (bars_view)
	{
		if (bars_view->x0 > screen->x0)
		{
			quads[count].x0 = screen->x0;
			quads[count].x1 = bars_view->x0;
			quads[count].y0 = screen->y0;
			quads[count].y1 = screen->y1;
			count++;
		}
		if (bars_view->x1 < screen->x1)
		{
			quads[count].x0 = bars_view->x1;
			quads[count].x1 = screen->x1;
			quads[count].y0 = screen->y0;
			quads[count].y1 = screen->y1;
			count++;
		}
	}
	return count;
}

float ae_split_fov_upright_factor(float view_height, float window_height)
{
	if (!(view_height > 0.0f) || !(window_height > 0.0f))
		return 1.0f;
	return window_height / (AE_SPLIT_ONE_VIEW_WINDOW * view_height);
}

float ae_split_fov_tangent_factor(float view_width, float view_height, float window_height)
{
	float across, upright;

	if (!(view_width > 0.0f) || !(view_height > 0.0f) || !(window_height > 0.0f))
		return 1.0f;
	across = AE_SPLIT_FOV_ASPECT / AE_SPLIT_ONE_VIEW_WINDOW * window_height / view_width;
	upright = ae_split_fov_upright_factor(view_height, window_height);
	return sqrtf(across * upright);
}

float ae_split_fov_widen(float angle, float factor)
{
	float widened;

	if (!(angle > 0.0f) || !(angle < 3.14159265f) || factor == 1.0f || !(factor > 0.0f) || !(factor < 100.0f))
		return angle;
	widened = 2.0f * atanf(factor * tanf(angle * 0.5f));
	return widened > 0.0f && widened < 3.14159265f ? widened : angle;
}

float ae_split_viewmodel_band_height(float view_height, float upright, float factor)
{
	if (!(view_height > 0.0f) || !(upright > 0.0f) || !(factor > 0.0f))
		return view_height;
	return view_height * upright / factor;
}

void ae_split_viewmodel_band(float view_width, float view_height, float band_height, struct ae_split_rect *band)
{
	float width = band_height * 16.0f / 9.0f;

	band->x0 = (int)floorf((view_width - width) * 0.5f + 0.5f);
	band->x1 = (int)floorf((view_width + width) * 0.5f + 0.5f);
	band->y1 = (int)floorf(view_height + 0.5f);
	band->y0 = (int)floorf(view_height - band_height + 0.5f);
}

void ae_split_viewmodel_bounds(float view_width, float view_height, float band_height, float *x0, float *x1, float *y0,
	float *y1)
{
	/* (one player's screen: its tangents over the angle, the window being AE_SPLIT_ONE_VIEW_WINDOW of its height) */
	float up = 1.0f / AE_SPLIT_ONE_VIEW_WINDOW;
	float across = AE_SPLIT_FOV_ASPECT * up;
	float band_width = band_height * 16.0f / 9.0f;

	if (!(view_width > 0.0f) || !(view_height > 0.0f) || !(band_height > 0.0f))
	{
		*x0 = *y0 = -1.0f;
		*x1 = *y1 = 1.0f;
		return;
	}
	/* (across: the view is view_width / band_width of the band's 2 across, over x's view height / width) */
	*x1 = across * (view_width / band_width) * view_height / view_width;
	*x0 = -*x1;
	/* (up: the band's 2 up spans its height, from the view's foot) */
	*y0 = -up;
	*y1 = -up + 2.0f * up * view_height / band_height;
}

void ae_split_viewmodel_map(float aspect, float world_tangent, float const world_bounds[4], float weapon_tangent,
	float const weapon_bounds[4], struct ae_split_view_map *map)
{
	/* (bounds: x0, x1, y0, y1. A tangent X shows at (2 X / (aspect t) - (b0 + b1)) / (b1 - b0) of the viewport under
	each projection; the same place under the world's gives X' = ratio X + offset) */
	float weapon_width = weapon_bounds[1] - weapon_bounds[0], weapon_height = weapon_bounds[3] - weapon_bounds[2];
	float world_width = world_bounds[1] - world_bounds[0], world_height = world_bounds[3] - world_bounds[2];

	map->ax = map->ay = 1.0f;
	map->cx = map->cy = 0.0f;
	if (!(aspect > 0.0f) || !(world_tangent > 0.0f) || !(weapon_tangent > 0.0f) || !(weapon_width > 0.0f) ||
		!(weapon_height > 0.0f) || !(world_width > 0.0f) || !(world_height > 0.0f))
	{
		return;
	}
	map->ax = world_tangent / weapon_tangent * world_width / weapon_width;
	map->cx = aspect * world_tangent * 0.5f *
		(world_bounds[0] + world_bounds[1] - (weapon_bounds[0] + weapon_bounds[1]) * world_width / weapon_width);
	map->ay = world_tangent / weapon_tangent * world_height / weapon_height;
	map->cy = world_tangent * 0.5f *
		(world_bounds[2] + world_bounds[3] - (weapon_bounds[2] + weapon_bounds[3]) * world_height / weapon_height);
}

float ae_split_hud_share_factor(int side_by_side, int bars, float screen_width, float view_width)
{
	if (side_by_side)
		return 0.5f;
	if (bars && view_width > 0.0f && screen_width > view_width)
		return screen_width / view_width;
	return 1.0f;
}
