/*
AE_BURST_RIFLE.H

port: weapon pack. The AE Halo 2 battle rifle's behaviour (ae_burst_rifle.c):
the weapons of the pack on the loaded map whose pack entry says behaviour 1
(AE_PACK_BEHAVIOUR_AE_BATTLE_RIFLE) fire Halo 2's burst (three rounds a new
pull, then a recovery), with Halo 2's zoom as seen, aim assist, spread,
bullet, magazine and the owner's balance. Every other weapon, a map's own
battle rifle included, keeps its tags. The hooks are single lines in
weapons.c, aim_assist.c and projectiles.c (ae_hooks.txt).

Include after cseries.h.
*/

#ifndef __AE_BURST_RIFLE_H
#define __AE_BURST_RIFLE_H

/* ---------- weapons.c */

/* (weapons_initialize_for_new_map, its end) the burst rifles of the pack added to the map set up, the burst
states cleared; a map without the pack has none */
void ae_burst_rifle_new_map(void);
/* whether a weapon tag is one of the burst rifles set up on this map */
boolean ae_burst_rifle_definition(long weapon_definition_index);
/* (weapon_trigger_fire) the damage effect fired on the firer's view: NONE (no kick) for a burst rifle's
trigger 0, else damage_effect_index */
long ae_burst_rifle_kick(long weapon_definition_index, short trigger_index, long damage_effect_index);
/* (weapon_trigger_fire, after the round) one round of a burst counted (fired or not) */
void ae_burst_rifle_round(long weapon_index, short trigger_index, boolean fired);
/* (weapon_update, the trigger's tick) the burst driver: a new pull starts three rounds, which go out whatever
the trigger does; then the recovery; trigger_down is what the trigger does this tick */
void ae_burst_rifle_trigger(long weapon_index, short trigger_index, boolean *trigger_down);

/* ---------- aim_assist.c */

/* (unit_get_aim_assist_parameters) zoomed, a burst rifle's aim assist ranges end at its bullet's range */
void ae_burst_rifle_zoomed_assist(long weapon_definition_index, short zoom_level, real *autoaim_distance,
	real *magnetism_distance);

/* ---------- projectiles.c (debug.weapon_hit_log) */

/* (projectile_collision) before and after a round's impact damage on an object: one "ae rifle hit" line for a
burst rifle's round fired by a local player of this machine */
void ae_burst_rifle_hit_before(long projectile_index, long object_index);
void ae_burst_rifle_hit_after(long projectile_index, long object_index, short material_index);

/* ---------- the zoomed view (render_particles.c, Task 9) */

/* the burst rifle the local player is zoomed in with (and its holder in *unit_index), or NONE */
long ae_burst_rifle_local_player_zoomed(short local_player_index, long *unit_index);

/* ---------- a collision model's head (the hit log, network_test.c's head hits) */

/* whether material_index of the object's collision model is a head (a headshot's material) */
boolean ae_damage_material_is_head(long object_index, short material_index);
/* the first material of the object's collision model that is a head, or NONE */
short ae_damage_head_material_index(long object_index);

#endif
