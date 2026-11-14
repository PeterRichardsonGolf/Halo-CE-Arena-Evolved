/*
AE_PLANE_RULES.H

Which collision planes of a map are fixed before they are used (ae_plane_rules.c). No engine includes: the unit test in
port/linux/tests/ae_plane_rules_test.c compiles it alone.
*/

#ifndef __AE_PLANE_RULES_H
#define __AE_PLANE_RULES_H

#define AE_PLANE_UNIT_OK 0
#define AE_PLANE_UNIT_RESCALE 1
#define AE_PLANE_UNIT_UNUSABLE 2

/* what to do with a plane whose normal has this squared length: nothing when it is 1 within 0.0001 (the engine's own
test of a valid normal, so every plane of a valid map is as it was); divide the plane (its normal and its distance) by the
normal's length when it is off by more but is a normal (some maps' surface planes are 0.1 percent long: an engine assert
met them in the collision tests); the surface is left out when the length is zero, not a number or beyond any real map's */
int ae_plane_unit_action(float normal_length_squared);

#endif
