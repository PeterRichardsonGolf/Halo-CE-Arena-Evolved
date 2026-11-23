#ifndef AE_HIJACK_H
#define AE_HIJACK_H

#include "ae_hijack_rules.h"

/*
AE_HIJACK.H

HIJACK, the gametype's (ae_variant_ext's hijack; the rules: ae_hijack_rules.h). units.c's, players.c's and director.c's
AE hooks.
*/

/* (units.c's unit_find_nearby_seat, every machine) whether that unit is offered the driver's seat a player sits in */
boolean ae_hijack_offer(long unit_index, long occupant_unit_index, long vehicle_index, short seat_index);
/* (players.c's enter / evict action, the host) the takeover: TRUE when it happened (the driver thrown out, the unit in
the seat) */
boolean ae_hijack_try(long unit_index, long vehicle_index, short seat_index, long occupant_unit_index);
/* (units.c's unit_can_enter_seat) whether a unit just thrown out is kept out of the seats */
boolean ae_hijack_locked_out(long unit_index);
/* (units.c's unit_can_enter_seat) whether an enemy riding in another seat no longer keeps a player out (MIXED-TEAM VEHICLES
ON, or a hijack taking the seat) */
boolean ae_hijack_mixed_riders_now(void);

#endif
