/*
AE_BOTS_VANTAGE.H

The engine's side of the bots' vantage spots (ae_bots_vantage_rules.h has the rules and the scoring): the map's places that
matter and the best overlooks over them, found once a map as the bots' first ticks of a game run (a bounded step a tick, after
ae_bots_lanes.c's graph), never at the map's load, and the same for the same map. From halo-ce-vita (GPL-3.0) via Halo
Content Evolved (Watanoge/halo-content-evolved, AGPL-3.0-or-later). bots.vantage (true): the bots' holds (ae_bots_vantage.inc);
false: the spots are not even looked for.
*/

#ifndef __AE_BOTS_VANTAGE_H
#define __AE_BOTS_VANTAGE_H

#include "ae_bots_vantage_rules.h"

/* a new game's map: the spots forgotten */
void ae_bots_vantage_reset(void);
/* once a tick (ae_bots_lanes_update's caller): a bounded step; TRUE once the spots are made, or the map has none */
boolean ae_bots_vantage_update(void);
boolean ae_bots_vantage_ready(void);
/* the map's spots, best first, and the places they were scored by */
long ae_bots_vantage_count(void);
struct ae_vantage_spot const *ae_bots_vantage_spot(long index);
long ae_bots_vantage_place_count(void);
boolean ae_bots_vantage_place(long index, struct ae_vantage_place *place, char const **name);
/* a place by its name ("hill", "stand 0", "stand 1", "ball spawn"): the first of that name; FALSE for none */
boolean ae_bots_vantage_named_place(char const *name, long *index);

#endif
