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

void ae_flow_draw_rows(struct ae_flow_screen *flow, float width)
{
	struct ae_density const *d = &flow->density;
	float x = ae_flow_left_x(flow), y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), gap = ae_flow_u(flow, d->metrics->gap);
	float body = ae_flow_u(flow, d->metrics->body), line = ae_flow_u(flow, d->metrics->body_line), help_y;
	char lines[2][160];
	short index;
	int count;

	for (index = 0; index < flow->count; index++)
	{
		struct ae_flow_row const *row = &flow->rows[index];
		struct ae_row widget;

		if (row->kind == AE_FLOW_ROW_HEADER)
		{
			y += ae_widget_group(d, x, y, width, row->label);
			flow->rects[index].width = 0.0f;
			continue;
		}
		memset(&widget, 0, sizeof(widget));
		widget.label = row->label;
		widget.value = row->value;
		if (index == flow->focus)
			widget.flags |= AE_ROW_FOCUSED;
		if (row->kind == AE_FLOW_ROW_OPTION)
		{
			struct ae_setup_access access;

			if (ae_setup_row(row->option) && ae_setup_row(row->option)->ae)
				widget.flags |= AE_ROW_AE;
			ae_setup_edit_access(&access);
			if (access.changed(access.context, row->option))
			{
				widget.flags |= AE_ROW_CHANGED;
				widget.base_value = ae_setup_value_text(row->option, ae_setup_edit_base(row->option));
			}
		}
		flow->rects[index].x = x;
		flow->rects[index].y = y;
		flow->rects[index].width = width;
		flow->rects[index].height = ae_widget_row(d, x, y, width, &widget, flow->hit_rows, index);
		y += flow->rects[index].height + gap;
	}
	help_y = ae_flow_content_bottom(flow) - line * 2.0f;
	if (flow->focus < 0 || flow->focus >= flow->count || !flow->rows[flow->focus].help ||
		help_y < y + ae_flow_u(flow, HELP_GAP_U))
		return;
	ae_draw_rect(x, help_y - ae_flow_u(flow, HELP_GAP_U) * 0.5f, width, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f,
		AE_COLOR_RULE);
	count = ae_wrap_text(AE_FONT_BODY, body, flow->rows[flow->focus].help, width, lines, 2);
	for (index = 0; index < count; index++)
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
