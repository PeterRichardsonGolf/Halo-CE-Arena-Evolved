/*
AE_WAYPOINT_INK.H

The item waypoints' places to a fraction of a HUD unit (hud_item_timers.c's labels): the map's waypoint arrow's ink,
and the labels' pictures drawn about a centre that is not a whole unit. The HUD draws in units of 480 lines to the
view's height (2.25 pixels at 1080p) and places a bitmap's middle on a whole unit; a label centred on whole units was
up to a unit off its arrow and its rows up to a unit off each other.
*/

#ifndef __AE_WAYPOINT_INK_H
#define __AE_WAYPOINT_INK_H

#include "cseries.h"

struct bitmap_data;

/* (hud_nav_points.c, an AE hook line) the arrow custom_render_nav_point_placed has just drawn: its sprite, scale and
turn */
void ae_waypoint_arrow_drawn(struct bitmap_data const *bitmap, real_rectangle2d const *clip, real scale, real theta);
/* before a waypoint's arrow is placed: no arrow drawn yet */
void ae_waypoint_arrow_forget(void);
/* a new map: the sprites' inks read on the old one are dropped (its bitmaps may be at the same addresses) */
void ae_waypoint_ink_forget_all(void);
/* the arrow last drawn: where its ink's middle is across from the middle of its sprite (where the game puts the
waypoint's point), in the HUD's units; 0 for none drawn, a turned one (at the view's edge) or its pixels not read.
A map's own arrow art need not sit in the middle of its sprite (the stock double chevron, drawn for an item behind a
wall, sits about a unit left of it at 1080p) */
real ae_waypoint_arrow_ink_x(void);

/* a sprite (a label's picture) drawn with its middle at x, y (the view's HUD units, x to a fraction of one) at scale,
unturned: hud_draw_bitmap_direct's centred draw, its size the same */
void ae_waypoint_sprite_draw(struct bitmap_data const *bitmap, real_rectangle2d const *clip, real x, real y,
	real scale, pixel32 color);

#endif
