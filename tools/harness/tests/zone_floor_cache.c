#include "harness.h"
#include <math.h>
/* ae_zone_world.c's floor heights kept per view and ring point: one kept in the previous game (its stamp later than
the new game's clock) is that map's, so it is neither used nor kept as the fallback, as an empty slot */

#include "config.inc"
struct plane { real_vector3d n; };
struct collision_result { real_point3d point; struct plane plane; };
static struct { struct { real_point3d position; } camera; } render;
static long global_current_collision_user_depth;
static long global_current_collision_users[8];
enum { MAXIMUM_COLLISION_USER_STACK_DEPTH = 8, _collision_test_front_facing_surfaces_bit, _collision_test_structure_bit,
	_collision_test_objects_bit, _collision_test_objects_scenery_bit, _collision_test_objects_machines_bit };
static int rays_cast;
static boolean collision_test_vector(unsigned long flags, real_point3d *start, real_vector3d *vector, long ignore,
	struct collision_result *result)
{
	rays_cast++;
	return FALSE;
}
#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	int rays;

	CASE("previous-game-entry")
	{
		/* the last game kept 50.0 here at tick 90000; the new game is at tick 100, its frame has no ray left */
		floors[0][5].z = 50.0f;
		floors[0][5].radius = 3.0f;
		floors[0][5].stamp = 90000;
		rays = 0;
		CHECK(floor_at(0, 5, 1.0f, 2.0f, 7.0f, 3.0f, 100, &rays) == 7.0f, "the previous map's floor height used");
		rays = 1;
		CHECK(floor_at(0, 5, 1.0f, 2.0f, 7.0f, 3.0f, 100, &rays) == 7.0f && rays_cast == 1,
			"a missed ray keeps the previous map's height");
		CHECK(floors[0][5].stamp == 100, "the slot not taken for this game");
		return 0;
	}
	CASE("fresh-entry")
	{
		floors[1][9].z = 12.0f;
		floors[1][9].radius = 3.0f;
		floors[1][9].stamp = 90;
		rays = 1;
		CHECK(floor_at(1, 9, 0.0f, 0.0f, 7.0f, 3.0f, 100, &rays) == 12.0f && rays_cast == 0, "a fresh height not kept");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
