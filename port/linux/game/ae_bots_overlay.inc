/*
AE_BOTS_OVERLAY.INC

Arena Evolved: the DEV MODE bot navigation overlay's reads of the bots' state (ae_bots_overlay.h): the walkable surfaces
closed to the bots' searches and each driving bot's place and path. #included at the end of ae_bots_lan.inc.
*/

#include "ae_bots_overlay.h"

#define AE_BOTS_OVERLAY_CLOSED 4096
#define AE_BOTS_OVERLAY_REREAD_TICKS 90

static struct
{
	byte const *walkable;
	long read_tick;
	long count;
	long surface[AE_BOTS_OVERLAY_CLOSED];
	/* each one's middle and the reach of its farthest corner, worked out at the read: a frame tests these first */
	float centre_x[AE_BOTS_OVERLAY_CLOSED], centre_y[AE_BOTS_OVERLAY_CLOSED], reach[AE_BOTS_OVERLAY_CLOSED];
} ae_bots_overlay_closed;

/* the surfaces the map opens and the bots' copy has closed, read again after a while or for another copy */
static void ae_bots_overlay_closed_read(void)
{
	long now = game_time_get(), surface_index, count;
	byte const *map_walkable;

	if (!bots_paths.walkable || !bots_paths.source)
	{
		ae_bots_overlay_closed.walkable = NULL;
		ae_bots_overlay_closed.count = 0;
		return;
	}
	if (ae_bots_overlay_closed.walkable == bots_paths.walkable && now >= ae_bots_overlay_closed.read_tick &&
		now - ae_bots_overlay_closed.read_tick < AE_BOTS_OVERLAY_REREAD_TICKS)
	{
		return;
	}
	ae_bots_overlay_closed.walkable = bots_paths.walkable;
	ae_bots_overlay_closed.read_tick = now;
	ae_bots_overlay_closed.count = 0;
	map_walkable = (byte const *)xbox_pointer(bots_paths.source->pathfinding_surfaces.address);
	count = bots_paths.structure.pathfinding_surfaces.count;
	for (surface_index = 0; surface_index < count; surface_index++)
	{
		if (TEST_FLAG(map_walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT) &&
			!TEST_FLAG(bots_paths.walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT) &&
			ae_bots_overlay_closed.count < AE_BOTS_OVERLAY_CLOSED)
		{
			ae_bots_overlay_closed.surface[ae_bots_overlay_closed.count++] = surface_index;
		}
	}
	{
		struct collision_bsp const *bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
		real_point3d corners[AE_BOTS_OVERLAY_CORNERS];
		long index;

		for (index = 0; index < ae_bots_overlay_closed.count; index++)
		{
			long corner_count = bots_surface_corners(bsp, ae_bots_overlay_closed.surface[index], corners), corner;
			float sx = 0.0f, sy = 0.0f, far2 = 0.0f;

			for (corner = 0; corner < corner_count; corner++)
			{
				sx += (float)corners[corner].x;
				sy += (float)corners[corner].y;
			}
			if (corner_count > 0)
			{
				sx /= (float)corner_count;
				sy /= (float)corner_count;
			}
			for (corner = 0; corner < corner_count; corner++)
			{
				float dx = (float)corners[corner].x - sx, dy = (float)corners[corner].y - sy;

				if (dx * dx + dy * dy > far2)
					far2 = dx * dx + dy * dy;
			}
			ae_bots_overlay_closed.centre_x[index] = sx;
			ae_bots_overlay_closed.centre_y[index] = sy;
			ae_bots_overlay_closed.reach[index] = corner_count > 0 ? __builtin_sqrtf(far2) : -1.0f;
		}
	}
}

long ae_bots_overlay_closed_count(void)
{
	ae_bots_overlay_closed_read();
	return ae_bots_overlay_closed.count;
}

long ae_bots_overlay_closed_near(real_point3d const *centre, real range, real_point3d *corners, long *corner_counts, long max)
{
	struct collision_bsp const *bsp;
	long index, found = 0;

	ae_bots_overlay_closed_read();
	if (ae_bots_overlay_closed.count <= 0)
		return 0;
	bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	for (index = 0; index < ae_bots_overlay_closed.count && found < max; index++)
	{
		real_point3d *out = corners + found * AE_BOTS_OVERLAY_CORNERS;
		float far = (float)range + ae_bots_overlay_closed.reach[index];
		float cdx = ae_bots_overlay_closed.centre_x[index] - (float)centre->x;
		float cdy = ae_bots_overlay_closed.centre_y[index] - (float)centre->y;
		long corner_count, corner;
		boolean is_near = FALSE;

		/* (no corner can be in range: skipped without walking its edges) */
		if (ae_bots_overlay_closed.reach[index] < 0.0f || cdx * cdx + cdy * cdy > far * far)
			continue;
		corner_count = bots_surface_corners(bsp, ae_bots_overlay_closed.surface[index], out);

		for (corner = 0; corner < corner_count && !is_near; corner++)
		{
			real dx = out[corner].x - centre->x, dy = out[corner].y - centre->y;

			is_near = (boolean)(dx * dx + dy * dy <= range * range);
		}
		if (is_near && corner_count >= 3)
			corner_counts[found++] = corner_count;
	}
	return found;
}

long ae_bots_overlay_drivers(struct ae_bots_overlay_driver *out, long max)
{
	long index, found = 0, now = game_time_get();

	for (index = 0; index < BOTS_BRAINS && found < max; index++)
	{
		struct bots_bot const *bot = &bots_globals.bots[index];
		struct ae_bots_vehicle_bot const *state = &ae_bots_vehicle_bots[index];
		struct object_datum const *vehicle;
		struct ae_bots_overlay_driver *driver = &out[found];
		char const *route;
		int point, step;

		if (bot->player_index == NONE || state->vehicle_index == NONE || state->seat != AE_BOTS_SEAT_DRIVER)
			continue;
		vehicle = (struct object_datum const *)object_try_and_get(state->vehicle_index);
		if (!vehicle)
			continue;
		driver->bot_index = index;
		driver->vehicle_class = state->vehicle_class;
		driver->position = vehicle->object.position;
		driver->have_next = ae_bot_nav_driver_place(index, now, &route, &point, &driver->next) ? 1 : 0;
		driver->step_count = 0;
		if (bot->path.valid)
		{
			for (step = bot->path.step_index; step < bot->path.step_count && driver->step_count < AE_BOTS_OVERLAY_STEPS; step++)
				driver->steps[driver->step_count++] = bot->path.steps[step].point;
			if (!driver->have_next && driver->step_count > 0)
			{
				driver->have_next = 1;
				driver->next = driver->steps[0];
			}
		}
		found++;
	}
	return found;
}
