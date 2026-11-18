#include <stdio.h>

#include "ae_grenade_shot_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) ((a) - (b) < 0.0001f && (b) - (a) < 0.0001f)

int main(void)
{
	struct ae_grenade_shot_point from = { 0.f, 0.f, 0.f }, to = { 10.f, 0.f, 0.f };
	struct ae_grenade_shot_point on = { 5.f, 0.f, 0.f }, off = { 5.f, 0.3f, 0.f }, behind = { -2.f, 0.f, 0.f };
	struct ae_grenade_shot_point past = { 12.f, 0.1f, 0.f }, still = { 0.f, 0.f, 0.f };
	float t;

	/* the hit size: the bounds, clamped to 0.15-0.35 (a too small or not-a-number bound is the smallest) */
	CHECK(NEAR(ae_grenade_shot_radius(0.05f), 0.15f));
	CHECK(NEAR(ae_grenade_shot_radius(0.25f), 0.25f));
	CHECK(NEAR(ae_grenade_shot_radius(1.0f), 0.35f));
	{
		volatile float zero = 0.0f;

		CHECK(NEAR(ae_grenade_shot_radius(zero / zero), 0.15f));
	}

	/* the hit test: a bullet's segment against the ball */
	CHECK(ae_grenade_shot_hit(&from, &to, &on, 0.15f, &t) && NEAR(t, 0.5f));
	CHECK(!ae_grenade_shot_hit(&from, &to, &off, 0.15f, &t));
	CHECK(ae_grenade_shot_hit(&from, &to, &off, 0.35f, &t));
	/* (behind the start and beyond the end: the nearest point of the segment is its end) */
	CHECK(!ae_grenade_shot_hit(&from, &to, &behind, 0.35f, &t) && NEAR(t, 0.0f));
	CHECK(!ae_grenade_shot_hit(&from, &to, &past, 0.35f, &t) && NEAR(t, 1.0f));
	/* (a segment of no length: the point itself) */
	CHECK(ae_grenade_shot_hit(&from, &from, &still, 0.15f, &t) && NEAR(t, 0.0f));
	CHECK(!ae_grenade_shot_hit(&from, &from, &on, 0.15f, &t));

	/* the wall test stops short of the grenade; a short ray is whole */
	CHECK(NEAR(ae_grenade_shot_wall_length(5.0f, 0.25f), 4.65f));
	CHECK(NEAR(ae_grenade_shot_wall_length(0.3f, 0.25f), 0.3f));

	/* the pickup's kind: the equipment's powerup and type first, then the globals' item, else none */
	CHECK(ae_grenade_shot_pickup_kind(1, 1, 2, -1) == 1);
	CHECK(ae_grenade_shot_pickup_kind(1, 0, 2, 1) == 0);
	CHECK(ae_grenade_shot_pickup_kind(0, 1, 2, 0) == 0);
	CHECK(ae_grenade_shot_pickup_kind(1, 5, 2, -1) == -1);
	CHECK(ae_grenade_shot_pickup_kind(1, -1, 2, 1) == 1);
	CHECK(ae_grenade_shot_pickup_kind(0, 0, 2, -1) == -1);
	CHECK(ae_grenade_shot_pickup_kind(0, 0, 2, 2) == -1);

	/* the nearest along the bullet's path wins */
	CHECK(ae_grenade_shot_nearer(0.2f, 0.5f) && !ae_grenade_shot_nearer(0.5f, 0.5f) && ae_grenade_shot_nearer(0.0f, 2.0f));

	/* the host's checks of a client's report */
	CHECK(ae_grenade_shot_report_near(1.0f, 0) && !ae_grenade_shot_report_near(1.6f, 0));
	CHECK(ae_grenade_shot_report_near(0.5f, 1) && !ae_grenade_shot_report_near(0.7f, 1) && !ae_grenade_shot_report_near(-1.0f, 1));
	CHECK(ae_grenade_shot_report_rate_allows(100, 97) && !ae_grenade_shot_report_rate_allows(100, 98));
	CHECK(ae_grenade_shot_report_rate_allows(5, 400));
	CHECK(ae_grenade_shot_is_repeat(0.5f, 110, 100) && !ae_grenade_shot_is_repeat(0.5f, 120, 100));
	CHECK(!ae_grenade_shot_is_repeat(3.0f, 110, 100) && !ae_grenade_shot_is_repeat(0.5f, 90, 100));
	CHECK(ae_grenade_shot_copy_near(2.0f) && !ae_grenade_shot_copy_near(2.5f));

	/* the message: the kind in range, no unknown flag */
	CHECK(ae_grenade_shot_message_ok(0, 2, 0) && ae_grenade_shot_message_ok(1, 2, AE_GRENADE_SHOT_FLAG_PICKUP));
	CHECK(!ae_grenade_shot_message_ok(2, 2, 0) && !ae_grenade_shot_message_ok(-1, 2, 0) && !ae_grenade_shot_message_ok(0, 2, 2));

	/* a report needs a bullet the host saw lately, its line passing the grenade */
	CHECK(ae_grenade_shot_fire_recent(100, 100) && ae_grenade_shot_fire_recent(100, 80) && !ae_grenade_shot_fire_recent(100, 79));
	CHECK(!ae_grenade_shot_fire_recent(100, 101));
	{
		struct ae_grenade_shot_point near_line = { 30.f, 1.0f, 0.f }, far_line = { 30.f, 2.0f, 0.f };
		struct ae_grenade_shot_point way_on = { 300.f, 0.f, 0.f }, behind_far = { -5.f, 0.f, 0.f };

		CHECK(ae_grenade_shot_on_ray(&from, &to, &near_line));
		CHECK(!ae_grenade_shot_on_ray(&from, &to, &far_line));
		CHECK(ae_grenade_shot_on_ray(&from, &to, &way_on));
		CHECK(!ae_grenade_shot_on_ray(&from, &to, &behind_far));
		CHECK(!ae_grenade_shot_on_ray(&from, &from, &on));
	}

	if (failures)
		return 1;
	printf("ae_grenade_shot_rules: ok\n");
	return 0;
}
