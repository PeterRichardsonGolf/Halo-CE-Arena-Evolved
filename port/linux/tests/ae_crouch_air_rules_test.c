#include <stdio.h>

#include "ae_crouch_air_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the stock player graph's crouched pistol: its airborne is the unarmed body's, its landings the rocket launcher's */
	CHECK(!ae_crouch_air_name_is_own("pistol", "stand unarmed airborne"));
	CHECK(!ae_crouch_air_name_is_own("pistol", "stand missile land-soft"));
	CHECK(!ae_crouch_air_name_is_own("pistol", "stand missile land-hard"));
	/* the rifle's own crouched clips stay */
	CHECK(ae_crouch_air_name_is_own("rifle", "crouch rifle airborne"));
	CHECK(ae_crouch_air_name_is_own("pistol", "Crouch Pistol land-soft"));
	/* a class whose name starts another's is not it ("pistol" is not "pistols") */
	CHECK(!ae_crouch_air_name_is_own("pistol", "crouch pistols airborne"));
	CHECK(!ae_crouch_air_name_is_own("", "crouch  airborne"));
	CHECK(!ae_crouch_air_name_is_own(NULL, "crouch pistol airborne") && !ae_crouch_air_name_is_own("pistol", NULL));
	/* only the crouch seat */
	CHECK(ae_crouch_air_seat("crouch") && !ae_crouch_air_seat("stand") && !ae_crouch_air_seat(NULL));
	if (failures)
		return 1;
	printf("ae_crouch_air_rules: all passed\n");
	return 0;
}
