/*
AE_SPAWN_MODERN_RULES.H

The rules of AE MODERN SPAWNS' recent deaths (ae_spawn_modern.c), with nothing of the engine's
(port/linux/tests/ae_spawn_modern_rules_test.c).

A player slot's last death pushes its own and its allies' spawns away from where it happened for a while. The game
clock starts over at each game, so a death tick later than the clock is the previous game's and counts for nothing,
and a new game forgets every slot's death.
*/

#ifndef __AE_SPAWN_MODERN_RULES_H
#define __AE_SPAWN_MODERN_RULES_H

/* whether a death at death_tick (0: none) still counts at now: in this game, within window ticks */
int ae_spawn_recent_death(long now, long death_tick, long window);

/* a new game: every slot's death tick set to 0 (none) */
void ae_spawn_deaths_forget(long *death_ticks, int count);

#endif
