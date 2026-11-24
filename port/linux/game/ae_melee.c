/*
AE_MELEE.C

MELEE (ae_melee.h): the glue between damage.c and ae_melee_rules.h. The host's damage decides, as all damage.
*/

#include "cseries.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/unit_definitions.h"

#include "ae_melee.h"
#include "ae_melee_rules.h"
#include "ae_variant_ext.h"

/* the shield side's mark for one blow: on whom, in which tick, and the points before the floor; the body side takes
it away. Bound to the victim and the tick, so a blow whose body side never came cannot reach another. */
static long ae_melee_marked_victim = NONE;
static long ae_melee_marked_tick;
static real ae_melee_marked_unraised;

/* the player behind an object, through its parents (as damage.c finds the players the gametype's multiplier is for) */
static long ae_melee_player_from_object_or_parents(long object_index)
{
	while (object_index != NONE)
	{
		if (unit_try_and_get(object_index))
			return player_index_from_unit_index(object_index);
		object_index = object_get(object_index)->object.parent_object_index;
	}
	return NONE;
}

/* the damage multiplier the blow got in damage.c (game_engine_get_damage_multiplier, same players), which the floor
follows; 1 outside a game engine */
static float ae_melee_damage_scale(long victim_index, struct damage_data const *damage)
{
	if (!game_engine_running())
		return 1.0f;
	return (float)game_engine_get_damage_multiplier(ae_melee_player_from_object_or_parents(damage->owner_object_index),
		ae_melee_player_from_object_or_parents(victim_index));
}

static boolean ae_melee_covers(long victim_index, struct damage_data const *damage, boolean is_melee)
{
	struct object_datum *victim;
	boolean player_owner;

	if (!is_melee || !ae_melee_rule_on(ae_variant_ext_game()->melee_rule) || damage == NULL ||
		!object_try_and_get_and_verify_type(victim_index, _object_mask_unit))
	{
		return FALSE;
	}
	victim = object_get(victim_index);
	if (victim->object.parent_object_index != NONE || player_index_from_unit_index(victim_index) == NONE)
		return FALSE;
	player_owner = damage->owner_player_index != NONE;
	if (!player_owner && damage->owner_object_index != NONE &&
		object_try_and_get_and_verify_type(damage->owner_object_index, _object_mask_unit))
	{
		player_owner = player_index_from_unit_index(damage->owner_object_index) != NONE;
	}
	return player_owner;
}

real ae_melee_shield_points(long victim_index, struct damage_data const *damage, boolean is_melee, real points)
{
	ae_melee_marked_victim = NONE;
	if (!ae_melee_covers(victim_index, damage, is_melee))
		return points;
	ae_melee_marked_victim = victim_index;
	ae_melee_marked_tick = game_time_get();
	ae_melee_marked_unraised = points;
	return (real)ae_melee_blow_points(ae_variant_ext_game()->melee_rule, (float)points,
		(float)object_get_maximum_body_vitality(victim_index, FALSE), ae_melee_damage_scale(victim_index, damage));
}

real ae_melee_shield_unraised(long victim_index)
{
	if (ae_melee_marked_victim == victim_index && ae_melee_marked_tick == game_time_get())
		return ae_melee_marked_unraised;
	return 0.f;
}

real ae_melee_body_points(long victim_index, struct damage_data const *damage, boolean is_melee, real fraction,
	real maximum_body)
{
	boolean counted = ae_melee_marked_victim == victim_index && ae_melee_marked_tick == game_time_get();
	int rule = ae_variant_ext_game()->melee_rule;
	float points;

	ae_melee_marked_victim = NONE;
	if (!ae_melee_covers(victim_index, damage, is_melee) || maximum_body <= 0.f)
		return fraction;
	points = (float)(fraction * maximum_body);
	if (ae_melee_body_raise(counted, points))
		points = ae_melee_blow_points(rule, points, (float)maximum_body,
			ae_melee_damage_scale(victim_index, damage));
	if (points > 0.f)
		points = ae_melee_kill_points(points, (float)maximum_body);
	return (real)points / maximum_body;
}

/* a back smack (the owner, 11-09: "only back smacks should insta kill"): the blow's strike point within the cone
behind the victim's look on the ground plane (ae_melee_from_behind), not Halo CE's half plane, which takes in a blow
from the side; a biped only, and not one whose unit is never unsuspecting */
static boolean melee_from_behind(long victim_index, real_point3d const *point)
{
	struct unit_datum *unit = unit_try_and_get(victim_index);
	real_vector3d direction;

	if (!unit || unit->object.type != _object_type_biped ||
		TEST_FLAG(unit_definition_get(unit->definition_index)->unit.flags, _unit_never_unsuspecting_bit))
	{
		return FALSE;
	}
	vector_from_points3d(point, &unit->object.bounding_sphere_center, &direction);
	return ae_melee_from_behind((float)direction.i, (float)direction.j, (float)unit->unit.looking_vector.i,
		(float)unit->unit.looking_vector.j) != 0;
}

boolean ae_melee_force_kill(long victim_index, struct damage_data const *damage, boolean is_melee)
{
	struct object_datum *victim;
	boolean enemies;

	/* the covered check has the rule, the melee, the player owner and a standing (not seated) player victim */
	if (!ae_melee_covers(victim_index, damage, is_melee))
		return FALSE;
	victim = object_get(victim_index);
	if (TEST_FLAG(victim->object.damage_flags, _object_cannot_take_damage_bit))
		return FALSE;
	/* an owner_team_index of NONE counts as not an enemy (stricter than stock) */
	enemies = damage->owner_team_index != NONE &&
		game_team_is_enemy(victim->object.owner_team_index, damage->owner_team_index);
	return ae_melee_kills_from_behind(ae_variant_ext_game()->melee_rule, is_melee, enemies, FALSE,
		melee_from_behind(victim_index, &damage->epicenter)) != 0;
}

void ae_melee_mark_clear(void)
{
	ae_melee_marked_victim = NONE;
}

void ae_melee_ray_origin(long unit_index, real_point3d *origin)
{
	struct unit_datum *unit;
	float out[3];

	if (!ae_melee_rule_on(ae_variant_ext_game()->melee_rule) ||
		!object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
		return;
	unit = unit_get(unit_index);
	ae_melee_origin_from((float)unit->object.position.x, (float)unit->object.position.y, (float)origin->z,
		(float)unit->unit.aiming_vector.i, (float)unit->unit.aiming_vector.j, out);
	origin->x = (real)out[0];
	origin->y = (real)out[1];
	origin->z = (real)out[2];
}
