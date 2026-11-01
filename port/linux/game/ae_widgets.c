/* ae_widgets.c: Arena Evolved menus, the widget core (see ae_widgets.h). No engine includes.

Sizes are spec units (u at FULL, view u at VIEW) turned into drawing units by the density (ae_style.h); the VIEW
pixel floors come from ae_size_text / _minor / _row. Text is placed by its capitals: ae_draw's y is the capitals'
top, so a line is centred on a point by putting the capitals' middle there. Clips and boxes around text allow 2
window pixels each side (horizontal ink can sit 1-2 px outside the advance box). */

#include <stdio.h>
#include <string.h>

#include "../src/ae_draw.h"
#include "../src/ae_font.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_widgets.h"

/* spec 4.1-4.3, in spec units */
#define NOTCH_U 6.0f
#define LABEL_GAP_U 16.0f          /* label to the right block (a ruling: the spec gives none) */
#define ARROW_INK_U 11.0f          /* the value arrows' ink width */
#define ARROW_CELL_U 28.6f         /* 2.6 x 11 u (a ruling: how "2.6 x 11 u cell" reads) */
#define DOT_U 10.0f
#define DOT_GAP_U 12.0f            /* label to the changed dot */
#define BADGE_GAP_U 8.0f           /* dot to the AE badge */
#define BADGE_PAD_U 14.0f          /* the badge's box: text + 14 u (10 let THIS PC / RESTART touch it: settings spec 13) */
#define BADGE_HEIGHT_FULL_U 20.0f  /* (a ruling: not in the spec; mockup 12) */
#define BADGE_HEIGHT_VIEW_U 18.0f
#define BASE_GAP_U 12.0f           /* the struck base value to the value */
#define STRIKE_U 2.0f
/* (0.3.7 SETTINGS, spec 3) the slider: its value's column, its track and the gap between; the sub-screen's chevron */
#define SLIDER_COLUMN_U 112.0f
#define SLIDER_TRACK_U 120.0f
#define SLIDER_TRACK_HEIGHT_U 4.0f
#define SLIDER_GAP_U 16.0f
#define CHEVRON_U 7.0f
#define CHEVRON_GAP_U 10.0f
/* (0.4.2) a sub-screen summary's marquee: its speed (u a second) and its pause at each end */
#define MARQUEE_SPEED_U 30.0f
#define MARQUEE_PAUSE_MS 1500ul
#define MARQUEE_LEAST_U 96.0f
/* (phase B) a key row's cap: its text, height and the room around the text */
#define KEY_CAP_TEXT_U 15.0f
#define KEY_CAP_HEIGHT_U 30.0f
#define KEY_CAP_PAD_U 18.0f
#define PRIMARY_FRAME_U 2.0f       /* (0.3.9) the primary action's menu blue frame */
#define GROUP_INSET_U 10.0f
#define GROUP_TRACKING 0.09f
#define TRACK_PAD_U 6.0f
#define TRACK_MOUSE_U 10.0f
#define TRACK_STRIP_U 24.0f
#define THUMB_MINIMUM_U 40.0f
#define WHEEL_ROWS 3
#define MORE_LINE 2.0f             /* the "N more" line: its text size x this (1.5 put it 6 px under the last row) */
#define MINOR_FLOOR_PIXELS 14.0f
#define CLIP_ALLOWANCE_PIXELS 2.0f
/* Overpass's triangles (U+25C0 / U+25B6): ink 0.594 em wide, from 0.008 to 0.692 em above the baseline (their middle
0.35 em up), capitals 0.70 em */
#define TRIANGLE_INK_EM 0.594f
#define TRIANGLE_MIDDLE_EM 0.35f
#define CAPITALS_EM 0.70f

static const char ellipsis[] = "\xE2\x80\xA6";

/* ---------- hits */

static struct ae_hit hits[AE_HITS_MAXIMUM];
static int hit_count;
static short hit_layer;
/* a hit found no room this frame (the last added are the topmost: popovers'); the hooks log it */
static int hits_overflowed;
/* (ae_widget_list while its rows draw: their hits are cut to the list's rows, layout units) */
static int hit_clip_on;
static float hit_clip[4];

void ae_hits_clear(void)
{
	hit_count = 0;
	hit_layer = 0;
	hits_overflowed = 0;
	hit_clip_on = 0;
}

int ae_hits_overflowed(void)
{
	return hits_overflowed;
}

void ae_hits_layer(short layer)
{
	hit_layer = layer;
}

void ae_hit_add(float x, float y, float width, float height, short id, short part, short index)
{
	struct ae_view view;
	struct ae_hit *hit;
	float right, bottom;

	float left, top;

	/* (a closing screen's ghost takes no hits: it is off the stack) */
	if (ae_ui_drawing_ghost())
		return;

	if (width <= 0.0f || height <= 0.0f)
		return;
	ae_draw_current_view(&view);
	ae_view_to_layout(&view, x, y, &left, &top);
	ae_view_to_layout(&view, x + width, y + height, &right, &bottom);
	if (hit_clip_on)
	{
		left = left > hit_clip[0] ? left : hit_clip[0];
		top = top > hit_clip[1] ? top : hit_clip[1];
		right = right < hit_clip[2] ? right : hit_clip[2];
		bottom = bottom < hit_clip[3] ? bottom : hit_clip[3];
	}
	/* (nothing of it left: a row scrolled out of the list) */
	if (right <= left || bottom <= top)
		return;
	if (hit_count >= AE_HITS_MAXIMUM)
	{
		hits_overflowed = 1;
		return;
	}
	hit = &hits[hit_count++];
	hit->rect.x = left;
	hit->rect.y = top;
	hit->rect.width = right - left;
	hit->rect.height = bottom - top;
	hit->id = id;
	hit->part = part;
	hit->index = index;
	hit->layer = hit_layer;
}

int ae_hit_at(float x, float y, short minimum_layer, struct ae_hit *hit)
{
	int index;

	for (index = hit_count - 1; index >= 0; index--)
	{
		struct ae_hit const *candidate = &hits[index];

		if (candidate->layer >= minimum_layer && x >= candidate->rect.x && y >= candidate->rect.y &&
			x < candidate->rect.x + candidate->rect.width && y < candidate->rect.y + candidate->rect.height)
		{
			if (hit)
				*hit = *candidate;
			return 1;
		}
	}
	return 0;
}

/* ---------- text */

/* copies text into out (out_size bytes with its 0), whole code points only; the bytes copied */
static int copy_text(const char *text, int length, char *out, int out_size)
{
	const char *cursor = text;
	int used = 0;

	if (out_size <= 0)
		return 0;
	while (*cursor && cursor - text < length)
	{
		const char *start = cursor;
		int bytes;

		ae_font_utf8_next(&cursor);
		bytes = (int)(cursor - start);
		if (used + bytes > out_size - 1 || cursor - text > length)
			break;
		memcpy(out + used, start, (size_t)bytes);
		used += bytes;
	}
	out[used] = 0;
	return used;
}

int ae_fit_text(int font, float size, const char *text, float width, char *out, int out_size)
{
	char trial[512];
	const char *cursor;
	int best = 0, fits;

	if (!out || out_size <= 0)
		return 0;
	if (!text)
		text = "";
	if (ae_draw_text_width(font, size, text) <= width)
	{
		/* (whole, unless out is too small for it) */
		return copy_text(text, (int)strlen(text), out, out_size) < (int)strlen(text);
	}
	/* the longest prefix that fits with the ellipsis, on a code point boundary */
	cursor = text;
	while (*cursor)
	{
		int length;

		ae_font_utf8_next(&cursor);
		length = (int)(cursor - text);
		if (length + (int)sizeof(ellipsis) > out_size || length + (int)sizeof(ellipsis) > (int)sizeof(trial))
			break;
		memcpy(trial, text, (size_t)length);
		memcpy(trial + length, ellipsis, sizeof(ellipsis));
		fits = ae_draw_text_width(font, size, trial) <= width;
		if (!fits)
			break;
		best = length;
	}
	/* (no space before the ellipsis) */
	while (best > 0 && text[best - 1] == ' ')
		best--;
	if (!best && ae_draw_text_width(font, size, ellipsis) > width)
	{
		out[0] = 0;
		return 1;
	}
	if (best + (int)sizeof(ellipsis) > out_size)
	{
		out[0] = 0;
		return 1;
	}
	memcpy(out, text, (size_t)best);
	memcpy(out + best, ellipsis, sizeof(ellipsis));
	return 1;
}

int ae_wrap_text(int font, float size, const char *text, float width, char lines[][160], int maximum)
{
	char trial[160];
	const char *cursor = text ? text : "";
	int line = 0;

	while (line < maximum)
	{
		const char *start, *end, *fitting = NULL;

		while (*cursor == ' ')
			cursor++;
		if (!*cursor)
			break;
		/* (the last line takes the rest, cut with "…" when it doesn't fit) */
		if (line == maximum - 1)
		{
			ae_fit_text(font, size, cursor, width, lines[line++], 160);
			break;
		}
		start = cursor;
		end = cursor;
		for (;;)
		{
			const char *word_end = end;

			while (*word_end == ' ')
				word_end++;
			while (*word_end && *word_end != ' ')
				word_end++;
			if (word_end - start >= (long)sizeof(trial))
				break;
			memcpy(trial, start, (size_t)(word_end - start));
			trial[word_end - start] = 0;
			if (ae_draw_text_width(font, size, trial) > width)
				break;
			fitting = word_end;
			end = word_end;
			if (!*word_end)
				break;
		}
		if (!fitting)
		{
			/* (a word wider than the width: cut) */
			const char *word_end = start;

			while (*word_end && *word_end != ' ')
				word_end++;
			copy_text(start, (int)(word_end - start), trial, sizeof(trial));
			ae_fit_text(font, size, trial, width, lines[line++], 160);
			cursor = word_end;
			continue;
		}
		copy_text(start, (int)(fitting - start), lines[line++], 160);
		cursor = fitting;
	}
	return line;
}

/* ---------- small shapes */

static float units(struct ae_density const *density, float spec_units)
{
	return spec_units * density->unit;
}

/* a colour's alpha times a factor */
static unsigned int scaled_alpha(unsigned int rgba, float factor)
{
	float alpha = (float)(rgba & 0xFFu) * factor;

	return (rgba & 0xFFFFFF00u) | (unsigned int)(alpha < 0.0f ? 0.0f : alpha > 255.0f ? 255.0f : alpha + 0.5f);
}

/* the capitals' top for a line of text centred on center_y */
static float text_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

void ae_widget_separator(struct ae_density const *density, float x, float center_y, float diameter_u,
	unsigned int rgba)
{
	float radius = units(density, diameter_u) * 0.5f;

	ae_draw_rect(x - radius, center_y - radius, 2.0f * radius, 2.0f * radius, radius, rgba);
}

void ae_widget_dashed(float x, float y, float width, float height, float thickness, float dash, unsigned int rgba)
{
	float along;

	if (dash <= 0.0f || thickness <= 0.0f)
		return;
	for (along = 0.0f; along < width; along += 2.0f * dash)
	{
		float length = width - along < dash ? width - along : dash;

		ae_draw_rect(x + along, y, length, thickness, 0.0f, rgba);
		ae_draw_rect(x + along, y + height - thickness, length, thickness, 0.0f, rgba);
	}
	for (along = 0.0f; along < height; along += 2.0f * dash)
	{
		float length = height - along < dash ? height - along : dash;

		ae_draw_rect(x, y + along, thickness, length, 0.0f, rgba);
		ae_draw_rect(x + width - thickness, y + along, thickness, length, 0.0f, rgba);
	}
}

/* ---------- row */

static float minor_size(struct ae_density const *density)
{
	return ae_size_minor(density);
}

/* the minor floor (VIEW: 14 px) for small texts the spec sizes below it at small views: group headers, badges */
static float floored_minor(struct ae_density const *density, float spec_units)
{
	return ae_size(density, spec_units, density->kind == AE_DENSITY_VIEW ? MINOR_FLOOR_PIXELS : 0.0f);
}

static float badge_size(struct ae_density const *density)
{
	return floored_minor(density, density->metrics->badge);
}

/* the accent badge's text: AE, or the row's own (COMMUNITY, NHE) */
static const char *row_source_badge(struct ae_row const *row)
{
	return row->source_badge && row->source_badge[0] ? row->source_badge : ae_string(AE_STR_BADGE_AE);
}

/* the badge's box: 20 u tall at FULL, 18 at VIEW, and never shorter than its text needs (a floored text) */
#define BADGE_TEXT_LINE 1.5f
static float badge_height(struct ae_density const *density)
{
	float height = units(density, density->kind == AE_DENSITY_VIEW ? BADGE_HEIGHT_VIEW_U : BADGE_HEIGHT_FULL_U);
	float needed = badge_size(density) * BADGE_TEXT_LINE;

	return height > needed ? height : needed;
}

void ae_row_layout(struct ae_density const *density, float width, struct ae_row const *row,
	struct ae_row_layout *layout)
{
	float pad = units(density, density->metrics->pad), text = ae_size_text(density);
	float right = width - pad, extras = 0.0f, room, gap, block_left, label_full, value_size;
	int focused = (row->flags & AE_ROW_FOCUSED) != 0, disabled = (row->flags & AE_ROW_DISABLED) != 0;
	int changed = (row->flags & AE_ROW_CHANGED) && !disabled, badge = (row->flags & AE_ROW_AE) != 0;
	int slider = (row->flags & AE_ROW_SLIDER) && !disabled;
	int quiet = (row->flags & (AE_ROW_SUBSCREEN | AE_ROW_STATUS | AE_ROW_LINK | AE_ROW_KEY)) != 0;
	int arrows = focused && row->value && !disabled && !quiet;
	int key = (row->flags & AE_ROW_KEY) && !disabled;
	const char *shown = disabled ? row->reason : row->value;
	/* (a marquee's summary measured in the font it is drawn in) */
	int marquee = (row->flags & AE_ROW_MARQUEE) && (row->flags & AE_ROW_SUBSCREEN) && !disabled;
	int value_font = marquee ? AE_FONT_BODY : AE_FONT_ROW;
	int index;

	memset(layout, 0, sizeof(*layout));
	layout->height = ae_size_row(density);
	layout->arrow_cell = units(density, ARROW_CELL_U);
	/* (a disabled row's reason and a sub-screen's summary at the minor size, as the mockups: 04, 25; settings 3) */
	value_size = disabled || (row->flags & AE_ROW_SUBSCREEN) ? minor_size(density) : text;
	if (key)
		value_size = units(density, KEY_CAP_TEXT_U);
	/* the right block first: arrow, value (or reason), arrow (a slider's track), the sub-screen's chevron */
	if (arrows)
	{
		right -= layout->arrow_cell;
		layout->arrow_right_x = right;
	}
	if (row->flags & (AE_ROW_SUBSCREEN | AE_ROW_LINK))
	{
		layout->chevron_x = right - units(density, CHEVRON_U);
		right = layout->chevron_x - units(density, CHEVRON_GAP_U);
	}
	if (shown)
	{
		snprintf(layout->value, sizeof(layout->value), "%s", shown);
		layout->value_width = ae_draw_text_width(value_font, value_size, shown);
		if (key)
			layout->value_width += units(density, KEY_CAP_PAD_U);
	}
	/* the label's whole width and what follows it (the dot, the badges) are reserved before anything on the left of
	the value: the label never gives way while the struck base or the value could (settings spec 3) */
	label_full = row->label && row->label[0] ? ae_draw_text_width(AE_FONT_ROW, text, row->label) : 0.0f;
	layout->dot = changed;
	layout->badge = badge;
	if (changed)
		extras += units(density, DOT_GAP_U) + units(density, DOT_U);
	if (badge)
	{
		layout->badge_width = ae_draw_text_width(AE_FONT_ROW, badge_size(density), row_source_badge(row)) +
			units(density, BADGE_PAD_U);
		extras += units(density, changed ? BADGE_GAP_U : DOT_GAP_U) + layout->badge_width;
	}
	for (index = 0; index < AE_ROW_BADGES; index++)
	{
		if (!row->badges[index] || !row->badges[index][0])
			continue;
		layout->badges_width[layout->badge_count] = ae_draw_text_width(AE_FONT_ROW, badge_size(density),
			row->badges[index]) + units(density, BADGE_PAD_U);
		extras += units(density, changed || badge || layout->badge_count ? BADGE_GAP_U : DOT_GAP_U) +
			layout->badges_width[layout->badge_count];
		layout->badge_count++;
	}
	layout->label_x = pad;
	gap = shown || arrows || changed ? units(density, LABEL_GAP_U) : 0.0f;
	/* the value cut ("…") only as far as the label needs, never under a third of its own width */
	if (shown && !slider && !key)
	{
		float needed = layout->label_x + label_full + extras + gap + (arrows ? layout->arrow_cell : 0.0f);
		float left = right - layout->value_width;

		if (needed > left)
		{
			float allowed = right - needed, least = layout->value_width / 3.0f;

			/* (a marquee shows all of it in turn: its room may be less, down to MARQUEE_LEAST_U) */
			if (marquee && !ae_motion_reduced() && least > units(density, MARQUEE_LEAST_U))
				least = units(density, MARQUEE_LEAST_U);

			if (allowed < least)
				allowed = least;
			/* (a marquee keeps its summary whole and scrolls it through the room; REDUCE MOTION cuts it) */
			if (allowed < layout->value_width && marquee && !ae_motion_reduced())
			{
				/* (a pixel short of the label's need: its room never rounds below its width) */
				allowed -= density->pixel;
				layout->value_overflow = layout->value_width - allowed;
				layout->value_width = allowed;
				layout->value_offset = ae_marquee_offset(layout->value_overflow, units(density, MARQUEE_SPEED_U),
					row->marquee_ms);
			}
			else if (allowed < layout->value_width)
			{
				ae_fit_text(value_font, value_size, shown, allowed, layout->value, sizeof(layout->value));
				layout->value_width = ae_draw_text_width(value_font, value_size, layout->value);
			}
		}
	}
	layout->value_x = right - layout->value_width;
	block_left = layout->value_x;
	if (arrows)
	{
		layout->arrow_left_x = layout->value_x - layout->arrow_cell;
		block_left = layout->arrow_left_x;
	}
	if (slider)
	{
		/* (the value right in its column; the track left of the column, or of the left arrow hugging a wide value) */
		float column = right - units(density, SLIDER_COLUMN_U);

		if (block_left < column)
			column = block_left;
		layout->track_width = units(density, SLIDER_TRACK_U);
		layout->track_x = column - units(density, SLIDER_GAP_U) - layout->track_width;
		block_left = layout->track_x;
	}
	right = block_left;
	/* the struck base value: whole when it fits beside the label, else shortened, else left out (it is the least
	needed: the help says it too) */
	if (changed && row->base_value)
	{
		float room_for_base = right - units(density, BASE_GAP_U) - gap - layout->label_x - label_full - extras;

		if (room_for_base > 0.0f)
			ae_fit_text(AE_FONT_BODY, minor_size(density), row->base_value, room_for_base, layout->base,
				sizeof(layout->base));
		if (layout->base[0])
		{
			layout->base_width = ae_draw_text_width(AE_FONT_BODY, minor_size(density), layout->base);
			layout->base_x = right - units(density, BASE_GAP_U) - layout->base_width;
			right = layout->base_x;
		}
	}
	/* the label gets the rest, less the gap and the dot and badges after it, shortened with "…" only now; when even
	the dot and the badges have no room, they are left out rather than drawn over the right block */
	room = right - gap - layout->label_x - extras;
	if (room < 0.0f && extras > 0.0f)
	{
		layout->dot = layout->badge = 0;
		layout->badge_width = 0.0f;
		layout->badge_count = 0;
		room += extras;
	}
	layout->label_cut = ae_fit_text(AE_FONT_ROW, text, row->label ? row->label : "", room, layout->label,
		sizeof(layout->label));
	layout->label_width = layout->label[0] ? ae_draw_text_width(AE_FONT_ROW, text, layout->label) : 0.0f;
	layout->dot_x = layout->label_x + layout->label_width + units(density, DOT_GAP_U);
	layout->badge_x = layout->dot ? layout->dot_x + units(density, DOT_U) + units(density, BADGE_GAP_U) :
		layout->label_x + layout->label_width + units(density, DOT_GAP_U);
	{
		float x = layout->badge ? layout->badge_x + layout->badge_width + units(density, BADGE_GAP_U) : layout->badge_x;

		for (index = 0; index < layout->badge_count; index++)
		{
			layout->badges_x[index] = x;
			x += layout->badges_width[index] + units(density, BADGE_GAP_U);
		}
	}
}

/* (0.3.7) the slider's track: the rule's colour at .25, or the selection text's on the bar */
#define SLIDER_TRACK 0xC8D6E040u
#define SLIDER_TRACK_ON_BAR 0x0F171E40u

/* the nth badge given (the layout counts only those with text) */
static const char *row_badge(struct ae_row const *row, int nth)
{
	int index;

	for (index = 0; index < AE_ROW_BADGES; index++)
	{
		if (row->badges[index] && row->badges[index][0] && nth-- == 0)
			return row->badges[index];
	}
	return "";
}

/* a value arrow (U+25C0 left, U+25B6 right) centred in its cell */
static void arrow(struct ae_density const *density, float cell_x, float cell_width, float center_y, int right,
	unsigned int rgba)
{
	float em = units(density, ARROW_INK_U) / TRIANGLE_INK_EM;

	/* (the triangle's middle on the row's: the baseline 0.35 em below it, the capitals' top 0.70 em above that) */
	ae_draw_text(AE_FONT_ROW, em, cell_x + cell_width * 0.5f, center_y - (CAPITALS_EM - TRIANGLE_MIDDLE_EM) * em,
		AE_ALIGN_CENTER, rgba, right ? "\xE2\x96\xB6" : "\xE2\x97\x80");
}

float ae_widget_row(struct ae_density const *density, float x, float y, float width, struct ae_row const *row,
	short hit_id, short index)
{
	struct ae_row_layout layout;
	unsigned int flags = row->flags;
	int focused = (flags & AE_ROW_FOCUSED) != 0, disabled = (flags & AE_ROW_DISABLED) != 0;
	int by_list = (flags & AE_ROW_BAR_BY_LIST) != 0;
	int on_bar = (flags & AE_ROW_ON_BAR) || (focused && !by_list);
	int arrows;
	float pad = units(density, density->metrics->pad), text = ae_size_text(density), center_y, allowance;
	float corner = units(density, density->metrics->corner);
	unsigned int value_color = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT;
	const char *shown;
	int badges;

	ae_row_layout(density, width, row, &layout);
	arrows = focused && row->value && !disabled && !(flags & (AE_ROW_SUBSCREEN | AE_ROW_STATUS | AE_ROW_LINK | AE_ROW_KEY));
	shown = (disabled ? row->reason : row->value) ? layout.value : NULL;
	center_y = y + layout.height * 0.5f;
	allowance = CLIP_ALLOWANCE_PIXELS * density->pixel;

	/* the background: the selection bar (unless the list draws the moving one), else the row's own */
	if (by_list && (flags & AE_ROW_UNDER_BAR))
		;
	else if (focused && !by_list)
	{
		ae_draw_rect(x, y, width, layout.height, corner, disabled ? AE_COLOR_DISABLED_BAR : AE_COLOR_SELECTION);
		ae_draw_rect(x, y, units(density, NOTCH_U), layout.height, 0.0f, disabled ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
	}
	else
	{
		/* (a disabled row's background at 43 %: its reason stays readable in full warning colour) */
		ae_draw_rect(x, y, width, layout.height, corner,
			disabled ? scaled_alpha(AE_COLOR_ROW, AE_DISABLED_ROW_ALPHA) : AE_COLOR_ROW);
		if (flags & AE_ROW_HOVER)
			ae_draw_rect(x, y, width, layout.height, corner, AE_COLOR_HOVER);
		/* (0.3.9, mockups-039/start-game-button A: the primary action framed in menu blue, never filled: white is the
		focus's alone) */
		if (flags & AE_ROW_PRIMARY)
			ae_draw_outline(x, y, width, layout.height, corner, units(density, PRIMARY_FRAME_U),
				disabled ? scaled_alpha(AE_COLOR_WARNING, AE_DISABLED_ROW_ALPHA) : AE_COLOR_WARNING);
	}

	/* the arrow cells: lit under the mouse, and on the pressed side for a value tick's time (spec 7) */
	if (arrows)
	{
		int ticking = row->tick && row->tick_side && ae_motion_running(row->tick);

		if ((flags & AE_ROW_HOVER_LEFT) || (ticking && row->tick_side < 0))
			ae_draw_rect(x + layout.arrow_left_x, y, layout.arrow_cell, layout.height, 0.0f, AE_COLOR_ARROW_HOVER);
		if ((flags & AE_ROW_HOVER_RIGHT) || (ticking && row->tick_side > 0))
			ae_draw_rect(x + layout.arrow_right_x, y, layout.arrow_cell, layout.height, 0.0f, AE_COLOR_ARROW_HOVER);
	}

	/* the texts, clipped to the row's padding (a value wider than the row is cut there, never drawn over the next
	widget) */
	ae_draw_clip_push(x + pad - allowance, y, width - 2.0f * pad + 2.0f * allowance, layout.height);
	/* (the primary action: its label centred: a button, not a label / value row; left of a disabled row's reason when
	the middle would reach it. (0.4.3, owner 2026-10-28) no START mark: START no longer presses it, only A / a click) */
	if ((flags & AE_ROW_PRIMARY) && layout.label[0])
	{
		float group = layout.label_width, left = x + (width - group) * 0.5f;

		if (shown && left + group > x + layout.value_x - units(density, LABEL_GAP_U))
			left = x + layout.label_x;
		ae_draw_text(AE_FONT_ROW, text, left, text_top(AE_FONT_ROW, text, center_y), AE_ALIGN_LEFT,
			disabled ? AE_COLOR_DISABLED_TEXT : on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE, layout.label);
	}
	else if (layout.label[0])
	{
		ae_draw_text(AE_FONT_ROW, text, x + layout.label_x, text_top(AE_FONT_ROW, text, center_y), AE_ALIGN_LEFT,
			disabled ? AE_COLOR_DISABLED_TEXT : on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT, layout.label);
	}
	if (layout.dot)
	{
		float diameter = units(density, DOT_U);

		ae_draw_rect(x + layout.dot_x, center_y - diameter * 0.5f, diameter, diameter, diameter * 0.5f,
			on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	}
	if (layout.badge)
	{
		float height = badge_height(density);
		float size = badge_size(density);

		if (on_bar)
			ae_draw_rect(x + layout.badge_x, center_y - height * 0.5f, layout.badge_width, height, 0.0f,
				AE_COLOR_SELECTION_TEXT);
		else
			ae_draw_outline(x + layout.badge_x, center_y - height * 0.5f, layout.badge_width, height, 0.0f,
				units(density, 1.0f), AE_COLOR_ACCENT);
		ae_draw_text(AE_FONT_ROW, size, x + layout.badge_x + layout.badge_width * 0.5f, text_top(AE_FONT_ROW, size,
			center_y), AE_ALIGN_CENTER, AE_COLOR_ACCENT, row_source_badge(row));
	}
	for (badges = 0; badges < layout.badge_count; badges++)
	{
		float height = badge_height(density);
		float size = badge_size(density);
		unsigned int color = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED;

		ae_draw_outline(x + layout.badges_x[badges], center_y - height * 0.5f, layout.badges_width[badges], height, 0.0f,
			units(density, 1.0f), color);
		ae_draw_text(AE_FONT_ROW, size, x + layout.badges_x[badges] + layout.badges_width[badges] * 0.5f,
			text_top(AE_FONT_ROW, size, center_y), AE_ALIGN_CENTER, color, row_badge(row, badges));
	}
	if (layout.track_width > 0.0f)
	{
		float height = units(density, SLIDER_TRACK_HEIGHT_U), fill = row->slider;

		if (!(fill >= 0.0f))
			fill = 0.0f;
		if (fill > 1.0f)
			fill = 1.0f;
		ae_draw_rect(x + layout.track_x, center_y - height * 0.5f, layout.track_width, height, height * 0.5f,
			on_bar ? SLIDER_TRACK_ON_BAR : SLIDER_TRACK);
		if (fill > 0.0f)
			ae_draw_rect(x + layout.track_x, center_y - height * 0.5f, layout.track_width * fill, height, height * 0.5f,
				on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	}
	if (flags & (AE_ROW_SUBSCREEN | AE_ROW_LINK))
	{
		float size = units(density, CHEVRON_U), thickness = units(density, 2.0f);
		unsigned int color = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT;

		ae_draw_line(x + layout.chevron_x, center_y - size, x + layout.chevron_x + size, center_y, thickness, color);
		ae_draw_line(x + layout.chevron_x + size, center_y, x + layout.chevron_x, center_y + size, thickness, color);
	}
	if (layout.base[0])
	{
		float minor = minor_size(density), strike = units(density, STRIKE_U);

		ae_draw_text(AE_FONT_BODY, minor, x + layout.base_x, text_top(AE_FONT_BODY, minor, center_y), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, layout.base);
		ae_draw_rect(x + layout.base_x, center_y - strike * 0.5f, layout.base_width, strike, 0.0f, AE_COLOR_MUTED);
	}
	if (shown)
	{
		float right = x + layout.value_x + layout.value_width;
		unsigned int color = disabled && !on_bar ? AE_COLOR_WARNING : value_color;   /* (on the bar: its own text colour, readable) */

		/* (a status's value and a sub-screen's summary muted, unless on the bar) */
		if (!disabled && !on_bar && (flags & (AE_ROW_SUBSCREEN | AE_ROW_STATUS)))
			color = AE_COLOR_MUTED;
		if ((flags & AE_ROW_SUBSCREEN) && layout.value_overflow > 0.0f)
		{
			/* (the marquee: the whole summary, clipped to its room, scrolled) */
			ae_draw_clip_push(x + layout.value_x, y, layout.value_width, layout.height);
			ae_draw_text(AE_FONT_BODY, minor_size(density), x + layout.value_x - layout.value_offset,
				text_top(AE_FONT_BODY, minor_size(density), center_y), AE_ALIGN_LEFT, color, shown);
			ae_draw_clip_pop();
			shown = NULL;
		}
		else if (flags & AE_ROW_SUBSCREEN)
		{
			ae_draw_text(AE_FONT_BODY, minor_size(density), right, text_top(AE_FONT_BODY, minor_size(density), center_y),
				AE_ALIGN_RIGHT, color, shown);
			shown = NULL;
		}
		/* (a key: its cap, light on the row, navy on the selection bar, the text the other of the two) */
		else if ((flags & AE_ROW_KEY) && !disabled)
		{
			float cap = units(density, KEY_CAP_HEIGHT_U), size = units(density, KEY_CAP_TEXT_U);

			ae_draw_rect(x + layout.value_x, center_y - cap * 0.5f, layout.value_width, cap, units(density, 3.0f),
				on_bar ? AE_COLOR_KEY_CAP_TEXT : AE_COLOR_KEY_CAP);
			ae_draw_text(AE_FONT_ROW, size, x + layout.value_x + layout.value_width * 0.5f,
				text_top(AE_FONT_ROW, size, center_y), AE_ALIGN_CENTER, on_bar ? AE_COLOR_TITLE : AE_COLOR_KEY_CAP_TEXT,
				shown);
			shown = NULL;
		}
	}
	if (shown)
	{
		float right = x + layout.value_x + layout.value_width;
		unsigned int color = disabled ? AE_COLOR_WARNING : value_color;

		if (!disabled && !on_bar && (flags & AE_ROW_STATUS))
			color = AE_COLOR_MUTED;

		/* (a value tick: the old value out, the new in from the pressed side) */
		if (!disabled && row->tick && row->tick_side && row->old_value && ae_motion_running(row->tick))
		{
			float old_x, old_alpha, new_x, new_alpha;

			ae_motion_value_tick(ae_motion_progress(row->tick), row->tick_side, &old_x, &old_alpha, &new_x, &new_alpha);
			ae_draw_text(AE_FONT_ROW, text, right + units(density, old_x), text_top(AE_FONT_ROW, text, center_y),
				AE_ALIGN_RIGHT, scaled_alpha(color, old_alpha), row->old_value);
			ae_draw_text(AE_FONT_ROW, text, right + units(density, new_x), text_top(AE_FONT_ROW, text, center_y),
				AE_ALIGN_RIGHT, scaled_alpha(color, new_alpha), shown);
		}
		else if (disabled)
			ae_draw_text(AE_FONT_ROW, minor_size(density), right, text_top(AE_FONT_ROW, minor_size(density), center_y),
				AE_ALIGN_RIGHT, color, shown);
		else
			ae_draw_text(AE_FONT_ROW, text, right, text_top(AE_FONT_ROW, text, center_y), AE_ALIGN_RIGHT, color, shown);
	}
	if (arrows)
	{
		arrow(density, x + layout.arrow_left_x, layout.arrow_cell, center_y, 0, value_color);
		arrow(density, x + layout.arrow_right_x, layout.arrow_cell, center_y, 1, value_color);
	}
	ae_draw_clip_pop();

	/* hits: the row, its value (cut to the row: a value wider than it), its arrows (the last added wins) */
	ae_hit_add(x, y, width, layout.height, hit_id, AE_PART_ROW, index);
	if (shown && !disabled)
	{
		float value_left = layout.value_x > 0.0f ? layout.value_x : 0.0f;
		float value_right = layout.value_x + layout.value_width < width ? layout.value_x + layout.value_width : width;

		ae_hit_add(x + value_left, y, value_right - value_left, layout.height, hit_id, AE_PART_VALUE, index);
	}
	/* (a slider's track: a click or drag there sets the value at that point; the hit's rect is the track) */
	if (layout.track_width > 0.0f)
		ae_hit_add(x + layout.track_x, y, layout.track_width, layout.height, hit_id, AE_PART_TRACK, index);
	if (arrows)
	{
		ae_hit_add(x + layout.arrow_left_x, y, layout.arrow_cell, layout.height, hit_id, AE_PART_ARROW_LEFT, index);
		ae_hit_add(x + layout.arrow_right_x, y, layout.arrow_cell, layout.height, hit_id, AE_PART_ARROW_RIGHT, index);
	}
	return layout.height;
}

float ae_widget_group(struct ae_density const *density, float x, float y, float width, const char *label)
{
	float size = floored_minor(density, density->metrics->group);
	float height = units(density, density->metrics->group_height);
	float inset = units(density, GROUP_INSET_U), allowance = CLIP_ALLOWANCE_PIXELS * density->pixel;

	if (label && *label)
	{
		/* (on the lower part of its height: the space above separates it from the rows before) */
		ae_draw_clip_push(x + inset - allowance, y, width - 2.0f * inset + 2.0f * allowance, height);
		ae_draw_text_tracked(AE_FONT_BODY, size, GROUP_TRACKING, x + inset,
			y + height - inset - ae_draw_cap_height(AE_FONT_BODY, size), AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
		ae_draw_clip_pop();
	}
	return height;
}

float ae_marquee_offset(float overflow, float speed, unsigned long ms)
{
	unsigned long travel, at;

	if (!(overflow > 0.0f) || !(speed > 0.0f))
		return 0.0f;
	travel = (unsigned long)(overflow / speed * 1000.0f);
	if (travel < 1)
		travel = 1;
	at = ms % (2ul * (MARQUEE_PAUSE_MS + travel));
	if (at < MARQUEE_PAUSE_MS)
		return 0.0f;
	at -= MARQUEE_PAUSE_MS;
	if (at < travel)
		return overflow * (float)at / (float)travel;
	at -= travel;
	if (at < MARQUEE_PAUSE_MS)
		return overflow;
	at -= MARQUEE_PAUSE_MS;
	return overflow * (1.0f - (float)at / (float)travel);
}

/* ---------- list and scrollbar */

static float more_size(struct ae_density const *density)
{
	return ae_size(density, density->metrics->sub_line, density->kind == AE_DENSITY_VIEW ? MINOR_FLOOR_PIXELS : 0.0f);
}

static float more_line(struct ae_density const *density)
{
	return more_size(density) * MORE_LINE;
}

float ae_list_view_rows_top(struct ae_density const *density, struct ae_list_view const *view, float y)
{
	return y + more_line(density) * ae_motion_value(&view->top);
}

void ae_list_view_init(struct ae_list_view *view, short count, short rows)
{
	memset(view, 0, sizeof(*view));
	ae_list_init(&view->list, count, rows);
	view->previous_focus = view->list.focus;
	view->hover = -1;
	view->scroll.from = view->scroll.to = (float)view->list.first;
	view->bar.from = view->bar.to = (float)view->list.focus;
}

short ae_list_view_rows(struct ae_density const *density, float height)
{
	float pitch = ae_size_row(density) + units(density, density->metrics->gap);
	/* (one "N more" line under the rows; the "▲ N more" line over them only while items are above: ae_widget_list
	shows a row less then) */
	float usable = height - more_line(density) + units(density, density->metrics->gap);
	int rows = pitch > 0.0f ? (int)(usable / pitch) : 1;

	return (short)(rows < 1 ? 1 : rows);
}

void ae_scrollbar_thumb(short count, short rows, float first, float track_height, float minimum,
	float *thumb_y, float *thumb_height)
{
	float last_first;

	*thumb_y = 0.0f;
	*thumb_height = 0.0f;
	if (count <= rows || rows <= 0 || track_height <= 0.0f)
		return;
	*thumb_height = track_height * (float)rows / (float)count;
	if (*thumb_height < minimum)
		*thumb_height = minimum < track_height ? minimum : track_height;
	last_first = (float)(count - rows);
	if (first < 0.0f)
		first = 0.0f;
	if (first > last_first)
		first = last_first;
	*thumb_y = (track_height - *thumb_height) * first / last_first;
}

short ae_scrollbar_first(short count, short rows, float track_height, float thumb_height, float thumb_y)
{
	float room = track_height - thumb_height, first;

	if (count <= rows || room <= 0.0f)
		return 0;
	first = thumb_y / room * (float)(count - rows);
	if (first < 0.0f)
		first = 0.0f;
	if (first > (float)(count - rows))
		first = (float)(count - rows);
	return (short)(first + 0.5f);
}

/* "▲ N more" / "▼ N more", centred on center_x in a line from top */
static void more(struct ae_density const *density, float center_x, float top, int up, int count)
{
	char text[64], number[48];
	float size = more_size(density);

	snprintf(number, sizeof(number), ae_string(AE_STR_MORE), count);
	snprintf(text, sizeof(text), "%s %s", up ? "\xE2\x96\xB2" : "\xE2\x96\xBC", number);
	ae_draw_text(AE_FONT_BODY, size, center_x, text_top(AE_FONT_BODY, size, top + more_line(density) * 0.5f),
		AE_ALIGN_CENTER, AE_COLOR_MUTED, text);
}

float ae_widget_more_line(struct ae_density const *density)
{
	return more_line(density);
}

void ae_widget_more(struct ae_density const *density, float center_x, float top, int up, int count)
{
	more(density, center_x, top, up, count);
}

void ae_widget_list(struct ae_density const *density, struct ae_list_view *view, float x, float y, float width,
	float height, ae_list_item_draw draw_item, void *context, short hit_id)
{
	struct ae_view current;
	struct ae_list *list = &view->list;
	float row = ae_size_row(density), gap = units(density, density->metrics->gap), pitch = row + gap;
	float strip = units(density, TRACK_STRIP_U), allowance = CLIP_ALLOWANCE_PIXELS * density->pixel;
	float rows_top = y, rows_width = width - strip, rows_height, first, bar;
	short rows = ae_list_view_rows(density, height), item, last;
	int bar_moving;

	/* (the "▲ N more" line only while items are above: then it takes the top, and a row less shows; the rows move
	down to make its room with the scroll, never at once) */
	{
		float top = list->first > 0 && rows > 1 ? 1.0f : 0.0f;

		if (top > 0.0f)
			rows--;
		if (view->top.to != top)
			ae_motion_start(&view->top, view->top.to, top, AE_MOTION_SCROLL_MS);
		if (view->dragging)
			ae_motion_finish(&view->top);
		rows_top += more_line(density) * ae_motion_value(&view->top);
	}
	if (list->rows != rows)
	{
		/* (the visible rows changed: the window follows the focus again) */
		list->rows = rows;
		if (list->count > 0)
			ae_list_set_focus(list, list->focus);
	}
	rows_height = (float)rows * pitch - gap;
	/* the motions: the window's first row and the bar's row, 100 ms each, from wherever they are */
	if (view->scroll.to != (float)list->first)
		ae_motion_start(&view->scroll, view->scroll.to, (float)list->first, AE_MOTION_SCROLL_MS);
	if (list->focus != view->previous_focus)
	{
		ae_motion_start(&view->bar, view->previous_focus >= 0 ? (float)view->previous_focus : (float)list->focus,
			(float)list->focus, AE_MOTION_SELECTION_MS);
		view->previous_focus = list->focus;
	}
	/* (a thumb being dragged follows the pointer at once) */
	if (view->dragging)
		ae_motion_finish(&view->scroll);
	first = ae_motion_value(&view->scroll);
	bar = ae_motion_value(&view->bar);
	bar_moving = list->focus >= 0 && ae_motion_running(&view->bar);

	/* the rows */
	ae_draw_clip_push(x - allowance, rows_top, rows_width + 2.0f * allowance, rows_height);
	if (bar_moving)
	{
		float bar_y = rows_top + (bar - first) * pitch;

		ae_draw_rect(x, bar_y, rows_width, row, units(density, density->metrics->corner), AE_COLOR_SELECTION);
		ae_draw_rect(x, bar_y, units(density, NOTCH_U), row, 0.0f, AE_COLOR_ACCENT);
	}
	item = (short)(first < 0.0f ? 0 : (int)first);
	last = (short)((int)(first + (float)rows) + 1);
	if (last > list->count)
		last = list->count;
	/* (the rows' hits, the list's and the rows' own, cut to the rows: none over the "N more" lines) */
	{
		struct ae_view rows_view;

		ae_draw_current_view(&rows_view);
		ae_view_to_layout(&rows_view, x, rows_top, &hit_clip[0], &hit_clip[1]);
		ae_view_to_layout(&rows_view, x + rows_width, rows_top + rows_height, &hit_clip[2], &hit_clip[3]);
		hit_clip_on = 1;
	}
	for (; item < last; item++)
	{
		float row_y = rows_top + ((float)item - first) * pitch, distance = (float)item - bar;
		unsigned int flags = 0;

		/* (a row wholly outside the rows: not drawn, no hit) */
		if (row_y >= rows_top + rows_height || row_y + row <= rows_top)
			continue;

		if (distance < 0.0f)
			distance = -distance;
		if (item == list->focus)
			flags |= AE_ROW_FOCUSED;
		if (bar_moving)
		{
			/* (the text turns dark once the moving bar is more than half over the row: spec 7) */
			flags |= AE_ROW_BAR_BY_LIST;
			if (distance < 1.0f)
				flags |= AE_ROW_UNDER_BAR;
			if (distance < 0.5f)
				flags |= AE_ROW_ON_BAR;
		}
		else if (item == list->focus)
			flags |= AE_ROW_ON_BAR;
		if (view->mouse && item == view->hover && item != list->focus)
			flags |= AE_ROW_HOVER;
		ae_hit_add(x, row_y, rows_width, row, hit_id, AE_PART_ITEM, item);
		if (draw_item)
			draw_item(context, item, x, row_y, rows_width, row, flags);
	}
	hit_clip_on = 0;
	ae_draw_clip_pop();

	/* overflow: a one-row fade at a cut edge, and how many more */
	if (list->first > 0)
	{
		ae_draw_gradient(x, rows_top, rows_width, row, 0.0f, AE_COLOR_FADE, AE_COLOR_FADE & 0xFFFFFF00u);
		more(density, x + rows_width * 0.5f, y, 1, list->first);
	}
	if (list->first + rows < list->count)
	{
		ae_draw_gradient(x, rows_top + rows_height - row, rows_width, row, 0.0f, AE_COLOR_FADE & 0xFFFFFF00u,
			AE_COLOR_FADE);
		more(density, x + rows_width * 0.5f, rows_top + rows_height, 0, list->count - (list->first + rows));
	}

	/* the scrollbar, in the list's own coordinates (its track from rows_top): the same in every view */
	/* (the list's rectangle, for the wheel: anywhere over it, gaps, fades and "N more" lines included) */
	ae_draw_current_view(&current);
	view->area.x = current.x + x * current.scale;
	view->area.y = current.y + y * current.scale;
	view->area.width = width * current.scale;
	view->area.height = height * current.scale;
	view->layer = hit_layer;
	view->track_height = view->thumb_height = 0.0f;
	if (list->count > rows)
	{
		float track_width = units(density, view->mouse ? TRACK_MOUSE_U : TRACK_PAD_U);
		float track_x = x + width - strip * 0.5f - track_width * 0.5f, thumb_y, thumb_height;
		unsigned int thumb_color = view->dragging ? AE_COLOR_RING : view->mouse ? AE_COLOR_THUMB_MOUSE : AE_COLOR_MUTED;

		ae_scrollbar_thumb(list->count, rows, first, rows_height, units(density, THUMB_MINIMUM_U), &thumb_y,
			&thumb_height);
		ae_draw_rect(track_x, rows_top, track_width, rows_height, track_width * 0.5f, AE_COLOR_TRACK);
		ae_draw_rect(track_x, rows_top + thumb_y, track_width, thumb_height, track_width * 0.5f, thumb_color);
		ae_hit_add(x + width - strip, rows_top, strip, rows_height, hit_id, AE_PART_TRACK, -1);
		ae_hit_add(x + width - strip, rows_top + thumb_y, strip, thumb_height, hit_id, AE_PART_THUMB, -1);
		/* (for the pointer: in layout units) */
		ae_draw_current_view(&current);
		view->track_top = current.y + rows_top * current.scale;
		view->track_height = rows_height * current.scale;
		view->thumb_height = thumb_height * current.scale;
		view->drag_grab = view->dragging ? view->drag_grab : 0.0f;
	}
}

int ae_list_view_event(struct ae_list_view *view, struct ae_event const *event)
{
	struct ae_list *list;
	short before, page;

	if (!view || !event)
		return 0;
	list = &view->list;
	page = (short)(list->rows > 1 ? list->rows - 1 : 1);
	before = list->focus;
	switch (event->action)
	{
	case AE_ACTION_UP: ae_list_move(list, -1, 0); break;
	case AE_ACTION_DOWN: ae_list_move(list, 1, 0); break;
	case AE_ACTION_PAGE_UP: ae_list_move(list, (short)-page, 0); break;
	case AE_ACTION_PAGE_DOWN: ae_list_move(list, page, 0); break;
	default: return 0;
	}
	/* (pad or key input: the scrollbar's pad look again, no hover) */
	view->mouse = 0;
	view->hover = -1;
	if (list->focus != before)
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
	return 1;
}

short ae_list_view_pointer(struct ae_list_view *view, struct ae_pointer const *pointer, short hit_id)
{
	struct ae_list *list;
	struct ae_hit hit;
	int over;

	if (!view || !pointer)
		return -1;
	list = &view->list;
	over = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == hit_id;
	if (!pointer->touch && (pointer->moved || pointer->left_clicks || pointer->wheel_steps))
		view->mouse = 1;
	/* a thumb being dragged: the window follows it until the button is let go of */
	if (view->dragging)
	{
		if (!pointer->left_held)
			view->dragging = 0;
		else if (pointer->moved)
		{
			short first = ae_scrollbar_first(list->count, list->rows, view->track_height, view->thumb_height,
				pointer->y - view->drag_grab - view->track_top);

			ae_list_scroll(list, (short)(first - list->first));
		}
		return -1;
	}
	/* the wheel anywhere over the list: 3 rows a notch (away from the user: up); the focus moves only if it leaves the
	window */
	if (pointer->wheel_steps && pointer->x >= view->area.x && pointer->y >= view->area.y &&
		pointer->x < view->area.x + view->area.width && pointer->y < view->area.y + view->area.height &&
		!ae_hit_at(pointer->x, pointer->y, (short)(view->layer + 1), NULL))
		ae_list_scroll(list, (short)(-WHEEL_ROWS * pointer->wheel_steps));
	/* hover focuses only when the pointer moves (scrolling never steals the focus), and never scrolls */
	if (pointer->moved)
	{
		view->hover = over && hit.index >= 0 && hit.part != AE_PART_TRACK && hit.part != AE_PART_THUMB ? hit.index : -1;
		if (view->hover >= 0 && view->hover < list->count)
			list->focus = view->hover;
	}
	if (pointer->left_clicks && over)
	{
		if (hit.part == AE_PART_THUMB)
		{
			view->dragging = 1;
			view->drag_grab = pointer->y - hit.rect.y;
			return -1;
		}
		if (hit.part == AE_PART_TRACK)
		{
			float thumb_top = view->track_top;
			float room = view->track_height - view->thumb_height;
			short page = (short)(list->rows > 1 ? list->rows - 1 : 1);

			if (list->count > list->rows)
				thumb_top += room * (float)list->first / (float)(list->count - list->rows);
			ae_list_scroll(list, pointer->y < thumb_top ? (short)-page : page);
			return -1;
		}
		if (hit.index >= 0 && hit.index < list->count)
		{
			list->focus = hit.index;
			ae_sound_request(AE_SOUND_FORWARD, 0);
			return hit.index;
		}
	}
	return -1;
}
