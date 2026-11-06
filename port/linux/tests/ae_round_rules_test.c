#include <stdio.h>
#include <string.h>
#include "ae_round_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int members[AE_ROUND_SIDES], alive[AE_ROUND_SIDES], winner = 99;
	struct ae_variant_ext ext;

	/* the player limit: 15 players have room for one more, 16 none; bots fill what the humans leave */
	CHECK(!ae_round_players_full(15) && ae_round_players_full(16) && ae_round_players_full(40));
	CHECK(ae_round_bot_cap(40, 1) == 15 && ae_round_bot_cap(40, 5) == 11 && ae_round_bot_cap(40, 0) == 15);
	CHECK(ae_round_bot_cap(40, 20) == 0 && ae_round_bot_cap(3, 1) == 3 && ae_round_bot_cap(-2, 1) == 0);
	/* a bot's call on an orb: near, unhurt, no enemy close, the nearest: go; each other case says why not (an enemy in view
	farther than 5 wu, however long, changes nothing: PLAYFIGHT bots see each other for ever) */
	CHECK(ae_round_bot_revive_decide(12.0f, -1, 30.0f, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(12.0f, 600, 6.0f, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(-1.0f, -1, -1.0f, 0) == AE_ROUND_REVIVE_NO_ORB);
	CHECK(ae_round_bot_revive_decide(26.0f, -1, -1.0f, 0) == AE_ROUND_REVIVE_FAR);
	CHECK(ae_round_bot_revive_decide(25.0f, -1, -1.0f, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(10.0f, 44, -1.0f, 0) == AE_ROUND_REVIVE_UNDER_FIRE);
	CHECK(ae_round_bot_revive_decide(10.0f, 45, -1.0f, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(10.0f, 100, 4.9f, 0) == AE_ROUND_REVIVE_ENEMY_NEAR);
	CHECK(ae_round_bot_revive_decide(10.0f, 100, 5.0f, 0) == AE_ROUND_REVIVE_GO);
	CHECK(ae_round_bot_revive_decide(10.0f, 100, -1.0f, 2) == AE_ROUND_REVIVE_NOT_NEAREST);
	CHECK(ae_round_bot_revive_decide(10.0f, 10, 3.0f, 1) == AE_ROUND_REVIVE_UNDER_FIRE);
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

	if (failures)
		return 1;
	printf("ae_round_rules_test: ok\n");
	return 0;
}
