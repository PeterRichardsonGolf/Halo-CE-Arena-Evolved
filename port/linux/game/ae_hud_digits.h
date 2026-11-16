/*
AE_HUD_DIGITS.H

CE's counter digits (the map's HUD digits sheet, _interface_hud_digits, with
the high-resolution HUD's copy where the port has one) drawn at any size and
colour through the HUD's own screen-quad pass (the same blend and texture the
HUD's numbers use), for the score strip (HUD STYLE AE) and the AE panels.
Layout: ae_hud_digit_cells.h.

Positions and sizes are the view's SCREEN pixels (x from the view's left, y
from its top); the HUD draws in units of 480 to the screen's height, which
this file converts to (ae_hud_pixels_per_unit).

Styles (spec 11.10, 11.13, 14.15 part 1):
- GLOW (the strip): a soft dark halo (four passes 1.5 x (cap / 24) px out,
  (6,10,18) at glow_alpha x 0.5), then the digits, emboldened;
- KEYLINE (the panels): a 1 px DARK (6,10,18) keyline at 0.90 round every
  glyph, then the digits (their fill at fill_alpha, the keyline always full);
- OUTLINE (the panels' zero count): the glyph as a 1.5 px line on screen,
  keylined (its inside dark: an approximation, the HUD pass cannot cut);
- PLAIN: the digits alone, emboldened (tests).
Every style emboldens the digits by 3 % of the cap height (a second pass that
far to the right).

debug.digits (HALO_DEBUG_DIGITS="1:42 17 10"): the text in magenta, PLAIN, at
24 px cap, its left at x 40 and its baseline at y 150 of the first view, and
one log line with its advance and ink (tools/ae_test/strip_checks.py digits);
under it, to look at, the same text in GLOW (white), KEYLINE (21 px cells) and
OUTLINE (the HUD's blue), 50 px apart.
*/

#ifndef __AE_HUD_DIGITS_H
#define __AE_HUD_DIGITS_H

#include "ae_hud_digit_cells.h"

union real_argb_color;

enum ae_hud_digits_style { _ae_digits_glow, _ae_digits_keyline, _ae_digits_outline, _ae_digits_plain };

struct ae_hud_digits_options {
	enum ae_hud_digits_style style;
	real cell_px;      /* a fixed cell per character; 0: the default advances */
	boolean ink_left;  /* each glyph's ink at its cell's left, else centred */
	real fill_alpha;   /* x the colour's alpha (the keyline / glow keep theirs) */
	real glow_alpha;   /* GLOW */
};

/* width in px; digits tabular, ':' narrow */
real ae_hud_digits_measure(const wchar_t *text, real cap_px);
/* the strip's digits: GLOW (glow dilated dark pass first, spec 11.13) */
void ae_hud_digits_draw(short local_player_index, real x, real baseline_y, const wchar_t *text, real cap_px,
	const union real_argb_color *color, real glow_alpha);
void ae_hud_digits_draw_ex(short local_player_index, real x, real baseline_y, const wchar_t *text, real cap_px,
	const union real_argb_color *color, const struct ae_hud_digits_options *options);
/* the text's ink from its first glyph's left to its last's right (from x = 0), to centre a block by its ink */
boolean ae_hud_digits_ink_bounds(const wchar_t *text, real cap_px, real cell_px, real *left, real *right);
/* screen pixels per HUD unit (the screen's height / 480) */
real ae_hud_pixels_per_unit(void);
/* (a map unloaded: ae_buttons_tags_unloaded) the stock sheet's placeholder let go */
void ae_hud_digits_tags_unloaded(void);
/* (in the HUD's draw, each view: debug.digits) */
void ae_hud_digits_debug_draw(short local_player_index);

#endif /* __AE_HUD_DIGITS_H */
