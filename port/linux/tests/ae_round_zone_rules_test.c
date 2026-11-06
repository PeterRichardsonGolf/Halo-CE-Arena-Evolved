#include <math.h>
#include <stdio.h>
#include "ae_round_zone_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 0.01)

int main(void)
{
	struct ae_zone_plan plan;
	real shield, body, centre_x, centre_y;
	real points[3][2] = { { 10.0f, 0.0f }, { -20.0f, 0.0f }, { 0.0f, 5.0f } };
	int seconds, killed;

	/* a small map: 60 s, 30 s closing to a tenth, 20 s, 30 s to nothing */
	ae_zone_plan_make(&plan, 20.0f, 60, 30);
	CHECK(plan.wait_seconds == 60 && plan.closing_seconds == 30 && plan.wait_2_seconds == 20 &&
		plan.closing_2_seconds == 30);
	CHECK(NEAR(plan.mid_radius, 2.0f));
	CHECK(ae_zone_phase_at(&plan, 0) == AE_ZONE_PHASE_WAIT && NEAR(ae_zone_radius_at(&plan, 0), 20.0f));
	CHECK(ae_zone_phase_ticks_left(&plan, 0) == 1800);
	CHECK(ae_zone_phase_at(&plan, 1799) == AE_ZONE_PHASE_WAIT);
	CHECK(ae_zone_phase_at(&plan, 1800) == AE_ZONE_PHASE_CLOSING && NEAR(ae_zone_radius_at(&plan, 1800), 20.0f));
	CHECK(NEAR(ae_zone_radius_at(&plan, 1800 + 450), 11.0f));
	CHECK(NEAR(ae_zone_next_radius(&plan, 1800), 2.0f));
	CHECK(ae_zone_phase_at(&plan, 2700) == AE_ZONE_PHASE_WAIT_2 && NEAR(ae_zone_radius_at(&plan, 2700), 2.0f));
	CHECK(ae_zone_phase_at(&plan, 3300) == AE_ZONE_PHASE_CLOSING_2 && NEAR(ae_zone_radius_at(&plan, 3750), 1.0f));
	CHECK(NEAR(ae_zone_next_radius(&plan, 3300), 0.0f));
	CHECK(ae_zone_phase_at(&plan, 4200) == AE_ZONE_PHASE_CLOSED && NEAR(ae_zone_radius_at(&plan, 4200), 0.0f));
	CHECK(ae_zone_phase_ticks_left(&plan, 4200) == 0 && ae_zone_phase_ticks_left(&plan, 3300) == 900);

	/* a big map: the first closing of 54 units takes 48 s, its edge at half a walking speed */
	ae_zone_plan_make(&plan, 60.0f, 60, 30);
	CHECK(plan.closing_seconds == 48 && plan.closing_2_seconds == 30);
	for (seconds = 60; seconds < 108; seconds++)
	{
		real step = ae_zone_radius_at(&plan, (long)seconds * 30) - ae_zone_radius_at(&plan, (long)(seconds + 1) * 30);

		CHECK(step <= AE_ZONE_WALK_SPEED * 0.5f + 0.01f);
	}
	/* (no phase past the wire's byte; a radius of nothing is the least one) */
	ae_zone_plan_make(&plan, 5000.0f, 60, 30);
	CHECK(plan.closing_seconds == AE_ZONE_LONGEST_PHASE_SECONDS);
	ae_zone_plan_make(&plan, 0.0f, 0, 0);
	CHECK(NEAR(plan.start_radius, AE_ZONE_LEAST_RADIUS) && plan.wait_seconds == 1);

	/* inside: the edge counts */
	CHECK(ae_zone_inside(0.0f, 0.0f, 5.0f, 3.0f, 4.0f) && !ae_zone_inside(0.0f, 0.0f, 5.0f, 3.0f, 4.1f));

	/* damage: eight seconds from full, shield first; with no shield, the body alone */
	shield = 1.0f, body = 1.0f, killed = 0;
	for (seconds = 1; seconds <= 8 && !killed; seconds++)
	{
		killed = ae_zone_damage(75.0f, 75.0f, &shield, &body);
		if (seconds == 4)
			CHECK(NEAR(shield, 0.0f) && NEAR(body, 1.0f));
		if (seconds < 8)
			CHECK(!killed);
	}
	CHECK(killed && seconds == 9 && body == 0.0f);
	shield = 0.0f, body = 1.0f;
	for (seconds = 1, killed = 0; seconds <= 8 && !killed; seconds++)
		killed = ae_zone_damage(0.0f, 75.0f, &shield, &body);
	CHECK(killed && seconds == 9);
	/* (an overshield takes the first seconds; a unit of no body dies) */
	shield = 3.0f, body = 1.0f;
	CHECK(!ae_zone_damage(75.0f, 75.0f, &shield, &body) && NEAR(shield, 2.75f));
	shield = 0.0f, body = 1.0f;
	CHECK(ae_zone_damage(0.0f, 0.0f, &shield, &body) && body == 0.0f);

	/* the centre: a fair hill sits about as far from both teams' starts */
	CHECK(ae_zone_fair_hill(0.0f, 0.0f, -50.0f, 0.0f, 50.0f, 0.0f));
	CHECK(ae_zone_fair_hill(5.0f, 20.0f, -50.0f, 0.0f, 50.0f, 0.0f));
	CHECK(!ae_zone_fair_hill(30.0f, 0.0f, -50.0f, 0.0f, 50.0f, 0.0f));
	/* the start radius: the farthest spawn and a tenth more */
	CHECK(NEAR(ae_zone_start_radius(0.0f, 0.0f, points, 3), 22.0f));
	CHECK(NEAR(ae_zone_start_radius(0.0f, 0.0f, points, 0), AE_ZONE_LEAST_RADIUS));
	/* a point toward the centre */
	ae_zone_point_toward(0.0f, 0.0f, 30.0f, 40.0f, 10.0f, &centre_x, &centre_y);
	CHECK(NEAR(centre_x, 6.0f) && NEAR(centre_y, 8.0f));
	ae_zone_point_toward(1.0f, 1.0f, 1.0f, 1.0f, 10.0f, &centre_x, &centre_y);
	CHECK(NEAR(centre_x, 1.0f) && NEAR(centre_y, 1.0f));

	/* every plan closes all the way: radius 0 at the end of the last closing (never a safe circle left), whatever the start */
	{
		static real const starts[] = { 3.0f, 20.0f, 55.0f, 160.0f, 400.0f };
		int at;

		for (at = 0; at < (int)(sizeof(starts) / sizeof(starts[0])); at++)
		{
			long total, tick;
			real last = 1e9f;

			ae_zone_plan_make(&plan, starts[at], 60, 30);
			total = (long)(plan.wait_seconds + plan.closing_seconds + plan.wait_2_seconds + plan.closing_2_seconds) *
				AE_ZONE_TICKS_PER_SECOND;
			CHECK(NEAR(ae_zone_radius_at(&plan, total), 0.0f) && ae_zone_phase_at(&plan, total) == AE_ZONE_PHASE_CLOSED);
			CHECK(ae_zone_radius_at(&plan, total - 1) < plan.mid_radius * 0.05f + 0.01f);
			for (tick = 0; tick <= total; tick += 15)
			{
				real r = ae_zone_radius_at(&plan, tick);

				CHECK(r <= last + 0.0001f);
				last = r;
			}
			CHECK(ae_zone_next_radius(&plan, total - 10) == 0.0f);
		}
	}
	/* the clock cell, the warnings, the edge distance, the tint, the pulse, the two looks */
	{
		long left;
		real alpha, ramp, alpha_top, bright = 0.0f;
		int step;

		ae_zone_plan_make(&plan, 20.0f, 60, 30);
		CHECK(ae_zone_clock(&plan, 0, &left) == AE_ZONE_CLOCK_COUNT && left == 60);
		CHECK(ae_zone_clock(&plan, 1, &left) == AE_ZONE_CLOCK_COUNT && left == 60);
		CHECK(ae_zone_clock(&plan, 1799, &left) == AE_ZONE_CLOCK_COUNT && left == 1);
		CHECK(ae_zone_clock(&plan, 1800, &left) == AE_ZONE_CLOCK_CLOSING && left == 30);
		CHECK(ae_zone_clock(&plan, 2700, &left) == AE_ZONE_CLOCK_COUNT && left == 20);
		CHECK(ae_zone_clock(&plan, 3300, &left) == AE_ZONE_CLOCK_FINAL);
		CHECK(ae_zone_clock(&plan, 4200, &left) == AE_ZONE_CLOCK_FINAL && left == 0);
		CHECK(ae_zone_warning(&plan, 0) == 0 && ae_zone_warning(&plan, 900) == 1 && ae_zone_warning(&plan, 1500) == 2);
		CHECK(ae_zone_warning(&plan, 1800) == 3 && ae_zone_warning(&plan, 2700) == 0 && ae_zone_warning(&plan, 3300) == 4);
		ae_zone_plan_make(&plan, 20.0f, 20, 30);
		CHECK(ae_zone_warning(&plan, 0) == 0 && ae_zone_warning(&plan, 400) == 2);
		CHECK(NEAR(ae_zone_edge_distance(0.0f, 0.0f, 10.0f, 6.0f, 8.0f), 0.0f));
		CHECK(ae_zone_edge_distance(0.0f, 0.0f, 10.0f, 3.0f, 0.0f) > 6.9f &&
			ae_zone_edge_distance(0.0f, 0.0f, 10.0f, 13.0f, 0.0f) < -2.9f);
		CHECK(NEAR(ae_zone_tint_alpha(0.0f, 0.5f), AE_ZONE_TINT_ALPHA) && NEAR(ae_zone_tint_alpha(0.5f, 0.5f), 0.0f));
		CHECK(ae_zone_tint_alpha(0.5f, 0.0f) > 0.29f && NEAR(ae_zone_tint_alpha(0.3f, 0.3f), 0.0f));
		CHECK(ae_zone_tint_alpha(0.05f, 0.5f) > ae_zone_tint_alpha(0.15f, 0.5f));
		CHECK(NEAR(ae_zone_pulse_alpha(0), AE_ZONE_PULSE_ALPHA) && NEAR(ae_zone_pulse_alpha(AE_ZONE_PULSE_TICKS), 0.0f) &&
			NEAR(ae_zone_pulse_alpha(-1), 0.0f));
		CHECK(NEAR(ae_zone_wall_height('a'), 10.0f) && NEAR(ae_zone_wall_height('b'), 5.0f));
		/* A and B: translucent everywhere, a bright foot, the top thin; B's ribs repeat every AE_ZONE_RIB_SPACING */
		for (step = 0; step < 200; step++)
		{
			ae_zone_wall_sample('a', (real)(step % 10), (real)step * 0.37f, step * 7L, &alpha, &ramp);
			CHECK(alpha >= 0.0f && alpha <= 1.0f && ramp >= 0.0f && ramp <= 1.0f);
			ae_zone_wall_sample('b', (real)(step % 5), (real)step * 0.37f, step * 7L, &alpha, &ramp);
			CHECK(alpha >= 0.0f && alpha <= 1.0f && ramp >= 0.0f && ramp <= 1.0f);
		}
		ae_zone_wall_sample('a', 0.0f, 3.0f, 0, &alpha, &ramp);
		ae_zone_wall_sample('a', 9.9f, 3.0f, 0, &alpha_top, &ramp);
		CHECK(alpha >= 0.6f && alpha_top < 0.1f);
		ae_zone_wall_sample('b', 2.0f, 0.75f, 0, &alpha, &ramp);
		ae_zone_wall_sample('b', 2.0f, 0.75f + AE_ZONE_RIB_SPACING, 0, &alpha_top, &ramp);
		CHECK(NEAR(alpha, alpha_top));
		ae_zone_wall_sample('b', 2.0f, 0.75f, 0, &alpha, &ramp);
		ae_zone_wall_sample('b', 2.0f, 0.0f, 0, &alpha_top, &ramp);
		CHECK(alpha > alpha_top);
		/* the reveal: nothing before the first closing (or without a running clock), eases in, whole after */
		ae_zone_plan_make(&plan, 60.0f, 60, 30);
		CHECK(NEAR(ae_zone_reveal(&plan, 0, 5000), 0.0f) && NEAR(ae_zone_reveal(&plan, 1, 0), 0.0f));
		CHECK(NEAR(ae_zone_reveal(&plan, 1, 60L * 30 - 1), 0.0f) && NEAR(ae_zone_reveal(&plan, 1, 60L * 30), 0.0f));
		CHECK(NEAR(ae_zone_reveal(&plan, 1, 60L * 30 + AE_ZONE_REVEAL_TICKS / 2), 0.5f));
		CHECK(NEAR(ae_zone_reveal(&plan, 1, 60L * 30 + AE_ZONE_REVEAL_TICKS), 1.0f) && NEAR(ae_zone_reveal(&plan, 1, 99999), 1.0f));
		/* the round clock: the zone's total, or the time limit when less; counts down; rounds up */
		CHECK(ae_zone_round_seconds_left(180, 0, 0) == 180 && ae_zone_round_seconds_left(180, 0, 30) == 179);
		CHECK(ae_zone_round_seconds_left(180, 0, 31) == 180 - 1 && ae_zone_round_seconds_left(180, 120, 0) == 120);
		CHECK(ae_zone_round_seconds_left(100, 300, 0) == 100 && ae_zone_round_seconds_left(100, 0, 99999) == 0);
		/* the leash: outside always; near the edge only while it closes */
		CHECK(ae_zone_leashes(AE_ZONE_PHASE_WAIT, -0.5f) && !ae_zone_leashes(AE_ZONE_PHASE_WAIT, 0.5f));
		CHECK(ae_zone_leashes(AE_ZONE_PHASE_CLOSING, 5.0f) && !ae_zone_leashes(AE_ZONE_PHASE_CLOSING, 7.0f));
		CHECK(ae_zone_leashes(AE_ZONE_PHASE_CLOSING_2, 0.0f) && !ae_zone_leashes(AE_ZONE_PHASE_WAIT_2, 5.0f));
		(void)bright;
	}

	if (failures)
		return 1;
	printf("ae_round_zone_rules_test: ok\n");
	return 0;
}
