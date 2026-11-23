/*
AE_STRIP_LAYOUT.H

The score strip's layout (spec 11.5, 11.7, 11.10b, 11.12, 14.15 part 6, gate
correction G5): a model and a view's geometry in, every element's place out.
Pure: text is measured by functions the caller passes (the renderer passes the
real metrics, port/linux/tests/ae_strip_layout_test.c fixed-width fakes).

Units: the view's pixels. Lengths in the spec are px at a 1080p single view
(s = 1) and are multiplied by strip_scale (the view factor, less when the gap
rule shrinks the strip). Small text never goes under its floor on screen: 12 px,
14 px in quarter views (s under 0.8); counter digits never under 12 px cap.

CTF's flag lines: a sub-line's box holds the flag glyph (AE_STRIP_FLAG_GLYPH x
its px) and a gap before its text, in reading order (the glyph first).

A text's box: x, w its extent; y + h its BASELINE, h its size in px (Overpass
px for text, cap height for counter digits), so the renderer draws at
(x, y + h) at size h. Bars and cells: the box is the bottom edge's span; the
top edge leans 0.46 x h toward the clock ('/' left, '\' right).
*/

#ifndef __AE_STRIP_LAYOUT_H
#define __AE_STRIP_LAYOUT_H

#include "ae_strip_model.h"

struct ae_strip_geometry {           /* inputs, in the view's pixels */
	real view_left, view_top, view_width, view_height;
	real s;                          /* view factor: 1.00 full, 0.85 half, 0.75 quarter (x 1440p/4K height scale) */
	real shared_line_y;              /* motion sensor "15m" baseline (spec 11.12) */
	real list_left_x;                /* the timers list's left edge in this view (or view right when no list) */
	real centre_x;                   /* HUD area centre */
};
struct ae_strip_box { real x, y, w, h; boolean visible; };
struct ae_strip_layout {
	real score_baseline_y, clock_baseline_y;
	struct ae_strip_box left_score, right_score, clock, context, lead, goal, left_sub, right_sub;
	struct ae_strip_box left_bar, right_bar;            /* lean 0.46, mirrored */
	struct ae_strip_box marker, left_pips, right_pips, left_cells, right_cells;
	real bar_length;                                    /* 150 x s, shrunk toward 60 x s before scaling */
	real strip_scale;                                   /* <= s when the gap rule forces it */
	real backing_cx, backing_cy, backing_rx, backing_ry; /* backing_ry: the upper radius; the lower is 30/46 of it */
	/* (additions to the plan's layout) */
	struct ae_strip_box left_alive, right_alive;        /* "6 ALIVE" where the cells would be */
	real right_end_x;                                   /* the strip's rightmost ink, for the gap check */
	real left_end_x;
	struct ae_strip_box played;                         /* MATCH CLOCK BOTH: the time played, over the clock */
};

/* the layout's constants (px at 1080p, s = 1): spec 11.5 and the mockups (mock4.py) */
#define AE_STRIP_GAP_TO_CLOCK 30.0f       /* scores' inner edges from the clock cell */
#define AE_STRIP_BAR_GAP 16.0f            /* bars (and cells, "ALIVE") beyond each score */
#define AE_STRIP_BAR_LENGTH 150.0f
#define AE_STRIP_BAR_MINIMUM 60.0f
#define AE_STRIP_BAR_HEIGHT 9.0f
#define AE_STRIP_BAR_BOTTOM 7.5f          /* bar bottom above the score baseline (bar centre 12 above) */
#define AE_STRIP_LIST_GAP 48.0f           /* the strip's right end from the timers list */
#define AE_STRIP_SCORE_CAP 24.0f
#define AE_STRIP_CLOCK_CAP 20.0f
#define AE_STRIP_CONTEXT_PX 16.0f
#define AE_STRIP_SUB_PX 14.0f
#define AE_STRIP_GOAL_PX 13.0f
#define AE_STRIP_SMALL_CLOCK_PX 15.0f     /* timed and race modes: the clock as a grey label */
#define AE_STRIP_ALIVE_PX 16.0f
#define AE_STRIP_NAME_PX 18.0f            /* race / rally: the leader's name in the score's place */
#define AE_STRIP_SUFFIX_PX 15.0f          /* race / rally: "ND" after my place's digit, 2 px after it */
#define AE_STRIP_CONTEXT_RISE 40.0f       /* context / lead baseline above the score baseline */
#define AE_STRIP_GOAL_DROP 19.0f          /* "TO 50" baseline under the score baseline */
#define AE_STRIP_TIMED_GOAL_DROP 10.0f    /* timed modes: "TO 5:00" under the small clock */
#define AE_STRIP_TIMED_CLOCK_RISE 6.0f    /* timed modes: the small clock's baseline above the score baseline */
#define AE_STRIP_SUB_DROP 22.0f
#define AE_STRIP_SUB_DROP_PIPS 36.0f      /* sub-lines under pips */
#define AE_STRIP_PIPS_DROP 17.0f          /* pips' bottom under the score baseline */
#define AE_STRIP_PIP_PITCH 9.0f
#define AE_STRIP_PIP_WIDTH 4.0f
#define AE_STRIP_PIP_HEIGHT 9.0f
#define AE_STRIP_CELL_WIDTH 15.0f
#define AE_STRIP_ALIVE_DROP 6.0f          /* "6 ALIVE" baseline under the bar's centre (12 - 6 above the score) */
#define AE_STRIP_MARKER_WIDTH 13.0f
#define AE_STRIP_MARKER_HEIGHT 6.76f      /* 13 x 0.52 */
#define AE_STRIP_MARKER_RISE 31.0f        /* the marker's tip above the score baseline */
#define AE_STRIP_MARKER_CLEAR 8.0f        /* a context / lead line wide enough to reach over the marker: this far above it (the clearance rule; was 6) */
#define AE_STRIP_MARKER_SIDE_CLEAR 8.0f   /* ... reaching within this of its sides counts too (the clearance rule) */
#define AE_STRIP_LEAN 0.46f
#define AE_STRIP_TEXT_FLOOR 12.0f
#define AE_STRIP_QUARTER_TEXT_FLOOR 14.0f /* quarter views (s under 0.8), 14.15 part 6 */
#define AE_STRIP_QUARTER_VIEW_FACTOR 0.8f
#define AE_STRIP_DIGITS_FLOOR 12.0f
#define AE_STRIP_MINIMUM_SCALE 0.5f       /* the gap rule never scales the strip under half the view factor */
#define AE_STRIP_FLAG_GLYPH 0.95f         /* CTF's flag glyph before a flag line, x the line's px (mock4.py) */
#define AE_STRIP_FLAG_GAP 0.35f           /* and the room after it, x the line's px */
#define AE_STRIP_PLAYED_PX 13.0f          /* MATCH CLOCK BOTH's time played: the goal line's size */
#define AE_STRIP_PLAYED_CLOCK_GAP 10.0f   /* its ink over the clock's top (8, and room for the last seconds' pulse) */
#define AE_STRIP_PLAYED_CONTEXT_GAP 8.0f  /* the context / lead line's lowest ink over it */
#define AE_STRIP_TEXT_CAP 0.70f           /* Overpass's capital height, of its px (ae_hud_text.h AE_HUD_TEXT_CAP) */
#define AE_STRIP_TEXT_DESCENT 0.22f       /* and its descenders' depth ("Player"'s y), of its px */

/* FFA's YOU colour (spec 11.4): my armour colour lifted until its relative luminance is at least 0.33 (the tints'
level beside the outline), its hue kept; WHITE (229,240,255) when the armour is white, black or grey (saturation
under 0.2), too dark to lift (lightness under 0.08) or too close to white to stay distinct (lightness over 0.9).
FALSE when it is white. (The renderer's colour helper; in this pure file to be unit-tested.) */
struct ae_strip_rgb { real red, green, blue; };
boolean ae_score_strip_me_color(const struct ae_strip_rgb *armour, struct ae_strip_rgb *out);

void ae_strip_layout_compute(const struct ae_strip_model *model, const struct ae_strip_geometry *geometry,
	real (*measure_digits)(const wchar_t *text, real cap_px), real (*measure_text)(const wchar_t *text, real px),
	struct ae_strip_layout *layout);
/* the size in px a strip text of `base` px (at 1080p) is drawn at, with the strip at `scale` in a view of factor
`s`: base x scale, never under the floor */
real ae_strip_text_px(real base, real scale, real s);
/* the counter digits' cap height in px: base x scale, never under 12 */
real ae_strip_digits_px(real base, real scale);

#endif /* __AE_STRIP_LAYOUT_H */
