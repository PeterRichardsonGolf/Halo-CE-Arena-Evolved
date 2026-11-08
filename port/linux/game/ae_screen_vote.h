/*
AE_SCREEN_VOTE.H

A dedicated server's map vote in AE's menus (0.7.7; ae_screen_vote.c): the server's choices for the next game as a
popover over the screen the player is on in its lobby; A votes, B hides it until the next vote; the winner shown a
moment once it is over.
*/

#ifndef __AE_SCREEN_VOTE_H
#define __AE_SCREEN_VOTE_H

/* each frame, from AE's tick (ae_hooks.c): a vote begun opens it, its winner shown closes it */
void ae_screen_vote_tick(unsigned long now);
/* whether it is open */
int ae_screen_vote_open(void);

#endif
