/*
AE_SENSOR_GEOMETRY.H

HUD STYLE AE's motion sensor, its pure parts (spec 14.15 part 4, G3, G4; plan
Task 16), unit-tested in port/linux/tests/ae_sensor_test.c.
*/

#ifndef __AE_SENSOR_GEOMETRY_H
#define __AE_SENSOR_GEOMETRY_H

#include "ae_pure_types.h"

#define AE_SENSOR_INNER_RING 0.616f       /* half the range: CE draws distance as r x (d / range)^0.7 */
#define AE_SENSOR_SIDE_DIVIDERS_MAX 75.0f /* G4: the left and right dividers hide past this half-angle */

/* the view wedge's half-angle in degrees: atan(tan(vertical FOV / 2) x width / height), clamped 5..85 */
real ae_sensor_wedge_half_angle(real vertical_fov_radians, real width, real height);
/* the inner ring's radius */
real ae_sensor_inner_ring(real rim_radius);
/* G4: whether the left and right dividers show */
boolean ae_sensor_side_dividers_shown(real half_angle_degrees);
/* the range label's text ("15m"): as CE's own sensor art labels its range of 20 world units, 0.75 m a world unit,
rounded; out holds at least 8 characters */
void ae_sensor_range_label(real range_world_units, char *out);

#endif /* __AE_SENSOR_GEOMETRY_H */
