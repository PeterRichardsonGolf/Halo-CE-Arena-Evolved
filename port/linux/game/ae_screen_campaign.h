/*
AE_SCREEN_CAMPAIGN.H

AE's CAMPAIGN screen (ae_screen_campaign.c; the 0.3.7 menus): the levels player 1 has reached, the saved game, a
difficulty; a level played, or the saved game continued; split-screen co-op for up to four and X, a level hosted online as co-op (0.9.4). Opened from AE's main menu.
*/

#ifndef __AE_SCREEN_CAMPAIGN_H
#define __AE_SCREEN_CAMPAIGN_H

/* opens CAMPAIGN over the stack (player 1's campaign read now); 0 when the stack is full */
int ae_screen_campaign_open(void);

/* (DEV MODE `menu campaign [seat] [players <2-4>] [error]`) CAMPAIGN opened over the stack; players: that many players seated
(guests on controllers 2 up; 0 or 1: player 1 alone); error: the co-op start's error dialog shown (a player without a
profile). An open CAMPAIGN is replaced */
int ae_screen_campaign_open_staged(int players, int error);

#endif
