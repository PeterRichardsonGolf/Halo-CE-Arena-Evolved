/*
AE_HIT_COVER.C

HIT COVER (network.hit_cover, default on; ae_hit_cover_rules.h). The host already needs a line from the shooter to the
target for a client's reported shot (network_damage.c, distributed_report_line_of_sight); an explosion or a melee blow
had none, so a client's grenade or punch through a wall counted. Now the host tests the lines the rules give against the
level's structure, with the shots' collision flags: the structure's front faces, its invisible, breakable and two-sided
surfaces passed through, and no objects. The game's own area damage (damage.c) is stricter (vehicles, scenery and
machines block it too), so a hit the client's game dealt is never refused for an object its copy did not have in the
way. Vehicle collisions are not tested.

A refusal costs the shooter the hit, as a refused shot does, and says nothing to them; debug.txt gets one line.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "real_math.h"
#include "objects/objects.h"
#include "physics/collisions.h"

#include "ae_hit_cover_rules.h"
#include "ae_hit_cover.h"

void platform_log(const char *format, ...);
int config_boolean(char const *name);
unsigned long config_changes(void);

static long ae_hit_cover_refused_count;

/* network.hit_cover, read again when a setting changes */
static boolean ae_hit_cover_enabled(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean enabled;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		enabled = config_boolean("network.hit_cover") != 0;
	}
	return enabled;
}

static struct ae_hit_cover_point ae_hit_cover_point_from(real_point3d const *point)
{
	struct ae_hit_cover_point result;

	result.x = point->x;
	result.y = point->y;
	result.z = point->z;
	return result;
}

static real_point3d ae_hit_cover_point_to(struct ae_hit_cover_point const *point)
{
	real_point3d result;

	result.x = point->x;
	result.y = point->y;
	result.z = point->z;
	return result;
}

boolean ae_hit_cover_clear(boolean melee, short player_index, real_point3d const *origin,
	real_point3d const *epicenter, long object_index, real_point3d const *seen_position)
{
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_ignore_invisible_surfaces_bit) | FLAG(_collision_test_ignore_breakable_surfaces_bit) |
		FLAG(_collision_test_ignore_two_sided_surfaces_bit);
	struct ae_hit_cover_line lines[AE_HIT_COVER_MAXIMUM_LINES];
	struct ae_hit_cover_point from_origin, from_epicenter, centre;
	struct object_datum *target;
	real_point3d target_origin;
	int blocked[AE_HIT_COVER_MAXIMUM_LINES];
	int count;
	int index;

	if (!ae_hit_cover_enabled())
		return TRUE;
	/* the target's centre where the host had it (as the shots' check has it) */
	target = (struct object_datum *)object_get(object_index);
	object_get_origin(object_index, &target_origin);
	centre.x = seen_position->x + target->object.bounding_sphere_center.x - target_origin.x;
	centre.y = seen_position->y + target->object.bounding_sphere_center.y - target_origin.y;
	centre.z = seen_position->z + target->object.bounding_sphere_center.z - target_origin.z;
	from_origin = ae_hit_cover_point_from(origin);
	from_epicenter = ae_hit_cover_point_from(epicenter);
	count = ae_hit_cover_lines(melee ? 1 : 0, &from_origin, &from_epicenter, &centre, lines);
	for (index = 0; index < count; index++)
	{
		struct collision_result collision;
		real_point3d from = ae_hit_cover_point_to(&lines[index].from);
		real_point3d to = ae_hit_cover_point_to(&lines[index].to);

		blocked[index] = collision_test_line(flags, &from, &to, NONE, &collision) ? 1 : 0;
		if (!blocked[index])
			return TRUE;
	}
	if (!ae_hit_cover_refused(1, count, blocked))
		return TRUE;
	ae_hit_cover_refused_count++;
	/* (error, not platform_log: the line reaches the TEST RUN event stream too, for expect lines) */
	error(_error_silent, "hit cover: player %d's %s at object %ld refused (a wall between; %ld refused so far)",
		(int)player_index, melee ? "melee blow" : "explosion", object_index, ae_hit_cover_refused_count);
	return FALSE;
}

long ae_hit_cover_refusals(void)
{
	return ae_hit_cover_refused_count;
}
