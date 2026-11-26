/*
AE_SCREEN_CHALLENGES.H

The CHALLENGES screen: today's three and this week's three challenges of a local player's profile with their counters and XP, the
focused one's detail, and the career rank block. Opened by LT on the CAREER screen and by DEV MODE's `menu challenges [player <1-4>]`.
*/

#ifndef __AE_SCREEN_CHALLENGES_H
#define __AE_SCREEN_CHALLENGES_H

/* local player 0 .. 3's challenges. 1 when open */
int ae_screen_challenges_open(char const *crumb, short local_player);
void ae_screen_challenges_close(void);

#endif
