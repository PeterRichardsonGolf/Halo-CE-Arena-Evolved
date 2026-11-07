#include <stdio.h>

#include "ae_tracked_grenade_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* Only a pack's own weapon whose trigger tracks its projectile, and only the shot's first projectile. */
static void test_tracks(void)
{
	CHECK(ae_tracked_grenade_tracks(1, 1, 0));
	CHECK(!ae_tracked_grenade_tracks(0, 1, 0));	/* a stock or map weapon with the flag keeps the game's way */
	CHECK(!ae_tracked_grenade_tracks(1, 0, 0));	/* a pack weapon without the flag */
	CHECK(!ae_tracked_grenade_tracks(1, 1, 1));	/* a second projectile of the shot */
}

/* Held: the fuse waits, up to the cap. Let go (or the weapon gone) after the minimum: set off; a tap: its fuse. */
static void test_tick(void)
{
	CHECK(ae_tracked_grenade_tick_action(1, 1, 0) == AE_TRACKED_GRENADE_HOLD);
	CHECK(ae_tracked_grenade_tick_action(1, 1, AE_TRACKED_GRENADE_MAX_HOLD_TICKS) == AE_TRACKED_GRENADE_HOLD);
	CHECK(ae_tracked_grenade_tick_action(1, 1, AE_TRACKED_GRENADE_MAX_HOLD_TICKS + 1) == AE_TRACKED_GRENADE_RUN);
	CHECK(ae_tracked_grenade_tick_action(1, 0, AE_TRACKED_GRENADE_MIN_HOLD_TICKS - 1) == AE_TRACKED_GRENADE_RUN);
	CHECK(ae_tracked_grenade_tick_action(1, 0, AE_TRACKED_GRENADE_MIN_HOLD_TICKS) == AE_TRACKED_GRENADE_DETONATE);
	CHECK(ae_tracked_grenade_tick_action(1, 0, 200) == AE_TRACKED_GRENADE_DETONATE);
	CHECK(ae_tracked_grenade_tick_action(0, 1, 200) == AE_TRACKED_GRENADE_DETONATE);	/* the weapon gone: let go */
	CHECK(ae_tracked_grenade_tick_action(0, 0, 2) == AE_TRACKED_GRENADE_RUN);
	CHECK(ae_tracked_grenade_tick_action(1, 1, -1) == AE_TRACKED_GRENADE_RUN);	/* a clock gone back */
	CHECK(AE_TRACKED_GRENADE_MIN_HOLD_TICKS < AE_TRACKED_GRENADE_MAX_HOLD_TICKS);
}

/* A stale row (another projectile under the handle, or none) is forgotten; a weapon of another tag is no shooter. */
static void test_same(void)
{
	CHECK(ae_tracked_grenade_same_projectile(1, 77, 77));
	CHECK(!ae_tracked_grenade_same_projectile(1, 78, 77));
	CHECK(!ae_tracked_grenade_same_projectile(0, 77, 77));
	CHECK(!ae_tracked_grenade_same_projectile(1, -1, -1));	/* a cleared row (a new map) */
	CHECK(ae_tracked_grenade_same_weapon(1, 5, 5));
	CHECK(!ae_tracked_grenade_same_weapon(1, 6, 5));
	CHECK(!ae_tracked_grenade_same_weapon(0, 5, 5));
	CHECK(!ae_tracked_grenade_same_weapon(1, -1, -1));
}

/* A weapon dropped or put away keeps its last control: not held, whatever its trigger bit says. */
static void test_trigger_held(void)
{
	CHECK(ae_tracked_grenade_trigger_held(1, 1, 1));
	CHECK(!ae_tracked_grenade_trigger_held(1, 0, 1));
	CHECK(!ae_tracked_grenade_trigger_held(0, 1, 1));
	CHECK(!ae_tracked_grenade_trigger_held(1, 1, 0));
}

/* A shooter that is gone leaves the projectile ownerless; one that is there, or none, is kept. */
static void test_owner(void)
{
	CHECK(ae_tracked_grenade_owner(28, 1) == 28);
	CHECK(ae_tracked_grenade_owner(28, 0) == -1);	/* the unit was deleted: the handle names nothing */
	CHECK(ae_tracked_grenade_owner(-1, 0) == -1);
	CHECK(ae_tracked_grenade_owner(-1, 1) == -1);
	CHECK(ae_tracked_grenade_owner(0, 0) == -1);	/* index 0 is an object's index, not none */
}

/* A pack grenade's fuse runs from its first bounce; stock tags, held, let-go and not-yet-bounced grenades are as before. */
static void test_fuse(void)
{
	long after_bounce = AE_TRACKED_GRENADE_STARTS_AFTER_FIRST_BOUNCE;

	CHECK(ae_tracked_grenade_fuse_runs(AE_TRACKED_GRENADE_RUN, 1, after_bounce, 1));
	CHECK(!ae_tracked_grenade_fuse_runs(AE_TRACKED_GRENADE_RUN, 1, after_bounce, 0));	/* no bounce yet */
	CHECK(!ae_tracked_grenade_fuse_runs(AE_TRACKED_GRENADE_RUN, 0, after_bounce, 1));	/* a stock grenade */
	CHECK(!ae_tracked_grenade_fuse_runs(AE_TRACKED_GRENADE_RUN, 1, 2, 1));	/* at rest: the game's own */
	CHECK(!ae_tracked_grenade_fuse_runs(AE_TRACKED_GRENADE_RUN, 1, 0, 1));	/* immediately: the game's own */
	CHECK(!ae_tracked_grenade_fuse_runs(AE_TRACKED_GRENADE_HOLD, 1, after_bounce, 1));	/* held: waits */
	CHECK(!ae_tracked_grenade_fuse_runs(AE_TRACKED_GRENADE_DETONATE, 1, after_bounce, 1));	/* let go: set off */
}

int main(void)
{
	test_tracks();
	test_tick();
	test_same();
	test_trigger_held();
	test_owner();
	test_fuse();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
