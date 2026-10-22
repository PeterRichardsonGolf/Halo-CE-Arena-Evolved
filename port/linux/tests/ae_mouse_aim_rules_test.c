#include <stdio.h>
#include <string.h>
#include "ae_crouch_rules.h"
#include "ae_melee_unzoom_rules.h"
#include "ae_mouse_aim_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* game_engine.h's bits beside it: SENSOR RANGE's 6-7, CONTROLLER CROUCH's 8, MELEE UNZOOMS' 9, the port's from 16 */
#define SENSOR_RANGE_MASK 0xC0UL

int main(void)
{
	/* MOUSE AIM ASSIST (seed revision 22): bit 10 of the universal variant's flags; the other bits kept */
	{
		unsigned long const samples[] = { 0x0UL, 0x3FUL, 0xC0UL, 0x100UL, 0x200UL, 0xFFFFFFFFUL, 0x80000001UL,
			0x0001FE3FUL, 0x7FFF0000UL, 0x400UL };
		unsigned int sample;
		int rule;

		for (sample = 0; sample < sizeof(samples) / sizeof(samples[0]); sample++)
		{
			for (rule = 0; rule < NUMBER_OF_AE_MOUSE_AIM_ASSISTS; rule++)
			{
				unsigned long flags = ae_mouse_aim_assist_flags_set(samples[sample], rule);

				CHECK(ae_mouse_aim_assist_get(flags) == rule);
				CHECK((flags & ~AE_MOUSE_AIM_ASSIST_FLAG) == (samples[sample] & ~AE_MOUSE_AIM_ASSIST_FLAG));
				CHECK(ae_controller_crouch_get(flags) == ae_controller_crouch_get(samples[sample]));
				CHECK(ae_melee_unzoom_get(flags) == ae_melee_unzoom_get(samples[sample]));
			}
			/* (a rule out of the list is ALLOWED) */
			CHECK(ae_mouse_aim_assist_get(ae_mouse_aim_assist_flags_set(samples[sample], 2)) ==
				_ae_mouse_aim_assist_allowed);
			CHECK(ae_mouse_aim_assist_get(ae_mouse_aim_assist_flags_set(samples[sample], -1)) ==
				_ae_mouse_aim_assist_allowed);
		}
		/* (no bit: ALLOWED, today's behaviour; the bits beside it are not it) */
		CHECK(ae_mouse_aim_assist_get(0x0UL) == _ae_mouse_aim_assist_allowed);
		CHECK(ae_mouse_aim_assist_get(0x3FUL | SENSOR_RANGE_MASK | AE_CONTROLLER_CROUCH_FLAG | AE_MELEE_UNZOOM_FLAG) ==
			_ae_mouse_aim_assist_allowed);
		CHECK(ae_mouse_aim_assist_get(0xFFFFFBFFUL) == _ae_mouse_aim_assist_allowed);
		CHECK(ae_mouse_aim_assist_get(0x400UL) == _ae_mouse_aim_assist_off);
		CHECK((AE_MOUSE_AIM_ASSIST_FLAG & (SENSOR_RANGE_MASK | AE_CONTROLLER_CROUCH_FLAG | AE_MELEE_UNZOOM_FLAG)) == 0);
		CHECK(AE_MOUSE_AIM_ASSIST_FLAG < 0x10000UL);
	}
	/* the host's policy (message 90's block_mouse): its own setting OR the gametype's OFF; ALLOWED with the setting
	off is the only allowed one, so an old setting still blocks */
	{
		CHECK(ae_mouse_aim_assist_policy(0, _ae_mouse_aim_assist_allowed) == 0);
		CHECK(ae_mouse_aim_assist_policy(1, _ae_mouse_aim_assist_allowed) == 1);
		CHECK(ae_mouse_aim_assist_policy(0, _ae_mouse_aim_assist_off) == 1);
		CHECK(ae_mouse_aim_assist_policy(1, _ae_mouse_aim_assist_off) == 1);
		/* (a gametype's flags as the host has them: an AE seed's, an NHE seed's, a stock one's) */
		CHECK(ae_mouse_aim_assist_policy(0, ae_mouse_aim_assist_get(0x247B0720UL)) == 1);
		CHECK(ae_mouse_aim_assist_policy(0, ae_mouse_aim_assist_get(0x10600020UL)) == 0);
		CHECK(ae_mouse_aim_assist_policy(0, ae_mouse_aim_assist_get(0x00000021UL)) == 0);
		/* (the wire's value: 0 or 1, what OpenCE's clients take) */
		CHECK(ae_mouse_aim_assist_policy(7, _ae_mouse_aim_assist_allowed) == 1);
	}
	/* the log's words (tools/ae_test/seed_rules.py reads them) */
	{
		CHECK(!strcmp(ae_mouse_aim_assist_name(_ae_mouse_aim_assist_off), "off"));
		CHECK(!strcmp(ae_mouse_aim_assist_name(_ae_mouse_aim_assist_allowed), "allowed"));
		CHECK(!strcmp(ae_mouse_aim_assist_name(5), "allowed"));
	}
	printf("ae_mouse_aim_rules: %d failed\n", failures);
	return failures ? 1 : 0;
}
