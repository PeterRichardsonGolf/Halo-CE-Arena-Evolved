/*
AE_CROUCH_AIR.C

See ae_crouch_air.h.
*/

#include <string.h>

#include "cseries.h"
#include "models/model_animation_definitions.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"

#include "ae_crouch_air.h"
#include "ae_crouch_air_rules.h"

static short ae_crouch_air_stand_clip(struct animation_graph *graph, const char *class_label, short class_animation)
{
	short seat_index;

	for (seat_index = 0; seat_index < graph->unit_seats.count; seat_index++)
	{
		struct animation_graph_unit_seat *seat = TAG_BLOCK_GET_ELEMENT(&graph->unit_seats, seat_index,
			struct animation_graph_unit_seat);
		short class_index;

		if (strcmp(seat->label, "stand"))
			continue;
		for (class_index = 0; class_index < seat->weapon_classes.count; class_index++)
		{
			struct animation_graph_weapon_class *weapon_class = TAG_BLOCK_GET_ELEMENT(&seat->weapon_classes, class_index,
				struct animation_graph_weapon_class);

			if (strcmp(weapon_class->label, class_label) || class_animation >= weapon_class->animations.count)
				continue;
			return animation_graph_animation_index_get(&weapon_class->animations)[class_animation].animation_index;
		}
	}
	return NONE;
}

short ae_crouch_air_animation(struct animation_graph *graph, struct animation_graph_unit_seat *seat,
	struct animation_graph_weapon_class *weapon_class, short class_animation, short animation_index)
{
	short stand;

	if (class_animation != _unit_weapon_class_animation_airborne && class_animation != _unit_weapon_class_animation_land_soft &&
		class_animation != _unit_weapon_class_animation_land_hard)
		return animation_index;
	if (!graph || !seat || !weapon_class || !ae_crouch_air_seat(seat->label))
		return animation_index;
	if (animation_index != NONE && animation_index >= 0 && animation_index < graph->animations.count &&
		ae_crouch_air_name_is_own(weapon_class->label,
			TAG_BLOCK_GET_ELEMENT(&graph->animations, animation_index, struct animation)->name))
		return animation_index;
	stand = ae_crouch_air_stand_clip(graph, weapon_class->label, class_animation);
	return stand != NONE ? stand : animation_index;
}
