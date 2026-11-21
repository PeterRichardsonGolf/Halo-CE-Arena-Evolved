/* The score strip's layout (port/linux/game/ae_strip_layout.c) with fixed-width fake metrics: one bottom line
(11.12, G5), the gap rule (bars shrink first, then the strip scales), every mode's places, the text floors, and
Review Focus 4 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ae_strip_layout.h"

static int failures;
static int checks;
#define AE_CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } \
	while (0)
#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 0.01f)

static real fake_digits(const wchar_t *t, real cap) { return (real)wcslen(t) * 0.6f * cap; }
static real fake_text(const wchar_t *t, real px) { return (real)wcslen(t) * 0.55f * px; }

static real bottom(const struct ae_strip_box *b) { return b->y + b->h; }
static real right(const struct ae_strip_box *b) { return b->x + b->w; }

static void team_slayer(struct ae_strip_snapshot *s)
{
	memset(s, 0, sizeof(*s));
	s->mode = _ae_strip_slayer; s->teams = TRUE; s->score_to_win = 50; s->clock_seconds = 102;
	s->me.team_index = 1; s->me.score = 17; s->other.team_index = 0; s->other.score = 10;
	s->hill_move_seconds = -1; s->next_flag_meters = -1; s->respawns_left = -1;
}

static void ctf(struct ae_strip_snapshot *s)
{
	team_slayer(s);
	s->mode = _ae_strip_ctf; s->score_to_win = 3; s->me.score = 2; s->other.score = 1;
	s->me.flag = _ae_flag_dropped; s->me.flag_return_seconds = 18;
	s->other.flag = _ae_flag_carried; wcscpy(s->other.flag_carrier, L"bot5");
}

static void lay(const struct ae_strip_snapshot *s, const struct ae_strip_geometry *g, struct ae_strip_model *m,
	struct ae_strip_layout *l)
{
	ae_strip_model_build(s, m);
	ae_strip_layout_compute(m, g, fake_digits, fake_text, l);
}

static void test_1p_team_slayer_on_shared_line(void)
{
	struct ae_strip_snapshot s = {0}; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 1.0f, 998.0f, 1600.0f, 960.0f };
	s.mode = _ae_strip_slayer; s.teams = TRUE; s.score_to_win = 50; s.clock_seconds = 102;
	s.me.team_index = 1; s.me.score = 17; s.other.team_index = 0; s.other.score = 10;
	ae_strip_model_build(&s, &m);
	ae_strip_layout_compute(&m, &g, fake_digits, fake_text, &l);
	AE_CHECK(l.goal.visible && l.goal.y + l.goal.h <= 998.0f + 1.0f && l.goal.y + l.goal.h >= 998.0f - 3.0f);
	AE_CHECK(l.left_bar.visible && l.right_bar.visible);
	AE_CHECK(l.right_bar.x + l.right_bar.w <= 1600.0f - 48.0f);
	AE_CHECK(l.left_score.x + l.left_score.w <= l.clock.x - 29.0f);
	/* (the spec's numbers: score baseline 998 - 19, full bars 16 beyond the scores, 9 tall; 1:42's cell = "0:00") */
	AE_CHECK(NEAR(l.score_baseline_y, 979.0f) && NEAR(l.clock_baseline_y, 979.0f));
	AE_CHECK(NEAR(l.strip_scale, 1.0f) && NEAR(l.bar_length, 150.0f) && NEAR(l.left_bar.w, 150.0f));
	AE_CHECK(NEAR(l.clock.w, fake_digits(L"0:00", 20.0f)) && NEAR(l.clock.x + l.clock.w / 2.0f, 960.0f));
	AE_CHECK(NEAR(l.clock.h, 20.0f) && NEAR(l.left_score.h, 24.0f));
	AE_CHECK(NEAR(l.left_score.x + l.left_score.w, l.clock.x - 30.0f) && NEAR(l.right_score.x, right(&l.clock) + 30.0f));
	AE_CHECK(NEAR(right(&l.left_bar), l.left_score.x - 16.0f) && NEAR(l.right_bar.x, right(&l.right_score) + 16.0f));
	AE_CHECK(NEAR(bottom(&l.left_bar), 979.0f - 7.5f) && NEAR(l.left_bar.h, 9.0f));
	/* (the lead centred 40 above the baseline; no context, no subs) */
	AE_CHECK(l.lead.visible && NEAR(bottom(&l.lead), 939.0f) && NEAR(l.lead.x + l.lead.w / 2.0f, 960.0f));
	AE_CHECK(!l.context.visible && !l.left_sub.visible && !l.right_sub.visible);
	/* (the marker's tip 31 above the baseline, over my score's middle) */
	AE_CHECK(l.marker.visible && NEAR(bottom(&l.marker), 948.0f) && NEAR(l.marker.w, 13.0f));
	AE_CHECK(NEAR(l.marker.x + l.marker.w / 2.0f, l.left_score.x + l.left_score.w / 2.0f));
	/* (the backing centred on the strip, 6 above the baseline, past its ends) */
	AE_CHECK(NEAR(l.backing_cx, 960.0f) && NEAR(l.backing_cy, 973.0f) && NEAR(l.backing_ry, 46.0f));
	AE_CHECK(l.backing_rx >= right(&l.right_bar) - 960.0f);
	AE_CHECK(NEAR(l.right_end_x, right(&l.right_bar)) && NEAR(l.left_end_x, l.left_bar.x));
	AE_CHECK(!l.left_pips.visible && !l.left_cells.visible && !l.left_alive.visible);
}

static void test_4p_gap_shrinks_bars_then_scales(void)   /* Review Focus 4 */
{
	struct ae_strip_snapshot s = {0}; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 960, 540, 0.75f, 499.0f, 700.0f, 480.0f };
	s.mode = _ae_strip_ctf; s.teams = TRUE; s.score_to_win = 3;
	s.me.team_index = 1; s.me.score = 2; s.other.team_index = 0; s.other.score = 1;
	s.other.flag = _ae_flag_carried; wcscpy(s.other.flag_carrier, L"averylongname15");
	ae_strip_model_build(&s, &m);
	ae_strip_layout_compute(&m, &g, fake_digits, fake_text, &l);
	AE_CHECK(l.right_bar.x + l.right_bar.w <= 700.0f - 48.0f * 0.75f + 0.5f);
	AE_CHECK(l.bar_length >= 60.0f * 0.75f - 0.01f);
	AE_CHECK(l.right_sub.x + l.right_sub.w <= 700.0f - 48.0f * 0.75f + 0.5f);
}

static void test_gap_rule_steps(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 960, 540, 0.75f, 499.0f, 900.0f, 480.0f };
	real limit;

	team_slayer(&s);
	/* (room: full bars at the view factor) */
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.strip_scale, 0.75f) && NEAR(l.bar_length, 150.0f * 0.75f));
	/* (less room: the bars shrink, the strip keeps its scale) */
	g.list_left_x = 640.0f; limit = 640.0f - 36.0f;
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.strip_scale, 0.75f) && l.bar_length < 150.0f * 0.75f && l.bar_length > 60.0f * 0.75f);
	AE_CHECK(l.right_end_x <= limit + 0.01f && l.right_end_x >= limit - 0.5f);
	/* (less again: bars at 60, then the whole strip scales) */
	g.list_left_x = 600.0f; limit = 600.0f - 36.0f;
	lay(&s, &g, &m, &l);
	AE_CHECK(l.strip_scale < 0.75f && NEAR(l.bar_length, 60.0f * l.strip_scale));
	AE_CHECK(l.right_end_x <= limit + 0.01f);
	AE_CHECK(NEAR(l.right_bar.w, l.bar_length) && NEAR(l.left_bar.w, l.bar_length));
	/* (the lowest line stays on the shared line at the smaller scale) */
	AE_CHECK(NEAR(bottom(&l.goal), 499.0f));
	/* (no room at all: never under half the view factor; never a negative scale) */
	g.list_left_x = 400.0f;
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.strip_scale, 0.75f * 0.5f));
}

static void test_timed_mode_small_clock(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 1.0f, 998.0f, 1700.0f, 960.0f };

	/* (team king: no sub-lines, so "TO 5:00" 10 under the scores is the lowest line) */
	team_slayer(&s);
	s.mode = _ae_strip_king; s.score_to_win = 300; s.clock_seconds = 370; s.me.score = 52; s.other.score = 35;
	s.hill = _ae_hill_yours;
	lay(&s, &g, &m, &l);
	AE_CHECK(l.clock.visible && NEAR(l.clock.h, 15.0f) && NEAR(l.clock.w, fake_text(L"0:00", 15.0f)));
	AE_CHECK(NEAR(l.score_baseline_y, 988.0f) && NEAR(l.clock_baseline_y, 982.0f) && NEAR(bottom(&l.clock), 982.0f));
	AE_CHECK(l.goal.visible && NEAR(bottom(&l.goal), 998.0f));
	AE_CHECK(l.context.visible && NEAR(bottom(&l.context), 948.0f) && !l.lead.visible);
	AE_CHECK(l.left_bar.visible && NEAR(l.left_score.w, fake_digits(L"0:52", 24.0f)));
	/* (team oddball: "YOU 0:31" under my score; "TO 5:00" joins it on the line, G5) */
	team_slayer(&s);
	s.mode = _ae_strip_oddball; s.score_to_win = 300; s.me.score = 72; s.other.score = 48; s.own_score = 31;
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.score_baseline_y, 976.0f) && NEAR(bottom(&l.goal), 998.0f) && NEAR(bottom(&l.left_sub), 998.0f));
}

static void test_race_closes_up_without_bars(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 1.0f, 998.0f, 1700.0f, 960.0f };

	team_slayer(&s);
	s.teams = FALSE; s.mode = _ae_strip_race; s.score_to_win = 0; s.me.place = 2; s.me.score = 2; s.other.score = 3;
	s.laps = 3; s.checkpoint = 7; s.checkpoints = 9; wcscpy(s.other.name, L"bot1");
	lay(&s, &g, &m, &l);
	AE_CHECK(!l.left_bar.visible && !l.right_bar.visible && !l.goal.visible);
	AE_CHECK(l.left_sub.visible && l.right_sub.visible && NEAR(bottom(&l.left_sub), 998.0f));
	/* (my place: the digit at 24 + 2 px + "ND" at 15 px; the leader's name at 18 px) */
	AE_CHECK(NEAR(l.left_score.w, fake_digits(L"2", 24.0f) + 2.0f + fake_text(L"ND", 15.0f)));
	AE_CHECK(NEAR(l.right_score.w, fake_text(L"bot1", 18.0f)) && NEAR(l.right_score.h, 18.0f));
	AE_CHECK(NEAR(l.clock.h, 15.0f));
	/* (closed up: the strip spans the scores and the subs, about 300 px at 1p) */
	AE_CHECK(l.right_end_x - l.left_end_x < 300.0f);
	AE_CHECK(NEAR(right(&l.left_sub), l.left_score.x + l.left_score.w) && NEAR(l.right_sub.x, l.right_score.x));
}

static void test_rounds_pips_and_cells(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 1.0f, 998.0f, 1700.0f, 960.0f };

	team_slayer(&s);
	s.mode = _ae_strip_rounds; s.rounds_to_win = 5; s.me.rounds_won = 2; s.other.rounds_won = 1;
	s.me.players = 4; s.me.alive = 3; s.other.players = 3; s.other.alive = 2; s.respawns_left = 15;
	lay(&s, &g, &m, &l);
	AE_CHECK(l.left_pips.visible && l.right_pips.visible && l.left_cells.visible && l.right_cells.visible);
	/* (pips 17 under the scores, the lowest line; cells 15 each where the bars would be) */
	AE_CHECK(NEAR(l.score_baseline_y, 998.0f - 17.0f) && NEAR(bottom(&l.left_pips), 998.0f));
	AE_CHECK(NEAR(l.left_pips.x + l.left_pips.w / 2.0f, l.left_score.x + l.left_score.w / 2.0f));
	AE_CHECK(NEAR(l.left_cells.w, 60.0f) && NEAR(l.right_cells.w, 45.0f));
	AE_CHECK(NEAR(right(&l.left_cells), l.left_score.x - 16.0f) && NEAR(l.right_cells.x, right(&l.right_score) + 16.0f));
	AE_CHECK(!l.left_bar.visible && !l.left_alive.visible);
	/* (over 4 a side: "6 ALIVE" in the cells' place, at 16 px, 6 under the bar's centre) */
	s.me.players = 8; s.me.alive = 6;
	lay(&s, &g, &m, &l);
	AE_CHECK(!l.left_cells.visible && l.left_alive.visible && l.right_alive.visible);
	AE_CHECK(NEAR(l.left_alive.w, fake_text(L"6 ALIVE", 16.0f)) && NEAR(right(&l.left_alive), l.left_score.x - 16.0f));
	AE_CHECK(NEAR(bottom(&l.left_alive), l.score_baseline_y - 6.0f) && NEAR(l.right_alive.x, right(&l.right_score) + 16.0f));
}

static void test_other_views(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry half = { 0, 0, 1920, 540, 0.85f, 499.0f, 1745.0f, 960.0f };
	struct ae_strip_geometry wide = { 0, 0, 2560, 1080, 1.0f, 998.0f, 2348.0f, 1280.0f };
	struct ae_strip_geometry lower = { 0, 540, 1920, 540, 0.85f, 1039.0f, 1745.0f, 960.0f };

	team_slayer(&s);
	/* (2p: the spec's 482.8 score baseline; the strip at the view factor; text at 0.85 over the floor) */
	lay(&s, &half, &m, &l);
	AE_CHECK(NEAR(l.strip_scale, 0.85f) && NEAR(l.score_baseline_y, 499.0f - 19.0f * 0.85f));
	AE_CHECK(NEAR(l.left_score.h, 24.0f * 0.85f) && NEAR(l.clock.h, 20.0f * 0.85f));
	AE_CHECK(NEAR(l.lead.h, 16.0f * 0.85f) && NEAR(l.goal.h, 12.0f));
	AE_CHECK(NEAR(l.bar_length, 150.0f * 0.85f) && right(&l.right_bar) < 1745.0f - 48.0f * 0.85f);
	/* (the lower view: the same strip, its own line) */
	lay(&s, &lower, &m, &l);
	AE_CHECK(NEAR(bottom(&l.goal), 1039.0f));
	/* (21:9: centred on the HUD area's centre, as 1p) */
	lay(&s, &wide, &m, &l);
	AE_CHECK(NEAR(l.strip_scale, 1.0f) && NEAR(l.backing_cx, 1280.0f) && NEAR(l.clock.x + l.clock.w / 2.0f, 1280.0f));
	AE_CHECK(NEAR(1280.0f - l.left_end_x, l.right_end_x - 1280.0f));
}

static void test_one_baseline_g5(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry views[] = {
		{ 0, 0, 1920, 1080, 1.0f, 998.0f, 1600.0f, 960.0f },
		{ 0, 0, 1920, 540, 0.85f, 499.0f, 1700.0f, 960.0f },
		{ 0, 0, 960, 540, 0.75f, 499.0f, 740.0f, 480.0f } };
	unsigned i;

	ctf(&s);
	for (i = 0; i < sizeof(views) / sizeof(views[0]); i++)
	{
		lay(&s, &views[i], &m, &l);
		AE_CHECK(l.goal.visible && l.left_sub.visible && l.right_sub.visible);
		AE_CHECK(NEAR(bottom(&l.goal), views[i].shared_line_y) && NEAR(bottom(&l.left_sub), views[i].shared_line_y) &&
			NEAR(bottom(&l.right_sub), views[i].shared_line_y));
		AE_CHECK(NEAR(l.score_baseline_y, views[i].shared_line_y - 22.0f * l.strip_scale));
		/* (a flag line holds its glyph and a gap before the text) */
		AE_CHECK(NEAR(l.left_sub.w, fake_text(L"DROPPED 0:18", l.left_sub.h) + 1.3f * l.left_sub.h));
		/* (sub-lines from the scores' inner edges outward: never into the goal line) */
		AE_CHECK(right(&l.left_sub) <= l.goal.x && l.right_sub.x >= right(&l.goal));
	}
	/* (the spec's CTF baselines: 976, 480.2, 482.5) */
	lay(&s, &views[0], &m, &l); AE_CHECK(NEAR(l.score_baseline_y, 976.0f));
	lay(&s, &views[1], &m, &l); AE_CHECK(NEAR(l.score_baseline_y, 480.3f));
	lay(&s, &views[2], &m, &l); AE_CHECK(NEAR(l.score_baseline_y, 482.5f));
}

static void test_no_limit_score_is_lowest_line(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 1.0f, 998.0f, 1700.0f, 960.0f };

	team_slayer(&s); s.score_to_win = 0;
	lay(&s, &g, &m, &l);
	AE_CHECK(!l.goal.visible && !l.left_sub.visible && !l.right_sub.visible && !l.left_bar.visible);
	AE_CHECK(NEAR(l.score_baseline_y, 998.0f));
	/* (FFA with no limit: the sub-lines are the lowest) */
	s.teams = FALSE; s.me.place = 2; wcscpy(s.other.name, L"bot3");
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.score_baseline_y, 976.0f) && NEAR(bottom(&l.right_sub), 998.0f));
}

static void test_quarter_view_floors_and_long_names(void)   /* Review Focus 4 */
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 960, 540, 0.75f, 499.0f, 740.0f, 480.0f };
	real limit = 740.0f - 48.0f * 0.75f;

	team_slayer(&s);
	s.teams = FALSE; s.score_to_win = 25; s.clock_seconds = 725; s.me.score = 104; s.other.score = 99;
	s.me.place = 11; s.i_lead = TRUE; wcscpy(s.other.name, L"averylongname15");
	lay(&s, &g, &m, &l);
	/* (quarter views: small text at 14 px, digits never under 12 cap) */
	AE_CHECK(NEAR(l.right_sub.h, 14.0f) && NEAR(l.goal.h, 14.0f) && NEAR(l.lead.h, 14.0f));
	AE_CHECK(NEAR(l.right_sub.w, fake_text(L"2ND averylongname15", 14.0f)));
	AE_CHECK(l.left_score.h >= 12.0f && l.clock.h >= 12.0f);
	/* (the 12:05 cell, 3-digit scores: nothing past the list's gap, nothing overlapping) */
	AE_CHECK(NEAR(l.clock.w, fake_digits(L"00:00", ae_strip_digits_px(20.0f, l.strip_scale))));
	AE_CHECK(l.right_end_x <= limit + 0.5f);
	AE_CHECK(right(&l.right_sub) <= limit + 0.5f && right(&l.right_bar) <= limit + 0.5f);
	AE_CHECK(right(&l.left_score) <= l.clock.x && l.right_score.x >= right(&l.clock));
	AE_CHECK(right(&l.left_sub) <= l.goal.x && l.right_sub.x >= right(&l.goal));
	AE_CHECK(right(&l.left_bar) <= l.left_score.x && l.right_bar.x >= right(&l.right_score));
	/* (the floor helpers) */
	AE_CHECK(NEAR(ae_strip_text_px(14.0f, 0.75f, 0.75f), 14.0f) && NEAR(ae_strip_text_px(14.0f, 0.85f, 0.85f), 12.0f));
	AE_CHECK(NEAR(ae_strip_text_px(16.0f, 1.0f, 1.0f), 16.0f) && NEAR(ae_strip_text_px(16.0f, 2.0f, 2.0f), 32.0f));
	AE_CHECK(NEAR(ae_strip_digits_px(24.0f, 0.4f), 12.0f) && NEAR(ae_strip_digits_px(24.0f, 0.75f), 18.0f));
}

static void test_clock_cell(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 1.0f, 998.0f, 1700.0f, 960.0f };

	team_slayer(&s); s.clock_seconds = 7;
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.clock.w, fake_digits(L"0:00", 20.0f)));
	s.clock_seconds = 599;
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.clock.w, fake_digits(L"0:00", 20.0f)));
	s.clock_seconds = 600;
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.clock.w, fake_digits(L"00:00", 20.0f)));
	/* (NHE maps: no clock cell; the scores 30 from the centre) */
	s.clock_hidden = TRUE;
	lay(&s, &g, &m, &l);
	AE_CHECK(!l.clock.visible && NEAR(right(&l.left_score), 930.0f) && NEAR(l.right_score.x, 990.0f));
}

static void test_bad_input(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 0.0f, 998.0f, 1700.0f, 960.0f };

	team_slayer(&s);
	/* (a zero view factor reads as 1; no measure functions: nothing laid out, no crash) */
	lay(&s, &g, &m, &l);
	AE_CHECK(NEAR(l.strip_scale, 1.0f));
	memset(&l, 0x55, sizeof(l));
	ae_strip_layout_compute(&m, &g, NULL, fake_text, &l);
	AE_CHECK(!l.goal.visible && l.strip_scale == 0.0f);
	ae_strip_layout_compute(NULL, &g, fake_digits, fake_text, &l);
}

/* FFA's YOU colour (spec 11.4): my armour colour lifted to the tint rule, or white */
static real luminance(const struct ae_strip_rgb *c)
{
	real channel[3] = { c->red, c->green, c->blue };
	real linear[3];
	int i;

	for (i = 0; i < 3; i++)
		linear[i] = channel[i] <= 0.04045f ? channel[i] / 12.92f : powf((channel[i] + 0.055f) / 1.055f, 2.4f);
	return 0.2126f * linear[0] + 0.7152f * linear[1] + 0.0722f * linear[2];
}

static int is_white(const struct ae_strip_rgb *c)
{
	return NEAR(c->red, 229.0f / 255.0f) && NEAR(c->green, 240.0f / 255.0f) && NEAR(c->blue, 1.0f);
}

static void test_me_color(void)
{
	struct ae_strip_rgb in, out;
	static const real whites[][3] = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f },
		{ 0.62f, 0.60f, 0.58f }, { 0.03f, 0.02f, 0.05f } };
	unsigned i;

	/* (white, black, grey, a near-grey, a colour too dark to lift: white) */
	for (i = 0; i < sizeof(whites) / sizeof(whites[0]); i++)
	{
		in.red = whites[i][0]; in.green = whites[i][1]; in.blue = whites[i][2];
		AE_CHECK(!ae_score_strip_me_color(&in, &out) && is_white(&out));
	}
	/* (a dark red armour: lifted, still red, light enough beside its outline) */
	in.red = 0.55f; in.green = 0.05f; in.blue = 0.05f;
	AE_CHECK(ae_score_strip_me_color(&in, &out));
	AE_CHECK(luminance(&out) >= 0.33f - 0.01f && out.red > out.green && out.red > out.blue && !is_white(&out));
	/* (a light colour already over the rule: kept) */
	in.red = 1.0f; in.green = 0.85f; in.blue = 0.1f;
	AE_CHECK(ae_score_strip_me_color(&in, &out) && NEAR(out.red, 1.0f) && NEAR(out.green, 0.85f) && NEAR(out.blue, 0.1f));
	/* (a cobalt: lifted, its hue kept) */
	in.red = 0.15f; in.green = 0.2f; in.blue = 0.6f;
	AE_CHECK(ae_score_strip_me_color(&in, &out) && luminance(&out) >= 0.33f - 0.01f && out.blue > out.red);
	/* (a near-white pastel: too close to white to stay distinct) */
	in.red = 0.95f; in.green = 0.93f; in.blue = 1.0f;
	AE_CHECK(!ae_score_strip_me_color(&in, &out) && is_white(&out));
	AE_CHECK(!ae_score_strip_me_color(NULL, &out) && is_white(&out));
}

static void test_wide_context_clears_the_marker(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;
	struct ae_strip_geometry g = { 0, 0, 1920, 1080, 1.0f, 998.0f, 1600.0f, 960.0f };

	/* ("YOU ARE THE JUGGERNAUT" reaches over my score: its foot 8 above the marker's top, not 40 above the baseline
	and touching it) */
	memset(&s, 0, sizeof(s));
	s.mode = _ae_strip_juggernaut; s.score_to_win = 15; s.clock_seconds = 801; s.me.team_index = -1;
	s.other.team_index = -1; s.me.place = 1; s.hill_move_seconds = -1; s.next_flag_meters = -1; s.respawns_left = -1;
	s.context_side = _ae_strip_side_me; wcscpy(s.context_name, L"me");
	lay(&s, &g, &m, &l);
	AE_CHECK(l.marker.visible && l.context.visible);
	AE_CHECK(l.context.x < right(&l.marker) && l.marker.x < right(&l.context));
	AE_CHECK(NEAR(bottom(&l.context), l.marker.y - 8.0f));
	AE_CHECK(bottom(&l.context) < 979.0f - 40.0f);
	/* (a narrow line clear of the marker stays where it was: the team slayer test's lead at 939) */
}

/* (0.3.9) Bomb's "BOMB PLANTED" over the clock ended 2 px right of the marker (4v4 cells, 2v2 cells, text): the
clearance rule's 8 px from the marker in every view, lifted 8 over it when nearer */
static void test_bomb_planted_clears_the_marker(void)
{
	static const struct ae_strip_geometry views[] = {
		{ 0, 0, 1920, 1080, 1.0f, 998.0f, 1600.0f, 960.0f },
		{ 0, 0, 1920, 540, 0.85f, 500.0f, 1600.0f, 960.0f },
		{ 0, 0, 960, 540, 0.75f, 500.0f, 800.0f, 480.0f },
	};
	long players[] = { 1, 2, 4, 6 };
	size_t v, p;

	for (v = 0; v < sizeof(views) / sizeof(views[0]); v++)
		for (p = 0; p < sizeof(players) / sizeof(players[0]); p++)
		{
			struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l;

			memset(&s, 0, sizeof(s));
			s.mode = _ae_strip_rounds; s.teams = TRUE; s.rounds_to_win = 4; s.clock_seconds = 28;
			s.me.team_index = 0; s.other.team_index = 1; s.me.players = s.other.players = players[p];
			s.me.alive = s.other.alive = 1; s.hill_move_seconds = -1; s.next_flag_meters = -1; s.respawns_left = -1;
			s.bomb_planted = TRUE; s.bomb_planted_side = _ae_strip_side_other;
			lay(&s, &views[v], &m, &l);
			AE_CHECK(l.marker.visible && l.context.visible);
			AE_CHECK(right(&l.marker) + 8.0f * l.strip_scale <= l.context.x + 0.01f ||
				bottom(&l.context) <= l.marker.y - 8.0f * l.strip_scale + 0.01f);
		}
}

/* the played line's ink (its caps: digits and ':' have no descenders) */
static real played_ink_top(const struct ae_strip_layout *l) { return bottom(&l->played) - AE_STRIP_TEXT_CAP * l->played.h; }

/* the clearance rule: two boxes are at least `gap` apart on one axis */
static int clear(const struct ae_strip_box *a, real a_top, const struct ae_strip_box *b, real b_top, real gap)
{
	return right(a) + gap <= b->x || right(b) + gap <= a->x || bottom(a) + gap <= b_top || bottom(b) + gap <= a_top;
}

/* MATCH CLOCK BOTH: the time played 10 over the clock's top; the context / lead line 8 over it (its descenders
too); clear of the marker and the scores in every view; nothing moves without it */
static void test_time_played_over_the_clock(void)
{
	static const struct ae_strip_geometry views[] = {
		{ 0, 0, 1920, 1080, 1.0f, 998.0f, 1600.0f, 960.0f },  /* 1080p */
		{ 0, 0, 1280, 720, 0.667f, 665.0f, 1066.0f, 640.0f }, /* 720p: the text floors at 12 */
		{ 0, 0, 1920, 540, 0.85f, 499.0f, 1700.0f, 960.0f },  /* 2 players */
		{ 0, 0, 960, 540, 0.75f, 499.0f, 700.0f, 480.0f },    /* 4 players: floors 14 */
	};
	struct ae_strip_snapshot s; struct ae_strip_model m; struct ae_strip_layout l, before;
	long i, mode;

	for (mode = 0; mode < 3; mode++)
		for (i = 0; i < (long)(sizeof(views) / sizeof(views[0])); i++)
		{
			const struct ae_strip_geometry *g = &views[i];
			real k, clock_top, lead_bottom;
			const struct ae_strip_box *line;

			team_slayer(&s);
			if (mode == 1)
			{
				/* (oddball: the small clock, wide timed scores beside it, a context line with a descender) */
				s.mode = _ae_strip_oddball; s.score_to_win = 300; s.me.score = 72; s.other.score = 48;
				wcscpy(s.context_name, L"Player"); s.context_side = _ae_strip_side_other;
			}
			else if (mode == 2)
			{
				/* (free for all: sub-lines, the goal on their line) */
				s.teams = FALSE; s.me.place = 1; s.i_lead = TRUE; s.me.score = 9; s.other.score = 3;
			}
			s.clock_seconds = 899;
			lay(&s, g, &m, &before);
			s.played_shown = TRUE; s.played_seconds = 3599;
			lay(&s, g, &m, &l);
			k = l.strip_scale;
			AE_CHECK(l.played.visible && l.played.h >= 12.0f);
			AE_CHECK(NEAR(l.played.x + l.played.w / 2.0f, g->centre_x));
			clock_top = m.clock_small ? l.clock_baseline_y - AE_STRIP_TEXT_CAP * l.clock.h : l.clock.y;
			AE_CHECK(NEAR(clock_top - bottom(&l.played), AE_STRIP_PLAYED_CLOCK_GAP * k));
			line = l.context.visible ? &l.context : &l.lead;
			AE_CHECK(line->visible);
			lead_bottom = bottom(line) + AE_STRIP_TEXT_DESCENT * line->h;
			AE_CHECK(played_ink_top(&l) - lead_bottom >= AE_STRIP_PLAYED_CONTEXT_GAP * k - 0.01f);
			/* (8 at 1080p, scaled with the strip) */
			AE_CHECK(clear(&l.played, played_ink_top(&l), &l.marker, l.marker.y, 8.0f * k));
			AE_CHECK(clear(&l.played, played_ink_top(&l), &l.left_score, l.left_score.y, 8.0f * k));
			AE_CHECK(clear(&l.played, played_ink_top(&l), &l.right_score, l.right_score.y, 8.0f * k));
			/* (the scores, the clock, the goal and the bottom line stay where they were) */
			AE_CHECK(NEAR(l.score_baseline_y, before.score_baseline_y) && NEAR(l.clock.x, before.clock.x));
			AE_CHECK(NEAR(bottom(&l.goal), bottom(&before.goal)) && NEAR(l.strip_scale, before.strip_scale));
			AE_CHECK(!before.played.visible && bottom(line) <= bottom(before.context.visible ? &before.context :
				&before.lead) + 0.01f);
		}
}

int main(void)
{
	test_me_color();
	test_1p_team_slayer_on_shared_line();
	test_4p_gap_shrinks_bars_then_scales();
	test_gap_rule_steps();
	test_timed_mode_small_clock();
	test_race_closes_up_without_bars();
	test_rounds_pips_and_cells();
	test_other_views();
	test_one_baseline_g5();
	test_no_limit_score_is_lowest_line();
	test_quarter_view_floors_and_long_names();
	test_clock_cell();
	test_bad_input();
	test_wide_context_clears_the_marker();
	test_bomb_planted_clears_the_marker();
	test_time_played_over_the_clock();
	printf("%d checks, %d failures\n", checks, failures);
	return failures != 0;
}
