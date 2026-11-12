/*
AE_SCREEN_PARTY.H

Arena Evolved's PARTY manager (menus phase 1, spec §8; ae_screen_party.c): a popover over the right column, opened by
X (F) or a click on MY PARTY. MY PARTY's head and count; PLAYERS: a card per seat (player 1's first), the focused one's
chips CHANGE PROFILE and REMOVE (guests only); a pad choosing its profile has its card here; a free controller's open
card. THIS PC: ADD A PLAYER (the next START or A on a free controller seats it; "THE PARTY IS FULL" at 4/4) and
PROFILES. B: a tap closes it; a 0.8 s hold on a guest's card removes the guest (ae_party_rules.h). Player 1 owns it;
other controllers' START joins from here.

Its log: "ae party: open (n/4)", "ae party: closed", "ae party: player N joined (controller N, profile NAME)" (the main
menu's), "ae party: NAME left (controller N)", "ae party: add a player: waiting", "ae party: full (4/4)".
*/

#ifndef __AE_SCREEN_PARTY_H
#define __AE_SCREEN_PARTY_H

struct ae_flow_screen;

/* over the screen whose flow is given (its density and frame); 0 when it could not open */
int ae_screen_party_open(struct ae_flow_screen const *under);
void ae_screen_party_close(void);
/* (0.8.2) whether the PARTY manager is open over the screen of this flow: that screen leaves its MY PARTY column (and
LAST GAME) and its prompts out under it; the manager's panel and footer take their place (they showed through its
panel and mixed with its prompts) */
int ae_screen_party_covers(struct ae_flow_screen const *flow);

#endif
