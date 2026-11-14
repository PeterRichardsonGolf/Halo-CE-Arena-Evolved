/*
AE_HEAD_CHECK_RULES.C

See ae_head_check_rules.h.
*/

#include "ae_head_check_rules.h"

int ae_head_check_on_head(struct ae_head_check_point const *hit, struct ae_head_check_point const *seen,
	struct ae_head_check_point const *head_offset)
{
	return ae_head_check_on_head_span(hit, seen, head_offset, 0.0f, 0.0f);
}

int ae_head_check_on_head_span(struct ae_head_check_point const *hit, struct ae_head_check_point const *seen,
	struct ae_head_check_point const *head_offset, float below, float above)
{
	float reach = (float)AE_HEAD_CHECK_REACH_100 / 100.0f;
	float dx = hit->x - (seen->x + head_offset->x);
	float dy = hit->y - (seen->y + head_offset->y);
	float dz = hit->z - (seen->z + head_offset->z);
	float distance_squared;

	if (!(below > 0.0f))
		below = 0.0f;
	if (!(above > 0.0f))
		above = 0.0f;
	/* (the nearest point of the span: up to above over the head, down to below under it) */
	if (dz > above)
		dz -= above;
	else if (dz < -below)
		dz += below;
	else if (dz == dz)
		dz = 0.0f;
	distance_squared = dx * dx + dy * dy + dz * dz;
	/* (a number that is not one is not on the head either) */
	return distance_squared <= reach * reach ? 1 : 0;
}

int ae_head_check_body_material(int count, int const *head, int const *type, int head_material, int indirect)
{
	int index;

	if (head_material >= 0 && head_material < count)
	{
		for (index = 0; index < count; index++)
		{
			if (!head[index] && type[index] == type[head_material])
				return index;
		}
	}
	if (indirect >= 0 && indirect < count && !head[indirect])
		return indirect;
	for (index = 0; index < count; index++)
	{
		if (!head[index])
			return index;
	}
	return -1;
}
