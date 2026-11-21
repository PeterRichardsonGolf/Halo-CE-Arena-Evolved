/* ae_medal_modern_rules_test.c: the MODERN medal stack's metrics, rows, limit, pops and gaps (ae_medal_modern_rules.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ae_marker_rules.h"
#include "ae_medal_modern_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.01f)

static struct ae_mm_row medal_row(int medal, int points, int picture)
{
	struct ae_mm_row row;

	memset(&row, 0, sizeof(row));
	row.kind = AE_MM_KIND_MEDAL;
	row.medal = medal;
	row.points = points;
	row.has_picture = picture;
	return row;
}

static struct ae_mm_row killed_row(void)
{
	struct ae_mm_row row;

	memset(&row, 0, sizeof(row));
	row.kind = AE_MM_KIND_KILLED;
	row.victim = 4;
	return row;
}

static void metrics_test(void)
{
	struct ae_mm_metrics m;

	/* one view at 1080 lines: the stage's own numbers */
	ae_mm_metrics_of(1920.0f, 1080.0f, FALSE, &m);
	CHECK(NEAR(m.name_px, 19.0f) && NEAR(m.score_px, 28.0f) && NEAR(m.picture, 56.0f));
	CHECK(NEAR(m.row_picture, 58.0f) && NEAR(m.row_text, 40.0f));
	CHECK(NEAR(m.name_right, 720.0f) && NEAR(m.picture_left, 727.0f) && NEAR(m.picture_right, 783.0f));
	CHECK(NEAR(m.score_middle, 468.0f) && NEAR(m.limit, 667.0f) && NEAR(m.centre_x, 960.0f));
	CHECK(NEAR(m.rows_top, 468.0f + 0.65f * 28.0f + 4.0f));
	/* scaled by the view's height, the centre by its width */
	ae_mm_metrics_of(2560.0f, 1440.0f, FALSE, &m);
	CHECK(NEAR(m.name_px, 19.0f * 4.0f / 3.0f) && NEAR(m.name_right, 1280.0f - 240.0f * 4.0f / 3.0f) && NEAR(m.limit, 667.0f * 4.0f / 3.0f));
	CHECK(NEAR(m.score_middle, 468.0f * 4.0f / 3.0f) && NEAR(m.picture, 56.0f * 4.0f / 3.0f));
	/* a small view holds its text at floors */
	ae_mm_metrics_of(1280.0f, 720.0f, FALSE, &m);
	CHECK(m.name_px >= 16.0f && m.score_px >= 20.0f && m.row_text >= 26.0f);
	/* two views: a view 540 lines high, text 18, the pictures never under 20, the rows never under 26 */
	ae_mm_metrics_of(1920.0f, 540.0f, TRUE, &m);
	CHECK(NEAR(m.name_px, 18.0f) && m.picture >= 20.0f && m.row_text >= 26.0f && m.row_picture >= m.picture + 2.0f);
	CHECK(NEAR(m.name_right, 960.0f - 120.0f) && NEAR(m.limit, 333.5f));
}

static void gaps_test(void)
{
	struct ae_mm_metrics m;
	struct ae_mm_box stack, other;

	ae_mm_metrics_of(1920.0f, 1080.0f, FALSE, &m);
	/* the reticle's zone: the pictures' right edge (783) to the centre (960) less the zone's 66 */
	CHECK(NEAR(ae_mm_zone_gap(&m), 111.0f) && ae_mm_zone_gap(&m) >= AE_MM_CLEAR);
	ae_mm_box_of(&m, AE_MM_WIDEST_EM, &stack);
	CHECK(NEAR(stack.x1, 783.0f) && NEAR(stack.y1, 667.0f) && NEAR(stack.x0, 720.0f - 13.0f * 19.0f));
	CHECK(NEAR(stack.y0, 468.0f - 0.65f * 28.0f));
	/* boxes: apart, beside, meeting */
	other.x0 = 800.0f; other.x1 = 900.0f; other.y0 = 0.0f; other.y1 = 1000.0f;
	CHECK(NEAR(ae_mm_gap(&stack, &other), 17.0f));
	other.x0 = 700.0f; other.x1 = 900.0f; other.y0 = 700.0f; other.y1 = 800.0f;
	CHECK(NEAR(ae_mm_gap(&stack, &other), 33.0f));
	other.x0 = 800.0f; other.y0 = 700.0f; other.x1 = 900.0f; other.y1 = 800.0f;
	CHECK(NEAR(ae_mm_gap(&stack, &other), (float)sqrt(17.0 * 17.0 + 33.0 * 33.0)));
	other.x0 = 700.0f; other.y0 = 600.0f; other.x1 = 900.0f; other.y1 = 800.0f;
	CHECK(ae_mm_gap(&stack, &other) < 0.0f);
	/* the stack's box clear of other lines by 8 or not */
	other.x0 = 800.0f; other.y0 = 0.0f; other.x1 = 900.0f; other.y1 = 1000.0f;
	CHECK(ae_mm_clears(&stack, &other, 1, AE_MM_CLEAR));
	other.x0 = 788.0f;
	CHECK(!ae_mm_clears(&stack, &other, 1, AE_MM_CLEAR));
	other.x0 = 791.0f;
	CHECK(ae_mm_clears(&stack, &other, 1, AE_MM_CLEAR) && ae_mm_clears(&stack, &other, 0, AE_MM_CLEAR));
	/* the feed's widest rows (452 wide, five rows up from 782, its tick 13 left of the sensor's left edge at 140) meet the stack's limit:
	a gap under 8, which is reported, not hidden */
	{
		struct ae_mm_box feed;

		feed.x0 = 127.0f; feed.x1 = 127.0f + 452.0f; feed.y1 = 782.0f; feed.y0 = 782.0f - 4.0f * 28.5f - 26.5f;
		CHECK(NEAR(feed.y0, 641.5f));
		CHECK(ae_mm_gap(&stack, &feed) < 0.0f);
		/* a medal stack's rows hold three picture rows, so a text row's bottom is at 486.2 + 3 x 58 = 660.2 at the lowest picture row */
		stack.y1 = m.rows_top + 3.0f * 58.0f;
		CHECK(NEAR(stack.y1, 664.2f));
	}
}

static void rows_test(void)
{
	struct ae_mm_metrics m;
	struct ae_mm_stack stack;
	struct ae_mm_row medal = medal_row(1, 15, 1), text = medal_row(2, 5, 0), killed = killed_row();
	real y, alpha, scale;

	ae_mm_metrics_of(1920.0f, 1080.0f, FALSE, &m);
	CHECK(AE_MM_LIFE == 90 && AE_MM_FULL_LIFE == 60 && AE_MM_MOST == 5 && AE_MM_FADE_TICKS == 15);
	CHECK(NEAR(ae_mm_row_height(&m, &medal), 58.0f) && NEAR(ae_mm_row_height(&m, &text), 40.0f) && NEAR(ae_mm_row_height(&m, &killed), 40.0f));
	ae_mm_reset(&stack);
	CHECK(ae_mm_add(&stack, &m, &medal, 100) == 0);
	/* the first row: at the rows' top, its name popping from 1.3 to 1 in 100 ms */
	CHECK(ae_mm_row_pose(&stack.rows[0], 100, &y, &alpha, &scale) && NEAR(y, 0.0f) && NEAR(alpha, 1.0f) && NEAR(scale, 1.3f));
	CHECK(ae_mm_row_pose(&stack.rows[0], 101, &y, &alpha, &scale) && NEAR(scale, 1.2f));
	CHECK(ae_mm_row_pose(&stack.rows[0], 103, &y, &alpha, &scale) && NEAR(scale, 1.0f));
	/* the next one is under it, the newest at the bottom */
	CHECK(ae_mm_add(&stack, &m, &killed, 105) == 0);
	CHECK(ae_mm_row_pose(&stack.rows[1], 105, &y, &alpha, &scale) && NEAR(y, 58.0f));
	/* 3 s, then a fade over 0.5 s */
	CHECK(ae_mm_row_pose(&stack.rows[0], 189, &y, &alpha, &scale) && NEAR(alpha, 1.0f));
	CHECK(ae_mm_row_pose(&stack.rows[0], 197, &y, &alpha, &scale) && alpha > 0.4f && alpha < 0.6f);
	CHECK(!ae_mm_row_pose(&stack.rows[0], 205, &y, &alpha, &scale));
	/* the row below closes up over 4 ticks once the first is gone */
	ae_mm_step(&stack, &m, 204);
	CHECK(ae_mm_row_pose(&stack.rows[1], 204, &y, &alpha, &scale) && NEAR(y, 58.0f));
	ae_mm_step(&stack, &m, 205);
	CHECK(ae_mm_count(&stack) == 1);
	CHECK(ae_mm_row_pose(&stack.rows[0], 205, &y, &alpha, &scale) && NEAR(y, 58.0f));
	CHECK(ae_mm_row_pose(&stack.rows[0], 207, &y, &alpha, &scale) && NEAR(y, 29.0f));
	CHECK(ae_mm_row_pose(&stack.rows[0], 209, &y, &alpha, &scale) && NEAR(y, 0.0f));
	/* a new game (the clock starting again) empties the stack */
	ae_mm_step(&stack, &m, 3);
	CHECK(ae_mm_count(&stack) == 0);
}

static void limit_test(void)
{
	struct ae_mm_metrics m;
	struct ae_mm_stack stack;
	struct ae_mm_row medal = medal_row(1, 15, 1), killed = killed_row();
	real y, alpha, scale;
	int index;

	ae_mm_metrics_of(1920.0f, 1080.0f, FALSE, &m);
	/* three picture rows fit under the limit (486.2 + 174 = 660.2); a fourth would pass 667: the oldest goes first */
	ae_mm_reset(&stack);
	for (index = 0; index < 3; index++)
	{
		medal.medal = 10 + index;
		CHECK(ae_mm_add(&stack, &m, &medal, 50) == 0);
	}
	medal.medal = 13;
	CHECK(ae_mm_add(&stack, &m, &medal, 51) == 1);
	CHECK(stack.rows[0].gone_at == 51 && stack.rows[0].medal == 10 && ae_mm_live(&stack, 51) == 3);
	/* the newest is still at the bottom: under the two that stay */
	CHECK(ae_mm_row_pose(&stack.rows[3], 51, &y, &alpha, &scale) && NEAR(y, 116.0f));
	/* a Killed row (40) after three picture rows pushes the oldest out too: 174 + 40 passes the room */
	ae_mm_reset(&stack);
	for (index = 0; index < 3; index++)
		ae_mm_add(&stack, &m, &medal, 50);
	CHECK(ae_mm_add(&stack, &m, &killed, 52) == 1);
	/* the room by text rows: four 40 rows fit (160), a fifth does not (200 > 180.8) */
	ae_mm_reset(&stack);
	for (index = 0; index < 4; index++)
		CHECK(ae_mm_add(&stack, &m, &killed, 60) == 0);
	CHECK(ae_mm_add(&stack, &m, &killed, 61) == 1);
	CHECK(ae_mm_live(&stack, 61) == 4);
	/* never more than five rows, whatever their size (a limit far down) */
	ae_mm_metrics_of(1920.0f, 1080.0f, FALSE, &m);
	m.limit = 5000.0f;
	ae_mm_reset(&stack);
	for (index = 0; index < 5; index++)
		ae_mm_add(&stack, &m, &killed, 70);
	CHECK(ae_mm_live(&stack, 70) == 5);
	CHECK(ae_mm_add(&stack, &m, &killed, 71) == 1 && ae_mm_live(&stack, 71) == 5);
}

static void full_life_test(void)
{
	struct ae_mm_metrics m;
	struct ae_mm_stack stack;
	struct ae_mm_row killed = killed_row();
	int index;

	/* room for five rows: with five up, the oldest leaves once it has had 2 s (60 ticks) */
	ae_mm_metrics_of(1920.0f, 1080.0f, FALSE, &m);
	m.limit = 5000.0f;
	ae_mm_reset(&stack);
	for (index = 0; index < 4; index++)
		ae_mm_add(&stack, &m, &killed, 100 + index);
	CHECK(stack.rows[0].life_end == 190);
	ae_mm_add(&stack, &m, &killed, 110);
	CHECK(stack.rows[0].life_end == 160 && stack.rows[1].life_end == 191);
	/* one that has had its 2 s already starts to leave at once */
	ae_mm_reset(&stack);
	for (index = 0; index < 4; index++)
		ae_mm_add(&stack, &m, &killed, 0);
	ae_mm_step(&stack, &m, 80);
	ae_mm_add(&stack, &m, &killed, 80);
	CHECK(stack.rows[0].life_end == 80);
}

static void score_test(void)
{
	struct ae_mm_metrics m;
	struct ae_mm_stack stack;
	struct ae_mm_row a = medal_row(1, 15, 1), b = medal_row(2, 50, 1), killed = killed_row();
	real alpha, scale;

	ae_mm_metrics_of(1920.0f, 1080.0f, FALSE, &m);
	ae_mm_reset(&stack);
	CHECK(ae_mm_score(&stack, 10) == 0 && !ae_mm_score_pose(&stack, 10, &alpha, &scale));
	/* a Killed row alone adds no points and shows none */
	ae_mm_add(&stack, &m, &killed, 10);
	CHECK(ae_mm_score(&stack, 10) == 0 && !ae_mm_score_pose(&stack, 10, &alpha, &scale));
	/* the points are the shown medals' sum; they pop over 200 ms (6 ticks) when they change */
	ae_mm_add(&stack, &m, &a, 20);
	CHECK(ae_mm_score(&stack, 20) == 15 && ae_mm_score_pose(&stack, 20, &alpha, &scale) && NEAR(scale, 1.3f) && NEAR(alpha, 1.0f));
	CHECK(ae_mm_score_pose(&stack, 23, &alpha, &scale) && NEAR(scale, 1.15f));
	CHECK(ae_mm_score_pose(&stack, 26, &alpha, &scale) && NEAR(scale, 1.0f));
	ae_mm_add(&stack, &m, &b, 40);
	CHECK(ae_mm_score(&stack, 40) == 65 && ae_mm_score_pose(&stack, 40, &alpha, &scale) && NEAR(scale, 1.3f));
	/* the first medal's row fading still counts; once it is gone its points go and the sum pops again, the points fading with the
	clearest medal row */
	CHECK(ae_mm_score(&stack, 111) == 65);
	ae_mm_step(&stack, &m, 111);
	CHECK(ae_mm_score_pose(&stack, 111, &alpha, &scale) && NEAR(scale, 1.0f) && alpha > 0.99f);
	ae_mm_step(&stack, &m, 125);
	CHECK(ae_mm_score(&stack, 125) == 50 && ae_mm_score_pose(&stack, 125, &alpha, &scale) && NEAR(scale, 1.3f));
	CHECK(ae_mm_score_pose(&stack, 140, &alpha, &scale) && alpha > 0.0f && alpha < 1.0f);
	CHECK(!ae_mm_score_pose(&stack, 146, &alpha, &scale));
}

static void word_line_test(void)
{
	wchar_t out[64];
	float gold[3] = { 1.0f, 0.5f, 0.0f };

	CHECK(ae_mm_word_line(out, 64, L"Double Kill", gold) == 4 + 11);
	CHECK(out[0] == AE_RUN_RGB && AE_RUN_BYTE_VALUE(out[1]) == 255 && AE_RUN_BYTE_VALUE(out[2]) == 128 && AE_RUN_BYTE_VALUE(out[3]) == 0);
	ae_feed_strip_runs(out);
	CHECK(!wcscmp(out, L"Double Kill"));
	/* a colour out of 0 to 1 is held to it; none is white */
	gold[0] = 3.0f; gold[1] = -1.0f;
	ae_mm_word_line(out, 64, L"A", gold);
	CHECK(AE_RUN_BYTE_VALUE(out[1]) == 255 && AE_RUN_BYTE_VALUE(out[2]) == 0);
	ae_mm_word_line(out, 64, L"A", NULL);
	CHECK(AE_RUN_BYTE_VALUE(out[1]) == 255 && AE_RUN_BYTE_VALUE(out[3]) == 255);
	/* no room: nothing */
	CHECK(ae_mm_word_line(out, 8, L"Double Kill", gold) == 0 && out[0] == 0);
	CHECK(ae_mm_word_line(NULL, 8, L"A", gold) == 0 && ae_mm_word_line(out, 64, NULL, gold) == 0);
}

int main(void)
{
	metrics_test();
	gaps_test();
	rows_test();
	limit_test();
	full_life_test();
	score_test();
	word_line_test();
	if (failures)
	{
		printf("ae_medal_modern_rules_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("ae_medal_modern_rules_test: ok\n");
	return 0;
}
