/*
AE_VEHICLE_DEATH.H

VEHICLES DESTRUCTIBLE as the gametype's rule (ae_vehicle_death_rules.h): what the AE hook lines in
source/objects/damage.c and source/game/game_engine.c call (port/linux/game/ae_hooks.txt lists them). The host (or a
local game) decides; a client takes the host's word through the object's dead bit. Included after cseries.h.
*/

#ifndef __AE_VEHICLE_DEATH_H
#define __AE_VEHICLE_DEATH_H

struct damage_data;

/* object_damage_body (damage.c): the body damage multiplier of the material the object was hit on, explosive
when the damage detonates explosives. Under ON a vehicle whose tag gives it no health takes the hit of the damage
effect's weapon class (ae_vehicle_balance_rules.h), gets the table's health on the first hit that does any, and its
0 multipliers count as 1; any other vehicle takes only explosives' damage */
real ae_vehicle_death_body_multiplier(long object_index, struct damage_data const *damage, boolean explosive,
	real multiplier);
/* object_damage_body: the damage effect's material modifier for the body: 1 on a vehicle the rule makes mortal (the
class's hit is the whole of it), else the modifier */
real ae_vehicle_death_material_modifier(long object_index, real modifier);
/* object_damage_body: whether the tag's "only hurt while occupied" keeps an empty vehicle unhurt (not one the rule
makes mortal) */
boolean ae_vehicle_death_occupied_only(long object_index);
/* object_damage_body, the object's health just ran out: TRUE when it holds at 0 instead (ON, a vehicle with a
living rider: "cannot die with riders"); the damage is remembered as its last */
boolean ae_vehicle_death_held(long object_index, struct damage_data const *damage);
/* object_damage_body, once its body was depleted: the explosion a stock multiplayer vehicle's tag lacks (owned by
the destroyer), the wreck noted to be cleared */
void ae_vehicle_death_depleted(long object_index, struct damage_data const *damage);
/* object_deplete_body, every machine (a client's copy dies by the host's dead bit): the explosion effect a stock
multiplayer vehicle's tag lacks, on a client (NONE for none, and on the host, which plays its own) */
long ae_vehicle_death_effect(long object_index);
/* game_engine_update_vehicle_respawn: the gametype's respawn time; the fallback for a destroyed vehicle where it is
"never" */
long ae_vehicle_death_respawn_ticks(long respawn_ticks);
/* game_engine_update_vehicle_respawn: that time by the vehicle's class (light 1x, special 1.5x, air 2x, heavy 3x) where
the rule is ON; unchanged for a gametype of Halo CE's own */
long ae_vehicle_death_class_respawn_ticks(long definition_index, long respawn_ticks);
/* a vehicle home at the first look: logs its class and respawn time */
void ae_vehicle_death_class_log(long definition_index);
/* game_engine_update_vehicle_respawn: whether an intact vehicle away from its pad goes back (not under the
fallback: "never" stays never for it) */
boolean ae_vehicle_death_intact_returns(void);
/* game_engine_update: held vehicles whose riders are gone, wrecks cleared, the HALO_VEHICLE_DEATH_TEST driver */
void ae_vehicle_death_update(void);
/* the "AE vehicles destructible:" line of the game's rules (from the variant's flags) */
void ae_vehicle_death_log_rules(unsigned long flags);

#endif
