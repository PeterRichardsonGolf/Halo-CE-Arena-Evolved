/*
AE_HUD_DIGIT_CELLS.H

CE's counter digits (the HUD's hud_counter_numbers sheet, the interface tag
_interface_hud_digits) laid out at any size: the pure part of ae_hud_digits.c,
unit-tested in port/linux/tests/ae_hud_digit_cells_test.c.

A glyph's sprite cell is drawn whole; within it the digits' ink spans the rows
1/12 .. 11/12 (the cap) and, across, the measured fractions below (from the
sheet's high-resolution copy, port/assets/hud/hud_counter_numbers__0.png). Each
character of a text gets a cell of its own on the line (an ADVANCE): a digit
0.80 of the cap height, a colon 0.50 (spec 14.15 part 7), a space 0.50, or the
caller's fixed cell width (the panels' 21 px at 24 px cap, 0.875); the glyph's
ink is centred in its cell (or put at its left). Characters the sheet has no
glyph for take a digit's advance and draw nothing.
*/

#ifndef __AE_HUD_DIGIT_CELLS_H
#define __AE_HUD_DIGIT_CELLS_H

#include <wchar.h>

#include "ae_pure_types.h"

#define AE_DIGITS_CAP_TOP (1.0f / 12.0f)       /* the cap's rows in a glyph's cell */
#define AE_DIGITS_CAP_BOTTOM (11.0f / 12.0f)
#define AE_DIGITS_ADVANCE 0.80f                /* x cap height */
#define AE_DIGITS_COLON_ADVANCE 0.50f
#define AE_DIGITS_SPACE_ADVANCE 0.50f
#define AE_DIGITS_EMBOLDEN 0.03f               /* the second pass's offset, x cap height (14.15 part 1) */

/* the sheet's sprite (frame) of a character: 0-9, 10 '.', 11 ':', 12 '-'; NONE (-1) for none */
short ae_digit_frame(wchar_t character);
/* a glyph's ink across its cell (0..1); FALSE for a character with no glyph */
boolean ae_digit_ink(wchar_t character, real *left, real *right);
/* a character's advance: cell_px when > 0 (every character the same), else the defaults x cap */
real ae_digit_advance(wchar_t character, real cap_px, real cell_px);
/* the text's width: the advances added up */
real ae_digits_measure_cells(const wchar_t *text, real cap_px, real cell_px);

/* the clock's colon (spec 14.15 part 7, owner: "even gaps like 4:12"): with the default advances only the
colon moves inside its cell, by (CL[left digit] - CR[right digit]) / 2 x the cap height, so the mean ink gap
over its two dots is the same on both sides; 0 beside anything but digits */
real ae_digits_colon_shift(wchar_t left, wchar_t right);

/* one glyph's quad on screen: the whole sprite cell, `aspect` its width over its height (from the sprite) */
struct ae_digit_quad { real x0, y0, x1, y1; boolean visible; };
/* the quad of a text's character `index`, drawn from x with its baseline at baseline_y; ink_left: the glyph's ink
at its cell's left edge, else centred (a colon between digits moved by ae_digits_colon_shift, with the default
advances) */
void ae_digit_quad(const wchar_t *text, long index, real x, real baseline_y, real cap_px, real cell_px,
	boolean ink_left, real aspect, struct ae_digit_quad *quad);
/* the text's ink from its first drawn glyph's left to its last's right, from x = 0; FALSE when nothing draws */
boolean ae_digits_ink_bounds_cells(const wchar_t *text, real cap_px, real cell_px, boolean ink_left, real *left,
	real *right);

#endif /* __AE_HUD_DIGIT_CELLS_H */
