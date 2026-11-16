#include <stdio.h>
#include <string.h>

#include "ae_bots_learnt_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_learnt_map map, again;
static char text[16384];

#define HEADER "layout 3 bloodgulch.map 8420ad9a"

/* the file: read back as written; another map's header, or none, is not used */
static void file_tests(void)
{
	CHECK(ae_learnt_read(&map, "layout 3 bloodgulch.map 8420ad9a\nwedge 12 3\nstuck 40 4 85.00 -127.00 1.40\n"
		"jump 1 2 3 4 5 6 2 1\nnonsense line\nwedge -1 2\nwedge 13 9\njump 1 2 3 4 5 6 1 3\n", HEADER));
	CHECK(map.wedge_count == 2 && map.wedges[0].surface == 12 && map.wedges[0].count == 3);
	CHECK(map.wedges[1].count == AE_LEARNT_WEDGE_MOST);
	CHECK(map.stuck_count == 1 && map.stucks[0].surface == 40 && map.stucks[0].count == 4 && map.stucks[0].y == -127.0f);
	/* (the jump missed three times is not read: dropped) */
	CHECK(map.jump_count == 1 && map.jumps[0].uses == 2 && map.jumps[0].misses == 1 && map.jumps[0].to[2] == 6.0f);
	CHECK(ae_learnt_write(&map, text, (int)sizeof text) > 0);
	CHECK(ae_learnt_read(&again, text, HEADER) && again.wedge_count == 2 && again.stuck_count == 1 && again.jump_count == 1);
	CHECK(!strncmp(text, HEADER "\nwedge 12 3\n", strlen(HEADER) + 12));
	/* (too small: nothing) */
	CHECK(ae_learnt_write(&map, text, 20) == 0);
	CHECK(!ae_learnt_read(&map, "layout 3 bloodgulch.map 00000000\nwedge 12 3\n", HEADER) && map.wedge_count == 0 &&
		!strcmp(map.header, HEADER));
	CHECK(!ae_learnt_read(&map, "", HEADER) && !ae_learnt_read(&map, NULL, HEADER));
	/* (a broken file's numbers: not used) */
	CHECK(ae_learnt_read(&map, HEADER "\njump nan 0 0 1 2 3 1 0\njump 0 0 0 1e38 2 3 1 0\nstuck 4 3 inf 0 0\n"
		"jump 0 0 0 1 2 3 1 0\n", HEADER) && map.jump_count == 1 && map.stuck_count == 0);
	/* (Windows line ends) */
	CHECK(ae_learnt_read(&map, HEADER "\r\nwedge 5 1\r\n", HEADER) && map.wedge_count == 1);
}

/* wedges add up, and fade in a game of driving with none */
static void wedge_tests(void)
{
	ae_learnt_read(&map, HEADER "\nwedge 12 3\nwedge 13 1\n", HEADER);
	ae_learnt_wedge(&map, 12);
	ae_learnt_wedge(&map, 99);
	ae_learnt_wedge(&map, 99);
	ae_learnt_game_over(&map, 200, 300);
	/* (12: 3 + 1; 13: none this game, 1 - 1 = dropped; 99: new, 2) */
	CHECK(map.wedge_count == 2 && map.wedges[0].surface == 12 && map.wedges[0].count == 4 && map.wedges[1].surface == 99 &&
		map.wedges[1].count == 2);
	/* (a game with little driving: nothing fades) */
	ae_learnt_game_over(&map, 10, 300);
	CHECK(map.wedge_count == 2 && map.wedges[0].count == 4);
	/* (never past the most) */
	ae_learnt_wedge(&map, 12);
	ae_learnt_game_over(&map, 200, 300);
	CHECK(map.wedges[0].count == AE_LEARNT_WEDGE_MOST && map.wedges[1].count == 1);
}

/* stuck: one report a bot, two a game; the foot penalty from three */
static void stuck_tests(void)
{
	ae_learnt_read(&map, HEADER "\n", HEADER);
	ae_learnt_stuck(&map, 40, 1, 85.0f, -127.0f, 1.4f);
	ae_learnt_stuck(&map, 40, 1, 85.0f, -127.0f, 1.4f);
	CHECK(map.stuck_count == 1 && map.stucks[0].fresh == 1);
	ae_learnt_stuck(&map, 40, 2, 85.0f, -127.0f, 1.4f);
	ae_learnt_stuck(&map, 40, 3, 85.0f, -127.0f, 1.4f);
	CHECK(map.stucks[0].fresh == AE_LEARNT_STUCK_A_GAME);
	ae_learnt_game_over(&map, 0, 300);
	CHECK(map.stucks[0].count == 2 && ae_learnt_stuck_levels(map.stucks[0].count) == 0);
	/* (the next game: the same bot again counts) */
	ae_learnt_stuck(&map, 40, 1, 85.0f, -127.0f, 1.4f);
	ae_learnt_game_over(&map, 0, 300);
	CHECK(map.stucks[0].count == 3 && ae_learnt_stuck_levels(3) == 1);
	CHECK(ae_learnt_stuck_levels(6) == AE_LEARNT_STUCK_LEVELS && ae_learnt_stuck_levels(5) == 3 && ae_learnt_stuck_levels(0) == 0);
	/* (a short game fades nothing; a long one with no report takes one off) */
	ae_learnt_game_over(&map, 0, 60);
	CHECK(map.stucks[0].count == 3);
	ae_learnt_game_over(&map, 0, 300);
	CHECK(map.stucks[0].count == 2);
	/* (no bot of a number past the table, no piece below 0) */
	ae_learnt_stuck(&map, 41, AE_LEARNT_BOTS, 0.0f, 0.0f, 0.0f);
	ae_learnt_stuck(&map, -1, 1, 0.0f, 0.0f, 0.0f);
	CHECK(map.stuck_count == 1);
}

/* a person's jumps: which are kept, the same one again, bots' misses */
static void jump_tests(void)
{
	float a[3] = { 10.0f, 10.0f, 0.0f }, up[3] = { 11.2f, 10.0f, 1.2f }, flat[3] = { 11.5f, 10.0f, 0.3f };
	float near_a[3] = { 10.5f, 10.5f, 0.2f }, near_up[3] = { 11.7f, 10.0f, 1.0f }, drop[3] = { 11.5f, 10.0f, -2.5f };
	int index, first;

	CHECK(ae_learnt_jump_keep(a, up, 0.5f, 5.0f));
	CHECK(ae_learnt_jump_keep(a, drop, 0.6f, 6.0f));
	CHECK(!ae_learnt_jump_keep(a, flat, 0.6f, 6.0f));
	CHECK(!ae_learnt_jump_keep(a, up, 0.2f, 6.0f));
	CHECK(!ae_learnt_jump_keep(a, up, 0.6f, 4.9f));
	/* (too fast across the ground for a run's jump: 1.2 units in 0.4 s is fine, 12 in 0.5 s is a grenade's) */
	{
		float far_up[3] = { 22.0f, 10.0f, 1.2f };

		CHECK(ae_learnt_jump_keep(a, up, 0.4f, 6.0f));
		CHECK(!ae_learnt_jump_keep(a, far_up, 0.5f, 6.0f));
	}
	ae_learnt_read(&map, HEADER "\n", HEADER);
	first = ae_learnt_jump_add(&map, a, up);
	CHECK(first == 0 && map.jump_count == 1 && map.jumps[0].uses == 1);
	CHECK(ae_learnt_jump_add(&map, near_a, near_up) == 0 && map.jumps[0].uses == 2 && map.jump_count == 1);
	CHECK(ae_learnt_jump_add(&map, a, drop) == 1 && map.jump_count == 2);
	/* (a result finds its jump by its ends) */
	CHECK(ae_learnt_jump_find(&map, near_a, near_up) == 0 && ae_learnt_jump_find(&map, a, drop) == 1);
	CHECK(ae_learnt_jump_find(&map, drop, a) == -1);
	/* (missed twice, never made: a miss; at three, gone) */
	for (index = 0; index < AE_LEARNT_JUMP_MISSES; index++)
	{
		ae_learnt_jump_result(&map, 1, 0);
		ae_learnt_jump_result(&map, 1, 0);
		ae_learnt_game_over(&map, 0, 300);
	}
	CHECK(map.jump_count == 1 && map.jumps[0].uses == 2);
	/* (missed twice but made once: no miss) */
	ae_learnt_jump_result(&map, 0, 0);
	ae_learnt_jump_result(&map, 0, 0);
	ae_learnt_jump_result(&map, 0, 1);
	ae_learnt_jump_result(&map, 7, 0);
	ae_learnt_game_over(&map, 0, 300);
	CHECK(map.jump_count == 1 && map.jumps[0].misses == 0);
	/* (full: the least used gives way) */
	ae_learnt_read(&map, HEADER "\n", HEADER);
	for (index = 0; index < AE_LEARNT_JUMPS; index++)
	{
		float from[3], to[3];

		from[0] = (float)index * 10.0f; from[1] = 0.0f; from[2] = 0.0f;
		to[0] = from[0] + 3.0f; to[1] = 0.0f; to[2] = 1.0f;
		ae_learnt_jump_add(&map, from, to);
		if (index)
			ae_learnt_jump_add(&map, from, to);
	}
	CHECK(map.jump_count == AE_LEARNT_JUMPS && map.jumps[0].uses == 1);
	CHECK(ae_learnt_jump_add(&map, a, drop) == 0 && map.jumps[0].from[0] == 10.0f && map.jump_count == AE_LEARNT_JUMPS);
}

int main(void)
{
	file_tests();
	wedge_tests();
	stuck_tests();
	jump_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_bots_learnt_rules: ok\n");
	return failures != 0;
}
