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
	_item_timer_fuel_rod,	/* (AE: the Halo PC and MCC maps' heavy weapons; no callout clips yet) */
	_item_timer_flamethrower,
	_item_timer_pad,	/* (AE: a weapon spawner's POWER pad of another class (sword, hammer...; ae_spawners.c): named by its weapon, no callout clips) */
	NUMBER_OF_ITEM_TIMER_POWER_CLASSES,
	_item_timer_other = NUMBER_OF_ITEM_TIMER_POWER_CLASSES,
};

/* the power classes the callouts call: rockets to camo (callouts.c); the fuel
rod and the flamethrower have no clips in the voice packs yet */
#define NUMBER_OF_ITEM_TIMER_CALLED_CLASSES (_item_timer_camo + 1)

/* TRAINING's waypoints over the power entries (item_timer_waypoint_shown),
as Halo 1: NHE's Training mode's, in AE's window (0.7.8; NHE's was 10 s
before to 20 s after): on from 30 s before a spawn (the whole wait when the
period is shorter) to 15 s after it, 0.6 world units over the spawn point */
#define ITEM_TIMER_WAYPOINT_BEFORE_TICKS (30 * TICKS_PER_SECOND)
#define ITEM_TIMER_WAYPOINT_AFTER_TICKS (15 * TICKS_PER_SECOND)
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
	wchar_t label[24];	/* the power list's: "SNIPER", "RED ROCKETS", "OS/CAMO", "SHOTGUN" */
	short number;	/* 1, 2, ... among power entries of the same label (two RED ROCKETS), else 0 */
	long class_definitions[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];	/* the item of each power class it can spawn (as the gametype remaps it), else NONE */
	long equipment_index; /* AE hook */
	boolean own_name; /* AE hook */
};

/* ---------- prototypes/ITEM_TIMERS.C */

void item_timers_map_begin(void);	/* rebuilds the table for this map and game type */
boolean item_timers_bases(real_point3d bases[2]); /* AE hook */
boolean item_timers_shotgun_is_power(void);	/* SHOTGUN AS POWER (display.shotgun_power): the shotgun is a power item */
short item_timers_count(void);
struct item_timer const *item_timers_get(short index);
long item_timer_ticks_left(struct item_timer const *timer);	/* 1..period */
boolean item_timers_training_shown(void);	/* TRAINING's markers may show, now */
boolean item_timers_waypoints_shown(void);	/* the waypoints (TRAINING's, or TIMERS' with waypoints) may show, now */
boolean item_timers_waypoints_in_sight_only(void);	/* LINE OF SIGHT: only while the spot is in view */
boolean item_timer_waypoint_shown(struct item_timer const *timer);	/* the waypoint over it, now (in its window) */
boolean item_timer_on_map(struct item_timer const *timer);	/* its last spawn was under 20 s ago */
short item_timer_spawned_class(struct item_timer const *timer);	/* the item on the map's class, else NONE */
void item_timer_waypoint_name(struct item_timer const *timer, wchar_t *name, short size);	/* "RED OVERSHIELD" */
short item_timer_waypoint_class(struct item_timer const *timer);	/* the class its waypoint's name is of, else NONE */
long item_timer_waypoint_definition(struct item_timer const *timer);	/* the item of that class, else NONE */
long item_timers_equipment_phase(long equipment_index); /* AE hook */
boolean item_timers_dev_respawn(char const *name, long seconds, char *label, long label_size); /* AE hook */
void item_timers_update(void);	/* per tick: logs TRAINING's waypoints going on and off */

#endif // __ITEM_TIMERS_H
