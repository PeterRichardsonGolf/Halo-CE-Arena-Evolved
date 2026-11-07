#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_rating_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static void test_levels(void)
{
	int level;

	EQ(ae_rating_level_points(0), 0);
	EQ(ae_rating_level_points(1), 100);
	EQ(ae_rating_level_points(2), 200);
	EQ(ae_rating_level_points(3), 400);
	EQ(ae_rating_level_points(7), 1600);
	EQ(ae_rating_level_points(8), 2000);
	EQ(ae_rating_level_points(10), 3000);
	EQ(ae_rating_level_points(39), 17500);
	EQ(ae_rating_level_points(40), 18100);
	EQ(ae_rating_level_points(41), 18900);
	EQ(ae_rating_level_points(42), 20000);
	EQ(ae_rating_level_points(43), 21500);
	EQ(ae_rating_level_points(44), 23500);
	EQ(ae_rating_level_points(45), 26100);
	EQ(ae_rating_level_points(46), 29400);
	EQ(ae_rating_level_points(47), 33500);
	EQ(ae_rating_level_points(48), 38500);
	EQ(ae_rating_level_points(49), 44500);
	EQ(ae_rating_level_points(50), AE_RATING_POINTS_MAX);
	for (level = 1; level < AE_RATING_LEVELS; level++)
		CHECK(ae_rating_level_points(level) > ae_rating_level_points(level - 1));
	/* boundaries */
	EQ(ae_rating_level_from_points(0, 0), 0);
	EQ(ae_rating_level_from_points(99, 0), 0);
	EQ(ae_rating_level_from_points(100, 0), 1);
	EQ(ae_rating_level_from_points(399, 0), 2);
	EQ(ae_rating_level_from_points(400, 0), 3);
	EQ(ae_rating_level_from_points(1999, 0), 7);
	EQ(ae_rating_level_from_points(2000, 0), 8);
	EQ(ae_rating_level_from_points(3000, 0), 10);
	EQ(ae_rating_level_from_points(17499, 0), 38);
	EQ(ae_rating_level_from_points(17500, 0), 39);
	EQ(ae_rating_level_from_points(18099, 0), 39);
	EQ(ae_rating_level_from_points(18100, 0), 40);
	EQ(ae_rating_level_from_points(44499, 0), 48);
	EQ(ae_rating_level_from_points(44500, 0), 49);
	EQ(ae_rating_level_from_points(AE_RATING_POINTS_MAX, 0), 49);
	EQ(ae_rating_level_shown(0), 1);
	EQ(ae_rating_level_shown(49), 50);
	/* hysteresis: level 10 starts at 3000, level 9 at 2500, the middle of level 9's band 2750 */
	EQ(ae_rating_level_from_points(2800, 10), 10);
	EQ(ae_rating_level_from_points(2751, 10), 10);
	EQ(ae_rating_level_from_points(2750, 10), 9);
	EQ(ae_rating_level_from_points(2400, 10), 8);       /* two levels down: no keeping */
	EQ(ae_rating_level_from_points(2800, 0), 9);
	EQ(ae_rating_level_from_points(2800, 9), 9);
	EQ(ae_rating_level_from_points(3200, 10), 10);
	EQ(ae_rating_level_from_points(50, 1), 0);
	EQ(ae_rating_level_from_points(51, 1), 1);
	/* the top band */
	EQ(ae_rating_level_from_points(44000, 50), 48);
}

static void test_pairs(void)
{
	int d;

	/* the base tables */
	EQ(ae_rating_win_base(0), 180);
	EQ(ae_rating_win_base(119), 180);
	EQ(ae_rating_win_base(120), 170);
	EQ(ae_rating_win_base(127), 100);
	EQ(ae_rating_win_base(134), 30);
	EQ(ae_rating_win_base(135), 20);
	EQ(ae_rating_win_base(254), 20);
	EQ(ae_rating_loss_base(119), -20);
	EQ(ae_rating_loss_base(120), -30);
	EQ(ae_rating_loss_base(127), -100);
	EQ(ae_rating_loss_base(134), -170);
	EQ(ae_rating_loss_base(135), -180);
	for (d = 1; d < AE_RATING_DIFFERENCES; d++)
	{
		CHECK(ae_rating_win_base(d) <= ae_rating_win_base(d - 1));
		CHECK(ae_rating_loss_base(d) <= ae_rating_loss_base(d - 1));
	}
	/* the weights */
	EQ(ae_rating_win_weight(0), 100);
	EQ(ae_rating_win_weight(40), 100);
	EQ(ae_rating_win_weight(41), 95);
	EQ(ae_rating_win_weight(42), 90);
	EQ(ae_rating_win_weight(43), 85);
	EQ(ae_rating_win_weight(44), 80);
	EQ(ae_rating_win_weight(45), 70);
	EQ(ae_rating_win_weight(46), 65);
	EQ(ae_rating_win_weight(47), 60);
	EQ(ae_rating_win_weight(48), 55);
	EQ(ae_rating_win_weight(49), 50);
	EQ(ae_rating_loss_weight(0), 0);
	EQ(ae_rating_loss_weight(1), 5);
	EQ(ae_rating_loss_weight(3), 20);
	EQ(ae_rating_loss_weight(5), 45);
	EQ(ae_rating_loss_weight(14), 95);
	EQ(ae_rating_loss_weight(15), 100);
	EQ(ae_rating_loss_weight(49), 100);
	/* wins and losses at -8 .. +8 levels, own level 20 (weights 100) */
	EQ(ae_rating_pair_change(20, 28, 0, 1), 180);   /* the other 8 levels above */
	EQ(ae_rating_pair_change(20, 29, 0, 1), 180);
	EQ(ae_rating_pair_change(20, 27, 0, 1), 170);
	EQ(ae_rating_pair_change(20, 20, 0, 1), 100);
	EQ(ae_rating_pair_change(20, 14, 0, 1), 40);
	EQ(ae_rating_pair_change(20, 15, 0, 1), 50);
	EQ(ae_rating_pair_change(20, 13, 0, 1), 30);    /* 7 below */
	EQ(ae_rating_pair_change(20, 12, 0, 1), 20);    /* 8 below */
	EQ(ae_rating_pair_change(20, 5, 0, 1), 20);
	EQ(ae_rating_pair_change(20, 28, 1, 0), -20);
	EQ(ae_rating_pair_change(20, 27, 1, 0), -30);
	EQ(ae_rating_pair_change(20, 20, 1, 0), -100);
	EQ(ae_rating_pair_change(20, 13, 1, 0), -170);
	EQ(ae_rating_pair_change(20, 12, 1, 0), -180);
	/* weights by own level */
	EQ(ae_rating_pair_change(0, 0, 1, 0), 0);
	EQ(ae_rating_pair_change(5, 5, 1, 0), -45);
	EQ(ae_rating_pair_change(14, 14, 1, 0), -95);
	EQ(ae_rating_pair_change(15, 15, 1, 0), -100);
	EQ(ae_rating_pair_change(3, 10, 1, 0), -6);     /* -30 * 20 / 100 */
	EQ(ae_rating_pair_change(1, 8, 1, 0), -1);      /* -30 * 5 / 100 = -1.5: cut toward zero */
	EQ(ae_rating_pair_change(41, 41, 0, 1), 95);
	EQ(ae_rating_pair_change(45, 45, 0, 1), 70);
	EQ(ae_rating_pair_change(49, 49, 0, 1), 50);
	EQ(ae_rating_pair_change(49, 49, 1, 0), -100);
	/* places: equal or both none: nothing; none behind placed */
	EQ(ae_rating_pair_change(20, 20, 2, 2), 0);
	EQ(ae_rating_pair_change(20, 20, -1, -1), 0);
	EQ(ae_rating_pair_change(20, 20, 3, -1), 100);
	EQ(ae_rating_pair_change(20, 20, -1, 3), -100);
}

static struct ae_rating_entry entry(int present, int rated, int level, int place, int team)
{
	struct ae_rating_entry e;

	e.present = present; e.rated = rated; e.level = level; e.place = place; e.team = team;
	e.quit = 0; e.quit_order = -1; e.party = -1;
	return e;
}

static void test_games(void)
{
	struct ae_rating_entry ffa[4], teams[8], pair[2];
	struct ae_rating_state state;
	int i;

	/* free for all: the player (level 20, third of four... second) against 20 (first), 12 (third), 28 (fourth) */
	ffa[0] = entry(1, 1, 20, 1, 0);
	ffa[1] = entry(1, 1, 20, 0, 1);
	ffa[2] = entry(1, 1, 12, 2, 2);
	ffa[3] = entry(1, 1, 28, 3, 3);
	EQ(ae_rating_game_change(ffa, 4, 0, 1000), 33);     /* (-100 + 20 + 180) / 3 */
	ffa[0].place = 0; ffa[1].place = 1;
	EQ(ae_rating_game_change(ffa, 4, 0, 1000), 100);    /* (100 + 20 + 180) / 3 */
	/* an opponent who is not rated, a guest or never there does not count */
	ffa[2].rated = 0;
	EQ(ae_rating_game_change(ffa, 4, 0, 1000), 140);    /* (100 + 180) / 2 */
	ffa[3].present = 0;
	EQ(ae_rating_game_change(ffa, 4, 0, 1000), 100);
	ffa[1].present = 0;
	EQ(ae_rating_game_change(ffa, 4, 0, 1000), 0);
	/* the floor and the ceiling of the points */
	pair[0] = entry(1, 1, 20, 1, 0);
	pair[1] = entry(1, 1, 20, 0, 1);
	EQ(ae_rating_game_change(pair, 2, 0, 30), -30);
	EQ(ae_rating_game_change(pair, 2, 0, 0), 0);
	pair[0].place = 0; pair[1].place = 1;
	EQ(ae_rating_game_change(pair, 2, 0, AE_RATING_POINTS_MAX - 10), 10);
	/* teams: 4 against 4, every member takes the team's place; the player's own team does not count */
	for (i = 0; i < 8; i++)
		teams[i] = entry(1, 1, 20, i < 4 ? 0 : 1, i < 4 ? 0 : 1);
	EQ(ae_rating_game_change(teams, 8, 0, 1000), 100);
	EQ(ae_rating_game_change(teams, 8, 5, 1000), -100);
	teams[1].level = 49;    /* a teammate: no matter */
	EQ(ae_rating_game_change(teams, 8, 0, 1000), 100);
	teams[6].level = 29;    /* one opponent far above: (180 + 100 * 3) / 4 */
	EQ(ae_rating_game_change(teams, 8, 0, 1000), 120);
	/* one entry for each team (parties up to a team): the same function */
	pair[0] = entry(1, 1, 20, 0, 0);
	pair[1] = entry(1, 1, 30, 1, 1);
	EQ(ae_rating_game_change(pair, 2, 0, 5000), 180);
	EQ(ae_rating_game_change(pair, 2, 1, 5000), -180);
	pair[0].place = 1; pair[1].place = 0;
	EQ(ae_rating_game_change(pair, 2, 0, 5000), -20);
	EQ(ae_rating_game_change(pair, 2, 1, 5000), 20);
	/* a quitter: he has no place, so he is behind a player who placed; the player who placed beats him */
	pair[0] = entry(1, 1, 20, -1, 0);
	pair[1] = entry(1, 1, 20, 0, 1);
	EQ(ae_rating_quit_change(AE_RATING_QUIT_AS_LAST, pair, 2, 0, 1000), -100);
	EQ(ae_rating_quit_change(AE_RATING_QUIT_NONE, pair, 2, 0, 1000), 0);
	EQ(ae_rating_game_change(pair, 2, 1, 1000), 100);
	pair[0].place = 0;
	EQ(ae_rating_quit_change(AE_RATING_QUIT_AS_LAST, pair, 2, 0, 1000), -100);  /* his place is taken away */
	/* a standing */
	ae_rating_state_init(&state, 2800);
	EQ(state.level, 9);
	EQ(state.highest, 9);
	ae_rating_apply(&state, 300);
	EQ(state.points, 3100);
	EQ(state.level, 10);
	EQ(state.highest, 10);
	ae_rating_apply(&state, -300);      /* 2800: still above the middle of the band below */
	EQ(state.level, 10);
	ae_rating_apply(&state, -100);      /* 2700 */
	EQ(state.level, 9);
	EQ(state.highest, 10);
	ae_rating_apply(&state, -100000);
	EQ(state.points, 0);
	EQ(state.level, 0);
	ae_rating_apply(&state, AE_RATING_POINTS_MAX + 5L);
	EQ(state.points, AE_RATING_POINTS_MAX);
	EQ(state.level, 49);
	EQ(state.highest, 49);
}

static void test_quit_protection(void)
{
	struct ae_rating_entry e[8];
	int i;

	/* 4 v 4, team 1 wins (place 0), team 0 loses (place 1), all level 20; parties: 0,1 together on team 0 */
	for (i = 0; i < 8; i++)
	{
		e[i] = entry(1, 1, 20, i < 4 ? 1 : 0, i < 4 ? 0 : 1);
		e[i].party = i;
	}
	e[0].party = 100; e[1].party = 100;
	/* no quit: the plain change */
	EQ(ae_rating_protected_change(e, 8, 0, 1000, -1), -100);
	EQ(ae_rating_protected_change(e, 8, 5, 1000, 500000), 100);
	CHECK(!ae_rating_protected_counts_quit(e, 8, 0));
	/* player 2 (alone, team 0) quits first, late: he takes the full loss and the quit */
	e[2].quit = 1; e[2].quit_order = 0;
	EQ(ae_rating_protected_change(e, 8, 2, 1000, 300000), -100);
	CHECK(ae_rating_protected_counts_quit(e, 8, 2));
	/* his teammate outside his party, who stays and loses: nothing; a gain would stay */
	EQ(ae_rating_protected_change(e, 8, 0, 1000, 300000), 0);
	EQ(ae_rating_protected_change(e, 8, 3, 1000, 300000), 0);
	/* the winners keep their gain (the quitter is behind everyone, they win against the other team as before) */
	EQ(ae_rating_protected_change(e, 8, 5, 1000, 300000), 100);
	/* the first quitter's party member who stays takes the normal change */
	e[2].party = 100;
	EQ(ae_rating_protected_change(e, 8, 0, 1000, 300000), -100);
	EQ(ae_rating_protected_change(e, 8, 3, 1000, 300000), 0);
	e[2].party = 2;
	/* a later leaver outside any quitter's party: no loss, no quit */
	e[3].quit = 1; e[3].quit_order = 1;
	EQ(ae_rating_protected_change(e, 8, 3, 1000, 300000), 0);
	CHECK(!ae_rating_protected_counts_quit(e, 8, 3));
	/* a later leaver of the first quitter's party: 25% of the normal (quit) loss, and the quit counts */
	e[3].party = 2;
	EQ(ae_rating_protected_change(e, 8, 3, 1000, 300000), -25);
	CHECK(ae_rating_protected_counts_quit(e, 8, 3));
	/* a party with an earlier quitter that is not the first */
	e[3].party = 3;
	e[1].quit = 1; e[1].quit_order = 2;       /* party 100: no earlier quitter of party 100 */
	EQ(ae_rating_protected_change(e, 8, 1, 1000, 300000), 0);
	e[0].quit = 1; e[0].quit_order = 3;       /* party 100 again: player 1 left before him */
	EQ(ae_rating_protected_change(e, 8, 0, 1000, 300000), -25);
	CHECK(ae_rating_protected_counts_quit(e, 8, 0));
	CHECK(!ae_rating_protected_counts_quit(e, 8, 1));
	/* an early first quit caps everyone else at +-20; the first quitter is not capped */
	e[0].quit = 0; e[0].quit_order = -1; e[1].quit = 0; e[1].quit_order = -1; e[3].quit = 0; e[3].quit_order = -1;
	EQ(ae_rating_protected_change(e, 8, 2, 1000, 119999), -100);
	EQ(ae_rating_protected_change(e, 8, 5, 1000, 119999), AE_RATING_QUIT_EARLY_CAP);
	EQ(ae_rating_protected_change(e, 8, 5, 1000, 120000), 100);
	EQ(ae_rating_protected_change(e, 8, 5, 1000, -1), 100);
	EQ(ae_rating_protected_change(e, 8, 5, 1000, 0), AE_RATING_QUIT_EARLY_CAP);
	/* an early quit: a party member of the first quitter who stays and loses is capped to -20, not protected */
	e[2].party = 100;
	EQ(ae_rating_protected_change(e, 8, 0, 1000, 60000), -AE_RATING_QUIT_EARLY_CAP);
	/* an early later leaver of the quitter's party: 25 % then the cap does not bite */
	e[2].party = 2; e[3].party = 2; e[3].quit = 1; e[3].quit_order = 1;
	EQ(ae_rating_protected_change(e, 8, 3, 1000, 60000), -20);
	/* the gain of a stayer is also capped when early; the points floor holds */
	EQ(ae_rating_protected_change(e, 8, 6, 5, 60000), 20);
	/* a rejoined player is passed as quit = 0: the plain change when no one else quit */
	e[2].quit = 0; e[2].quit_order = -1; e[3].quit = 0; e[3].quit_order = -1;
	EQ(ae_rating_protected_change(e, 8, 2, 1000, 60000), -100);
	/* a quitter counts behind everyone for the stayers' change in the plain function too */
	e[2].quit = 1; e[2].quit_order = 0; e[2].place = 0;
	EQ(ae_rating_game_change(e, 8, 5, 1000), 100);
	EQ(ae_rating_game_change(e, 8, 2, 1000), -100);
}

static void test_window(void)
{
	struct ae_rating_window w;
	struct ae_rating_search s;
	int minimum, maximum, level, step;

	ae_rating_window_default(&w);
	EQ(ae_rating_window_upper(&w, 0), 5);
	EQ(ae_rating_window_upper(&w, 5), 10);
	EQ(ae_rating_window_upper(&w, 6), 12);
	EQ(ae_rating_window_upper(&w, 15), 21);
	EQ(ae_rating_window_upper(&w, 16), 23);
	EQ(ae_rating_window_upper(&w, 25), 32);
	EQ(ae_rating_window_upper(&w, 26), 34);
	EQ(ae_rating_window_upper(&w, 30), 38);
	EQ(ae_rating_window_upper(&w, 31), 40);
	EQ(ae_rating_window_upper(&w, 35), 44);
	EQ(ae_rating_window_upper(&w, 36), 46);
	EQ(ae_rating_window_upper(&w, 49), 59);
	EQ(ae_rating_window_upper(&w, 120), 127);
	EQ(ae_rating_window_upper(&w, 127), 127);
	for (level = 0; level < AE_RATING_WINDOW_LEVELS; level++)
		CHECK(ae_rating_window_upper(&w, level) >= level);
	EQ(ae_rating_window_low(&w, 0), 0);
	EQ(ae_rating_window_low(&w, 5), 0);
	EQ(ae_rating_window_low(&w, 6), 1);
	EQ(ae_rating_window_low(&w, 10), 5);
	EQ(ae_rating_window_low(&w, 20), 14);
	EQ(ae_rating_window_low(&w, 30), 23);
	EQ(ae_rating_window_low(&w, 49), 39);
	/* a party plays at the higher of its member's level and the low of its best */
	EQ(ae_rating_party_level(&w, 10, 30), 23);
	EQ(ae_rating_party_level(&w, 30, 30), 30);
	EQ(ae_rating_party_level(&w, 40, 30), 40);
	EQ(ae_rating_party_level(&w, 10, 10), 10);
	/* a solo search at level 10: [10,10] widening to [5,16] in 6 steps */
	ae_rating_search_setup(&w, 10, 10, &s);
	EQ(s.low, 5); EQ(s.high, 10); EQ(s.upper, 16); EQ(s.span, 6);
	ae_rating_search_range(&s, 0, &minimum, &maximum);
	EQ(minimum, 10); EQ(maximum, 10);
	ae_rating_search_range(&s, 1, &minimum, &maximum);
	EQ(minimum, 9); EQ(maximum, 11);
	ae_rating_search_range(&s, 5, &minimum, &maximum);
	EQ(minimum, 5); EQ(maximum, 15);
	ae_rating_search_range(&s, 6, &minimum, &maximum);
	EQ(minimum, 5); EQ(maximum, 16);
	ae_rating_search_range(&s, AE_RATING_STEP_OPEN, &minimum, &maximum);
	EQ(minimum, 0); EQ(maximum, 127);
	CHECK(ae_rating_search_allows(&s, 0, 10, 10));
	CHECK(!ae_rating_search_allows(&s, 0, 9, 10));
	CHECK(!ae_rating_search_allows(&s, 0, 10, 11));
	CHECK(ae_rating_search_allows(&s, 3, 8, 12));
	CHECK(ae_rating_search_allows(&s, AE_RATING_STEP_OPEN, 0, 127));
	/* the steps: ranked ends at the span, social opens once and ends */
	step = 0;
	for (level = 0; level < 6; level++)
		step = ae_rating_search_next(&s, step, 1);
	EQ(step, 6);
	EQ(ae_rating_search_next(&s, step, 1), AE_RATING_STEP_END);
	EQ(ae_rating_search_next(&s, step, 0), AE_RATING_STEP_OPEN);
	EQ(ae_rating_search_next(&s, AE_RATING_STEP_OPEN, 0), AE_RATING_STEP_END);
	/* the minimum score */
	EQ(ae_rating_search_minimum_score(0, 6), 200);
	EQ(ae_rating_search_minimum_score(3, 6), 150);
	EQ(ae_rating_search_minimum_score(6, 6), 100);
	EQ(ae_rating_search_minimum_score(AE_RATING_STEP_OPEN, 6), 100);
	EQ(ae_rating_search_minimum_score(0, 0), 100);
	/* a party of 10 and 20: the window is that of the pair */
	ae_rating_search_setup(&w, 10, 20, &s);
	EQ(s.low, 5); EQ(s.high, 20); EQ(s.upper, 27); EQ(s.span, 17);
	ae_rating_search_range(&s, 17, &minimum, &maximum);
	EQ(minimum, 5); EQ(maximum, 27);
	ae_rating_search_range(&s, 10, &minimum, &maximum);
	EQ(minimum, 10); EQ(maximum, 20);
	CHECK(!ae_rating_search_exhausted(149));
	CHECK(ae_rating_search_exhausted(150));
}

static struct ae_rating_candidate candidate(int level, int same, int players)
{
	struct ae_rating_candidate c;

	memset(&c, 0, sizeof(c));
	c.level = level; c.same_playlist = same; c.players = players;
	return c;
}

static void test_score(void)
{
	struct ae_rating_candidate c;
	unsigned strict = AE_RATING_LANGUAGES_STRICT;

	c = candidate(20, 0, 0);
	EQ(ae_rating_candidate_score(&c, 20, strict), 100);
	c.level = 21; EQ(ae_rating_candidate_score(&c, 20, strict), 85);
	c.level = 19; EQ(ae_rating_candidate_score(&c, 20, strict), 85);
	c.level = 24; EQ(ae_rating_candidate_score(&c, 20, strict), 40);
	c.level = 25; EQ(ae_rating_candidate_score(&c, 20, strict), 0);
	c = candidate(20, 1, 10);
	EQ(ae_rating_candidate_score(&c, 20, strict), 170);    /* 100 + 50 + 20 */
	c = candidate(20, 1, 500);
	EQ(ae_rating_candidate_score(&c, 20, strict), 510);    /* the players capped at 180: 100 + 50 + 360 */
	/* round trip */
	EQ(ae_rating_round_trip_score(0), 100);
	EQ(ae_rating_round_trip_score(100), 80);
	EQ(ae_rating_round_trip_score(150), 65);
	EQ(ae_rating_round_trip_score(200), 50);
	EQ(ae_rating_round_trip_score(250), 25);
	EQ(ae_rating_round_trip_score(300), 0);
	EQ(ae_rating_round_trip_score(400), -100);
	EQ(ae_rating_round_trip_score(450), -150);
	EQ(ae_rating_round_trip_score(500), -200);
	EQ(ae_rating_round_trip_score(5000), -200);
	EQ(ae_rating_round_trip_score(-5), 100);
	c = candidate(20, 0, 0);
	c.round_trip_known = 1; c.round_trip_ms = 0;
	EQ(ae_rating_candidate_score(&c, 20, strict), 200);
	c.round_trip_ms = 300;
	EQ(ae_rating_candidate_score(&c, 20, strict), 100);
	c.round_trip_ms = 400;
	EQ(ae_rating_candidate_score(&c, 20, strict), 0);
	/* language */
	EQ(ae_rating_language_score(0, 0, strict), 0);
	EQ(ae_rating_language_score(1, 1, strict), 100);
	EQ(ae_rating_language_score(6, 6, strict), 100);
	EQ(ae_rating_language_score(0, 3, strict), -50);
	EQ(ae_rating_language_score(0, 1, strict), -100);
	EQ(ae_rating_language_score(7, 2, strict), -100);
	EQ(ae_rating_language_score(2, 7, strict), -100);
	EQ(ae_rating_language_score(2, 3, 0), -50);
	/* a weak host: only when mine is strong; the details gate the language and the bandwidth terms */
	c = candidate(20, 0, 0);
	c.host_up = 100000; c.host_down = 500000; c.my_up = 300000; c.my_down = 300000;
	EQ(ae_rating_candidate_score(&c, 20, strict), 100);
	c.has_details = 1;
	EQ(ae_rating_candidate_score(&c, 20, strict), 200);
	c.my_down = 100000;
	EQ(ae_rating_candidate_score(&c, 20, strict), 100);
	c.my_down = 300000; c.host_up = 300000;
	EQ(ae_rating_candidate_score(&c, 20, strict), 100);
	c.my_language = 0; c.host_language = 3;
	EQ(ae_rating_candidate_score(&c, 20, strict), 50);
	/* when to take the best */
	CHECK(ae_rating_search_ready(4, 4, 0));
	CHECK(ae_rating_search_ready(4, 3, 6000));
	CHECK(ae_rating_search_ready(8, 2, 6000));
	CHECK(!ae_rating_search_ready(8, 2, 5000));
	CHECK(!ae_rating_search_ready(8, 1, 9999));
	CHECK(!ae_rating_search_ready(0, 0, 9999));
	CHECK(ae_rating_search_ready(3, 0, 6000));      /* a quarter of 3 is none: after the wait */
}

static void test_delay(void)
{
	EQ(ae_rating_start_delay_players(30000, 4, 8, 3), AE_RATING_NEVER);
	EQ(ae_rating_start_delay_players(30000, 4, 8, 9), AE_RATING_NEVER);
	EQ(ae_rating_start_delay_players(30000, 4, 8, 8), 0);
	EQ(ae_rating_start_delay_players(30000, 4, 8, 4), 30000);
	EQ(ae_rating_start_delay_players(30000, 4, 8, 6), 15000);
	EQ(ae_rating_start_delay_players(120000, 2, 2, 2), 0);
	EQ(ae_rating_start_delay_teams(30000, 2, 3, 4, 5), AE_RATING_NEVER);
	EQ(ae_rating_start_delay_teams(30000, 2, 3, 4, 6), 30000);
	EQ(ae_rating_start_delay_teams(30000, 2, 3, 4, 7), 30000);
	EQ(ae_rating_start_delay_teams(30000, 2, 3, 4, 8), 0);
	EQ(ae_rating_start_delay_teams(45000, 2, 5, 8, 10), 45000);
	EQ(ae_rating_start_delay_teams(45000, 2, 5, 8, 12), 30000);
	EQ(ae_rating_start_delay_teams(45000, 2, 5, 8, 13), 30000);
	EQ(ae_rating_start_delay_teams(45000, 2, 5, 8, 14), 15000);
	EQ(ae_rating_start_delay_teams(45000, 2, 5, 8, 16), 0);
	EQ(ae_rating_start_delay_teams(45000, 2, 5, 8, 17), AE_RATING_NEVER);
}

static void test_balance(void)
{
	long weights[16] = {40, 30, 20, 10};
	int party[16] = {0, 1, 2, 3};
	int team_of[16], greedy[16];
	long cost, cost_greedy;
	struct ae_rating_rng rng;
	int trial, i, count, teams, size;

	/* cost */
	team_of[0] = 0; team_of[1] = 1; team_of[2] = 1; team_of[3] = 0;
	EQ(ae_rating_balance_cost(2, 4, weights, team_of), 60);
	team_of[0] = 0; team_of[1] = 0; team_of[2] = 1; team_of[3] = 1;
	EQ(ae_rating_balance_cost(2, 4, weights, team_of), 7 * 40 + 3 * 40);
	EQ(ae_rating_balance_count_spread(2, 4, team_of), 0);
	team_of[3] = 0;
	EQ(ae_rating_balance_count_spread(2, 4, team_of), 2);
	/* four solo players */
	CHECK(ae_rating_balance_teams(2, 2, 4, weights, party, 1, 1, 0, team_of, &cost));
	EQ(team_of[0], 0); EQ(team_of[1], 1); EQ(team_of[2], 1); EQ(team_of[3], 0);
	EQ(cost, 60);
	/* a party of two stays together */
	party[0] = 0; party[1] = 0; party[2] = 1; party[3] = 2;
	CHECK(ae_rating_balance_teams(2, 2, 4, weights, party, 1, 1, 0, team_of, &cost));
	EQ(team_of[0], 0); EQ(team_of[1], 0); EQ(team_of[2], 1); EQ(team_of[3], 1);
	EQ(cost, 400);
	/* too small a team size: refused */
	party[0] = 0; party[1] = 1; party[2] = 2; party[3] = 3;
	CHECK(!ae_rating_balance_teams(2, 1, 4, weights, party, 1, 1, 0, team_of, &cost));
	/* six solo players */
	{
		long w6[6] = {60, 50, 40, 30, 20, 10};
		int p6[6] = {0, 1, 2, 3, 4, 5}, t6[6], n[2] = {0, 0};

		CHECK(ae_rating_balance_teams(2, 3, 6, w6, p6, 1, 1, 0, t6, &cost));
		for (i = 0; i < 6; i++)
			n[t6[i]]++;
		EQ(n[0], 3); EQ(n[1], 3);
		EQ(cost, 160);
	}
	/* the swaps never leave the teams worse than the deal */
	ae_rating_rng_seed(&rng, 7);
	for (trial = 0; trial < 300; trial++)
	{
		long w[16];
		int pp[16];

		teams = 2 + (int)ae_rating_rng_below(&rng, 3);
		size = 1 + (int)ae_rating_rng_below(&rng, 5);
		count = teams * size;
		if (count > 16)
			count = 16;
		for (i = 0; i < count; i++)
		{
			w[i] = (long)ae_rating_rng_below(&rng, 50);
			pp[i] = (int)ae_rating_rng_below(&rng, 4);
		}
		if (!ae_rating_balance_teams(teams, size + 3, count, w, pp, 0, 1, 0, greedy, &cost_greedy))
			continue;
		CHECK(ae_rating_balance_teams(teams, size + 3, count, w, pp, 1, 1, 0, team_of, &cost));
		CHECK(ae_rating_balance_count_spread(teams, count, team_of) <= ae_rating_balance_count_spread(teams, count, greedy));
		if (ae_rating_balance_count_spread(teams, count, team_of) == ae_rating_balance_count_spread(teams, count, greedy))
			CHECK(cost <= cost_greedy);
		for (i = 0; i < count; i++)
			CHECK(team_of[i] >= 0 && team_of[i] < teams);
		/* a party is never split */
		for (i = 0; i < count; i++)
		{
			int j;

			for (j = 0; j < count; j++)
				if (pp[i] == pp[j])
					CHECK(team_of[i] == team_of[j]);
		}
	}
}

static void test_bots(void)
{
	struct ae_rating_window w;
	struct ae_rating_rng a, b;
	struct ae_rating_roster r1, r2;
	int counts[AE_RATING_LEVELS], level, i, ok = 1;
	long sum = 0;

	ae_rating_window_default(&w);
	EQ(ae_rating_bot_anchor(AE_RATING_BOT_RECRUIT), 5);
	EQ(ae_rating_bot_anchor(AE_RATING_BOT_NORMAL), 17);
	EQ(ae_rating_bot_anchor(AE_RATING_BOT_HARD), 30);
	EQ(ae_rating_bot_anchor(AE_RATING_BOT_LEGENDARY), 42);
	EQ(ae_rating_bot_anchor(AE_RATING_BOT_ELITE), 48);
	EQ(ae_rating_level_shown(ae_rating_bot_anchor(AE_RATING_BOT_ELITE)), 49);
	EQ(ae_rating_bot_difficulty(0), AE_RATING_BOT_RECRUIT);
	EQ(ae_rating_bot_difficulty(11), AE_RATING_BOT_RECRUIT);    /* a tie goes down */
	EQ(ae_rating_bot_difficulty(12), AE_RATING_BOT_NORMAL);
	EQ(ae_rating_bot_difficulty(23), AE_RATING_BOT_NORMAL);
	EQ(ae_rating_bot_difficulty(24), AE_RATING_BOT_HARD);
	EQ(ae_rating_bot_difficulty(36), AE_RATING_BOT_HARD);
	EQ(ae_rating_bot_difficulty(37), AE_RATING_BOT_LEGENDARY);
	EQ(ae_rating_bot_difficulty(45), AE_RATING_BOT_LEGENDARY);
	EQ(ae_rating_bot_difficulty(46), AE_RATING_BOT_ELITE);
	EQ(ae_rating_bot_difficulty(49), AE_RATING_BOT_ELITE);
	/* the draw: inside the window, most at the player's own level, repeatable */
	ae_rating_rng_seed(&a, 42);
	ae_rating_rng_seed(&b, 42);
	memset(counts, 0, sizeof(counts));
	for (i = 0; i < 4000; i++)
	{
		level = ae_rating_roster_draw_level(&w, &a, 20);
		CHECK(level >= 14 && level <= 27);
		CHECK(level == ae_rating_roster_draw_level(&w, &b, 20));
		counts[level]++;
		sum += level;
	}
	CHECK(counts[20] > counts[27] * 3);
	CHECK(counts[20] > counts[14] * 2);
	CHECK(sum / 4000 >= 19 && sum / 4000 <= 21);
	for (level = 14; level <= 27; level++)
		CHECK(counts[level] > 0);
	/* the top: the window is cut at level 49 */
	for (i = 0; i < 500; i++)
		CHECK(ae_rating_roster_draw_level(&w, &a, 49) >= 39 && ae_rating_roster_draw_level(&w, &a, 49) <= 49);
	/* a roster of 4 against 4 */
	ae_rating_rng_seed(&a, 9);
	ae_rating_rng_seed(&b, 9);
	CHECK(ae_rating_roster_make(&w, &a, 20, 2, 4, &r1));
	CHECK(ae_rating_roster_make(&w, &b, 20, 2, 4, &r2));
	CHECK(memcmp(&r1, &r2, sizeof(r1)) == 0);
	EQ(r1.count, 8); EQ(r1.team_count, 2);
	EQ(r1.bot[0].level, 20); EQ(r1.bot[0].difficulty, -1);
	{
		int n[2] = {0, 0};
		long w8[8];
		int t8[8];

		for (i = 0; i < 8; i++)
		{
			CHECK(r1.bot[i].team == 0 || r1.bot[i].team == 1);
			n[r1.bot[i].team]++;
			w8[i] = r1.bot[i].level;
			t8[i] = r1.bot[i].team;
			if (i)
			{
				CHECK(r1.bot[i].level >= 14 && r1.bot[i].level <= 27);
				CHECK(r1.bot[i].difficulty == ae_rating_bot_difficulty(r1.bot[i].level));
				CHECK(r1.bot[i].shooting_jitter >= -1 && r1.bot[i].shooting_jitter <= 1);
				CHECK(r1.bot[i].movement_jitter >= -1 && r1.bot[i].movement_jitter <= 1);
			}
		}
		EQ(n[0], 4); EQ(n[1], 4);
		EQ(r1.cost, ae_rating_balance_cost(2, 8, w8, t8));
	}
	/* a free for all: every seat its own team; shapes that do not fit */
	CHECK(ae_rating_roster_make(&w, &a, 30, 0, 8, &r1));
	EQ(r1.count, 8); EQ(r1.team_count, 0);
	for (i = 0; i < 8; i++)
		EQ(r1.bot[i].team, i);
	CHECK(!ae_rating_roster_make(&w, &a, 30, 4, 5, &r1));
	CHECK(!ae_rating_roster_make(&w, &a, 30, 0, 17, &r1));
	CHECK(!ae_rating_roster_make(&w, &a, 30, 2, 0, &r1));
	/* a 1v1 */
	CHECK(ae_rating_roster_make(&w, &a, 5, 2, 1, &r1));
	EQ(r1.count, 2);
	CHECK(r1.bot[0].team != r1.bot[1].team);
	(void)ok;
}

int main(void)
{
	test_levels();
	test_pairs();
	test_games();
	test_quit_protection();
	test_window();
	test_score();
	test_delay();
	test_balance();
	test_bots();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
