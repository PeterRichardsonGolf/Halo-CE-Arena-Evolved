/*
AE_SCREEN_CAMPAIGN.H

AE's CAMPAIGN screen (ae_screen_campaign.c; the 0.3.7 menus): the levels player 1 has reached, the saved game, a
difficulty; a level played, or the saved game continued; split-screen co-op for up to four and X, a level hosted online as co-op (0.9.4). Opened from AE's main menu.
*/

#ifndef __AE_SCREEN_CAMPAIGN_H
#define __AE_SCREEN_CAMPAIGN_H

/* opens CAMPAIGN over the stack (player 1's campaign read now); 0 when the stack is full */
int ae_screen_campaign_open(void);

/* CAMPAIGN opened as ae_screen_campaign_open does, the stock level `map` (its file name: a10, a30 ...) focused; 0 and nothing opened when
it is not one of the ten */
int ae_screen_campaign_open_on_level(char const *map);

/* the custom campaign scenes of `menu campaign ... workshop [demo|fault|none|saved|damaged]`: WORKSHOP the first
installed custom campaign shown (the made-up one when none is installed), its first mission focused; DEMO the made-up
one whatever is installed; FAULT the real list with a made-up fault line, that line focused; NONE no custom campaign and
no fault (the line that says where they come from, focused); SAVED the made-up one with a made-up saved game (mission 2,
HEROIC), its CONTINUE focused; DAMAGED the made-up one whose saved game cannot go on, its CONTINUE focused */
enum { AE_CAMPAIGN_STAGE_REAL, AE_CAMPAIGN_STAGE_WORKSHOP, AE_CAMPAIGN_STAGE_DEMO, AE_CAMPAIGN_STAGE_FAULT,
	AE_CAMPAIGN_STAGE_NONE, AE_CAMPAIGN_STAGE_SAVED, AE_CAMPAIGN_STAGE_DAMAGED };

/* (DEV MODE `menu campaign [seat] [players <2-4>] [error] [workshop ...]`) CAMPAIGN opened over the stack; players: that
many players seated (guests on controllers 2 up; 0 or 1: player 1 alone); error: the co-op start's error dialog shown (a
player without a profile); workshop: an AE_CAMPAIGN_STAGE_*. An open CAMPAIGN is replaced */
int ae_screen_campaign_open_staged(int players, int error, int workshop);
/* after a staged open: a made-up saved game of the stock campaign (level 1-10 at a difficulty 0-3; level 0: none), the first row
of a kind focused ("skip", "continue", "squad", "down", "dialog": CONTINUE with its dialog open; "": as opened), the screen's difficulty (0-3; -1: as it is) */
void ae_screen_campaign_stage_more(int save_level, int save_difficulty, char const *focus, int difficulty);

#endif
