/* ae_map_skins_rules_test.c: a map's own look for AE's weapons (ae_map_skins_rules.h) */

#include <stdio.h>

#include "ae_map_skins_rules.h"

static int failures;

#define CHECK(condition) \
	do { if (!(condition)) { printf("%s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

int main(void)
{
	/* the Digsite maps: the item's suffix at the end, case aside */
	CHECK(ae_map_skins_rules_digsite("5@ws3477809506"));
	CHECK(ae_map_skins_rules_digsite("birdcage_dig@WS3477809506"));
	CHECK(!ae_map_skins_rules_digsite("@ws3477809506"));
	CHECK(!ae_map_skins_rules_digsite("5@ws34778095060"));
	CHECK(!ae_map_skins_rules_digsite("5@ws347780950"));
	CHECK(!ae_map_skins_rules_digsite("5@ws2886943774"));
	CHECK(!ae_map_skins_rules_digsite("bloodgulch"));
	CHECK(!ae_map_skins_rules_digsite(NULL));

	/* the map's own weapons: a Workshop map keeps both; a Digsite map keeps its needler only; any other map neither */
	CHECK(ae_map_skins_rules_keeps_own("5@ws2886943774", 0));
	CHECK(ae_map_skins_rules_keeps_own("5@ws2886943774", 1));
	CHECK(!ae_map_skins_rules_keeps_own("5@ws3477809506", 0));
	CHECK(ae_map_skins_rules_keeps_own("5@ws3477809506", 1));
	CHECK(!ae_map_skins_rules_keeps_own("bloodgulch", 0));
	CHECK(!ae_map_skins_rules_keeps_own("h2d_foundation", 1));
	CHECK(!ae_map_skins_rules_keeps_own(NULL, 0));

	/* a weapon's Digsite look: its name and "_digsite", case aside, a slash a backslash */
	CHECK(ae_map_skins_rules_is_look("ae\\packs\\battle_rifle\\cmt\\weapons\\evolved\\battle_rifle\\battle_rifle",
		"ae\\packs\\battle_rifle\\cmt\\weapons\\evolved\\battle_rifle\\battle_rifle_digsite"));
	CHECK(ae_map_skins_rules_is_look("a\\b\\rifle", "A/B/Rifle_DIGSITE"));
	CHECK(!ae_map_skins_rules_is_look("a\\b\\rifle", "a\\b\\rifle"));
	CHECK(!ae_map_skins_rules_is_look("a\\b\\rifle", "a\\b\\rifle_digsite2"));
	CHECK(!ae_map_skins_rules_is_look("a\\b\\rifle", "a\\b\\rifle_digsit"));
	CHECK(!ae_map_skins_rules_is_look("a\\b\\rifle", "a\\c\\rifle_digsite"));
	CHECK(!ae_map_skins_rules_is_look("", "_digsite"));
	CHECK(!ae_map_skins_rules_is_look(NULL, "x_digsite") && !ae_map_skins_rules_is_look("x", NULL));

	/* a Digsite map's battle rifle: by its bullet */
	CHECK(ae_map_skins_rules_digsite_rifle_bullet("weapons\\battle rifle\\bullet"));
	CHECK(ae_map_skins_rules_digsite_rifle_bullet("Weapons/Battle Rifle/Bullet"));
	CHECK(!ae_map_skins_rules_digsite_rifle_bullet("weapons\\battle rifle\\bullet2"));
	CHECK(!ae_map_skins_rules_digsite_rifle_bullet("weapons\\pistol\\bullet"));
	CHECK(!ae_map_skins_rules_digsite_rifle_bullet(NULL));

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_map_skins_rules_test: ok\n");
	return failures ? 1 : 0;
}
