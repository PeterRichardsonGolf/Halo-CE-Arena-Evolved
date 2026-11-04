/*
AE_SWORD_LUNGE.H

Arena Evolved: the energy sword's lunge (ae_sword_lunge.c has the numbers). A swing (the melee button, or the sword's
right trigger, which is its swing) with an enemy in the lunge's reach, the reticle's red, carries the swinger to him.
The carry style (the default) makes that lunge the attack; the strike style keeps a lunge followed by a strike. Halo
CE has no lunge: its melee is a 0.8 unit reach in front of the head.

The sword: the weapon of the energy sword pack (AE_WEAPON_SLOT_ENERGY_SWORD), found by its slot where that pack is
added to the map (its numbers written into its tags as each map starts). The host decides; the lunge is not
networked yet (a client's prediction is not told).
*/

#ifndef __AE_SWORD_LUNGE_H
#define __AE_SWORD_LUNGE_H

/* weapons.c's new map (weapons_initialize_for_new_map): the packs' swords set up, the lunges forgotten */
void ae_sword_lunge_new_map(void);

/* units.c's unit_control: a player's sword's right trigger becomes the melee button, its weapon not fired */
void ae_sword_lunge_control(long unit_index);

/* bipeds.c's player melee, whether the strike style starts its strike now though the melee button is up: a lunge has arrived */
boolean ae_sword_lunge_strike_now(long biped_index);

/* bipeds.c's player melee, a swing about to start: TRUE when the lunge takes it (a sword's holder with an enemy in
reach is carried to him; a lunge under way holds every swing), FALSE when the swing starts now */
boolean ae_sword_lunge_begin(long unit_index);

/* bipeds.c's player melee, a swing that started: the strike style's lunge blow lands next tick */
void ae_sword_lunge_swing(long biped_index);

/* the lunge's blow (unit_cause_player_melee_damage): the carry style calls it on contact; bipeds.c calls it at a swing's frame */
void ae_sword_lunge_melee_damage(long biped_index);

/* bipeds.c's biped_update_moving, before the biped's physics: a lunging biped's velocity this tick (no
acceleration of its own while it lunges) */
void ae_sword_lunge_physics(long biped_index, real_vector3d *velocity, real *acceleration_maximum,
	real *airborne_acceleration_maximum);

/* the bots (ae_bots_combat.inc): whether the unit holds a sword that lunges, its reach (0 for none), whether its
lunge is ready (none under way, none in the last 1.1 s) */
boolean ae_sword_lunge_unit_has_sword(long unit_index);
real ae_sword_lunge_reach(long unit_index);
boolean ae_sword_lunge_ready(long unit_index);

#endif
