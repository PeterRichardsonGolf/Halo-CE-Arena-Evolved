/*
AE_FALL_DAMAGE_RULES.H

FALL DAMAGE, pure (port/linux/tests/ae_fall_damage_rules_test.c): the gametype's three values, OFF, CLASSIC and HALO 2
(GAME OPTIONS > SPARTAN). ae_fall_damage.c reads it.

OFF is the old FALL DAMAGE OFF, the variant's flag no falling damage (set = off). CLASSIC is that flag clear and the
map's own numbers, as ever. HALO 2 is universal_variant.ball_melee's bit 3 (0x08) with the flag clear: landings hurt from 6
world units, hurt fully from 10, and a fall past 14 kills, whatever the map's globals say. The byte's other bits (BALL
MELEE's low two, NEEDLER bit 2, SPAWNS, the weapon slots, the marks) are kept. A build without this reads HALO 2 as
CLASSIC (the map's numbers, the host deals the damage) and OFF as OFF.

Every machine applies it from the gametype (the host's variant on a client). Players only: vehicles keep the map's
own fall limit (ae_fall_damage_vehicle_velocity).
*/

#ifndef __AE_FALL_DAMAGE_RULES_H
#define __AE_FALL_DAMAGE_RULES_H

#include <math.h>

enum { AE_FALL_DAMAGE_OFF = 0, AE_FALL_DAMAGE_CLASSIC = 1, AE_FALL_DAMAGE_HALO2 = 2, AE_FALL_DAMAGES };

#define AE_FALL_DAMAGE_HALO2_BIT 0x08

/* the distances, world units: a landing starts to hurt, hurts fully, and a fall kills */
#define AE_FALL_DAMAGE_LOWER 6.0f
#define AE_FALL_DAMAGE_UPPER 10.0f
#define AE_FALL_DAMAGE_MAXIMUM 14.0f

/* the gametype's value: the no falling damage flag set, else the byte's HALO 2 bit */
static inline int ae_fall_damage_of(int no_falling_damage, unsigned char byte)
{
	if (no_falling_damage)
		return AE_FALL_DAMAGE_OFF;
	return (byte & AE_FALL_DAMAGE_HALO2_BIT) ? AE_FALL_DAMAGE_HALO2 : AE_FALL_DAMAGE_CLASSIC;
}

/* the byte with the value's HALO 2 bit set or clear, the other bits kept */
static inline unsigned char ae_fall_damage_byte_set(unsigned char byte, int value)
{
	return (unsigned char)(value == AE_FALL_DAMAGE_HALO2 ? byte | AE_FALL_DAMAGE_HALO2_BIT :
		byte & ~AE_FALL_DAMAGE_HALO2_BIT);
}

/* the no falling damage flag for the value */
static inline int ae_fall_damage_flag_for(int value)
{
	return value == AE_FALL_DAMAGE_OFF;
}

/* the velocity a body has after falling a distance, from rest: the map's own runtime velocities are worked out the same
way (distance 3, 6 and 16.5 give Halo CE's 0.1463, 0.2068 and 0.3430 a tick) */
static inline float ae_fall_damage_velocity(float gravity, float distance)
{
	return sqrtf(2.0f * gravity * distance);
}

#endif
