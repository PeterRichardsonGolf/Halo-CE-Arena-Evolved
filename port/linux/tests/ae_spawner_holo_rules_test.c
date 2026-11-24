#include <math.h>
#include <stdio.h>

#include "ae_spawner_holo_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-3f)

/* a triangle cut at x <= limit: whole, a corner kept (3), two kept (4), nothing */
static void test_clip_triangle(void)
{
	struct ae_holo_point3 triangle[3] = { {0, 0, 0}, {2, 0, 0}, {0, 2, 1} };
	struct ae_holo_point3 axis = { 1, 0, 0 }, out[4];
	int n, index;

	CHECK(ae_holo_clip_triangle(triangle, axis, 5.0f, out) == 3);
	CHECK(ae_holo_clip_triangle(triangle, axis, -1.0f, out) == 0);
	n = ae_holo_clip_triangle(triangle, axis, 1.0f, out);
	CHECK(n == 4);
	for (index = 0; index < n; index++)
		CHECK(out[index].x <= 1.0f + 1e-4f);
	{
		struct ae_holo_point3 tip[3] = { {0, 0, 0}, {4, 1, 0}, {4, -1, 0} };

		n = ae_holo_clip_triangle(tip, axis, 2.0f, out);
		CHECK(n == 3);
		CHECK(NEAR(out[0].x, 0.0f));
		CHECK(NEAR(out[1].x, 2.0f) && NEAR(out[1].y, 0.5f));
		CHECK(NEAR(out[2].x, 2.0f) && NEAR(out[2].y, -0.5f));
	}
}

/* the fill edge's line across a triangle: its two ends on the plane, none when the plane misses */
static void test_cut_triangle(void)
{
	struct ae_holo_point3 triangle[3] = { {0, 0, 0}, {4, 1, 2}, {4, -1, 2} };
	struct ae_holo_point3 axis = { 1, 0, 0 }, from, to;

	CHECK(ae_holo_cut_triangle(triangle, axis, 2.0f, &from, &to) == 1);
	CHECK(NEAR(from.x, 2.0f) && NEAR(to.x, 2.0f));
	CHECK(NEAR(from.z, 1.0f) && NEAR(to.z, 1.0f));
	CHECK(NEAR(fabsf(from.y - to.y), 1.0f));
	CHECK(ae_holo_cut_triangle(triangle, axis, 9.0f, &from, &to) == 0);
	CHECK(ae_holo_cut_triangle(triangle, axis, -1.0f, &from, &to) == 0);
}

/* the line width grows with distance and never goes under the minimum */
static void test_line_width(void)
{
	CHECK(NEAR(ae_holo_line_width(0.0f, 2.0f), AE_HOLO_LINE_WIDTH_MINIMUM));
	CHECK(NEAR(ae_holo_line_width(10.0f, 2.0f), 10.0f * AE_HOLO_RADIANS_PER_PIXEL * 2.0f));
	CHECK(ae_holo_line_width(10.0f, 2.0f) > ae_holo_line_width(5.0f, 2.0f));
}

static void test_fill_and_fade(void)
{
	CHECK(NEAR(ae_holo_fill(900, 900), 0.0f));
	CHECK(NEAR(ae_holo_fill(450, 900), 0.5f));
	CHECK(NEAR(ae_holo_fill(0, 900), 1.0f));
	CHECK(NEAR(ae_holo_fill(10, 0), 1.0f));
	CHECK(NEAR(ae_holo_fill(1000, 900), 0.0f));
	CHECK(NEAR(ae_holo_edge(2.0f, 6.0f, 0.25f), 3.0f));
	CHECK(NEAR(ae_holo_edge(2.0f, 6.0f, 3.0f), 6.0f));
	CHECK(NEAR(ae_holo_distance_alpha(3.0f), 1.0f));
	CHECK(NEAR(ae_holo_distance_alpha(AE_HOLO_FULL_DISTANCE), 1.0f));
	CHECK(NEAR(ae_holo_distance_alpha(13.0f), 0.5f));
	CHECK(NEAR(ae_holo_distance_alpha(AE_HOLO_GONE_DISTANCE), 0.0f));
	CHECK(NEAR(ae_holo_distance_alpha(40.0f), 0.0f));
}

static void test_pulse(void)
{
	long ticks;

	CHECK(NEAR(ae_holo_pulse(600), 1.0f));
	CHECK(NEAR(ae_holo_pulse(91), 1.0f));
	for (ticks = 1; ticks <= 90; ticks++)
	{
		float p = ae_holo_pulse(ticks);

		CHECK(p >= 0.55f - 1e-3f && p <= 1.0f + 1e-3f);
	}
	CHECK(ae_holo_pulse(15) < ae_holo_pulse(30));
}

/* a weapon on a spot: only one the spawner makes hides the ghost; its pose is learnt only while the spawner is ready */
static void test_seen(void)
{
	/* a weapon a player dropped or swapped onto a spot: not its weapon, so the ghost shows over it (no READY, no pose) */
	CHECK(ae_holo_seen(0, 0) == 0);
	CHECK(ae_holo_seen(0, 1) == 0);
	CHECK(ae_holo_seen(1, 0) == AE_HOLO_SEEN_PRESENT);
	CHECK(ae_holo_seen(1, 1) == (AE_HOLO_SEEN_PRESENT | AE_HOLO_SEEN_POSE));
}

/* the first ghost faces as the spawn will place the weapon: the item's facing when the game faces items, else +x */
static struct ae_holo_point3 p3(float x, float y, float z)
{
	struct ae_holo_point3 p;

	p.x = x;
	p.y = y;
	p.z = z;
	return p;
}

/* the ghost lies where its weapon lies: a pose only from a settled weapon, through the root node the game drew it by,
and the lift over the ground grows with distance */
static void test_pose_where_the_weapon_lies(void)
{
	struct ae_holo_frame node, inverse, skin;
	struct ae_holo_point3 grip;

	/* a weapon falling or knocked about teaches no pose; a settled one does */
	CHECK(!ae_holo_at_rest(0, p3(0.0f, 0.0f, -0.2f)));
	CHECK(!ae_holo_at_rest(0, p3(0.02f, 0.0f, 0.0f)));
	CHECK(ae_holo_at_rest(0, p3(0.002f, 0.0f, -0.003f)));
	CHECK(ae_holo_at_rest(1, p3(0.0f, 0.0f, -0.2f)));

	/* a weapon on its side on a floor at z -2.75, whose model sits 0.6 up its root node's left axis (its default pose):
	the node the game draws by is on the floor, so the model's points go 0.6 back down the node's left, not up */
	node.origin = p3(-4.85f, 5.67f, -2.75f);
	node.forward = p3(1.0f, 0.0f, 0.0f);
	node.left = p3(0.0f, 0.0f, 1.0f);
	node.up = p3(0.0f, -1.0f, 0.0f);
	inverse.origin = p3(0.0f, -0.6f, 0.0f);
	inverse.forward = p3(1.0f, 0.0f, 0.0f);
	inverse.left = p3(0.0f, 1.0f, 0.0f);
	inverse.up = p3(0.0f, 0.0f, 1.0f);
	ae_holo_skin(&node, &inverse, &skin);
	grip = ae_holo_frame_point(&skin, p3(0.0f, 0.6f, 0.0f));
	CHECK(NEAR(grip.x, -4.85f) && NEAR(grip.y, 5.67f) && NEAR(grip.z, -2.75f));
	CHECK(NEAR(skin.origin.z, -3.35f) && NEAR(skin.forward.x, 1.0f) && NEAR(skin.up.y, -1.0f));
	/* a turned default pose: the model's forward is the node's up */
	inverse.origin = p3(0.0f, 0.0f, 0.0f);
	inverse.forward = p3(0.0f, 0.0f, 1.0f);
	inverse.up = p3(-1.0f, 0.0f, 0.0f);
	ae_holo_skin(&node, &inverse, &skin);
	CHECK(NEAR(skin.forward.y, -1.0f) && NEAR(skin.up.x, -1.0f) && NEAR(skin.origin.z, -2.75f));

	CHECK(NEAR(ae_holo_lift(1.0f), 0.004f));
	CHECK(ae_holo_lift(12.0f) > 0.015f && ae_holo_lift(12.0f) < 0.03f);
}

/* a ghost in range and in view stays drawn at the edges: it comes in at the gone distance and with any of it ahead of the
eye, and goes only a margin past either */
static void test_view_margins(void)
{
	CHECK(ae_holo_in_range(0, AE_HOLO_GONE_DISTANCE - 0.1f));
	CHECK(!ae_holo_in_range(0, AE_HOLO_GONE_DISTANCE + 0.1f));
	CHECK(ae_holo_in_range(1, AE_HOLO_GONE_DISTANCE + 0.5f));
	CHECK(!ae_holo_in_range(1, AE_HOLO_GONE_DISTANCE + AE_HOLO_KEEP_DISTANCE + 0.1f));

	/* a ghost beside the eye (its middle a little behind) is partly in view */
	CHECK(ae_holo_in_view(0, -0.3f, 0.5f));
	CHECK(!ae_holo_in_view(0, -0.6f, 0.5f));
	CHECK(ae_holo_in_view(1, -1.2f, 0.5f));
	CHECK(!ae_holo_in_view(1, -1.6f, 0.5f));
	CHECK(ae_holo_in_view(0, 3.0f, 0.5f));
}

static void test_first_pose(void)
{
	struct ae_holo_point3 forward, up;

	ae_holo_first_pose(1, 1.5707963f, &forward, &up);
	CHECK(NEAR(forward.x, 0.0f) && NEAR(forward.y, 1.0f) && NEAR(forward.z, 0.0f));
	CHECK(NEAR(up.x, -1.0f) && NEAR(up.y, 0.0f) && NEAR(up.z, 0.0f));
	ae_holo_first_pose(0, 1.5707963f, &forward, &up);	/* the facing is not the spawn's: the placement's default */
	CHECK(NEAR(forward.x, 1.0f) && NEAR(forward.y, 0.0f) && NEAR(forward.z, 0.0f));
	CHECK(NEAR(up.x, 0.0f) && NEAR(up.y, 1.0f) && NEAR(up.z, 0.0f));
	ae_holo_first_pose(1, 0.0f, &forward, &up);
	CHECK(NEAR(forward.x, 1.0f) && NEAR(forward.y, 0.0f));
}

/* the outline: white on a POWER pad, ice on a rack, red over both while RED */
static void test_outline_ink(void)
{
	CHECK(ae_holo_outline_ink(0, 0) == AE_HOLO_INK_ICE);
	CHECK(ae_holo_outline_ink(1, 0) == AE_HOLO_INK_WHITE);
	CHECK(ae_holo_outline_ink(0, 1) == AE_HOLO_INK_RED);
	CHECK(ae_holo_outline_ink(1, 1) == AE_HOLO_INK_RED);
}

/* the shape cache: a free slot first, else the least recently used (never slot 0 just for being first) */
static void test_cache_slot(void)
{
	long used[4] = { 5, -1, 3, -1 };

	CHECK(ae_holo_cache_slot(used, 4) == 1);
	used[1] = 9;
	used[3] = 7;
	CHECK(ae_holo_cache_slot(used, 4) == 2);
	used[2] = 10;
	CHECK(ae_holo_cache_slot(used, 4) == 0);
	used[0] = 11;
	CHECK(ae_holo_cache_slot(used, 4) == 3);
	used[3] = 9;
	CHECK(ae_holo_cache_slot(used, 4) == 1);	/* a tie: the first */
	CHECK(ae_holo_cache_slot(used, 0) == 0);
}

int main(void)
{
	test_clip_triangle();
	test_cut_triangle();
	test_line_width();
	test_fill_and_fade();
	test_pulse();
	test_seen();
	test_first_pose();
	test_outline_ink();
	test_cache_slot();
	test_pose_where_the_weapon_lies();
	test_view_margins();
	printf("ae_spawner_holo_rules: %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
