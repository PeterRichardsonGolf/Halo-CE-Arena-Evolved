/*
AE_BOTS_SQUAD_GRID.INC

Arena Evolved: the campaign AI squad's walkable grid (ae_bots_squad_rules.h, THE WALKABLE GRID), included by
ae_bots_squad.inc. A plan is flooded out from a centre (the player's feet) AE_SQUAD_GRID_A_TICK points a tick; the plan in use
stays until the next is done. Floors and lines are tested against the level, its scenery and its machines (not bodies or
vehicles, which move). A point's floor is kept by its cell (bsp, x, y, z) for AE_SQUAD_GRID_KEEP_TICKS.
*/

#define AE_SQUAD_GRID_CACHE 4096
#define AE_SQUAD_GRID_KEEP_TICKS (10 * TICKS_PER_SECOND)
#define AE_SQUAD_GRID_RAYS (FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) | \
	FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) | FLAG(_collision_test_objects_machines_bit))

struct ae_squad_grid_point
{
	real_point3d floor;
	real flood;      /* the walk from the centre */
	boolean wall;    /* a knee line to a neighbour is blocked: a wall or an object beside it */
};

struct ae_squad_grid_plan
{
	struct ae_squad_grid_point points[AE_SQUAD_GRID_POINTS];
	long count;
	real_point3d centre;
	long bsp;
	long built_tick;
	boolean ready;
};

static struct
{
	struct ae_squad_grid_plan plans[2];
	int current;          /* the plan in use; the other is the one built */
	boolean building;
	real reach;
	long head;            /* the next point of the plan built to flood out from */
	long build_ticks;
	struct
	{
		long ix, iy, iz;
		short bsp;
		byte state;       /* 0 unknown, 1 a floor, 2 none */
		real floor_z;
		long tick;
	} cache[AE_SQUAD_GRID_CACHE];
	long rays;            /* this second's */
	long rays_second;
} ae_squad_grid;

static long ae_squad_grid_bsp(void)
{
	return (long)global_structure_bsp_index_get();
}

/* the floor at x, y near height z: within AE_SQUAD_GRID_RISE + 0.3 below or above, with a biped's headroom */
static boolean ae_squad_grid_floor(real x, real y, real z, real_point3d *floor_at)
{
	struct collision_result collision;
	real_point3d start, above;
	real_vector3d down, up;
	long ix, iy, iz, slot;
	unsigned long key;
	short bsp = (short)ae_squad_grid_bsp();
	long now = game_time_get();

	ae_bots_squad_grid_cell((float)x, (float)y, (float)z, &ix, &iy, &iz);
	key = ae_bots_squad_grid_key(bsp, (float)x, (float)y, (float)z);
	slot = (long)(key % AE_SQUAD_GRID_CACHE);
	if (ae_squad_grid.cache[slot].state && ae_squad_grid.cache[slot].ix == ix && ae_squad_grid.cache[slot].iy == iy &&
		ae_squad_grid.cache[slot].iz == iz && ae_squad_grid.cache[slot].bsp == bsp && now >= ae_squad_grid.cache[slot].tick &&
		now - ae_squad_grid.cache[slot].tick < AE_SQUAD_GRID_KEEP_TICKS)
	{
		floor_at->x = x;
		floor_at->y = y;
		floor_at->z = ae_squad_grid.cache[slot].floor_z;
		return ae_squad_grid.cache[slot].state == 1;
	}
	ae_squad_grid.cache[slot].ix = ix;
	ae_squad_grid.cache[slot].iy = iy;
	ae_squad_grid.cache[slot].iz = iz;
	ae_squad_grid.cache[slot].bsp = bsp;
	ae_squad_grid.cache[slot].tick = now;
	ae_squad_grid.cache[slot].state = 2;
	start.x = x;
	start.y = y;
	start.z = z + AE_SQUAD_GRID_RISE + 0.3f;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -2.0f * (AE_SQUAD_GRID_RISE + 0.3f);
	ae_squad_grid.rays++;
	if (!collision_test_vector(AE_SQUAD_GRID_RAYS, &start, &down, NONE, &collision) || collision.plane.n.k < AE_SQUAD_PLACE_FLAT)
		return FALSE;
	*floor_at = collision.point;
	above = collision.point;
	above.z += 0.1f;
	up.i = 0.0f;
	up.j = 0.0f;
	up.k = AE_SQUAD_PLACE_HEADROOM;
	ae_squad_grid.rays++;
	if (collision_test_vector(AE_SQUAD_GRID_RAYS | FLAG(_collision_test_back_facing_surfaces_bit), &above, &up, NONE, &collision))
		return FALSE;
	ae_squad_grid.cache[slot].state = 1;
	ae_squad_grid.cache[slot].floor_z = floor_at->z;
	return TRUE;
}

/* the knee-high line between two floor points is clear */
static boolean ae_squad_grid_knee_line(real_point3d const *a, real_point3d const *b)
{
	struct collision_result collision;
	real_point3d start = *a;
	real_vector3d way;

	start.z += AE_SQUAD_GRID_KNEE;
	way.i = b->x - a->x;
	way.j = b->y - a->y;
	way.k = b->z - a->z;
	ae_squad_grid.rays++;
	return !collision_test_vector(AE_SQUAD_GRID_RAYS, &start, &way, NONE, &collision);
}

/* a new plan about the centre out to reach; the plan in use stands until it is done */
static void ae_squad_grid_request(real_point3d const *centre, real reach)
{
	struct ae_squad_grid_plan *plan = &ae_squad_grid.plans[1 - ae_squad_grid.current];
	real_point3d floor_at;

	plan->count = 0;
	plan->ready = FALSE;
	plan->centre = *centre;
	plan->bsp = ae_squad_grid_bsp();
	ae_squad_grid.reach = reach;
	ae_squad_grid.head = 0;
	ae_squad_grid.build_ticks = 0;
	ae_squad_grid.building = FALSE;
	if (!ae_squad_grid_floor(centre->x, centre->y, centre->z, &floor_at))
		floor_at = *centre;
	plan->points[0].floor = floor_at;
	plan->points[0].flood = 0.0f;
	plan->points[0].wall = FALSE;
	plan->count = 1;
	ae_squad_grid.building = TRUE;
}

/* the plan's point at the cell of x, y (on the floor near z), NONE none */
static long ae_squad_grid_find(struct ae_squad_grid_plan const *plan, real x, real y, real z)
{
	long ix, iy, iz, index;

	ae_bots_squad_grid_cell((float)x, (float)y, 0.0f, &ix, &iy, &iz);
	for (index = 0; index < plan->count; index++)
	{
		long jx, jy, jz;

		ae_bots_squad_grid_cell((float)plan->points[index].floor.x, (float)plan->points[index].floor.y, 0.0f, &jx, &jy, &jz);
		if (jx == ix && jy == iy && fabs(plan->points[index].floor.z - z) <= AE_SQUAD_GRID_RISE * 2.0f)
			return index;
	}
	return NONE;
}

/* the plan built floods out AE_SQUAD_GRID_A_TICK neighbours tested a tick */
static void ae_squad_grid_service(long now)
{
	static const signed char steps[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
	struct ae_squad_grid_plan *plan = &ae_squad_grid.plans[1 - ae_squad_grid.current];
	long tested = 0;

	if (now % TICKS_PER_SECOND == 0)
	{
		ae_squad_grid.rays_second = ae_squad_grid.rays;
		/* (the busiest second's rays) */
		ae_squad.stats[AE_SQUAD_STAT_RAYS] = MAX(ae_squad.stats[AE_SQUAD_STAT_RAYS], ae_squad_grid.rays);
		ae_squad_grid.rays = 0;
	}
	if (!ae_squad_grid.building)
		return;
	ae_squad_grid.build_ticks++;
	while (ae_squad_grid.head < plan->count && tested < AE_SQUAD_GRID_A_TICK)
	{
		struct ae_squad_grid_point *from = &plan->points[ae_squad_grid.head];
		int step;

		for (step = 0; step < 8; step++)
		{
			real x = from->floor.x + steps[step][0] * AE_SQUAD_GRID_STEP, y = from->floor.y + steps[step][1] * AE_SQUAD_GRID_STEP;
			real length = step < 4 ? AE_SQUAD_GRID_STEP : AE_SQUAD_GRID_STEP * 1.41421f;
			real_point3d floor_at;
			boolean clear;

			if (from->flood + length > ae_squad_grid.reach || plan->count >= AE_SQUAD_GRID_POINTS)
				continue;
			if (ae_squad_grid_find(plan, x, y, from->floor.z) != NONE)
				continue;
			tested++;
			if (!ae_squad_grid_floor(x, y, from->floor.z, &floor_at))
			{
				if (step < 4)
					from->wall = TRUE;
				continue;
			}
			clear = ae_squad_grid_knee_line(&from->floor, &floor_at);
			if (!ae_bots_squad_grid_step_ok((float)(floor_at.z - from->floor.z), clear))
			{
				if (step < 4)
					from->wall = TRUE;
				continue;
			}
			plan->points[plan->count].floor = floor_at;
			plan->points[plan->count].flood = from->flood + length;
			plan->points[plan->count].wall = FALSE;
			plan->count++;
		}
		ae_squad_grid.head++;
	}
	if (ae_squad_grid.head >= plan->count)
	{
		plan->ready = TRUE;
		plan->built_tick = now;
		ae_squad_grid.building = FALSE;
		ae_squad_grid.current = 1 - ae_squad_grid.current;
		ae_squad.stats[AE_SQUAD_STAT_GRID_POINTS] = plan->count;
		ae_squad.stats[AE_SQUAD_STAT_GRID_TICKS] = ae_squad_grid.build_ticks;
	}
}

/* the plan in use, NULL before the first is done or when the level's bsp changed since */
static struct ae_squad_grid_plan const *ae_squad_grid_ready(void)
{
	struct ae_squad_grid_plan const *plan = &ae_squad_grid.plans[ae_squad_grid.current];

	return plan->ready && plan->bsp == ae_squad_grid_bsp() ? plan : NULL;
}

/* every plan and kept floor forgotten (a new bsp, a revert) */
static void ae_squad_grid_forget(void)
{
	csmemset(&ae_squad_grid, 0, sizeof(ae_squad_grid));
}
