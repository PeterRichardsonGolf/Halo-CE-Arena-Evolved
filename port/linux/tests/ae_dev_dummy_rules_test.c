#include <math.h>
#include <stdio.h>

#include "ae_dev_dummy_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int within(float a, float b)
{
	return fabsf(a - b) < 0.001f;
}

/* a dummy stands ahead of the player and faces back at them; a group lines up across the line of sight */
static void spot_tests(void)
{
	struct ae_dummy_spot spot = ae_dummy_spot(0, 1, 100.0f, 50.0f, 3.0f, 1.0f, 0.0f, 10.0f, 0);

	CHECK(within(spot.x, 110.0f) && within(spot.y, 50.0f) && within(spot.z, 3.5f) && within(fabsf(spot.yaw), 3.14159265f));
	/* (the line of sight need not be of unit length, nor level with the axes: 10 ahead along +y) */
	spot = ae_dummy_spot(0, 1, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 10.0f, 0);
	CHECK(within(spot.x, 0.0f) && within(spot.y, 10.0f) && within(spot.yaw, -1.5707963f));
	/* (no line of sight: along +x) */
	spot = ae_dummy_spot(0, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 0);
	CHECK(within(spot.x, 5.0f) && within(spot.y, 0.0f));
	/* (here: at the player's own spot, facing back the way the player looks) */
	spot = ae_dummy_spot(0, 1, 7.0f, 8.0f, 1.0f, 1.0f, 0.0f, 10.0f, 1);
	CHECK(within(spot.x, 7.0f) && within(spot.y, 8.0f));
	/* (three together, looking along +x: abreast across y, the middle one on the line, 1.5 apart, the left one first
	at +y) */
	{
		struct ae_dummy_spot a = ae_dummy_spot(0, 3, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 0);
		struct ae_dummy_spot b = ae_dummy_spot(1, 3, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 0);
		struct ae_dummy_spot c = ae_dummy_spot(2, 3, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 0);

		CHECK(within(a.x, 10.0f) && within(b.x, 10.0f) && within(c.x, 10.0f));
		CHECK(within(a.y, -1.5f) && within(b.y, 0.0f) && within(c.y, 1.5f));
	}
}

/* the heal after 3 s unhurt, the respawn 1 s after the death (never later than the game's own timer, never under 1) */
static void timer_tests(void)
{
	CHECK(!ae_dummy_heal_due(100, 100) && !ae_dummy_heal_due(100, 189) && ae_dummy_heal_due(100, 190));
	CHECK(ae_dummy_respawn_timer(100, 100, 150) == 30);
	CHECK(ae_dummy_respawn_timer(100, 110, 150) == 20);
	CHECK(ae_dummy_respawn_timer(100, 129, 150) == 1);
	CHECK(ae_dummy_respawn_timer(100, 500, 150) == 1);
	/* (the game's own timer shorter: kept) */
	CHECK(ae_dummy_respawn_timer(100, 100, 12) == 12);
	CHECK(ae_dummy_respawn_timer(100, 100, 1) == 1);
}

/* a flagged bot is given up when it never came, or came and went; never while it is in the game */
static void give_up_tests(void)
{
	CHECK(!ae_dummy_given_up(1, 1, 0, 9999) && !ae_dummy_given_up(1, 0, 0, 9999));
	CHECK(!ae_dummy_given_up(0, 0, 100, 399) && ae_dummy_given_up(0, 0, 100, 400));
	CHECK(ae_dummy_given_up(0, 1, 100, 101));
}

/* the bots a group takes: the free ones in order, after those already on their way */
static void pick_tests(void)
{
	unsigned char present[10] = { 1, 1, 0, 0, 1, 0, 0, 0, 0, 0 };
	int out[8];

	CHECK(ae_dummy_pick(present, 10, 0, 2, out) == 2 && out[0] == 2 && out[1] == 3);
	CHECK(ae_dummy_pick(present, 10, 1, 2, out) == 2 && out[0] == 3 && out[1] == 5);
	CHECK(ae_dummy_pick(present, 10, 5, 2, out) == 2 && out[0] == 8 && out[1] == 9);
	/* (not enough room) */
	CHECK(ae_dummy_pick(present, 10, 6, 2, out) == 1 && out[0] == 9);
	CHECK(ae_dummy_pick(present, 10, 0, 0, out) == 0);
}

/* the dummy in the game leaves first, the lowest; else the game's pick */
static void leaver_tests(void)
{
	unsigned char flag[6] = { 0, 0, 1, 0, 1, 0 };
	unsigned char present[6] = { 1, 1, 0, 1, 1, 1 };

	CHECK(ae_dummy_leaver(flag, present, 6, 5) == 4);
	present[4] = 0;
	CHECK(ae_dummy_leaver(flag, present, 6, 5) == 5);
	flag[2] = flag[4] = 0;
	CHECK(ae_dummy_leaver(flag, present, 6, 3) == 3);
}

int main(void)
{
	spot_tests();
	timer_tests();
	give_up_tests();
	pick_tests();
	leaver_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_dev_dummy_rules: ok\n");
	return failures != 0;
}
