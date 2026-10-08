/* ae_widgets_nav.c: Arena Evolved menus, navigation widgets (see ae_widgets.h): tabs, page dots, the prompt footer
with drawn key caps, the in-view panel frame and its help strip. No engine includes.

Spec 4.4 (tabs), 4.10 (prompts), 4.13 (page dots), 6 (the in-view panel). Tabs, page dots and (Task 8) the value
picker step through one helper, step(): wrapping, skipping what can't be chosen. Text is placed by its capitals'
middle; clips allow 2 window pixels of horizontal ink. */

#include <stdio.h>
#include <string.h>

#include "../src/ae_draw.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_widgets.h"

/* spec units */
#define TAB_GAP_U 40.0f
#define TAB_UNDERLINE_U 4.0f
#define TAB_DOT_U 8.0f
#define TAB_FADE_U 70.0f
#define TAB_FADE_STRIPS 14
#define TAB_END_GAP_U 12.0f
#define TAB_HEIGHT_LINES 2.0f          /* the strip: twice its text size tall */
#define DOT_U 8.0f
#define DOT_GAP_U 11.0f
#define PILL_U 22.0f
#define DOTS_TITLE_GAP_U 12.0f
#define DOTS_END_GAP_U 14.0f
#define PROMPT_GAP_U 32.0f
#define KEY_CAP_CORNER_U 4.0f
#define RULE_U 1.0f
#define PANEL_STRIPE_U 3.0f
#define MINOR_FLOOR_PIXELS 14.0f
#define CLIP_ALLOWANCE_PIXELS 2.0f
/* the key cap (a ruling: the spec gives no dimensions): its text half its height, its sides 0.3 of it, at least as
wide as tall */
#define CAP_TEXT 0.5f
#define CAP_SIDE 0.3f
/* a keyboard prompt's button around its cap and label: 0.2 of the cap above and below, 0.3 at the sides and between */
#define BUTTON_PAD_Y 0.2f
#define BUTTON_PAD_X 0.3f
/* a glyph to its label */
#define GLYPH_GAP 0.3f

/* ---------- small helpers */

static float units(struct ae_density const *density, float spec_units)
{
	return spec_units * density->unit;
}

/* a text size with VIEW's 14 px minor floor */
static float floored_minor(struct ae_density const *density, float spec_units)
{
	return ae_size(density, spec_units, density->kind == AE_DENSITY_VIEW ? MINOR_FLOOR_PIXELS : 0.0f);
}

static unsigned int scaled_alpha(unsigned int rgba, float factor)
{
	float alpha = (float)(rgba & 0xFFu) * factor;

	return (rgba & 0xFFFFFF00u) | (unsigned int)(alpha < 0.0f ? 0.0f : alpha > 255.0f ? 255.0f : alpha + 0.5f);
}

static float text_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

/* a hit cut to [left, right) in x (drawing units): nothing when none of it is left */
static void hit_between(float x, float y, float width, float height, float left, float right, short id, short part,
	short index)
{
	float x0 = x > left ? x : left, x1 = x + width < right ? x + width : right;

	if (x1 > x0)
		ae_hit_add(x0, y, x1 - x0, height, id, part, index);
}

/* the one step through a ring of count (direction -1 / 1): wrapping, passing over what skip refuses (NULL: none);
current when nothing else can be chosen. Tabs, page dots and the value picker step through it (preflight P31) */
static short step(short count, short current, short direction, int (*skip)(void const *context, short index),
	void const *context)
{
	short index = current, tries;

	if (count <= 0)
		return current;
	direction = (short)(direction < 0 ? -1 : 1);
	/* (no valid current: the first that can be chosen, from the end the direction starts at) */
	if (index < 0 || index >= count)
	{
		for (tries = 0, index = (short)(direction > 0 ? 0 : count - 1); tries < count; tries++, index = (short)(index + direction))
			if (!skip || !skip(context, index))
				return index;
		return current;
	}
	for (tries = 0; tries < count; tries++)
	{
		index = (short)((index + direction + count) % count);
		if (index == current)
			break;
		if (!skip || !skip(context, index))
			return index;
	}
	return current;
}

/* the device prompts show (the last used: ae_ui) as an ae_draw device font; -1 for the keyboard */
static int device_font(int device)
{
	switch (device)
	{
	case AE_DEVICE_PLAYSTATION: return AE_FONT_PLAYSTATION;
	case AE_DEVICE_NINTENDO: return AE_FONT_NINTENDO;
	case AE_DEVICE_KEYBOARD_MOUSE: return -1;
	}
	return AE_FONT_XBOX;
}

/* ---------- key caps and end glyphs */

/* a cap's height for a nominal one: its words half its height, never below VIEW's 14 px minor floor; when the floor
binds the cap grows to hold them (never clips them) */
static float cap_height(struct ae_density const *density, float height)
{
	float floor = density->kind == AE_DENSITY_VIEW ? MINOR_FLOOR_PIXELS * density->pixel : 0.0f;

	return height * CAP_TEXT < floor ? floor / CAP_TEXT : height;
}

static float key_cap_width(struct ae_density const *density, float height, const char *words)
{
	float cap = cap_height(density, height);
	float width = ae_draw_text_width(AE_FONT_ROW, cap * CAP_TEXT, words ? words : "") + 2.0f * CAP_SIDE * cap;

	return width > cap ? width : cap;
}

float ae_widget_key_cap(struct ae_density const *density, float x, float y, float height, const char *words)
{
	float cap = cap_height(density, height), width = key_cap_width(density, height, words), size = cap * CAP_TEXT;

	/* (a grown cap keeps the nominal one's middle) */
	y -= (cap - height) * 0.5f;
	height = cap;
	ae_draw_rect(x, y, width, height, units(density, KEY_CAP_CORNER_U), AE_COLOR_KEY_CAP);
	if (words && *words)
		ae_draw_text(AE_FONT_ROW, size, x + width * 0.5f, text_top(AE_FONT_ROW, size, y + height * 0.5f), AE_ALIGN_CENTER,
			AE_COLOR_KEY_CAP_TEXT, words);
	return width;
}

/* an end of a strip (tabs, page dots): the LB / RB glyph (Q / E cap on the keyboard), height tall, centred on
center_y with its left at x; previous: LB / Q; returns its width (measure only when draw is 0) */
static float end_glyph(struct ae_density const *density, float x, float center_y, float height, int previous, int draw)
{
	int device = ae_ui_last_device(), font = device_font(device);
	int button = previous ? AE_BUTTON_LEFT_SHOULDER : AE_BUTTON_RIGHT_SHOULDER;

	if (font < 0)
	{
		const char *key = ae_prompt_key(button);

		if (!draw)
			return key_cap_width(density, height, key);
		return ae_widget_key_cap(density, x, center_y - height * 0.5f, height, key);
	}
	if (!draw)
		return ae_draw_button_width(font, button, height);
	return ae_draw_button(font, button, height, x, center_y - height * 0.5f, ae_prompt_tint(device, button));
}

/* ---------- tabs */

static int tab_disabled(void const *context, short index)
{
	struct ae_tabs const *tabs = context;

	return tabs->tabs[index].disabled != 0;
}

short ae_tabs_step(struct ae_tabs const *tabs, short direction)
{
	return step(tabs->count, tabs->active, direction, tab_disabled, tabs);
}

/* (a horizontal fade: strips of falling alpha, opaque at the cut edge; to_right: the edge is on the left) */
static void fade(struct ae_density const *density, float x, float y, float height, int edge_on_left)
{
	float width = units(density, TAB_FADE_U), strip = width / (float)TAB_FADE_STRIPS;
	int index;

	for (index = 0; index < TAB_FADE_STRIPS; index++)
	{
		float strength = 1.0f - ((float)index + 0.5f) / (float)TAB_FADE_STRIPS;
		float strip_x = edge_on_left ? x + strip * (float)index : x + width - strip * (float)(index + 1);

		ae_draw_rect(strip_x, y, strip, height, 0.0f, scaled_alpha(AE_COLOR_FADE, strength));
	}
}

float ae_widget_tabs(struct ae_density const *density, struct ae_tabs *tabs, float x, float y, float width,
	short hit_id)
{
	float size = floored_minor(density, density->metrics->tab_text), height = size * TAB_HEIGHT_LINES;
	float center_y = y + height * 0.5f, glyph = ae_size_glyph(density), gap = units(density, TAB_GAP_U);
	float end_gap = units(density, TAB_END_GAP_U), allowance = CLIP_ALLOWANCE_PIXELS * density->pixel;
	float left_end, right_end, area_x, area_width, total = 0.0f, offset, target, active_left = 0.0f, active_width = 0.0f;
	float positions[32], widths[32], cap = ae_draw_cap_height(AE_FONT_ROW, size);
	short index, count = tabs->count < 32 ? tabs->count : 32;

	/* the ends, then the strip between them */
	left_end = end_glyph(density, x, center_y, glyph, 1, 0);
	right_end = end_glyph(density, x, center_y, glyph, 0, 0);
	end_glyph(density, x, center_y, glyph, 1, 1);
	end_glyph(density, x + width - right_end, center_y, glyph, 0, 1);
	ae_hit_add(x, center_y - glyph * 0.5f, left_end, glyph, hit_id, AE_PART_ARROW_LEFT, -1);
	ae_hit_add(x + width - right_end, center_y - glyph * 0.5f, right_end, glyph, hit_id, AE_PART_ARROW_RIGHT, -1);
	area_x = x + left_end + end_gap;
	area_width = width - left_end - right_end - 2.0f * end_gap;
	for (index = 0; index < count; index++)
	{
		positions[index] = total;
		widths[index] = ae_draw_text_width(AE_FONT_ROW, size, tabs->tabs[index].label ? tabs->tabs[index].label : "");
		total += widths[index] + (index + 1 < count ? gap : 0.0f);
		if (index == tabs->active)
		{
			active_left = positions[index];
			active_width = widths[index];
		}
	}
	/* overflow: the strip scrolls (100 ms) to put the active tab in its middle, within its ends, so it is whole and
	clear of the fades */
	target = 0.0f;
	if (total > area_width)
	{
		target = active_left + active_width * 0.5f - area_width * 0.5f;
		if (target < 0.0f)
			target = 0.0f;
		if (target > total - area_width)
			target = total - area_width;
	}
	/* (a strip never drawn, all zero, starts where it belongs: no slide as its screen opens) */
	if (!tabs->drawn)
		tabs->strip.from = tabs->strip.to = target;
	tabs->drawn = 1;
	if (tabs->strip.to != target)
		ae_motion_start(&tabs->strip, tabs->strip.to, target, AE_MOTION_SCROLL_MS);
	offset = ae_motion_value(&tabs->strip);

	ae_draw_clip_push(area_x - allowance, y, area_width + 2.0f * allowance, height);
	for (index = 0; index < count; index++)
	{
		struct ae_tab const *tab = &tabs->tabs[index];
		float label_x = area_x + positions[index] - offset;
		unsigned int color = index == tabs->active ? AE_COLOR_TITLE : index == tabs->hover ? AE_COLOR_TEXT :
			AE_COLOR_MUTED;

		if (tab->disabled)
			color = scaled_alpha(AE_COLOR_MUTED, AE_DISABLED_ROW_ALPHA);
		if (tab->label)
			ae_draw_text(AE_FONT_ROW, size, label_x, text_top(AE_FONT_ROW, size, center_y), AE_ALIGN_LEFT, color,
				tab->label);
		if (index == tabs->active)
			ae_draw_rect(label_x, center_y + cap * 0.5f + units(density, 8.0f), widths[index], units(density, TAB_UNDERLINE_U),
				0.0f, AE_COLOR_ACCENT);
		/* the change dot at the label's top right */
		if (tab->changed)
		{
			float dot = units(density, TAB_DOT_U);

			ae_draw_rect(label_x + widths[index] + units(density, 2.0f), center_y - cap * 0.5f - dot * 0.5f, dot, dot,
				dot * 0.5f, AE_COLOR_ACCENT);
		}
		hit_between(label_x - gap * 0.5f, y, widths[index] + gap, height, area_x, area_x + area_width, hit_id, AE_PART_TAB,
			index);
	}
	ae_draw_clip_pop();
	/* the cut edges: a 70 u fade and ‹ › */
	if (offset > 0.5f)
	{
		fade(density, area_x, y, height, 1);
		ae_draw_text(AE_FONT_ROW, size, area_x, text_top(AE_FONT_ROW, size, center_y), AE_ALIGN_LEFT, AE_COLOR_TEXT,
			"\xE2\x80\xB9");
	}
	if (offset < total - area_width - 0.5f)
	{
		fade(density, area_x + area_width - units(density, TAB_FADE_U), y, height, 0);
		ae_draw_text(AE_FONT_ROW, size, area_x + area_width, text_top(AE_FONT_ROW, size, center_y), AE_ALIGN_RIGHT,
			AE_COLOR_TEXT, "\xE2\x80\xBA");
	}
	return height;
}

/* a switch to another tab: the cursor sound when it changes */
static int tabs_go(struct ae_tabs *tabs, short active)
{
	if (active != tabs->active)
		ae_sound_request(AE_SOUND_CURSOR, 0);
	tabs->active = active;
	return 1;
}

int ae_tabs_event(struct ae_tabs *tabs, struct ae_event const *event)
{
	if (!tabs || !event)
		return 0;
	switch (event->action)
	{
	case AE_ACTION_TAB_PREVIOUS: return tabs_go(tabs, ae_tabs_step(tabs, -1));
	case AE_ACTION_TAB_NEXT: return tabs_go(tabs, ae_tabs_step(tabs, 1));
	}
	return 0;
}

int ae_tabs_pointer(struct ae_tabs *tabs, struct ae_pointer const *pointer, short hit_id)
{
	struct ae_hit hit;
	int over;

	if (!tabs || !pointer)
		return 0;
	over = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == hit_id;
	if (pointer->moved)
		tabs->hover = over && hit.part == AE_PART_TAB ? hit.index : -1;
	if (!pointer->left_clicks || !over)
		return 0;
	if (hit.part == AE_PART_ARROW_LEFT)
		return tabs_go(tabs, ae_tabs_step(tabs, -1));
	if (hit.part == AE_PART_ARROW_RIGHT)
		return tabs_go(tabs, ae_tabs_step(tabs, 1));
	if (hit.part != AE_PART_TAB || hit.index < 0 || hit.index >= tabs->count)
		return 0;
	/* (a disabled tab: the screen shows why) */
	if (tabs->tabs[hit.index].disabled)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 2;
	}
	return tabs_go(tabs, hit.index);
}

/* ---------- page dots */

short ae_page_step(short count, short current, short direction)
{
	return step(count, current, direction, NULL, NULL);
}

/* the height an end of a strip takes: its glyph's, or its key cap's (grown at VIEW to hold its words) */
static float end_height(struct ae_density const *density, float glyph)
{
	return ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE ? cap_height(density, glyph) : glyph;
}

/* the dots' row's height (its ends included), for the panel's layout */
static float dots_row_height(struct ae_density const *density)
{
	float glyph = ae_size_glyph(density), cap = cap_height(density, glyph);

	/* (as tall as the taller of a glyph and a grown cap: the device can change between frames) */
	return cap > glyph ? cap : glyph;
}

/* page dots within max_width (0: no limit). Short of room the gaps shrink, down to DOTS_GAP_MINIMUM_U; shorter still,
the dots give way to a "3 / 12" counter in the current page's colour (spec 4.13 has no word on many pages: a ruling) */
#define DOTS_GAP_MINIMUM_U 4.0f
static void page_dots(struct ae_density const *density, float center_x, float y, const char *page_title,
	short count, short current, unsigned int current_color, short hit_id, float max_width)
{
	float dot = units(density, DOT_U), dot_gap = units(density, DOT_GAP_U), pill = units(density, PILL_U);
	float glyph = ae_size_glyph(density), title = floored_minor(density, density->metrics->tab_text);
	float row_center, width, dot_x, end_gap = units(density, DOTS_END_GAP_U), left_end, right_end, ends;
	short index;
	int counter = 0;
	char text[32];

	if (page_title && *page_title)
	{
		ae_draw_text(AE_FONT_ROW, title, center_x, y, AE_ALIGN_CENTER, AE_COLOR_TITLE, page_title);
		y += ae_draw_cap_height(AE_FONT_ROW, title) + units(density, DOTS_TITLE_GAP_U);
	}
	/* (the row's middle half the taller end down: a grown key cap stays inside the row) */
	row_center = y + (end_height(density, glyph) > glyph ? end_height(density, glyph) : glyph) * 0.5f;
	left_end = end_glyph(density, 0, row_center, glyph, 1, 0);
	right_end = end_glyph(density, 0, row_center, glyph, 0, 0);
	ends = left_end + right_end + 2.0f * end_gap;
	width = count > 0 ? (float)(count - 1) * (dot + dot_gap) + (current >= 0 && current < count ? pill : dot) : 0.0f;
	if (max_width > 0.0f && width + ends > max_width && count > 1)
	{
		/* (the gaps shrink first) */
		dot_gap = (max_width - ends - (float)(count - 1) * dot - pill) / (float)(count - 1);
		if (dot_gap < units(density, DOTS_GAP_MINIMUM_U))
		{
			counter = 1;
			snprintf(text, sizeof(text), ae_string(AE_STR_COUNT), current + 1, count);
			width = ae_draw_text_width(AE_FONT_ROW, floored_minor(density, density->metrics->minor), text);
		}
		else
			width = (float)(count - 1) * (dot + dot_gap) + pill;
	}
	dot_x = center_x - width * 0.5f;
	if (counter)
	{
		float size = floored_minor(density, density->metrics->minor);

		ae_draw_text(AE_FONT_ROW, size, center_x, text_top(AE_FONT_ROW, size, row_center), AE_ALIGN_CENTER, current_color,
			text);
	}
	else
	{
		for (index = 0; index < count; index++)
		{
			float item = index == current ? pill : dot;

			ae_draw_rect(dot_x, row_center - dot * 0.5f, item, dot, dot * 0.5f, index == current ? current_color :
				AE_COLOR_DOT);
			/* (clickable: a hit as tall as the row, over the dot and half the gaps; none without a hit id) */
			if (hit_id >= 0)
				ae_hit_add(dot_x - dot_gap * 0.5f, row_center - glyph * 0.5f, item + dot_gap, glyph, hit_id, AE_PART_DOT,
					index);
			dot_x += item + dot_gap;
		}
	}
	/* LB / RB (Q / E) at the ends */
	end_glyph(density, center_x - width * 0.5f - end_gap - left_end, row_center, glyph, 1, 1);
	end_glyph(density, center_x + width * 0.5f + end_gap, row_center, glyph, 0, 1);
	if (hit_id >= 0)
	{
		ae_hit_add(center_x - width * 0.5f - end_gap - left_end, row_center - glyph * 0.5f, left_end, glyph, hit_id,
			AE_PART_ARROW_LEFT, -1);
		ae_hit_add(center_x + width * 0.5f + end_gap, row_center - glyph * 0.5f, right_end, glyph, hit_id,
			AE_PART_ARROW_RIGHT, -1);
	}
}

void ae_widget_page_dots(struct ae_density const *density, float center_x, float y, const char *page_title,
	short count, short current, unsigned int current_color, short hit_id)
{
	page_dots(density, center_x, y, page_title, count, current, current_color, hit_id, 0.0f);
}

/* ---------- prompts */

const char *ae_prompt_key(int button)
{
	switch (button)
	{
	case AE_BUTTON_A: return ae_string(AE_STR_KEY_ENTER);
	case AE_BUTTON_B: return ae_string(AE_STR_KEY_ESC);
	case AE_BUTTON_X: return ae_string(AE_STR_KEY_CTRL_F);
	case AE_BUTTON_Y: return ae_string(AE_STR_KEY_R);
	case AE_BUTTON_LEFT_SHOULDER: return ae_string(AE_STR_KEY_Q);
	case AE_BUTTON_RIGHT_SHOULDER: return ae_string(AE_STR_KEY_E);
	case AE_BUTTON_LEFT_TRIGGER: return ae_string(AE_STR_KEY_PAGE_UP);
	case AE_BUTTON_RIGHT_TRIGGER: return ae_string(AE_STR_KEY_PAGE_DOWN);
	case AE_BUTTON_START: return ae_string(AE_STR_KEY_ENTER);
	}
	return NULL;
}

unsigned int ae_prompt_tint(int device, int button)
{
	static const unsigned int xbox_tints[4] = { AE_TINT_XBOX_A, AE_TINT_XBOX_B, AE_TINT_XBOX_X, AE_TINT_XBOX_Y };
	static const unsigned int playstation_tints[4] =
	{
		AE_TINT_PS_CROSS, AE_TINT_PS_CIRCLE, AE_TINT_PS_SQUARE, AE_TINT_PS_TRIANGLE,
	};

	/* (face buttons only; Nintendo, shoulders, triggers and START monochrome) */
	if (button < AE_BUTTON_A || button > AE_BUTTON_Y)
		return AE_COLOR_KEY_CAP;
	if (device == AE_DEVICE_XBOX)
		return xbox_tints[button - AE_BUTTON_A];
	if (device == AE_DEVICE_PLAYSTATION)
		return playstation_tints[button - AE_BUTTON_A];
	return AE_COLOR_KEY_CAP;
}

/* the prompt under the pointer after its last move, per footer (hit id): the hover wash */
static short prompt_hover_id = -1, prompt_hover_index = -1;

/* a prompt's parts (drawing units): its mark (glyph or cap) and label widths, its label as drawn */
struct prompt_parts
{
	float mark, label_width, width;
	const char *key;
	char label[160];
};

/* the prompts' parts for a device, labels shortened to label_room each (0: whole); their total width with the gaps */
static float prompt_parts(struct ae_density const *density, int device, struct ae_prompt const *prompts, short count,
	float label_room, struct prompt_parts *parts)
{
	int font = device_font(device);
	float glyph = ae_size_glyph(density), label_size = floored_minor(density, density->metrics->footer);
	float gap = units(density, PROMPT_GAP_U), total = 0.0f;
	short index;

	for (index = 0; index < count; index++)
	{
		struct ae_prompt const *prompt = &prompts[index];
		struct prompt_parts *part = &parts[index];

		if (label_room > 0.0f)
			ae_fit_text(AE_FONT_BODY, label_size, prompt->label ? prompt->label : "", label_room, part->label,
				sizeof(part->label));
		else
			snprintf(part->label, sizeof(part->label), "%s", prompt->label ? prompt->label : "");
		part->label_width = part->label[0] ? ae_draw_text_width(AE_FONT_BODY, label_size, part->label) : 0.0f;
		part->key = NULL;
		if (font < 0)
		{
			float cap = cap_height(density, glyph), pad_x = cap * BUTTON_PAD_X;

			part->key = prompt->key ? prompt->key : ae_prompt_key(prompt->button);
			part->mark = part->key ? key_cap_width(density, glyph, part->key) : 0.0f;
			part->width = pad_x + (part->key ? part->mark + pad_x : 0.0f) + part->label_width + pad_x;
		}
		else
		{
			if (device == AE_DEVICE_PLAYSTATION && (prompt->button == AE_BUTTON_START || prompt->button == AE_BUTTON_BACK))
				part->mark = key_cap_width(density, glyph,
					ae_string(prompt->button == AE_BUTTON_START ? AE_STR_CAP_OPTIONS : AE_STR_CAP_CREATE));
			else
				part->mark = ae_draw_button_width(font, prompt->button, glyph);
			part->width = part->mark + glyph * GLYPH_GAP + part->label_width;
		}
		total += part->width + (index + 1 < count ? gap : 0.0f);
	}
	return total;
}

void ae_widget_prompts(struct ae_density const *density, float x, float center_y, struct ae_prompt const *prompts,
	short count, const char *status, float right_x, short pressed, short hit_id)
{
	static struct prompt_parts parts[16];
	int device = ae_ui_last_device(), font = device_font(device);
	float glyph = ae_size_glyph(density), label_size = floored_minor(density, density->metrics->footer);
	float gap = units(density, PROMPT_GAP_U), rule = units(density, RULE_U), pen = x, total, room = right_x - x;
	float status_width = status && *status ? ae_draw_text_width(AE_FONT_BODY, label_size, status) : 0.0f;
	short index;

	if (count > 16)
		count = 16;
	/* room: right_x is the row's right end (the status's too). Short of it the status goes first, then the labels
	are shortened alike with "…" */
	total = prompt_parts(density, device, prompts, count, 0.0f, parts);
	if (status_width > 0.0f && total + gap + status_width > room)
		status_width = 0.0f;
	if (total > room && count > 0)
	{
		/* (the widest label room that fits, by halving: every label at most that wide) */
		float low = 0.0f, high = 0.0f;
		int round;

		for (index = 0; index < count; index++)
			high = parts[index].label_width > high ? parts[index].label_width : high;
		for (round = 0; round < 16; round++)
		{
			float middle = (low + high) * 0.5f;

			if (prompt_parts(density, device, prompts, count, middle, parts) <= room)
				low = middle;
			else
				high = middle;
		}
		/* (even no labels short of room: the caps alone, as they are) */
		total = prompt_parts(density, device, prompts, count, low > 0.0f ? low : 0.001f, parts);
	}
	for (index = 0; index < count; index++)
	{
		struct ae_prompt const *prompt = &prompts[index];
		struct prompt_parts const *part = &parts[index];

		if (font < 0)
		{
			/* the keyboard: one button, a rule outline around a drawn key cap and the label */
			float cap = cap_height(density, glyph), pad_x = cap * BUTTON_PAD_X;
			float height = cap * (1.0f + 2.0f * BUTTON_PAD_Y), top = center_y - height * 0.5f;

			if (index == pressed)
				ae_draw_rect(pen, top, part->width, height, 0.0f, AE_COLOR_PRESSED);
			else if (hit_id == prompt_hover_id && index == prompt_hover_index)
				ae_draw_rect(pen, top, part->width, height, 0.0f, AE_COLOR_HOVER);
			ae_draw_outline(pen, top, part->width, height, 0.0f, rule, index == pressed ? AE_COLOR_ACCENT : AE_COLOR_RULE);
			if (part->key)
				ae_widget_key_cap(density, pen + pad_x, center_y - glyph * 0.5f, glyph, part->key);
			if (part->label[0])
				ae_draw_text(AE_FONT_BODY, label_size, pen + pad_x + (part->key ? part->mark + pad_x : 0.0f),
					text_top(AE_FONT_BODY, label_size, center_y), AE_ALIGN_LEFT, AE_COLOR_TEXT, part->label);
			ae_hit_add(pen, top, part->width, height, hit_id, AE_PART_PROMPT, index);
		}
		else
		{
			/* a pad: its glyph (face buttons tinted), or PlayStation's drawn OPTIONS / CREATE caps */
			if (device == AE_DEVICE_PLAYSTATION && (prompt->button == AE_BUTTON_START || prompt->button == AE_BUTTON_BACK))
				ae_widget_key_cap(density, pen, center_y - glyph * 0.5f, glyph,
					ae_string(prompt->button == AE_BUTTON_START ? AE_STR_CAP_OPTIONS : AE_STR_CAP_CREATE));
			else
				ae_draw_button(font, prompt->button, glyph, pen, center_y - glyph * 0.5f, ae_prompt_tint(device, prompt->button));
			if (part->label[0])
				ae_draw_text(AE_FONT_BODY, label_size, pen + part->mark + glyph * GLYPH_GAP, text_top(AE_FONT_BODY, label_size,
					center_y), AE_ALIGN_LEFT, AE_COLOR_TEXT, part->label);
			ae_hit_add(pen, center_y - glyph * 0.5f, part->width, glyph, hit_id, AE_PART_PROMPT, index);
		}
		pen += part->width + gap;
	}
	if (status_width > 0.0f)
		ae_draw_text(AE_FONT_BODY, label_size, right_x, text_top(AE_FONT_BODY, label_size, center_y), AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, status);
}

int ae_prompts_pointer(struct ae_prompt const *prompts, short count, struct ae_pointer const *pointer,
	short hit_id, struct ae_event *event)
{
	struct ae_hit hit;
	int over;

	if (!prompts || !pointer || !event)
		return 0;
	over = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == hit_id && hit.part == AE_PART_PROMPT &&
		hit.index >= 0 && hit.index < count;
	/* (the hover is this footer's only while over it: another footer's move never clears it) */
	if (pointer->moved)
	{
		if (over)
		{
			prompt_hover_id = hit_id;
			prompt_hover_index = hit.index;
		}
		else if (prompt_hover_id == hit_id)
			prompt_hover_id = prompt_hover_index = -1;
	}
	if (!pointer->left_clicks || !over)
		return 0;
	event->player = 0;
	event->action = (unsigned char)prompts[hit.index].action;
	event->device = AE_DEVICE_KEYBOARD_MOUSE;
	event->repeat = 0;
	return 1;
}

/* ---------- the in-view panel (spec 6) */

/* the panel's parts, drawing units of the view: its rectangle, header row, dots row, footer */
struct panel_layout
{
	struct ae_rect rect;
	float pad, header_center, header_size, name_size, emblem, dots_y, content_top, help_y, help_height, rule_y,
		prompts_center;
};

static void panel_layout(struct ae_density const *density, float view_width, float view_height, struct panel_layout *l)
{
	struct ae_rect panel;
	float glyph = ae_size_glyph(density), stripe = units(density, PANEL_STRIPE_U);
	float body = floored_minor(density, density->metrics->body), line = units(density, density->metrics->body_line);
	float prompts_height, button, base_scale = ae_view_scale(view_width, view_height, 1.0f);

	/* (the density's s is the view's scale times UI SCALE: UI SCALE is their ratio; the panel's window pixels from
	the view's top left, to the view's own units) */
	ae_view_panel_rect(view_width, view_height, base_scale > 0.0f ? density->s / base_scale : 1.0f, &panel);
	l->rect.x = panel.x * density->pixel;
	l->rect.y = panel.y * density->pixel;
	l->rect.width = panel.width * density->pixel;
	l->rect.height = panel.height * density->pixel;
	l->pad = units(density, density->metrics->pad);
	l->header_size = ae_size_text(density);
	l->name_size = floored_minor(density, density->metrics->minor);
	l->emblem = l->header_size * 1.3f;
	l->header_center = l->rect.y + stripe + l->pad + l->emblem * 0.5f;
	l->dots_y = l->header_center + l->emblem * 0.5f + l->pad * 0.5f;
	/* (the rows sized by what they really hold: a key cap grown at VIEW, a keyboard prompt's button around one; the
	content gives up the room) */
	l->content_top = l->dots_y + dots_row_height(density) + l->pad * 0.5f;
	/* the footer, from the bottom: the prompts row, a rule over it, the help strip's two lines */
	if (line < body * 1.4f)
		line = body * 1.4f;
	button = cap_height(density, glyph) * (1.0f + 2.0f * BUTTON_PAD_Y);
	prompts_height = (button > glyph ? button : glyph) + 2.0f * units(density, density->metrics->gap);
	l->prompts_center = l->rect.y + l->rect.height - l->pad - prompts_height * 0.5f;
	l->rule_y = l->rect.y + l->rect.height - l->pad - prompts_height;
	l->help_height = 2.0f * line;
	l->help_y = l->rule_y - units(density, density->metrics->gap) - l->help_height;
	/* (a view too small for it all: the footer starts under the dots, never over the header) */
	if (l->help_y < l->content_top)
	{
		l->help_y = l->content_top;
		l->rule_y = l->help_y + l->help_height + units(density, density->metrics->gap);
		l->prompts_center = l->rule_y + prompts_height * 0.5f;
	}
}

void ae_widget_view_panel(struct ae_density const *density, float view_width, float view_height, short player,
	unsigned int player_color, const char *page_name, short page, short page_count, struct ae_rect *content)
{
	struct panel_layout l;
	char text[32];
	float x, cap;

	panel_layout(density, view_width, view_height, &l);
	/* the fill (95 %; no dim over the rest of the view) and the player's stripe */
	ae_draw_rect(l.rect.x, l.rect.y, l.rect.width, l.rect.height, units(density, density->metrics->corner),
		AE_COLOR_VIEW_PANEL);
	ae_draw_rect(l.rect.x, l.rect.y, l.rect.width, units(density, PANEL_STRIPE_U), 0.0f, player_color);
	/* the header: emblem with the slot number, "PLAYER n", the page's name on the right */
	x = l.rect.x + l.pad;
	ae_draw_rect(x, l.header_center - l.emblem * 0.5f, l.emblem, l.emblem, units(density, density->metrics->corner),
		player_color);
	snprintf(text, sizeof(text), "%d", player + 1);
	cap = l.header_size * 0.8f;
	ae_draw_text(AE_FONT_ROW, cap, x + l.emblem * 0.5f, text_top(AE_FONT_ROW, cap, l.header_center), AE_ALIGN_CENTER,
		AE_COLOR_SELECTION_TEXT, text);
	snprintf(text, sizeof(text), ae_string(AE_STR_PLAYER_N), player + 1);
	ae_draw_text(AE_FONT_ROW, l.header_size, x + l.emblem + l.pad * 0.6f, text_top(AE_FONT_ROW, l.header_size,
		l.header_center), AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	if (page_name && *page_name)
		ae_draw_text(AE_FONT_ROW, l.name_size, l.rect.x + l.rect.width - l.pad, text_top(AE_FONT_ROW, l.name_size,
			l.header_center), AE_ALIGN_RIGHT, AE_COLOR_TEXT, page_name);
	/* the page dots, the current one in the player's colour (always beside "PLAYER n") */
	if (page_count > 0)
		page_dots(density, l.rect.x + l.rect.width * 0.5f, l.dots_y, NULL, page_count, page, player_color, -1,
			l.rect.width - 2.0f * l.pad);
	/* the rule over the prompts */
	ae_draw_rect(l.rect.x + l.pad, l.rule_y, l.rect.width - 2.0f * l.pad, units(density, RULE_U), 0.0f, AE_COLOR_RULE);
	if (content)
	{
		content->x = l.rect.x + l.pad;
		content->y = l.content_top;
		content->width = l.rect.width - 2.0f * l.pad;
		content->height = l.help_y - l.content_top;
		if (content->height < 0.0f)
			content->height = 0.0f;
	}
}

void ae_view_panel_footer(struct ae_density const *density, float view_width, float view_height, float *help_y,
	float *prompts_center_y)
{
	struct panel_layout l;

	panel_layout(density, view_width, view_height, &l);
	*help_y = l.help_y;
	*prompts_center_y = l.prompts_center;
}

void ae_widget_help_strip(struct ae_density const *density, float x, float y, float width, const char *text)
{
	char lines[2][160];
	float size = floored_minor(density, density->metrics->body), line = units(density, density->metrics->body_line);
	int count, index;

	if (line < size * 1.4f)
		line = size * 1.4f;
	count = ae_wrap_text(AE_FONT_BODY, size, text, width, lines, 2);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, size, x, text_top(AE_FONT_BODY, size, y + line * ((float)index + 0.5f)), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, lines[index]);
}
