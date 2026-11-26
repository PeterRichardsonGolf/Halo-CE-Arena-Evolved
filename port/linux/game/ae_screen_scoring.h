/*
AE_SCREEN_SCORING.H

The SCORING screen (ae_screen_scoring.c): the campaign's score and medals as Content Evolved's settings have them: SCORING
(game.campaign_score), CO-OP SCORING (network.coop_score_type), MEDAL POPUPS (game.campaign_medals) and CHECKPOINT COSTS POINTS
(game.campaign_score_revert_cost). The last three wait for SCORING (NEEDS SCORING). On the right: the mission's multipliers,
par time and the best run of player 1's profile, and the medals that can be won. Multiplayer's score and medals are not here.
*/

#ifndef __AE_SCREEN_SCORING_H
#define __AE_SCREEN_SCORING_H

/* opens SCORING over the stack for a stock level (0 to 9; -1 for none) at a difficulty (0 to 3), the mission the panel speaks of */
int ae_screen_scoring_open(short level, short difficulty);
/* (DEV MODE `menu scoring [off|on]`) opened over the stack with game.campaign_score set; an open one is replaced */
int ae_screen_scoring_open_staged(int scoring_on);

#endif
