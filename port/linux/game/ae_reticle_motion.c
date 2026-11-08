/*
AE_RETICLE_MOTION.C

The unified reticle motion (ae_reticle_motion.h): one signal for every code-drawn crosshair, from the weapon's own state.

SPREAD is the held weapon's trigger error (the game grows it while the trigger is held and lets it fall back), as an
angle above the trigger's minimum, in pixels at the view's field of view: the lines mark the real cone. KICK is a shot
(the held weapon's rounds loaded going down, once a pull for a burst) as ae_rifle_reticle_scale's curve: rise fast,
settle slowly. MOVE is the player's ground speed. A weapon's own numbers stay its tags' and its rifle profile's: only
what this machine draws moves. Nothing is sent; nothing is kept but each local player's last weapon, its rounds and
the time of its last shot.
*/

#include <math.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "render/render.h"
#include "render/render_cameras.h"

#include "ae_burst_rifle.h"
#include "ae_crosshair_rules.h"
#include "ae_reticle_motion.h"
#include "ae_rifle_profiles.h"


void platform_log(char const *format, ...);

/* a weapon without a profile or a spread: a modest kick */
enum { DEFAULT_KICK_PERCENT = 35, DEFAULT_KICK_MS = 300 };

static struct
{
	long weapon_index;
	short rounds_loaded;
	boolean shot;
	unsigned long shot_ms;
	real from;
} states[MAXIMUM_LOCAL_PLAYERS];

static real vertical_fov(short local_player_index)
{
	/* (the view's own: the camera's field of view in radians; the game's default when none is known) */
	real fov = render.camera.vertical_field_of_view;

	(void)local_player_index;
	return fov > 0.1f && fov < 3.0f ? fov : 1.2217f;
}

void ae_reticle_motion_of(short local_player_index, long weapon_index, struct ae_reticle_motion *motion)
{
	struct weapon_datum *weapon;
	struct weapon_definition *definition;
	struct ae_rifle_profile const *profile;
	real lower = 0.0f, upper = 0.0f, growth, growth_ms;
	short rounds_loaded;
	unsigned long now;

	motion->spread = motion->kick = motion->move = motion->error = 0.0f;
	memset(&motion->cls, 0, sizeof(motion->cls));
	if (weapon_index == NONE || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	weapon = weapon_get(weapon_index);
	definition = weapon_definition_get(weapon->definition_index);
	profile = ae_burst_rifle_profile_of(weapon->definition_index);
	if (definition->weapon.triggers.count > 0)
	{
		struct weapon_trigger_definition const *trigger =
			TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, 0, struct weapon_trigger_definition);

		lower = trigger->projectile_error_angle_lower_bound;
		upper = trigger->projectile_error_angle_upper_bound;
		/* (a gametype with NO SPREAD has no cone to show: the weapon kicks by its rate, as a weapon with a fixed cone does) */
		if (game_engine_no_spread() != _no_spread_off)
			upper = lower;
		ae_xh_motion_class_of((real)trigger->final_rate_of_fire, (real)(lower * 57.2957795f), (real)(upper * 57.2957795f),
			&motion->cls);
		/* (once for each weapon: the class its tag gives it, for the test runs) */
		{
			static long logged[32];
			static int logged_count;
			int seen;

			for (seen = 0; seen < logged_count && logged[seen] != weapon->definition_index; seen++)
				;
			if (seen == logged_count && logged_count < 32)
			{
				logged[logged_count++] = weapon->definition_index;
				platform_log("crosshair: motion class %s: %.1f shots a second, cone %.2f to %.2f degrees: %s %.2f %s %d ms",
					tag_get_name(weapon->definition_index), (double)trigger->final_rate_of_fire, (double)(lower * 57.2957795f),
					(double)(upper * 57.2957795f), motion->cls.has_spread ? "spread gain" : "kick growth",
					(double)(motion->cls.has_spread ? motion->cls.gain : motion->cls.growth), "settling", motion->cls.ms);
			}
		}
		if (upper > lower && game_engine_no_spread() == _no_spread_off)
		{
			real error = weapon->weapon.triggers[0].error;

			motion->error = error < 0.0f ? 0.0f : error > 1.0f ? 1.0f : error;
			{
				static unsigned long last_ms;
				unsigned long at = system_milliseconds();

				if (error > 0.02f && at - last_ms > 700)
				{
					last_ms = at;
					platform_log("crosshair: trigger error %.2f (%s), cone now %.2f degrees", (double)error,
						tag_get_name(weapon->definition_index), (double)((lower + (upper - lower) * motion->error) * 57.2957795f));
				}
			}
			motion->spread = ae_xh_spread_pixels((upper - lower) * (error < 0.0f ? 0.0f : error > 1.0f ? 1.0f : error),
				vertical_fov(local_player_index));
		}
	}
	/* (the kick: a weapon with a spread has none) */
	if (upper > lower && !profile)
		goto move;
	growth = profile ? profile->reticle_growth : motion->cls.growth > 0.0f ? motion->cls.growth : (real)DEFAULT_KICK_PERCENT / 100.0f;
	growth_ms = profile ? (real)profile->reticle_growth_ms : motion->cls.ms > 0 ? (real)motion->cls.ms : (real)DEFAULT_KICK_MS;
	rounds_loaded = weapon->weapon.magazines[0].rounds_loaded;
	now = system_milliseconds();
	if (states[local_player_index].weapon_index != weapon_index)
	{
		states[local_player_index].weapon_index = weapon_index;
		states[local_player_index].shot = FALSE;
	}
	else if (rounds_loaded < states[local_player_index].rounds_loaded &&
		!(states[local_player_index].shot &&
			now - states[local_player_index].shot_ms < (profile ? ae_rifle_pull_ms(profile) : 0)))
	{
		states[local_player_index].from = states[local_player_index].shot ?
			ae_rifle_reticle_scale(states[local_player_index].from, growth, (short)growth_ms,
				now - states[local_player_index].shot_ms) : 1.0f;
		states[local_player_index].shot = TRUE;
		states[local_player_index].shot_ms = now;
	}
	states[local_player_index].rounds_loaded = rounds_loaded;
	if (states[local_player_index].shot && growth > 0.0f)
	{
		real scale = ae_rifle_reticle_scale(states[local_player_index].from, growth, (short)growth_ms,
			now - states[local_player_index].shot_ms);

		motion->kick = (scale - 1.0f) / growth;
		if (motion->kick < 0.0f)
			motion->kick = 0.0f;
	}
move:
	{
		struct player_datum *player = player_try_and_get(local_player_get_player_index(local_player_index));

		if (player && player->unit_index != NONE)
		{
			real_vector3d velocity;
			real speed;

			object_get_velocities(player->unit_index, &velocity, NULL);
			speed = sqrtf(velocity.i * velocity.i + velocity.j * velocity.j);
			/* (a Spartan's top ground speed is about 0.07 world units a tick: the cue is 3 pixels at that) */
			motion->move = speed / 0.07f * 3.0f;
			if (motion->move > 3.0f)
				motion->move = 3.0f;
		}
	}
}
