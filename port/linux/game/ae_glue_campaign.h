/*
AE_GLUE_CAMPAIGN.H

AE's CAMPAIGN glue (ae_glue_campaign.c; the 0.3.7 menus): player 1's campaign as the PC menus read it
(menu_functions.c's campaign_profile and campaign_levels_read): the levels' names, which are reached (played, the one
after the last finished, the first) and the difficulties each is finished on, the saved game; and a level started, or
the saved game continued, as campaign_start does. Menus' thread only. Include after cseries.h.
*/

#ifndef __AE_GLUE_CAMPAIGN_H
#define __AE_GLUE_CAMPAIGN_H

enum { AE_CAMPAIGN_LEVELS = 10, AE_CAMPAIGN_DIFFICULTIES = 4 };

struct ae_campaign_level
{
	char name[64];
	int reached;
	int finished[AE_CAMPAIGN_DIFFICULTIES];   /* (the game marks NORMAL, HEROIC, LEGENDARY: [1]-[3]) */
};

struct ae_campaign
{
	int have_profile;
	char profile[48];
	struct ae_campaign_level levels[AE_CAMPAIGN_LEVELS];
	short saved_level, saved_difficulty;      /* -1: no saved game */
	short difficulty;                         /* the last one played */
};

/* player 1's campaign now (their profile made player 1's, as campaign_profile does); 0 when there is no profile */
int ae_campaign_read(struct ae_campaign *campaign);
/* a level (0-9) at a difficulty (0-3) for player 1 on controller 0: the game loads it; 0 when not reached */
int ae_campaign_start(struct ae_campaign const *campaign, short level, short difficulty);
/* the saved game goes on; 0 when there is none */
int ae_campaign_continue(struct ae_campaign const *campaign);
/* EASY, NORMAL, HEROIC, LEGENDARY */
const char *ae_campaign_difficulty_name(short difficulty);

#endif
