/*
AE_SCREEN_END_CARD.H

The end-of-mission card (ae_screen_end_card.c): with AE's menus on, the summary of a campaign mission won with SCORING on is
AE's screen, drawn over the held level in the post-game report's layout (campaign_score.c's summary, one card for each local
player in turn), in place of the game's own card. The left side says how the mission was played (time, kills, deaths, the
medals won); the right how the score was made (points, the difficulty's, the skulls' and the time's multipliers, the final
score; the team's table in co-op). A goes on: the next player's card, the next mission, or the finish after the last mission. A
button held as the card appears does not count.
*/

#ifndef __AE_SCREEN_END_CARD_H
#define __AE_SCREEN_END_CARD_H

/* once a frame (ae_hooks.c): opens as a summary is up, closes once none is */
void ae_screen_end_card_tick(void);
/* (campaign_score_draw's hook) 1 when AE's menus draw the summary */
int ae_end_card_wanted(void);

/* (DEV MODE `menu endcard [players <1-4>] [last] [best]`) a made-up summary of that many players' cards over the stack; last: the last
mission's; best: a new best score and time. A goes through the cards, the last closes it */
int ae_screen_end_card_open_staged(int players, int last_mission, int best);

#endif
