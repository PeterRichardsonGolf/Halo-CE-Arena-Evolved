/*
AE_HAMMER.C

The gravity hammer's swing blow and blast push (ae_hammer.h, ae_hammer_rules.h): a swing lands the hammer's blow as
the melee button does, with longer rays and without the blow's recoil on the swinger, and the melee button waits out
the trigger's cycle after a swing; the blast's damage tag pushes by the hammer's rules, on the host and on a
client's replay of the host's damage alike (both come through the damage's aftermath). The blow is every machine's
own, as a melee blow is: a client's hit is its report (network_damage_deals). With the hammer in hand the reticle is red
when the swing's own rays, a little short, land on a living enemy body; this machine's players' presses and blows are
logged ("gravity hammer: press", "blow", "red on" and "red off": red, the nearest enemy's distance and angle, whether
the blow killed).
*/

#include <math.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "physics/collision_usage.h"
#include "physics/collisions.h"
#include "tag_files/tag_files.h"
#include "units/units.h"

#include "ae_hammer.h"
#include "ae_hammer_rules.h"
#include "ae_melee.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_registry.h"

void platform_log(const char *format, ...);

/* (inside a swing's blow only: the rays' scale until they are cast, and the swing for the whole blow) */
static real ae_hammer_reach_scale = 1.f;
static boolean ae_hammer_swinging = FALSE;

static boolean ae_hammer_weapon(
	long weapon_index)
{
	return weapon_index != NONE &&
		ae_hammer_rules_weapon(tag_get_name(object_get(weapon_index)->definition_index));
}

/* (the hammer's definition on this map, NONE for none) */
static long ae_hammer_definition = NONE;

/* (each local player's: the reticle red last worked out, the enemy the rays found, the trigger held) */
static struct
{
	long unit_index;
	boolean red;
	long target;
	boolean trigger;
} ae_hammer_view[4];

void ae_hammer_new_map(void)
{
	unsigned int id = ae_weapon_registry_id_of_word("gravity_hammer");
	short index;

	ae_hammer_definition = id ? ae_weapon_id_definition(id) : NONE;
	for (index = 0; index < NUMBEROF(ae_hammer_view); index++)
	{
		ae_hammer_view[index].unit_index = NONE;
		ae_hammer_view[index].red = FALSE;
		ae_hammer_view[index].target = NONE;
		ae_hammer_view[index].trigger = FALSE;
	}
}

static boolean alive(long unit_index)
{
	struct object_datum *object = unit_index != NONE ?
		object_try_and_get_and_verify_type(unit_index, _object_mask_unit) : NULL;

	return object && !TEST_FLAG(object->object.damage_flags, _object_dead_bit);
}

static boolean enemies(long unit_index, long other_index)
{
	return game_team_is_enemy(object_get(unit_index)->object.owner_team_index,
		object_get(other_index)->object.owner_team_index);
}

/* the swing's blow as unit_cause_player_melee_damage casts it (its 25 rays from the head, the melee origin rule's
origin, the hammer's reach times share; a part hit counts as its parent, a body before anything else, the nearest
body first): the living enemy body it lands on, NONE for none */
static long blow_target(long unit_index, real share)
{
	struct unit_datum *unit = unit_get(unit_index);
	real_vector3d *facing = &unit->unit.aiming_vector;
	real_vector3d perpendicular, cross, ray;
	real_point3d origin;
	struct collision_result collision;
	long best = NONE;
	real best_t = 0.f;
	long outer, inner;
	real scale = ae_hammer_rules_swing_reach_scale() * share;

	{
		struct object_marker marker;

		object_get_marker_by_name(unit_index, "head", &marker, 1);
		origin = marker.matrix.position;
		ae_melee_ray_origin(unit_index, &origin);
	}
	normalize3d(perpendicular3d(facing, &perpendicular));
	cross_product3d(facing, &perpendicular, &cross);
	if (global_current_collision_user_depth >= MAXIMUM_COLLISION_USER_STACK_DEPTH)
		return NONE;
	global_current_collision_users[global_current_collision_user_depth++] = 8;
	for (outer = -2; outer <= 2; outer++)
	{
		for (inner = -2; inner <= 2; inner++)
		{
			ray.i = (facing->i * 0.8f + (cross.i * (real)inner + (real)outer * perpendicular.i) * 0.1f) * scale;
			ray.j = (facing->j * 0.8f + (cross.j * (real)inner + (real)outer * perpendicular.j) * 0.1f) * scale;
			ray.k = (facing->k * 0.8f + ((real)outer * perpendicular.k + (real)inner * cross.k) * 0.1f) * scale;
			if (collision_test_vector(_collision_test_for_projectiles_flags, &origin, &ray, unit_index, &collision) &&
				collision.type == _collision_result_object)
			{
				long hit = collision.object_index;
				struct object_datum *object = object_get(hit);

				if (object->object.type != _object_type_weapon && object->object.parent_object_index != NONE)
				{
					hit = object->object.parent_object_index;
					object = object_get(hit);
				}
				if (object->object.type == _object_type_biped && (best == NONE || collision.t < best_t))
				{
					best = hit;
					best_t = collision.t;
				}
			}
		}
	}
	--global_current_collision_user_depth;
	return best != NONE && alive(best) && enemies(unit_index, best) ? best : NONE;
}

/* the nearest living enemy body: its distance from the head to its centre, from the swinger's feet to its feet
(level), and its angle off the aim (degrees); NONE for none */
static long nearest_enemy(long unit_index, real *distance, real *level, real *degrees)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct unit_datum *unit = unit_get(unit_index);
	real_point3d head;
	long best = NONE;

	unit_get_head_position(unit_index, &head);
	*distance = *level = *degrees = 0.f;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *other;
		real_vector3d to;
		real length, cosine;

		if (player->unit_index == NONE || player->unit_index == unit_index || !alive(player->unit_index) ||
			!enemies(unit_index, player->unit_index))
		{
			continue;
		}
		other = object_get(player->unit_index);
		vector_from_points3d(&head, &other->object.bounding_sphere_center, &to);
		length = magnitude3d(&to);
		if (best != NONE && length >= *distance)
			continue;
		best = player->unit_index;
		*distance = length;
		*level = (real)sqrt((other->object.position.x - unit->object.position.x) *
			(other->object.position.x - unit->object.position.x) +
			(other->object.position.y - unit->object.position.y) *
			(other->object.position.y - unit->object.position.y));
		cosine = length > 0.0001f ? dot_product3d(&to, &unit->unit.aiming_vector) /
			(length * magnitude3d(&unit->unit.aiming_vector)) : 1.f;
		*degrees = (real)(acos(PIN(cosine, -1.f, 1.f)) * 180.0 / 3.14159265358979);
	}
	return best;
}

/* (enemy, distance, level, degrees: nearest_enemy's, worked out before what is logged happened) */
static void log_view(char const *what, long unit_index, boolean red, long target, long enemy, real distance,
	real level, real degrees, char const *tail)
{
	struct unit_datum *unit = unit_get(unit_index);

	if (enemy == NONE)
	{
		platform_log("gravity hammer: %s: red %s, no living enemy%s", what, red ? "yes" : "no", tail);
		return;
	}
	platform_log("gravity hammer: %s: red %s, rays land on %s, nearest enemy %08lX %.2f units (feet %.2f) %.1f degrees "
		"off the aim (pitch %.1f), at %.2f %.2f%s", what, red ? "yes" : "no", target == enemy ? "it" : target != NONE ?
		"another" : "none", (unsigned long)enemy, (double)distance, (double)level, (double)degrees,
		(double)(asin(PIN(unit->unit.aiming_vector.k, -1.f, 1.f)) * 180.0 / 3.14159265358979),
		(double)unit->object.position.x, (double)unit->object.position.y, tail);
}

unsigned long system_milliseconds(void);

/* when each local player's last red change was logged */
static unsigned long red_logged_at[4];

void ae_hammer_red_level(short local_player_index, real *autoaim_level)
{
	long player_index, unit_index, weapon_index;
	struct unit_datum *unit;
	boolean trigger, was_red;

	if (local_player_index < 0 || local_player_index >= NUMBEROF(ae_hammer_view) || ae_hammer_definition == NONE)
		return;
	player_index = local_player_get_player_index(local_player_index);
	unit_index = player_index != NONE ? player_get(player_index)->unit_index : NONE;
	if (!alive(unit_index))
		return;
	/* (in a seat that aims (a turret, a gunner's), the reticle is the seat's: the hammer neither swings nor colours it) */
	if (unit_get_aiming_unit_index(unit_index) != unit_index)
	{
		ae_hammer_view[local_player_index].unit_index = NONE;
		return;
	}
	unit = unit_get(unit_index);
	weapon_index = unit->unit.current_weapon_index != NONE ?
		unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index) : NONE;
	if (weapon_index == NONE || object_get(weapon_index)->definition_index != ae_hammer_definition)
	{
		ae_hammer_view[local_player_index].unit_index = NONE;
		return;
	}
	was_red = ae_hammer_view[local_player_index].unit_index == unit_index && ae_hammer_view[local_player_index].red;
	ae_hammer_view[local_player_index].unit_index = unit_index;
	ae_hammer_view[local_player_index].target = blow_target(unit_index, ae_hammer_rules_red_reach_share());
	ae_hammer_view[local_player_index].red = ae_hammer_view[local_player_index].target != NONE;
	/* (a change logged at most every half second: a body at the edge turns it on and off as the head bobs) */
	if (ae_hammer_view[local_player_index].red != was_red &&
		system_milliseconds() - red_logged_at[local_player_index] >= 500)
	{
		real distance, level, degrees;

		red_logged_at[local_player_index] = system_milliseconds();
		long enemy;

		enemy = nearest_enemy(unit_index, &distance, &level, &degrees);
		log_view(was_red ? "red off" : "red on", unit_index, ae_hammer_view[local_player_index].red,
			ae_hammer_view[local_player_index].target, enemy, distance, level, degrees, "");
	}
	/* (red: the level CE shows red at; else under it, whatever the tag's autoaim worked out) */
	*autoaim_level = ae_hammer_view[local_player_index].red ? 1.f : MIN(*autoaim_level, 0.99f);
	trigger = TEST_FLAG(unit->unit.control_flags, _unit_control_weapon_primary_trigger_bit) != 0;
	if (trigger && !ae_hammer_view[local_player_index].trigger)
	{
		real distance, level, degrees;
		long enemy = nearest_enemy(unit_index, &distance, &level, &degrees);

		log_view("press", unit_index, ae_hammer_view[local_player_index].red, ae_hammer_view[local_player_index].target,
			enemy, distance, level, degrees, "");
	}
	ae_hammer_view[local_player_index].trigger = trigger;
}

void ae_hammer_swing_fired(
	long weapon_index,
	long owner_object_index)
{
	if (owner_object_index == NONE || !object_try_and_get_and_verify_type(owner_object_index, _object_mask_unit) ||
		!ae_hammer_weapon(weapon_index))
	{
		return;
	}
	{
		short index;
		boolean seen = FALSE, red = FALSE;
		long target = NONE, enemy = NONE;
		real distance = 0.f, level = 0.f, degrees = 0.f;

		/* (logged for this machine's players only: the red their view showed, before the blow) */
		for (index = 0; index < NUMBEROF(ae_hammer_view); index++)
		{
			if (ae_hammer_view[index].unit_index == owner_object_index)
			{
				seen = TRUE;
				red = ae_hammer_view[index].red;
			}
		}
		if (seen)
		{
			target = blow_target(owner_object_index, 1.f);
			enemy = nearest_enemy(owner_object_index, &distance, &level, &degrees);
		}
		ae_hammer_reach_scale = ae_hammer_rules_swing_reach_scale();
		ae_hammer_swinging = TRUE;
		unit_cause_player_melee_damage(owner_object_index);
		ae_hammer_reach_scale = 1.f;
		ae_hammer_swinging = FALSE;
		if (seen)
		{
			/* (the body the blow landed on, else the nearest enemy; on a network client the host deals the damage, so
			this is the client's view right after the blow) */
			long victim = target != NONE ? target : enemy;

			log_view("blow", owner_object_index, red, target, enemy, distance, level, degrees, victim == NONE ? "" :
				alive(victim) ? ", killed no" : ", killed yes");
		}
	}
}

real ae_hammer_melee_reach_scale(void)
{
	return ae_hammer_reach_scale;
}

void ae_hammer_melee_rays_cast(void)
{
	ae_hammer_reach_scale = 1.f;
}

boolean ae_hammer_swing_blow(void)
{
	return ae_hammer_swinging;
}

boolean ae_hammer_melee_blocked(
	long weapon_index)
{
	struct weapon_datum *weapon;
	struct weapon_definition *definition;
	struct weapon_trigger_definition *trigger;
	long since;

	if (!ae_hammer_weapon(weapon_index))
		return FALSE;
	weapon = weapon_get(weapon_index);
	definition = weapon_definition_get(weapon->definition_index);
	if (definition->weapon.triggers.count < 1)
		return FALSE;
	trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, 0, struct weapon_trigger_definition);
	/* (a hammer never swung has no time of its last swing) */
	since = weapon->weapon.game_time_last_fired > 0 ? game_time_get() - weapon->weapon.game_time_last_fired : -1;
	return ae_hammer_rules_melee_blocked(since, trigger->initial_rate_of_fire, (float)TICKS_PER_SECOND) != 0;
}

boolean ae_hammer_blast_pushes(
	long object_index,
	struct damage_data const *damage,
	real_vector3d *acceleration)
{
	struct object_datum const *object;
	float blast[3];
	float center[3];
	float velocity[3];

	if (damage->definition_index == NONE || !ae_hammer_rules_blast(tag_get_name(damage->definition_index)))
		return TRUE;
	object = object_get(object_index);
	blast[0] = damage->epicenter.x;
	blast[1] = damage->epicenter.y;
	blast[2] = damage->epicenter.z;
	center[0] = object->object.bounding_sphere_center.x;
	center[1] = object->object.bounding_sphere_center.y;
	center[2] = object->object.bounding_sphere_center.z;
	if (!ae_hammer_rules_push(blast, center, object->object.bounding_sphere_radius,
		object_definition_get(object->definition_index)->object.acceleration_scale,
		object->object.type == _object_type_biped, velocity))
	{
		return FALSE;
	}
	/* (a second's velocity change, a tick's acceleration) */
	acceleration->i = velocity[0] / TICKS_PER_SECOND;
	acceleration->j = velocity[1] / TICKS_PER_SECOND;
	acceleration->k = velocity[2] / TICKS_PER_SECOND;
	return TRUE;
}
