#include <stdio.h>
#include <string.h>
#include "ae_round_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_revive_mate_competes(void)
{
	/* (a nearer mate with an enemy within 5 wu, or on its way to another orb, does not take the revive) */
	CHECK(ae_round_revive_mate_competes(1, -1.0f) && ae_round_revive_mate_competes(1, 6.0f));
	CHECK(!ae_round_revive_mate_competes(1, 4.0f) && !ae_round_revive_mate_competes(0, -1.0f) &&
		!ae_round_revive_mate_competes(0, 20.0f));
	/* (with the filter, a nearer mate that cannot go leaves the farther eligible bot to start) */
	{
		int nearer = ae_round_revive_mate_competes(1, 3.0f) ? 1 : 0;

		CHECK(ae_round_bot_revive_decide(8.0f, -1, -1.0f, nearer, 0) == AE_ROUND_REVIVE_GO);
	}
}

int main(void)
{
	int members[AE_ROUND_SIDES], alive[AE_ROUND_SIDES], winner = 99;
	struct ae_variant_ext ext;

	test_revive_mate_competes();

	/* the player limit: 15 players have room for one more, 16 none; bots fill what the humans leave */
	CHECK(!ae_round_players_full(15) && ae_round_players_full(16) && ae_round_players_full(40));
	CHECK(ae_round_bot_cap(40, 1) == 15 && ae_round_bot_cap(40, 5) == 11 && ae_round_bot_cap(40, 0) == 15);
	CHECK(ae_round_bot_cap(40, 20) == 0 && ae_round_bot_cap(3, 1) == 3 && ae_round_bot_cap(-2, 1) == 0);
	/* a bot's call on an orb: near, unhurt, no enemy close, the nearest: go; each other case says why not (an enemy in view
	farther than 5 wu, however long, changes nothing: PLAYFIGHT bots see each other for ever) */
	CHECK(ae_round_bot_revive_decide(12.0f, -1, 30.0f, 0, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(12.0f, 600, 6.0f, 0, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(-1.0f, -1, -1.0f, 0, 0) == AE_ROUND_REVIVE_NO_ORB);
	CHECK(ae_round_bot_revive_decide(26.0f, -1, -1.0f, 0, 0) == AE_ROUND_REVIVE_FAR);
	CHECK(ae_round_bot_revive_decide(25.0f, -1, -1.0f, 0, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(10.0f, 44, -1.0f, 0, 0) == AE_ROUND_REVIVE_UNDER_FIRE);
	CHECK(ae_round_bot_revive_decide(10.0f, 45, -1.0f, 0, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(10.0f, 100, 4.9f, 0, 0) == AE_ROUND_REVIVE_ENEMY_NEAR);
	CHECK(ae_round_bot_revive_decide(10.0f, 100, 5.0f, 0, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(10.0f, 100, -1.0f, 2, 0) == AE_ROUND_REVIVE_NOT_NEAREST);
	CHECK(ae_round_bot_revive_decide(10.0f, 10, 3.0f, 1, 0) == AE_ROUND_REVIVE_UNDER_FIRE);
	/* on the walk a hit does not stop the bot (Jenkins at 9.5 wu): only at the orb, or an enemy within 5 wu */
	CHECK(ae_round_bot_revive_decide(9.5f, 10, 6.1f, 0, 1) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(9.5f, 10, 6.1f, 0, 0) == AE_ROUND_REVIVE_UNDER_FIRE);
	CHECK(ae_round_bot_revive_decide(9.5f, 10, 4.0f, 0, 1) == AE_ROUND_REVIVE_ENEMY_NEAR);
	CHECK(ae_round_bot_revive_decide(1.0f, 10, -1.0f, 0, 1) == AE_ROUND_REVIVE_UNDER_FIRE);
	CHECK(ae_round_bot_revive_decide(1.1f, 10, -1.0f, 0, 1) == AE_ROUND_REVIVE_GO);
	/* two orbs down: the pinned one stays the pick even when the other is nearer; gone or out of range: the nearest */
	{
		real two[2] = { 12.0f, 6.0f }, gone[2] = { -1.0f, 6.0f }, far[2] = { 30.0f, 8.0f };

		CHECK(ae_round_revive_orb_pick(-1, 2, two, 25.0f) == 1 && ae_round_revive_orb_pick(0, 2, two, 25.0f) == 0);
		CHECK(ae_round_revive_orb_pick(0, 2, gone, 25.0f) == 1 && ae_round_revive_orb_pick(0, 2, far, 25.0f) == 1);
		CHECK(ae_round_revive_orb_pick(0, 2, far, 5.0f) == -1 && ae_round_revive_orb_pick(-1, 0, two, 25.0f) == -1);
	}
	/* the near tie: a mate 1 wu nearer takes the orb from a bot that is not going, not from the one that is */
	CHECK(ae_round_revive_mate_is_nearer(9.0f, 10.0f, 0, 0) && !ae_round_revive_mate_is_nearer(9.0f, 10.0f, 0, 1));
	CHECK(!ae_round_revive_mate_is_nearer(7.5f, 10.0f, 0, 1) && ae_round_revive_mate_is_nearer(7.0f, 10.0f, 0, 1));
	CHECK(ae_round_revive_mate_is_nearer(10.0f, 10.0f, 1, 0) && !ae_round_revive_mate_is_nearer(10.0f, 10.0f, 0, 0));
	CHECK(!ae_round_revive_mate_is_nearer(10.0f, 10.0f, 1, 1));
	/* the wipe judge: two teams standing play on; one wiped loses; both on one tick tie */
	memset(members, 0, sizeof(members));
	memset(alive, 0, sizeof(alive));
	members[0] = 4, members[1] = 4, alive[0] = 2, alive[1] = 1;
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_GOES_ON && winner == -1);
	alive[1] = 0;
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_WON && winner == 0);
	alive[0] = 0, alive[1] = 3;
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_WON && winner == 1);
	alive[1] = 0;
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_TIED && winner == -1);
	/* (a side with nobody in the round is not judged: a lone side plays on until it is wiped, then ties) */
	members[1] = 0, alive[0] = 1;
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_GOES_ON);
	alive[0] = 0;
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_TIED);
	memset(members, 0, sizeof(members));
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_GOES_ON);
	/* without teams: the last player standing of many */
	memset(members, 0, sizeof(members));
	memset(alive, 0, sizeof(alive));
	members[0] = members[3] = members[7] = 1;
	alive[7] = 1;
	CHECK(ae_round_judge(AE_ROUND_SIDES, members, alive, &winner) == AE_ROUND_WON && winner == 7);
	alive[3] = 1;
	CHECK(ae_round_judge(AE_ROUND_SIDES, members, alive, &winner) == AE_ROUND_GOES_ON);
	/* (more sides than it judges: held to AE_ROUND_SIDES) */
	CHECK(ae_round_judge(1000, members, alive, &winner) == AE_ROUND_GOES_ON);

	/* the match: first to 5 of 9; spent level plays on with RESOLVE TIES */
	CHECK(!ae_round_match_over(9, 5, 1, 1, 1, 0));
	CHECK(ae_round_match_over(9, 5, 1, 5, 5, 0));
	CHECK(ae_round_match_over(9, 5, 1, 9, 5, 4));
	CHECK(!ae_round_match_over(9, 5, 1, 9, 4, 4));
	CHECK(ae_round_match_over(9, 5, 0, 9, 4, 4));
	CHECK(ae_round_match_over(9, 5, 1, 10, 5, 4));
	CHECK(ae_round_match_over(9, 5, 1, 9, 4, 3));
	/* (a fixed number of rounds: all played, the most wins) */
	CHECK(!ae_round_match_over(4, 0, 0, 3, 3, 0));
	CHECK(ae_round_match_over(4, 0, 0, 4, 3, 1));

	/* the sides switch from the to-win round on: round 5 of 9 */
	CHECK(ae_round_halftime(9, 5) == 5 && ae_round_halftime(4, 0) == 3);
	CHECK(!ae_round_sides_switched(4, 9, 5, 1) && ae_round_sides_switched(5, 9, 5, 1));
	CHECK(ae_round_sides_switched(10, 9, 5, 1));
	CHECK(!ae_round_sides_switched(5, 9, 5, 0) && !ae_round_sides_switched(1, 1, 0, 1));

	/* revive: 3 s, each later one 1.5 times as long, held to the growth's limit */
	{
		struct ae_variant_ext bleed;

		ae_variant_ext_default(&bleed);
		CHECK(ae_round_bleedout_seconds(&bleed) == 0);
		bleed.bleedout_s = 30;
		CHECK(ae_round_bleedout_seconds(&bleed) == 30);
	}
	CHECK(ae_round_revive_ticks(30, 0) == 90);
	CHECK(ae_round_revive_ticks(30, 1) == 135);
	CHECK(ae_round_revive_ticks(30, 2) == 202);
	CHECK(ae_round_revive_ticks(30, 3) == 303);
	CHECK(ae_round_revive_ticks(30, 100) == ae_round_revive_ticks(30, AE_ROUND_REVIVE_GROWTH_LIMIT));
	CHECK(ae_round_revive_ticks(0, 0) == 1);
	/* (letting go, or stepping out of reach, starts it over) */
	CHECK(ae_round_revive_step(0, 1) == 1 && ae_round_revive_step(40, 1) == 41 && ae_round_revive_step(40, 0) == 0);

	/* the frozen countdown: 5 s from the round's start */
	CHECK(ae_round_countdown_left(300, 300) == AE_ROUND_PRE_TICKS);
	CHECK(ae_round_countdown_left(300, 300 + AE_ROUND_PRE_TICKS - 1) == 1);
	CHECK(ae_round_countdown_left(300, 300 + AE_ROUND_PRE_TICKS) == 0);
	CHECK(ae_round_countdown_left(300, 299) == 0 && ae_round_countdown_left(-1, 5) == 0);

	/* the settings: the gametype's own rounds are first to 5 of 9; revive, zone and switch on unless turned off */
	ae_variant_ext_default(&ext);
	ext.mode = AE_EXT_MODE_ELIMINATION;
	CHECK(ae_round_max_rounds(&ext) == 9 && ae_round_to_win(&ext) == 5);
	ext.rounds = AE_EXT_ROUNDS_BEST_OF_5;
	CHECK(ae_round_max_rounds(&ext) == 5 && ae_round_to_win(&ext) == 3);
	ext.rounds = AE_EXT_ROUNDS_BEST_OF_9;
	CHECK(ae_round_max_rounds(&ext) == 9 && ae_round_to_win(&ext) == 5);
	ext.rounds = AE_EXT_ROUNDS_4;
	CHECK(ae_round_max_rounds(&ext) == 4 && ae_round_to_win(&ext) == 0);
	CHECK(ae_round_revive_on(&ext, 1) && !ae_round_revive_on(&ext, 0));
	CHECK(ae_round_zone_on(&ext) && ae_round_switch_on(&ext, 1) && !ae_round_switch_on(&ext, 0));
	CHECK(ae_round_revive_tenths(&ext) == 30 && ae_round_zone_wait_seconds(&ext) == 60 &&
		ae_round_zone_shrink_seconds(&ext) == 30);
	ext.round_flags = AE_EXT_ROUND_NO_REVIVE | AE_EXT_ROUND_NO_ZONE | AE_EXT_ROUND_NO_SIDE_SWITCH;
	ext.revive_10ths = 20;
	ext.zone_wait_s = 45;
	ext.zone_shrink_s = 15;
	CHECK(!ae_round_revive_on(&ext, 1) && !ae_round_zone_on(&ext) && !ae_round_switch_on(&ext, 1));
	CHECK(ae_round_revive_tenths(&ext) == 20 && ae_round_zone_wait_seconds(&ext) == 45 &&
		ae_round_zone_shrink_seconds(&ext) == 15);

	/* the scoreboard's word: a downed player (one life, so out of lives too) reads DOWN, not DEAD */
	CHECK(ae_scoreboard_status_rule(1, 1, 0, 0, 0) == AE_SCOREBOARD_STATUS_DOWN);
	CHECK(ae_scoreboard_status_rule(0, 1, 0, 0, 0) == AE_SCOREBOARD_STATUS_DEAD);
	CHECK(ae_scoreboard_status_rule(0, 0, 1, 0, 0) == AE_SCOREBOARD_STATUS_DEAD);
	CHECK(ae_scoreboard_status_rule(0, 0, 0, 1, 0) == AE_SCOREBOARD_STATUS_DEAD);
	CHECK(ae_scoreboard_status_rule(0, 0, 0, 0, 1) == AE_SCOREBOARD_STATUS_QUIT);
	CHECK(ae_scoreboard_status_rule(0, 0, 0, 0, 0) == AE_SCOREBOARD_STATUS_NONE);

	/* names */
	CHECK(!strcmp(ae_round_phase_name(AE_ROUND_PHASE_LIVE), "live") && !strcmp(ae_round_phase_name(99), "?"));
	CHECK(!strcmp(ae_round_player_state_name(AE_ROUND_PLAYER_DOWN), "down") &&
		!strcmp(ae_round_player_state_name(-1), "?"));

	/* the builds that play the round modes: 0.7.6 and later, and the 0.7.5 branch's -dev builds */
	CHECK(ae_round_build_has_it("Arena Evolved 0.7.6-beta") && ae_round_build_has_it("Arena Evolved 0.7.6-beta-dev"));
	CHECK(ae_round_build_has_it("Arena Evolved 0.7.10-beta") && ae_round_build_has_it("Arena Evolved 0.8.0-beta"));
	CHECK(ae_round_build_has_it("Arena Evolved 1.0.0") && ae_round_build_has_it("Arena Evolved 0.7.5-beta-dev"));
	CHECK(!ae_round_build_has_it("Arena Evolved 0.7.5-beta") && !ae_round_build_has_it("Arena Evolved 0.7.4-beta-dev"));
	CHECK(!ae_round_build_has_it("Arena Evolved 0.5.0-beta") && !ae_round_build_has_it("OpenCE 0.9.0"));
	CHECK(!ae_round_build_has_it("") && !ae_round_build_has_it(NULL) && !ae_round_build_has_it("Arena Evolved "));

	/* a bot breaks off to revive a person after 1 s without an enemy, a bot after 3 s */
	CHECK(ae_round_bot_revive_safe(31, 1) && !ae_round_bot_revive_safe(30, 1) && !ae_round_bot_revive_safe(60, 0) &&
		ae_round_bot_revive_safe(91, 0));

	/* a downed player's spot drops to the ground: found below, kept on the ground, none in reach = out */
	{
		float z = 0.0f;

		CHECK(ae_round_ground_spot_rule(1, 2.0f, 12.0f, &z) && z == 2.0f);
		CHECK(ae_round_ground_spot_rule(1, 5.0f, 5.0f, &z) && z == 5.0f);
		CHECK(!ae_round_ground_spot_rule(0, 0.0f, 12.0f, &z));
		CHECK(!ae_round_ground_spot_rule(1, -20.0f, 12.0f, &z));
		CHECK(!ae_round_ground_spot_rule(1, 13.0f, 12.0f, &z));
	}

	/* EXTERMINATION and ATTRITION: the mode's kind; a team whose players all wait to respawn is wiped (EXTERMINATION), but a
	paid respawn is still in the round (ATTRITION) */
	CHECK(ae_round_kind_of_mode(AE_EXT_MODE_ELIMINATION) == AE_ROUND_KIND_ELIMINATION);
	CHECK(ae_round_kind_of_mode(AE_EXT_MODE_EXTERMINATION) == AE_ROUND_KIND_EXTERMINATION);
	CHECK(ae_round_kind_of_mode(AE_EXT_MODE_ATTRITION) == AE_ROUND_KIND_ATTRITION);
	CHECK(ae_round_kind_of_mode(AE_EXT_MODE_NONE) == AE_ROUND_KIND_NONE && ae_round_kind_of_mode(AE_EXT_MODE_TERRITORIES) == AE_ROUND_KIND_NONE);
	CHECK(!ae_round_kind_respawns(AE_ROUND_KIND_ELIMINATION) && ae_round_kind_respawns(AE_ROUND_KIND_EXTERMINATION) &&
		ae_round_kind_respawns(AE_ROUND_KIND_ATTRITION));
	memset(members, 0, sizeof(members));
	memset(alive, 0, sizeof(alive));
	members[0] = 4, members[1] = 4;
	/* red: 0 living, 4 waiting to respawn; blue: 2 living */
	alive[0] = ae_round_standing(AE_ROUND_KIND_EXTERMINATION, 0, 4);
	alive[1] = ae_round_standing(AE_ROUND_KIND_EXTERMINATION, 2, 0);
	CHECK(alive[0] == 0 && ae_round_judge(2, members, alive, &winner) == AE_ROUND_WON && winner == 1);
	alive[0] = ae_round_standing(AE_ROUND_KIND_ATTRITION, 0, 4);
	CHECK(alive[0] == 4 && ae_round_judge(2, members, alive, &winner) == AE_ROUND_GOES_ON);
	alive[0] = ae_round_standing(AE_ROUND_KIND_ATTRITION, 0, 0);
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_WON && winner == 1);
	/* the time-out (0.7.8, owner 11-04): a tied round; the match's kills decide the match after 3 such ties */
	{
		int who = 9;

		/* (a time-out is a tied round; after 3 ties the match's kills decide the match; level kills go on) */
		CHECK(!ae_round_timeout_decides(1, 30, 3, &who) && who == -1);
		CHECK(!ae_round_timeout_decides(2, 30, 3, &who) && who == -1);
		CHECK(ae_round_timeout_decides(3, 30, 3, &who) && who == 0);
		CHECK(ae_round_timeout_decides(4, 12, 15, &who) && who == 1);
		CHECK(!ae_round_timeout_decides(3, 9, 9, &who) && who == -1);
	}
	/* the sides: EXTERMINATION switches every other round, ELIMINATION at its halfway round */
	CHECK(!ae_round_kind_switched(AE_ROUND_KIND_EXTERMINATION, 1, 9, 5, 1) && ae_round_kind_switched(AE_ROUND_KIND_EXTERMINATION, 2, 9, 5, 1) &&
		!ae_round_kind_switched(AE_ROUND_KIND_EXTERMINATION, 3, 9, 5, 1) && ae_round_kind_switched(AE_ROUND_KIND_EXTERMINATION, 4, 9, 5, 1));
	CHECK(!ae_round_kind_switched(AE_ROUND_KIND_EXTERMINATION, 2, 9, 5, 0));
	CHECK(!ae_round_kind_switched(AE_ROUND_KIND_ELIMINATION, 4, 9, 5, 1) && ae_round_kind_switched(AE_ROUND_KIND_ELIMINATION, 5, 9, 5, 1));
	/* ATTRITION's pool: 2 a player; a death spends one while it lasts, then the player goes down; the warnings */
	{
		int pool = ae_round_pool_size(4), start = pool, spent = 0, warned_low = 0, warned_empty = 0;

		CHECK(pool == 8 && ae_round_pool_size(2) == 4 && ae_round_pool_size(0) == 2);
		for (;;)
		{
			int before = pool, warn;

			if (!ae_round_pool_spend(&pool))
				break;
			spent++;
			warn = ae_round_pool_warning(start, before, pool);
			if (warn == AE_ROUND_POOL_WARN_LOW)
			{
				warned_low++;
				CHECK(pool == 3);
			}
			if (warn == AE_ROUND_POOL_WARN_EMPTY)
			{
				warned_empty++;
				CHECK(pool == 0);
			}
		}
		CHECK(spent == 8 && pool == 0 && warned_low == 1 && warned_empty == 1);
		CHECK(!ae_round_pool_spend(&pool) && pool == 0);
		CHECK(ae_round_pool_low_mark(8) == 3 && ae_round_pool_low_mark(4) == 3 && ae_round_pool_low_mark(12) == 7);
	}
	/* two teams' pools side by side, as ae_round_host holds them: a spend of one leaves the other, down to 0 */
	{
		int pools[2] = { 3, 0 };
		int pass;

		CHECK(ae_round_pool_spend(&pools[0]) && pools[0] == 2 && pools[1] == 0);
		CHECK(!ae_round_pool_spend(&pools[1]) && pools[1] == 0 && pools[0] == 2);
		pools[1] = 5;
		for (pass = 0; pass < 4; pass++)
			(void)ae_round_pool_spend(&pools[0]);
		CHECK(pools[0] == 0 && pools[1] == 5);
	}
	/* the pool empty: nobody alive or waiting to respawn, the rest down = a wipe (the downed are not standing) */
	alive[0] = ae_round_standing(AE_ROUND_KIND_ATTRITION, 0, 0);
	alive[1] = ae_round_standing(AE_ROUND_KIND_ATTRITION, 1, 0);
	CHECK(ae_round_judge(2, members, alive, &winner) == AE_ROUND_WON && winner == 1);
	/* ATTRITION's zone: both pools empty, or 10 minutes; its closings from 120 s (by 1:00) to 30 s (3:00 and later) */
	CHECK(!ae_round_zone_trigger(1, 0, 100 * 30) && !ae_round_zone_trigger(0, 2, 100 * 30) && ae_round_zone_trigger(0, 0, 100 * 30));
	CHECK(!ae_round_zone_trigger(3, 3, 599 * 30) && ae_round_zone_trigger(3, 3, 600 * 30));
	CHECK(ae_round_zone_closing_seconds(0) == 120 && ae_round_zone_closing_seconds(60 * 30) == 120 &&
		ae_round_zone_closing_seconds(180 * 30) == 30 && ae_round_zone_closing_seconds(500 * 30) == 30);
	CHECK(ae_round_zone_closing_seconds(120 * 30) == 75);
	/* the respawn modes need a build with them: 0.7.7 or later, or a -dev of 0.7.6 */
	/* (0.7.8 or later; the released 0.7.7 has no respawn modes; a 0.7.7 -dev build is the fix branch's own) */
	CHECK(!ae_round_build_has_respawn_modes("Arena Evolved 0.7.7.1-beta") && ae_round_build_has_respawn_modes("Arena Evolved 0.7.8-beta") &&
		ae_round_build_has_respawn_modes("Arena Evolved 0.8.0") && ae_round_build_has_respawn_modes("Arena Evolved 1.0.0"));
	CHECK(!ae_round_build_has_respawn_modes("Arena Evolved 0.7.7-beta") && !ae_round_build_has_respawn_modes("Arena Evolved 0.7.6-beta") &&
		!ae_round_build_has_respawn_modes("Arena Evolved 0.7.6-beta-dev") && !ae_round_build_has_respawn_modes("OpenCE 0.7.8") &&
		!ae_round_build_has_respawn_modes(NULL) && !ae_round_build_has_respawn_modes("Arena Evolved x"));
	CHECK(ae_round_build_has_respawn_modes_dev("Arena Evolved 0.7.7-beta-dev") && !ae_round_build_has_respawn_modes_dev("Arena Evolved 0.7.7-beta") &&
		!ae_round_build_has_respawn_modes_dev("Arena Evolved 0.7.71-beta-dev") && !ae_round_build_has_respawn_modes_dev("Arena Evolved 0.7.6-beta-dev"));

	if (failures)
		return 1;
	printf("ae_round_rules_test: ok\n");
	return 0;
}
