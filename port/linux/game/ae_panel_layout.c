/*
AE_PANEL_LAYOUT.C

Where HUD STYLE AE's panels go (ae_panel_layout.h; spec 14.15 parts 2, 3 and
6, G1, G6). Pure. The geometry is the rev13 generator's
(.superpowers/sdd/2026-10-11-score-strip/hud-panels/mock13.py ammo_group /
shield_group, mock8.py magazine), here in the view's pixels.
*/

#include <math.h>
#include <string.h>

#include "ae_panel_layout.h"

/* spec px at scale 1 (14.15 part 2, 3) */
#define LEFT_LINE_X AE_PANEL_LEFT_LINE_X
#define BOX_HEIGHT 36.0f
#define DIGIT_BASELINE 30.0f
#define DIGIT_CAP 24.0f
#define DIGIT_CELL 19.2f                /* rev5 (0.8 x cap; owner 10-15: rev5's proportions) */
#define DIGIT_CAP_FLOOR 12.0f
#define LINE_WIDTH 1.5f
#define GROUP_WIDTH 225.0f               /* rev5: CE's own width (lean line to lean line) */
#define NUMBER_BOX 121.0f
#define BOX_GAP 5.0f
#define GRENADE_BOX 99.0f
/* digits -> the "x" (its bottom-left, from the last digit's ink on the middle row), the x's size, the x -> the round's
box; the round and battery glyphs' heights; digits -> battery glyph (from the same ink) */
#define DIGITS_TO_X 10.36f
#define X_SIZE 12.0f
#define X_INK_WIDTH 0.86f
#define X_TO_ROUND 9.0f
#define ROUND_HEIGHT 23.0f
#define DIGITS_TO_BATTERY 6.36f
#define BATTERY_HEIGHT 20.0f
/* the grenade box's inner 96 at mid-height: 8 / frag 20 / 3 / cell 17 / 5 / plasma 17 / 3 / cell 17 / 6 (rev5's box) */
#define GRENADE_EDGE 8.0f
#define GRENADE_FRAG 20.0f
#define GRENADE_TO_CELL 3.0f
#define GRENADE_CELL 17.0f
#define GRENADE_PAIRS 5.0f
#define GRENADE_PLASMA 17.0f
#define FRAG_HEIGHT 24.0f
#define BOXES_TO_MAGAZINE 10.0f
#define ROW_GAP 4.0f
#define STANDING_MAXIMUM 32.0f
#define LYING_MAXIMUM 30.0f
#define BAR_HEIGHT 12.0f
#define SEAT_ROW_GAP 8.0f
#define SEAT_LABEL_GAP 8.0f
#define SEAT_BAR_BASELINE 11.0f
/* shield group */
#define SHIELD_BAR 236.0f
#define SHIELD_HEIGHT 20.0f
#define SHIELD_ICON 25.0f
#define SHIELD_ICON_GAP 7.0f
#define CELLS_GAP 6.0f
#define CELL_HEIGHT 10.0f
#define CELLS_LENGTH 170.0f
#define CELL_SHARE 0.72f
#define CROSS_SIZE 12.0f
#define CROSS_FLOOR 9.0f
#define CROSS_GAP 8.0f
#define CROSS_GAP_FLOOR 5.0f
#define BUFF_FIRST 18.0f
#define BUFF_CLEAR 6.0f
#define BUFF_PITCH 20.0f
#define BUFF_NAME_GAP 8.0f
#define LABEL_CAP 0.74f                 /* Overpass 900's cap height over its px */
#define LABEL_PX 14.0f
/* the soft backings (mock8 backing_at): centre offsets and radii */
#define AMMO_BACKING_DX (-6.0f)
#define AMMO_BACKING_DY 46.0f
#define AMMO_BACKING_RX 80.0f           /* + half the group */
#define AMMO_BACKING_RY 96.0f
#define SHIELD_BACKING_DX (-10.0f)      /* from the bar's middle */
#define SHIELD_BACKING_DY 18.0f
#define SHIELD_BACKING_RX 70.0f         /* + half the bar */
#define SHIELD_BACKING_RY 52.0f
#define SHIELD_BACKING_ROW_DY 9.0f
#define SHIELD_BACKING_ROW_RY 12.0f

/* CE's art, measured on port/assets/hud (alpha > 30): a round's ink box and the next round's ink left; the glyphs'
ink on their middle rows */
void ae_panel_metrics_ce(struct ae_panel_metrics *metrics)
{
	static const struct ae_panel_art_metrics art[NUMBER_OF_AE_ARTS] = {
		{ 0.7167f, 0.9535f, FALSE },    /* AR (hud_ammo_alphas 1): 43 x 60 */
		{ 0.6068f, 0.9296f, FALSE },    /* pistol (5): 71 x 117 */
		{ 0.8047f, 0.6796f, FALSE },    /* shell (7): 103 x 128 */
		{ 0.6723f, 0.5125f, FALSE },    /* needle (4): 80 x 119 */
		{ 5.3333f, 1.0833f, TRUE },     /* sniper (6): 384 x 72 */
		{ 3.3438f, 0.9953f, TRUE },     /* rocket (2): 428 x 128 */
		{ 1.0f, 1.0f, FALSE },          /* bar: no art */
	};

	memset(metrics, 0, sizeof(*metrics));
	metrics->digit_lean = 0.44f;        /* '0': 25.8 px of ink box at 24 px cap, 15.25 on its middle row */
	metrics->round_aspect = 0.8f;       /* 64 x 80 */
	metrics->round_mid_right = 0.7625f;
	metrics->battery_aspect = 2.625f;   /* 210 x 80, holes filled */
	metrics->battery_mid_right = 2.6375f;  /* with its edge's anti-aliasing (the renders: + 2 px at 20) */
	metrics->frag_aspect = 0.8333f;     /* 65 x 78 */
	metrics->frag_mid_left = 0.0385f;
	metrics->shield_aspect = 0.9091f;   /* 80 x 88 */
	memcpy(metrics->art, art, sizeof(art));
}

real ae_hud_corner_margin(real scale)
{
	return AE_HUD_CORNER_MARGIN * scale;
}

real ae_panel_right_column_x(const struct ae_panel_view *v)
{
	return v->shield_right_x;
}

real ae_panel_label_px(real scale)
{
	real px = LABEL_PX * scale;

	if (scale < 0.8f)
		return px > LABEL_PX ? px : LABEL_PX;
	return px > 12.0f ? px : 12.0f;
}

void ae_panel_para_bounds(const struct ae_panel_para *p, struct ae_panel_rect *out)
{
	real shift = p->lean * (p->y1 - p->y0);

	out->x0 = p->x0 + (shift < 0.0f ? shift : 0.0f);
	out->x1 = p->x1 + (shift > 0.0f ? shift : 0.0f);
	out->y0 = p->y0;
	out->y1 = p->y1;
}

static void para(struct ae_panel_para *p, real x0, real x1, real y0, real y1, real lean)
{
	p->x0 = x0; p->x1 = x1; p->y0 = y0; p->y1 = y1; p->lean = lean;
}

static void grow(struct ae_panel_rect *r, boolean *any, real x0, real y0, real x1, real y1)
{
	if (!*any)
	{
		r->x0 = x0; r->y0 = y0; r->x1 = x1; r->y1 = y1;
		*any = TRUE;
		return;
	}
	if (x0 < r->x0) r->x0 = x0;
	if (y0 < r->y0) r->y0 = y0;
	if (x1 > r->x1) r->x1 = x1;
	if (y1 > r->y1) r->y1 = y1;
}

static void grow_para(struct ae_panel_rect *r, boolean *any, const struct ae_panel_para *p)
{
	struct ae_panel_rect b;

	ae_panel_para_bounds(p, &b);
	grow(r, any, b.x0, b.y0, b.x1, b.y1);
}

static void ink(const struct ae_panel_metrics *g, const wchar_t *text, real cap, real cell, real *left, real *right)
{
	long n = 0;

	if (g->digits_ink && g->digits_ink(text, cap, cell, left, right))
		return;
	while (text[n])
		n++;
	/* a stand-in: a digit's ink centred in its cell, 0.6 of the cap wide plus the lean */
	*left = (cell - (0.6f + g->digit_lean) * cap) / 2.0f;
	*right = (real)(n - 1) * cell + cell - *left;
}

static real text_width(const struct ae_panel_metrics *g, const wchar_t *text, real px)
{
	long n = 0;

	if (g->text_width)
		return g->text_width(text, px);
	while (text[n])
		n++;
	return (real)n * 0.6f * px;
}

/* the magazine's rounds under the boxes (14.15 part 2); returns the block's bottom */
static real layout_magazine(const struct ae_panel_model *m, const struct ae_panel_metrics *g, real s, real top,
	real line_x0, real line_y0, struct ae_panel_layout *out)
{
	const struct ae_panel_art_metrics *art = &g->art[m->art >= 0 && m->art < NUMBER_OF_AE_ARTS ? m->art : 0];
	real width = GROUP_WIDTH * s, height, round_width, pitch;
	short longest = 0, row, i, count = 0;
	real y = top;

	for (row = 0; row < m->row_count && row < AE_PANEL_MAXIMUM_ROWS; row++)
		if (m->rows[row] > longest)
			longest = m->rows[row];
	if (longest <= 0)
		return top;
	if (art->lying)
	{
		/* the tallest row that still ends on the right line at its top: h x aspect x ((n - 1) pitch + 1) - lean h = W */
		real fit = width / (art->aspect * ((real)(longest - 1) * art->pitch + 1.0f) - AE_PANEL_LEAN);
		height = fit < LYING_MAXIMUM * s ? fit : LYING_MAXIMUM * s;
		round_width = height * art->aspect;
		pitch = round_width * art->pitch;
	}
	else
	{
		real native = width / ((real)(longest - 1) * art->pitch + 1.0f) / art->aspect;
		height = native < STANDING_MAXIMUM * s ? native : STANDING_MAXIMUM * s;
		round_width = height * art->aspect;
		pitch = longest > 1 ? (width + AE_PANEL_LEAN * height - round_width) / (real)(longest - 1) : 0.0f;
	}
	out->round_width = round_width;
	for (row = 0; row < m->row_count && row < AE_PANEL_MAXIMUM_ROWS; row++)
	{
		real x = line_x0 + AE_PANEL_LEAN * (line_y0 - (y + height));

		for (i = 0; i < m->rows[row] && count < AE_PANEL_LAYOUT_MAXIMUM_ROUNDS; i++, count++)
		{
			out->round[count].x = x + (real)i * pitch;
			out->round[count].y = y;
			out->round[count].height = height;
			out->round[count].lit = (boolean)(i < m->row_lit[row]);
		}
		y += height + ROW_GAP * s;
	}
	out->round_count = count;
	return y - ROW_GAP * s;
}

static void layout_seats(const struct ae_panel_model *m, const struct ae_panel_metrics *g, real s, real top,
	real line_x0, real line_y0, struct ae_panel_layout *out, boolean *any)
{
	const struct ae_panel_art_metrics *rocket = &g->art[_ae_art_rocket];
	real y = top;
	short i;

	out->seat_count = m->seat_count;
	for (i = 0; i < m->seat_count && i < AE_PANEL_MAXIMUM_SEAT_ROWS; i++)
	{
		real furthest;

		out->seat[i].shell = m->seat[i].shell;
		out->seat[i].label_px = ae_panel_label_px(s);
		if (m->seat[i].shell)
		{
			real h = LYING_MAXIMUM * s;

			out->seat[i].shell_x = line_x0 + AE_PANEL_LEAN * (line_y0 - (y + h));
			out->seat[i].shell_y = y;
			out->seat[i].shell_height = h;
			out->round_width = h * rocket->aspect;
			furthest = out->seat[i].shell_x + out->round_width;
			out->seat[i].label_baseline_y = y + h / 2.0f + 5.0f * s;
			grow(&out->ammo_group, any, out->seat[i].shell_x, y, furthest, y + h);
			y += h + SEAT_ROW_GAP * s;
		}
		else
		{
			real h = BAR_HEIGHT * s, x0 = line_x0 + AE_PANEL_LEAN * (line_y0 - (y + h));

			para(&out->seat[i].bar, x0, x0 + GROUP_WIDTH * s, y, y + h, AE_PANEL_LEAN);
			furthest = x0 + GROUP_WIDTH * s + AE_PANEL_LEAN * h;
			out->seat[i].label_baseline_y = y + SEAT_BAR_BASELINE * s;
			grow_para(&out->ammo_group, any, &out->seat[i].bar);
			y += h + SEAT_ROW_GAP * s;
		}
		/* each row's label trails its own row's furthest ink by 8 px (owner 10-15: rev13's look, over G6's shared x:
		the Scorpion's CANNON sits beside its shell, MG after the bar) */
		{
			static const wchar_t widest[] = L"CANNON";

			out->seat[i].label_x = furthest + SEAT_LABEL_GAP * s;
			grow(&out->ammo_group, any, out->seat[i].label_x,
				out->seat[i].label_baseline_y - LABEL_CAP * out->seat[i].label_px,
				out->seat[i].label_x + text_width(g, widest, out->seat[i].label_px), out->seat[i].label_baseline_y);
		}
	}
}

static void layout_ammo(const struct ae_panel_model *m, const struct ae_panel_metrics *g,
	const struct ae_panel_view *v, struct ae_panel_layout *out)
{
	const real s = v->scale;
	const real top = v->ammo_origin_y;
	const real line_y0 = top + BOX_HEIGHT * s, line_x0 = v->ammo_origin_x + LEFT_LINE_X * s;
	const real mid = top + BOX_HEIGHT * s / 2.0f;
	const real line_mid = line_x0 + AE_PANEL_LEAN * (line_y0 - mid);
	real cap = DIGIT_CAP * s, cell;
	boolean any = FALSE;
	real bottom = line_y0;

	out->left_line_x0 = line_x0;
	out->left_line_y0 = line_y0;
	out->right_line_x0 = line_x0 + GROUP_WIDTH * s;
	if (cap < DIGIT_CAP_FLOOR)
		cap = DIGIT_CAP_FLOOR;
	cell = cap * (DIGIT_CELL / DIGIT_CAP);
	out->digit_baseline_y = top + DIGIT_BASELINE * s;
	out->digit_cap = cap;
	out->digit_cell = cell;

	if (m->weapon_kind == _ae_panel_none)
		return;
	out->ammo_group_shown = TRUE;
	out->backing_ammo.x0 = line_x0 + GROUP_WIDTH * s / 2.0f + AMMO_BACKING_DX * s - (GROUP_WIDTH * s / 2.0f +
		AMMO_BACKING_RX * s);
	out->backing_ammo.x1 = line_x0 + GROUP_WIDTH * s / 2.0f + AMMO_BACKING_DX * s + (GROUP_WIDTH * s / 2.0f +
		AMMO_BACKING_RX * s);
	if (m->weapon_kind == _ae_panel_seat)
	{
		layout_seats(m, g, s, top, line_x0, line_y0, out, &any);
		/* a seat's rows from the panel's top (mock13: centre (half the group + 10, 14), radii (half + 96, 52)) */
		out->backing_ammo.x0 = line_x0 - 86.0f * s;
		out->backing_ammo.x1 = line_x0 + (GROUP_WIDTH + 106.0f) * s;
		out->backing_ammo.y0 = top - 38.0f * s;
		out->backing_ammo.y1 = top + 66.0f * s;
		return;
	}
	out->backing_ammo.y0 = top + (AMMO_BACKING_DY - AMMO_BACKING_RY) * s;
	out->backing_ammo.y1 = top + (AMMO_BACKING_DY + AMMO_BACKING_RY) * s;

	/* the boxes */
	para(&out->number_box, line_x0, line_x0 + NUMBER_BOX * s, top, line_y0, AE_PANEL_LEAN);
	para(&out->grenade_box, line_x0 + (NUMBER_BOX + BOX_GAP) * s, line_x0 + (NUMBER_BOX + BOX_GAP + GRENADE_BOX) * s,
		top, line_y0, AE_PANEL_LEAN);
	grow_para(&out->ammo_group, &any, &out->number_box);
	grow_para(&out->ammo_group, &any, &out->grenade_box);

	/* the number block (digits + "x" + glyph), centred by its ink at mid-height */
	if (m->number[0])
	{
		const real inset = g->digit_lean * cap / 2.0f;
		const real inner_left = line_mid + LINE_WIDTH * s, inner_right = line_mid + (NUMBER_BOX - LINE_WIDTH) * s;
		real ink_left, ink_right, zeros_left, zeros_right, block_right, x0;
		real x_mark = 0.0f, glyph = 0.0f;
		short i;

		ink(g, m->number, cap, cell, &ink_left, &ink_right);
		ink(g, L"000", cap, cell, &zeros_left, &zeros_right);
		ink_left += inset;
		zeros_right -= inset;
		out->number_shown = TRUE;
		if (m->glyph == _ae_glyph_round)
		{
			x_mark = zeros_right + DIGITS_TO_X * s;
			glyph = x_mark + (X_INK_WIDTH * X_SIZE + X_TO_ROUND) * s;
			out->glyph_height = ROUND_HEIGHT * s;
			block_right = glyph + g->round_mid_right * out->glyph_height;
			out->x_mark_shown = out->glyph_shown = TRUE;
		}
		else if (m->glyph == _ae_glyph_battery)
		{
			glyph = zeros_right + DIGITS_TO_BATTERY * s;
			out->glyph_height = BATTERY_HEIGHT * s;
			block_right = glyph + g->battery_mid_right * out->glyph_height;
			out->glyph_shown = TRUE;
		}
		else
		{
			/* the number alone (the tank): its own digits' ink */
			real a, b;
			ink(g, m->number, cap, cell, &a, &b);
			block_right = b - inset;
		}
		x0 = inner_left + ((inner_right - inner_left) - (block_right - ink_left)) / 2.0f - ink_left;
		for (i = 0; i < 3; i++)
			out->number_cell_x[i] = x0 + (real)i * cell;
		out->x_mark_x = x0 + x_mark;
		out->x_mark_size = X_SIZE * s;
		out->glyph_x = x0 + glyph;
		out->glyph_y = out->digit_baseline_y - out->glyph_height;
	}

	/* the grenade box: fixed glyph and cell x at mid-height, each count's ink centred in its cell */
	if (m->grenade[0].shown)
	{
		const real inner = line_mid + (NUMBER_BOX + BOX_GAP) * s + LINE_WIDTH * s;
		real x = inner + GRENADE_EDGE * s;
		short i;

		out->frag_height = FRAG_HEIGHT * s;
		out->frag_x = x - g->frag_mid_left * out->frag_height;
		out->frag_y = out->digit_baseline_y - out->frag_height;
		x += (GRENADE_FRAG + GRENADE_TO_CELL) * s;
		out->count_cell_x[0] = x;
		x += (GRENADE_CELL + GRENADE_PAIRS) * s;
		out->plasma_diameter = GRENADE_PLASMA * s;
		out->plasma_cx = x + out->plasma_diameter / 2.0f;
		out->plasma_cy = mid;
		x += (GRENADE_PLASMA + GRENADE_TO_CELL) * s;
		out->count_cell_x[1] = x;
		for (i = 0; i < 2; i++)
		{
			real a, b;

			ink(g, m->grenade[i].count, cap, cell, &a, &b);
			out->count_ink_x[i] = out->count_cell_x[i] + GRENADE_CELL * s / 2.0f - (a + b) / 2.0f;
		}
	}

	/* under the boxes: the magazine, then (heat) the bar */
	{
		real y = line_y0 + BOXES_TO_MAGAZINE * s;

		if (m->row_count > 0)
		{
			short i;

			bottom = layout_magazine(m, g, s, y, line_x0, line_y0, out);
			for (i = 0; i < out->round_count; i++)
				grow(&out->ammo_group, &any, out->round[i].x, out->round[i].y, out->round[i].x + out->round_width,
					out->round[i].y + out->round[i].height);
			y = bottom + SEAT_ROW_GAP * s;
		}
		if (m->bar_shown)
		{
			real h = BAR_HEIGHT * s, x0 = line_x0 + AE_PANEL_LEAN * (line_y0 - (y + h));

			para(&out->bar, x0, x0 + GROUP_WIDTH * s, y, y + h, AE_PANEL_LEAN);
			out->bar_shown = TRUE;
			grow_para(&out->ammo_group, &any, &out->bar);
			bottom = y + h;
		}
	}
	if (bottom + 0.0f > out->backing_ammo.y1)
		out->backing_ammo.y1 = bottom;
}

static void layout_shield(const struct ae_panel_model *m, const struct ae_panel_metrics *g,
	const struct ae_panel_view *v, struct ae_panel_layout *out)
{
	const real s = v->scale;
	const real right = ae_panel_right_column_x(v), top = v->shield_top_y;
	const real cells_top = top + (SHIELD_HEIGHT + CELLS_GAP) * s, cells_bottom = cells_top + CELL_HEIGHT * s;
	const real cells_right = right;   /* (owner 10-15: under the bar's bottom-right corner, not its lean carried down) */
	const real pitch = CELLS_LENGTH * s / (real)AE_PANEL_HEALTH_CELLS, cell = pitch * CELL_SHARE;
	real size, gap, first_left_mid, baseline, px;
	boolean any = FALSE;
	short i;

	para(&out->shield_bar, right - SHIELD_BAR * s, right, top, top + SHIELD_HEIGHT * s, -AE_PANEL_LEAN);
	out->shield_icon_height = SHIELD_ICON * s;
	out->shield_icon_x = out->shield_bar.x0 - AE_PANEL_LEAN * SHIELD_HEIGHT * s / 2.0f - SHIELD_ICON_GAP * s -
		out->shield_icon_height * g->shield_aspect;
	out->shield_icon_y = top + SHIELD_HEIGHT * s / 2.0f - out->shield_icon_height / 2.0f;
	grow_para(&out->shield_group, &any, &out->shield_bar);
	grow(&out->shield_group, &any, out->shield_icon_x, out->shield_icon_y, out->shield_icon_x +
		out->shield_icon_height * g->shield_aspect, out->shield_icon_y + out->shield_icon_height);

	for (i = 0; i < AE_PANEL_HEALTH_CELLS; i++)
	{
		real x0 = cells_right - CELLS_LENGTH * s + (real)i * pitch + (pitch - cell);

		para(&out->health_cell[i], x0, x0 + cell, cells_top, cells_bottom, -AE_PANEL_LEAN);
		grow_para(&out->shield_group, &any, &out->health_cell[i]);
	}
	/* G1: the cross, 12 x scale, never under 9 px, a multiple of 3 (arms a third), pixel-snapped, centred on the
	cells' row, 8 x scale (never under 5) left of the first cell's left edge at mid-height */
	size = floorf(CROSS_SIZE * s + 0.5f);
	if (size < CROSS_FLOOR)
		size = CROSS_FLOOR;
	size -= fmodf(size, 3.0f);
	gap = CROSS_GAP * s > CROSS_GAP_FLOOR ? CROSS_GAP * s : CROSS_GAP_FLOOR;
	first_left_mid = out->health_cell[0].x0 - AE_PANEL_LEAN * CELL_HEIGHT * s / 2.0f;
	out->cross_size = size;
	out->cross_x = floorf(first_left_mid - gap - size + 0.5f);
	out->cross_y = floorf((cells_top + cells_bottom) / 2.0f - size / 2.0f + 0.5f);
	grow(&out->shield_group, &any, out->cross_x, out->cross_y, out->cross_x + size, out->cross_y + size);

	/* own-buff rows: right-aligned on the right column, the first 18 px under the cells and never under 6 px on
	screen from them to the cap top, 20 px apart */
	px = ae_panel_label_px(s);
	baseline = cells_bottom + (BUFF_FIRST * s > BUFF_CLEAR + LABEL_CAP * px ? BUFF_FIRST * s : BUFF_CLEAR + LABEL_CAP * px);
	out->buff_count = m->buff_count;
	for (i = 0; i < m->buff_count && i < AE_PANEL_MAXIMUM_BUFFS; i++)
	{
		real time_width = text_width(g, L"0:00", px);

		out->buff[i].px = px;
		out->buff[i].baseline_y = baseline;
		out->buff[i].time_right_x = right;
		out->buff[i].name_right_x = right - time_width - BUFF_NAME_GAP * s;
		grow(&out->shield_group, &any, out->buff[i].name_right_x - text_width(g, m->buff[i].name, px),
			baseline - LABEL_CAP * px, right, baseline);
		baseline += BUFF_PITCH * s;
	}

	{
		real cx = right - SHIELD_BAR * s / 2.0f + SHIELD_BACKING_DX * s;
		real cy = top + (SHIELD_BACKING_DY + SHIELD_BACKING_ROW_DY * (real)out->buff_count) * s;
		real rx = (SHIELD_BAR / 2.0f + SHIELD_BACKING_RX) * s;
		real ry = (SHIELD_BACKING_RY + SHIELD_BACKING_ROW_RY * (real)out->buff_count) * s;

		out->backing_shield.x0 = cx - rx; out->backing_shield.x1 = cx + rx;
		out->backing_shield.y0 = cy - ry; out->backing_shield.y1 = cy + ry;
		if (out->backing_shield.y0 < 0.0f)
			out->backing_shield.y0 = 0.0f;
		if (out->backing_shield.x1 > v->view_width)
			out->backing_shield.x1 = v->view_width;
	}
}

void ae_panel_layout_build(const struct ae_panel_model *m, const struct ae_panel_metrics *g,
	const struct ae_panel_view *v, struct ae_panel_layout *out)
{
	memset(out, 0, sizeof(*out));
	layout_ammo(m, g, v, out);
	layout_shield(m, g, v, out);
	/* the backings are soft ellipses: clipped to the view (they never draw outside it) */
	if (out->backing_ammo.x0 < 0.0f) out->backing_ammo.x0 = 0.0f;
	if (out->backing_ammo.y0 < 0.0f) out->backing_ammo.y0 = 0.0f;
	if (out->backing_ammo.y1 > v->view_height) out->backing_ammo.y1 = v->view_height;
}
