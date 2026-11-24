#include <stdio.h>
#include <string.h>
#include "ae_campaign_weapons_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned int primary, sidearm, id;
	unsigned long seed;
	int each, taken, kind, again;
	int seen[32];

	/* the kind is the leaf of the tag path */
	CHECK(ae_campaign_weapons_kind_of("weapons\\assault rifle\\assault rifle") == AE_CAMPAIGN_WEAPONS_ASSAULT_RIFLE);
	CHECK(ae_campaign_weapons_kind_of("Weapons\\Pistol\\Pistol") == AE_CAMPAIGN_WEAPONS_PISTOL);
	CHECK(ae_campaign_weapons_kind_of("weapons/sniper rifle/sniper rifle") == AE_CAMPAIGN_WEAPONS_SNIPER_RIFLE);
	CHECK(ae_campaign_weapons_kind_of("weapons\\needler\\needler") == AE_CAMPAIGN_WEAPONS_NEEDLER);
	CHECK(ae_campaign_weapons_kind_of("weapons\\rocket launcher\\rocket launcher") == AE_CAMPAIGN_WEAPONS_ROCKET_LAUNCHER);
	CHECK(ae_campaign_weapons_kind_of("pistol") == AE_CAMPAIGN_WEAPONS_PISTOL);
	/* the rest stays the level's own: other leaves, a folder named for a weapon, an ammo or a bullet tag */
	CHECK(ae_campaign_weapons_kind_of("weapons\\shotgun\\shotgun") == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(ae_campaign_weapons_kind_of("weapons\\plasma rifle\\plasma rifle") == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(ae_campaign_weapons_kind_of("weapons\\flamethrower\\flamethrower") == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(ae_campaign_weapons_kind_of("weapons\\pistol\\pistol ammo") == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(ae_campaign_weapons_kind_of("weapons\\pistol\\bullet") == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(ae_campaign_weapons_kind_of("pistol\\plasma pistol") == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(ae_campaign_weapons_kind_of("weapons\\assault rifle\\") == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(ae_campaign_weapons_kind_of("") == AE_CAMPAIGN_WEAPONS_NONE && ae_campaign_weapons_kind_of(NULL) == AE_CAMPAIGN_WEAPONS_NONE);
	CHECK(!strcmp(ae_campaign_weapons_kind_name(AE_CAMPAIGN_WEAPONS_PISTOL), "pistol"));
	CHECK(!strcmp(ae_campaign_weapons_kind_name(99), "none"));

	/* the share setting */
	CHECK(ae_campaign_weapons_share_clamp(0) == 0 && ae_campaign_weapons_share_clamp(100) == 100 && ae_campaign_weapons_share_clamp(35) == 35);
	CHECK(ae_campaign_weapons_share_clamp(-1) == AE_CAMPAIGN_WEAPONS_SHARE_DEFAULT && ae_campaign_weapons_share_clamp(101) == AE_CAMPAIGN_WEAPONS_SHARE_DEFAULT);

	/* the seed: the same for the same place, other for another place, map or salt; the map's case does not matter */
	seed = ae_campaign_weapons_seed("b30", 1.0f, 2.0f, 3.0f, 0);
	CHECK(seed == ae_campaign_weapons_seed("b30", 1.0f, 2.0f, 3.0f, 0));
	CHECK(seed == ae_campaign_weapons_seed("B30", 1.0f, 2.0f, 3.0f, 0));
	CHECK(seed != ae_campaign_weapons_seed("b30", 1.5f, 2.0f, 3.0f, 0));
	CHECK(seed != ae_campaign_weapons_seed("a10", 1.0f, 2.0f, 3.0f, 0));
	CHECK(seed != ae_campaign_weapons_seed("b30", 1.0f, 2.0f, 3.0f, 1));
	CHECK(ae_campaign_weapons_seed(NULL, 0, 0, 0, 0) == ae_campaign_weapons_seed("", 0, 0, 0, 0));

	/* the share: none, all, and about the percent over many places */
	CHECK(!ae_campaign_weapons_share_takes(seed, 0) && ae_campaign_weapons_share_takes(seed, 100));
	for (each = 0, taken = 0; each < 1000; each++)
		taken += ae_campaign_weapons_share_takes(ae_campaign_weapons_seed("d20", (float)each, 7.0f, 0.0f, 0), 60);
	CHECK(taken > 540 && taken < 660);
	for (each = 0, taken = 0; each < 1000; each++)
		taken += ae_campaign_weapons_share_takes(ae_campaign_weapons_seed("d20", (float)each, 7.0f, 0.0f, 0), 10);
	CHECK(taken > 60 && taken < 140);

	/* the table: a kind's weapons are the cache's ids, every one of a many-weapon kind comes up */
	CHECK(ae_campaign_weapons_pick(AE_CAMPAIGN_WEAPONS_SNIPER_RIFLE, seed) == 9);
	CHECK(ae_campaign_weapons_pick(AE_CAMPAIGN_WEAPONS_NEEDLER, seed) == 8);
	CHECK(ae_campaign_weapons_pick(AE_CAMPAIGN_WEAPONS_ROCKET_LAUNCHER, seed) == 11);
	CHECK(ae_campaign_weapons_pick(AE_CAMPAIGN_WEAPONS_NONE, seed) == 0 && ae_campaign_weapons_pick(77, seed) == 0);
	for (kind = AE_CAMPAIGN_WEAPONS_ASSAULT_RIFLE; kind <= AE_CAMPAIGN_WEAPONS_PISTOL; kind++)
	{
		memset(seen, 0, sizeof seen);
		for (each = 0; each < 200; each++)
		{
			id = ae_campaign_weapons_pick(kind, ae_campaign_weapons_seed("c40", (float)each, 0.0f, 0.0f, 5));
			CHECK(id > 0 && id < 32);
			if (id < 32)
				seen[id]++;
		}
		again = 0;
		for (each = 0; each < 32; each++)
			again += seen[each] > 0;
		CHECK(again == (kind == AE_CAMPAIGN_WEAPONS_ASSAULT_RIFLE ? 3 : 2));
	}
	CHECK(ae_campaign_weapons_pick(AE_CAMPAIGN_WEAPONS_PISTOL, seed) == ae_campaign_weapons_pick(AE_CAMPAIGN_WEAPONS_PISTOL, seed));

	/* the squad's loadout word */
	CHECK(ae_campaign_weapons_loadout_of_word("ae") == AE_CAMPAIGN_WEAPONS_LOADOUT_AE);
	CHECK(ae_campaign_weapons_loadout_of_word("AE") == AE_CAMPAIGN_WEAPONS_LOADOUT_AE);
	CHECK(ae_campaign_weapons_loadout_of_word("standard") == AE_CAMPAIGN_WEAPONS_LOADOUT_STANDARD);
	CHECK(ae_campaign_weapons_loadout_of_word("") == AE_CAMPAIGN_WEAPONS_LOADOUT_STANDARD);
	CHECK(ae_campaign_weapons_loadout_of_word("aero") == AE_CAMPAIGN_WEAPONS_LOADOUT_STANDARD);
	CHECK(ae_campaign_weapons_loadout_of_word(NULL) == AE_CAMPAIGN_WEAPONS_LOADOUT_STANDARD);

	/* the squad: each member's own pair, wrapping past four, a negative slot is the first */
	ae_campaign_weapons_squad(0, &primary, &sidearm);
	CHECK(primary == 12 && sidearm == 13);
	ae_campaign_weapons_squad(1, &primary, &sidearm);
	CHECK(primary == 1 && sidearm == 17);
	ae_campaign_weapons_squad(3, &primary, &sidearm);
	CHECK(primary == 4 && sidearm == 17);
	ae_campaign_weapons_squad(4, &primary, &sidearm);
	CHECK(primary == 12 && sidearm == 13);
	ae_campaign_weapons_squad(-2, &primary, &sidearm);
	CHECK(primary == 12 && sidearm == 13);

	/* the AE WEAPONS row: off, then SOME 30, MOST 60, ALL 100; a hand-written share lands on the step at or above it */
	CHECK(ae_campaign_weapons_level_of(0, 60) == AE_CAMPAIGN_WEAPONS_LEVEL_OFF && ae_campaign_weapons_level_of(0, 0) == 0);
	CHECK(ae_campaign_weapons_level_of(1, 30) == AE_CAMPAIGN_WEAPONS_LEVEL_SOME && ae_campaign_weapons_level_of(1, 10) == AE_CAMPAIGN_WEAPONS_LEVEL_SOME);
	CHECK(ae_campaign_weapons_level_of(1, 60) == AE_CAMPAIGN_WEAPONS_LEVEL_MOST && ae_campaign_weapons_level_of(1, 45) == AE_CAMPAIGN_WEAPONS_LEVEL_MOST);
	CHECK(ae_campaign_weapons_level_of(1, 100) == AE_CAMPAIGN_WEAPONS_LEVEL_ALL && ae_campaign_weapons_level_of(1, 99) == AE_CAMPAIGN_WEAPONS_LEVEL_ALL);
	CHECK(ae_campaign_weapons_level_of(1, -5) == AE_CAMPAIGN_WEAPONS_LEVEL_MOST);
	for (each = 0; each < AE_CAMPAIGN_WEAPONS_LEVELS; each++)
		CHECK(each == AE_CAMPAIGN_WEAPONS_LEVEL_OFF ? ae_campaign_weapons_level_share(each) == 0 :
			ae_campaign_weapons_level_of(1, ae_campaign_weapons_level_share(each)) == each);
	CHECK(ae_campaign_weapons_level_share(2) == 60 && ae_campaign_weapons_level_share(9) == 0);
	CHECK(!strcmp(ae_campaign_weapons_level_name(0), "OFF") && !strcmp(ae_campaign_weapons_level_name(1), "SOME") &&
		!strcmp(ae_campaign_weapons_level_name(2), "MOST") && !strcmp(ae_campaign_weapons_level_name(3), "ALL") &&
		!strcmp(ae_campaign_weapons_level_name(-1), "OFF"));
	CHECK(ae_campaign_weapons_level_step(0, 1) == 1 && ae_campaign_weapons_level_step(3, 1) == 0 &&
		ae_campaign_weapons_level_step(0, -1) == 3 && ae_campaign_weapons_level_step(2, -1) == 1 &&
		ae_campaign_weapons_level_step(77, 1) == 1);

	/* guests need the cache in a campaign game with AE WEAPONS on, or with an AE squad asked for; never in another game */
	CHECK(ae_campaign_weapons_join_needs_cache(1, 1, 0, 0));
	CHECK(ae_campaign_weapons_join_needs_cache(1, 0, 1, 2));
	CHECK(!ae_campaign_weapons_join_needs_cache(1, 0, 1, 0));
	CHECK(!ae_campaign_weapons_join_needs_cache(1, 0, 0, 3));
	CHECK(!ae_campaign_weapons_join_needs_cache(1, 0, 0, 0));
	CHECK(!ae_campaign_weapons_join_needs_cache(0, 1, 1, 3));

	if (!failures)
		printf("ae_campaign_weapons_rules: ok\n");
	return failures != 0;
}
