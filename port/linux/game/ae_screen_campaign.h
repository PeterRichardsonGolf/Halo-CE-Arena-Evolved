/*
AE_SCREEN_CAMPAIGN.H

AE's CAMPAIGN screen (ae_screen_campaign.c; the 0.3.7 menus): the levels player 1 has reached, the saved game, a
difficulty; a level played, or the saved game continued. Opened from AE's main menu.
*/

#ifndef __AE_SCREEN_CAMPAIGN_H
#define __AE_SCREEN_CAMPAIGN_H

/* opens CAMPAIGN over the stack (player 1's campaign read now); 0 when the stack is full */
int ae_screen_campaign_open(void);

#endif
