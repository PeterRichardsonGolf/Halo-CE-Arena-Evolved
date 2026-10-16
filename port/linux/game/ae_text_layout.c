/*
AE_TEXT_LAYOUT.C

The high-res text laid out by its own fonts' widths (ae_text_layout.h).
*/

#include "ae_text_layout.h"
#include "../src/text_hires.h"

/* text_hires.c (ae_text_hires.h): a character's advance in a high-res font, in units */
float text_hires_advance(long font, unsigned long code);

#include <math.h>
#include <string.h>

/* (text_hires.c's MAXIMUM_FONTS) */
#define CACHED_FONTS 16
#define CACHED_CODES 256

enum
{
	_slot_measure,
	_slot_draw,
	NUMBER_OF_SLOTS
};

static struct
{
	/* the high-res font the string being laid out is drawn with; -1: the tag's layout */
	long font;
	struct
	{
		float exact;
		short last;
		int valid;
	} pens[NUMBER_OF_SLOTS];
	/* the fraction of a unit the character being drawn starts at, and its exact place, pen to next pen */
	float offset;
	float left, right;
} layout = { -1, { { 0.0f, 0, 0 } }, 0.0f, 0.0f, 0.0f };

/* the fonts' advances of the first 256 characters, as they are asked for */
static float advances[CACHED_FONTS][CACHED_CODES];
static unsigned char advances_known[CACHED_FONTS][CACHED_CODES];

static float character_advance(long font, unsigned long code, short tag_width)
{
	float advance;

	if (font < CACHED_FONTS && code < CACHED_CODES && advances_known[font][code])
		return advances[font][code];
	advance = text_hires_advance(font, code);
	/* (a character the font has not: never in a string the font draws but for control characters; the tag's width,
	not kept, as callers give different ones) */
	if (advance < 0.0f)
		return (float)tag_width;
	if (font < CACHED_FONTS && code < CACHED_CODES)
	{
		advances[font][code] = advance;
		advances_known[font][code] = 1;
	}
	return advance;
}

static void begin(long font_index)
{
	/* (a tag index is a datum, negative as a long: only NONE is none) */
	layout.font = font_index == -1 ? -1 : ae_text_layout_hires_font(font_index);
	if (layout.font < 0)
		layout.font = -1;
	memset(layout.pens, 0, sizeof(layout.pens));
	layout.offset = 0.0f;
}

/* (as rasterizer_draw_string decides it: the font draws the string only if it has every character) */
void ae_text_layout_begin(long font_index, char const *string)
{
	begin(font_index);
	for (; layout.font >= 0 && string && *string; string++)
	{
		if (!text_hires_covers(layout.font, (unsigned char)*string))
			layout.font = -1;
	}
}

void ae_text_layout_begin_unicode(long font_index, wchar_t const *string)
{
	begin(font_index);
	for (; layout.font >= 0 && string && *string; string++)
	{
		if (!text_hires_covers(layout.font, (unsigned short)*string))
			layout.font = -1;
	}
}

short ae_text_pen_advance(short pen, float advance, float *exact, short *last, int *valid, float *offset)
{
	float start = *valid && *last == pen ? *exact : (float)pen;

	*offset = start - (float)pen;
	*exact = start + advance;
	*last = (short)floorf(*exact + 0.5f);
	*valid = 1;
	return *last;
}

short ae_text_layout_advance(int slot, unsigned long code, short tag_width, short pen)
{
	float offset;
	short next;

	if (layout.font < 0 || slot < 0 || slot >= NUMBER_OF_SLOTS)
		return (short)(pen + tag_width);
	next = ae_text_pen_advance(pen, character_advance(layout.font, code, tag_width), &layout.pens[slot].exact,
		&layout.pens[slot].last, &layout.pens[slot].valid, &offset);
	if (slot == _slot_draw)
	{
		layout.offset = offset;
		layout.left = (float)pen + offset;
		layout.right = layout.pens[slot].exact;
	}
	return next;
}

float ae_text_line_start(short justification, short x0, short width, short leading_width, float measured,
	short cursor_x)
{
	switch (justification)
	{
	case 1:
		return (float)(x0 + width - leading_width) - measured;
	case 2:
		return (float)x0 + ((float)width - measured) * 0.5f;
	default:
		return (float)cursor_x;
	}
}

void ae_text_layout_line(short justification, short x0, short width, short leading_width, short line_width,
	short cursor_x)
{
	float measured;

	if (layout.font < 0)
		return;
	/* (the measure's exact width, unless the line was cut back to its last word's end, which keeps whole units) */
	measured = layout.pens[_slot_measure].valid && layout.pens[_slot_measure].last == line_width ?
		layout.pens[_slot_measure].exact : (float)line_width;
	layout.pens[_slot_draw].exact = ae_text_line_start(justification, x0, width, leading_width, measured, cursor_x);
	layout.pens[_slot_draw].last = cursor_x;
	layout.pens[_slot_draw].valid = 1;
	layout.pens[_slot_measure].valid = 0;
}

float ae_text_layout_glyph_offset(short tag_width, float glyph_advance)
{
	if (layout.font < 0)
		return ((float)tag_width - glyph_advance) * 0.5f;
	return layout.offset;
}

void ae_text_layout_character_bounds(short x, short dx, short bitmap_width, short *x0, short *x1)
{
	if (layout.font < 0 || x != 0 || dx != bitmap_width || !x0 || !x1)
		return;
	*x0 = (short)floorf(layout.left + 0.001f);
	*x1 = (short)ceilf(layout.right - 0.001f);
}
