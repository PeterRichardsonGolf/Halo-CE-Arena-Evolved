#include <stdio.h>
#include "ae_crouch_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* game_engine.h's bits beside it: SENSOR RANGE's 6-7, the port's own from 16 */
#define SENSOR_RANGE_MASK 0xC0UL

int main(void)
{
	/* CONTROLLER CROUCH (seed revision 21): bit 8 of the universal variant's flags; the other bits kept and never
	read as it */
	{
		unsigned long const samples[] = { 0x0UL, 0x3FUL, 0xC0UL, 0xFFFFFFFFUL, 0x80000001UL, 0x0001FE3FUL,
			0x7FFF0000UL, 0x100UL };
		unsigned int sample;
		int rule;

		for (sample = 0; sample < sizeof(samples) / sizeof(samples[0]); sample++)
		{
			for (rule = 0; rule < NUMBER_OF_AE_CONTROLLER_CROUCHES; rule++)
			{
				unsigned long flags = ae_controller_crouch_flags_set(samples[sample], rule);

				CHECK(ae_controller_crouch_get(flags) == rule);
				CHECK((flags & ~AE_CONTROLLER_CROUCH_FLAG) == (samples[sample] & ~AE_CONTROLLER_CROUCH_FLAG));
			}
			/* (a rule out of the list is HALO CE) */
			CHECK(ae_controller_crouch_get(ae_controller_crouch_flags_set(samples[sample], 2)) ==
				_ae_controller_crouch_halo_ce);
			CHECK(ae_controller_crouch_get(ae_controller_crouch_flags_set(samples[sample], -1)) ==
				_ae_controller_crouch_halo_ce);
		}
		/* (every gametype from before has 0 there: HALO CE; the Xbox game's bits 0-5, SENSOR RANGE's 6-7 and the
		port's from 16 are not it) */
		CHECK(ae_controller_crouch_get(0x0UL) == _ae_controller_crouch_halo_ce);
		CHECK(ae_controller_crouch_get(0x3FUL | SENSOR_RANGE_MASK) == _ae_controller_crouch_halo_ce);
		CHECK(ae_controller_crouch_get(0xFFFFFEFFUL) == _ae_controller_crouch_halo_ce);
		CHECK(ae_controller_crouch_get(0x100UL) == _ae_controller_crouch_stays_down);
		CHECK((AE_CONTROLLER_CROUCH_FLAG & SENSOR_RANGE_MASK) == 0);
		CHECK(AE_CONTROLLER_CROUCH_FLAG < 0x10000UL);
	}
	/* the crouch gate: only a held crouch under STAYS DOWN; HALO CE leaves Halo's own gate */
	{
		CHECK(ae_controller_crouch_holds(_ae_controller_crouch_stays_down, 1));
		CHECK(!ae_controller_crouch_holds(_ae_controller_crouch_stays_down, 0));
		CHECK(!ae_controller_crouch_holds(_ae_controller_crouch_halo_ce, 1));
		CHECK(!ae_controller_crouch_holds(_ae_controller_crouch_halo_ce, 0));
	}
	/* the log's words (tools/ae_test/seed_rules.py reads them) */
	{
		CHECK(ae_controller_crouch_name(_ae_controller_crouch_stays_down)[0] == 's');
		CHECK(ae_controller_crouch_name(_ae_controller_crouch_halo_ce)[0] == 'h');
		CHECK(ae_controller_crouch_name(5)[0] == 'h');
	}
	printf("ae_crouch_rules: %d failed\n", failures);
	return failures ? 1 : 0;
}
