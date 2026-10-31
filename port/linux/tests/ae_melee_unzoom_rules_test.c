#include <stdio.h>
#include <string.h>
#include "ae_crouch_rules.h"
#include "ae_melee_unzoom_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* game_engine.h's bits beside it: SENSOR RANGE's 6-7, CONTROLLER CROUCH's 8, the port's own from 16 */
#define SENSOR_RANGE_MASK 0xC0UL

int main(void)
{
	/* MELEE UNZOOMS (seed revision 22): bit 9 of the universal variant's flags; the other bits kept and never read
	as it */
	{
		unsigned long const samples[] = { 0x0UL, 0x3FUL, 0xC0UL, 0x100UL, 0xFFFFFFFFUL, 0x80000001UL, 0x0001FE3FUL,
			0x7FFF0000UL, 0x200UL };
		unsigned int sample;
		int rule;

		for (sample = 0; sample < sizeof(samples) / sizeof(samples[0]); sample++)
		{
			for (rule = 0; rule < NUMBER_OF_AE_MELEE_UNZOOMS; rule++)
			{
				unsigned long flags = ae_melee_unzoom_flags_set(samples[sample], rule);

				CHECK(ae_melee_unzoom_get(flags) == rule);
				CHECK((flags & ~AE_MELEE_UNZOOM_FLAG) == (samples[sample] & ~AE_MELEE_UNZOOM_FLAG));
				/* (CONTROLLER CROUCH, the bit before it, untouched) */
				CHECK(ae_controller_crouch_get(flags) == ae_controller_crouch_get(samples[sample]));
			}
			/* (a rule out of the list is OFF) */
			CHECK(ae_melee_unzoom_get(ae_melee_unzoom_flags_set(samples[sample], 2)) == _ae_melee_unzoom_off);
			CHECK(ae_melee_unzoom_get(ae_melee_unzoom_flags_set(samples[sample], -1)) == _ae_melee_unzoom_off);
		}
		/* (every gametype from before has 0 there: OFF; the Xbox game's bits 0-5, SENSOR RANGE's 6-7, CONTROLLER
		CROUCH's 8 and the port's from 16 are not it) */
		CHECK(ae_melee_unzoom_get(0x0UL) == _ae_melee_unzoom_off);
		CHECK(ae_melee_unzoom_get(0x3FUL | SENSOR_RANGE_MASK | AE_CONTROLLER_CROUCH_FLAG) == _ae_melee_unzoom_off);
		CHECK(ae_melee_unzoom_get(0xFFFFFDFFUL) == _ae_melee_unzoom_off);
		CHECK(ae_melee_unzoom_get(0x200UL) == _ae_melee_unzoom_on);
		CHECK((AE_MELEE_UNZOOM_FLAG & (SENSOR_RANGE_MASK | AE_CONTROLLER_CROUCH_FLAG)) == 0);
		CHECK(AE_MELEE_UNZOOM_FLAG < 0x10000UL);
	}
	/* the local player's zoom: none while melee is held under ON, else as it was (none stays none) */
	{
		short level;

		for (level = -1; level < 3; level++)
		{
			CHECK(ae_melee_unzoom_local_level(_ae_melee_unzoom_on, 1, level) == -1);
			CHECK(ae_melee_unzoom_local_level(_ae_melee_unzoom_on, 0, level) == level);
			CHECK(ae_melee_unzoom_local_level(_ae_melee_unzoom_off, 1, level) == level);
			CHECK(ae_melee_unzoom_local_level(_ae_melee_unzoom_off, 0, level) == level);
		}
	}
	/* the melee gate's zoom term: unzoomed always; zoomed only under ON (Halo CE: no melee while zoomed) */
	{
		CHECK(ae_melee_unzoom_allows(_ae_melee_unzoom_off, 0));
		CHECK(ae_melee_unzoom_allows(_ae_melee_unzoom_on, 0));
		CHECK(ae_melee_unzoom_allows(_ae_melee_unzoom_on, 1));
		CHECK(!ae_melee_unzoom_allows(_ae_melee_unzoom_off, 1));
	}
	/* the log's words (tools/ae_test/seed_rules.py reads them) */
	{
		CHECK(!strcmp(ae_melee_unzoom_name(_ae_melee_unzoom_on), "on"));
		CHECK(!strcmp(ae_melee_unzoom_name(_ae_melee_unzoom_off), "off"));
		CHECK(!strcmp(ae_melee_unzoom_name(5), "off"));
	}
	printf("ae_melee_unzoom_rules: %d failed\n", failures);
	return failures ? 1 : 0;
}
