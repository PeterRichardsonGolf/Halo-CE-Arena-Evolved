/*
AE_CAREER_XP_RULES.H

The CAREER XP ladder: a progression apart from skill and from the social EXP, that is never lost. Pure code (no engine
headers): port/linux/tests/ae_career_xp_test.c runs it.

The career is 11 tours of 30 ranks (Rookie up to General), 330 steps in all, shown as the rank's name and its tour. The XP
goes on counting past the last step up to AE_XP_MAX and the step stays the last. A step costs 400 + 60 r XP, r being the rank's
place in its tour counted from 0, so the first ranks come fast: General of tour 1 is reached at 35,960 XP (about 180 games of 200
XP), a new tour starts every 38,100 XP and General of tour 11 at 416,960 XP.

XP comes from finished games of any kind (a finish, a win or tie, kills, medals and objectives: AE TOURNAMENT, bot games, custom
games, LAN and online alike) and from campaign levels by difficulty. A game left before its end, or one shorter than a minute,
gives none.
*/

#ifndef __AE_CAREER_XP_RULES_H
#define __AE_CAREER_XP_RULES_H

enum
{
	AE_XP_TOURS = 11,
	AE_XP_RANKS = 30,
	AE_XP_STEPS = AE_XP_TOURS * AE_XP_RANKS,
	AE_XP_MAX = 9999999,
	/* a game under this many seconds (when its length is known) gives no XP */
	AE_XP_MIN_SECONDS = 60,
	/* a game's XP */
	AE_XP_FINISH = 50,
	AE_XP_WIN = 50,
	AE_XP_TIE = 25,
	AE_XP_PER_KILL = 2,
	AE_XP_PER_MEDAL = 4,
	AE_XP_PER_OBJECTIVE = 10,
	/* what each of kills, medals and objectives can add to one game */
	AE_XP_PART_CAP = 100,
	/* the campaign's difficulties (the game's order) */
	AE_XP_DIFFICULTIES = 4,
	AE_XP_NAME = 32
};

/* the facts of one game for one local player */
struct ae_xp_game
{
	int won, tied, quit;
	long kills, medals, objectives;
	/* the game's length in seconds, 0 or less when it is not known */
	long seconds;
};

/* what a game paid, by part */
struct ae_xp_award
{
	long finish, win, kills, medals, objectives, total;
};

/* the XP a step (0 .. AE_XP_STEPS - 1) starts at (step 0 starts at 0); 0 out of range */
long ae_xp_step_start(int step);
/* the step (0 .. AE_XP_STEPS - 1) of this XP */
int ae_xp_step(long xp);
/* a step's tour (1 .. AE_XP_TOURS, counted up from the first) and its rank in the tour (1 .. AE_XP_RANKS) */
int ae_xp_tour(int step);
int ae_xp_rank(int step);
/* the rank's name for a rank 1 .. 30 (Rookie ... General), "" out of range */
char const *ae_xp_rank_name(int rank);
/* the XP still to the next step; 0 on the last */
long ae_xp_to_next(long xp);
/* how far the XP is through its step, 0 .. 100 (100 on the last) */
int ae_xp_percent(long xp);
/* "SERGEANT, TOUR 3" for a step; written whole, cut short when the place is small */
void ae_xp_title(int step, char *out, int size);

/* a game's XP: award is filled (NULL allowed), the total returned; 0 for a quit or a game too short */
long ae_xp_game_award(struct ae_xp_game const *game, struct ae_xp_award *award);
/* a campaign level's XP by difficulty 0 .. 3 (easy .. legendary); 0 out of range */
long ae_xp_campaign_award(int difficulty);
/* the XP after adding delta, held to 0 .. AE_XP_MAX */
long ae_xp_add(long xp, long delta);

#endif
