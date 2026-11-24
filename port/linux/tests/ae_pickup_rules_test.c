#include <stdio.h>
#include <string.h>
#include "ae_pickup_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* which maps are Halo 2's: the file, after any folder, starts with h2d_ or h2_ */
	CHECK(ae_pickup_map_is_halo2("h2d_turf"));
	CHECK(ae_pickup_map_is_halo2("maps\\H2 Demakes\\H2D_Coagulation"));
	CHECK(ae_pickup_map_is_halo2("H2_Zanzibar@ce"));
	CHECK(ae_pickup_map_is_halo2("maps/h2_warlock"));
	CHECK(!ae_pickup_map_is_halo2("bloodgulch"));
	CHECK(!ae_pickup_map_is_halo2("octagon_b"));
	CHECK(!ae_pickup_map_is_halo2("maps\\h2d_turf\\beavercreek"));
	CHECK(!ae_pickup_map_is_halo2("ah2d_turf"));
	CHECK(!ae_pickup_map_is_halo2(""));
	CHECK(!ae_pickup_map_is_halo2(NULL));

	/* the kinds: the stock list's assault rifle and sniper anywhere; a pistol only on a Halo 2 map, by the list or a
	magnum's name; a Halo 2 map's own brute shot by its name; the rest none */
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_ASSAULT_RIFLE, "weapons\\assault rifle\\assault rifle", 0) ==
		AE_PICKUP_KIND_ASSAULT_RIFLE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_SNIPER_RIFLE, "weapons\\sniper rifle\\sniper rifle", 0) ==
		AE_PICKUP_KIND_SNIPER_RIFLE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_PISTOL, "weapons\\pistol\\pistol", 0) == AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_PISTOL, "weapons\\pistol\\pistol", 1) == AE_PICKUP_KIND_PISTOL);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "halo2\\weapons\\human\\magnum\\Magnum", 1) == AE_PICKUP_KIND_PISTOL);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "halo2\\weapons\\human\\magnum\\magnum", 0) == AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "halo2\\weapons\\brute\\brute shot\\brute shot", 1) ==
		AE_PICKUP_KIND_BRUTE_SHOT);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_ROCKET_LAUNCHER, "h2\\weapons\\Brute_Shot", 1) == AE_PICKUP_KIND_BRUTE_SHOT);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "halo2\\weapons\\brute shot\\brute shot", 0) == AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "halo2\\weapons\\brute shot\\brute shot ammo", 1) == AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "halo2\\weapons\\magnum ammo", 1) == AE_PICKUP_KIND_NONE);
	/* (a folder named so is not the weapon: the leaf decides) */
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "brute shot\\plasma rifle", 1) == AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_ROCKET_LAUNCHER, "weapons\\rocket launcher\\rocket launcher", 1) ==
		AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_SHOTGUN, "weapons\\shotgun\\shotgun", 1) == AE_PICKUP_KIND_NONE);
	/* (the needler anywhere, 0.9.3: a gametype not naming the NEEDLER leaves the map's stock one otherwise) */
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "weapons\\needler\\needler", 0) == AE_PICKUP_KIND_NEEDLER);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "weapons\\needler\\Needler", 1) == AE_PICKUP_KIND_NEEDLER);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "h2\\weapons\\needler_h2", 1) == AE_PICKUP_KIND_NEEDLER);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "weapons\\needler\\super needler", 0) == AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, "needler\\plasma rifle", 0) == AE_PICKUP_KIND_NONE);
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_NONE, NULL, 1) == AE_PICKUP_KIND_NONE);
	/* (a Halo 2 map's assault rifle and sniper are swapped as anywhere) */
	CHECK(ae_pickup_kind_of(AE_WEAPON_STOCK_ASSAULT_RIFLE, "weapons\\assault rifle\\assault rifle", 1) ==
		AE_PICKUP_KIND_ASSAULT_RIFLE);

	/* the table: the assault rifle spots take turns, COMBAT AR, SUPPRESSED AR, the map's own */
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_ASSAULT_RIFLE, 0) == 12);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_ASSAULT_RIFLE, 1) == 5);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_ASSAULT_RIFLE, 2) == 0);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_ASSAULT_RIFLE, 3) == 12);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_ASSAULT_RIFLE, 7) == 5);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_SNIPER_RIFLE, 5) == 9);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_PISTOL, 0) == 13);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_BRUTE_SHOT, 0) == 11);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_NEEDLER, 4) == 8);
	CHECK(ae_pickup_weapon(AE_PICKUP_KIND_NONE, 0) == 0);

	/* the weapon set: the spot's own weapon swaps only where the set left it what it was */
	CHECK(ae_pickup_set_keeps(AE_WEAPON_STOCK_ASSAULT_RIFLE, AE_WEAPON_STOCK_ASSAULT_RIFLE, 1));
	CHECK(ae_pickup_set_keeps(AE_WEAPON_STOCK_ASSAULT_RIFLE, AE_WEAPON_STOCK_ASSAULT_RIFLE, 0));
	CHECK(!ae_pickup_set_keeps(AE_WEAPON_STOCK_ASSAULT_RIFLE, AE_WEAPON_STOCK_SNIPER_RIFLE, 0));
	CHECK(!ae_pickup_set_keeps(AE_WEAPON_STOCK_PISTOL, AE_WEAPON_STOCK_SNIPER_RIFLE, 0));
	CHECK(!ae_pickup_set_keeps(AE_WEAPON_STOCK_SHOTGUN, AE_WEAPON_STOCK_ASSAULT_RIFLE, 0));
	CHECK(ae_pickup_set_keeps(AE_WEAPON_STOCK_SNIPER_RIFLE, AE_WEAPON_STOCK_SNIPER_RIFLE, 0));
	/* (a Halo 2 map's own magnum or brute shot is no stock weapon: kept only by the same definition) */
	CHECK(ae_pickup_set_keeps(AE_WEAPON_STOCK_NONE, AE_WEAPON_STOCK_NONE, 1));
	CHECK(!ae_pickup_set_keeps(AE_WEAPON_STOCK_NONE, AE_WEAPON_STOCK_NONE, 0));
	CHECK(!ae_pickup_set_keeps(AE_WEAPON_STOCK_NONE, AE_WEAPON_STOCK_ROCKET_LAUNCHER, 0));

	if (failures)
		return 1;
	printf("ae_pickup_rules: all checks passed\n");
	return 0;
}
