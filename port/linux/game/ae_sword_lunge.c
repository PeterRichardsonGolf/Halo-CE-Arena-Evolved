/*
AE_SWORD_LUNGE.C

The AE energy sword's lunge (ae_sword_lunge.h). A swing (the melee button, or the sword's right trigger, which is
its swing) with an enemy in the lunge's reach, the reticle's red, carries the swinger to him. The carry style (the
default) plays the lunge as the attack and damages on contact; the strike style keeps a lunge followed by a strike.
The right trigger is a swing: ae_sword_lunge_control turns the trigger into the melee button and fires nothing.

THE NUMBERS:
  reach 3.5 units, angle 25 degrees (0.7.5, owner play-test: the latest-gen sword's reach and targeting cone, cut for
    a game without sprint): the sword's autoaim range and angle (its lunge is its autoaim's; CE's reticle turns red at
    level 1, which is inside HALF the autoaim range and angle (aim_assist.c compute_attenuation), so the sword's tag
    gets 7.0 and 50). Red means it will lunge.
  the cone reaches the edge of his body (0.7.6, owner play: point-blank enemies 29-59 degrees off his centre got no
    lunge): 25 degrees plus the angle half a body (0.3) takes up, about 30 at the reach and 60 at arm's length.
  no lunge within 0.9 units; 1.1 s between lunges (a swing in that time is a swing where he stands, 0.7.6);
    magnetism 6 units, 10 degrees.
  the blow 200: one blow kills from full shields (CE's multiplayer cyborg has 150: 75 and 75), every material 1;
    an overshield's 225 more is not.
  the lunge's speed, 15 units a second (the latest-gen's lunge speed): 3.5 units in about 0.25 s; at most 12 ticks.
  it ends when he gets past the reach + 0.5, when it has moved its starting gap + 0.3, or after 12 ticks; held up
    within 0.75 of him (at contact), its blow lands as an arrival's.
Each has an env override for tuning (HALO_AE_LUNGE_RANGE, _ANGLE, _MINIMUM, _SPEED, _COOLDOWN; HALO_AE_SWORD_LUNGE=0
off). HALO_AE_LUNGE_STYLE=carry|strike chooses the order, target turn and end rules (carry by default).

WHICH WEAPONS: the weapon of the cache's energy sword id (the registry's "energy_sword", ae_weapon_ids.h), set up as each map starts
where that pack is added. Never by a pack's behaviour byte. Test aid HALO_AE_LUNGE_ANY_WEAPON=1: every weapon
lunges (box tests with no pack).

The host decides; the lunge is not networked yet (a client's prediction is not told). Logs: each lunge, its end
(ticks, units covered) and the blow's result; HALO_AE_LUNGE_TRACE=1 adds a line a tick. Test aid
HALO_AE_LUNGE_SHOTS=n: at most n screenshots (debug.screenshot_directory) of this machine's players: the reticle red
with a sword, a swing 4 and 14 ticks in, a lunge 4 ticks in, a lunge's blow; HALO_AE_LUNGE_TEST_JUMP=1 jumps first and lunges in the air, HALO_AE_LUNGE_TEST_DROP=<tick> drops the sword then,
HALO_AE_LUNGE_TEST_CLOSE=1 shoots an enemy's sword close up; HALO_AE_LUNGE_TEST_AIM=1 turns this machine's players holding a
sword to the nearest enemy within the reach (any angle), swinging as the reticle turns red, so a box test's
scripted player lunges.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "real_math.h"
#include "tag_files/tag_groups.h"
#include "game/aim_assist.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "interface/first_person_weapons.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "sound/game_sound.h"
#include "objects/damage_effect_definitions.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/bipeds.h"

#include "ae_pack_format.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_ids.h"
#include "ae_sword_lunge.h"
#include "ae_sword_lunge_rules.h"

void platform_log(const char *format, ...);
boolean network_game_distributed_client(void);
long tag_loaded(long group_tag, const char *name);
#ifndef HALO_SERVER
void halo_test_screenshot(void);
#endif

#define DEGREES(x) ((x) * 0.017453292f)

/* ---------- the numbers (the header comment's sources) */

#define LUNGE_RANGE 3.5f
#define LUNGE_ANGLE_DEGREES 25.0f
/* half a body's width, for the cone's reach to his edge (CE's player pill is about 0.2 across the shoulders) */
#define LUNGE_BODY_RADIUS 0.3f
#define LUNGE_MINIMUM 0.9f
#define LUNGE_COOLDOWN_SECONDS 1.1f
#define LUNGE_SPEED 15.0f
#define MAGNETISM_RANGE 6.0f
#define MAGNETISM_ANGLE_DEGREES 10.0f
#define BLOW_DAMAGE 200.0f
/* where the lunge stops: centre to centre (CE's blow reaches 0.8 from the head, units.c
unit_cause_player_melee_damage; two players' pills touch at about 0.4) */
#define STRIKE_DISTANCE 0.6f
/* a lunge held up this near him is in contact: its blow lands */
#define CONTACT_DISTANCE 0.75f
/* the most a lunge moves past its starting gap */
#define BUDGET_SLACK 0.3f
/* a run's speed (units a tick) the lunge slows to when it ends */
#define RUN_PER_TICK (2.25f / 30.f)
/* a lunge held up (a body, a wall) this many ticks in a row strikes where it is */
#define STALL_TICKS 2
#define STALL_PER_TICK 0.03f
/* the target turn: 3 pi radians a second, 18 degrees a tick. */
#define CARRY_TURN_PER_TICK (9.424778f / 30.f)

enum
{
	AE_SWORD_LUNGE_SLOTS = 32,
	AE_SWORD_DEFINITIONS = 8,
};

enum
{
	/* none, or only the cooldown kept */
	_lunge_idle = 0,
	/* carried at the target; the swing waits */
	_lunge_moving,
	/* there: the swing starts next tick (bipeds.c), its blow the tick after */
	_lunge_strike_pending,
	/* the swing running; its blow aims at the target */
	_lunge_striking,
};

static struct
{
	long biped_index;
	long target_index;
	long player_index; /* (the log's: a dead biped's is gone) */
	short state;
	short ticks;
	short stalled;
	boolean lunged; /* (the blow ends a lunge, not a swing at point blank) */
	long next_lunge_time;
	long started_time;
	long strike_time; /* (when it arrived: the swing must start within a few ticks) */
	real_point3d start;
	real_point3d last;
	real budget; /* (the most it may move: the starting gap + BUDGET_SLACK) */
	real target_vitality; /* (the target's before the blow) */
} ae_sword_lunges[AE_SWORD_LUNGE_SLOTS];

static struct
{
	boolean ready;
	boolean trace;
	boolean any_weapon;
	short definition_count;
	long definitions[AE_SWORD_DEFINITIONS];
	long lunges;
	long arrivals;
	long kills;
	long shots_left; /* (the test aids') */
	boolean test_aim;
	boolean test_jump; /* (the test aids': a jump, then the lunge in the air) */
	boolean test_close; /* (a close shot of an enemy sword holder) */
	long test_drop; /* (the tick the sword is dropped at, 0 never) */
	long last_close_shot;
	long dropped_weapon; /* (the drop aid's: the dropped sword, logged where it lies each second) */
	long last_red_shot;
	long swing_shot_unit; /* (the test aids': a plain swing's shots, 4 and 14 ticks into its animation) */
	long swing_shot_at;
	long swing_shots; /* (at most this many: a plain swing's shot would use the whole budget) */
	enum ae_sword_lunge_style style;
} ae_sword;

/* ---------- the pack's sounds (the energy sword pack holds ZTeam's melee sounds and a pitched impact for the clash) */

#define SOUND_GROUP 0x736E6421 /* 'snd!' */
#define LUNGE_SOUND_1 "ae\\packs\\energy_sword\\zteam\\objects\\weapons\\single\\energy_sword\\h2\\sounds\\melee_1"
#define LUNGE_SOUND_2 "ae\\packs\\energy_sword\\zteam\\objects\\weapons\\single\\energy_sword\\h2\\sounds\\melee_2"

/* the pack's sound of that name (once a lunge starts or two swords clash) at a point, heard by whoever is near it as
the sound tag says; nothing when the pack is not on the map */
static void play_pack_sound(char const *name, real_point3d const *position, long object_index, real scale)
{
	long sound_index = tag_loaded(SOUND_GROUP, name);
	struct sound_location location;

	if (sound_index == NONE)
		return;
	location.position = *position;
	location.forward = *global_up3d;
	location.translational_velocity = *global_zero_vector3d;
	location.game_location = object_get(object_index)->object.location;
	unattached_impulse_sound_new(sound_index, &location, scale);
}

static void play_lunge_sound(long unit_index)
{
	real_point3d position;

	object_get_origin(unit_index, &position);
	/* (the two swings of ZTeam's sword, in turn) */
	play_pack_sound((ae_sword.lunges & 1) ? LUNGE_SOUND_2 : LUNGE_SOUND_1, &position, unit_index, 1.0f);
}

static real setting(char const *name, real fallback)
{
	char const *value = getenv(name);

	return value && *value ? (real)atof(value) : fallback;
}

static boolean enabled(void)
{
	char const *value = getenv("HALO_AE_SWORD_LUNGE");

	return !(value && value[0] == '0');
}

static real lunge_range(void) { return setting("HALO_AE_LUNGE_RANGE", LUNGE_RANGE); }
static real lunge_angle(void) { return DEGREES(setting("HALO_AE_LUNGE_ANGLE", LUNGE_ANGLE_DEGREES)); }
static real lunge_minimum(void) { return setting("HALO_AE_LUNGE_MINIMUM", LUNGE_MINIMUM); }
static real lunge_speed(void) { return setting("HALO_AE_LUNGE_SPEED", LUNGE_SPEED); }
static long lunge_cooldown_ticks(void)
{
	return (long)(setting("HALO_AE_LUNGE_COOLDOWN", LUNGE_COOLDOWN_SECONDS) * 30.f + 0.5f);
}

static void reset(void)
{
	short index;

	memset(ae_sword_lunges, 0, sizeof(ae_sword_lunges));
	for (index = 0; index < AE_SWORD_LUNGE_SLOTS; index++)
	{
		ae_sword_lunges[index].biped_index = NONE;
		ae_sword_lunges[index].target_index = NONE;
	}
	ae_sword.ready = TRUE;
	ae_sword.trace = setting("HALO_AE_LUNGE_TRACE", 0.f) != 0.f;
	ae_sword.any_weapon = setting("HALO_AE_LUNGE_ANY_WEAPON", 0.f) != 0.f;
	ae_sword.lunges = ae_sword.arrivals = ae_sword.kills = 0;
	ae_sword.shots_left = (long)setting("HALO_AE_LUNGE_SHOTS", 0.f);
	ae_sword.last_red_shot = NONE;
	ae_sword.swing_shot_unit = NONE;
	ae_sword.swing_shots = 4;
	ae_sword.test_aim = setting("HALO_AE_LUNGE_TEST_AIM", 0.f) != 0.f;
	ae_sword.test_jump = setting("HALO_AE_LUNGE_TEST_JUMP", 0.f) != 0.f;
	ae_sword.test_close = setting("HALO_AE_LUNGE_TEST_CLOSE", 0.f) != 0.f;
	ae_sword.test_drop = (long)setting("HALO_AE_LUNGE_TEST_DROP", 0.f);
	ae_sword.last_close_shot = NONE;
	ae_sword.dropped_weapon = NONE;
	ae_sword.style = ae_sword_lunge_style_from_text(getenv("HALO_AE_LUNGE_STYLE"));
}

/* this machine's player's view turned at a target's middle (the lunge's lock; the test aid's) */
static void face(long unit_index, short local_player_index, long target_index)
{
	real_point3d head, center;
	real radius;
	real_vector3d to;

	unit_get_head_position(unit_index, &head);
	object_get_bounding_sphere(target_index, &center, &radius);
	vector_from_points3d(&head, &center, &to);
	if (normalize3d(&to) > 0.f)
		player_control_set_facing(local_player_index, &to);
}

/* the carry turns the aim and look toward the tracked object, but pins
each tick's yaw and pitch instead of snapping straight to it. */
static void track(long unit_index, short local_player_index, long target_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	real_point3d head, center;
	real radius;
	real_vector3d to, current;
	real_euler_angles2d target_angles, current_angles;

	unit_get_head_position(unit_index, &head);
	object_get_bounding_sphere(target_index, &center, &radius);
	vector_from_points3d(&head, &center, &to);
	current = unit->unit.looking_vector;
	if (normalize3d(&to) == 0.f || normalize3d(&current) == 0.f)
		return;
	euler_angles2d_from_vector3d(&target_angles, &to);
	euler_angles2d_from_vector3d(&current_angles, &current);
	current_angles.yaw = ae_sword_lunge_turn_angle(current_angles.yaw, target_angles.yaw, CARRY_TURN_PER_TICK);
	current_angles.pitch = ae_sword_lunge_turn_angle(current_angles.pitch, target_angles.pitch, CARRY_TURN_PER_TICK);
	vector3d_from_euler_angles2d(&current, &current_angles);
	player_control_set_facing(local_player_index, &current);
}

/* the test aid's screenshot of a moment of this machine's player (HALO_AE_LUNGE_SHOTS) */
static void test_shot(long unit_index, char const *what)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct player_datum *player = unit->unit.player_index != NONE ? player_get(unit->unit.player_index) : NULL;

	if (ae_sword.shots_left <= 0 || !player || player->local_player_index == NONE)
		return;
	ae_sword.shots_left--;
	platform_log("sword lunge: test shot: %s (local player %d)", what, (int)player->local_player_index);
#ifndef HALO_SERVER
	halo_test_screenshot();
#endif
}

/* a pack's sword: its numbers into its tags, as the map starts (every machine the same) */
static void sword_setup(long definition_index)
{
	struct weapon_definition *definition = weapon_definition_get(definition_index);
	struct aim_assist_parameters *aim = &definition->weapon.aim_assist_parameters;
	char const *name = tag_get_name(definition_index);

	if (ae_sword.definition_count >= AE_SWORD_DEFINITIONS)
		return;
	ae_sword.definitions[ae_sword.definition_count++] = definition_index;
	/* (the reticle red where the lunge reaches: CE's autoaim level is 1 inside half its range and angle) */
	aim->autoaim_distance = 2.f * lunge_range();
	aim->autoaim_angle = 2.f * lunge_angle();
	aim->magnetism_distance = MAGNETISM_RANGE;
	aim->magnetism_angle = DEGREES(MAGNETISM_ANGLE_DEGREES);
	if (definition->weapon.melee_attack_damage.index != NONE)
	{
		struct damage_effect_definition *damage =
			damage_effect_definition_get(definition->weapon.melee_attack_damage.index);
		short material;

		damage->damage.damage_minimum = BLOW_DAMAGE;
		damage->damage.damage_lower_bound = BLOW_DAMAGE;
		damage->damage.damage_upper_bound = BLOW_DAMAGE;
		for (material = 0; material < MAXIMUM_NUMBER_OF_MATERIAL_TYPES; material++)
			damage->damage.material_modifiers[material] = 1.f;
	}
	platform_log("sword lunge: '%s' set up: reach %.2f units, %.1f degrees (the reticle red there), magnetism "
		"%.1f units, blow %.0f, %.1f units a second, none within %.2f, %.2f s between lunges, %s style",
		name ? name : "?", (double)lunge_range(), (double)(lunge_angle() / DEGREES(1.f)), (double)MAGNETISM_RANGE,
		(double)BLOW_DAMAGE, (double)lunge_speed(), (double)lunge_minimum(), (double)(lunge_cooldown_ticks() / 30.f),
		ae_sword.style == _ae_sword_lunge_style_carry ? "carry" : "strike");
}

static boolean definition_is_sword(long definition_index)
{
	short index;

	if (definition_index == NONE)
		return FALSE;
	if (ae_sword.any_weapon)
		return TRUE;
	for (index = 0; index < ae_sword.definition_count; index++)
	{
		if (ae_sword.definitions[index] == definition_index)
			return TRUE;
	}
	return FALSE;
}

static boolean holds_sword(long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	long weapon = unit->unit.current_weapon_index != NONE ?
		unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index) : NONE;

	return weapon != NONE && definition_is_sword(object_get(weapon)->definition_index);
}

static boolean alive(long unit_index)
{
	struct object_datum *object = unit_index != NONE ?
		object_try_and_get_and_verify_type(unit_index, _object_mask_unit) : NULL;

	return object && !TEST_FLAG(object->object.damage_flags, _object_dead_bit);
}

static real vitality(long unit_index)
{
	struct object_datum *object = alive(unit_index) ? object_get(unit_index) : NULL;

	return object ? object->object.body_vitality + object->object.shield_vitality : 0.f;
}

/* the test aid: the nearest living enemy within the reach, any angle, faced */
static long test_aim(long unit_index, short local_player_index, real range, boolean sword_only)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct player_datum *own = player_get(unit->unit.player_index);
	boolean teams = game_engine_has_teams();
	struct data_iterator iterator;
	struct player_datum *player;
	real best = range;
	long target = NONE;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real distance;

		if (player == own || player->unit_index == NONE || !alive(player->unit_index) ||
			(teams && player->team_index == own->team_index) ||
			(sword_only && !ae_sword_lunge_unit_has_sword(player->unit_index)))
		{
			continue;
		}
		distance = distance3d(&unit->object.position, &object_get(player->unit_index)->object.position);
		if (distance < best)
		{
			best = distance;
			target = player->unit_index;
		}
	}
	if (target != NONE)
	{
		/* (at his autoaim point, as the lunge measures: his middle may lie outside its angle) */
		real_point3d head, point;
		real_vector3d aim, to;
		real distance, angle;

		unit_get_head_position(unit_index, &head);
		unit_get_aiming_vector(unit_index, &aim);
		if (autoaim_compute_target(target, &head, &aim, unit_index, &point, &to, &distance, &angle))
			player_control_set_facing(local_player_index, &to);
		else
			face(unit_index, local_player_index, target);
	}
	return target;
}

/* the living enemy player in the lunge's reach (the reticle's red: within the range and the angle of his
autoaim point, seen from the head), the nearest to the aim; its distance from the head */
static long find_target(long unit_index, real *distance_out)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct player_datum *own = player_get(unit->unit.player_index);
	boolean teams = game_engine_has_teams();
	struct data_iterator iterator;
	struct player_datum *player;
	real_point3d head;
	real_vector3d aim;
	real range = lunge_range();
	real best_angle = 0.f;
	long best = NONE;

	unit_get_head_position(unit_index, &head);
	unit_get_aiming_vector(unit_index, &aim);
	normalize3d(&aim);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d point;
		real_vector3d direction;
		real distance, angle;

		if (player == own || player->unit_index == NONE || player->unit_index == unit_index ||
			!alive(player->unit_index) || (teams && player->team_index == own->team_index))
		{
			continue;
		}
		if (!autoaim_compute_target(player->unit_index, &head, &aim, unit_index, &point, &direction, &distance,
			&angle) || distance > range || angle > ae_sword_lunge_cone(lunge_angle(), distance, LUNGE_BODY_RADIUS) ||
			(best != NONE && angle >= best_angle))
		{
			continue;
		}
		best = player->unit_index;
		best_angle = angle;
		*distance_out = distance;
	}
	return best;
}

static short slot_of(long biped_index, boolean make)
{
	short index, free_slot = NONE;

	for (index = 0; index < AE_SWORD_LUNGE_SLOTS; index++)
	{
		if (ae_sword_lunges[index].biped_index == biped_index)
			return index;
		if (free_slot == NONE && (ae_sword_lunges[index].biped_index == NONE ||
			(ae_sword_lunges[index].state == _lunge_idle &&
			game_time_get() >= ae_sword_lunges[index].next_lunge_time)))
		{
			free_slot = index;
		}
	}
	if (!make || free_slot == NONE)
		return NONE;
	memset(&ae_sword_lunges[free_slot], 0, sizeof(ae_sword_lunges[free_slot]));
	ae_sword_lunges[free_slot].biped_index = biped_index;
	ae_sword_lunges[free_slot].target_index = NONE;
	return free_slot;
}

static short active_slot(long biped_index)
{
	return ae_sword.ready ? slot_of(biped_index, FALSE) : NONE;
}

/* ---------- public code */

void ae_sword_lunge_new_map(void)
{
	unsigned int id;
	long definition_index;

	reset();
	ae_sword.definition_count = 0;
	/* (by the cache's weapon id, the registry's "energy_sword": this weapon's behaviour byte is 0, as the other packs' are) */
	id = ae_weapon_registry_id_of_word("energy_sword");
	definition_index = id ? ae_weapon_id_definition(id) : NONE;
	if (definition_index != NONE)
		sword_setup(definition_index);
}

boolean ae_sword_lunge_unit_has_sword(long unit_index)
{
	if (!enabled() || unit_index == NONE || !object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
		return FALSE;
	return holds_sword(unit_index);
}

real ae_sword_lunge_reach(long unit_index)
{
	return ae_sword_lunge_unit_has_sword(unit_index) ? lunge_range() : 0.f;
}

boolean ae_sword_lunge_ready(long unit_index)
{
	short slot = active_slot(unit_index);

	return slot == NONE || (ae_sword_lunges[slot].state == _lunge_idle &&
		game_time_get() >= ae_sword_lunges[slot].next_lunge_time);
}

/* the trace's: one line each time a player's weapon slots, current or desired weapon change */
static void inventory_trace(long unit_index)
{
	static long last[AE_SWORD_LUNGE_SLOTS][MAXIMUM_WEAPONS_PER_UNIT + 2];
	static long units[AE_SWORD_LUNGE_SLOTS];
	struct unit_datum *unit = unit_get(unit_index);
	long now[MAXIMUM_WEAPONS_PER_UNIT + 2];
	short slot, index, free_index = NONE;
	char text[320];
	int length = 0;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		now[slot] = unit->unit.weapon_object_indices[slot];
	now[MAXIMUM_WEAPONS_PER_UNIT] = unit->unit.current_weapon_index;
	now[MAXIMUM_WEAPONS_PER_UNIT + 1] = unit->unit.desired_weapon_index;
	for (index = 0; index < AE_SWORD_LUNGE_SLOTS; index++)
	{
		if (units[index] == unit_index + 1)
			break;
		if (free_index == NONE && !units[index])
			free_index = index;
	}
	if (index == AE_SWORD_LUNGE_SLOTS)
	{
		if (free_index == NONE)
			return;
		index = free_index;
		units[index] = unit_index + 1;
		memset(last[index], 0x7F, sizeof(last[index]));
	}
	if (!memcmp(last[index], now, sizeof(now)))
		return;
	/* (a weapon that left its slot: whether its object is still there, and where: dropped, or deleted) */
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		if (last[index][slot] != NONE && last[index][slot] != 0x7F7F7F7F && now[slot] == NONE)
		{
			struct object_datum *left = object_try_and_get(last[index][slot]);

			platform_log("sword lunge: the weapon of slot %d of unit %08lX (%08lX) %s", (int)slot, (unsigned long)unit_index,
				(unsigned long)last[index][slot], left ? (TEST_FLAG(left->object.flags, _object_connected_to_map_bit) ?
				"is in the world" : "is not in the world") : "is deleted");
		}
	}
	memcpy(last[index], now, sizeof(now));
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		if (now[slot] == NONE)
			length += snprintf(text + length, sizeof(text) - (size_t)length, " [%d] none", (int)slot);
		else
		{
			struct weapon_datum *weapon = weapon_get(now[slot]);
			char const *name = tag_get_name(weapon->definition_index);
			char const *last_name = name ? strrchr(name, '\\') : NULL;

			length += snprintf(text + length, sizeof(text) - (size_t)length, " [%d] %s age %.2f rounds %d/%d%s", (int)slot,
				last_name ? last_name + 1 : (name ? name : "?"), (double)weapon->weapon.age,
				(int)weapon->weapon.magazines[0].rounds_loaded, (int)weapon->weapon.magazines[0].rounds_total,
				TEST_FLAG(object_get(now[slot])->object.flags, _object_invisible_bit) ? " (hidden)" : "");
		}
		if (length >= (int)sizeof(text) - 1)
			break;
	}
	platform_log("sword lunge: inventory of unit %08lX (local player %d), tick %ld:%s; current %d, desired %d",
		(unsigned long)unit_index, (int)player_get(unit->unit.player_index)->local_player_index, (long)game_time_get(), text, (int)now[MAXIMUM_WEAPONS_PER_UNIT],
		(int)now[MAXIMUM_WEAPONS_PER_UNIT + 1]);
}

void ae_sword_lunge_control(long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct player_datum *player;

	/* (HALO_AE_LUNGE_TRACE: a player's weapon slots when they change: the weapon in each, which is current and desired,
	each weapon's age and rounds, the way a switch or a drop shows) */
	if (ae_sword.ready && ae_sword.trace && unit->unit.player_index != NONE)
		inventory_trace(unit_index);

	/* (the test aid: a plain swing's shot, 8 ticks into its animation) */
	if (ae_sword.ready && ae_sword.swing_shot_unit == unit_index && game_time_get() >= ae_sword.swing_shot_at)
	{
		ae_sword.swing_shot_unit = NONE;
		if (ae_sword.swing_shots > 0)
		{
			ae_sword.swing_shots--;
			test_shot(unit_index, ae_sword.swing_shots & 1 ? "a swing 4 ticks in" : "a swing 14 ticks in");
			/* (the second shot of a swing: 10 ticks later) */
			if (ae_sword.swing_shots & 1)
			{
				ae_sword.swing_shot_unit = unit_index;
				ae_sword.swing_shot_at = game_time_get() + 10;
			}
		}
	}
	/* (the test aid: this machine's player turned to the nearest enemy in reach) */
	if (ae_sword.ready && ae_sword.test_aim && unit->unit.player_index != NONE && alive(unit_index) &&
		(player = player_get(unit->unit.player_index))->local_player_index != NONE && holds_sword(unit_index))
	{
		real distance;

		test_aim(unit_index, player->local_player_index, lunge_range(), FALSE);
		/* (and it swings as the reticle turns red, its lunge ready; the jump aid jumps first and swings in the air) */
		if (ae_sword_lunge_ready(unit_index) && find_target(unit_index, &distance) != NONE)
		{
			if (!ae_sword.test_jump || TEST_FLAG(biped_get(unit_index)->biped.flags, _biped_airborne_bit))
				SET_FLAG(unit->unit.control_flags, _unit_control_use_equipment_bit, TRUE);
			else
				SET_FLAG(unit->unit.control_flags, _unit_control_jump_bit, TRUE);
		}
	}
	/* (the test aid: the sword dropped at a tick, as a death drops it, to be picked up again) */
	if (ae_sword.ready && ae_sword.test_drop && game_time_get() == ae_sword.test_drop && unit->unit.player_index != NONE &&
		alive(unit_index) && player_get(unit->unit.player_index)->local_player_index != NONE && holds_sword(unit_index))
	{
		platform_log("sword lunge: test aid: player %ld drops the sword", unit->unit.player_index & 0xFFFF);
		ae_sword.dropped_weapon = unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index);
		unit_drop_current_weapon(unit_index, TRUE);
	}
	if (ae_sword.ready && ae_sword.test_drop && ae_sword.dropped_weapon != NONE && game_time_get() > ae_sword.test_drop &&
		(game_time_get() - ae_sword.test_drop) % 30 == 1)
	{
		struct object_datum *lying = object_try_and_get(ae_sword.dropped_weapon);

		if (!lying)
			platform_log("sword lunge: test aid: the dropped sword is gone");
		else
			platform_log("sword lunge: test aid: the dropped sword lies %.2f units from the player (%s)",
				(double)distance3d(&lying->object.position, &unit->object.position),
				TEST_FLAG(lying->object.flags, _object_connected_to_map_bit) ? "in the world" : "held");
	}
	/* (the test aid: this machine's player faces the nearest enemy holding a sword within 4 units and shoots it, at most
	once in 3 s: a bot's sword close up) */
	if (ae_sword.ready && ae_sword.test_close && ae_sword.shots_left > 0 && unit->unit.player_index != NONE &&
		alive(unit_index) && (player = player_get(unit->unit.player_index))->local_player_index != NONE &&
		(ae_sword.last_close_shot == NONE || game_time_get() - ae_sword.last_close_shot > 90) &&
		test_aim(unit_index, player->local_player_index, 4.0f, TRUE) != NONE)
	{
		ae_sword.last_close_shot = game_time_get();
		test_shot(unit_index, "an enemy's sword close up");
	}
	/* (the test aid: the reticle red, a sword in hand, at most a shot every 3 s) */
	if (ae_sword.ready && ae_sword.shots_left > 0 && unit->unit.player_index != NONE && alive(unit_index) &&
		(player = player_get(unit->unit.player_index))->local_player_index != NONE &&
		(ae_sword.last_red_shot == NONE || game_time_get() - ae_sword.last_red_shot > 90) &&
		player_control_get_autoaim_level(player->local_player_index) >= 1.f && ae_sword_lunge_unit_has_sword(unit_index))
	{
		ae_sword.last_red_shot = game_time_get();
		test_shot(unit_index, "the reticle red");
	}
	/* (the sword's right trigger is its swing: the melee button's, and nothing fired) */
	if (unit->unit.player_index == NONE ||
		!TEST_FLAG(unit->unit.control_flags, _unit_control_weapon_primary_trigger_bit) ||
		!ae_sword_lunge_unit_has_sword(unit_index))
	{
		return;
	}
	SET_FLAG(unit->unit.control_flags, _unit_control_weapon_primary_trigger_bit, FALSE);
	SET_FLAG(unit->unit.control_flags, _unit_control_use_equipment_bit, TRUE);
	unit->unit.primary_trigger = 0.f;
}

boolean ae_sword_lunge_strike_now(long biped_index)
{
	short slot = active_slot(biped_index);

	if (slot == NONE || ae_sword_lunges[slot].state != _lunge_strike_pending)
		return FALSE;
	/* (a swing that could not start, a weapon swapped or a death: no blow later) */
	if (game_time_get() - ae_sword_lunges[slot].strike_time > 3)
	{
		ae_sword_lunges[slot].state = _lunge_idle;
		ae_sword_lunges[slot].target_index = NONE;
		return FALSE;
	}
	return TRUE;
}

boolean ae_sword_lunge_begin(long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	real distance = 0.f;
	long target;
	short slot;

	if (!ae_sword.ready)
		reset();
	slot = slot_of(unit_index, FALSE);
	if (slot != NONE && ae_sword_lunges[slot].state == _lunge_moving)
		return TRUE;
	if (slot != NONE && ae_sword_lunges[slot].state == _lunge_strike_pending)
	{
		/* (the swing that starts now is the lunge's blow) */
		ae_sword_lunges[slot].state = _lunge_striking;
		return FALSE;
	}
	if (slot != NONE && ae_sword_lunges[slot].state != _lunge_idle)
	{
		/* (an earlier swing's that never struck) */
		ae_sword_lunges[slot].state = _lunge_idle;
		ae_sword_lunges[slot].target_index = NONE;
	}
	if (!enabled() || unit->unit.player_index == NONE || unit->object.type != _object_type_biped ||
		network_game_distributed_client() || !holds_sword(unit_index))
	{
		return FALSE;
	}
	if (slot != NONE && game_time_get() < ae_sword_lunges[slot].next_lunge_time &&
		ae_sword_lunge_cooldown_blocks_melee(ae_sword.style))
	{
		/* the far-melee timer refuses another sword melee; it does not
		turn one into a point-blank swing. */
		platform_log("sword lunge: player %ld's swing is blocked: lunge not ready",
			unit->unit.player_index & 0xFFFF);
		return TRUE;
	}
	target = find_target(unit_index, &distance);
	if (target == NONE)
	{
		platform_log("sword lunge: player %ld swings, no enemy in reach", unit->unit.player_index & 0xFFFF);
		if (player_get(unit->unit.player_index)->local_player_index != NONE)
		{
			ae_sword.swing_shot_unit = unit_index;
			ae_sword.swing_shot_at = game_time_get() + 4;
		}
		return FALSE;
	}
	slot = slot_of(unit_index, TRUE);
	if (slot == NONE)
		return FALSE;
	ae_sword_lunges[slot].target_index = target;
	ae_sword_lunges[slot].player_index = unit->unit.player_index & 0xFFFF;
	ae_sword_lunges[slot].target_vitality = vitality(target);
	if (distance <= lunge_minimum() || game_time_get() < ae_sword_lunges[slot].next_lunge_time)
	{
		/* (at point blank, or too soon after a lunge: a swing where he stands, its blow aimed at him) */
		ae_sword_lunges[slot].state = _lunge_striking;
		ae_sword_lunges[slot].lunged = FALSE;
		platform_log("sword lunge: player %ld swings at unit %08lX %.2f units away (%s)",
			unit->unit.player_index & 0xFFFF, (unsigned long)target, (double)distance,
			distance <= lunge_minimum() ? "close" : "lunge not ready");
		return FALSE;
	}
	ae_sword_lunges[slot].state = _lunge_moving;
	ae_sword_lunges[slot].lunged = TRUE;
	ae_sword_lunges[slot].stalled = 0;
	ae_sword_lunges[slot].ticks = ae_sword_lunge_duration_ticks(ae_sword.style, distance, lunge_speed());
	ae_sword_lunges[slot].started_time = game_time_get();
	ae_sword_lunges[slot].next_lunge_time = game_time_get() + lunge_cooldown_ticks();
	ae_sword_lunges[slot].start = unit->object.position;
	ae_sword_lunges[slot].last = unit->object.position;
	ae_sword_lunges[slot].budget = distance + BUDGET_SLACK;
	ae_sword.lunges++;
	play_lunge_sound(unit_index);
	/* (first person: the sword's lunge animation: the primary fire message plays its ground lunge, the secondary fire
	message its lunge in the air (the pack's animations table); a swing's own is the melee message's) */
	first_person_weapon_message_from_unit(unit_index, TEST_FLAG(biped_get(unit_index)->biped.flags, _biped_airborne_bit) ?
		_first_person_weapon_message_secondary_fire : _first_person_weapon_message_primary_fire);
	{
		real_point3d own_center, target_center;
		real radius;

		object_get_bounding_sphere(unit_index, &own_center, &radius);
		object_get_bounding_sphere(target, &target_center, &radius);
		platform_log("sword lunge: player %ld lunges %.2f units at unit %08lX (at most %d ticks; centres %.2f apart) %s",
			unit->unit.player_index & 0xFFFF, (double)distance, (unsigned long)target, ae_sword_lunges[slot].ticks,
			(double)distance3d(&own_center, &target_center),
			TEST_FLAG(biped_get(unit_index)->biped.flags, _biped_airborne_bit) ?
			"in the air: the lunge-airborne animation (secondary fire message)" : "on the ground: the lunge animation");
	}
	return TRUE;
}

void ae_sword_lunge_swing(long biped_index)
{
	struct biped_datum *biped = biped_get(biped_index);
	short slot = active_slot(biped_index);

	/* (a lunge's blow lands as its swing starts: the next tick) */
	if (slot != NONE && ae_sword_lunges[slot].state == _lunge_striking && ae_sword_lunges[slot].lunged)
		biped->biped.player_melee_attack_tick = biped->biped.player_melee_ticks;
}

void ae_sword_lunge_melee_damage(long biped_index)
{
	short slot = active_slot(biped_index);
	long target = slot != NONE && ae_sword_lunges[slot].state == _lunge_striking ?
		ae_sword_lunges[slot].target_index : NONE;
	struct unit_datum *unit = unit_get(biped_index);

	if (target != NONE && !alive(biped_index))
	{
		/* (a swinger killed before his blow lands strikes no more: two lunges meeting end with the one who struck
		first, not both dead; a clash, both thrown back, is not made) */
		platform_log("sword lunge: player %ld's blow is lost: he died first", ae_sword_lunges[slot].player_index);
		ae_sword_lunges[slot].state = _lunge_idle;
		ae_sword_lunges[slot].target_index = NONE;
		return;
	}
	if (target != NONE && alive(target))
	{
		/* (the blow aimed at him, as the lunger is turned to his target: CE's blow is a ray from the head) */
		real_point3d head, center;
		real radius;
		real_vector3d to;

		unit_get_head_position(biped_index, &head);
		object_get_bounding_sphere(target, &center, &radius);
		vector_from_points3d(&head, &center, &to);
		if (normalize3d(&to) > 0.f)
			unit->unit.aiming_vector = to;
	}
	unit_cause_player_melee_damage(biped_index);
	if (target != NONE && ae_sword_lunges[slot].lunged)
		test_shot(biped_index, "a lunge's blow");
	if (target != NONE)
	{
		boolean killed = !alive(target);

		if (killed)
			ae_sword.kills++;
		platform_log("sword lunge: player %ld's blow %s unit %08lX (%s; this map: %ld lunges, %ld arrived, %ld kills)",
			unit->unit.player_index & 0xFFFF,
			killed ? "killed" : vitality(target) < ae_sword_lunges[slot].target_vitality ? "hit" : "missed",
			(unsigned long)target, ae_sword_lunges[slot].lunged ? "after a lunge" : "a swing",
			ae_sword.lunges, ae_sword.arrivals, ae_sword.kills);
	}
	if (slot != NONE && ae_sword_lunges[slot].state == _lunge_striking)
	{
		ae_sword_lunges[slot].state = _lunge_idle;
		ae_sword_lunges[slot].target_index = NONE;
	}
}

void ae_sword_lunge_physics(long biped_index, real_vector3d *velocity, real *acceleration_maximum,
	real *airborne_acceleration_maximum)
{
	struct unit_datum *unit;
	real_point3d own_center, target_center, now;
	real radius, distance, step, speed, moved;
	real_vector3d to, target_velocity;
	short slot = active_slot(biped_index);
	char const *ended = NULL;
	enum ae_sword_lunge_end end = _ae_sword_lunge_end_arrived;
	enum ae_sword_lunge_end_action action;

	if (slot == NONE || ae_sword_lunges[slot].state != _lunge_moving)
		return;
	unit = unit_get(biped_index);
	now = unit->object.position;
	moved = distance3d(&now, &ae_sword_lunges[slot].last);
	ae_sword_lunges[slot].last = now;
	if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit) || !alive(ae_sword_lunges[slot].target_index))
	{
		platform_log("sword lunge: player %ld's lunge ended: %s", ae_sword_lunges[slot].player_index,
			TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ? "he died" : "his target died");
		ae_sword_lunges[slot].state = _lunge_idle;
		ae_sword_lunges[slot].target_index = NONE;
		return;
	}
	object_get_bounding_sphere(biped_index, &own_center, &radius);
	object_get_bounding_sphere(ae_sword_lunges[slot].target_index, &target_center, &radius);
	vector_from_points3d(&own_center, &target_center, &to);
	distance = normalize3d(&to);
	speed = lunge_speed() / 30.f;
	/* The carry style tracks at its bounded turn rate; strike keeps a hard lock. */
	if (unit->unit.player_index != NONE && player_get(unit->unit.player_index)->local_player_index != NONE)
	{
		short local_player_index = player_get(unit->unit.player_index)->local_player_index;

		if (ae_sword.style == _ae_sword_lunge_style_carry)
			track(biped_index, local_player_index, ae_sword_lunges[slot].target_index);
		else
			face(biped_index, local_player_index, ae_sword_lunges[slot].target_index);
	}
	if (game_time_get() - ae_sword_lunges[slot].started_time == 4)
		test_shot(biped_index, "a lunge 4 ticks in");
	if (ae_sword.trace)
	{
		platform_log("sword lunge trace: tick %ld moved %.3f (%.3f in all), %.2f from him, velocity was %.3f",
			game_time_get() - ae_sword_lunges[slot].started_time, (double)moved,
			(double)distance3d(&now, &ae_sword_lunges[slot].start), (double)distance, (double)magnitude3d(velocity));
	}
	/* (held up: a tick that moved under a tenth of the lunge's step, twice running, after its first) */
	if (game_time_get() - ae_sword_lunges[slot].started_time > 1 && moved < STALL_PER_TICK)
		ae_sword_lunges[slot].stalled++;
	else
		ae_sword_lunges[slot].stalled = 0;
	{
		struct ae_sword_lunge_tick tick;
		int judged;

		tick.distance = distance;
		tick.strike = STRIKE_DISTANCE;
		tick.contact = CONTACT_DISTANCE;
		tick.reach = lunge_range();
		tick.covered = distance3d(&now, &ae_sword_lunges[slot].start);
		tick.budget = ae_sword_lunges[slot].budget;
		tick.ticks_left = --ae_sword_lunges[slot].ticks;
		tick.stalled = ae_sword_lunges[slot].stalled;
		tick.stall_ticks = STALL_TICKS;
		judged = ae_sword_lunge_judge(&tick);
		if (judged >= 0)
		{
			end = (enum ae_sword_lunge_end)judged;
			ended = end == _ae_sword_lunge_end_arrived ? (distance <= STRIKE_DISTANCE ? "arrived" : "arrived (held up at contact)") :
				end == _ae_sword_lunge_end_timeout ? "ran out of time" :
				end == _ae_sword_lunge_end_stalled ? "was held up" : "lost him";
		}
	}
	if (ended)
	{
		/* there (or as near as it gets): leave no more than a run's speed */
		if (magnitude3d(velocity) > RUN_PER_TICK)
		{
			normalize3d(velocity);
			scale_vector3d(velocity, RUN_PER_TICK, velocity);
		}
		if (ended[0] == 'a')
			ae_sword.arrivals++;
		platform_log("sword lunge: player %ld %s after %ld ticks, %.2f units covered, %.2f from him",
			unit->unit.player_index & 0xFFFF, ended, game_time_get() - ae_sword_lunges[slot].started_time,
			(double)distance3d(&now, &ae_sword_lunges[slot].start), (double)distance);
		action = ae_sword_lunge_end_action(ae_sword.style, end);
		if (action == _ae_sword_lunge_end_action_damage)
		{
			/* The carry style's lunge has no animation damage frame: contact is the attack. */
			ae_sword_lunges[slot].state = _lunge_striking;
			ae_sword_lunge_melee_damage(biped_index);
		}
		else if (action == _ae_sword_lunge_end_action_strike)
		{
			ae_sword_lunges[slot].state = _lunge_strike_pending;
			ae_sword_lunges[slot].strike_time = game_time_get();
		}
		else
		{
			ae_sword_lunges[slot].state = _lunge_idle;
			ae_sword_lunges[slot].target_index = NONE;
		}
		return;
	}
	step = distance - STRIKE_DISTANCE;
	if (step > speed)
		step = speed;
	scale_vector3d(&to, step, velocity);
	if (ae_sword.style == _ae_sword_lunge_style_carry)
	{
		/* Mode 6 gives character physics the target root's velocity as well as
		the relative target point and separation: keep the closing speed when he moves. */
		object_get_velocities(ae_sword_lunges[slot].target_index, &target_velocity, NULL);
		add_vectors3d(velocity, &target_velocity, velocity);
	}
	*acceleration_maximum = 0.f;
	*airborne_acceleration_maximum = 0.f;
}
