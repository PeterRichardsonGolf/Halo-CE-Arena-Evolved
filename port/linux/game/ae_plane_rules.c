/*
AE_PLANE_RULES.C

Which collision planes of a map are fixed before they are used (ae_plane_rules.h).
*/

#include "ae_plane_rules.h"

int ae_plane_unit_action(float normal_length_squared)
{
	float off = normal_length_squared - 1.0f;

	/* (a not a number fails every comparison: it is unusable) */
	if (!(normal_length_squared > 0.0f && normal_length_squared < 4.0f))
	{
		return AE_PLANE_UNIT_UNUSABLE;
	}
	if (off < 0.0001f && off > -0.0001f)
	{
		return AE_PLANE_UNIT_OK;
	}
	return AE_PLANE_UNIT_RESCALE;
}
