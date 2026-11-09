/*
AE_BURST_RIFLE.C

port: weapon pack. The AE battle rifle (ae_burst_rifle.h): the
behaviour approved in the prototype (ae/br-prototype b2b489ac,
played as halo-pack5f with the v6e donor; the weapon pack plan's "Rifle
numbers and look" table and its rulings R7, R12, R13, R14).

Which weapons: the packs on the loaded map (ae_weapon_pack_slot_added), each of their
weapon entries whose behaviour names a profile (ae_rifle_kind_of_behaviour: 1 the battle
rifle, 2 the EVO RIFLE, 3 the DMR, 4 the burst PDW, which is no rifle but fires its
bursts the same way, its rounds a tick apart; 5 the RAVAGER, no rifle either, whose
tags keep their own bolts and charged shot: the driver only paces its tap fire, below). Never
a tag picked by its name: a map's own battle rifle (CE+ X's) keeps its tags
(and is replaced by the pack's where the map refers to it: ae_weapon_pack.c).
The pack's tags are the map's own copy, made again for every map, so the
numbers below are written into them once a map, as the map starts
(weapons_initialize_for_new_map); every machine of a game does the same.

The numbers are constants (R14: no settings or environment tune them, or a
client could tune its own aim assist and zoom). The numbers are plain
values written here; no game data or code is copied.

The burst: a NEW pull (the trigger up last tick, down now) starts three
rounds, one every second tick (15 a second); they go out whatever
the trigger does. The recovery is then counted from the tick after the
burst's last round, whole ticks with the fraction carried from burst to
burst, and a pull during it is lost (the barrel clears its fire request
when its recovery ends). The next burst comes the tick
after a pull once the recovery is over.

The tags' rounds (a profile's tags_rounds, the RAVAGER, 0.7.6): its trigger 0
charges (the stock plasma pistol's way), so a tap fires as the trigger is
released, by the tags. That bolt starts the burst: the next rounds go out
round_ticks apart, each fired by the trigger's idle state at once (no charging:
ae_burst_rifle_forced_round), the player's own trigger held up meanwhile; then
the recovery, during which the trigger stays up (a tap is lost; a hold starts
charging once it ends). Its charged shot (trigger 1) is followed by the recovery
too. A burst stops when the weapon overheats, its battery is empty or it is put
away. Nothing else of the weapon is touched: its bolts, damage, heat and battery a
bolt, zoom, aim assist, spread and kick are its tags'.

Test aids (port_config.c, docs/testing.md): debug.weapon_log (each round,
any machine), debug.burst_autofire (the holder pulls again as soon as it
may: a host or no network game only), debug.weapon_hit_log (each of this
machine's players' rounds that hits an object).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "game/game.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "game/player_rumble.h"
#include "items/projectile_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "items/weapon_datum_flags.h"
#include "objects/damage_effect_definitions.h"
#include "math/periodic_functions.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "physics/collision_model_definitions.h"
#include "camera/observer.h"
#include "effects/particles.h"
#include "render/render_cameras.h"
#include "units/units.h"
#include "view_fov.h"

#include "ae_pack_format.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_registry.h"
#include "ae_burst_rifle.h"
#include "ae_rifle_profiles.h"
#include "ae_objective_log.h"

int config_boolean(const char *name);
long config_integer(const char *name);
double config_real(const char *name);

/* ---------- the numbers */

#define DEGREES(x) ((x) * 0.017453292f)

/* each rifle's own numbers: its PROFILE (ae_rifle_profiles.c: the BATTLE RIFLE's burst, the EVO RIFLE's and the
DMR's single shots, the PDW's burst), by its pack weapon entry's behaviour. The rest here is every AE rifle's */

/* a pull's rounds go out one every profile round_ticks: the rifles' 2 (rate of fire 15), the PDW's 1 (30); a burst
is three of them (the barrel's "maximum shots" 3) */
#define ROUNDS_PER_SECOND(profile) ((real)TICKS_PER_SECOND / (real)(profile)->round_ticks)
/* a burst cut short (the rifle reloading, put away, dropped): this many ticks before the next pull counts (the
prototype's) */
#define BURST_CUT_PAUSE_TICKS 8

/* the error rises 10 a second firing and falls 10 a second after (the barrel's error acceleration /
deceleration, the unzoomed one-handed case): a third a tick; a profile's equal bounds make no bloom */
#define ERROR_RATE_PER_TICK (10.0f / TICKS_PER_SECOND)

/* the damage's modifiers on a cyborg's shield and body are each profile's (ae_rifle_profiles.c: the rifles' x1.06 and
x1.5, the PDW's x1.0) */

/* zoomed, the aim assist's ranges (multiplied by the magnification in aim_assist.c) end at the bullet's range (the
owner's, b2b489ac: at 3x they were 43.6 and 53.8 wu, past anything a round reaches; R13) */

/* the zoom: one level, a profile's magnification as SEEN (chosen 2026-10-10: 3x for the
battle rifle), a constant (R12). Seen means: the cyborg's 70 degrees / 3 = 23.3 degrees wide at 4:3 (17.6 vertical).
CE's camera keeps 0.85 of a field of view's tangent (render_camera_get_adjusted_field_of_view_tangent), so CE's field
of view F is seen as 2 atan(0.85 tan(F/2)): CE needs F = 2 atan(tan(23.3 / 2) / 0.85) = 27.3 degrees, a CE
magnification of 70 / 27.3 = 2.563 */
#define UNIT_FIELD_OF_VIEW_DEGREES 70.0f
#define CAMERA_TANGENT_FACTOR 0.85f

/* each round's kick (10-30). The kick was removed whole before (473a99d18), rumble and all. Now: both motors in a short shaped pulse (0.034 s, curves from about 0.9 and 0.8), a camera
impulse 0.5 s with a very early fade, 0.5 degrees of rotation, 0.03 wu pushback, jitter 0.01 to 0.025; no camera
shake, no flash. CE's rumble is one strength a motor for a time: the pulse at the curves' peaks for 0.08 s (0.034 is
under a 30 Hz tick). Every AE rifle takes the same. The impulse OFF (in play it felt like recoil
instead of steady; the view holds still): rumble only */
#define KICK_RUMBLE_HIGH 0.9f
#define KICK_RUMBLE_LOW 0.8f
#define KICK_RUMBLE_SECONDS 0.08f

/* (weapons.c's struct trigger_firing_effect, private to it: a trigger's firing effect, its effects and damage
effects for firing, overheated and empty) */
struct burst_rifle_firing_effect
{
	short shots_lower_bound;
	short shots_upper_bound;
	long unused[8];
	struct tag_reference effects[3];
	struct tag_reference damage_effects[3];
};

/* (weapons.c's enum weapon_trigger_flags, private to it: a weapon_trigger's flag 0) */
#define TRIGGER_RELEASED_SINCE_LAST_SHOT_BIT 0
/* (weapons.c's MAXIMUM_NUMBER_OF_MAGAZINES_PER_WEAPON: a weapon datum's magazines, weapons.h) */
#define WEAPON_MAGAZINES_MAXIMUM 2
typedef char verify_weapon_magazines_maximum[
	sizeof(((struct weapon_datum *)0)->weapon.magazines) / sizeof(struct weapon_magazine) == WEAPON_MAGAZINES_MAXIMUM ? 1 : -1];

/* (debug.burst_autofire 2) the pull in the recovery: with this many of its ticks left (the recovery is 7
ticks at 0.20 s: 6 and the tick it starts on); and the next pull this many ticks after the recovery */
#define AUTOFIRE_EARLY_PAUSE_TICKS 3
#define AUTOFIRE_LATE_TICKS 15

/* the weapons the driver plays on one map (ae_rifle_profiles.h's AE_RIFLE_WEAPONS_A_MAP: every pack on the map, so
every profile's weapon at once, as DEV MODE and the TEST RUN lists add every installed pack; 4 was one pack's) */
#define BURST_RIFLES_MAXIMUM AE_RIFLE_WEAPONS_A_MAP

/* ---------- globals */

static struct
{
	/* the weapons set up, each with its bullet (NONE: none kept) and its profile (ae_rifle_profiles.h) */
	struct ae_rifle_weapons weapons;
	/* the test aids' settings, read once a map (debug.weapon_log, debug.burst_autofire, debug.weapon_hit_log) */
	boolean weapon_log;
	long autofire;
	boolean hit_log;
	/* the rumble-only firing damage effects (the pack's own, cleared of their view kick) */
	short kick_count;
	long kick_definitions[BURST_RIFLES_MAXIMUM * 3];
	/* (test aids: once a map) */
	boolean zoomed_assist_logged;
	boolean autofire_logged;
	boolean kick_logged;
	short zoomed_view_ticks[MAXIMUM_LOCAL_PLAYERS];
	/* (debug.weapon_log: bursts started on this map, by anyone) */
	long bursts_started;
} burst_rifles;

/* a weapon's burst, by its datum's absolute index (its identifier tells a new weapon in the same slot) */
static struct burst_state
{
	long identifier;
	char rounds_remaining;
	char pause_ticks;
	/* the trigger as it was last tick: a burst takes a new pull */
	char trigger_was_down;
	/* (a single shot) a pull during the recovery, kept: it fires the tick the recovery ends */
	char pull_held;
	/* the recovery's fraction of a tick, carried */
	real recovery_carry;
	/* (debug.weapon_log: the tick of the last burst's first round) */
	long burst_start_tick;
	/* (debug.burst_autofire 2: the ticks the rifle has been ready since its recovery ended) */
	long autofire_idle_ticks;
	/* (the tags' rounds) the ticks to the burst's next round, and that round asked of the trigger's idle state this
	tick (ae_burst_rifle_forced_round) */
	char round_wait;
	char force_round;
} burst_states[MAXIMUM_OBJECTS_PER_MAP];

/* (render_particles: the view being drawn, zoomed in with a burst rifle or not) */
static struct
{
	short local_player_index;
	long weapon_index;
	long unit_index;
	/* (debug.weapon_log: each zoom's first particle left out is logged, a few a map) */
	boolean logged[MAXIMUM_LOCAL_PLAYERS];
	short logged_count;
} zoomed_particles = { NONE, NONE, NONE };

/* (the hit log: the victim before the damage) */
static struct
{
	long projectile_index;
	long object_index;
	real shield;
	real body;
} hit_before = { NONE, NONE, 0.0f, 0.0f };

/* ---------- private code */

static struct burst_state *burst_state_get(
	long weapon_index)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(weapon_index);
	struct burst_state *state;

	if (weapon_index == NONE || slot < 0 || slot >= MAXIMUM_OBJECTS_PER_MAP)
		return NULL;
	state = &burst_states[slot];
	if (state->identifier != DATUM_INDEX_TO_IDENTIFIER(weapon_index))
	{
		state->identifier = DATUM_INDEX_TO_IDENTIFIER(weapon_index);
		state->rounds_remaining = 0;
		state->pause_ticks = 0;
		state->trigger_was_down = TRUE;
		state->pull_held = FALSE;
		state->recovery_carry = 0.0f;
		state->burst_start_tick = NONE;
		state->autofire_idle_ticks = 0;
		state->round_wait = 0;
		state->force_round = FALSE;
	}
	return state;
}

/* the recovery's ticks after a pull: the profile's, the fraction carried (none while they are whole ticks), and the
tick the barrel stops on */
static char burst_recovery_ticks(
	struct burst_state *burst,
	struct ae_rifle_profile const *profile)
{
	real total = (real)profile->recovery_ticks;
	long ticks = (long)total;

	burst->recovery_carry += total - (real)ticks;
	if (burst->recovery_carry >= 1.0f)
	{
		burst->recovery_carry -= 1.0f;
		ticks++;
	}
	return (char)PIN(ticks + 1, 1, 127);
}

static boolean magazine_index_valid(
	struct weapon_definition const *weapon_definition,
	short magazine_index)
{
	return magazine_index >= 0 && magazine_index < weapon_definition->weapon.magazines.count &&
		magazine_index < WEAPON_MAGAZINES_MAXIMUM;
}

static boolean local_player_unit(
	long unit_index)
{
	struct unit_datum const *unit;

	if (unit_index == NONE)
		return FALSE;
	unit = unit_get(unit_index);
	return unit->unit.player_index != NONE && player_get(unit->unit.player_index)->local_player_index != NONE;
}

/* a burst rifle's profile, by its weapon definition; NULL for a weapon that is not one */
static struct ae_rifle_profile const *profile_of_definition(
	long weapon_definition_index)
{
	return ae_rifle_weapons_profile(&burst_rifles.weapons, weapon_definition_index);
}

/* one weapon of a pack set up with its profile's numbers */
static void burst_rifle_setup(
	long weapon_definition_index,
	enum ae_rifle_kind kind)
{
	struct ae_rifle_profile const *profile = ae_rifle_profile(kind);
	struct weapon_definition *weapon_definition = weapon_definition_get(weapon_definition_index);
	char const *name = tag_get_name(weapon_definition_index);
	struct weapon_trigger_definition *trigger_definition;
	long projectile_definition_index = NONE;
	char const *name_of_tag;

	if (!profile || weapon_definition->weapon.triggers.count < 1 || burst_rifles.weapons.count >= BURST_RIFLES_MAXIMUM)
	{
		error(_error_silent, "ae burst rifle: '%s' not set up (%s)", name ? name : "?", !profile ? "no profile" :
			weapon_definition->weapon.triggers.count < 1 ? "no trigger" : "too many burst rifles");
		return;
	}
	trigger_definition = TAG_BLOCK_GET_ELEMENT(&weapon_definition->weapon.triggers, 0, struct weapon_trigger_definition);
	if (profile->tags_rounds)
	{
		/* (the tags' shape the pacing stands on: trigger 0 charges and a second trigger takes the charged shot, so a
		tap fires as the trigger is released; else it is not set up and plays as its tags. A spew (a charged release
		spews instead of firing trigger 1) or a battery's rate of fire penalty (the burst's next rounds come late, and a
		round 3 ticks late ends the burst) is logged) */
		if (trigger_definition->charging_time <= 0.0f || weapon_definition->weapon.triggers.count < 2)
		{
			error(_error_silent, "ae burst rifle: '%s' not set up as the %s: its trigger 0 must charge (%g s) and a second "
				"trigger take the charged shot (%d trigger(s))", name ? name : "?", profile->name,
				(double)trigger_definition->charging_time, (int)weapon_definition->weapon.triggers.count);
			return;
		}
		if (trigger_definition->spew_time > 0.0f || weapon_definition->weapon.age_rate_of_fire_penalty > 0.0f)
		{
			error(_error_silent, "ae burst rifle: '%s' (the %s): its tags spew %g s / slow with the battery x%g: the pacing "
				"assumes neither", name ? name : "?", profile->name, (double)trigger_definition->spew_time,
				(double)weapon_definition->weapon.age_rate_of_fire_penalty);
		}
		trigger_definition->initial_rate_of_fire = ROUNDS_PER_SECOND(profile);
		trigger_definition->final_rate_of_fire = ROUNDS_PER_SECOND(profile);
		/* (the tags' own rounds: only the pacing is ours; its bolt, damage, heat, battery, zoom, aim assist, spread,
		kick and charged shot stay the tags'. No bullet is kept: none of its damage is a rifle round's) */
		ae_rifle_weapons_add(&burst_rifles.weapons, weapon_definition_index, NONE, kind);
		error(_error_silent, "ae burst rifle: '%s' set up as the %s (the tags' rounds: %d a tap, %g a second, recovery %g s; "
			"%d trigger(s), charging %g s, heat %g and battery %g a round)", name ? name : "?", profile->name,
			profile->rounds_a_pull, (double)ROUNDS_PER_SECOND(profile), (double)((real)profile->recovery_ticks / TICKS_PER_SECOND),
			(int)weapon_definition->weapon.triggers.count, (double)trigger_definition->charging_time,
			(double)trigger_definition->heat_generated_per_round, (double)trigger_definition->age_generated_per_round);
		return;
	}
	trigger_definition->initial_rate_of_fire = ROUNDS_PER_SECOND(profile);
	trigger_definition->final_rate_of_fire = ROUNDS_PER_SECOND(profile);
	trigger_definition->projectile_error_angle_lower_bound = DEGREES(profile->error_lower_degrees);
	trigger_definition->projectile_error_angle_upper_bound = DEGREES(profile->error_upper_degrees);
	trigger_definition->projectile_error_inner_cone_angle = 0.0f;
	trigger_definition->runtime_error_acceleration_time = ERROR_RATE_PER_TICK;
	trigger_definition->runtime_error_deceleration_time = ERROR_RATE_PER_TICK;
	trigger_definition->rounds_between_tracers = profile->rounds_between_tracers;
	error(_error_silent, "ae burst rifle: spread %.1f to %.1f deg, inner cone 0, a tracer every %d rounds",
		(double)profile->error_lower_degrees, (double)profile->error_upper_degrees, profile->rounds_between_tracers + 1);

	/* the zoom (R12: the tag's own value is not used; logged where it is not the 3x as seen); none for a profile
	without one (the PDW) */
	if (profile->zoom_seen <= 0.0f)
	{
		if (weapon_definition->weapon.zoom_level_count != 0)
			error(_error_silent, "ae burst rifle: the tag's zoom (%d level(s)) differs: not used",
				(int)weapon_definition->weapon.zoom_level_count);
		error(_error_silent, "ae burst rifle: no zoom");
		weapon_definition->weapon.zoom_level_count = 0;
		weapon_definition->weapon.zoom_magnification_minimum = 1.0f;
		weapon_definition->weapon.zoom_magnification_maximum = 1.0f;
	}
	else
	{
		real seen = DEGREES(UNIT_FIELD_OF_VIEW_DEGREES / profile->zoom_seen);
		real ce_field_of_view = 2.0f * (real)atan(tan(seen * 0.5f) / CAMERA_TANGENT_FACTOR);
		real magnification = DEGREES(UNIT_FIELD_OF_VIEW_DEGREES) / ce_field_of_view;

		if (weapon_definition->weapon.zoom_level_count != 1 ||
			weapon_definition->weapon.zoom_magnification_minimum != profile->zoom_seen ||
			weapon_definition->weapon.zoom_magnification_maximum != profile->zoom_seen)
		{
			error(_error_silent, "ae burst rifle: the tag's zoom (%d level(s) %.2fx-%.2fx) differs: not used",
				(int)weapon_definition->weapon.zoom_level_count, (double)weapon_definition->weapon.zoom_magnification_minimum,
				(double)weapon_definition->weapon.zoom_magnification_maximum);
		}
		error(_error_silent, "ae burst rifle: zoom 1 level %.3fx (%.2fx as seen)", (double)magnification,
			(double)profile->zoom_seen);
		weapon_definition->weapon.zoom_level_count = 1;
		weapon_definition->weapon.zoom_magnification_minimum = magnification;
		weapon_definition->weapon.zoom_magnification_maximum = magnification;
	}

	/* the tag-made burst (CMT's rifle charges for a frame, then spews while each round heats it): off, the
	burst is ours (e3770937) */
	if (trigger_definition->charging_time > 0.0f || trigger_definition->spew_time > 0.0f ||
		TEST_FLAG(trigger_definition->flags, _weapon_trigger_discharging_spews_bit))
	{
		error(_error_silent, "ae burst rifle: '%s' had a burst of its own (charging %g s, spew %g s, heat %g a round): off",
			name ? name : "?", (double)trigger_definition->charging_time, (double)trigger_definition->spew_time,
			(double)trigger_definition->heat_generated_per_round);
		trigger_definition->charging_time = 0.0f;
		trigger_definition->charged_time = 0.0f;
		trigger_definition->overcharged_action = 0;
		trigger_definition->spew_time = 0.0f;
		trigger_definition->heat_generated_per_round = 0.0f;
		SET_FLAG(trigger_definition->flags, _weapon_trigger_discharging_spews_bit, FALSE);
	}

	if (magazine_index_valid(weapon_definition, trigger_definition->magazine_index))
	{
		struct weapon_magazine_definition *magazine_definition = TAG_BLOCK_GET_ELEMENT(&weapon_definition->weapon.magazines,
			trigger_definition->magazine_index, struct weapon_magazine_definition);

		/* (a reload fills the whole magazine: CMT's DMR reloads 12 of its 12, so the EVO RIFLE's and the DMR's 15
		took two reloads, the owner 10-30) */
		magazine_definition->rounds_loaded_maximum = profile->rounds_loaded;
		magazine_definition->rounds_reloaded = profile->rounds_loaded;
		magazine_definition->rounds_total_initial = profile->rounds_total_initial;
		magazine_definition->rounds_total_maximum = profile->rounds_total_maximum;
		error(_error_silent, "ae burst rifle: magazine %d loaded (a reload fills it), %d at first, reserve at most %d",
			profile->rounds_loaded, profile->rounds_total_initial, profile->rounds_total_maximum);
	}

	/* each round's kick (the pack's own firing damage effects only) */
	{
		short effect;

		for (effect = 0; effect < trigger_definition->firing_effects.count; effect++)
		{
			struct burst_rifle_firing_effect *firing_effect =
				TAG_BLOCK_GET_ELEMENT(&trigger_definition->firing_effects, effect, struct burst_rifle_firing_effect);
			long damage_index = firing_effect->damage_effects[0].index;
			struct damage_effect_definition *damage;
			struct vibrate_frequency_definition *high, *low;

			if (damage_index == NONE || !ae_weapon_pack_tag_is_own(damage_index))
				continue;
			damage = damage_effect_definition_get(damage_index);
			memset(&damage->screen_flash, 0, sizeof(damage->screen_flash));
			memset(&damage->camera_impulse, 0, sizeof(damage->camera_impulse));
			memset(&damage->camera_shake, 0, sizeof(damage->camera_shake));
			high = &damage->vibrate.vibrate_frequencies[_vibrate_frequency_high];
			low = &damage->vibrate.vibrate_frequencies[_vibrate_frequency_low];
			high->frequency = KICK_RUMBLE_HIGH;
			high->duration = KICK_RUMBLE_SECONDS;
			low->frequency = KICK_RUMBLE_LOW;
			low->duration = KICK_RUMBLE_SECONDS;
			if (burst_rifles.kick_count < BURST_RIFLES_MAXIMUM * 3)
				burst_rifles.kick_definitions[burst_rifles.kick_count++] = damage_index;
			if (effect == 0)
				error(_error_silent, "ae burst rifle: a round's kick rumble %.2f / %.2f for %.2f s, the view steady",
					(double)high->frequency, (double)low->frequency, (double)high->duration);
		}
	}

	projectile_definition_index = trigger_definition->projectile.index;
	/* (only the pack's own tags are retuned: a bullet or damage effect the pack shares with the map is the map's
	weapons' too) */
	if (projectile_definition_index != NONE && !ae_weapon_pack_tag_is_own(projectile_definition_index))
	{
		name_of_tag = tag_get_name(projectile_definition_index);
		error(_error_silent, "ae burst rifle: the bullet '%s' is the map's, not the pack's: left as it is",
			name_of_tag ? name_of_tag : "?");
	projectile_definition_index = NONE;
	}
	else if (projectile_definition_index != NONE)
	{
		struct projectile_definition *projectile = projectile_definition_get(projectile_definition_index);

		error(_error_silent, "ae burst rifle: bullet was %g wu a tick (%g a second), range %g wu, gravity %g",
			(double)projectile->projectile.initial_velocity, (double)(projectile->projectile.initial_velocity * TICKS_PER_SECOND),
			(double)projectile->projectile.maximum_range, (double)projectile->projectile.air_gravity_scale);
		projectile->projectile.initial_velocity = profile->bullet_speed / TICKS_PER_SECOND;
		projectile->projectile.final_velocity = profile->bullet_final_speed / TICKS_PER_SECOND;
		projectile->projectile.maximum_range = profile->bullet_range;
		projectile->projectile.air_gravity_scale = 0.0f;
		error(_error_silent, "ae burst rifle: bullet now %g wu a second slowing to %g (over %g to %g wu), range %g wu, gravity 0",
			(double)(projectile->projectile.initial_velocity * TICKS_PER_SECOND),
			(double)(projectile->projectile.final_velocity * TICKS_PER_SECOND),
			(double)projectile->projectile.air_damage_range_lower_bound, (double)projectile->projectile.air_damage_range_upper_bound,
			(double)projectile->projectile.maximum_range);

		if (projectile->projectile.impact_damage.index != NONE &&
			!ae_weapon_pack_tag_is_own(projectile->projectile.impact_damage.index))
		{
			name_of_tag = tag_get_name(projectile->projectile.impact_damage.index);
			error(_error_silent, "ae burst rifle: the bullet's damage '%s' is the map's, not the pack's: left as it is",
				name_of_tag ? name_of_tag : "?");
		}
		else if (projectile->projectile.impact_damage.index != NONE)
		{
			struct damage_effect_definition *damage = damage_effect_definition_get(projectile->projectile.impact_damage.index);

			/* (damage.c: a round's damage is the minimum and the bounds mixed by its speed's place between the final and
			the first speed: the minimum is the damage once slowed) */
			damage->damage.damage_minimum = profile->round_damage_slowed;
			damage->damage.damage_lower_bound = profile->round_damage;
			damage->damage.damage_upper_bound = profile->round_damage;
			damage->damage.material_modifiers[_material_cyborg_energy_shield] = profile->shield_modifier;
			damage->damage.material_modifiers[_material_cyborg] = profile->body_modifier;
			error(_error_silent, "ae burst rifle: damage %g a round (%g slowed), shield x%.2f, body x%.2f",
				(double)profile->round_damage, (double)profile->round_damage_slowed, (double)profile->shield_modifier,
				(double)profile->body_modifier);
		}
	}

	{
		struct aim_assist_parameters *aim = &weapon_definition->weapon.aim_assist_parameters;

		error(_error_silent, "ae burst rifle: aim assist was autoaim %.2f deg / %.1f wu, magnetism %.2f deg / %.1f wu, deviation %.2f deg",
			(double)(aim->autoaim_angle * 57.29578f), (double)aim->autoaim_distance, (double)(aim->magnetism_angle * 57.29578f),
			(double)aim->magnetism_distance, (double)(aim->deviation_angle * 57.29578f));
		aim->autoaim_angle = DEGREES(profile->autoaim_degrees);
		aim->autoaim_distance = profile->autoaim_distance;
		aim->magnetism_angle = DEGREES(profile->magnetism_degrees);
		aim->magnetism_distance = profile->magnetism_distance;
		aim->deviation_angle = 0.0f;
		error(_error_silent, "ae burst rifle: aim assist now autoaim %.2f deg / %.1f wu, magnetism %.2f deg / %.1f wu, deviation 0",
			(double)(aim->autoaim_angle * 57.29578f), (double)aim->autoaim_distance, (double)(aim->magnetism_angle * 57.29578f),
			(double)aim->magnetism_distance);
		error(_error_silent, "ae burst rifle: zoomed aim assist ranges at most %.1f wu", (double)profile->bullet_range);
	}

	ae_rifle_weapons_add(&burst_rifles.weapons, weapon_definition_index, projectile_definition_index, kind);
	error(_error_silent, "ae burst rifle: '%s' set up as the %s (%d rounds a pull, %g a second, recovery %g s, damage %g)",
		name ? name : "?", profile->name, profile->rounds_a_pull, (double)ROUNDS_PER_SECOND(profile), (double)((real)profile->recovery_ticks / TICKS_PER_SECOND), (double)profile->round_damage);
}

static boolean burst_rifle_projectile(
	long projectile_definition_index)
{
	short index;

	for (index = 0; index < burst_rifles.weapons.count; index++)
	{
		if (projectile_definition_index != NONE && burst_rifles.weapons.projectiles[index] == projectile_definition_index)
			return TRUE;
	}
	return FALSE;
}

/* (debug.weapon_log) the local player's zoomed view, once a zoom is settled (a second in): its vertical angle as
the frame draws it (main.c: the observer's field of view through display.fov's view, view_fov.c) */
static void zoomed_view_log(
	long weapon_index,
	long unit_index)
{
	struct unit_datum const *unit = unit_get(unit_index);
	short local_player_index;
	struct observer_result const *observer;
	real native, drawn;

	if (unit->unit.player_index == NONE)
		return;
	local_player_index = player_get(unit->unit.player_index)->local_player_index;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	if (ae_burst_rifle_local_player_zoomed(local_player_index, &unit_index) != weapon_index)
	{
		burst_rifles.zoomed_view_ticks[local_player_index] = 0;
		return;
	}
	if (++burst_rifles.zoomed_view_ticks[local_player_index] != TICKS_PER_SECOND)
		return;
	observer = observer_get_camera(local_player_index);
	if (!observer)
		return;
	native = 2.0f * (real)atan(0.75f * render_camera_get_adjusted_field_of_view_tangent(observer->field_of_view));
	drawn = render_fov_vertical(local_player_index, native);
	error(_error_silent, "ae burst rifle: zoomed view %.2f deg vertical (the camera's %.2f deg; display.fov %g)",
		(double)(drawn * 57.29578f), (double)(native * 57.29578f), config_real("display.fov"));
}

/* (the tags' rounds, trigger 0, each tick before its state) the burst's next rounds, round_ticks apart, the player's
trigger held up meanwhile and through the recovery; else the trigger as the player holds it (the tags: a hold charges,
a tap fires as it is released) */
static void burst_tags_rounds_trigger(
	struct weapon_datum *weapon,
	struct burst_state *burst,
	struct ae_rifle_profile const *profile,
	struct weapon_trigger *trigger,
	boolean *trigger_down)
{
	/* (no magazine: its heat and battery; the weapon held and its holder not busy) */
	boolean ready = !TEST_FLAG(weapon->weapon.flags, _weapon_overheated_bit) && weapon->weapon.age < 1.0f &&
		weapon->object.parent_object_index != NONE && !TEST_FLAG(weapon->weapon.control_flags, _weapon_control_user_busy_bit);

	burst->force_round = FALSE;
	burst->trigger_was_down = *trigger_down ? TRUE : FALSE;
	/* (a round the trigger did not fire for a while, underwater or the like: the burst is over) */
	if (burst->rounds_remaining > 0 && (!ready || burst->round_wait < -2))
	{
		burst->rounds_remaining = 0;
		burst->pause_ticks = ready ? burst_recovery_ticks(burst, profile) : BURST_CUT_PAUSE_TICKS;
		if (burst_rifles.weapon_log)
			error(_error_silent, "ae burst rifle: tick %ld, the burst cut short (%s)", (long)game_time_get(),
				ready ? "a round not fired" : "overheated, empty or put away");
	}
	if (burst->rounds_remaining > 0)
	{
		*trigger_down = FALSE;
		if (--burst->round_wait <= 0)
		{
			/* (the trigger's idle state fires it at once: ae_burst_rifle_forced_round) */
			*trigger_down = TRUE;
			burst->force_round = TRUE;
			SET_FLAG(trigger->flags, TRIGGER_RELEASED_SINCE_LAST_SHOT_BIT, TRUE);
		}
	}
	else if (burst->pause_ticks > 0)
	{
		burst->pause_ticks--;
		*trigger_down = FALSE;
	}
}

/* ---------- public code */

void ae_burst_rifle_new_map(
	void)
{
	short slot;
	short weapon;

	memset(&burst_rifles, 0, sizeof(burst_rifles));
	memset(burst_states, 0, sizeof(burst_states));
	memset(&zoomed_particles, 0, sizeof(zoomed_particles));
	zoomed_particles.local_player_index = NONE;
	zoomed_particles.weapon_index = NONE;
	zoomed_particles.unit_index = NONE;
	hit_before.projectile_index = NONE;
	burst_rifles.weapon_log = config_boolean("debug.weapon_log") ? TRUE : FALSE;
	burst_rifles.autofire = config_integer("debug.burst_autofire");
	burst_rifles.hit_log = config_boolean("debug.weapon_hit_log") ? TRUE : FALSE;
	/* (every weapon on the map, by id: the cache's 1-24, ae_weapon_registry.h; each entry's behaviour says which rifle it is,
	whatever its place) */
	for (slot = 1; slot <= AE_WEAPON_REGISTRY_LEGACY_LAST; slot++)
	{
		if (!ae_weapon_pack_slot_added(slot))
			continue;
		for (weapon = 0; weapon < ae_weapon_pack_weapon_count(slot); weapon++)
		{
			struct ae_pack_weapon_record const *entry = ae_weapon_pack_weapon_entry(slot, weapon);
			long definition_index = ae_weapon_pack_weapon_definition(slot, weapon);

			enum ae_rifle_kind kind = entry ? ae_rifle_kind_of_behaviour(entry->behaviour) : AE_RIFLE_NONE;

			if (kind != AE_RIFLE_NONE && definition_index != NONE)
				burst_rifle_setup(definition_index, kind);
		}
	}
}

/* the supercombine thump (the NEEDLER pack): the shooter's pad, both motors, a heavier pulse that dies away over 0.35 s
(the victim's own rumble is the explosion damage effect's). Only for a projectile of a pack's own tag, a local player's */
void ae_burst_rifle_super_thump(
	long owner_player_index,
	long projectile_definition_index)
{
	struct rumble_definition thump;
	short local_player_index;

	if (owner_player_index == NONE || !ae_weapon_pack_tag_is_own(projectile_definition_index))
		return;
	local_player_index = player_get(owner_player_index)->local_player_index;
	if (local_player_index == NONE)
		return;
	memset(&thump, 0, sizeof(thump));
	thump.motors[0].scale = 1.0f;
	thump.motors[0].duration = 0.35f;
	thump.motors[0].transition_function = 3;
	thump.motors[1].scale = 0.6f;
	thump.motors[1].duration = 0.2f;
	thump.motors[1].transition_function = 3;
	rumble_player_impulse(local_player_index, &thump, 1.0f, 1.0f);
}

boolean ae_burst_rifle_definition(
	long weapon_definition_index)
{
	return ae_rifle_weapons_profile(&burst_rifles.weapons, weapon_definition_index) != NULL;
}

struct ae_rifle_profile const *ae_burst_rifle_profile_of(
	long weapon_definition_index)
{
	return profile_of_definition(weapon_definition_index);
}

short ae_burst_rifle_pull_ticks(
	long weapon_definition_index)
{
	struct ae_rifle_profile const *profile = profile_of_definition(weapon_definition_index);

	return (short)ae_rifle_pull_spacing_ticks(profile);
}

boolean ae_burst_rifle_rifle(
	long weapon_definition_index)
{
	struct ae_rifle_profile const *profile = profile_of_definition(weapon_definition_index);

	return profile && profile->rifle;
}

boolean ae_burst_rifle_tags_rounds(
	long weapon_definition_index)
{
	struct ae_rifle_profile const *profile = profile_of_definition(weapon_definition_index);

	return profile && profile->tags_rounds;
}

struct ae_rifle_profile const *ae_burst_rifle_damage_profile(
	long damage_definition_index)
{
	short index;

	for (index = 0; index < burst_rifles.weapons.count && damage_definition_index != NONE; index++)
	{
		long projectile_index = burst_rifles.weapons.projectiles[index];

		if (projectile_index != NONE &&
			projectile_definition_get(projectile_index)->projectile.impact_damage.index == damage_definition_index)
		{
			return ae_rifle_profile((enum ae_rifle_kind)burst_rifles.weapons.kinds[index]);
		}
	}
	return NULL;
}

boolean ae_burst_rifle_damage(
	long damage_definition_index)
{
	short index;

	for (index = 0; index < burst_rifles.weapons.count && damage_definition_index != NONE; index++)
	{
		long projectile_index = burst_rifles.weapons.projectiles[index];

		if (projectile_index != NONE &&
			projectile_definition_get(projectile_index)->projectile.impact_damage.index == damage_definition_index)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* no kick on the firer's view (the battle rifle has none: dd1cf7dd) */
long ae_burst_rifle_kick(
	long weapon_definition_index,
	short trigger_index,
	long damage_effect_index)
{
	short index;

	struct ae_rifle_profile const *profile = profile_of_definition(weapon_definition_index);

	/* (the tags' rounds keep their tags' kick) */
	if (trigger_index != 0 || !profile || profile->tags_rounds)
		return damage_effect_index;
	/* a rumble-only effect (setup cleared its view kick) goes on: the rumble */
	for (index = 0; index < burst_rifles.kick_count; index++)
	{
		if (burst_rifles.kick_definitions[index] == damage_effect_index)
			return damage_effect_index;
	}
	if (damage_effect_index != NONE && burst_rifles.weapon_log && !burst_rifles.kick_logged)
	{
		char const *name = tag_get_name(damage_effect_index);

		burst_rifles.kick_logged = TRUE;
		error(_error_silent, "ae burst rifle: the firer's kick '%s' removed", name ? name : "?");
	}
	return NONE;
}

void ae_burst_rifle_round(
	long weapon_index,
	short trigger_index,
	boolean fired)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct ae_rifle_profile const *profile = profile_of_definition(weapon->definition_index);
	struct burst_state *burst;

	/* (the objective log's shots: each weapon, burst rifle or not) */
	if (fired)
		ae_obj_fired(weapon_index, trigger_index, 1);
	if (!profile || (trigger_index != 0 && !profile->tags_rounds))
		return;
	burst = burst_state_get(weapon_index);
	if (!burst)
		return;
	if (profile->tags_rounds)
	{
		if (trigger_index != 0)
		{
			/* (the charged shot, trigger 1: the recovery after it) */
			if (fired)
			{
				burst->rounds_remaining = 0;
				burst->pause_ticks = burst_recovery_ticks(burst, profile);
				if (burst_rifles.weapon_log)
					error(_error_silent, "ae burst rifle: tick %ld, the charged shot, recovery %d", (long)game_time_get(),
						(int)burst->pause_ticks);
			}
			return;
		}
		if (burst->rounds_remaining <= 0)
		{
			/* (a tap's bolt, fired by the tags as the trigger is released: the burst's first) */
			if (!fired)
				return;
			burst_rifles.bursts_started++;
			burst->rounds_remaining = (char)profile->rounds_a_pull;
		}
		burst->round_wait = (char)profile->round_ticks;
	}
	else if (trigger_index != 0)
		return;
	if (burst->rounds_remaining <= 0)
		return;
	burst->rounds_remaining = fired ? burst->rounds_remaining - 1 : 0;
	if (burst->rounds_remaining == 0)
		burst->pause_ticks = burst_recovery_ticks(burst, profile);
	if (burst_rifles.weapon_log)
	{
		struct weapon_definition const *weapon_definition = weapon_definition_get(weapon->definition_index);
		struct weapon_trigger_definition const *trigger_definition =
			TAG_BLOCK_GET_ELEMENT(&weapon_definition->weapon.triggers, 0, struct weapon_trigger_definition);
		struct weapon_magazine const *magazine = magazine_index_valid(weapon_definition, trigger_definition->magazine_index) ?
			&weapon->weapon.magazines[trigger_definition->magazine_index] : NULL;
		long tick = (long)game_time_get();

		if (profile->tags_rounds)
		{
			short round = (short)(profile->rounds_a_pull - burst->rounds_remaining);

			if (round == 1)
			{
				error(_error_silent, "ae burst rifle: burst at tick %ld, %ld ticks after the last burst's first round",
					tick, burst->burst_start_tick == NONE ? -1L : tick - burst->burst_start_tick);
				burst->burst_start_tick = tick;
			}
			error(_error_silent, "ae burst rifle: tick %ld, round %d of %d %s, heat %.3f, battery used %.3f, recovery %d",
				tick, (int)round, profile->rounds_a_pull, fired ? "fired" : "not fired", (double)weapon->weapon.heat,
				(double)weapon->weapon.age, burst->rounds_remaining == 0 ? (int)burst->pause_ticks : 0);
			return;
		}
		short round = (short)(profile->rounds_a_pull - burst->rounds_remaining);

		if (round == 1)
		{
			error(_error_silent, "ae burst rifle: burst at tick %ld, %ld ticks after the last burst's first round",
				tick, burst->burst_start_tick == NONE ? -1L : tick - burst->burst_start_tick);
			burst->burst_start_tick = tick;
		}
		error(_error_silent, "ae burst rifle: tick %ld, round %d of %d %s, %d loaded, %d in reserve, recovery %d, firing effect %d of %d",
			tick, (int)round, profile->rounds_a_pull, fired ? "fired" : "not fired", magazine ? (int)magazine->rounds_loaded : 0,
			magazine ? (int)magazine->rounds_total : 0, burst->rounds_remaining == 0 ? (int)burst->pause_ticks : 0,
			(int)weapon->weapon.triggers[0].firing_effect_index, (int)trigger_definition->firing_effects.count);
	}
}

void ae_burst_rifle_trigger(
	long weapon_index,
	short trigger_index,
	boolean *trigger_down)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct weapon_definition *weapon_definition;
	struct weapon_trigger_definition *trigger_definition;
	struct weapon_trigger *trigger;
	struct weapon_magazine *magazine = NULL;
	struct ae_rifle_profile const *profile = profile_of_definition(weapon->definition_index);
	struct burst_state *burst;
	boolean ready, pulled;

	if (trigger_index != 0 || !profile)
		return;
	burst = burst_state_get(weapon_index);
	if (!burst)
		return;
	weapon_definition = weapon_definition_get(weapon->definition_index);
	trigger_definition = TAG_BLOCK_GET_ELEMENT(&weapon_definition->weapon.triggers, 0, struct weapon_trigger_definition);
	trigger = &weapon->weapon.triggers[0];
	if (profile->tags_rounds)
	{
		burst_tags_rounds_trigger(weapon, burst, profile, trigger, trigger_down);
		return;
	}
	if (magazine_index_valid(weapon_definition, trigger_definition->magazine_index))
		magazine = &weapon->weapon.magazines[trigger_definition->magazine_index];
	ready = magazine && magazine->state == _magazine_idle && magazine->rounds_loaded > 0 &&
		weapon->object.parent_object_index != NONE && !TEST_FLAG(weapon->weapon.control_flags, _weapon_control_user_busy_bit);

	pulled = *trigger_down && !burst->trigger_was_down;
	burst->trigger_was_down = *trigger_down ? TRUE : FALSE;
	/* (debug.burst_autofire, a test aid, for a local player holding the rifle, only where this machine makes the
	game, a host or no network game: 1 pulls again as soon as the rifle allows, the fastest cadence; 2 pulls once in
	the middle of each recovery, which must be lost, and again AUTOFIRE_LATE_TICKS after the recovery) */
	if (!pulled && burst_rifles.autofire && burst->rounds_remaining == 0 && local_player_unit(weapon->object.parent_object_index))
	{
		boolean honoured = game_connection() != _game_connection_network_client;
		boolean pull = FALSE;

		if (!burst_rifles.autofire_logged)
		{
			burst_rifles.autofire_logged = TRUE;
			error(_error_silent, honoured ? "ae burst rifle: debug.burst_autofire %ld on" :
				"ae burst rifle: debug.burst_autofire %ld ignored: a client in another's game", burst_rifles.autofire);
		}
		if (burst->pause_ticks > 0)
		{
			burst->autofire_idle_ticks = 0;
			if (burst_rifles.autofire == 2 && burst->pause_ticks == AUTOFIRE_EARLY_PAUSE_TICKS)
			{
				pull = TRUE;
				if (burst_rifles.weapon_log)
					error(_error_silent, "ae burst rifle: tick %ld, a pull during the recovery (%d ticks left)",
						(long)game_time_get(), (int)burst->pause_ticks);
			}
		}
		else
		{
			pull = burst_rifles.autofire != 2 || ++burst->autofire_idle_ticks > AUTOFIRE_LATE_TICKS;
		}
		if (honoured && pull)
		{
			pulled = TRUE;
			*trigger_down = TRUE;
		}
	}
	if (!ready)
		burst->pull_held = FALSE;
	if (burst->rounds_remaining > 0 && !ready)
	{
		burst->rounds_remaining = 0;
		burst->pause_ticks = BURST_CUT_PAUSE_TICKS;
	}

	if (burst->rounds_remaining > 0)
	{
		*trigger_down = TRUE;
	}
	else if (burst->pause_ticks > 0)
	{
		/* (a single shot: the owner's "the trigger fights you") a pull during the recovery is kept, not lost, and
		fires as the recovery ends, as Halo's single-shot rifles do; the burst keeps its lost pull */
		if (profile->keeps_pull && pulled)
			burst->pull_held = TRUE;
		burst->pause_ticks--;
		*trigger_down = FALSE;
	}
	else if ((pulled || burst->pull_held) && ready)
	{
		burst->pull_held = FALSE;
		/* (a kept pull: the trigger may be up by now) */
		*trigger_down = TRUE;
		burst_rifles.bursts_started++;
		burst->rounds_remaining = (char)profile->rounds_a_pull;
		/* each burst starts the trigger's firing effects again at the first (bd493899: CMT's three per-round fire
		sounds play in order every burst) */
		if (trigger_definition->firing_effects.count > 0 &&
			!TEST_FLAG(trigger_definition->flags, _weapon_trigger_random_firing_effects_bit))
		{
			trigger->firing_effect_index = (short)(trigger_definition->firing_effects.count - 1);
			trigger->firing_effect_shots_remaining = 0;
			trigger->firing_effects_used_flags = 0;
		}
	}
	else
	{
		*trigger_down = FALSE;
	}

	if (*trigger_down)
		SET_FLAG(trigger->flags, TRIGGER_RELEASED_SINCE_LAST_SHOT_BIT, TRUE);

	if (burst_rifles.weapon_log && local_player_unit(weapon->object.parent_object_index))
		zoomed_view_log(weapon_index, weapon->object.parent_object_index);
}

boolean ae_burst_rifle_forced_round(
	long weapon_index,
	short trigger_index)
{
	struct ae_rifle_profile const *profile;
	struct burst_state *burst;

	if (trigger_index != 0 || !burst_rifles.weapons.count)
		return FALSE;
	profile = profile_of_definition(weapon_get(weapon_index)->definition_index);
	if (!profile || !profile->tags_rounds)
		return FALSE;
	burst = burst_state_get(weapon_index);
	if (!burst || !burst->force_round)
		return FALSE;
	burst->force_round = FALSE;
	return TRUE;
}

void ae_burst_rifle_zoomed_assist(
	long weapon_definition_index,
	short zoom_level,
	real *autoaim_distance,
	real *magnetism_distance)
{
	struct ae_rifle_profile const *profile = profile_of_definition(weapon_definition_index);

	if (zoom_level == NONE || !profile || profile->tags_rounds)
		return;
	if (!burst_rifles.zoomed_assist_logged)
	{
		burst_rifles.zoomed_assist_logged = TRUE;
		error(_error_silent, "ae burst rifle: zoomed aim assist ranges %.1f / %.1f wu, capped at %.1f",
			(double)*autoaim_distance, (double)*magnetism_distance, (double)profile->bullet_range);
	}
	*autoaim_distance = MIN(*autoaim_distance, profile->bullet_range);
	*magnetism_distance = MIN(*magnetism_distance, profile->bullet_range);
}

void ae_burst_rifle_hit_before(
	long projectile_index,
	long object_index)
{
	struct object_datum const *projectile;
	struct object_datum const *victim;

	hit_before.projectile_index = NONE;
	if (!burst_rifles.weapons.count || object_index == NONE || !burst_rifles.hit_log)
		return;
	projectile = object_get(projectile_index);
	if (!burst_rifle_projectile(projectile->definition_index) || projectile->object.owner_player_index == NONE ||
		player_get(projectile->object.owner_player_index)->local_player_index == NONE)
	{
		return;
	}
	victim = object_get(object_index);
	hit_before.projectile_index = projectile_index;
	hit_before.object_index = object_index;
	hit_before.shield = victim->object.shield_vitality;
	hit_before.body = victim->object.body_vitality;
}

void ae_burst_rifle_hit_after(
	long projectile_index,
	long object_index,
	short material_index)
{
	struct object_datum const *victim;
	char const *name;

	if (hit_before.projectile_index == NONE || hit_before.projectile_index != projectile_index ||
		hit_before.object_index != object_index)
	{
		return;
	}
	hit_before.projectile_index = NONE;
	victim = object_get(object_index);
	name = tag_get_name(victim->definition_index);
	error(_error_silent, "ae rifle hit: tick %ld object %08lx (%s) material %d head %d | shield %.4f -> %.4f body %.4f -> %.4f dead %d",
		(long)game_time_get(), (unsigned long)object_index, name ? name : "?", (int)material_index,
		ae_damage_material_is_head(object_index, material_index) ? 1 : 0, (double)hit_before.shield,
		(double)victim->object.shield_vitality, (double)hit_before.body, (double)victim->object.body_vitality,
		TEST_FLAG(victim->object.damage_flags, _object_dead_bit) ? 1 : 0);
}

long ae_burst_rifle_local_player_zoomed(
	short local_player_index,
	long *unit_index)
{
	long player_index;
	struct player_datum *player;
	struct unit_datum *unit;
	long weapon_index;

	*unit_index = NONE;
	if (!burst_rifles.weapons.count || local_player_index == NONE || player_control_get_zoom_level(local_player_index) == NONE)
		return NONE;
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return NONE;
	player = player_get(player_index);
	if (player->unit_index == NONE)
		return NONE;
	unit = unit_get(player->unit_index);
	weapon_index = unit_inventory_get_weapon(player->unit_index, unit->unit.current_weapon_index);
	/* (the engine's zoom only: the tags' rounds keep their tags' zoom and its particles) */
	if (weapon_index == NONE || !profile_of_definition(weapon_get(weapon_index)->definition_index) ||
		profile_of_definition(weapon_get(weapon_index)->definition_index)->tags_rounds)
		return NONE;
	*unit_index = player->unit_index;

	return weapon_index;
}

/* the particle rule while zoomed (86ecf3cb): the prototype's, by the rifle the view's player is zoomed in with */
void ae_burst_rifle_particles_begin(
	short local_player_index)
{
	zoomed_particles.local_player_index = local_player_index;
	zoomed_particles.weapon_index = NONE;
	zoomed_particles.unit_index = NONE;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || !burst_rifles.weapons.count)
		return;
	zoomed_particles.weapon_index = ae_burst_rifle_local_player_zoomed(local_player_index, &zoomed_particles.unit_index);
	/* (a zoom ended: the next one's first hidden particle is logged again) */
	if (zoomed_particles.weapon_index == NONE)
		zoomed_particles.logged[local_player_index] = FALSE;
}

boolean ae_burst_rifle_particle_hidden(
	struct particle_datum const *particle,
	boolean owned_by_local_player)
{
	short local_player_index = zoomed_particles.local_player_index;

	if (zoomed_particles.weapon_index == NONE)
		return FALSE;
	if (particle->object_index != zoomed_particles.weapon_index && particle->object_index != zoomed_particles.unit_index &&
		!(owned_by_local_player && (TEST_FLAG(particle->flags, _particle_datum_dont_draw_third_person_bit) ||
			TEST_FLAG(particle->flags, _particle_datum_attached_to_local_player_bit))))
	{
		return FALSE;
	}
	/* (debug.weapon_log: the first of each zoom, up to 16 a map) */
	if (burst_rifles.weapon_log && !zoomed_particles.logged[local_player_index] && zoomed_particles.logged_count < 16)
	{
		struct particle_definition const *definition = particle_definition_get(particle->definition_index);
		char const *name = definition && definition->bitmap.index != NONE ? tag_get_name(definition->bitmap.index) : NULL;

		zoomed_particles.logged[local_player_index] = TRUE;
		zoomed_particles.logged_count++;
		error(_error_silent, "ae burst rifle: tick %ld, zoomed: local player %d sees no particle of the rifle or its "
			"holder ('%s' left out)", (long)game_time_get(), (int)local_player_index, name ? name : "?");
	}
	return TRUE;
}

/* (damage.c's _damage_material_head_bit: a collision material's flag 0) */
#define DAMAGE_MATERIAL_HEAD_BIT 0

boolean ae_damage_material_is_head(
	long object_index,
	short material_index)
{
	struct object_definition const *definition = object_definition_get(object_get(object_index)->definition_index);
	struct collision_model const *collision_model;

	if (definition->object.collision_model.index == NONE || material_index < 0)
		return FALSE;
	collision_model = collision_model_definition_get(definition->object.collision_model.index);
	if (material_index >= collision_model->resistance.materials.count)
		return FALSE;

	return TEST_FLAG(TAG_BLOCK_GET_ELEMENT(&collision_model->resistance.materials, material_index,
		struct damage_resistance_material)->flags, DAMAGE_MATERIAL_HEAD_BIT) ? TRUE : FALSE;
}

short ae_damage_head_material_index(
	long object_index)
{
	struct object_definition const *definition = object_definition_get(object_get(object_index)->definition_index);
	struct collision_model const *collision_model;
	short index;

	if (definition->object.collision_model.index == NONE)
		return NONE;
	collision_model = collision_model_definition_get(definition->object.collision_model.index);
	for (index = 0; index < collision_model->resistance.materials.count; index++)
	{
		struct damage_resistance_material const *material = TAG_BLOCK_GET_ELEMENT(
			&collision_model->resistance.materials, index, struct damage_resistance_material);

		if (TEST_FLAG(material->flags, DAMAGE_MATERIAL_HEAD_BIT))
			return index;
	}

	return NONE;
}
