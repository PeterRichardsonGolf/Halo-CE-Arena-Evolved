/*
AE_GLUE_ZOOM.H

What the AE hook lines of the zoom action's modes call in player_control.c and
game_engine.c (ae_hooks.txt lists those lines; notes/ae-hooks.md says where
each goes). The state machine is ae_zoom.c's. Plain types only.
*/

#ifndef __AE_GLUE_ZOOM_H
#define __AE_GLUE_ZOOM_H

/* (player_control.c, where the input blob's zoom bit is set) records whether the zoom action is down in a bit of the
blob's player control flags that nothing else uses */
void ae_zoom_note_held(unsigned long *player_control_flags, int held);
/* (player_control.c, once the blob is final) in the HOLD and BOTH modes takes away the blob's press bit (zoom_bit,
which the engine steps the level on), so that the level is ae_zoom_update's alone; the TOGGLE mode leaves it */
void ae_zoom_filter_press(unsigned long *player_control_flags, int zoom_bit);
/* (player_control.c, after the engine's own step of the level) the level to hold: unchanged in the TOGGLE mode. May
act: whether the weapon can zoom now (a weapon in hand, camera control, no cinematic). */
short ae_zoom_update(short local_player_index, short zoom_level, long weapon_index, int may_act,
	unsigned long player_control_flags);
/* (game_engine.c game_engine_log_rules, a match's start) logs "zoom: mode ..., hold time ..." */
void ae_zoom_log_settings(void);

#endif
