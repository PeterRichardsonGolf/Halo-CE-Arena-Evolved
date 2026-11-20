#ifndef AE_MAP_VOLUMES_H
#define AE_MAP_VOLUMES_H

/*
AE_MAP_VOLUMES.H

MAP VOLUMES (ae_map_volumes_rules.h): a multiplayer map's kill zones and lifts by their trigger volumes' names.
*/

#include "cseries.h"

/* (game_engine.c's players' update, each player each tick, beside the teleporters) the host's kill of a living
player in a kill zone; a lift's move (as a teleporter's, every machine) */
void ae_map_volumes_player_update(long player_index);
/* whether trigger volume `volume` is a kill zone (ae_bots_falls.inc: deadly for the bots from the map's load) */
boolean ae_map_volumes_kills(long volume);

#endif
