/*
AE_TRACKED_GRENADE_RULES.C

The pure rules of a tracked grenade (ae_tracked_grenade_rules.h).
*/

#include "ae_tracked_grenade_rules.h"

int ae_tracked_grenade_tracks(int pack_own_weapon, int trigger_tracks, long projectile_in_shot)
{
	return pack_own_weapon && trigger_tracks && projectile_in_shot == 0;
}

int ae_tracked_grenade_same_projectile(int projectile_there, long projectile_definition, long recorded_definition)
{
	return projectile_there && recorded_definition != -1 && projectile_definition == recorded_definition;
}

int ae_tracked_grenade_same_weapon(int weapon_there, long weapon_definition, long recorded_definition)
{
	return weapon_there && recorded_definition != -1 && weapon_definition == recorded_definition;
}

int ae_tracked_grenade_trigger_held(int weapon_there, int in_hand, int trigger_down)
{
	return weapon_there && in_hand && trigger_down;
}

int ae_tracked_grenade_tick_action(int weapon_there, int trigger_held, long held_ticks)
{
	if (weapon_there && trigger_held && held_ticks >= 0)
		return held_ticks <= AE_TRACKED_GRENADE_MAX_HOLD_TICKS ? AE_TRACKED_GRENADE_HOLD : AE_TRACKED_GRENADE_RUN;
	return held_ticks >= AE_TRACKED_GRENADE_MIN_HOLD_TICKS ? AE_TRACKED_GRENADE_DETONATE : AE_TRACKED_GRENADE_RUN;
}

long ae_tracked_grenade_owner(long owner_index, int owner_there)
{
	return owner_index != -1 && !owner_there ? -1 : owner_index;
}

int ae_tracked_grenade_fuse_runs(int tick_action, int pack_own_projectile, long timer_starts, int collided_once)
{
	return tick_action == AE_TRACKED_GRENADE_RUN && pack_own_projectile &&
		timer_starts == AE_TRACKED_GRENADE_STARTS_AFTER_FIRST_BOUNCE && collided_once;
}
