/* ae_widgets_pick.c: Arena Evolved menus, picking (see ae_widgets.h): the value picker's open list (a popover screen),
chips, the help / preview panel. No engine includes.

Spec 4.5 (value picker), 4.6 (chips), 4.7 (help panel). A popover's spec is in layout units of the whole frame, with
the view it draws in (preflight P12: a VIEW popover draws in the player's own view, inside its panel). The in-place
value step is ae_value_step (ae_widgets_nav.c: the shared step, no wrapping). Text is placed by its capitals' middle;
2 px of horizontal ink are allowed. */

#include <stdio.h>
#include <string.h>

#include "../src/ae_draw.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_widgets.h"

/* spec units */
#define PICKER_TEXT_U 22.0f
#define PICKER_EXTRA_U 76.0f
#define PICKER_MINIMUM_U 240.0f
#define PICKER_ITEM_PER_ROW (42.0f / 50.0f)   /* items 42 u where rows are 50 u (VIEW: in the same ratio) */
#define PICKER_VISIBLE 8
#define PICKER_TEXT_X_U 48.0f                 /* the values' left, after the check's column */
#define PICKER_CHECK_X_U 24.0f                /* the check's middle */
#define PICKER_CHECK_U 6.0f                   /* the check's half width */
#define PICKER_CHECK_STROKE_U 3.0f
#define PICKER_TRACK_U 6.0f
#define PICKER_WHEEL_ROWS 3
#define NOTCH_U 6.0f
#define CHIP_PAD_U 14.0f
#define CHIP_GAP_U 8.0f
#define CHIP_COUNT_GAP_U 10.0f
#define CHIP_RING_GAP_U 3.0f
#define CHIP_RING_U 2.0f
#define CHIP_DASH_U 4.0f
#define STRIKE_U 2.0f
#define HELP_INSET_U 28.0f
#define HELP_GAP_U 14.0f
#define HELP_TRACKING 0.09f
#define HELP_DOT_U 5.0f
#define HELP_DASH_U 6.0f
#define TEXT_FLOOR_PIXELS 16.0f
#define MINOR_FLOOR_PIXELS 14.0f
#define LINE 1.4f                            /* a line of text: its size x this, at least */
/* the picker's hits (one picker open at a time) */
enum { PICKER_HIT_ID = 0x7E01 };

static float units(struct ae_density const *density, float spec_units)
{
	return spec_units * density->unit;
}

static float floored(struct ae_density const *density, float spec_units, float floor_pixels)
{
	return ae_size(density, spec_units, density->kind == AE_DENSITY_VIEW ? floor_pixels : 0.0f);
}

static float text_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

/* ---------- the value picker */

static float picker_text(struct ae_density const *density)
{
	return floored(density, PICKER_TEXT_U, TEXT_FLOOR_PIXELS);
}

float ae_picker_width(struct ae_density const *density, const char *const *values, short count)
{
	float longest = 0.0f, minimum = units(density, PICKER_MINIMUM_U);
	short index;

	for (index = 0; index < count; index++)
	{
		float width = values && values[index] ? ae_draw_text_width(AE_FONT_ROW, picker_text(density), values[index]) : 0.0f;

		longest = width > longest ? width : longest;
	}
	longest += units(density, PICKER_EXTRA_U);
	return longest > minimum ? longest : minimum;
}

void ae_picker_place(struct ae_rect const *row, struct ae_rect const *bounds, short count, short current,
	float item_height, float width, struct ae_rect *popover, short *first_visible)
{
	short visible = count < PICKER_VISIBLE ? count : PICKER_VISIBLE, first;
	float y;

	/* (no more items than the bounds hold) */
	if (item_height > 0.0f && (float)visible * item_height > bounds->height)
		visible = (short)(bounds->height / item_height);
	if (visible < 1)
		visible = 1;
	if (current < 0 || current >= count)
		current = 0;
	/* the window: the current item in it, a little up from the middle where it can be */
	first = (short)(current - visible / 2 + (visible > 1 ? 1 : 0));
	if (first > count - visible)
		first = (short)(count - visible);
	if (first < 0)
		first = 0;
	/* the current item over the row; upward (then into the bounds) near the bottom; never outside them */
	y = row->y + row->height * 0.5f - ((float)(current - first) + 0.5f) * item_height;
	if (y + (float)visible * item_height > bounds->y + bounds->height)
		y = bounds->y + bounds->height - (float)visible * item_height;
	if (y < bounds->y)
		y = bounds->y;
	popover->height = (float)visible * item_height;
	popover->width = width < bounds->width ? width : bounds->width;
	popover->x = row->x + row->width - popover->width;
	if (popover->x + popover->width > bounds->x + bounds->width)
		popover->x = bounds->x + bounds->width - popover->width;
	if (popover->x < bounds->x)
		popover->x = bounds->x;
	popover->y = y;
	*first_visible = first;
}

/* the open picker (one at a time) */
static struct
{
	struct ae_picker_spec spec;
	struct ae_list list;
	struct ae_rect popover;        /* layout units */
} picker;

static void picker_enter(struct ae_screen *screen)
{
	(void)screen;
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* a drawing unit's y of the popover's first item, its items' height, in the current view */
static void picker_frame(struct ae_rect *popover, float *item)
{
	struct ae_view view;

	ae_draw_current_view(&view);
	popover->x = (picker.popover.x - view.x) / view.scale;
	popover->y = (picker.popover.y - view.y) / view.scale;
	popover->width = picker.popover.width / view.scale;
	popover->height = picker.popover.height / view.scale;
	*item = popover->height / (float)(picker.list.rows > 0 ? picker.list.rows : 1);
}

static void picker_draw(struct ae_screen *screen)
{
	struct ae_density const *density = &picker.spec.density;
	struct ae_rect popover;
	float item, size = picker_text(density), track = units(density, PICKER_TRACK_U);
	short row;

	(void)screen;
	/* (P12: in the player's own view for VIEW; else in the view before_draw set, the dialog's scale included) */
	if (picker.spec.view.width > 0.0f && picker.spec.view.height > 0.0f)
		ae_draw_view(picker.spec.view.x, picker.spec.view.y, picker.spec.view.width, picker.spec.view.height);
	picker_frame(&popover, &item);
	/* clicks anywhere else close it: a hit over the whole view under the popover's own */
	ae_hit_add(-100000.0f, -100000.0f, 200000.0f, 200000.0f, PICKER_HIT_ID, AE_PART_OUTSIDE, -1);
	ae_draw_rect(popover.x, popover.y, popover.width, popover.height, units(density, density->metrics->corner),
		AE_COLOR_POPOVER);
	ae_hit_add(popover.x, popover.y, popover.width, popover.height, PICKER_HIT_ID, AE_PART_CARD, -1);
	for (row = 0; row < picker.list.rows; row++)
	{
		short index = ae_list_item_at_row(&picker.list, row);
		float y = popover.y + (float)row * item, center = y + item * 0.5f;
		int focused = index == picker.list.focus, current = index == picker.spec.current;
		unsigned int color = focused ? AE_COLOR_SELECTION_TEXT : current ? AE_COLOR_ACCENT : AE_COLOR_TEXT;

		if (index < 0)
			break;
		/* the focus: a white bar with a notch */
		if (focused)
		{
			ae_draw_rect(popover.x, y, popover.width - (picker.list.count > picker.list.rows ? track * 2.0f : 0.0f), item,
				0.0f, AE_COLOR_SELECTION);
			ae_draw_rect(popover.x, y, units(density, NOTCH_U), item, 0.0f, AE_COLOR_ACCENT);
		}
		/* the current value: an accent check (two strokes) */
		if (current)
		{
			float x = popover.x + units(density, PICKER_CHECK_X_U), half = units(density, PICKER_CHECK_U);
			float stroke = units(density, PICKER_CHECK_STROKE_U);

			ae_draw_line(x - half, center, x - half * 0.35f, center + half * 0.65f, stroke, color);
			ae_draw_line(x - half * 0.35f, center + half * 0.65f, x + half, center - half * 0.8f, stroke, color);
		}
		if (picker.spec.values && picker.spec.values[index])
			ae_draw_text(AE_FONT_ROW, size, popover.x + units(density, PICKER_TEXT_X_U), text_top(AE_FONT_ROW, size, center),
				AE_ALIGN_LEFT, color, picker.spec.values[index]);
		ae_hit_add(popover.x, y, popover.width, item, PICKER_HIT_ID, AE_PART_ITEM, index);
	}
	/* more than it shows: a 6 u scrollbar */
	if (picker.list.count > picker.list.rows)
	{
		float thumb_y, thumb_height, x = popover.x + popover.width - track * 1.5f;

		ae_scrollbar_thumb(picker.list.count, picker.list.rows, (float)picker.list.first, popover.height, item,
			&thumb_y, &thumb_height);
		ae_draw_rect(x, popover.y, track, popover.height, track * 0.5f, AE_COLOR_TRACK);
		ae_draw_rect(x, popover.y + thumb_y, track, thumb_height, track * 0.5f, AE_COLOR_MUTED);
	}
}

/* picks an item: the callback, the forward sound, closed */
static void picker_pick(short index)
{
	void (*picked)(short, void *) = picker.spec.picked;
	void *context = picker.spec.context;

	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_pop();
	if (picked)
		picked(index, context);
}

static int picker_handle(struct ae_screen *screen, struct ae_event const *event)
{
	short before = picker.list.focus, page = (short)(picker.list.rows > 1 ? picker.list.rows - 1 : 1);

	(void)screen;
	switch (event->action)
	{
	case AE_ACTION_UP: ae_list_move(&picker.list, -1, 0); break;
	case AE_ACTION_DOWN: ae_list_move(&picker.list, 1, 0); break;
	case AE_ACTION_PAGE_UP: ae_list_move(&picker.list, (short)-page, 0); break;
	case AE_ACTION_PAGE_DOWN: ae_list_move(&picker.list, page, 0); break;
	case AE_ACTION_ACCEPT:
		picker_pick(picker.list.focus);
		return 1;
	/* (B: not handled: the stack closes it, with the back sound) */
	case AE_ACTION_BACK: return 0;
	default: return 1;
	}
	if (picker.list.focus != before)
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
	return 1;
}

static void picker_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_hit hit;
	int mine;

	(void)screen;
	mine = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == PICKER_HIT_ID;
	if (pointer->wheel_steps && mine && hit.part != AE_PART_OUTSIDE)
		ae_list_scroll(&picker.list, (short)(-PICKER_WHEEL_ROWS * pointer->wheel_steps));
	/* (hover focuses, never scrolls) */
	if (pointer->moved && mine && hit.part == AE_PART_ITEM && hit.index >= 0 && hit.index < picker.list.count)
		picker.list.focus = hit.index;
	if (!pointer->left_clicks)
		return;
	if (mine && hit.part == AE_PART_ITEM && hit.index >= 0 && hit.index < picker.list.count)
		picker_pick(hit.index);
	else if (!mine || hit.part == AE_PART_OUTSIDE)
	{
		/* a click outside: closed unchanged */
		ae_sound_request(AE_SOUND_BACK, 0);
		ae_ui_pop();
	}
}

static struct ae_screen_class const picker_class =
{
	.name = "picker", .enter = picker_enter, .handle = picker_handle, .draw = picker_draw, .pointer = picker_pointer,
	.popover = 1,
};

int ae_picker_open(struct ae_picker_spec const *spec, short owner)
{
	float scale, item;
	short first, rows;

	if (!spec || spec->count <= 0)
		return 0;
	picker.spec = *spec;
	/* (the density's drawing units are its view's: layout units are those x the view's scale) */
	scale = spec->view.width > 0.0f && spec->view.height > 0.0f ? spec->view.height / (float)AE_LAYOUT_HEIGHT : 1.0f;
	item = ae_size_row(&spec->density) * PICKER_ITEM_PER_ROW * scale;
	ae_picker_place(&spec->row, &spec->bounds, spec->count, spec->current, item,
		ae_picker_width(&spec->density, spec->values, spec->count) * scale, &picker.popover, &first);
	rows = (short)(picker.popover.height / item + 0.5f);
	ae_list_init(&picker.list, spec->count, rows);
	ae_list_set_focus(&picker.list, spec->current >= 0 && spec->current < spec->count ? spec->current : 0);
	/* (the window as placed: the current item over the row) */
	picker.list.first = first;
	return ae_ui_push(&picker_class, owner, &picker);
}

/* ---------- chips */

static float chip_height(struct ae_density const *density)
{
	return units(density, density->metrics->chip);
}

static float chip_text(struct ae_density const *density)
{
	return floored(density, density->metrics->chip_text, MINOR_FLOOR_PIXELS);
}

/* a chip's width: its label (and count), 14 u in from each side */
static float chip_width(struct ae_density const *density, struct ae_chip const *chip, char *count_text, int count_size)
{
	float width = ae_draw_text_width(AE_FONT_ROW, chip_text(density), chip->label ? chip->label : "");

	count_text[0] = 0;
	if (chip->count >= 0)
	{
		snprintf(count_text, (size_t)count_size, "%d", chip->count);
		width += units(density, CHIP_COUNT_GAP_U) + ae_draw_text_width(AE_FONT_ROW, chip_text(density), count_text);
	}
	return width + 2.0f * units(density, CHIP_PAD_U);
}

float ae_chips_layout(struct ae_density const *density, float width, struct ae_chip const *chips, short count,
	struct ae_rect *rects)
{
	float x = 0.0f, y = 0.0f, height = chip_height(density), gap = units(density, CHIP_GAP_U);
	short index;
	char number[16];

	for (index = 0; index < count; index++)
	{
		float chip = chip_width(density, &chips[index], number, sizeof(number));

		/* (wrapping: a chip that doesn't fit starts the next row, unless it is the row's first) */
		if (x > 0.0f && x + chip > width)
		{
			x = 0.0f;
			y += height + gap;
		}
		rects[index].x = x;
		rects[index].y = y;
		rects[index].width = chip;
		rects[index].height = height;
		x += chip + gap;
	}
	return count > 0 ? y + height : 0.0f;
}

float ae_widget_chips(struct ae_density const *density, float x, float y, float width, struct ae_chip const *chips,
	short count, short hit_id)
{
	struct ae_rect rects[32];
	float height, size = chip_text(density), corner = units(density, density->metrics->corner);
	short index;

	if (count > 32)
		count = 32;
	height = ae_chips_layout(density, width, chips, count, rects);
	for (index = 0; index < count; index++)
	{
		struct ae_chip const *chip = &chips[index];
		struct ae_rect r = rects[index];
		unsigned int flags = chip->flags, text = AE_COLOR_MUTED, count_color = AE_COLOR_MUTED;
		float center = y + r.y + r.height * 0.5f, label_width;
		char number[16];

		r.x += x;
		r.y += y;
		chip_width(density, chip, number, sizeof(number));
		if (flags & AE_CHIP_DISABLED)
		{
			ae_draw_outline(r.x, r.y, r.width, r.height, corner, units(density, 1.0f), AE_COLOR_CHIP_DISABLED);
			text = count_color = AE_COLOR_CHIP_DISABLED_TEXT;
		}
		else if (flags & AE_CHIP_UNSUPPORTED)
			ae_widget_dashed(r.x, r.y, r.width, r.height, units(density, 1.0f), units(density, CHIP_DASH_U), AE_COLOR_RULE);
		else if (flags & AE_CHIP_ON)
		{
			ae_draw_rect(r.x, r.y, r.width, r.height, corner, AE_COLOR_ACCENT);
			text = count_color = AE_COLOR_SELECTION_TEXT;
			/* on + focus: a 2 u white ring 3 u outside */
			if (flags & AE_CHIP_FOCUSED)
			{
				float out = units(density, CHIP_RING_GAP_U + CHIP_RING_U);

				ae_draw_outline(r.x - out, r.y - out, r.width + 2.0f * out, r.height + 2.0f * out, corner + out,
					units(density, CHIP_RING_U), AE_COLOR_RING);
			}
		}
		else if (flags & AE_CHIP_FOCUSED)
		{
			ae_draw_rect(r.x, r.y, r.width, r.height, corner, AE_COLOR_SELECTION);
			text = count_color = AE_COLOR_SELECTION_TEXT;
		}
		else
		{
			ae_draw_outline(r.x, r.y, r.width, r.height, corner, units(density, 1.0f), AE_COLOR_RULE);
			if (flags & AE_CHIP_HOVER)
				ae_draw_rect(r.x, r.y, r.width, r.height, corner, AE_COLOR_HOVER);
		}
		label_width = ae_draw_text(AE_FONT_ROW, size, r.x + units(density, CHIP_PAD_U), text_top(AE_FONT_ROW, size, center),
			AE_ALIGN_LEFT, text, chip->label ? chip->label : "");
		if (number[0])
			ae_draw_text(AE_FONT_ROW, size, r.x + units(density, CHIP_PAD_U) + label_width + units(density, CHIP_COUNT_GAP_U),
				text_top(AE_FONT_ROW, size, center), AE_ALIGN_LEFT, count_color, number);
		/* unsupported: struck */
		if (flags & AE_CHIP_UNSUPPORTED && !(flags & AE_CHIP_DISABLED))
			ae_draw_rect(r.x + units(density, CHIP_PAD_U), center - units(density, STRIKE_U) * 0.5f, label_width,
				units(density, STRIKE_U), 0.0f, AE_COLOR_MUTED);
		ae_hit_add(r.x, r.y, r.width, r.height, hit_id, AE_PART_CHIP, index);
	}
	return height;
}

/* ---------- the help panel */

/* the device's Y (keyboard: the R cap), its left at x, its middle at center_y; its width */
static float reset_mark(struct ae_density const *density, float x, float center_y)
{
	float glyph = floored(density, density->metrics->minor, MINOR_FLOOR_PIXELS) * LINE;

	switch (ae_ui_last_device())
	{
	case AE_DEVICE_KEYBOARD_MOUSE:
		return ae_widget_key_cap(density, x, center_y - glyph * 0.5f, glyph, ae_prompt_key(AE_BUTTON_Y));
	case AE_DEVICE_PLAYSTATION:
		return ae_draw_button(AE_FONT_PLAYSTATION, AE_BUTTON_Y, glyph, x, center_y - glyph * 0.5f,
			ae_prompt_tint(AE_DEVICE_PLAYSTATION, AE_BUTTON_Y));
	case AE_DEVICE_NINTENDO:
		return ae_draw_button(AE_FONT_NINTENDO, AE_BUTTON_Y, glyph, x, center_y - glyph * 0.5f,
			ae_prompt_tint(AE_DEVICE_NINTENDO, AE_BUTTON_Y));
	case AE_DEVICE_XBOX: break;
	}
	return ae_draw_button(AE_FONT_XBOX, AE_BUTTON_Y, glyph, x, center_y - glyph * 0.5f,
		ae_prompt_tint(AE_DEVICE_XBOX, AE_BUTTON_Y));
}

void ae_widget_help(struct ae_density const *density, struct ae_rect const *rect, struct ae_help const *help,
	short hit_id, struct ae_rect *preview)
{
	char lines[12][160], text[200];
	float inset = units(density, HELP_INSET_U), gap = units(density, HELP_GAP_U);
	float x = rect->x + inset, width = rect->width - 2.0f * inset, y = rect->y + inset;
	float title = floored(density, density->metrics->help_title, TEXT_FLOOR_PIXELS);
	float body = floored(density, density->metrics->body, MINOR_FLOOR_PIXELS);
	float body_line = units(density, density->metrics->body_line);
	float minor = floored(density, density->metrics->minor, MINOR_FLOOR_PIXELS), minor_line = minor * LINE * 1.2f;
	float group = floored(density, density->metrics->group, MINOR_FLOOR_PIXELS);
	int count, index;

	if (body_line < body * LINE)
		body_line = body * LINE;
	if (preview)
		preview->x = preview->y = preview->width = preview->height = 0.0f;
	ae_draw_rect(rect->x, rect->y, rect->width, rect->height, units(density, density->metrics->corner), AE_COLOR_PANEL);
	ae_draw_outline(rect->x, rect->y, rect->width, rect->height, units(density, density->metrics->corner),
		units(density, 1.0f), AE_COLOR_RULE);
	if (!help)
		return;
	/* the title */
	if (help->title && *help->title)
	{
		ae_draw_text(AE_FONT_ROW, title, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, help->title);
		y += ae_draw_cap_height(AE_FONT_ROW, title) + gap;
	}
	/* the body, 21 on 31 u */
	count = help->body ? ae_wrap_text(AE_FONT_BODY, body, help->body, width, lines, 12) : 0;
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, text_top(AE_FONT_BODY, body, y + body_line * ((float)index + 0.5f)),
			AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
	y += (float)count * body_line + gap;
	/* VALUES and every value as a chip, the current one filled */
	if (help->values && help->value_count > 0)
	{
		struct ae_chip chips[16];
		short value, values = help->value_count < 16 ? help->value_count : 16;

		ae_draw_text_tracked(AE_FONT_BODY, group, HELP_TRACKING, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
			ae_string(AE_STR_VALUES));
		y += ae_draw_cap_height(AE_FONT_BODY, group) + gap;
		for (value = 0; value < values; value++)
		{
			chips[value].label = help->values[value];
			chips[value].count = -1;
			chips[value].flags = value == help->current ? AE_CHIP_ON : 0;
		}
		y += ae_widget_chips(density, x, y, width, chips, values, hit_id) + gap;
	}
	/* Default: X */
	if (help->default_value)
	{
		snprintf(text, sizeof(text), ae_string(AE_STR_DEFAULT), help->default_value);
		ae_draw_text(AE_FONT_BODY, minor, x, text_top(AE_FONT_BODY, minor, y + minor_line * 0.5f), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, text);
		y += minor_line;
	}
	/* changed: "Changed from X" • [Y] Reset (the mark and Reset clickable) */
	if (help->changed_from)
	{
		float center = y + minor_line * 0.5f, pen = x, mark_x;

		snprintf(text, sizeof(text), ae_string(AE_STR_CHANGED_FROM), help->changed_from);
		pen += ae_draw_text(AE_FONT_BODY, minor, pen, text_top(AE_FONT_BODY, minor, center), AE_ALIGN_LEFT, AE_COLOR_ACCENT,
			text);
		pen += gap * 0.6f;
		ae_widget_separator(density, pen + units(density, HELP_DOT_U) * 0.5f, center, HELP_DOT_U, AE_COLOR_ACCENT);
		pen += units(density, HELP_DOT_U) + gap * 0.6f;
		mark_x = pen;
		pen += reset_mark(density, pen, center) + gap * 0.4f;
		pen += ae_draw_text(AE_FONT_BODY, minor, pen, text_top(AE_FONT_BODY, minor, center), AE_ALIGN_LEFT, AE_COLOR_TEXT,
			ae_string(AE_STR_RESET));
		ae_hit_add(mark_x, y, pen - mark_x, minor_line, hit_id, AE_PART_PROMPT, -1);
		y += minor_line;
	}
	/* the preview area: the rest, dashed */
	if (help->preview)
	{
		float top = y + gap, bottom = rect->y + rect->height - inset;

		if (bottom > top && preview)
		{
			preview->x = x;
			preview->y = top;
			preview->width = width;
			preview->height = bottom - top;
			ae_widget_dashed(x, top, width, bottom - top, units(density, 1.0f), units(density, HELP_DASH_U), AE_COLOR_RULE);
		}
	}
}
