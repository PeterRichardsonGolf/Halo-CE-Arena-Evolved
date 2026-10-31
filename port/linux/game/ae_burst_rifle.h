/*
AE_BURST_RIFLE.H

port: weapon pack. The AE battle rifle's behaviour (ae_burst_rifle.c):
the weapons of the pack on the loaded map whose pack entry says behaviour 1
(AE_PACK_BEHAVIOUR_AE_BATTLE_RIFLE) fire a burst (three rounds a new
pull, then a recovery), with the zoom as seen, aim assist, spread,
bullet, magazine and the owner's balance. Every other weapon, a map's own
battle rifle included, keeps its tags. The hooks are single lines in
weapons.c, aim_assist.c and projectiles.c (ae_hooks.txt).

Include after cseries.h.
*/

#ifndef __AE_BURST_RIFLE_H
#define __AE_BURST_RIFLE_H

/* ---------- weapons.c */

/* the AE rifle profile a weapon definition was set up with (ae_rifle_profiles.h), NULL for any other weapon */
struct ae_rifle_profile const *ae_burst_rifle_profile_of(long weapon_definition_index);

/* (weapons_initialize_for_new_map, its end) the burst rifles of the pack added to the map set up, the burst
states cleared; a map without the pack has none */
void ae_burst_rifle_new_map(void);
/* whether a weapon tag is one of the burst rifles set up on this map */
boolean ae_burst_rifle_definition(long weapon_definition_index);
/* the ticks between a rifle's pulls at its quickest (its profile's: a pull's rounds 2 apart, then its recovery); 0 for
a weapon that is not an AE rifle */
short ae_burst_rifle_pull_ticks(long weapon_definition_index);
/* whether a damage effect is a burst rifle's rounds' (its bullet's impact damage) on this map */
boolean ae_burst_rifle_damage(long damage_definition_index);
/* (0.7.0) the AE rifle profile whose rounds do that damage, NULL for any other */
struct ae_rifle_profile const *ae_burst_rifle_damage_profile(long damage_definition_index);
/* (weapon_trigger_fire) the damage effect fired on the firer's view: for a burst rifle's trigger 0 the pack's own
effect setup made rumble-only, else NONE (no kick); other weapons' damage_effect_index */
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

/* ---------- the zoomed view (render_particles.c) */

struct particle_datum;

/* the burst rifle the local player is zoomed in with (and its holder in *unit_index), or NONE */
long ae_burst_rifle_local_player_zoomed(short local_player_index, long *unit_index);
/* (render_particles, before its particles are gathered) the view being drawn: local_player_index the local player
seen in first person, or another value (MAXIMUM_LOCAL_PLAYERS, NONE) for none */
void ae_burst_rifle_particles_begin(short local_player_index);
/* (render_particles, each particle) TRUE when the particle is left out of the view: zoomed in with a burst rifle,
its holder sees no particle on the rifle or on himself, nor his own first-person ones (the muzzle flash, which
would stand in front of the scope: none shows while zoomed) */
boolean ae_burst_rifle_particle_hidden(struct particle_datum const *particle, boolean owned_by_local_player);

/* ---------- a collision model's head (the hit log, network_test.c's head hits) */

/* whether material_index of the object's collision model is a head (a headshot's material) */
boolean ae_damage_material_is_head(long object_index, short material_index);
/* the first material of the object's collision model that is a head, or NONE */
short ae_damage_head_material_index(long object_index);

#endif
