/*
AE_ITEM_WAYPOINT_RULES.H

When a power item's waypoint shows (item_timers.c's item_timer_waypoint_shown), as pure rules (port/linux/tests/ae_item_waypoint_rules_test.c
runs them). Only WHEN it shows: nothing here draws or places it.

TIMERS' waypoints (HUD + WAYPOINTS, LINE OF SIGHT): the waypoint shows while the item lies on the map, from the game's start, until it is
taken; it also shows for the countdown, ITEM_TIMER_WAYPOINT_BEFORE_TICKS before each respawn, taken or not. TRAINING keeps its fixed
window (from the countdown to AFTER ticks past each spawn, none at the start).

All times are game ticks. phase is a dev respawn's shift of the entry's cycle (0 in a game: spawns at the multiples of the period; item_timers.c's
DEV command item respawn): its spawns are at the game times where now + phase is a multiple of the period. taken_spawn_tick is the spawn (the last multiple of the period; 0 for the item lying at the start) whose item
has been taken, or -1 for none.
*/

#ifndef __AE_ITEM_WAYPOINT_RULES_H
#define __AE_ITEM_WAYPOINT_RULES_H

enum
{
	AE_IW_NONE,
	AE_IW_ON_THE_MAP,		/* its item lies on the map */
	AE_IW_RESPAWN_SOON,		/* the countdown before the respawn */
	AE_IW_REASONS
};

int ae_iw_reason(long now, long phase, long period, long before_ticks, long after_ticks, long taken_spawn_tick, int training);

/* the phase that puts the next spawn `in_ticks` after now */
long ae_iw_phase(long now, long period, long in_ticks);

/* "on the map", "respawn soon"; "" for none */
char const *ae_iw_reason_word(int reason);

#endif
