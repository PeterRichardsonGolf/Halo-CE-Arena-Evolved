/*
AE_DS5_GAME.H

Arena Evolved: what each local player wants on a DualSense, from the game (the platform side,
port/linux/src/ae_ds5.c, sends it; Settings > Gamepads' ADAPTIVE TRIGGERS, TRIGGER STRENGTH and LIGHTBAR). Plain
types only.
*/

#ifndef __AE_DS5_GAME_H
#define __AE_DS5_GAME_H

/* (player_control.c handle_one_player_input, after the zoom level is set, for a player with a unit) the weapon object
in hand (NONE: none) and the zoom level (NONE: not zoomed) */
void ae_ds5_player(short local_player_index, long weapon_index, short zoom_level);

#endif
