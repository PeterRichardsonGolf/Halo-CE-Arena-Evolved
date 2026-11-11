/* HUD STYLE AE's panel layout (port/linux/game/ae_panel_layout.c): spec 14.15 parts 2, 3 and 6, G1 and G6, plan
Task 12 and Review Focus 6 and 10; the digits' ink is the real one (ae_hud_digit_cells.c), the label face a
stand-in */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ae_hud_digit_cells.h"
#include "ae_panel_layout.h"

static int failures;
static int checks;
#define AE_CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } \
	while (0)
#define AE_NEAR(a, b, d) do { double a_ = (a), b_ = (b); checks++; if (fabs(a_ - b_) > (d)) { \
	printf("FAIL %s:%d %s = %.3f, wanted %.3f +- %.2f\n", __FILE__, __LINE__, #a, a_, b_, (double)(d)); failures++; } } \
	while (0)

static boolean digits_ink(const wchar_t *text, real cap_px, real cell_px, real *left, real *right)
{
	return ae_digits_ink_bounds_cells(text, cap_px, cell_px, FALSE, left, right);
}

static real text_width(const wchar_t *text, real px)
{
	return (real)wcslen(text) * 0.6f * px;
}

static struct ae_panel_metrics metrics;

static void view(struct ae_panel_view *v, real s, real ox, real oy, real rx, real ry, real w, real h)
{
	v->scale = s; v->ammo_origin_x = ox; v->ammo_origin_y = oy; v->shield_right_x = rx; v->shield_top_y = ry;
	v->view_width = w; v->view_height = h;
}

static void view_1p(struct ae_panel_view *v) { view(v, 1.0f, 150, 83, 1758, 83, 1920, 1080); }
static void view_4p(struct ae_panel_view *v) { view(v, 0.75f, 96, 39, 856, 39, 960, 540); }

static void weapon(struct ae_panel_model *m, short hint, short magazine, short loaded, short reserve)
{
	struct ae_panel_snapshot s;

	memset(&s, 0, sizeof(s));
	s.has_weapon = TRUE; s.weapon_uses_rounds = TRUE; s.weapon_type_hint = hint; s.rounds_per_magazine = magazine;
	s.rounds_loaded = loaded; s.rounds_reserve = reserve; s.grenades[0] = 2; s.grenades[1] = 1;
	s.shield = 1; s.health = 1;
	ae_panel_model_build(&s, m);
}

static void energy(struct ae_panel_model *m, real charge)
{
	struct ae_panel_snapshot s;

	memset(&s, 0, sizeof(s));
	s.has_weapon = TRUE; s.weapon_has_battery = TRUE; s.weapon_has_heat = TRUE; s.battery = charge;
	s.weapon_type_hint = _ae_hint_none; s.shield = 1; s.health = 1;
	ae_panel_model_build(&s, m);
}

/* x of the left lean line at y */
static real left_line(const struct ae_panel_layout *l, real y)
{
	return l->left_line_x0 + AE_PANEL_LEAN * (l->left_line_y0 - y);
}

/* the number block's padding at mid-height from the number box's inner edges to its ink */
static void number_padding(const struct ae_panel_model *m, const struct ae_panel_layout *l, real s, real *left,
	real *right)
{
	real mid = l->number_box.y1 - (l->number_box.y1 - l->number_box.y0) / 2.0f;
	real inner_left = left_line(l, mid) + 1.5f * s, inner_right = left_line(l, mid) + 121.0f * s - 1.5f * s;
	real ink_left, ink_right, block_right;
	real inset = metrics.digit_lean * l->digit_cap / 2.0f;

	digits_ink(m->number, l->digit_cap, l->digit_cell, &ink_left, &ink_right);
	if (m->glyph == _ae_glyph_round)
		block_right = l->glyph_x + metrics.round_mid_right * l->glyph_height;
	else if (m->glyph == _ae_glyph_battery)
		block_right = l->glyph_x + metrics.battery_mid_right * l->glyph_height;
	else
		block_right = l->number_cell_x[0] + ink_right - inset;
	*left = l->number_cell_x[0] + ink_left + inset - inner_left;
	*right = inner_right - block_right;
}

static void test_number_box_1p(void)
{
	struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v; real a, b;

	view_1p(&v);
	weapon(&m, _ae_hint_assault_rifle, 60, 60, 180); ae_panel_layout_build(&m, &metrics, &v, &l);
	number_padding(&m, &l, 1.0f, &a, &b); AE_NEAR(a, 11.3, 1.0); AE_NEAR(b, 11.3, 1.0); AE_NEAR(a, b, 0.6);
	AE_CHECK(l.number_shown && l.x_mark_shown && l.glyph_shown);
	AE_NEAR(l.digit_baseline_y, 83 + 30, 0.01); AE_NEAR(l.digit_cap, 24, 0.01); AE_NEAR(l.digit_cell, 19.2, 0.01);
	AE_NEAR(l.glyph_height, 23, 0.01); AE_NEAR(l.glyph_y + l.glyph_height, 113, 0.01);
	AE_NEAR(l.number_cell_x[1] - l.number_cell_x[0], 19.2, 0.01);
	weapon(&m, _ae_hint_pistol, 12, 12, 36); ae_panel_layout_build(&m, &metrics, &v, &l);
	number_padding(&m, &l, 1.0f, &a, &b); AE_NEAR(a, 8.6, 1.0); AE_NEAR(b, 8.6, 1.0);
	weapon(&m, _ae_hint_pistol, 12, 12, 4); ae_panel_layout_build(&m, &metrics, &v, &l);
	number_padding(&m, &l, 1.0f, &a, &b); AE_NEAR(a, 8.6, 1.0); AE_NEAR(b, 8.6, 1.0);
	energy(&m, 0.87f); ae_panel_layout_build(&m, &metrics, &v, &l);
	number_padding(&m, &l, 1.0f, &a, &b); AE_NEAR(a, 2.6, 1.0); AE_NEAR(b, 2.6, 1.0);   /* (rev5's box: tight) */
	AE_CHECK(!l.x_mark_shown && l.glyph_shown); AE_NEAR(l.glyph_height, 20, 0.01);
	weapon(&m, _ae_hint_flamethrower, 100, 64, 200); ae_panel_layout_build(&m, &metrics, &v, &l);
	number_padding(&m, &l, 1.0f, &a, &b); AE_NEAR(a, 32.2, 1.0); AE_NEAR(b, 32.2, 1.0); AE_NEAR(a, b, 0.6);
	AE_CHECK(!l.x_mark_shown && !l.glyph_shown && l.bar_shown && l.round_count == 0);
}

static void test_boxes_1p(void)
{
	struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v;
	real mid, inner, ink_left, ink_right, inset;

	view_1p(&v);
	weapon(&m, _ae_hint_assault_rifle, 60, 60, 180); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_NEAR(l.left_line_x0, 150 + 19.2, 0.01); AE_NEAR(l.left_line_y0, 83 + 36, 0.01);
	AE_NEAR(l.right_line_x0 - l.left_line_x0, 225, 0.01);   /* rev5: CE's width (owner 10-15) */
	AE_NEAR(l.number_box.x1 - l.number_box.x0, 121, 0.01); AE_NEAR(l.grenade_box.x1 - l.grenade_box.x0, 99, 0.01);
	AE_NEAR(l.grenade_box.x0 - l.number_box.x1, 5, 0.01);                  /* the gap between the boxes */
	AE_NEAR(l.grenade_box.x1, l.right_line_x0, 0.01);
	AE_NEAR(l.number_box.y0, 83, 0.01); AE_NEAR(l.number_box.y1, 119, 0.01);
	AE_NEAR(l.number_box.lean, AE_PANEL_LEAN, 0.0001);

	/* the grenade box: frag ink 8 px from the inner edge; a "1" count ~12 px from the right edge */
	mid = l.grenade_box.y1 - 18.0f; inner = l.grenade_box.x0 + AE_PANEL_LEAN * 18.0f + 1.5f;
	AE_NEAR(l.frag_x + metrics.frag_mid_left * l.frag_height - inner, 8, 0.5);
	AE_NEAR(l.frag_height, 24, 0.01); AE_NEAR(l.frag_y + l.frag_height, 113, 0.01);
	AE_NEAR(l.count_cell_x[0] - inner, 31, 0.01); AE_NEAR(l.count_cell_x[1] - inner, 73, 0.01);
	AE_NEAR(l.plasma_cx - inner, 61.5, 0.01); AE_NEAR(l.plasma_diameter, 17, 0.01); AE_NEAR(l.plasma_cy, mid, 0.01);
	inset = metrics.digit_lean * l.digit_cap / 2.0f;
	digits_ink(m.grenade[1].count, l.digit_cap, l.digit_cell, &ink_left, &ink_right);   /* "1" */
	AE_NEAR(inner + 96 - (l.count_ink_x[1] + ink_right - inset), 12.4, 1.0);
	/* each count's ink centred in its cell */
	digits_ink(m.grenade[0].count, l.digit_cap, l.digit_cell, &ink_left, &ink_right);   /* "2" */
	AE_NEAR(l.count_ink_x[0] + (ink_left + ink_right) / 2.0f, l.count_cell_x[0] + 8.5, 0.01);
}

static void test_magazine_1p(void)
{
	struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v;
	short i, row;

	view_1p(&v);
	weapon(&m, _ae_hint_assault_rifle, 60, 9, 180); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 60);
	AE_NEAR(l.round[0].y, 83 + 36 + 10, 0.01);                              /* boxes -> magazine 10 px */
	AE_NEAR(l.round[0].height + 2, 18.4, 1.0);                              /* rev5: CE's own size */
	AE_NEAR(l.round[1].x - l.round[0].x, 11.6, 0.1);                        /* the pitch */
	for (row = 0; row < 3; row++)
	{
		const real bottom = l.round[row * 20].y + l.round[row * 20].height;
		AE_NEAR(l.round[row * 20].x, left_line(&l, bottom), 0.01);          /* starts on the left lean line */
		/* the last round ENDS on the right line (at its top: the round leans) */
		AE_NEAR(l.round[row * 20 + 19].x + l.round_width, left_line(&l, bottom) + 225 + AE_PANEL_LEAN *
			l.round[0].height, 0.01);
		if (row)
			AE_NEAR(l.round[row * 20].y - (l.round[row * 20 - 1].y + l.round[0].height), 4, 0.01);  /* rows 4 apart */
		for (i = 0; i < 20; i++)
			AE_CHECK(l.round[row * 20 + i].lit == (row == 0 && i < 9));     /* 9 of 60: the top row's first 9 (CE's) */
	}
	/* sniper: lying, left-aligned, CE's spacing */
	weapon(&m, _ae_hint_sniper, 4, 3, 8); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 4); AE_CHECK(l.round[0].height <= 30.01f);
	AE_NEAR(l.round[1].x - l.round[0].x, l.round_width * metrics.art[_ae_art_sniper].pitch, 0.01);
	AE_CHECK(l.round[0].lit && l.round[1].lit && l.round[2].lit && !l.round[3].lit);   /* the bottom row's right */
	AE_CHECK(l.round[1].x + l.round_width <= left_line(&l, l.round[1].y + l.round[1].height) + 225 +
		AE_PANEL_LEAN * l.round[1].height + 0.01f);
	/* rockets: 2 lying, left-aligned, never past the right line */
	weapon(&m, _ae_hint_rocket, 2, 2, 4); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 2 && l.round[0].height <= 30.01f && l.round[1].x > l.round[0].x);
	AE_CHECK(l.round[1].x + l.round_width <= l.right_line_x0 + AE_PANEL_LEAN * 36 + 0.01f);
	/* the fuel rod's 3 + 2 */
	weapon(&m, _ae_hint_fuel_rod_rounds, 5, 5, 4); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 5); AE_CHECK(l.round[3].y > l.round[2].y);
	AE_CHECK(l.round[2].x + l.round_width <= left_line(&l, l.round[2].y + l.round[2].height) + 225 +
		AE_PANEL_LEAN * l.round[2].height + 0.01f);
	/* the heat bar: lean line to lean line, 225 x 12 */
	energy(&m, 0.5f); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.bar_shown); AE_NEAR(l.bar.x1 - l.bar.x0, 225, 0.01); AE_NEAR(l.bar.y1 - l.bar.y0, 12, 0.01);
	AE_NEAR(l.bar.x0, left_line(&l, l.bar.y1), 0.01); AE_NEAR(l.bar.y0, 83 + 46, 0.01);
}

static void test_shield_group_1p(void)
{
	struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v; struct ae_panel_snapshot s;
	real cells_bottom;
	short i;

	view_1p(&v);
	memset(&s, 0, sizeof(s)); s.has_weapon = TRUE; s.weapon_uses_rounds = TRUE; s.weapon_type_hint = _ae_hint_pistol;
	s.rounds_per_magazine = 12; s.shield = 1; s.health = 1; s.camo_ticks = 900; s.overshield_seconds = 40;
	ae_panel_model_build(&s, &m); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_NEAR(ae_panel_right_column_x(&v), 1758, 0.001);
	AE_NEAR(l.shield_bar.x1, 1758, 0.01); AE_NEAR(l.shield_bar.x1 - l.shield_bar.x0, 236, 0.01);
	AE_NEAR(l.shield_bar.y1 - l.shield_bar.y0, 20, 0.01); AE_NEAR(l.shield_bar.lean, -AE_PANEL_LEAN, 0.0001);
	AE_NEAR(l.shield_icon_height, 25, 0.01);
	AE_NEAR(l.shield_icon_y + 12.5, 83 + 10, 0.01);                          /* centred on the bar's height */
	AE_NEAR(l.shield_bar.x0 - AE_PANEL_LEAN * 10 - (l.shield_icon_x + 25 * metrics.shield_aspect), 7, 0.01);
	AE_CHECK(l.buff_count == 2);
	for (i = 0; i < 2; i++)
		AE_NEAR(l.buff[i].time_right_x, 1758, 1.0);                         /* one right column */
	AE_NEAR(l.buff[1].baseline_y - l.buff[0].baseline_y, 20, 0.01);
	AE_NEAR(l.buff[0].px, 14, 0.01);
	AE_NEAR(l.buff[0].time_right_x - l.buff[0].name_right_x, text_width(L"0:00", 14) + 8, 0.01);
	/* health: 8 cells 10 tall, 6 under the bar, over 170 px; the last ending under the bar's bottom-right corner */
	AE_NEAR(l.health_cell[0].y0, 83 + 26, 0.01); AE_NEAR(l.health_cell[0].y1, 83 + 36, 0.01);
	AE_NEAR(l.health_cell[7].x1, 1758, 0.01);
	AE_NEAR(l.health_cell[1].x0 - l.health_cell[0].x0, 170.0 / 8, 0.01);
	AE_NEAR(l.health_cell[0].x1 - l.health_cell[0].x0, 170.0 / 8 * 0.72, 0.01);
	/* health -> the first buff's cap top: 8 px */
	cells_bottom = l.health_cell[0].y1;
	AE_NEAR(l.buff[0].baseline_y - 0.74 * l.buff[0].px - cells_bottom, 8, 1.0);
	/* the cross (G1): 12 px, centred on the cell row, 8 px from the first cell */
	AE_NEAR(l.cross_size, 12, 0.01); AE_NEAR(l.cross_y + l.cross_size / 2, 83 + 31, 0.51);
	AE_NEAR(l.health_cell[0].x0 - AE_PANEL_LEAN * 5 - (l.cross_x + l.cross_size), 8, 0.51);
	AE_CHECK(l.cross_x == floorf(l.cross_x) && l.cross_y == floorf(l.cross_y));  /* pixel-snapped */
	/* the bar ends on the right column (and so the timers list) */
	AE_NEAR(l.shield_group.x1, 1758, 1.0);
}

static void test_quarter_view(void)
{
	struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v; struct ae_panel_snapshot s;
	real block_bottom, gap;

	view_4p(&v);
	memset(&s, 0, sizeof(s)); s.has_weapon = TRUE; s.weapon_uses_rounds = TRUE;
	s.weapon_type_hint = _ae_hint_assault_rifle; s.rounds_per_magazine = 60; s.rounds_loaded = 60; s.rounds_reserve = 180;
	s.shield = 1; s.health = 1; s.camo_ticks = 900;
	ae_panel_model_build(&s, &m); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_NEAR(l.grenade_box.x1 - l.number_box.x0, 168.75, 0.5);              /* group 225 x 0.75 */
	AE_NEAR(l.number_box.y1 - l.number_box.y0, 27, 0.01);                   /* boxes 27 */
	AE_NEAR(l.digit_cap, 18, 0.01);                                          /* digits 18 */
	block_bottom = l.round[59].y + l.round[59].height;
	/* the AR block's bottom: rev5's smaller rounds */
	AE_NEAR(block_bottom, 116.45, 1.0);
	AE_CHECK(l.cross_size >= 9.0f);                                          /* G1 */
	AE_NEAR(l.shield_bar.x1 - l.shield_bar.x0, 177, 0.01); AE_NEAR(l.shield_bar.y1 - l.shield_bar.y0, 15, 0.01);
	gap = l.buff[0].baseline_y - 0.74f * l.buff[0].px - l.health_cell[0].y1;
	AE_CHECK(gap >= 5.99f);                                                  /* 6 px on screen */
	AE_NEAR(l.buff[0].px, 14, 0.01);                                         /* the quarter-view floor */
	AE_NEAR(l.buff[0].time_right_x, 856, 1.0);
}

static void test_small_views(void)
{
	struct ae_panel_view v;
	view(&v, 0.4f, 60, 20, 500, 20, 640, 360);
	AE_NEAR(ae_panel_label_px(1.0f), 14, 0.001); AE_NEAR(ae_panel_label_px(0.85f), 12, 0.001);
	AE_NEAR(ae_panel_label_px(0.75f), 14, 0.001); AE_NEAR(ae_panel_label_px(1.5f), 21, 0.001);
	{
		struct ae_panel_model m; struct ae_panel_layout l;
		weapon(&m, _ae_hint_assault_rifle, 60, 60, 180); ae_panel_layout_build(&m, &metrics, &v, &l);
		AE_CHECK(l.cross_size >= 9.0f); AE_CHECK(l.digit_cap >= 12.0f);       /* the floors */
		AE_NEAR(fmod(l.cross_size, 3.0), 0, 0.001);                          /* arms a third */
	}
}

/* Review Focus 6: odd magazines */
static void test_odd_magazines(void)
{
	struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v; short i;

	view_1p(&v);
	weapon(&m, _ae_hint_none, 61, 30, 4); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 0 && l.bar_shown); AE_CHECK(l.bar.x1 - l.bar.x0 <= 301.01f);
	weapon(&m, _ae_hint_none, 250, 250, 4); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 0 && l.bar_shown);
	weapon(&m, _ae_hint_none, 1, 1, 4); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 1 && l.round[0].lit);
	AE_CHECK(l.round[0].x + l.round_width <= l.right_line_x0 + AE_PANEL_LEAN * 36 + 0.01f);
	weapon(&m, _ae_hint_none, 0, 0, 12); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.round_count == 0 && !l.bar_shown && l.number_shown);
	/* every magazine 1 .. 60 stays inside the group's width */
	for (i = 1; i <= 60; i++)
	{
		short r;
		weapon(&m, _ae_hint_none, i, i, 4); ae_panel_layout_build(&m, &metrics, &v, &l);
		AE_CHECK(l.round_count == i);
		for (r = 0; r < l.round_count; r++)
		{
			real bottom = l.round[r].y + l.round[r].height;
			if (l.round[r].x < left_line(&l, bottom) - 0.01f ||
				l.round[r].x + l.round_width > left_line(&l, bottom) + 225 + AE_PANEL_LEAN * l.round[r].height + 0.01f)
			{
				printf("FAIL magazine %d round %d outside the group\n", i, r);
				failures++;
				break;
			}
		}
	}
	/* no weapon: nothing in the ammo group */
	{
		struct ae_panel_snapshot s; memset(&s, 0, sizeof(s)); s.shield = 1; s.health = 1;
		ae_panel_model_build(&s, &m); ae_panel_layout_build(&m, &metrics, &v, &l);
		AE_CHECK(!l.ammo_group_shown && l.round_count == 0 && !l.number_shown && !l.bar_shown);
	}
}

/* a vehicle seat's rows and labels (owner 10-15: each label trails its own row, rev13's look, over G6's shared x) */
static void test_seats(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v;

	view_1p(&v);
	memset(&s, 0, sizeof(s)); s.shield = 1; s.health = 1; s.in_seat = TRUE; s.weapon_type_hint = _ae_hint_none;
	s.seat_row_count = 2; s.seat_rows[0].kind = _ae_seat_cannon; s.seat_rows[1].kind = _ae_seat_mg;
	ae_panel_model_build(&s, &m); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_CHECK(l.seat_count == 2 && l.seat[0].shell && !l.seat[1].shell);
	AE_NEAR(l.seat[0].shell_y, 83, 0.01); AE_NEAR(l.seat[0].shell_height, 30, 0.01);
	AE_NEAR(l.seat[0].shell_x, left_line(&l, 83 + 30), 0.01);
	AE_NEAR(l.seat[1].bar.y0, 83 + 30 + 8, 0.01); AE_NEAR(l.seat[1].bar.x1 - l.seat[1].bar.x0, 225, 0.01);
	AE_NEAR(l.seat[0].label_x, l.seat[0].shell_x + l.round_width + 8, 0.01);   /* CANNON 8 px after its shell */
	AE_CHECK(l.seat[0].label_x < l.seat[1].label_x);
	AE_NEAR(l.seat[1].label_x, l.seat[1].bar.x1 + AE_PANEL_LEAN * 12 + 8, 0.01);
	AE_NEAR(l.seat[0].label_px, 14, 0.01);
	AE_CHECK(!l.number_shown && l.round_count == 0);
	/* Banshee: bolts + bomb, 8 px apart */
	s.seat_rows[0].kind = _ae_seat_bolts; s.seat_rows[1].kind = _ae_seat_bomb;
	ae_panel_model_build(&s, &m); ae_panel_layout_build(&m, &metrics, &v, &l);
	AE_NEAR(l.seat[0].bar.y0, 83, 0.01); AE_NEAR(l.seat[1].bar.y0, 83 + 12 + 8, 0.01);
	AE_NEAR(l.seat[0].label_x - l.seat[1].label_x, AE_PANEL_LEAN * (12 + 8), 0.01);   /* BOMB follows the lean */
}

/* Review Focus 10: no rectangle leaves the view, at 1p, 2p, 4p and 21:9 */
static int inside(const struct ae_panel_rect *r, const struct ae_panel_view *v)
{
	return r->x0 >= -0.01f && r->y0 >= -0.01f && r->x1 <= v->view_width + 0.01f && r->y1 <= v->view_height + 0.01f;
}

static void check_inside(const struct ae_panel_layout *l, const struct ae_panel_view *v, const char *what)
{
	struct ae_panel_rect r;
	short i;
	int ok = 1;

	ae_panel_para_bounds(&l->number_box, &r); ok &= inside(&r, v);
	ae_panel_para_bounds(&l->grenade_box, &r); ok &= inside(&r, v);
	ae_panel_para_bounds(&l->shield_bar, &r); ok &= inside(&r, v);
	for (i = 0; i < AE_PANEL_HEALTH_CELLS; i++) { ae_panel_para_bounds(&l->health_cell[i], &r); ok &= inside(&r, v); }
	ok &= inside(&l->backing_ammo, v); ok &= inside(&l->backing_shield, v);
	ok &= inside(&l->ammo_group, v); ok &= inside(&l->shield_group, v);
	for (i = 0; i < l->round_count; i++)
	{
		r.x0 = l->round[i].x; r.y0 = l->round[i].y; r.x1 = r.x0 + l->round_width; r.y1 = r.y0 + l->round[i].height;
		ok &= inside(&r, v);
	}
	r.x0 = l->shield_icon_x; r.y0 = l->shield_icon_y; r.x1 = r.x0 + 1; r.y1 = r.y0 + l->shield_icon_height;
	ok &= inside(&r, v);
	r.x0 = l->cross_x; r.y0 = l->cross_y; r.x1 = r.x0 + l->cross_size; r.y1 = r.y0 + l->cross_size; ok &= inside(&r, v);
	checks++;
	if (!ok) { printf("FAIL %s: a part leaves the view\n", what); failures++; }
}

static void test_inside_views(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m; struct ae_panel_layout l; struct ae_panel_view v;
	static const struct { real s, ox, oy, rx, ry, w, h; const char *what; } views[] = {
		{ 1.0f, 150, 83, 1758, 83, 1920, 1080, "1p" },
		{ 1.0f, 199, 83, 2348, 83, 2560, 1080, "21:9" },
		{ 0.85f, 172, 39, 1745, 39, 1920, 540, "2p" },
		{ 0.75f, 96, 39, 856, 39, 960, 540, "4p" },
	};
	int i;

	memset(&s, 0, sizeof(s)); s.has_weapon = TRUE; s.weapon_uses_rounds = TRUE;
	s.weapon_type_hint = _ae_hint_assault_rifle; s.rounds_per_magazine = 60; s.rounds_loaded = 30; s.rounds_reserve = 180;
	s.shield = 2.5f; s.health = 1; s.camo_ticks = 900; s.overshield_seconds = 50;
	ae_panel_model_build(&s, &m);
	for (i = 0; i < (int)(sizeof(views) / sizeof(views[0])); i++)
	{
		view(&v, views[i].s, views[i].ox, views[i].oy, views[i].rx, views[i].ry, views[i].w, views[i].h);
		ae_panel_layout_build(&m, &metrics, &v, &l);
		check_inside(&l, &v, views[i].what);
	}
}

int main(void)
{
	ae_panel_metrics_ce(&metrics);
	metrics.digits_ink = digits_ink;
	metrics.text_width = text_width;
	test_number_box_1p();
	test_boxes_1p();
	test_magazine_1p();
	test_shield_group_1p();
	test_quarter_view();
	test_small_views();
	test_odd_magazines();
	test_seats();
	test_inside_views();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
