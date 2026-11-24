/*
AE_SPAWNER_HOLO_H

port: the weapon spawners' hologram (rc2; the rules: ae_spawner_holo_rules.h). A spawner has no rack and no plate: it is
the weapon's own spot on the ground. While a weapon is returning, its GHOST lies exactly where and how the weapon will
lie: the weapon's own model in the pose its weapon last lay in there while ready (before it was ever seen there, the
item's placement with the weapon on its side on the ground), its silhouette outlined and its faces toward the viewer
filling from the stock to the muzzle as the timer runs, a bright line at the fill edge (build-up). When the weapon is
back the real one lies in it and the ghost is gone. A spot whose weapon was taken and is still in play (RED: an expired
rack's) keeps the whole ghost in red; a POWER pad's timer text and waypoint are the item timers'. Colours: the menus' ice
blue for the body and a rack's outline, a white outline round a POWER pad's weapon, red for IN USE; whole up to 10 world
units, fading out by 16; the last 3 seconds pulse. Drawn in the world
pass (render.c after the zone's wall: depth tested, alpha blended, under the HUD) through the debug geometry's
triangles, on every machine, from ae_spawners.h's view of the spawners. The log names why a spot's ghost is not drawn
each time that changes. Display only; nothing is networked.
*/

#ifndef AE_SPAWNER_HOLO_H
#define AE_SPAWNER_HOLO_H

/* the world pass's hook, per local player's view */
void ae_spawner_holo_draw(short local_player_index);

/* (ae_spawners.c's timers) whether a weapon lies on the spawner's spot now, as the last look at the map's weapons saw it
(a weapon seen in the last 10 ticks still counts); TRUE when the spot is unknown, so a timer is never cut on a guess */
boolean ae_spawner_holo_weapon_lies(long equipment_index);

/* the hologram's pass is off with this setting (debug.spawner_holo_off, HALO_AE_SPAWNER_HOLO_OFF; shown by default) */

#endif
