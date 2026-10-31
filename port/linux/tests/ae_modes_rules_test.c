#include <stdio.h>
#include "ae_modes_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* One in the Chamber: a kill's round goes in the magazine while it has room, else it is carried */
	{
		short loaded = 0, carried = 0;

		ae_modes_rules_round_for_kill(&loaded, &carried, 12);
		CHECK(loaded == 1 && carried == 0);
		ae_modes_rules_round_for_kill(&loaded, &carried, 12);
		CHECK(loaded == 2 && carried == 0);
		loaded = 12;
		ae_modes_rules_round_for_kill(&loaded, &carried, 12);
		CHECK(loaded == 12 && carried == 1);
	}
	/* Frag Nation: grenades by the killing damage's tag */
	{
		short frags, plasmas;

		ae_modes_rules_grenades_for_kill("weapons\\frag grenade\\explosion", &frags, &plasmas);
		CHECK(frags == 1 && plasmas == 0);
		ae_modes_rules_grenades_for_kill("weapons\\plasma grenade\\explosion", &frags, &plasmas);
		CHECK(frags == 0 && plasmas == 1);
		ae_modes_rules_grenades_for_kill("weapons\\plasma grenade\\attached", &frags, &plasmas);
		CHECK(frags == 0 && plasmas == 2);
		/* a mod's path, another case */
		ae_modes_rules_grenades_for_kill("CMT\\Weapons\\Plasma_Grenade\\Attached", &frags, &plasmas);
		CHECK(frags == 0 && plasmas == 2);
		/* not a grenade's: nothing */
		ae_modes_rules_grenades_for_kill("weapons\\pistol\\bullet", &frags, &plasmas);
		CHECK(frags == 0 && plasmas == 0);
		ae_modes_rules_grenades_for_kill("weapons\\plasma rifle\\bolt", &frags, &plasmas);
		CHECK(frags == 0 && plasmas == 0);
		ae_modes_rules_grenades_for_kill(NULL, &frags, &plasmas);
		CHECK(frags == 0 && plasmas == 0);
	}
	/* capped counts */
	CHECK(ae_modes_rules_add_capped(2, 1, 4) == 3);
	CHECK(ae_modes_rules_add_capped(3, 2, 4) == 4);
	CHECK(ae_modes_rules_add_capped(4, 1, 4) == 4);
	CHECK(ae_modes_rules_add_capped(6, 1, 4) == 6);

	if (failures)
		return 1;
	printf("ae_modes_rules_test: all passed\n");
	return 0;
}
