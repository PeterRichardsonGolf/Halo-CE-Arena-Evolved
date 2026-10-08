#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_draw_stub.h"
#include "ae_font.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_widgets.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s (case %s)\n", __FILE__, __LINE__, #c, case_name); failures++; } } while (0)
static const char *case_name = "-";

static int near(float a, float b, float tolerance)
{
	return fabsf(a - b) <= tolerance;
}

/* the densities every check runs at: FULL 1920x1080 100 %, FULL 1280x720 130 %, VIEW quarters 640x360 and 960x540 */
struct density_case
{
	const char *name;
	float window_height;
	int view;            /* nonzero: VIEW, drawn in the top-left quarter */
	float ui_scale;
	float view_width, view_height;   /* pixels (VIEW) */
};
static const struct density_case cases[] =
{
	{ "FULL 1920x1080 100%", 1080, 0, 1.0f, 0, 0 },
	{ "FULL 1280x720 130%", 720, 0, 1.3f, 0, 0 },
	{ "VIEW 640x360", 720, 1, 1.0f, 640, 360 },
	{ "VIEW 960x540", 1080, 1, 1.0f, 960, 540 },
};

/* a frame for a case: the stub reset, the view set, the density; the row width (drawing units) */
static float set_up(struct density_case const *c, struct ae_density *d)
{
	case_name = c->name;
	ae_stub_reset(1920, c->window_height);
	ae_hits_clear();
	if (c->view)
	{
		struct ae_rect panel;

		/* (the quarter: half the 1920 x 1080 layout each way) */
		ae_draw_view(0, 0, 960, 540);
		ae_density_view(c->view_width, c->view_height, c->ui_scale, d);
		ae_view_panel_rect(c->view_width, c->view_height, c->ui_scale, &panel);
		return panel.width * d->pixel;
	}
	ae_density_full(c->window_height, c->ui_scale, d);
	return 800.0f * d->unit;
}

static struct ae_stub_call const *text_call(const char *text)
{
	int index = ae_stub_find_text(text, 0);

	return index >= 0 ? ae_stub_get(index) : NULL;
}

/* the last call of a kind with a colour, -1 */
static int find_call(int kind, unsigned int rgba)
{
	int index;

	for (index = ae_stub_count() - 1; index >= 0; index--)
		if (ae_stub_get(index)->kind == kind && ae_stub_get(index)->rgba == rgba)
			return index;
	return -1;
}

/* a list's item: a row "ROW n" / "VALUE" with the list's flags */
struct list_context { struct ae_density const *density; short hit_id; };
static unsigned int item_flags[64];
static void draw_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct list_context const *list = context;
	struct ae_row row;
	char label[32];

	(void)height;
	memset(&row, 0, sizeof(row));
	snprintf(label, sizeof(label), "ROW %d", item + 1);
	row.label = label;
	row.value = "VALUE";
	row.flags = flags;
	if (item < 64)
		item_flags[item] = flags;
	ae_widget_row(list->density, x, y, width, &row, list->hit_id, item);
}

static void rows(struct density_case const *c)
{
	struct ae_density d;
	struct ae_row row;
	struct ae_row_layout layout;
	struct ae_stub_call const *call;

	float width = set_up(c, &d), allowance;
	int index;

	allowance = 2.0f * d.pixel;
	/* long text: label cut, value whole */
	memset(&row, 0, sizeof(row));
	row.label = "AN EXTREMELY LONG DISPLAY NAME THAT GOES ON";
	row.value = "TEAM AE COMP SLAYER";
	ae_row_layout(&d, width, &row, &layout);
	CHECK(layout.label_cut && strstr(layout.label, "\xE2\x80\xA6"));
	CHECK(layout.label_x + layout.label_width <= layout.value_x - 16 * d.unit + 0.01f);
	CHECK(near(layout.value_x + layout.value_width, width - d.metrics->pad * d.unit, 0.01f));
	ae_widget_row(&d, 0, 0, width, &row, 1, 0);
	CHECK(ae_stub_find_text("TEAM AE COMP SLAYER", 0) >= 0);         /* never shortened */
	CHECK(ae_stub_find_text(layout.label, 0) >= 0 && ae_stub_find_text(row.label, 0) < 0);
	/* (the drawn label ends before the value, its ink allowed 2 px) */
	call = text_call(layout.label);
	CHECK(call && call->x + call->width <= layout.value_x - 16 * d.unit + allowance);
	/* the same with focus (arrows) and changed (struck base, dot): the label still clear of the right block */
	row.flags = AE_ROW_FOCUSED | AE_ROW_CHANGED | AE_ROW_AE;
	row.base_value = "TEAM AE CASUAL SLAYER";
	ae_row_layout(&d, width, &row, &layout);
	CHECK(layout.label_cut);
	/* (the indicators and the label stay clear of the right block: the base value shortened or left out first) */
	CHECK(layout.label_x + layout.label_width <= (layout.base[0] ? layout.base_x : layout.arrow_left_x) - 16 * d.unit +
		0.01f);
	CHECK(!layout.badge || layout.badge_x + layout.badge_width <=
		(layout.base[0] ? layout.base_x : layout.arrow_left_x) - 16 * d.unit + 0.01f);
	CHECK(!layout.base[0] || !strcmp(layout.base, row.base_value) || strstr(layout.base, "\xE2\x80\xA6"));
	CHECK(layout.arrow_left_x + layout.arrow_cell <= layout.value_x + 0.01f &&
		layout.value_x + layout.value_width <= layout.arrow_right_x + 0.01f);
	/* a value wider than the row: whole, but clipped inside the row's padding */
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	memset(&row, 0, sizeof(row));
	row.label = "X";
	row.value = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
	ae_widget_row(&d, 0, 0, width, &row, 1, 0);
	call = text_call(row.value);
	CHECK(call && call->width > width);
	CHECK(call && call->clipped && call->clip[0] >= 0.0f && call->clip[2] <= width &&
		near(call->clip[0], d.metrics->pad * d.unit - allowance, 0.01f) &&
		near(call->clip[2], width - d.metrics->pad * d.unit + allowance, 0.01f));
	/* the label had no room: drawn as nothing rather than over the value */
	CHECK(ae_stub_find_text("X", 0) < 0);
	/* the value's hit is cut to the row (none left of it) */
	{
		struct ae_view view;
		struct ae_hit hit;
		float lx, ly;

		ae_hits_clear();
		ae_widget_row(&d, 0, 0, width, &row, 1, 0);
		ae_draw_current_view(&view);
		ae_view_to_layout(&view, 1.0f, 10.0f, &lx, &ly);
		CHECK(ae_hit_at(lx, ly, 0, &hit) && hit.part == AE_PART_VALUE && hit.rect.x >= view.x - 0.01f);
		ae_view_to_layout(&view, -1.0f, 10.0f, &lx, &ly);
		CHECK(!ae_hit_at(lx, ly, 0, &hit) || hit.rect.x >= view.x - 0.01f);
	}
	/* disabled: the reason replaces the value, in warning colour; the label muted; the row at 43 % */
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	memset(&row, 0, sizeof(row));
	row.label = "V-SYNC";
	row.value = "ON";
	row.reason = "Needs FULLSCREEN";
	row.flags = AE_ROW_DISABLED;
	ae_widget_row(&d, 0, 0, width, &row, 1, 0);
	call = text_call("Needs FULLSCREEN");
	CHECK(call && call->rgba == AE_COLOR_WARNING && call->align == AE_ALIGN_RIGHT);
	CHECK(ae_stub_find_text("ON", 0) < 0);
	call = text_call("V-SYNC");
	CHECK(call && call->rgba == AE_COLOR_DISABLED_TEXT);
	CHECK(find_call(AE_STUB_RECT, (AE_COLOR_ROW & 0xFFFFFF00u) | 81u) >= 0);   /* .74 x .43 of 255 */
	/* focused and disabled: the 59 % bar and a warning notch, still focusable (no arrows) */
	row.flags = AE_ROW_DISABLED | AE_ROW_FOCUSED;
	ae_widget_row(&d, 0, 100, width, &row, 1, 1);
	CHECK(find_call(AE_STUB_RECT, AE_COLOR_DISABLED_BAR) >= 0 && find_call(AE_STUB_RECT, AE_COLOR_WARNING) >= 0);
	CHECK(ae_stub_find_text("\xE2\x96\xB6", 0) < 0);
	/* changed + AE: dot 12 u after the label, badge after the dot, struck base left of the value (a 2 u rect
	through it) */
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	memset(&row, 0, sizeof(row));
	row.label = "HEALTH";
	row.value = "HALO 2";
	row.base_value = "REACH";
	row.flags = AE_ROW_CHANGED | AE_ROW_AE;
	ae_row_layout(&d, width, &row, &layout);
	ae_widget_row(&d, 0, 0, width, &row, 1, 0);
	index = find_call(AE_STUB_RECT, AE_COLOR_ACCENT);
	CHECK(index >= 0);
	if (index >= 0)
	{
		call = ae_stub_get(index);
		CHECK(near(call->width, 10 * d.unit, 0.01f) &&
			near(call->x, layout.label_x + layout.label_width + 12 * d.unit, 0.01f));
		CHECK(layout.badge_x >= call->x + call->width + 8 * d.unit - 0.01f);
	}
	index = find_call(AE_STUB_OUTLINE, AE_COLOR_ACCENT);
	CHECK(index >= 0 && near(ae_stub_get(index)->x, layout.badge_x, 0.01f));
	call = text_call("REACH");
	CHECK(call && call->rgba == AE_COLOR_MUTED && call->x + call->width <= layout.value_x - 12 * d.unit + 0.01f);
	index = find_call(AE_STUB_RECT, AE_COLOR_MUTED);
	CHECK(index >= 0 && call && near(ae_stub_get(index)->height, 2 * d.unit, 0.01f) &&
		near(ae_stub_get(index)->x, call->x, 0.01f) && near(ae_stub_get(index)->width, call->width, 0.01f));
	/* the badge's text: "AE", on the bar a dark fill with accent text */
	CHECK(ae_stub_find_text("AE", 0) >= 0);
	row.flags |= AE_ROW_FOCUSED;
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	ae_widget_row(&d, 0, 0, width, &row, 1, 0);
	CHECK(find_call(AE_STUB_RECT, AE_COLOR_SELECTION_TEXT) >= 0 && text_call("AE") &&
		text_call("AE")->rgba == AE_COLOR_ACCENT);
	CHECK(text_call("HALO 2") && text_call("HALO 2")->rgba == AE_COLOR_SELECTION_TEXT);
	CHECK(ae_stub_find_text("\xE2\x97\x80", 0) >= 0 && ae_stub_find_text("\xE2\x96\xB6", 0) >= 0);
	/* VIEW floors: at 640x360 the row text is 16 px, a minor text 14 px */
	if (c->view && c->view_height == 360)
	{
		call = text_call("HEALTH");
		CHECK(call && near(ae_stub_text_em_pixels(call), 16, .1f));
		call = text_call("REACH");
		CHECK(call && near(ae_stub_text_em_pixels(call), 14, .1f));
		/* (the AE badge: 12 view u is 8 px here, floored to 14; its box tall enough for it) */
		call = text_call("AE");
		CHECK(call && near(ae_stub_text_em_pixels(call), 14, .1f));
		index = find_call(AE_STUB_RECT, AE_COLOR_SELECTION_TEXT);   /* (focused: the badge's dark fill) */
		CHECK(index >= 0 && ae_stub_get(index)->height * 360.0f / 1080.0f >= 14.0f * 1.5f - 0.1f);
	}
	/* the value arrows' cells and hits; an arrow cell lights under the mouse */
	ae_stub_reset(1920, c->window_height);
	ae_hits_clear();
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	memset(&row, 0, sizeof(row));
	row.label = "FRAME RATE LIMIT";
	row.value = "165";
	row.flags = AE_ROW_FOCUSED | AE_ROW_HOVER_RIGHT;
	ae_row_layout(&d, width, &row, &layout);
	CHECK(near(layout.arrow_cell, 28.6f * d.unit, 0.01f));
	ae_widget_row(&d, 0, 0, width, &row, 3, 7);
	index = find_call(AE_STUB_RECT, AE_COLOR_ARROW_HOVER);
	CHECK(index >= 0 && near(ae_stub_get(index)->x, layout.arrow_right_x, 0.01f));
	{
		struct ae_hit hit;
		struct ae_view view;
		float lx, ly;

		ae_draw_current_view(&view);
		ae_view_to_layout(&view, layout.arrow_right_x + layout.arrow_cell * 0.5f, layout.height * 0.5f, &lx, &ly);
		CHECK(ae_hit_at(lx, ly, 0, &hit) && hit.id == 3 && hit.part == AE_PART_ARROW_RIGHT && hit.index == 7);
		ae_view_to_layout(&view, layout.label_x + 2, layout.height * 0.5f, &lx, &ly);
		CHECK(ae_hit_at(lx, ly, 0, &hit) && hit.part == AE_PART_ROW && hit.index == 7);
	}
	/* a value tick: the arrow cell on the pressed side flashes for the tick's time (spec 7), the old value goes out */
	{
		struct ae_motion tick;

		memset(&tick, 0, sizeof(tick));
		ae_motion_set_reduced(0);
		ae_motion_set_now(1000);
		ae_motion_start(&tick, 0, 1, AE_MOTION_VALUE_TICK_MS);
		ae_motion_set_now(1060);
		ae_stub_reset(1920, c->window_height);
		if (c->view)
			ae_draw_view(0, 0, 960, 540);
		row.flags = AE_ROW_FOCUSED;
		row.tick = &tick;
		row.tick_side = -1;
		row.old_value = "144";
		ae_widget_row(&d, 0, 0, width, &row, 3, 7);
		index = find_call(AE_STUB_RECT, AE_COLOR_ARROW_HOVER);
		CHECK(index >= 0 && near(ae_stub_get(index)->x, layout.arrow_left_x, 0.01f));
		CHECK(ae_stub_find_text("144", 0) >= 0 && ae_stub_find_text("165", 0) >= 0);
		ae_motion_set_now(1200);
		ae_stub_reset(1920, c->window_height);
		if (c->view)
			ae_draw_view(0, 0, 960, 540);
		ae_widget_row(&d, 0, 0, width, &row, 3, 7);
		CHECK(find_call(AE_STUB_RECT, AE_COLOR_ARROW_HOVER) < 0 && ae_stub_find_text("144", 0) < 0);
	}
	/* the group header: Overpass 750 at the group size, muted, tracked .09 em */
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	CHECK(near(ae_widget_group(&d, 0, 0, width, "DISPLAY"), d.metrics->group_height * d.unit, 0.01f));
	call = text_call("DISPLAY");
	CHECK(call && call->font == AE_FONT_BODY && call->rgba == AE_COLOR_MUTED && near(call->thickness, 0.09f, 1e-6f) &&
		near(call->x, 10 * d.unit, 0.01f));
	/* (its size: 18 u FULL, 15 view u VIEW, never below the 14 px minor floor: 10 px at a 720p quarter otherwise) */
	if (c->view && c->view_height == 360)
		CHECK(call && near(ae_stub_text_em_pixels(call), 14, .1f));
	else
		CHECK(call && near(call->size, d.metrics->group * d.unit, 0.01f));
	CHECK(!ae_stub_overflowed());
}

static void lists(struct density_case const *c)
{
	struct ae_density d;
	struct ae_list_view view;
	struct list_context context;
	struct ae_pointer pointer;
	struct ae_hit hit;
	struct ae_event event;
	float width = set_up(c, &d), height = 700.0f * d.unit;
	short rows, focus;
	int index;

	if (c->view)
		height = 400.0f * d.pixel;
	context.density = &d;
	context.hit_id = 9;
	rows = ae_list_view_rows(&d, height);
	CHECK(rows >= 3);
	ae_motion_set_reduced(1);
	ae_list_view_init(&view, 40, rows);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	CHECK(view.list.rows == rows && view.list.focus == 0 && ae_stub_find_text("ROW 1", 0) >= 0);
	/* "▼ N more" below, nothing above; the rows clipped to the list */
	{
		char more[32];

		snprintf(more, sizeof(more), "\xE2\x96\xBC %d more", 40 - rows);
		CHECK(ae_stub_find_text(more, 0) >= 0);
		CHECK(text_call("ROW 1") && text_call("ROW 1")->clipped);
	}
	/* events: down moves (cursor), page down by the visible rows minus one */
	event.player = 0; event.action = AE_ACTION_DOWN; event.device = AE_DEVICE_XBOX; event.repeat = 0;
	ae_sound_reset();
	CHECK(ae_list_view_event(&view, &event) && view.list.focus == 1 && ae_sound_take(0) == AE_SOUND_CURSOR);
	event.action = AE_ACTION_PAGE_DOWN;
	CHECK(ae_list_view_event(&view, &event) && view.list.focus == 1 + rows - 1);
	event.action = AE_ACTION_UP; event.repeat = 1;
	CHECK(ae_list_view_event(&view, &event) && view.list.focus == rows - 1);
	event.action = AE_ACTION_ACCEPT;
	CHECK(!ae_list_view_event(&view, &event));
	/* wheel keeps focus: the pointer over the 4th visible row, not moved, a notch down: the window scrolls 3, the
	focus stays (still in the window) */
	CHECK(rows >= 6);
	ae_list_view_init(&view, 40, rows);
	ae_list_set_focus(&view.list, 4);
	ae_stub_reset(1920, c->window_height);
	ae_hits_clear();
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	{
		struct ae_view current;
		float row_y = 0;

		ae_draw_current_view(&current);
		/* (the 4th row's middle, from its hit) */
		for (index = ae_stub_count() - 1; index >= 0; index--)
			if (ae_stub_get(index)->kind == AE_STUB_TEXT && !strcmp(ae_stub_get(index)->text, "ROW 4"))
				row_y = ae_stub_get(index)->y;
		memset(&pointer, 0, sizeof(pointer));
		ae_view_to_layout(&current, 40 * d.unit, row_y, &pointer.x, &pointer.y);
		CHECK(ae_hit_at(pointer.x, pointer.y, 0, &hit) && hit.id == 9 && hit.index == 3);
		focus = view.list.focus;
		pointer.wheel_steps = -1;
		CHECK(ae_list_view_pointer(&view, &pointer, 9) == -1);
		CHECK(view.list.focus == focus && view.list.first == 3);
		/* drawn again (the rows under the pointer have moved), then moved over a row: it takes the focus (hover),
		never scrolling */
		ae_stub_reset(1920, c->window_height);
		ae_hits_clear();
		if (c->view)
			ae_draw_view(0, 0, 960, 540);
		ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
		CHECK(ae_hit_at(pointer.x, pointer.y, 0, &hit) && hit.index == 6);
		pointer.wheel_steps = 0;
		pointer.moved = 1;
		ae_list_view_pointer(&view, &pointer, 9);
		CHECK(view.list.focus == 6 && view.hover == 6 && view.mouse == 1 && view.list.first == 3);
		/* a click: the item, the forward sound */
		pointer.moved = 0;
		pointer.left_clicks = 1;
		ae_sound_reset();
		CHECK(ae_list_view_pointer(&view, &pointer, 9) == 6 && ae_sound_take(0) == AE_SOUND_FORWARD);
	}
	/* hits are layout units even inside a right-hand view */
	ae_stub_reset(1920, c->window_height);
	ae_hits_clear();
	ae_draw_view(960, 0, 960, 540);
	ae_list_view_init(&view, 40, rows);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	{
		struct ae_view current;
		struct ae_stub_call const *first_row = text_call("ROW 1");
		float lx, ly;

		ae_draw_current_view(&current);
		CHECK(first_row != NULL);
		if (first_row)
		{
			ae_view_to_layout(&current, first_row->x, first_row->y, &lx, &ly);
			CHECK(lx >= 960 && ae_hit_at(lx, ly, 0, &hit) && hit.id == 9 && hit.index == 0 && hit.rect.x >= 960);
			/* (and nothing of it at the same point of the left half) */
			CHECK(!ae_hit_at(lx - 960, ly, 0, &hit));
		}
	}
	/* roster shrink under the focus (Review Focus 4): the focus on the last item, the bar's target the new row */
	ae_motion_set_reduced(0);
	ae_motion_set_now(5000);
	ae_list_view_init(&view, 8, rows);
	ae_list_set_focus(&view.list, 5);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	ae_list_set_count(&view.list, 2);
	CHECK(view.list.focus == 1);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	CHECK(view.bar.to == 1.0f && view.previous_focus == 1);
	/* the moving bar: a row's text turns dark once the bar is more than half over it (spec 7) */
	ae_motion_set_now(6000);
	ae_list_view_init(&view, 8, rows);
	ae_list_set_focus(&view.list, 1);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	ae_motion_set_now(7000);
	ae_list_set_focus(&view.list, 2);                        /* the bar from row 2 to row 3 */
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	ae_motion_set_now(7020);                                  /* t = .2: eased .488, the bar at 1.488 */
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	CHECK((item_flags[1] & AE_ROW_ON_BAR) && !(item_flags[2] & AE_ROW_ON_BAR) && (item_flags[2] & AE_ROW_UNDER_BAR));
	CHECK(text_call("ROW 2") && text_call("ROW 2")->rgba == AE_COLOR_SELECTION_TEXT);
	CHECK(text_call("ROW 3") && text_call("ROW 3")->rgba == AE_COLOR_TEXT);
	ae_motion_set_now(7050);                                  /* t = .5: eased .875, the bar at 1.875 */
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	CHECK(!(item_flags[1] & AE_ROW_ON_BAR) && (item_flags[2] & AE_ROW_ON_BAR));
	CHECK(text_call("ROW 3") && text_call("ROW 3")->rgba == AE_COLOR_SELECTION_TEXT);
	CHECK(text_call("ROW 2") && text_call("ROW 2")->rgba == AE_COLOR_TEXT);
	ae_motion_set_now(7200);                                  /* over: the row draws its own bar again */
	ae_stub_reset(1920, c->window_height);
	if (c->view)
		ae_draw_view(0, 0, 960, 540);
	ae_widget_list(&d, &view, 0, 0, width, height, draw_item, &context, 9);
	CHECK((item_flags[2] & AE_ROW_ON_BAR) && !(item_flags[2] & AE_ROW_BAR_BY_LIST));
	ae_motion_set_reduced(1);
	CHECK(!ae_stub_overflowed());
}

/* the thumb in the list's own coordinates: the same in a left and a right quarter view (the M1 bug) */
static void thumbs(void)
{
	struct ae_density d;
	struct ae_list_view left, right;
	struct list_context context;
	struct ae_rect track[2], thumb[2];
	float ty, th, width, height;
	int side;

	case_name = "thumb";
	ae_scrollbar_thumb(40, 10, 30, 500, 40, &ty, &th); CHECK(near(ty, 375, .01f) && near(th, 125, .01f));
	CHECK(ae_scrollbar_first(40, 10, 500, 125, 375) == 30);
	CHECK(ae_scrollbar_first(40, 10, 500, 125, 0) == 0 && ae_scrollbar_first(40, 10, 500, 125, 999) == 30);
	ae_scrollbar_thumb(1000, 10, 0, 500, 40, &ty, &th); CHECK(near(th, 40, .01f) && ty == 0);
	ae_scrollbar_thumb(1000, 10, 990, 500, 40, &ty, &th); CHECK(near(ty, 460, .01f));
	ae_scrollbar_thumb(5, 10, 0, 500, 40, &ty, &th); CHECK(th == 0);   /* no thumb */
	/* the M1 bug: a list in the left and the right quarter, VIEW density, thumb at the same offset from its track */
	ae_density_view(960, 540, 1.0f, &d);
	context.density = &d;
	context.hit_id = 4;
	width = 400.0f * d.unit;
	height = 300.0f * d.unit;
	ae_motion_set_reduced(1);
	for (side = 0; side < 2; side++)
	{
		struct ae_list_view *view = side ? &right : &left;
		int index;

		ae_stub_reset(1920, 1080);
		ae_draw_view(side ? 960.0f : 0.0f, 0, 960, 540);
		ae_list_view_init(view, 40, 5);
		ae_list_set_focus(&view->list, 25);
		ae_widget_list(&d, view, 30 * d.unit, 50 * d.unit, width, height, draw_item, &context, 4);
		for (index = 0; index < ae_stub_count(); index++)
		{
			struct ae_stub_call const *call = ae_stub_get(index);

			if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_TRACK)
				ae_stub_pixels(call, &track[side]);
			if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_MUTED)
				ae_stub_pixels(call, &thumb[side]);
		}
	}
	CHECK(near(track[1].x - track[0].x, 960, .01f));
	CHECK(near(thumb[0].x - track[0].x, thumb[1].x - track[1].x, .01f) &&
		near(thumb[0].y - track[0].y, thumb[1].y - track[1].y, .01f) && thumb[0].y > track[0].y);
	CHECK(near(thumb[0].height, thumb[1].height, .01f));
	/* the drag: the thumb grabbed and moved to the track's end shows the last rows */
	{
		struct ae_pointer pointer;
		struct ae_hit hit;
		int index;

		ae_hits_clear();
		ae_stub_reset(1920, 1080);
		ae_draw_view(960, 0, 960, 540);
		ae_list_view_init(&right, 40, 5);
		ae_widget_list(&d, &right, 30 * d.unit, 50 * d.unit, width, height, draw_item, &context, 4);
		memset(&pointer, 0, sizeof(pointer));
		for (index = 0; index < ae_stub_count(); index++)
			if (ae_stub_get(index)->kind == AE_STUB_RECT && ae_stub_get(index)->rgba == AE_COLOR_MUTED)
			{
				struct ae_rect thumb_rect;

				ae_stub_pixels(ae_stub_get(index), &thumb_rect);
				/* (pixels to layout units: the window is 1080 tall, one unit a pixel) */
				pointer.x = thumb_rect.x + thumb_rect.width * 0.5f;
				pointer.y = thumb_rect.y + 2;
			}
		CHECK(ae_hit_at(pointer.x, pointer.y, 0, &hit) && hit.part == AE_PART_THUMB);
		pointer.left_clicks = 1;
		pointer.left_held = 1;
		ae_list_view_pointer(&right, &pointer, 4);
		CHECK(right.dragging);
		pointer.left_clicks = 0;
		pointer.moved = 1;
		pointer.y += right.track_height;
		ae_list_view_pointer(&right, &pointer, 4);
		CHECK(right.list.first == 35);
		pointer.left_held = 0;
		pointer.moved = 0;
		ae_list_view_pointer(&right, &pointer, 4);
		CHECK(!right.dragging);
	}
}

static void texts(void)
{
	char out[160], lines[4][160];
	int count;

	case_name = "text";
	ae_stub_reset(1920, 1080);
	CHECK(!ae_fit_text(AE_FONT_ROW, 24, "SHORT", 500, out, sizeof(out)) && !strcmp(out, "SHORT"));
	CHECK(ae_fit_text(AE_FONT_ROW, 24, "A MUCH LONGER LABEL THAN FITS", 150, out, sizeof(out)));
	CHECK(strstr(out, "\xE2\x80\xA6") && ae_draw_text_width(AE_FONT_ROW, 24, out) <= 150);
	CHECK(out[strlen(out) - 4] != ' ');                          /* no space before the ellipsis */
	/* never splits a UTF-8 sequence: "ÉÉÉÉÉÉ" cut keeps whole É's */
	CHECK(ae_fit_text(AE_FONT_ROW, 24, "\xC3\x89\xC3\x89\xC3\x89\xC3\x89\xC3\x89\xC3\x89\xC3\x89\xC3\x89", 60, out,
		sizeof(out)));
	CHECK((strlen(out) - 3) % 2 == 0);
	/* a tiny out: no partial sequence */
	CHECK(ae_fit_text(AE_FONT_ROW, 24, "\xC3\x89\xC3\x89\xC3\x89", 1000, out, 4) && !strcmp(out, "\xC3\x89"));
	CHECK(ae_fit_text(AE_FONT_ROW, 24, "WIDE", 1, out, sizeof(out)) && out[0] == 0);
	/* wrapping at spaces; a word too wide is cut; the last line cut when more text is left */
	count = ae_wrap_text(AE_FONT_BODY, 21, "the quick brown fox jumps over the lazy dog", 200, lines, 4);
	CHECK(count >= 2 && count <= 4 && !strncmp(lines[0], "the", 3));
	CHECK(ae_draw_text_width(AE_FONT_BODY, 21, lines[0]) <= 200 && ae_draw_text_width(AE_FONT_BODY, 21, lines[1]) <= 200);
	count = ae_wrap_text(AE_FONT_BODY, 21, "the quick brown fox jumps over the lazy dog", 200, lines, 2);
	CHECK(count == 2 && strstr(lines[1], "\xE2\x80\xA6"));
	count = ae_wrap_text(AE_FONT_BODY, 21, "SUPERCALIFRAGILISTICEXPIALIDOCIOUS word", 120, lines, 3);
	CHECK(count == 2 && strstr(lines[0], "\xE2\x80\xA6") && !strcmp(lines[1], "word"));
	CHECK(ae_wrap_text(AE_FONT_BODY, 21, "   ", 120, lines, 3) == 0 && ae_wrap_text(AE_FONT_BODY, 21, NULL, 120, lines, 3) == 0);
	/* the separator is a drawn dot, no text */
	{
		struct ae_density d;
		int before = ae_stub_count();

		ae_density_full(1080, 1.0f, &d);
		ae_widget_separator(&d, 100, 50, 4, AE_COLOR_MUTED);
		CHECK(ae_stub_count() == before + 1 && ae_stub_get(before)->kind == AE_STUB_RECT &&
			near(ae_stub_get(before)->width, 4, 1e-4f) && near(ae_stub_get(before)->size, 2, 1e-4f));
		before = ae_stub_count();
		ae_widget_dashed(0, 0, 100, 40, 1, 10, AE_COLOR_RULE);
		CHECK(ae_stub_count() - before == 2 * 5 + 2 * 2);
	}
	CHECK(!ae_stub_overflowed());
}

/* a list at one density: 0, 1 and exactly as many items as rows (no thumb, no "more"); the "N more" line takes no
hover or click (I1); the wheel only over the list; a dragged thumb follows at once; the hits' cap */
static void list_edges(void)
{
	struct ae_density d;
	struct ae_list_view view;
	struct list_context context;
	struct ae_pointer pointer;
	struct ae_hit hit;
	struct ae_event event;
	float width, height;
	short rows, counts[3];
	int index, n, more_lines;
	char more[32];

	case_name = "list edges";
	ae_density_full(1080, 1.0f, &d);
	context.density = &d;
	context.hit_id = 5;
	width = 600;
	height = 500;
	rows = ae_list_view_rows(&d, height);
	counts[0] = 0;
	counts[1] = 1;
	counts[2] = rows;
	ae_motion_set_reduced(1);
	for (n = 0; n < 3; n++)
	{
		ae_stub_reset(1920, 1080);
		ae_hits_clear();
		ae_list_view_init(&view, counts[n], rows);
		ae_widget_list(&d, &view, 100, 100, width, height, draw_item, &context, 5);
		CHECK(find_call(AE_STUB_RECT, AE_COLOR_TRACK) < 0 && find_call(AE_STUB_RECT, AE_COLOR_MUTED) < 0);
		more_lines = 0;
		for (index = 0; index < ae_stub_count(); index++)
			if (ae_stub_get(index)->kind == AE_STUB_TEXT && strstr(ae_stub_get(index)->text, "more"))
				more_lines++;
		CHECK(more_lines == 0);
		CHECK(counts[n] == 0 ? ae_stub_find_text("ROW 1", 0) < 0 : ae_stub_find_text("ROW 1", 0) >= 0);
		snprintf(more, sizeof(more), "ROW %d", counts[n] + 1);
		CHECK(ae_stub_find_text(more, 0) < 0);
		/* (events on an empty list: handled, nothing moves, no sound) */
		event.player = 0; event.action = AE_ACTION_DOWN; event.device = AE_DEVICE_XBOX; event.repeat = 0;
		ae_sound_reset();
		CHECK(ae_list_view_event(&view, &event));
		CHECK(counts[n] > 1 ? view.list.focus == 1 : view.list.focus == (counts[n] ? 0 : -1));
		CHECK(counts[n] > 1 ? ae_sound_take(0) == AE_SOUND_CURSOR : ae_sound_take(0) == AE_SOUND_NONE);
		/* (a click on the first row: its item; on an empty list: nothing) */
		memset(&pointer, 0, sizeof(pointer));
		pointer.x = 150;
		pointer.y = text_call("ROW 1") ? text_call("ROW 1")->y : 100 + 60;
		pointer.left_clicks = 1;
		CHECK(ae_list_view_pointer(&view, &pointer, 5) == (counts[n] ? 0 : -1));
	}
	/* I1: 40 items at rest; the "▼ N more" line below the rows takes no hover and no click */
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_list_view_init(&view, 40, rows);
	ae_widget_list(&d, &view, 100, 100, width, height, draw_item, &context, 5);
	snprintf(more, sizeof(more), "\xE2\x96\xBC %d more", 40 - rows);
	index = ae_stub_find_text(more, 0);
	CHECK(index >= 0);
	if (index >= 0)
	{
		struct ae_stub_call const *call = ae_stub_get(index);

		memset(&pointer, 0, sizeof(pointer));
		pointer.x = 100 + 40;
		pointer.y = call->y + 2;
		CHECK(!ae_hit_at(pointer.x, pointer.y, 0, &hit));
		pointer.moved = 1;
		ae_list_view_pointer(&view, &pointer, 5);
		CHECK(view.list.focus == 0 && view.hover == -1 && view.list.first == 0);
		pointer.moved = 0;
		pointer.left_clicks = 1;
		CHECK(ae_list_view_pointer(&view, &pointer, 5) == -1 && view.list.focus == 0);
		/* (and the item after the last visible row is not drawn at all) */
		snprintf(more, sizeof(more), "ROW %d", rows + 1);
		CHECK(ae_stub_find_text(more, 0) < 0);
	}
	/* M2: the wheel scrolls the list only with the pointer over it */
	memset(&pointer, 0, sizeof(pointer));
	pointer.x = 1500;
	pointer.y = 300;
	pointer.wheel_steps = -1;
	ae_list_view_pointer(&view, &pointer, 5);
	CHECK(view.list.first == 0);
	pointer.x = 140;
	pointer.wheel_steps = -1;
	ae_list_view_pointer(&view, &pointer, 5);
	CHECK(view.list.first == 3);
	/* M6: a dragged thumb is drawn where the window is, at once (no 100 ms trail) */
	ae_motion_set_reduced(0);
	ae_motion_set_now(9000);
	ae_list_view_init(&view, 40, rows);
	ae_widget_list(&d, &view, 100, 100, width, height, draw_item, &context, 5);
	view.dragging = 1;
	ae_list_scroll(&view.list, 20);
	ae_stub_reset(1920, 1080);
	ae_widget_list(&d, &view, 100, 100, width, height, draw_item, &context, 5);
	CHECK(view.scroll.to == (float)view.list.first && ae_motion_value(&view.scroll) == (float)view.list.first);
	view.dragging = 0;
	ae_motion_set_reduced(1);
	/* M4: past AE_HITS_MAXIMUM hits the rest are dropped and flagged; a clear starts over */
	ae_hits_clear();
	for (index = 0; index < AE_HITS_MAXIMUM; index++)
		ae_hit_add((float)index, 0, 1, 1, 1, AE_PART_ROW, 0);
	CHECK(!ae_hits_overflowed());
	ae_hit_add(0, 0, 1, 1, 1, AE_PART_ROW, 0);
	CHECK(ae_hits_overflowed());
	ae_hits_clear();
	CHECK(!ae_hits_overflowed());
	CHECK(!ae_stub_overflowed());
}

int main(void)
{
	int index;

	for (index = 0; index < (int)(sizeof(cases) / sizeof(cases[0])); index++)
	{
		rows(&cases[index]);
		lists(&cases[index]);
	}
	thumbs();
	texts();
	list_edges();
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
