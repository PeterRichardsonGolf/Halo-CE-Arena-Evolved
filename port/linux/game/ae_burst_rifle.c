/*
AE_BURST_RIFLE.C

port: weapon pack. The AE Halo 2 battle rifle (ae_burst_rifle.h): the
behaviour the owner approved in the prototype (ae/br-prototype b2b489ac,
played as halo-pack5f with the v6e donor; the weapon pack plan's "Rifle
numbers and look" table and its rulings R7, R12, R13, R14).

Which weapons: the pack on the loaded map (ae_weapon_pack_added), each of its
weapon entries whose behaviour is 1 (AE_PACK_BEHAVIOUR_AE_BATTLE_RIFLE). Never
a tag picked by its name: a map's own battle rifle (CE+ X's) keeps its tags.
The pack's tags are the map's own copy, made again for every map, so the
numbers below are written into them once a map, as the map starts
(weapons_initialize_for_new_map); every machine of a game does the same.

The numbers are constants (R14: no settings or environment tune them, or a
client could tune its own aim assist and zoom). Halo 2's are numbers read
from Halo 2 Vista's shared.map (its battle rifle's weapon tag) and the CC0
decompile's function names (kirklandsig/halo2-decompiled, weapons.cpp); no
game data or code is copied. The owner's own decisions are named as his.

The burst: a NEW pull (the trigger up last tick, down now) starts three
rounds, one every second tick (Halo 2's 15 a second); they go out whatever
the trigger does. The recovery is then counted from the tick after the
burst's last round, whole ticks with the fraction carried from burst to
burst, and a pull during it is lost (Halo 2's barrel clears its fire request
when its recovery ends: function_103dd0). The next burst comes the tick
after a pull once the recovery is over.

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
#include "items/projectile_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/damage_effect_definitions.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "physics/collision_model_definitions.h"
#include "camera/observer.h"
#include "render/render_cameras.h"
#include "units/units.h"
#include "view_fov.h"

#include "ae_pack_format.h"
#include "ae_weapon_pack.h"
#include "ae_burst_rifle.h"

int config_boolean(const char *name);
double config_real(const char *name);

/* ---------- the numbers */

#define DEGREES(x) ((x) * 0.017453292f)

/* the burst: three rounds a pull (Halo 2: the barrel's "maximum shots" 3, function_102100) */
#define BURST_ROUNDS 3
/* 15 rounds a second inside a burst: one every second tick (Halo 2's battle rifle, shared.map barrel 0: rate of
fire 15) */
#define BURST_ROUNDS_PER_SECOND 15.0f
/* the recovery after a burst, 0.15 s (the owner's "Test B" balance, 2026-10-10; Halo 2's fire recovery time is
0.26 s, shared.map barrel 0, started by function_103e60): 4.5 ticks, so the first rounds of successive bursts come
10 and 11 ticks apart */
#define BURST_RECOVERY_SECONDS 0.15f
/* a burst cut short (the rifle reloading, put away, dropped): this many ticks before the next pull counts (the
prototype's) */
#define BURST_CUT_PAUSE_TICKS 8

/* spread: a cone from 0 to 0.3 degrees, held (the owner's decision 6b, 14c96999: Halo 2's 0.3 is the cone's
start, shared.map barrel 0, opening to 0.6 as it fires; held at 0.3 here because CE's small head lets fewer of
Halo 2's rounds land and the 4-burst kill needs all twelve) */
#define ERROR_ANGLE_LOWER DEGREES(0.3f)
#define ERROR_ANGLE_UPPER DEGREES(0.3f)
/* the error rises 10 a second firing and falls 10 a second after (Halo 2's barrel error acceleration /
deceleration, weapons.cpp's unzoomed one-handed case): a third a tick */
#define ERROR_RATE_PER_TICK (10.0f / TICKS_PER_SECOND)
/* a tracer every third round (Halo 2's "rounds between tracers" 2, shared.map) */
#define ROUNDS_BETWEEN_TRACERS 2

/* the magazine: 36 loaded; a fresh rifle 36 + 72; the reserve at most 108 (Halo 2's 36 and 144, shared.map; its
144 counts the loaded magazine, CE's maximum is the reserve alone: 986aa5b7) */
#define MAGAZINE_ROUNDS_LOADED 36
#define MAGAZINE_ROUNDS_TOTAL_INITIAL 108
#define MAGAZINE_ROUNDS_TOTAL_MAXIMUM 108

/* the bullet: 400 world units a second, no drop, 40 world units at most (Halo 2's, shared.map: 12015bf4) */
#define BULLET_SPEED_PER_SECOND 400.0f
#define BULLET_RANGE 40.0f
/* the damage: 6.5 a round (Halo 2's, shared.map); on CE's cyborg shield x1.06 (the owner's "Test B": 11 rounds
empty CE's 75 shield, so the 12th round, the 4th burst's last, can be the killing head shot); on the body x1.5
(the prototype's, as CE's 75 health against Halo 2's) */
#define ROUND_DAMAGE 6.5f
#define SHIELD_MODIFIER 1.06f
#define BODY_MODIFIER 1.5f

/* aim assist: autoaim 3.5 degrees / 17 wu, magnetism 7 degrees / 21 wu, deviation 0 (Halo 2's battle rifle,
shared.map: 3 and 6 degrees, 17 and 21 wu, deviation 0; the angles a little wider for close range on a
controller: the owner's, d8790177) */
#define AUTOAIM_ANGLE DEGREES(3.5f)
#define AUTOAIM_DISTANCE 17.0f
#define MAGNETISM_ANGLE DEGREES(7.0f)
#define MAGNETISM_DISTANCE 21.0f
/* zoomed, the aim assist's ranges (multiplied by the magnification in aim_assist.c) end at the bullet's range:
40 wu (the owner's, b2b489ac: at 3x they were 43.6 and 53.8 wu, past anything a round reaches; R13) */
#define ZOOMED_ASSIST_RANGE_MAXIMUM BULLET_RANGE

/* the zoom: one level, Halo 2-style 3x as SEEN (the owner's choice, 2026-10-10; Halo 2's own is 2x, shared.map),
a constant (R12). Seen means: the cyborg's 70 degrees / 3 = 23.3 degrees wide at 4:3 (17.6 vertical). CE's camera
keeps 0.85 of a field of view's tangent (render_camera_get_adjusted_field_of_view_tangent), so CE's field of view F
is seen as 2 atan(0.85 tan(F/2)): CE needs F = 2 atan(tan(23.3 / 2) / 0.85) = 27.3 degrees, a CE magnification of
70 / 27.3 = 2.563 */
#define ZOOM_SEEN 3.0f
#define UNIT_FIELD_OF_VIEW_DEGREES 70.0f
#define CAMERA_TANGENT_FACTOR 0.85f

/* (weapons.c's enum weapon_trigger_flags, private to it: a weapon_trigger's flag 0) */
#define TRIGGER_RELEASED_SINCE_LAST_SHOT_BIT 0
/* (weapons.c's MAXIMUM_NUMBER_OF_MAGAZINES_PER_WEAPON: a weapon datum's magazines, weapons.h) */
#define WEAPON_MAGAZINES_MAXIMUM 2
typedef char verify_weapon_magazines_maximum[
	sizeof(((struct weapon_datum *)0)->weapon.magazines) / sizeof(struct weapon_magazine) == WEAPON_MAGAZINES_MAXIMUM ? 1 : -1];

/* the burst rifles one map may hold (one pack, its weapons) */
#define BURST_RIFLES_MAXIMUM 4

/* ---------- globals */

static struct
{
	short count;
	long weapon_definitions[BURST_RIFLES_MAXIMUM];
	long projectile_definitions[BURST_RIFLES_MAXIMUM];
	/* (debug.weapon_log, test aids: once a map) */
	boolean zoomed_assist_logged;
	boolean autofire_logged;
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
	char pad;
	/* the recovery's fraction of a tick, carried */
	real recovery_carry;
	/* (debug.weapon_log: the tick of the last burst's first round) */
	long burst_start_tick;
} burst_states[MAXIMUM_OBJECTS_PER_MAP];

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
		state->recovery_carry = 0.0f;
		state->burst_start_tick = NONE;
	}
	return state;
}

/* the recovery's ticks after a burst: 0.15 s in whole ticks, the fraction carried, and the tick the barrel
stops on */
static char burst_recovery_ticks(
	struct burst_state *burst)
{
	real total = BURST_RECOVERY_SECONDS * TICKS_PER_SECOND;
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

/* one weapon of the pack set up with the numbers above */
static void burst_rifle_setup(
	long weapon_definition_index)
{
	struct weapon_definition *weapon_definition = weapon_definition_get(weapon_definition_index);
	char const *name = tag_get_name(weapon_definition_index);
	struct weapon_trigger_definition *trigger_definition;
	long projectile_definition_index = NONE;

	if (weapon_definition->weapon.triggers.count < 1 || burst_rifles.count >= BURST_RIFLES_MAXIMUM)
	{
		error(_error_silent, "ae burst rifle: '%s' not set up (%s)", name ? name : "?",
			weapon_definition->weapon.triggers.count < 1 ? "no trigger" : "too many burst rifles");
		return;
	}
	trigger_definition = TAG_BLOCK_GET_ELEMENT(&weapon_definition->weapon.triggers, 0, struct weapon_trigger_definition);
	trigger_definition->initial_rate_of_fire = BURST_ROUNDS_PER_SECOND;
	trigger_definition->final_rate_of_fire = BURST_ROUNDS_PER_SECOND;
	trigger_definition->projectile_error_angle_lower_bound = ERROR_ANGLE_LOWER;
	trigger_definition->projectile_error_angle_upper_bound = ERROR_ANGLE_UPPER;
	trigger_definition->projectile_error_inner_cone_angle = 0.0f;
	trigger_definition->runtime_error_acceleration_time = ERROR_RATE_PER_TICK;
	trigger_definition->runtime_error_deceleration_time = ERROR_RATE_PER_TICK;
	trigger_definition->rounds_between_tracers = ROUNDS_BETWEEN_TRACERS;
	error(_error_silent, "ae burst rifle: spread %.1f to %.1f deg, inner cone 0, a tracer every %d rounds",
		(double)(ERROR_ANGLE_LOWER / DEGREES(1.0f)), (double)(ERROR_ANGLE_UPPER / DEGREES(1.0f)), ROUNDS_BETWEEN_TRACERS + 1);

	/* the zoom (R12: the tag's own value is not used; logged where it is not Halo 2's 3x as seen) */
	{
		real seen = DEGREES(UNIT_FIELD_OF_VIEW_DEGREES / ZOOM_SEEN);
		real ce_field_of_view = 2.0f * (real)atan(tan(seen * 0.5f) / CAMERA_TANGENT_FACTOR);
		real magnification = DEGREES(UNIT_FIELD_OF_VIEW_DEGREES) / ce_field_of_view;

		if (weapon_definition->weapon.zoom_level_count != 1 ||
			weapon_definition->weapon.zoom_magnification_minimum != ZOOM_SEEN ||
			weapon_definition->weapon.zoom_magnification_maximum != ZOOM_SEEN)
		{
			error(_error_silent, "ae burst rifle: the tag's zoom (%d level(s) %.2fx-%.2fx) differs: not used",
				(int)weapon_definition->weapon.zoom_level_count, (double)weapon_definition->weapon.zoom_magnification_minimum,
				(double)weapon_definition->weapon.zoom_magnification_maximum);
		}
		error(_error_silent, "ae burst rifle: zoom 1 level %.3fx (Halo 2's %.2fx as seen)", (double)magnification,
			(double)ZOOM_SEEN);
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

		magazine_definition->rounds_loaded_maximum = MAGAZINE_ROUNDS_LOADED;
		magazine_definition->rounds_total_initial = MAGAZINE_ROUNDS_TOTAL_INITIAL;
		magazine_definition->rounds_total_maximum = MAGAZINE_ROUNDS_TOTAL_MAXIMUM;
		error(_error_silent, "ae burst rifle: magazine %d loaded, %d at first, reserve at most %d",
			MAGAZINE_ROUNDS_LOADED, MAGAZINE_ROUNDS_TOTAL_INITIAL, MAGAZINE_ROUNDS_TOTAL_MAXIMUM);
	}

	projectile_definition_index = trigger_definition->projectile.index;
	if (projectile_definition_index != NONE)
	{
		struct projectile_definition *projectile = projectile_definition_get(projectile_definition_index);

		error(_error_silent, "ae burst rifle: bullet was %g wu a tick (%g a second), range %g wu, gravity %g",
			(double)projectile->projectile.initial_velocity, (double)(projectile->projectile.initial_velocity * TICKS_PER_SECOND),
			(double)projectile->projectile.maximum_range, (double)projectile->projectile.air_gravity_scale);
		projectile->projectile.initial_velocity = BULLET_SPEED_PER_SECOND / TICKS_PER_SECOND;
		projectile->projectile.final_velocity = BULLET_SPEED_PER_SECOND / TICKS_PER_SECOND;
		projectile->projectile.maximum_range = BULLET_RANGE;
		projectile->projectile.air_gravity_scale = 0.0f;
		error(_error_silent, "ae burst rifle: bullet now %g wu a second, range %g wu, gravity 0",
			(double)(projectile->projectile.initial_velocity * TICKS_PER_SECOND), (double)projectile->projectile.maximum_range);

		if (projectile->projectile.impact_damage.index != NONE)
		{
			struct damage_effect_definition *damage = damage_effect_definition_get(projectile->projectile.impact_damage.index);

			damage->damage.damage_minimum = ROUND_DAMAGE;
			damage->damage.damage_lower_bound = ROUND_DAMAGE;
			damage->damage.damage_upper_bound = ROUND_DAMAGE;
			damage->damage.material_modifiers[_material_cyborg_energy_shield] = SHIELD_MODIFIER;
			damage->damage.material_modifiers[_material_cyborg] = BODY_MODIFIER;
			error(_error_silent, "ae burst rifle: damage %g a round, shield x%.2f, body x%.2f", (double)ROUND_DAMAGE,
				(double)SHIELD_MODIFIER, (double)BODY_MODIFIER);
		}
	}

	{
		struct aim_assist_parameters *aim = &weapon_definition->weapon.aim_assist_parameters;

		error(_error_silent, "ae burst rifle: aim assist was autoaim %.2f deg / %.1f wu, magnetism %.2f deg / %.1f wu, deviation %.2f deg",
			(double)(aim->autoaim_angle * 57.29578f), (double)aim->autoaim_distance, (double)(aim->magnetism_angle * 57.29578f),
			(double)aim->magnetism_distance, (double)(aim->deviation_angle * 57.29578f));
		aim->autoaim_angle = AUTOAIM_ANGLE;
		aim->autoaim_distance = AUTOAIM_DISTANCE;
		aim->magnetism_angle = MAGNETISM_ANGLE;
		aim->magnetism_distance = MAGNETISM_DISTANCE;
		aim->deviation_angle = 0.0f;
		error(_error_silent, "ae burst rifle: aim assist now autoaim %.2f deg / %.1f wu, magnetism %.2f deg / %.1f wu, deviation 0 (Halo 2's)",
			(double)(aim->autoaim_angle * 57.29578f), (double)aim->autoaim_distance, (double)(aim->magnetism_angle * 57.29578f),
			(double)aim->magnetism_distance);
		error(_error_silent, "ae burst rifle: zoomed aim assist ranges at most %.1f wu", (double)ZOOMED_ASSIST_RANGE_MAXIMUM);
	}

	burst_rifles.weapon_definitions[burst_rifles.count] = weapon_definition_index;
	burst_rifles.projectile_definitions[burst_rifles.count] = projectile_definition_index;
	burst_rifles.count++;
	error(_error_silent, "ae burst rifle: '%s' set up (%d rounds a pull, %g a second, recovery %g s, damage %g)",
		name ? name : "?", BURST_ROUNDS, (double)BURST_ROUNDS_PER_SECOND, (double)BURST_RECOVERY_SECONDS, (double)ROUND_DAMAGE);
}

static boolean burst_rifle_projectile(
	long projectile_definition_index)
{
	short index;

	for (index = 0; index < burst_rifles.count; index++)
	{
		if (projectile_definition_index != NONE && burst_rifles.projectile_definitions[index] == projectile_definition_index)
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

/* ---------- public code */

void ae_burst_rifle_new_map(
	void)
{
	short slot = ae_weapon_pack_added();
	short weapon;

	memset(&burst_rifles, 0, sizeof(burst_rifles));
	memset(burst_states, 0, sizeof(burst_states));
	hit_before.projectile_index = NONE;
	if (!slot)
		return;
	for (weapon = 0; weapon < ae_weapon_pack_weapon_count(slot); weapon++)
	{
		struct ae_pack_weapon_record const *entry = ae_weapon_pack_weapon_entry(slot, weapon);
		long definition_index = ae_weapon_pack_weapon_definition(slot, weapon);

		if (entry && entry->behaviour == AE_PACK_BEHAVIOUR_AE_BATTLE_RIFLE && definition_index != NONE)
			burst_rifle_setup(definition_index);
	}
}

boolean ae_burst_rifle_definition(
	long weapon_definition_index)
{
	short index;

	for (index = 0; index < burst_rifles.count; index++)
	{
		if (weapon_definition_index != NONE && burst_rifles.weapon_definitions[index] == weapon_definition_index)
			return TRUE;
	}
	return FALSE;
}

/* no kick on the firer's view (Halo 2's battle rifle has none: dd1cf7dd) */
long ae_burst_rifle_kick(
	long weapon_definition_index,
	short trigger_index,
	long damage_effect_index)
{
	return trigger_index == 0 && ae_burst_rifle_definition(weapon_definition_index) ? NONE : damage_effect_index;
}

void ae_burst_rifle_round(
	long weapon_index,
	short trigger_index,
	boolean fired)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct burst_state *burst;

	if (trigger_index != 0 || !ae_burst_rifle_definition(weapon->definition_index))
		return;
	burst = burst_state_get(weapon_index);
	if (!burst || burst->rounds_remaining <= 0)
		return;
	burst->rounds_remaining = fired ? burst->rounds_remaining - 1 : 0;
	if (burst->rounds_remaining == 0)
		burst->pause_ticks = burst_recovery_ticks(burst);
	if (config_boolean("debug.weapon_log"))
	{
		long tick = (long)game_time_get();
		short round = (short)(BURST_ROUNDS - burst->rounds_remaining);

		if (round == 1)
		{
			error(_error_silent, "ae burst rifle: burst at tick %ld, %ld ticks after the last burst's first round",
				tick, burst->burst_start_tick == NONE ? -1L : tick - burst->burst_start_tick);
			burst->burst_start_tick = tick;
		}
		error(_error_silent, "ae burst rifle: tick %ld, round %d of %d %s, %d loaded, %d in reserve, recovery %d",
			tick, (int)round, BURST_ROUNDS, fired ? "fired" : "not fired", (int)weapon->weapon.magazines[0].rounds_loaded,
			(int)weapon->weapon.magazines[0].rounds_total, burst->rounds_remaining == 0 ? (int)burst->pause_ticks : 0);
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
	struct burst_state *burst;
	boolean ready, pulled;

	if (trigger_index != 0 || !ae_burst_rifle_definition(weapon->definition_index))
		return;
	burst = burst_state_get(weapon_index);
	if (!burst)
		return;
	weapon_definition = weapon_definition_get(weapon->definition_index);
	trigger_definition = TAG_BLOCK_GET_ELEMENT(&weapon_definition->weapon.triggers, 0, struct weapon_trigger_definition);
	trigger = &weapon->weapon.triggers[0];
	if (magazine_index_valid(weapon_definition, trigger_definition->magazine_index))
		magazine = &weapon->weapon.magazines[trigger_definition->magazine_index];
	ready = magazine && magazine->state == _magazine_idle && magazine->rounds_loaded > 0 &&
		weapon->object.parent_object_index != NONE && !TEST_FLAG(weapon->weapon.control_flags, _weapon_control_user_busy_bit);

	pulled = *trigger_down && !burst->trigger_was_down;
	burst->trigger_was_down = *trigger_down ? TRUE : FALSE;
	/* (debug.burst_autofire: a local player holding the rifle pulls again as soon as it may, to measure the
	fastest cadence; only where this machine makes the game, a host or no network game) */
	if (!pulled && burst->rounds_remaining == 0 && burst->pause_ticks == 0 && local_player_unit(weapon->object.parent_object_index) &&
		config_boolean("debug.burst_autofire"))
	{
		boolean honoured = game_connection() != _game_connection_network_client;

		if (!burst_rifles.autofire_logged)
		{
			burst_rifles.autofire_logged = TRUE;
			error(_error_silent, honoured ? "ae burst rifle: debug.burst_autofire on" :
				"ae burst rifle: debug.burst_autofire ignored: a client in another's game");
		}
		if (honoured)
		{
			pulled = TRUE;
			*trigger_down = TRUE;
		}
	}
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
		burst->pause_ticks--;
		*trigger_down = FALSE;
	}
	else if (pulled && ready)
	{
		burst_rifles.bursts_started++;
		burst->rounds_remaining = BURST_ROUNDS;
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

	if (config_boolean("debug.weapon_log") && local_player_unit(weapon->object.parent_object_index))
		zoomed_view_log(weapon_index, weapon->object.parent_object_index);
}

void ae_burst_rifle_zoomed_assist(
	long weapon_definition_index,
	short zoom_level,
	real *autoaim_distance,
	real *magnetism_distance)
{
	if (zoom_level == NONE || !ae_burst_rifle_definition(weapon_definition_index))
		return;
	if (!burst_rifles.zoomed_assist_logged)
	{
		burst_rifles.zoomed_assist_logged = TRUE;
		error(_error_silent, "ae burst rifle: zoomed aim assist ranges %.1f / %.1f wu, capped at %.1f",
			(double)*autoaim_distance, (double)*magnetism_distance, (double)ZOOMED_ASSIST_RANGE_MAXIMUM);
	}
	*autoaim_distance = MIN(*autoaim_distance, ZOOMED_ASSIST_RANGE_MAXIMUM);
	*magnetism_distance = MIN(*magnetism_distance, ZOOMED_ASSIST_RANGE_MAXIMUM);
}

void ae_burst_rifle_hit_before(
	long projectile_index,
	long object_index)
{
	struct object_datum const *projectile;
	struct object_datum const *victim;

	hit_before.projectile_index = NONE;
	if (!burst_rifles.count || object_index == NONE || !config_boolean("debug.weapon_hit_log"))
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
	if (!burst_rifles.count || local_player_index == NONE || player_control_get_zoom_level(local_player_index) == NONE)
		return NONE;
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return NONE;
	player = player_get(player_index);
	if (player->unit_index == NONE)
		return NONE;
	unit = unit_get(player->unit_index);
	weapon_index = unit_inventory_get_weapon(player->unit_index, unit->unit.current_weapon_index);
	if (weapon_index == NONE || !ae_burst_rifle_definition(weapon_get(weapon_index)->definition_index))
		return NONE;
	*unit_index = player->unit_index;

	return weapon_index;
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
