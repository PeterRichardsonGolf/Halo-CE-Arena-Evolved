/* ae_mark_prompt_rules_test.c: the aim prompt's words, the row's sizes and where it sits (ae_mark_prompt_rules.h). */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ae_mark_prompt_rules.h"
#include "ae_mark_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, tolerance) do { double x_ = (a), y_ = (b); if (fabs(x_ - y_) > (tolerance)) { \
	printf("FAIL %s:%d %s = %f, wanted %f\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)
#define WORD(tier, verb, name, text) do { wchar_t const *word_ = ae_prompt_word((tier), (verb), (name)); \
	if (!word_ || wcscmp(word_, (text))) { printf("FAIL %s:%d %s: wanted %ls, got %ls\n", __FILE__, __LINE__, #text, \
		(text), word_ ? word_ : L"(none)"); failures++; } } while (0)

static void box(struct ae_prompt_box *b, float x0, float y0, float x1, float y1)
{
	b->x0 = x0;
	b->y0 = y0;
	b->x1 = x1;
	b->y1 = y1;
}

/* a view of 1920 x 1080 and a row of 120 x 28, at scale s */
static void base(struct ae_prompt_in *in, float s)
{
	memset(in, 0, sizeof(*in));
	in->view_width = 1920.0f;
	in->view_height = 1080.0f;
	in->scale = s;
	in->width = 120.0f * s;
	in->height = 28.0f * s;
}

/* the distance from the view's middle to the row's nearest edge */
static float nearest(struct ae_prompt_in const *in, float x, float y)
{
	float cx = in->view_width / 2.0f, cy = in->view_height / 2.0f;
	float dx = cx < x ? x - cx : cx > x + in->width ? cx - (x + in->width) : 0.0f;
	float dy = cy < y ? y - cy : cy > y + in->height ? cy - (y + in->height) : 0.0f;

	return hypotf(dx, dy);
}

static int meets(struct ae_prompt_in const *in, float x, float y, struct ae_prompt_box const *b, float margin)
{
	return x < b->x1 + margin && b->x0 - margin < x + in->width && y < b->y1 + margin && b->y0 - margin < y + in->height;
}

static void words_test(void)
{
	/* the spec's table */
	WORD(AE_TIER_ENEMY, AE_CALL_MARK, NULL, L"ENEMY");
	WORD(AE_TIER_ENEMY, AE_CALL_WHEEL, NULL, L"ENEMY");
	WORD(AE_TIER_GROUND, AE_CALL_MARK, NULL, L"GO HERE");
	WORD(AE_TIER_BODY, AE_CALL_MARK, NULL, L"FIGHT HERE");
	WORD(AE_TIER_PICKUP, AE_CALL_MARK, L"SNIPER RIFLE", L"SNIPER");
	WORD(AE_TIER_PICKUP, AE_CALL_MARK, L"ROCKET LAUNCHER", L"ROCKETS");
	WORD(AE_TIER_PICKUP, AE_CALL_MARK, L"ACTIVE CAMO", L"CAMO");
	WORD(AE_TIER_PICKUP, AE_CALL_MARK, L"OVERSHIELD", L"OVERSHIELD");
	WORD(AE_TIER_PICKUP, AE_CALL_MARK, L"SHOTGUN", L"SHOTGUN");
	WORD(AE_TIER_PICKUP, AE_CALL_MARK, L"ASSAULT RIFLE", L"ASSAULT RIFLE");
	WORD(AE_TIER_PICKUP, AE_CALL_MARK, NULL, L"ITEM");
	WORD(AE_TIER_NAMED, AE_CALL_MARK, L"WARTHOG", L"WARTHOG");
	WORD(AE_TIER_NAMED, AE_CALL_MARK, L"FLAG", L"FLAG");
	WORD(AE_TIER_NAMED, AE_CALL_MARK, L"HILL", L"HILL");
	WORD(AE_TIER_NAMED, AE_CALL_MARK, L"TELEPORTER", L"TELEPORTER");
	WORD(AE_TIER_NAMED, AE_CALL_MARK, L"BOMB SITE", L"BOMB");
	/* the answers to a marker, whatever it is on */
	WORD(AE_TIER_MARKER, AE_CALL_CANCEL, NULL, L"CANCEL");
	WORD(AE_TIER_MARKER, AE_CALL_OK, NULL, L"OK");
	WORD(AE_TIER_MARKER, AE_CALL_NEVER_MIND, NULL, L"NEVER MIND");
	WORD(AE_TIER_MARKER, AE_CALL_DIBS, NULL, L"DIBS");
	/* (your own mark's target takes it back ahead of any tier) */
	WORD(AE_TIER_ENEMY, AE_CALL_CANCEL, NULL, L"CANCEL");
	WORD(AE_TIER_PICKUP, AE_CALL_CANCEL, L"SNIPER RIFLE", L"CANCEL");
	/* nothing to say */
	CHECK(ae_prompt_word(0, AE_CALL_MARK, NULL) == NULL);
	CHECK(ae_prompt_word(99, AE_CALL_MARK, NULL) == NULL);
	CHECK(ae_prompt_word(AE_TIER_MARKER, AE_CALL_MARK, NULL) == NULL);
	/* only an enemy's mark is in enemy red */
	CHECK(ae_prompt_word_is_enemy(AE_TIER_ENEMY, AE_CALL_MARK) == 1);
	CHECK(ae_prompt_word_is_enemy(AE_TIER_ENEMY, AE_CALL_WHEEL) == 1);
	CHECK(ae_prompt_word_is_enemy(AE_TIER_ENEMY, AE_CALL_CANCEL) == 0);
	CHECK(ae_prompt_word_is_enemy(AE_TIER_GROUND, AE_CALL_MARK) == 0);
	CHECK(ae_prompt_word_is_enemy(AE_TIER_PICKUP, AE_CALL_MARK) == 0);
	CHECK(ae_prompt_word_is_enemy(AE_TIER_MARKER, AE_CALL_OK) == 0);
}

static void metrics_test(void)
{
	struct ae_prompt_metrics m;

	ae_prompt_metrics_get(0, 1.0f, &m);
	NEAR(m.chip_height, 24.0f, 0.001);
	NEAR(m.word_px, 20.0f, 0.001);
	NEAR(m.chip_gap, 8.0f, 0.001);
	NEAR(m.halo, 2.0f, 0.001);
	ae_prompt_metrics_get(1, 1.0f, &m);
	NEAR(m.chip_height, 20.0f, 0.001);
	NEAR(m.word_px, 16.0f, 0.001);
	NEAR(m.chip_gap, 8.0f, 0.001);
	/* 1440 lines: all of it x 4/3, on whole pixels */
	ae_prompt_metrics_get(0, 1440.0f / 1080.0f, &m);
	NEAR(m.chip_height, 32.0f, 0.001);
	NEAR(m.word_px, 27.0f, 0.001);
	NEAR(m.chip_gap, 11.0f, 0.001);
	NEAR(m.halo, 3.0f, 0.001);
	ae_prompt_metrics_get(1, 1440.0f / 1080.0f, &m);
	NEAR(m.chip_height, 27.0f, 0.001);
	NEAR(m.word_px, 21.0f, 0.001);
	/* a hud scale of nothing is the least that can be drawn, never 0 */
	ae_prompt_metrics_get(0, 0.0f, &m);
	CHECK(m.chip_height >= 1.0f && m.word_px >= 1.0f);
}

static void place_test(void)
{
	struct ae_prompt_in in;
	float x, y;
	int kind;

	/* centred above the target, its foot 8 clear */
	base(&in, 1.0f);
	box(&in.target, 1020.0f, 460.0f, 1080.0f, 500.0f);
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_ABOVE);
	NEAR(x, 990.0f, 0.001);
	NEAR(y, 424.0f, 0.001);
	NEAR(y + in.height, in.target.y0 - 8.0f, 0.001);

	/* one name over the target: above the name, 8 clear, centred on it */
	base(&in, 1.0f);
	box(&in.target, 1020.0f, 460.0f, 1080.0f, 500.0f);
	in.tags = 1;
	box(&in.tag, 1005.0f, 436.0f, 1095.0f, 458.0f);
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_ABOVE_TAG);
	NEAR(x, 990.0f, 0.001);
	NEAR(y + in.height, in.tag.y0 - 8.0f, 0.001);

	/* a pile: beside the target, on the side away from the view's middle, 8 clear */
	base(&in, 1.0f);
	box(&in.target, 1020.0f, 460.0f, 1080.0f, 500.0f);
	in.tags = 2;
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_BESIDE);
	NEAR(x, 1088.0f, 0.001);
	NEAR(y, 466.0f, 0.001);
	base(&in, 1.0f);
	box(&in.target, 840.0f, 460.0f, 900.0f, 500.0f);
	in.tags = 3;
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_BESIDE);
	NEAR(x + in.width, 832.0f, 0.001);
	NEAR(y, 466.0f, 0.001);

	/* the reticle zone over where it would stand: beside too */
	base(&in, 1.0f);
	box(&in.target, 1000.0f, 500.0f, 1060.0f, 560.0f);
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_BESIDE);
	NEAR(x, 1068.0f, 0.001);
	NEAR(y, 516.0f, 0.001);

	/* a target at the middle (the ground at the aim): beside on the right, moved out of the zone */
	base(&in, 1.0f);
	box(&in.target, 958.0f, 538.0f, 962.0f, 542.0f);
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_BESIDE);
	CHECK(nearest(&in, x, y) >= 66.0f - 0.001f && nearest(&in, x, y) < 67.5f);
	CHECK(x > 960.0f);

	/* past 150 x s from the middle: none */
	base(&in, 1.0f);
	box(&in.target, 1500.0f, 460.0f, 1560.0f, 500.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);
	/* (a target above the middle, whose prompt would go further up) */
	base(&in, 1.0f);
	box(&in.target, 1020.0f, 300.0f, 1080.0f, 340.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);
	/* (a pile too far) */
	base(&in, 1.0f);
	box(&in.target, 1500.0f, 460.0f, 1560.0f, 500.0f);
	in.tags = 2;
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);

	/* the text rows in the way: below the target, its top 8 clear of the target's foot */
	base(&in, 1.0f);
	box(&in.target, 1100.0f, 460.0f, 1160.0f, 500.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_ABOVE);
	NEAR(x, 1070.0f, 0.001);
	NEAR(y, 424.0f, 0.001);
	in.row_count = 1;
	box(&in.rows[0], 1060.0f, 410.0f, 1200.0f, 440.0f);
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_BELOW);
	NEAR(x, 1070.0f, 0.001);
	NEAR(y, 508.0f, 0.001);
	NEAR(y, in.target.y1 + 8.0f, 0.001);
	/* (a row 7 px off the row still touches: 8 clear is the rule) */
	in.row_count = 1;
	box(&in.rows[0], 1060.0f, 400.0f, 1200.0f, 417.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_BELOW);
	box(&in.rows[0], 1060.0f, 400.0f, 1200.0f, 416.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_ABOVE);
	/* no place clears both: none */
	in.row_count = 2;
	box(&in.rows[0], 1060.0f, 410.0f, 1200.0f, 440.0f);
	box(&in.rows[1], 1060.0f, 500.0f, 1200.0f, 545.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);

	/* a view's top: its prompt would go off the screen, and under the target it is too far from the middle */
	base(&in, 1.0f);
	box(&in.target, 1100.0f, 10.0f, 1160.0f, 50.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);

	/* the same at 0.75: the clearances and the cap scale */
	base(&in, 0.75f);
	box(&in.target, 1020.0f, 470.0f, 1080.0f, 500.0f);
	kind = ae_prompt_place(&in, &x, &y);
	CHECK(kind == AE_PROMPT_ABOVE);
	NEAR(x, 1005.0f, 0.001);
	NEAR(y + in.height, in.target.y0 - 6.0f, 0.001);
	base(&in, 0.75f);
	box(&in.target, 960.0f, 470.0f, 1000.0f, 500.0f);
	in.tags = 2;
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_BESIDE);
	NEAR(x, 1006.0f, 0.001);
	/* (0.75's cap is 112.5: this one's nearest edge is 152 off) */
	base(&in, 0.75f);
	box(&in.target, 1100.0f, 460.0f, 1160.0f, 500.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);
}

/* wherever a place is found it clears the zone, stays within the cap and the view, and keeps 8 x s off the target, the
name and the rows */
static void sweep_test(void)
{
	static float const scales[3] = { 1.0f, 0.85f, 0.75f };
	int placed = 0, none = 0, below = 0, beside = 0;
	int s_index, tx, ty, size, mode;

	for (s_index = 0; s_index < 3; s_index++)
	{
		for (size = 0; size < 4; size++)
		{
			float side = 4.0f + (float)size * 60.0f;

			for (tx = 0; tx <= 1920; tx += 60)
			{
				for (ty = 0; ty <= 1080; ty += 45)
				{
					for (mode = 0; mode < 5; mode++)
					{
						struct ae_prompt_in in;
						float x, y, s = scales[s_index], gap = 8.0f * s;
						int kind;

						base(&in, s);
						box(&in.target, (float)tx - side / 2.0f, (float)ty - side / 2.0f, (float)tx + side / 2.0f,
							(float)ty + side / 2.0f);
						if (mode == 1 || mode == 3)
						{
							in.tags = 1;
							box(&in.tag, (float)tx - 40.0f, in.target.y0 - 24.0f, (float)tx + 40.0f, in.target.y0 - 2.0f);
						}
						if (mode == 2)
							in.tags = 2;
						if (mode >= 3)
						{
							in.row_count = 1;
							box(&in.rows[0], 960.0f - 200.0f + 60.0f * (float)(tx % 3), 540.0f - 120.0f, 960.0f + 100.0f,
								540.0f - 90.0f);
						}
						kind = ae_prompt_place(&in, &x, &y);
						if (kind == AE_PROMPT_NONE)
						{
							none++;
							continue;
						}
						placed++;
						below += kind == AE_PROMPT_BELOW;
						beside += kind == AE_PROMPT_BESIDE;
						CHECK(nearest(&in, x, y) >= 66.0f * s - 0.001f);
						CHECK(nearest(&in, x, y) <= 150.0f * s + 0.001f);
						CHECK(x >= 0.0f && y >= 0.0f && x + in.width <= in.view_width && y + in.height <= in.view_height);
						CHECK(x == floorf(x) && y == floorf(y));
						CHECK(!meets(&in, x, y, &in.target, gap - 0.001f));
						if (in.tags == 1 && kind == AE_PROMPT_ABOVE_TAG)
							CHECK(!meets(&in, x, y, &in.tag, gap - 0.001f));
						if (in.row_count)
							CHECK(!meets(&in, x, y, &in.rows[0], gap - 0.001f));
					}
				}
			}
		}
	}
	/* (the sweep reaches every kind of place and refuses some) */
	CHECK(placed > 100 && none > 100 && below > 0 && beside > 0);
}

static void refuse_test(void)
{
	struct ae_prompt_in in;
	float x, y;

	/* a row with no size, a scale of nothing, a view with no size: nothing to place */
	base(&in, 1.0f);
	box(&in.target, 1020.0f, 460.0f, 1080.0f, 500.0f);
	in.width = 0.0f;
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);
	base(&in, 0.0f);
	box(&in.target, 1020.0f, 460.0f, 1080.0f, 500.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);
	base(&in, 1.0f);
	in.view_width = 0.0f;
	box(&in.target, 1020.0f, 460.0f, 1080.0f, 500.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);
	/* a row count past the list's size reads only the list */
	base(&in, 1.0f);
	box(&in.target, 1020.0f, 460.0f, 1080.0f, 500.0f);
	in.row_count = 1000;
	(void)ae_prompt_place(&in, &x, &y);
	/* a target with a NaN corner is no target */
	base(&in, 1.0f);
	box(&in.target, NAN, 460.0f, 1080.0f, 500.0f);
	CHECK(ae_prompt_place(&in, &x, &y) == AE_PROMPT_NONE);
}

static void side_keep_test(void)
{
	struct ae_prompt_in in;
	float x, y, left_x, right_x;

	/* none yet: right; a target near the middle keeps the side, however it wobbles */
	CHECK(ae_prompt_side_keep(960.0f, 960.0f, 1.0f, 0) == 2);
	CHECK(ae_prompt_side_keep(959.0f, 960.0f, 1.0f, 2) == 2);
	CHECK(ae_prompt_side_keep(961.0f, 960.0f, 1.0f, 1) == 1);
	CHECK(ae_prompt_side_keep(960.0f - 66.0f, 960.0f, 1.0f, 2) == 2);
	/* past the zone on the other side: it goes over */
	CHECK(ae_prompt_side_keep(960.0f - 67.0f, 960.0f, 1.0f, 2) == 1);
	CHECK(ae_prompt_side_keep(960.0f + 67.0f, 960.0f, 1.0f, 1) == 2);
	/* in a split view the zone is smaller */
	CHECK(ae_prompt_side_keep(960.0f - 67.0f, 960.0f, 0.75f, 2) == 1);
	CHECK(ae_prompt_side_keep(960.0f - 49.0f, 960.0f, 0.75f, 2) == 2);
	/* the place follows it: the same target, under the reticle, left and right, and a wobble does not move it */
	base(&in, 1.0f);
	box(&in.target, 940.0f, 520.0f, 980.0f, 560.0f);
	in.side = 1;
	CHECK(ae_prompt_place(&in, &left_x, &y) != AE_PROMPT_NONE);
	in.side = 2;
	CHECK(ae_prompt_place(&in, &right_x, &y) != AE_PROMPT_NONE);
	CHECK(left_x < 960.0f && right_x > 960.0f);
	box(&in.target, 944.0f, 520.0f, 984.0f, 560.0f);
	CHECK(ae_prompt_place(&in, &x, &y) != AE_PROMPT_NONE && x > 960.0f);
	box(&in.target, 936.0f, 520.0f, 976.0f, 560.0f);
	CHECK(ae_prompt_place(&in, &x, &y) != AE_PROMPT_NONE && x > 960.0f);
}

static void pin_test(void)
{
	struct ae_prompt_in in;
	float x = 0.0f, y = 0.0f, first_x, first_y;

	/* by the reticle: in. 66 + 16 + 28 = 110; out only past 137.5 */
	CHECK(ae_prompt_pin_keep(960.0f, 540.0f, 960.0f, 540.0f, 1.0f, 28.0f, 0) == 1);
	CHECK(ae_prompt_pin_keep(960.0f + 109.0f, 540.0f, 960.0f, 540.0f, 1.0f, 28.0f, 0) == 1);
	CHECK(ae_prompt_pin_keep(960.0f + 111.0f, 540.0f, 960.0f, 540.0f, 1.0f, 28.0f, 0) == 0);
	CHECK(ae_prompt_pin_keep(960.0f + 130.0f, 540.0f, 960.0f, 540.0f, 1.0f, 28.0f, 1) == 1);
	CHECK(ae_prompt_pin_keep(960.0f + 138.0f, 540.0f, 960.0f, 540.0f, 1.0f, 28.0f, 1) == 0);
	/* the place does not move with the target, on any side of the middle, and stays out of the zone */
	base(&in, 1.0f);
	box(&in.target, 940.0f, 520.0f, 980.0f, 560.0f);
	CHECK(ae_prompt_place_pinned(&in, &first_x, &first_y) == AE_PROMPT_BESIDE);
	CHECK(first_x >= 960.0f + 66.0f);
	box(&in.target, 900.0f, 600.0f, 940.0f, 640.0f);
	CHECK(ae_prompt_place_pinned(&in, &x, &y) == AE_PROMPT_BESIDE && x == first_x && y == first_y);
	box(&in.target, 985.0f, 470.0f, 1025.0f, 510.0f);
	CHECK(ae_prompt_place_pinned(&in, &x, &y) == AE_PROMPT_BESIDE && x == first_x && y == first_y);
	/* a text row there: the left; on both: none */
	in.row_count = 1;
	box(&in.rows[0], first_x, first_y, first_x + 120.0f, first_y + 28.0f);
	CHECK(ae_prompt_place_pinned(&in, &x, &y) == AE_PROMPT_BESIDE && x + in.width <= 960.0f - 66.0f + 1.0f);
	box(&in.rows[0], 0.0f, first_y, 1920.0f, first_y + 28.0f);
	CHECK(ae_prompt_place_pinned(&in, &x, &y) == AE_PROMPT_NONE);
}

int main(void)
{
	pin_test();
	side_keep_test();
	words_test();
	metrics_test();
	place_test();
	sweep_test();
	refuse_test();
	if (failures)
	{
		printf("%d failed\n", failures);
		return 1;
	}
	printf("ok\n");
	return 0;
}
