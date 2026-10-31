/*
AE_PANEL_DRAW.H

HUD STYLE AE's panels' primitives (spec 14.15 parts 1, 2, 3, 8; plan Task
13), drawn through the HUD's own screen-quad pass, in the view's screen pixels
(x from the view's left, y from its top), so HUD scale, HUD AREA, alpha and
split viewports are the HUD's:

- the OUTLINE: a 1.5 px HUD-blue line at 0.85 round a leaning box with four
  equal 3 px corner radii, keylined both sides (1 px DARK at 0.90); boxes add
  a fill (HUD blue 0.08 at the top, x 0.73 at the bottom);
- bars: the same outline, the fill inside it, 1 px DARK dividers (0.67)
  between 5 cells, the overshield's layers;
- CE's art (the map's own HUD bitmaps, through the port's high-resolution
  copies where the map's pixels are the stock ones): one round of
  hud_ammo_alphas 1 / 5 / 7 / 4 / 6 / 2 as a slanted strip of its sheet (so a
  neighbour never shows), the round of hud_ammo_type_icons 0 and the frag of
  hud_ammo_type_icons 2; the shield icon (hud_unit_backgrounds 0) and the
  battery (hud_ammo_type_icons 4) as their SOLID silhouettes, polygons
  measured on port/assets/hud;
- the plasma disc, the "x", the health cells and the cross, drawn as shapes.

Every primitive has the keyline: the same shape in DARK (6,10,18) at 0.90, 1
px larger, drawn first (G2: a dimmed glyph keeps its keyline at full).
*/

#ifndef __AE_PANEL_DRAW_H
#define __AE_PANEL_DRAW_H

#include "ae_pure_types.h"
#include "ae_panel_layout.h"

enum ae_panel_tone      /* the tokens (14.15 part 1) */
{
	_ae_tone_white,     /* (229,240,255) */
	_ae_tone_blue,      /* HUD BLUE (178,224,255) */
	_ae_tone_grey,      /* (188,198,212) */
	_ae_tone_dark,      /* (6,10,18) */
	_ae_tone_red,       /* (255,77,77) */
	_ae_tone_green,     /* OVERSHIELD (120,255,160) */
	_ae_tone_green_edge,/* (214,255,226) */
	_ae_tone_overshield1, /* CE's classic HUD's first overshield layer, pink (198,86,123) */
	_ae_tone_overshield2, /* its second, grass green (111,208,78) */
	NUMBER_OF_AE_TONES
};

/* a tone's colour (0..1) */
void ae_panel_tone_rgb(short tone, real rgb[3]);
/* every draw below at fade x its alpha, keylines too, until set back to 1 (a scripted blink: ae_panels.c,
ae_sensor.c) */
void ae_panel_draw_set_fade(real fade);

/* a box in the outline rule; fill_alpha 0: no fill */
void ae_panel_draw_box(const struct ae_panel_para *box, real scale, real fill_alpha, short line_tone);
/* a bar: the dark track, the fill from the left (fill 0..1) in fill_tone, overshield level 1 (the classic HUD's pink)
and level 2 (its green, over the first), each with a 1 px light top edge, the 5 cells' dividers, the outline */
void ae_panel_draw_bar(const struct ae_panel_para *bar, real scale, real fill, short fill_tone, short line_tone,
	real overshield1, real overshield2);
/* the same with the fill and the line at fade x their alpha, keylines at full (the shield's off beat while it blinks) */
void ae_panel_draw_bar_faded(const struct ae_panel_para *bar, real scale, real fill, short fill_tone, short line_tone,
	real overshield1, real overshield2, real fade, real trail, real trail_alpha);
/* one round of CE's bullet art (enum ae_panel_art) with its ink box's top-left at (x, y), height tall: lit in tone
at 0.94; spent: 0.28 with a weaker keyline (0.39: the keyline follows the fill's alpha + 0.15, as rev13) */
void ae_panel_draw_art(short art, real x, real y, real height, boolean lit, short tone);
/* a glyph (enum ae_panel_glyph: round, battery, frag, shield) with its box's top-left at (x, y), height tall, in tone
at alpha; the keyline at 0.90 whatever the alpha (G2) */
void ae_panel_draw_glyph(short glyph, real x, real y, real height, short tone, real alpha);
/* a glyph's width at a height (its art's aspect) */
real ae_panel_glyph_width(short glyph, real height);
/* the plasma grenade: a hard disc with a 1 px DARK ring at 46 % of its radius, keylined */
void ae_panel_draw_plasma(real cx, real cy, real diameter, short tone, real alpha);
/* the "x": two 2.2 px strokes in a leaning square of size, its bottom-left on the baseline */
void ae_panel_draw_x(real x, real baseline_y, real size, real scale, short tone);
/* a health cell: lit, the tone at the fill's alpha; lost, the dark track with a 1 px rim in blue (red when tone is) */
void ae_panel_draw_cell(const struct ae_panel_para *cell, boolean lit, short tone);
/* the health plus sign, whole pixels, arms a third */
void ae_panel_draw_cross(real x, real y, real size, short tone);
/* a straight line with the keyline */
void ae_panel_draw_line(real x0, real y0, real x1, real y1, real width, short tone, real alpha, boolean keyline);
/* a filled circle / ring, keyline optional */
void ae_panel_draw_disc(real cx, real cy, real radius, short tone, real alpha, boolean keyline);
void ae_panel_draw_ring(real cx, real cy, real radius, real width, short tone, real alpha, boolean keyline);
/* a filled circle in any colour (the sensor's blips), its edge 1 px soft */
void ae_panel_fill_disc(real cx, real cy, real radius, const union real_argb_color *color);
/* a filled polygon with 1 px soft edges, its outline moved grow px out (the keyline: grow 1); it is filled as a fan
from its corners' centroid, so every corner must be seen from there */
void ae_panel_fill_polygon_soft(const real (*points)[2], short count, real grow, const union real_argb_color *color);
/* a filled convex polygon (screen px, clockwise or not) */
void ae_panel_draw_polygon(const real (*points)[2], short count, short tone, real alpha);

/* whether the HUD's art is found in this map (hud_ammo_alphas, hud_ammo_type_icons, hud_unit_backgrounds) */
boolean ae_panel_art_found(short *alphas, short *icons, short *backgrounds);

/* a map's tags loaded or went: the sheets' bitmap groups are looked up again (ae_buttons_tags.c calls it) */
void ae_panel_draw_tags_changed(void);

/* debug.ae_panel_primitives (HALO_DEBUG_AE_PANEL_PRIMITIVES=1): one of each primitive on a flat grey plate in the
first view, for tools/ae_test/panel_checks.py primitives; one log line with the places */
void ae_panel_draw_debug(short local_player_index);

#endif /* __AE_PANEL_DRAW_H */
