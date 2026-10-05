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
void item_timers_update(void);	/* per tick: the voice (Task 6); empty until then */

#endif // __ITEM_TIMERS_H
