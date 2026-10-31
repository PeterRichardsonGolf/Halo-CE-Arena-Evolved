/*
AE_RADAR.H

PLAYERS ON RADAR ENEMIES (the rules: ae_radar_rules.h): what the AE hook lines
in source/game/game_engine.c and source/interface/hud.c call
(port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
*/

#ifndef __AE_RADAR_H
#define __AE_RADAR_H

/* game_engine_draw_object_in_motion_sensor's answer for the unit, given its
stock answer (the variant's bit, then FRIENDS) and the motion sensor's viewer
(a local player index, NONE in the shared scan): under ENEMIES in a team game,
FALSE for the viewer's allies (a vehicle by its gunner, else its driver) */
boolean ae_radar_draw_object(boolean draw_object, long unit_index, short local_player_index);

/* whether a player's name over his head may show, as the motion tracker would
show him (hud.c): game_engine_draw_object_in_motion_sensor's, but allies'
names stay under ENEMIES */
boolean ae_radar_name_shown(long unit_index);

#endif
