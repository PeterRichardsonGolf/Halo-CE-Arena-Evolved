#include <stdio.h>
#include <string.h>

#include "ae_view_bots_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a stand-in measure: 8 units a byte (the ellipsis's three bytes count as one glyph) */
static float measure(char const *text, void *context)
{
	float width = 0.0f;

	(void)context;
	for (; *text; text++)
		if (((unsigned char)*text & 0xC0) != 0x80)
			width += 8.0f;
	return width;
}

/* the names: which system chose the goal, the stock brain's by its goal's kind, the codes; roles, marks, orders */
static void name_tests(void)
{
	CHECK(!strcmp(ae_vb_source_word(AE_VB_SOURCE_VANTAGE, 0), "VANTAGE"));
	CHECK(!strcmp(ae_vb_source_word(AE_VB_SOURCE_SQUAD, 0), "SQUAD"));
	CHECK(!strcmp(ae_vb_source_word(AE_VB_SOURCE_BASE, AE_VB_GOAL_ENEMY), "HUNT"));
	CHECK(!strcmp(ae_vb_source_word(AE_VB_SOURCE_BASE, AE_VB_GOAL_ITEM), "ITEM"));
	CHECK(!strcmp(ae_vb_source_word(AE_VB_SOURCE_BASE, AE_VB_GOAL_ROAM), "ROAM"));
	CHECK(!strcmp(ae_vb_source_word(AE_VB_SOURCE_NONE, 0), "") && !strcmp(ae_vb_source_word(99, 0), ""));
	CHECK(!strcmp(ae_vb_source_code(AE_VB_SOURCE_VEHICLE), "VEH") && !strcmp(ae_vb_source_code(AE_VB_SOURCE_MARK), "MRK") &&
		!strcmp(ae_vb_source_code(AE_VB_SOURCE_ROUTE), "RTE") && !strcmp(ae_vb_source_code(AE_VB_SOURCE_BASE), "BASE"));
	CHECK(!strcmp(ae_vb_role_name(1), "DEFEND OURS") && !strcmp(ae_vb_role_name(11), "POWER") &&
		!strcmp(ae_vb_role_name(12), "ROAM") && !strcmp(ae_vb_role_name(0), "") && !strcmp(ae_vb_role_name(13), ""));
	CHECK(!strcmp(ae_vb_mark_name(0), "ENEMY") && !strcmp(ae_vb_mark_name(4), "RIDE") && !strcmp(ae_vb_mark_name(5), ""));
	CHECK(!strcmp(ae_vb_order_name(1), "FOLLOW") && !strcmp(ae_vb_order_name(6), "GO") && !strcmp(ae_vb_order_name(-1), "NONE"));
}

/* the state word's priority, fighting from the last fight's tick, metres and ages */
static void state_tests(void)
{
	char text[16];

	CHECK(ae_vb_state_pick(1, 1, 1, 1) == AE_VB_STATE_FIGHT);
	CHECK(ae_vb_state_pick(0, 1, 1, 1) == AE_VB_STATE_STUCK);
	CHECK(ae_vb_state_pick(0, 0, 1, 1) == AE_VB_STATE_NO_PATH);
	CHECK(ae_vb_state_pick(0, 0, 0, 1) == AE_VB_STATE_TETHER && ae_vb_state_pick(0, 0, 0, 0) == AE_VB_STATE_NONE);
	CHECK(!strcmp(ae_vb_state_word(AE_VB_STATE_NO_PATH), "NO PATH") && !strcmp(ae_vb_state_word(AE_VB_STATE_NONE), ""));
	CHECK(ae_vb_fighting(100, 80) && !ae_vb_fighting(100, 70) && !ae_vb_fighting(100, 120));
	CHECK(ae_vb_metres(9.0f) == 27 && ae_vb_metres(-1.0f) == 0 && ae_vb_metres(0.1f) == 0);
	ae_vb_age_text(41 * 30 + 5, text, sizeof(text));
	CHECK(!strcmp(text, "41 s"));
	ae_vb_age_text(-3, text, sizeof(text));
	CHECK(!strcmp(text, "0 s"));
	ae_vb_age_text(150 * 30, text, sizeof(text));
	CHECK(!strcmp(text, "2 m"));
}

/* the band: 676-1340 at 1920 (16 left of the card at 1356), shifted with a wider frame; a quarter view centres it */
static void band_tests(void)
{
	float box[4];

	CHECK(ae_vb_board_left(1920.0f, 0.0f, 1920.0f, 0) == 676.0f);
	CHECK(ae_vb_board_left(2560.0f, 0.0f, 2560.0f, 0) == 996.0f);
	CHECK(ae_vb_board_left(960.0f, 0.0f, 960.0f, 0) >= 8.0f);
	CHECK(ae_vb_board_left(1920.0f, 960.0f, 960.0f, 1) == 960.0f + 148.0f);
	CHECK(ae_vb_board_top(0.0f, 0) == 64.0f && ae_vb_board_top(540.0f, 1) == 588.0f && ae_vb_board_top(0.0f, 1) == 48.0f);
	ae_vb_crosshair_area(0.0f, 0.0f, 1920.0f, 1080.0f, box);
	CHECK(box[0] == 897.0f && box[1] == 477.0f && box[2] == 1023.0f && box[3] == 603.0f);
}

/* the layout: the spec's heights (BTB 64-241 with a strip, arena one squad full width), the third squad under the shorter
column, the split screen's strip rule, and what gives way when it is too tall */
static void layout_tests(void)
{
	struct ae_vb_geometry g1, g2;
	struct ae_vb_layout l;
	short btb[2] = { 3, 4 }, arena[1] = { 3 }, three[3] = { 4, 2, 3 }, five[2] = { 5, 3 }, big[2] = { 12, 3 };
	float height;

	ae_vb_geometry_of(0, &g1);
	ae_vb_geometry_of(1, &g2);
	/* (BTB 8v8: header 22, squads 22 + 4 rows of 22, a one-line strip 7 + 22, padding 8 + 8: 64 to 241) */
	height = ae_vb_layout(&g1, 0, 676.0f, 64.0f, 469.0f, btb, 2, 1, &l);
	CHECK(height == 177.0f && l.columns == 2 && l.col_w == 306.0f && l.col_x[1] == 330.0f && l.strip_lines == 1);
	CHECK(l.place[0].column == 0 && l.place[1].column == 1 && l.place[1].y == 0.0f && l.place[1].shown == 4);
	/* (no strip: 29 shorter) */
	CHECK(ae_vb_layout(&g1, 0, 676.0f, 64.0f, 469.0f, btb, 2, 0, &l) == 148.0f && l.strip_lines == 0);
	/* (arena: one squad uses both columns' room, 636) */
	ae_vb_layout(&g1, 0, 676.0f, 64.0f, 469.0f, arena, 1, 1, &l);
	CHECK(l.columns == 1 && l.col_w == 636.0f);
	/* (a third squad under the shorter column: B (2 rows) is shorter than A (4)) */
	ae_vb_layout(&g1, 0, 676.0f, 64.0f, 469.0f, three, 3, 0, &l);
	CHECK(l.place[2].column == 1 && l.place[2].y == 22.0f + 2 * 22.0f + 6.0f);
	CHECK(l.body_h == 66.0f + 6.0f + 88.0f);
	/* (2p: rows 20, no header; P1 three rows and the strip 48-165 as the spec) */
	height = ae_vb_layout(&g2, 1, 676.0f, 48.0f, 199.0f, btb, 2, 1, &l);
	CHECK(l.strip_lines == 1 && height == 6.0f + 20.0f + 4 * 20.0f + 5.0f + 20.0f + 6.0f);
	/* (2p: a squad of 5 drops the strip) */
	ae_vb_layout(&g2, 1, 676.0f, 48.0f, 199.0f, five, 2, 1, &l);
	CHECK(l.strip_lines == 0 && 48.0f + l.height <= 199.0f);
	/* (too tall: the strip goes first, then the long column is cut, its last line counting the rest) */
	height = ae_vb_layout(&g1, 0, 676.0f, 64.0f, 300.0f, big, 2, 2, &l);
	CHECK(l.strip_lines == 0 && 64.0f + height <= 300.0f);
	CHECK(l.place[0].shown >= 1 && l.place[0].more == 12 - l.place[0].shown && l.place[1].shown == 3 && l.place[1].more == 0);
	CHECK(l.body_h == 22.0f + (float)(l.place[0].shown + 1) * 22.0f);
}

/* the strip: the focus bot's WHY at any age; else the newest under 30 s; else none */
static void strip_tests(void)
{
	CHECK(ae_vb_strip_pick(1, 1, 1, 10) == AE_VB_STRIP_FOCUS);
	CHECK(ae_vb_strip_pick(1, 0, 1, 10) == AE_VB_STRIP_NEWEST);
	CHECK(ae_vb_strip_pick(0, 0, 1, 899) == AE_VB_STRIP_NEWEST);
	CHECK(ae_vb_strip_pick(0, 0, 1, 900) == AE_VB_STRIP_NONE);
	CHECK(ae_vb_strip_pick(0, 0, 0, 0) == AE_VB_STRIP_NONE);
}

/* the tags: over the label 8 clear; after it when blocked; the number alone; faint in the crosshair area */
static void tag_tests(void)
{
	struct ae_vb_box label = { 1158.0f, 432.0f, 1262.0f, 460.0f }, view = { 0.0f, 0.0f, 1920.0f, 1080.0f }, box;
	struct ae_vb_box blocks[2];
	float area[4] = { 897.0f, 477.0f, 1023.0f, 603.0f };
	int kind;

	kind = ae_vb_tag_place(&label, 80.0f, 10.0f, 22.0f, NULL, 0, &view, 8.0f, &box);
	CHECK(kind == AE_VB_TAG_ABOVE && box.y1 == 424.0f && box.y0 == 402.0f && box.x0 == 1170.0f && box.x1 == 1250.0f);
	/* (a label over it: after the name, 8 to its right, centred on it) */
	blocks[0].x0 = 1180.0f; blocks[0].y0 = 380.0f; blocks[0].x1 = 1240.0f; blocks[0].y1 = 410.0f;
	kind = ae_vb_tag_place(&label, 80.0f, 10.0f, 22.0f, blocks, 1, &view, 8.0f, &box);
	CHECK(kind == AE_VB_TAG_AFTER && box.x0 == 1270.0f && box.y0 == 435.0f);
	/* (both blocked, and the right too: the number alone over it, a 22 circle, which the first block still meets) */
	blocks[1].x0 = 1265.0f; blocks[1].y0 = 400.0f; blocks[1].x1 = 1400.0f; blocks[1].y1 = 500.0f;
	kind = ae_vb_tag_place(&label, 80.0f, 10.0f, 22.0f, blocks, 2, &view, 8.0f, &box);
	CHECK(kind == AE_VB_TAG_FAINT && box.x1 - box.x0 == 22.0f);
	CHECK(ae_vb_tag_alpha(&box, area, kind == AE_VB_TAG_FAINT) == AE_VB_FADE);
	/* (a narrow block over the middle only: the number fits beside it? no: centred, it meets it; after is blocked) */
	blocks[0].x0 = 1300.0f; blocks[0].y0 = 380.0f; blocks[0].x1 = 1400.0f; blocks[0].y1 = 500.0f;
	kind = ae_vb_tag_place(&label, 200.0f, 10.0f, 22.0f, blocks, 1, &view, 8.0f, &box);
	CHECK(kind == AE_VB_TAG_NUMBER && box.y1 == 424.0f);
	/* (the gap rule: exactly 8 is clear, 7 is not) */
	{
		struct ae_vb_box a = { 0.0f, 0.0f, 10.0f, 10.0f }, b = { 18.0f, 0.0f, 30.0f, 10.0f }, c = { 17.0f, 0.0f, 30.0f, 10.0f };

		CHECK(ae_vb_box_gap(&a, &b) == 8.0f && ae_vb_box_gap(&a, &c) == 7.0f && ae_vb_box_gap(&a, &a) == 0.0f);
	}
	/* (in the crosshair area: faint) */
	box.x0 = 1000.0f; box.y0 = 500.0f; box.x1 = 1080.0f; box.y1 = 522.0f;
	CHECK(ae_vb_tag_alpha(&box, area, 0) == AE_VB_FADE);
	box.x0 = 1030.0f; box.x1 = 1110.0f;
	CHECK(ae_vb_tag_alpha(&box, area, 0) == 1.0f);
	/* (outside the view: not a free place) */
	label.x0 = 1850.0f; label.x1 = 1915.0f;
	kind = ae_vb_tag_place(&label, 120.0f, 10.0f, 22.0f, NULL, 0, &view, 8.0f, &box);
	CHECK(kind == AE_VB_TAG_NUMBER && box.x1 <= 1920.0f);
}

/* the pips: a direction from the view, the edge it meets, sliding clear of a block */
static void pip_tests(void)
{
	struct ae_vb_box view = { 0.0f, 0.0f, 1920.0f, 1080.0f }, block, box;
	float dx, dy, x, y;
	int side;

	/* (to the right, in front) */
	ae_vb_off_direction(5.0f, 0.0f, -1.0f, &dx, &dy);
	CHECK(dx == 1.0f && dy == 0.0f);
	side = ae_vb_pip_anchor(dx, dy, &view, 34.0f, &x, &y);
	CHECK(side == AE_VB_SIDE_RIGHT && x == 1886.0f && y == 540.0f);
	/* (behind, a little left: the bottom edge, left of the middle) */
	ae_vb_off_direction(-1.0f, 0.0f, 5.0f, &dx, &dy);
	CHECK(dy == 1.0f && dx < 0.0f && dx > -1.0f);
	side = ae_vb_pip_anchor(dx, dy, &view, 34.0f, &x, &y);
	CHECK(side == AE_VB_SIDE_BOTTOM && y == 1046.0f && x < 960.0f);
	/* (above, in front) */
	ae_vb_off_direction(0.0f, 3.0f, -1.0f, &dx, &dy);
	CHECK(dy == -1.0f && ae_vb_pip_anchor(dx, dy, &view, 34.0f, &x, &y) == AE_VB_SIDE_TOP && y == 34.0f);
	/* (a block where it would go: slid along the edge, 8 clear) */
	CHECK(ae_vb_pip_place(AE_VB_SIDE_RIGHT, 1886.0f, 540.0f, 150.0f, 24.0f, 12.0f, NULL, 0, &view, 8.0f, &box));
	CHECK(box.x1 == 1886.0f && box.x0 == 1886.0f - 162.0f && box.y0 == 528.0f);
	block.x0 = 1700.0f; block.y0 = 500.0f; block.x1 = 1900.0f; block.y1 = 560.0f;
	CHECK(ae_vb_pip_place(AE_VB_SIDE_RIGHT, 1886.0f, 540.0f, 150.0f, 24.0f, 12.0f, &block, 1, &view, 8.0f, &box));
	CHECK(ae_vb_box_gap(&box, &block) >= 8.0f && box.x1 == 1886.0f);
	/* (nowhere free: none) */
	block.x0 = 0.0f; block.y0 = 0.0f; block.x1 = 1920.0f; block.y1 = 1080.0f;
	CHECK(!ae_vb_pip_place(AE_VB_SIDE_LEFT, 34.0f, 540.0f, 150.0f, 24.0f, 12.0f, &block, 1, &view, 8.0f, &box));
	/* (together: within 30 m of each other) */
	{
		float near[3][3] = { { 0.0f, 0.0f, 0.0f }, { 5.0f, 0.0f, 0.0f }, { 0.0f, 8.0f, 0.0f } };
		float far[2][3] = { { 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f } };

		CHECK(ae_vb_together((float const (*)[3])near, 3));
		CHECK(!ae_vb_together((float const (*)[3])far, 2));
		CHECK(ae_vb_together((float const (*)[3])far, 1));
	}
}

/* the aim: the nearest within 6 degrees, named; held 1 s */
static void aim_tests(void)
{
	float degrees[4] = { 5.0f, 2.0f, 1.0f, 7.0f };
	int eligible[4] = { 1, 1, 0, 1 };

	CHECK(ae_vb_aim_pick(degrees, eligible, 4) == 1);
	eligible[1] = 0;
	CHECK(ae_vb_aim_pick(degrees, eligible, 4) == 0);
	eligible[0] = 0;
	CHECK(ae_vb_aim_pick(degrees, eligible, 4) == -1);
	CHECK(ae_vb_aim_hold(3, 100, -1, 129) == 3 && ae_vb_aim_hold(3, 100, -1, 130) == -1 && ae_vb_aim_hold(3, 100, 2, 101) == 2);
	CHECK(ae_vb_aim_hold(-1, 100, -1, 101) == -1);
}

/* fitting text: cut with an ellipsis, no dot or space left before it; wrapped at spaces, the last line cut */
static void text_tests(void)
{
	char out[64];
	char lines[2][AE_VB_LINE];
	int used;

	ae_vb_fit("rockets", 100.0f, measure, NULL, out, sizeof(out));
	CHECK(!strcmp(out, "rockets"));
	ae_vb_fit("rockets \xC2\xB7 64 m", 72.0f, measure, NULL, out, sizeof(out));
	CHECK(!strcmp(out, "rockets\xE2\x80\xA6"));
	ae_vb_fit("abcdefghij", 40.0f, measure, NULL, out, sizeof(out));
	CHECK(!strcmp(out, "abcd\xE2\x80\xA6"));
	used = ae_vb_wrap("could not get to the vehicle in time", 1000.0f, 2, measure, NULL, lines);
	CHECK(used == 1 && !strcmp(lines[0], "could not get to the vehicle in time"));
	used = ae_vb_wrap("could not get to the vehicle in time", 160.0f, 2, measure, NULL, lines);
	CHECK(used == 2 && !strcmp(lines[0], "could not get to the") && !strcmp(lines[1], "vehicle in time"));
	used = ae_vb_wrap("could not get to the vehicle in time at all, ever", 160.0f, 2, measure, NULL, lines);
	CHECK(used == 2 && measure(lines[1], NULL) <= 160.0f && strstr(lines[1], "\xE2\x80\xA6") != NULL);
	CHECK(ae_vb_wrap("", 100.0f, 2, measure, NULL, lines) == 0 && ae_vb_wrap(NULL, 100.0f, 2, measure, NULL, lines) == 0);
}

int main(void)
{
	name_tests();
	state_tests();
	band_tests();
	layout_tests();
	strip_tests();
	tag_tests();
	pip_tests();
	aim_tests();
	text_tests();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_view_bots_rules: ok\n");
	return 0;
}
