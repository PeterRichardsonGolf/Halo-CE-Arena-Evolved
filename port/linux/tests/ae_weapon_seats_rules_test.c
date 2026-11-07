#include <stdio.h>
#include "ae_weapon_seats_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the stock player's graph: stand and crouch have every label, the alert seat the rifle's and the shotgun's only, the
	vehicle seats hold nothing */
	char const *const names[] = { "W-driver", "crouch", "stand", "alert", "S-rider" };
	unsigned char const holds[] = { 0, 1, 1, 1, 1 };
	unsigned char pistol[] = { 0, 1, 1, 0, 1 };
	unsigned char rifle[] = { 0, 1, 1, 1, 1 };
	unsigned char missing_stand[] = { 0, 1, 0, 0, 1 };
	short holding, first;

	CHECK(!ae_weapon_seats_counted("alert") && !ae_weapon_seats_counted("ALERT"));
	CHECK(ae_weapon_seats_counted("stand") && ae_weapon_seats_counted("alerted") && ae_weapon_seats_counted("aler") &&
		ae_weapon_seats_counted("") && ae_weapon_seats_counted(NULL));
	/* the pistol's label is lacking in the alert seat only: that seat is left out, so the pistol's label passes */
	CHECK(ae_weapon_seats_lacking(5, names, holds, pistol, &holding, &first) == 0 && holding == 3 && first == -1);
	/* a label held everywhere passes */
	CHECK(ae_weapon_seats_lacking(5, names, holds, rifle, &holding, &first) == 0 && holding == 3);
	/* any other seat lacking the label still refuses it: the first such seat is told */
	CHECK(ae_weapon_seats_lacking(5, names, holds, missing_stand, &holding, &first) == 1 && holding == 3 && first == 2);
	/* a seat that holds nothing (the driver's) and has not the label is not counted; one that has it is */
	CHECK(ae_weapon_seats_lacking(1, names, holds, pistol, &holding, &first) == 0 && holding == 0);
	{
		unsigned char driver_has[] = { 1, 0, 0, 0, 0 };

		CHECK(ae_weapon_seats_lacking(5, names, holds, driver_has, &holding, &first) == 3 && holding == 4 && first == 1);
	}
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_weapon_seats_rules_test: all passed\n");
	return failures != 0;
}
