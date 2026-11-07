/*
AE_ITEM_PROMPT_RULES.C

Pure rules for the ITEM PROMPT (ae_item_prompt_rules.h). No engine includes.
*/

#include "ae_item_prompt_rules.h"

#include <math.h>
#include <wchar.h>
#include <wctype.h>

/* Overpass's capitals are 0.70 of its em (ae_hud_text.h) */
#define IP_CAP 0.70f
/* the action's floor, the HUD's */
#define IP_ACTION_FLOOR 16
/* the sizes at k = 1: the default 70 % at 1080 lines */
#define IP_NAME_BASE 22.0f
#define IP_ACTION_BASE 18.0f
#define IP_ROW_GAP_BASE 6.0f
#define IP_ROW_GAP_FLOOR 4
#define IP_CHIP_RATIO 1.39f
#define IP_WIDTH 338.0f
#define IP_TOP 93.0f
/* the waypoint text scale at the default size in one view: 0.7 x 0.7 */
#define IP_WAYPOINT_DEFAULT 0.49f

static int whole(float value)
{
	return (int)floorf(value + 0.5f);
}

/* ---------- the words */

int ae_ip_words(int kind, wchar_t const *detail, wchar_t *before, wchar_t *after, int after_size)
{
	wchar_t const *verb = L"Press";
	wchar_t const *rest = L"";
	int has_detail = detail && detail[0];

	if (before)
		before[0] = 0;
	if (after && after_size > 0)
		after[0] = 0;
	if (!before || !after || after_size < 2)
		return 0;
	switch (kind)
	{
	case AE_IP_PICKUP: verb = L"Hold"; rest = L"to pick up"; break;
	case AE_IP_SWAP: verb = L"Hold"; rest = L"to swap"; break;
	case AE_IP_ENTER: rest = L"to enter"; break;
	case AE_IP_FLIP: rest = L"to flip"; break;
	case AE_IP_DEVICE: rest = L"to touch device"; break;
	case AE_IP_CUSTOM: rest = L"to"; break;
	case AE_IP_SWITCH: rest = L"to switch"; break;
	case AE_IP_REVIVE: verb = L"Hold"; rest = L"to revive"; break;
	default:
		return 0;
	}
	wcsncpy(before, verb, AE_IP_BEFORE_SIZE - 1);
	before[AE_IP_BEFORE_SIZE - 1] = 0;
	wcsncpy(after, rest, (size_t)after_size - 1);
	after[after_size - 1] = 0;
	if (has_detail && (kind == AE_IP_ENTER || kind == AE_IP_CUSTOM))
	{
		size_t used = wcslen(after);

		/* (a seat's word, then "seat": "to enter driver seat"; a custom device's own words after "to") */
		if (used + 1 < (size_t)after_size)
		{
			after[used++] = L' ';
			after[used] = 0;
			wcsncat(after, detail, (size_t)after_size - used - 1);
		}
		if (kind == AE_IP_ENTER && wcslen(after) + 6 < (size_t)after_size)
			wcscat(after, L" seat");
	}
	return 1;
}

int ae_ip_kind_named(int kind)
{
	return kind == AE_IP_PICKUP || kind == AE_IP_SWAP || kind == AE_IP_ENTER || kind == AE_IP_FLIP || kind == AE_IP_SWITCH ||
		kind == AE_IP_REVIVE;
}

void ae_ip_upper(wchar_t const *source, wchar_t *name, int size)
{
	int index = 0;

	if (!name || size < 1)
		return;
	for (; source && source[index] && index < size - 1; index++)
		name[index] = (wchar_t)towupper((wint_t)source[index]);
	name[index] = 0;
}

/* ---------- sizes */

float ae_ip_scale(float waypoint_scale, float drawable_height)
{
	float height = drawable_height > 0.0f ? drawable_height : 1080.0f;

	if (!(waypoint_scale > 0.0f))
		return 0.0f;
	return waypoint_scale / IP_WAYPOINT_DEFAULT * height / 1080.0f;
}

/* the chip for an action size: 1.39 x it, odd or even as the capitals' height is, so it centres on them */
static int chip_for(int action)
{
	int chip = whole(IP_CHIP_RATIO * (float)action);
	int caps = whole(IP_CAP * (float)action);

	if ((chip & 1) != (caps & 1))
		chip++;
	return chip;
}

static int gap_for(int action)
{
	return whole((float)action / 3.0f);
}

void ae_ip_sizes_get(float k, struct ae_ip_sizes *sizes)
{
	if (!(k > 0.0f))
		k = 0.0f;
	sizes->action = whole(IP_ACTION_BASE * k);
	if (sizes->action < IP_ACTION_FLOOR)
		sizes->action = IP_ACTION_FLOOR;
	sizes->name = whole(IP_NAME_BASE * k);
	if (sizes->name < sizes->action + 1)
		sizes->name = sizes->action + 1;
	sizes->chip = chip_for(sizes->action);
	sizes->gap = gap_for(sizes->action);
	sizes->row_gap = whole(IP_ROW_GAP_BASE * k);
	if (sizes->row_gap < IP_ROW_GAP_FLOOR)
		sizes->row_gap = IP_ROW_GAP_FLOOR;
}

void ae_ip_sizes_for_action(struct ae_ip_sizes const *nominal, int action, struct ae_ip_sizes *sizes)
{
	*sizes = *nominal;
	if (action < IP_ACTION_FLOOR)
		action = IP_ACTION_FLOOR;
	sizes->action = action;
	sizes->chip = chip_for(action);
	sizes->gap = gap_for(action);
}

float ae_ip_width_limit(float view_scale)
{
	return IP_WIDTH * (view_scale > 0.0f ? view_scale : 0.0f);
}

float ae_ip_top(float view_centre_y, float view_scale)
{
	return floorf(view_centre_y + IP_TOP * view_scale + 0.5f);
}

int ae_ip_chip_is_word(int chip_px, wchar_t const *label)
{
	int letters = 0;

	if (!label || chip_px >= AE_IP_WORD_BELOW)
		return 0;
	/* (a mouse button's picture stays a picture) */
	if (!wcsncmp(label, L"Mouse", 5) || !wcsncmp(label, L"MOUSE", 5))
		return 0;
	for (; *label; label++)
		if (iswalpha((wint_t)*label))
			letters++;
	return letters >= 4;
}

/* ---------- fit */

int ae_ip_text_fit(wchar_t const *text, ae_ip_measure measure, float px, float limit, wchar_t *out, int size)
{
	int length, keep;
	wchar_t cut[AE_IP_AFTER_SIZE + 4];

	if (!out || size < 1)
		return 0;
	out[0] = 0;
	if (!text)
		return 0;
	length = (int)wcslen(text);
	if (length >= size)
		length = size - 1;
	wmemcpy(out, text, (size_t)length);
	out[length] = 0;
	if (!measure || measure(out, px) <= limit)
		return 0;
	/* (the most characters that fit with the three dots) */
	for (keep = length - 1; keep >= 1; keep--)
	{
		if (keep + 3 >= size || keep + 3 >= (int)(sizeof(cut) / sizeof(cut[0])))
			continue;
		wmemcpy(cut, text, (size_t)keep);
		cut[keep] = L'.';
		cut[keep + 1] = L'.';
		cut[keep + 2] = L'.';
		cut[keep + 3] = 0;
		if (measure(cut, px) <= limit)
		{
			wcscpy(out, cut);
			return 1;
		}
	}
	/* (not even one letter: the dots alone) */
	if (size > 3)
		wcscpy(out, L"...");
	return 1;
}

int ae_ip_name_fit(wchar_t const *name, ae_ip_measure measure, float px, float limit, wchar_t *out, int size)
{
	return ae_ip_text_fit(name, measure, px, limit, out, size);
}

/* the action row's width at a size: the words, the chip and the gap each side of it */
static float row_width(struct ae_ip_text const *text, struct ae_ip_sizes const *sizes, ae_ip_measure measure,
	ae_ip_chip_width chip_width, void *context, float *before, float *after, float *chip)
{
	*before = measure(text->before, (float)sizes->action);
	*after = text->after[0] ? measure(text->after, (float)sizes->action) : 0.0f;
	*chip = chip_width ? chip_width(sizes->chip, sizes->action, context) : (float)sizes->chip;
	return *before + (float)sizes->gap + *chip + (text->after[0] ? (float)sizes->gap + *after : 0.0f);
}

void ae_ip_layout_get(float k, float limit, struct ae_ip_text const *text, ae_ip_measure measure, ae_ip_chip_width chip_width,
	void *context, struct ae_ip_layout *layout)
{
	int action;

	ae_ip_sizes_get(k, &layout->nominal);
	layout->drawn = layout->nominal;
	layout->name[0] = 0;
	layout->after[0] = 0;
	layout->name_width = layout->before_width = layout->after_width = layout->chip_width = layout->row_width = 0.0f;
	if (!measure || !text)
		return;
	/* (the action first: a pixel at a time to the floor, chip and gaps with it) */
	for (action = layout->nominal.action;; action--)
	{
		ae_ip_sizes_for_action(&layout->nominal, action, &layout->drawn);
		layout->row_width = row_width(text, &layout->drawn, measure, chip_width, context, &layout->before_width,
			&layout->after_width, &layout->chip_width);
		if (layout->row_width <= limit || action <= IP_ACTION_FLOOR)
			break;
	}
	wcsncpy(layout->after, text->after, AE_IP_AFTER_SIZE - 1);
	layout->after[AE_IP_AFTER_SIZE - 1] = 0;
	if (layout->row_width > limit && text->after[0])
	{
		/* (at the floor and still too wide: the words after the chip get what is left of the limit) */
		float fixed = layout->before_width + (float)layout->drawn.gap + layout->chip_width + (float)layout->drawn.gap;

		ae_ip_text_fit(text->after, measure, (float)layout->drawn.action, limit - fixed, layout->after, AE_IP_AFTER_SIZE);
		layout->after_width = measure(layout->after, (float)layout->drawn.action);
		layout->row_width = fixed + layout->after_width;
	}
	if (!text->name[0])
		return;
	/* (then the name: down, never under the action's size, then cut) */
	{
		int name_px = layout->nominal.name;

		wcsncpy(layout->name, text->name, AE_IP_NAME_SIZE - 1);
		layout->name[AE_IP_NAME_SIZE - 1] = 0;
		while (name_px > layout->drawn.action && measure(layout->name, (float)name_px) > limit)
			name_px--;
		layout->drawn.name = name_px;
		ae_ip_name_fit(text->name, measure, (float)name_px, limit, layout->name, AE_IP_NAME_SIZE);
		layout->name_width = measure(layout->name, (float)name_px);
	}
}

/* ---------- place */

void ae_ip_place_get(float centre_x, float top, struct ae_ip_layout const *layout, struct ae_ip_place *place)
{
	float name_baseline, chip_top_nominal;

	/* (the nominal sizes fix the baselines: the name's from the capitals' top, the chip's top under it by the row gap) */
	name_baseline = floorf(top + IP_CAP * (float)layout->nominal.name + 0.5f);
	chip_top_nominal = name_baseline + (float)layout->nominal.row_gap;
	place->name_baseline = name_baseline;
	place->name_x = floorf(centre_x - layout->name_width / 2.0f + 0.5f);
	place->chip_middle = chip_top_nominal + (float)layout->nominal.chip / 2.0f;
	place->chip_top = floorf(place->chip_middle - (float)layout->drawn.chip / 2.0f + 0.5f);
	place->action_baseline = floorf(place->chip_middle + IP_CAP * (float)layout->drawn.action / 2.0f + 0.5f);
	place->row_x = floorf(centre_x - layout->row_width / 2.0f + 0.5f);
	place->before_x = place->row_x;
	place->chip_x = place->before_x + layout->before_width + (float)layout->drawn.gap;
	place->after_x = place->chip_x + layout->chip_width + (float)layout->drawn.gap;
}

/* ---------- time */

float ae_ip_alpha(long now, long began, long last_true, int instant)
{
	float in = 1.0f, out = 1.0f;
	long gone = now - last_true;

	if (!instant)
	{
		long shown = now - began;

		in = shown <= 0 ? 0.0f : shown >= AE_IP_FADE_IN_TICKS ? 1.0f : (float)shown / (float)AE_IP_FADE_IN_TICKS;
	}
	if (gone > AE_IP_HOLD_TICKS)
	{
		long fading = gone - AE_IP_HOLD_TICKS;

		out = fading >= AE_IP_FADE_OUT_TICKS ? 0.0f : 1.0f - (float)fading / (float)AE_IP_FADE_OUT_TICKS;
	}
	return in * out;
}

/* ---------- the split line */

float ae_ip_split_room(float view_width, float left, float view_scale, float rest_width)
{
	float room = view_width - 16.0f * view_scale - left - rest_width;

	return room > 0.0f ? room : 0.0f;
}

int ae_ip_split_chip(float text_px)
{
	int chip = whole(1.35f * IP_CAP * text_px);
	int caps = whole(IP_CAP * text_px);

	if ((chip & 1) != (caps & 1))
		chip++;
	return chip;
}

int ae_ip_split_gap(float text_px)
{
	return whole(text_px / 3.0f);
}

float ae_ip_split_name_gap(float text_px)
{
	return text_px;
}

void ae_ip_split_fit(float room_total, float name_gap, float fixed_width, float after_gap, wchar_t const *name, wchar_t const *after,
	ae_ip_measure measure, float px, wchar_t *name_out, wchar_t *after_out)
{
	float name_room, name_width = 0.0f, after_room;

	name_out[0] = 0;
	after_out[0] = 0;
	if (name && name[0])
	{
		/* (the name has the line less what must be there without the action's words) */
		name_room = room_total - name_gap - fixed_width - after_gap;
		ae_ip_text_fit(name, measure, px, name_room > 0.0f ? name_room : 0.0f, name_out, AE_IP_NAME_SIZE);
		name_width = measure(name_out, px) + name_gap;
	}
	if (after && after[0])
	{
		after_room = room_total - name_width - fixed_width - after_gap;
		ae_ip_text_fit(after, measure, px, after_room > 0.0f ? after_room : 0.0f, after_out, AE_IP_AFTER_SIZE);
	}
}
