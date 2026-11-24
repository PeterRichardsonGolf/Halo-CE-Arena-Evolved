/*
AE_BOT_NAV_OVERLAY.H

Arena Evolved: DEV MODE's drawing of the bots' vehicle navigation in the world, for finding where a route is wrong. Setting
debug.bot_nav_show (0 off, the default), or the dev console's `nav show [off|routes|all]`:
- routes (1): the nav file's routes as lines a little over the ground, ice blue (a scorpion's route the darker blue), a
  square at each point, a chevron mid-segment pointing the way;
- all (2) adds: the nav file's keepouts as dashed amber rings, the scenery's obstacle circles as red rings at their height,
  the circles learnt from hits as thicker red rings, the walkable surfaces closed to the bots outlined in red, this game's
  drive troubles as diamonds (red crashes, orange wedges, purple stuck), the learnt wedge places as hollow red diamonds,
  and for each driving bot a white line to its next waypoint and a thin ice-blue line through the rest of its path.
Only what is within 40 wu of the camera is drawn, into a fixed buffer of triangles (what does not fit is counted). Drawn in
the world pass from ae_zone_world_draw (depth tested, see-through, under the HUD) through the debug geometry's triangles.
halo.log: "bot nav overlay: <map>: N routes (N points), N obstacles, N keepouts, N learnt circles, N closed surfaces, N drive
spots" when it turns on and when the map changes while on. Display only; nothing is networked.
*/

#ifndef __AE_BOT_NAV_OVERLAY_H
#define __AE_BOT_NAV_OVERLAY_H

#include "cseries.h"

/* the world pass's call, per local player's view */
void ae_bot_nav_overlay_draw(short local_player_index);

#endif
