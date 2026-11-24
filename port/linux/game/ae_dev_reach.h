/*
AE_DEV_REACH.H

DEV MODE's can't-reach report (owner 2026-11-15, ask-me: "Can't-reach report"; the rules: ae_dev_reach_rules.h): `dummy
reach [<n>]` sends n walking dummies (ae_dev_dummy.h: the bots' own routes, never shooting, no target taken) from every
spawn of the loaded map to every flag stand, ball spawn, hill, race checkpoint and power item. Each walk's line goes to
the log ("dev reach: spawn 2 -> red flag: stuck at (x y z) after 12 s, 14.1 u short at best"); at the end a summary, and
the whole report in the config folder's reach/<map file>.txt. `dummy clear` stops it (the walks so far written).
*/

#ifndef __AE_DEV_REACH_H
#define __AE_DEV_REACH_H

#include "cseries.h"

/* the run started with `walkers` walkers for the player: the walks' count (and bots.count's new value in *bots_wanted),
or 0 and why in error */
int ae_dev_reach_start(long player_index, int walkers, long *bots_wanted, char *error, int error_size);
/* the run stopped (dummy clear, a map change): what was walked written */
void ae_dev_reach_stop(void);
boolean ae_dev_reach_running(void);
/* (ae_dev_dummy_tick, each tick for each walking dummy) where its unit is, whether it lives, the tick it was last put at
its start */
void ae_dev_reach_sample(long bot_index, float x, float y, float z, boolean alive, long placed_tick, long now);

#endif
