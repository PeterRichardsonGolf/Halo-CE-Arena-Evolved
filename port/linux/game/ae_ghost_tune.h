/*
AE_GHOST_TUNE.H
THE AE GHOST (ae_ghost_tune_rules.h): what the AE hook lines in source/items/weapons.c and source/game/game_engine.c
call (port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
*/

#ifndef __AE_GHOST_TUNE_H
#define __AE_GHOST_TUNE_H

/* weapons_new_map: the map's ghost_mp and its cannon found, their own numbers kept */
void ae_ghost_tune_new_map(void);
/* game_engine_update, every machine: the AE ghost's numbers put in while the gametype's VEHICLES DESTRUCTIBLE is ON,
the map's own back otherwise; one "ae ghost:" line each time it changes */
void ae_ghost_tune_update(void);

#endif
