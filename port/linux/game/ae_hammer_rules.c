/* AE_HAMMER_RULES.C: the gravity hammer's blast push (ae_hammer_rules.h) */

#include <math.h>
#include <stddef.h>

#include "ae_hammer_rules.h"

#define AE_HAMMER_BLAST_TAG "ae\\packs\\gravity_hammer\\hammer\\effects\\push"
#define AE_HAMMER_WEAPON_TAG "ae\\packs\\gravity_hammer\\hammer\\gravity_hammer"
/* the swing's blow: rays this long from the melee blow's origin (the game's are 0.8), so they reach past the front of a
body whose centre is 2.8 units from the swinger (a body's front is about 0.2 before its centre; the origin is up to 0.16
ahead: the tip at 2.66), and not the front of one at 3 (2.8) */
#define AE_HAMMER_SWING_RAYS 2.5f
#define AE_HAMMER_BLOW_RAY 0.8f
/* the blow's rays fan this far to each side of the 0.8 ahead (two steps of 0.1, units.c) */
#define AE_HAMMER_BLOW_SPREAD 0.2f
/* the reticle red (CE: autoaim level 1, inside half the autoaim range) to a body whose centre is this far past the
rays' tip: a cyborg's front is 0.2 before its centre, so a body red there is always reached */
#define AE_HAMMER_RED_BODY_FRONT 0.12f
/* from the object's surface */
#define AE_HAMMER_PUSH_RANGE 1.5f
/* units a second, times the object's acceleration scale */
#define AE_HAMMER_PUSH 3.5f
/* the point pushed toward: the centre, raised by this much of the radius */
#define AE_HAMMER_PUSH_LIFT 0.05f
/* a body (a biped) takes the push as a body of this acceleration scale, whatever its own */
#define AE_HAMMER_BODY_SCALE 1.2f
#define AE_HAMMER_BODY_FLOOR 0.3f
#define AE_HAMMER_OBJECT_FLOOR 0.05f

/* whether path is name, whatever its case */
static int ae_hammer_rules_is(
	char const *path,
	char const *name)
{
	size_t index;

	if (!path)
		return 0;
	for (index = 0; name[index]; index++)
	{
		char c = path[index];

		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		if (c != name[index])
			return 0;
	}
	return path[index] == 0;
}

int ae_hammer_rules_blast(
	char const *damage_tag_path)
{
	return ae_hammer_rules_is(damage_tag_path, AE_HAMMER_BLAST_TAG);
}

int ae_hammer_rules_weapon(
	char const *weapon_tag_path)
{
	return ae_hammer_rules_is(weapon_tag_path, AE_HAMMER_WEAPON_TAG);
}

float ae_hammer_rules_autoaim_range(
	float origin_ahead)
{
	return 2.f * (origin_ahead + AE_HAMMER_SWING_RAYS + AE_HAMMER_RED_BODY_FRONT);
}

float ae_hammer_rules_autoaim_angle(void)
{
	return 2.f * atanf(AE_HAMMER_BLOW_SPREAD / AE_HAMMER_BLOW_RAY);
}

float ae_hammer_rules_swing_reach_scale(void)
{
	return AE_HAMMER_SWING_RAYS / AE_HAMMER_BLOW_RAY;
}

int ae_hammer_rules_push(
	float const blast[3],
	float const center[3],
	float radius,
	float acceleration_scale,
	int body,
	float velocity[3])
{
	float to_center[3];
	float distance;
	float push = AE_HAMMER_PUSH * (body ? AE_HAMMER_BODY_SCALE : acceleration_scale);
	float length;
	int axis;

	velocity[0] = velocity[1] = velocity[2] = 0.f;
	for (axis = 0; axis < 3; axis++)
		to_center[axis] = center[axis] - blast[axis];
	distance = sqrtf(to_center[0] * to_center[0] + to_center[1] * to_center[1] + to_center[2] * to_center[2]);
	distance = distance > radius ? distance - radius : 0.f;
	if (distance > AE_HAMMER_PUSH_RANGE)
		return 0;
	if (push <= (body ? AE_HAMMER_BODY_FLOOR : AE_HAMMER_OBJECT_FLOOR))
		return 0;

	to_center[2] += AE_HAMMER_PUSH_LIFT * radius;
	length = sqrtf(to_center[0] * to_center[0] + to_center[1] * to_center[1] + to_center[2] * to_center[2]);
	if (length < 0.0001f)
	{
		velocity[2] = push;
		return 1;
	}
	for (axis = 0; axis < 3; axis++)
		velocity[axis] = to_center[axis] / length * push;
	return 1;
}

int ae_hammer_rules_melee_blocked(
	long ticks_since_swing,
	float swings_a_second,
	float ticks_a_second)
{
	float cycle;

	if (ticks_since_swing < 0 || swings_a_second <= 0.f)
		return 0;
	cycle = ticks_a_second / swings_a_second;
	return (float)ticks_since_swing < cycle;
}
