/* The DEV MODE weapon wheel's rules (port/linux/game/ae_dev_wheel_rules.c) */
#include <stdio.h>
#include <string.h>

#include "ae_dev_wheel_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	short total = 0, page;

	/* (the pages: stock human and covenant weapons by name; a plasma pistol is covenant; the packs' the rest) */
	CHECK(ae_dev_wheel_category("weapons\\pistol\\pistol") == AE_DEV_WHEEL_HUMAN);
	CHECK(ae_dev_wheel_category("weapons\\plasma pistol\\plasma pistol") == AE_DEV_WHEEL_COVENANT);
	CHECK(ae_dev_wheel_category("weapons\\needler\\needler") == AE_DEV_WHEEL_COVENANT);
	CHECK(ae_dev_wheel_category("weapons\\Sniper Rifle\\sniper rifle") == AE_DEV_WHEEL_HUMAN);
	CHECK(ae_dev_wheel_category("weapons\\flamethrower\\flamethrower") == AE_DEV_WHEEL_HUMAN);
	CHECK(ae_dev_wheel_category("weapons\\battle rifle\\battle rifle") == AE_DEV_WHEEL_OTHER);
	CHECK(ae_dev_wheel_category(NULL) == AE_DEV_WHEEL_OTHER);
	CHECK(ae_dev_wheel_category("ae\\packs\\smg\\weapons\\smg\\smg") == AE_DEV_WHEEL_OTHER);
	CHECK(ae_dev_wheel_category("ae\\packs\\carbine\\weapons\\carbine\\carbine") == AE_DEV_WHEEL_OTHER);
	/* (at most 8 a page, the pages' sizes add up and differ by one) */
	CHECK(ae_dev_wheel_pages(0) == 0 && ae_dev_wheel_pages(8) == 1 && ae_dev_wheel_pages(9) == 2 &&
		ae_dev_wheel_pages(96) == 12);
	for (page = 0; page < ae_dev_wheel_pages(19); page++)
	{
		CHECK(ae_dev_wheel_page_size(19, page) <= 8);
		total = (short)(total + ae_dev_wheel_page_size(19, page));
	}
	CHECK(total == 19 && ae_dev_wheel_page_size(19, 0) == 7 && ae_dev_wheel_page_size(19, 2) == 6);
	CHECK(ae_dev_wheel_page_size(19, 3) == 0);
	/* (the pick: the dead zone takes nothing; slices clockwise from the top; 8 slices of 45 degrees) */
	CHECK(ae_dev_wheel_pick(8, 0.0f, 0.0f, -1) == -1);
	CHECK(ae_dev_wheel_pick(8, 0.0f, 0.2f, -1) == -1);
	CHECK(ae_dev_wheel_pick(8, 0.0f, 0.6f, -1) == 0);
	CHECK(ae_dev_wheel_pick(8, 0.6f, 0.0f, -1) == 2);
	CHECK(ae_dev_wheel_pick(8, 0.0f, -0.6f, -1) == 4);
	CHECK(ae_dev_wheel_pick(8, -0.6f, 0.0f, -1) == 6);
	CHECK(ae_dev_wheel_pick(1, 0.5f, 0.1f, -1) == 0);
	CHECK(ae_dev_wheel_pick(0, 0.5f, 0.5f, -1) == -1);
	/* (sticky: slice 0 ends at 22.5 degrees; 26 is inside the 9 degrees it holds on, 33 is past it) */
	CHECK(ae_dev_wheel_pick(8, 0.6f * 0.438f, 0.6f * 0.899f, 0) == 0);
	CHECK(ae_dev_wheel_pick(8, 0.6f * 0.545f, 0.6f * 0.839f, 0) == 1);
	CHECK(ae_dev_wheel_pick(8, 0.6f * 0.438f, 0.6f * 0.899f, -1) == 1);
	/* (back past the top, the other way, and across the wrap) */
	CHECK(ae_dev_wheel_pick(8, -0.6f * 0.438f, 0.6f * 0.899f, 0) == 0);
	CHECK(ae_dev_wheel_pick(8, -0.6f * 0.545f, 0.6f * 0.839f, 0) == 7);
	CHECK(ae_dev_wheel_pick(8, 0.0f, 0.15f, 3) == -1);
	/* (a name's margin: the middle of a slice's ring is deep inside, outside it negative) */
	CHECK(ae_dev_wheel_margin(1.0f, 8, 0, 0.0f, -207.0f) > 50.0f);
	CHECK(ae_dev_wheel_margin(1.0f, 8, 0, 0.0f, 100.0f) < 0.0f);
	CHECK(ae_dev_wheel_margin(1.0f, 8, 0, 207.0f, 0.0f) < 0.0f);
	CHECK(ae_dev_wheel_margin(1.0f, 8, 2, 207.0f, 0.0f) > 50.0f);
	CHECK(ae_dev_wheel_margin(1.0f, 8, 0, 0.0f, -330.0f) < 0.0f);
	CHECK(ae_dev_wheel_scale(1080.0f) == 1.0f && ae_dev_wheel_scale(540.0f) == 0.6f && ae_dev_wheel_scale(100.0f) == 0.5f);
	/* (a pack's weapon shows its pack's name: the EVO RIFLE's tag is named "dmr", as the DMR's is) */
	{
		char evo[64], dmr[64];

		CHECK(ae_dev_wheel_pack_name("ae\\packs\\evo_rifle\\weapons\\evolved\\dmr\\dmr", evo, sizeof(evo)));
		CHECK(ae_dev_wheel_pack_name("ae\\packs\\dmr\\weapons\\evolved\\dmr\\dmr", dmr, sizeof(dmr)));
		CHECK(!strcmp(evo, "EVO RIFLE") && !strcmp(dmr, "DMR") && strcmp(evo, dmr));
		CHECK(ae_dev_wheel_pack_name("AE\\Packs\\needler\\weapons\\needler\\needler", evo, sizeof(evo)) &&
			!strcmp(evo, "NEEDLER"));
		CHECK(ae_dev_wheel_pack_name("ae\\packs\\smg_silenced\\weapons\\smg\\smg", evo, sizeof(evo)) &&
			!strcmp(evo, "SUPPRESSED SMG"));
		/* (a pack this build does not know: its folder in capitals) */
		CHECK(ae_dev_wheel_pack_name("ae\\packs\\plasma_carbine\\weapons\\x\\x", evo, sizeof(evo)) &&
			!strcmp(evo, "PLASMA CARBINE"));
		/* (not a pack's: the stock weapons, a bad path, a cut name) */
		CHECK(!ae_dev_wheel_pack_name("weapons\\needler\\needler", evo, sizeof(evo)) && !evo[0]);
		CHECK(!ae_dev_wheel_pack_name("ae\\packs\\", evo, sizeof(evo)));
		CHECK(!ae_dev_wheel_pack_name("ae\\packs\\dmr", evo, sizeof(evo)));
		CHECK(!ae_dev_wheel_pack_name(NULL, evo, sizeof(evo)));
		CHECK(ae_dev_wheel_pack_name("ae\\packs\\evo_rifle\\w\\dmr", evo, 4) && !strcmp(evo, "EVO"));
	}
	if (failures)
		return 1;
	printf("ae_dev_wheel_rules_test: ok\n");
	return 0;
}
