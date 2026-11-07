/*
AE_FALL_DAMAGE.H
FALL DAMAGE HALO 2 (ae_fall_damage_rules.h): what the AE hook lines in source/items/weapons.c, source/game/game_engine.c and
source/units/vehicles.c call (port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
*/

#ifndef __AE_FALL_DAMAGE_H
#define __AE_FALL_DAMAGE_H

/* weapons_new_map: the map's falling damage numbers are read again before they are changed */
void ae_fall_damage_new_map(void);
/* game_engine_update, every machine: the HALO 2 numbers put in while the gametype's FALL DAMAGE is HALO 2, the map's own
back otherwise; one "ae fall damage:" line each time it changes */
void ae_fall_damage_update(void);
/* vehicles.c: the velocity a falling vehicle is destroyed at, the map's own even while players follow HALO 2 */
float ae_fall_damage_vehicle_velocity(float globals_velocity);

#endif
