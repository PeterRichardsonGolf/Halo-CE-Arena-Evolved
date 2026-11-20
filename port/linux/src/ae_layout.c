/*
AE_LAYOUT.C

The AE menus' layout arithmetic (ae_layout.h). No includes but its header:
it builds anywhere, alone, for the unit tests.
*/

#include "ae_layout.h"

/* the menus' coordinates: 640 columns centred in a picture 480 lines tall */
enum { MENU_WIDTH = 640, MENU_HEIGHT = 480 };

void ae_draw_layout(int picture_x, int picture_y, int picture_width, int picture_height, struct ae_layout *layout)
{
	layout->height = (float)AE_LAYOUT_HEIGHT;
	layout->scale = picture_height > 0 ? (float)picture_height / (float)AE_LAYOUT_HEIGHT : 1.0f;
	layout->width = (float)picture_width / layout->scale;
	layout->origin_x = (float)picture_x;
	layout->origin_y = (float)picture_y;
}

void ae_layout_to_pixels(struct ae_layout const *layout, float x, float y, float *pixel_x, float *pixel_y)
{
	*pixel_x = layout->origin_x + x * layout->scale;
	*pixel_y = layout->origin_y + y * layout->scale;
}

void ae_layout_from_pixels(struct ae_layout const *layout, float pixel_x, float pixel_y, float *x, float *y)
{
	*x = (pixel_x - layout->origin_x) / layout->scale;
	*y = (pixel_y - layout->origin_y) / layout->scale;
}

void ae_layout_view(float x, float y, float width, float height, struct ae_view *view)
{
	view->x = x;
	view->y = y;
	view->width = width;
	view->height = height;
	view->scale = height > 0.0f ? height / (float)AE_LAYOUT_HEIGHT : 1.0f;
}

float ae_view_width(struct ae_view const *view)
{
	return view->width / view->scale;
}

void ae_view_to_layout(struct ae_view const *view, float x, float y, float *layout_x, float *layout_y)
{
	*layout_x = view->x + x * view->scale;
	*layout_y = view->y + y * view->scale;
}

void ae_view_from_layout(struct ae_view const *view, float layout_x, float layout_y, float *x, float *y)
{
	*x = (layout_x - view->x) / view->scale;
	*y = (layout_y - view->y) / view->scale;
}

void ae_layout_from_menu_point(short menu_x, short menu_y, int screen_width, float layout_width, float *x, float *y)
{
	float width = screen_width > 0 ? (float)screen_width : (float)MENU_WIDTH;

	/* (the menus' point is a whole pixel, floored (d3d8_gl.c ui_point_from_window): its middle) */
	*x = ((float)menu_x + 0.5f + (width - (float)MENU_WIDTH) * 0.5f) * layout_width / width;
	*y = ((float)menu_y + 0.5f) * (float)AE_LAYOUT_HEIGHT / (float)MENU_HEIGHT;
}

void ae_layout_to_menu_point(float x, float y, int screen_width, float layout_width, float *menu_x, float *menu_y)
{
	float width = screen_width > 0 ? (float)screen_width : (float)MENU_WIDTH;

	/* (no picture yet, layout_width 0: the menus' own aspect) */
	if (layout_width <= 0.0f)
		layout_width = width * (float)AE_LAYOUT_HEIGHT / (float)MENU_HEIGHT;
	*menu_x = x * width / layout_width - (width - (float)MENU_WIDTH) * 0.5f;
	*menu_y = y * (float)MENU_HEIGHT / (float)AE_LAYOUT_HEIGHT;
}

short ae_prompts_fit(float const *widths, short const *rank, short count, float gap, float room, unsigned char *keep)
{
	short index, kept = 0;
	float total = 0.0f;

	for (index = 0; index < count; index++)
	{
		keep[index] = 1;
		total += widths[index] + (kept > 0 ? gap : 0.0f);
		kept++;
	}
	/* (the lowest priority goes first, the later of equals) */
	while (kept > 0 && total > room)
	{
		short drop = -1, other = 0;

		for (index = 0; index < count; index++)
			if (keep[index] && (drop < 0 || rank[index] >= rank[drop]))
				drop = index;
		keep[drop] = 0;
		kept--;
		total = 0.0f;
		for (index = 0; index < count; index++)
			if (keep[index])
				total += widths[index] + (other++ > 0 ? gap : 0.0f);
	}
	return kept;
}
