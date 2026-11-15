/*
AE_DEV_WHEEL.H

DEV MODE's WEAPON WHEEL (owner 2026-11-01: "for dev all weapons give me a doom eternal weapon wheel don't need icons
just test when you hold weapon change button"): with DEV MODE on, holding SWITCH WEAPON opens a ring of every weapon
the map has loaded (its own, the packs', the ones no spawn places) by name; the look stick or the mouse points at
one, letting go takes it into the player's hands. A tap switches weapons as always; a hold let go without pointing
takes nothing. While it is open the player does not turn, fire, throw, strike or act.

Only where DEV MODE is on (ae_dev_mode_on: never a network game's client), on foot.
*/

#ifndef AE_DEV_WHEEL_H
#define AE_DEV_WHEEL_H

#include "ae_pure_types.h"

/* player_control.c's input blob: SWITCH WEAPON held or not this frame. With DEV MODE on its switch (rotate_bit in
player_flags) waits for the button to be let go of, and is not made when the hold opened the wheel */
void ae_dev_wheel_note_button(short local_player_index, int held, unsigned long *player_flags, unsigned long rotate_bit);
/* player_control.c: the wheel opens after the hold, takes the look as its pointer (the look then none) and the
fire and action buttons; letting go picks */
void ae_dev_wheel_input(short local_player_index, real time_delta_sec, real *yaw, real *pitch, unsigned long *unit_flags);
/* players.c, each tick a player's input is taken: the weapon picked goes into its unit's hands */
void ae_dev_wheel_apply(long player_index);
/* game_engine.c: the weapon the wheel makes is the one picked (no weapon set, no map remap) */
boolean ae_dev_wheel_giving(void);
/* hud_weapon.c: the ring over the view */
void ae_dev_wheel_draw(short local_player_index);

#endif
