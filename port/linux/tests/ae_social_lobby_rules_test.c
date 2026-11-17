#include <stdio.h>
#include <string.h>
#include "ae_social_lobby_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static void test_stages(void)
{
	struct ae_social_shape shape = {2, 4};
	struct ae_social_roster roster;
	struct ae_social_lobby lobby;
	int seed, last_stage, seat, ever_found;
	long t;

	for (seed = 1; seed <= 100; seed++)
	{
		CHECK(ae_social_roster_draw(&shape, 1, 1600, 0, (unsigned long)seed, &roster));
		ae_social_lobby_init(&lobby, &roster, (unsigned long)seed);
		EQ(lobby.stage, AE_SOCIAL_STAGE_SEARCHING);
		EQ(ae_social_lobby_joined(&lobby), 1);
		/* the search is 1.5 .. 2.5 s, or until the range is full; the joins end inside the cap */
		CHECK(lobby.found_ms >= AE_SOCIAL_LOBBY_STEP_MS && lobby.found_ms <= 2500);
		CHECK(lobby.found_ms >= 1500 || lobby.found_ms == (long)(lobby.span + 1) * AE_SOCIAL_LOBBY_STEP_MS);
		CHECK(lobby.starting_ms <= AE_SOCIAL_LOBBY_CAP_MS && lobby.starting_ms >= lobby.found_ms);
		/* the stages in order, never back */
		last_stage = 0; ever_found = 0;
		for (t = 0; t < 20000; t += 50)
		{
			ae_social_lobby_advance(&lobby, 50);
			CHECK(lobby.stage >= last_stage);
			last_stage = lobby.stage;
			ever_found |= lobby.stage == AE_SOCIAL_STAGE_FOUND || lobby.stage == AE_SOCIAL_STAGE_STARTING;
			/* the joined count never falls */
			CHECK(ae_social_lobby_joined(&lobby) >= 1 && ae_social_lobby_joined(&lobby) <= 8);
			/* nobody but the human joins while searching */
			if (lobby.stage == AE_SOCIAL_STAGE_SEARCHING)
				EQ(ae_social_lobby_joined(&lobby), 1);
			if (lobby.stage == AE_SOCIAL_STAGE_STARTING)
			{
				EQ(ae_social_lobby_joined(&lobby), 8);
				CHECK(ae_social_lobby_countdown_ms(&lobby) > 0 && ae_social_lobby_countdown_ms(&lobby) <= 5000);
			}
		}
		EQ(lobby.stage, AE_SOCIAL_STAGE_GO);
		CHECK(ever_found);
		EQ(ae_social_lobby_joined(&lobby), 8);
		EQ(ae_social_lobby_countdown_ms(&lobby), 0);
		/* nearest first: the join times follow the roster's order */
		for (seat = 1; seat < lobby.order_count; seat++)
			CHECK(lobby.join_ms[lobby.order[seat]] >= lobby.join_ms[lobby.order[seat - 1]]);
	}
	/* the same seed, the same schedule */
	{
		struct ae_social_lobby again;

		CHECK(ae_social_roster_draw(&shape, 1, 1600, 0, 9, &roster));
		ae_social_lobby_init(&lobby, &roster, 9);
		ae_social_lobby_init(&again, &roster, 9);
		CHECK(!memcmp(&lobby, &again, sizeof(lobby)));
	}
}

static void test_skip_cancel(void)
{
	struct ae_social_shape shape = {2, 4};
	struct ae_social_roster roster;
	struct ae_social_lobby lobby;

	CHECK(ae_social_roster_draw(&shape, 1, 1600, 0, 4, &roster));
	/* A at any stage: GO, everyone in */
	ae_social_lobby_init(&lobby, &roster, 4);
	ae_social_lobby_advance(&lobby, 100);
	CHECK(ae_social_lobby_skip(&lobby));
	EQ(lobby.stage, AE_SOCIAL_STAGE_GO);
	EQ(ae_social_lobby_joined(&lobby), 8);
	ae_social_lobby_advance(&lobby, 1000);
	EQ(lobby.stage, AE_SOCIAL_STAGE_GO);
	CHECK(!ae_social_lobby_skip(&lobby));
	CHECK(!ae_social_lobby_cancel(&lobby));
	/* B while searching or found or starting: cancelled, and it stays so */
	ae_social_lobby_init(&lobby, &roster, 4);
	CHECK(ae_social_lobby_cancel(&lobby));
	EQ(lobby.stage, AE_SOCIAL_STAGE_CANCELLED);
	ae_social_lobby_advance(&lobby, 30000);
	EQ(lobby.stage, AE_SOCIAL_STAGE_CANCELLED);
	CHECK(!ae_social_lobby_skip(&lobby));
	ae_social_lobby_init(&lobby, &roster, 4);
	ae_social_lobby_advance(&lobby, lobby.starting_ms + 10);
	EQ(lobby.stage, AE_SOCIAL_STAGE_STARTING);
	CHECK(ae_social_lobby_cancel(&lobby));
	/* a huge step crosses every stage at once */
	ae_social_lobby_init(&lobby, &roster, 4);
	ae_social_lobby_advance(&lobby, 60000);
	EQ(lobby.stage, AE_SOCIAL_STAGE_GO);
	/* a negative step is nothing */
	ae_social_lobby_init(&lobby, &roster, 4);
	ae_social_lobby_advance(&lobby, -50);
	EQ(lobby.elapsed, 0);
}

static void test_range_and_fixed(void)
{
	struct ae_social_shape shape = {2, 4};
	struct ae_social_roster roster;
	struct ae_social_lobby lobby;
	int previous = 0, step;

	CHECK(ae_social_roster_draw(&shape, 1, 1600, 0, 11, &roster));
	ae_social_lobby_init(&lobby, &roster, 11);
	EQ(lobby.span, roster.span);
	EQ(ae_social_lobby_step(&lobby), 0);
	while (lobby.stage == AE_SOCIAL_STAGE_SEARCHING)
	{
		ae_social_lobby_advance(&lobby, 50);
		step = ae_social_lobby_step(&lobby);
		CHECK(step >= previous && step <= lobby.span);
		previous = step;
	}
	/* the strip keeps the width it got to */
	ae_social_lobby_advance(&lobby, 3000);
	EQ(ae_social_lobby_step(&lobby), previous);
	/* a FIXED lobby has no range */
	CHECK(ae_social_roster_draw(&shape, 1, 1600, 4, 11, &roster));
	ae_social_lobby_init(&lobby, &roster, 11);
	ae_social_lobby_advance(&lobby, 1000);
	EQ(ae_social_lobby_step(&lobby), 0);
	CHECK(lobby.found_ms >= 1500 && lobby.found_ms <= 2500);
}

static void test_newest(void)
{
	struct ae_social_shape shape = {2, 4};
	struct ae_social_roster roster;
	struct ae_social_lobby lobby;
	int seat, fresh;

	CHECK(ae_social_roster_draw(&shape, 1, 1600, 0, 5, &roster));
	ae_social_lobby_init(&lobby, &roster, 5);
	ae_social_lobby_advance(&lobby, lobby.found_ms + 1);
	/* the human is never "new"; a bot is for 1.5 s after it joined */
	CHECK(!ae_social_lobby_seat_new(&lobby, 0));
	ae_social_lobby_advance(&lobby, lobby.join_ms[lobby.order[2]] - lobby.elapsed);
	fresh = 0;
	for (seat = 1; seat < 8; seat++)
		fresh += ae_social_lobby_seat_new(&lobby, seat);
	CHECK(fresh >= 1);
	CHECK(ae_social_lobby_seat_in(&lobby, lobby.order[2]));
	CHECK(!ae_social_lobby_seat_in(&lobby, 99));
}

static void test_names(void)
{
	char names[AE_SOCIAL_SEATS][AE_SOCIAL_LOBBY_NAME];

	char again[AE_SOCIAL_SEATS][AE_SOCIAL_LOBBY_NAME];

	CHECK(ae_social_lobby_names(7, 3, names));
	CHECK(names[0][0] && strcmp(names[0], names[1]) && strcmp(names[1], names[2]) && strcmp(names[0], names[2]));
	CHECK(strncmp(names[0], "BOT ", 4));
	CHECK(ae_social_lobby_names(7, 3, again));
	CHECK(!strcmp(names[0], again[0]) && !strcmp(names[2], again[2]));
	CHECK(ae_social_lobby_names(1, 0, names));
	CHECK(!ae_social_lobby_names(1, AE_SOCIAL_SEATS + 1, names));
}

int main(void)
{
	test_stages();
	test_skip_cancel();
	test_range_and_fixed();
	test_newest();
	test_names();
	if (failures)
		return 1;
	printf("ae_social_lobby_rules_test: ok\n");
	return 0;
}
