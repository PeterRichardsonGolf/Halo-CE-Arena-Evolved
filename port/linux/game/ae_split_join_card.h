/*
AE_SPLIT_JOIN_CARD.H

The card a pad with no player gets in a game in progress: its START opens it, "PRESS A TO JOIN" (A joins: the host adds the
player and the screen gives them a view) and B (or START again) puts it away. When the game would not take the player it says
why instead (the host's switch off, a full game, a tournament match) and any press of A or B closes it. Drawn over the
whole screen in AE's menus' layer, read from the pad directly: it is no screen on AE's stack, so the players already
playing keep their own pause menus. Rules: ae_split_join_rules.h; the game's calls: ae_glue_split_join.h.
*/

#ifndef __AE_SPLIT_JOIN_CARD_H
#define __AE_SPLIT_JOIN_CARD_H

/* once a frame (ae_hooks.c): a free pad's START opens a card, an open card's A and B */
void ae_split_join_card_tick(void);
/* (ae_hooks.c, in the frame's first view) the open cards */
void ae_split_join_card_draw(void);
/* a card is open for the controller (its press is the card's, not the game's) */
int ae_split_join_card_open(short controller);
/* DEV MODE: a card opened for the controller (1-3) with no pad press; 0 when the game would show none */
int ae_split_join_card_stage(short controller);

#endif
