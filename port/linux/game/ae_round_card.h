/*
AE_ROUND_CARD.H

(0.7.9) The ROUND WON / ROUND LOST card: for 3 seconds from the start of a round's POST phase, every local view shows the
round's verdict over the darkened game (the verdict as that viewer reads it, why the round ended, the round score, the
round's MVP, the next round's footer), drawn in AE's menus' layer (ae_draw.h) in the scoreboard's colours. The data is
the round state's (ae_round_wire.h's END tail); rules in ae_round_card_rules.h.
*/
#ifndef AE_ROUND_CARD_H
#define AE_ROUND_CARD_H

#include "cseries.h"

/* (interface.c) the HUD is not drawn while this view's card shows */
boolean ae_round_card_hides_hud(short local_player_index);

/* (interface.c) the card in the local view being drawn, when it shows */
void ae_round_card_draw(short local_player_index);

/* DEV MODE `roundcard won|lost|tied [reason]`: the card on every local view for 3 s with sample data (RED 2, BLUE 1,
MVP "Kestrel", 3 kills, round 4 of 7); reason is a word of the reason line ("eliminated", "bomb", "time", ...), none
by default. FALSE (with the answer) when the words are not known */
boolean ae_round_card_dev_stage(char const *verdict, char const *reason, char *answer, int size);

#endif
