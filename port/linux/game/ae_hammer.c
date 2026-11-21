/*
AE_HAMMER.C

The gravity hammer's swing blow and blast push (ae_hammer.h, ae_hammer_rules.h): a swing lands the hammer's blow as
the melee button does, with longer rays and without the blow's recoil on the swinger, and the melee button waits out
the trigger's cycle after a swing; the blast's damage tag pushes by the hammer's rules, on the host and on a
client's replay of the host's damage alike (both come through the damage's aftermath). The blow is every machine's
own, as a melee blow is: a client's hit is its report (network_damage_deals).
*/

#include "cseries.h"
#include "game/game.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "tag_files/tag_files.h"
#include "units/units.h"

#include "ae_hammer.h"
#include "ae_hammer_rules.h"

/* (inside a swing's blow only: the rays' scale until they are cast, and the swing for the whole blow) */
static real ae_hammer_reach_scale = 1.f;
static boolean ae_hammer_swinging = FALSE;

static boolean ae_hammer_weapon(
	long weapon_index)
{
	return weapon_index != NONE &&
		ae_hammer_rules_weapon(tag_get_name(object_get(weapon_index)->definition_index));
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
	ae_hammer_reach_scale = ae_hammer_rules_swing_reach_scale();
	ae_hammer_swinging = TRUE;
	unit_cause_player_melee_damage(owner_object_index);
	ae_hammer_reach_scale = 1.f;
	ae_hammer_swinging = FALSE;
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
