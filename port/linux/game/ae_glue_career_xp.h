/*
AE_GLUE_CAREER_XP.H

The career XP's and the daily / weekly challenges' contact with the game (ae_career_xp.h keeps the lines in career.txt through
ae_glue_career.h). A finished game's XP is added once, from the report the service record takes (ae_glue_service_record.c), for
each profile of this PC that played in it; a campaign level's XP when the level is won. Menus' thread only.

Log: "career xp: +N (why), total T, <rank>, tour N" for each game or level that paid; "career xp: rank up to <rank>, tour N"; "career challenge:
<id> done, +N xp"; "career xp: <profile>: the career file could not be written: not counted" when the write fails.
Dev event: career_xp (profile, gained, total, step, challenges paid).
*/

#ifndef __AE_GLUE_CAREER_XP_H
#define __AE_GLUE_CAREER_XP_H

#include "ae_career_xp.h"
#include "ae_service_record.h"

/* the local day (days since 1970-01-01 by the PC's date) */
long ae_career_xp_today(void);
/* a finished game's XP and challenge progress for every profile of this PC in it; how many profiles it paid */
int ae_career_xp_game_ended(struct ae_sr_game const *game);
/* a campaign level won on this difficulty (0 .. 3): the XP and progress of each profile on this PC that plays; how many it paid */
int ae_career_xp_campaign_won(int difficulty);
/* the profile's career XP, 0 for none */
long ae_career_xp_profile_xp(char const *profile);

/* what the screens show of a local player's career: the rank (the step's name, its tour), the XP and the way to the next rank, and
   today's and this week's six challenges (made for today first) */
struct ae_career_view
{
	long xp, to_next;
	int step, rank, tour, percent;
	char name[40], next[40];
	int daily_done, weekly_done;
	/* 1 for a guest: today's challenges with no counters, no rank, no XP */
	int guest;
	struct
	{
		char label[48];
		long progress, target, xp;
		int done, metric;
	} slot[AE_CH_SLOTS];
};
/* 1 with the view of local player 0 .. 3 (a profile's, or DEV MODE's made-up one); a guest's view has guest set and holds the day's
   challenges only (what the screens show a guest); 0 only when there is nothing to show */
int ae_career_xp_view(short local, struct ae_career_view *view);

/* the last game that paid a local player: its XP by part, the rank before and after, the challenge that paid. Cleared when the next
   game starts */
struct ae_career_last
{
	int valid;
	long gained, challenge_xp;
	int step_before, step_after, paid_count;
	struct ae_xp_award award;
	char challenge[48];
};
int ae_career_xp_last(short local, struct ae_career_last *last);
void ae_career_xp_last_forget(void);

/* the pictures of MCC's career insignia and skill ranks: 0 auto (shown when the install has them), 1 off (AE's own look) */
void ae_career_art_set(int mode);
int ae_career_art_on(void);

/* DEV MODE `social career ...`: xp <n> | rank <1-30> <1-11> | challenge done <1-6> | last <xp> [rankup] [challenge] | art auto|off */
struct ae_dev_cmd;
int ae_career_xp_dev(struct ae_dev_cmd const *cmd, short player, char *text, int size);

#endif
