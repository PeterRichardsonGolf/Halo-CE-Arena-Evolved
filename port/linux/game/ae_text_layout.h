/*
AE_TEXT_LAYOUT.H

The high-res text laid out by its own fonts' widths. Upstream's high-res text (port/linux/src/text_hires.c) keeps the
game's layout, from the font tags' character widths, and draws each Overpass glyph centred in its tag character's
place: a narrow letter (a 1, an l) sits in the middle of a wider space, so left-aligned columns (the carnage report's
places and names) wobble and right-aligned numbers end short. AE lays a string the high-res text draws (every one of
its characters in the font) out by the font's own advances, with its tabular digits (port/linux/src/ae_text_hires.h),
and draws each glyph where its own pen is. draw_string.c measures and draws with these (AE hook lines), so the bounds
the game computes for a string (draw_string_compute_bounds) agree with what is drawn; a string drawn from the tag's
bitmaps (high-res text off, or a character the font has not) keeps the tag's widths. The pen keeps its fraction of a
unit from one character to the next, and a right-aligned or centred line starts where its exact width puts it.
*/

#ifndef AE_TEXT_LAYOUT_H
#define AE_TEXT_LAYOUT_H

#include <wchar.h>

/* a string about to be laid out with a font tag (draw_string, draw_unicode_string): AE's layout if the high-res text
will draw it, the tag's otherwise */
void ae_text_layout_begin(long font_index, char const *string);
void ae_text_layout_begin_unicode(long font_index, wchar_t const *string);

/* the pen after a character (code, and its width in the font tag): slot 0 measures a line, slot 1 draws it */
short ae_text_layout_advance(int slot, unsigned long code, short tag_width, short pen);

/* a line about to be drawn: its justification (enum text_justification: 0 left, 1 right, 2 centre), its bounds' left
and width, the font's leading width, the width measured (slot 0) and the pen it starts at */
void ae_text_layout_line(short justification, short x0, short width, short leading_width, short line_width,
	short cursor_x);

/* where the high-res glyph of the character being drawn goes, from its pen, before its own left edge: the pen's
fraction of a unit with AE's layout, the old centring in the tag character's place without it */
float ae_text_layout_glyph_offset(short tag_width, float glyph_advance);

/* a character's left and right in the bounds draw_string_compute_bounds gives (x0, x1: the font tag's bitmap at the
pen), when it is not cut (x, the columns cut at its left, and dx of bitmap_width shown): its place in AE's layout, from
its pen to the next, when AE lays the string out */
void ae_text_layout_character_bounds(short x, short dx, short bitmap_width, short *x0, short *x1);

/* rasterizer_text.c (an AE hook line): the high-res text's font for a font tag, as rasterizer_draw_string picks it
(-1 if none) */
long ae_text_layout_hires_font(long font_index);

/* the pen's arithmetic (pure: the unit tests'): pen + advance with its fraction kept in *exact (restarted at pen when
*valid is 0 or *last is not pen); *offset gets the fraction the character starts at */
short ae_text_pen_advance(short pen, float advance, float *exact, short *last, int *valid, float *offset);
/* the exact start of a line of exact width measured: right-aligned at x0 + width - leading_width, centred in the
bounds, or left at cursor_x */
float ae_text_line_start(short justification, short x0, short width, short leading_width, float measured,
	short cursor_x);

#endif
