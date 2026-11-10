/*
AE_REVIVE_ORB.H

ELIMINATION's downed player in the world: a translucent glowing orb on the floor at the spot where they went down, in their TEAM's
colour (red team red, blue team blue), seen by everyone, with a small marker turning inside it, a soft glow on the floor and a
gentle pulse. Their body is not drawn while they are down (ae_revive_hides_body). Code-drawn world geometry in vertex colours,
as the danger zone's wall (ae_zone_world.c): no tag, no pack. The state is the round state's (ae_round_view: the host's own,
or a joined PC's from message 106). An OUT player (eliminated) keeps a normal body; a revive brings the player back at the spot.
*/
#ifndef AE_REVIVE_ORB_H
#define AE_REVIVE_ORB_H

#include "cseries.h"

/* (an AE hook in render.c's world pass, beside the zone's wall) one view's orbs */
void ae_revive_orb_draw(short local_player_index);
/* (an AE hook in render_object_list) TRUE: this object is a downed player's body: it is not drawn */
boolean ae_revive_hides_body(long object_index);

#endif
