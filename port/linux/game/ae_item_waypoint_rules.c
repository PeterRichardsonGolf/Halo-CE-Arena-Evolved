/* ae_item_waypoint_rules.c: when a power item's waypoint shows (ae_item_waypoint_rules.h). */
#include "ae_item_waypoint_rules.h"

int ae_iw_reason(
	long now,
	long phase,
	long period,
	long before_ticks,
	long after_ticks,
	long taken_spawn_tick,
	int training)
{
	long spawn, age;

	if (period <= 0 || now < 0)
		return AE_IW_NONE;
	/* (at a respawn tick the period is whole again: the new item is on the map) */
	if (period - (now + phase) % period <= before_ticks)
		return AE_IW_RESPAWN_SOON;
	age = (now + phase) % period;
	spawn = now - age;
	/* (TRAINING's: not the item placed as the game starts) */
	if (training)
		return spawn > 0 && age < after_ticks ? AE_IW_ON_THE_MAP : AE_IW_NONE;

	return taken_spawn_tick == spawn ? AE_IW_NONE : AE_IW_ON_THE_MAP;
}

long ae_iw_phase(
	long now,
	long period,
	long in_ticks)
{
	if (period <= 0)
		return 0;

	return (period - (now + in_ticks) % period) % period;
}

char const *ae_iw_reason_word(
	int reason)
{
	switch (reason)
	{
		case AE_IW_ON_THE_MAP: return "on the map";
		case AE_IW_RESPAWN_SOON: return "respawn soon";
	}

	return "";
}
