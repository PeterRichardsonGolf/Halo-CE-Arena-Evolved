#include <stdio.h>

#include "ae_shield_pop_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define FINISHER 25.f
#define HOST_MARGIN 0.01f
#define GUEST_MARGIN 0.02f
#define SHIELD 75.f

/* the hit (1, 2, ...) after which the pop shows, the shield kept as the game keeps it (a part of its maximum, each
hit taking its own part away); 0 when it never shows before the shield is gone. *breaking: the hit that breaks it */
static int pop_after(enum ae_shield_pop_hit kind, float hit, float margin, int *breaking)
{
	float vitality = 1.f;
	float points = ae_shield_pop_points(kind, FINISHER, hit);
	int shot;

	*breaking = 0;
	for (shot = 1; shot < 100; shot++)
	{
		float normalized = hit * (1.f / SHIELD);

		if (normalized > vitality)
		{
			*breaking = shot;
			return 0;
		}
		vitality -= normalized;
		if (kind != _ae_shield_pop_hit_no_spill && ae_shield_pop_shows(SHIELD * vitality, points, margin))
			return shot;
	}
	return 0;
}

int main(void)
{
	int breaking;
	float magnum = 25.f;
	float sidekick = 26.f * 0.54f;
	float dmr = 17.875f * 1.06f;
	float evo = 17.875f * 1.06f;
	float ar = 7.f * 0.9f;
	float br = 6.f * 1.06f;

	/* the kinds: a profile's no_spill first, then a burst profile, then the damage's headshots */
	CHECK(ae_shield_pop_hit_kind(1, 1, 1, 1) == _ae_shield_pop_hit_no_spill);
	CHECK(ae_shield_pop_hit_kind(0, 1, 1, 1) == _ae_shield_pop_hit_no_spill);
	CHECK(ae_shield_pop_hit_kind(1, 1, 0, 3) == _ae_shield_pop_hit_burst);
	CHECK(ae_shield_pop_hit_kind(1, 1, 0, 1) == _ae_shield_pop_hit_headshot);
	CHECK(ae_shield_pop_hit_kind(1, 0, 1, 3) == _ae_shield_pop_hit_headshot);
	CHECK(ae_shield_pop_hit_kind(0, 0, 0, 0) == _ae_shield_pop_hit_plain);

	/* the points */
	CHECK(ae_shield_pop_points(_ae_shield_pop_hit_plain, FINISHER, 4.f) == FINISHER);
	CHECK(ae_shield_pop_points(_ae_shield_pop_hit_burst, FINISHER, br) == FINISHER);
	CHECK(ae_shield_pop_points(_ae_shield_pop_hit_headshot, FINISHER, sidekick) == sidekick);
	CHECK(ae_shield_pop_points(_ae_shield_pop_hit_headshot, FINISHER, 300.f) == FINISHER);
	CHECK(ae_shield_pop_points(_ae_shield_pop_hit_headshot, FINISHER, 0.f) == 0.f);
	CHECK(ae_shield_pop_points(_ae_shield_pop_hit_no_spill, FINISHER, evo) == 0.f);

	/* the magnum: 25 left after 2 pops (its exact 25, with the margin); the 3rd to the head kills */
	CHECK(pop_after(_ae_shield_pop_hit_headshot, magnum, HOST_MARGIN, &breaking) == 2);
	CHECK(pop_after(_ae_shield_pop_hit_headshot, magnum, GUEST_MARGIN, &breaking) == 2);
	CHECK(ae_shield_pop_shows(25.f, 25.f, HOST_MARGIN));
	CHECK(ae_shield_pop_shows(25.009f, 25.f, HOST_MARGIN));
	CHECK(!ae_shield_pop_shows(25.05f, 25.f, HOST_MARGIN));

	/* the sidekick: 18.84 left after 4 is one hit too many; 4.8 after 5 pops (the 6th to the head kills) */
	CHECK(pop_after(_ae_shield_pop_hit_headshot, sidekick, HOST_MARGIN, &breaking) == 5);
	CHECK(!ae_shield_pop_shows(SHIELD - 4.f * sidekick, sidekick, HOST_MARGIN));
	CHECK(pop_after(_ae_shield_pop_hit_headshot, sidekick, GUEST_MARGIN, &breaking) == 5);
	/* (with the finishing points alone it popped after 4: the bug) */
	CHECK(pop_after(_ae_shield_pop_hit_plain, sidekick, HOST_MARGIN, &breaking) == 4);

	/* the DMR (single shots that spill): 18.16 left after 3 pops (the 4th to the head kills) */
	CHECK(ae_shield_pop_hit_kind(1, 1, 0, 1) == _ae_shield_pop_hit_headshot);
	CHECK(pop_after(_ae_shield_pop_hit_headshot, dmr, HOST_MARGIN, &breaking) == 3);
	CHECK(pop_after(_ae_shield_pop_hit_headshot, dmr, GUEST_MARGIN, &breaking) == 3);

	/* the EVO RIFLE (no_spill): never early; the 4th round breaks the shield (the pop), the 5th kills */
	CHECK(pop_after(_ae_shield_pop_hit_no_spill, evo, HOST_MARGIN, &breaking) == 0 && breaking == 4);
	/* (a guest sees the break as the shield sent comes to nothing: shown at 0, not at 18.16) */
	CHECK(ae_shield_pop_shows(0.f, ae_shield_pop_points(_ae_shield_pop_hit_no_spill, FINISHER, evo), GUEST_MARGIN));
	CHECK(!ae_shield_pop_shows(SHIELD - 3.f * evo, ae_shield_pop_points(_ae_shield_pop_hit_no_spill, FINISHER, evo),
		GUEST_MARGIN));

	/* the AR (no headshots): the finishing points, 25 */
	CHECK(ae_shield_pop_points(_ae_shield_pop_hit_plain, FINISHER, ar) == FINISHER);
	CHECK(pop_after(_ae_shield_pop_hit_plain, ar, HOST_MARGIN, &breaking) == 8);

	/* the BATTLE RIFLE (bursts): unchanged, the finishing points (its 8th round leaves 24.12) */
	CHECK(pop_after(_ae_shield_pop_hit_burst, br, HOST_MARGIN, &breaking) == 8);

	/* a guest's pop fires on a hit only: a weapon switch that moves the pop's point over a damaged shield (20 of 75 left,
	the EVO RIFLE's 0 points then the magnum's 25) brings it 'down' without a pop (review 2026-11-10) */
	CHECK(!ae_shield_pop_fires(0, 1, 20.f / 75.f, 20.f / 75.f));
	CHECK(ae_shield_pop_fires(0, 1, 30.f / 75.f, 20.f / 75.f));
	CHECK(!ae_shield_pop_fires(1, 1, 30.f / 75.f, 20.f / 75.f));
	CHECK(!ae_shield_pop_fires(0, 0, 30.f / 75.f, 20.f / 75.f));
	CHECK(!ae_shield_pop_fires(0, 1, 20.f / 75.f, 25.f / 75.f));

	if (failures)
		return 1;
	printf("ae_shield_pop_rules_test: ok\n");
	return 0;
}
