#ifndef AE_HAMMER_H
#define AE_HAMMER_H

struct damage_data;

/*
AE_HAMMER.H

The gravity hammer's swing blow and blast push (the rules: ae_hammer_rules.h). damage.c's, weapons.c's and units.c's AE
hooks. Included after cseries.h.
*/

/* (damage.c, object_damage_aftermath, before the push by the object's type) whether the damage pushes the object:
TRUE for any other damage, its acceleration as it is; for the gravity hammer's blast, TRUE and its push this tick in
acceleration, or FALSE when the blast does not push the object */
boolean ae_hammer_blast_pushes(long object_index, struct damage_data const *damage, real_vector3d *acceleration);

/* (weapons.c, weapon_trigger_fire, once a trigger has fired) a swing of the gravity hammer lands the hammer's blow on
what it hits with longer rays (a body whose centre is within 2.8), as the melee button's blow does (unit_cause_player_melee_damage) */
void ae_hammer_swing_fired(long weapon_index, long owner_object_index);

/* (units.c, unit_cause_player_melee_damage, each ray) how much longer the blow's rays are: 1 but inside a swing's
blow, until its rays are cast */
real ae_hammer_melee_reach_scale(void);

/* (units.c, unit_cause_player_melee_damage, after its rays) the rays are cast: no later ray is longer */
void ae_hammer_melee_rays_cast(void);

/* (units.c, unit_cause_player_melee_damage) whether this blow is a swing's: its recoil on the swinger (the weapon's
melee response) is left out */
boolean ae_hammer_swing_blow(void);

/* (weapons.c, weapon_prevents_melee_attack) whether the hammer refuses the melee button: within its trigger's cycle
after a swing (one swing, one cooldown) */
boolean ae_hammer_melee_blocked(long weapon_index);

#endif
