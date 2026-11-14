/*
AE_REVIVE_HUD.H

ELIMINATION's revive made visible (HUD STYLE AE only; owner-approved mockups 2026-11-05, rules ae_revive_hud_rules.h, the state from the
round state's message, ae_round_internal.h's ae_round_view):
- a downed teammate: the open plus marker over the spot in the owner's six-segment ring (team colour; the revive's progress white),
  the name over it and the distance over the name, seen through walls, teammates only;
- a downed teammate in reach: the item prompt "<NAME> / Hold [X] to revive" (ae_item_prompt.c) with the same six segments round the
  button, filling together with the marker's ring and emptying on release;
- the downed player's own view: the owner's DOWNED helmet in its ring top centre, "DOWNED", and who is reviving them, or
  WAITING FOR REVIVE with the nearest teammate or the bleed-out time left (BLEEDOUT, the gametype's seconds).
The classic HUD style keeps the dead-ally cross and says the downed state in the game's own line.
*/
#ifndef AE_REVIVE_HUD_H
#define AE_REVIVE_HUD_H

#include "cseries.h"

/* (ae_dead_ally_markers_draw's start, an AE hook line: one view's revive HUD, each frame) */
void ae_revive_hud_draw(short local_player_index);
/* (the item prompt's) the six segments round a button centred at (cx, cy) in the view's pixels: white segments filled, the others dim */
void ae_revive_hud_button_ring(real cx, real cy, real radius, int white_segments, real alpha);
/* (ae_dead_ally_markers.c) the player is shown by the revive marker: the dead-ally cross leaves them out */
boolean ae_revive_hud_owns_marker(long player_index);
/* (game_engine_get_state_message, an AE hook line) a downed player's own line: HUD STYLE AE draws its own view and the state line stays empty
(*result FALSE); another style gets "DOWNED: WAITING FOR REVIVE". TRUE when the player is downed and the line is answered */
boolean ae_revive_hud_state_message(long player_index, wchar_t *message, long count, boolean *result);

#endif
