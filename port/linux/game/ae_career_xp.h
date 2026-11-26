/*
AE_CAREER_XP.H

A finished game's or a campaign level's career XP and challenge progress, put on a profile's lines of the career record
(ae_career_record.h). Pure code (no engine headers): port/linux/tests/ae_career_xp_test.c runs it; ae_glue_career_xp.c calls it at
a game's end.

The profile's challenges are made for the day and the week first (ae_career_challenges_refresh: a period that has changed is picked
again from its date, its counters empty; a line whose kinds are not the picks of its period, an edited file or a changed table, is
picked again too). A challenge that reaches its target pays its XP once, in the same call that counted it.
*/

#ifndef __AE_CAREER_XP_H
#define __AE_CAREER_XP_H

#include "ae_career_challenge_rules.h"
#include "ae_career_record.h"

/* a challenge that paid in this call */
struct ae_xp_paid
{
	char id[AE_CH_ID];
	long xp;
};

struct ae_xp_result
{
	long xp_before, xp_after;
	/* the career steps (ae_career_xp_rules.h) before and after */
	int step_before, step_after;
	/* the game's own XP (campaign: all in .total), the challenges' XP, and the challenges that paid */
	struct ae_xp_award award;
	long challenge_xp;
	int paid_count;
	struct ae_xp_paid paid[AE_CH_SLOTS];
};

/* the profile's challenges line made for the day and the week: 1 with the line in *out (may be NULL), 0 when the table is full */
int ae_career_challenges_refresh(struct ae_career *career, char const *profile, long day, struct ae_career_challenges **out);

/* a finished multiplayer game of the profile (the facts of ae_xp_game for XP, the counters' game for the challenges): the game's
   XP and every challenge it moved, added to the lines. 1 when anything was added, 0 when nothing was (a quit, a game too short) or
   a table is full; result (may be NULL) is always filled */
int ae_career_xp_game(struct ae_career *career, char const *profile, struct ae_xp_game const *game, struct ae_ch_game const *counts,
	long day, unsigned long clock, struct ae_xp_result *result);
/* a campaign level won on a difficulty 0 .. 3 */
int ae_career_xp_campaign(struct ae_career *career, char const *profile, int difficulty, long day, unsigned long clock,
	struct ae_xp_result *result);

#endif
