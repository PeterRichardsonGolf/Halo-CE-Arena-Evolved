/* ae_style.c: Arena Evolved menus, the look's metrics, frame and densities (see ae_style.h). No engine includes. */

#include "ae_style.h"

/* (field order: row gap pad text minor sub_line chip chip_text glyph card emblem tab_text help_title body body_line
group group_height badge footer title corner) */
struct ae_metrics const ae_metrics_full =
{
	50, 6, 20, 24, 18, 16, 34, 18, 30, 66, 42, 24, 28, 21, 31, 18, 38, 13, 20, 60, 2,
};
/* (VIEW has no screen title; its group header height, which the spec leaves out, is the FULL one's in the rows'
ratio: 38 x 40 / 50 = 30, a ruling) */
struct ae_metrics const ae_metrics_view =
{
	40, 4, 14, 22, 17, 14, 28, 16, 24, 52, 32, 19, 22, 17, 24, 15, 30, 12, 17, 0, 2,
};

/* the frame's aspect, and the §6 rule's reference view */
/* (a 16:9 box 1080 tall: 1080 x 16 / 9, multiplied first so it is exactly 1920) */
#define FRAME_WIDTH (1080.0f * 16.0f / 9.0f)
/* (margins: 5 % of the frame width, as a division so 1920 gives exactly 96) */
#define FRAME_MARGIN_PARTS 20.0f
#define VIEW_REFERENCE_WIDTH 800.0f
#define VIEW_REFERENCE_HEIGHT 540.0f
#define VIEW_SCALE_LOW 0.6f
#define VIEW_SCALE_HIGH 1.6f
/* §6 panel */
#define PANEL_WIDTH_SHARE 0.46f
#define PANEL_WIDTH_UNITS 470.0f
#define PANEL_HEIGHT_SHARE 0.86f
/* §6 floors, window pixels */
#define FLOOR_TEXT 16.0f
#define FLOOR_MINOR 14.0f
#define FLOOR_GLYPH 18.0f
#define ROW_PER_TEXT 1.6f

static float smaller(float a, float b)
{
	return a < b ? a : b;
}

static float larger(float a, float b)
{
	return a > b ? a : b;
}

float ae_view_scale(float view_width, float view_height, float ui_scale)
{
	float s = smaller(view_height / VIEW_REFERENCE_HEIGHT, view_width / VIEW_REFERENCE_WIDTH);

	s = s < VIEW_SCALE_LOW ? VIEW_SCALE_LOW : s > VIEW_SCALE_HIGH ? VIEW_SCALE_HIGH : s;
	return s * ui_scale;
}

void ae_density_full(float window_height, float ui_scale, struct ae_density *density)
{
	density->kind = AE_DENSITY_FULL;
	density->pixel = window_height > 0.0f ? 1080.0f / window_height : 1.0f;
	density->unit = ui_scale;
	density->s = density->unit / density->pixel;
	density->metrics = &ae_metrics_full;
}

void ae_density_view(float view_width, float view_height, float ui_scale, struct ae_density *density)
{
	density->kind = AE_DENSITY_VIEW;
	density->pixel = view_height > 0.0f ? 1080.0f / view_height : 1.0f;
	density->s = ae_view_scale(view_width, view_height, ui_scale);
	density->unit = density->s * density->pixel;
	density->metrics = &ae_metrics_view;
}

float ae_size(struct ae_density const *density, float spec_units, float floor_pixels)
{
	return larger(spec_units * density->unit, floor_pixels * density->pixel);
}

/* a floor that only VIEW has */
static float view_floor(struct ae_density const *density, float pixels)
{
	return density->kind == AE_DENSITY_VIEW ? pixels : 0.0f;
}

float ae_size_text(struct ae_density const *density)
{
	return ae_size(density, density->metrics->text, view_floor(density, FLOOR_TEXT));
}

float ae_size_minor(struct ae_density const *density)
{
	return ae_size(density, density->metrics->minor, view_floor(density, FLOOR_MINOR));
}

float ae_size_glyph(struct ae_density const *density)
{
	return ae_size(density, density->metrics->glyph, view_floor(density, FLOOR_GLYPH));
}

float ae_size_row(struct ae_density const *density)
{
	float row = density->metrics->row * density->unit;

	return density->kind == AE_DENSITY_VIEW ? larger(row, ae_size_text(density) * ROW_PER_TEXT) : row;
}

void ae_view_panel_rect(float view_width, float view_height, float ui_scale, struct ae_rect *panel)
{
	/* (centred in the view on both axes: the owner's decision, over spec 6's top-left place; it covers the crosshair
	while open) */
	panel->width = smaller(PANEL_WIDTH_SHARE * view_width,
		PANEL_WIDTH_UNITS * ae_view_scale(view_width, view_height, ui_scale));
	panel->height = PANEL_HEIGHT_SHARE * view_height;
	panel->x = (view_width - panel->width) * 0.5f;
	panel->y = (view_height - panel->height) * 0.5f;
}

void ae_frame_compute(float layout_width, float ui_scale, struct ae_frame *frame)
{
	float width = smaller(layout_width, FRAME_WIDTH);

	frame->rect.x = (layout_width - width) * 0.5f;
	frame->rect.y = 0.0f;
	frame->rect.width = width;
	frame->rect.height = 1080.0f;
	frame->margin = width / FRAME_MARGIN_PARTS;
	frame->unit = ui_scale;
}

float ae_frame_x(struct ae_frame const *frame, float fraction)
{
	return frame->rect.x + fraction * frame->rect.width;
}

float ae_frame_y(struct ae_frame const *frame, float fraction)
{
	return frame->rect.y + fraction * frame->rect.height;
}

float ae_volume_from_config(double volume)
{
	/* (written so a NaN fails the first test) */
	return !(volume >= 0.0) ? 0.0f : volume > 1.0 ? 1.0f : (float)volume;
}

float ae_ui_scale_from_percent(int percent)
{
	static const int steps[] = { 90, 100, 115, 130 };
	static const float scales[] = { 0.9f, 1.0f, 1.15f, 1.3f };
	int index, best = 1;

	if (percent <= 0)
		return 1.0f;
	for (index = 0; index < 4; index++)
	{
		int distance = percent > steps[index] ? percent - steps[index] : steps[index] - percent;
		int best_distance = percent > steps[best] ? percent - steps[best] : steps[best] - percent;

		/* (a tie goes up: the later step) */
		if (distance <= best_distance)
			best = index;
	}
	return scales[best];
}
