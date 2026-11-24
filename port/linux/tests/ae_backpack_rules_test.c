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

	/* the fit: the gravity hammer pack's model turned onto its up axis, moved out and leaned, whatever the case;
	another map's hammer, a stock weapon and no path at all left as they are */
	{
		struct ae_backpack_fit fit;

		ae_backpack_rules_fit("ae\\packs\\gravity_hammer\\hammer\\gravity_hammer", &fit);
		CHECK(fit.turn_degrees == -32.2f && fit.out == 0.05f && fit.lean_degrees == 15.f);
		ae_backpack_rules_fit("AE\\Packs\\Gravity_Hammer\\hammer\\gravity_hammer", &fit);
		CHECK(fit.turn_degrees == -32.2f && fit.out == 0.05f && fit.lean_degrees == 15.f);
		ae_backpack_rules_fit("weapons\\gravity hammer\\gravity hammer", &fit);
		CHECK(fit.turn_degrees == 0.f && fit.out == 0.f && fit.lean_degrees == 0.f);
		ae_backpack_rules_fit("weapons\\rocket launcher\\rocket launcher", &fit);
		CHECK(fit.turn_degrees == 0.f && fit.out == 0.f && fit.lean_degrees == 0.f);
		CHECK(fit.barrel[0] == 0.f && fit.barrel[1] == 0.f && fit.barrel[2] == 0.f && fit.up == 0.f && fit.side == 0.f);
		fit.out = 1.f;
		fit.side = 1.f;
		fit.barrel[2] = 1.f;
		fit.top[1] = 1.f;
		fit.up = 1.f;
		fit.along = 1.f;
		ae_backpack_rules_fit(NULL, &fit);
		CHECK(fit.along == 0.f && fit.top[1] == 0.f);
		CHECK(fit.turn_degrees == 0.f && fit.out == 0.f && fit.lean_degrees == 0.f && fit.barrel[2] == 0.f &&
			fit.top[1] == 0.f && fit.up == 0.f && fit.side == 0.f);
	}
	/* the fuel rod: across the back as a rifle lies, on its place's barrel and its place's top (none of its own: its
	side is its thin way, flat on the back), moved to the left and a little down so its tube is on the spine, and out by
	its thickness; not turned, leaned or moved along its barrel; the gravity hammer and the rocket launcher stay across the back */
	{
		struct ae_backpack_fit fit;

		ae_backpack_rules_fit("weapons\\plasma_cannon\\plasma_cannon", &fit);
		CHECK(fit.barrel[0] == 0.f && fit.barrel[1] == 0.f && fit.barrel[2] == 0.f);
		CHECK(fit.top[0] == 0.f && fit.top[1] == 0.f && fit.top[2] == 0.f);
		CHECK(fit.side > 0.1f && fit.side < 0.2f && fit.out > 0.02f && fit.out < 0.05f);
		CHECK(fit.up < 0.f && fit.up > -0.06f && fit.along == 0.f);
		CHECK(fit.turn_degrees == 0.f && fit.lean_degrees == 0.f);
		ae_backpack_rules_fit("Weapons\\Plasma_Cannon\\Plasma_Cannon", &fit);
		CHECK(fit.top[0] == 0.f && fit.side > 0.1f);
		ae_backpack_rules_fit("ae\\packs\\gravity_hammer\\hammer\\gravity_hammer", &fit);
		CHECK(fit.barrel[2] == 0.f && fit.up == 0.f);
		ae_backpack_rules_fit("weapons\\rocket launcher\\rocket launcher", &fit);
		CHECK(fit.barrel[2] == 0.f && fit.up == 0.f && fit.side == 0.f && fit.along == 0.f && fit.top[0] == 0.f);
	}
	/* the flamethrower: its barrel set back against its node's turn (in the plane of the back, at right angles to its
	top), moved back along its barrel (up, to the right) and a little out */
	{
		struct ae_backpack_fit fit;

		ae_backpack_rules_fit("weapons\\flamethrower\\flamethrower", &fit);
		CHECK(fit.barrel[0] == 0.f && fit.barrel[1] > 0.9f && fit.barrel[2] > 0.f && fit.top[0] == 0.f);
		CHECK(fit.barrel[1] * fit.top[1] + fit.barrel[2] * fit.top[2] > -0.001f &&
			fit.barrel[1] * fit.top[1] + fit.barrel[2] * fit.top[2] < 0.001f);
		CHECK(fit.up > 0.f && fit.side < 0.f && fit.out > 0.f && fit.out < 0.1f);
		CHECK(fit.turn_degrees == 0.f && fit.lean_degrees == 0.f);
	}
	/* the hammer still goes across the back */
	CHECK(ae_backpack_rules_kind("ae\\packs\\gravity_hammer\\hammer\\gravity_hammer", 0.4f) == AE_BACKPACK_KIND_BACK);

	if (failures)
		return 1;
	printf("ae_backpack_rules_test: all passed\n");
	return 0;
}
