/* ae_marker_rules_test.c: a marker's size, the feed's merged lines and words, which message slot a line takes, and which
rows the open wheel hides (ae_marker_rules.h). */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ae_marker_rules.h"
#include "ae_mark_rules.h"
#include "ae_wheel_rules.h"
#include "ae_waypoint_icons.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, tolerance) do { double x_ = (a), y_ = (b); if (fabs(x_ - y_) > (tolerance)) { \
	printf("FAIL %s:%d %s = %f, wanted %f\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)
#define SAME(text, wanted) do { if (wcscmp((text), (wanted))) { printf("FAIL %s:%d %s: wanted %ls, got %ls\n", __FILE__, \
	__LINE__, #text, (wanted), (text)); failures++; } } while (0)

/* ---------- marker_size */

static void marker_size_test(void)
{
	/* 36 x scale x f(e), whole pixels: 30 m at the waypoints' own scale is 1 here */
	NEAR(ae_marker_size(1.0f, 0.21f), 26.0f, 0.0001);
	NEAR(ae_marker_size(1.0f, 0.72f), 36.0f, 0.0001);
	NEAR(ae_marker_size(1.0f, 0.98f), 43.0f, 0.0001);
	/* f: 0.72 to 0.30, linear to 1 at 0.50, 1 to 0.70, linear to 1.20 at 1.00, then 1.20 */
	NEAR(ae_marker_f(0.0f), 0.72f, 0.0001);
	NEAR(ae_marker_f(0.30f), 0.72f, 0.0001);
	NEAR(ae_marker_f(0.40f), 0.86f, 0.0001);
	NEAR(ae_marker_f(0.50f), 1.0f, 0.0001);
	NEAR(ae_marker_f(0.60f), 1.0f, 0.0001);
	NEAR(ae_marker_f(0.70f), 1.0f, 0.0001);
	NEAR(ae_marker_f(0.85f), 1.1f, 0.0001);
	NEAR(ae_marker_f(1.00f), 1.2f, 0.0001);
	NEAR(ae_marker_f(1.50f), 1.2f, 0.0001);
	NEAR(ae_marker_size(1.0f, 0.40f), 31.0f, 0.0001);
	NEAR(ae_marker_size(1.0f, 1.0f), 43.0f, 0.0001);
	NEAR(ae_marker_size(1.0f, 3.0f), 43.0f, 0.0001);
	/* halves round down: 36 x 0.5 x 0.72 is 12.96, so the floor decides; 20.5 and 21.5 round to 20 and 21 */
	NEAR(ae_marker_size(1.0f, 0.60f), 36.0f, 0.0001);
	NEAR(ae_marker_size(20.5f / 36.0f, 0.60f), 20.0f, 0.0001);
	NEAR(ae_marker_size(21.5f / 36.0f, 0.60f), 21.0f, 0.0001);
	/* the floor is 20 */
	NEAR(ae_marker_size(0.3f, 0.10f), 20.0f, 0.0001);
	NEAR(ae_marker_size(0.0f, 0.50f), 20.0f, 0.0001);
	NEAR(ae_marker_size(-1.0f, 0.50f), 20.0f, 0.0001);
	NEAR(ae_marker_size(NAN, 0.50f), 20.0f, 0.0001);
	NEAR(ae_marker_size(1.0f, NAN), 36.0f, 0.0001);
	/* a nearer waypoint scale makes it larger */
	CHECK(ae_marker_size(2.0f, 0.60f) > ae_marker_size(1.0f, 0.60f));
	/* e: the ellipse's radii, 840 and 340 at 1080p */
	NEAR(ae_marker_e(840.0f, 0.0f, 1920.0f, 1080.0f), 1.0f, 0.0001);
	NEAR(ae_marker_e(0.0f, 340.0f, 1920.0f, 1080.0f), 1.0f, 0.001);
	NEAR(ae_marker_e(-420.0f, 0.0f, 1920.0f, 1080.0f), 0.5f, 0.0001);
	NEAR(ae_marker_e(0.0f, 0.0f, 1920.0f, 1080.0f), 0.0f, 0.0001);
	NEAR(ae_marker_e(420.0f, 170.0f, 1920.0f, 1080.0f), sqrtf(0.25f + 0.25f), 0.002);
	/* a view of another size has its own ellipse */
	NEAR(ae_marker_e(420.0f, 0.0f, 960.0f, 540.0f), 1.0f, 0.0001);
	NEAR(ae_marker_e(210.0f, 0.0f, 960.0f, 540.0f), 0.5f, 0.0001);
	NEAR(ae_marker_e(1.0f, 1.0f, 0.0f, 0.0f), 1.0f, 0.0001);
	/* the size eases to its target over 120 ms */
	NEAR(ae_marker_ease(20.0f, 40.0f, 0), 20.0f, 0.0001);
	CHECK(ae_marker_ease(20.0f, 40.0f, 30) > 20.0f && ae_marker_ease(20.0f, 40.0f, 30) < 40.0f);
	CHECK(ae_marker_ease(20.0f, 40.0f, 120) > 39.0f);
	NEAR(ae_marker_ease(20.0f, 40.0f, 100000), 40.0f, 0.001);
	NEAR(ae_marker_ease(40.0f, 20.0f, 1000), 20.0f, 0.01);
	NEAR(ae_marker_ease(20.0f, 40.0f, -5), 20.0f, 0.0001);
	/* a repeat: 1.12 a step, two steps at most, and a number off the network is bound */
	NEAR(ae_marker_repeat_scale(0), 1.0f, 0.0001);
	NEAR(ae_marker_repeat_scale(1), 1.12f, 0.0001);
	NEAR(ae_marker_repeat_scale(2), 1.2544f, 0.0001);
	NEAR(ae_marker_repeat_scale(3), 1.2544f, 0.0001);
	NEAR(ae_marker_repeat_scale(255), 1.2544f, 0.0001);
	NEAR(ae_marker_repeat_scale(-4), 1.0f, 0.0001);
}

/* ---------- the OK flash and names */

static void ok_test(void)
{
	float radius, alpha, lift;

	CHECK(ae_marker_flash(0, &radius, &alpha, &lift));
	NEAR(radius, 0.0f, 0.0001);
	NEAR(alpha, 1.0f, 0.0001);
	NEAR(lift, 1.15f, 0.0001);
	CHECK(ae_marker_flash(120, &radius, &alpha, &lift));
	CHECK(radius > 19.0f && radius < 38.0f);
	CHECK(alpha > 0.0f && alpha < 1.0f);
	CHECK(lift > 1.0f && lift < 1.15f);
	/* the lift is over by 160 ms, the ring at 240 */
	CHECK(ae_marker_flash(160, &radius, &alpha, &lift));
	NEAR(lift, 1.0f, 0.0001);
	CHECK(ae_marker_flash(239, &radius, &alpha, &lift));
	CHECK(radius > 36.0f && radius <= 38.0f);
	CHECK(!ae_marker_flash(240, &radius, &alpha, &lift));
	CHECK(!ae_marker_flash(5000, &radius, &alpha, &lift));
	CHECK(!ae_marker_flash(-1, &radius, &alpha, &lift));
	{
		int shown, more;

		ae_ok_names_split(0, &shown, &more);
		CHECK(shown == 0 && more == 0);
		ae_ok_names_split(1, &shown, &more);
		CHECK(shown == 1 && more == 0);
		ae_ok_names_split(3, &shown, &more);
		CHECK(shown == 3 && more == 0);
		ae_ok_names_split(5, &shown, &more);
		CHECK(shown == 3 && more == 2);
		ae_ok_names_split(-2, &shown, &more);
		CHECK(shown == 0 && more == 0);
	}
}

/* ---------- the lead line */

static void lead_in(struct ae_marker_lead_in *in, float s, float gx, float gy, float size, float width, float height)
{
	in->view_width = 1920.0f * s;
	in->view_height = 1080.0f * s;
	in->s = s;
	in->glyph_x = gx;
	in->glyph_y = gy;
	in->glyph_size = size;
	in->block_width = width;
	in->block_height = height;
}

static void lead_test(void)
{
	struct ae_marker_lead_in in;
	struct ae_marker_lead_out out;

	/* a glyph right of the middle: out to the right. Its upper right corner is (1053, 464): the line starts 6 off it, goes
	16 across and 22 up, and the rows stand 9 above the shelf, which runs 4 past the widest (127) */
	lead_in(&in, 1.0f, 1040.0f, 477.0f, 26.0f, 127.0f, 72.0f);
	ae_marker_lead(&in, &out);
	CHECK(out.dir == 1 && !out.raised && !out.flipped);
	NEAR(out.start_x, 1059.0f, 0.001);
	NEAR(out.start_y, 458.0f, 0.001);
	NEAR(out.elbow_x, 1075.0f, 0.001);
	NEAR(out.elbow_y, 436.0f, 0.001);
	NEAR(out.shelf_end_x, 1075.0f + 131.0f, 0.001);
	NEAR(out.rows_bottom, 427.0f, 0.001);
	NEAR(out.box[0], 1075.0f, 0.001);
	NEAR(out.box[1], 355.0f, 0.001);
	NEAR(out.box[2], 1206.0f, 0.001);
	NEAR(out.box[3], 436.0f, 0.001);
	/* left of the middle: out to the left, the rows ending at the elbow */
	lead_in(&in, 1.0f, 800.0f, 600.0f, 22.0f, 139.0f, 48.0f);
	ae_marker_lead(&in, &out);
	CHECK(out.dir == -1 && !out.raised);
	NEAR(out.start_x, 800.0f - 11.0f - 6.0f, 0.001);
	NEAR(out.start_y, 600.0f - 11.0f - 6.0f, 0.001);
	NEAR(out.elbow_x, out.start_x - 16.0f, 0.001);
	NEAR(out.elbow_y, out.start_y - 22.0f, 0.001);
	NEAR(out.shelf_end_x, out.elbow_x - 143.0f, 0.001);
	NEAR(out.box[0], out.shelf_end_x, 0.001);
	NEAR(out.box[2], out.elbow_x, 0.001);
	/* a view's scale takes all of it: 0.85 */
	lead_in(&in, 0.85f, 1000.0f, 400.0f, 20.0f, 100.0f, 40.0f);
	ae_marker_lead(&in, &out);
	NEAR(out.elbow_x - out.start_x, 16.0f * 0.85f, 0.001);
	NEAR(out.start_y - out.elbow_y, 22.0f * 0.85f, 0.001);
	NEAR(out.elbow_y - out.rows_bottom, 9.0f * 0.85f, 0.001);
	/* at the right edge the lead flips toward the middle (a corner of the glyph on the middle's side) */
	lead_in(&in, 1.0f, 1869.0f, 760.0f, 22.0f, 122.0f, 30.0f);
	ae_marker_lead(&in, &out);
	CHECK(out.dir == -1 && out.flipped && !out.raised);
	NEAR(out.start_x, 1869.0f - 11.0f - 6.0f, 0.001);
	CHECK(out.box[0] >= 8.0f && out.box[2] <= 1920.0f - 8.0f);
	/* the left edge flips to the right */
	lead_in(&in, 1.0f, 60.0f, 700.0f, 22.0f, 122.0f, 30.0f);
	ae_marker_lead(&in, &out);
	CHECK(out.dir == 1 && out.flipped);
	/* a block that would meet the reticle's zone rises 36: a glyph below the middle, its rows over the crosshair */
	lead_in(&in, 1.0f, 990.0f, 640.0f, 22.0f, 100.0f, 72.0f);
	ae_marker_lead(&in, &out);
	CHECK(out.raised);
	NEAR(out.start_y - out.elbow_y, 36.0f, 0.001);
	NEAR(out.box[3], out.elbow_y, 0.001);
	/* the same, a glyph far off the crosshair: not */
	lead_in(&in, 1.0f, 1500.0f, 640.0f, 22.0f, 100.0f, 72.0f);
	ae_marker_lead(&in, &out);
	CHECK(!out.raised);
	/* the zone scales with the view */
	lead_in(&in, 0.75f, 990.0f * 0.75f, 640.0f * 0.75f, 20.0f, 100.0f, 72.0f);
	ae_marker_lead(&in, &out);
	CHECK(out.raised);
}

/* ---------- feed_merge */

static void feed_merge_test(void)
{
	struct ae_feed_merge merge;

	memset(&merge, 0, sizeof(merge));
	/* the first line is new; the same call by the same caller within 6 s counts x2, x3 */
	CHECK(ae_feed_merge(&merge, 3, 7, 1000) == 0);
	CHECK(ae_feed_merge(&merge, 3, 7, 1090) == 2);
	CHECK(ae_feed_merge(&merge, 3, 7, 1180) == 3);
	/* the 6 s run from the last: a third 5 s after the second keeps merging, 10 s after the first */
	CHECK(ae_feed_merge(&merge, 3, 7, 1330) == 4);
	/* exactly 6 s still merges; 6.1 s is a new line */
	CHECK(ae_feed_merge(&merge, 3, 7, 1510) == 5);
	CHECK(ae_feed_merge(&merge, 3, 7, 1693) == 0);
	CHECK(ae_feed_merge(&merge, 3, 7, 1700) == 2);
	/* another caller is a new line, and it is now the last: the first caller's next is a new line too */
	CHECK(ae_feed_merge(&merge, 4, 7, 1710) == 0);
	CHECK(ae_feed_merge(&merge, 3, 7, 1720) == 0);
	/* another call by the same caller is a new line */
	CHECK(ae_feed_merge(&merge, 3, 8, 1730) == 0);
	CHECK(ae_feed_merge(&merge, 3, 8, 1740) == 2);
	/* game time going back (a new game) is a new line, and so is a stale state */
	CHECK(ae_feed_merge(&merge, 3, 8, 10) == 0);
	CHECK(ae_feed_merge(&merge, 3, 8, 20) == 2);
	/* the count stops at the most */
	{
		int round, last = 0;

		for (round = 0; round < 200; round++)
			last = ae_feed_merge(&merge, 3, 8, 30 + round);
		CHECK(last == AE_FEED_MERGE_MOST);
	}
	/* no state: nothing to merge into */
	CHECK(ae_feed_merge(NULL, 3, 8, 30) == 0);
}

/* ---------- the words and the line */

static void feed_words_test(void)
{
	wchar_t out[64];

	CHECK(ae_feed_words(out, 64, AE_WHEEL_ENEMY, AE_MARK_KIND_ENEMY, 0, NULL, -1) > 0);
	SAME(out, L"Enemy spotted");
	ae_feed_words(out, 64, AE_WHEEL_GOING, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Going here");
	/* the place call of someone dead is "Go here" */
	ae_feed_words(out, 64, AE_WHEEL_GOING, AE_MARK_KIND_PLACE, 1, NULL, -1);
	SAME(out, L"Go here");
	ae_feed_words(out, 64, AE_WHEEL_REGROUP, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Regroup");
	ae_feed_words(out, 64, AE_WHEEL_RIDE, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Need a ride");
	ae_feed_words(out, 64, AE_WHEEL_ATTACK, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Attacking here");
	ae_feed_words(out, 64, AE_WHEEL_DEFEND, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Defending here");
	ae_feed_words(out, 64, AE_WHEEL_WATCH, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Watching here");
	/* a tap: what it is on; dead, help */
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_ITEM, 0, L"Rockets", -1);
	SAME(out, L"Rockets here");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 0, L"Fight", -1);
	SAME(out, L"Fight here");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_ENEMY, 0, NULL, -1);
	SAME(out, L"Enemy spotted");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Go here");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 1, NULL, -1);
	SAME(out, L"Help me");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_ITEM, 1, L"Rockets", -1);
	SAME(out, L"Help me");
	/* a timer: m:ss */
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 0, L"Rockets", 9);
	SAME(out, L"Rockets 0:09");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 0, L"Sniper", 125);
	SAME(out, L"Sniper 2:05");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 0, L"Sniper", 0);
	SAME(out, L"Sniper 0:00");
	/* a number off the network is bound; no noun reads as the ground */
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 0, L"Sniper", 999999);
	SAME(out, L"Sniper 99:59");
	ae_feed_words(out, 64, AE_WHEEL_SMART, AE_MARK_KIND_PLACE, 0, L"", 20);
	SAME(out, L"Go here");
	ae_feed_words(out, 64, 99, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Go here");
	ae_feed_words(out, 64, -1, AE_MARK_KIND_PLACE, 0, NULL, -1);
	SAME(out, L"Go here");
	/* no room */
	CHECK(ae_feed_words(out, 5, AE_WHEEL_ENEMY, AE_MARK_KIND_ENEMY, 0, NULL, -1) == 0);
	CHECK(ae_feed_words(NULL, 64, AE_WHEEL_ENEMY, AE_MARK_KIND_ENEMY, 0, NULL, -1) == 0);
	/* the noun: the first letter alone in capitals */
	ae_feed_noun(out, 64, L"ROCKETS");
	SAME(out, L"Rockets");
	ae_feed_noun(out, 64, L"ASSAULT RIFLE");
	SAME(out, L"Assault rifle");
	ae_feed_noun(out, 64, L"OS/CAMO");
	SAME(out, L"OS/CAMO");
	ae_feed_noun(out, 4, L"ROCKETS");
	SAME(out, L"Roc");
	ae_feed_noun(out, 64, NULL);
	SAME(out, L"");
}

static void feed_line_test(void)
{
	wchar_t out[64];
	wchar_t wanted[64];

	CHECK(ae_feed_line(out, 64, L"Jenkins", L"Rockets here", 0) > 0);
	swprintf(wanted, 64, L"%lc" L"Jenkins:" L"%lc" L" Rockets here", AE_RUN_BLUE, AE_RUN_WHITE);
	SAME(out, wanted);
	ae_feed_line(out, 64, L"Jenkins", L"Rockets here", 1);
	SAME(out, wanted);
	ae_feed_line(out, 64, L"Jenkins", L"Rockets here", 2);
	swprintf(wanted, 64, L"%lc" L"Jenkins:" L"%lc" L" Rockets here " L"%lc" L"x2", AE_RUN_BLUE, AE_RUN_WHITE, AE_RUN_BLUE);
	SAME(out, wanted);
	ae_feed_line(out, 64, L"Kojo", L"Need a ride", 12);
	swprintf(wanted, 64, L"%lc" L"Kojo:" L"%lc" L" Need a ride " L"%lc" L"x12", AE_RUN_BLUE, AE_RUN_WHITE, AE_RUN_BLUE);
	SAME(out, wanted);
	/* a name with a run in it does not colour the line */
	{
		wchar_t name[16];

		swprintf(name, 16, L"A%lcB%lc", AE_RUN_BLUE, AE_RUN_WHITE);
		wchar_t made[64], plain[64];

		ae_feed_line(made, 64, name, L"Need a ride", 0);
		swprintf(plain, 64, L"%lc" L"AB:" L"%lc" L" Need a ride", AE_RUN_BLUE, AE_RUN_WHITE);
		SAME(made, plain);
	}
	/* the runs come out */
	ae_feed_strip_runs(out);
	SAME(out, L"Kojo: Need a ride x12");
	/* the line stays in its room, whatever the name */
	CHECK(ae_feed_line(out, 12, L"A very long name indeed", L"Need a ride", 3) > 0);
	CHECK(wcslen(out) < 12);
	CHECK(ae_feed_line(out, 2, L"Jenkins", L"Rockets here", 0) == 0);
	CHECK(ae_feed_line(NULL, 64, L"Jenkins", L"Rockets here", 0) == 0);
	ae_feed_line(out, 64, NULL, NULL, 0);
	CHECK(wcslen(out) < 64);
}

/* ---------- the style */

static void style_test(void)
{
	struct ae_marker_ink ink;
	float ce[3] = { 0.2f, 0.9f, 0.3f };

	/* HUD STYLE AE: the strip's own inks, whatever CE's colour is */
	ae_marker_ink_get(1, ce, &ink);
	NEAR(ink.line[0], 178.0 / 255.0, 0.0001);
	NEAR(ink.line[1], 224.0 / 255.0, 0.0001);
	NEAR(ink.white[0], 229.0 / 255.0, 0.0001);
	NEAR(ink.blue[0], 117.0 / 255.0, 0.0001);
	NEAR(ink.blue[1], 186.0 / 255.0, 0.0001);
	NEAR(ink.red[1], 77.0 / 255.0, 0.0001);
	NEAR(ink.dark[2], 18.0 / 255.0, 0.0001);
	NEAR(ink.flash[0], 245.0 / 255.0, 0.0001);
	ae_marker_ink_get(1, NULL, &ink);
	NEAR(ink.line[2], 1.0, 0.0001);
	/* any other style: the line and the label in CE's colour, the marks' neutral white, a black keyline */
	ae_marker_ink_get(0, ce, &ink);
	NEAR(ink.line[0], 0.2, 0.0001);
	NEAR(ink.line[1], 0.9, 0.0001);
	NEAR(ink.blue[2], 0.3, 0.0001);
	NEAR(ink.white[0], 0.898, 0.0001);
	NEAR(ink.red[1], 0.298, 0.0001);
	NEAR(ink.red[2], 0.251, 0.0001);
	NEAR(ink.dark[1], 0.0, 0.0001);
	/* the name and distance row: not in ICONS */
	CHECK(ae_marker_name_row_shown(AE_WAYPOINT_TEXT));
	CHECK(ae_marker_name_row_shown(AE_WAYPOINT_BOTH));
	CHECK(!ae_marker_name_row_shown(AE_WAYPOINT_ICONS));
}

/* ---------- feed_slots */

static void slot(struct ae_message_slot *s, int valid, int is_feed, long time)
{
	s->valid = valid;
	s->is_feed = is_feed;
	s->time = time;
}

static void feed_slots_test(void)
{
	struct ae_message_slot slots[4];

	/* a free slot first, for either kind */
	slot(&slots[0], 1, 0, 100);
	slot(&slots[1], 0, 0, -1);
	slot(&slots[2], 1, 1, 50);
	slot(&slots[3], 0, 0, -1);
	CHECK(ae_message_slot_choice(slots, 4, 0) == 1);
	CHECK(ae_message_slot_choice(slots, 4, 1) == 1);
	/* every slot full: a kill replaces the oldest feed line first, then the oldest kill */
	slot(&slots[0], 1, 0, 100);
	slot(&slots[1], 1, 1, 300);
	slot(&slots[2], 1, 1, 200);
	slot(&slots[3], 1, 0, 50);
	CHECK(ae_message_slot_choice(slots, 4, 0) == 2);
	slot(&slots[2], 1, 0, 200);
	CHECK(ae_message_slot_choice(slots, 4, 0) == 1);
	slot(&slots[1], 1, 0, 300);
	/* no feed line: the oldest kill, as the game chose before (the first of equals) */
	CHECK(ae_message_slot_choice(slots, 4, 0) == 3);
	slot(&slots[0], 1, 0, 50);
	CHECK(ae_message_slot_choice(slots, 4, 0) == 0);
	/* a feed line takes the oldest feed line, and none when every row holds a kill */
	CHECK(ae_message_slot_choice(slots, 4, 1) == AE_SLOT_NONE);
	slot(&slots[2], 1, 1, 200);
	slot(&slots[1], 1, 1, 150);
	CHECK(ae_message_slot_choice(slots, 4, 1) == 1);
	/* the oldest of equal feed lines is the first */
	slot(&slots[2], 1, 1, 150);
	CHECK(ae_message_slot_choice(slots, 4, 1) == 1);
	CHECK(ae_message_slot_choice(slots, 4, 0) == 1);
	/* all feed lines: a kill and a feed line both take the oldest */
	slot(&slots[0], 1, 1, 40);
	slot(&slots[3], 1, 1, 45);
	CHECK(ae_message_slot_choice(slots, 4, 0) == 0);
	CHECK(ae_message_slot_choice(slots, 4, 1) == 0);
	/* a lone free slot at the end */
	slot(&slots[3], 0, 0, -1);
	CHECK(ae_message_slot_choice(slots, 4, 1) == 3);
	CHECK(ae_message_slot_choice(slots, 4, 0) == 3);
	/* bad input: no line */
	CHECK(ae_message_slot_choice(NULL, 4, 0) == AE_SLOT_NONE);
	CHECK(ae_message_slot_choice(slots, 0, 1) == AE_SLOT_NONE);
}

/* ---------- row_hidden_under_ring */

static void row_hidden_under_ring_test(void)
{
	/* a ring of 234 about (960, 540), a limit of 8 */
	float row[4] = { 0.0f, 0.0f, 100.0f, 20.0f };
	float word[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	/* inside radius plus limit: hidden. The row's nearest corner is (100, 20), 910 and 520 from the middle */
	row[0] = 960.0f + 100.0f; row[1] = 540.0f - 10.0f; row[2] = row[0] + 100.0f; row[3] = row[1] + 20.0f;
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	/* a row far off is not */
	row[0] = 960.0f + 400.0f; row[2] = row[0] + 100.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	/* the edge: its nearest point exactly at radius plus limit is clear; a pixel in is hidden */
	row[0] = 960.0f + 242.0f; row[1] = 540.0f - 10.0f; row[2] = row[0] + 100.0f; row[3] = row[1] + 20.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	row[0] -= 1.0f;
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	/* a row above the ring, along its top */
	row[0] = 900.0f; row[2] = 1020.0f; row[3] = 540.0f - 234.0f - 8.0f; row[1] = row[3] - 20.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	row[3] += 1.0f;
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	/* a row over the middle: hidden */
	row[0] = 900.0f; row[1] = 500.0f; row[2] = 1000.0f; row[3] = 520.0f;
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	/* the answer is the ring's alone: it does not depend on a focus, only on the radius the caller gives (the focused one's) */
	row[0] = 960.0f + 245.0f; row[1] = 530.0f; row[2] = row[0] + 80.0f; row[3] = 550.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 240.0f, NULL, 8.0f));
	/* the word's box, with the limit, hides a row that clears the ring */
	row[0] = 100.0f; row[1] = 100.0f; row[2] = 300.0f; row[3] = 120.0f;
	word[0] = 310.0f; word[1] = 90.0f; word[2] = 400.0f; word[3] = 130.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	word[0] = 308.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	word[0] = 307.0f;
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	word[0] = 250.0f;
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	/* the word's box above or below the row, with the limit */
	word[0] = 150.0f; word[1] = 128.0f; word[2] = 200.0f; word[3] = 160.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	word[1] = 127.0f;
	CHECK(ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	word[0] = 500.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	/* an empty word box is no box */
	word[0] = 150.0f; word[1] = 100.0f; word[2] = 150.0f; word[3] = 100.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, word, 8.0f));
	/* in a split view the limit is 5.44 and the ring's radius the split one's */
	row[0] = 800.0f; row[1] = 270.0f; row[2] = 900.0f; row[3] = 290.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 480.0f, 270.0f, 159.0f, NULL, 5.44f));
	row[0] = 480.0f + 159.0f + 5.0f;
	row[2] = row[0] + 100.0f;
	CHECK(ae_row_hidden_by_wheel(row, 480.0f, 270.0f, 159.0f, NULL, 5.44f));
	/* a row of no size or NaN: not hidden, nothing to draw under it */
	row[0] = NAN; row[1] = 0.0f; row[2] = 10.0f; row[3] = 10.0f;
	CHECK(!ae_row_hidden_by_wheel(row, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
	CHECK(!ae_row_hidden_by_wheel(NULL, 960.0f, 540.0f, 234.0f, NULL, 8.0f));
}

int main(void)
{
	marker_size_test();
	ok_test();
	feed_merge_test();
	feed_words_test();
	feed_line_test();
	feed_slots_test();
	style_test();
	row_hidden_under_ring_test();
	lead_test();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_marker_rules_test: ok\n");
	return 0;
}
