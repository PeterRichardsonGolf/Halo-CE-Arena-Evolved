/*
AE_SHRINK_WEAPONS.C

SHRINK EMPTY WEAPONS (ae_shrink_weapons.h).
*/

#include "cseries.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "../src/port_config.h"

#include "ae_shrink_weapons.h"

enum
{
	/* (the copies, used in turn: a frame's drawing may keep an object's matrices until its transparent parts are
	drawn, so a copy is not reused until this many others have been made) */
	SHRUNK_COPIES = 128,
	SHRUNK_NODES = 32,
};

#define SHRUNK_SCALE 0.6f

static real_matrix4x3 shrunk[SHRUNK_COPIES][SHRUNK_NODES];
static int shrunk_next;

int ae_weapon_is_empty(int magazine_count, const int *rounds, float age)
{
	int index;

	if (magazine_count <= 0)
		return age >= 1.0f;
	for (index = 0; index < magazine_count; index++)
	{
		if (rounds[index] > 0)
			return 0;
	}
	return 1;
}

static boolean shrink_wanted(void)
{
	/* (read again only when a setting has changed: this runs for every object drawn) */
	static unsigned long changes_seen = (unsigned long)-1;
	static boolean on;

	if (config_changes() != changes_seen)
	{
		changes_seen = config_changes();
		on = config_boolean("display.shrink_empty_weapons") != 0;
	}
	return on;
}

real_matrix4x3 const *ae_shrink_empty_weapon(long object_index, real_matrix4x3 const *nodes)
{
	struct weapon_datum *weapon;
	struct weapon_definition const *definition;
	int rounds[2] = { 0, 0 };
	int magazines, index;
	long count;
	real_matrix4x3 *copy;
	real_point3d centre;

	if (!nodes || !shrink_wanted())
		return nodes;
	{
		struct object_datum *object = object_try_and_get(object_index);

		if (!object || object->object.type != _object_type_weapon || object->object.parent_object_index != NONE)
			return nodes;
	}
	weapon = (struct weapon_datum *)object_get(object_index);
	definition = weapon_definition_get(weapon->definition_index);
	if (!definition)
		return nodes;
	magazines = (int)MIN(definition->weapon.magazines.count, 2);
	for (index = 0; index < magazines; index++)
		rounds[index] = weapon->weapon.magazines[index].rounds_loaded + weapon->weapon.magazines[index].rounds_total;
	if (!ae_weapon_is_empty(magazines, rounds, weapon->weapon.age))
		return nodes;
	count = weapon->object.node_matrices.size / (long)sizeof(real_matrix4x3);
	if (count <= 0 || count > SHRUNK_NODES)
		return nodes;
	copy = shrunk[shrunk_next];
	shrunk_next = (shrunk_next + 1) % SHRUNK_COPIES;
	centre = weapon->object.bounding_sphere_center;
	for (index = 0; index < count; index++)
	{
		/* (about the middle: the scale smaller, the place drawn in toward the middle) */
		copy[index] = nodes[index];
		copy[index].scale = nodes[index].scale * SHRUNK_SCALE;
		copy[index].position.x = centre.x + (nodes[index].position.x - centre.x) * SHRUNK_SCALE;
		copy[index].position.y = centre.y + (nodes[index].position.y - centre.y) * SHRUNK_SCALE;
		copy[index].position.z = centre.z + (nodes[index].position.z - centre.z) * SHRUNK_SCALE;
	}
	return copy;
}
