/*
AE_PANEL_LAYOUT.H

Where HUD STYLE AE's panels go (spec 14.15 parts 2, 3 and 6, gate corrections
G1 and G6; plan Task 12): a panel model (ae_panel_model.h) and a view's
anchors in, every line, box, digit cell, round, bar, cell and row out, in the
view's pixels (x from the view's left, y from its top). Pure: the art's and
the digits' measures come in as plain numbers and two callbacks
(struct ae_panel_metrics), so the unit test
(port/linux/tests/ae_panel_layout_test.c) builds it without the game.

All sizes are spec px at a 1920x1080 single view times the view's HUD scale.
Shapes that lean are parallelograms given by their BOTTOM edge (x0 .. x1 at
y1) and their lean: the top edge is shifted by lean x height (+0.46 for the
left cluster's '/', -0.46 for the right cluster's '\').
*/

#ifndef __AE_PANEL_LAYOUT_H
#define __AE_PANEL_LAYOUT_H

#include <wchar.h>

#include "ae_pure_types.h"
#include "ae_panel_model.h"

#define AE_PANEL_LEAN 0.46f

struct ae_panel_rect { real x0, y0, x1, y1; };        /* axis-aligned: left, top, right, bottom */
struct ae_panel_para { real x0, x1, y0, y1, lean; };  /* bottom edge x0 .. x1 at y1, top at y0, top shifted by lean x h */

/* one round's art: its ink box's width over its height, the next round's ink left minus this one's (over the ink
width), and whether it lies (sniper, rockets) */
struct ae_panel_art_metrics { real aspect, pitch; boolean lying; };

struct ae_panel_metrics
{
	/* the digits' ink box from the first cell's left (x = 0), every character in a cell_px cell, ink centred
	(ae_digits_ink_bounds_cells); NULL: a fixed-width stand-in */
	boolean (*digits_ink)(const wchar_t *text, real cap_px, real cell_px, real *left, real *right);
	/* a text's width in the panels' label face at px; NULL: a stand-in */
	real (*text_width)(const wchar_t *text, real px);
	real digit_lean;            /* CE's counter digits lean: the ink on the middle row is inside the box by this x cap / 2 */
	real round_aspect, round_mid_right;      /* hud_ammo_type_icons 0's round: w / h; its ink's right on its middle row, x h */
	real battery_aspect, battery_mid_right;  /* hud_ammo_type_icons 4 */
	real frag_aspect, frag_mid_left;         /* hud_ammo_type_icons 2 */
	real shield_aspect;                      /* hud_unit_backgrounds 0's shield icon */
	struct ae_panel_art_metrics art[NUMBER_OF_AE_ARTS];
};

/* CE's art as measured on port/assets/hud (tools: the panel art table in ae_panel_layout.c); the callbacks NULL */
void ae_panel_metrics_ce(struct ae_panel_metrics *metrics);

struct ae_panel_view
{
	real scale;                                 /* the view's HUD scale (1p 1.0, 2p 0.85, quarter views 0.75) */
	real ammo_origin_x, ammo_origin_y;          /* CE ammo panel's top-left in the view */
	real shield_right_x, shield_top_y;          /* shield meter's bottom-right x, box row top */
	real view_width, view_height;
};

enum { AE_PANEL_LAYOUT_MAXIMUM_ROUNDS = 64 };

struct ae_panel_layout                          /* all in view px */
{
	real left_line_x0, left_line_y0, right_line_x0;  /* the two lean lines' x at the box row's bottom (y0) */
	struct ae_panel_para number_box, grenade_box;
	struct ae_panel_rect backing_ammo, backing_shield;  /* the soft backings' ellipses' boxes */
	real digit_baseline_y, digit_cap, digit_cell;
	real number_cell_x[3];                      /* each digit cell's left: draw the number from number_cell_x[0] */
	boolean number_shown, x_mark_shown, glyph_shown;
	real x_mark_x, x_mark_size;                 /* the "x": its bottom-left on the baseline, its leaning square's size */
	real glyph_x, glyph_y, glyph_height;        /* the round / battery glyph's box top-left, height */
	real frag_x, frag_y, frag_height;           /* box top-left */
	real plasma_cx, plasma_cy, plasma_diameter;
	real count_cell_x[2];                       /* each count's cell left at mid-height */
	real count_ink_x[2];                        /* where to draw each count's text from so its ink is centred in its cell */
	short round_count;
	struct { real x, y, height; boolean lit; } round[AE_PANEL_LAYOUT_MAXIMUM_ROUNDS];  /* ink box top-left, height */
	real round_width;
	struct ae_panel_para bar; boolean bar_shown;    /* heat / tank */
	struct ae_panel_para shield_bar;
	real shield_icon_x, shield_icon_y, shield_icon_height;  /* box top-left */
	struct ae_panel_para health_cell[AE_PANEL_HEALTH_CELLS];
	real cross_x, cross_y, cross_size;          /* the plus sign's box top-left and size, whole px */
	struct { real name_right_x, time_right_x, baseline_y, px; } buff[AE_PANEL_MAXIMUM_BUFFS];
	short buff_count;
	struct { struct ae_panel_para bar; boolean shell; real shell_x, shell_y, shell_height;
		real label_x, label_baseline_y, label_px; } seat[AE_PANEL_MAXIMUM_SEAT_ROWS];
	short seat_count;
	struct ae_panel_rect ammo_group, shield_group;  /* what each group covers (for the waypoints' fade) */
	boolean ammo_group_shown;
};

void ae_panel_layout_build(const struct ae_panel_model *m, const struct ae_panel_metrics *g,
	const struct ae_panel_view *v, struct ae_panel_layout *out);
/* = v->shield_right_x: the shield bar, the own-buff rows and the timers list all end on it */
real ae_panel_right_column_x(const struct ae_panel_view *v);
/* a para's bounding box */
void ae_panel_para_bounds(const struct ae_panel_para *p, struct ae_panel_rect *out);
/* the panels' small labels on screen: 14 px x scale, never under 12 px; 14 px in quarter views (scale under 0.8) */
real ae_panel_label_px(real scale);
/* the ammo boxes' left lean line: its x at the box row's bottom, from the ammo group's origin (px x scale) */
#define AE_PANEL_LEFT_LINE_X 19.2f
/* one edge margin for the HUD's top corners (owner 10-15: "all corners square"; the bottom row stays as it is), px at a
1080p single view: from the view's left edge to the ammo boxes' bottom-left corner, from its right edge to the right
column (shield bar, health cells, timers list) */
#define AE_HUD_CORNER_MARGIN 162.0f
real ae_hud_corner_margin(real scale);

#endif /* __AE_PANEL_LAYOUT_H */
