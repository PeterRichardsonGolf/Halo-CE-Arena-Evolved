/*
AE_VEHICLE_DEATH.H

SPIKE (not a feature yet): destructible vehicles in multiplayer, host side, behind HALO_VEHICLE_DEATH=1.
*/

#ifndef __AE_VEHICLE_DEATH_H
#define __AE_VEHICLE_DEATH_H

struct damage_data;

/* object_damage_body (damage.c): the body damage multiplier of the material the vehicle was hit on; a vehicle the
rule makes mortal gets its health pool here on its first hit, and its MP tag's 0 multipliers count as 1 */
real ae_vehicle_death_body_multiplier(long object_index, real multiplier);
/* object_damage_body, once its body was depleted: the riders die, credited to the damage's owner */
void ae_vehicle_death_depleted(long object_index, struct damage_data const *damage);
/* object_deplete_body, every machine (a client's copy dies by the host's dead bit): the explosion effect a stock
multiplayer vehicle's tag lacks, NONE for none */
long ae_vehicle_death_effect(long object_index);
/* game_engine_update_vehicle_respawn: the gametype's respawn time, a fallback where it is "never" */
long ae_vehicle_death_respawn_ticks(long respawn_ticks);
/* game_engine_update: wrecks cleared, and the HALO_VEHICLE_DEATH_TEST driver */
void ae_vehicle_death_update(void);

#endif
