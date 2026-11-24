/*
AE_RETICLE_GROWTH.C

RETICLE GROWTH: an AE rifle's aim reticle grows a little with each shot and
comes back, so a shot is felt in the reticle (the owner's picks, 10-30: the
EVO RIFLE's whole reticle grows; the BATTLE RIFLE's ring keeps its size while
its ticks spread, once a burst: "a little less busy"; the DMR's middle stays while its outer ring kicks out). How much, how long and which items stay put are the rifle's
profile's (ae_rifle_profiles.c); the shape is ae_rifle_reticle_scale's.

Only the drawing changes: the spread is the profile's own (none for these
rifles). A shot is seen as the held weapon's loaded rounds going down, which a
network client sees as well as the host, so it shows the same on any machine;
nothing is sent and nothing kept but each local player's last weapon, its
rounds and the time of the last shot. debug.reticle_growth (a percent, -1 the
profile's) sizes it for the owner's tests.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"

#include "ae_burst_rifle.h"
#include "ae_crosshair.h"
#include "ae_reticle_motion.h"
#include "ae_rifle_profiles.h"
#include "ae_reticle_growth.h"

long config_integer(char const *name);
void platform_log(char const *format, ...);

static struct
{
	long weapon_index;
	short rounds_loaded;
	boolean shot;
	unsigned long shot_ms;
	real from;
} growth_states[MAXIMUM_LOCAL_PLAYERS];

static real growth_of(
	struct ae_rifle_profile const *profile)
{
	long percent = config_integer("debug.reticle_growth");

	return percent >= 0 ? (real)percent / 100.0f : profile->reticle_growth;
}

static real growth_scale(
	short local_player_index,
	long weapon_index,
	boolean aim,
	short item_index)
{
	struct ae_rifle_profile const *profile;
	struct weapon_datum *weapon;
	real growth;
	short rounds_loaded;
	unsigned long now;

	if (!aim || weapon_index == NONE || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return 1.0f;
	/* (0.7.7 RETICLE MOTION: OFF keeps every reticle still; ALL WEAPONS gives a weapon without a rifle profile the motion
	a drawn crosshair has: its real spread and a shot's kick, ae_reticle_motion.c's) */
	if (ae_crosshair_stock_motion(local_player_index) == AE_XH_STOCK_MOTION_OFF)
		return 1.0f;
	weapon = weapon_get(weapon_index);
	profile = ae_burst_rifle_profile_of(weapon->definition_index);
	if (!profile)
	{
		struct ae_reticle_motion motion;
		real scale;

		if (ae_crosshair_stock_motion(local_player_index) != AE_XH_STOCK_MOTION_ALL)
			return 1.0f;
		ae_reticle_motion_of(local_player_index, weapon_index, &motion);
		scale = (real)ae_xh_stock_motion_scale(&motion.cls, (float)motion.kick, (float)motion.error, (float)motion.spread);
		/* (the peak each weapon reaches, logged each time it passes the last by 0.05, or its error by 0.2: for the owner's tests) */
		{
			static long peak_weapon[MAXIMUM_LOCAL_PLAYERS];
			static real peak[MAXIMUM_LOCAL_PLAYERS], peak_error[MAXIMUM_LOCAL_PLAYERS];

			if (peak_weapon[local_player_index] != weapon->definition_index)
			{
				peak_weapon[local_player_index] = weapon->definition_index;
				peak[local_player_index] = 1.0f;
				peak_error[local_player_index] = 0.0f;
			}
			if (scale > peak[local_player_index] + 0.05f || motion.error > peak_error[local_player_index] + 0.2f)
			{
				peak[local_player_index] = scale > peak[local_player_index] ? scale : peak[local_player_index];
				peak_error[local_player_index] = motion.error;
				platform_log("crosshair: motion peak %.2f (%s: error %.2f, kick %.2f)", (double)scale,
					tag_get_name(weapon->definition_index), (double)motion.error, (double)motion.kick);
			}
		}
		return scale;
	}
	growth = growth_of(profile);
	rounds_loaded = weapon->weapon.magazines[0].rounds_loaded;
	now = system_milliseconds();
	/* (the state follows the held weapon: a new one starts at rest; a round gone is a shot, from the size it has now,
	once a pull: a burst's later rounds start nothing; a reload's rounds coming back are not) */
	if (growth_states[local_player_index].weapon_index != weapon_index)
	{
		growth_states[local_player_index].weapon_index = weapon_index;
		growth_states[local_player_index].shot = FALSE;
	}
	else if (rounds_loaded < growth_states[local_player_index].rounds_loaded &&
		!(growth_states[local_player_index].shot && now - growth_states[local_player_index].shot_ms < ae_rifle_pull_ms(profile)))
	{
		growth_states[local_player_index].from = growth_states[local_player_index].shot ?
			ae_rifle_reticle_scale(growth_states[local_player_index].from, growth, profile->reticle_growth_ms,
				now - growth_states[local_player_index].shot_ms) :
			1.0f;
		growth_states[local_player_index].shot = TRUE;
		growth_states[local_player_index].shot_ms = now;
	}
	growth_states[local_player_index].rounds_loaded = rounds_loaded;
	if (!growth_states[local_player_index].shot ||
		(item_index >= 0 && item_index < 8 && TEST_FLAG(profile->reticle_fixed_items, item_index)))
	{
		return 1.0f;
	}
	return ae_rifle_reticle_scale(growth_states[local_player_index].from, growth, profile->reticle_growth_ms,
		now - growth_states[local_player_index].shot_ms);
}

/* the growth, and the player's RETICLE SIZE on top of it (HUD > RETICLE: the weapon's own reticle's size) */
real ae_reticle_growth_scale(
	short local_player_index,
	long weapon_index,
	boolean aim,
	short item_index)
{
	real scale = growth_scale(local_player_index, weapon_index, aim, item_index);

	return aim ? scale * ae_crosshair_stock_size(local_player_index) : scale;
}
