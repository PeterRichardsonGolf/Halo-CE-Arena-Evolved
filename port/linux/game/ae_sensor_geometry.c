/*
AE_SENSOR_GEOMETRY.C

HUD STYLE AE's motion sensor, its pure parts (ae_sensor_geometry.h).
*/

#include <math.h>
#include <stdio.h>

#include "ae_sensor_geometry.h"

#define AE_PI 3.14159265f
/* CE's own sensor art reads "15m" for its range of 20 world units (hud_globals' motion_sensor_range on the
stock maps): its label's metres are 0.75 a world unit, so AE's label says what CE's says */
#define METRES_PER_WORLD_UNIT 0.75f

real ae_sensor_wedge_half_angle(real vertical_fov_radians, real width, real height)
{
	real aspect = height > 0.0f ? width / height : 16.0f / 9.0f;
	real half = (real)atan(tan(vertical_fov_radians / 2.0f) * aspect) * 180.0f / AE_PI;

	if (!(half >= 5.0f))
		return 5.0f;
	return half > 85.0f ? 85.0f : half;
}

real ae_sensor_inner_ring(real rim_radius)
{
	return rim_radius * AE_SENSOR_INNER_RING;
}

boolean ae_sensor_side_dividers_shown(real half_angle_degrees)
{
	return (boolean)(half_angle_degrees <= AE_SENSOR_SIDE_DIVIDERS_MAX);
}

void ae_sensor_range_label(real range_world_units, char *out)
{
	real metres = range_world_units * METRES_PER_WORLD_UNIT;
	long rounded = metres > 0.0f ? (long)(metres + 0.5f) : 0;

	if (rounded > 999)
		rounded = 999;
	snprintf(out, 8, "%ldm", rounded);
}
