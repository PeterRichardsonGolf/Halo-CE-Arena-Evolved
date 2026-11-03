/* The score strip's model (port/linux/game/ae_strip_model.c): what the strip says in every mode, plan Task 2's
rules and Review Focus 1, 3 and 4 */
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ae_strip_model.h"

static int failures;
static int checks;
#define AE_CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } \
	while (0)
#define AE_CHECK_WSTR(a, b) do { checks++; if (wcscmp((a), (b)) != 0) { printf("FAIL %s:%d %ls != %ls\n", __FILE__, \
	__LINE__, (a), (b)); failures++; } } while (0)

/* whether the characters [from, from + count) of a text all have this colour */
static int colored(const struct ae_strip_text *t, long from, long count, enum ae_strip_color color)
{
	long i;

	if (from + count > (long)wcslen(t->text))
		return 0;
	for (i = from; i < from + count; i++)
		if (t->colors[i] != color)
			return 0;
	return 1;
}

static void base(struct ae_strip_snapshot *s, enum ae_strip_mode mode, int teams)
{
	memset(s, 0, sizeof(*s));
	s->mode = mode;
	s->teams = (boolean)teams;
	s->clock_seconds = 102;
	s->me.team_index = teams ? 1 : -1;
	s->other.team_index = teams ? 0 : -1;
	s->hill_move_seconds = -1;
	s->next_flag_meters = -1;
	s->respawns_left = -1;
	wcscpy(s->me.name, L"me");
	wcscpy(s->other.name, L"bot3");
}

static void test_team_slayer_basic(void)
{
	struct ae_strip_snapshot s = {0}; struct ae_strip_model m;
	s.mode = _ae_strip_slayer; s.teams = TRUE; s.score_to_win = 50; s.clock_seconds = 102;
	s.me.team_index = 1; s.me.score = 17; s.other.team_index = 0; s.other.score = 10; s.i_lead = TRUE;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_score.text, L"17"); AE_CHECK(m.left_score.color == _ae_strip_color_team_blue);
	AE_CHECK_WSTR(m.right_score.text, L"10"); AE_CHECK(m.right_score.color == _ae_strip_color_team_red);
	AE_CHECK_WSTR(m.clock.text, L"1:42"); AE_CHECK_WSTR(m.lead.text, L"+7");
	AE_CHECK(m.lead.color == _ae_strip_color_team_blue);
	AE_CHECK_WSTR(m.goal.text, L"TO 50"); AE_CHECK(m.bars);
	AE_CHECK(m.left_fill > 0.339f && m.left_fill < 0.341f);
	AE_CHECK(m.right_fill > 0.199f && m.right_fill < 0.201f);
	AE_CHECK(m.marker_left);
	/* (the clock neutral, normal size; no subs, no context in team slayer) */
	AE_CHECK(!m.clock_small && m.clock.color == _ae_strip_color_clock);
	AE_CHECK(m.goal.color == _ae_strip_color_grey);
	AE_CHECK_WSTR(m.context.text, L""); AE_CHECK_WSTR(m.left_sub.text, L""); AE_CHECK_WSTR(m.right_sub.text, L"");
	AE_CHECK(!m.round_cells && m.pips_total == 0 && !m.flag_glyphs);
}

static void test_team_colours_follow_the_team_index(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_slayer, 1);
	s.score_to_win = 50; s.me.team_index = 0; s.other.team_index = 1; s.me.score = 4; s.other.score = 9;
	ae_strip_model_build(&s, &m);
	AE_CHECK(m.left_score.color == _ae_strip_color_team_red && m.right_score.color == _ae_strip_color_team_blue);
	/* (the enemy leads: "+5" in the enemy's colour) */
	AE_CHECK_WSTR(m.lead.text, L"+5"); AE_CHECK(m.lead.color == _ae_strip_color_team_blue);
	s.other.score = 4;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.lead.text, L"TIED"); AE_CHECK(m.lead.color == _ae_strip_color_grey);
}

static void test_empty_enemy_team(void)     /* Review Focus 1 */
{
	struct ae_strip_snapshot s = {0}; struct ae_strip_model m;
	s.mode = _ae_strip_slayer; s.teams = TRUE; s.score_to_win = 50;
	s.me.team_index = 0; s.me.score = 3; s.other.team_index = 1; s.other.score = 0; s.other.players = 0;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.right_score.text, L"0"); AE_CHECK(m.right_fill == 0.0f);
	AE_CHECK(m.bars && m.left_fill > 0.059f && m.left_fill < 0.061f);
}

static void test_no_limits(void)            /* Review Focus 3 */
{
	struct ae_strip_snapshot s = {0}; struct ae_strip_model m;
	s.mode = _ae_strip_slayer; s.teams = FALSE; s.score_to_win = 0; s.clock_counts_up = TRUE; s.clock_seconds = 75;
	ae_strip_model_build(&s, &m);
	AE_CHECK(!m.bars); AE_CHECK_WSTR(m.goal.text, L""); AE_CHECK_WSTR(m.clock.text, L"1:15");
	AE_CHECK(m.left_fill == 0.0f && m.right_fill == 0.0f);
}

static void test_long_clock_and_score(void)  /* Review Focus 4 */
{
	struct ae_strip_snapshot s = {0}; struct ae_strip_model m;
	s.mode = _ae_strip_slayer; s.teams = TRUE; s.score_to_win = 150; s.clock_seconds = 725;
	s.me.team_index = 1; s.me.score = 104; s.other.team_index = 0; s.other.score = 99;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.clock.text, L"12:05"); AE_CHECK_WSTR(m.left_score.text, L"104");
	/* (past the limit: the bar stays full) */
	s.me.score = 160;
	ae_strip_model_build(&s, &m);
	AE_CHECK(m.left_fill == 1.0f);
	/* (a negative score, suicides in FFA: shown, its bar empty) */
	s.me.score = -2;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_score.text, L"-2"); AE_CHECK(m.left_fill == 0.0f);
}

static void test_clock_hidden_and_negative(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_slayer, 1);
	s.clock_hidden = TRUE;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.clock.text, L"");
	s.clock_hidden = FALSE; s.clock_seconds = -4;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.clock.text, L"0:00");
}

/* MATCH CLOCK BOTH: the time played over the clock, grey, the value alone; nothing without it or the clock */
static void test_time_played(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_slayer, 1);
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.played.text, L"");
	s.played_shown = TRUE; s.played_seconds = 192;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.played.text, L"3:12"); AE_CHECK(colored(&m.played, 0, 4, _ae_strip_color_grey));
	AE_CHECK_WSTR(m.clock.text, L"1:42");
	s.played_seconds = 754;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.played.text, L"12:34");
	/* (timed modes too, under their small clock's rules; never on NHE's maps, where the clock is hidden) */
	base(&s, _ae_strip_oddball, 1);
	s.played_shown = TRUE; s.played_seconds = 31;
	ae_strip_model_build(&s, &m);
	AE_CHECK(m.clock_small); AE_CHECK_WSTR(m.played.text, L"0:31");
	s.clock_hidden = TRUE;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.played.text, L"");
}

static void test_rounds_text_over_four(void)
{
	struct ae_strip_snapshot s = {0}; struct ae_strip_model m;
	s.mode = _ae_strip_rounds; s.teams = TRUE; s.rounds_to_win = 5;
	s.me.team_index = 1; s.me.rounds_won = 2; s.me.players = 8; s.me.alive = 6;
	s.other.team_index = 0; s.other.rounds_won = 1; s.other.players = 4; s.other.alive = 3;
	ae_strip_model_build(&s, &m);
	AE_CHECK(!m.round_cells); AE_CHECK_WSTR(m.left_alive.text, L"6 ALIVE"); AE_CHECK_WSTR(m.right_alive.text, L"3 ALIVE");
	AE_CHECK(m.pips_total == 5 && m.left_pips_lit == 2 && m.right_pips_lit == 1);
	AE_CHECK(m.left_alive.color == _ae_strip_color_white);
}

static void test_rounds_cells_and_respawns(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_rounds, 1);
	s.rounds_to_win = 4; s.respawns_left = 15; s.me.rounds_won = 2; s.other.rounds_won = 1;
	s.me.players = 4; s.me.alive = 3; s.other.players = 4; s.other.alive = 2;
	ae_strip_model_build(&s, &m);
	AE_CHECK(m.round_cells && m.left_cells == 4 && m.left_lit == 3 && m.right_cells == 4 && m.right_lit == 2);
	AE_CHECK_WSTR(m.left_alive.text, L""); AE_CHECK_WSTR(m.right_alive.text, L"");
	AE_CHECK_WSTR(m.left_score.text, L"2"); AE_CHECK_WSTR(m.right_score.text, L"1");
	AE_CHECK(m.pips_total == 4 && m.left_pips_lit == 2 && m.right_pips_lit == 1);
	AE_CHECK_WSTR(m.context.text, L"RESPAWNS 15"); AE_CHECK_WSTR(m.lead.text, L"");
	AE_CHECK(colored(&m.context, 0, 9, _ae_strip_color_grey) && colored(&m.context, 9, 2, _ae_strip_color_white));
	/* (rounds: cells instead of bars, no goal line) */
	AE_CHECK(!m.bars); AE_CHECK_WSTR(m.goal.text, L"");
	/* (no respawn count: the lead in rounds) */
	s.respawns_left = -1;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L""); AE_CHECK_WSTR(m.lead.text, L"+1");
	/* (alive never above the side's players, never negative) */
	s.me.alive = 6; s.other.alive = -1;
	ae_strip_model_build(&s, &m);
	AE_CHECK(m.left_lit == 4 && m.right_lit == 0);
}

/* (0.3.9) rounds on Bomb: the viewer's job over the clock in its team's colour, the planted bomb in the planters' */
static void test_rounds_bomb(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_rounds, 1);
	s.rounds_to_win = 6; s.me.players = 2; s.me.alive = 2; s.other.players = 2; s.other.alive = 1;
	s.bomb_role = _ae_bomb_role_attack;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"ATTACK"); AE_CHECK(colored(&m.context, 0, 6, _ae_strip_color_team_blue));
	AE_CHECK_WSTR(m.lead.text, L""); AE_CHECK(m.round_cells && m.right_lit == 1 && m.pips_total == 6);
	/* (SEARCH AND DESTROY: the viewer carries the bomb, in its team's colour) */
	s.bomb_carrying = TRUE;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"YOU HAVE THE BOMB");
	AE_CHECK(colored(&m.context, 0, 17, _ae_strip_color_team_blue));
	s.bomb_carrying = FALSE;
	s.bomb_role = _ae_bomb_role_defend;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"DEFEND"); AE_CHECK(colored(&m.context, 0, 6, _ae_strip_color_team_blue));
	/* (planted: by the other side, in its colour; the clock is the fuse, as the reader puts it) */
	s.bomb_planted = TRUE; s.bomb_planted_side = _ae_strip_side_other; s.clock_seconds = 41;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"BOMB PLANTED");
	AE_CHECK(colored(&m.context, 0, 5, _ae_strip_color_grey) && colored(&m.context, 5, 7, _ae_strip_color_team_red));
	AE_CHECK_WSTR(m.clock.text, L"0:41"); AE_CHECK(!m.clock_small);
	s.bomb_planted_side = _ae_strip_side_me;
	ae_strip_model_build(&s, &m);
	AE_CHECK(colored(&m.context, 5, 7, _ae_strip_color_team_blue));
	/* (the attackers' warning: a defender on it, in the defenders' colour) */
	s.bomb_defusing = TRUE;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"BOMB DEFUSING"); AE_CHECK(colored(&m.context, 5, 8, _ae_strip_color_team_red));
	s.bomb_defusing = FALSE;
	/* (Grifball, no job: the lead as in any round mode) */
	s.bomb_planted = FALSE; s.bomb_role = _ae_bomb_role_none; s.me.rounds_won = 1;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L""); AE_CHECK_WSTR(m.lead.text, L"+1");
}

static void test_ffa_me_behind_and_leading(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_slayer, 0);
	s.score_to_win = 25; s.me.score = 9; s.other.score = 14; s.me.place = 3;
	ae_strip_model_build(&s, &m);
	AE_CHECK(m.left_score.color == _ae_strip_color_me && m.right_score.color == _ae_strip_color_white);
	AE_CHECK_WSTR(m.left_sub.text, L"YOU 3RD"); AE_CHECK(colored(&m.left_sub, 0, 7, _ae_strip_color_me));
	AE_CHECK_WSTR(m.right_sub.text, L"LEADER bot3"); AE_CHECK(colored(&m.right_sub, 0, 11, _ae_strip_color_white));
	AE_CHECK_WSTR(m.lead.text, L"+5"); AE_CHECK(m.lead.color == _ae_strip_color_white);
	AE_CHECK_WSTR(m.goal.text, L"TO 25");
	/* (me leading: right is 2ND, the lead in my colour) */
	s.me.score = 17; s.other.score = 10; s.me.place = 1; s.i_lead = TRUE;
	wcscpy(s.other.name, L"averylongname15");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_sub.text, L"YOU 1ST"); AE_CHECK_WSTR(m.right_sub.text, L"2ND averylongname15");
	AE_CHECK_WSTR(m.lead.text, L"+7"); AE_CHECK(m.lead.color == _ae_strip_color_me);
	/* (a place the engine does not have yet reads as 1ST) */
	s.me.place = 0;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_sub.text, L"YOU 1ST");
}

static void test_format_helpers(void)
{
	wchar_t text[16];
	long places[] = { 1, 2, 3, 4, 10, 11, 12, 13, 14, 21, 22, 23, 101, 111, 112 };
	const wchar_t *wanted[] = { L"1ST", L"2ND", L"3RD", L"4TH", L"10TH", L"11TH", L"12TH", L"13TH", L"14TH", L"21ST",
		L"22ND", L"23RD", L"101ST", L"111TH", L"112TH" };
	unsigned i;

	for (i = 0; i < sizeof(places) / sizeof(places[0]); i++)
	{
		ae_strip_format_place(places[i], text, 16);
		AE_CHECK_WSTR(text, wanted[i]);
	}
	ae_strip_format_time(0, text, 16); AE_CHECK_WSTR(text, L"0:00");
	ae_strip_format_time(7, text, 16); AE_CHECK_WSTR(text, L"0:07");
	ae_strip_format_time(102, text, 16); AE_CHECK_WSTR(text, L"1:42");
	ae_strip_format_time(599, text, 16); AE_CHECK_WSTR(text, L"9:59");
	ae_strip_format_time(600, text, 16); AE_CHECK_WSTR(text, L"10:00");
	ae_strip_format_time(725, text, 16); AE_CHECK_WSTR(text, L"12:05");
	ae_strip_format_time(-3, text, 16); AE_CHECK_WSTR(text, L"0:00");
	/* (a short buffer: cut, always terminated) */
	ae_strip_format_time(725, text, 3); AE_CHECK_WSTR(text, L"12");
	ae_strip_format_place(11, text, 2); AE_CHECK_WSTR(text, L"1");
}

static void test_ctf_states(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_ctf, 1);
	s.score_to_win = 3; s.me.score = 2; s.other.score = 1;
	s.me.flag = _ae_flag_home; s.other.flag = _ae_flag_carried; wcscpy(s.other.flag_carrier, L"bot5");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.goal.text, L"TO 3"); AE_CHECK_WSTR(m.lead.text, L"+1");
	AE_CHECK(m.flag_glyphs);
	AE_CHECK_WSTR(m.left_sub.text, L"HOME"); AE_CHECK(m.left_sub.color == _ae_strip_color_grey);
	/* (their flag carried by one of mine: the carrier's name in my team's colour) */
	AE_CHECK_WSTR(m.right_sub.text, L"bot5"); AE_CHECK(m.right_sub.color == _ae_strip_color_team_blue);
	AE_CHECK(colored(&m.right_sub, 0, 4, _ae_strip_color_team_blue));
	s.me.flag = _ae_flag_dropped; s.me.flag_return_seconds = 18;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_sub.text, L"DROPPED 0:18"); AE_CHECK(m.left_sub.color == _ae_strip_color_grey);
	/* (the label grey, the value white) */
	AE_CHECK(colored(&m.left_sub, 0, 8, _ae_strip_color_grey) && colored(&m.left_sub, 8, 4, _ae_strip_color_white));
	/* (my flag carried by an enemy: the name in their colour) */
	s.me.flag = _ae_flag_carried; wcscpy(s.me.flag_carrier, L"bot9");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_sub.text, L"bot9"); AE_CHECK(m.left_sub.color == _ae_strip_color_team_red);
	/* (a team with no flag in play, single-flag CTF: no line) */
	s.other.flag = _ae_flag_none;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.right_sub.text, L"");
	/* (a 15-character carrier fits) */
	wcscpy(s.me.flag_carrier, L"averylongname15");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_sub.text, L"averylongname15");
}

static void test_oddball_team_and_ffa(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_oddball, 1);
	s.score_to_win = 300; s.me.score = 72; s.other.score = 48; s.own_score = 31;
	wcscpy(s.context_name, L"bot2"); s.context_side = _ae_strip_side_me;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_score.text, L"1:12"); AE_CHECK_WSTR(m.right_score.text, L"0:48");
	AE_CHECK_WSTR(m.context.text, L"BALL bot2");
	AE_CHECK(colored(&m.context, 0, 5, _ae_strip_color_grey) && colored(&m.context, 5, 4, _ae_strip_color_team_blue));
	AE_CHECK_WSTR(m.left_sub.text, L"YOU 0:31"); AE_CHECK_WSTR(m.right_sub.text, L"");
	AE_CHECK(colored(&m.left_sub, 0, 4, _ae_strip_color_grey) && colored(&m.left_sub, 4, 4, _ae_strip_color_white));
	AE_CHECK_WSTR(m.goal.text, L"TO 5:00");
	AE_CHECK(m.clock_small && m.clock.color == _ae_strip_color_grey && m.bars); AE_CHECK_WSTR(m.lead.text, L"");
	AE_CHECK(m.left_fill > 0.239f && m.left_fill < 0.241f);
	/* (an enemy holds it: their colour; nobody: no context line) */
	s.context_side = _ae_strip_side_other;
	ae_strip_model_build(&s, &m);
	AE_CHECK(colored(&m.context, 5, 4, _ae_strip_color_team_red));
	s.context_name[0] = 0;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L""); AE_CHECK_WSTR(m.lead.text, L"");
	/* (FFA: YOU / LEADER, the holder white) */
	base(&s, _ae_strip_oddball, 0);
	s.score_to_win = 300; s.me.score = 44; s.other.score = 80; s.me.place = 2;
	wcscpy(s.context_name, L"bot3"); s.context_side = _ae_strip_side_other;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_sub.text, L"YOU 2ND"); AE_CHECK_WSTR(m.right_sub.text, L"LEADER bot3");
	AE_CHECK_WSTR(m.context.text, L"BALL bot3"); AE_CHECK(colored(&m.context, 5, 4, _ae_strip_color_white));
	AE_CHECK_WSTR(m.left_score.text, L"0:44");
}

static void test_king_states_and_move(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;
	static const wchar_t *wanted[] = { L"HILL EMPTY", L"HILL YOURS", L"HILL CONTESTED", L"HILL THEIRS" };
	static const enum ae_strip_color state_colors[] = { _ae_strip_color_white, _ae_strip_color_team_blue,
		_ae_strip_color_white, _ae_strip_color_team_red };
	int state;

	for (state = _ae_hill_empty; state <= _ae_hill_theirs; state++)
	{
		base(&s, _ae_strip_king, 1);
		s.score_to_win = 300; s.hill = (enum ae_strip_hill_state)state;
		ae_strip_model_build(&s, &m);
		AE_CHECK_WSTR(m.context.text, wanted[state]);
		AE_CHECK(colored(&m.context, 0, 5, _ae_strip_color_grey));
		AE_CHECK(colored(&m.context, 5, (long)wcslen(wanted[state]) - 5, state_colors[state]));
		AE_CHECK(m.clock_small && m.bars); AE_CHECK_WSTR(m.goal.text, L"TO 5:00");
	}
	s.hill = _ae_hill_contested; s.hill_move_seconds = 23;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"HILL CONTESTED  MOVES IN 0:23");
	AE_CHECK(colored(&m.context, 14, 11, _ae_strip_color_grey) && colored(&m.context, 25, 4, _ae_strip_color_white));
	/* (FFA: YOURS in my colour) */
	base(&s, _ae_strip_king, 0);
	s.score_to_win = 300; s.hill = _ae_hill_yours; s.me.place = 3;
	ae_strip_model_build(&s, &m);
	AE_CHECK(colored(&m.context, 5, 5, _ae_strip_color_me)); AE_CHECK_WSTR(m.left_sub.text, L"YOU 3RD");
}

static void test_race_and_rally(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_race, 0);
	s.me.place = 2; s.me.score = 2; s.other.score = 3; s.laps = 3; s.checkpoint = 7; s.checkpoints = 9;
	wcscpy(s.other.name, L"bot1"); s.clock_seconds = 167; s.clock_counts_up = TRUE;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_score.text, L"2ND"); AE_CHECK(m.left_score.color == _ae_strip_color_me);
	AE_CHECK_WSTR(m.right_score.text, L"bot1"); AE_CHECK(m.right_score.color == _ae_strip_color_white);
	AE_CHECK_WSTR(m.left_sub.text, L"LAP 2 OF 3"); AE_CHECK_WSTR(m.right_sub.text, L"LEADER LAP 3");
	AE_CHECK(colored(&m.left_sub, 0, 4, _ae_strip_color_grey) && colored(&m.left_sub, 4, 1, _ae_strip_color_white) &&
		colored(&m.left_sub, 5, 5, _ae_strip_color_grey));
	AE_CHECK(colored(&m.right_sub, 0, 11, _ae_strip_color_grey) && colored(&m.right_sub, 11, 1, _ae_strip_color_white));
	AE_CHECK_WSTR(m.context.text, L"CHECKPOINT 7 OF 9");
	AE_CHECK(colored(&m.context, 11, 1, _ae_strip_color_white));
	AE_CHECK(!m.bars && m.clock_small); AE_CHECK_WSTR(m.goal.text, L""); AE_CHECK_WSTR(m.lead.text, L"");
	AE_CHECK_WSTR(m.clock.text, L"2:47");
	/* (I lead: the second's name and lap) */
	s.i_lead = TRUE; s.me.place = 1; s.me.score = 3; s.other.score = 2;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_score.text, L"1ST"); AE_CHECK_WSTR(m.right_sub.text, L"2ND LAP 2");
	/* (a team race: my place in my team's colour) */
	s.teams = TRUE; s.me.team_index = 0;
	ae_strip_model_build(&s, &m);
	AE_CHECK(m.left_score.color == _ae_strip_color_team_red);
	/* (a race to an unknown number of laps or checkpoints: no "OF") */
	base(&s, _ae_strip_race, 0);
	s.me.place = 1; s.me.score = 1; s.checkpoint = 2; wcscpy(s.other.name, L"bot1");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_sub.text, L"LAP 1"); AE_CHECK_WSTR(m.context.text, L"CHECKPOINT 2");

	base(&s, _ae_strip_rally, 0);
	s.me.place = 2; s.me.score = 6; s.other.score = 9; s.score_to_win = 15; s.next_flag_meters = 34;
	wcscpy(s.other.name, L"bot1");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_score.text, L"2ND"); AE_CHECK_WSTR(m.right_score.text, L"bot1");
	AE_CHECK_WSTR(m.left_sub.text, L"POINTS 6 OF 15"); AE_CHECK_WSTR(m.right_sub.text, L"LEADER 9 POINTS");
	AE_CHECK(colored(&m.right_sub, 7, 1, _ae_strip_color_white) && colored(&m.right_sub, 8, 7, _ae_strip_color_grey));
	AE_CHECK_WSTR(m.context.text, L"NEXT FLAG 34 M"); AE_CHECK(colored(&m.context, 10, 4, _ae_strip_color_white));
	AE_CHECK(!m.bars && m.clock_small); AE_CHECK_WSTR(m.goal.text, L"");
	s.next_flag_meters = -1;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"NEXT FLAG");
	s.i_lead = TRUE;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.right_sub.text, L"2ND 9 POINTS");
}

static void test_juggernaut(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_juggernaut, 0);
	s.score_to_win = 10; s.me.score = 4; s.other.score = 6; s.me.place = 2;
	wcscpy(s.context_name, L"bot3"); s.context_side = _ae_strip_side_other;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"JUGGERNAUT bot3");
	AE_CHECK(colored(&m.context, 0, 11, _ae_strip_color_grey) && colored(&m.context, 11, 4, _ae_strip_color_white));
	AE_CHECK_WSTR(m.lead.text, L""); AE_CHECK_WSTR(m.goal.text, L"TO 10"); AE_CHECK(m.bars);
	AE_CHECK_WSTR(m.left_sub.text, L"YOU 2ND"); AE_CHECK_WSTR(m.right_sub.text, L"LEADER bot3");
	s.context_side = _ae_strip_side_me; wcscpy(s.context_name, L"me");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"YOU ARE THE JUGGERNAUT"); AE_CHECK(m.context.color == _ae_strip_color_me);
	AE_CHECK(colored(&m.context, 0, 22, _ae_strip_color_me));
	/* (no juggernaut yet: the lead instead) */
	s.context_side = _ae_strip_side_none; s.context_name[0] = 0;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L""); AE_CHECK_WSTR(m.lead.text, L"+2");
}

static void test_accumulation_and_reverse_tag(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_accumulation, 0);
	s.score_to_win = 300; s.me.score = 65; s.other.score = 52; s.me.place = 1; s.i_lead = TRUE;
	s.balls_held = 3; s.balls_total = 16;
	wcscpy(s.other.name, L"bot6");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"YOU HOLD 3 OF 16");
	AE_CHECK(colored(&m.context, 0, 9, _ae_strip_color_grey) && colored(&m.context, 9, 1, _ae_strip_color_me) &&
		colored(&m.context, 10, 6, _ae_strip_color_grey));
	AE_CHECK_WSTR(m.left_score.text, L"1:05"); AE_CHECK(m.clock_small); AE_CHECK_WSTR(m.goal.text, L"TO 5:00");
	AE_CHECK_WSTR(m.right_sub.text, L"2ND bot6");
	s.reverse_tag = TRUE; wcscpy(s.context_name, L"bot6"); s.context_side = _ae_strip_side_other;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"IT bot6"); AE_CHECK(colored(&m.context, 3, 4, _ae_strip_color_white));
	s.context_side = _ae_strip_side_me; wcscpy(s.context_name, L"me");
	ae_strip_model_build(&s, &m);
	AE_CHECK(colored(&m.context, 3, 2, _ae_strip_color_me));
}

static void test_elimination(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_elimination, 0);
	s.score_to_win = 25; s.me.score = 3; s.other.score = 5;
	s.lives_left = 1; s.players_left = 5; s.players_total = 8;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"LAST LIFE  5 OF 8 LEFT");
	AE_CHECK(colored(&m.context, 0, 9, _ae_strip_color_white) && colored(&m.context, 11, 1, _ae_strip_color_white) &&
		colored(&m.context, 12, 10, _ae_strip_color_grey));
	s.lives_left = 3;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"LIVES 3  5 OF 8 LEFT");
	AE_CHECK(colored(&m.context, 0, 6, _ae_strip_color_grey) && colored(&m.context, 6, 1, _ae_strip_color_white));
	/* (endurance without a lives count, or without a player count) */
	s.lives_left = 0;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"5 OF 8 LEFT");
	s.lives_left = 2; s.players_total = 0;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"LIVES 2");
}

static void test_phantoms(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	base(&s, _ae_strip_phantoms, 0);
	s.score_to_win = 10; s.me.score = 4; s.other.score = 3; s.i_lead = TRUE; s.me.place = 1;
	wcscpy(s.context_name, L"bot7"); s.context_side = _ae_strip_side_other; wcscpy(s.other.name, L"bot4");
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"TARGET bot7");
	AE_CHECK(colored(&m.context, 0, 7, _ae_strip_color_grey) && colored(&m.context, 7, 4, _ae_strip_color_white));
	AE_CHECK_WSTR(m.right_sub.text, L"2ND bot4"); AE_CHECK_WSTR(m.goal.text, L"TO 10");
}

static void test_out_of_range_inputs(void)
{
	struct ae_strip_snapshot s; struct ae_strip_model m;

	/* (a hill state or mode out of range: no crash, a plain line) */
	base(&s, _ae_strip_king, 1);
	s.hill = (enum ae_strip_hill_state)9;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.context.text, L"HILL EMPTY");
	base(&s, (enum ae_strip_mode)42, 1);
	s.me.score = 1;
	ae_strip_model_build(&s, &m);
	AE_CHECK_WSTR(m.left_score.text, L"1");
	/* (names without a terminator within their 16 characters are cut) */
	base(&s, _ae_strip_ctf, 1);
	s.me.flag = _ae_flag_carried;
	wmemset(s.me.flag_carrier, L'x', AE_STRIP_NAME_LENGTH);
	ae_strip_model_build(&s, &m);
	AE_CHECK(wcslen(m.left_sub.text) == AE_STRIP_NAME_LENGTH - 1);
	ae_strip_model_build(NULL, &m);
	ae_strip_model_build(&s, NULL);
}

int main(void)
{
	test_team_slayer_basic();
	test_team_colours_follow_the_team_index();
	test_empty_enemy_team();
	test_no_limits();
	test_long_clock_and_score();
	test_clock_hidden_and_negative();
	test_time_played();
	test_rounds_text_over_four();
	test_rounds_cells_and_respawns();
	test_rounds_bomb();
	test_ffa_me_behind_and_leading();
	test_format_helpers();
	test_ctf_states();
	test_oddball_team_and_ffa();
	test_king_states_and_move();
	test_race_and_rally();
	test_juggernaut();
	test_accumulation_and_reverse_tag();
	test_elimination();
	test_phantoms();
	test_out_of_range_inputs();
	printf("%d checks, %d failures\n", checks, failures);
	return failures != 0;
}
