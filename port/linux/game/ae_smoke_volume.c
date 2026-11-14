/*
AE_SMOKE_VOLUME.C

The live smoke volumes (ae_smoke_volume.h; the numbers and why: ae_smoke_rules.h). A volume is its centre and the
game tick of the detonation, and nothing else: its age comes from the game clock, so it stands the same length of
time on every machine and ends by itself. At most AE_SMOKE_MAXIMUM stand; one more takes the oldest's place.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "math/real_math.h"

#include "ae_smoke_rules.h"
#include "ae_smoke_grenade.h"
#include "ae_smoke_volume.h"

static struct
{
	boolean used;
	long start;
	float centre[3];
} ae_smoke_volumes[AE_SMOKE_MAXIMUM];

static long ae_smoke_last_time = -1;
static long ae_smoke_made;
static long ae_smoke_refusals;
static long ae_smoke_refusal_log_time = -1000;

/* a game clock that went back is a new game: the volumes of the last are gone */
static void ae_smoke_clock_check(void)
{
	long now = game_time_get();

	if (ae_smoke_last_time >= 0 && now < ae_smoke_last_time)
		ae_smoke_volume_reset();
	ae_smoke_last_time = now;
}

void ae_smoke_volume_reset(void)
{
	long index;

	for (index = 0; index < AE_SMOKE_MAXIMUM; index++)
		ae_smoke_volumes[index].used = FALSE;
	ae_smoke_made = 0;
	ae_smoke_refusals = 0;
	ae_smoke_refusal_log_time = -1000;
	ae_smoke_last_time = -1;
}

long ae_smoke_volume_count(void)
{
	long index, count = 0, now;

	ae_smoke_clock_check();
	now = game_time_get();

	for (index = 0; index < AE_SMOKE_MAXIMUM; index++)
		if (ae_smoke_volumes[index].used && ae_smoke_alive(now - ae_smoke_volumes[index].start))
			count++;
	return count;
}

void ae_smoke_detonated(long projectile_definition_index, real_point3d const *at)
{
	long index, slot = NONE, oldest = 0, now;

	if (!ae_smoke_grenade_is_smoke(projectile_definition_index))
		return;
	ae_smoke_clock_check();
	now = game_time_get();
	for (index = 0; index < AE_SMOKE_MAXIMUM; index++)
	{
		if (!ae_smoke_volumes[index].used || !ae_smoke_alive(now - ae_smoke_volumes[index].start))
		{
			slot = index;
			break;
		}
		if (ae_smoke_volumes[index].start < ae_smoke_volumes[oldest].start)
			oldest = index;
	}
	if (slot == NONE)
		slot = oldest;
	ae_smoke_volumes[slot].used = TRUE;
	ae_smoke_volumes[slot].start = now;
	ae_smoke_volumes[slot].centre[0] = at->x;
	ae_smoke_volumes[slot].centre[1] = at->y;
	ae_smoke_volumes[slot].centre[2] = at->z + AE_SMOKE_LIFT;
	ae_smoke_made++;
	error(_error_silent, "ae smoke: volume %ld at %.2f %.2f %.2f radius %.2f from tick %ld, holds until %ld, gone at %ld "
		"(%ld standing)", ae_smoke_made, (double)ae_smoke_volumes[slot].centre[0], (double)ae_smoke_volumes[slot].centre[1],
		(double)ae_smoke_volumes[slot].centre[2], (double)AE_SMOKE_RADIUS, now, now + AE_SMOKE_HOLD_TICKS,
		now + AE_SMOKE_LIFE_TICKS, ae_smoke_volume_count());
}

boolean ae_smoke_blocks_sight(real_point3d const *eye, real_point3d const *point)
{
	float a[3], b[3];
	long index, now;

	if (!ae_smoke_made)
		return FALSE;
	ae_smoke_clock_check();
	now = game_time_get();
	a[0] = eye->x;
	a[1] = eye->y;
	a[2] = eye->z;
	b[0] = point->x;
	b[1] = point->y;
	b[2] = point->z;
	for (index = 0; index < AE_SMOKE_MAXIMUM; index++)
	{
		if (!ae_smoke_volumes[index].used)
			continue;
		if (ae_smoke_blocks_at(now - ae_smoke_volumes[index].start, ae_smoke_volumes[index].centre, a, b))
		{
			ae_smoke_refusals++;
			/* (a line a second at most: the bots ask every tick) */
			if (now - ae_smoke_refusal_log_time >= AE_SMOKE_TICKS_PER_SECOND)
			{
				ae_smoke_refusal_log_time = now;
				error(_error_silent, "ae smoke: sight refused through volume at %.2f %.2f %.2f, age %ld ticks (%ld "
					"refusals so far)", (double)ae_smoke_volumes[index].centre[0],
					(double)ae_smoke_volumes[index].centre[1], (double)ae_smoke_volumes[index].centre[2],
					now - ae_smoke_volumes[index].start, ae_smoke_refusals);
			}
			return TRUE;
		}
	}
	return FALSE;
}
