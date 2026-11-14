/*
AE_HEAD_CHECK.C

HEAD CHECK (network.head_check, default on; ae_head_check_rules.h). A client's reported shot names the material its
bullet hit on its own copy of the target, and a head material is a headshot. The host now tests the shot's point against
the head node of its own target: the node's place on the target now, less the target's origin now, put where the host
had the target when the shooter saw it (the report's target_position, which distributed_report_valid already held to the
host's history). A point further from it than the head's size and a little room is a body hit: the material becomes the
target's body material and the shot deals that damage. Explosions and melee blows are not tested (they name no head).

A swap costs the shooter the headshot, not the hit, and says nothing to them; debug.txt gets one line.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "real_math.h"
#include "objects/objects.h"
#include "objects/damage_resistances.h"
#include "physics/collision_model_definitions.h"
#include "units/bipeds.h"
#include "units/biped_definitions.h"

#include "ae_head_check_rules.h"
#include "ae_head_check.h"

void platform_log(const char *format, ...);
int config_boolean(char const *name);
unsigned long config_changes(void);

/* damage.c's _damage_material_head_bit */
#define AE_HEAD_CHECK_MATERIAL_HEAD_BIT 0
/* the most materials a target's collision model is read for */
#define AE_HEAD_CHECK_MAXIMUM_MATERIALS 64

static long ae_head_check_swapped_count;

/* network.head_check, read again when a setting changes */
static boolean ae_head_check_enabled(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean enabled;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		enabled = config_boolean("network.head_check") != 0;
	}
	return enabled;
}

static struct ae_head_check_point ae_head_check_point_from(real_point3d const *point)
{
	struct ae_head_check_point result;

	result.x = point->x;
	result.y = point->y;
	result.z = point->z;
	return result;
}

short ae_head_check_material(short player_index, long object_index, short node_index, short material_index,
	real_point3d const *hit, real_point3d const *seen_position)
{
	struct object_datum *object;
	long collision_model_index;
	struct collision_model *collision_model;
	struct damage_resistance_material const *material;
	real_matrix4x3 const *node;
	real_point3d origin;
	struct ae_head_check_point hit_point, seen, head_offset;
	float below = 0.0f, above = 0.0f;
	int head[AE_HEAD_CHECK_MAXIMUM_MATERIALS], type[AE_HEAD_CHECK_MAXIMUM_MATERIALS];
	int count, index, body;

	if (!ae_head_check_enabled() || material_index == NONE || node_index == NONE)
		return material_index;
	object = (struct object_datum *)object_get(object_index);
	collision_model_index = object_definition_get(object->definition_index)->object.collision_model.index;
	if (collision_model_index == NONE)
		return material_index;
	collision_model = collision_model_definition_get(collision_model_index);
	count = collision_model->resistance.materials.count;
	/* (distributed_damage_indices_valid held the material and node to the object's) */
	if (material_index < 0 || material_index >= count || count > AE_HEAD_CHECK_MAXIMUM_MATERIALS)
		return material_index;
	material = TAG_BLOCK_GET_ELEMENT(&collision_model->resistance.materials, material_index,
		struct damage_resistance_material);
	if (!TEST_FLAG(material->flags, AE_HEAD_CHECK_MATERIAL_HEAD_BIT))
		return material_index;
	/* the head node's place on the target now, from its origin now */
	node = object_get_node_matrix(object_index, node_index);
	object_get_origin(object_index, &origin);
	head_offset.x = node->position.x - origin.x;
	head_offset.y = node->position.y - origin.y;
	head_offset.z = node->position.z - origin.z;
	/* a biped's head moves with its crouch, and the host has its crouch now, not when the shot hit: the head may have
	been as low as crouched or as high as standing (the biped's two camera heights apart) */
	if (object->object.type == _object_type_biped)
	{
		struct biped_datum const *biped = (struct biped_datum const *)object;
		struct biped_definition const *definition = biped_definition_get(object->definition_index);
		real drop = definition->biped.standing_camera_height - definition->biped.crouching_camera_height;
		real crouch = biped->biped.crouch;

		if (!(crouch >= 0.0f))
			crouch = 0.0f;
		if (crouch > 1.0f)
			crouch = 1.0f;
		if (drop > 0.0f && drop < 1.0f)
		{
			above = (float)(crouch * drop);
			below = (float)((1.0f - crouch) * drop);
		}
	}
	hit_point = ae_head_check_point_from(hit);
	seen = ae_head_check_point_from(seen_position);
	if (ae_head_check_on_head_span(&hit_point, &seen, &head_offset, below, above))
		return material_index;
	for (index = 0; index < count; index++)
	{
		struct damage_resistance_material const *each = TAG_BLOCK_GET_ELEMENT(&collision_model->resistance.materials,
			index, struct damage_resistance_material);

		head[index] = TEST_FLAG(each->flags, AE_HEAD_CHECK_MATERIAL_HEAD_BIT) ? 1 : 0;
		type[index] = each->material_type;
	}
	body = ae_head_check_body_material(count, head, type, material_index,
		collision_model->resistance.indirect_damage_material_index);
	if (body < 0)
		return material_index;
	ae_head_check_swapped_count++;
	/* (error, not platform_log: the line reaches the TEST RUN event stream too, for expect lines) */
	error(_error_silent, "head check: player %d's head hit on object %ld is off the head, a body hit (%ld so far)",
		(int)player_index, object_index, ae_head_check_swapped_count);
	return (short)body;
}

long ae_head_check_swaps(void)
{
	return ae_head_check_swapped_count;
}
