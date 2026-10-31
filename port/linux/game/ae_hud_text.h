/*
AE_HUD_TEXT.H

AE's HUD text in the score strip's style (spec 11.3, 12.2): the HUD's own text
face (the messaging font the port draws in Overpass 900) at a size in screen
pixels, with the strip's dark outline (6,10,18) at 0.88 (2 px at 36 px,
scaling with the square root of the size) and soft shadow (0.55, 1.5 px down,
about 2 px of blur); or a crisp 1 px keyline (the panels' labels, 14.15); and
the soft dark backing behind a group (an elliptical falloff, no edge).

Sizes are the font's em in screen pixels (Overpass's capitals are 0.70 of it);
positions are the view's screen pixels, x from its left, y its baseline from
its top. The HUD draws in units of 480 to the screen's height
(ae_hud_pixels_per_unit), which this file converts to.
*/

#ifndef __AE_HUD_TEXT_H
#define __AE_HUD_TEXT_H

#include <wchar.h>

#include "ae_pure_types.h"

union real_argb_color;

enum ae_hud_text_edge { _ae_text_plain, _ae_text_outline, _ae_text_keyline };

#define AE_HUD_TEXT_CAP 0.70f /* Overpass's capital height, of its em */

/* the text's advance in screen pixels at that size */
real ae_hud_text_measure(const wchar_t *text, real px);
/* outline: the strip's outline + shadow (spec 11.3), else none */
void ae_hud_text_draw(short local_player_index, real x, real baseline_y, const wchar_t *text, real px,
	const union real_argb_color *color, boolean outline);
void ae_hud_text_draw_ex(short local_player_index, real x, real baseline_y, const wchar_t *text, real px,
	const union real_argb_color *color, enum ae_hud_text_edge edge);
/* (rasterizer_xbox_text.c's AE hooks: an outlined text's passes in one batch) */
int ae_hud_text_hold_begin(void const *parameters, int size);
int ae_hud_text_hold_end(void);
/* the soft dark backing: centre, radii (rx across; ry above the centre, 30/46 of it below), peak alpha */
void ae_hud_backing_draw(short local_player_index, real cx, real cy, real rx, real ry, real peak_alpha);

/* (shapes in the HUD pass, screen pixels: a filled quadrilateral of four corners in order, one colour each, and a
line of a width) */
void ae_hud_fill_quad(const real points[4][2], const union real_argb_color *colors);
/* (0.3.9) the flat quads from here to ae_hud_quads_end drawn as one (nothing else may draw between them) */
void ae_hud_quads_begin(void);
void ae_hud_quads_end(void);
void ae_hud_fill_quad_color(const real points[4][2], const union real_argb_color *color);
void ae_hud_line(real x0, real y0, real x1, real y1, real width, const union real_argb_color *color);

#endif /* __AE_HUD_TEXT_H */
