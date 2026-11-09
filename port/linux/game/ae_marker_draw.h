/*
AE_MARKER_DRAW.H

The MARKERS drawn (ae_marker_draw.c): the glyph of a mark in the world, eased in size by where it sits against the view's middle,
its rows above it or, near the crosshair or with a fact, on a lead line out of its corner (the aim prompt's row on top, then the
fact, the name and the distance, the owner), and the OK flash and OK row. ae_marks.c reads the world into a struct ae_marker for
each mark; this file lays it out and draws it, in the view's pixels as the wheel is. The numbers are ae_marker_rules.c's (unit
tested); the look is the design's (marker-*.png).

Include after cseries.h, math/real_math.h and bitmaps/bitmaps.h.
*/

#ifndef __AE_MARKER_DRAW_H
#define __AE_MARKER_DRAW_H

#include <wchar.h>

#define AE_MARKER_NAME_SIZE 28
#define AE_MARKER_OK_NAMES 3

struct bitmap_data;

/* what one mark is, as the world has it now. Places are the view's HUD units from its top left, as ae_marks_draw finds them */
struct ae_marker
{
	short slot;				/* the marker's absolute player index: the state kept for it (its size's easing, its flash) */
	long born;				/* when the mark was made (game ticks): a new mark starts that state over */
	short glyph;			/* AE_WAYPOINT_GLYPH_*, its dark outline the next one */
	boolean enemy;			/* in the enemy red; any other in the HUD white */
	boolean rifle;			/* the game's rifle sprite instead of a glyph (no call asks for it now): FALSE when the map has none */
	boolean pin;			/* a place's pin stands on its tip */
	boolean clamped;		/* at the ellipse's edge: off the screen, or past the marks' ellipse */
	real x, y;				/* the point it marks, in the view */
	real distance;			/* in world units */
	real alpha;				/* the mark's age's */
	real appear;			/* its size as it appears */
	real fade;				/* 0.3 behind one of the callout wheel's sectors */
	int repeats;			/* the same call made again, bound by the caller to 0 to 2 */
	wchar_t name[AE_MARKER_NAME_SIZE];	/* the name row's word: HERE, ATTACK, a weapon's own name; empty for none */
	wchar_t fact[AE_MARKER_NAME_SIZE];	/* ROCKETS 0:09, FLAG DROPPED, GUNNER OPEN; empty for none */
	struct bitmap_data const *fact_sprite;	/* the power item's picture over its fact, or NULL */
	real fact_clip[4];		/* its clip: x0, y0, x1, y1 */
	wchar_t owner[AE_MARKER_NAME_SIZE];	/* the caller's name, or "You said OK" (a teammate's mark only) */
	wchar_t action[AE_MARKER_NAME_SIZE];	/* the aim prompt's word when this is the marker the player aims at; else empty */
	unsigned short responders;	/* who has said OK, for the flash: a bit for each absolute player index */
	int ok_count;			/* how many have said OK, none of them muted here */
	wchar_t ok_names[AE_MARKER_OK_NAMES][AE_MARKER_NAME_SIZE];	/* the first of them */
};

/* ae_marks_draw's, once for each mark this view shows */
void ae_marker_draw(short local_player_index, struct ae_marker const *marker);

#endif
