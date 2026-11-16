/*
AE_VEHICLE_PACK.H

The vehicle damage pack's look under VEHICLES DESTRUCTIBLE (ae_vehicle_pack_rules.h): what ae_vehicle_death.c calls.
Every machine shows what its own pack gives: the pack's tags are found by name on the loaded map
(ae_weapon_pack_vehicle_pack_added), and a machine without them shows the core's basic death. Nothing here is sent
over the network: the effects and the wrecks are each machine's own (effects and scenery are no networked
objects). Included after cseries.h.
*/

#ifndef __AE_VEHICLE_PACK_H
#define __AE_VEHICLE_PACK_H

/* a vehicle the rule destroyed, on every machine (the host from ae_vehicle_death_depleted, a client as the host's
dead bit kills its copy): the pack's explosion beside the map's, the parts coming off, and the wreck standing in
for it (or, for a kind with none, its body burning); nothing without the pack or for a vehicle of no kind */
void ae_vehicle_pack_died(long vehicle_index);
/* each tick, every machine: the damaged look of the vehicles whose health this machine knows (the host's, a local
game's), the looks of vehicles gone forgotten */
void ae_vehicle_pack_update(void);

#endif
