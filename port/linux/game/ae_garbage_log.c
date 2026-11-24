/*
AE_GARBAGE_LOG.C

The garbage collection's dead bodies: a dead biped counts as garbage, and the collection deletes the garbage no
player sees (objects.c's object_visible_to_any_player: in a cluster a player's view reaches, and inside the nearest
player's bounding sphere or within 45 degrees of their aim; in network co-op no further than 30 world units). One
line for each body it deletes, up to AE_GARBAGE_LOG_LINES a run:
"garbage: a body collected: elite, mode active, active garbage 52, nearest player 6.2 wu, 61 degrees off aim,
outside the map 0, ragdoll 1"
*/

#include "cseries.h"
#include "errors.h"
#include "real_math.h"
#include "memory/data.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "tag_files/tag_files.h"

#include <math.h>

#include "ae_garbage_log.h"
#include "biped_ragdoll.h"

enum { AE_GARBAGE_LOG_LINES = 200 };

static long ae_garbage_logged;

void ae_garbage_log_body(long object_index, long active_garbage, short mode)
{
	struct object_datum *object = object_get(object_index);
	real nearest = -1.0f, off_aim = -1.0f;
	long player_index;
	static char const *const modes[] = { "everything", "active", "space" };

	if (!TEST_FLAG(_object_mask_unit, object->object.type) || !TEST_FLAG(object->object.damage_flags, _object_dead_bit))
		return;
	if (ae_garbage_logged >= AE_GARBAGE_LOG_LINES)
		return;
	ae_garbage_logged++;
	for (player_index = data_next_index(player_data, NONE); player_index != NONE;
		player_index = data_next_index(player_data, player_index))
	{
		struct player_datum *player = player_get(player_index);
		real_point3d head;
		real_vector3d to;
		real distance;

		if (player->unit_index == NONE)
			continue;
		unit_get_head_position(player->unit_index, &head);
		distance = normalize3d(vector_from_points3d(&head, &object->object.bounding_sphere_center, &to));
		if (nearest < 0.0f || distance < nearest)
		{
			real cosine = dot_product3d(&unit_get(player->unit_index)->unit.desired_aiming_vector, &to);

			nearest = distance;
			off_aim = acosf(cosine > 1.0f ? 1.0f : cosine < -1.0f ? -1.0f : cosine) * 57.29578f;
		}
	}
	error(_error_silent, "garbage: a body collected: %s, mode %s, active garbage %ld, nearest player %.1f wu, %.0f "
		"degrees off aim, outside the map %d, ragdoll %d",
		tag_name_strip_path(tag_get_name(object->definition_index)),
		mode >= 0 && mode < (short)NUMBEROF(modes) ? modes[mode] : "?", active_garbage, nearest, off_aim,
		TEST_FLAG(object->object.flags, _object_outside_of_map_bit) ? 1 : 0,
		biped_ragdoll_active(object_index) ? 1 : 0);
}
