/*
AE_ZONE_HUD.H

The round modes' danger zone on the HUD (ae_zone_view.h's state, ae_round_zone_rules.h's rules): outside it a violet tint at
the frame's edge, a dark-red pulse at each damage tick, "RETURN TO THE ZONE" with an arrow and the distance; inside near its
edge (while it closes, or in the last ten seconds before it does) a "ZONE EDGE" line; the 30 s and 10 s lines and a tick at
10 s. Drawn in the HUD pass for the viewer's own living player; a machine without the round state draws nothing.
*/
#ifndef AE_ZONE_HUD_H
#define AE_ZONE_HUD_H

void ae_zone_hud_draw(short local_player_index);

#endif
