#ifndef AE_MELEE_H
#define AE_MELEE_H

struct damage_data;

/*
AE_MELEE.H

MELEE, the gametype's (ae_variant_ext's melee_rule; the numbers: ae_melee_rules.h). damage.c's AE hooks. Included
after cseries.h.

The rule covers a melee blow of a player on a player's unit that is not in a vehicle. The blow counts once: on a
standing shield the shield side raises it to its floor and the leftover carries on (ae_shield_bleed.c converts it by
ae_melee_shield_unraised), and the body side leaves it as it is; on an empty shield the body side raises it.
*/

/* (damage.c, object_damage_shield) the shield points of this blow */
real ae_melee_shield_points(long victim_index, struct damage_data const *damage, boolean is_melee, real points);

/* (ae_shield_bleed.c) the unraised shield points of the covered blow now on victim_index, 0 when there is none */
real ae_melee_shield_unraised(long victim_index);

/* (damage.c, object_damage_body) the body's damage of this blow as a fraction of max health (in and out; max health
only converts it inside a covered blow, any other damage comes back as it came) */
real ae_melee_body_points(long victim_index, struct damage_data const *damage, boolean is_melee, real fraction,
	real maximum_body);

/* (damage.c, the force kill) whether this blow on victim_index kills outright from behind: the covered blow, on an
enemy, the victim unsuspecting of its epicenter and able to take damage. The stock lethal-to-the-unsuspecting kill
stays as it is. */
boolean ae_melee_force_kill(long victim_index, struct damage_data const *damage, boolean is_melee);

/* (units.c, unit_cause_player_melee_damage, after the head marker's position) AE only: every weapon's blow starts from
the same point, the unit's position plus the reach along the aim's horizontal, at the head marker's height as posed
(the weapon's stance leans the head forward, a rifle's 0.163 and a pistol's 0.064); CLASSIC leaves it as it is */
void ae_melee_ray_origin(long unit_index, real_point3d *origin);

/* (damage.c, after an object's shield and body) the shield side's mark ends with the object's pass */
void ae_melee_mark_clear(void);

#endif
