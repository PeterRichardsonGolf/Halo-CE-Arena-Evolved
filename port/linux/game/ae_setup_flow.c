/*
AE_SETUP_FLOW.C

What AE's menu screens of rows share. See ae_setup_flow.h.
*/

#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_setup_flow.h"
#include "ae_setup_options.h"
#include "ae_sound.h"
#include "ae_strings.h"

/* sizes, u */
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define HELP_GAP_U 28.0f
#define FOOTER_ABOVE_U 62.0f
#define FOOTER_ROOM_U 40.0f

float ae_flow_u(struct ae_flow_screen const *flow, float spec_units)
{
	return spec_units * flow->frame.unit;
}

float ae_flow_left_x(struct ae_flow_screen const *flow)
{
	return flow->frame.rect.x + flow->frame.margin;
}

float ae_flow_right_x(struct ae_flow_screen const *flow)
{
	return flow->frame.rect.x + flow->frame.rect.width - flow->frame.margin;
}

float ae_flow_cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

void ae_flow_full_density(struct ae_flow_screen *flow)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &flow->density);
	ae_frame_compute(layout.width, scale, &flow->frame);
}

void ae_flow_title(struct ae_flow_screen *flow, const char *text, const char *crumb)
{
	float size = ae_flow_u(flow, TITLE_U), top = ae_frame_y(&flow->frame, AE_FLOW_TITLE_Y);
	float crumb_size = ae_flow_u(flow, CRUMB_U), middle = top + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;

	ae_draw_text(AE_FONT_TITLE, size, ae_flow_left_x(flow), top, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	if (crumb)
		ae_draw_text_tracked(AE_FONT_BODY, crumb_size, CRUMB_TRACKING, ae_flow_right_x(flow),
			ae_flow_cap_top(AE_FONT_BODY, crumb_size, middle), AE_ALIGN_RIGHT, AE_COLOR_TEXT, crumb);
	ae_draw_rect(ae_flow_left_x(flow), ae_frame_y(&flow->frame, AE_FLOW_RULE_Y), ae_flow_right_x(flow) -
		ae_flow_left_x(flow), ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
}

float ae_flow_content_bottom(struct ae_flow_screen const *flow)
{
	return flow->frame.rect.height - ae_flow_u(flow, FOOTER_ABOVE_U) - ae_flow_u(flow, FOOTER_ROOM_U);
}

/* a row's height with the gap under it (a header's has none) */
static float flow_row_pitch(struct ae_flow_screen const *flow, short index)
{
	struct ae_density const *d = &flow->density;

	if (flow->rows[index].kind == AE_FLOW_ROW_HEADER)
		return ae_widget_group(d, 0.0f, 0.0f, 0.0f, NULL);
	return ae_size_row(d) + ae_flow_u(flow, d->metrics->gap) +
		(flow->rows[index].primary ? ae_flow_u(flow, AE_FLOW_PRIMARY_ROOM_U) : 0.0f);
}

/* the last row drawn from first in height: the "▲ N more" line's room taken when rows are above, the "▼ N more"
line's when rows are left below; first at the least */
static short flow_last_row(struct ae_flow_screen const *flow, short first, float height)
{
	float more = ae_widget_more_line(&flow->density), room, used;
	short index, pass;

	for (pass = 0; pass < 2; pass++)
	{
		room = height - (first > 0 ? more : 0.0f) - (pass ? more : 0.0f);
		used = 0.0f;
		for (index = first; index < flow->count; index++)
		{
			used += flow_row_pitch(flow, index);
			if (used > room)
				break;
		}
		if (index >= flow->count)
			return (short)(flow->count - 1);
		if (pass)
			return index > first ? (short)(index - 1) : first;
	}
	return first;
}

/* the rows (not headers) from first to last */
static int flow_rows_between(struct ae_flow_screen const *flow, short first, short last)
{
	int count = 0;
	short index;

	for (index = first; index <= last; index++)
		count += flow->rows[index].kind != AE_FLOW_ROW_HEADER;
	return count;
}

/* first moved so the focus shows (with its header when it is the first row under one), and back up while the rows
below it would fit */
static void flow_scroll_to_focus(struct ae_flow_screen *flow, float height)
{
	short focus = flow->focus;

	if (flow->first < 0 || flow->first >= flow->count)
		flow->first = 0;
	if (focus < 0 || focus >= flow->count)
		return;
	if (focus < flow->first)
		flow->first = focus;
	if (flow->first == focus && focus > 0 && flow->rows[focus - 1].kind == AE_FLOW_ROW_HEADER)
		flow->first = (short)(focus - 1);
	while (flow->first < focus && flow_last_row(flow, flow->first, height) < focus)
		flow->first++;
	while (flow->first > 0 && flow_last_row(flow, (short)(flow->first - 1), height) == flow->count - 1)
		flow->first--;
}

/* a row drawn at *y (its widget's look, its rect kept), *y past it and its gap; a primary row's room above it first */
static void flow_draw_row(struct ae_flow_screen *flow, short index, float x, float *y, float width)
{
	struct ae_density const *d = &flow->density;
	struct ae_flow_row const *row = &flow->rows[index];
	struct ae_row widget;

	if (row->kind == AE_FLOW_ROW_HEADER)
	{
		*y += ae_widget_group(d, x, *y, width, row->label);
		flow->rects[index].width = 0.0f;
		return;
	}
	memset(&widget, 0, sizeof(widget));
	widget.label = row->label;
	widget.value = row->value;
	if (row->primary)
	{
		*y += ae_flow_u(flow, AE_FLOW_PRIMARY_ROOM_U);
		widget.flags |= AE_ROW_PRIMARY;
	}
	/* (a quiet line: never lit, its help still shown) */
	if (index == flow->focus && !row->quiet)
		widget.flags |= AE_ROW_FOCUSED;
	if (row->status)
		widget.flags |= AE_ROW_STATUS;
	if (row->subscreen)
	{
		widget.flags |= AE_ROW_SUBSCREEN | AE_ROW_MARQUEE;
		widget.marquee_ms = row->marquee_ms;
	}
	if (row->reason)
	{
		widget.flags |= AE_ROW_DISABLED;
		widget.reason = row->reason;
	}
	if (row->kind == AE_FLOW_ROW_OPTION)
	{
		struct ae_setup_access access;

		if (ae_setup_row(row->option) && ae_setup_row(row->option)->source != AE_SRC_CLASSIC)
		{
			widget.flags |= AE_ROW_AE;
			widget.source_badge = ae_setup_source_badge(ae_setup_row(row->option));
		}
		ae_setup_edit_access(&access);
		if (access.changed(access.context, row->option))
		{
			widget.flags |= AE_ROW_CHANGED;
			widget.base_value = ae_setup_value_text(row->option, ae_setup_edit_base(row->option));
		}
	}
	flow->rects[index].x = x;
	flow->rects[index].y = *y;
	flow->rects[index].width = width;
	flow->rects[index].height = ae_widget_row(d, x, *y, width, &widget, flow->hit_rows, index);
	*y += flow->rects[index].height + ae_flow_u(flow, d->metrics->gap);
}

void ae_flow_draw_rows(struct ae_flow_screen *flow, float width)
{
	struct ae_density const *d = &flow->density;
	float x = ae_flow_left_x(flow), y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	float body = ae_flow_u(flow, d->metrics->body), line = ae_flow_u(flow, d->metrics->body_line), help_y;
	float more = ae_widget_more_line(d), bottom = ae_flow_content_bottom(flow), room;
	char lines[2][160];
	char expanded[320];
	short index, last, count = flow->count, pinned = -1;
	int lines_count, any_help = 0;

	/* (the rows end above the help's room when any row has help: it never drops off) */
	for (index = 0; index < flow->count; index++)
		any_help |= flow->rows[index].help != NULL;
	help_y = bottom - line * 2.0f;
	if (any_help)
		bottom = help_y - ae_flow_u(flow, HELP_GAP_U);
	room = bottom - y;
	/* (0.3.9: a primary last row is pinned under the rows that scroll: their window ends above it, so it never
	scrolls away at 130 % (mockups-039/start-game-button A); the focus on it leaves the window where it was) */
	if (count > 1 && flow->rows[count - 1].primary)
	{
		pinned = (short)(count - 1);
		room -= flow_row_pitch(flow, pinned);
		flow->count = pinned;
	}
	flow_scroll_to_focus(flow, room);
	last = flow_last_row(flow, flow->first, room);
	if (flow->first > 0)
	{
		ae_widget_more(d, x + width * 0.5f, y, 1, flow_rows_between(flow, 0, (short)(flow->first - 1)));
		y += more;
	}
	for (index = 0; index < flow->count; index++)
	{
		if (index < flow->first || index > last)
		{
			flow->rects[index].width = 0.0f;
			continue;
		}
		flow_draw_row(flow, index, x, &y, width);
	}
	if (last < flow->count - 1)
	{
		ae_widget_more(d, x + width * 0.5f, y, 0, flow_rows_between(flow, (short)(last + 1), (short)(flow->count - 1)));
		y += more;
	}
	flow->count = count;
	if (pinned >= 0)
		flow_draw_row(flow, pinned, x, &y, width);
	if (flow->focus < 0 || flow->focus >= flow->count || !flow->rows[flow->focus].help ||
		help_y < y + ae_flow_u(flow, HELP_GAP_U))
		return;
	ae_draw_rect(x, help_y - ae_flow_u(flow, HELP_GAP_U) * 0.5f, width, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f,
		AE_COLOR_RULE);
	lines_count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(flow->rows[flow->focus].help, expanded,
		sizeof(expanded)), width, lines, 2);
	for (index = 0; index < lines_count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, help_y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

void ae_flow_draw_prompts(struct ae_flow_screen *flow)
{
	ae_widget_prompts(&flow->density, ae_flow_left_x(flow), flow->frame.rect.height - ae_flow_u(flow, FOOTER_ABOVE_U),
		flow->prompts, flow->prompt_count, NULL, ae_flow_right_x(flow), -1, flow->hit_prompts);
}

struct ae_prompt ae_flow_prompt(int button, const char *label, int action)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = NULL;
	prompt.action = action;
	prompt.pair = 0;
	return prompt;
}

int ae_flow_focus_step(struct ae_flow_screen *flow, short direction, int repeat)
{
	short next = flow->focus;

	do
		next = (short)(next + direction);
	while (next >= 0 && next < flow->count && flow->rows[next].kind == AE_FLOW_ROW_HEADER);
	if (next < 0 || next >= flow->count)
		return 0;
	flow->focus = next;
	ae_sound_request(AE_SOUND_CURSOR, repeat);
	return 1;
}

short ae_flow_rows_pointer(struct ae_flow_screen *flow, struct ae_pointer const *pointer)
{
	struct ae_hit hit;

	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit) || hit.id != flow->hit_rows || hit.index < 0 ||
		hit.index >= flow->count)
		return -1;
	if (pointer->moved && flow->rows[hit.index].kind != AE_FLOW_ROW_HEADER)
		flow->focus = hit.index;
	if (!pointer->left_clicks)
		return -1;
	if (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT)
		return (short)(-2 - (hit.part == AE_PART_ARROW_RIGHT));
	return hit.index;
}

void ae_flow_error(struct ae_flow_screen *flow, const char *title, const char *body)
{
	struct ae_dialog_spec spec;

	/* (an error before the screen's first draw, the host that fails to open: no density yet, and the dialog's layout
	read its metrics) */
	if (!flow->density.metrics)
		ae_flow_full_density(flow);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_ERROR;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = ae_string(AE_STR_BACK);
	spec.choice_count = 1;
	spec.has_cancel = 1;
	spec.density = flow->density;
	spec.bounds = flow->frame.rect;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}
