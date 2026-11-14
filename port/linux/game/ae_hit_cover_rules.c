/*
AE_HIT_COVER_RULES.C

See ae_hit_cover_rules.h.
*/

#include <math.h>

#include "ae_hit_cover_rules.h"

/* `from` moved toward `to` by the nudge, no further than half the way */
static struct ae_hit_cover_point ae_hit_cover_nudged(struct ae_hit_cover_point const *from,
	struct ae_hit_cover_point const *to)
{
	struct ae_hit_cover_point point = *from;
	float dx = to->x - from->x, dy = to->y - from->y, dz = to->z - from->z;
	float length = sqrtf(dx * dx + dy * dy + dz * dz);
	float step = (float)AE_HIT_COVER_NUDGE_100 / 100.0f;

	if (!(length > 0.0f))
		return point;
	if (step > length * 0.5f)
		step = length * 0.5f;
	point.x += dx / length * step;
	point.y += dy / length * step;
	point.z += dz / length * step;
	return point;
}

int ae_hit_cover_lines(int melee, struct ae_hit_cover_point const *origin, struct ae_hit_cover_point const *epicenter,
	struct ae_hit_cover_point const *centre, struct ae_hit_cover_line lines[AE_HIT_COVER_MAXIMUM_LINES])
{
	if (melee)
	{
		lines[0].from = *origin;
		lines[0].to = *centre;
		lines[1].from = *epicenter;
		lines[1].to = *centre;
	}
	else
	{
		lines[0].from = ae_hit_cover_nudged(epicenter, centre);
		lines[0].to = *centre;
		lines[1].to = *centre;
		lines[1].to.z += (float)AE_HIT_COVER_RAISE_100 / 100.0f;
		lines[1].from = ae_hit_cover_nudged(epicenter, &lines[1].to);
	}
	return 2;
}

int ae_hit_cover_refused(int enabled, int count, int const *blocked)
{
	int index;

	if (!enabled || count <= 0)
		return 0;
	for (index = 0; index < count; index++)
	{
		if (!blocked[index])
			return 0;
	}
	return 1;
}
