/*
HUD_ITEM_TIMERS.H

port: MATCH CLOCK, CAMPAIGN TIMER, the power list of the gametype's TIMERS and TRAINING,
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
/* where hud_unit.c drew the player's shield and health meters (the view's
coordinates), whose right edge MATCH CLOCK's corner lines up with */
void hud_item_timers_set_meters(short local_player_index, rectangle2d const *bounds);
/* where the weapon's and unit's HUD drew their elements in the top left
corner (the view's coordinates; NULL for none), which the power list keeps
clear of */
void hud_item_timers_set_top_left(short local_player_index, rectangle2d const *bounds);
/* how far the power list moves the HUD's messages down in the view this
frame (hud_messaging.c; read once) */
short hud_item_timers_messages_offset(short local_player_index);
/* whether MATCH CLOCK's or CAMPAIGN TIMER's clock is drawn now (hud_unit.c
finds the meters only for it) */
boolean hud_item_timers_clock_shown(void);
/* the text scale of the first view (the power list's, the waypoints'
labels'), which the performance overlay (main.c) is drawn at */
real hud_item_timers_overlay_text_scale(void);
void hud_item_timers_initialize_for_new_map(void);
/* port: CAMPAIGN TIMER: drawn for render.local_player_index; ticked by
game_tick; told of a game state loaded (game_state.c) */
void hud_draw_campaign_timer(void);
void hud_campaign_timer_tick(void);
void hud_campaign_timer_game_state_loaded(void);

#endif // __HUD_ITEM_TIMERS_H
