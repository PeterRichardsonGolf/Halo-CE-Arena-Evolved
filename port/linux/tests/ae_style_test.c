#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_style.h"
#include "ae_draw_stub.h"
#include "ae_font.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near(float a, float b, float tolerance)
{
	return fabsf(a - b) <= tolerance;
}

/* the colour table: alpha = round(a x 255) for the spec's fractions */
static void colours(void)
{
	CHECK((AE_COLOR_PANEL & 0xFF) == 209 && (AE_COLOR_RULE & 0xFF) == 51 && (AE_COLOR_ROW & 0xFF) == 189);
	CHECK((AE_COLOR_HOVER & 0xFF) == 36 && (AE_COLOR_SCRIM & 0xFF) == 158 && (AE_COLOR_VIEW_PANEL & 0xFF) == 242);
	CHECK(AE_COLOR_CHIP_DISABLED_TEXT == ((AE_COLOR_MUTED & 0xFFFFFF00u) | 150u));   /* muted .59 */
	CHECK(AE_COLOR_RING == AE_COLOR_SELECTION);
	CHECK(AE_COLOR_DISABLED_TEXT == ((AE_COLOR_MUTED & 0xFFFFFF00u) | 171u));        /* muted .67 */
}

static void frames(void)
{
	struct ae_frame f;

	ae_frame_compute(1920, 1.0f, &f); CHECK(f.rect.x == 0 && f.rect.width == 1920 && f.margin == 96);
	CHECK(f.rect.y == 0 && f.rect.height == 1080 && f.unit == 1.0f);
	ae_frame_compute(1440, 1.0f, &f); CHECK(f.rect.width == 1440 && near(f.margin, 72, .01f));      /* 4:3 */
	ae_frame_compute(1350, 1.0f, &f); CHECK(f.rect.x == 0 && f.rect.width == 1350);                  /* 5:4 */
	ae_frame_compute(2560, 1.0f, &f); CHECK(f.rect.x == 320 && f.rect.width == 1920);                /* 21:9 centred */
	CHECK(near(ae_frame_x(&f, 0.5f), 1280, .01f) && near(ae_frame_y(&f, 0.25f), 270, .01f));
	CHECK(ae_frame_x(&f, 0.0f) == 320 && near(ae_frame_x(&f, 1.0f), 2240, .01f));
	ae_frame_compute(1920, 1.3f, &f); CHECK(near(f.rect.width / f.unit, 1477, 1) && near(1080 / f.unit, 831, 1));
	CHECK(f.margin == 96);   /* (5 % of the frame's width, in layout units) */
}

static void views(void)
{
	/* the §6 table: view, s, panel, row text px */
	static const float table[][6] =
	{
		{ 1920,  540, 1.00f, 470, 464, 22 },
		{  960,  540, 1.00f, 442, 464, 22 },
		{ 1280,  720, 1.33f, 589, 619, 29 },
		{ 1280,  360, 0.67f, 313, 310, 16 },
		{  640,  360, 0.67f, 294, 310, 16 },
		{  720,  540, 0.90f, 331, 464, 20 },
		{ 1920, 1080, 1.60f, 752, 929, 35 },
	};
	struct ae_density d;
	struct ae_rect p;
	int row;

	for (row = 0; row < (int)(sizeof(table) / sizeof(table[0])); row++)
	{
		float const *r = table[row];
		float w = r[0], h = r[1], s = ae_view_scale(w, h, 1.0f);

		CHECK(near(s, r[2], .01f));
		ae_view_panel_rect(w, h, 1.0f, &p);
		CHECK(near(p.width, r[3], .6f) && near(p.height, r[4], .6f));
		CHECK(near(p.x, .03f * w, .01f) && near(p.y, .07f * h, .01f));
		/* the panel never covers the view centre */
		CHECK(p.x + p.width < w / 2);
		ae_density_view(w, h, 1.0f, &d);
		CHECK(d.kind == AE_DENSITY_VIEW && d.metrics == &ae_metrics_view && near(d.s, s, 1e-5f));
		CHECK(near(ae_size_text(&d) / d.pixel, r[5], .6f));
		/* (drawing units: the view's own 1080 over its h pixels) */
		CHECK(near(d.pixel, 1080.0f / h, 1e-4f) && near(d.unit, s * d.pixel, 1e-4f));
	}
	/* floors at the 720p quarter */
	ae_density_view(640, 360, 1, &d);
	CHECK(near(ae_size_text(&d) / d.pixel, 16, .01f));
	CHECK(near(ae_size_minor(&d) / d.pixel, 14, .01f) && near(ae_size_glyph(&d) / d.pixel, 18, .01f));
	CHECK(near(ae_size_row(&d) / d.pixel, 26.67f, .05f));      /* max(40 s, 16 x 1.6) */
	/* no floor needed at 1440p: the spec sizes */
	ae_density_view(1280, 720, 1, &d);
	CHECK(near(ae_size_minor(&d) / d.pixel, 17 * 4.0f / 3, .02f) && near(ae_size_glyph(&d) / d.pixel, 32, .02f));
	CHECK(near(ae_size_row(&d) / d.pixel, 40 * 4.0f / 3, .05f));
	/* ae_size: spec units, the floor only when larger */
	CHECK(near(ae_size(&d, 10, 0) / d.pixel, 13.33f, .01f) && near(ae_size(&d, 10, 20) / d.pixel, 20, .01f));
	/* UI SCALE multiplies s and the panel's cap, never past the 46 % */
	CHECK(near(ae_view_scale(960, 540, 1.3f), 1.3f, 1e-5f) && near(ae_view_scale(4000, 4000, 1.3f), 1.6f * 1.3f, 1e-5f));
	CHECK(near(ae_view_scale(100, 100, 1.0f), 0.6f, 1e-5f));
	ae_view_panel_rect(1920, 540, 1.3f, &p); CHECK(near(p.width, 611, .6f));
	ae_view_panel_rect(960, 540, 1.3f, &p); CHECK(near(p.width, 441.6f, .1f));
	/* the VIEW group header ruling: 30 = 38 x 40 / 50 */
	CHECK(ae_metrics_view.group_height == 30 && ae_metrics_full.group_height == 38);
	CHECK(ae_metrics_full.row == 50 && ae_metrics_full.title == 60 && ae_metrics_full.corner == 2 &&
		ae_metrics_view.row == 40 && ae_metrics_view.title == 0 && ae_metrics_view.emblem == 32);
}

static void scales(void)
{
	struct ae_density d;

	/* UI scale */
	CHECK(ae_ui_scale_from_percent(0) == 1.0f && ae_ui_scale_from_percent(95) == 1.0f &&
		ae_ui_scale_from_percent(120) == 1.15f && ae_ui_scale_from_percent(400) == 1.3f &&
		ae_ui_scale_from_percent(90) == .9f);
	CHECK(ae_ui_scale_from_percent(-5) == 1.0f && ae_ui_scale_from_percent(1) == .9f &&
		ae_ui_scale_from_percent(107) == 1.0f && ae_ui_scale_from_percent(108) == 1.15f &&   /* (100 / 115 meet at 107.5) */
		ae_ui_scale_from_percent(122) == 1.15f && ae_ui_scale_from_percent(123) == 1.3f &&
		ae_ui_scale_from_percent(115) == 1.15f && ae_ui_scale_from_percent(130) == 1.3f);
	/* FULL: 1 u = window_height / 1080 x scale pixels */
	ae_density_full(720, 1.3f, &d); CHECK(near(d.s, 720.0f / 1080 * 1.3f, 1e-4f));
	CHECK(d.kind == AE_DENSITY_FULL && d.metrics == &ae_metrics_full);
	CHECK(near(d.unit, 1.3f, 1e-5f) && near(d.pixel, 1.5f, 1e-5f));
	/* FULL has no floors */
	CHECK(near(ae_size_text(&d), 24 * 1.3f, 1e-4f) && near(ae_size_minor(&d), 18 * 1.3f, 1e-4f));
	CHECK(near(ae_size_glyph(&d), 30 * 1.3f, 1e-4f) && near(ae_size_row(&d), 50 * 1.3f, 1e-4f));
}

/* the recording stub: a 1920x1080 window, text in the right-hand view */
static void stub(void)
{
	struct ae_stub_call const *call;
	struct ae_rect pixels;
	struct ae_view view;
	float top, bottom, width, measured;
	int index;

	ae_stub_reset(1920, 1080);
	CHECK(ae_font_ready(AE_FACE_OPENCE) && ae_font_ready(AE_FACE_OVERPASS_900) && ae_font_ready(AE_FACE_OVERPASS_750));
	ae_draw_view(960, 540, 960, 540);
	ae_draw_current_view(&view);
	CHECK(view.x == 960 && view.scale == 0.5f && near(ae_draw_view_width(), 1920, .01f));
	CHECK(near(ae_draw_units_per_pixel(), 2.0f, 1e-5f));
	width = ae_draw_text(AE_FONT_TITLE, 60, 100, 200, AE_ALIGN_LEFT, AE_COLOR_TITLE, "SETTINGS");
	measured = ae_font_measure(AE_FACE_OPENCE, 60, 0, "SETTINGS", NULL, NULL);
	CHECK(width > 0 && near(width, measured, 1e-4f));
	index = ae_stub_find_text("SETTINGS", 0);
	CHECK(index >= 0 && ae_stub_find_text("SETTINGS", index + 1) == -1 && ae_stub_find_text("SETTING", 0) == -1);
	call = ae_stub_get(index);
	CHECK(call->kind == AE_STUB_TEXT && call->font == AE_FONT_TITLE && call->size == 60 && call->x == 100);
	CHECK(call->view.x == 960 && call->view.y == 540 && call->rgba == AE_COLOR_TITLE && !call->clipped);
	/* its ink box as ae_draw_text_box gives it: 2 px padding (4 of this view's units) */
	ae_draw_text_box(AE_FONT_TITLE, 60, "SETTINGS", &top, &bottom);
	CHECK(call->top == top && call->bottom == bottom && near(call->height, bottom - top, 1e-4f));
	CHECK(top < 0 && bottom > ae_font_cap_height(AE_FACE_OPENCE, 60) + 3.9f);
	ae_stub_pixels(call, &pixels);
	CHECK(pixels.x >= 960 && near(pixels.x, 960 + 50, .01f) && near(pixels.width, width * 0.5f, .01f));
	CHECK(pixels.y > 540 && pixels.y + pixels.height < 1080);
	CHECK(near(ae_stub_text_em_pixels(call), 30, 1e-4f));
	/* tracking and alignment */
	CHECK(near(ae_draw_text_tracked_width(AE_FONT_BODY, 18, 0.09f, "GAME"),
		ae_font_measure(AE_FACE_OVERPASS_750, 18, 0.09f, "GAME", NULL, NULL), 1e-4f));
	ae_draw_text_tracked(AE_FONT_BODY, 18, 0.09f, 500, 10, AE_ALIGN_RIGHT, AE_COLOR_TEXT, "GAME");
	call = ae_stub_get(ae_stub_count() - 1);
	ae_stub_pixels(call, &pixels);
	CHECK(near(pixels.x + pixels.width, 960 + 250, .01f));
	/* a device font's glyphs are square */
	CHECK(ae_draw_button_width(AE_FONT_XBOX, AE_BUTTON_A, 30) == 30 && ae_draw_text_width(AE_FONT_KEYBOARD, 20, "ab") == 40);
	/* a clip pushed in the right-hand view: recorded in that view's units, intersected, popped */
	ae_draw_clip_push(10, 20, 100, 50);
	ae_draw_rect(0, 0, 400, 400, 2, AE_COLOR_ROW);
	call = ae_stub_get(ae_stub_count() - 1);
	CHECK(call->kind == AE_STUB_RECT && call->clipped && near(call->clip[0], 10, 1e-3f) && near(call->clip[1], 20, 1e-3f) &&
		near(call->clip[2], 110, 1e-3f) && near(call->clip[3], 70, 1e-3f));
	ae_draw_clip_push(50, 0, 1000, 1000);
	ae_draw_rect(0, 0, 1, 1, 0, AE_COLOR_ROW);
	call = ae_stub_get(ae_stub_count() - 1);
	CHECK(near(call->clip[0], 50, 1e-3f) && near(call->clip[1], 20, 1e-3f) && near(call->clip[2], 110, 1e-3f) &&
		near(call->clip[3], 70, 1e-3f));
	ae_draw_clip_pop();
	ae_draw_clip_pop();
	ae_draw_rect(0, 0, 1, 1, 0, AE_COLOR_ROW);
	call = ae_stub_get(ae_stub_count() - 1);
	CHECK(!call->clipped && call->clip[0] == 0 && near(call->clip[2], 1920, .01f) && near(call->clip[3], 1080, .01f));
	/* alpha: applied to every colour until changed; view_full resets it, ae_draw_view keeps it */
	ae_draw_set_alpha(0.5f);
	ae_draw_rect(0, 0, 1, 1, 0, 0xFFFFFFFFu);
	CHECK(ae_stub_get(ae_stub_count() - 1)->rgba == 0xFFFFFF80u);
	ae_draw_view(0, 0, 960, 540);
	ae_draw_line(0, 0, 30, 40, 4, 0x102030FFu);
	call = ae_stub_get(ae_stub_count() - 1);
	CHECK(call->kind == AE_STUB_LINE && call->rgba == 0x10203080u && call->x1 == 30 && call->y1 == 40 && call->thickness == 4);
	CHECK(call->view.x == 0 && call->view.width == 960);
	ae_draw_view_full();
	CHECK(ae_stub_get(ae_stub_count() - 1)->kind == AE_STUB_VIEW);
	ae_draw_rect(0, 0, 1, 1, 0, 0xFFFFFFFFu);
	CHECK(ae_stub_get(ae_stub_count() - 1)->rgba == 0xFFFFFFFFu);
	CHECK(!ae_stub_overflowed());
	/* past 8192 calls: not recorded, the overflow flag set; a reset clears it */
	for (index = ae_stub_count(); index <= 8192; index++)
		ae_draw_rect(0, 0, 1, 1, 0, AE_COLOR_ROW);
	CHECK(ae_stub_count() == 8192 && ae_stub_overflowed());
	/* (a fresh frame: nothing recorded, the full view, opaque) */
	ae_stub_reset(1920, 1080);
	CHECK(!ae_stub_overflowed());
	CHECK(ae_stub_count() == 0 && ae_stub_get(0) == NULL);
	ae_draw_current_view(&view);
	CHECK(view.x == 0 && view.width == 1920 && view.height == 1080);
}

int main(void)
{
	colours();
	frames();
	views();
	scales();
	stub();
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
