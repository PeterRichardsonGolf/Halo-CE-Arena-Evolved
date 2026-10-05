/*
ITEM_TIMERS.H

port: the netgame equipment's spawns for the gametype's TIMERS and TRAINING
(item_timers.c).
*/

#ifndef __ITEM_TIMERS_H
#define __ITEM_TIMERS_H
#pragma once

#include "math/real_math.h"

/* ---------- constants */

enum item_timer_class
{
	_item_timer_rockets = 0,
	_item_timer_sniper,
	_item_timer_overshield,
	_item_timer_camo,
	NUMBER_OF_ITEM_TIMER_POWER_CLASSES,
	_item_timer_other = NUMBER_OF_ITEM_TIMER_POWER_CLASSES,
};

/* TRAINING's waypoints over the power entries (item_timer_waypoint_shown),
as Halo 1: NHE's Training mode's: on from 10 s before a spawn to 20 s after
it, 0.6 world units over the spawn point */
#define ITEM_TIMER_WAYPOINT_BEFORE_TICKS (10 * TICKS_PER_SECOND)
#define ITEM_TIMER_WAYPOINT_AFTER_TICKS (20 * TICKS_PER_SECOND)
#define ITEM_TIMER_WAYPOINT_HEIGHT 0.6f

/* ---------- structures */

struct item_timer
{
	real_point3d position;
	long period_ticks;
	short timer_class;	/* enum item_timer_class */
	wchar_t label[16];
};

/* ---------- prototypes/ITEM_TIMERS.C */

void item_timers_map_begin(void);	/* rebuilds the table for this map and game type */
short item_timers_count(void);
struct item_timer const *item_timers_get(short index);
long item_timer_ticks_left(struct item_timer const *timer);	/* 1..period */
boolean item_timer_waypoint_shown(struct item_timer const *timer);	/* TRAINING's waypoint over it, now */
void item_timers_update(void);	/* per tick: logs TRAINING's waypoints going on and off; the voice (Task 6) */

#endif // __ITEM_TIMERS_H
