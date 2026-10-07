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
	_item_timer_shotgun,	/* (port: a one-shot kill up close in CE: timed as a power weapon) */
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

/* the base a power entry is at (item_timers_map_begin): the red team's,
the blue team's, or neither (as near to both) */
enum item_timer_side
{
	_item_timer_side_middle = 0,
	_item_timer_side_red,
	_item_timer_side_blue,
	NUMBER_OF_ITEM_TIMER_SIDES
};

/* ---------- structures */

struct item_timer
{
	real_point3d position;
	long period_ticks;
	short timer_class;	/* enum item_timer_class */
	unsigned short classes;	/* a flag for each class it can spawn (OS/CAMO: both) */
	short side;	/* enum item_timer_side */
	boolean side_prefix;	/* RED / BLUE before its name: the same item is at the other base */
	wchar_t label[16];	/* the power list's: "SNIPER", "RED ROCKETS", "OS/CAMO", "SHOTGUN" */
};

/* ---------- prototypes/ITEM_TIMERS.C */

void item_timers_map_begin(void);	/* rebuilds the table for this map and game type */
short item_timers_count(void);
struct item_timer const *item_timers_get(short index);
long item_timer_ticks_left(struct item_timer const *timer);	/* 1..period */
boolean item_timers_training_shown(void);	/* TRAINING's markers may show, now */
boolean item_timer_waypoint_shown(struct item_timer const *timer);	/* TRAINING's waypoint over it, now */
boolean item_timer_on_map(struct item_timer const *timer);	/* its last spawn was under 20 s ago */
short item_timer_spawned_class(struct item_timer const *timer);	/* the item on the map's class, else NONE */
void item_timer_waypoint_name(struct item_timer const *timer, wchar_t *name, short size);	/* "RED OVERSHIELD" */
void item_timers_update(void);	/* per tick: logs TRAINING's waypoints going on and off */

#endif // __ITEM_TIMERS_H
