#include <stdio.h>
#include "ae_smoke_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.001f)

int main(void)
{
	float centre[3] = { 10.0f, 20.0f, 5.0f };
	float far_away[3] = { 10.0f, 40.0f, 5.0f }, near_side[3] = { 10.0f, 10.0f, 5.0f };

	/* the owner's times: 10 s there, 3 s fade, gone at 13 s (ticks at 30 a second) */
	CHECK(AE_SMOKE_HOLD_TICKS == 300 && AE_SMOKE_FADE_TICKS == 90 && AE_SMOKE_LIFE_TICKS == 390);
	CHECK(!ae_smoke_alive(-1) && ae_smoke_alive(0) && ae_smoke_alive(389) && !ae_smoke_alive(390));
	CHECK(NEAR(ae_smoke_radius_at(0), AE_SMOKE_RADIUS) && NEAR(ae_smoke_radius_at(299), AE_SMOKE_RADIUS));
	CHECK(NEAR(ae_smoke_radius_at(300), AE_SMOKE_RADIUS));
	CHECK(NEAR(ae_smoke_radius_at(345), AE_SMOKE_RADIUS * 0.8f));
	CHECK(ae_smoke_radius_at(389) > AE_SMOKE_RADIUS * AE_SMOKE_FADE_RADIUS_SCALE &&
		ae_smoke_radius_at(389) < AE_SMOKE_RADIUS * 0.61f);
	CHECK(ae_smoke_radius_at(390) == 0.0f && ae_smoke_radius_at(-5) == 0.0f);

	/* a line through the middle: the whole diameter; a line missing it: none */
	CHECK(NEAR(ae_smoke_chord(near_side, far_away, centre, 2.8f), 5.6f));
	{
		float a[3] = { 0.0f, 20.0f, 5.0f }, b[3] = { 20.0f, 20.0f, 5.0f }, c[3] = { 20.0f, 20.0f, 9.0f };
		float d[3] = { 0.0f, 20.0f, 9.0f };

		CHECK(NEAR(ae_smoke_chord(a, b, centre, 2.8f), 5.6f));
		CHECK(ae_smoke_chord(d, c, centre, 2.8f) == 0.0f);
	}
	/* a graze: the chord under the minimum sees through, one past it is blocked */
	{
		float a[3] = { 0.0f, 22.75f, 5.0f }, b[3] = { 20.0f, 22.75f, 5.0f };
		float e[3] = { 0.0f, 22.0f, 5.0f }, f[3] = { 20.0f, 22.0f, 5.0f };

		CHECK(ae_smoke_chord(a, b, centre, 2.8f) < AE_SMOKE_MIN_CHORD);
		CHECK(!ae_smoke_blocks_at(0, centre, a, b));
		CHECK(ae_smoke_blocks_at(0, centre, e, f));
	}
	/* the segment ends inside the sphere: only the part inside counts; both ends inside: its whole length */
	{
		float inside[3] = { 10.0f, 21.0f, 5.0f }, inside2[3] = { 10.0f, 19.0f, 5.0f };

		CHECK(NEAR(ae_smoke_chord(near_side, inside, centre, 2.8f), 3.8f));
		CHECK(NEAR(ae_smoke_chord(inside, inside2, centre, 2.8f), 2.0f));
		CHECK(ae_smoke_blocks_at(10, centre, inside, inside2));
	}
	/* a segment stopping short of the cloud, a degenerate one, a spent volume */
	{
		float short_end[3] = { 10.0f, 15.0f, 5.0f };

		CHECK(ae_smoke_chord(near_side, short_end, centre, 2.8f) == 0.0f);
		CHECK(ae_smoke_chord(near_side, near_side, centre, 2.8f) == 0.0f);
		CHECK(!ae_smoke_blocks_at(390, centre, near_side, far_away));
		CHECK(ae_smoke_blocks_at(389, centre, near_side, far_away));
	}
	/* the same numbers give the same answer, whichever way the line is drawn */
	CHECK(NEAR(ae_smoke_chord(near_side, far_away, centre, 2.8f), ae_smoke_chord(far_away, near_side, centre, 2.8f)));

	if (failures)
		return 1;
	printf("ae_smoke_rules_test: ok\n");
	return 0;
}
