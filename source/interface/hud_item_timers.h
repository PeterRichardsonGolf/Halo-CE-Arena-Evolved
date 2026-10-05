/*
HUD_ITEM_TIMERS.H

port: MATCH CLOCK, the power list of the gametype's TIMERS and TRAINING,
and TRAINING's waypoints (hud_item_timers.c).
*/

#ifndef __HUD_ITEM_TIMERS_H
#define __HUD_ITEM_TIMERS_H
#pragma once

/* ---------- prototypes/HUD_ITEM_TIMERS.C */

void hud_draw_item_timers(void);	/* for render.local_player_index, in render.camera */
void hud_draw_item_waypoints(short local_player_index);	/* TRAINING's, in render.camera */
/* where hud_unit.c drew the motion sensor's background (the view's
coordinates), which MATCH CLOCK's corner lines up with */
void hud_item_timers_set_motion_sensor(short local_player_index, rectangle2d const *bounds);
void hud_item_timers_initialize_for_new_map(void);

#endif // __HUD_ITEM_TIMERS_H
