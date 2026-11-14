#include <stdio.h>
#include "ae_backpack_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the stock weapons by their tag paths */
	CHECK(ae_backpack_rules_kind("weapons\\assault rifle\\assault rifle", 0.2f) == AE_BACKPACK_KIND_BACK);
	CHECK(ae_backpack_rules_kind("weapons\\shotgun\\shotgun", 0.2f) == AE_BACKPACK_KIND_BACK);
	CHECK(ae_backpack_rules_kind("weapons\\rocket launcher\\rocket launcher", 0.3f) == AE_BACKPACK_KIND_BACK);
	CHECK(ae_backpack_rules_kind("weapons\\plasma_cannon\\plasma_cannon", 0.3f) == AE_BACKPACK_KIND_BACK);
	CHECK(ae_backpack_rules_kind("weapons\\sniper rifle\\sniper rifle", 0.3f) == AE_BACKPACK_KIND_LONG);
	CHECK(ae_backpack_rules_kind("weapons\\pistol\\pistol", 0.1f) == AE_BACKPACK_KIND_THIGH);
	CHECK(ae_backpack_rules_kind("weapons\\plasma pistol\\plasma pistol", 0.1f) == AE_BACKPACK_KIND_THIGH);
	CHECK(ae_backpack_rules_kind("weapons\\plasma rifle\\plasma rifle", 0.15f) == AE_BACKPACK_KIND_THIGH);
	CHECK(ae_backpack_rules_kind("weapons\\needler\\mp_needler", 0.15f) == AE_BACKPACK_KIND_THIGH);
	/* whatever the case; a mod's paths */
	CHECK(ae_backpack_rules_kind("CMT\\Weapons\\Human\\Magnum\\Magnum", 0.2f) == AE_BACKPACK_KIND_THIGH);
	CHECK(ae_backpack_rules_kind("h2\\weapons\\smg\\smg", 0.2f) == AE_BACKPACK_KIND_THIGH);
	CHECK(ae_backpack_rules_kind("weapons\\battle_rifle\\battle_rifle", 0.2f) == AE_BACKPACK_KIND_BACK);
	/* no word: by the radius; no path at all: the same */
	CHECK(ae_backpack_rules_kind("weapons\\thing\\thing", 0.05f) == AE_BACKPACK_KIND_THIGH);
	CHECK(ae_backpack_rules_kind("weapons\\thing\\thing", 0.3f) == AE_BACKPACK_KIND_BACK);
	CHECK(ae_backpack_rules_kind(NULL, 0.3f) == AE_BACKPACK_KIND_BACK);

	/* the places in slot order */
	{
		int back = 0, thighs = 0;

		CHECK(ae_backpack_rules_place(AE_BACKPACK_KIND_BACK, &back, &thighs) == AE_BACKPACK_PLACE_BACK);
		CHECK(ae_backpack_rules_place(AE_BACKPACK_KIND_THIGH, &back, &thighs) == AE_BACKPACK_PLACE_THIGH);
		CHECK(ae_backpack_rules_place(AE_BACKPACK_KIND_LONG, &back, &thighs) == AE_BACKPACK_PLACE_BACK_SECOND);
		CHECK(ae_backpack_rules_place(AE_BACKPACK_KIND_THIGH, &back, &thighs) == AE_BACKPACK_PLACE_THIGH_SECOND);
		/* a third of either: not drawn */
		CHECK(ae_backpack_rules_place(AE_BACKPACK_KIND_BACK, &back, &thighs) == AE_BACKPACK_PLACE_NONE);
		CHECK(ae_backpack_rules_place(AE_BACKPACK_KIND_THIGH, &back, &thighs) == AE_BACKPACK_PLACE_NONE);
	}
	/* a lone sniper rifle stands upright */
	{
		int back = 0, thighs = 0;

		CHECK(ae_backpack_rules_place(AE_BACKPACK_KIND_LONG, &back, &thighs) == AE_BACKPACK_PLACE_LONG);
	}

	if (failures)
		return 1;
	printf("ae_backpack_rules_test: all passed\n");
	return 0;
}
