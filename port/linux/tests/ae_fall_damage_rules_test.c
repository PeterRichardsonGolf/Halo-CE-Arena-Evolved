#include <math.h>
#include <stdio.h>
#include "ae_fall_damage_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) (fabsf((a) - (b)) < (e))

int main(void)
{
	/* the three values: OFF the flag, CLASSIC none, HALO 2 the bit with the flag clear (the flag wins) */
	CHECK(ae_fall_damage_of(1, 0x00) == AE_FALL_DAMAGE_OFF && ae_fall_damage_of(1, 0xFF) == AE_FALL_DAMAGE_OFF);
	CHECK(ae_fall_damage_of(0, 0x00) == AE_FALL_DAMAGE_CLASSIC);
	CHECK(ae_fall_damage_of(0, 0x08) == AE_FALL_DAMAGE_HALO2);
	/* (every other bit of the byte leaves CLASSIC alone: BALL MELEE, NEEDLER, SPAWNS, the weapon slots, the marks) */
	CHECK(ae_fall_damage_of(0, 0xF7) == AE_FALL_DAMAGE_CLASSIC);

	/* setting a value: the byte's bit 3 only, the flag for OFF only */
	CHECK(ae_fall_damage_byte_set(0x00, AE_FALL_DAMAGE_HALO2) == 0x08);
	CHECK(ae_fall_damage_byte_set(0x08, AE_FALL_DAMAGE_CLASSIC) == 0x00);
	CHECK(ae_fall_damage_byte_set(0x08, AE_FALL_DAMAGE_OFF) == 0x00);
	CHECK(ae_fall_damage_byte_set(0xF5, AE_FALL_DAMAGE_HALO2) == 0xFD && ae_fall_damage_byte_set(0xFD, AE_FALL_DAMAGE_CLASSIC) == 0xF5);
	CHECK(ae_fall_damage_flag_for(AE_FALL_DAMAGE_OFF) && !ae_fall_damage_flag_for(AE_FALL_DAMAGE_CLASSIC) &&
		!ae_fall_damage_flag_for(AE_FALL_DAMAGE_HALO2));
	{
		int value;

		for (value = 0; value < AE_FALL_DAMAGES; value++)
			CHECK(ae_fall_damage_of(ae_fall_damage_flag_for(value), ae_fall_damage_byte_set(0x00, value)) == value);
	}

	/* the velocities are the map's own formula: Halo CE's 3, 6 and 16.5 world units are 0.1463, 0.2068 and 0.3430 a tick */
	CHECK(NEAR(ae_fall_damage_velocity(0.0035651792f, 3.0f), 0.14626f, 0.0001f));
	CHECK(NEAR(ae_fall_damage_velocity(0.0035651792f, 6.0f), 0.20684f, 0.0001f));
	CHECK(NEAR(ae_fall_damage_velocity(0.0035651792f, 16.5f), 0.34300f, 0.0001f));
	CHECK(AE_FALL_DAMAGE_LOWER < AE_FALL_DAMAGE_UPPER && AE_FALL_DAMAGE_UPPER < AE_FALL_DAMAGE_MAXIMUM);

	if (failures)
		return 1;
	printf("ae_fall_damage_rules_test: ok\n");
	return 0;
}
